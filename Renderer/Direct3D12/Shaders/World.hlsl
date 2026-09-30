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
    uint   NumClipPlanes;		// > 0: nested view, clip to ClipPlanes instead of the rect
    float4 ClipPlanes[8];		// camera space, inside when dot(xyz, p) + w >= 0
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
    float Alpha;				// 0..1
    float LightShiftU;			// lightmap StartU/StartV
    float LightShiftV;
    float2 LightOffset;			// the lightmap's origin in its texture (an atlas page), * 16
    float2 LightDiv;			// that texture's size * 16
    uint2 FacePadding;
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
    output.Position.x = ZScale * ((cx * ProjScale) * sx + Z * ((XCenter + 0.5f) * sx - 1.0f));
    output.Position.y = ZScale * ((cy * ProjScale) * sy + Z * (1.0f - (YCenter + 0.5f) * sy));
    output.Position.z = w - 1.0f;		// depth 1 - 1/w, as VSMain for w >= 1
    output.Position.w = w;
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

VS_OUTPUT VSWorld(VS_INPUT input)
{
    VS_OUTPUT output;
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
    VS_OUTPUT output;
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
