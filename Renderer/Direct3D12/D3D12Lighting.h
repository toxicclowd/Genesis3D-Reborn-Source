/****************************************************************************************/
/*  D3D12LIGHTING.H                                                                     */
/*                                                                                      */
/*  Frame lighting (roadmap Phase 3): clustered forward shading and real-time shadows.  */
/*                                                                                      */
/*  The engine gives every light of the frame in world space with the main camera       */
/*  (DRV_Driver::World_SetFrame). Before the scene's draws are recorded, a compute pass */
/*  bins the point and spot lights into a froxel grid of the main camera (16 x 9 tiles, */
/*  24 exponential depth slices), and dynamic lights that cast shadows get shadow maps  */
/*  in one depth atlas: 6 cube faces for a point light, one view for a spot light and   */
/*  4 cascades for a directional light. PSWorldPBR (Shaders\World.hlsl) loops over its  */
/*  cluster's lights; views of other cameras (portals, mirrors) test every light.       */
/*                                                                                      */
/*  [Shadows] in Direct3D12Driver.ini: AtlasSize (4096), MaxLights (8), Distance (3000, */
/*  how far directional shadows reach).                                                  */
/****************************************************************************************/
#ifndef D3D12LIGHTING_H
#define D3D12LIGHTING_H

#include <vector>
#include <d3d12.h>
#include "DCommon.h"

struct D3D12FrameConstants;

// A mesh drawn this frame (world space), for shadow maps.
struct D3D12ShadowCaster
{
	D3D12_VERTEX_BUFFER_VIEW	Vertices;		// position first in each vertex
	UINT						NumVertices;
};

grBoolean	D3D12Lighting_Startup();
void		D3D12Lighting_Shutdown();
// After the shader-visible heap is (re)created.
void		D3D12Lighting_WriteDescriptors();
// Call at BeginScene: forgets the last frame's lights.
void		D3D12Lighting_BeginFrame();

void		DRIVERCC D3D12Lighting_SetFrame(const DRV_WorldView* MainView, const DRV_Light* Lights, int32 NumLights);

// True when a view (model-to-camera and model-to-world rows) looks through the main camera.
bool		D3D12Lighting_IsMainCamera(const float ModelToCamera[3][4], const float ModelToWorld[3][4]);
// The main camera given with the frame's lights; false when there is none.
bool		D3D12Lighting_GetMainView(DRV_WorldView* View);

// Records the shadow maps and the light clusters. Call before the scene's draws, outside
// any render pass state the caller relies on (it sets its own targets and viewports).
void		D3D12Lighting_Prepare(ID3D12GraphicsCommandList* CommandList, const std::vector<D3D12ShadowCaster>& Meshes);
// The lighting part of the frame constants.
void		D3D12Lighting_FillFrameConstants(D3D12FrameConstants& Constants);
// Binds the lights, clusters and shadow views (ROOT_PARAM_LIGHTS..ROOT_PARAM_SHADOW_VIEWS).
void		D3D12Lighting_Bind(ID3D12GraphicsCommandList* CommandList);

#endif // D3D12LIGHTING_H
