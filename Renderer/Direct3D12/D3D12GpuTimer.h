/****************************************************************************************/
/*  D3D12GPUTIMER.H                                                                     */
/*                                                                                      */
/*  GPU frame timing with timestamp queries (roadmap Phase 0 debugging).                */
/*                                                                                      */
/*  Each frame slot records timestamps at the start of the scene, the start of the     */
/*  present pass and the end of the frame. They are resolved into a readback buffer    */
/*  and read when that slot comes round again, after its fence has completed, so       */
/*  timing never stalls the CPU. Results are published through DRV_Driver::GPUTimings. */
/****************************************************************************************/
#ifndef D3D12GPUTIMER_H
#define D3D12GPUTIMER_H

#include <d3d12.h>
#include "DCommon.h"

enum D3D12_GPU_MARK
{
	GPU_MARK_SCENE_BEGIN = 0,
	GPU_MARK_PRESENT_BEGIN,
	GPU_MARK_FRAME_END,
	GPU_MARK_COUNT
};

grBoolean		D3D12Timer_Startup();
void			D3D12Timer_Shutdown();
// Reads back the slot's previous results. Call once the slot's fence has completed.
void			D3D12Timer_BeginFrame(UINT FrameIndex);
void			D3D12Timer_Mark(ID3D12GraphicsCommandList* CommandList, D3D12_GPU_MARK Mark);
// Resolves this frame's timestamps; call after the last mark, before Close.
void			D3D12Timer_EndFrame(ID3D12GraphicsCommandList* CommandList);
DRV_GPUTimings*	D3D12Timer_GetTimings();

#endif // D3D12GPUTIMER_H
