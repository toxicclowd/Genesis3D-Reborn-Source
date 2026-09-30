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
#include "D3D12LightmapAtlas.h"
#include "DDSTextureLoader12.h"

#define MAX_THANDLES 4096
#define MAX_TEXTURE_MIP_LEVELS 16

static grTexture g_TextureList[MAX_THANDLES];
static ComPtr<ID3D12DescriptorHeap> g_pSRVHeap;
static UINT g_nSRVDescriptorSize = 0;
static const UINT SRV_HEAP_SIZE = MAX_THANDLES + D3D12_RESERVED_SRV_COUNT;

static D3D12_CPU_DESCRIPTOR_HANDLE CpuSRV(UINT Index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE Handle = g_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
	Handle.ptr += static_cast<SIZE_T>(Index) * g_nSRVDescriptorSize;
	return Handle;
}

// Every slot always holds a valid view, so indexing the heap from a shader never
// touches an uninitialized descriptor.
static void WriteNullSRV(UINT Index)
{
	if (!g_pSRVHeap)
		return;
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Desc.Texture2D.MipLevels = 1;
	g_pDevice->CreateShaderResourceView(nullptr, &Desc, CpuSRV(Index));
}

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

static int32 BytesPerPixel(grPixelFormat Format)
{
	switch (Format)
	{
	case GR_PIXELFORMAT_8BIT:
		return 1;
	case GR_PIXELFORMAT_16BIT_555_RGB:
	case GR_PIXELFORMAT_16BIT_565_RGB:
	case GR_PIXELFORMAT_16BIT_1555_ARGB:
	case GR_PIXELFORMAT_16BIT_4444_ARGB:
		return 2;
	case GR_PIXELFORMAT_24BIT_RGB:
		return 3;
	case GR_PIXELFORMAT_32BIT_XRGB:
	case GR_PIXELFORMAT_32BIT_ARGB:
		return 4;
	default:
		return 0;
	}
}

static DXGI_FORMAT ConvertPixelFormatToDXGI(const grRDriver_PixelFormat* PixelFormat)
{
	if (!PixelFormat)
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	switch (PixelFormat->PixelFormat)
	{
	case GR_PIXELFORMAT_8BIT:
		return DXGI_FORMAT_R8_UNORM;
	case GR_PIXELFORMAT_16BIT_555_RGB:
	case GR_PIXELFORMAT_16BIT_1555_ARGB:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case GR_PIXELFORMAT_16BIT_565_RGB:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case GR_PIXELFORMAT_16BIT_4444_ARGB:
		return DXGI_FORMAT_B4G4R4A4_UNORM;
	case GR_PIXELFORMAT_24BIT_RGB:
		// DXGI has no three-byte RGB texture format. Unlock expands it to RGBA.
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GR_PIXELFORMAT_32BIT_XRGB:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case GR_PIXELFORMAT_32BIT_ARGB:
	default:
		// Jet3D ARGB DWORDs are B,G,R,A bytes on little-endian Windows.
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
}

static void ResetTexture(grTexture& Handle, int32 Id)
{
	for (int32 MipLevel = 0; MipLevel < MAX_TEXTURE_MIP_LEVELS; ++MipLevel)
	{
		delete[] Handle.MipData[MipLevel];
		Handle.MipData[MipLevel] = nullptr;
		Handle.MipDataCapacity[MipLevel] = 0;
	}
	if (Handle.pTexture)
		WriteNullSRV(static_cast<UINT>(Id));
	Handle.pTexture.Reset();
	Handle.id = Id;
	Handle.Active = GR_FALSE;
	Handle.Width = 0;
	Handle.Height = 0;
	Handle.NumMipLevels = 0;
	Handle.stride = 0;
	Handle.Log = 0;
	Handle.Format = DXGI_FORMAT_UNKNOWN;
	std::memset(&Handle.DriverFormat, 0, sizeof(Handle.DriverFormat));
	Handle.Lightmap = GR_FALSE;
	Handle.LockedMipMask = 0;
	Handle.DriverOwned = GR_FALSE;
	Handle.ResourceState = D3D12_RESOURCE_STATE_COMMON;
	Handle.SRVHandle.ptr = 0;
	Handle.SRVDescriptorIndex = 0;
	Handle.AtlasPage = -1;
	Handle.AtlasX = Handle.AtlasY = 0;
	Handle.AtlasResident = GR_FALSE;
	Handle.Retired = GR_FALSE;
	Handle.RetireFence = 0;
}

static grTexture* FindFreeTHandle(UINT64 CompletedFence, bool* bAnyRetired)
{
	for (int32 i = 0; i < MAX_THANDLES; ++i)
	{
		grTexture& Handle = g_TextureList[i];
		if (Handle.Active)
			continue;
		if (!Handle.Retired || Handle.RetireFence <= CompletedFence)
			return &Handle;
		*bAnyRetired = true;
	}
	return nullptr;
}

static grTexture* GetNextTHandle()
{
	bool bAnyRetired = false;
	grTexture* Handle = FindFreeTHandle(D3D12GetCompletedFenceValue(), &bAnyRetired);

	// Every free slot may still be in use by queued frames. Outside a scene nothing
	// unsubmitted refers to them, so waiting for the GPU frees them all.
	if (!Handle && bAnyRetired && !g_bInScene)
	{
		D3D12WaitForGPU();
		Handle = FindFreeTHandle(D3D12GetCompletedFenceValue(), &bAnyRetired);
	}
	if (!Handle)
		return nullptr;

	ResetTexture(*Handle, Handle->id);
	Handle->Active = GR_TRUE;
	return Handle;
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

static grBoolean FillUploadBuffer(
	grTexture* Handle,
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
		return GR_FALSE;
	}

	uint8* Destination = static_cast<uint8*>(Mapped) + Footprint.Offset;
	const uint8* Source = Handle->MipData[MipLevel];
	const int32 MipWidth = Handle->Width >> MipLevel;
	const int32 Width = (MipWidth > 1) ? MipWidth : 1;
	const int32 SourceBpp = BytesPerPixel(Handle->DriverFormat.PixelFormat);
	const size_t SourcePitch = static_cast<size_t>(Width) * SourceBpp;

	if (Handle->DriverFormat.PixelFormat == GR_PIXELFORMAT_24BIT_RGB)
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

	// The last row is not padded out to RowPitch, and the buffer ends right after it.
	D3D12_RANGE WrittenRange = {
		static_cast<SIZE_T>(Footprint.Offset),
		static_cast<SIZE_T>(Footprint.Offset +
			static_cast<UINT64>(Footprint.Footprint.RowPitch) * (NumRows - 1) + RowSize)
	};
	Upload->Unmap(0, &WrittenRange);
	return GR_TRUE;
}

static void RecordTextureCopy(
	ID3D12GraphicsCommandList* CommandList,
	grTexture* Handle,
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

static grBoolean UploadLockedTexture(grTexture* Handle, int32 MipLevel)
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
		return GR_FALSE;
	}

	if (!FillUploadBuffer(Handle, MipLevel, Upload.Get(), Footprint, NumRows, RowSize))
		return GR_FALSE;

	if (g_bInScene && g_pCommandList)
	{
		RecordTextureCopy(g_pCommandList.Get(), Handle, MipLevel, Upload.Get(), Footprint);
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Upload);
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Handle->pTexture);
		return GR_TRUE;
	}

	ComPtr<ID3D12CommandAllocator> Allocator;
	Hr = g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator));
	if (FAILED(Hr))
		return GR_FALSE;

	ComPtr<ID3D12GraphicsCommandList> CommandList;
	Hr = g_pDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		Allocator.Get(),
		nullptr,
		IID_PPV_ARGS(&CommandList));
	if (FAILED(Hr))
		return GR_FALSE;

	RecordTextureCopy(CommandList.Get(), Handle, MipLevel, Upload.Get(), Footprint);
	Hr = CommandList->Close();
	if (FAILED(Hr))
		return GR_FALSE;

	ID3D12CommandList* Lists[] = { CommandList.Get() };
	g_pCommandQueue->ExecuteCommandLists(1, Lists);
	D3D12WaitForGPU();
	return GR_TRUE;
}

grBoolean D3D12_THandle_Startup()
{
	D3D12Log::GetPtr()->Printf("D3D12_THandle_Startup called");
	g_pSRVHeap.Reset();
	for (int32 i = 0; i < MAX_THANDLES; ++i)
		ResetTexture(g_TextureList[i], i);
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		g_FrameUploadResources[i].clear();

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = SRV_HEAP_SIZE;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT Hr = g_pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&g_pSRVHeap));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create SRV descriptor heap - HR: 0x%08X", Hr);
		return GR_FALSE;
	}
	g_nSRVDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (UINT i = 0; i < SRV_HEAP_SIZE; ++i)
		WriteNullSRV(i);
	return GR_TRUE;
}

grBoolean D3D12_THandle_Shutdown()
{
	D3D12Lightmap_Shutdown();	// its pages are handles in g_TextureList
	g_pSRVHeap.Reset();
	for (int32 i = 0; i < MAX_THANDLES; ++i)
		ResetTexture(g_TextureList[i], i);
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		g_FrameUploadResources[i].clear();
	g_nSRVDescriptorSize = 0;
	return GR_TRUE;
}

void D3D12_THandle_BeginFrame(UINT FrameIndex)
{
	if (FrameIndex < FRAME_COUNT)
		g_FrameUploadResources[FrameIndex].clear();
}

grTexture* DRIVERCC D3D12_THandle_Create(
	int32 Width,
	int32 Height,
	int32 NumMipLevels,
	const grRDriver_PixelFormat* PixelFormat)
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

	grTexture* Handle = GetNextTHandle();
	if (!Handle)
		return nullptr;

	if (PixelFormat)
		Handle->DriverFormat = *PixelFormat;
	else
	{
		Handle->DriverFormat.PixelFormat = GR_PIXELFORMAT_32BIT_ARGB;
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
	Handle->Lightmap = GR_FALSE;
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
	Handle->SRVHandle = CpuSRV(Handle->SRVDescriptorIndex);

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

grTexture* DRIVERCC D3D12_THandle_CreateFromFile(grVFile*)
{
	return nullptr;
}

// Records the copy of every subresource of a freshly created texture (in COPY_DEST) and
// its transition to PIXEL_SHADER_RESOURCE, on the scene's command list when a scene is
// open (the upload buffer then lives until the frame completes), else on a one-off list.
static grBoolean UploadSubresources(grTexture* Handle, const std::vector<D3D12_SUBRESOURCE_DATA>& Subresources)
{
	const UINT NumSubresources = static_cast<UINT>(Subresources.size());
	const D3D12_RESOURCE_DESC TextureDesc = Handle->pTexture->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Footprints(NumSubresources);
	std::vector<UINT> NumRows(NumSubresources);
	std::vector<UINT64> RowSizes(NumSubresources);
	UINT64 UploadSize = 0;
	g_pDevice->GetCopyableFootprints(&TextureDesc, 0, NumSubresources, 0,
		Footprints.data(), NumRows.data(), RowSizes.data(), &UploadSize);

	D3D12_HEAP_PROPERTIES UploadHeap = {};
	UploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC UploadDesc = BufferDescription(UploadSize);
	ComPtr<ID3D12Resource> Upload;
	HRESULT Hr = g_pDevice->CreateCommittedResource(&UploadHeap, D3D12_HEAP_FLAG_NONE, &UploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Upload));
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: DDS upload buffer creation failed - HR: 0x%08X", Hr);
		return GR_FALSE;
	}

	uint8* Mapped = nullptr;
	D3D12_RANGE ReadRange = { 0, 0 };
	if (FAILED(Upload->Map(0, &ReadRange, reinterpret_cast<void**>(&Mapped))))
		return GR_FALSE;
	for (UINT i = 0; i < NumSubresources; ++i)
	{
		// Rows are block rows for BC formats; RowSizes[i] is the packed size of one.
		const uint8* Source = static_cast<const uint8*>(Subresources[i].pData);
		uint8* Destination = Mapped + Footprints[i].Offset;
		for (UINT Row = 0; Row < NumRows[i]; ++Row)
			std::memcpy(Destination + static_cast<size_t>(Row) * Footprints[i].Footprint.RowPitch,
				Source + static_cast<size_t>(Row) * Subresources[i].RowPitch,
				static_cast<size_t>(RowSizes[i]));
	}
	Upload->Unmap(0, nullptr);

	auto Record = [&](ID3D12GraphicsCommandList* CommandList)
	{
		for (UINT i = 0; i < NumSubresources; ++i)
		{
			D3D12_TEXTURE_COPY_LOCATION Destination = {};
			Destination.pResource = Handle->pTexture.Get();
			Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			Destination.SubresourceIndex = i;
			D3D12_TEXTURE_COPY_LOCATION Source = {};
			Source.pResource = Upload.Get();
			Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			Source.PlacedFootprint = Footprints[i];
			CommandList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
		}
		D3D12_RESOURCE_BARRIER ToShader = {};
		ToShader.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		ToShader.Transition.pResource = Handle->pTexture.Get();
		ToShader.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		ToShader.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		ToShader.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		CommandList->ResourceBarrier(1, &ToShader);
	};

	if (g_bInScene && g_pCommandList)
	{
		Record(g_pCommandList.Get());
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Upload);
		g_FrameUploadResources[g_nCurrentFrameIndex].push_back(Handle->pTexture);
	}
	else
	{
		ComPtr<ID3D12CommandAllocator> Allocator;
		ComPtr<ID3D12GraphicsCommandList> CommandList;
		if (FAILED(g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator))) ||
			FAILED(g_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator.Get(), nullptr,
				IID_PPV_ARGS(&CommandList))))
			return GR_FALSE;
		Record(CommandList.Get());
		if (FAILED(CommandList->Close()))
			return GR_FALSE;
		ID3D12CommandList* Lists[] = { CommandList.Get() };
		g_pCommandQueue->ExecuteCommandLists(1, Lists);
		D3D12WaitForGPU();
	}
	Handle->ResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	return GR_TRUE;
}

grTexture* DRIVERCC D3D12_THandle_CreateFromDDS(const void* Data, uint32 Size)
{
	if (!g_pDevice || !g_pSRVHeap || !Data || Size < 128)
		return nullptr;

	ComPtr<ID3D12Resource> Resource;
	std::vector<D3D12_SUBRESOURCE_DATA> Subresources;
	bool IsCubeMap = false;
	HRESULT Hr = DirectX::LoadDDSTextureFromMemoryEx(g_pDevice.Get(), static_cast<const uint8_t*>(Data), Size,
		0, D3D12_RESOURCE_FLAG_NONE, DirectX::DDS_LOADER_IGNORE_SRGB, Resource.GetAddressOf(), Subresources,
		nullptr, &IsCubeMap);
	if (FAILED(Hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: DDS texture load failed - HR: 0x%08X", Hr);
		return nullptr;
	}
	const D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
	if (IsCubeMap || Desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || Desc.DepthOrArraySize != 1 ||
		Desc.MipLevels > MAX_TEXTURE_MIP_LEVELS)
	{
		D3D12Log::GetPtr()->Printf("ERROR: DDS texture is not a plain 2D texture");
		return nullptr;
	}

	grTexture* Handle = GetNextTHandle();
	if (!Handle)
		return nullptr;
	Handle->pTexture = Resource;
	Handle->Width = static_cast<int32>(Desc.Width);
	Handle->Height = static_cast<int32>(Desc.Height);
	Handle->NumMipLevels = Desc.MipLevels;
	Handle->Log = static_cast<uint8>(GetLog(Handle->Width, Handle->Height));
	Handle->stride = Handle->Width;
	Handle->Format = Desc.Format;
	Handle->DriverFormat.PixelFormat = GR_PIXELFORMAT_32BIT_ARGB;	// what GetInfo reports
	Handle->DriverFormat.Flags = RDRIVER_PF_3D;
	Handle->Lightmap = GR_FALSE;
	Handle->DriverOwned = GR_TRUE;		// compressed texels cannot be locked
	Handle->ResourceState = D3D12_RESOURCE_STATE_COPY_DEST;

	Handle->SRVDescriptorIndex = static_cast<UINT>(Handle->id);
	Handle->SRVHandle = CpuSRV(Handle->SRVDescriptorIndex);
	if (!UploadSubresources(Handle, Subresources))
	{
		const int32 Id = Handle->id;
		ResetTexture(*Handle, Id);
		return nullptr;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = Handle->Format;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Texture2D.MipLevels = static_cast<UINT>(Handle->NumMipLevels);
	g_pDevice->CreateShaderResourceView(Handle->pTexture.Get(), &SrvDesc, Handle->SRVHandle);

	D3D12Log::GetPtr()->Printf("DDS texture created - ID %d, %dx%d, %d mip(s), format %d",
		Handle->id, Handle->Width, Handle->Height, Handle->NumMipLevels, static_cast<int>(Handle->Format));
	return Handle;
}

grBoolean DRIVERCC D3D12_THandle_Destroy(grTexture* Handle)
{
	if (!Handle || !Handle->Active)
		return GR_FALSE;

	// Queued frames may still sample this texture, so the resource and its descriptor
	// stay put until the GPU passes the current fence; GetNextTHandle recycles the slot.
	for (int32 MipLevel = 0; MipLevel < MAX_TEXTURE_MIP_LEVELS; ++MipLevel)
	{
		delete[] Handle->MipData[MipLevel];
		Handle->MipData[MipLevel] = nullptr;
		Handle->MipDataCapacity[MipLevel] = 0;
	}
	Handle->LockedMipMask = 0;
	D3D12Lightmap_Release(Handle);
	Handle->Active = GR_FALSE;
	Handle->Retired = GR_TRUE;
	Handle->RetireFence = D3D12GetPendingFenceValue();
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12_THandle_Lock(grTexture* Handle, int32 MipLevel, void** Bits)
{
	if (!Handle || !Handle->Active || !Bits || Handle->DriverOwned ||
		MipLevel < 0 || MipLevel >= Handle->NumMipLevels ||
		MipLevel >= MAX_TEXTURE_MIP_LEVELS ||
		(Handle->LockedMipMask & (1u << MipLevel)) != 0)
		return GR_FALSE;

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
			return GR_FALSE;
		}
		Handle->MipDataCapacity[MipLevel] = Required;
	}

	Handle->stride = Width;
	Handle->LockedMipMask |= (1u << MipLevel);
	*Bits = Handle->MipData[MipLevel];
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12_THandle_Unlock(grTexture* Handle, int32 MipLevel)
{
	if (!Handle || !Handle->Active || MipLevel < 0 ||
		MipLevel >= Handle->NumMipLevels || MipLevel >= MAX_TEXTURE_MIP_LEVELS ||
		(Handle->LockedMipMask & (1u << MipLevel)) == 0)
		return GR_FALSE;

	const grBoolean Result = UploadLockedTexture(Handle, MipLevel);
	Handle->LockedMipMask &= ~(1u << MipLevel);
	return Result;
}

grBoolean DRIVERCC D3D12_THandle_GetInfo(grTexture* Handle, int32 MipLevel, grTexture_Info* Info)
{
	if (!Handle || !Handle->Active || !Info || MipLevel < 0 || MipLevel >= Handle->NumMipLevels)
		return GR_FALSE;

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
	return GR_TRUE;
}

ID3D12Resource* D3D12_THandle_GetResource(grTexture* Handle)
{
	return (Handle && Handle->Active) ? Handle->pTexture.Get() : nullptr;
}

int32 D3D12_THandle_GetID(grTexture* Handle)
{
	return (Handle && Handle->Active) ? Handle->id : -1;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12_THandle_GetSRV(grTexture* Handle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE Result = {};
	if (Handle && Handle->Active)
		Result = Handle->SRVHandle;
	return Result;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12_THandle_GetGPUSRV(grTexture* Handle)
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

UINT D3D12_THandle_GetDescriptorIndex(grTexture* Handle)
{
	return (Handle && Handle->Active) ? Handle->SRVDescriptorIndex : 0;
}

void D3D12_THandle_GetReservedSRV(UINT Slot, D3D12_CPU_DESCRIPTOR_HANDLE* Cpu, D3D12_GPU_DESCRIPTOR_HANDLE* Gpu)
{
	const UINT Index = MAX_THANDLES + Slot;
	if (Cpu)
		*Cpu = CpuSRV(Index);
	if (Gpu)
	{
		*Gpu = g_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
		Gpu->ptr += static_cast<UINT64>(Index) * g_nSRVDescriptorSize;
	}
}

UINT D3D12_THandle_GetReservedIndex(UINT Slot)
{
	return MAX_THANDLES + Slot;
}

int32 D3D12_LightmapBytesPerPixel(grPixelFormat Format)
{
	return BytesPerPixel(Format);
}

void D3D12_ConvertLightmapTexels(grPixelFormat Format, const uint8* RGBData, int32 Width, int32 Height,
								  uint8* Destination, size_t RowPitch)
{
	for (int32 y = 0; y < Height; ++y)
	{
		const uint8* Source = RGBData + static_cast<size_t>(y) * Width * 3;
		uint8* Row = Destination + static_cast<size_t>(y) * RowPitch;
		for (int32 x = 0; x < Width; ++x)
		{
			const uint8 R = Source[x * 3 + 0];
			const uint8 G = Source[x * 3 + 1];
			const uint8 B = Source[x * 3 + 2];
			switch (Format)
			{
			case GR_PIXELFORMAT_8BIT:
				Row[x] = static_cast<uint8>((static_cast<uint32>(R) + G + B) / 3);
				break;
			case GR_PIXELFORMAT_16BIT_555_RGB:
			case GR_PIXELFORMAT_16BIT_1555_ARGB:
				reinterpret_cast<uint16*>(Row)[x] = static_cast<uint16>(
					0x8000u | ((R >> 3) << 10) | ((G >> 3) << 5) | (B >> 3));
				break;
			case GR_PIXELFORMAT_16BIT_565_RGB:
				reinterpret_cast<uint16*>(Row)[x] = static_cast<uint16>(
					((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
				break;
			case GR_PIXELFORMAT_16BIT_4444_ARGB:
				reinterpret_cast<uint16*>(Row)[x] = static_cast<uint16>(
					0xF000u | ((R >> 4) << 8) | ((G >> 4) << 4) | (B >> 4));
				break;
			case GR_PIXELFORMAT_24BIT_RGB:
				Row[x * 3 + 0] = R;
				Row[x * 3 + 1] = G;
				Row[x * 3 + 2] = B;
				break;
			case GR_PIXELFORMAT_32BIT_XRGB:
			case GR_PIXELFORMAT_32BIT_ARGB:
			default:
				Row[x * 4 + 0] = B;
				Row[x * 4 + 1] = G;
				Row[x * 4 + 2] = R;
				Row[x * 4 + 3] = 255;
				break;
			}
		}
	}
}

grBoolean D3D12_THandle_UpdateLightmap(grTexture* Handle, const uint8* RGBData)
{
	if (!Handle || !RGBData)
		return GR_FALSE;

	void* Bits = nullptr;
	if (!D3D12_THandle_Lock(Handle, 0, &Bits))
		return GR_FALSE;

	D3D12_ConvertLightmapTexels(Handle->DriverFormat.PixelFormat, RGBData, Handle->Width, Handle->Height,
		static_cast<uint8*>(Bits), static_cast<size_t>(Handle->Width) * BytesPerPixel(Handle->DriverFormat.PixelFormat));

	if (!D3D12_THandle_Unlock(Handle, 0))
		return GR_FALSE;
	Handle->Lightmap = GR_TRUE;
	return GR_TRUE;
}
