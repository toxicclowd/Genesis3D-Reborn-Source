/****************************************************************************************/
/*  D3D12WORLDGEOMETRY.H                                                                */
/*                                                                                      */
/*  GPU world path (roadmap Phase 1): the driver side of DRV_Driver::WorldGeometry_*.   */
/*                                                                                      */
/*  The engine uploads a BSP's draw faces once (model-space vertices, one triangle fan */
/*  per face). Each frame it queues the visible faces with the same layers and flags   */
/*  it would give RenderWorldPoly; the poly cache keeps them in order with the other   */
/*  draws and merges runs of faces that share state into single indexed draws.        */
/*  Shaders\World.hlsl transforms and projects them exactly as the CPU path would.     */
/*                                                                                      */
/*  Needs bindless textures; [Render] WorldPath=0 turns it off.                         */
/****************************************************************************************/
#ifndef D3D12WORLDGEOMETRY_H
#define D3D12WORLDGEOMETRY_H

#include <vector>
#include <d3d12.h>
#include <wrl/client.h>
#include "DCommon.h"

using Microsoft::WRL::ComPtr;

// Per-face data read by World.hlsl (WorldFace).
struct D3D12WorldFaceData
{
	uint32	BaseTexture;
	uint32	LightTexture;
	float	InvScaleU;
	float	InvScaleV;
	float	ShiftU;
	float	ShiftV;
	float	TextureScale;
	float	Alpha;
	float	LightShiftU;	// lightmap StartU/StartV
	float	LightShiftV;
	float	LightOffsetU;	// the lightmap's origin in its texture (atlas page), * 16
	float	LightOffsetV;
	float	LightDivU;		// that texture's width/height * 16
	float	LightDivV;
	uint32	Padding[2];
};

struct D3D12WorldGeometry
{
	uint32						Handle;
	ComPtr<ID3D12Resource>		VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW	VertexBufferView;
	std::vector<uint32>			Indices;		// copied into each frame's index buffer
	std::vector<DRV_WorldFace>	Faces;

	// This frame's per-face data (upload ring), allocated when the first face is queued.
	UINT64						FaceDataFrame;
	D3D12WorldFaceData*			FaceDataCPU;
	D3D12_GPU_VIRTUAL_ADDRESS	FaceDataGPU;

	// Destroyed geometry stays alive until the GPU has passed RetireFence.
	bool						Retired;
	UINT64						RetireFence;
};

grBoolean	D3D12World_Startup();
void		D3D12World_Shutdown();
// Call at BeginScene, once the frame slot's fence has completed.
void		D3D12World_BeginFrame();

uint32		DRIVERCC D3D12World_Create(const DRV_WorldVertex* Verts, int32 NumVerts,
									   const uint32* Indices, int32 NumIndices,
									   const DRV_WorldFace* Faces, int32 NumFaces);
grBoolean	DRIVERCC D3D12World_Destroy(uint32 Geometry);
int32		DRIVERCC D3D12World_RenderFace(uint32 Geometry, uint32 Face, const DRV_WorldView* View,
										   grRDriver_Layer* Layers, int32 NumLayers,
										   void* LMapCBContext, uint32 Flags, float Alpha);

#endif // D3D12WORLDGEOMETRY_H
