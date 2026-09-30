//=====================================================================================
//	Shadow.hlsl
//
//	Shadow maps (roadmap Phase 3, D3D12Lighting.cpp): depth only, into one view of
//	the shadow atlas. Casters are the world geometry (model space, placed by
//	ModelToWorld) and the frame's world meshes (world space).
//
//	Same build rules as TLPoly.hlsl (DXC SM 6.0, embedded source for the SM 5.1
//	fallback, no #includes).
//=====================================================================================

cbuffer ShadowDraw : register(b0)
{
    float4 ModelToWorld[3];		// rows: rotation, w = translation
    uint   ViewIndex;
    uint3  DrawPadding;
};

struct ShadowView
{
    float4 Rows[4];				// world to clip
    float4 Rect;
    float4 Params;
};
StructuredBuffer<ShadowView> ShadowViews : register(t0);

float4 VSShadow(float3 Position : POSITION) : SV_POSITION
{
    float4 p = float4(Position, 1.0f);
    float4 w = float4(dot(ModelToWorld[0], p), dot(ModelToWorld[1], p), dot(ModelToWorld[2], p), 1.0f);
    ShadowView view = ShadowViews[ViewIndex];
    return float4(dot(view.Rows[0], w), dot(view.Rows[1], w), dot(view.Rows[2], w), dot(view.Rows[3], w));
}
