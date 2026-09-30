/****************************************************************************************/
/*  D3D12UPLOADRING.H                                                                   */
/*                                                                                      */
/*  Per-frame linear allocator in upload memory (roadmap Phase 0 frame structure).      */
/*                                                                                      */
/*  Each frame slot owns persistently mapped upload pages, kept at their high-water    */
/*  mark. Allocations are valid for the frame that makes them; D3D12Upload_BeginFrame  */
/*  recycles a slot's pages once its fence has completed. Used for per-frame and        */
/*  per-view constant buffers and for the poly cache's transformed vertices.            */
/****************************************************************************************/
#ifndef D3D12UPLOADRING_H
#define D3D12UPLOADRING_H

#include <d3d12.h>
#include "DCommon.h"

struct D3D12UploadAllocation
{
	void*						CPU;
	D3D12_GPU_VIRTUAL_ADDRESS	GPU;
	ID3D12Resource*				Resource;	// for copies: the page buffer and the offset in it
	UINT64						Offset;
};

grBoolean	D3D12Upload_Startup();
void		D3D12Upload_Shutdown();
void		D3D12Upload_BeginFrame(UINT FrameIndex);
// Allocates from the current frame slot (g_nCurrentFrameIndex).
grBoolean	D3D12Upload_Allocate(UINT64 Size, UINT64 Alignment, D3D12UploadAllocation* Out);

#endif // D3D12UPLOADRING_H
