/****************************************************************************************/
/*  D3D12LIGHTMAPATLAS.CPP                                                              */
/*                                                                                      */
/*  Lightmap atlases for the GPU world path. See D3D12LightmapAtlas.h.                  */
/****************************************************************************************/
#include <algorithm>
#include <cstring>
#include <vector>

#include "D3D12LightmapAtlas.h"
#include "D3D12TextureMgr.h"
#include "D3D12UploadRing.h"
#include "D3D12Config.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	const int32 Border = 1;
	const int32 MaxPages = 8;
	const UINT StageWidth = 512;		// texels per row of a flush's packed upload

	struct Shelf
	{
		int32	Y;
		int32	Height;
		int32	X;			// next free column
	};

	struct Page
	{
		grTexture*			Texture;
		grPixelFormat		Format;
		int32				Bpp;
		int32				Count;		// lightmaps placed; the page is repacked when it empties
		int32				NextY;
		std::vector<Shelf>	Shelves;
	};

	// Converted texels (border included) waiting for the next flush.
	struct PendingUpload
	{
		grTexture*	Owner;
		size_t		Page;
		UINT		Width;
		UINT		Height;
		size_t		DataOffset;		// into PendingData, Width * Height * Bpp bytes
	};

	bool						Enabled = false;
	int32						PageSize = 1024;
	std::vector<Page>			Pages;
	std::vector<PendingUpload>	Pending;
	std::vector<uint8>			PendingData;

	bool Allocate(Page& Target, int32 Width, int32 Height, int32* X, int32* Y)
	{
		// The shortest shelf that fits, else a new shelf.
		Shelf* Best = nullptr;
		for (Shelf& Candidate : Target.Shelves)
		{
			if (Candidate.Height >= Height && Candidate.X + Width <= PageSize &&
				(!Best || Candidate.Height < Best->Height))
				Best = &Candidate;
		}
		if (!Best)
		{
			if (Target.NextY + Height > PageSize)
				return false;
			Target.Shelves.push_back({ Target.NextY, Height, 0 });
			Target.NextY += Height;
			Best = &Target.Shelves.back();
		}
		*X = Best->X;
		*Y = Best->Y;
		Best->X += Width;
		return true;
	}

	int32 CreatePage(const grTexture& Lightmap)
	{
		if (static_cast<int32>(Pages.size()) >= MaxPages)
			return -1;
		grTexture* Texture = D3D12_THandle_Create(PageSize, PageSize, 1, &Lightmap.DriverFormat);
		if (!Texture)
			return -1;
		Texture->DriverOwned = GR_TRUE;		// not lockable by the engine

		Page NewPage = {};
		NewPage.Texture = Texture;
		NewPage.Format = Lightmap.DriverFormat.PixelFormat;
		NewPage.Bpp = D3D12_LightmapBytesPerPixel(NewPage.Format);
		Pages.push_back(NewPage);
		D3D12Log::GetPtr()->Printf("Lightmap atlas page %d: %dx%d, format %d",
			static_cast<int>(Pages.size() - 1), PageSize, PageSize, static_cast<int>(Texture->Format));
		return static_cast<int32>(Pages.size() - 1);
	}

	bool PlaceNew(grTexture& Handle)
	{
		const int32 Width = Handle.Width + 2 * Border;
		const int32 Height = Handle.Height + 2 * Border;
		const int32 Bpp = D3D12_LightmapBytesPerPixel(Handle.DriverFormat.PixelFormat);
		if ((Bpp != 2 && Bpp != 4) || Width > PageSize || Height > PageSize ||
			static_cast<UINT>(Width) > StageWidth)
			return false;

		int32 X = 0, Y = 0;
		for (size_t i = 0; i <= Pages.size(); ++i)
		{
			int32 Index = static_cast<int32>(i);
			if (i == Pages.size())
			{
				Index = CreatePage(Handle);
				if (Index < 0)
					return false;
			}
			Page& Target = Pages[static_cast<size_t>(Index)];
			if (Target.Format != Handle.DriverFormat.PixelFormat || !Allocate(Target, Width, Height, &X, &Y))
				continue;

			++Target.Count;
			Handle.AtlasPage = Index;
			Handle.AtlasX = static_cast<uint16>(X + Border);
			Handle.AtlasY = static_cast<uint16>(Y + Border);
			Handle.AtlasResident = GR_FALSE;
			return true;
		}
		return false;
	}

	void QueueUpload(grTexture& Handle, const uint8* RGBData)
	{
		// A newer upload of the same lightmap replaces a queued one.
		for (size_t i = 0; Handle.AtlasResident && i < Pending.size(); ++i)
		{
			if (Pending[i].Owner == &Handle)
			{
				Pending.erase(Pending.begin() + static_cast<std::ptrdiff_t>(i));
				break;
			}
		}

		const Page& Target = Pages[static_cast<size_t>(Handle.AtlasPage)];
		const size_t Bpp = static_cast<size_t>(Target.Bpp);
		PendingUpload Copy = {};
		Copy.Owner = &Handle;
		Copy.Page = static_cast<size_t>(Handle.AtlasPage);
		Copy.Width = static_cast<UINT>(Handle.Width + 2 * Border);
		Copy.Height = static_cast<UINT>(Handle.Height + 2 * Border);
		Copy.DataOffset = PendingData.size();
		PendingData.resize(PendingData.size() + Copy.Width * Copy.Height * Bpp);

		// Interior, then replicate the edge texels into the border.
		uint8* Texels = PendingData.data() + Copy.DataOffset;
		const size_t Pitch = Copy.Width * Bpp;
		D3D12_ConvertLightmapTexels(Target.Format, RGBData, Handle.Width, Handle.Height,
			Texels + Pitch * Border + Bpp * Border, Pitch);
		for (UINT y = Border; y < Copy.Height - Border; ++y)
		{
			uint8* Row = Texels + Pitch * y;
			std::memcpy(Row, Row + Bpp, Bpp);
			std::memcpy(Row + (Copy.Width - 1) * Bpp, Row + (Copy.Width - 2) * Bpp, Bpp);
		}
		std::memcpy(Texels, Texels + Pitch, Pitch);
		std::memcpy(Texels + Pitch * (Copy.Height - 1), Texels + Pitch * (Copy.Height - 2), Pitch);

		Pending.push_back(Copy);
	}

	void Transition(ID3D12GraphicsCommandList* CommandList, const std::vector<size_t>& Touched,
					D3D12_RESOURCE_STATES State)
	{
		std::vector<D3D12_RESOURCE_BARRIER> Barriers;
		for (size_t Index : Touched)
		{
			grTexture* Texture = Pages[Index].Texture;
			if (Texture->ResourceState == State)
				continue;
			D3D12_RESOURCE_BARRIER Barrier = {};
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barrier.Transition.pResource = Texture->pTexture.Get();
			Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Barrier.Transition.StateBefore = Texture->ResourceState;
			Barrier.Transition.StateAfter = State;
			Barriers.push_back(Barrier);
			Texture->ResourceState = State;
		}
		if (!Barriers.empty())
			CommandList->ResourceBarrier(static_cast<UINT>(Barriers.size()), Barriers.data());
	}

	// Packs the pending uploads for pages of one pixel format into a single upload-ring
	// footprint (rows of lightmaps side by side) and copies each into its page.
	bool RecordFormat(ID3D12GraphicsCommandList* CommandList, grPixelFormat Format, std::vector<bool>& Done)
	{
		size_t Bpp = 0;
		DXGI_FORMAT DxgiFormat = DXGI_FORMAT_UNKNOWN;
		std::vector<size_t> Batch;
		for (size_t i = 0; i < Pending.size(); ++i)
		{
			const Page& Target = Pages[Pending[i].Page];
			if (Done[i] || Target.Format != Format)
				continue;
			Bpp = static_cast<size_t>(Target.Bpp);
			DxgiFormat = Target.Texture->Format;
			Batch.push_back(i);
		}
		if (Batch.empty())
			return true;

		// Lay the batch out in rows StageWidth texels wide.
		std::vector<UINT> StageX(Batch.size()), StageY(Batch.size());
		UINT X = 0, Y = 0, RowHeight = 0;
		for (size_t b = 0; b < Batch.size(); ++b)
		{
			const PendingUpload& Copy = Pending[Batch[b]];
			if (X + Copy.Width > StageWidth)
			{
				X = 0;
				Y += RowHeight;
				RowHeight = 0;
			}
			StageX[b] = X;
			StageY[b] = Y;
			X += Copy.Width;
			RowHeight = (std::max)(RowHeight, Copy.Height);
		}
		const UINT StageHeight = Y + RowHeight;
		const UINT RowPitch = static_cast<UINT>((StageWidth * Bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
			~static_cast<size_t>(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1));

		D3D12UploadAllocation Upload = {};
		if (!D3D12Upload_Allocate(static_cast<UINT64>(RowPitch) * StageHeight,
								  D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &Upload))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Lightmap upload allocation failed (%u rows)", StageHeight);
			return false;
		}

		uint8* Stage = static_cast<uint8*>(Upload.CPU);
		for (size_t b = 0; b < Batch.size(); ++b)
		{
			const PendingUpload& Copy = Pending[Batch[b]];
			const uint8* Source = PendingData.data() + Copy.DataOffset;
			for (UINT y = 0; y < Copy.Height; ++y)
			{
				std::memcpy(Stage + static_cast<size_t>(RowPitch) * (StageY[b] + y) + StageX[b] * Bpp,
							Source + static_cast<size_t>(Copy.Width) * Bpp * y, Copy.Width * Bpp);
			}
		}

		D3D12_TEXTURE_COPY_LOCATION Source = {};
		Source.pResource = Upload.Resource;
		Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Source.PlacedFootprint.Offset = Upload.Offset;
		Source.PlacedFootprint.Footprint.Format = DxgiFormat;
		Source.PlacedFootprint.Footprint.Width = StageWidth;
		Source.PlacedFootprint.Footprint.Height = StageHeight;
		Source.PlacedFootprint.Footprint.Depth = 1;
		Source.PlacedFootprint.Footprint.RowPitch = RowPitch;

		for (size_t b = 0; b < Batch.size(); ++b)
		{
			const PendingUpload& Copy = Pending[Batch[b]];
			D3D12_TEXTURE_COPY_LOCATION Destination = {};
			Destination.pResource = Pages[Copy.Page].Texture->pTexture.Get();
			Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			Destination.SubresourceIndex = 0;

			D3D12_BOX Box = { StageX[b], StageY[b], 0, StageX[b] + Copy.Width, StageY[b] + Copy.Height, 1 };
			CommandList->CopyTextureRegion(&Destination,
				static_cast<UINT>(Copy.Owner->AtlasX - Border), static_cast<UINT>(Copy.Owner->AtlasY - Border), 0,
				&Source, &Box);
			Done[Batch[b]] = true;
		}
		return true;
	}
}

void D3D12Lightmap_Startup()
{
	D3D12Lightmap_Shutdown();
	PageSize = D3D12Config_GetInt("Render", "LightmapAtlasSize", 1024);
	if (PageSize < 64)
		PageSize = 64;
	if (PageSize > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		PageSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	Enabled = D3D12Config_GetBool("Render", "LightmapAtlas", true);
	D3D12Log::GetPtr()->Printf(Enabled
		? "Lightmaps: atlas enabled (%d x %d pages)"
		: "Lightmaps: atlas disabled ([Render] LightmapAtlas)", PageSize, PageSize);
}

void D3D12Lightmap_Shutdown()
{
	// The pages are texture handles; the texture manager frees them.
	Pages.clear();
	Pending.clear();
	PendingData.clear();
}

grBoolean D3D12Lightmap_Place(grTexture* Handle, const uint8* RGBData, D3D12LightmapPlacement* Out)
{
	if (!Enabled || !Handle || !Out || Handle->AtlasPage == -2)
		return GR_FALSE;

	if (Handle->AtlasPage < 0)
	{
		if (!RGBData)
			return GR_FALSE;
		if (!PlaceNew(*Handle))
		{
			Handle->AtlasPage = -2;
			return GR_FALSE;
		}
	}
	if (RGBData)
	{
		QueueUpload(*Handle, RGBData);
		Handle->AtlasResident = GR_TRUE;
	}
	if (!Handle->AtlasResident)
		return GR_FALSE;

	const Page& Target = Pages[static_cast<size_t>(Handle->AtlasPage)];
	Out->DescriptorIndex = D3D12_THandle_GetDescriptorIndex(Target.Texture);
	Out->OffsetU = static_cast<float>(Handle->AtlasX * 16);
	Out->OffsetV = static_cast<float>(Handle->AtlasY * 16);
	Out->PageSize = static_cast<float>(PageSize * 16);
	return GR_TRUE;
}

void D3D12Lightmap_Release(grTexture* Handle)
{
	if (!Handle || Handle->AtlasPage < 0 || static_cast<size_t>(Handle->AtlasPage) >= Pages.size())
		return;

	for (size_t i = 0; i < Pending.size(); ++i)
	{
		if (Pending[i].Owner == Handle)
		{
			Pending.erase(Pending.begin() + static_cast<std::ptrdiff_t>(i));
			break;
		}
	}

	Page& Target = Pages[static_cast<size_t>(Handle->AtlasPage)];
	if (--Target.Count <= 0)
	{
		Target.Count = 0;
		Target.NextY = 0;
		Target.Shelves.clear();
	}
	Handle->AtlasPage = -1;
	Handle->AtlasResident = GR_FALSE;
}

void D3D12Lightmap_RecordUploads(ID3D12GraphicsCommandList* CommandList)
{
	if (Pending.empty() || !CommandList)
		return;

	std::vector<size_t> Touched;
	for (const PendingUpload& Copy : Pending)
	{
		if (std::find(Touched.begin(), Touched.end(), Copy.Page) == Touched.end())
			Touched.push_back(Copy.Page);
	}

	Transition(CommandList, Touched, D3D12_RESOURCE_STATE_COPY_DEST);
	std::vector<bool> Done(Pending.size(), false);
	for (size_t i = 0; i < Pending.size(); ++i)
	{
		if (!Done[i] && !RecordFormat(CommandList, Pages[Pending[i].Page].Format, Done))
			break;
	}
	Transition(CommandList, Touched, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Anything that could not be staged gets uploaded again next time it is drawn.
	for (size_t i = 0; i < Pending.size(); ++i)
	{
		if (!Done[i])
			Pending[i].Owner->AtlasResident = GR_FALSE;
	}
	Pending.clear();
	PendingData.clear();
}
