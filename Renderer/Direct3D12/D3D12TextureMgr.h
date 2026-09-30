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

// The shader-visible SRV heap holds one descriptor per texture handle (index = handle
// id, so shaders can index it directly), followed by slots reserved for the driver.
#define D3D12_RESERVED_SRV_COUNT		16
#define D3D12_RESERVED_SRV_SCENE		0		// HDR scene color, read by the present pass
#define D3D12_RESERVED_SRV_DEPTH		1		// scene depth (R32_FLOAT), read by post-processing
#define D3D12_RESERVED_SRV_COMPOSITE	2		// post-processed scene plus overlay, read by the present pass
#define D3D12_RESERVED_SRV_SHADOW		3		// shadow map atlas (D3D12Lighting.cpp)
#define D3D12_RESERVED_SRV_POST			4		// post-processing targets, up to the end (D3D12Post.cpp)

// Texture Manager initialization and shutdown
grBoolean D3D12_THandle_Startup();
grBoolean D3D12_THandle_Shutdown();
void D3D12_THandle_BeginFrame(UINT FrameIndex);

// Texture creation and destruction
grTexture* DRIVERCC D3D12_THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const grRDriver_PixelFormat* PixelFormat);
grTexture* DRIVERCC D3D12_THandle_CreateFromFile(grVFile* File);
grTexture* DRIVERCC D3D12_THandle_CreateFromDDS(const void* Data, uint32 Size);
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
UINT D3D12_THandle_GetDescriptorIndex(grTexture* Handle);
void D3D12_THandle_GetReservedSRV(UINT Slot, D3D12_CPU_DESCRIPTOR_HANDLE* Cpu, D3D12_GPU_DESCRIPTOR_HANDLE* Gpu);
// A reserved slot's index in the heap, for bindless shaders.
UINT D3D12_THandle_GetReservedIndex(UINT Slot);
grBoolean D3D12_THandle_UpdateLightmap(grTexture* Handle, const uint8* RGBData);
int32 D3D12_LightmapBytesPerPixel(grPixelFormat Format);
// Converts engine RGB lightmap texels to Format (rows RowPitch bytes apart).
void D3D12_ConvertLightmapTexels(grPixelFormat Format, const uint8* RGBData, int32 Width, int32 Height,
								 uint8* Destination, size_t RowPitch);

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

	// A destroyed handle keeps its resource and descriptor until the GPU has finished
	// every frame that could still read them (fence value RetireFence).
	grBoolean Retired;
	UINT64 RetireFence;

	// A lightmap's place in the GPU world path's atlas (D3D12LightmapAtlas.cpp).
	int32 AtlasPage;                        // -1 = not placed, -2 = does not fit the atlas
	uint16 AtlasX;                          // interior origin, inside the 1-texel border
	uint16 AtlasY;
	grBoolean AtlasResident;                // texels uploaded
} grTexture;

#endif // D3D12_TEXTURE_MANAGER_H
