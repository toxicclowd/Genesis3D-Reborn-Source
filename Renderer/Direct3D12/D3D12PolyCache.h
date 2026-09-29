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

// Cached polygon entry
typedef struct PolyCacheEntry
{
	int32 StartVertex;
	int32 NumVertices;
	grTexture* Layers[MAX_LAYERS];
	int32 NumLayers;
	uint32 Flags;
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

	std::vector<ComPtr<ID3D12Resource>> m_FrameVertexBuffers[FRAME_COUNT];
	
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

	// Flush cached geometry
	grBoolean Flush();

private:
	grBoolean AddPolygon(grTLVertex* Pnts, int32 NumPoints, grRDriver_Layer* Layers,
		int32 NumLayers, uint32 Flags, grBoolean WorldCoordinates);
	grBoolean UploadVertices(ComPtr<ID3D12Resource>& VertexBuffer,
		D3D12_VERTEX_BUFFER_VIEW& VertexBufferView);
};

#endif // D3D12_POLY_CACHE_H
