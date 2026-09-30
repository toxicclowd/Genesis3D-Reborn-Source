/****************************************************************************************/
/*  D3D12POST.CPP                                                                       */
/*                                                                                      */
/*  Post-processing (see D3D12Post.h).                                                  */
/****************************************************************************************/
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "D3D12Post.h"
#include "D3D12PSOManager.h"
#include "D3D12SceneTarget.h"
#include "D3D12Lighting.h"
#include "D3D12Look.h"
#include "D3D12Shaders.h"
#include "D3D12TextureMgr.h"
#include "D3D12UploadRing.h"
#include "D3D12Config.h"
#include "D3D12Common.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

extern ComPtr<ID3D12Resource> g_pDepthStencil;

namespace
{
	// PostFlags in Shaders\Post.hlsl
	const uint32	POST_INPUT_LINEAR = 0x0001;
	const uint32	POST_AUTO_EXPOSURE = 0x0002;
	const uint32	POST_BLOOM = 0x0004;
	const uint32	POST_SSAO = 0x0008;
	const uint32	POST_FOG = 0x0010;
	const uint32	POST_HEIGHT_FOG = 0x0020;
	const uint32	POST_DITHER = 0x0040;
	const uint32	POST_CRT = 0x0080;
	const uint32	POST_HAVE_CAMERA = 0x0100;
	const uint32	POST_PALETTE = 0x0200;

	const UINT			BloomLevels = 6;
	const DXGI_FORMAT	CompositeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT	BloomFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	const DXGI_FORMAT	AOFormat = DXGI_FORMAT_R8_UNORM;

	// RTV heap slots
	const UINT		RTV_COMPOSITE = 0;
	const UINT		RTV_BLOOM = 1;
	const UINT		RTV_AO = RTV_BLOOM + BloomLevels;
	const UINT		RTV_COUNT = RTV_AO + 2;
	// Reserved SRV slots from D3D12_RESERVED_SRV_POST
	const UINT		SRV_BLOOM = D3D12_RESERVED_SRV_POST;
	const UINT		SRV_AO = SRV_BLOOM + BloomLevels;
	static_assert(SRV_AO + 2 <= D3D12_RESERVED_SRV_COUNT, "post-processing needs more reserved SRV slots");

	// cbuffer PostConstants in Shaders\Post.hlsl
	struct PostConstants
	{
		float	SceneSize[2];			// the scene target's size
		float	RegionSize[2];			// the part of it the scene was drawn in (render scale)
		float	OutputSize[2];
		float	InvOutputSize[2];
		uint32	SourceIndex;			// heap indices
		uint32	SceneIndex;
		uint32	DepthIndex;
		uint32	BloomIndex;
		uint32	AOIndex;
		uint32	Flags;					// POST_*
		uint32	Tonemapper;
		uint32	PaletteLevels;
		float	ExposureEV;
		float	BloomIntensity;
		float	SSAORadius;
		float	SSAOIntensity;
		float	FogColor[3];
		float	FogStart;
		float	FogEnd;
		float	HeightFogDensity;
		float	HeightFogBase;
		float	HeightFogFalloff;
		float	HeightFogColor[3];
		float	FrameIndex;
		float	CameraRows[3][4];		// main camera, world to camera
		float	Scale;
		float	XCenter;
		float	YCenter;
		float	ZScale;
		float	SourceSize[2];			// the pass's source texture
		float	SourceRegion[2];		// the part of it to read
		float	MinEV;
		float	MaxEV;
		float	KeyValue;
		float	AdaptRate;
		float	NumBloomLevels;
		float	RenderScale;
		float	Padding[2];
	};

	struct Target
	{
		ComPtr<ID3D12Resource>	Resource;
		D3D12_RESOURCE_STATES	State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		UINT					Width = 0;
		UINT					Height = 0;
		UINT					RTV = 0;		// slot in RTVHeap
		UINT					SRV = 0;		// reserved SRV slot

		void TransitionTo(ID3D12GraphicsCommandList* CommandList, D3D12_RESOURCE_STATES After)
		{
			if (!Resource || State == After)
				return;
			TransitionResource(CommandList, Resource.Get(), State, After);
			State = After;
		}
	};

	bool							Available = false;
	UINT							Width = 0;
	UINT							Height = 0;
	UINT							RegionWidth = 0;		// what the bloom and AO targets were made for
	UINT							RegionHeight = 0;
	ComPtr<ID3D12DescriptorHeap>	RTVHeap;
	UINT							RTVSize = 0;
	Target							Composite;
	Target							Bloom[BloomLevels];
	Target							AO[2];
	ComPtr<ID3D12Resource>			Histogram;
	D3D12_RESOURCE_STATES			HistogramState = D3D12_RESOURCE_STATE_COMMON;
	ComPtr<ID3D12Resource>			Exposure;
	D3D12_RESOURCE_STATES			ExposureState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES			DepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	float							AutoExposureMinEV = -3.0f;
	float							AutoExposureMaxEV = 2.0f;
	float							AutoExposureKey = 0.18f;

	ComPtr<ID3D12RootSignature>		RootSignature;
	ComPtr<ID3D12PipelineState>		BloomDownPSO;
	ComPtr<ID3D12PipelineState>		BloomUpPSO;
	ComPtr<ID3D12PipelineState>		SSAOPSO;
	ComPtr<ID3D12PipelineState>		SSAOBlurPSO;
	ComPtr<ID3D12PipelineState>		CompositePSO;
	ComPtr<ID3D12PipelineState>		HistogramClearPSO;
	ComPtr<ID3D12PipelineState>		HistogramPSO;
	ComPtr<ID3D12PipelineState>		ExposurePSO;

	D3D12_CPU_DESCRIPTOR_HANDLE RTV(UINT Slot)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE Handle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
		Handle.ptr += static_cast<SIZE_T>(Slot) * RTVSize;
		return Handle;
	}

	bool CreateTarget(Target& T, UINT W, UINT H, DXGI_FORMAT Format, UINT RTVSlot, UINT SRVSlot)
	{
		T.Resource.Reset();
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = (std::max)(W, 1u);
		Desc.Height = (std::max)(H, 1u);
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = Format;
		Desc.SampleDesc.Count = 1;
		Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		D3D12_CLEAR_VALUE Clear = {};
		Clear.Format = Format;
		if (FAILED(CreateCommittedResource(g_pDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&Desc, &T.Resource, &Clear)))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Post-processing target %ux%u creation failed", W, H);
			return false;
		}
		T.State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		T.Width = static_cast<UINT>(Desc.Width);
		T.Height = Desc.Height;
		T.RTV = RTVSlot;
		T.SRV = SRVSlot;
		g_pDevice->CreateRenderTargetView(T.Resource.Get(), nullptr, RTV(RTVSlot));
		D3D12_CPU_DESCRIPTOR_HANDLE SRV = {};
		D3D12_THandle_GetReservedSRV(SRVSlot, &SRV, nullptr);
		g_pDevice->CreateShaderResourceView(T.Resource.Get(), nullptr, SRV);
		return true;
	}

	ComPtr<ID3D12Resource> CreateUAVBuffer(UINT64 Size)
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Width = Size;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		ComPtr<ID3D12Resource> Buffer;
		if (FAILED(CreateCommittedResource(g_pDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, &Desc, &Buffer)))
			return nullptr;
		return Buffer;
	}

	bool CreateRootSignature()
	{
		D3D12_DESCRIPTOR_RANGE HeapRange = {};
		HeapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		HeapRange.NumDescriptors = UINT_MAX;
		HeapRange.BaseShaderRegister = 0;
		HeapRange.RegisterSpace = 1;
		HeapRange.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER Parameters[5] = {};
		Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		Parameters[0].Descriptor.ShaderRegister = 0;
		Parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameters[1].DescriptorTable.NumDescriptorRanges = 1;
		Parameters[1].DescriptorTable.pDescriptorRanges = &HeapRange;
		Parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		Parameters[2].Descriptor.ShaderRegister = 0;
		Parameters[2].Descriptor.RegisterSpace = 2;
		Parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		Parameters[3].Descriptor.ShaderRegister = 0;
		Parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		Parameters[4].Descriptor.ShaderRegister = 1;
		for (auto& Parameter : Parameters)
			Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC Samplers[2] = {};
		for (UINT i = 0; i < 2; ++i)
		{
			Samplers[i].Filter = i ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			Samplers[i].AddressU = Samplers[i].AddressV = Samplers[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			Samplers[i].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			Samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
			Samplers[i].ShaderRegister = i;
			Samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		D3D12_ROOT_SIGNATURE_DESC Desc = {};
		Desc.NumParameters = _countof(Parameters);
		Desc.pParameters = Parameters;
		Desc.NumStaticSamplers = _countof(Samplers);
		Desc.pStaticSamplers = Samplers;

		ComPtr<ID3DBlob> Signature;
		ComPtr<ID3DBlob> Errors;
		HRESULT Hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Errors);
		if (SUCCEEDED(Hr))
			Hr = g_pDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Post root signature failed (0x%08X): %s", Hr,
				Errors ? static_cast<const char*>(Errors->GetBufferPointer()) : "no details");
			return false;
		}
		return true;
	}

	ComPtr<ID3D12PipelineState> CreatePipeline(D3D12_SHADER_ID PixelShader, DXGI_FORMAT Format, bool Additive)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
		Desc.pRootSignature = RootSignature.Get();
		Desc.VS = D3D12Shaders_Get(SHADER_POST_VS);
		Desc.PS = D3D12Shaders_Get(PixelShader);
		D3D12_RENDER_TARGET_BLEND_DESC& Blend = Desc.BlendState.RenderTarget[0];
		Blend.BlendEnable = Additive ? TRUE : FALSE;
		Blend.SrcBlend = D3D12_BLEND_ONE;
		Blend.DestBlend = D3D12_BLEND_ONE;
		Blend.BlendOp = D3D12_BLEND_OP_ADD;
		Blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		Blend.DestBlendAlpha = D3D12_BLEND_ZERO;
		Blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		Blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		Desc.SampleMask = UINT_MAX;
		Desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		Desc.RasterizerState.DepthClipEnable = TRUE;
		Desc.DepthStencilState.DepthEnable = FALSE;
		Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		Desc.NumRenderTargets = 1;
		Desc.RTVFormats[0] = Format;
		Desc.SampleDesc.Count = 1;
		ComPtr<ID3D12PipelineState> PSO;
		if (FAILED(g_pDevice->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PSO))))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Post pipeline (shader %d) creation failed", static_cast<int>(PixelShader));
			return nullptr;
		}
		return PSO;
	}

	ComPtr<ID3D12PipelineState> CreateComputePipeline(D3D12_SHADER_ID Shader)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC Desc = {};
		Desc.pRootSignature = RootSignature.Get();
		Desc.CS = D3D12Shaders_Get(Shader);
		ComPtr<ID3D12PipelineState> PSO;
		if (FAILED(g_pDevice->CreateComputePipelineState(&Desc, IID_PPV_ARGS(&PSO))))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Post compute pipeline (shader %d) creation failed", static_cast<int>(Shader));
			return nullptr;
		}
		return PSO;
	}

	// Bloom and AO targets follow the scene's drawn size.
	bool EnsureRegionTargets(UINT W, UINT H)
	{
		if (W == RegionWidth && H == RegionHeight && Bloom[0].Resource && AO[0].Resource)
			return true;
		UINT BW = W, BH = H;
		for (UINT i = 0; i < BloomLevels; ++i)
		{
			BW = (std::max)(BW / 2, 1u);
			BH = (std::max)(BH / 2, 1u);
			if (!CreateTarget(Bloom[i], BW, BH, BloomFormat, RTV_BLOOM + i, SRV_BLOOM + i))
				return false;
		}
		for (UINT i = 0; i < 2; ++i)
		{
			if (!CreateTarget(AO[i], W, H, AOFormat, RTV_AO + i, SRV_AO + i))
				return false;
		}
		RegionWidth = W;
		RegionHeight = H;
		return true;
	}

	void SetViewport(ID3D12GraphicsCommandList* CommandList, UINT W, UINT H)
	{
		D3D12_VIEWPORT Viewport = { 0.0f, 0.0f, static_cast<float>(W), static_cast<float>(H), 0.0f, 1.0f };
		D3D12_RECT Scissor = { 0, 0, static_cast<LONG>(W), static_cast<LONG>(H) };
		CommandList->RSSetViewports(1, &Viewport);
		CommandList->RSSetScissorRects(1, &Scissor);
	}

	// One full-screen pass into Out (a render target), reading what Constants names.
	void Pass(ID3D12GraphicsCommandList* CommandList, ID3D12PipelineState* PSO, Target& Out, PostConstants Constants,
			  UINT SourceIndex, UINT SourceW, UINT SourceH, UINT RegionW, UINT RegionH)
	{
		Constants.SourceIndex = SourceIndex;
		Constants.SourceSize[0] = static_cast<float>(SourceW);
		Constants.SourceSize[1] = static_cast<float>(SourceH);
		Constants.SourceRegion[0] = static_cast<float>(RegionW);
		Constants.SourceRegion[1] = static_cast<float>(RegionH);
		Constants.OutputSize[0] = static_cast<float>(Out.Width);
		Constants.OutputSize[1] = static_cast<float>(Out.Height);
		Constants.InvOutputSize[0] = 1.0f / Out.Width;
		Constants.InvOutputSize[1] = 1.0f / Out.Height;
		D3D12UploadAllocation Upload = {};
		if (!D3D12Upload_Allocate(sizeof(Constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &Upload))
			return;
		std::memcpy(Upload.CPU, &Constants, sizeof(Constants));

		Out.TransitionTo(CommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE Handle = RTV(Out.RTV);
		CommandList->OMSetRenderTargets(1, &Handle, FALSE, nullptr);
		SetViewport(CommandList, Out.Width, Out.Height);
		CommandList->SetPipelineState(PSO);
		CommandList->SetGraphicsRootConstantBufferView(0, Upload.GPU);
		CommandList->DrawInstanced(3, 1, 0, 0);
	}

	void UAVBarrier(ID3D12GraphicsCommandList* CommandList, ID3D12Resource* Resource)
	{
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		Barrier.UAV.pResource = Resource;
		CommandList->ResourceBarrier(1, &Barrier);
	}

	void BufferTo(ID3D12GraphicsCommandList* CommandList, ID3D12Resource* Buffer, D3D12_RESOURCE_STATES& State, D3D12_RESOURCE_STATES After)
	{
		if (State == After)
			return;
		TransitionResource(CommandList, Buffer, State, After);
		State = After;
	}
}

grBoolean D3D12Post_Startup()
{
	D3D12Post_Shutdown();
	if (!g_pDevice || !g_pPSOManager || !g_pPSOManager->IsBindless() || !D3D12Scene_IsEnabled())
	{
		D3D12Log::GetPtr()->Printf("Post: post-processing needs bindless textures and the scene target, disabled");
		return GR_TRUE;
	}

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
	HeapDesc.NumDescriptors = RTV_COUNT;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	if (FAILED(g_pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&RTVHeap))))
		return GR_FALSE;
	RTVSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	Histogram = CreateUAVBuffer(256 * sizeof(uint32));
	Exposure = CreateUAVBuffer(4 * sizeof(float));
	HistogramState = ExposureState = D3D12_RESOURCE_STATE_COMMON;
	if (!Histogram || !Exposure || !CreateRootSignature())
		return GR_FALSE;

	BloomDownPSO = CreatePipeline(SHADER_POST_PS_BLOOM_DOWN, BloomFormat, false);
	BloomUpPSO = CreatePipeline(SHADER_POST_PS_BLOOM_UP, BloomFormat, true);
	SSAOPSO = CreatePipeline(SHADER_POST_PS_SSAO, AOFormat, false);
	SSAOBlurPSO = CreatePipeline(SHADER_POST_PS_SSAO_BLUR, AOFormat, false);
	CompositePSO = CreatePipeline(SHADER_POST_PS_COMPOSITE, CompositeFormat, false);
	HistogramClearPSO = CreateComputePipeline(SHADER_POST_CS_HISTOGRAM_CLEAR);
	HistogramPSO = CreateComputePipeline(SHADER_POST_CS_HISTOGRAM);
	ExposurePSO = CreateComputePipeline(SHADER_POST_CS_EXPOSURE);
	if (!BloomDownPSO || !BloomUpPSO || !SSAOPSO || !SSAOBlurPSO || !CompositePSO ||
		!HistogramClearPSO || !HistogramPSO || !ExposurePSO)
		return GR_FALSE;

	char Value[64];
	if (D3D12Config_GetString("Look", "AutoExposureMinEV", Value, sizeof(Value)))
		AutoExposureMinEV = static_cast<float>(std::atof(Value));
	if (D3D12Config_GetString("Look", "AutoExposureMaxEV", Value, sizeof(Value)))
		AutoExposureMaxEV = static_cast<float>(std::atof(Value));
	if (D3D12Config_GetString("Look", "AutoExposureKey", Value, sizeof(Value)))
		AutoExposureKey = (std::max)(0.01f, static_cast<float>(std::atof(Value)));

	Available = true;
	D3D12Log::GetPtr()->Printf("Post: post-processing ready");
	return GR_TRUE;
}

void D3D12Post_Shutdown()
{
	Available = false;
	Composite = Target();
	for (auto& T : Bloom)
		T = Target();
	for (auto& T : AO)
		T = Target();
	RegionWidth = RegionHeight = 0;
	Width = Height = 0;
	Histogram.Reset();
	Exposure.Reset();
	RTVHeap.Reset();
	RootSignature.Reset();
	BloomDownPSO.Reset();
	BloomUpPSO.Reset();
	SSAOPSO.Reset();
	SSAOBlurPSO.Reset();
	CompositePSO.Reset();
	HistogramClearPSO.Reset();
	HistogramPSO.Reset();
	ExposurePSO.Reset();
}

grBoolean D3D12Post_Resize(UINT NewWidth, UINT NewHeight)
{
	if (!Available)
		return GR_TRUE;
	Width = NewWidth;
	Height = NewHeight;
	RegionWidth = RegionHeight = 0;		// recreated at the next frame's size
	for (auto& T : Bloom)
		T = Target();
	for (auto& T : AO)
		T = Target();
	if (!CreateTarget(Composite, Width, Height, CompositeFormat, RTV_COMPOSITE, D3D12_RESERVED_SRV_COMPOSITE))
	{
		Available = false;
		return GR_FALSE;
	}

	// The depth buffer, read as R32_FLOAT
	if (g_pDepthStencil)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRV = {};
		SRV.Format = DXGI_FORMAT_R32_FLOAT;
		SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRV.Texture2D.MipLevels = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE Handle = {};
		D3D12_THandle_GetReservedSRV(D3D12_RESERVED_SRV_DEPTH, &Handle, nullptr);
		g_pDevice->CreateShaderResourceView(g_pDepthStencil.Get(), &SRV, Handle);
	}
	DepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	return GR_TRUE;
}

bool D3D12Post_Needed(bool HDRScene)
{
	if (!Available || !Composite.Resource)
		return false;
	return HDRScene || D3D12Look_GetFog().Enabled;
}

grBoolean D3D12Post_Run(ID3D12GraphicsCommandList* CommandList, const D3D12FrameConstants& Frame, bool HDRScene,
						D3D12_CPU_DESCRIPTOR_HANDLE* OverlayRTV)
{
	if (!Available || !Composite.Resource || !g_pDepthStencil)
		return GR_FALSE;

	const DRV_LookSettings& Look = D3D12Look_Get();
	const D3D12Fog& Fog = D3D12Look_GetFog();
	const float Scale = HDRScene ? Frame.RenderScale : 1.0f;
	const UINT RW = (std::max)(1u, static_cast<UINT>(std::floor(Width * Scale + 0.5f)));
	const UINT RH = (std::max)(1u, static_cast<UINT>(std::floor(Height * Scale + 0.5f)));
	if (!EnsureRegionTargets(RW, RH))
		return GR_FALSE;

	DRV_WorldView Camera = {};
	const bool HaveCamera = D3D12Lighting_GetMainView(&Camera);

	PostConstants C = {};
	C.SceneSize[0] = static_cast<float>(Width);
	C.SceneSize[1] = static_cast<float>(Height);
	C.RegionSize[0] = static_cast<float>(RW);
	C.RegionSize[1] = static_cast<float>(RH);
	C.SceneIndex = D3D12_THandle_GetReservedIndex(D3D12_RESERVED_SRV_SCENE);
	C.DepthIndex = D3D12_THandle_GetReservedIndex(D3D12_RESERVED_SRV_DEPTH);
	C.BloomIndex = D3D12_THandle_GetReservedIndex(SRV_BLOOM);
	C.AOIndex = D3D12_THandle_GetReservedIndex(SRV_AO + 1);
	if (HDRScene)
	{
		C.Flags |= POST_INPUT_LINEAR;
		if (Look.AutoExposure)
			C.Flags |= POST_AUTO_EXPOSURE;
		if (Look.Bloom && Look.BloomIntensity > 0.0f)
			C.Flags |= POST_BLOOM;
		if (Look.SSAO && Look.SSAOIntensity > 0.0f && HaveCamera)
			C.Flags |= POST_SSAO;
		if (Look.HeightFogDensity > 0.0f && HaveCamera)
			C.Flags |= POST_HEIGHT_FOG;
		if (Look.Profile == DRV_LOOK_STYLIZED)
		{
			if (Look.PaletteBits > 0)
				C.Flags |= POST_PALETTE;
			if (Look.Dither)
				C.Flags |= POST_DITHER;
			if (Look.CRT)
				C.Flags |= POST_CRT;
		}
		C.Tonemapper = static_cast<uint32>(Look.Tonemapper);
	}
	if (Fog.Enabled && HaveCamera)
		C.Flags |= POST_FOG;
	if (HaveCamera)
		C.Flags |= POST_HAVE_CAMERA;
	C.PaletteLevels = (Look.PaletteBits > 0) ? (1u << Look.PaletteBits) : 256u;
	C.ExposureEV = Look.ExposureEV;
	C.BloomIntensity = Look.BloomIntensity;
	C.SSAORadius = Look.SSAORadius;
	C.SSAOIntensity = Look.SSAOIntensity;
	std::memcpy(C.FogColor, Fog.Color, sizeof(C.FogColor));
	C.FogStart = Fog.Start;
	C.FogEnd = Fog.End;
	C.HeightFogDensity = Look.HeightFogDensity;
	C.HeightFogBase = Look.HeightFogBase;
	C.HeightFogFalloff = Look.HeightFogFalloff;
	std::memcpy(C.HeightFogColor, Look.HeightFogColor, sizeof(C.HeightFogColor));
	C.FrameIndex = static_cast<float>(Frame.FrameNumber & 0xFFFF);
	std::memcpy(C.CameraRows, Frame.MainWorldToCamera, sizeof(C.CameraRows));
	C.Scale = Frame.MainScale;
	C.XCenter = Frame.MainXCenter;
	C.YCenter = Frame.MainYCenter;
	C.ZScale = Frame.MainZScale;
	C.MinEV = AutoExposureMinEV;
	C.MaxEV = AutoExposureMaxEV;
	C.KeyValue = AutoExposureKey;
	// Per frame, so the result does not depend on the frame rate's timing (regression shots)
	C.AdaptRate = 1.0f - std::exp(-1.5f / 60.0f);
	C.NumBloomLevels = static_cast<float>(BloomLevels);
	C.RenderScale = Scale;

	D3D12BeginMarker(CommandList, "Post-processing");
	ID3D12DescriptorHeap* Heap = D3D12_THandle_GetDescriptorHeap();
	D3D12Scene_TransitionColor(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	const D3D12_RESOURCE_STATES DepthRead = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	BufferTo(CommandList, g_pDepthStencil.Get(), DepthState, DepthRead);

	// A small zeroed buffer for root views a pass does not read
	static const uint8 Zeros[256] = {};
	D3D12UploadAllocation Dummy = {};
	if (!D3D12Upload_Allocate(sizeof(Zeros), 256, &Dummy))
		return GR_FALSE;
	std::memcpy(Dummy.CPU, Zeros, sizeof(Zeros));

	// Auto exposure: histogram of the scene's luminance, then the adapted exposure
	if (C.Flags & POST_AUTO_EXPOSURE)
	{
		D3D12UploadAllocation Upload = {};
		PostConstants CC = C;
		CC.SourceIndex = C.SceneIndex;
		if (D3D12Upload_Allocate(sizeof(CC), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &Upload))
		{
			std::memcpy(Upload.CPU, &CC, sizeof(CC));
			BufferTo(CommandList, Histogram.Get(), HistogramState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			BufferTo(CommandList, Exposure.Get(), ExposureState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			CommandList->SetComputeRootSignature(RootSignature.Get());
			CommandList->SetComputeRootConstantBufferView(0, Upload.GPU);
			CommandList->SetComputeRootDescriptorTable(1, Heap->GetGPUDescriptorHandleForHeapStart());
			CommandList->SetComputeRootShaderResourceView(2, Dummy.GPU);
			CommandList->SetComputeRootUnorderedAccessView(3, Histogram->GetGPUVirtualAddress());
			CommandList->SetComputeRootUnorderedAccessView(4, Exposure->GetGPUVirtualAddress());
			CommandList->SetPipelineState(HistogramClearPSO.Get());
			CommandList->Dispatch(1, 1, 1);
			UAVBarrier(CommandList, Histogram.Get());
			CommandList->SetPipelineState(HistogramPSO.Get());
			CommandList->Dispatch((RW + 15) / 16, (RH + 15) / 16, 1);
			UAVBarrier(CommandList, Histogram.Get());
			CommandList->SetPipelineState(ExposurePSO.Get());
			CommandList->Dispatch(1, 1, 1);
		}
	}
	BufferTo(CommandList, Exposure.Get(), ExposureState,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	CommandList->SetGraphicsRootSignature(RootSignature.Get());
	CommandList->SetGraphicsRootDescriptorTable(1, Heap->GetGPUDescriptorHandleForHeapStart());
	CommandList->SetGraphicsRootShaderResourceView(2, Exposure->GetGPUVirtualAddress());
	// Not read by the pixel shaders, but every root parameter must hold something
	CommandList->SetGraphicsRootUnorderedAccessView(3, Histogram->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootUnorderedAccessView(4, Histogram->GetGPUVirtualAddress());
	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (C.Flags & POST_SSAO)
	{
		Pass(CommandList, SSAOPSO.Get(), AO[0], C, C.DepthIndex, Width, Height, RW, RH);
		AO[0].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Pass(CommandList, SSAOBlurPSO.Get(), AO[1], C, D3D12_THandle_GetReservedIndex(AO[0].SRV), AO[0].Width, AO[0].Height, AO[0].Width, AO[0].Height);
		AO[1].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	if (C.Flags & POST_BLOOM)
	{
		// Down the chain, the first step from the scene (13-tap, Karis average)
		Pass(CommandList, BloomDownPSO.Get(), Bloom[0], C, C.SceneIndex, Width, Height, RW, RH);
		for (UINT i = 1; i < BloomLevels; ++i)
		{
			Bloom[i - 1].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			Pass(CommandList, BloomDownPSO.Get(), Bloom[i], C, D3D12_THandle_GetReservedIndex(Bloom[i - 1].SRV),
				Bloom[i - 1].Width, Bloom[i - 1].Height, Bloom[i - 1].Width, Bloom[i - 1].Height);
		}
		// Back up, each level adding the tent-filtered level below it
		for (UINT i = BloomLevels - 1; i > 0; --i)
		{
			Bloom[i].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			Pass(CommandList, BloomUpPSO.Get(), Bloom[i - 1], C, D3D12_THandle_GetReservedIndex(Bloom[i].SRV),
				Bloom[i].Width, Bloom[i].Height, Bloom[i].Width, Bloom[i].Height);
		}
		Bloom[0].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	else
	{
		Bloom[0].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	AO[1].TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Pass(CommandList, CompositePSO.Get(), Composite, C, C.SceneIndex, Width, Height, RW, RH);

	// The overlay may still test depth
	BufferTo(CommandList, g_pDepthStencil.Get(), DepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D12EndMarker(CommandList);

	if (OverlayRTV)
		*OverlayRTV = RTV(Composite.RTV);
	return GR_TRUE;
}

void D3D12Post_EndFrame(ID3D12GraphicsCommandList* CommandList)
{
	Composite.TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
