/****************************************************************************************/
/*  D3D12POLYCACHE.CPP                                                                  */
/*                                                                                      */
/*  DirectX 12 Polygon Cache Implementation                                            */
/*  Handles geometry batching for efficient rendering                                  */
/*                                                                                      */
/****************************************************************************************/
#include "D3D12PolyCache.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"
#include <string.h>

D3D12PolyCache::D3D12PolyCache()
	: m_NumVerts(0)
	, m_MaxVerts(0)
	, m_bInitialized(false)
{
}

D3D12PolyCache::~D3D12PolyCache()
{
	Shutdown();
}

jeBoolean D3D12PolyCache::Initialize(int32 maxVerts)
{
	D3D12Log::GetPtr()->Printf("D3D12PolyCache::Initialize - MaxVerts: %d", maxVerts);

	m_MaxVerts = maxVerts;
	m_NumVerts = 0;

	// Create vertex buffer
	UINT64 bufferSize = sizeof(PolyVert) * m_MaxVerts;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = bufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = g_pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr,
		IID_PPV_ARGS(&m_pVertexBuffer)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create vertex buffer - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	// Create upload buffer
	D3D12_HEAP_PROPERTIES uploadHeapProps = {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask = 1;

	hr = g_pDevice->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pVertexUploadBuffer)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create upload buffer - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	// Set up vertex buffer view
	m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	m_VertexBufferView.StrideInBytes = sizeof(PolyVert);

	m_bInitialized = true;
	D3D12Log::GetPtr()->Printf("PolyCache initialized successfully");
	return JE_TRUE;
}

void D3D12PolyCache::Shutdown()
{
	if (!m_bInitialized)
		return;

	D3D12Log::GetPtr()->Printf("D3D12PolyCache::Shutdown");

	// Clear static buffers
	for (auto& buffer : m_StaticBuffers)
	{
		if (buffer.Active)
		{
			buffer.pVertexBuffer.Reset();
			if (buffer.Layers)
			{
				delete[] buffer.Layers;
				buffer.Layers = nullptr;
			}
			buffer.Active = JE_FALSE;
		}
	}
	m_StaticBuffers.clear();

	// Release resources
	m_pVertexBuffer.Reset();
	m_pVertexUploadBuffer.Reset();

	m_Cache.clear();
	m_Vertices.clear();
	m_bInitialized = false;
}

jeBoolean D3D12PolyCache::AddGouraudPoly(jeTLVertex* Pnts, int32 NumPoints, uint32 Flags)
{
	if (!m_bInitialized || !Pnts || NumPoints < 3)
		return JE_FALSE;

	// Check if we have room
	if (m_NumVerts + NumPoints > m_MaxVerts)
	{
		Flush();
	}

	// Convert vertices to PolyVert format
	int32 startVertex = m_NumVerts;
	for (int32 i = 0; i < NumPoints; i++)
	{
		PolyVert vert;
		vert.x = Pnts[i].x;
		vert.y = Pnts[i].y;
		vert.z = Pnts[i].z;
		vert.rhw = 1.0f / Pnts[i].z;  // Perspective divide
		vert.diffuse = (Pnts[i].r << 16) | (Pnts[i].g << 8) | Pnts[i].b | (Pnts[i].a << 24);
		vert.u = Pnts[i].u;
		vert.v = Pnts[i].v;
		vert.lu = 0.0f;
		vert.lv = 0.0f;

		m_Vertices.push_back(vert);
		m_NumVerts++;
	}

	// Add to cache
	PolyCacheEntry entry;
	entry.StartVertex = startVertex;
	entry.NumVertices = NumPoints;
	entry.NumLayers = 0;
	entry.Flags = Flags;
	for (int i = 0; i < MAX_LAYERS; i++)
		entry.Layers[i] = nullptr;

	m_Cache.push_back(entry);

	return JE_TRUE;
}

jeBoolean D3D12PolyCache::AddMiscTexturePoly(jeTLVertex* Pnts, int32 NumPoints, jeRDriver_Layer* Layers, int32 NumLayers, uint32 Flags)
{
	if (!m_bInitialized || !Pnts || NumPoints < 3)
		return JE_FALSE;

	// Check if we have room
	if (m_NumVerts + NumPoints > m_MaxVerts)
	{
		Flush();
	}

	// Convert vertices
	int32 startVertex = m_NumVerts;
	for (int32 i = 0; i < NumPoints; i++)
	{
		PolyVert vert;
		vert.x = Pnts[i].x;
		vert.y = Pnts[i].y;
		vert.z = Pnts[i].z;
		vert.rhw = 1.0f / Pnts[i].z;
		vert.diffuse = (Pnts[i].r << 16) | (Pnts[i].g << 8) | Pnts[i].b | (Pnts[i].a << 24);
		vert.u = Pnts[i].u;
		vert.v = Pnts[i].v;
		vert.lu = 0.0f;
		vert.lv = 0.0f;

		m_Vertices.push_back(vert);
		m_NumVerts++;
	}

	// Add to cache
	PolyCacheEntry entry;
	entry.StartVertex = startVertex;
	entry.NumVertices = NumPoints;
	entry.NumLayers = (NumLayers < MAX_LAYERS) ? NumLayers : MAX_LAYERS;
	entry.Flags = Flags;

	for (int i = 0; i < MAX_LAYERS; i++)
	{
		if (i < NumLayers && Layers)
			entry.Layers[i] = Layers[i].THandle;
		else
			entry.Layers[i] = nullptr;
	}

	m_Cache.push_back(entry);

	return JE_TRUE;
}

jeBoolean D3D12PolyCache::AddWorldPoly(jeTLVertex* Pnts, int32 NumPoints, jeRDriver_Layer* Layers, int32 NumLayers, void* LMapCBContext, uint32 Flags)
{
	// For now, treat world polys like misc texture polys
	// TODO: Implement lightmap callback handling
	return AddMiscTexturePoly(Pnts, NumPoints, Layers, NumLayers, Flags);
}

jeBoolean D3D12PolyCache::Flush()
{
	if (!m_bInitialized || m_Cache.empty())
		return JE_TRUE;

	D3D12Log::GetPtr()->Printf("PolyCache::Flush - %d polys, %d verts", (int)m_Cache.size(), m_NumVerts);

	// Upload vertices to GPU
	if (!UploadVertices())
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to upload vertices");
		return JE_FALSE;
	}

	// TODO: Set up PSO and render the cached polygons
	// This would involve:
	// 1. Setting the appropriate PSO based on poly flags
	// 2. Binding textures
	// 3. Drawing each cached entry
	// For now, we just clear the cache

	// Clear cache
	m_Cache.clear();
	m_Vertices.clear();
	m_NumVerts = 0;

	return JE_TRUE;
}

jeBoolean D3D12PolyCache::UploadVertices()
{
	if (m_Vertices.empty())
		return JE_TRUE;

	// Map upload buffer
	void* pData = nullptr;
	HRESULT hr = m_pVertexUploadBuffer->Map(0, nullptr, &pData);
	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to map upload buffer - HR: 0x%08X", hr);
		return JE_FALSE;
	}

	// Copy vertex data
	memcpy(pData, m_Vertices.data(), m_NumVerts * sizeof(PolyVert));

	m_pVertexUploadBuffer->Unmap(0, nullptr);

	// Copy from upload buffer to vertex buffer
	// TODO: This should be done via command list during rendering
	// For now, we just acknowledge the upload

	return JE_TRUE;
}

void D3D12PolyCache::EnableAlpha(jeBoolean Enable)
{
	// TODO: Set blend state in PSO
	// For now, just log
	D3D12Log::GetPtr()->Printf("EnableAlpha: %d", Enable);
}

//================================================================================
//	Static Mesh Management
//================================================================================

uint32 D3D12PolyCache::AddStaticBuffer(jeHWVertex* Points, int32 NumPoints, jeRDriver_Layer* Layers, int32 NumLayers, uint32 Flags)
{
	D3D12Log::GetPtr()->Printf("AddStaticBuffer: %d points, %d layers", NumPoints, NumLayers);

	// Find free slot
	uint32 index = 0;
	for (size_t i = 0; i < m_StaticBuffers.size(); i++)
	{
		if (!m_StaticBuffers[i].Active)
		{
			index = static_cast<uint32>(i);
			break;
		}
	}

	// Add new buffer if no free slot
	if (index == 0 && (m_StaticBuffers.empty() || m_StaticBuffers[0].Active))
	{
		StaticBuffer newBuffer;
		memset(&newBuffer, 0, sizeof(StaticBuffer));
		m_StaticBuffers.push_back(newBuffer);
		index = static_cast<uint32>(m_StaticBuffers.size() - 1);
	}

	StaticBuffer& buffer = m_StaticBuffers[index];
	buffer.Active = JE_TRUE;
	buffer.NumVerts = NumPoints;
	buffer.Flags = Flags;

	// Create vertex buffer for static mesh
	UINT64 bufferSize = sizeof(jeHWVertex) * NumPoints;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = bufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = g_pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr,
		IID_PPV_ARGS(&buffer.pVertexBuffer)
	);

	if (FAILED(hr))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Failed to create static vertex buffer");
		buffer.Active = JE_FALSE;
		return 0;
	}

	// Set up vertex buffer view
	buffer.VertexBufferView.BufferLocation = buffer.pVertexBuffer->GetGPUVirtualAddress();
	buffer.VertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	buffer.VertexBufferView.StrideInBytes = sizeof(jeHWVertex);

	// Copy layer info
	if (NumLayers > 0 && Layers)
	{
		buffer.NumLayers = NumLayers;
		buffer.Layers = new jeRDriver_Layer[NumLayers];
		memcpy(buffer.Layers, Layers, NumLayers * sizeof(jeRDriver_Layer));
	}
	else
	{
		buffer.NumLayers = 0;
		buffer.Layers = nullptr;
	}

	// TODO: Upload vertex data via command list

	D3D12Log::GetPtr()->Printf("Static buffer created - ID: %d", index);
	return index + 1;  // Return 1-based index
}

jeBoolean D3D12PolyCache::RemoveStaticBuffer(uint32 id)
{
	if (id == 0 || id > m_StaticBuffers.size())
		return JE_FALSE;

	uint32 index = id - 1;
	StaticBuffer& buffer = m_StaticBuffers[index];

	if (!buffer.Active)
		return JE_FALSE;

	D3D12Log::GetPtr()->Printf("RemoveStaticBuffer: %d", id);

	buffer.pVertexBuffer.Reset();
	if (buffer.Layers)
	{
		delete[] buffer.Layers;
		buffer.Layers = nullptr;
	}
	buffer.Active = JE_FALSE;

	return JE_TRUE;
}

jeBoolean D3D12PolyCache::RenderStaticBuffer(uint32 id, int32 StartVertex, int32 NumPolys, jeXForm3d* XForm)
{
	if (id == 0 || id > m_StaticBuffers.size())
		return JE_FALSE;

	uint32 index = id - 1;
	StaticBuffer& buffer = m_StaticBuffers[index];

	if (!buffer.Active)
		return JE_FALSE;

	// TODO: Render the static mesh
	// This would involve:
	// 1. Setting the world matrix from XForm
	// 2. Binding the appropriate PSO
	// 3. Binding textures from layers
	// 4. Drawing the geometry
	// For now, just acknowledge the call

	D3D12Log::GetPtr()->Printf("RenderStaticBuffer: ID %d, Start %d, NumPolys %d", id, StartVertex, NumPolys);

	return JE_TRUE;
}
