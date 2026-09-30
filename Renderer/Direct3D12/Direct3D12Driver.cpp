/****************************************************************************************/
/*  DIRECT3D12DRIVER.CPP                                                                */
/*                                                                                      */
/*  DirectX 12 Driver Implementation                                                    */
/*                                                                                      */
/****************************************************************************************/
#include "Direct3D12Driver.h"
#include "D3D12Common.h"
#include "D3D12TextureMgr.h"
#include "D3D12PolyCache.h"
#include "D3D12PSOManager.h"
#include "D3D12Config.h"
#include "D3D12SceneTarget.h"
#include "D3D12UploadRing.h"
#include "D3D12GpuTimer.h"
#include "D3D12WorldGeometry.h"
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>

// Enable D3D12 debug layer in debug builds
#ifdef _DEBUG
#define ENABLE_D3D12_DEBUG_LAYER
#endif

//================================================================================
//	Global Variables
//================================================================================
HWND									g_hWnd = nullptr;
ComPtr<ID3D12Device>					g_pDevice;
ComPtr<ID3D12CommandQueue>				g_pCommandQueue;
ComPtr<IDXGISwapChain3>					g_pSwapChain;
ComPtr<ID3D12GraphicsCommandList>		g_pCommandList;
ComPtr<ID3D12DescriptorHeap>			g_pRTVHeap;
ComPtr<ID3D12DescriptorHeap>			g_pDSVHeap;
ComPtr<ID3D12Resource>					g_pDepthStencil;
ComPtr<ID3D12Fence>						g_pFence;

float									g_fGamma = 1.0f;
UINT									g_nRTVDescriptorSize = 0;
UINT									g_nCurrentFrameIndex = 0;
UINT64									g_nFenceValue = 0;
HANDLE									g_hFenceEvent = nullptr;

FrameResources							g_FrameResources[FRAME_COUNT];
D3D12_GPU_VIRTUAL_ADDRESS				g_FrameConstantsGPU = 0;
static LARGE_INTEGER					g_StartTime = {};

bool									g_bInitialized = false;
bool									g_bActive = false;
bool									g_bInScene = false;
bool									g_bWireframe = false;

int32									g_nScreenWidth = 0;
int32									g_nScreenHeight = 0;
static UINT							g_nLastPresentedFrameIndex = 0;
static uint64_t					g_nPresentedFrameCount = 0;

DRV_EngineSettings						g_EngineSettings;
char									g_szLastError[512] = "No error";

// PolyCache for geometry batching
D3D12PolyCache*							g_pPolyCache = nullptr;

// PSO Manager for pipeline states
D3D12PSOManager*						g_pPSOManager = nullptr;

// Global camera pointer for simple storage
static grCamera* g_pCurrentCamera = nullptr;
// Stored transforms
static grXForm3d g_WorldMatrix{};
static grXForm3d g_ViewMatrix{};
static grXForm3d g_ProjectionMatrix{};
static bool g_HasWorld = false;
static bool g_HasView = false;
static bool g_HasProjection = false;

// Gamma LUT similar to D3D9
static struct RGB_LUT { uint32 R[256]; uint32 G[256]; uint32 B[256]; uint32 A[256]; } g_Lut1;
static float g_LocalGamma = 1.0f;

static void BuildRGBGammaTables(float Gamma)
{
    int32_t GammaTable[256];
    if (Gamma == 1.0f)
    {
        for (int i=0;i<256;i++) GammaTable[i]=i;
    }
    else
    {
        for (int i=0;i<256;i++)
        {
            float Ratio = (i + 0.5f) / 255.5f;
            float RGB = (float)(255.0 * pow((double)Ratio, 1.0 / (double)Gamma) + 0.5);
            if (RGB < 0.0f) RGB = 0.0f; if (RGB > 255.0f) RGB = 255.0f;
            GammaTable[i] = (int)RGB;
        }
    }
    for (int i=0;i<256;i++)
    {
        int Val = GammaTable[i];
        // Match D3D9 conversion (565)
        g_Lut1.R[i] = (((uint32)Val >> 3) << 11) & 0xF800;
        g_Lut1.G[i] = (((uint32)Val >> 2) << 5 ) & 0x07E0;
        g_Lut1.B[i] = (((uint32)Val >> 3)      ) & 0x001F;
        g_Lut1.A[i] = 0;
    }
}

// Simple render state mirror storage
static std::unordered_map<uint32,uint32> g_RenderStates;

//================================================================================
//	Utility Functions
//================================================================================

void D3D12Matrix_ToXForm3d(const D3D12Matrix* mat, grXForm3d* XForm)
{
	XForm->AX = mat->m[0][0];
	XForm->AY = mat->m[0][1];
	XForm->AZ = mat->m[0][2];

	XForm->BX = mat->m[1][0];
	XForm->BY = mat->m[1][1];
	XForm->BZ = mat->m[1][2];

	XForm->CX = mat->m[2][0];
	XForm->CY = mat->m[2][1];
	XForm->CZ = mat->m[2][2];

	XForm->Translation.X = mat->m[3][0];
	XForm->Translation.Y = mat->m[3][1];
	XForm->Translation.Z = mat->m[3][2];
}

void grXForm3d_ToD3D12Matrix(const grXForm3d* XForm, D3D12Matrix* mat)
{
	mat->m[0][0] = XForm->AX;
	mat->m[0][1] = XForm->AY;
	mat->m[0][2] = XForm->AZ;
	mat->m[0][3] = 0.0f;

	mat->m[1][0] = XForm->BX;
	mat->m[1][1] = XForm->BY;
	mat->m[1][2] = XForm->BZ;
	mat->m[1][3] = 0.0f;

	mat->m[2][0] = XForm->CX;
	mat->m[2][1] = XForm->CY;
	mat->m[2][2] = XForm->CZ;
	mat->m[2][3] = 0.0f;

	mat->m[3][0] = XForm->Translation.X;
	mat->m[3][1] = XForm->Translation.Y;
	mat->m[3][2] = XForm->Translation.Z;
	mat->m[3][3] = 1.0f;
}

void D3D12WaitForGPU()
{
	if (!g_pCommandQueue || !g_pFence || !g_hFenceEvent)
		return;

	const UINT64 fence = g_nFenceValue;
	g_pCommandQueue->Signal(g_pFence.Get(), fence);
	g_nFenceValue++;

	if (g_pFence->GetCompletedValue() < fence)
	{
		g_pFence->SetEventOnCompletion(fence, g_hFenceEvent);
		WaitForSingleObject(g_hFenceEvent, INFINITE);
	}
}

// Debug-layer messages go to Direct3D12Driver.log, since the tools rarely run
// under a debugger that would show them.
static void __stdcall D3D12DebugMessage(
	D3D12_MESSAGE_CATEGORY,
	D3D12_MESSAGE_SEVERITY Severity,
	D3D12_MESSAGE_ID Id,
	LPCSTR Description,
	void*)
{
	static int Logged = 0;
	const int MaxLogged = 200;
	if (Severity > D3D12_MESSAGE_SEVERITY_WARNING || Logged > MaxLogged)
		return;
	if (++Logged > MaxLogged)
	{
		D3D12Log::GetPtr()->Printf("D3D12 debug layer: further messages suppressed");
		return;
	}
	const char* Level = (Severity == D3D12_MESSAGE_SEVERITY_WARNING) ? "WARNING"
		: (Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) ? "CORRUPTION" : "ERROR";
	D3D12Log::GetPtr()->Printf("D3D12 %s #%d: %s", Level, static_cast<int>(Id), Description ? Description : "");
}

UINT64 D3D12GetCompletedFenceValue()
{
	return g_pFence ? g_pFence->GetCompletedValue() : 0;
}

UINT64 D3D12GetPendingFenceValue()
{
	// The next Signal (end of frame or D3D12WaitForGPU) uses g_nFenceValue.
	return g_nFenceValue;
}

static void MoveToNextFrame()
{
	const UINT SubmittedFrameIndex = g_nCurrentFrameIndex;
	const UINT64 SubmittedFenceValue = g_nFenceValue++;
	g_pCommandQueue->Signal(g_pFence.Get(), SubmittedFenceValue);
	g_FrameResources[SubmittedFrameIndex].FenceValue = SubmittedFenceValue;

	g_nCurrentFrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

	if (g_pFence->GetCompletedValue() < g_FrameResources[g_nCurrentFrameIndex].FenceValue)
	{
		g_pFence->SetEventOnCompletion(g_FrameResources[g_nCurrentFrameIndex].FenceValue, g_hFenceEvent);
		WaitForSingleObject(g_hFenceEvent, INFINITE);
	}

}

//================================================================================
//	Enumeration Functions
//================================================================================

grBoolean DRIVERCC D3D12Drv_EnumSubDrivers(DRV_ENUM_DRV_CB* Cb, void* Context)
{
	D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumSubDrivers START ===");

	if (!Cb)
	{
		D3D12Log::GetPtr()->Printf("ERROR: NULL callback");
		return GR_FALSE;
	}

	D3D12Log::GetPtr()->Printf("Registering DirectX 12 sub-driver");

	try
	{
		// Keep the historical D3D prefix so already-built applications can find
		// this driver while clearly identifying the actual rendering API.
		Cb(0, "(D3D) DirectX 12", Context);
		D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumSubDrivers END SUCCESS ===");
		return GR_TRUE;
	}
	catch (...)
	{
		D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in callback");
		return GR_FALSE;
	}
}

grBoolean DRIVERCC D3D12Drv_EnumModes(S32 Driver, char* DriverName, DRV_ENUM_MODES_CB* Cb, void* Context)
{
    D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumModes START ===");
    if (!Cb) { D3D12Log::GetPtr()->Printf("ERROR: NULL callback in EnumModes"); return GR_FALSE; }
    try
    {
        ComPtr<IDXGIFactory4> pFactory; HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory)); if (FAILED(hr)) return GR_FALSE;
        ComPtr<IDXGIAdapter1> pAdapter; hr = pFactory->EnumAdapters1(0,&pAdapter); if (FAILED(hr)||!pAdapter) return GR_FALSE;
        ComPtr<IDXGIOutput> pOutput; hr = pAdapter->EnumOutputs(0,&pOutput); if (FAILED(hr)||!pOutput) return GR_FALSE;
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM; UINT numModes=0; hr=pOutput->GetDisplayModeList(format,0,&numModes,nullptr); if(FAILED(hr)||numModes==0) return GR_FALSE;
        std::vector<DXGI_MODE_DESC> modes(numModes); hr=pOutput->GetDisplayModeList(format,0,&numModes,modes.data()); if(FAILED(hr)) return GR_FALSE;
        int index=0; for(UINT i=0;i<numModes;i++)
        {
            if (modes[i].Width > 2048 || modes[i].Height > 1024) continue; // mirror limit
            if ((modes[i].RefreshRate.Numerator / modes[i].RefreshRate.Denominator) != 60) continue; // match D3D9 60Hz filter
            char modename[32]; sprintf_s(modename,"%dx%dx32",modes[i].Width,modes[i].Height); // match D3D9 formatting WxHxBpp
            if(!Cb(index,modename,modes[i].Width,modes[i].Height,32,Context)) break; index++;
        }
        Cb(index,"WindowMode",-1,-1,-1,Context); // match D3D9 window mode name
        D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumModes END SUCCESS ===");
        return GR_TRUE;
    }
    catch(...) { D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in EnumModes"); return GR_FALSE; }
}

grBoolean DRIVERCC D3D12Drv_EnumPixelFormats(DRV_ENUM_PFORMAT_CB* Cb, void* Context)
{
    D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumPixelFormats START ===");

    if (!Cb)
    {
        D3D12Log::GetPtr()->Printf("ERROR: NULL callback in EnumPixelFormats");
        return GR_FALSE;
    }

    try
    {
        // Prepare list mirroring legacy driver entries
        const struct { grPixelFormat pf; uint32 flags; const char* desc; } entries[] = {
            { GR_PIXELFORMAT_32BIT_ARGB, RDRIVER_PF_3D | RDRIVER_PF_COMBINE_LIGHTMAP,                 "Base 3D" },
            { GR_PIXELFORMAT_32BIT_ARGB, RDRIVER_PF_3D | RDRIVER_PF_COMBINE_LIGHTMAP | RDRIVER_PF_ALPHA, "3D Alpha" },
            { GR_PIXELFORMAT_32BIT_ARGB, RDRIVER_PF_2D | RDRIVER_PF_CAN_DO_COLORKEY,                  "2D Decal" },
            { GR_PIXELFORMAT_32BIT_ARGB, RDRIVER_PF_LIGHTMAP,                                        "Lightmap" },
            { GR_PIXELFORMAT_16BIT_1555_ARGB, RDRIVER_PF_3D | RDRIVER_PF_COMBINE_LIGHTMAP | RDRIVER_PF_ALPHA, "Legacy 1555" }
        };

        grRDriver_PixelFormat fmt;
        for (const auto &e : entries)
        {
            memset(&fmt, 0, sizeof(fmt));
            fmt.PixelFormat = e.pf;
            fmt.Flags = e.flags;
            D3D12Log::GetPtr()->Printf("EnumPixelFormat: %s PF=0x%08X Flags=0x%08X", e.desc, (uint32)fmt.PixelFormat, fmt.Flags);
            if (!Cb(&fmt, Context))
            {
                D3D12Log::GetPtr()->Printf("Callback returned false; stopping enumeration early.");
                D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumPixelFormats END (Early Stop) ===");
                return GR_TRUE; // Early stop is still success
            }
        }

        D3D12Log::GetPtr()->Printf("=== D3D12Drv_EnumPixelFormats END SUCCESS ===");
        return GR_TRUE;
    }
    catch (...)
    {
        D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in EnumPixelFormats");
        return GR_FALSE;
    }
}

grBoolean DRIVERCC D3D12Drv_GetDeviceCaps(grDeviceCaps* DeviceCaps)
{
	if (!DeviceCaps)
	{
		D3D12Log::GetPtr()->Printf("ERROR: NULL DeviceCaps pointer");
		return GR_FALSE;
	}

	// This is queried for every visible BSP face, so keep it allocation-free and
	// silent. Advertising HWTRANSFORM would route the engine to static buffers,
	// which this transformed-vertex renderer deliberately does not expose.
	DeviceCaps->SuggestedDefaultRenderFlags = GR_RENDER_FLAG_BILINEAR_FILTER;
	DeviceCaps->CanChangeRenderFlags = 0xFFFFFFFF;
	return GR_TRUE;
}

//================================================================================
//	Initialization Functions
//================================================================================

grBoolean DRIVERCC D3D12Drv_Init(DRV_DriverHook* hook)
{
	D3D12Log::GetPtr()->Printf("===========================================");
	D3D12Log::GetPtr()->Printf("D3D12Drv_Init called");

	if (!hook)
	{
		D3D12Log::GetPtr()->Printf("ERROR: NULL hook parameter");
		strcpy_s(g_szLastError, "NULL initialization hook");
		return GR_FALSE;
	}

	D3D12Log::GetPtr()->Printf("Window: 0x%p", hook->hWnd);
	D3D12Log::GetPtr()->Printf("Width: %d, Height: %d", hook->Width, hook->Height);
	D3D12Log::GetPtr()->Printf("Driver: %s", hook->DriverName ? hook->DriverName : "NULL");
	D3D12Log::GetPtr()->Printf("Mode: %s", hook->ModeName ? hook->ModeName : "NULL");
	D3D12Log::GetPtr()->Printf("===========================================");

	if (g_bInitialized)
	{
		D3D12Log::GetPtr()->Printf("WARNING: Driver already initialized - shutting down first");
		D3D12Drv_Shutdown();
	}

	try
	{
		// Reset scene state
		g_bInScene = false;
		g_nPresentedFrameCount = 0;
		g_hWnd = hook->hWnd;
		g_nScreenWidth = hook->Width;
		g_nScreenHeight = hook->Height;

		// Windowed modes use -1 for their dimensions. Resolve those values from
		// the actual render window so the swap chain, viewport, and scissor agree.
		if ((g_nScreenWidth <= 0 || g_nScreenHeight <= 0) && g_hWnd)
		{
			RECT clientRect = {};
			if (GetClientRect(g_hWnd, &clientRect))
			{
				g_nScreenWidth = clientRect.right - clientRect.left;
				g_nScreenHeight = clientRect.bottom - clientRect.top;
			}
		}

		if (g_nScreenWidth <= 0)
			g_nScreenWidth = 1024;
		if (g_nScreenHeight <= 0)
			g_nScreenHeight = 768;

		D3D12Log::GetPtr()->Printf("State variables initialized");

		HRESULT hr;

		// Debug layer: on by default in Debug builds, [Debug] DebugLayer in the ini.
#ifdef ENABLE_D3D12_DEBUG_LAYER
		const bool bDebugLayer = D3D12Config_GetBool("Debug", "DebugLayer", true);
#else
		const bool bDebugLayer = D3D12Config_GetBool("Debug", "DebugLayer", false);
#endif
		if (bDebugLayer)
		{
			D3D12Log::GetPtr()->Printf("Attempting to enable D3D12 debug layer...");
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				D3D12Log::GetPtr()->Printf("D3D12 Debug layer enabled");

				ComPtr<ID3D12Debug1> debugController1;
				if (D3D12Config_GetBool("Debug", "GPUValidation", false) &&
					SUCCEEDED(debugController.As(&debugController1)))
				{
					debugController1->SetEnableGPUBasedValidation(TRUE);
					D3D12Log::GetPtr()->Printf("D3D12 GPU-based validation enabled");
				}
			}
			else
			{
				D3D12Log::GetPtr()->Printf("WARNING: Failed to enable D3D12 debug layer");
			}
		}
		else
		{
			D3D12Log::GetPtr()->Printf("D3D12 debug layer not enabled");
		}

		// Create DXGI Factory
		D3D12Log::GetPtr()->Printf("Creating DXGI Factory...");
		ComPtr<IDXGIFactory4> pFactory;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create DXGI factory - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create DXGI factory");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("DXGI Factory created successfully");

		// Get hardware adapter
		D3D12Log::GetPtr()->Printf("Enumerating adapters...");
		ComPtr<IDXGIAdapter1> pAdapter;
		bool bWarp = false;
		if (D3D12Config_GetBool("Device", "Warp", false))
		{
			// WARP renders the same image on every machine (render regression, CI).
			bWarp = SUCCEEDED(pFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter)));
			if (bWarp)
				D3D12Log::GetPtr()->Printf("Using the WARP software adapter ([Device] Warp)");
			else
				D3D12Log::GetPtr()->Printf("WARNING: WARP adapter unavailable, using hardware");
		}
		for (UINT adapterIndex = 0; !bWarp && DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);

			D3D12Log::GetPtr()->Printf("Adapter %d: %ls", adapterIndex, desc.Description);

			// Skip software adapter
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				D3D12Log::GetPtr()->Printf("  Skipping software adapter");
				continue;
			}

			// Check if adapter supports D3D12
			if (SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				D3D12Log::GetPtr()->Printf("  Found D3D12 compatible adapter!");
				break;
			}
		}

		if (!pAdapter)
		{
			D3D12Log::GetPtr()->Printf("ERROR: No D3D12 compatible adapter found");
			strcpy_s(g_szLastError, "No D3D12 compatible adapter found");
			return GR_FALSE;
		}

		// Create D3D12 Device
		D3D12Log::GetPtr()->Printf("Creating D3D12 Device...");
		hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pDevice));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create D3D12 device - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create D3D12 device");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("D3D12 Device created successfully");

		if (bDebugLayer)
		{
			// ID3D12InfoQueue1 needs a recent debug layer (Windows 11 / Agility SDK).
			ComPtr<ID3D12InfoQueue1> InfoQueue;
			DWORD CallbackCookie = 0;
			if (SUCCEEDED(g_pDevice.As(&InfoQueue)) &&
				SUCCEEDED(InfoQueue->RegisterMessageCallback(
					D3D12DebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &CallbackCookie)))
				D3D12Log::GetPtr()->Printf("D3D12 debug-layer messages are logged here");
			else
				D3D12Log::GetPtr()->Printf("D3D12 debug-layer messages go to the debugger output only");
		}

		// Create Command Queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		D3D12Log::GetPtr()->Printf("Creating command queue...");
		hr = g_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_pCommandQueue));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create command queue - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create command queue");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("Command queue created");

		// Create Swap Chain
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FRAME_COUNT;
		swapChainDesc.Width = g_nScreenWidth > 0 ? g_nScreenWidth : 1024;
		swapChainDesc.Height = g_nScreenHeight > 0 ? g_nScreenHeight : 768;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> pSwapChain1;
		D3D12Log::GetPtr()->Printf("Creating swap chain...");
		hr = pFactory->CreateSwapChainForHwnd(
			g_pCommandQueue.Get(),
			g_hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&pSwapChain1);

		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create swap chain - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create swap chain");
			return GR_FALSE;
		}

		hr = pSwapChain1.As(&g_pSwapChain);
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to query IDXGISwapChain3 - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to query swap chain interface");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("Swap chain created (%dx%d)", swapChainDesc.Width, swapChainDesc.Height);

		// Disable Alt+Enter fullscreen toggle
		pFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

		g_nCurrentFrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
		g_nLastPresentedFrameIndex = g_nCurrentFrameIndex;

		// Create RTV Descriptor Heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FRAME_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		D3D12Log::GetPtr()->Printf("Creating RTV descriptor heap...");
		hr = g_pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pRTVHeap));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create RTV heap - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create RTV descriptor heap");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("RTV descriptor heap created");

		g_nRTVDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create RTVs for each frame
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < FRAME_COUNT; i++)
		{
			hr = g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_FrameResources[i].pRenderTarget));
			if (FAILED(hr))
			{
				D3D12Log::GetPtr()->Printf("ERROR: Failed to get swap chain buffer %d - HR: 0x%08X", i, hr);
				strcpy_s(g_szLastError, "Failed to get swap chain buffers");
				return GR_FALSE;
			}

			g_pDevice->CreateRenderTargetView(g_FrameResources[i].pRenderTarget.Get(), nullptr, rtvHandle);
			g_FrameResources[i].RTVHandle = rtvHandle;
			rtvHandle.ptr += g_nRTVDescriptorSize;

			// Create command allocator for this frame
			hr = g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_FrameResources[i].pCommandAllocator));
			if (FAILED(hr))
			{
				D3D12Log::GetPtr()->Printf("ERROR: Failed to create command allocator %d - HR: 0x%08X", i, hr);
				strcpy_s(g_szLastError, "Failed to create command allocators");
				return GR_FALSE;
			}

			g_FrameResources[i].FenceValue = 0;
		}
		D3D12Log::GetPtr()->Printf("Frame resources created (%d frames)", FRAME_COUNT);

		// Create DSV Descriptor Heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		D3D12Log::GetPtr()->Printf("Creating DSV descriptor heap...");
		hr = g_pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_pDSVHeap));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create DSV heap - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create DSV descriptor heap");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("DSV descriptor heap created");

		// Create Depth/Stencil Buffer
		D3D12_RESOURCE_DESC depthStencilDesc = {};
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = swapChainDesc.Width;
		depthStencilDesc.Height = swapChainDesc.Height;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = DXGI_FORMAT_R32_TYPELESS;	// sampled as R32_FLOAT by later passes
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12Log::GetPtr()->Printf("Creating depth/stencil buffer (%dx%d)...", swapChainDesc.Width, swapChainDesc.Height);
		hr = g_pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&g_pDepthStencil));

		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create depth/stencil buffer - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create depth/stencil buffer");
			return GR_FALSE;
		}

		// Create Depth/Stencil View
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
		g_pDevice->CreateDepthStencilView(g_pDepthStencil.Get(), &dsvDesc, dsvHandle);
		D3D12Log::GetPtr()->Printf("Depth/stencil buffer created");

		// Create Command List
		hr = g_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			g_FrameResources[0].pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_pCommandList));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create command list - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create command list");
			return GR_FALSE;
		}

		// Command lists are created in recording state, close it
		g_pCommandList->Close();
		D3D12Log::GetPtr()->Printf("Command list created");

		// Create Fence
		hr = g_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pFence));
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create fence - HR: 0x%08X", hr);
			strcpy_s(g_szLastError, "Failed to create fence");
			return GR_FALSE;
		}

		g_nFenceValue = 1;

		// Create fence event
		g_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!g_hFenceEvent)
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to create fence event");
			strcpy_s(g_szLastError, "Failed to create fence event");
			return GR_FALSE;
		}
		D3D12Log::GetPtr()->Printf("Fence created");

		// Initialize texture manager
		if (!D3D12_THandle_Startup())
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to initialize texture manager");
			strcpy_s(g_szLastError, "Failed to initialize texture manager");
			return GR_FALSE;
		}

		// The scene target's SRV lives in the texture heap's reserved slots.
		if (!D3D12Scene_Create(swapChainDesc.Width, swapChainDesc.Height))
		{
			strcpy_s(g_szLastError, "Failed to create the scene target");
			return GR_FALSE;
		}

		if (!D3D12Upload_Startup())
		{
			strcpy_s(g_szLastError, "Failed to create the upload ring");
			return GR_FALSE;
		}

		// Optional: without timestamps the overlay just omits GPU times.
		D3D12Timer_Startup();
		QueryPerformanceCounter(&g_StartTime);

		// Create and initialize PolyCache
		g_pPolyCache = new D3D12PolyCache();
		if (!g_pPolyCache || !g_pPolyCache->Initialize(10000))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to initialize PolyCache");
			strcpy_s(g_szLastError, "Failed to initialize PolyCache");
			return GR_FALSE;
		}

		// Create and initialize PSO Manager
		g_pPSOManager = new D3D12PSOManager();
		if (!g_pPSOManager || !g_pPSOManager->Initialize())
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to initialize PSO Manager");
			strcpy_s(g_szLastError, "Failed to initialize PSO Manager");
			return GR_FALSE;
		}

		// Needs the PSO manager, which decides whether textures are bindless.
		D3D12World_Startup();

		g_bInitialized = true;
		g_bActive = true;

		D3D12Log::GetPtr()->Printf("===========================================");
		D3D12Log::GetPtr()->Printf("DirectX 12 Driver initialized successfully!");
		D3D12Log::GetPtr()->Printf("===========================================");

		return GR_TRUE;
	}
	catch (const std::exception& e)
	{
		D3D12Log::GetPtr()->Printf("ERROR: Exception during initialization: %s", e.what());
		strcpy_s(g_szLastError, "Exception during initialization");
		return GR_FALSE;
	}
}

grBoolean DRIVERCC D3D12Drv_Shutdown()
{
	D3D12Log::GetPtr()->Printf("D3D12Drv_Shutdown called");

	if (!g_bInitialized)
	{
		D3D12Log::GetPtr()->Printf("WARNING: Driver not initialized");
		return GR_TRUE;
	}

	// Nothing may be released while the GPU could still be using it.
	D3D12WaitForGPU();

	// Shutdown PSO Manager
	if (g_pPSOManager)
	{
		g_pPSOManager->Shutdown();
		delete g_pPSOManager;
		g_pPSOManager = nullptr;
	}

	// Shutdown PolyCache
	if (g_pPolyCache)
	{
		g_pPolyCache->Shutdown();
		delete g_pPolyCache;
		g_pPolyCache = nullptr;
	}

	// Shutdown texture manager
	D3D12_THandle_Shutdown();

	D3D12World_Shutdown();
	D3D12Scene_Shutdown();
	D3D12Upload_Shutdown();
	D3D12Timer_Shutdown();
	g_FrameConstantsGPU = 0;

	// Close fence event
	if (g_hFenceEvent)
	{
		CloseHandle(g_hFenceEvent);
		g_hFenceEvent = nullptr;
	}

	// Release resources
	g_pFence.Reset();
	g_pCommandList.Reset();

	for (UINT i = 0; i < FRAME_COUNT; i++)
	{
		g_FrameResources[i].pCommandAllocator.Reset();
		g_FrameResources[i].pRenderTarget.Reset();
		g_FrameResources[i].FenceValue = 0;
	}

	g_pRTVHeap.Reset();
	g_pDSVHeap.Reset();
	g_pDepthStencil.Reset();
	g_pSwapChain.Reset();
	g_pCommandQueue.Reset();
	g_pDevice.Reset();

	g_bInitialized = false;
	g_bActive = false;
	g_bWireframe = false;
	g_hWnd = nullptr;

	D3D12Log::GetPtr()->Printf("DirectX 12 Driver shut down successfully");
	D3D12Log::Destroy();

	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_Reset()
{
    D3D12Log::GetPtr()->Printf("D3D12Drv_Reset called (parity with D3D9)");
	    D3D12WaitForGPU();
    D3D12_THandle_Shutdown();
    if (g_pPolyCache) g_pPolyCache->Shutdown();
    if (!D3D12_THandle_Startup()) { D3D12Log::GetPtr()->Printf("ERROR: Texture manager restart failed"); return GR_FALSE; }
    if (!D3D12Scene_Create(static_cast<UINT>(g_nScreenWidth), static_cast<UINT>(g_nScreenHeight))) { D3D12Log::GetPtr()->Printf("ERROR: Scene target re-init failed"); return GR_FALSE; }
    if (g_pPolyCache && !g_pPolyCache->Initialize(10000)) { D3D12Log::GetPtr()->Printf("ERROR: PolyCache re-init failed"); return GR_FALSE; }
    return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_UpdateWindow()
{
	if (!g_bInitialized || !g_hWnd || !g_pDevice || !g_pSwapChain)
		return GR_FALSE;
	if (g_bInScene)
	{
		D3D12Log::GetPtr()->Printf("ERROR: UpdateWindow called during a scene");
		return GR_FALSE;
	}

	RECT ClientRect = {};
	if (!GetClientRect(g_hWnd, &ClientRect))
		return GR_FALSE;
	const int32 Width = ClientRect.right - ClientRect.left;
	const int32 Height = ClientRect.bottom - ClientRect.top;
	// A minimized window has no drawable client area. Keep the current buffers and
	// resize them when the application restores the window.
	if (Width <= 0 || Height <= 0)
		return GR_TRUE;
	if (Width == g_nScreenWidth && Height == g_nScreenHeight)
		return GR_TRUE;

	D3D12Log::GetPtr()->Printf("Resizing swap chain from %dx%d to %dx%d",
		g_nScreenWidth, g_nScreenHeight, Width, Height);
	D3D12WaitForGPU();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		D3D12_THandle_BeginFrame(i);
		D3D12Upload_BeginFrame(i);
		if (g_pPolyCache)
			g_pPolyCache->BeginFrame(i);
		g_FrameResources[i].pRenderTarget.Reset();
		g_FrameResources[i].FenceValue = 0;
	}
	g_pDepthStencil.Reset();
	D3D12Scene_Release();

	HRESULT Hr = g_pSwapChain->ResizeBuffers(
		FRAME_COUNT, static_cast<UINT>(Width), static_cast<UINT>(Height),
		DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: ResizeBuffers failed - HR: 0x%08X", Hr);
		return GR_FALSE;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle =
		g_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		Hr = g_pSwapChain->GetBuffer(
			i, IID_PPV_ARGS(&g_FrameResources[i].pRenderTarget));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf(
				"ERROR: Failed to reacquire resized back buffer %u - HR: 0x%08X", i, Hr);
			return GR_FALSE;
		}
		g_pDevice->CreateRenderTargetView(
			g_FrameResources[i].pRenderTarget.Get(), nullptr, RtvHandle);
		g_FrameResources[i].RTVHandle = RtvHandle;
		RtvHandle.ptr += g_nRTVDescriptorSize;
	}

	D3D12_RESOURCE_DESC DepthDesc = {};
	DepthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthDesc.Width = static_cast<UINT64>(Width);
	DepthDesc.Height = static_cast<UINT>(Height);
	DepthDesc.DepthOrArraySize = 1;
	DepthDesc.MipLevels = 1;
	DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_HEAP_PROPERTIES HeapProps = {};
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	ClearValue.DepthStencil.Depth = 1.0f;
	Hr = g_pDevice->CreateCommittedResource(
		&HeapProps, D3D12_HEAP_FLAG_NONE, &DepthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue,
		IID_PPV_ARGS(&g_pDepthStencil));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf(
			"ERROR: Failed to recreate depth buffer - HR: 0x%08X", Hr);
		return GR_FALSE;
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	g_pDevice->CreateDepthStencilView(
		g_pDepthStencil.Get(), &DsvDesc,
		g_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	if (!D3D12Scene_Create(static_cast<UINT>(Width), static_cast<UINT>(Height)))
		return GR_FALSE;

	g_nScreenWidth = Width;
	g_nScreenHeight = Height;
	g_nCurrentFrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
	g_nLastPresentedFrameIndex = g_nCurrentFrameIndex;
	D3D12Log::GetPtr()->Printf("Swap chain resize completed");
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_SetActive(grBoolean Active)
{
	D3D12Log::GetPtr()->Printf("D3D12Drv_SetActive: %d", Active);

	g_bActive = (Active != GR_FALSE);

	return GR_TRUE;
}

//================================================================================
//	Scene Management Functions
//================================================================================

grBoolean DRIVERCC D3D12Drv_BeginScene(grBoolean Clear, grBoolean ClearZ, RECT* WorldRect, grBoolean Wireframe)
{
	if (!g_bInitialized)
	{
		D3D12Log::GetPtr()->Printf("ERROR: BeginScene called before driver initialization");
		return GR_FALSE;
	}

	if (!g_pDevice || !g_pCommandList || !g_pSwapChain)
	{
		D3D12Log::GetPtr()->Printf("ERROR: BeginScene called with invalid D3D12 resources");
		return GR_FALSE;
	}

	// If already in scene, end it first to maintain proper state
	if (g_bInScene)
	{
		D3D12Log::GetPtr()->Printf("WARNING: BeginScene called while already in scene - ending previous scene");
		D3D12Drv_EndScene();  // This will now always succeed and clean up state
	}

	try
	{
		// Reset command allocator for this frame
		HRESULT hr = g_FrameResources[g_nCurrentFrameIndex].pCommandAllocator->Reset();
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to reset command allocator - HR: 0x%08X", hr);
			return GR_FALSE;
		}

		// Reset command list
		hr = g_pCommandList->Reset(g_FrameResources[g_nCurrentFrameIndex].pCommandAllocator.Get(), nullptr);
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to reset command list - HR: 0x%08X", hr);
			return GR_FALSE;
		}

		// The frame-slot fence has completed before this allocator is reused, so
		// transient vertex and texture upload resources for this slot can be released,
		// and its GPU timestamps can be read.
		D3D12Timer_BeginFrame(g_nCurrentFrameIndex);
		D3D12Upload_BeginFrame(g_nCurrentFrameIndex);
		D3D12World_BeginFrame();
		D3D12Timer_Mark(g_pCommandList.Get(), GPU_MARK_SCENE_BEGIN);
		D3D12BeginMarker(g_pCommandList.Get(), "Scene");

		// One shader-visible heap (textures + reserved driver slots) for the whole frame.
		ID3D12DescriptorHeap* TextureHeap = D3D12_THandle_GetDescriptorHeap();
		g_pCommandList->SetDescriptorHeaps(1, &TextureHeap);

		// Per-frame constants (FrameConstants, b1).
		D3D12UploadAllocation FrameUpload = {};
		if (!D3D12Upload_Allocate(sizeof(D3D12FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &FrameUpload))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Frame constants allocation failed");
			g_pCommandList->Close();
			return GR_FALSE;
		}
		{
			LARGE_INTEGER Now = {}, Frequency = {};
			QueryPerformanceCounter(&Now);
			QueryPerformanceFrequency(&Frequency);
			const float Width = static_cast<float>(g_nScreenWidth > 1 ? g_nScreenWidth : 1);
			const float Height = static_cast<float>(g_nScreenHeight > 1 ? g_nScreenHeight : 1);
			D3D12FrameConstants Constants = {};
			Constants.ViewportSize[0] = Width;
			Constants.ViewportSize[1] = Height;
			Constants.InvViewportSize[0] = 1.0f / Width;
			Constants.InvViewportSize[1] = 1.0f / Height;
			Constants.FrameNumber = static_cast<uint32>(g_nPresentedFrameCount);
			Constants.TimeSeconds = static_cast<float>(
				static_cast<double>(Now.QuadPart - g_StartTime.QuadPart) / static_cast<double>(Frequency.QuadPart));
			std::memcpy(FrameUpload.CPU, &Constants, sizeof(Constants));
			g_FrameConstantsGPU = FrameUpload.GPU;
		}

		D3D12_THandle_BeginFrame(g_nCurrentFrameIndex);
		if (g_pPolyCache)
			g_pPolyCache->BeginFrame(g_nCurrentFrameIndex);
		g_bWireframe = (Wireframe != GR_FALSE);
		g_D3D12Drv.NumRenderedPolys = 0;

		// Transition render target to RENDER_TARGET state
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_FrameResources[g_nCurrentFrameIndex].pRenderTarget.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		g_pCommandList->ResourceBarrier(1, &barrier);

		// Set render target with depth/stencil. With the scene target on, the engine
		// draws into it and EndScene's present pass fills the back buffer.
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = D3D12Scene_IsEnabled()
			? D3D12Scene_Begin(g_pCommandList.Get())
			: g_FrameResources[g_nCurrentFrameIndex].RTVHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
		g_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		// Clear the render target when requested by the engine.
		if (Clear)
		{
			const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			g_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		}

		// Clear depth/stencil if requested
		if (ClearZ)
		{
			g_pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}

		// Set viewport and scissor rect
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = static_cast<float>(g_nScreenWidth > 0 ? g_nScreenWidth : 1024);
		viewport.Height = static_cast<float>(g_nScreenHeight > 0 ? g_nScreenHeight : 768);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		g_pCommandList->RSSetViewports(1, &viewport);

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = g_nScreenWidth > 0 ? g_nScreenWidth : 1024;
		scissorRect.bottom = g_nScreenHeight > 0 ? g_nScreenHeight : 768;
		g_pCommandList->RSSetScissorRects(1, &scissorRect);

		g_bInScene = true;

		return GR_TRUE;
	}
	catch (...)
	{
		D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in BeginScene");
		g_bInScene = false;
		return GR_FALSE;
	}
}

grBoolean DRIVERCC D3D12Drv_EndScene(void)
{
	if (!g_bInScene)
	{
		D3D12Log::GetPtr()->Printf("WARNING: EndScene called without matching BeginScene - allowing anyway");
		return GR_TRUE;  // Return success to avoid engine state mismatch
	}

	// Even if not fully initialized, we need to clear the scene state
	if (!g_bInitialized)
	{
		D3D12Log::GetPtr()->Printf("WARNING: EndScene called but driver not initialized - clearing state anyway");
		g_bInScene = false;
		return GR_TRUE;  // Return success to avoid engine state mismatch
	}

	try
	{
		// Flush any cached geometry
		if (g_pPolyCache && !g_pPolyCache->Flush())
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to flush DX12 polygon cache");
			g_bInScene = false;
			return GR_FALSE;
		}

		D3D12Timer_Mark(g_pCommandList.Get(), GPU_MARK_PRESENT_BEGIN);
		if (D3D12Scene_IsEnabled() &&
			!D3D12Scene_Present(g_pCommandList.Get(), g_FrameResources[g_nCurrentFrameIndex].RTVHandle))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Present pass failed");
			g_bInScene = false;
			return GR_FALSE;
		}
		D3D12EndMarker(g_pCommandList.Get());
		D3D12Timer_Mark(g_pCommandList.Get(), GPU_MARK_FRAME_END);
		D3D12Timer_EndFrame(g_pCommandList.Get());

		// Transition render target back to PRESENT state
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_FrameResources[g_nCurrentFrameIndex].pRenderTarget.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		g_pCommandList->ResourceBarrier(1, &barrier);

		// Close and execute command list
		HRESULT hr = g_pCommandList->Close();
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to close command list - HR: 0x%08X", hr);
			g_bInScene = false;
			return GR_FALSE;
		}

		const UINT SubmittedFrameIndex = g_nCurrentFrameIndex;
		ID3D12CommandList* ppCommandLists[] = { g_pCommandList.Get() };
		g_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present
		hr = g_pSwapChain->Present(1, 0);
		if (FAILED(hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Failed to present - HR: 0x%08X", hr);
			g_bInScene = false;
			return GR_FALSE;
		}
		g_nLastPresentedFrameIndex = SubmittedFrameIndex;

		// Move to next frame
		MoveToNextFrame();

		g_bInScene = false;
		++g_nPresentedFrameCount;
		if (g_nPresentedFrameCount == 1 || (g_nPresentedFrameCount % 600) == 0)
		{
			D3D12Log::GetPtr()->Printf(
				"Presented frame %llu (%d polygon(s))",
				static_cast<unsigned long long>(g_nPresentedFrameCount),
				g_D3D12Drv.NumRenderedPolys);
		}

		return GR_TRUE;
	}
	catch (...)
	{
		D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in EndScene");
		g_bInScene = false;
		return GR_FALSE;
	}
}

grBoolean DRIVERCC D3D12Drv_BeginBatch(void)
{
	// For now, batching is handled internally
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_EndBatch(void)
{
	// For now, batching is handled internally
	return GR_TRUE;
}

//================================================================================
//	Rendering Functions
//================================================================================

grBoolean DRIVERCC D3D12Drv_RenderGouraudPoly(grTLVertex* Pnts, int32 NumPoints, uint32 Flags)
{
	if (!g_pPolyCache)
		return GR_FALSE;

	return g_pPolyCache->AddGouraudPoly(Pnts, NumPoints, Flags);
}

grBoolean DRIVERCC D3D12Drv_RenderWorldPoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, void* LMapCBContext, uint32 Flags)
{
	if (!g_pPolyCache)
		return GR_FALSE;

	return g_pPolyCache->AddWorldPoly(Pnts, NumPoints, Layers, NumLayers, LMapCBContext, Flags);
}

grBoolean DRIVERCC D3D12Drv_RenderMiscTexturePoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags)
{
	if (!g_pPolyCache)
		return GR_FALSE;

	return g_pPolyCache->AddMiscTexturePoly(Pnts, NumPoints, Layers, NumLayers, Flags);
}

grBoolean DRIVERCC D3D12Drv_DrawDecal(grTexture* Handle, RECT* SrcRect, int32 x, int32 y)
{
	if (!g_bInScene || !g_pPolyCache || !Handle || !Handle->Active)
		return GR_FALSE;

	RECT Source = { 0, 0, Handle->Width, Handle->Height };
	if (SrcRect)
		Source = *SrcRect;
	const int32 Width = Source.right - Source.left;
	const int32 Height = Source.bottom - Source.top;
	if (Width <= 0 || Height <= 0)
		return GR_FALSE;

	const float U0 = static_cast<float>(Source.left) / Handle->Width;
	const float V0 = static_cast<float>(Source.top) / Handle->Height;
	const float U1 = static_cast<float>(Source.right) / Handle->Width;
	const float V1 = static_cast<float>(Source.bottom) / Handle->Height;
	// Quad edges on pixel edges, so each texel lands on one pixel. TLPoly.hlsl adds
	// half a pixel (engine coordinates put pixel centers on integers), so take it off.
	grTLVertex Vertices[4] = {};
	const float Left = static_cast<float>(x) - 0.5f;
	const float Top = static_cast<float>(y) - 0.5f;
	const float X[4] = { Left, Left + Width, Left + Width, Left };
	const float Y[4] = { Top, Top, Top + Height, Top + Height };
	const float U[4] = { U0, U1, U1, U0 };
	const float V[4] = { V0, V0, V1, V1 };
	for (int32 i = 0; i < 4; ++i)
	{
		Vertices[i].x = X[i];
		Vertices[i].y = Y[i];
		Vertices[i].z = 1.0f;
		Vertices[i].r = Vertices[i].g = Vertices[i].b = Vertices[i].a = 255.0f;
		Vertices[i].u = U[i];
		Vertices[i].v = V[i];
	}

	grRDriver_Layer Layer = {};
	Layer.THandle = Handle;
	Layer.ScaleU = Layer.ScaleV = 1.0f;
	uint32 Flags = GR_RENDER_FLAG_ALPHA | GR_RENDER_FLAG_CLAMP_UV |
		GR_RENDER_FLAG_NO_ZTEST | GR_RENDER_FLAG_NO_ZWRITE | GR_RENDER_FLAG_BILINEAR_FILTER;
	if (Handle->DriverFormat.Flags & RDRIVER_PF_CAN_DO_COLORKEY)
		Flags |= GR_RENDER_FLAG_COLORKEY;
	return g_pPolyCache->AddMiscTexturePoly(Vertices, 4, &Layer, 1, Flags);
}

grBoolean DRIVERCC D3D12Drv_Screenshot(const char* filename)
{
	if (!g_pDevice || !g_pCommandQueue || !g_pSwapChain || g_bInScene)
		return GR_FALSE;

	ComPtr<ID3D12Resource> BackBuffer = g_FrameResources[g_nLastPresentedFrameIndex].pRenderTarget;
	if (!BackBuffer)
		return GR_FALSE;
	D3D12WaitForGPU();

	const D3D12_RESOURCE_DESC TextureDesc = BackBuffer->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint = {};
	UINT NumRows = 0;
	UINT64 RowSize = 0;
	UINT64 ReadbackSize = 0;
	g_pDevice->GetCopyableFootprints(
		&TextureDesc, 0, 1, 0, &Footprint, &NumRows, &RowSize, &ReadbackSize);

	D3D12_HEAP_PROPERTIES ReadbackHeap = {};
	ReadbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC BufferDesc = {};
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Width = ReadbackSize;
	BufferDesc.Height = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> Readback;
	HRESULT Hr = g_pDevice->CreateCommittedResource(
		&ReadbackHeap,
		D3D12_HEAP_FLAG_NONE,
		&BufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&Readback));
	if (FAILED(Hr))
		return GR_FALSE;

	ComPtr<ID3D12CommandAllocator> Allocator;
	if (FAILED(g_pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator))))
		return GR_FALSE;
	ComPtr<ID3D12GraphicsCommandList> CommandList;
	if (FAILED(g_pDevice->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator.Get(), nullptr,
		IID_PPV_ARGS(&CommandList))))
		return GR_FALSE;

	D3D12_RESOURCE_BARRIER ToCopy = {};
	ToCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ToCopy.Transition.pResource = BackBuffer.Get();
	ToCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	ToCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	ToCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	CommandList->ResourceBarrier(1, &ToCopy);

	D3D12_TEXTURE_COPY_LOCATION Destination = {};
	Destination.pResource = Readback.Get();
	Destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Destination.PlacedFootprint = Footprint;
	D3D12_TEXTURE_COPY_LOCATION Source = {};
	Source.pResource = BackBuffer.Get();
	Source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Source.SubresourceIndex = 0;
	CommandList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);

	D3D12_RESOURCE_BARRIER ToPresent = ToCopy;
	ToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	ToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	CommandList->ResourceBarrier(1, &ToPresent);
	if (FAILED(CommandList->Close()))
		return GR_FALSE;
	ID3D12CommandList* Lists[] = { CommandList.Get() };
	g_pCommandQueue->ExecuteCommandLists(1, Lists);
	D3D12WaitForGPU();

	void* Mapped = nullptr;
	D3D12_RANGE ReadRange = { 0, static_cast<SIZE_T>(ReadbackSize) };
	if (FAILED(Readback->Map(0, &ReadRange, &Mapped)))
		return GR_FALSE;

	const char* OutputName = filename ? filename : "screenshot.bmp";
	FILE* File = nullptr;
	if (fopen_s(&File, OutputName, "wb") != 0)
	{
		Readback->Unmap(0, nullptr);
		return GR_FALSE;
	}

	const uint32 Width = static_cast<uint32>(TextureDesc.Width);
	const uint32 Height = TextureDesc.Height;
	const uint32 PixelBytes = Width * 4;
	const uint32 FileSize = 54 + PixelBytes * Height;
	uint8 Header[54] = {};
	Header[0] = 'B';
	Header[1] = 'M';
	std::memcpy(&Header[2], &FileSize, sizeof(FileSize));
	const uint32 PixelOffset = 54;
	const uint32 InfoSize = 40;
	std::memcpy(&Header[10], &PixelOffset, sizeof(PixelOffset));
	std::memcpy(&Header[14], &InfoSize, sizeof(InfoSize));
	std::memcpy(&Header[18], &Width, sizeof(Width));
	const int32 TopDownHeight = -static_cast<int32>(Height);
	std::memcpy(&Header[22], &TopDownHeight, sizeof(TopDownHeight));
	const uint16 Planes = 1;
	const uint16 BitsPerPixel = 32;
	std::memcpy(&Header[26], &Planes, sizeof(Planes));
	std::memcpy(&Header[28], &BitsPerPixel, sizeof(BitsPerPixel));
	std::fwrite(Header, 1, sizeof(Header), File);

	std::vector<uint8> BgraRow(PixelBytes);
	const uint8* Pixels = static_cast<const uint8*>(Mapped) + Footprint.Offset;
	for (uint32 Row = 0; Row < Height; ++Row)
	{
		const uint8* Rgba = Pixels + static_cast<size_t>(Row) * Footprint.Footprint.RowPitch;
		for (uint32 X = 0; X < Width; ++X)
		{
			BgraRow[X * 4 + 0] = Rgba[X * 4 + 2];
			BgraRow[X * 4 + 1] = Rgba[X * 4 + 1];
			BgraRow[X * 4 + 2] = Rgba[X * 4 + 0];
			BgraRow[X * 4 + 3] = Rgba[X * 4 + 3];
		}
		std::fwrite(BgraRow.data(), 1, BgraRow.size(), File);
	}
	std::fclose(File);
	D3D12_RANGE NoWrites = { 0, 0 };
	Readback->Unmap(0, &NoWrites);
	D3D12Log::GetPtr()->Printf("Screenshot written: %s", OutputName);
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_SetGamma(float gamma)
{
    g_fGamma = gamma; g_LocalGamma = gamma; BuildRGBGammaTables(gamma); D3D12Log::GetPtr()->Printf("SetGamma: %f", gamma); return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_GetGamma(float* gamma)
{
    if (gamma) *gamma = g_LocalGamma; return GR_TRUE;
}

// Single SetCamera implementation (avoid duplicate definitions)
grBoolean DRIVERCC D3D12Drv_SetCamera(grCamera* Camera)
{
    g_pCurrentCamera = Camera;
    D3D12Log::GetPtr()->Printf("D3D12Drv_SetCamera: camera pointer %p stored", Camera);
    return GR_TRUE;
}

// Implement missing matrix and static mesh APIs
grBoolean DRIVERCC D3D12Drv_SetMatrix(uint32 Type, grXForm3d* Matrix)
{
    if (!Matrix)
        return GR_FALSE;
    switch (Type)
    {
        case GR_XFORM_TYPE_WORLD:      g_WorldMatrix = *Matrix;      g_HasWorld = true;      break;
        case GR_XFORM_TYPE_VIEW:       g_ViewMatrix = *Matrix;       g_HasView = true;       break;
        case GR_XFORM_TYPE_PROJECTION: g_ProjectionMatrix = *Matrix; g_HasProjection = true; break;
        default: return GR_FALSE;
    }
    return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_GetMatrix(uint32 Type, grXForm3d* Matrix)
{
    if (!Matrix)
        return GR_FALSE;
    switch (Type)
    {
        case GR_XFORM_TYPE_WORLD:      if (!g_HasWorld) return GR_FALSE;      *Matrix = g_WorldMatrix;      return GR_TRUE;
        case GR_XFORM_TYPE_VIEW:       if (!g_HasView) return GR_FALSE;       *Matrix = g_ViewMatrix;       return GR_TRUE;
        case GR_XFORM_TYPE_PROJECTION: if (!g_HasProjection) return GR_FALSE; *Matrix = g_ProjectionMatrix; return GR_TRUE;
        default: return GR_FALSE;
    }
}

uint32 DRIVERCC D3D12Drv_CreateStaticMesh(grHWVertex* Points, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags)
{
    if (!g_pPolyCache || !Points || NumPoints <= 0)
        return 0; // 0 == failure id
    return g_pPolyCache->AddStaticBuffer(Points, NumPoints, Layers, NumLayers, Flags);
}

grBoolean DRIVERCC D3D12Drv_RemoveStaticMesh(uint32 id)
{
    if (!g_pPolyCache || id == 0)
        return GR_FALSE;
    return g_pPolyCache->RemoveStaticBuffer(id);
}

grBoolean DRIVERCC D3D12Drv_RenderStaticMesh(uint32 id, int32 StartVertex, int32 NumPolys, grXForm3d* XForm)
{
    if (!g_pPolyCache || id == 0)
        return GR_FALSE;
    return g_pPolyCache->RenderStaticBuffer(id, StartVertex, NumPolys, XForm);
}

//================================================================================
//	Driver Structure
//================================================================================

DRV_Driver g_D3D12Drv =
{
	"DirectX 12 Driver v1.0",
	DRV_VERSION_MAJOR,
	DRV_VERSION_MINOR,

	DRV_ERROR_NONE,
	g_szLastError,

	D3D12Drv_EnumSubDrivers,
	D3D12Drv_EnumModes,
	D3D12Drv_EnumPixelFormats,

	D3D12Drv_GetDeviceCaps,

	D3D12Drv_Init,
	D3D12Drv_Shutdown,
	D3D12Drv_Reset,
	D3D12Drv_UpdateWindow,
	D3D12Drv_SetActive,

	D3D12_THandle_Create,
	nullptr,  // THandle_CreateFromFile - Set to NULL like D3D driver
	D3D12_THandle_Destroy,

	D3D12_THandle_Lock,
	D3D12_THandle_Unlock,

	nullptr,  // SetPalette
	nullptr,  // GetPalette

	nullptr,  // SetAlpha
	nullptr,  // GetAlpha

	D3D12_THandle_GetInfo,

	D3D12Drv_BeginScene,
	D3D12Drv_EndScene,

	D3D12Drv_BeginBatch,
	D3D12Drv_EndBatch,

	D3D12Drv_RenderGouraudPoly,
	D3D12Drv_RenderWorldPoly,
	D3D12Drv_RenderMiscTexturePoly,

	D3D12Drv_DrawDecal,

	0, 0, 0,  // NumWorldPixels, NumWorldSpans, NumRenderedPolys
	nullptr,  // CacheInfo

	D3D12Drv_Screenshot,

	D3D12Drv_SetGamma,
	D3D12Drv_GetGamma,

	D3D12Drv_SetMatrix,
	D3D12Drv_GetMatrix,
	D3D12Drv_SetCamera,  // SetCamera now implemented

	&g_EngineSettings,

	nullptr,  // SetupLightmap

	nullptr,  // DrawText: none, so the engine draws its bitmap font through DrawDecal
	nullptr,  // SetFog

	D3D12Drv_CreateStaticMesh,
	D3D12Drv_RemoveStaticMesh,
	D3D12Drv_RenderStaticMesh,

	D3D12Drv_CreateFont,
	D3D12Drv_DrawFont,
	D3D12Drv_DestroyFont,

	D3D12Drv_SetRenderState,

	nullptr,	// GPUTimings, set in DriverHook

	D3D12World_Create,
	D3D12World_Destroy,
	D3D12World_RenderFace,
	D3D12World_RenderMesh,
	D3D12World_SetLights,
	D3D12World_RenderFacePBR,
	D3D12_THandle_CreateFromDDS
};

//================================================================================
//	Driver Entry Points
//================================================================================

extern "C" DRIVERAPI BOOL DriverHook(DRV_Driver** Driver)
{
	// Create log file IMMEDIATELY - before anything else
	D3D12Log::GetPtr()->Printf("===========================================");
	D3D12Log::GetPtr()->Printf("DriverHook called - D3D12 Driver Loading");
	D3D12Log::GetPtr()->Printf("Driver pointer location: 0x%p", Driver);
	D3D12Log::GetPtr()->Printf("===========================================");

	if (!Driver)
	{
		D3D12Log::GetPtr()->Printf("ERROR: NULL Driver pointer passed to DriverHook");
		return FALSE;
	}

	try
	{
		// Verify driver structure integrity
		D3D12Log::GetPtr()->Printf("Initializing driver structure...");
		D3D12Log::GetPtr()->Printf("Driver structure at: 0x%p", &g_D3D12Drv);
		D3D12Log::GetPtr()->Printf("Driver name: %s", g_D3D12Drv.Name);
		D3D12Log::GetPtr()->Printf("Driver version: %d.%d", g_D3D12Drv.VersionMajor, g_D3D12Drv.VersionMinor);
		D3D12Log::GetPtr()->Printf("Init function: 0x%p", g_D3D12Drv.Init);
		D3D12Log::GetPtr()->Printf("BeginScene function: 0x%p", g_D3D12Drv.BeginScene);
		D3D12Log::GetPtr()->Printf("EndScene function: 0x%p", g_D3D12Drv.EndScene);

		g_EngineSettings.CanSupportFlags = (DRV_SUPPORT_ALPHA | DRV_SUPPORT_COLORKEY | DRV_SUPPORT_GAMMA);
		g_EngineSettings.PreferenceFlags = 0;

		g_D3D12Drv.GPUTimings = D3D12Timer_GetTimings();
		*Driver = &g_D3D12Drv;

		D3D12Log::GetPtr()->Printf("DriverHook successful - returning driver structure at 0x%p", *Driver);
		D3D12Log::GetPtr()->Printf("===========================================");
		return TRUE;
	}
	catch (...)
	{
		D3D12Log::GetPtr()->Printf("CRITICAL ERROR: Exception in DriverHook");
		return FALSE;
	}
}

extern "C" DRIVERAPI void* grEngine_D3D12Driver(void)
{
	D3D12Log::GetPtr()->Printf("grEngine_D3D12Driver called");
	return (void*)DriverHook;
}

// Basic font struct mimic (reuse grFont from D3D9 style if not defined for D3D12)
struct grFont { void* Reserved; }; // placeholder

grFont* DRIVERCC D3D12Drv_CreateFont(int32 Height, int32 Width, uint32 Weight, grBoolean Italic, const char* facename)
{
    // Stub: return simple allocated object
    grFont* f = new grFont();
    return f;
}

grBoolean DRIVERCC D3D12Drv_DrawFont(grFont* Font, int32 x, int32 y, uint32 Color, const char* text)
{
    // Stub: no actual draw yet
    return (Font && text) ? GR_TRUE : GR_FALSE;
}

grBoolean DRIVERCC D3D12Drv_DestroyFont(grFont** Font)
{
    if (!Font || !*Font) return GR_FALSE;
    delete *Font; *Font = nullptr; return GR_TRUE;
}

grBoolean DRIVERCC D3D12Drv_SetRenderState(uint32 state, uint32 value)
{
    g_RenderStates[state] = value; return GR_TRUE;
}


