//=====================================================================================
//	Present.hlsl
//
//	Final pass: copies the HDR scene target to the swap-chain back buffer. For now it
//	is a pass-through (the scene still holds 0..1 values), so levels look exactly as
//	they did when the engine drew straight to the back buffer. Tonemapping and the
//	Look Profiles (roadmap Phase 3) go here.
//
//	Same build rules as TLPoly.hlsl: DXC SM 6.0 at build time, embedded source for
//	the SM 5.0 run-time fallback, no #includes.
//=====================================================================================

Texture2D<float4> SceneColor : register(t0);

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

// One triangle that covers the whole viewport; no vertex buffer needed.
VS_OUTPUT VSFullscreen(uint VertexId : SV_VertexID)
{
    VS_OUTPUT output;
    float2 uv = float2((VertexId << 1) & 2, VertexId & 2);
    output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PSPresent(VS_OUTPUT input) : SV_TARGET
{
    // The scene and back buffer are the same size, so read texels 1:1.
    float4 color = SceneColor.Load(int3(input.Position.xy, 0));
    return float4(saturate(color.rgb), 1.0f);
}
