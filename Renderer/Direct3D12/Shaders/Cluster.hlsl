//=====================================================================================
//	Cluster.hlsl
//
//	Light clusters (roadmap Phase 3, D3D12Lighting.cpp): bins the frame's point and
//	spot lights into the main camera's froxel grid. Each thread takes one cluster
//	(a screen tile times an exponential depth slice), finds its box in camera space
//	and keeps the lights whose sphere touches it. PSWorldPBR (World.hlsl) reads the
//	result for its pixel's cluster.
//
//	Same build rules as TLPoly.hlsl (DXC SM 6.0, embedded source for the SM 5.1
//	fallback, no #includes).
//=====================================================================================

cbuffer ClusterConstants : register(b0)
{
    float4 WorldToCamera[3];	// the main camera
    float  Scale;				// its screen projection (grCamera)
    float  XCenter;
    float  YCenter;
    float  ZNear;
    float2 Tile;				// cluster size in back buffer pixels
    float  ZLogScale;			// slice = log(Z / ZNear) * ZLogScale
    uint   NumLights;
    uint   NumDirLights;		// the first lights are directional: not binned
    uint   MaxPerCluster;
    uint2  ClusterPadding;
    uint4  Dims;
};

struct Light
{
    float3 Pos;
    float  Radius;
    float3 Color;
    uint   Flags;
    float3 Dir;
    float  CosOuter;
    float  CosInner;
    uint   ShadowView;
    uint2  LightPadding;
};
StructuredBuffer<Light> Lights : register(t0);
RWStructuredBuffer<uint> Counts : register(u0);
RWStructuredBuffer<uint> Items : register(u1);

// Camera space: x right, y up, the camera looks down -z; Z = -z is the depth.
float3 CameraPoint(float sx, float sy, float Z)
{
    return float3((sx - XCenter) * Z / Scale, -(sy - YCenter) * Z / Scale, Z);
}

[numthreads(64, 1, 1)]
void CSClusterLights(uint3 id : SV_DispatchThreadID)
{
    uint cluster = id.x;
    if (cluster >= Dims.x * Dims.y * Dims.z)
        return;
    uint cx = cluster % Dims.x;
    uint cy = (cluster / Dims.x) % Dims.y;
    uint cz = cluster / (Dims.x * Dims.y);

    // Slice 0 starts at the eye; the last one reaches far past the far plane
    float z0 = (cz == 0) ? 0.0f : ZNear * exp(cz / ZLogScale);
    float z1 = (cz + 1 == Dims.z) ? 1e7f : ZNear * exp((cz + 1) / ZLogScale);
    float2 s0 = float2(cx, cy) * Tile - 0.5f;
    float2 s1 = s0 + Tile + 1.0f;

    float3 lo = float3(1e30f, 1e30f, z0);
    float3 hi = float3(-1e30f, -1e30f, z1);
    [unroll] for (uint c = 0; c < 8; c++)
    {
        float3 p = CameraPoint((c & 1) ? s1.x : s0.x, (c & 2) ? s1.y : s0.y, (c & 4) ? z1 : z0);
        lo.xy = min(lo.xy, p.xy);
        hi.xy = max(hi.xy, p.xy);
    }

    uint n = 0;
    uint first = cluster * MaxPerCluster;
    [loop] for (uint i = NumDirLights; i < NumLights; i++)
    {
        Light light = Lights[i];
        float4 w = float4(light.Pos, 1.0f);
        float3 p = float3(dot(WorldToCamera[0], w), dot(WorldToCamera[1], w), -dot(WorldToCamera[2], w));
        float3 d = max(max(lo - p, 0.0f), p - hi);
        if (dot(d, d) <= light.Radius * light.Radius)
        {
            if (n < MaxPerCluster)
                Items[first + n] = i;
            n++;
        }
    }
    Counts[cluster] = min(n, MaxPerCluster);
}
