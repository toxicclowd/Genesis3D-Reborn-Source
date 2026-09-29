/****************************************************************************************/
/*  JEFACEINFO.C                                                                        */
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
#include <stdio.h>
#include <memory.h>
#include <math.h>

// Public dependents
#include "grFaceInfo.h"

// Private dependents
#include "Ram.h"
#include "Errorlog.h"

//=======================================================================================
//	grFaceInfo_SetDefaults
//=======================================================================================
GRAPI void GRCC grFaceInfo_SetDefaults(grFaceInfo *FaceInfo)
{
	assert(FaceInfo);

	memset(FaceInfo, 0, sizeof(*FaceInfo));

	FaceInfo->Alpha = 100.0f;

	FaceInfo->DrawScaleU = 1.0f;
	FaceInfo->DrawScaleV = 1.0f;

	FaceInfo->LMapScaleU = 1.0f;
	FaceInfo->LMapScaleV = 1.0f;

	FaceInfo->MaterialIndex = GR_MATERIAL_ARRAY_NULL_INDEX;
}

#define FLOAT_COMPARE(a, b) (fabs((b)-(a)) < 0.01f)
//=======================================================================================
//	grFaceInfo_Compare
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_Compare(const grFaceInfo *Face1, const grFaceInfo *Face2)
{
	// FIXME:  Try to test the ones that are going to most likely fail first...
	if (Face1->Flags != Face2->Flags)
		return GR_FALSE;
	if (Face1->Alpha != Face2->Alpha)
		return GR_FALSE;

	if (!FLOAT_COMPARE(Face1->ShiftU, Face2->ShiftU))
		return GR_FALSE;

	if (!FLOAT_COMPARE(Face1->ShiftV, Face2->ShiftV))
		return GR_FALSE;

	if (!FLOAT_COMPARE(Face1->Rotate, Face2->Rotate))
		return GR_FALSE;

	if (Face1->DrawScaleU != Face2->DrawScaleU)
		return GR_FALSE;
	if (Face1->DrawScaleV != Face2->DrawScaleV)
		return GR_FALSE;
	if (Face1->LMapScaleU != Face2->LMapScaleU)
		return GR_FALSE;
	if (Face1->LMapScaleV != Face2->LMapScaleV)
		return GR_FALSE;
	if (Face1->MaterialIndex != Face2->MaterialIndex)
		return GR_FALSE;

	if (Face1->PortalCamera != Face2->PortalCamera)
		return GR_FALSE;

	return GR_TRUE;
}

//====================================================================================
//	grFaceInfo_NeedsLightmap
//====================================================================================
GRAPI grBoolean GRCC grFaceInfo_NeedsLightmap(const grFaceInfo *pFaceInfo)
{
	if (pFaceInfo->Flags & FACEINFO_NO_LIGHTMAP)
		return GR_FALSE;

	//if (pFaceInfo->PortalCamera)
	//	return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grFaceInfo_ArrayCreate
//=======================================================================================
GRAPI grFaceInfo_Array * GRCC grFaceInfo_ArrayCreate(int32 StartFaces)
{
	return (grFaceInfo_Array*)grGArray_Create(StartFaces, sizeof(grFaceInfo));
}

//=======================================================================================
//	grFaceInfo_ArrayCreateFromFile
//=======================================================================================
GRAPI grFaceInfo_Array * GRCC grFaceInfo_ArrayCreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr)
{
	return (grFaceInfo_Array*)grGArray_CreateFromFile(VFile, IOFunc, IOFuncContext, PtrMgr);
}

//=======================================================================================
//	grFaceInfo_ArrayWriteToFile
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_ArrayWriteToFile(const grFaceInfo_Array *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr)
{
	return grGArray_WriteToFile((grGArray*)Array, VFile, IOFunc, IOFuncContext, PtrMgr);
}

//=======================================================================================
//	grFaceInfo_ArrayCreateRef
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_ArrayCreateRef(grFaceInfo_Array *Array)
{
	return grGArray_CreateRef((grGArray*)Array);
}

//=======================================================================================
//	grFaceInfo_ArrayDestroy
//=======================================================================================
GRAPI void GRCC grFaceInfo_ArrayDestroy(grFaceInfo_Array **Array)
{
	grGArray_Destroy((grGArray**)Array);
}

//=======================================================================================
//	grFaceInfo_ArrayIndexIsValid
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_ArrayIndexIsValid(grFaceInfo_ArrayIndex Index)
{
	if (Index == GR_GARRAY_NULL_INDEX)
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grFaceInfo_ArrayIsValid
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_ArrayIsValid(const grFaceInfo_Array* Array)
{
	return (grGArray_IsValid((grGArray*)Array));
}

//=======================================================================================
//	grFaceInfo_ArrayAddFaceInfo
//=======================================================================================
GRAPI grFaceInfo_ArrayIndex GRCC grFaceInfo_ArrayAddFaceInfo(grFaceInfo_Array *Array, const grFaceInfo *FaceInfo)
{
	grGArray_Index	Index;

	Index = grGArray_AddElement((grGArray*)Array, (grGArray_Element*)FaceInfo);

	return (grFaceInfo_ArrayIndex)Index;
}

//=======================================================================================
//	grFaceInfo_ArrayShareFaceInfo
//=======================================================================================
GRAPI grFaceInfo_ArrayIndex GRCC grFaceInfo_ArrayShareFaceInfo(grFaceInfo_Array *Array, const grFaceInfo *FaceInfo)
{
	grFaceInfo			*pFaceInfo;
	grGArray_RefType	*pRef;
	int32				i;

	pFaceInfo = (grFaceInfo*)grGArray_GetElements((grGArray*)Array);
	pRef = grGArray_GetRefCounts((grGArray*)Array);

	for (i=0; i< grGArray_GetSize((grGArray*)Array); i++, pRef++, *pFaceInfo++)
	{
		if (*pRef == 0 || *pRef >= GR_GARRAY_MAX_ELEMENT_REFCOUNT)
			continue;					// If it's empty, or totally full, we can't share with it...

		if (grFaceInfo_Compare(pFaceInfo, FaceInfo))
		{
			assert(*pRef < GR_GARRAY_MAX_ELEMENT_REFCOUNT);
			(*pRef)++;
			return (grFaceInfo_ArrayIndex)i;
		}
	}

	return grFaceInfo_ArrayAddFaceInfo(Array, FaceInfo);
}

//=======================================================================================
//	grFaceInfo_ArrayRefFaceInfoIndex
//=======================================================================================
GRAPI grBoolean GRCC grFaceInfo_ArrayRefFaceInfoIndex(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index)
{
	return grGArray_RefElement((grGArray*)Array, Index);
}

//=======================================================================================
//	grFaceInfo_ArrayRemoveFaceInfo
//=======================================================================================
GRAPI void GRCC grFaceInfo_ArrayRemoveFaceInfo(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex *Index)
{
	grGArray_RemoveElement((grGArray*)Array, (grGArray_Index*)Index);
}

//=======================================================================================
//	grFaceInfo_ArraySetFaceInfoByIndex
//=======================================================================================
GRAPI void GRCC grFaceInfo_ArraySetFaceInfoByIndex(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index, const grFaceInfo *FaceInfo)
{
	grGArray_SetElementByIndex((grGArray*)Array, (grGArray_Index)Index, (grGArray_Element*)FaceInfo);
}

//=======================================================================================
//	grFaceInfo_ArrayGetFaceInfoByIndex
//=======================================================================================
GRAPI const grFaceInfo * GRCC grFaceInfo_ArrayGetFaceInfoByIndex(const grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index)
{
	return (grFaceInfo*)grGArray_GetElementByIndex((grGArray*)Array, (grGArray_Index)Index);
}

