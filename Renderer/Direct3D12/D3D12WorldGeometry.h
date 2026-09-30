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
	// PBR material (roadmap Phase 2), read by PSWorldPBR only.
	uint32	NormalTexture;
	uint32	OrmTexture;
	uint32	EmissiveTexture;
	uint32	MaterialFlags;	// D3D12_WORLD_MATERIAL_*
	float	BaseColor[4];
	float	Roughness;
	float	Metal;
	float	AlphaCutoff;
	float	MaterialPadding;
	float	Emissive[3];
	float	MaterialPadding2;
};

// D3D12WorldFaceData::MaterialFlags (World.hlsl MAT_*).
#define D3D12_WORLD_MATERIAL_NORMAL		0x0001u
#define D3D12_WORLD_MATERIAL_ORM		0x0002u
#define D3D12_WORLD_MATERIAL_EMISSIVE	0x0004u
#define D3D12_WORLD_MATERIAL_LIGHTMAP	0x0008u
#define D3D12_WORLD_MATERIAL_RETRO		0x0010u
#define D3D12_WORLD_MATERIAL_CUTOUT		0x0020u
#define D3D12_WORLD_MATERIAL_TWO_SIDED	0x0040u

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
void		DRIVERCC D3D12World_SetLights(const DRV_WorldLight* Lights, int32 NumLights);
int32		DRIVERCC D3D12World_RenderFacePBR(uint32 Geometry, uint32 Face, const DRV_WorldView* View,
											  grRDriver_Layer* Layers, int32 NumLayers,
											  void* LMapCBContext, uint32 Flags, float Alpha,
											  const DRV_WorldMaterial* Material);
int32		DRIVERCC D3D12World_RenderMesh(const DRV_MeshVertex* Verts, int32 NumVerts, const DRV_WorldView* View,
										   grRDriver_Layer* Layer, uint32 Flags);

#endif // D3D12WORLDGEOMETRY_H
