/****************************************************************************************/
/*  D3D12PSOMANAGER.CPP                                                                 */
/*                                                                                      */
/*  Shader and immutable pipeline-state management for Jet3D's transformed polygons.   */
/****************************************************************************************/
#include <cfloat>
#include <cstring>

#include "D3D12PSOManager.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	const uint32 STATE_ALPHA = 0x1;
	const uint32 STATE_NO_ZTEST = 0x2;
	const uint32 STATE_NO_ZWRITE = 0x4;
	const uint32 STATE_WIREFRAME = 0x8;

	const char* ShaderCode = R"(
cbuffer DrawConstants : register(b0)
{
    float ViewportWidth;
    float ViewportHeight;
    uint  DrawFlags;
    uint  DrawPadding;
};

Texture2D BaseTexture : register(t0);
Texture2D LightTexture : register(t1);
SamplerState LinearWrapSampler  : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState PointWrapSampler   : register(s2);
SamplerState PointClampSampler  : register(s3);

struct VS_INPUT
{
    float4 Position : POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD0;
    float2 LMCoord  : TEXCOORD1;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;

    // grTLVertex contains pixel-space x/y and positive camera-space z. The legacy
    // driver used XYZRHW with depth = 1 - 1/z. Constructing this clip position
    // reproduces that projection and preserves perspective-correct UV interpolation.
    float width = max(ViewportWidth, 1.0f);
    float height = max(ViewportHeight, 1.0f);
    float cameraZ = max(input.Position.z, 0.0001f);
    float ndcX = input.Position.x * (2.0f / width) - 1.0f;
    float ndcY = 1.0f - input.Position.y * (2.0f / height);
    float depth = saturate(1.0f - rcp(cameraZ));
    output.Position = float4(ndcX * cameraZ, ndcY * cameraZ, depth * cameraZ, cameraZ);
    output.Color = saturate(input.Color);
    output.TexCoord = input.TexCoord;
    output.LMCoord = input.LMCoord;
    return output;
}

float4 SampleBase(float2 uv)
{
    bool clampUV = (DrawFlags & 0x00000008u) != 0;
    bool linearFilter = (DrawFlags & 0x00000400u) != 0;
    if (linearFilter)
        return clampUV ? BaseTexture.Sample(LinearClampSampler, uv)
                       : BaseTexture.Sample(LinearWrapSampler, uv);
    return clampUV ? BaseTexture.Sample(PointClampSampler, uv)
                   : BaseTexture.Sample(PointWrapSampler, uv);
}

float4 SampleLight(float2 uv)
{
    return ((DrawFlags & 0x00000400u) != 0)
        ? LightTexture.Sample(LinearClampSampler, uv)
        : LightTexture.Sample(PointClampSampler, uv);
}

float4 PSGouraud(VS_OUTPUT input) : SV_TARGET
{
    return input.Color;
}

float4 PSTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.TexCoord);
    if ((DrawFlags & 0x00000004u) != 0)
        clip(base.a - 0.5f);

    float alpha = input.Color.a;
    if ((DrawFlags & 0x00000005u) != 0)
        alpha *= base.a;
    return saturate(float4(base.rgb * input.Color.rgb, alpha));
}

float4 PSMultiTexture(VS_OUTPUT input) : SV_TARGET
{
    float4 base = SampleBase(input.TexCoord);
    if ((DrawFlags & 0x00000004u) != 0)
        clip(base.a - 0.5f);

    float3 light = SampleLight(input.LMCoord).rgb;
    float alpha = input.Color.a;
    if ((DrawFlags & 0x00000005u) != 0)
        alpha *= base.a;
    return saturate(float4(base.rgb * light * input.Color.rgb, alpha));
}
)";

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
{
}

D3D12PSOManager::~D3D12PSOManager()
{
	Shutdown();
}

grBoolean D3D12PSOManager::Initialize()
{
	Shutdown();
	if (!CompileShaders() || !CreateRootSignature())
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
	m_VS_Gouraud.Reset();
	m_PS_Gouraud.Reset();
	m_PS_Texture.Reset();
	m_PS_MultiTex.Reset();
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

grBoolean D3D12PSOManager::CompileShaders()
{
	m_VS_Gouraud = CompileShader(ShaderCode, "VSMain", "vs_5_0");
	m_PS_Gouraud = CompileShader(ShaderCode, "PSGouraud", "ps_5_0");
	m_PS_Texture = CompileShader(ShaderCode, "PSTexture", "ps_5_0");
	m_PS_MultiTex = CompileShader(ShaderCode, "PSMultiTexture", "ps_5_0");
	return (m_VS_Gouraud && m_PS_Gouraud && m_PS_Texture && m_PS_MultiTex)
		? GR_TRUE
		: GR_FALSE;
}

ComPtr<ID3DBlob> D3D12PSOManager::CompileShader(
	const char* Code,
	const char* EntryPoint,
	const char* Target)
{
	ComPtr<ID3DBlob> Shader;
	ComPtr<ID3DBlob> Errors;
	UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	CompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	const HRESULT Hr = D3DCompile(
		Code,
		std::strlen(Code),
		"Jet3D_D3D12.hlsl",
		nullptr,
		nullptr,
		EntryPoint,
		Target,
		CompileFlags,
		0,
		&Shader,
		&Errors);
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf(
			"ERROR: %s compilation failed (0x%08X): %s",
			EntryPoint,
			Hr,
			Errors ? static_cast<const char*>(Errors->GetBufferPointer()) : "no compiler details");
		return nullptr;
	}
	return Shader;
}

grBoolean D3D12PSOManager::CreateRootSignature()
{
	D3D12_DESCRIPTOR_RANGE Ranges[2] = {};
	for (UINT i = 0; i < 2; ++i)
	{
		Ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		Ranges[i].NumDescriptors = 1;
		Ranges[i].BaseShaderRegister = i;
		Ranges[i].RegisterSpace = 0;
		Ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_ROOT_PARAMETER Parameters[3] = {};
	Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	Parameters[0].Constants.ShaderRegister = 0;
	Parameters[0].Constants.RegisterSpace = 0;
	Parameters[0].Constants.Num32BitValues = 4;
	Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	for (UINT i = 0; i < 2; ++i)
	{
		Parameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameters[i + 1].DescriptorTable.NumDescriptorRanges = 1;
		Parameters[i + 1].DescriptorTable.pDescriptorRanges = &Ranges[i];
		Parameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_STATIC_SAMPLER_DESC Samplers[4] = {
		MakeSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		MakeSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		MakeSampler(2, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		MakeSampler(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
	};

	D3D12_ROOT_SIGNATURE_DESC Desc = {};
	Desc.NumParameters = _countof(Parameters);
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

	ID3DBlob* PixelShader = m_PS_Gouraud.Get();
	if (Type == PSO_TEXTURE)
		PixelShader = m_PS_Texture.Get();
	else if (Type == PSO_MULTITEX)
		PixelShader = m_PS_MultiTex.Get();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
	Desc.pRootSignature = m_pRootSignature.Get();
	Desc.VS = { m_VS_Gouraud->GetBufferPointer(), m_VS_Gouraud->GetBufferSize() };
	Desc.PS = { PixelShader->GetBufferPointer(), PixelShader->GetBufferSize() };

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
	Desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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
