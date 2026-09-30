/****************************************************************************************/
/*  D3D12UPLOADRING.CPP                                                                 */
/*                                                                                      */
/*  Per-frame linear allocator in upload memory (see D3D12UploadRing.h).                */
/****************************************************************************************/
#include <vector>

#include "D3D12UploadRing.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	const UINT64 PageSize = 4 * 1024 * 1024;

	struct Page
	{
		ComPtr<ID3D12Resource>	Buffer;
		uint8*					CPU;
		UINT64					Size;
		UINT64					Used;
	};

	std::vector<Page>	Pages[FRAME_COUNT];
	size_t				CurrentPage[FRAME_COUNT];

	bool CreatePage(UINT64 Size, Page& Out)
	{
		D3D12_HEAP_PROPERTIES Heap = {};
		Heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Width = Size;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		const HRESULT Hr = g_pDevice->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_NONE, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Out.Buffer));
		if (FAILED(Hr))
		{
			D3D12Log::GetPtr()->Printf("ERROR: Upload page (%llu bytes) failed - HR: 0x%08X",
				static_cast<unsigned long long>(Size), Hr);
			return false;
		}

		// Upload heaps can stay mapped for their whole lifetime.
		D3D12_RANGE NoReads = { 0, 0 };
		void* Mapped = nullptr;
		if (FAILED(Out.Buffer->Map(0, &NoReads, &Mapped)))
		{
			Out.Buffer.Reset();
			return false;
		}
		Out.CPU = static_cast<uint8*>(Mapped);
		Out.Size = Size;
		Out.Used = 0;
		return true;
	}
}

grBoolean D3D12Upload_Startup()
{
	D3D12Upload_Shutdown();
	for (UINT Frame = 0; Frame < FRAME_COUNT; ++Frame)
	{
		Page First = {};
		if (!CreatePage(PageSize, First))
			return GR_FALSE;
		Pages[Frame].push_back(First);
	}
	return GR_TRUE;
}

void D3D12Upload_Shutdown()
{
	for (UINT Frame = 0; Frame < FRAME_COUNT; ++Frame)
	{
		Pages[Frame].clear();
		CurrentPage[Frame] = 0;
	}
}

void D3D12Upload_BeginFrame(UINT FrameIndex)
{
	if (FrameIndex >= FRAME_COUNT || Pages[FrameIndex].empty())
		return;

	// Pages are kept at the high-water mark and reused from the start.
	for (Page& Each : Pages[FrameIndex])
		Each.Used = 0;
	CurrentPage[FrameIndex] = 0;
}

grBoolean D3D12Upload_Allocate(UINT64 Size, UINT64 Alignment, D3D12UploadAllocation* Out)
{
	if (!Out || Size == 0 || g_nCurrentFrameIndex >= FRAME_COUNT)
		return GR_FALSE;
	if (Alignment == 0)
		Alignment = 1;

	std::vector<Page>& FramePages = Pages[g_nCurrentFrameIndex];
	if (FramePages.empty())
		return GR_FALSE;

	size_t& Current = CurrentPage[g_nCurrentFrameIndex];
	Page* Target = &FramePages[Current];
	UINT64 Offset = (Target->Used + Alignment - 1) / Alignment * Alignment;
	while (Offset + Size > Target->Size)
	{
		// Move on to the next page, creating one when the frame needs more than before.
		if (++Current == FramePages.size())
		{
			Page Extra = {};
			if (!CreatePage((Size > PageSize) ? Size : PageSize, Extra))
			{
				--Current;
				return GR_FALSE;
			}
			FramePages.push_back(Extra);
		}
		Target = &FramePages[Current];
		Offset = (Target->Used + Alignment - 1) / Alignment * Alignment;
	}

	Target->Used = Offset + Size;
	Out->CPU = Target->CPU + Offset;
	Out->GPU = Target->Buffer->GetGPUVirtualAddress() + Offset;
	Out->Resource = Target->Buffer.Get();
	Out->Offset = Offset;
	return GR_TRUE;
}
