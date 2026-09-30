/****************************************************************************************/
/*  D3D12LIGHTING.CPP                                                                   */
/*                                                                                      */
/*  Frame lighting: light clusters and shadow maps (see D3D12Lighting.h).               */
/****************************************************************************************/
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "D3D12Lighting.h"
#include "D3D12PSOManager.h"
#include "D3D12Shaders.h"
#include "D3D12TextureMgr.h"
#include "D3D12UploadRing.h"
#include "D3D12WorldGeometry.h"
#include "D3D12Look.h"
#include "D3D12Config.h"
#include "D3D12Common.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	const UINT		ClusterX = 16;
	const UINT		ClusterY = 9;
	const UINT		ClusterZ = 24;
	const UINT		MaxPerCluster = 64;
	const UINT		NumClusters = ClusterX * ClusterY * ClusterZ;
	const float		ClusterZNear = 8.0f;
	const float		ClusterZFarDefault = 16384.0f;
	const UINT		BruteForceLights = 256;		// lights tested per pixel outside the clusters

	const UINT		AtlasCells = 8;				// the shadow atlas is AtlasCells x AtlasCells cells
	const UINT		MaxShadowViews = AtlasCells * AtlasCells;
	const UINT		NumCascades = 4;
	const uint32	NoShadow = 0xFFFFFFFFu;

	// StructuredBuffer<Light> in World.hlsl and Cluster.hlsl
	struct GpuLight
	{
		float	Pos[3];
		float	Radius;
		float	Color[3];
		uint32	Flags;
		float	Dir[3];
		float	CosOuter;
		float	CosInner;
		uint32	ShadowView;			// first view, NoShadow = none
		uint32	Padding[2];
	};
	static_assert(sizeof(GpuLight) == 64, "GpuLight layout");

	// StructuredBuffer<ShadowView> in World.hlsl and Shadow.hlsl
	struct GpuShadowView
	{
		float	Rows[4][4];			// world to clip
		float	Rect[4];			// atlas uv origin and size
		float	Params[4];			// x: cascade far depth, y: texel size in world units (at distance 1 when z = 0), z: 1 = orthographic
	};
	static_assert(sizeof(GpuShadowView) == 96, "GpuShadowView layout");

	// cbuffer ClusterConstants in Cluster.hlsl
	struct ClusterConstants
	{
		float	WorldToCamera[3][4];
		float	Scale;
		float	XCenter;
		float	YCenter;
		float	ZNear;
		float	Tile[2];
		float	ZLogScale;
		uint32	NumLights;
		uint32	NumDirLights;
		uint32	MaxPerCluster;
		uint32	Padding[2];
		uint32	Dims[4];
	};

	struct V3
	{
		float x, y, z;
	};
	V3 Make(float x, float y, float z) { V3 v = { x, y, z }; return v; }
	V3 Make(const float* p) { return Make(p[0], p[1], p[2]); }
	V3 Add(const V3& a, const V3& b) { return Make(a.x + b.x, a.y + b.y, a.z + b.z); }
	V3 Sub(const V3& a, const V3& b) { return Make(a.x - b.x, a.y - b.y, a.z - b.z); }
	V3 Mul(const V3& a, float s) { return Make(a.x * s, a.y * s, a.z * s); }
	float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	V3 Cross(const V3& a, const V3& b) { return Make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
	float Length(const V3& a) { return std::sqrt(Dot(a, a)); }
	V3 Normalize(const V3& a)
	{
		const float l = Length(a);
		return (l > 1e-8f) ? Mul(a, 1.0f / l) : Make(0.0f, -1.0f, 0.0f);
	}

	// Right-handed-free basis: x right, y up, z = Forward (like a D3D view).
	void Basis(const V3& Forward, V3& Right, V3& Up)
	{
		const V3 WorldUp = (std::fabs(Forward.y) < 0.99f) ? Make(0.0f, 1.0f, 0.0f) : Make(0.0f, 0.0f, 1.0f);
		Right = Normalize(Cross(WorldUp, Forward));
		Up = Cross(Forward, Right);
	}

	void SetRow(float Row[4], const V3& v, float w)
	{
		Row[0] = v.x;
		Row[1] = v.y;
		Row[2] = v.z;
		Row[3] = w;
	}

	// Perspective view from Eye along Forward; depth 0 at Near, 1 at Far.
	void PerspectiveRows(const V3& Eye, const V3& Forward, float TanHalf, float Near, float Far, float Rows[4][4])
	{
		V3 Right, Up;
		Basis(Forward, Right, Up);
		const float A = Far / (Far - Near);
		const float B = -Near * Far / (Far - Near);
		SetRow(Rows[0], Mul(Right, 1.0f / TanHalf), -Dot(Right, Eye) / TanHalf);
		SetRow(Rows[1], Mul(Up, 1.0f / TanHalf), -Dot(Up, Eye) / TanHalf);
		SetRow(Rows[2], Mul(Forward, A), -Dot(Forward, Eye) * A + B);
		SetRow(Rows[3], Forward, -Dot(Forward, Eye));
	}

	// Orthographic view centered on Center, HalfSize wide each way; depth covers Back units
	// toward the light and Ahead units past the center.
	void OrthoRows(const V3& Center, const V3& Forward, float HalfSize, float Back, float Ahead, float Rows[4][4])
	{
		V3 Right, Up;
		Basis(Forward, Right, Up);
		const float Depth = Back + Ahead;
		SetRow(Rows[0], Mul(Right, 1.0f / HalfSize), -Dot(Right, Center) / HalfSize);
		SetRow(Rows[1], Mul(Up, 1.0f / HalfSize), -Dot(Up, Center) / HalfSize);
		SetRow(Rows[2], Mul(Forward, 1.0f / Depth), (Back - Dot(Forward, Center)) / Depth);
		SetRow(Rows[3], Make(0.0f, 0.0f, 0.0f), 1.0f);
	}

	// Frame state
	bool							HaveMain = false;
	DRV_WorldView					MainView = {};
	float							MainRows[3][4] = {};
	std::vector<DRV_Light>			FrameLights;
	std::vector<GpuLight>			GpuLights;
	UINT							NumDirLights = 0;
	std::vector<GpuShadowView>		ShadowViews;
	struct ViewLight
	{
		UINT	Light;			// index into GpuLights
		UINT	X, Y, Size;		// atlas pixels
	};
	std::vector<ViewLight>			ShadowViewLights;
	bool							ClustersValid = false;
	D3D12_GPU_VIRTUAL_ADDRESS		LightsGPU = 0;
	D3D12_GPU_VIRTUAL_ADDRESS		ShadowViewsGPU = 0;
	D3D12_GPU_VIRTUAL_ADDRESS		DummyGPU = 0;
	float							ClusterZLogScale = 1.0f;

	// Resources
	bool							Available = false;
	ComPtr<ID3D12Resource>			ClusterCounts;
	ComPtr<ID3D12Resource>			ClusterItems;
	D3D12_RESOURCE_STATES			ClusterState = D3D12_RESOURCE_STATE_COMMON;
	ComPtr<ID3D12RootSignature>		ClusterRootSignature;
	ComPtr<ID3D12PipelineState>		ClusterPSO;

	ComPtr<ID3D12Resource>			Atlas;
	UINT							AtlasSize = 4096;
	D3D12_RESOURCE_STATES			AtlasState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	ComPtr<ID3D12DescriptorHeap>	AtlasDSVHeap;
	ComPtr<ID3D12RootSignature>		ShadowRootSignature;
	ComPtr<ID3D12PipelineState>		ShadowPSO;
	int32							MaxShadowLights = 8;
	float							ShadowDistance = 3000.0f;

	void Transition(ID3D12GraphicsCommandList* CommandList, ID3D12Resource* Resource,
					D3D12_RESOURCE_STATES& State, D3D12_RESOURCE_STATES After)
	{
		if (State == After)
			return;
		TransitionResource(CommandList, Resource, State, After);
		State = After;
	}

	ComPtr<ID3D12Resource> CreateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS Flags)
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Width = Size;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.Flags = Flags;
		ComPtr<ID3D12Resource> Buffer;
		if (FAILED(CreateCommittedResource(g_pDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, &Desc, &Buffer)))
			return nullptr;
		return Buffer;
	}

	ComPtr<ID3D12RootSignature> SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc, const char* Name)
	{
		ComPtr<ID3DBlob> Signature;
		ComPtr<ID3DBlob> Errors;
		ComPtr<ID3D12RootSignature> Result;
		HRESULT Hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Errors);
		if (SUCCEEDED(Hr))
			Hr = g_pDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&Result));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: %s root signature failed (0x%08X): %s", Name, Hr,
				Errors ? static_cast<const char*>(Errors->GetBufferPointer()) : "no details");
			return nullptr;
		}
		return Result;
	}

	bool CreateClusterPipeline()
	{
		D3D12_ROOT_PARAMETER Parameters[4] = {};
		Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		Parameters[0].Descriptor.ShaderRegister = 0;
		Parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		Parameters[1].Descriptor.ShaderRegister = 0;
		Parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		Parameters[2].Descriptor.ShaderRegister = 0;
		Parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		Parameters[3].Descriptor.ShaderRegister = 1;
		for (auto& Parameter : Parameters)
			Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC Desc = {};
		Desc.NumParameters = _countof(Parameters);
		Desc.pParameters = Parameters;
		ClusterRootSignature = SerializeRootSignature(Desc, "Cluster");
		if (!ClusterRootSignature)
			return false;

		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.pRootSignature = ClusterRootSignature.Get();
		PSODesc.CS = D3D12Shaders_Get(SHADER_CLUSTER_CS);
		if (FAILED(g_pDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&ClusterPSO))))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Light cluster pipeline creation failed");
			return false;
		}
		return true;
	}

	bool CreateShadowPipeline()
	{
		D3D12_ROOT_PARAMETER Parameters[2] = {};
		Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		Parameters[0].Constants.ShaderRegister = 0;
		Parameters[0].Constants.Num32BitValues = 16;
		Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		Parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		Parameters[1].Descriptor.ShaderRegister = 0;
		Parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC Desc = {};
		Desc.NumParameters = _countof(Parameters);
		Desc.pParameters = Parameters;
		Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ShadowRootSignature = SerializeRootSignature(Desc, "Shadow");
		if (!ShadowRootSignature)
			return false;

		// Every vertex format the casters use starts with a float3 position.
		D3D12_INPUT_ELEMENT_DESC Layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.pRootSignature = ShadowRootSignature.Get();
		PSODesc.VS = D3D12Shaders_Get(SHADER_SHADOW_VS);
		PSODesc.SampleMask = UINT_MAX;
		PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		PSODesc.RasterizerState.DepthBias = 0;
		PSODesc.RasterizerState.SlopeScaledDepthBias = 1.5f;
		PSODesc.RasterizerState.DepthBiasClamp = 0.0f;
		PSODesc.RasterizerState.DepthClipEnable = TRUE;
		PSODesc.DepthStencilState.DepthEnable = TRUE;
		PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		PSODesc.InputLayout = { Layout, _countof(Layout) };
		PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		PSODesc.NumRenderTargets = 0;
		PSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		PSODesc.SampleDesc.Count = 1;
		if (FAILED(g_pDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&ShadowPSO))))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Shadow map pipeline creation failed");
			return false;
		}
		return true;
	}

	bool CreateAtlas()
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = AtlasSize;
		Desc.Height = AtlasSize;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = DXGI_FORMAT_R32_TYPELESS;
		Desc.SampleDesc.Count = 1;
		Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE Clear = {};
		Clear.Format = DXGI_FORMAT_D32_FLOAT;
		Clear.DepthStencil.Depth = 1.0f;
		if (FAILED(CreateCommittedResource(g_pDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&Desc, &Atlas, &Clear)))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Shadow atlas (%u x %u) creation failed", AtlasSize, AtlasSize);
			return false;
		}
		AtlasState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
		HeapDesc.NumDescriptors = 1;
		HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(g_pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&AtlasDSVHeap))))
			return false;
		D3D12_DEPTH_STENCIL_VIEW_DESC DSV = {};
		DSV.Format = DXGI_FORMAT_D32_FLOAT;
		DSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		g_pDevice->CreateDepthStencilView(Atlas.Get(), &DSV, AtlasDSVHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	// Main camera eye in world space (-R^T t).
	V3 MainEye()
	{
		V3 Eye;
		Eye.x = -(MainRows[0][0] * MainRows[0][3] + MainRows[1][0] * MainRows[1][3] + MainRows[2][0] * MainRows[2][3]);
		Eye.y = -(MainRows[0][1] * MainRows[0][3] + MainRows[1][1] * MainRows[1][3] + MainRows[2][1] * MainRows[2][3]);
		Eye.z = -(MainRows[0][2] * MainRows[0][3] + MainRows[1][2] * MainRows[1][3] + MainRows[2][2] * MainRows[2][3]);
		return Eye;
	}

	// A camera-space point of the main camera, in world space.
	V3 MainCameraToWorld(const V3& Camera)
	{
		const V3 Local = Make(Camera.x - MainRows[0][3], Camera.y - MainRows[1][3], Camera.z - MainRows[2][3]);
		return Make(
			MainRows[0][0] * Local.x + MainRows[1][0] * Local.y + MainRows[2][0] * Local.z,
			MainRows[0][1] * Local.x + MainRows[1][1] * Local.y + MainRows[2][1] * Local.z,
			MainRows[0][2] * Local.x + MainRows[1][2] * Local.y + MainRows[2][2] * Local.z);
	}

	// Shadow atlas cells, allocated per frame.
	struct CellAllocator
	{
		bool Used[AtlasCells][AtlasCells] = {};
		// A free Size x Size block of cells; returns false when there is none.
		bool Allocate(UINT Size, UINT& CellX, UINT& CellY)
		{
			for (UINT y = 0; y + Size <= AtlasCells; y += Size)
			{
				for (UINT x = 0; x + Size <= AtlasCells; x += Size)
				{
					bool Free = true;
					for (UINT j = 0; j < Size && Free; ++j)
						for (UINT i = 0; i < Size && Free; ++i)
							Free = !Used[y + j][x + i];
					if (!Free)
						continue;
					for (UINT j = 0; j < Size; ++j)
						for (UINT i = 0; i < Size; ++i)
							Used[y + j][x + i] = true;
					CellX = x;
					CellY = y;
					return true;
				}
			}
			return false;
		}
		UINT FreeCount() const
		{
			UINT n = 0;
			for (UINT y = 0; y < AtlasCells; ++y)
				for (UINT x = 0; x < AtlasCells; ++x)
					n += Used[y][x] ? 0 : 1;
			return n;
		}
	};

	void AddView(UINT LightIndex, UINT CellX, UINT CellY, UINT CellSpan, const float Rows[4][4], float FarDepth, float TexelWorld, bool Ortho)
	{
		const UINT CellSize = AtlasSize / AtlasCells;
		GpuShadowView View = {};
		std::memcpy(View.Rows, Rows, sizeof(View.Rows));
		View.Rect[0] = static_cast<float>(CellX * CellSize) / AtlasSize;
		View.Rect[1] = static_cast<float>(CellY * CellSize) / AtlasSize;
		View.Rect[2] = View.Rect[3] = static_cast<float>(CellSpan * CellSize) / AtlasSize;
		View.Params[0] = FarDepth;
		View.Params[1] = TexelWorld;
		View.Params[2] = Ortho ? 1.0f : 0.0f;
		ShadowViews.push_back(View);
		ViewLight VL = { LightIndex, CellX * CellSize, CellY * CellSize, CellSpan * CellSize };
		ShadowViewLights.push_back(VL);
	}

	// Directional cascades over the main camera's view out to ShadowDistance.
	bool AddCascades(UINT LightIndex, const V3& Dir, CellAllocator& Cells)
	{
		if (!HaveMain || Cells.FreeCount() < NumCascades * 4)
			return false;
		const float Near = 4.0f;
		const float Far = (MainView.ZFar > 0.0f) ? (std::min)(ShadowDistance, MainView.ZFar) : ShadowDistance;
		const UINT CellSpan = 2;
		const float TileSize = static_cast<float>(CellSpan * (AtlasSize / AtlasCells));
		float SplitNear = Near;
		for (UINT c = 0; c < NumCascades; ++c)
		{
			const float t = static_cast<float>(c + 1) / NumCascades;
			const float SplitFar = 0.75f * Near * std::pow(Far / Near, t) + 0.25f * (Near + (Far - Near) * t);

			// The slice's corners, then a bounding sphere
			V3 Corners[8];
			int n = 0;
			for (int d = 0; d < 2; ++d)
			{
				const float Z = d ? SplitFar : SplitNear;
				for (int sy = -1; sy <= 1; sy += 2)
					for (int sx = -1; sx <= 1; sx += 2)
					{
						const V3 Camera = Make(sx * MainView.HalfWidth * Z / MainView.Scale,
											   sy * MainView.HalfHeight * Z / MainView.Scale, -Z);
						Corners[n++] = MainCameraToWorld(Camera);
					}
			}
			V3 Center = Make(0.0f, 0.0f, 0.0f);
			for (const V3& Corner : Corners)
				Center = Add(Center, Corner);
			Center = Mul(Center, 1.0f / 8.0f);
			float Radius = 0.0f;
			for (const V3& Corner : Corners)
				Radius = (std::max)(Radius, Length(Sub(Corner, Center)));
			Radius = std::ceil(Radius / 16.0f) * 16.0f;		// steadier when the camera turns

			// Move the center in whole texels so the shadow does not crawl
			V3 Right, Up;
			Basis(Dir, Right, Up);
			const float Texel = 2.0f * Radius / TileSize;
			const float cx = Dot(Right, Center), cy = Dot(Up, Center);
			Center = Add(Center, Add(Mul(Right, std::floor(cx / Texel) * Texel - cx), Mul(Up, std::floor(cy / Texel) * Texel - cy)));

			UINT CellX, CellY;
			if (!Cells.Allocate(CellSpan, CellX, CellY))
				return c > 0;
			float Rows[4][4];
			OrthoRows(Center, Dir, Radius, Radius + 4096.0f, Radius, Rows);
			AddView(LightIndex, CellX, CellY, CellSpan, Rows, SplitFar, Texel, true);
			SplitNear = SplitFar;
		}
		return true;
	}

	void BuildFrame()
	{
		GpuLights.clear();
		ShadowViews.clear();
		ShadowViewLights.clear();
		NumDirLights = 0;

		// Directional lights first
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (const DRV_Light& Light : FrameLights)
			{
				const bool Directional = (Light.Flags & DRV_LIGHT_TYPE_MASK) == DRV_LIGHT_DIRECTIONAL;
				if (Directional != (Pass == 0))
					continue;
				GpuLight Out = {};
				std::memcpy(Out.Pos, Light.Pos, sizeof(Out.Pos));
				Out.Radius = Light.Radius;
				std::memcpy(Out.Color, Light.Color, sizeof(Out.Color));
				Out.Flags = Light.Flags;
				const V3 Dir = Normalize(Make(Light.Dir));
				Out.Dir[0] = Dir.x;
				Out.Dir[1] = Dir.y;
				Out.Dir[2] = Dir.z;
				Out.CosOuter = Light.CosOuter;
				Out.CosInner = (std::max)(Light.CosInner, Light.CosOuter + 1e-4f);
				Out.ShadowView = NoShadow;
				GpuLights.push_back(Out);
				if (Directional)
					++NumDirLights;
			}
		}

		// Shadows: dynamic lights that cast them, directional first, then the nearest
		const DRV_LookSettings& Look = D3D12Look_Get();
		if (!Atlas || !ShadowPSO || !D3D12Look_IsHDR() || !Look.Shadows)
			return;
		std::vector<UINT> Candidates;
		for (UINT i = 0; i < GpuLights.size(); ++i)
		{
			if ((GpuLights[i].Flags & (DRV_LIGHT_CAST_SHADOWS | DRV_LIGHT_STATIC)) == DRV_LIGHT_CAST_SHADOWS)
				Candidates.push_back(i);
		}
		if (Candidates.empty())
			return;
		const V3 Eye = HaveMain ? MainEye() : Make(0.0f, 0.0f, 0.0f);
		std::stable_sort(Candidates.begin(), Candidates.end(), [&](UINT a, UINT b)
		{
			const bool DirA = a < NumDirLights, DirB = b < NumDirLights;
			if (DirA != DirB)
				return DirA;
			const float da = Length(Sub(Make(GpuLights[a].Pos), Eye)) - GpuLights[a].Radius;
			const float db = Length(Sub(Make(GpuLights[b].Pos), Eye)) - GpuLights[b].Radius;
			return da < db;
		});

		CellAllocator Cells;
		const float CellSize = static_cast<float>(AtlasSize / AtlasCells);
		int32 NumShadowed = 0;
		for (UINT Index : Candidates)
		{
			if (NumShadowed >= MaxShadowLights)
				break;
			GpuLight& Light = GpuLights[Index];
			const UINT FirstView = static_cast<UINT>(ShadowViews.size());
			const UINT Type = Light.Flags & DRV_LIGHT_TYPE_MASK;
			const V3 Pos = Make(Light.Pos);
			const V3 Dir = Make(Light.Dir);
			bool Ok = false;
			if (Type == DRV_LIGHT_DIRECTIONAL)
			{
				Ok = AddCascades(Index, Dir, Cells);
			}
			else if (Type == DRV_LIGHT_SPOT)
			{
				UINT CellX, CellY;
				if (Cells.Allocate(2, CellX, CellY))
				{
					const float CosOuter = (std::max)(-0.999f, (std::min)(Light.CosOuter, 0.999f));
					const float TanHalf = (std::min)(std::tan(std::acos(CosOuter)) * 1.05f, 20.0f);
					float Rows[4][4];
					PerspectiveRows(Pos, Dir, TanHalf, 1.0f, (std::max)(Light.Radius, 2.0f), Rows);
					AddView(Index, CellX, CellY, 2, Rows, 0.0f, 2.0f * TanHalf / (2.0f * CellSize), false);
					Ok = true;
				}
			}
			else if (Cells.FreeCount() >= 6)
			{
				// Cube faces +X, -X, +Y, -Y, +Z, -Z (World.hlsl picks one by the major axis)
				static const float Axes[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
				const float TanHalf = 1.02f;
				for (int Face = 0; Face < 6; ++Face)
				{
					UINT CellX, CellY;
					Cells.Allocate(1, CellX, CellY);
					float Rows[4][4];
					PerspectiveRows(Pos, Make(Axes[Face]), TanHalf, 1.0f, (std::max)(Light.Radius, 2.0f), Rows);
					AddView(Index, CellX, CellY, 1, Rows, 0.0f, 2.0f * TanHalf / CellSize, false);
				}
				Ok = true;
			}
			if (Ok)
			{
				Light.ShadowView = FirstView;
				++NumShadowed;
			}
		}
	}

	void RenderShadows(ID3D12GraphicsCommandList* CommandList, const std::vector<D3D12ShadowCaster>& Meshes)
	{
		if (ShadowViews.empty())
		{
			Transition(CommandList, Atlas.Get(), AtlasState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			return;
		}

		D3D12BeginMarker(CommandList, "Shadow maps");
		Transition(CommandList, Atlas.Get(), AtlasState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE DSV = AtlasDSVHeap->GetCPUDescriptorHandleForHeapStart();
		CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DSV);

		std::vector<D3D12_RECT> Rects;
		for (const ViewLight& VL : ShadowViewLights)
		{
			D3D12_RECT Rect = { static_cast<LONG>(VL.X), static_cast<LONG>(VL.Y),
								static_cast<LONG>(VL.X + VL.Size), static_cast<LONG>(VL.Y + VL.Size) };
			Rects.push_back(Rect);
		}
		CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, static_cast<UINT>(Rects.size()), Rects.data());

		CommandList->SetGraphicsRootSignature(ShadowRootSignature.Get());
		CommandList->SetPipelineState(ShadowPSO.Get());
		CommandList->SetGraphicsRootShaderResourceView(1, ShadowViewsGPU);
		CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		struct DrawConstants
		{
			float	ModelToWorld[3][4];
			uint32	ViewIndex;
			uint32	Padding[3];
		};
		DrawConstants Identity = {};
		Identity.ModelToWorld[0][0] = Identity.ModelToWorld[1][1] = Identity.ModelToWorld[2][2] = 1.0f;

		for (UINT v = 0; v < ShadowViewLights.size(); ++v)
		{
			const ViewLight& VL = ShadowViewLights[v];
			const GpuLight& Light = GpuLights[VL.Light];
			const bool Directional = (Light.Flags & DRV_LIGHT_TYPE_MASK) == DRV_LIGHT_DIRECTIONAL;

			D3D12_VIEWPORT Viewport = { static_cast<float>(VL.X), static_cast<float>(VL.Y),
										static_cast<float>(VL.Size), static_cast<float>(VL.Size), 0.0f, 1.0f };
			CommandList->RSSetViewports(1, &Viewport);
			CommandList->RSSetScissorRects(1, &Rects[v]);

			D3D12World_ForEachGeometry([&](const D3D12WorldGeometry& Geometry)
			{
				const float (*M)[4] = Geometry.ModelToWorld;
				if (!Directional)
				{
					const V3 c = Make(Geometry.BoundsCenter);
					const V3 World = Make(M[0][0] * c.x + M[0][1] * c.y + M[0][2] * c.z + M[0][3],
										  M[1][0] * c.x + M[1][1] * c.y + M[1][2] * c.z + M[1][3],
										  M[2][0] * c.x + M[2][1] * c.y + M[2][2] * c.z + M[2][3]);
					if (Length(Sub(World, Make(Light.Pos))) > Light.Radius + Geometry.BoundsRadius)
						return;
				}
				DrawConstants Constants = {};
				std::memcpy(Constants.ModelToWorld, M, sizeof(Constants.ModelToWorld));
				Constants.ViewIndex = v;
				CommandList->SetGraphicsRoot32BitConstants(0, 16, &Constants, 0);
				CommandList->IASetVertexBuffers(0, 1, &Geometry.VertexBufferView);
				CommandList->IASetIndexBuffer(&Geometry.IndexBufferView);
				CommandList->DrawIndexedInstanced(static_cast<UINT>(Geometry.Indices.size()), 1, 0, 0, 0);
			});

			Identity.ViewIndex = v;
			CommandList->SetGraphicsRoot32BitConstants(0, 16, &Identity, 0);
			for (const D3D12ShadowCaster& Mesh : Meshes)
			{
				CommandList->IASetVertexBuffers(0, 1, &Mesh.Vertices);
				CommandList->DrawInstanced(Mesh.NumVertices, 1, 0, 0);
			}
		}

		Transition(CommandList, Atlas.Get(), AtlasState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		D3D12EndMarker(CommandList);
	}

	void BuildClusters(ID3D12GraphicsCommandList* CommandList)
	{
		ClustersValid = false;
		if (!HaveMain || !ClusterPSO || GpuLights.size() <= NumDirLights)
			return;

		D3D12UploadAllocation Upload = {};
		if (!D3D12Upload_Allocate(sizeof(ClusterConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &Upload))
			return;
		ClusterConstants Constants = {};
		std::memcpy(Constants.WorldToCamera, MainRows, sizeof(MainRows));
		Constants.Scale = MainView.Scale;
		Constants.XCenter = MainView.XCenter;
		Constants.YCenter = MainView.YCenter;
		Constants.ZNear = ClusterZNear;
		Constants.Tile[0] = std::ceil(static_cast<float>((std::max)(g_nScreenWidth, static_cast<int32>(1))) / ClusterX);
		Constants.Tile[1] = std::ceil(static_cast<float>((std::max)(g_nScreenHeight, static_cast<int32>(1))) / ClusterY);
		Constants.ZLogScale = ClusterZLogScale;
		Constants.NumLights = static_cast<uint32>(GpuLights.size());
		Constants.NumDirLights = NumDirLights;
		Constants.MaxPerCluster = MaxPerCluster;
		Constants.Dims[0] = ClusterX;
		Constants.Dims[1] = ClusterY;
		Constants.Dims[2] = ClusterZ;
		Constants.Dims[3] = MaxPerCluster;
		std::memcpy(Upload.CPU, &Constants, sizeof(Constants));

		D3D12BeginMarker(CommandList, "Light clusters");
		D3D12_RESOURCE_BARRIER Barriers[2] = {};
		UINT NumBarriers = 0;
		if (ClusterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			for (ID3D12Resource* Buffer : { ClusterCounts.Get(), ClusterItems.Get() })
			{
				D3D12_RESOURCE_BARRIER& B = Barriers[NumBarriers++];
				B.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				B.Transition.pResource = Buffer;
				B.Transition.StateBefore = ClusterState;
				B.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				B.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			}
			CommandList->ResourceBarrier(NumBarriers, Barriers);
			ClusterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		CommandList->SetComputeRootSignature(ClusterRootSignature.Get());
		CommandList->SetPipelineState(ClusterPSO.Get());
		CommandList->SetComputeRootConstantBufferView(0, Upload.GPU);
		CommandList->SetComputeRootShaderResourceView(1, LightsGPU);
		CommandList->SetComputeRootUnorderedAccessView(2, ClusterCounts->GetGPUVirtualAddress());
		CommandList->SetComputeRootUnorderedAccessView(3, ClusterItems->GetGPUVirtualAddress());
		CommandList->Dispatch((NumClusters + 63) / 64, 1, 1);

		const D3D12_RESOURCE_STATES Readable = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		NumBarriers = 0;
		for (ID3D12Resource* Buffer : { ClusterCounts.Get(), ClusterItems.Get() })
		{
			D3D12_RESOURCE_BARRIER& B = Barriers[NumBarriers++];
			B = {};
			B.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			B.Transition.pResource = Buffer;
			B.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			B.Transition.StateAfter = Readable;
			B.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		}
		CommandList->ResourceBarrier(NumBarriers, Barriers);
		ClusterState = Readable;
		D3D12EndMarker(CommandList);
		ClustersValid = true;
	}

	D3D12_GPU_VIRTUAL_ADDRESS UploadArray(const void* Data, size_t Size)
	{
		D3D12UploadAllocation Upload = {};
		if (Size == 0 || !D3D12Upload_Allocate(Size, 256, &Upload))
			return 0;
		std::memcpy(Upload.CPU, Data, Size);
		return Upload.GPU;
	}
}

grBoolean D3D12Lighting_Startup()
{
	D3D12Lighting_Shutdown();
	if (!g_pDevice || !g_pPSOManager || !g_pPSOManager->IsBindless())
	{
		D3D12Log::GetPtr()->Printf("Lighting: frame lighting needs bindless textures, disabled");
		return GR_TRUE;
	}

	AtlasSize = static_cast<UINT>((std::max)(512, (std::min)(D3D12Config_GetInt("Shadows", "AtlasSize", 4096), 8192)));
	AtlasSize = (AtlasSize / AtlasCells) * AtlasCells;
	MaxShadowLights = (std::max)(0, D3D12Config_GetInt("Shadows", "MaxLights", 8));
	char Value[64];
	ShadowDistance = D3D12Config_GetString("Shadows", "Distance", Value, sizeof(Value)) ? static_cast<float>(std::atof(Value)) : 3000.0f;
	if (ShadowDistance < 100.0f)
		ShadowDistance = 100.0f;

	ClusterCounts = CreateBuffer(NumClusters * sizeof(uint32), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ClusterItems = CreateBuffer(static_cast<UINT64>(NumClusters) * MaxPerCluster * sizeof(uint32), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ClusterState = D3D12_RESOURCE_STATE_COMMON;
	if (!ClusterCounts || !ClusterItems || !CreateClusterPipeline())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Light clusters could not be created; lights are tested per pixel");
		ClusterPSO.Reset();
	}
	if (!CreateAtlas() || !CreateShadowPipeline())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Shadow maps could not be created; dynamic lights cast no shadows");
		Atlas.Reset();
		ShadowPSO.Reset();
	}
	Available = true;
	D3D12Lighting_WriteDescriptors();
	D3D12Log::GetPtr()->Printf("Lighting: %ux%ux%u light clusters, %ux%u shadow atlas, up to %d shadowed lights",
		ClusterX, ClusterY, ClusterZ, AtlasSize, AtlasSize, MaxShadowLights);
	return GR_TRUE;
}

void D3D12Lighting_Shutdown()
{
	ClusterCounts.Reset();
	ClusterItems.Reset();
	ClusterRootSignature.Reset();
	ClusterPSO.Reset();
	Atlas.Reset();
	AtlasDSVHeap.Reset();
	ShadowRootSignature.Reset();
	ShadowPSO.Reset();
	Available = false;
	HaveMain = false;
	FrameLights.clear();
	GpuLights.clear();
	ShadowViews.clear();
	ShadowViewLights.clear();
}

void D3D12Lighting_WriteDescriptors()
{
	if (!Atlas || !D3D12_THandle_GetDescriptorHeap())
		return;
	D3D12_SHADER_RESOURCE_VIEW_DESC SRV = {};
	SRV.Format = DXGI_FORMAT_R32_FLOAT;
	SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRV.Texture2D.MipLevels = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE Handle = {};
	D3D12_THandle_GetReservedSRV(D3D12_RESERVED_SRV_SHADOW, &Handle, nullptr);
	g_pDevice->CreateShaderResourceView(Atlas.Get(), &SRV, Handle);
}

void D3D12Lighting_BeginFrame()
{
	HaveMain = false;
	FrameLights.clear();
	GpuLights.clear();
	ShadowViews.clear();
	ShadowViewLights.clear();
	NumDirLights = 0;
	ClustersValid = false;
	LightsGPU = ShadowViewsGPU = DummyGPU = 0;
}

void DRIVERCC D3D12Lighting_SetFrame(const DRV_WorldView* View, const DRV_Light* Lights, int32 NumLights)
{
	HaveMain = false;
	FrameLights.clear();
	if (View && View->Scale > 0.0f)
	{
		MainView = *View;
		const grXForm3d& M = View->ModelToCamera;
		const float Rows[3][4] = {
			{ M.AX, M.AY, M.AZ, M.Translation.X },
			{ M.BX, M.BY, M.BZ, M.Translation.Y },
			{ M.CX, M.CY, M.CZ, M.Translation.Z }
		};
		std::memcpy(MainRows, Rows, sizeof(Rows));
		HaveMain = true;
		const float ZFar = (View->ZFar > ClusterZNear * 2.0f) ? View->ZFar : ClusterZFarDefault;
		ClusterZLogScale = ClusterZ / std::log(ZFar / ClusterZNear);
	}
	if (Lights && NumLights > 0)
	{
		NumLights = (std::min)(NumLights, static_cast<int32>(DRV_MAX_FRAME_LIGHTS));
		FrameLights.assign(Lights, Lights + NumLights);
	}
}

bool D3D12Lighting_IsMainCamera(const float ModelToCamera[3][4], const float ModelToWorld[3][4])
{
	if (!HaveMain)
		return false;
	// World to this camera = ModelToCamera * inverse(ModelToWorld); ModelToWorld is rigid.
	for (int i = 0; i < 3; ++i)
	{
		float Row[4];
		for (int j = 0; j < 3; ++j)
			Row[j] = ModelToCamera[i][0] * ModelToWorld[0][j] + ModelToCamera[i][1] * ModelToWorld[1][j] + ModelToCamera[i][2] * ModelToWorld[2][j];
		Row[3] = ModelToCamera[i][3] - (Row[0] * ModelToWorld[0][3] + Row[1] * ModelToWorld[1][3] + Row[2] * ModelToWorld[2][3]);
		for (int j = 0; j < 3; ++j)
		{
			if (std::fabs(Row[j] - MainRows[i][j]) > 1e-3f)
				return false;
		}
		if (std::fabs(Row[3] - MainRows[i][3]) > 0.05f)
			return false;
	}
	return true;
}

bool D3D12Lighting_GetMainView(DRV_WorldView* View)
{
	if (!HaveMain)
		return false;
	if (View)
		*View = MainView;
	return true;
}

void D3D12Lighting_Prepare(ID3D12GraphicsCommandList* CommandList, const std::vector<D3D12ShadowCaster>& Meshes)
{
	if (!Available)
		return;

	BuildFrame();

	static const uint8 Zeros[256] = {};
	DummyGPU = UploadArray(Zeros, sizeof(Zeros));
	LightsGPU = GpuLights.empty() ? DummyGPU : UploadArray(GpuLights.data(), GpuLights.size() * sizeof(GpuLight));
	ShadowViewsGPU = ShadowViews.empty() ? DummyGPU : UploadArray(ShadowViews.data(), ShadowViews.size() * sizeof(GpuShadowView));
	if (!LightsGPU || !ShadowViewsGPU)
	{
		GpuLights.clear();
		ShadowViews.clear();
		ShadowViewLights.clear();
		LightsGPU = ShadowViewsGPU = DummyGPU;
	}

	if (Atlas && ShadowPSO)
		RenderShadows(CommandList, Meshes);
	BuildClusters(CommandList);

	// The cluster buffers are read by every PBR draw, even when this frame did not fill them
	if (ClusterState != (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) &&
		ClusterCounts && ClusterItems)
	{
		const D3D12_RESOURCE_STATES Readable = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		TransitionResource(CommandList, ClusterCounts.Get(), ClusterState, Readable);
		TransitionResource(CommandList, ClusterItems.Get(), ClusterState, Readable);
		ClusterState = Readable;
	}
}

void D3D12Lighting_FillFrameConstants(D3D12FrameConstants& Constants)
{
	std::memcpy(Constants.MainWorldToCamera, MainRows, sizeof(MainRows));
	Constants.MainScale = HaveMain ? MainView.Scale : 1.0f;
	Constants.MainXCenter = MainView.XCenter;
	Constants.MainYCenter = MainView.YCenter;
	Constants.MainZScale = MainView.ZScale;
	Constants.NumLights = Available ? static_cast<uint32>(GpuLights.size()) : 0u;
	Constants.NumDirLights = Available ? NumDirLights : 0u;
	Constants.ShadowAtlasIndex = Atlas ? D3D12_THandle_GetReservedIndex(D3D12_RESERVED_SRV_SHADOW) : 0u;
	Constants.ShadowAtlasTexel = 1.0f / static_cast<float>(AtlasSize);
	Constants.ClusterDims[0] = ClusterX;
	Constants.ClusterDims[1] = ClusterY;
	Constants.ClusterDims[2] = ClusterZ;
	Constants.ClusterDims[3] = ClustersValid ? MaxPerCluster : BruteForceLights;
	Constants.ClusterTile[0] = std::ceil(static_cast<float>((std::max)(g_nScreenWidth, static_cast<int32>(1))) / ClusterX);
	Constants.ClusterTile[1] = std::ceil(static_cast<float>((std::max)(g_nScreenHeight, static_cast<int32>(1))) / ClusterY);
	Constants.ClusterZNear = ClusterZNear;
	Constants.ClusterZLogScale = ClusterZLogScale;
	if (ClustersValid)
		Constants.FrameFlags |= D3D12_FRAME_CLUSTERS;
}

void D3D12Lighting_Bind(ID3D12GraphicsCommandList* CommandList)
{
	if (!Available || !DummyGPU || !ClusterCounts || !ClusterItems)
		return;
	CommandList->SetGraphicsRootShaderResourceView(ROOT_PARAM_LIGHTS, LightsGPU ? LightsGPU : DummyGPU);
	CommandList->SetGraphicsRootShaderResourceView(ROOT_PARAM_CLUSTER_COUNTS, ClusterCounts->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootShaderResourceView(ROOT_PARAM_CLUSTER_ITEMS, ClusterItems->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootShaderResourceView(ROOT_PARAM_SHADOW_VIEWS, ShadowViewsGPU ? ShadowViewsGPU : DummyGPU);
}
