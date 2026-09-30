/****************************************************************************************/
/*  D3D12WORLDGEOMETRY.CPP                                                              */
/*                                                                                      */
/*  GPU world path (see D3D12WorldGeometry.h).                                          */
/****************************************************************************************/
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "D3D12WorldGeometry.h"
#include "D3D12PolyCache.h"
#include "D3D12PSOManager.h"
#include "D3D12TextureMgr.h"
#include "D3D12UploadRing.h"
#include "D3D12LightmapAtlas.h"
#include "D3D12Config.h"
#include "D3D12Lighting.h"
#include "Direct3D12Driver.h"
#include "D3D12Log.h"

extern D3D12PolyCache* g_pPolyCache;

namespace
{
	// Handles are (generation << 20) | (slot + 1), so a handle from before a driver
	// re-initialization never finds a new geometry.
	const uint32 SlotBits = 20;
	const uint32 SlotMask = (1u << SlotBits) - 1;

	uint32										Generation = 0;
	bool										Enabled = false;
	std::vector<std::unique_ptr<D3D12WorldGeometry>>	Slots;
	UINT64										FrameSerial = 0;

	// The last view this frame, so faces of the same traversal share one constant buffer.
	DRV_WorldView								LastView;
	UINT64										LastViewFrame = 0;
	D3D12_GPU_VIRTUAL_ADDRESS					LastViewGPU = 0;

	struct ViewConstants
	{
		float	ModelToCamera[3][4];
		float	Scale;
		float	XCenter;
		float	YCenter;
		float	ZScale;
		float	ZFar;
		float	HalfWidth;
		float	HalfHeight;
		uint32	NumClipPlanes;
		float	ClipPlanes[DRV_WORLD_MAX_CLIP_PLANES][4];
		float	EyePos[4];						// the camera in world space
		float	ModelToWorld[3][4];
		uint32	UseClusters;					// the frame's main camera: light clusters apply
		uint32	ViewPadding[3];
	};

	// Rows of a rigid transform (rotation, w = translation).
	void XFormRows(const grXForm3d& M, float Rows[3][4])
	{
		const float R[3][4] = {
			{ M.AX, M.AY, M.AZ, M.Translation.X },
			{ M.BX, M.BY, M.BZ, M.Translation.Y },
			{ M.CX, M.CY, M.CZ, M.Translation.Z }
		};
		std::memcpy(Rows, R, sizeof(R));
	}

	float SafeReciprocal(float Value)
	{
		return (std::fabs(Value) > 0.000001f) ? (1.0f / Value) : 1.0f;
	}

	float NormalizeColor(float Value)
	{
		return (std::max)(0.0f, (std::min)(Value, 255.0f)) * (1.0f / 255.0f);
	}

	D3D12WorldGeometry* Lookup(uint32 Handle)
	{
		const uint32 Slot = (Handle & SlotMask);
		if (Slot == 0 || (Handle >> SlotBits) != (Generation & ((1u << (32 - SlotBits)) - 1)))
			return nullptr;
		if (Slot - 1 >= Slots.size())
			return nullptr;
		D3D12WorldGeometry* Geometry = Slots[Slot - 1].get();
		return (Geometry && Geometry->Handle == Handle && !Geometry->Retired) ? Geometry : nullptr;
	}

	bool TextureReady(grTexture* Handle)
	{
		return Handle && D3D12_THandle_GetResource(Handle) &&
			Handle->ResourceState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	// A static buffer filled once and left in FinalState.
	bool UploadBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, ComPtr<ID3D12Resource>& Buffer)
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Width = Size;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES DefaultHeap = {};
		DefaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_HEAP_PROPERTIES UploadHeap = {};
		UploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

		// Buffers start in COMMON and are promoted to COPY_DEST by the copy.
		ComPtr<ID3D12Resource> Upload;
		if (FAILED(g_pDevice->CreateCommittedResource(&DefaultHeap, D3D12_HEAP_FLAG_NONE, &Desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&Buffer))) ||
			FAILED(g_pDevice->CreateCommittedResource(&UploadHeap, D3D12_HEAP_FLAG_NONE, &Desc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Upload))))
		{
			D3D12Log::GetPtr()->Printf("ERROR: World buffer (%llu bytes) creation failed",
				static_cast<unsigned long long>(Size));
			return false;
		}

		void* Mapped = nullptr;
		D3D12_RANGE NoReads = { 0, 0 };
		if (FAILED(Upload->Map(0, &NoReads, &Mapped)))
			return false;
		std::memcpy(Mapped, Data, static_cast<size_t>(Size));
		Upload->Unmap(0, nullptr);

		// A one-off copy on the queue. Everything already queued finishes first, and the
		// wait means no open command list can see the buffer before it is filled.
		ComPtr<ID3D12CommandAllocator> Allocator;
		ComPtr<ID3D12GraphicsCommandList> CommandList;
		if (FAILED(g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator))) ||
			FAILED(g_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator.Get(), nullptr,
				IID_PPV_ARGS(&CommandList))))
			return false;

		CommandList->CopyBufferRegion(Buffer.Get(), 0, Upload.Get(), 0, Size);
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Transition.pResource = Buffer.Get();
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		Barrier.Transition.StateAfter = FinalState;
		CommandList->ResourceBarrier(1, &Barrier);
		if (FAILED(CommandList->Close()))
			return false;
		ID3D12CommandList* Lists[] = { CommandList.Get() };
		g_pCommandQueue->ExecuteCommandLists(1, Lists);
		D3D12WaitForGPU();
		return true;
	}

	bool UploadGeometry(D3D12WorldGeometry& Geometry, const DRV_WorldVertex* Verts, int32 NumVerts)
	{
		const UINT64 VertexSize = static_cast<UINT64>(NumVerts) * sizeof(DRV_WorldVertex);
		const UINT64 IndexSize = static_cast<UINT64>(Geometry.Indices.size()) * sizeof(uint32);
		if (!UploadBuffer(Verts, VertexSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, Geometry.VertexBuffer) ||
			!UploadBuffer(Geometry.Indices.data(), IndexSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, Geometry.IndexBuffer))
			return false;

		Geometry.VertexBufferView.BufferLocation = Geometry.VertexBuffer->GetGPUVirtualAddress();
		Geometry.VertexBufferView.SizeInBytes = static_cast<UINT>(VertexSize);
		Geometry.VertexBufferView.StrideInBytes = sizeof(DRV_WorldVertex);
		Geometry.IndexBufferView.BufferLocation = Geometry.IndexBuffer->GetGPUVirtualAddress();
		Geometry.IndexBufferView.SizeInBytes = static_cast<UINT>(IndexSize);
		Geometry.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;

		// Bounding sphere: the box's center and the farthest vertex from it
		float Min[3] = { Verts[0].Pos[0], Verts[0].Pos[1], Verts[0].Pos[2] };
		float Max[3] = { Min[0], Min[1], Min[2] };
		for (int32 i = 1; i < NumVerts; ++i)
		{
			for (int c = 0; c < 3; ++c)
			{
				Min[c] = (std::min)(Min[c], Verts[i].Pos[c]);
				Max[c] = (std::max)(Max[c], Verts[i].Pos[c]);
			}
		}
		for (int c = 0; c < 3; ++c)
			Geometry.BoundsCenter[c] = (Min[c] + Max[c]) * 0.5f;
		float Radius2 = 0.0f;
		for (int32 i = 0; i < NumVerts; ++i)
		{
			float d2 = 0.0f;
			for (int c = 0; c < 3; ++c)
				d2 += (Verts[i].Pos[c] - Geometry.BoundsCenter[c]) * (Verts[i].Pos[c] - Geometry.BoundsCenter[c]);
			Radius2 = (std::max)(Radius2, d2);
		}
		Geometry.BoundsRadius = std::sqrt(Radius2);
		Geometry.HasTransform = false;
		return true;
	}

	D3D12_GPU_VIRTUAL_ADDRESS ViewConstantsFor(const DRV_WorldView* View)
	{
		if (LastViewFrame == FrameSerial && LastViewGPU &&
			std::memcmp(&LastView, View, sizeof(DRV_WorldView)) == 0)
			return LastViewGPU;

		D3D12UploadAllocation Upload = {};
		if (!D3D12Upload_Allocate(sizeof(ViewConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &Upload))
			return 0;

		ViewConstants Constants = {};
		float Rows[3][4];
		XFormRows(View->ModelToCamera, Rows);
		std::memcpy(Constants.ModelToCamera, Rows, sizeof(Rows));
		XFormRows(View->ModelToWorld, Constants.ModelToWorld);
		Constants.Scale = View->Scale;
		Constants.XCenter = View->XCenter;
		Constants.YCenter = View->YCenter;
		Constants.ZScale = View->ZScale;
		Constants.ZFar = View->ZFar;
		Constants.HalfWidth = View->HalfWidth;
		Constants.HalfHeight = View->HalfHeight;
		Constants.NumClipPlanes = (View->NumClipPlanes <= 0) ? 0u :
			static_cast<uint32>((View->NumClipPlanes < DRV_WORLD_MAX_CLIP_PLANES) ? View->NumClipPlanes : DRV_WORLD_MAX_CLIP_PLANES);
		std::memcpy(Constants.ClipPlanes, View->ClipPlanes, sizeof(Constants.ClipPlanes));
		// The rotation is orthonormal (mirrors only flip it), so the eye is -R^T t in model
		// space, then taken to world space.
		float EyeModel[3];
		for (int i = 0; i < 3; ++i)
			EyeModel[i] = -(Rows[0][i] * Rows[0][3] + Rows[1][i] * Rows[1][3] + Rows[2][i] * Rows[2][3]);
		for (int i = 0; i < 3; ++i)
			Constants.EyePos[i] = Constants.ModelToWorld[i][0] * EyeModel[0] + Constants.ModelToWorld[i][1] * EyeModel[1] +
				Constants.ModelToWorld[i][2] * EyeModel[2] + Constants.ModelToWorld[i][3];
		Constants.EyePos[3] = 1.0f;
		Constants.UseClusters = (View->NumClipPlanes == 0 && D3D12Lighting_IsMainCamera(Rows, Constants.ModelToWorld)) ? 1u : 0u;
		std::memcpy(Upload.CPU, &Constants, sizeof(Constants));

		LastView = *View;
		LastViewFrame = FrameSerial;
		LastViewGPU = Upload.GPU;
		return Upload.GPU;
	}

	D3D12WorldFaceData* FaceDataFor(D3D12WorldGeometry& Geometry)
	{
		if (Geometry.FaceDataFrame != FrameSerial || !Geometry.FaceDataCPU)
		{
			// Only the faces queued this frame are written; the shaders never read the rest.
			D3D12UploadAllocation Upload = {};
			if (!D3D12Upload_Allocate(Geometry.Faces.size() * sizeof(D3D12WorldFaceData), 256, &Upload))
				return nullptr;
			Geometry.FaceDataCPU = static_cast<D3D12WorldFaceData*>(Upload.CPU);
			Geometry.FaceDataGPU = Upload.GPU;
			Geometry.FaceDataFrame = FrameSerial;
		}
		return Geometry.FaceDataCPU;
	}
}

grBoolean D3D12World_Startup()
{
	D3D12World_Shutdown();
	++Generation;
	Enabled = g_pPSOManager && g_pPSOManager->IsBindless() && D3D12Config_GetBool("Render", "WorldPath", true);
	D3D12Log::GetPtr()->Printf(Enabled
		? "World: GPU world path enabled"
		: "World: GPU world path disabled (needs bindless textures; [Render] WorldPath)");
	D3D12Lightmap_Startup();
	return GR_TRUE;
}

void D3D12World_Shutdown()
{
	Slots.clear();
	LastViewGPU = 0;
	LastViewFrame = 0;
	Enabled = false;
}

void D3D12World_BeginFrame()
{
	++FrameSerial;

	const UINT64 Completed = D3D12GetCompletedFenceValue();
	for (auto& Slot : Slots)
	{
		if (Slot && Slot->Retired && Slot->RetireFence <= Completed)
			Slot.reset();
	}
}

uint32 DRIVERCC D3D12World_Create(const DRV_WorldVertex* Verts, int32 NumVerts,
								  const uint32* Indices, int32 NumIndices,
								  const DRV_WorldFace* Faces, int32 NumFaces)
{
	if (!Enabled || !g_pDevice || !Verts || !Indices || !Faces || NumVerts <= 0 || NumIndices <= 0 || NumFaces <= 0)
		return 0;

	for (int32 i = 0; i < NumIndices; ++i)
	{
		if (Indices[i] >= static_cast<uint32>(NumVerts))
		{
			D3D12Log::GetPtr()->Printf("ERROR: World geometry index %d out of range", i);
			return 0;
		}
	}
	for (int32 i = 0; i < NumFaces; ++i)
	{
		if (static_cast<UINT64>(Faces[i].FirstIndex) + Faces[i].NumIndices > static_cast<UINT64>(NumIndices))
		{
			D3D12Log::GetPtr()->Printf("ERROR: World geometry face %d out of range", i);
			return 0;
		}
	}

	std::unique_ptr<D3D12WorldGeometry> Geometry(new D3D12WorldGeometry());
	Geometry->Indices.assign(Indices, Indices + NumIndices);
	Geometry->Faces.assign(Faces, Faces + NumFaces);
	Geometry->FaceDataFrame = 0;
	Geometry->FaceDataCPU = nullptr;
	Geometry->FaceDataGPU = 0;
	Geometry->Retired = false;
	Geometry->RetireFence = 0;
	if (!UploadGeometry(*Geometry, Verts, NumVerts))
		return 0;

	size_t Slot = 0;
	while (Slot < Slots.size() && Slots[Slot])
		++Slot;
	if (Slot == Slots.size())
	{
		if (Slots.size() >= SlotMask)
			return 0;
		Slots.emplace_back();
	}

	Geometry->Handle = ((Generation & ((1u << (32 - SlotBits)) - 1)) << SlotBits) | static_cast<uint32>(Slot + 1);
	const uint32 Handle = Geometry->Handle;
	Slots[Slot] = std::move(Geometry);

	D3D12Log::GetPtr()->Printf("World geometry %08X: %d faces, %d vertices, %d indices",
		Handle, NumFaces, NumVerts, NumIndices);
	return Handle;
}

grBoolean DRIVERCC D3D12World_Destroy(uint32 Handle)
{
	D3D12WorldGeometry* Geometry = Lookup(Handle);
	if (!Geometry)
		return GR_FALSE;

	// Queued faces and in-flight frames may still use it.
	Geometry->Retired = true;
	Geometry->RetireFence = D3D12GetPendingFenceValue();
	return GR_TRUE;
}

// A PBR material's part of a face record.
static void FillMaterial(D3D12WorldFaceData& Data, const DRV_WorldMaterial& Mat, bool Lightmap)
{
	const DRV_WorldMaterial* Material = &Mat;
	// A map that is not ready yet shades with its constant, like a missing one.
	Data.NormalTexture = Data.OrmTexture = Data.EmissiveTexture = 0;
	if (Material->NormalMap && TextureReady(Material->NormalMap))
	{
		Data.NormalTexture = D3D12_THandle_GetDescriptorIndex(Material->NormalMap);
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_NORMAL;
	}
	if (Material->ORMMap && TextureReady(Material->ORMMap))
	{
		Data.OrmTexture = D3D12_THandle_GetDescriptorIndex(Material->ORMMap);
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_ORM;
	}
	if (Material->EmissiveMap && TextureReady(Material->EmissiveMap))
	{
		Data.EmissiveTexture = D3D12_THandle_GetDescriptorIndex(Material->EmissiveMap);
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_EMISSIVE;
	}
	Data.MaterialFlags |= Lightmap ? D3D12_WORLD_MATERIAL_LIGHTMAP : D3D12_WORLD_MATERIAL_VERTEX_LIGHT;
	if (Material->Flags & DRV_MATERIAL_RETRO)
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_RETRO;
	if (Material->Flags & DRV_MATERIAL_TWO_SIDED)
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_TWO_SIDED;
	if (Material->Flags & DRV_MATERIAL_LEGACY)
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_LEGACY;
	if (Material->AlphaMode == DRV_MATERIAL_ALPHA_CUTOUT)
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_CUTOUT;
	std::memcpy(Data.BaseColor, Material->BaseColor, sizeof(Data.BaseColor));
	Data.Roughness = Material->Roughness;
	Data.Metal = Material->Metal;
	Data.AlphaCutoff = Material->AlphaCutoff;
	std::memcpy(Data.Emissive, Material->Emissive, sizeof(Data.Emissive));
	Data.HeightScale = Material->HeightScale;
	Data.HeightTexture = 0;
	if (Material->HeightMap && Material->HeightScale > 0.0f && TextureReady(Material->HeightMap))
	{
		Data.HeightTexture = D3D12_THandle_GetDescriptorIndex(Material->HeightMap);
		Data.MaterialFlags |= D3D12_WORLD_MATERIAL_HEIGHT;
	}
}

static int32 RenderFace(uint32 Handle, uint32 Face, const DRV_WorldView* View,
						grRDriver_Layer* Layers, int32 NumLayers,
						void* LMapCBContext, uint32 Flags, float Alpha,
						const DRV_WorldMaterial* Material)
{
	if (!g_bInScene || !g_pPolyCache || !View || !Layers || NumLayers < 1)
		return DRV_WORLD_FACE_FALLBACK;

	D3D12WorldGeometry* Geometry = Lookup(Handle);
	if (!Geometry || Face >= Geometry->Faces.size())
		return DRV_WORLD_FACE_STALE;
	XFormRows(View->ModelToWorld, Geometry->ModelToWorld);
	Geometry->HasTransform = true;

	// The lightmap goes into the atlas when it can, else into its own handle exactly
	// as D3D12PolyCache::AddWorldPoly does it.
	D3D12LightmapPlacement Atlas = {};
	bool InAtlas = false;
	if (NumLayers > 1 && Layers[1].THandle)
	{
		grTexture* Lightmap = Layers[1].THandle;
		const uint8* RGB = nullptr;
		bool Dynamic = false;
		if (LMapCBContext && g_D3D12Drv.SetupLightmap)
		{
			grRDriver_LMapCBInfo LightInfo = {};
			g_D3D12Drv.SetupLightmap(&LightInfo, LMapCBContext);
			RGB = static_cast<const uint8*>(LightInfo.RGBLight[0]);
			Dynamic = LightInfo.Dynamic != 0;
		}
		InAtlas = D3D12Lightmap_Place(Lightmap, (RGB && (Dynamic || !Lightmap->AtlasResident)) ? RGB : nullptr, &Atlas) != 0;
		if (!InAtlas && RGB && (Dynamic || !Lightmap->Lightmap))
		{
			if (!D3D12_THandle_UpdateLightmap(Lightmap, RGB))
				NumLayers = 1;
		}
	}

	// The transformed-poly path draws a face whose texture is not ready untextured.
	if (!TextureReady(Layers[0].THandle))
		return DRV_WORLD_FACE_FALLBACK;
	if (NumLayers > 1 && !InAtlas && !TextureReady(Layers[1].THandle))
		NumLayers = 1;
	if (NumLayers > 2)
		NumLayers = 2;

	D3D12WorldFaceData* FaceData = FaceDataFor(*Geometry);
	const D3D12_GPU_VIRTUAL_ADDRESS ViewGPU = ViewConstantsFor(View);
	if (!FaceData || !ViewGPU)
		return DRV_WORLD_FACE_FALLBACK;

	D3D12WorldFaceData& Data = FaceData[Face];
	Data.BaseTexture = D3D12_THandle_GetDescriptorIndex(Layers[0].THandle);
	Data.InvScaleU = SafeReciprocal(Layers[0].ScaleU);
	Data.InvScaleV = SafeReciprocal(Layers[0].ScaleV);
	Data.ShiftU = Layers[0].ShiftU;
	Data.ShiftV = Layers[0].ShiftV;
	Data.TextureScale = static_cast<float>(1u << Layers[0].THandle->Log);
	Data.LightTexture = 0;
	Data.LightShiftU = Data.LightShiftV = 0.0f;
	Data.LightOffsetU = Data.LightOffsetV = 0.0f;
	Data.LightDivU = Data.LightDivV = 1.0f;
	if (NumLayers > 1)
	{
		Data.LightShiftU = Layers[1].ShiftU;
		Data.LightShiftV = Layers[1].ShiftV;
		if (InAtlas)
		{
			Data.LightTexture = Atlas.DescriptorIndex;
			Data.LightOffsetU = Atlas.OffsetU;
			Data.LightOffsetV = Atlas.OffsetV;
			Data.LightDivU = Data.LightDivV = Atlas.PageSize;
		}
		else
		{
			Data.LightTexture = D3D12_THandle_GetDescriptorIndex(Layers[1].THandle);
			Data.LightDivU = static_cast<float>(Layers[1].THandle->Width * 16);
			Data.LightDivV = static_cast<float>(Layers[1].THandle->Height * 16);
		}
	}
	Data.Alpha = NormalizeColor(Alpha);
	Data.Padding[0] = Data.Padding[1] = 0;
	Data.MaterialFlags = 0;
	if (Material)
		FillMaterial(Data, *Material, NumLayers > 1);
	return g_pPolyCache->AddWorldFace(Geometry, Face, ViewGPU, Geometry->FaceDataGPU, NumLayers, Flags, Material != nullptr)
		? DRV_WORLD_FACE_DRAWN
		: DRV_WORLD_FACE_FALLBACK;
}

int32 DRIVERCC D3D12World_RenderFace(uint32 Handle, uint32 Face, const DRV_WorldView* View,
									 grRDriver_Layer* Layers, int32 NumLayers,
									 void* LMapCBContext, uint32 Flags, float Alpha)
{
	return RenderFace(Handle, Face, View, Layers, NumLayers, LMapCBContext, Flags, Alpha, nullptr);
}

int32 DRIVERCC D3D12World_RenderFacePBR(uint32 Handle, uint32 Face, const DRV_WorldView* View,
										grRDriver_Layer* Layers, int32 NumLayers,
										void* LMapCBContext, uint32 Flags, float Alpha,
										const DRV_WorldMaterial* Material)
{
	return RenderFace(Handle, Face, View, Layers, NumLayers, LMapCBContext, Flags, Alpha, Material);
}

int32 DRIVERCC D3D12World_RenderMeshPBR(const DRV_MeshVertexPBR* Verts, int32 NumVerts, const DRV_WorldView* View,
										grRDriver_Layer* Layer, uint32 Flags, const DRV_WorldMaterial* Material)
{
	if (!Enabled || !g_pPolyCache || !g_bInScene || !Verts || !View || !Layer || !Material || NumVerts < 3 ||
		!TextureReady(Layer->THandle))
		return DRV_WORLD_FACE_FALLBACK;
	const D3D12_GPU_VIRTUAL_ADDRESS ViewGPU = ViewConstantsFor(View);
	// The run's material, as a one-face table: VSMeshPBR gives every vertex face 0.
	D3D12UploadAllocation Upload = {};
	if (!ViewGPU || !D3D12Upload_Allocate(sizeof(D3D12WorldFaceData), 256, &Upload))
		return DRV_WORLD_FACE_FALLBACK;
	D3D12WorldFaceData Data = {};
	Data.BaseTexture = D3D12_THandle_GetDescriptorIndex(Layer->THandle);
	Data.InvScaleU = Data.InvScaleV = 1.0f;
	Data.TextureScale = 1.0f;
	Data.Alpha = 1.0f;
	Data.LightDivU = Data.LightDivV = 1.0f;
	FillMaterial(Data, *Material, false);
	std::memcpy(Upload.CPU, &Data, sizeof(Data));
	return g_pPolyCache->AddWorldMesh(Verts, NumVerts, sizeof(DRV_MeshVertexPBR), Layer->THandle, ViewGPU, Flags, Upload.GPU)
		? DRV_WORLD_FACE_DRAWN
		: DRV_WORLD_FACE_FALLBACK;
}

// Version 9-12 per-draw lights. PBR shading uses the frame's lights (World_SetFrame) since
// version 13, so these are ignored.
void DRIVERCC D3D12World_SetLights(const DRV_WorldLight*, int32)
{
}

void D3D12World_EnumGeometry(void (*Fn)(const D3D12WorldGeometry& Geometry, void* Context), void* Context)
{
	if (!Enabled)
		return;
	for (const auto& Slot : Slots)
	{
		if (Slot && !Slot->Retired && Slot->HasTransform && Slot->IndexBuffer)
			Fn(*Slot, Context);
	}
}

int32 DRIVERCC D3D12World_RenderMesh(const DRV_MeshVertex* Verts, int32 NumVerts, const DRV_WorldView* View,
									 grRDriver_Layer* Layer, uint32 Flags)
{
	if (!Enabled || !g_pPolyCache)
		return DRV_WORLD_FACE_FALLBACK;
	if (!Verts)
		return DRV_WORLD_FACE_DRAWN;	// the path is available
	if (!g_bInScene || !View || !Layer || !Layer->THandle || !D3D12_THandle_GetResource(Layer->THandle) || NumVerts < 3)
		return DRV_WORLD_FACE_FALLBACK;

	const D3D12_GPU_VIRTUAL_ADDRESS ViewGPU = ViewConstantsFor(View);
	if (!ViewGPU)
		return DRV_WORLD_FACE_FALLBACK;

	return g_pPolyCache->AddWorldMesh(Verts, NumVerts, sizeof(DRV_MeshVertex), Layer->THandle, ViewGPU, Flags, 0)
		? DRV_WORLD_FACE_DRAWN
		: DRV_WORLD_FACE_FALLBACK;
}
