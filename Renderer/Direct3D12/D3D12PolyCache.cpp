/****************************************************************************************/
/*  D3D12POLYCACHE.CPP                                                                  */
/*                                                                                      */
/*  Dynamic transformed-polygon batching for the Jet3D rendering contract.              */
/****************************************************************************************/
#include <algorithm>
#include <cmath>
#include <cstring>

#include "D3D12PolyCache.h"
#include "D3D12PSOManager.h"
#include "D3D12Log.h"
#include "D3D12Common.h"
#include "D3D12UploadRing.h"
#include "D3D12WorldGeometry.h"

namespace
{
	float NormalizeColor(float Value)
	{
		return (std::max)(0.0f, (std::min)(Value, 255.0f)) * (1.0f / 255.0f);
	}

	float SafeReciprocal(float Value)
	{
		return (std::fabs(Value) > 0.000001f) ? (1.0f / Value) : 1.0f;
	}
}

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

grBoolean D3D12PolyCache::Initialize(int32 MaxVerts)
{
	if (!g_pDevice || MaxVerts < 3)
		return GR_FALSE;

	Shutdown();
	m_MaxVerts = MaxVerts;
	m_NumVerts = 0;
	m_Vertices.reserve(static_cast<size_t>(MaxVerts));
	m_Cache.reserve(static_cast<size_t>(MaxVerts / 3));
	m_bInitialized = true;
	D3D12Log::GetPtr()->Printf("D3D12 polygon cache initialized for %d vertices", MaxVerts);
	return GR_TRUE;
}

void D3D12PolyCache::Shutdown()
{
	for (auto& Buffer : m_StaticBuffers)
	{
		Buffer.pVertexBuffer.Reset();
		delete[] Buffer.Layers;
		Buffer.Layers = nullptr;
		Buffer.Active = GR_FALSE;
	}
	m_StaticBuffers.clear();

	m_Cache.clear();
	m_Vertices.clear();
	m_NumVerts = 0;
	m_MaxVerts = 0;
	m_bInitialized = false;
}

void D3D12PolyCache::BeginFrame(UINT)
{
	// Vertex memory comes from the per-frame upload ring. A completed EndScene always empties these. Clearing here also recovers cleanly
	// if an application abandoned a scene after an error.
	m_Cache.clear();
	m_Vertices.clear();
	m_NumVerts = 0;
}

grBoolean D3D12PolyCache::AddPolygon(
	grTLVertex* Pnts,
	int32 NumPoints,
	grRDriver_Layer* Layers,
	int32 NumLayers,
	uint32 Flags,
	grBoolean WorldCoordinates)
{
	if (!m_bInitialized || !g_bInScene || !Pnts || NumPoints < 3)
		return GR_FALSE;

	const int32 TriangleVertexCount = (NumPoints - 2) * 3;
	if (m_NumVerts + TriangleVertexCount > m_MaxVerts && !m_Cache.empty())
	{
		if (!Flush())
			return GR_FALSE;
	}
	if (TriangleVertexCount > m_MaxVerts)
		m_MaxVerts = TriangleVertexCount;

	int32 UsableLayers = NumLayers;
	if (UsableLayers < 0)
		UsableLayers = 0;
	if (UsableLayers > MAX_LAYERS)
		UsableLayers = MAX_LAYERS;
	for (int32 Layer = 0; Layer < UsableLayers; ++Layer)
	{
		if (!Layers || !Layers[Layer].THandle || !D3D12_THandle_GetResource(Layers[Layer].THandle))
		{
			UsableLayers = Layer;
			break;
		}
	}

	std::vector<PolyVert> Converted(static_cast<size_t>(NumPoints));
	for (int32 i = 0; i < NumPoints; ++i)
	{
		PolyVert& Vertex = Converted[static_cast<size_t>(i)];
		Vertex.x = Pnts[i].x;
		Vertex.y = Pnts[i].y;
		Vertex.z = Pnts[i].z;
		Vertex.rhw = SafeReciprocal(Pnts[i].z);
		Vertex.r = NormalizeColor(Pnts[i].r);
		Vertex.g = NormalizeColor(Pnts[i].g);
		Vertex.b = NormalizeColor(Pnts[i].b);
		Vertex.a = NormalizeColor(Pnts[i].a);
		Vertex.u = Pnts[i].u;
		Vertex.v = Pnts[i].v;
		Vertex.lu = Pnts[i].pad1;
		Vertex.lv = Pnts[i].pad2;

		if (WorldCoordinates && UsableLayers > 0)
		{
			const grRDriver_Layer& TextureLayer = Layers[0];
			const float TextureScale = static_cast<float>(1u << TextureLayer.THandle->Log);
			Vertex.u = (Pnts[i].u * SafeReciprocal(TextureLayer.ScaleU) + TextureLayer.ShiftU) /
				TextureScale;
			Vertex.v = (Pnts[i].v * SafeReciprocal(TextureLayer.ScaleV) + TextureLayer.ShiftV) /
				TextureScale;

			if (UsableLayers > 1)
			{
				const grRDriver_Layer& LightLayer = Layers[1];
				const float LightScale = static_cast<float>((1u << LightLayer.THandle->Log) << 4);
				Vertex.lu = (Pnts[i].u - LightLayer.ShiftU + 8.0f) / LightScale;
				Vertex.lv = (Pnts[i].v - LightLayer.ShiftV + 8.0f) / LightScale;
			}
		}
	}

	PolyCacheEntry Entry = {};
	Entry.World = nullptr;
	Entry.StartVertex = m_NumVerts;
	Entry.NumVertices = TriangleVertexCount;
	Entry.NumLayers = UsableLayers;
	Entry.Flags = Flags;
	for (int32 Layer = 0; Layer < MAX_LAYERS; ++Layer)
		Entry.Layers[Layer] = (Layer < UsableLayers) ? Layers[Layer].THandle : nullptr;

	// D3D12 has no triangle-fan topology, so expand each convex Jet3D polygon.
	for (int32 i = 1; i < NumPoints - 1; ++i)
	{
		m_Vertices.push_back(Converted[0]);
		m_Vertices.push_back(Converted[static_cast<size_t>(i)]);
		m_Vertices.push_back(Converted[static_cast<size_t>(i + 1)]);
	}
	m_NumVerts += TriangleVertexCount;
	m_Cache.push_back(Entry);

	if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
		return Flush();
	return GR_TRUE;
}

grBoolean D3D12PolyCache::AddGouraudPoly(grTLVertex* Pnts, int32 NumPoints, uint32 Flags)
{
	return AddPolygon(Pnts, NumPoints, nullptr, 0, Flags, GR_FALSE);
}

grBoolean D3D12PolyCache::AddMiscTexturePoly(
	grTLVertex* Pnts,
	int32 NumPoints,
	grRDriver_Layer* Layers,
	int32 NumLayers,
	uint32 Flags)
{
	return AddPolygon(Pnts, NumPoints, Layers, NumLayers, Flags, GR_FALSE);
}

grBoolean D3D12PolyCache::AddWorldPoly(
	grTLVertex* Pnts,
	int32 NumPoints,
	grRDriver_Layer* Layers,
	int32 NumLayers,
	void* LMapCBContext,
	uint32 Flags)
{
	if (LMapCBContext && Layers && NumLayers > 1 && Layers[1].THandle &&
		g_D3D12Drv.SetupLightmap)
	{
		grRDriver_LMapCBInfo LightInfo = {};
		g_D3D12Drv.SetupLightmap(&LightInfo, LMapCBContext);
		if (LightInfo.RGBLight[0] && (LightInfo.Dynamic || !Layers[1].THandle->Lightmap))
		{
			if (!D3D12_THandle_UpdateLightmap(
				Layers[1].THandle,
				static_cast<const uint8*>(LightInfo.RGBLight[0])))
			{
				D3D12Log::GetPtr()->Printf("WARNING: Lightmap upload failed; drawing base texture only");
				NumLayers = 1;
			}
		}
	}

	return AddPolygon(Pnts, NumPoints, Layers, NumLayers, Flags, GR_TRUE);
}

grBoolean D3D12PolyCache::AddWorldFace(
	const D3D12WorldGeometry* World,
	uint32 Face,
	D3D12_GPU_VIRTUAL_ADDRESS View,
	D3D12_GPU_VIRTUAL_ADDRESS FaceData,
	int32 NumLayers,
	uint32 Flags)
{
	if (!m_bInitialized || !g_bInScene || !World || Face >= World->Faces.size())
		return GR_FALSE;

	PolyCacheEntry Entry = {};
	Entry.World = World;
	Entry.WorldFace = Face;
	Entry.WorldView = View;
	Entry.WorldFaces = FaceData;
	Entry.NumLayers = NumLayers;
	Entry.Flags = Flags;
	m_Cache.push_back(Entry);

	if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
		return Flush();
	return GR_TRUE;
}

grBoolean D3D12PolyCache::UploadVertices(D3D12_VERTEX_BUFFER_VIEW& VertexBufferView)
{
	if (m_Vertices.empty())
		return GR_TRUE;

	const UINT64 BufferSize = static_cast<UINT64>(m_Vertices.size()) * sizeof(PolyVert);
	D3D12UploadAllocation Upload = {};
	if (!D3D12Upload_Allocate(BufferSize, sizeof(PolyVert), &Upload))
	{
		D3D12Log::GetPtr()->Printf("ERROR: Vertex upload allocation failed (%llu bytes)",
			static_cast<unsigned long long>(BufferSize));
		return GR_FALSE;
	}
	std::memcpy(Upload.CPU, m_Vertices.data(), static_cast<size_t>(BufferSize));

	VertexBufferView.BufferLocation = Upload.GPU;
	VertexBufferView.SizeInBytes = static_cast<UINT>(BufferSize);
	VertexBufferView.StrideInBytes = sizeof(PolyVert);
	return GR_TRUE;
}

grBoolean D3D12PolyCache::Flush()
{
	if (!m_bInitialized || m_Cache.empty())
		return GR_TRUE;
	if (!g_bInScene || !g_pCommandList || !g_pPSOManager)
		return GR_FALSE;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
	if (!UploadVertices(VertexBufferView))
		return GR_FALSE;

	// GPU world faces draw from their geometry's vertex buffer through one index
	// buffer per flush, into which each face's indices are copied in cache order.
	std::vector<uint32> WorldStart(m_Cache.size(), 0);
	D3D12_INDEX_BUFFER_VIEW WorldIndexView = {};
	{
		size_t NumWorldIndices = 0;
		for (size_t i = 0; i < m_Cache.size(); ++i)
		{
			if (m_Cache[i].World)
			{
				WorldStart[i] = static_cast<uint32>(NumWorldIndices);
				NumWorldIndices += m_Cache[i].World->Faces[m_Cache[i].WorldFace].NumIndices;
			}
		}
		if (NumWorldIndices > 0)
		{
			D3D12UploadAllocation Upload = {};
			if (!D3D12Upload_Allocate(NumWorldIndices * sizeof(uint32), sizeof(uint32), &Upload))
				return GR_FALSE;
			uint32* Destination = static_cast<uint32*>(Upload.CPU);
			for (const PolyCacheEntry& Entry : m_Cache)
			{
				if (!Entry.World)
					continue;
				const DRV_WorldFace& Face = Entry.World->Faces[Entry.WorldFace];
				std::memcpy(Destination, &Entry.World->Indices[Face.FirstIndex], Face.NumIndices * sizeof(uint32));
				Destination += Face.NumIndices;
			}
			WorldIndexView.BufferLocation = Upload.GPU;
			WorldIndexView.SizeInBytes = static_cast<UINT>(NumWorldIndices * sizeof(uint32));
			WorldIndexView.Format = DXGI_FORMAT_R32_UINT;
		}
	}

	D3D12BeginMarker(g_pCommandList.Get(), "PolyCache flush");

	// The texture heap was set by BeginScene. Everything but the per-draw constants
	// (and, without bindless, the texture tables) is bound once per flush.
	const bool bBindless = g_pPSOManager->IsBindless();
	g_pCommandList->SetGraphicsRootSignature(g_pPSOManager->GetRootSignature());
	g_pCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAM_FRAME, g_FrameConstantsGPU);
	if (bBindless)
		g_pCommandList->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURES,
			D3D12_THandle_GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	g_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (WorldIndexView.SizeInBytes)
		g_pCommandList->IASetIndexBuffer(&WorldIndexView);

	const D3D12WorldGeometry* BoundWorld = nullptr;	// nullptr = the transformed-poly buffer
	bool bVertexBufferBound = false;
	D3D12_GPU_VIRTUAL_ADDRESS BoundView = 0;
	D3D12_GPU_VIRTUAL_ADDRESS BoundFaces = 0;

	for (size_t Index = 0; Index < m_Cache.size(); ++Index)
	{
		const PolyCacheEntry& Entry = m_Cache[Index];

		if (Entry.World)
		{
			// Merge the run of faces that share geometry, view, layers and flags.
			size_t End = Index + 1;
			UINT NumIndices = Entry.World->Faces[Entry.WorldFace].NumIndices;
			while (End < m_Cache.size())
			{
				const PolyCacheEntry& Next = m_Cache[End];
				if (Next.World != Entry.World || Next.WorldView != Entry.WorldView ||
					Next.WorldFaces != Entry.WorldFaces || Next.NumLayers != Entry.NumLayers ||
					Next.Flags != Entry.Flags)
					break;
				NumIndices += Next.World->Faces[Next.WorldFace].NumIndices;
				++End;
			}

			ID3D12PipelineState* Pipeline = g_pPSOManager->GetPSO(
				(Entry.NumLayers > 1) ? PSO_WORLD_MULTITEX : PSO_WORLD_TEXTURE,
				Entry.Flags,
				g_bWireframe ? GR_TRUE : GR_FALSE);
			if (!Pipeline)
				return GR_FALSE;
			g_pCommandList->SetPipelineState(Pipeline);

			if (!bVertexBufferBound || BoundWorld != Entry.World)
			{
				g_pCommandList->IASetVertexBuffers(0, 1, &Entry.World->VertexBufferView);
				BoundWorld = Entry.World;
				bVertexBufferBound = true;
			}
			if (BoundView != Entry.WorldView)
			{
				g_pCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAM_WORLD_VIEW, Entry.WorldView);
				BoundView = Entry.WorldView;
			}
			if (BoundFaces != Entry.WorldFaces)
			{
				g_pCommandList->SetGraphicsRootShaderResourceView(ROOT_PARAM_WORLD_FACES, Entry.WorldFaces);
				BoundFaces = Entry.WorldFaces;
			}

			const D3D12DrawConstants Constants = { Entry.Flags, 0, 0, 0 };
			g_pCommandList->SetGraphicsRoot32BitConstants(ROOT_PARAM_DRAW, 4, &Constants, 0);
			g_pCommandList->DrawIndexedInstanced(NumIndices, 1, WorldStart[Index], 0, 0);
			g_D3D12Drv.NumRenderedPolys += static_cast<S32>(End - Index);

			Index = End - 1;
			continue;
		}

		if (!bVertexBufferBound || BoundWorld)
		{
			g_pCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
			BoundWorld = nullptr;
			bVertexBufferBound = true;
		}

		int32 NumLayers = Entry.NumLayers;
		if (NumLayers > 0 && Entry.Layers[0]->ResourceState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			NumLayers = 0;
		if (NumLayers > 1 && Entry.Layers[1]->ResourceState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			NumLayers = 1;

		D3D12_PSO_TYPE Type = PSO_GOURAUD;
		if (NumLayers == 1)
			Type = PSO_TEXTURE;
		else if (NumLayers > 1)
			Type = PSO_MULTITEX;

		ID3D12PipelineState* Pipeline = g_pPSOManager->GetPSO(
			Type,
			Entry.Flags,
			g_bWireframe ? GR_TRUE : GR_FALSE);
		if (!Pipeline)
			return GR_FALSE;
		g_pCommandList->SetPipelineState(Pipeline);

		const D3D12DrawConstants Constants = {
			Entry.Flags,
			(NumLayers > 0) ? D3D12_THandle_GetDescriptorIndex(Entry.Layers[0]) : 0,
			(NumLayers > 1) ? D3D12_THandle_GetDescriptorIndex(Entry.Layers[1]) : 0,
			0
		};
		g_pCommandList->SetGraphicsRoot32BitConstants(ROOT_PARAM_DRAW, 4, &Constants, 0);
		if (!bBindless && NumLayers > 0)
			g_pCommandList->SetGraphicsRootDescriptorTable(ROOT_PARAM_BASE_TABLE, D3D12_THandle_GetGPUSRV(Entry.Layers[0]));
		if (!bBindless && NumLayers > 1)
			g_pCommandList->SetGraphicsRootDescriptorTable(ROOT_PARAM_LIGHT_TABLE, D3D12_THandle_GetGPUSRV(Entry.Layers[1]));

		g_pCommandList->DrawInstanced(
			static_cast<UINT>(Entry.NumVertices),
			1,
			static_cast<UINT>(Entry.StartVertex),
			0);
		++g_D3D12Drv.NumRenderedPolys;
	}

	D3D12EndMarker(g_pCommandList.Get());

	m_Cache.clear();
	m_Vertices.clear();
	m_NumVerts = 0;
	return GR_TRUE;
}

// Hardware-transformed static meshes are deliberately not advertised in device caps.
// Returning failure is safer than the old placeholder, which returned a valid-looking
// handle without ever uploading or drawing its vertex data.
uint32 D3D12PolyCache::AddStaticBuffer(
	grHWVertex*,
	int32,
	grRDriver_Layer*,
	int32,
	uint32)
{
	D3D12Log::GetPtr()->Printf("Static hardware buffers are not enabled for the transformed-vertex DX12 path");
	return 0;
}

grBoolean D3D12PolyCache::RemoveStaticBuffer(uint32)
{
	return GR_FALSE;
}

grBoolean D3D12PolyCache::RenderStaticBuffer(uint32, int32, int32, grXForm3d*)
{
	return GR_FALSE;
}
