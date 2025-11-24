/****************************************************************************************/
/*  D3D12PSOMANAGER.CPP                                                                 */
/*                                                                                      */
/*  DirectX 12 Pipeline State Object Manager Implementation                            */
/*                                                                                      */
/****************************************************************************************/
#include "D3D12PSOManager.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"
#include <string.h>

// Include compiled shader code (normally would be compiled from HLSL file)
// For simplicity, we'll compile at runtime
const char* g_ShaderCode = R"(
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj;
    float4 AmbientColor;
    float4 FogColor;
    float  FogStart;
    float  FogEnd;
    float2 Padding;
};

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
    output.Position = mul(input.Position, WorldViewProj);
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    output.LMCoord = input.LMCoord;
    return output;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return input.Color;
}
)";

D3D12PSOManager::D3D12PSOManager()
	: m_bInitialized(false)
{
}

D3D12PSOManager::~D3D12PSOManager()
{
	Shutdown();
}

jeBoolean D3D12PSOManager::Initialize()
{
	D3D12Log::GetPtr()->Printf("D3D12PSOManager::Initialize");

	// Compile shaders
	if (!CompileShaders())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to compile shaders");
		return JE_FALSE;
	}

	// Create root signature
	if (!CreateRootSignature())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create root signature");
		return JE_FALSE;
	}

	// Create PSOs
	if (!CreateGouraudPSO())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create Gouraud PSO");
		return JE_FALSE;
	}

	// For now, just create the basic Gouraud PSO
	// TODO: Create texture and multi-texture PSOs

	m_bInitialized = true;
	D3D12Log::GetPtr()->Printf("PSO Manager initialized successfully");
	return JE_TRUE;
}

void D3D12PSOManager::Shutdown()
{
	if (!m_bInitialized)
		return;

	D3D12Log::GetPtr()->Printf("D3D12PSOManager::Shutdown");

	// Release all PSOs
	for (int i = 0; i < PSO_COUNT; i++)
	{
		m_PSOs[i].Reset();
	}

	m_pRootSignature.Reset();

	// Release compiled shaders
	m_VS_Gouraud.Reset();
	m_PS_Gouraud.Reset();
	m_VS_Texture.Reset();
	m_PS_Texture.Reset();
	m_VS_MultiTex.Reset();
	m_PS_MultiTex.Reset();

	m_bInitialized = false;
}

ID3D12PipelineState* D3D12PSOManager::GetPSO(D3D12_PSO_TYPE type)
{
	if (!m_bInitialized || type >= PSO_COUNT)
		return nullptr;

	return m_PSOs[type].Get();
}

ID3D12RootSignature* D3D12PSOManager::GetRootSignature()
{
	return m_pRootSignature.Get();
}

jeBoolean D3D12PSOManager::CompileShaders()
{
	D3D12Log::GetPtr()->Printf("Compiling shaders...");

	// Compile vertex shader
	m_VS_Gouraud = CompileShader(g_ShaderCode, "VSMain", "vs_5_0");
	if (!m_VS_Gouraud)
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to compile vertex shader");
		return JE_FALSE;
	}

	// Compile pixel shader
	m_PS_Gouraud = CompileShader(g_ShaderCode, "PSMain", "ps_5_0");
	if (!m_PS_Gouraud)
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to compile pixel shader");
		return JE_FALSE;
	}

	D3D12Log::GetPtr()->Printf("Shaders compiled successfully");
	return JE_TRUE;
}

ComPtr<ID3DBlob> D3D12PSOManager::CompileShader(const char* shaderCode, const char* entryPoint, const char* target)
{
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorBlob;

	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompile(
		shaderCode,
		strlen(shaderCode),
		nullptr,
		nullptr,
		nullptr,
		entryPoint,
		target,
		compileFlags,
		0,
		&shader,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			D3D12Log::GetPtr()->Printf("Shader compile error: %s", (char*)errorBlob->GetBufferPointer());
		}
		return nullptr;
	}

	return shader;
}

jeBoolean D3D12PSOManager::CreateRootSignature()
{
	D3D12Log::GetPtr()->Printf("Creating root signature...");

	// Create a simple root signature with a constant buffer
	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam.Descriptor.ShaderRegister = 0;
	rootParam.Descriptor.RegisterSpace = 0;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 1;
	rootSigDesc.pParameters = &rootParam;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	if (FAILED(hr))
	{
		if (error)
		{
			D3D12Log::GetPtr()->Printf("Root signature error: %s", (char*)error->GetBufferPointer());
		}
		return JE_FALSE;
	}

	hr = g_pDevice->CreateRootSignature(
		0,
		signature->GetBufferPointer(),
		signature->GetBufferSize(),
		IID_PPV_ARGS(&m_pRootSignature)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create root signature - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	D3D12Log::GetPtr()->Printf("Root signature created successfully");
	return JE_TRUE;
}

jeBoolean D3D12PSOManager::CreateGouraudPSO()
{
	D3D12Log::GetPtr()->Printf("Creating Gouraud PSO...");

	// Define input layout
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Create PSO descriptor
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = m_pRootSignature.Get();
	psoDesc.VS = { m_VS_Gouraud->GetBufferPointer(), m_VS_Gouraud->GetBufferSize() };
	psoDesc.PS = { m_PS_Gouraud->GetBufferPointer(), m_PS_Gouraud->GetBufferSize() };
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	HRESULT hr = g_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSOs[PSO_GOURAUD]));
	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create Gouraud PSO - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	D3D12Log::GetPtr()->Printf("Gouraud PSO created successfully");
	return JE_TRUE;
}

jeBoolean D3D12PSOManager::CreateTexturePSO()
{
	// TODO: Implement texture PSO
	D3D12Log::GetPtr()->Printf("CreateTexturePSO not yet implemented");
	return JE_TRUE;
}

jeBoolean D3D12PSOManager::CreateMultiTexPSO()
{
	// TODO: Implement multi-texture PSO
	D3D12Log::GetPtr()->Printf("CreateMultiTexPSO not yet implemented");
	return JE_TRUE;
}

jeBoolean D3D12PSOManager::CreateAlphaPSOs()
{
	// TODO: Implement alpha blending PSOs
	D3D12Log::GetPtr()->Printf("CreateAlphaPSOs not yet implemented");
	return JE_TRUE;
}
