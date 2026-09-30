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
    // PBR faces only (roadmap Phase 2): the eye and WorldGeometry_SetLights' lights, model space.
    float4 EyePos;
    uint   NumLights;
    uint3  LightPadding;
    float4 LightPosRadius[8];
    float4 LightColor[8];		// 0..1 * brightness
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
    // PBR material (PSWorldPBR only)
    uint  NormalTexture;
    uint  OrmTexture;
    uint  EmissiveTexture;
    uint  MaterialFlags;		// MAT_*
    float4 BaseColor;			// linear tint
    float Roughness;			// times ORM.g
    float Metal;				// times ORM.b
    float AlphaCutoff;
    float MaterialPadding;
    float3 Emissive;			// linear, times the emissive map
    float MaterialPadding2;
};

#define MAT_NORMAL			0x0001u
#define MAT_ORM				0x0002u
#define MAT_EMISSIVE		0x0004u
#define MAT_LIGHTMAP		0x0008u
#define MAT_RETRO			0x0010u
#define MAT_CUTOUT			0x0020u
#define MAT_TWO_SIDED		0x0040u
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


//=====================================================================================
//	PBR world faces (roadmap Phase 2): metallic/roughness GGX (Cook-Torrance) with a
//	Lambert diffuse and Schlick Fresnel, shaded in linear space.
//
//	The lightmap is the diffuse irradiance, as before. It has no direction, so it also
//	lights the specular through a split-sum approximation. The dynamic lights (which
//	the engine has already added to the lightmap on the flat face) add GGX specular and
//	the normal map's diffuse detail. Normal maps use the DirectX convention (+Y down the
//	image, i.e. along +v). The pipeline is gamma space, so the result goes back to gamma 2.2.
//=====================================================================================
struct VS_PBR_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
    float3 ModelPos : TEXCOORD2;
    float3 Normal   : TEXCOORD3;
    float4 Tangent  : TEXCOORD4;
    float  Alpha    : TEXCOORD5;
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
    output.ModelPos = input.Position;
    output.Normal = input.Normal;
    output.Tangent = input.Tangent;
    output.Alpha = world.Color.a;
    output.Face = input.Face;
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

float4 PSWorldPBR(VS_PBR_OUTPUT input) : SV_TARGET
{
    WorldFace face = WorldFaces[input.Face];
    bool retro = (face.MaterialFlags & MAT_RETRO) != 0;

    float2 uv = input.TexCoord;
    float2 lmuv = input.LMCoord;
    float3 P = input.ModelPos;

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
    float3 E = float3(1.0f, 1.0f, 1.0f);
    if ((face.MaterialFlags & MAT_LIGHTMAP) != 0)
    {
        E = retro ? Textures[face.LightTexture].Sample(PointClampSampler, lmuv).rgb
                  : SampleLight(face.LightTexture, lmuv).rgb;
        E = SrgbToLinear(E);
    }
    float specAO = saturate(pow(NoV + ao, exp2(-16.0f * roughness - 1.0f)) - 1.0f + ao);
    float3 color = diffuseColor * E * ao + E * EnvBRDFApprox(F0, roughness, NoV) * specAO;

    [loop] for (uint i = 0; i < NumLights; i++)
    {
        float3 L = LightPosRadius[i].xyz - P;
        float dist = length(L);
        float falloff = LightPosRadius[i].w - dist;
        if (falloff <= 0.0f || dist < 1e-4f)
            continue;
        L /= dist;
        // The engine's dlight model (CombineDLightWithRGBMap): Color * (Radius - d) * NoL,
        // in 0..255 lightmap units.
        float3 radiance = SrgbToLinear(saturate(LightColor[i].rgb * (falloff / 255.0f)));
        float NoL = saturate(dot(N, L));
        float NoLflat = saturate(dot(Nface, L));
        // The lightmap already holds this light on the flat face: add the normal map's change.
        if ((face.MaterialFlags & MAT_LIGHTMAP) != 0)
            color += diffuseColor * (NoL - NoLflat) * radiance;
        else
            color += diffuseColor * NoL * radiance;
        if (NoL > 0.0f)
        {
            float3 H = normalize(V + L);
            float NoH = saturate(dot(N, H));
            float VoH = saturate(dot(V, H));
            float3 spec = D_GGX(NoH, a) * V_SmithGGX(NoV, NoL, a) * F_Schlick(F0, VoH);
            color += spec * (PI * NoL) * radiance;
        }
    }

    float3 emissive = face.Emissive;
    if ((face.MaterialFlags & MAT_EMISSIVE) != 0)
        emissive *= SrgbToLinear(SampleMaterial(face.EmissiveTexture, uv, retro).rgb);
    color += emissive;

    return float4(LinearToSrgb(max(color, 0.0f)), saturate(alpha));
}
