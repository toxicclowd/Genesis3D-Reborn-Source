/****************************************************************************************/
/*  DIRECT3D12DRIVER.H                                                                  */
/*                                                                                      */
/*  DirectX 12 Renderer for Jet3D Engine                                               */
/*  Modern replacement for the legacy Direct3D9Driver                                   */
/*                                                                                      */
/****************************************************************************************/
#ifndef DIRECT3D12DRIVER_H
#define DIRECT3D12DRIVER_H

#define DRIVERAPI	_declspec(dllexport)
#define INITGUID

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "DCommon.h"
#include "D3D12Log.h"

using Microsoft::WRL::ComPtr;

#define LOG_LEVEL								1

#ifdef _DEBUG
#define REPORT(x)							OutputDebugString(x)
#else
#define REPORT(x)
#endif

// DirectX 12 Driver version
#define D3D12DRV_VERSION_MAJOR					1
#define D3D12DRV_VERSION_MINOR					0

// Frame buffering count (triple buffering)
#define FRAME_COUNT								3

// Maximum textures that can be allocated
#define MAX_TEXTURES							4096

// Maximum static meshes
#define MAX_STATIC_MESHES						1024

// Descriptor heap sizes
#define MAX_SRV_DESCRIPTORS						4096
#define MAX_RTV_DESCRIPTORS						16
#define MAX_DSV_DESCRIPTORS						16

// Safe release macros
#define SAFE_DELETE(x)						{ if (x) delete x; x = nullptr; }
#define SAFE_DELETE_ARRAY(x)				{ if (x) delete [] x; x = nullptr; }
#define SAFE_RELEASE(x)						{ if (x) (x)->Release(); x = nullptr; }

//================================================================================
//	Forward Declarations
//================================================================================
class D3D12Log;
class D3D12TextureManager;
class D3D12GeometryBatcher;
class D3D12DescriptorHeap;
class D3D12PolyCache;
class D3D12PSOManager;

//================================================================================
//	Global State
//================================================================================
extern HWND									g_hWnd;
extern ComPtr<ID3D12Device>					g_pDevice;
extern ComPtr<ID3D12CommandQueue>			g_pCommandQueue;
extern ComPtr<IDXGISwapChain3>				g_pSwapChain;
extern ComPtr<ID3D12GraphicsCommandList>	g_pCommandList;
extern float									g_fGamma;
extern UINT									g_nCurrentFrameIndex;
extern int32									g_nScreenWidth;
extern int32									g_nScreenHeight;
extern bool									g_bInScene;
extern bool									g_bWireframe;
extern D3D12PSOManager*					g_pPSOManager;

//================================================================================
//	Helper Structures
//================================================================================

// Simple 4x4 matrix structure
struct D3D12Matrix
{
    float m[4][4];
};

// Frame resources (per frame in flight)
struct FrameResources
{
	ComPtr<ID3D12CommandAllocator>		pCommandAllocator;
	ComPtr<ID3D12Resource>				pRenderTarget;
	D3D12_CPU_DESCRIPTOR_HANDLE			RTVHandle;
	UINT64								FenceValue;
};

// Constant buffer data
struct ConstantBufferData
{
	D3D12Matrix							WorldViewProj;
	float									AmbientColor[4];
	float									FogColor[4];
	float									FogStart;
	float									FogEnd;
	float									Padding[2];
};

//================================================================================
//	Utility Functions
//================================================================================
void D3D12Matrix_ToXForm3d(const D3D12Matrix* mat, grXForm3d* XForm);
void grXForm3d_ToD3D12Matrix(const grXForm3d* XForm, D3D12Matrix* mat);
void D3D12WaitForGPU();

//================================================================================
//	Driver Interface Functions (from DCommon.h)
//================================================================================

// Enumeration functions
grBoolean DRIVERCC D3D12Drv_EnumSubDrivers(DRV_ENUM_DRV_CB* Cb, void* Context);
grBoolean DRIVERCC D3D12Drv_EnumModes(S32 Driver, char* DriverName, DRV_ENUM_MODES_CB* Cb, void* Context);
grBoolean DRIVERCC D3D12Drv_EnumPixelFormats(DRV_ENUM_PFORMAT_CB* Cb, void* Context);
grBoolean DRIVERCC D3D12Drv_GetDeviceCaps(grDeviceCaps* DeviceCaps);

// Initialization functions
grBoolean DRIVERCC D3D12Drv_Init(DRV_DriverHook* hook);
grBoolean DRIVERCC D3D12Drv_Shutdown();
grBoolean DRIVERCC D3D12Drv_Reset();
grBoolean DRIVERCC D3D12Drv_UpdateWindow();
grBoolean DRIVERCC D3D12Drv_SetActive(grBoolean Active);

// Texture functions
grTexture* DRIVERCC D3D12_THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const grRDriver_PixelFormat* PixelFormat);
grTexture* DRIVERCC D3D12_THandle_CreateFromFile(grVFile* File);
grBoolean DRIVERCC D3D12_THandle_Destroy(grTexture* THandle);
grBoolean DRIVERCC D3D12_THandle_Lock(grTexture* THandle, int32 MipLevel, void** Data);
grBoolean DRIVERCC D3D12_THandle_Unlock(grTexture* THandle, int32 MipLevel);
grBoolean DRIVERCC D3D12_THandle_GetInfo(grTexture* THandle, int32 MipLevel, grTexture_Info* Info);

// Scene management functions
grBoolean DRIVERCC D3D12Drv_BeginScene(grBoolean Clear, grBoolean ClearZ, RECT* WorldRect, grBoolean Wireframe);
grBoolean DRIVERCC D3D12Drv_EndScene(void);
grBoolean DRIVERCC D3D12Drv_BeginBatch(void);
grBoolean DRIVERCC D3D12Drv_EndBatch(void);

// Render functions
grBoolean DRIVERCC D3D12Drv_RenderGouraudPoly(grTLVertex* Pnts, int32 NumPoints, uint32 Flags);
grBoolean DRIVERCC D3D12Drv_RenderWorldPoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, void* LMapCBContext, uint32 Flags);
grBoolean DRIVERCC D3D12Drv_RenderMiscTexturePoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags);

// Decal functions
grBoolean DRIVERCC D3D12Drv_DrawDecal(grTexture* Handle, RECT* SrcRect, int32 x, int32 y);

// Utility functions
grBoolean DRIVERCC D3D12Drv_Screenshot(const char* filename);
grBoolean DRIVERCC D3D12Drv_GetGamma(float* gamma);
grBoolean DRIVERCC D3D12Drv_SetGamma(float gamma);

// Transform functions
grBoolean DRIVERCC D3D12Drv_SetMatrix(uint32 Type, grXForm3d* Matrix);
grBoolean DRIVERCC D3D12Drv_GetMatrix(uint32 Type, grXForm3d* Matrix);

// Static mesh functions
uint32 DRIVERCC D3D12Drv_CreateStaticMesh(grHWVertex* Points, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags);
grBoolean DRIVERCC D3D12Drv_RemoveStaticMesh(uint32 id);
grBoolean DRIVERCC D3D12Drv_RenderStaticMesh(uint32 id, int32 StartVertex, int32 NumPolys, grXForm3d* XForm);

// Font functions
grFont* DRIVERCC D3D12Drv_CreateFont(int32 Height, int32 Width, uint32 Weight, grBoolean Italic, const char* facename);
grBoolean DRIVERCC D3D12Drv_DrawFont(grFont* Font, int32 x, int32 y, uint32 Color, const char* text);
grBoolean DRIVERCC D3D12Drv_DestroyFont(grFont** Font);

// Render state functions
grBoolean DRIVERCC D3D12Drv_SetRenderState(uint32 state, uint32 value);
grBoolean DRIVERCC D3D12Drv_DrawText(char* text, int x, int y, uint32 color);

//================================================================================
//	Driver Structure Export
//================================================================================
typedef DRV_Driver D3D12Driver;
extern "C" DRIVERAPI D3D12Driver g_D3D12Drv;

#endif // DIRECT3D12DRIVER_H
