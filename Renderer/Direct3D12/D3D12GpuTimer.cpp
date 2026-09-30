/****************************************************************************************/
/*  D3D12GPUTIMER.CPP                                                                   */
/*                                                                                      */
/*  GPU frame timing with timestamp queries (see D3D12GpuTimer.h).                      */
/****************************************************************************************/
#include <cstdint>

#include "D3D12GpuTimer.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

namespace
{
	ComPtr<ID3D12QueryHeap>		QueryHeap;
	ComPtr<ID3D12Resource>		Readback;
	UINT64						Frequency = 0;
	UINT						CurrentFrame = 0;
	bool						Marked[FRAME_COUNT][GPU_MARK_COUNT];
	bool						Resolved[FRAME_COUNT];
	DRV_GPUTimings				Timings;
	int32						DrawCalls = 0;		// this frame, published at EndFrame
	int32						WorldDraws = 0;
	int32						WorldFaces = 0;

	UINT QueryIndex(UINT Frame, UINT Mark)
	{
		return Frame * GPU_MARK_COUNT + Mark;
	}

	float ToMs(UINT64 Begin, UINT64 End)
	{
		if (End <= Begin || Frequency == 0)
			return 0.0f;
		return static_cast<float>(static_cast<double>(End - Begin) * 1000.0 / static_cast<double>(Frequency));
	}
}

grBoolean D3D12Timer_Startup()
{
	D3D12Timer_Shutdown();

	if (FAILED(g_pCommandQueue->GetTimestampFrequency(&Frequency)) || Frequency == 0)
	{
		D3D12Log::GetPtr()->Printf("GPU timing unavailable (no timestamp frequency)");
		return GR_FALSE;
	}

	D3D12_QUERY_HEAP_DESC HeapDesc = {};
	HeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	HeapDesc.Count = FRAME_COUNT * GPU_MARK_COUNT;
	if (FAILED(g_pDevice->CreateQueryHeap(&HeapDesc, IID_PPV_ARGS(&QueryHeap))))
	{
		D3D12Log::GetPtr()->Printf("GPU timing unavailable (CreateQueryHeap failed)");
		return GR_FALSE;
	}

	D3D12_HEAP_PROPERTIES Heap = {};
	Heap.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC Desc = {};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Width = sizeof(UINT64) * FRAME_COUNT * GPU_MARK_COUNT;
	Desc.Height = 1;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if (FAILED(g_pDevice->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_NONE, &Desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&Readback))))
	{
		QueryHeap.Reset();
		D3D12Log::GetPtr()->Printf("GPU timing unavailable (readback buffer failed)");
		return GR_FALSE;
	}

	D3D12Log::GetPtr()->Printf("GPU timing enabled (timestamp frequency %llu Hz)",
		static_cast<unsigned long long>(Frequency));
	return GR_TRUE;
}

void D3D12Timer_Shutdown()
{
	QueryHeap.Reset();
	Readback.Reset();
	Frequency = 0;
	for (UINT Frame = 0; Frame < FRAME_COUNT; ++Frame)
	{
		Resolved[Frame] = false;
		for (UINT Mark = 0; Mark < GPU_MARK_COUNT; ++Mark)
			Marked[Frame][Mark] = false;
	}
	Timings = DRV_GPUTimings{};
	DrawCalls = WorldDraws = WorldFaces = 0;
}

void D3D12Timer_BeginFrame(UINT FrameIndex)
{
	if (FrameIndex >= FRAME_COUNT)
		return;
	CurrentFrame = FrameIndex;

	if (QueryHeap && Resolved[FrameIndex] &&
		Marked[FrameIndex][GPU_MARK_SCENE_BEGIN] && Marked[FrameIndex][GPU_MARK_FRAME_END])
	{
		const SIZE_T Offset = sizeof(UINT64) * QueryIndex(FrameIndex, 0);
		D3D12_RANGE ReadRange = { Offset, Offset + sizeof(UINT64) * GPU_MARK_COUNT };
		void* Mapped = nullptr;
		if (SUCCEEDED(Readback->Map(0, &ReadRange, &Mapped)))
		{
			const UINT64* Stamps = reinterpret_cast<const UINT64*>(static_cast<const uint8_t*>(Mapped) + Offset);
			const UINT64 PresentBegin = Marked[FrameIndex][GPU_MARK_PRESENT_BEGIN]
				? Stamps[GPU_MARK_PRESENT_BEGIN]
				: Stamps[GPU_MARK_FRAME_END];
			Timings.SceneMs = ToMs(Stamps[GPU_MARK_SCENE_BEGIN], PresentBegin);
			Timings.PresentMs = ToMs(PresentBegin, Stamps[GPU_MARK_FRAME_END]);
			Timings.TotalMs = ToMs(Stamps[GPU_MARK_SCENE_BEGIN], Stamps[GPU_MARK_FRAME_END]);
			Timings.Valid = 1;
			D3D12_RANGE NoWrites = { 0, 0 };
			Readback->Unmap(0, &NoWrites);
		}
	}

	Resolved[FrameIndex] = false;
	for (UINT Mark = 0; Mark < GPU_MARK_COUNT; ++Mark)
		Marked[FrameIndex][Mark] = false;
}

void D3D12Timer_Mark(ID3D12GraphicsCommandList* CommandList, D3D12_GPU_MARK Mark)
{
	if (!QueryHeap || Mark >= GPU_MARK_COUNT)
		return;
	CommandList->EndQuery(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryIndex(CurrentFrame, Mark));
	Marked[CurrentFrame][Mark] = true;
}

void D3D12Timer_CountDraws(int32 Draws, int32 NumWorldDraws, int32 NumWorldFaces)
{
	DrawCalls += Draws;
	WorldDraws += NumWorldDraws;
	WorldFaces += NumWorldFaces;
}

void D3D12Timer_EndFrame(ID3D12GraphicsCommandList* CommandList)
{
	Timings.DrawCalls = DrawCalls;
	Timings.WorldDraws = WorldDraws;
	Timings.WorldFaces = WorldFaces;
	DrawCalls = WorldDraws = WorldFaces = 0;

	if (!QueryHeap)
		return;
	for (UINT Mark = 0; Mark < GPU_MARK_COUNT; ++Mark)
	{
		// Resolving an unwritten query is invalid, so resolve only what was marked.
		if (Marked[CurrentFrame][Mark])
		{
			const UINT Index = QueryIndex(CurrentFrame, Mark);
			CommandList->ResolveQueryData(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, Index, 1,
				Readback.Get(), sizeof(UINT64) * Index);
		}
	}
	Resolved[CurrentFrame] = true;
}

DRV_GPUTimings* D3D12Timer_GetTimings()
{
	return &Timings;
}
