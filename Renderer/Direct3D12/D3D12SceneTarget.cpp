/****************************************************************************************/
/*  D3D12SCENETARGET.CPP                                                                */
/*                                                                                      */
/*  The HDR scene color target and the present pass (see D3D12SceneTarget.h).          */
/****************************************************************************************/
#include "D3D12SceneTarget.h"
#include "D3D12Shaders.h"
#include "D3D12Config.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"
#include "D3D12Common.h"
#include "D3D12TextureMgr.h"
#include "D3D12Post.h"
#include "D3D12Lighting.h"

namespace
{
	const DXGI_FORMAT SceneFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	int							Enabled = -1;			// -1 until read from the ini
	UINT						Width = 0;
	UINT						Height = 0;
	ComPtr<ID3D12Resource>		Color;
	D3D12_RESOURCE_STATES		ColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	ComPtr<ID3D12DescriptorHeap>	RTVHeap;
	// The SRV lives in the texture heap's reserved slot D3D12_RESERVED_SRV_SCENE, so
	// the whole frame runs on one shader-visible heap.
	ComPtr<ID3D12RootSignature>	PresentRootSignature;
	ComPtr<ID3D12PipelineState>	PresentPSO;

	void Transition(ID3D12GraphicsCommandList* CommandList, D3D12_RESOURCE_STATES After)
	{
		if (ColorState == After)
			return;
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Transition.pResource = Color.Get();
		Barrier.Transition.StateBefore = ColorState;
		Barrier.Transition.StateAfter = After;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		CommandList->ResourceBarrier(1, &Barrier);
		ColorState = After;
	}

	grBoolean CreatePresentPipeline()
	{
		if (PresentPSO)
			return GR_TRUE;

		D3D12_DESCRIPTOR_RANGE Range = {};
		Range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		Range.NumDescriptors = 1;
		Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER Parameter = {};
		Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameter.DescriptorTable.NumDescriptorRanges = 1;
		Parameter.DescriptorTable.pDescriptorRanges = &Range;
		Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC RootDesc = {};
		RootDesc.NumParameters = 1;
		RootDesc.pParameters = &Parameter;

		ComPtr<ID3DBlob> Signature;
		ComPtr<ID3DBlob> Errors;
		HRESULT Hr = D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Errors);
		if (SUCCEEDED(Hr))
			Hr = g_pDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(),
				IID_PPV_ARGS(&PresentRootSignature));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Present root signature failed - HR: 0x%08X", Hr);
			return GR_FALSE;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
		Desc.pRootSignature = PresentRootSignature.Get();
		Desc.VS = D3D12Shaders_Get(SHADER_PRESENT_VS);
		Desc.PS = D3D12Shaders_Get(SHADER_PRESENT_PS);
		Desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		Desc.SampleMask = UINT_MAX;
		Desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		Desc.RasterizerState.DepthClipEnable = TRUE;
		Desc.DepthStencilState.DepthEnable = FALSE;
		Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		Desc.NumRenderTargets = 1;
		Desc.RTVFormats[0] = BackBufferFormat;
		Desc.SampleDesc.Count = 1;
		Hr = g_pDevice->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PresentPSO));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Present pipeline failed - HR: 0x%08X", Hr);
			return GR_FALSE;
		}
		return GR_TRUE;
	}
}

bool D3D12Scene_IsEnabled()
{
	if (Enabled < 0)
	{
		Enabled = D3D12Config_GetBool("Render", "SceneTarget", true) ? 1 : 0;
		D3D12Log::GetPtr()->Printf(Enabled
			? "Rendering through the HDR scene target"
			: "Rendering straight to the back buffer ([Render] SceneTarget=0)");
	}
	return Enabled != 0;
}

DXGI_FORMAT D3D12Scene_GetFormat()
{
	return D3D12Scene_IsEnabled() ? SceneFormat : BackBufferFormat;
}

grBoolean D3D12Scene_Create(UINT NewWidth, UINT NewHeight)
{
	if (!D3D12Scene_IsEnabled())
		return GR_TRUE;
	if (!g_pDevice || NewWidth == 0 || NewHeight == 0)
		return GR_FALSE;

	D3D12Scene_Release();

	if (!RTVHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
		HeapDesc.NumDescriptors = 1;
		HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(g_pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&RTVHeap))))
			return GR_FALSE;
	}
	if (!D3D12_THandle_GetDescriptorHeap())
		return GR_FALSE;

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Width = NewWidth;
	Desc.Height = NewHeight;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = 1;
	Desc.Format = SceneFormat;
	Desc.SampleDesc.Count = 1;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE Clear = {};
	Clear.Format = SceneFormat;
	Clear.Color[3] = 1.0f;

	D3D12_HEAP_PROPERTIES Heap = {};
	Heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	const HRESULT Hr = g_pDevice->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_NONE, &Desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &Clear, IID_PPV_ARGS(&Color));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Scene target creation failed (%ux%u) - HR: 0x%08X", NewWidth, NewHeight, Hr);
		return GR_FALSE;
	}
	ColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	g_pDevice->CreateRenderTargetView(Color.Get(), nullptr, RTVHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_CPU_DESCRIPTOR_HANDLE SceneSRV = {};
	D3D12_THandle_GetReservedSRV(D3D12_RESERVED_SRV_SCENE, &SceneSRV, nullptr);
	g_pDevice->CreateShaderResourceView(Color.Get(), nullptr, SceneSRV);

	Width = NewWidth;
	Height = NewHeight;
	D3D12Log::GetPtr()->Printf("Scene target created (%ux%u, R16G16B16A16_FLOAT)", Width, Height);

	// The other driver-owned views in the heap's reserved slots
	D3D12Lighting_WriteDescriptors();
	return D3D12Post_Resize(Width, Height);
}

void D3D12Scene_Release()
{
	Color.Reset();
	ColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	Width = Height = 0;
}

void D3D12Scene_Shutdown()
{
	D3D12Scene_Release();
	PresentPSO.Reset();
	PresentRootSignature.Reset();
	RTVHeap.Reset();
	Enabled = -1;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Scene_Begin(ID3D12GraphicsCommandList* CommandList)
{
	Transition(CommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	return RTVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Scene_GetRTV()
{
	return RTVHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12Scene_TransitionColor(ID3D12GraphicsCommandList* CommandList, D3D12_RESOURCE_STATES State)
{
	if (Color)
		Transition(CommandList, State);
}

grBoolean D3D12Scene_Present(ID3D12GraphicsCommandList* CommandList, D3D12_CPU_DESCRIPTOR_HANDLE BackBufferRTV,
							 bool FromComposite)
{
	if (!Color || !CreatePresentPipeline())
		return GR_FALSE;

	D3D12BeginMarker(CommandList, "Present pass");
	Transition(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (FromComposite)
		D3D12Post_EndFrame(CommandList);

	D3D12_GPU_DESCRIPTOR_HANDLE SceneSRV = {};
	D3D12_THandle_GetReservedSRV(FromComposite ? D3D12_RESERVED_SRV_COMPOSITE : D3D12_RESERVED_SRV_SCENE, nullptr, &SceneSRV);
	CommandList->OMSetRenderTargets(1, &BackBufferRTV, FALSE, nullptr);
	CommandList->SetGraphicsRootSignature(PresentRootSignature.Get());
	CommandList->SetGraphicsRootDescriptorTable(0, SceneSRV);
	CommandList->SetPipelineState(PresentPSO.Get());
	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT Viewport = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
	D3D12_RECT Scissor = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &Scissor);
	CommandList->DrawInstanced(3, 1, 0, 0);
	D3D12EndMarker(CommandList);
	return GR_TRUE;
}
