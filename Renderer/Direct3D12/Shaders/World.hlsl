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

cbuffer FrameConstants : register(b1)
{
    float2 ViewportSize;
    float2 InvViewportSize;
    uint   FrameNumber;
    float  TimeSeconds;
    float2 FramePadding;
};

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
    float  ViewPadding;
};

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
    float LightShiftU;			// lightmap StartU/StartV
    float LightShiftV;
    float LightScale;			// (1 << Log) << 4 of the lightmap
    float Alpha;				// 0..1
    uint  FacePadding;
};
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
    float4 SideClip : SV_ClipDistance0;		// grFrustum's side planes through the eye
    float  FarClip  : SV_ClipDistance1;
};

VS_OUTPUT VSWorld(VS_INPUT input)
{
    VS_OUTPUT output;
    WorldFace face = WorldFaces[input.Face];
    float3 p = input.Position;

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
    output.Position.x = ZScale * ((cx * ProjScale) * sx + Z * ((XCenter + 0.5f) * sx - 1.0f));
    output.Position.y = ZScale * ((cy * ProjScale) * sy + Z * (1.0f - (YCenter + 0.5f) * sy));
    output.Position.z = w - 1.0f;		// depth 1 - 1/w, as VSMain for w >= 1
    output.Position.w = w;
    // The camera rect may be smaller than the viewport, so clip to the camera's own
    // frustum like the CPU path does: screen x within XCenter +/- HalfWidth is
    // cx*Scale/Z within +/- HalfWidth, i.e. (+/-cx)*Scale + HalfWidth*Z >= 0.
    output.SideClip = float4(cx * ProjScale + HalfWidth * Z,
                             HalfWidth * Z - cx * ProjScale,
                             HalfHeight * Z - cy * ProjScale,
                             cy * ProjScale + HalfHeight * Z);
    output.FarClip = (ZFar > 0.0f) ? (ZFar - Z) : 1.0f;

    output.Color = float4(1.0f, 1.0f, 1.0f, face.Alpha);

    // D3D12PolyCache::AddPolygon (world coordinates)
    output.TexCoord.x = (input.FaceUV.x * face.InvScaleU + face.ShiftU) / face.TextureScale;
    output.TexCoord.y = (input.FaceUV.y * face.InvScaleV + face.ShiftV) / face.TextureScale;
    output.LMCoord.x = (input.FaceUV.x - face.LightShiftU + 8.0f) / face.LightScale;
    output.LMCoord.y = (input.FaceUV.y - face.LightShiftV + 8.0f) / face.LightScale;
    output.Textures = uint2(face.BaseTexture, face.LightTexture);
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
    return saturate(float4(base.rgb * input.Color.rgb, alpha));
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
    return saturate(float4(base.rgb * light * input.Color.rgb, alpha));
}
