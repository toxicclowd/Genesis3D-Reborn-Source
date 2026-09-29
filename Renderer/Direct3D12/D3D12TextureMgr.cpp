/****************************************************************************************/
/*  D3D12TEXTUREMGR.CPP                                                                 */
/*                                                                                      */
/*  DirectX 12 texture storage, upload, and descriptor management.                      */
/****************************************************************************************/
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "D3D12TextureMgr.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

#define MAX_THANDLES 4096
#define MAX_TEXTURE_MIP_LEVELS 16

static jeTexture g_TextureList[MAX_THANDLES];
static ComPtr<ID3D12DescriptorHeap> g_pSRVHeap;
static UINT g_nSRVDescriptorSize = 0;

// Resources referenced by an open command list stay alive until its frame slot is
// reused. D3D12_THandle_BeginFrame is called only after that slot's fence completes.
static std::vector<ComPtr<ID3D12Resource>> g_FrameUploadResources[FRAME_COUNT];

static int32 GetLog(int32 Width, int32 Height)
{
	int32 Size = (Width > Height) ? Width : Height;
	int32 Log = 0;
	while (Size > 1)
	{
		Size >>= 1;
		++Log;
	}
	return Log;
}

static int32 BytesPerPixel(jePixelFormat Format)
{
	switch (Format)
	{
	case JE_PIXELFORMAT_8BIT:
		return 1;
	case JE_PIXELFORMAT_16BIT_555_RGB:
	case JE_PIXELFORMAT_16BIT_565_RGB:
	case JE_PIXELFORMAT_16BIT_1555_ARGB:
	case JE_PIXELFORMAT_16BIT_4444_ARGB:
		return 2;
	case JE_PIXELFORMAT_24BIT_RGB:
		return 3;
	case JE_PIXELFORMAT_32BIT_XRGB:
	case JE_PIXELFORMAT_32BIT_ARGB:
		return 4;
	default:
		return 0;
	}
}

static DXGI_FORMAT ConvertPixelFormatToDXGI(const jeRDriver_PixelFormat* PixelFormat)
{
	if (!PixelFormat)
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	switch (PixelFormat->PixelFormat)
	{
	case JE_PIXELFORMAT_8BIT:
		return DXGI_FORMAT_R8_UNORM;
	case JE_PIXELFORMAT_16BIT_555_RGB:
	case JE_PIXELFORMAT_16BIT_1555_ARGB:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case JE_PIXELFORMAT_16BIT_565_RGB:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case JE_PIXELFORMAT_16BIT_4444_ARGB:
		return DXGI_FORMAT_B4G4R4A4_UNORM;
	case JE_PIXELFORMAT_24BIT_RGB:
		// DXGI has no three-byte RGB texture format. Unlock expands it to RGBA.
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case JE_PIXELFORMAT_32BIT_XRGB:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case JE_PIXELFORMAT_32BIT_ARGB:
	default:
		// Jet3D ARGB DWORDs are B,G,R,A bytes on little-endian Windows.
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
}

static void ResetTexture(jeTexture& Handle, int32 Id)
{
	for (int32 MipLevel = 0; MipLevel < MAX_TEXTURE_MIP_LEVELS; ++MipLevel)
	{
		delete[] Handle.MipData[MipLevel];
		Handle.MipData[MipLevel] = nullptr;
		Handle.MipDataCapacity[MipLevel] = 0;
	}
	Handle.pTexture.Reset();
	Handle.id = Id;
	Handle.Active = JE_FALSE;
	Handle.Width = 0;
	Handle.Height = 0;
	Handle.NumMipLevels = 0;
	Handle.stride = 0;
	Handle.Log = 0;
	Handle.Format = DXGI_FORMAT_UNKNOWN;
	std::memset(&Handle.DriverFormat, 0, sizeof(Handle.DriverFormat));
	Handle.Lightmap = JE_FALSE;
	Handle.LockedMipMask = 0;
	Handle.DriverOwned = JE_FALSE;
	Handle.ResourceState = D3D12_RESOURCE_STATE_COMMON;
	Handle.SRVHandle.ptr = 0;
	Handle.SRVDescriptorIndex = 0;
}

static jeTexture* GetNextTHandle()
{
	for (int32 i = 0; i < MAX_THANDLES; ++i)
	{
		if (g_TextureList[i].Active == JE_FALSE)
		{
			ResetTexture(g_TextureList[i], i);
			g_TextureList[i].Active = JE_TRUE;
			return &g_TextureList[i];
		}
	}
	return nullptr;
}

static D3D12_RESOURCE_DESC BufferDescription(UINT64 Size)
{
	D3D12_RESOURCE_DESC Desc = {};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Width = Size;
	Desc.Height = 1;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = 1;
	Desc.Format = DXGI_FORMAT_UNKNOWN;
	Desc.SampleDesc.Count = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	return Desc;
}

static jeBoolean FillUploadBuffer(
	jeTexture* Handle,
	int32 MipLevel,
	ID3D12Resource* Upload,
	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& Footprint,
	UINT NumRows,
	UINT64 RowSize)
{
	void* Mapped = nullptr;
	D3D12_RANGE ReadRange = { 0, 0 };
	HRESULT Hr = Upload->Map(0, &ReadRange, &Mapped);
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Texture upload map failed - HR: 0x%08X", Hr);
		return JE_FALSE;
	}

	uint8* Destination = static_cast<uint8*>(Mapped) + Footprint.Offset;
	const uint8* Source = Handle->MipData[MipLevel];
	const int32 MipWidth = Handle->Width >> MipLevel;
	const int32 Width = (MipWidth > 1) ? MipWidth : 1;
	const int32 SourceBpp = BytesPerPixel(Handle->DriverFormat.PixelFormat);
	const size_t SourcePitch = static_cast<size_t>(Width) * SourceBpp;

	if (Handle->DriverFormat.PixelFormat == JE_PIXELFORMAT_24BIT_RGB)
	{
		for (UINT Row = 0; Row < NumRows; ++Row)
		{
			uint8* Dst = Destination + static_cast<size_t>(Row) * Footprint.Footprint.RowPitch;
			const uint8* Src = Source + static_cast<size_t>(Row) * SourcePitch;
			for (int32 X = 0; X < Width; ++X)
			{
				Dst[X * 4 + 0] = Src[X * 3 + 0];
				Dst[X * 4 + 1] = Src[X * 3 + 1];
				Dst[X * 4 + 2] = Src[X * 3 + 2];
				Dst[X * 4 + 3] = 255;
			}
		}
	}
	else
	{
		const size_t CopyBytes = static_cast<size_t>((std::min)(RowSize, static_cast<UINT64>(SourcePitch)));
		for (UINT Row = 0; Row < NumRows; ++Row)
		{
			std::memcpy(
				Destination + static_cast<size_t>(Row) * Footprint.Footprint.RowPitch,
				Source + static_cast<size_t>(Row) * SourcePitch,
				CopyBytes);
		}
	}

	D3D12_RANGE WrittenRange = {
		static_cast<SIZE_T>(Footprint.Offset),
		static_cast<SIZE_T>(Footprint.Offset + static_cast<UINT64>(Footprint.Footprint.RowPitch) * NumRows)
	};
	Upload->Unmap(0, &WrittenRange);
	return JE_TRUE;
}

static void RecordTextureCopy(
	ID3D12GraphicsCommandList* CommandList,
	jeTexture* Handle,
	int32 MipLevel,
	ID3D12Resource* Upload,
	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& Footprint)
{
	if (Handle->ResourceState != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		D3D12_RESOURCE_BARRIER ToCopy = {};
		ToCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		ToCopy.Transition.pResource = Handle->pTexture.Get();
		ToCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		ToCopy.Transition.StateBefore = Handle->ResourceState;
		ToCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		CommandList->ResourceBarrier(1, &ToCopy);
	}

	D3D12_TEXTURE_COPY_LOCATION Destination = {};
	Destination.pResource = Handle->pTexture.Get();
	Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Destination.SubresourceIndex = static_cast<UINT>(MipLevel);

	D3D12_TEXTURE_COPY_LOCATION Source = {};
	Source.pResource = Upload;
	Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Source.PlacedFootprint = Footprint;
	CommandList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);

	D3D12_RESOURCE_BARRIER ToShader = {};
	ToShader.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ToShader.Transition.pResource = Handle->pTexture.Get();
	ToShader.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	ToShader.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	ToShader.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	CommandList->ResourceBarrier(1, &ToShader);
	Handle->ResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

static jeBoolean UploadLockedTexture(jeTexture* Handle, int32 MipLevel)
{
	D3D12_RESOURCE_DESC TextureDesc = Handle->pTexture->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint = {};
	UINT NumRows = 0;
	UINT64 RowSize = 0;
	UINT64 UploadSize = 0;
	g_pDevice->GetCopyableFootprints(
		&TextureDesc,
		static_cast<UINT>(MipLevel),
		1,
		0,
		&Footprint,
		&NumRows,
		&RowSize,
		&UploadSize);

	D3D12_HEAP_PROPERTIES UploadHeap = {};
	UploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC UploadDesc = BufferDescription(UploadSize);
	ComPtr<ID3D12Resource> Upload;
	HRESULT Hr = g_pDevice->CreateCommittedResource(
		&UploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&UploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Upload));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Texture upload buffer creation failed - HR: 0x%08X", Hr);
		return JE_FALSE;
	}

	if (!FillUploadBuffer(Handle, MipLevel, Upload.Get(), Footprint, NumRows, RowSize))
		return JE_FALSE;

	if (g_bInScene && g_pCommandList)
	{
		RecordTextureCopy(g_pCommandList.Get(), Handle, MipLevel, Upload.Get(), Footprint);
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Upload);
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Handle->pTexture);
		return JE_TRUE;
	}

	ComPtr<ID3D12CommandAllocator> Allocator;
	Hr = g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator));
	if (FAILED(Hr))
		return JE_FALSE;

	ComPtr<ID3D12GraphicsCommandList> CommandList;
	Hr = g_pDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		Allocator.Get(),
		nullptr,
		IID_PPV_ARGS(&CommandList));
	if (FAILED(Hr))
		return JE_FALSE;

	RecordTextureCopy(CommandList.Get(), Handle, MipLevel, Upload.Get(), Footprint);
	Hr = CommandList->Close();
	if (FAILED(Hr))
		return JE_FALSE;

	ID3D12CommandList* Lists[] = { CommandList.Get() };
	g_pCommandQueue->ExecuteCommandLists(1, Lists);
	D3D12WaitForGPU();
	return JE_TRUE;
}

jeBoolean D3D12_THandle_Startup()
{
	D3D12Log::GetPtr()->Printf("D3D12_THandle_Startup called");
	for (int32 i = 0; i < MAX_THANDLES; ++i)
		ResetTexture(g_TextureList[i], i);
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		g_FrameUploadResources[i].clear();

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = MAX_SRV_DESCRIPTORS;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT Hr = g_pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&g_pSRVHeap));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create SRV descriptor heap - HR: 0x%08X", Hr);
		return JE_FALSE;
	}
	g_nSRVDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return JE_TRUE;
}

jeBoolean D3D12_THandle_Shutdown()
{
	for (int32 i = 0; i < MAX_THANDLES; ++i)
		ResetTexture(g_TextureList[i], i);
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		g_FrameUploadResources[i].clear();
	g_pSRVHeap.Reset();
	g_nSRVDescriptorSize = 0;
	return JE_TRUE;
}

void D3D12_THandle_BeginFrame(UINT FrameIndex)
{
	if (FrameIndex < FRAME_COUNT)
		g_FrameUploadResources[FrameIndex].clear();
}

jeTexture* DRIVERCC D3D12_THandle_Create(
	int32 Width,
	int32 Height,
	int32 NumMipLevels,
	const jeRDriver_PixelFormat* PixelFormat)
{
	if (!g_pDevice || !g_pSRVHeap || Width <= 0 || Height <= 0)
		return nullptr;

	const int32 Bpp = PixelFormat ? BytesPerPixel(PixelFormat->PixelFormat) : 4;
	if (Bpp == 0)
	{
		D3D12Log::GetPtr()->Printf("ERROR: Unsupported Jet3D texture pixel format");
		return nullptr;
	}

	int32 MaxMipLevels = 1;
	for (int32 Dimension = (Width > Height) ? Width : Height; Dimension > 1; Dimension >>= 1)
		++MaxMipLevels;

	jeTexture* Handle = GetNextTHandle();
	if (!Handle)
		return nullptr;

	if (PixelFormat)
		Handle->DriverFormat = *PixelFormat;
	else
	{
		Handle->DriverFormat.PixelFormat = JE_PIXELFORMAT_32BIT_ARGB;
		Handle->DriverFormat.Flags = RDRIVER_PF_3D;
	}

	Handle->Width = Width;
	Handle->Height = Height;
	Handle->NumMipLevels = NumMipLevels;
	if (Handle->NumMipLevels < 1)
		Handle->NumMipLevels = 1;
	if (Handle->NumMipLevels > MaxMipLevels)
		Handle->NumMipLevels = MaxMipLevels;
	Handle->Log = static_cast<uint8>(GetLog(Width, Height));
	Handle->stride = Width;
	Handle->Format = ConvertPixelFormatToDXGI(&Handle->DriverFormat);
	Handle->Lightmap = JE_FALSE;
	Handle->ResourceState = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_DESC TextureDesc = {};
	TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TextureDesc.Width = static_cast<UINT64>(Width);
	TextureDesc.Height = static_cast<UINT>(Height);
	TextureDesc.DepthOrArraySize = 1;
	TextureDesc.MipLevels = static_cast<UINT16>(Handle->NumMipLevels);
	TextureDesc.Format = Handle->Format;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES DefaultHeap = {};
	DefaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT Hr = g_pDevice->CreateCommittedResource(
		&DefaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&TextureDesc,
		Handle->ResourceState,
		nullptr,
		IID_PPV_ARGS(&Handle->pTexture));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create texture - HR: 0x%08X", Hr);
		const int32 Id = Handle->id;
		ResetTexture(*Handle, Id);
		return nullptr;
	}

	Handle->SRVDescriptorIndex = static_cast<UINT>(Handle->id);
	Handle->SRVHandle = g_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
	Handle->SRVHandle.ptr += static_cast<SIZE_T>(Handle->SRVDescriptorIndex) * g_nSRVDescriptorSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = Handle->Format;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Texture2D.MipLevels = static_cast<UINT>(Handle->NumMipLevels);
	g_pDevice->CreateShaderResourceView(Handle->pTexture.Get(), &SrvDesc, Handle->SRVHandle);

	D3D12Log::GetPtr()->Printf(
		"Texture created - ID %d, %dx%d, %d mip(s), format %d",
		Handle->id,
		Width,
		Height,
		Handle->NumMipLevels,
		static_cast<int>(Handle->Format));
	return Handle;
}

jeTexture* DRIVERCC D3D12_THandle_CreateFromFile(jeVFile*)
{
	return nullptr;
}

jeBoolean DRIVERCC D3D12_THandle_Destroy(jeTexture* Handle)
{
	if (!Handle || !Handle->Active)
		return JE_FALSE;
	const int32 Id = Handle->id;
	ResetTexture(*Handle, Id);
	return JE_TRUE;
}

jeBoolean DRIVERCC D3D12_THandle_Lock(jeTexture* Handle, int32 MipLevel, void** Bits)
{
	if (!Handle || !Handle->Active || !Bits || Handle->DriverOwned ||
		MipLevel < 0 || MipLevel >= Handle->NumMipLevels ||
		MipLevel >= MAX_TEXTURE_MIP_LEVELS ||
		(Handle->LockedMipMask & (1u << MipLevel)) != 0)
		return JE_FALSE;

	const int32 ShiftedWidth = Handle->Width >> MipLevel;
	const int32 ShiftedHeight = Handle->Height >> MipLevel;
	const int32 Width = (ShiftedWidth > 1) ? ShiftedWidth : 1;
	const int32 Height = (ShiftedHeight > 1) ? ShiftedHeight : 1;
	const int32 Bpp = BytesPerPixel(Handle->DriverFormat.PixelFormat);
	const size_t Required = static_cast<size_t>(Width) * Height * Bpp;
	if (Required > Handle->MipDataCapacity[MipLevel])
	{
		delete[] Handle->MipData[MipLevel];
		Handle->MipData[MipLevel] = new (std::nothrow) uint8[Required];
		if (!Handle->MipData[MipLevel])
		{
			Handle->MipDataCapacity[MipLevel] = 0;
			return JE_FALSE;
		}
		Handle->MipDataCapacity[MipLevel] = Required;
	}

	Handle->stride = Width;
	Handle->LockedMipMask |= (1u << MipLevel);
	*Bits = Handle->MipData[MipLevel];
	return JE_TRUE;
}

jeBoolean DRIVERCC D3D12_THandle_Unlock(jeTexture* Handle, int32 MipLevel)
{
	if (!Handle || !Handle->Active || MipLevel < 0 ||
		MipLevel >= Handle->NumMipLevels || MipLevel >= MAX_TEXTURE_MIP_LEVELS ||
		(Handle->LockedMipMask & (1u << MipLevel)) == 0)
		return JE_FALSE;

	const jeBoolean Result = UploadLockedTexture(Handle, MipLevel);
	Handle->LockedMipMask &= ~(1u << MipLevel);
	return Result;
}

jeBoolean DRIVERCC D3D12_THandle_GetInfo(jeTexture* Handle, int32 MipLevel, jeTexture_Info* Info)
{
	if (!Handle || !Handle->Active || !Info || MipLevel < 0 || MipLevel >= Handle->NumMipLevels)
		return JE_FALSE;

	Info->Width = Handle->Width >> MipLevel;
	Info->Height = Handle->Height >> MipLevel;
	if (Info->Width < 1)
		Info->Width = 1;
	if (Info->Height < 1)
		Info->Height = 1;
	Info->Stride = Info->Width;
	Info->PixelFormat = Handle->DriverFormat;
	Info->Flags = (Handle->DriverFormat.Flags & RDRIVER_PF_CAN_DO_COLORKEY)
		? RDRIVER_THANDLE_HAS_COLORKEY
		: 0;
	Info->ColorKey = Info->Flags ? 1 : 0;
	Info->Direct = Handle->pTexture.Get();
	return JE_TRUE;
}

ID3D12Resource* D3D12_THandle_GetResource(jeTexture* Handle)
{
	return (Handle && Handle->Active) ? Handle->pTexture.Get() : nullptr;
}

int32 D3D12_THandle_GetID(jeTexture* Handle)
{
	return (Handle && Handle->Active) ? Handle->id : -1;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12_THandle_GetSRV(jeTexture* Handle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE Result = {};
	if (Handle && Handle->Active)
		Result = Handle->SRVHandle;
	return Result;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12_THandle_GetGPUSRV(jeTexture* Handle)
{
	D3D12_GPU_DESCRIPTOR_HANDLE Result = {};
	if (!Handle || !Handle->Active || !g_pSRVHeap)
		return Result;
	Result = g_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
	Result.ptr += static_cast<UINT64>(Handle->SRVDescriptorIndex) * g_nSRVDescriptorSize;
	return Result;
}

ID3D12DescriptorHeap* D3D12_THandle_GetDescriptorHeap()
{
	return g_pSRVHeap.Get();
}

jeBoolean D3D12_THandle_UpdateLightmap(jeTexture* Handle, const uint8* RGBData)
{
	if (!Handle || !RGBData)
		return JE_FALSE;

	void* Bits = nullptr;
	if (!D3D12_THandle_Lock(Handle, 0, &Bits))
		return JE_FALSE;

	uint8* Destination = static_cast<uint8*>(Bits);
	const size_t PixelCount = static_cast<size_t>(Handle->Width) * Handle->Height;
	for (size_t i = 0; i < PixelCount; ++i)
	{
		const uint8 R = RGBData[i * 3 + 0];
		const uint8 G = RGBData[i * 3 + 1];
		const uint8 B = RGBData[i * 3 + 2];
		switch (Handle->DriverFormat.PixelFormat)
		{
		case JE_PIXELFORMAT_8BIT:
			Destination[i] = static_cast<uint8>((static_cast<uint32>(R) + G + B) / 3);
			break;
		case JE_PIXELFORMAT_16BIT_555_RGB:
		case JE_PIXELFORMAT_16BIT_1555_ARGB:
			reinterpret_cast<uint16*>(Destination)[i] = static_cast<uint16>(
				0x8000u | ((R >> 3) << 10) | ((G >> 3) << 5) | (B >> 3));
			break;
		case JE_PIXELFORMAT_16BIT_565_RGB:
			reinterpret_cast<uint16*>(Destination)[i] = static_cast<uint16>(
				((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
			break;
		case JE_PIXELFORMAT_16BIT_4444_ARGB:
			reinterpret_cast<uint16*>(Destination)[i] = static_cast<uint16>(
				0xF000u | ((R >> 4) << 8) | ((G >> 4) << 4) | (B >> 4));
			break;
		case JE_PIXELFORMAT_24BIT_RGB:
			Destination[i * 3 + 0] = R;
			Destination[i * 3 + 1] = G;
			Destination[i * 3 + 2] = B;
			break;
		case JE_PIXELFORMAT_32BIT_XRGB:
		case JE_PIXELFORMAT_32BIT_ARGB:
		default:
			Destination[i * 4 + 0] = B;
			Destination[i * 4 + 1] = G;
			Destination[i * 4 + 2] = R;
			Destination[i * 4 + 3] = 255;
			break;
		}
	}

	if (!D3D12_THandle_Unlock(Handle, 0))
		return JE_FALSE;
	Handle->Lightmap = JE_TRUE;
	return JE_TRUE;
}
