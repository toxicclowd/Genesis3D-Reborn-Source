/****************************************************************************************/
/*  D3D12LIGHTMAPATLAS.H                                                                */
/*                                                                                      */
/*  Lightmap atlases for the GPU world path (roadmap Phase 1).                          */
/*                                                                                      */
/*  The engine gives each face its own small lightmap handle. On the world path the     */
/*  driver packs them into shared atlas pages instead: each lightmap gets a rectangle   */
/*  with a 1-texel replicated border (so bilinear filtering matches a clamped texture), */
/*  and its texels go through the per-frame upload ring. The copies are recorded in one */
/*  batch at the start of each poly cache flush, before the draws that sample them.     */
/*  The transformed-poly path keeps using the per-face handles.                         */
/*                                                                                      */
/*  [Render] LightmapAtlas=0 turns it off.                                              */
/****************************************************************************************/
#ifndef D3D12LIGHTMAPATLAS_H
#define D3D12LIGHTMAPATLAS_H

#include <d3d12.h>
#include "DCommon.h"

struct D3D12LightmapPlacement
{
	UINT	DescriptorIndex;	// the atlas page
	float	OffsetU;			// interior origin in lightmap-space units (texels * 16)
	float	OffsetV;
	float	PageSize;			// page width/height in texels * 16
};

void		D3D12Lightmap_Startup();
void		D3D12Lightmap_Shutdown();

// Places Handle's lightmap in an atlas page. RGBData (engine RGB texels, Width x Height)
// is uploaded when given; a lightmap never uploaded needs it. Returns GR_FALSE when the
// lightmap cannot use the atlas; the caller then uses the handle's own texture.
grBoolean	D3D12Lightmap_Place(grTexture* Handle, const uint8* RGBData, D3D12LightmapPlacement* Out);
// Frees Handle's rectangle (the handle is being destroyed).
void		D3D12Lightmap_Release(grTexture* Handle);
// Records the queued texel uploads. Call before recording draws that may sample them.
void		D3D12Lightmap_RecordUploads(ID3D12GraphicsCommandList* CommandList);

#endif // D3D12LIGHTMAPATLAS_H
