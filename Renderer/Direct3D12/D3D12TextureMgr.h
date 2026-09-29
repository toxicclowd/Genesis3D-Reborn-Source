/****************************************************************************************/
/*  D3D12TEXTUREMGR.H                                                                   */
/*                                                                                      */
/*  DirectX 12 Texture Manager for Jet3D Engine                                        */
/*  Modeled after D3D9TextureMgr for compatibility                                     */
/*                                                                                      */
/****************************************************************************************/
#ifndef D3D12_TEXTURE_MANAGER_H
#define D3D12_TEXTURE_MANAGER_H

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "DCommon.h"

using Microsoft::WRL::ComPtr;

// Texture Manager initialization and shutdown
grBoolean D3D12_THandle_Startup();
grBoolean D3D12_THandle_Shutdown();
void D3D12_THandle_BeginFrame(UINT FrameIndex);

// Texture creation and destruction
grTexture* DRIVERCC D3D12_THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const grRDriver_PixelFormat* PixelFormat);
grTexture* DRIVERCC D3D12_THandle_CreateFromFile(grVFile* File);
grBoolean DRIVERCC D3D12_THandle_Destroy(grTexture* Handle);

// Texture locking and unlocking
grBoolean DRIVERCC D3D12_THandle_Lock(grTexture* Handle, int32 MipLevel, void** Bits);
grBoolean DRIVERCC D3D12_THandle_Unlock(grTexture* Handle, int32 MipLevel);

// Texture information
grBoolean DRIVERCC D3D12_THandle_GetInfo(grTexture* Handle, int32 MipLevel, grTexture_Info* Info);
ID3D12Resource* D3D12_THandle_GetResource(grTexture* Handle);
int32 D3D12_THandle_GetID(grTexture* Handle);
D3D12_CPU_DESCRIPTOR_HANDLE D3D12_THandle_GetSRV(grTexture* Handle);
D3D12_GPU_DESCRIPTOR_HANDLE D3D12_THandle_GetGPUSRV(grTexture* Handle);
ID3D12DescriptorHeap* D3D12_THandle_GetDescriptorHeap();
grBoolean D3D12_THandle_UpdateLightmap(grTexture* Handle, const uint8* RGBData);

// Texture structure for D3D12
typedef struct grTexture
{
	int32 id;

	grBoolean Active;
	ComPtr<ID3D12Resource> pTexture;        // GPU texture resource

	int32 Width;
	int32 Height;
	int32 NumMipLevels;

	int32 stride;
	uint8 Log;

	DXGI_FORMAT Format;                     // Native DXGI format used on GPU

	// Original driver pixel format & flags (as requested at creation)
	grRDriver_PixelFormat DriverFormat;     // Preserve PixelFormat + Flags for queries

	grBoolean Lightmap;
	uint32 LockedMipMask;                   // Jet3D may lock several mip levels together

	uint8* MipData[16];                     // Independent CPU storage for each mip level
	size_t MipDataCapacity[16];
	grBoolean DriverOwned;
	D3D12_RESOURCE_STATES ResourceState;

	D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle;  // Shader Resource View handle
	UINT SRVDescriptorIndex;                // Index in descriptor heap
} grTexture;

#endif // D3D12_TEXTURE_MANAGER_H
