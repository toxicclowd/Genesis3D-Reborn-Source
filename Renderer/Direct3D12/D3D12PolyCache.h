/****************************************************************************************/
/*  D3D12POLYCACHE.H                                                                    */
/*                                                                                      */
/*  DirectX 12 Polygon Cache for Geometry Batching                                     */
/*  Modeled after D3D9 PolyCache for compatibility                                     */
/*                                                                                      */
/****************************************************************************************/
#ifndef D3D12_POLY_CACHE_H
#define D3D12_POLY_CACHE_H

#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "DCommon.h"
#include "Direct3D12Driver.h"
#include "D3D12TextureMgr.h"

using Microsoft::WRL::ComPtr;

// Maximum number of layers (textures) per polygon
#define MAX_LAYERS 2

// Vertex structure for polygon cache (screen-space vertices)
typedef struct PolyVert
{
	float x, y, z, rhw;
	float r, g, b, a;
	float u, v;
	float lu, lv;  // Lightmap coordinates
} PolyVert;

struct D3D12WorldGeometry;

// Cached draw, kept in submission order. Transformed polys use StartVertex/NumVertices
// in the flush's vertex buffer; GPU world faces (World != NULL) use a face of World;
// world meshes (MeshVertices.SizeInBytes != 0) use their own vertices in the upload ring,
// with Layers[0] and WorldView.
typedef struct PolyCacheEntry
{
	int32 StartVertex;
	int32 NumVertices;
	grTexture* Layers[MAX_LAYERS];
	int32 NumLayers;
	uint32 Flags;

	const D3D12WorldGeometry* World;
	uint32 WorldFace;
	D3D12_GPU_VIRTUAL_ADDRESS WorldView;
	D3D12_GPU_VIRTUAL_ADDRESS WorldFaces;
	bool WorldPBR;						// shaded with the face's PBR material
	D3D12_VERTEX_BUFFER_VIEW MeshVertices;
	bool EndPass;						// DRV_Driver::World_EndPass: the 3D scene ends here
} PolyCacheEntry;

// Static mesh buffer
typedef struct StaticBuffer
{
	grBoolean Active;
	ComPtr<ID3D12Resource> pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	int32 NumVerts;
	grRDriver_Layer* Layers;
	int32 NumLayers;
	uint32 Flags;
} StaticBuffer;

// D3D12PolyCache class - manages geometry batching
class D3D12PolyCache
{
public:
	D3D12PolyCache();
	virtual ~D3D12PolyCache();

private:
	std::vector<PolyCacheEntry> m_Cache;
	std::vector<PolyVert> m_Vertices;
	std::vector<StaticBuffer> m_StaticBuffers;

	
	int32 m_NumVerts;
	int32 m_MaxVerts;
	bool m_bInitialized;

public:
	// Initialization
	grBoolean Initialize(int32 maxVerts = 10000);
	void Shutdown();
	void BeginFrame(UINT frameIndex);

	// Static mesh management
	uint32 AddStaticBuffer(grHWVertex* Points, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags);
	grBoolean RemoveStaticBuffer(uint32 id);
	grBoolean RenderStaticBuffer(uint32 id, int32 StartVertex, int32 NumPolys, grXForm3d* XForm);

	// Dynamic polygon batching
	grBoolean AddGouraudPoly(grTLVertex* Pnts, int32 NumPoints, uint32 Flags);
	grBoolean AddMiscTexturePoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, uint32 Flags);
	grBoolean AddWorldPoly(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers, int32 NumLayers, void* LMapCBContext, uint32 Flags);
	// A face of GPU world geometry (see D3D12WorldGeometry.h).
	grBoolean AddWorldFace(const D3D12WorldGeometry* World, uint32 Face, D3D12_GPU_VIRTUAL_ADDRESS View,
		D3D12_GPU_VIRTUAL_ADDRESS FaceData, int32 NumLayers, uint32 Flags, bool PBR = false);
	// A world mesh (DRV_MeshVertex triangle list, see DRV_Driver::WorldMesh_Render).
	// Stride is sizeof(DRV_MeshVertex), or sizeof(DRV_MeshVertexPBR) with Material (the
	// run's one-face material table) for a PBR mesh.
	grBoolean AddWorldMesh(const void* Verts, int32 NumVerts, UINT Stride, grTexture* Texture,
		D3D12_GPU_VIRTUAL_ADDRESS View, uint32 Flags, D3D12_GPU_VIRTUAL_ADDRESS Material);

	// The end of the 3D scene (DRV_Driver::World_EndPass): post-processing runs here.
	void AddEndPass();

	// Records the frame: the lighting passes, the scene, post-processing and the overlay.
	// Call once, at EndScene. *UsedComposite tells the present pass what to show.
	grBoolean Flush(bool* UsedComposite = nullptr);

private:
	// Binds what every draw of the main root signature needs.
	void BindCommon(D3D12_GPU_VIRTUAL_ADDRESS FrameConstants, const D3D12_INDEX_BUFFER_VIEW& WorldIndexView);
	grBoolean AddPolygon(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers,
		int32 NumLayers, uint32 Flags, grBoolean WorldCoordinates);
	grBoolean UploadVertices(D3D12_VERTEX_BUFFER_VIEW& VertexBufferView);
};

#endif // D3D12_POLY_CACHE_H
