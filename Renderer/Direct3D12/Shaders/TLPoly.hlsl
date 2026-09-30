//=====================================================================================
//	TLPoly.hlsl
//
//	Shaders for the engine's transformed-and-lit polygons (grTLVertex): world faces,
//	models, actors, particles, decals and the 2D overlay.
//
//	Built at compile time by DXC (shader model 6.0) into the driver; see the
//	G3DCompileShaders target in Direct3D12Driver.vcxproj. The same source is embedded
//	as a resource and compiled at run time (shader model 5.0) when the device has no
//	SM 6.0 support or [Shaders] SourceDir is set in Direct3D12Driver.ini, so it must
//	stay valid for both compilers and must not #include other files.
//=====================================================================================

cbuffer DrawConstants : register(b0)
{
    float ViewportWidth;
    float ViewportHeight;
    uint  DrawFlags;			// GR_RENDER_FLAG_*
    uint  DrawPadding;
};

// GR_RENDER_FLAG_* bits the pixel shaders look at.
#define FLAG_ALPHA			0x00000001u
#define FLAG_COLORKEY		0x00000004u
#define FLAG_CLAMP_UV		0x00000008u
#define FLAG_BILINEAR		0x00000400u

Texture2D BaseTexture : register(t0);
Texture2D LightTexture : register(t1);
SamplerState LinearWrapSampler  : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState PointWrapSampler   : register(s2);
SamplerState PointClampSampler  : register(s3);

struct VS_INPUT
{
    float4 Position : POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;

    // grTLVertex contains pixel-space x/y and positive camera-space z. The legacy
    // driver used XYZRHW with depth = 1 - 1/z. Constructing this clip position
    // reproduces that projection and preserves perspective-correct UV interpolation.
    float width = max(ViewportWidth, 1.0f);
    float height = max(ViewportHeight, 1.0f);
    float cameraZ = max(input.Position.z, 0.0001f);
    float ndcX = input.Position.x * (2.0f / width) - 1.0f;
    float ndcY = 1.0f - input.Position.y * (2.0f / height);
    float depth = saturate(1.0f - rcp(cameraZ));
    output.Position = float4(ndcX * cameraZ, ndcY * cameraZ, depth * cameraZ, cameraZ);
    output.Color = saturate(input.Color);
    output.TexCoord = input.TexCoord;
    output.LMCoord = input.LMCoord;
    return output;
}

float4 SampleBase(float2 uv)
{
    bool clampUV = (DrawFlags & FLAG_CLAMP_UV) != 0;
    bool linearFilter = (DrawFlags & FLAG_BILINEAR) != 0;
    if (linearFilter)
        return clampUV ? BaseTexture.Sample(LinearClampSampler, uv)
                       : BaseTexture.Sample(LinearWrapSampler, uv);
    return clampUV ? BaseTexture.Sample(PointClampSampler, uv)
                   : BaseTexture.Sample(PointWrapSampler, uv);
}

float4 SampleLight(float2 uv)
{
    return ((DrawFlags & FLAG_BILINEAR) != 0)
        ? LightTexture.Sample(LinearClampSampler, uv)
        : LightTexture.Sample(PointClampSampler, uv);
}

float4 PSGouraud(VS_OUTPUT input) : SV_TARGET
{
    return input.Color;
}

float4 PSTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.TexCoord);
    if ((DrawFlags & FLAG_COLORKEY) != 0)
        clip(base.a - 0.5f);

    float alpha = input.Color.a;
    if ((DrawFlags & (FLAG_ALPHA | FLAG_COLORKEY)) != 0)
        alpha *= base.a;
    return saturate(float4(base.rgb * input.Color.rgb, alpha));
}

float4 PSMultiTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.TexCoord);
    if ((DrawFlags & FLAG_COLORKEY) != 0)
        clip(base.a - 0.5f);

    float3 light = SampleLight(input.LMCoord).rgb;
    float alpha = input.Color.a;
    if ((DrawFlags & (FLAG_ALPHA | FLAG_COLORKEY)) != 0)
        alpha *= base.a;
    return saturate(float4(base.rgb * light * input.Color.rgb, alpha));
}
