/****************************************************************************************/
/*  D3D12PSOMANAGER.CPP                                                                 */
/*                                                                                      */
/*  Shader and immutable pipeline-state management for Jet3D's transformed polygons.   */
/****************************************************************************************/
#include <cfloat>
#include <cstring>

#include "D3D12PSOManager.h"
#include "D3D12Shaders.h"
#include "D3D12SceneTarget.h"
#include "D3D12Config.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	const uint32 STATE_ALPHA = 0x1;
	const uint32 STATE_NO_ZTEST = 0x2;
	const uint32 STATE_NO_ZWRITE = 0x4;
	const uint32 STATE_WIREFRAME = 0x8;

	D3D12_STATIC_SAMPLER_DESC MakeSampler(
		UINT ShaderRegister,
		D3D12_FILTER Filter,
		D3D12_TEXTURE_ADDRESS_MODE AddressMode)
	{
		D3D12_STATIC_SAMPLER_DESC Sampler = {};
		Sampler.Filter = Filter;
		Sampler.AddressU = AddressMode;
		Sampler.AddressV = AddressMode;
		Sampler.AddressW = AddressMode;
		Sampler.MipLODBias = 0.0f;
		Sampler.MaxAnisotropy = 1;
		Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		Sampler.MinLOD = 0.0f;
		Sampler.MaxLOD = FLT_MAX;
		Sampler.ShaderRegister = ShaderRegister;
		Sampler.RegisterSpace = 0;
		Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		return Sampler;
	}
}

D3D12PSOManager::D3D12PSOManager()
	: m_bInitialized(false)
	, m_bBindless(false)
{
}

D3D12PSOManager::~D3D12PSOManager()
{
	Shutdown();
}

grBoolean D3D12PSOManager::Initialize()
{
	Shutdown();
	// Bindless-lite needs resource binding tier 2 for a heap-sized SRV table.
	D3D12_FEATURE_DATA_D3D12_OPTIONS Options = {};
	const bool bTier2 = SUCCEEDED(g_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &Options, sizeof(Options))) &&
		Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2;
	m_bBindless = bTier2 && D3D12Config_GetBool("Render", "Bindless", true);
	D3D12Log::GetPtr()->Printf(m_bBindless
		? "Textures: bindless (indexed from the shader-visible heap)"
		: (bTier2 ? "Textures: descriptor tables ([Render] Bindless=0)"
		          : "Textures: descriptor tables (resource binding tier 1)"));

	if (!D3D12Shaders_Load(g_pDevice.Get()) || !CreateRootSignature())
	{
		Shutdown();
		return GR_FALSE;
	}

	// Build the normal, depth-tested variants now so shader/input-layout errors are
	// reported during driver initialization. Other state combinations are lazy.
	if (!CreatePSO(PSO_GOURAUD, 0) ||
		!CreatePSO(PSO_TEXTURE, 0) ||
		!CreatePSO(PSO_MULTITEX, 0))
	{
		Shutdown();
		return GR_FALSE;
	}

	m_bInitialized = true;
	D3D12Log::GetPtr()->Printf("D3D12 PSO manager initialized");
	return GR_TRUE;
}

void D3D12PSOManager::Shutdown()
{
	for (uint32 Type = 0; Type < PSO_COUNT; ++Type)
	{
		for (uint32 State = 0; State < PSO_STATE_COUNT; ++State)
			m_PSOs[Type][State].Reset();
	}
	m_pRootSignature.Reset();
	D3D12Shaders_Unload();
	m_bInitialized = false;
}

ID3D12PipelineState* D3D12PSOManager::GetPSO(
	D3D12_PSO_TYPE Type,
	uint32 Flags,
	grBoolean SceneWireframe)
{
	if (!m_bInitialized || Type < PSO_GOURAUD || Type >= PSO_COUNT)
		return nullptr;

	bool ForceAlpha = false;
	if (Type == PSO_ALPHA_GOURAUD)
	{
		Type = PSO_GOURAUD;
		ForceAlpha = true;
	}
	else if (Type == PSO_ALPHA_TEXTURE)
	{
		Type = PSO_TEXTURE;
		ForceAlpha = true;
	}

	uint32 State = 0;
	if (ForceAlpha || (Flags & (GR_RENDER_FLAG_ALPHA | GR_RENDER_FLAG_COLORKEY)))
		State |= STATE_ALPHA;
	if (Flags & GR_RENDER_FLAG_NO_ZTEST)
		State |= STATE_NO_ZTEST;
	if (Flags & GR_RENDER_FLAG_NO_ZWRITE)
		State |= STATE_NO_ZWRITE;
	if (SceneWireframe || (Flags & GR_RENDER_FLAG_WIREFRAME))
		State |= STATE_WIREFRAME;

	if (!m_PSOs[Type][State] && !CreatePSO(Type, State))
		return nullptr;
	return m_PSOs[Type][State].Get();
}

ID3D12RootSignature* D3D12PSOManager::GetRootSignature()
{
	return m_pRootSignature.Get();
}

grBoolean D3D12PSOManager::CreateRootSignature()
{
	// Root parameters (ROOT_PARAM_* in D3D12PSOManager.h):
	//   draw constants b0 (flags + texture indices), frame constants b1 (root CBV),
	//   then either one table spanning the whole SRV heap (t0 space1, bindless) or
	//   one single-texture table each for t0 and t1.
	D3D12_DESCRIPTOR_RANGE Ranges[2] = {};
	for (UINT i = 0; i < 2; ++i)
	{
		Ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		Ranges[i].NumDescriptors = 1;
		Ranges[i].BaseShaderRegister = i;
		Ranges[i].RegisterSpace = 0;
		Ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
	D3D12_DESCRIPTOR_RANGE HeapRange = {};
	HeapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	HeapRange.NumDescriptors = UINT_MAX;		// unbounded: the shader indexes the heap
	HeapRange.BaseShaderRegister = 0;
	HeapRange.RegisterSpace = 1;
	HeapRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER Parameters[4] = {};
	Parameters[ROOT_PARAM_DRAW].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	Parameters[ROOT_PARAM_DRAW].Constants.ShaderRegister = 0;
	Parameters[ROOT_PARAM_DRAW].Constants.Num32BitValues = 4;
	Parameters[ROOT_PARAM_DRAW].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	Parameters[ROOT_PARAM_FRAME].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters[ROOT_PARAM_FRAME].Descriptor.ShaderRegister = 1;
	Parameters[ROOT_PARAM_FRAME].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	UINT NumParameters = 2;
	if (m_bBindless)
	{
		Parameters[ROOT_PARAM_TEXTURES].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameters[ROOT_PARAM_TEXTURES].DescriptorTable.NumDescriptorRanges = 1;
		Parameters[ROOT_PARAM_TEXTURES].DescriptorTable.pDescriptorRanges = &HeapRange;
		Parameters[ROOT_PARAM_TEXTURES].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		NumParameters = 3;
	}
	else
	{
		for (UINT i = 0; i < 2; ++i)
		{
			Parameters[ROOT_PARAM_BASE_TABLE + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			Parameters[ROOT_PARAM_BASE_TABLE + i].DescriptorTable.NumDescriptorRanges = 1;
			Parameters[ROOT_PARAM_BASE_TABLE + i].DescriptorTable.pDescriptorRanges = &Ranges[i];
			Parameters[ROOT_PARAM_BASE_TABLE + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		NumParameters = 4;
	}

	D3D12_STATIC_SAMPLER_DESC Samplers[4] = {
		MakeSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		MakeSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		MakeSampler(2, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		MakeSampler(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
	};

	D3D12_ROOT_SIGNATURE_DESC Desc = {};
	Desc.NumParameters = NumParameters;
	Desc.pParameters = Parameters;
	Desc.NumStaticSamplers = _countof(Samplers);
	Desc.pStaticSamplers = Samplers;
	Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ComPtr<ID3DBlob> Signature;
	ComPtr<ID3DBlob> Errors;
	HRESULT Hr = D3D12SerializeRootSignature(
		&Desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&Signature,
		&Errors);
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf(
			"ERROR: Root signature serialization failed (0x%08X): %s",
			Hr,
			Errors ? static_cast<const char*>(Errors->GetBufferPointer()) : "no details");
		return GR_FALSE;
	}

	Hr = g_pDevice->CreateRootSignature(
		0,
		Signature->GetBufferPointer(),
		Signature->GetBufferSize(),
		IID_PPV_ARGS(&m_pRootSignature));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: CreateRootSignature failed - HR: 0x%08X", Hr);
		return GR_FALSE;
	}
	return GR_TRUE;
}

grBoolean D3D12PSOManager::CreatePSO(D3D12_PSO_TYPE Type, uint32 State)
{
	if (Type < PSO_GOURAUD || Type > PSO_MULTITEX || State >= PSO_STATE_COUNT)
		return GR_FALSE;

	D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_SHADER_ID PixelShader = m_bBindless ? SHADER_TLPOLY_PS_GOURAUD_BINDLESS : SHADER_TLPOLY_PS_GOURAUD;
	if (Type == PSO_TEXTURE)
		PixelShader = m_bBindless ? SHADER_TLPOLY_PS_TEXTURE_BINDLESS : SHADER_TLPOLY_PS_TEXTURE;
	else if (Type == PSO_MULTITEX)
		PixelShader = m_bBindless ? SHADER_TLPOLY_PS_MULTITEX_BINDLESS : SHADER_TLPOLY_PS_MULTITEX;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
	Desc.pRootSignature = m_pRootSignature.Get();
	Desc.VS = D3D12Shaders_Get(SHADER_TLPOLY_VS);
	Desc.PS = D3D12Shaders_Get(PixelShader);

	Desc.BlendState.AlphaToCoverageEnable = FALSE;
	Desc.BlendState.IndependentBlendEnable = FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC& Blend = Desc.BlendState.RenderTarget[0];
	Blend.BlendEnable = (State & STATE_ALPHA) ? TRUE : FALSE;
	Blend.LogicOpEnable = FALSE;
	Blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	Blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	Blend.BlendOp = D3D12_BLEND_OP_ADD;
	Blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	Blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	Blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	Blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	Blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	Desc.SampleMask = UINT_MAX;
	Desc.RasterizerState.FillMode = (State & STATE_WIREFRAME)
		? D3D12_FILL_MODE_WIREFRAME
		: D3D12_FILL_MODE_SOLID;
	Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	Desc.RasterizerState.FrontCounterClockwise = FALSE;
	Desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	Desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	Desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	Desc.RasterizerState.DepthClipEnable = TRUE;
	Desc.RasterizerState.MultisampleEnable = FALSE;
	Desc.RasterizerState.AntialiasedLineEnable = FALSE;
	Desc.RasterizerState.ForcedSampleCount = 0;
	Desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	Desc.DepthStencilState.DepthEnable = (State & STATE_NO_ZTEST) ? FALSE : TRUE;
	Desc.DepthStencilState.DepthWriteMask = (State & STATE_NO_ZWRITE)
		? D3D12_DEPTH_WRITE_MASK_ZERO
		: D3D12_DEPTH_WRITE_MASK_ALL;
	Desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	Desc.DepthStencilState.StencilEnable = FALSE;

	Desc.InputLayout = { InputLayout, _countof(InputLayout) };
	Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	Desc.NumRenderTargets = 1;
	Desc.RTVFormats[0] = D3D12Scene_GetFormat();
	Desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	Desc.SampleDesc.Count = 1;

	const HRESULT Hr = g_pDevice->CreateGraphicsPipelineState(
		&Desc,
		IID_PPV_ARGS(&m_PSOs[Type][State]));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf(
			"ERROR: CreateGraphicsPipelineState failed (type %u, state 0x%X) - HR: 0x%08X",
			static_cast<uint32>(Type),
			State,
			Hr);
		return GR_FALSE;
	}
	return GR_TRUE;
}
