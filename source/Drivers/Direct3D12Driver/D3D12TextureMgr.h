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
jeBoolean D3D12_THandle_Startup();
jeBoolean D3D12_THandle_Shutdown();

// Texture creation and destruction
jeTexture* DRIVERCC D3D12_THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const jeRDriver_PixelFormat* PixelFormat);
jeTexture* DRIVERCC D3D12_THandle_CreateFromFile(jeVFile* File);
jeBoolean DRIVERCC D3D12_THandle_Destroy(jeTexture* Handle);

// Texture locking and unlocking
jeBoolean DRIVERCC D3D12_THandle_Lock(jeTexture* Handle, int32 MipLevel, void** Bits);
jeBoolean DRIVERCC D3D12_THandle_Unlock(jeTexture* Handle, int32 MipLevel);

// Texture information
jeBoolean DRIVERCC D3D12_THandle_GetInfo(jeTexture* Handle, int32 MipLevel, jeTexture_Info* Info);
ID3D12Resource* D3D12_THandle_GetResource(jeTexture* Handle);
int32 D3D12_THandle_GetID(jeTexture* Handle);
D3D12_CPU_DESCRIPTOR_HANDLE D3D12_THandle_GetSRV(jeTexture* Handle);

// Texture structure for D3D12
typedef struct jeTexture
{
	int32 id;

	jeBoolean Active;
	ComPtr<ID3D12Resource> pTexture;        // GPU texture resource
	ComPtr<ID3D12Resource> pUploadBuffer;   // Upload buffer for texture data

	int32 Width;
	int32 Height;
	int32 NumMipLevels;

	int32 stride;
	uint8 Log;

	DXGI_FORMAT Format;

	jeBoolean Locked;
	jeBoolean Lightmap;

	uint8* Data;                            // CPU-side data when locked
	jeBoolean DriverOwned;

	D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle;  // Shader Resource View handle
	UINT SRVDescriptorIndex;                // Index in descriptor heap
} jeTexture;

#endif // D3D12_TEXTURE_MANAGER_H
