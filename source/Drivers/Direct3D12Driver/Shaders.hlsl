/****************************************************************************************/
/*  SHADERS.HLSL                                                                        */
/*                                                                                      */
/*  Basic HLSL Shaders for DirectX 12 Driver                                           */
/*                                                                                      */
/****************************************************************************************/

// Constant buffer for transformation matrices
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj;
    float4 AmbientColor;
    float4 FogColor;
    float  FogStart;
    float  FogEnd;
    float2 Padding;
};

// Texture and sampler
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

//================================================================================
//  Gouraud Shading (Colored Polygons)
//================================================================================

struct VS_GOURAUD_INPUT
{
    float4 Position : POSITION;
    float4 Color    : COLOR;
};

struct VS_GOURAUD_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

VS_GOURAUD_OUTPUT VS_Gouraud(VS_GOURAUD_INPUT input)
{
    VS_GOURAUD_OUTPUT output;
    output.Position = mul(input.Position, WorldViewProj);
    output.Color = input.Color;
    return output;
}

float4 PS_Gouraud(VS_GOURAUD_OUTPUT input) : SV_TARGET
{
    return input.Color;
}

//================================================================================
//  Textured Rendering (Single Texture)
//================================================================================

struct VS_TEXTURE_INPUT
{
    float4 Position : POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct VS_TEXTURE_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
};

VS_TEXTURE_OUTPUT VS_Texture(VS_TEXTURE_INPUT input)
{
    VS_TEXTURE_OUTPUT output;
    output.Position = mul(input.Position, WorldViewProj);
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    return output;
}

float4 PS_Texture(VS_TEXTURE_OUTPUT input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.TexCoord);
    return texColor * input.Color;
}

//================================================================================
//  Multi-Textured Rendering (Texture + Lightmap)
//================================================================================

Texture2D g_Lightmap : register(t1);

struct VS_MULTITEX_INPUT
{
    float4 Position  : POSITION;
    float4 Color     : COLOR;
    float2 TexCoord  : TEXCOORD0;
    float2 LMCoord   : TEXCOORD1;
};

struct VS_MULTITEX_OUTPUT
{
    float4 Position  : SV_POSITION;
    float4 Color     : COLOR;
    float2 TexCoord  : TEXCOORD0;
    float2 LMCoord   : TEXCOORD1;
};

VS_MULTITEX_OUTPUT VS_MultiTex(VS_MULTITEX_INPUT input)
{
    VS_MULTITEX_OUTPUT output;
    output.Position = mul(input.Position, WorldViewProj);
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    output.LMCoord = input.LMCoord;
    return output;
}

float4 PS_MultiTex(VS_MULTITEX_OUTPUT input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.TexCoord);
    float4 lmColor = g_Lightmap.Sample(g_Sampler, input.LMCoord);
    return texColor * lmColor * input.Color;
}
