//=====================================================================================
//	Post.hlsl
//
//	Post-processing (roadmap Phase 3, D3D12Post.cpp): auto exposure, SSAO, bloom, fog,
//	tonemapping and the Stylized look's palette, dither and CRT, from the scene target
//	into the composite target.
//
//	Same build rules as TLPoly.hlsl (DXC SM 6.0, embedded source for the SM 5.1
//	fallback, no #includes).
//=====================================================================================

cbuffer PostConstants : register(b0)
{
    float2 SceneSize;			// the scene target's size
    float2 RegionSize;			// the part the scene was drawn in (render scale)
    float2 OutputSize;
    float2 InvOutputSize;
    uint   SourceIndex;			// heap indices
    uint   SceneIndex;
    uint   DepthIndex;
    uint   BloomIndex;
    uint   AOIndex;
    uint   PostFlags;			// POST_*
    uint   Tonemapper;			// 0 none, 1 ACES, 2 AgX
    uint   PaletteLevels;
    float  ExposureEV;
    float  BloomIntensity;
    float  SSAORadius;
    float  SSAOIntensity;
    float3 FogColor;
    float  FogStart;
    float  FogEnd;
    float  HeightFogDensity;
    float  HeightFogBase;
    float  HeightFogFalloff;
    float3 HeightFogColor;
    float  FrameIndex;
    float4 CameraRows[3];		// main camera, world to camera
    float  Scale;
    float  XCenter;
    float  YCenter;
    float  ZScale;
    float2 SourceSize;			// this pass's source texture
    float2 SourceRegion;		// the part of it to read
    float  MinEV;
    float  MaxEV;
    float  KeyValue;
    float  AdaptRate;
    float  NumBloomLevels;
    float  PostRenderScale;
    float2 PostPadding;
};

#define POST_INPUT_LINEAR		0x0001u
#define POST_AUTO_EXPOSURE		0x0002u
#define POST_BLOOM				0x0004u
#define POST_SSAO				0x0008u
#define POST_FOG				0x0010u
#define POST_HEIGHT_FOG			0x0020u
#define POST_DITHER				0x0040u
#define POST_CRT				0x0080u
#define POST_HAVE_CAMERA		0x0100u
#define POST_PALETTE			0x0200u

Texture2D Textures[] : register(t0, space1);
StructuredBuffer<float4> ExposureIn : register(t0, space2);	// x: EV, y: target EV, z: 1 once set
RWStructuredBuffer<uint> HistogramOut : register(u0);
RWStructuredBuffer<float4> ExposureOut : register(u1);
SamplerState LinearClampSampler : register(s0);
SamplerState PointClampSampler  : register(s1);

static const float MinLogLum = -10.0f;
static const float MaxLogLum = 4.0f;
static const float PI = 3.14159265f;

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

VS_OUTPUT VSPost(uint VertexId : SV_VertexID)
{
    VS_OUTPUT output;
    float2 uv = float2((VertexId << 1) & 2, VertexId & 2);
    output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.UV = uv;
    return output;
}

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

// The source texture coordinates of an output pixel, over the source region.
float2 SourceUV(float2 uv)
{
    return uv * SourceRegion / SourceSize;
}

//-------------------------------------------------------------------------------------
//	Scene depth and positions
//-------------------------------------------------------------------------------------

// Camera depth (distance along the view) from the depth buffer; 1e30 for nothing drawn.
float DepthToZ(float depth)
{
    return (depth < 1.0f && ZScale > 0.0f) ? 1.0f / ((1.0f - depth) * ZScale) : 1e30f;
}

// Camera-space position of a scene pixel (render-scale pixel coordinates).
float3 CameraPosition(float2 regionPixel, float Z)
{
    float2 engine = (regionPixel + 0.5f) / PostRenderScale - 0.5f;
    return float3((engine.x - XCenter) * Z / Scale, -(engine.y - YCenter) * Z / Scale, -Z);
}

float3 CameraToWorld(float3 c)
{
    float3 local = c - float3(CameraRows[0].w, CameraRows[1].w, CameraRows[2].w);
    return float3(dot(float3(CameraRows[0].x, CameraRows[1].x, CameraRows[2].x), local),
                  dot(float3(CameraRows[0].y, CameraRows[1].y, CameraRows[2].y), local),
                  dot(float3(CameraRows[0].z, CameraRows[1].z, CameraRows[2].z), local));
}

float LoadZ(int2 pixel)
{
    pixel = clamp(pixel, int2(0, 0), int2(RegionSize) - 1);
    return DepthToZ(Textures[DepthIndex].Load(int3(pixel, 0)).r);
}

//-------------------------------------------------------------------------------------
//	Auto exposure
//-------------------------------------------------------------------------------------

groupshared uint LocalHistogram[256];

[numthreads(256, 1, 1)]
void CSHistogramClear(uint index : SV_GroupIndex)
{
    HistogramOut[index] = 0;
}

[numthreads(16, 16, 1)]
void CSHistogram(uint3 id : SV_DispatchThreadID, uint index : SV_GroupIndex)
{
    LocalHistogram[index] = 0;
    GroupMemoryBarrierWithGroupSync();
    if (id.x < (uint)RegionSize.x && id.y < (uint)RegionSize.y)
    {
        float lum = Luminance(Textures[SceneIndex].Load(int3(id.xy, 0)).rgb);
        uint bin = (lum < 1e-5f) ? 0u : (uint)(saturate((log2(lum) - MinLogLum) / (MaxLogLum - MinLogLum)) * 254.0f) + 1u;
        InterlockedAdd(LocalHistogram[bin], 1u);
    }
    GroupMemoryBarrierWithGroupSync();
    if (LocalHistogram[index] != 0)
        InterlockedAdd(HistogramOut[index], LocalHistogram[index]);
}

groupshared float WeightedBins[256];
groupshared float Counted[256];

[numthreads(256, 1, 1)]
void CSExposure(uint index : SV_GroupIndex)
{
    // The average log luminance of the lit pixels (bin 0 is black)
    float count = (float)HistogramOut[index];
    WeightedBins[index] = (index == 0) ? 0.0f : count * (float)(index - 1);
    Counted[index] = (index == 0) ? 0.0f : count;
    GroupMemoryBarrierWithGroupSync();
    [unroll] for (uint stride = 128; stride > 0; stride >>= 1)
    {
        if (index < stride)
        {
            WeightedBins[index] += WeightedBins[index + stride];
            Counted[index] += Counted[index + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (index == 0)
    {
        float4 previous = ExposureOut[0];
        float target = previous.x;
        if (Counted[0] > 0.0f)
        {
            float avgLog = WeightedBins[0] / Counted[0] / 254.0f * (MaxLogLum - MinLogLum) + MinLogLum;
            target = clamp(log2(KeyValue) - avgLog, MinEV, MaxEV);
        }
        float ev = (previous.z > 0.5f) ? lerp(previous.x, target, AdaptRate) : target;
        ExposureOut[0] = float4(ev, target, 1.0f, 0.0f);
    }
}

//-------------------------------------------------------------------------------------
//	SSAO (normals from depth)
//-------------------------------------------------------------------------------------

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float4 PSSSAO(VS_OUTPUT input) : SV_TARGET
{
    int2 pixel = int2(input.Position.xy);
    float Z = LoadZ(pixel);
    if (Z > 1e29f)
        return 1.0f;
    float3 P = CameraPosition(pixel, Z);

    // The normal from the flatter of each pair of neighbors
    float3 PR = CameraPosition(pixel + int2(1, 0), LoadZ(pixel + int2(1, 0)));
    float3 PL = CameraPosition(pixel - int2(1, 0), LoadZ(pixel - int2(1, 0)));
    float3 PD = CameraPosition(pixel + int2(0, 1), LoadZ(pixel + int2(0, 1)));
    float3 PU = CameraPosition(pixel - int2(0, 1), LoadZ(pixel - int2(0, 1)));
    float3 dx = (abs(PR.z - P.z) < abs(P.z - PL.z)) ? PR - P : P - PL;
    float3 dy = (abs(PD.z - P.z) < abs(P.z - PU.z)) ? PD - P : P - PU;
    float3 N = normalize(cross(dy, dx));
    if (dot(N, -P) < 0.0f)
        N = -N;

    float3 T = normalize(abs(N.y) < 0.99f ? cross(float3(0.0f, 1.0f, 0.0f), N) : cross(float3(1.0f, 0.0f, 0.0f), N));
    float3 B = cross(N, T);
    float rotation = InterleavedGradientNoise(input.Position.xy + FrameIndex * 0.0f) * 2.0f * PI;

    const uint Samples = 12;
    float occlusion = 0.0f;
    [unroll] for (uint i = 0; i < Samples; i++)
    {
        // A spiral over the hemisphere, rotated per pixel
        float t = (i + 0.5f) / Samples;
        float phi = rotation + i * 2.39996323f;
        float r = sqrt(t);
        float3 h = float3(cos(phi) * r, sin(phi) * r, sqrt(1.0f - t));
        float3 S = P + (T * h.x + B * h.y + N * h.z) * SSAORadius * lerp(0.2f, 1.0f, t * t);
        float SZ = -S.z;
        if (SZ <= 0.0f)
            continue;
        // Back to the scene's pixels
        float2 engine = float2(XCenter + S.x * Scale / SZ, YCenter - S.y * Scale / SZ);
        float2 region = (engine + 0.5f) * PostRenderScale - 0.5f;
        float sceneZ = LoadZ(int2(region + 0.5f));
        float range = saturate(SSAORadius / max(abs(Z - sceneZ), 1e-3f));
        occlusion += (sceneZ < SZ - 1.0f) ? range : 0.0f;
    }
    return 1.0f - occlusion / Samples;
}

// 4x4 box blur that stays on similar depths.
float4 PSSSAOBlur(VS_OUTPUT input) : SV_TARGET
{
    int2 pixel = int2(input.Position.xy);
    float Z = LoadZ(pixel);
    float sum = 0.0f;
    float weight = 0.0f;
    [unroll] for (int y = -2; y < 2; y++)
    {
        [unroll] for (int x = -2; x < 2; x++)
        {
            int2 p = clamp(pixel + int2(x, y), int2(0, 0), int2(RegionSize) - 1);
            float w = (abs(LoadZ(p) - Z) < 0.1f * Z + 4.0f) ? 1.0f : 0.0f;
            sum += Textures[SourceIndex].Load(int3(p, 0)).r * w;
            weight += w;
        }
    }
    return (weight > 0.0f) ? sum / weight : 1.0f;
}

//-------------------------------------------------------------------------------------
//	Bloom (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare")
//-------------------------------------------------------------------------------------

float3 SampleSource(float2 uv)
{
    return Textures[SourceIndex].SampleLevel(LinearClampSampler, SourceUV(uv), 0).rgb;
}

float KarisWeight(float3 c)
{
    return 1.0f / (1.0f + Luminance(c));
}

// 13-tap downsample; the first step (from the scene) averages with Karis weights against
// fireflies.
float4 PSBloomDown(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.UV;
    float2 t = 1.0f / SourceRegion;
    float3 a = SampleSource(uv + t * float2(-2, -2));
    float3 b = SampleSource(uv + t * float2( 0, -2));
    float3 c = SampleSource(uv + t * float2( 2, -2));
    float3 d = SampleSource(uv + t * float2(-2,  0));
    float3 e = SampleSource(uv);
    float3 f = SampleSource(uv + t * float2( 2,  0));
    float3 g = SampleSource(uv + t * float2(-2,  2));
    float3 h = SampleSource(uv + t * float2( 0,  2));
    float3 i = SampleSource(uv + t * float2( 2,  2));
    float3 j = SampleSource(uv + t * float2(-1, -1));
    float3 k = SampleSource(uv + t * float2( 1, -1));
    float3 l = SampleSource(uv + t * float2(-1,  1));
    float3 m = SampleSource(uv + t * float2( 1,  1));

    float3 result;
    if (SourceIndex == SceneIndex)
    {
        float3 g0 = (a + b + d + e) * 0.25f;
        float3 g1 = (b + c + e + f) * 0.25f;
        float3 g2 = (d + e + g + h) * 0.25f;
        float3 g3 = (e + f + h + i) * 0.25f;
        float3 g4 = (j + k + l + m) * 0.25f;
        float w0 = KarisWeight(g0), w1 = KarisWeight(g1), w2 = KarisWeight(g2), w3 = KarisWeight(g3), w4 = KarisWeight(g4);
        result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4 * 4.0f) / (w0 + w1 + w2 + w3 + w4 * 4.0f);
    }
    else
    {
        result = e * 0.125f + (a + c + g + i) * 0.03125f + (b + d + f + h) * 0.0625f + (j + k + l + m) * 0.125f;
    }
    return float4(max(result, 0.0f), 1.0f);
}

// 3x3 tent upsample, added onto the next level up (additive blending).
float4 PSBloomUp(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.UV;
    float2 t = 1.0f / SourceRegion;
    float3 result = SampleSource(uv) * 4.0f;
    result += (SampleSource(uv + t * float2(-1, 0)) + SampleSource(uv + t * float2(1, 0)) +
               SampleSource(uv + t * float2(0, -1)) + SampleSource(uv + t * float2(0, 1))) * 2.0f;
    result += SampleSource(uv + t * float2(-1, -1)) + SampleSource(uv + t * float2(1, -1)) +
              SampleSource(uv + t * float2(-1, 1)) + SampleSource(uv + t * float2(1, 1));
    return float4(result / 16.0f, 1.0f);
}

//-------------------------------------------------------------------------------------
//	Composite: fog, SSAO, bloom, exposure, tonemapping, Stylized
//-------------------------------------------------------------------------------------

float3 ACESFitted(float3 x)
{
    // Narkowicz 2015
    return saturate(x * (2.51f * x + 0.03f) / (x * (2.43f * x + 0.59f) + 0.14f));
}

float3 AgXContrast(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5f * x4 * x2 - 40.14f * x4 * x + 31.96f * x4 - 6.868f * x2 * x + 0.4298f * x2 + 0.1191f * x - 0.00232f;
}

float3 AgX(float3 color)
{
    // Troy Sobotka's AgX base, with Benjamin Wrensch's contrast curve fit
    const float3x3 Inset = float3x3(0.842479062253094f, 0.0423282422610123f, 0.0423756549057051f,
                                    0.0784335999999992f, 0.878468636469772f, 0.0784336f,
                                    0.0792237451477643f, 0.0791661274605434f, 0.879142973793104f);
    const float3x3 Outset = float3x3(1.19687900512017f, -0.0528968517574562f, -0.0529716355144438f,
                                     -0.0980208811401368f, 1.15190312990417f, -0.0980434501171241f,
                                     -0.0990297440797205f, -0.0989611768448433f, 1.15107367264116f);
    const float MinEVAgX = -12.47393f;
    const float MaxEVAgX = 4.026069f;
    color = mul(max(color, 1e-10f), Inset);
    color = clamp(log2(color), MinEVAgX, MaxEVAgX);
    color = (color - MinEVAgX) / (MaxEVAgX - MinEVAgX);
    color = AgXContrast(color);
    color = mul(color, Outset);
    // The curve's output is display-encoded; back to linear for the common encode below
    return pow(saturate(color), 2.2f);
}

static const float Bayer4[16] = {
    0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f,
    12.0f / 16.0f, 4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f,
    3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f,
    15.0f / 16.0f, 7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f
};

float4 PSComposite(VS_OUTPUT input) : SV_TARGET
{
    float2 outPixel = input.Position.xy;
    // Nearest-neighbour upscale of the render-scaled scene
    float2 regionPixel = min(floor(outPixel * RegionSize / OutputSize), RegionSize - 1.0f);
    int3 load = int3(regionPixel, 0);
    float3 color = Textures[SceneIndex].Load(load).rgb;
    float depth = Textures[DepthIndex].Load(load).r;
    bool linearIn = (PostFlags & POST_INPUT_LINEAR) != 0;
    float Z = DepthToZ(depth);

    if ((PostFlags & POST_SSAO) != 0)
        color *= lerp(1.0f, Textures[AOIndex].Load(load).r, SSAOIntensity);

    // The engine's distance fog (SetFog), linear in camera depth
    if ((PostFlags & POST_FOG) != 0)
    {
        float amount = saturate((Z - FogStart) / max(FogEnd - FogStart, 1e-3f));
        float3 fog = linearIn ? pow(FogColor, 2.2f) : FogColor;
        color = lerp(color, fog, amount);
    }

    // Height fog: density * exp(-falloff * (height - base)), integrated along the view ray
    if ((PostFlags & POST_HEIGHT_FOG) != 0)
    {
        float3 eye = CameraToWorld(float3(0.0f, 0.0f, 0.0f));
        float3 P = CameraToWorld(CameraPosition(regionPixel, min(Z, 20000.0f)));
        float3 ray = P - eye;
        float distance = length(ray);
        float dh = ray.y;
        float k = HeightFogFalloff;
        float start = HeightFogDensity * exp(-k * (eye.y - HeightFogBase));
        float integral = (abs(k * dh) > 1e-4f) ? start * (1.0f - exp(-k * dh)) / (k * dh) : start;
        float amount = 1.0f - exp(-integral * distance);
        float3 fog = linearIn ? pow(HeightFogColor, 2.2f) : HeightFogColor;
        color = lerp(color, fog, saturate(amount));
    }

    if (linearIn)
    {
        if ((PostFlags & POST_BLOOM) != 0)
        {
            float3 bloom = Textures[BloomIndex].SampleLevel(LinearClampSampler, (outPixel + 0.5f) * InvOutputSize, 0).rgb;
            color = lerp(color, bloom / NumBloomLevels, BloomIntensity);
        }

        float ev = ExposureEV;
        if ((PostFlags & POST_AUTO_EXPOSURE) != 0)
            ev += ExposureIn[0].x;
        color *= exp2(ev);

        if (Tonemapper == 1u)
            color = ACESFitted(color);
        else if (Tonemapper == 2u)
            color = AgX(color);
        color = pow(saturate(color), 1.0f / 2.2f);
    }

    // Stylized: a smaller palette with an ordered dither, per scene pixel
    if ((PostFlags & POST_PALETTE) != 0)
    {
        float levels = (float)PaletteLevels - 1.0f;
        uint2 cell = uint2(regionPixel) & 3u;
        float threshold = ((PostFlags & POST_DITHER) != 0) ? Bayer4[cell.y * 4 + cell.x] : 0.5f;
        color = floor(saturate(color) * levels + threshold) / levels;
    }

    // Stylized: scanlines (one per scene row) and a slight vignette
    if ((PostFlags & POST_CRT) != 0)
    {
        float row = frac((outPixel.y + 0.5f) * RegionSize.y / OutputSize.y);
        color *= 0.7f + 0.3f * sin(row * PI);
        float2 centered = outPixel * InvOutputSize * 2.0f - 1.0f;
        color *= saturate(1.0f - 0.25f * dot(centered, centered));
    }

    return float4(saturate(color), 1.0f);
}
