/****************************************************************************************/
/*  JEMATERIAL.C                                                                        */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Jet3D Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.jet3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Jet3D, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#include <assert.h>
#include <string.h>

// Public dependents
#include "grMaterial.h"

// Private dependents
#include "grMaterial._h"
#include "Errorlog.h"
#include "Array.h"
#include "Ram.h"
#include "Engine.h"

#include "grGArray.h"

#include "grResource.h"
#include "grPtrMgr._h"

#include "log.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

//=======================================================================================
//=======================================================================================
typedef struct grMaterial
{
	char		MatName[GR_MATERIAL_MAX_NAME_SIZE];

	char		BitmapName[GR_MATERIAL_MAX_NAME_SIZE];

#ifdef _USE_BITMAPS
		grBitmap	*Bitmap;
#else
		grMaterialSpec	*MatSpec;	// krouer: can hold every kind of texture
#endif

	float		DrawScaleU;
	float		DrawScaleV;

	grRGBA		Reflectivity;

	int32		RefCount;

	// krouer : add a new value in the file
	int8		ResourceKind;
} grMaterial;

typedef struct grMaterial_Array
{
	int32		RefCount;

	grEngine	*Engine;

	grArray		*Array;

} grMaterial_Array;

typedef struct grMaterial_Context
{
	grEngine		*Engine;
	grResourceMgr	*ResMgr;
	uint16			Version;
	uint16			Reserved;
} grMaterial_Context;

//=======================================================================================
//	** grMaterial **
//=======================================================================================

//=======================================================================================
//	grMaterial_Create
//=======================================================================================
GRAPI grMaterial * GRCC grMaterial_Create(const char *MatName)
{
	grMaterial	*Material;

	assert(strlen(MatName) < GR_MATERIAL_MAX_NAME_SIZE);

	Material = GR_RAM_ALLOCATE_STRUCT(grMaterial);

	if (!Material)
		return NULL;

	ZeroMem(Material);

	if (!grMaterial_CreateRef(Material))
	{
		grRam_Free(Material);
		return NULL;
	}

	strcpy(Material->MatName, MatName);

	return Material;
}

//=======================================================================================
//	grMaterial_CreateRef
//=======================================================================================
GRAPI grBoolean GRCC grMaterial_CreateRef(grMaterial *Material)
{
	assert(Material->RefCount >= 0);

	Material->RefCount++;

	return GR_TRUE;
}

//=======================================================================================
//	grMaterial_Destroy
//=======================================================================================
GRAPI void GRCC grMaterial_Destroy(grMaterial **Material)
{
	assert((*Material)->RefCount >= 0);

	(*Material)->RefCount--;
	
	if ((*Material)->RefCount == 0)
	{
#ifdef _USE_BITMAPS
		if ((*Material)->Bitmap)
			grBitmap_Destroy(&(*Material)->Bitmap);
#else
		if ((*Material)->MatSpec)
			grMaterialSpec_Destroy(&(*Material)->MatSpec);
#endif
		grRam_Free(*Material);
	}

	*Material = NULL;
}

//=======================================================================================
//	grMaterial_SetDefaults
//=======================================================================================
GRAPI void GRCC grMaterial_SetDefaults(grMaterial *Material)
{
	memset(Material, 0, sizeof(grMaterial));
	Material->ResourceKind = GR_RESOURCE_BITMAP;
}

//=======================================================================================
//	grMaterial_SetBitmap
//=======================================================================================
GRAPI grBoolean GRCC grMaterial_SetBitmap(grMaterial *Mat, grBitmap *Bitmap, const char *BitmapName)
{
	assert(Mat);
	assert(strlen(BitmapName) < GR_MATERIAL_MAX_NAME_SIZE);

#ifdef _USE_BITMAPS
	if (Mat->Bitmap)
		grBitmap_Destroy(&Mat->Bitmap);		// Destroy any old bitmaps
	
	if (Bitmap)
		grBitmap_CreateRef(Bitmap);			// Ref the new one (if there is one)
	
	Mat->Bitmap = Bitmap;
	strcpy(Mat->BitmapName, BitmapName);

	Mat->ResourceKind = GR_RESOURCE_BITMAP;

	return GR_TRUE;
#else
	return GR_FALSE;
#endif
}

//=======================================================================================
//	grMaterial_GetBitmap
//=======================================================================================
GRAPI const grBitmap * GRCC grMaterial_GetBitmap(const grMaterial *Mat)
{
	assert( Mat != NULL );
#ifdef _USE_BITMAPS
	return Mat->Bitmap;
#else
	return NULL;
#endif
}

//=======================================================================================
//	grMaterial_GetMaterialSpec
//=======================================================================================
GRAPI const grMaterialSpec	* GRCC grMaterial_GetMaterialSpec(const grMaterial *Mat)
{
	assert( Mat != NULL );
#ifndef _USE_BITMAPS
	return Mat->MatSpec;
#else
	return NULL;
#endif
}

//=======================================================================================
//	grMaterial_GetName
//=======================================================================================
GRAPI const char * GRCC grMaterial_GetName(const grMaterial *Mat)
{
	assert( Mat != NULL );

	return Mat->MatName;
}

GRAPI const char * GRCC grMaterial_GetBitmapName( const grMaterial *Mat)
{
	assert( Mat != NULL );

	return Mat->BitmapName;
}

//=======================================================================================
//	** grMaterial_Array **
//=======================================================================================

#ifndef _USE_BITMAPS
//=======================================================================================
//	AttachPBRLayers
//	The PBR layers (normal, ORM, emissive) need driver handles too. Layer 0 is attached
//	and detached by the callers, as before.
//=======================================================================================
static void AttachPBRLayers(grEngine *Engine, const grMaterialSpec *MatSpec, grBoolean Attach)
{
	int32		idx;

	if (!Engine || !MatSpec)
		return;

	for (idx=1; idx<GR_MATERIAL_MAX_LAYER; idx++)
	{
		int32		Type = grMaterialSpec_GetLayerType(MatSpec, idx);
		grBitmap	*pBitmap;

		if (Type != GR_MATERIAL_LAYER_NORMAL && Type != GR_MATERIAL_LAYER_ORM && Type != GR_MATERIAL_LAYER_EMISSIVE)
			continue;
		pBitmap = grMaterialSpec_GetLayerBitmap(MatSpec, idx);
		if (!pBitmap)
			continue;
		if (Attach)
		{
			if (!grEngine_AddBitmap(Engine, pBitmap, GR_ENGINE_BITMAP_TYPE_3D))
				grErrorLog_AddString(-1, "AttachPBRLayers:  grEngine_AddBitmap failed.", NULL);
		}
		else
		{
			if (!grEngine_RemoveBitmap(Engine, pBitmap))
				grErrorLog_AddString(-1, "AttachPBRLayers:  grEngine_RemoveBitmap failed.", NULL);
		}
	}
}
#endif

//=======================================================================================
//	grMaterial_ArrayCreate
//=======================================================================================
GRAPI grMaterial_Array * GRCC grMaterial_ArrayCreate(int32 StartSize)
{
	grMaterial_Array	*MArray;

	MArray = GR_RAM_ALLOCATE_STRUCT(grMaterial_Array);

	if (!MArray)
		return NULL;

	ZeroMem(MArray);

	MArray->Array = grArray_Create(sizeof(grMaterial), StartSize, 5);

	if (!MArray->Array)
	{
		grRam_Free(MArray);
		return NULL;
	}

	MArray->RefCount = 1;

	return MArray;
}

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
		((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define GR_MARRAY_TAG			MAKEFOURCC('G', 'E', 'M', 'A')		// 'GE' 'M'aterial 'A'rray
#define GR_MARRAY_VERSION		0x0001

//========================================================================================
//	ReadMaterial
//========================================================================================
static grBoolean ReadMaterial(grVFile *File, void *Element,void *Context)
{
	grMaterial	*Material;
	grMaterial_Context* WorldContext = (grMaterial_Context*) Context;
	uint16 Version = (uint16) WorldContext->Version;

	assert(Element);
	//assert(!Context);

	Material = (grMaterial*)Element;

	// Krouer: modify the Material read procedure
	// Read the material
	//if (!grVFile_Read(File, Material, sizeof(grMaterial)))
	//	return GR_FALSE;

	if (!grVFile_Read(File, Material->MatName, GR_MATERIAL_MAX_NAME_SIZE))
		return GR_FALSE;

	if (!grVFile_Read(File, Material->BitmapName, GR_MATERIAL_MAX_NAME_SIZE))
		return GR_FALSE;
	
	if (Version==0) {
#ifndef _USE_BITMAPS
		// Open the file
		if (!grVFile_Read(File, &Material->MatSpec, sizeof(grBitmap*)))
			return GR_FALSE;
#else
		// Open the file
		if (!grVFile_Read(File, &Material->Bitmap, sizeof(grBitmap*)))
			return GR_FALSE;
	} else {
		Material->Bitmap = NULL;
#endif
	}

	if (!grVFile_Read(File, &Material->DrawScaleU, sizeof(float)))
		return GR_FALSE;

	if (!grVFile_Read(File, &Material->DrawScaleV, sizeof(float)))
		return GR_FALSE;

	if (!grVFile_Read(File, &Material->Reflectivity, sizeof(grRGBA)))
		return GR_FALSE;

	if (Version==0) {
		if (!grVFile_Read(File, &Material->RefCount, sizeof(int32)))
			return GR_FALSE;
	}

	if (Version>0) {
		if (!grVFile_Read(File, &Material->ResourceKind, sizeof(int8)))
			return GR_FALSE;
	} else {
		Material->ResourceKind = GR_RESOURCE_BITMAP;
	}
	Log_Printf("ReadMaterial %s: Kind is %d\n", Material->MatName, Material->ResourceKind); 

	// Icestorm: Only to be on the save side!
	Material->RefCount = 1;

	if (Version == 0) {
#ifdef _USE_BITMAPS
		// Read the bitmap in to the material
		Material->Bitmap = grBitmap_CreateFromFile(File);
		grResource_ExportResource(WorldContext->ResMgr, Material->ResourceKind, Material->BitmapName, Material->Bitmap);
#else
		grBitmap* pBitmap;
		pBitmap = grBitmap_CreateFromFile(File); 
		grResource_ExportResource(WorldContext->ResMgr, Material->ResourceKind, Material->BitmapName, pBitmap);
		Material->MatSpec = grMaterialSpec_Create(WorldContext->Engine, WorldContext->ResMgr);
		grMaterialSpec_AddLayer(Material->MatSpec, 0, Material->ResourceKind, GR_MATERIAL_LAYER_BASE, 0, Material->BitmapName);
#endif
	} else {
		// Load texture from directory / pak file
		if (Material->ResourceKind == GR_RESOURCE_BITMAP) {
			Material->MatSpec = grMaterialSpec_Create(WorldContext->Engine, WorldContext->ResMgr);
			grMaterialSpec_AddLayer(Material->MatSpec, 0, Material->ResourceKind, GR_MATERIAL_LAYER_BASE, 0, Material->BitmapName);
		} else {
            Material->MatSpec = (grMaterialSpec*) grResource_GetResource(WorldContext->ResMgr, Material->ResourceKind, Material->BitmapName);
		}
	}

#ifndef _USE_BITMAPS
	// A material override (G3D_MATERIAL_OVERRIDES) replaces a bitmap material too, e.g. with
	// a PBR version; its base layer still finds the level's bitmap by name.
	if (Material->ResourceKind != GR_RESOURCE_MATERIAL && grResource_HasMaterialOverride(Material->BitmapName))
	{
		grMaterialSpec *Override = (grMaterialSpec*) grResource_GetResource(WorldContext->ResMgr, GR_RESOURCE_MATERIAL, Material->BitmapName);

		if (Override)
		{
			if (Material->MatSpec)
				grMaterialSpec_Destroy(&Material->MatSpec);
			Material->MatSpec = Override;
		}
	}
#endif

#ifdef _USE_BITMAPS
	if (!Material->Bitmap)
#else
	if (!Material->MatSpec)
#endif
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grMaterial_ArrayCreateFromFile
//=======================================================================================
GRAPI grMaterial_Array * GRCC grMaterial_ArrayCreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
{
	uint32				Tag;
	uint16				Version;
	uint16				lVersionOffsetSize;
	grMaterial_Context	MatCtx;
	grMaterial_Array	*Array = NULL;

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Array))
			return NULL;

		if (Array)
		{				
			if (!grMaterial_ArrayCreateRef(Array))
				return NULL;

			return Array;
		}
	}

	// Read header info
	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return NULL;

	if (Tag != GR_MARRAY_TAG)
		return NULL;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return NULL;

	if (Version > GR_MARRAY_VERSION)
		return NULL;

	// Read the array info
	Array = GR_RAM_ALLOCATE_STRUCT(grMaterial_Array);

	if (!Array)
		return NULL;

	ZeroMem(Array);

	MatCtx.Version = Version;
	MatCtx.ResMgr  = grPtrMgr_GetResourceMgr(PtrMgr);
	MatCtx.Engine  = grResourceMgr_GetEngine(MatCtx.ResMgr);
	lVersionOffsetSize = 0;
	if (Version == 0) {
		// Setup the Version 1 structure diff size
		lVersionOffsetSize = sizeof(int8);
	}

	Array->Array = grArray_CreateFromFile(VFile, lVersionOffsetSize, ReadMaterial, &MatCtx);
	
	if (!Array->Array)
	{
		grErrorLog_AddString(-1, "grMaterial_ArrayCreateFromFile : grArray_CreateFromFile failed.", NULL);
		grRam_Free(Array);
		return NULL;
	}

	Array->RefCount = 1;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Array))
			goto ExitWithError;
	}

	return Array;

	ExitWithError:
	{
		if (Array)
		{
			if (Array->Array)
				grArray_Destroy(&Array->Array);
		}

		grRam_Free(Array);

		return NULL;
	}
}

//========================================================================================
//	WriteMaterial
//========================================================================================
static grBoolean WriteMaterial(grVFile *File, void *Element,void *Context)
{
	grMaterial	*Material;
	grPtrMgr* WorldContext = (grPtrMgr*) Context;
	uint16 Version = (uint16) WorldContext->lData;

	assert(Element);

	Material = (grMaterial*)Element;

	// Krouer : modify the write material procedure
	// Write out the material
	//if (!grVFile_Write(File, Material, sizeof(grMaterial)))
	//	return GR_FALSE;

	if (!grVFile_Write(File, Material->MatName, GR_MATERIAL_MAX_NAME_SIZE))
		return GR_FALSE;

	if (!grVFile_Write(File, Material->BitmapName, GR_MATERIAL_MAX_NAME_SIZE))
		return GR_FALSE;
	
	if (Version==0) {
#ifdef _USE_BITMAPS
		if (!grVFile_Write(File, &Material->Bitmap, sizeof(grBitmap*)))
			return GR_FALSE;
#else
		uint32 val;
		val = 0;
		if (!grVFile_Write(File, &val, sizeof(uint32)))
			return GR_FALSE;
#endif
	}

	if (!grVFile_Write(File, &Material->DrawScaleU, sizeof(float)))
		return GR_FALSE;

	if (!grVFile_Write(File, &Material->DrawScaleV, sizeof(float)))
		return GR_FALSE;

	if (!grVFile_Write(File, &Material->Reflectivity, sizeof(grRGBA)))
		return GR_FALSE;

	if (Version==0) {
		if (!grVFile_Write(File, &Material->RefCount, sizeof(int32)))
			return GR_FALSE;
	}

	if (Version>0) {
		Log_Printf("WriteMaterial %s: Kind is %d\n", Material->MatName, Material->ResourceKind); 
		if (!grVFile_Write(File, &Material->ResourceKind, sizeof(int8)))
			return GR_FALSE;
	}

	if (Version==0) {
#ifdef _USE_BITMAPS
		// Write out the bitmap to the material
		if (!grBitmap_WriteToFile(Material->Bitmap, File))
			return GR_FALSE;
#endif
	}

	return GR_TRUE;
}

//=======================================================================================
//	grMaterial_ArrayWriteToFile
//=======================================================================================
GRAPI grBoolean GRCC grMaterial_ArrayWriteToFile(grMaterial_Array *MatArray, grVFile *VFile, grPtrMgr *PtrMgr)
{
	uint32				Tag;
	uint16				Version;

	if (PtrMgr)	
	{
		uint32		Count;
			
		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)MatArray, &Count))
			return GR_FALSE;

		if (Count)		// Ptr was on stack, so return 
			return GR_TRUE;
	}

	// Write TAG
	Tag = GR_MARRAY_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_MARRAY_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	PtrMgr->lData = Version;

	if (!grArray_WriteToFile(MatArray->Array, VFile, WriteMaterial, PtrMgr))
		return GR_FALSE;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)MatArray))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grMaterialArray_CreateRef
//=======================================================================================
GRAPI grBoolean GRCC grMaterial_ArrayCreateRef(grMaterial_Array *MatArray)
{
	assert(MatArray);
	assert(MatArray->RefCount >= 0);

	MatArray->RefCount++;

	return GR_TRUE;
}

//=======================================================================================
//	grMaterial_ArrayDestroy
//=======================================================================================
GRAPI void GRCC grMaterial_ArrayDestroy(grMaterial_Array **Array)
{
	assert(Array);
	assert(*Array);
	assert((*Array)->RefCount > 0);
	assert((*Array)->Array);

	(*Array)->RefCount--;

	if ((*Array)->RefCount > 0)
		return;

	// Icestorm: Don't forget those poor Bitmaps ;)
	{
		grMaterial	*Material = grMaterial_ArrayGetNextMaterial(*Array, NULL);

		while (Material)
		{
#ifdef _USE_BITMAPS
			if (Material->Bitmap)
			{
				if ((*Array)->Engine)
					if (!grEngine_RemoveBitmap((*Array)->Engine, Material->Bitmap))
						grErrorLog_AddString(-1, "grMaterial_ArrayDestroy:  grEngine_RemoveBitmap failed.", NULL);

				grBitmap_Destroy(&Material->Bitmap);
			}
#else
			if (Material->MatSpec)
			{
				grBitmap* pBitmap = grMaterialSpec_GetLayerBitmap(Material->MatSpec, 0);
				if (pBitmap && (*Array)->Engine) {
					if (!grEngine_RemoveBitmap((*Array)->Engine, pBitmap)) {
						grErrorLog_AddString(-1, "grMaterial_ArrayDestroy:  grEngine_RemoveBitmap failed.", NULL);
					}
				}
				AttachPBRLayers((*Array)->Engine, Material->MatSpec, GR_FALSE);

				grMaterialSpec_Destroy(&Material->MatSpec);
			}
#endif

			Material = grMaterial_ArrayGetNextMaterial(*Array, Material);
		}
	}

	grArray_Destroy(&(*Array)->Array);

	if ((*Array)->Engine)
		grEngine_Destroy(&(*Array)->Engine);	// Icestorm

	grRam_Free(*Array);

	*Array = NULL;
}

//=======================================================================================
//	grMaterial_ArrayCreateMaterial
//=======================================================================================
GRAPI grMaterial_ArrayIndex GRCC grMaterial_ArrayCreateMaterial(grMaterial_Array *MatArray, const char *MatName)
{
	grMaterial	*Material;

	assert(strlen(MatName) < GR_MATERIAL_MAX_NAME_SIZE);

	Material = (grMaterial*)grArray_GetNewElement(MatArray->Array);

	if (!Material)
		return GR_MATERIAL_ARRAY_NULL_INDEX;

	grMaterial_SetDefaults(Material);

	strcpy(Material->MatName, MatName);

	return grArray_GetElementIndex(Material);
}

//=======================================================================================
//	grMaterial_ArrayDestroyMaterial
//=======================================================================================
GRAPI void GRCC grMaterial_ArrayDestroyMaterial(grMaterial_Array *MatArray, grMaterial_ArrayIndex *Index)
{
	grMaterial	*Material;
	grBoolean	Ret;

	Material = (grMaterial*)grArray_GetElement(MatArray->Array, *Index);

	assert(Material);

#ifdef _USE_BITMAPS
	// Icestorm : Don't forget it's bitmap!
	if (Material->Bitmap)
	{
		if (MatArray->Engine)
			if (!grEngine_RemoveBitmap(MatArray->Engine, Material->Bitmap))
				grErrorLog_AddString(-1, "grMaterial_ArrayDestroyMaterial:  grEngine_RemoveBitmap failed.", NULL);
		grBitmap_Destroy(&Material->Bitmap);
	}
#else
	if (Material->MatSpec)
	{
		if (MatArray->Engine) {
			grBitmap* pBitmap;
			pBitmap = grMaterialSpec_GetLayerBitmap(Material->MatSpec, 0);
			if (!grEngine_RemoveBitmap(MatArray->Engine, pBitmap))
				grErrorLog_AddString(-1, "grMaterial_ArrayDestroyMaterial:  grEngine_RemoveBitmap failed.", NULL);
			AttachPBRLayers(MatArray->Engine, Material->MatSpec, GR_FALSE);
		}
		grMaterialSpec_Destroy(&Material->MatSpec);
	}
#endif

	Ret = grArray_FreeElement(MatArray->Array, (void*)Material);
	assert(Ret == GR_TRUE);

	*Index = GR_MATERIAL_ARRAY_NULL_INDEX;
}

//=======================================================================================
//	grMaterial_ArrayGetMaterialByIndex
//=======================================================================================
GRAPI const grMaterial * GRCC grMaterial_ArrayGetMaterialByIndex(const grMaterial_Array *Array, grMaterial_ArrayIndex Index)
{
	return (grMaterial*)grArray_GetElement(Array->Array, Index);
}

//=======================================================================================
//	grMaterial_ArrayGetMaterialIndex
//=======================================================================================
GRAPI grMaterial_ArrayIndex GRCC grMaterial_ArrayGetMaterialIndex(const grMaterial_Array *Array, const grMaterial *Material)
{
	assert(Array);

	return grArray_GetElementIndex((void*)Material);
}

//=======================================================================================
//	grMaterial_ArraySetMaterialBitmap
//=======================================================================================
GRAPI grBoolean GRCC grMaterial_ArraySetMaterialBitmap(grMaterial_Array *Array, grMaterial_ArrayIndex Index, grBitmap *Bitmap, const char *BitmapName)
{
	grMaterial	*Material;

	Material = (grMaterial*)grArray_GetElement(Array->Array, Index);

	if (!Material)
		return GR_FALSE;

#ifdef _USE_BITMAPS
	// Remove any old bitmaps in this material
	if (Array->Engine && Material->Bitmap)
	{
		if (!grEngine_RemoveBitmap(Array->Engine, Material->Bitmap))
		{
			grErrorLog_AddString(-1, "grMaterial_ArraySetMaterialBitmap:  grEngine_RemoveBitmap failed.", NULL);
			return GR_FALSE;
		}
	}

	if (!grMaterial_SetBitmap(Material, Bitmap, BitmapName))
	{
		grErrorLog_AddString(-1, "grMaterial_ArraySetMaterialBitmap:  grMaterial_SetBitmap failed.", NULL);
		return GR_FALSE;
	}

	if (Array->Engine)
	{
		if (!grEngine_AddBitmap(Array->Engine, Material->Bitmap, GR_ENGINE_BITMAP_TYPE_3D))
		{
			grErrorLog_AddString(-1, "grMaterial_ArraySetMaterialBitmap:  grEngine_RemoveBitmap failed.", NULL);
			return GR_FALSE;
		}
	}

	//Material->BitmapKind = GR_RESOURCE_BITMAP;

	return GR_TRUE;
#else
	return GR_FALSE;
#endif
}

GRAPI grBoolean GRCC grMaterial_ArraySetMaterialSpec(grMaterial_Array *Array, grMaterial_ArrayIndex Index, grMaterialSpec *MatSpec, const char *BitmapName)
{
	grBitmap*	pBitmap;
	grMaterial* pMaterial;

	pMaterial = (grMaterial*)grArray_GetElement(Array->Array, Index);

	if (!pMaterial)
		return GR_FALSE;

#ifndef _USE_BITMAPS
	if (pMaterial->MatSpec) {
		// remove the bitmap if any, if texture don't do anything here
		pBitmap = grMaterialSpec_GetLayerBitmap(pMaterial->MatSpec, 0);
		if (pBitmap && Array->Engine) {
			if (!grEngine_RemoveBitmap(Array->Engine, pBitmap))
			{
				grErrorLog_AddString(-1, "grMaterial_ArraySetMaterialBitmap:  grEngine_RemoveBitmap failed.", NULL);
				return GR_FALSE;
			}
		}
		AttachPBRLayers(Array->Engine, pMaterial->MatSpec, GR_FALSE);
	}

	grMaterialSpec_CreateRef(MatSpec);
	pMaterial->MatSpec = MatSpec;

    pMaterial->ResourceKind = GR_RESOURCE_MATERIAL;
    strcpy(pMaterial->BitmapName, BitmapName);

	// now add the new bitmap if any, nothing to do if the spec contains grTexture
	AttachPBRLayers(Array->Engine, pMaterial->MatSpec, GR_TRUE);
	pBitmap = grMaterialSpec_GetLayerBitmap(pMaterial->MatSpec, 0);
	if (pBitmap && Array->Engine)
	{
		if (!grEngine_AddBitmap(Array->Engine, pBitmap, GR_ENGINE_BITMAP_TYPE_3D))
		{
			grErrorLog_AddString(-1, "grMaterial_ArraySetMaterialBitmap:  grEngine_RemoveBitmap failed.", NULL);
			return GR_FALSE;
		}
	}
	
	return GR_TRUE;
#else
	return GR_FALSE;
#endif
}

//=======================================================================================
//	grMaterial_ArrayGetNextMaterial
//=======================================================================================
GRAPI grMaterial * GRCC grMaterial_ArrayGetNextMaterial(grMaterial_Array *Array, const grMaterial *Start)
{
	return (grMaterial*)grArray_GetNextElement(Array->Array, (void*)Start);
}

//=======================================================================================
//	grMaterial_ArraySetEngine
//=======================================================================================
grBoolean grMaterial_ArraySetEngine(grMaterial_Array *Array, grEngine *Engine)
{
	grMaterial		*Material;

	Material = NULL;
	while (Material = grMaterial_ArrayGetNextMaterial(Array, Material))
	{
#ifdef _USE_BITMAPS
		if (!Material->Bitmap)
			continue;

		// If there was a previous engine, remove the bitmap from that one, and attach it to the new one
		if (Array->Engine)
		{
			if (!grEngine_RemoveBitmap(Array->Engine, Material->Bitmap))
			{
				grErrorLog_AddString(-1, "grMaterial_ArraySetEngine:  grEngine_RemoveBitmap failed.", NULL);
				return GR_FALSE;
			}
		}

		if (!grEngine_AddBitmap(Engine, Material->Bitmap, GR_ENGINE_BITMAP_TYPE_3D))
		{
			grErrorLog_AddString(-1, "grMaterial_ArraySetEngine:  grEngine_RemoveBitmap failed.", NULL);
			return GR_FALSE;
		}
#else
#pragma message("Krouer: Must take into account the specification of the new texture mgr")
		grBitmap* pBitmap;

		if (!Material->MatSpec) {
			continue;
		}

		pBitmap = grMaterialSpec_GetLayerBitmap(Material->MatSpec, 0);
		// nothing to do if spec uses grTexture
		if (pBitmap) {
			// If there was a previous engine, remove the bitmap from that one, and attach it to the new one
			if (Array->Engine)
			{
				if (!grEngine_RemoveBitmap(Array->Engine, pBitmap))
				{
					grErrorLog_AddString(-1, "grMaterial_ArraySetEngine:  grEngine_RemoveBitmap failed.", NULL);
					return GR_FALSE;
				}
			}

			if (!grEngine_AddBitmap(Engine, pBitmap, GR_ENGINE_BITMAP_TYPE_3D))
			{
				grErrorLog_AddString(-1, "grMaterial_ArraySetEngine:  grEngine_RemoveBitmap failed.", NULL);
				return GR_FALSE;
			}
		}
		AttachPBRLayers(Array->Engine, Material->MatSpec, GR_FALSE);
		AttachPBRLayers(Engine, Material->MatSpec, GR_TRUE);
#endif
	}

	if (Array->Engine)
		grEngine_Destroy(&Array->Engine);	// Icestorm: We want to be sure Engine will be valid all the time!

	Array->Engine = Engine;
	grEngine_CreateRef(Engine);	// Icestorm

	return GR_TRUE;
}

/*
#if 1

#define GR_MATERIAL_LAYER_FLAG_BITMAP		(1<<0)
#define GR_MATERIAL_LAYER_FLAG_LIGHTMAP		(1<<1)

typedef struct grMaterial_Layer
{
	uint8				Flags;
	grBitmap			*Bitmap;

	// Local ScaleU/ScaleV to this layer only
	grFloat				ShiftU;
	grFloat				ShiftV;
	grFloat				ScaleU;
	grFloat				ScaleV;
} grMaterial_Layer;

#define GR_MATERIAL_FLAG_VIS_PORTAL			(1<<0)

typedef struct grMaterial
{
	uint16				RefCount;
	grMaterial			*Parent;

	uint32				Flags;

	uint32				AmbientColor;	// RGBA (8 bits for each component, 0..255)

	// Scale and rotation of all layers
	grFloat				BaseShiftU;
	grFloat				BaseShiftV;
	grFloat				BaseScaleU;
	grFloat				BaseScaleV;
	grFloat				BaseRotate;

	uint8				NumLayers;		// Maximum of 255 layers 
	grMaterial_Layer	*Layers;		// Array of allocated layers
} grMaterial;

#else

#define GR_MATERIAL_HAS_BASESCALEUVR	(1<<0)			// Material has Scale UVR in material

typedef struct grMaterial
{
	grMaterial_Def			*Def;

	uint16					RefCount;

	uint32					Flags;
	void					*Data;

} grMaterial;

typedef struct 
{
	grFloat		ShiftU;
	grFloat		ShiftV;
} grMaterial_BaseShiftUV;

typedef struct 
{
	grFloat		ScaleU;
	grFloat		ScaleV;
} grMaterial_BaseScaleUV;

typedef struct 
{
	grFloat		Rotate;
} grMaterial_BaseRotate;

uint32 MaterialFieldSizes[] = {
	sizeof(grMaterial_BaseScaleUVR),
};

//=======================================================================================
//	grMaterial_GetBaseScaleUV
//=======================================================================================
void grMaterial_GetBaseScaleUV(grMaterial *Material, grFloat *ScaleU, grFloat *ScaleV)
{
	assert(Material);
	assert(Material->Def);
	assert(ScaleU);
	assert(ScaleV);

	Flags = Material->Flags;

	if (Flags & GR_MATERIAL_HAS_BASESCALEUVR)
	{
		uint8		*Data;
		int32		i;
		uint32		Mask;

		Data = Material->Data;

		// Add up all the sizes of the valid fields to get the offset of the curent field
		for (i=0, Mask = 0; Mask <= GR_MATERIAL_HAS_BASESCALEUVR; i++, Mask+=Mask)
		{
			if (Flags & Mask)
				Data += MaterialFieldSizes[i];
		}

		*ScaleU = (grMaterial_BaseScaleUVR*)Data)->ScaleU;
		*ScaleV = (grMaterial_BaseScaleUVR*)Data)->ScaleV;
	}
	else
	{
		Def = Material->Def;

		*ScaleU = Def->BaseScaleU;
		*ScaleV = Def->BaseScaleV;
	}
}
#endif

//=======================================================================================
//	grMaterial_GetLayer
//=======================================================================================
grMaterial_Layer *grMaterial_GetLayer(grMaterial *Material, uint8 Layer)
{
	assert(Material);

}
*/

