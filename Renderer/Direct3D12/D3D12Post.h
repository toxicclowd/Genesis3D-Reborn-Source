/****************************************************************************************/
/*  D3D12POST.H                                                                         */
/*                                                                                      */
/*  Post-processing (roadmap Phase 3), from the scene target into the composite target  */
/*  that the overlay is drawn on and the present pass shows:                             */
/*    - auto exposure: a luminance histogram of the scene and a smoothed exposure,       */
/*    - SSAO from the depth buffer (normals from depth),                                 */
/*    - bloom: a 6-level downsample / tent upsample chain,                               */
/*    - fog: the engine's distance fog (SetFog) and height fog,                          */
/*    - tonemapping (ACES or AgX), then the Stylized options (palette, dither, CRT) and  */
/*      the nearest-neighbour upscale of a reduced render scale.                         */
/*  In the Classic look only the engine's fog uses it; without fog the scene target is  */
/*  shown as before.                                                                     */
/****************************************************************************************/
#ifndef D3D12POST_H
#define D3D12POST_H

#include <d3d12.h>
#include "DCommon.h"

struct D3D12FrameConstants;

grBoolean	D3D12Post_Startup();
void		D3D12Post_Shutdown();
// Size-dependent targets and the scene/depth descriptors; call after the scene target
// and the depth buffer are (re)created.
grBoolean	D3D12Post_Resize(UINT Width, UINT Height);

// Whether this frame's scene goes through the post pass. HDRScene: the scene was drawn
// in linear HDR (Enhanced/Stylized look with a world).
bool		D3D12Post_Needed(bool HDRScene);
// Runs the chain on the scene (drawn at Frame.RenderScale of the back buffer) and leaves
// the composite target bound-ready for the overlay; *OverlayRTV is where to draw it.
grBoolean	D3D12Post_Run(ID3D12GraphicsCommandList* CommandList, const D3D12FrameConstants& Frame, bool HDRScene,
						  D3D12_CPU_DESCRIPTOR_HANDLE* OverlayRTV);
// Makes the composite target readable by the present pass.
void		D3D12Post_EndFrame(ID3D12GraphicsCommandList* CommandList);

#endif // D3D12POST_H
