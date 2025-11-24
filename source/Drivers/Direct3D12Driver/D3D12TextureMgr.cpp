/****************************************************************************************/
/*  D3D12TEXTUREMGR.CPP                                                                 */
/*                                                                                      */
/*  DirectX 12 Texture Manager Implementation                                           */
/*  Modeled after D3D9TextureMgr for compatibility                                     */
/*                                                                                      */
/****************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "D3D12TextureMgr.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"
#include "D3D12Common.h"

// Maximum number of textures - matches MAX_TEXTURES from Direct3D12Driver.h
#define MAX_THANDLES 4096

// Global texture list
static jeTexture g_TextureList[MAX_THANDLES];

// SRV descriptor heap for textures
static ComPtr<ID3D12DescriptorHeap> g_pSRVHeap;
static UINT g_nSRVDescriptorSize = 0;
static UINT g_nNextSRVIndex = 0;

// Helper function to get log2 of texture size
static int32 GetLog(int32 Width, int32 Height)
{
	int32 Size = (Width > Height) ? Width : Height;
	int32 Log = 0;

	while (Size > 1)
	{
		Size >>= 1;
		Log++;
	}

	return Log;
}

// Helper function to get next available texture handle
static jeTexture* GetNextTHandle()
{
	for (int i = 0; i < MAX_THANDLES; i++)
	{
		if (g_TextureList[i].Active == JE_FALSE)
		{
			memset(&g_TextureList[i], 0, sizeof(jeTexture));
			g_TextureList[i].id = i;
			g_TextureList[i].Active = JE_TRUE;
			g_TextureList[i].DriverOwned = JE_FALSE;
			g_TextureList[i].Data = nullptr;
			g_TextureList[i].Lightmap = JE_FALSE;
			g_TextureList[i].Locked = JE_FALSE;
			return &g_TextureList[i];
		}
	}

	return nullptr;
}

// Helper function to convert pixel format to DXGI format
static DXGI_FORMAT GetDXGIFormat(const jeRDriver_PixelFormat* PixelFormat)
{
	if (!PixelFormat)
		return DXGI_FORMAT_R8G8B8A8_UNORM;

	switch (PixelFormat->PixelFormat)
	{
	case JE_PIXELFORMAT_8BIT:
		return DXGI_FORMAT_R8_UNORM;
	case JE_PIXELFORMAT_16BIT_555_RGB:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case JE_PIXELFORMAT_16BIT_565_RGB:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case JE_PIXELFORMAT_16BIT_1555_ARGB:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case JE_PIXELFORMAT_16BIT_4444_ARGB:
		return DXGI_FORMAT_B4G4R4A4_UNORM;
	case JE_PIXELFORMAT_24BIT_RGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM; // 24-bit not directly supported, use 32-bit
	case JE_PIXELFORMAT_32BIT_XRGB:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case JE_PIXELFORMAT_32BIT_ARGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

//================================================================================
//	Texture Manager Initialization
//================================================================================

jeBoolean D3D12_THandle_Startup()
{
	D3D12Log::GetPtr()->Printf("D3D12_THandle_Startup called");

	// Initialize texture list
	for (int i = 0; i < MAX_THANDLES; i++)
	{
		memset(&g_TextureList[i], 0, sizeof(jeTexture));
		g_TextureList[i].id = i;
		g_TextureList[i].Active = JE_FALSE;
	}

	// Create SRV descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = MAX_SRV_DESCRIPTORS;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	HRESULT hr = g_pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_pSRVHeap));
	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create SRV descriptor heap - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	g_nSRVDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_nNextSRVIndex = 0;

	D3D12Log::GetPtr()->Printf("Texture Manager initialized - SRV heap created");
	return JE_TRUE;
}

jeBoolean D3D12_THandle_Shutdown()
{
	D3D12Log::GetPtr()->Printf("D3D12_THandle_Shutdown called");

	// Destroy all active textures
	for (int i = 0; i < MAX_THANDLES; i++)
	{
		if (g_TextureList[i].Active == JE_TRUE)
		{
			D3D12_THandle_Destroy(&g_TextureList[i]);
		}
	}

	// Release SRV heap
	g_pSRVHeap.Reset();
	g_nNextSRVIndex = 0;

	D3D12Log::GetPtr()->Printf("Texture Manager shut down");
	return JE_TRUE;
}

//================================================================================
//	Texture Creation
//================================================================================

jeTexture* DRIVERCC D3D12_THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const jeRDriver_PixelFormat* PixelFormat)
{
	D3D12Log::GetPtr()->Printf("THandle_Create: %dx%d, %d mips", Width, Height, NumMipLevels);

	// Get next available texture handle
	jeTexture* Handle = GetNextTHandle();
	if (!Handle)
	{
		D3D12Log::GetPtr()->Printf("ERROR: No more empty THandle slots!");
		return nullptr;
	}

	// Set texture properties
	Handle->Width = Width;
	Handle->Height = Height;
	Handle->NumMipLevels = (NumMipLevels > 0) ? NumMipLevels : 1;
	Handle->Log = (uint8)GetLog(Width, Height);
	Handle->stride = Width;
	Handle->Format = GetDXGIFormat(PixelFormat);

	// Create texture resource
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = Width;
	textureDesc.Height = Height;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = Handle->NumMipLevels;
	textureDesc.Format = Handle->Format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	HRESULT hr = g_pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Handle->pTexture)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create texture resource - HR: 0x%08X", hr);
		D3D12_THandle_Destroy(Handle);
		return nullptr;
	}

	// Create upload buffer for texture data
	UINT64 uploadBufferSize;
	g_pDevice->GetCopyableFootprints(&textureDesc, 0, Handle->NumMipLevels, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

	D3D12_HEAP_PROPERTIES uploadHeapProps = {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC uploadBufferDesc = {};
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Alignment = 0;
	uploadBufferDesc.Width = uploadBufferSize;
	uploadBufferDesc.Height = 1;
	uploadBufferDesc.DepthOrArraySize = 1;
	uploadBufferDesc.MipLevels = 1;
	uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadBufferDesc.SampleDesc.Count = 1;
	uploadBufferDesc.SampleDesc.Quality = 0;
	uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = g_pDevice->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Handle->pUploadBuffer)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create upload buffer - HR: 0x%08X", hr);
		D3D12_THandle_Destroy(Handle);
		return nullptr;
	}

	// Create SRV for the texture
	if (g_nNextSRVIndex >= MAX_SRV_DESCRIPTORS)
	{
		D3D12Log::GetPtr()->Printf("ERROR: SRV descriptor heap full!");
		D3D12_THandle_Destroy(Handle);
		return nullptr;
	}

	Handle->SRVDescriptorIndex = g_nNextSRVIndex++;
	Handle->SRVHandle = g_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
	Handle->SRVHandle.ptr += Handle->SRVDescriptorIndex * g_nSRVDescriptorSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = Handle->Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = Handle->NumMipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	g_pDevice->CreateShaderResourceView(Handle->pTexture.Get(), &srvDesc, Handle->SRVHandle);

	D3D12Log::GetPtr()->Printf("Texture created successfully - ID: %d", Handle->id);
	return Handle;
}

jeTexture* DRIVERCC D3D12_THandle_CreateFromFile(jeVFile* File)
{
	D3D12Log::GetPtr()->Printf("THandle_CreateFromFile called (not yet implemented)");
	// TODO: Implement loading texture from file using WIC or similar
	// For now, return nullptr as this is less commonly used
	return nullptr;
}

//================================================================================
//	Texture Destruction
//================================================================================

jeBoolean DRIVERCC D3D12_THandle_Destroy(jeTexture* Handle)
{
	if (!Handle)
		return JE_FALSE;

	D3D12Log::GetPtr()->Printf("THandle_Destroy: ID %d", Handle->id);

	// Free CPU data if allocated
	if (Handle->Data)
	{
		delete[] Handle->Data;
		Handle->Data = nullptr;
	}

	// Release D3D12 resources
	Handle->pTexture.Reset();
	Handle->pUploadBuffer.Reset();

	// Mark as inactive
	Handle->Active = JE_FALSE;
	Handle->Locked = JE_FALSE;

	return JE_TRUE;
}

//================================================================================
//	Texture Locking and Unlocking
//================================================================================

jeBoolean DRIVERCC D3D12_THandle_Lock(jeTexture* Handle, int32 MipLevel, void** Bits)
{
	if (!Handle || !Bits)
		return JE_FALSE;

	if (Handle->Locked)
	{
		D3D12Log::GetPtr()->Printf("WARNING: Texture already locked - ID: %d", Handle->id);
		return JE_FALSE;
	}

	// Calculate size for this mip level
	int32 mipWidth = Handle->Width >> MipLevel;
	int32 mipHeight = Handle->Height >> MipLevel;
	if (mipWidth < 1) mipWidth = 1;
	if (mipHeight < 1) mipHeight = 1;

	// Allocate CPU-side buffer for texture data
	int32 bytesPerPixel = 4; // Assume 32-bit RGBA for now
	int32 dataSize = mipWidth * mipHeight * bytesPerPixel;

	if (!Handle->Data)
	{
		Handle->Data = new uint8[dataSize];
	}

	if (!Handle->Data)
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to allocate texture data buffer");
		return JE_FALSE;
	}

	*Bits = Handle->Data;
	Handle->Locked = JE_TRUE;

	return JE_TRUE;
}

jeBoolean DRIVERCC D3D12_THandle_Unlock(jeTexture* Handle, int32 MipLevel)
{
	if (!Handle)
		return JE_FALSE;

	if (!Handle->Locked)
	{
		D3D12Log::GetPtr()->Printf("WARNING: Texture not locked - ID: %d", Handle->id);
		return JE_TRUE;
	}

	// Upload texture data to GPU
	// This would normally be done via command list, but for simplicity
	// we'll just mark it as unlocked for now
	// TODO: Implement proper upload via command list

	Handle->Locked = JE_FALSE;

	D3D12Log::GetPtr()->Printf("Texture unlocked - ID: %d, MipLevel: %d", Handle->id, MipLevel);
	return JE_TRUE;
}

//================================================================================
//	Texture Information
//================================================================================

jeBoolean DRIVERCC D3D12_THandle_GetInfo(jeTexture* Handle, int32 MipLevel, jeTexture_Info* Info)
{
	if (!Handle || !Info)
		return JE_FALSE;

	// Calculate dimensions for this mip level
	int32 mipWidth = Handle->Width >> MipLevel;
	int32 mipHeight = Handle->Height >> MipLevel;
	if (mipWidth < 1) mipWidth = 1;
	if (mipHeight < 1) mipHeight = 1;

	Info->Width = mipWidth;
	Info->Height = mipHeight;
	Info->Stride = mipWidth * 4; // Assume 32-bit RGBA
	Info->Format.PixelFormat = JE_PIXELFORMAT_32BIT_ARGB; // Default format

	return JE_TRUE;
}

ID3D12Resource* D3D12_THandle_GetResource(jeTexture* Handle)
{
	if (!Handle)
		return nullptr;

	return Handle->pTexture.Get();
}

int32 D3D12_THandle_GetID(jeTexture* Handle)
{
	if (!Handle)
		return -1;

	return Handle->id;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12_THandle_GetSRV(jeTexture* Handle)
{
	if (!Handle)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
		return nullHandle;
	}

	return Handle->SRVHandle;
}
