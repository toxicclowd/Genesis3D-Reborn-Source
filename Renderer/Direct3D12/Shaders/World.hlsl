//=====================================================================================
//	World.hlsl
//
//	GPU world path (roadmap Phase 1): BSP draw faces uploaded once in model space and
//	transformed here, instead of being clipped, transformed and projected on the CPU.
//
//	The vertex shader reproduces grCamera_ProjectAndClampL and TLPoly.hlsl's VSMain
//	exactly, so world faces land on the same pixels and depths as the transformed
//	polys (actors, models, particles) they are drawn together with. Clip coordinates
//	are kept linear in camera space so the hardware clips against the same side
//	planes through the eye as grFrustum. One difference: the CPU path has no near
//	plane and clamps depth to 0 closer than w = 1 (2 units at the default ZScale),
//	while here depth clipping must stay on, or triangles reaching behind the eye
//	rasterize wrongly, so geometry closer than that is clipped.
//
//	Needs the bindless root signature. Same build rules as TLPoly.hlsl (DXC SM 6.0,
//	embedded source for the SM 5.1 fallback, no #includes).
//=====================================================================================

// Per draw (root constants): only DrawFlags is used; textures come per face.
cbuffer DrawConstants : register(b0)
{
    uint  DrawFlags;			// GR_RENDER_FLAG_*
    uint  BaseTextureIndex;
    uint  LightTextureIndex;
    uint  DrawPadding;
};

// D3D12FrameConstants (D3D12PSOManager.h); the same in every shader file.
cbuffer FrameConstants : register(b1)
{
    float2 ViewportSize;			// back buffer, engine pixel coordinates
    float2 InvViewportSize;
    uint   FrameNumber;
    float  TimeSeconds;
    uint   FrameFlags;				// FRAME_*
    float  RenderScale;				// scene resolution / back buffer
    float4 MainWorldToCamera[3];	// the main camera (light clusters)
    float  MainScale;
    float  MainXCenter;
    float  MainYCenter;
    float  MainZScale;
    uint   NumLights;
    uint   NumDirLights;			// the first NumDirLights lights are directional
    uint   ShadowAtlasIndex;
    float  ShadowAtlasTexel;
    uint4  ClusterDims;				// x, y, z, lights per cluster (or tested per pixel without clusters)
    float2 ClusterTile;				// back buffer pixels
    float  ClusterZNear;
    float  ClusterZLogScale;
    float  MaxRadiance;
    float  VertexSnap;
    float2 FramePadding;
};

#define FRAME_OUTPUT_LINEAR		0x0001u		// linear HDR scene, tonemapped by the post pass
#define FRAME_CLUSTERS			0x0002u
#define FRAME_SNAP_LIGHTING		0x0004u

// DRV_WorldView: model-to-camera rows (xyz = rotation row, w = translation) and the
// camera's screen projection.
cbuffer WorldView : register(b2)
{
    float4 ModelToCamera[3];
    float  ProjScale;
    float  XCenter;
    float  YCenter;
    float  ZScale;
    float  ZFar;				// 0 = no far clip
    float  HalfWidth;			// camera rect: XCenter +/- HalfWidth, YCenter +/- HalfHeight
    float  HalfHeight;
    uint   NumClipPlanes;		// > 0: nested view, clip to ClipPlanes instead of the rect
    float4 ClipPlanes[8];		// camera space, inside when dot(xyz, p) + w >= 0
    // PBR shading (roadmap Phases 2 and 3): the eye and the model's placement, world space.
    float4 EyePos;
    float4 ModelToWorld[3];
    uint   UseClusters;			// the frame's main camera: its light clusters apply
    uint3  ViewPadding;
};

// Frame lighting (D3D12Lighting.cpp)
struct Light
{
    float3 Pos;					// world space
    float  Radius;
    float3 Color;				// 0..1 * brightness
    uint   Flags;				// LIGHT_*
    float3 Dir;					// spot and directional: the direction it travels
    float  CosOuter;
    float  CosInner;
    uint   ShadowView;			// first shadow view, 0xFFFFFFFF = none
    uint2  LightPadding;
};
#define LIGHT_TYPE_MASK		0x0003u
#define LIGHT_POINT			0u
#define LIGHT_SPOT			1u
#define LIGHT_DIRECTIONAL	2u
#define LIGHT_STATIC		0x0004u		// baked into the lightmaps
#define LIGHT_CAST_SHADOWS	0x0008u		// a dynamic light the engine left out of the lightmaps

struct ShadowView
{
    float4 Rows[4];				// world to clip
    float4 Rect;				// atlas uv origin, size
    float4 Params;				// x: cascade far depth, y: world texel size (per unit of distance unless z), z: orthographic
};

StructuredBuffer<Light> FrameLights : register(t0, space3);
StructuredBuffer<uint> ClusterCounts : register(t1, space3);
StructuredBuffer<uint> ClusterItems : register(t2, space3);
StructuredBuffer<ShadowView> ShadowViews : register(t3, space3);
SamplerComparisonState ShadowSampler : register(s4);

// Per face, written by the driver for every face drawn this frame.
struct WorldFace
{
    uint  BaseTexture;			// heap indices
    uint  LightTexture;
    float InvScaleU;			// 1 / layer 0 ScaleU (as the transformed-poly path computes it)
    float InvScaleV;
    float ShiftU;				// layer 0 shift
    float ShiftV;
    float TextureScale;			// 1 << Log of the base texture
    float Alpha;				// 0..1
    float LightShiftU;			// lightmap StartU/StartV
    float LightShiftV;
    float2 LightOffset;			// the lightmap's origin in its texture (an atlas page), * 16
    float2 LightDiv;			// that texture's size * 16
    uint2 FacePadding;
    // PBR material (PSWorldPBR only)
    uint  NormalTexture;
    uint  OrmTexture;
    uint  EmissiveTexture;
    uint  MaterialFlags;		// MAT_*
    float4 BaseColor;			// linear tint
    float Roughness;			// times ORM.g
    float Metal;				// times ORM.b
    float AlphaCutoff;
    float HeightScale;			// parallax depth of height 0, in texture widths
    float3 Emissive;			// linear, times the emissive map
    uint  HeightTexture;
};

#define MAT_NORMAL			0x0001u
#define MAT_ORM				0x0002u
#define MAT_EMISSIVE		0x0004u
#define MAT_LIGHTMAP		0x0008u
#define MAT_RETRO			0x0010u
#define MAT_CUTOUT			0x0020u
#define MAT_TWO_SIDED		0x0040u
#define MAT_VERTEX_LIGHT	0x0080u		// meshes: irradiance from the vertex color
#define MAT_HEIGHT			0x0100u
#define MAT_LEGACY			0x0200u		// no PBR data (Enhanced look): a fullbright face stays unlit
StructuredBuffer<WorldFace> WorldFaces : register(t0, space2);

#define FLAG_ALPHA			0x00000001u
#define FLAG_COLORKEY		0x00000004u
#define FLAG_CLAMP_UV		0x00000008u
#define FLAG_BILINEAR		0x00000400u

Texture2D Textures[] : register(t0, space1);
SamplerState LinearWrapSampler  : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState PointWrapSampler   : register(s2);
SamplerState PointClampSampler  : register(s3);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Tangent  : TANGENT;
    float2 FaceUV   : TEXCOORD0;
    uint   Face     : FACE;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
    nointerpolation uint2 Textures : TEXINDEX;
    float4 ClipA    : SV_ClipDistance0;		// outer view: grFrustum's side planes through the eye
    float4 ClipB    : SV_ClipDistance1;		// outer view: x = far plane
};

// Transforms a model-space point by the view and fills Position and the clip distances.
void ProjectToView(float3 p, inout VS_OUTPUT output)
{
    // grXForm3d_Transform, same operation order.
    float cx = (p.x * ModelToCamera[0].x) + (p.y * ModelToCamera[0].y) + (p.z * ModelToCamera[0].z) + ModelToCamera[0].w;
    float cy = (p.x * ModelToCamera[1].x) + (p.y * ModelToCamera[1].y) + (p.z * ModelToCamera[1].z) + ModelToCamera[1].w;
    float cz = (p.x * ModelToCamera[2].x) + (p.y * ModelToCamera[2].y) + (p.z * ModelToCamera[2].z) + ModelToCamera[2].w;

    // grCamera_ProjectAndClampL gives screen x = cx*Scale/Z + XCenter, y = -cy*Scale/Z + YCenter
    // and z = Z*ZScale (Z = -cz). VSMain turns that into clip = (ndc.xy * z, z - 1, z),
    // including its half-pixel shift. Multiplying through by z keeps every component
    // linear in camera space.
    float Z = -cz;
    float w = Z * ZScale;
    float sx = 2.0f / max(ViewportSize.x, 1.0f);
    float sy = 2.0f / max(ViewportSize.y, 1.0f);
    float4 position;
    position.x = ZScale * ((cx * ProjScale) * sx + Z * ((XCenter + 0.5f) * sx - 1.0f));
    position.y = ZScale * ((cy * ProjScale) * sy + Z * (1.0f - (YCenter + 0.5f) * sy));
    position.z = w - 1.0f;		// depth 1 - 1/w, as VSMain for w >= 1
    position.w = w;

    // Stylized look: vertices snap to a coarse screen grid (the wobble of early 3D hardware)
    if (VertexSnap > 0.0f && w > 0.0f)
    {
        float2 grid = float2(VertexSnap, VertexSnap * ViewportSize.y / max(ViewportSize.x, 1.0f));
        float2 ndc = position.xy / w;
        ndc = (floor((ndc * 0.5f + 0.5f) * grid) + 0.5f) / grid * 2.0f - 1.0f;
        position.xy = ndc * w;
    }
    output.Position = position;
    // The camera rect may be smaller than the viewport, so clip to the camera's own
    // frustum like the CPU path does: screen x within XCenter +/- HalfWidth is
    // cx*Scale/Z within +/- HalfWidth, i.e. (+/-cx)*Scale + HalfWidth*Z >= 0.
    if (NumClipPlanes == 0)
    {
        output.ClipA = float4(cx * ProjScale + HalfWidth * Z,
                              HalfWidth * Z - cx * ProjScale,
                              HalfHeight * Z - cy * ProjScale,
                              cy * ProjScale + HalfHeight * Z);
        output.ClipB = float4((ZFar > 0.0f) ? (ZFar - Z) : 1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        // A portal or mirror view: its frustum is the portal polygon's edges seen from
        // the eye (grFrustum_SetFromLVerts2), plus the far plane, set by the engine.
        float4 c = float4(cx, cy, cz, 1.0f);
        float d[8];
        [unroll] for (uint i = 0; i < 8; i++)
            d[i] = (i < NumClipPlanes) ? dot(ClipPlanes[i], c) : 1.0f;
        output.ClipA = float4(d[0], d[1], d[2], d[3]);
        output.ClipB = float4(d[4], d[5], d[6], d[7]);
    }
}

// Model space to world space (ModelToWorld is rigid).
float3 ToWorld(float3 p)
{
    float4 q = float4(p, 1.0f);
    return float3(dot(ModelToWorld[0], q), dot(ModelToWorld[1], q), dot(ModelToWorld[2], q));
}

float3 ToWorldDir(float3 v)
{
    return float3(dot(ModelToWorld[0].xyz, v), dot(ModelToWorld[1].xyz, v), dot(ModelToWorld[2].xyz, v));
}

// The scene is linear HDR in the Enhanced and Stylized looks (the post pass tonemaps it).
float4 OutputColor(float4 c)
{
    if ((FrameFlags & FRAME_OUTPUT_LINEAR) != 0)
        c.rgb = pow(max(c.rgb, 0.0f), 2.2f);
    return c;
}

VS_OUTPUT VSWorld(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    WorldFace face = WorldFaces[input.Face];
    ProjectToView(input.Position, output);

    output.Color = float4(1.0f, 1.0f, 1.0f, face.Alpha);

    // D3D12PolyCache::AddPolygon (world coordinates)
    output.TexCoord.x = (input.FaceUV.x * face.InvScaleU + face.ShiftU) / face.TextureScale;
    output.TexCoord.y = (input.FaceUV.y * face.InvScaleV + face.ShiftV) / face.TextureScale;
    output.LMCoord.x = (input.FaceUV.x - face.LightShiftU + 8.0f + face.LightOffset.x) / face.LightDiv.x;
    output.LMCoord.y = (input.FaceUV.y - face.LightShiftV + 8.0f + face.LightOffset.y) / face.LightDiv.y;
    output.Textures = uint2(face.BaseTexture, face.LightTexture);
    return output;
}

// World meshes (DRV_MeshVertex): CPU-skinned actors. Texture coordinates and colors are
// used as given, like a transformed poly's; the texture comes from the draw constants.
struct VS_MESH_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;		// 0..255
};

VS_OUTPUT VSMesh(VS_MESH_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    ProjectToView(input.Position, output);
    output.Color = saturate(input.Color * (1.0f / 255.0f));
    output.TexCoord = input.TexCoord;
    output.LMCoord = float2(0.0f, 0.0f);
    output.Textures = uint2(BaseTextureIndex, 0);
    return output;
}

float4 SampleBase(uint index, float2 uv)
{
    bool clampUV = (DrawFlags & FLAG_CLAMP_UV) != 0;
    bool linearFilter = (DrawFlags & FLAG_BILINEAR) != 0;
    if (linearFilter)
        return clampUV ? Textures[index].Sample(LinearClampSampler, uv)
                       : Textures[index].Sample(LinearWrapSampler, uv);
    return clampUV ? Textures[index].Sample(PointClampSampler, uv)
                   : Textures[index].Sample(PointWrapSampler, uv);
}

float4 SampleLight(uint index, float2 uv)
{
    return ((DrawFlags & FLAG_BILINEAR) != 0)
        ? Textures[index].Sample(LinearClampSampler, uv)
        : Textures[index].Sample(PointClampSampler, uv);
}

// Same math as TLPoly.hlsl's PSTexture / PSMultiTexture.
float4 PSWorldTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.Textures.x, input.TexCoord);
    if ((DrawFlags & FLAG_COLORKEY) != 0)
        clip(base.a - 0.5f);

    float alpha = input.Color.a;
    if ((DrawFlags & (FLAG_ALPHA | FLAG_COLORKEY)) != 0)
        alpha *= base.a;
    return OutputColor(saturate(float4(base.rgb * input.Color.rgb, alpha)));
}

float4 PSWorldMultiTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.Textures.x, input.TexCoord);
    if ((DrawFlags & FLAG_COLORKEY) != 0)
        clip(base.a - 0.5f);

    float3 light = SampleLight(input.Textures.y, input.LMCoord).rgb;
    float alpha = input.Color.a;
    if ((DrawFlags & (FLAG_ALPHA | FLAG_COLORKEY)) != 0)
        alpha *= base.a;
    return OutputColor(saturate(float4(base.rgb * light * input.Color.rgb, alpha)));
}


//=====================================================================================
//	PBR world faces (roadmap Phase 2): metallic/roughness GGX (Cook-Torrance) with a
//	Lambert diffuse and Schlick Fresnel, shaded in linear space.
//
//	The lightmap is the diffuse irradiance, as before. It has no direction, so it also
//	lights the specular through a split-sum approximation. The frame's lights (roadmap
//	Phase 3, clustered) add GGX specular and the normal map's diffuse detail; lights the
//	engine left out of the lightmaps (dynamic lights that cast shadows, Enhanced look)
//	add their diffuse light too, shadowed. Normal maps use the DirectX convention (+Y
//	down the image, i.e. along +v). Shading is in world space. In the Classic look the
//	pipeline is gamma space, so the result goes back to gamma 2.2; otherwise it stays
//	linear HDR for the post pass.
//=====================================================================================
struct VS_PBR_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
    float3 Normal   : TEXCOORD3;		// world space
    float4 Tangent  : TEXCOORD4;
    float  Alpha    : TEXCOORD5;
    float3 Color    : COLOR;			// meshes: the CPU lighting (irradiance)
    nointerpolation uint Face : FACEINDEX;
    float4 ClipA    : SV_ClipDistance0;
    float4 ClipB    : SV_ClipDistance1;
};

VS_PBR_OUTPUT VSWorldPBR(VS_INPUT input)
{
    VS_OUTPUT world = VSWorld(input);
    VS_PBR_OUTPUT output;
    output.Position = world.Position;
    output.TexCoord = world.TexCoord;
    output.LMCoord = world.LMCoord;
    output.WorldPos = ToWorld(input.Position);
    output.Normal = ToWorldDir(input.Normal);
    output.Tangent = float4(ToWorldDir(input.Tangent.xyz), input.Tangent.w);
    output.Alpha = world.Color.a;
    output.Color = float3(1.0f, 1.0f, 1.0f);
    output.Face = input.Face;
    output.ClipA = world.ClipA;
    output.ClipB = world.ClipB;
    return output;
}

// PBR world meshes (DRV_MeshVertexPBR): CPU-skinned actors. The material is face 0 of a
// one-entry table, and the vertex color (the engine's lighting) is the irradiance.
struct VS_MESH_PBR_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;		// 0..255
    float3 Normal   : NORMAL;
    float4 Tangent  : TANGENT;
};

VS_PBR_OUTPUT VSMeshPBR(VS_MESH_PBR_INPUT input)
{
    VS_OUTPUT world = (VS_OUTPUT)0;
    ProjectToView(input.Position, world);
    VS_PBR_OUTPUT output;
    output.Position = world.Position;
    output.TexCoord = input.TexCoord;
    output.LMCoord = float2(0.0f, 0.0f);
    output.WorldPos = ToWorld(input.Position);
    output.Normal = ToWorldDir(input.Normal);
    output.Tangent = float4(ToWorldDir(input.Tangent.xyz), input.Tangent.w);
    output.Alpha = saturate(input.Color.a * (1.0f / 255.0f));
    output.Color = saturate(input.Color.rgb * (1.0f / 255.0f));
    output.Face = 0;
    output.ClipA = world.ClipA;
    output.ClipB = world.ClipB;
    return output;
}

static const float PI = 3.14159265f;

// A pure 2.2 power curve rather than the piecewise sRGB one: it keeps products exact
// (pow(a, 2.2) * pow(b, 2.2) = pow(a * b, 2.2)), so the base times the lightmap gives the
// same result as the gamma-space path, even in the dark scenes typical of old levels.
float3 SrgbToLinear(float3 c)
{
    return pow(max(c, 0.0f), 2.2f);
}

float3 LinearToSrgb(float3 c)
{
    return pow(saturate(c), 1.0f / 2.2f);
}

float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// Height-correlated Smith visibility, V = G / (4 NoL NoV).
float V_SmithGGX(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0f - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0f - a2) + a2);
    return 0.5f / max(GGXV + GGXL, 1e-5f);
}

float3 F_Schlick(float3 F0, float VoH)
{
    return F0 + (1.0f - F0) * pow(1.0f - VoH, 5.0f);
}

// Analytic fit of the split-sum environment BRDF (Karis, "Physically Based Shading on Mobile").
float3 EnvBRDFApprox(float3 F0, float roughness, float NoV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NoV)) * r.x + r.y;
    float2 AB = float2(-1.04f, 1.04f) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}

float4 SampleMaterial(uint index, float2 uv, bool retro)
{
    return retro ? Textures[index].Sample(PointWrapSampler, uv)
                 : Textures[index].Sample(LinearWrapSampler, uv);
}

// Parallax occlusion mapping: marches the view ray through the height field (1 = the
// surface, 0 = HeightScale deep) in tangent space and returns where it enters it.
float2 ParallaxUV(uint heightTexture, float2 uv, float3 viewTS, float scale, float2 dx, float2 dy)
{
    float steps = lerp(32.0f, 8.0f, saturate(viewTS.z));
    float layer = 1.0f / steps;
    float2 delta = -viewTS.xy / max(viewTS.z, 0.1f) * (scale / steps);
    float depth = 0.0f;
    float2 p = uv;
    float surface = 1.0f - Textures[heightTexture].SampleGrad(LinearWrapSampler, p, dx, dy).r;
    [loop] for (uint i = 0; i < 32 && depth < surface; i++)
    {
        p += delta;
        depth += layer;
        surface = 1.0f - Textures[heightTexture].SampleGrad(LinearWrapSampler, p, dx, dy).r;
    }
    // Interpolate between the last two steps
    float2 prev = p - delta;
    float after = surface - depth;
    float before = (1.0f - Textures[heightTexture].SampleGrad(LinearWrapSampler, prev, dx, dy).r) - (depth - layer);
    float w = (abs(after - before) > 1e-5f) ? saturate(after / (after - before)) : 0.0f;
    return lerp(p, prev, w);
}

// Filtered shadow lookup (3x3 PCF) of one shadow view; 1 = lit.
float ShadowFactor(uint viewIndex, float3 P, float3 N, float dist)
{
    ShadowView view = ShadowViews[viewIndex];
    // Normal offset against acne, one and a half texels
    float texel = (view.Params.z > 0.5f) ? view.Params.y : view.Params.y * dist;
    float4 q = float4(P + N * texel * 1.5f, 1.0f);
    float4 c = float4(dot(view.Rows[0], q), dot(view.Rows[1], q), dot(view.Rows[2], q), dot(view.Rows[3], q));
    if (c.w <= 0.0f)
        return 1.0f;
    float3 ndc = c.xyz / c.w;
    if (any(abs(ndc.xy) > 1.0f) || ndc.z > 1.0f)
        return 1.0f;
    float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
    float2 tileTexels = view.Rect.zw / ShadowAtlasTexel;
    uv = clamp(uv, 1.5f / tileTexels, 1.0f - 1.5f / tileTexels);
    float2 atlas = view.Rect.xy + uv * view.Rect.zw;
    float sum = 0.0f;
    [unroll] for (int y = -1; y <= 1; y++)
        [unroll] for (int x = -1; x <= 1; x++)
            sum += Textures[ShadowAtlasIndex].SampleCmpLevelZero(ShadowSampler, atlas + float2(x, y) * ShadowAtlasTexel, ndc.z);
    return sum * (1.0f / 9.0f);
}

// The shadow view a light uses at P, and its factor.
float LightShadow(Light light, float3 P, float3 N, float dist, float cameraZ)
{
    uint type = light.Flags & LIGHT_TYPE_MASK;
    uint view = light.ShadowView;
    if (type == LIGHT_DIRECTIONAL)
    {
        // Cascades by the main camera's depth
        [unroll] for (uint c = 0; c < 4; c++)
        {
            if (cameraZ < ShadowViews[view + c].Params.x || c == 3)
            {
                if (cameraZ > ShadowViews[view + c].Params.x)
                    return 1.0f;
                return ShadowFactor(view + c, P, N, 1.0f);
            }
        }
        return 1.0f;
    }
    if (type == LIGHT_POINT)
    {
        // The cube face of the major axis: +X, -X, +Y, -Y, +Z, -Z
        float3 d = P - light.Pos;
        float3 a = abs(d);
        uint face = (a.x >= a.y && a.x >= a.z) ? (d.x > 0.0f ? 0u : 1u)
                  : (a.y >= a.z) ? (d.y > 0.0f ? 2u : 3u)
                  : (d.z > 0.0f ? 4u : 5u);
        view += face;
    }
    return ShadowFactor(view, P, N, dist);
}

// Shading state shared by the light loop.
struct Surface
{
    float3 P;
    float3 N;					// shading normal (normal map)
    float3 Nface;				// geometric normal
    float3 V;
    float  NoV;
    float  a;					// roughness squared
    float3 diffuseColor;
    float3 F0;
    bool   lightmapped;			// MAT_LIGHTMAP
    bool   vertexLit;			// MAT_VERTEX_LIGHT: the engine's actor lighting (dynamic lights included)
    bool   unlit;				// a fullbright face of a material without PBR data: no lights
    float  bakedLum;			// the baked lighting, 0..1 lightmap units
    float  cameraZ;				// depth from the main camera
};

float3 ShadeLight(Light light, Surface s)
{
    uint type = light.Flags & LIGHT_TYPE_MASK;
    float3 L;
    float dist = 1.0f;
    float3 baseRadiance;
    if (type == LIGHT_DIRECTIONAL)
    {
        L = -light.Dir;
        baseRadiance = light.Color;
    }
    else
    {
        L = light.Pos - s.P;
        dist = length(L);
        float falloff = light.Radius - dist;
        if (falloff <= 0.0f || dist < 1e-4f)
            return 0.0f;
        L /= dist;
        // The engine's light model (lightmaps, CombineDLightWithRGBMap): Color * (Radius - d)
        // * N.L, in 0..255 lightmap units.
        baseRadiance = light.Color * (falloff / 255.0f);
        if (type == LIGHT_SPOT)
            baseRadiance *= smoothstep(light.CosOuter, light.CosInner, dot(-L, light.Dir));
    }
    baseRadiance = min(baseRadiance, MaxRadiance);
    float NoL = saturate(dot(s.N, L));
    float NoLflat = saturate(dot(s.Nface, L));
    if (NoLflat <= 0.0f && NoL <= 0.0f)
        return 0.0f;
    float3 radiance = SrgbToLinear(baseRadiance);
    bool baked = s.lightmapped || s.vertexLit;
    if (s.unlit)
        return 0.0f;
    // Is this light's diffuse light already in the baked lighting?
    bool inBake;
    if ((light.Flags & LIGHT_STATIC) != 0)
    {
        // Static: baked with shadows. Unlit (fullbright) faces ignore it. Where the baking
        // holds less than this light alone would give, it was blocked: fade its detail
        // and specular to match.
        if (!baked)
            return 0.0f;
        float expected = max(baseRadiance.r, max(baseRadiance.g, baseRadiance.b)) * NoLflat;
        float visibility = (expected > 1e-3f) ? saturate(s.bakedLum / expected) : 0.0f;
        visibility *= visibility;
        if (visibility <= 0.0f)
            return 0.0f;
        radiance *= visibility;
        inBake = true;
    }
    else
    {
        // Dynamic: in the lightmap unless the engine left it out for the GPU (it casts
        // shadows, or it is directional); the actor lighting always has it.
        inBake = s.vertexLit || (s.lightmapped && (light.Flags & LIGHT_CAST_SHADOWS) == 0 && type != LIGHT_DIRECTIONAL);
        if (light.ShadowView != 0xFFFFFFFFu)
        {
            float shadow = LightShadow(light, s.P, s.Nface, dist, s.cameraZ);
            if (shadow <= 0.0f)
                return 0.0f;
            radiance *= shadow;
        }
    }

    float3 color = 0.0f;
    if (inBake)
        color += s.diffuseColor * (NoL - NoLflat) * radiance;	// the normal map's change
    else
        color += s.diffuseColor * NoL * radiance;
    if (NoL > 0.0f)
    {
        float3 H = normalize(s.V + L);
        float NoH = saturate(dot(s.N, H));
        float VoH = saturate(dot(s.V, H));
        float3 spec = D_GGX(NoH, s.a) * V_SmithGGX(s.NoV, NoL, s.a) * F_Schlick(s.F0, VoH);
        color += spec * (PI * NoL) * radiance;
    }
    return color;
}

// The froxel of a pixel (SV_Position at the render scale) in the main camera's clusters.
uint ClusterIndex(float2 pixel, float cameraZ)
{
    float2 full = pixel / max(RenderScale, 1e-3f);
    uint x = min(uint(full.x / ClusterTile.x), ClusterDims.x - 1);
    uint y = min(uint(full.y / ClusterTile.y), ClusterDims.y - 1);
    uint z = (cameraZ <= ClusterZNear) ? 0u : min(uint(log(cameraZ / ClusterZNear) * ClusterZLogScale), ClusterDims.z - 1);
    return (z * ClusterDims.y + y) * ClusterDims.x + x;
}

float4 PSWorldPBR(VS_PBR_OUTPUT input) : SV_TARGET
{
    WorldFace face = WorldFaces[input.Face];
    bool retro = (face.MaterialFlags & MAT_RETRO) != 0;

    float2 uv = input.TexCoord;
    float2 lmuv = input.LMCoord;
    float3 P = input.WorldPos;

    // Derivatives outside any branch: faces with different flags can share a quad.
    float2 uvDX = ddx(uv), uvDY = ddy(uv);
    float2 lmDX = ddx(lmuv), lmDY = ddy(lmuv);
    float3 pDX = ddx(P), pDY = ddy(P);

    if (retro)
    {
        // Texel-snapped shading: move the shading point to the base texel's center.
        float2 size;
        Textures[face.BaseTexture].GetDimensions(size.x, size.y);
        float2 snapped = (floor(uv * size) + 0.5f) / size;
        float2 duv = snapped - uv;
        float det = uvDX.x * uvDY.y - uvDX.y * uvDY.x;
        if (abs(det) > 1e-12f)
        {
            float2 s = float2(duv.x * uvDY.y - duv.y * uvDY.x, uvDX.x * duv.y - uvDX.y * duv.x) / det;
            P += pDX * s.x + pDY * s.y;
            lmuv += lmDX * s.x + lmDY * s.y;
        }
        uv = snapped;
    }
    else if ((FrameFlags & FRAME_SNAP_LIGHTING) != 0 && (face.MaterialFlags & MAT_LIGHTMAP) != 0)
    {
        // Stylized: dynamic lights shade each lightmap texel (16 texture units) as one, the
        // way the engine's CPU dynamic lights do; the baked light stays filtered.
        float2 texels = face.LightDiv / 16.0f;
        float2 snapped = (floor(lmuv * texels) + 0.5f) / texels;
        float2 dlm = snapped - lmuv;
        float det = lmDX.x * lmDY.y - lmDX.y * lmDY.x;
        if (abs(det) > 1e-12f)
        {
            float2 s = float2(dlm.x * lmDY.y - dlm.y * lmDY.x, lmDX.x * dlm.y - lmDX.y * dlm.x) / det;
            P += pDX * s.x + pDY * s.y;
        }
    }

    if ((face.MaterialFlags & MAT_HEIGHT) != 0)
    {
        float3 n = normalize(input.Normal);
        float3 t = input.Tangent.xyz - n * dot(n, input.Tangent.xyz);
        t = (dot(t, t) > 1e-12f) ? normalize(t) : float3(1.0f, 0.0f, 0.0f);
        float3 b = cross(n, t) * input.Tangent.w;
        float3 v = normalize(EyePos.xyz - P);
        float3 viewTS = float3(dot(v, t), dot(v, b), dot(v, n));
        if (viewTS.z > 0.0f)
            uv = ParallaxUV(face.HeightTexture, uv, viewTS, face.HeightScale, uvDX, uvDY);
    }

    float4 base = retro ? Textures[face.BaseTexture].Sample(PointWrapSampler, uv)
                        : SampleBase(face.BaseTexture, uv);
    if ((DrawFlags & FLAG_COLORKEY) != 0)
        clip(base.a - 0.5f);
    float alpha = input.Alpha;
    if ((DrawFlags & (FLAG_ALPHA | FLAG_COLORKEY)) != 0 || (face.MaterialFlags & MAT_CUTOUT) != 0)
        alpha *= base.a * face.BaseColor.a;
    if ((face.MaterialFlags & MAT_CUTOUT) != 0)
        clip(alpha - face.AlphaCutoff);

    // Tangent frame (Gram-Schmidt: texture vectors need not be orthogonal).
    float3 Nface = normalize(input.Normal);
    float3 T = input.Tangent.xyz - Nface * dot(Nface, input.Tangent.xyz);
    T = (dot(T, T) > 1e-12f) ? normalize(T) : float3(1.0f, 0.0f, 0.0f);
    float3 B = cross(Nface, T) * input.Tangent.w;
    float3 N = Nface;
    if ((face.MaterialFlags & MAT_NORMAL) != 0)
    {
        float2 nxy = SampleMaterial(face.NormalTexture, uv, retro).rg * 2.0f - 1.0f;
        float nz = sqrt(saturate(1.0f - dot(nxy, nxy)));
        N = normalize(T * nxy.x + B * nxy.y + Nface * nz);
    }

    float3 V = normalize(EyePos.xyz - P);
    if ((face.MaterialFlags & MAT_TWO_SIDED) != 0 && dot(Nface, V) < 0.0f)
    {
        N = -N;
        Nface = -Nface;
    }
    float NoV = max(dot(N, V), 1e-4f);

    float3 orm = ((face.MaterialFlags & MAT_ORM) != 0) ? SampleMaterial(face.OrmTexture, uv, retro).rgb : float3(1.0f, 1.0f, 1.0f);
    float ao = orm.r;
    float roughness = clamp(orm.g * face.Roughness, 0.045f, 1.0f);
    float metal = saturate(orm.b * face.Metal);
    float a = roughness * roughness;

    float3 albedo = SrgbToLinear(base.rgb) * face.BaseColor.rgb;
    float3 diffuseColor = albedo * (1.0f - metal);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metal);

    // Without a lightmap the face is fullbright, like PSWorldTexture.
    Surface s;
    s.P = P;
    s.N = N;
    s.Nface = Nface;
    s.V = V;
    s.NoV = NoV;
    s.a = a;
    s.diffuseColor = diffuseColor;
    s.F0 = F0;
    s.lightmapped = (face.MaterialFlags & MAT_LIGHTMAP) != 0;
    s.vertexLit = !s.lightmapped && (face.MaterialFlags & MAT_VERTEX_LIGHT) != 0;
    s.unlit = !s.lightmapped && !s.vertexLit && (face.MaterialFlags & MAT_LEGACY) != 0;
    s.bakedLum = 0.0f;
    s.cameraZ = -(dot(MainWorldToCamera[2].xyz, P) + MainWorldToCamera[2].w);

    float3 E = float3(1.0f, 1.0f, 1.0f);
    if (s.lightmapped)
    {
        E = retro ? Textures[face.LightTexture].Sample(PointClampSampler, lmuv).rgb
                  : SampleLight(face.LightTexture, lmuv).rgb;
        s.bakedLum = max(E.r, max(E.g, E.b));
        E = SrgbToLinear(E);
    }
    else if (s.vertexLit)
    {
        s.bakedLum = max(input.Color.r, max(input.Color.g, input.Color.b));
        E = SrgbToLinear(input.Color);
    }
    float specAO = saturate(pow(NoV + ao, exp2(-16.0f * roughness - 1.0f)) - 1.0f + ao);
    float3 color = diffuseColor * E * ao + E * EnvBRDFApprox(F0, roughness, NoV) * specAO;

    // Directional lights reach everywhere
    [loop] for (uint d = 0; d < NumDirLights; d++)
        color += ShadeLight(FrameLights[d], s);

    // Point and spot lights: the pixel's cluster, or (other cameras) every light
    if (UseClusters != 0 && (FrameFlags & FRAME_CLUSTERS) != 0)
    {
        uint cluster = ClusterIndex(input.Position.xy, s.cameraZ);
        uint count = ClusterCounts[cluster];
        uint first = cluster * ClusterDims.w;
        [loop] for (uint i = 0; i < count; i++)
            color += ShadeLight(FrameLights[ClusterItems[first + i]], s);
    }
    else
    {
        uint last = min(NumLights, NumDirLights + ClusterDims.w);
        [loop] for (uint i = NumDirLights; i < last; i++)
            color += ShadeLight(FrameLights[i], s);
    }

    float3 emissive = face.Emissive;
    if ((face.MaterialFlags & MAT_EMISSIVE) != 0)
        emissive *= SrgbToLinear(SampleMaterial(face.EmissiveTexture, uv, retro).rgb);
    color += emissive;

    if ((FrameFlags & FRAME_OUTPUT_LINEAR) != 0)
        return float4(max(color, 0.0f), saturate(alpha));
    return float4(LinearToSrgb(max(color, 0.0f)), saturate(alpha));
}
