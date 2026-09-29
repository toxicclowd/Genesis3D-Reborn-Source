/****************************************************************************************/
/*  JEBRUSH.C                                                                           */
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
#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>
#include <assert.h>

// Private dependents
#include "Dcommon.h"
#include "Ram.h"
#include "Errorlog.h"
#include "grIndexPoly.h"
#include "grVertArray.h"

#include "Engine.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grFaceInfo.h"

#include "grPolyMgr.h"

#include "log.h"

// Public Dependents
#include "grBrush.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)
												   
static grBoolean grBrush_WriteHeader(const grBrush *Brush, grVFile *VFile);
static grBoolean grBrush_WriteFaces(const grBrush *Brush, grVFile *VFile);
static grBoolean grBrush_ReadHeader(grBrush *Brush, grVFile *VFile);
static grBoolean grBrush_ReadFaces(grBrush *Brush, grVFile *VFile);

static grBoolean DettachFaceInfoArray(grBrush *Brush);
static grBoolean AttachFaceInfoArray(grBrush *Brush, grFaceInfo_Array *Array);

static grBoolean grBrush_WriteArrays(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMgr);
static grBoolean grBrush_ReadArrays(grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMgr);

#define GR_BRUSH_HAS_FACEINFO_ARRAY	(1<<0)

//========================================================================================
//========================================================================================
// The grBrush
typedef struct grBrush
{
	int32					RefCount;
	grVertArray				*VertArray;

	// Face pool
	int32					NumFaces;		// Number of faces in the brush
	grBrush_Face			*Faces;			// Linked list of faces

	// Header data, and such
	grXForm3d				XForm;			// XForm to go form model to worldspace
	grXForm3d				WorldToLocked;	// XForm to go from worldspace to locked space
	grXForm3d				LockedToWorld;	// XForm to go from locked space to world space
	grBrush_Contents		Contents;

	// Arrays that this objects inherits from other objects
	grFaceInfo_Array		*FaceInfoArray;

	uint16					Flags;

	#ifdef _DEBUG
		struct grBrush		*Self;
	#endif

} grBrush;

// The grBrush_Face keeps track of faceinfo, poly (winding of verts, etc...)
typedef struct grBrush_Face
{
	grBrush					*Brush;				// Parent brush

	grIndexPoly				*Poly;
	grFaceInfo_ArrayIndex	FaceInfoIndex;		// Valid when there is a current FaceInfoArray on the brush
	grFaceInfo				*FaceInfo;			// Valid when there is NOT a current FaceInfoArray on the brush
	grPlane					Plane;

	struct grBrush_Face		*Next;
	struct grBrush_Face		*Prev;
} grBrush_Face;

//========================================================================================
//	grBrush_Create
//	Create a grBrush, with a default of n number of faces.
//	Faces may be added, or removed, by simply changing the size of the FaceArray
//========================================================================================
GRAPI grBrush* GRCC grBrush_Create(int32 EstimatedVerts)
{
	grBrush		*Brush;

	Brush = GR_RAM_ALLOCATE_STRUCT(grBrush);

	if (!Brush)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grBrush_Create:  Out of ram for brush.", NULL);
		return NULL;
	}

	// Clear out the brush
	ZeroMem(Brush);

#ifdef _DEBUG
	Brush->Self = Brush;		// For safety checking
#endif

	grXForm3d_SetIdentity(&Brush->XForm);
	grXForm3d_SetIdentity(&Brush->WorldToLocked);
	grXForm3d_SetIdentity(&Brush->LockedToWorld);

	// Create the brushes VArray
	Brush->VertArray = grVertArray_Create(EstimatedVerts);

	if (!Brush->VertArray)
		goto ExitWithError;	

	// Create the very first ref
	if (!grBrush_CreateRef(Brush))
		goto ExitWithError;

	return Brush;
	
	// Error, cleanup
	ExitWithError:
	{
		if (Brush)
		{
			if (Brush->VertArray)
				grVertArray_Destroy(&Brush->VertArray);

			grRam_Free(Brush);
		}
		return NULL;
	}
}

//========================================================================================
//	grBrush_CreateFromFile
//========================================================================================
GRAPI grBrush * GRCC grBrush_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
{
	grBrush		*Brush;

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Brush))
			return NULL;

		if (Brush)
		{
			if (!grBrush_CreateRef(Brush))
				return NULL;

			return Brush;		// Ptr found in stack, return it
		}
	}

	Brush = GR_RAM_ALLOCATE_STRUCT(grBrush);

	if (!Brush)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grBrush_CreateFromFile:  Out of ram for brush.", NULL);
		return NULL;
	}

	// Clear out the brush
	ZeroMem(Brush);

#ifdef _DEBUG
	Brush->Self = Brush;		// For safety checking
#endif

	// Create the very first ref
	if (!grBrush_CreateRef(Brush))
		goto ExitWithError;	

	if (!grBrush_ReadHeader(Brush, VFile))
		goto ExitWithError;

	if (!grBrush_ReadArrays(Brush, VFile, PtrMgr))
		goto ExitWithError;
		
	if (!grBrush_ReadFaces(Brush, VFile))
		goto ExitWithError;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Brush))
			goto ExitWithError;
	}

	return Brush;
	
	// Error, cleanup
	ExitWithError:
	{
		if (Brush)
		{
			if (Brush->VertArray)
				grVertArray_Destroy(&Brush->VertArray);

			grRam_Free(Brush);
		}
		return NULL;
	}
}


//========================================================================================
//	grBrush_WriteToFile
//========================================================================================
GRAPI grBoolean GRCC grBrush_WriteToFile(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMgr)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);
	assert(VFile);

	if (PtrMgr)
	{
		uint32		Count;

		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)Brush, &Count))
			return GR_FALSE;

		if (Count)
			return GR_TRUE;		// Ptr was on stack, so return
	}

	// Write the header
	if (!grBrush_WriteHeader(Brush, VFile))
	{
		grErrorLog_AddString(-1, "grBrush_WriteToFile:  grBrush_WriteHeader failed.\n", NULL);
		return GR_FALSE;
	}

	// Save the arrays out
	if (!grBrush_WriteArrays(Brush, VFile, PtrMgr))
	{
		grErrorLog_AddString(-1, "grBrush_WriteToFile:  grBrush_WriteArrays failed.\n", NULL);
		return GR_FALSE;
	}

	// Save all the faces
	if (!grBrush_WriteFaces(Brush, VFile))
	{
		grErrorLog_AddString(-1, "grBrush_WriteToFile:  grBrush_WriteFaces failed.\n", NULL);
		return GR_FALSE;
	}

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)Brush))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grBrush_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grBrush_CreateRef(grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);
	assert(Brush->RefCount >= 0);

	Brush->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grBrush_Destroy
//========================================================================================
GRAPI void GRCC grBrush_Destroy(grBrush **Brush)
{
	grBrush_Face	*Face, *Next;

	assert(Brush);
	assert(*Brush);
	assert((*Brush)->RefCount > 0);
	assert((*Brush)->VertArray);

	(*Brush)->RefCount--;

	if ((*Brush)->RefCount == 0)
	{
		grBoolean		Ret;

		for (Face = (*Brush)->Faces; Face; Face = Next)
		{
			Next = Face->Next;
		
			grBrush_DestroyFace(*Brush, &Face);
		}

		assert((*Brush)->Faces == NULL);
	
		Ret = DettachFaceInfoArray(*Brush);
		assert(Ret == GR_TRUE);

		grVertArray_Destroy(&(*Brush)->VertArray);

		grRam_Free(*Brush);
	}

	*Brush = NULL;
}

//========================================================================================
//	grBrush_IsValid
//========================================================================================
GRAPI grBoolean GRCC grBrush_IsValid(const grBrush *Brush)
{
	grBrush_Face	*Face;
	int32			NumFaces;

	if (!Brush)
		return GR_FALSE;

#ifdef _DEBUG
	if (Brush->Self != Brush)
		return GR_FALSE;
#endif

	NumFaces = 0;
	for (Face = Brush->Faces; Face; Face = Face->Next)
	{
		assert(Face->Poly);
		NumFaces++;
	}

	if (Brush->NumFaces != NumFaces)
		return GR_FALSE;
	
	return GR_TRUE;
}

//=======================================================================================
//	grBrush_IsConvex
//=======================================================================================
GRAPI grBoolean GRCC grBrush_IsConvex(const grBrush *Brush)
{
	grBrush_Face		*Face1;
	grBrush_Face		*Face2;

	for (Face1 = Brush->Faces; Face1; Face1 = Face1->Next)
	{
		if (!grIndexPoly_IsConvex(Face1->Poly, &Face1->Plane.Normal, Face1->Brush->VertArray))
			return GR_FALSE;

		for (Face2 = Brush->Faces; Face2; Face2 = Face2->Next)
		{
			int32			i;

			// Notice how we check Face1 against itself...  This is because we want to make sure
			// that the triangles within the faces themselves are also convex...

			for (i=0; i< Face2->Poly->NumVerts; i++)
			{
				const grVec3d	*pVert;
				grFloat			Val;

				pVert = grVertArray_GetVertByIndex(Brush->VertArray, Face2->Poly->Verts[i]);

				Val = grVec3d_DotProduct(&Face1->Plane.Normal,  pVert);

				if (Val < 0)
					return GR_FALSE;
			}
		}
	}

	return GR_TRUE;			// Brush is convex
}

//========================================================================================
//	grBrush_SetContents
//========================================================================================
GRAPI void GRCC grBrush_SetContents(grBrush *Brush, grBrush_Contents Contents)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	Brush->Contents = Contents;
}

//========================================================================================
//	grBrush_GetContents
//========================================================================================
GRAPI grBrush_Contents GRCC grBrush_GetContents(const grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return Brush->Contents;
}

//========================================================================================
//	grBrush_SetXForm
//========================================================================================
GRAPI void GRCC grBrush_SetXForm(grBrush *Brush, const grXForm3d *XForm, grBoolean Locked)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	if (Locked)
	{
	#if 1	
		{
		grXForm3d		Inverse;
		grXForm3d		xCopy = *XForm;

		xCopy.Flags = 0;
		grXForm3d_Orthonormalize(&xCopy);

		// Get the Inverse of the input orthonormalise XForm
		grXForm3d_GetInverse(&xCopy, &Inverse);
		
		// XForm the current xform by it
		Brush->XForm.Flags = 0;
		grXForm3d_Orthonormalize(&Brush->XForm);

		grXForm3d_Multiply(&Brush->XForm, &Inverse, &Inverse);

		// XForm the current locked xform by this resuling xform
		grXForm3d_Multiply(&Brush->WorldToLocked, &Inverse, &Brush->WorldToLocked);

		grXForm3d_GetInverse(&Brush->WorldToLocked, &Brush->LockedToWorld);
		}
	#else		// Test code, that does not save rotation or scaling
		{
		grVec3d			Diff, Save;
		Save = Brush->WorldToLocked.Translation;

		grXForm3d_SetIdentity(&Brush->WorldToLocked);
		grXForm3d_SetIdentity(&Brush->LockedToWorld);

		grVec3d_Subtract(&XForm->Translation, &Brush->XForm.Translation, &Diff);

		grVec3d_Subtract(&Save, &Diff, &Brush->WorldToLocked.Translation);

		Brush->LockedToWorld.Translation = Brush->WorldToLocked.Translation;
		grVec3d_Inverse(&Brush->LockedToWorld.Translation);
		}
	#endif

	}
	else
	{
		grXForm3d_SetIdentity(&Brush->WorldToLocked);
		grXForm3d_SetIdentity(&Brush->LockedToWorld);
	}
	
	Brush->XForm = *XForm;
}

//========================================================================================
//	grBrush_GetXForm
//========================================================================================
GRAPI const grXForm3d * GRCC grBrush_GetXForm(grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return &Brush->XForm;
}

//========================================================================================
//	grBrush_GetWorldToLockedXForm
//========================================================================================
GRAPI const grXForm3d * GRCC grBrush_GetWorldToLockedXForm(grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return &Brush->WorldToLocked;
}

//========================================================================================
//	grBrush_GetLockedToWorldXForm
//========================================================================================
GRAPI const grXForm3d * GRCC grBrush_GetLockedToWorldXForm(grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return &Brush->LockedToWorld;
}

//========================================================================================
//	grBrush_GetVertArray
//========================================================================================
GRAPI grVertArray * GRCC grBrush_GetVertArray(const grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return Brush->VertArray;
}

//========================================================================================
//	grBrush_SetFaceInfoArray
//========================================================================================
GRAPI grBoolean GRCC grBrush_SetFaceInfoArray(grBrush *Brush, grFaceInfo_Array *Array)
{
	assert(grFaceInfo_ArrayIsValid(Array));

	if (!DettachFaceInfoArray(Brush))
		return GR_FALSE;

	assert(grFaceInfo_ArrayIsValid(Array));

	if (!AttachFaceInfoArray(Brush, Array))
		return GR_FALSE;
	
	return GR_TRUE;
}

#define GR_BRUSH_LINK_FACE(b, f) {	\
			f->Next = b->Faces;		\
			if (b->Faces)			\
				b->Faces->Prev = f;	\
			b->Faces = f;			\
									\
			b->NumFaces++;			\
									\
			f->Brush = b;			\
			}

//========================================================================================
//	grBrush_CreateFace
//========================================================================================
GRAPI grBrush_Face * GRCC grBrush_CreateFace(grBrush *Brush, int32 NumVerts)
{
	grBrush_Face		*Face;

	assert(Brush);

	Face = GR_RAM_ALLOCATE_STRUCT(grBrush_Face);

	if (!Face)
		return NULL;

	ZeroMem(Face);

	// Create the poly for the face
	Face->Poly = grIndexPoly_Create((grVertArray_Index)NumVerts);

	if (!Face->Poly)
		goto ExitWithError;

	// Initialize the FaceInfoIndex to NULL
	Face->FaceInfoIndex = GR_FACEINFO_ARRAY_NULL_INDEX;

	// Create the FaceInfo.  We use this till we have an array to stuff it in
	Face->FaceInfo = GR_RAM_ALLOCATE_STRUCT(grFaceInfo);

	// Set the FaceInfo defaults
	grFaceInfo_SetDefaults(Face->FaceInfo);

	if (!Face->FaceInfo)
		goto ExitWithError;

	GR_BRUSH_LINK_FACE(Brush, Face);

	return Face;
	
	ExitWithError:
	{
		if (Face)
		{
			if (Face->Poly)
				grIndexPoly_Destroy(&Face->Poly);

			if (Face->FaceInfo)
				grRam_Free(Face->FaceInfo);

			grRam_Free(Face);
		}

		return NULL;
	}
}

//========================================================================================
//	grBrush_CreateFaceFromFile
//========================================================================================
GRAPI grBrush_Face * GRCC grBrush_CreateFaceFromFile(grBrush *Brush, grVFile *VFile)
{
	grBrush_Face		*Face;

	Face = GR_RAM_ALLOCATE_STRUCT(grBrush_Face);

	if (!Face)
		return NULL;

	ZeroMem(Face);

	if (Brush->FaceInfoArray)
	{
		// Read the FaceInfoIndex
		if (!grVFile_Read(VFile, &Face->FaceInfoIndex, sizeof(Face->FaceInfoIndex)))
			goto ExitWithError;

		// Ref our copy of the FaceInfoIndex
		if (!grFaceInfo_ArrayRefFaceInfoIndex(Brush->FaceInfoArray, Face->FaceInfoIndex))
			goto ExitWithError;
	}
	else
	{
		Face->FaceInfo = GR_RAM_ALLOCATE_STRUCT(grFaceInfo);

		if (!Face->FaceInfo)
			goto ExitWithError;

		// Read in the FaceInfo
		if (!grVFile_Read(VFile, Face->FaceInfo, sizeof(grFaceInfo)))
			goto ExitWithError;
	}
	
	// Create this face's IndexPoly
	Face->Poly = grIndexPoly_CreateFromFile(VFile);

	if (!Face->Poly)
		goto ExitWithError;

	GR_BRUSH_LINK_FACE(Brush, Face);

	return Face;

	ExitWithError:
	{
		if (Face)
		{
			if (Face->Poly)
				grIndexPoly_Destroy(&Face->Poly);

			if (Face->FaceInfo)
				grRam_Free(Face->FaceInfo);

			grRam_Free(Face);
		}

		return NULL;
	}
}

//========================================================================================
//	grBrush_WriteFaceToFile
//========================================================================================
GRAPI grBoolean GRCC grBrush_WriteFaceToFile(const grBrush *Brush, const grBrush_Face *Face, grVFile *VFile)
{
	if (Brush->FaceInfoArray)
	{
		assert(!Face->FaceInfo);

		// Write out the FaceInfoIndex
		if (!grVFile_Write(VFile, &Face->FaceInfoIndex, sizeof(Face->FaceInfoIndex)))
			return GR_FALSE;
	}
	else
	{
		assert(Face->FaceInfo);

		// Write out the FaceInfo
		if (!grVFile_Write(VFile, Face->FaceInfo, sizeof(grFaceInfo)))
			return GR_FALSE;
	}

	// Write out this face's poly
	if (!grIndexPoly_WriteToFile(Face->Poly, VFile))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grBrush_DestroyFace
//========================================================================================
GRAPI void GRCC grBrush_DestroyFace(grBrush *Brush, grBrush_Face **Face)
{
	grBrush_Face		*pFace;
	int32				v;

	assert(Face);
	assert(*Face);
	assert((*Face)->Brush == Brush);
	assert((*Face)->Poly);

	assert(Brush->NumFaces > 0);

	pFace = *Face;

	if (pFace->Next)
		pFace->Next->Prev = pFace->Prev;

	if (Brush->Faces == pFace)
	{
		assert(pFace->Prev == NULL);
		Brush->Faces = pFace->Next;
	}
	else
	{
		assert(pFace->Prev != NULL);
		pFace->Prev->Next = pFace->Next;
	}

	for (v=0; v< pFace->Poly->NumVerts; v++)
		if (pFace->Poly->Verts[v] != GR_VERTARRAY_NULL_INDEX)
			grVertArray_RemoveVert(Brush->VertArray, &pFace->Poly->Verts[v]);

	grIndexPoly_Destroy(&pFace->Poly);

	if (pFace->FaceInfo)
	{
		assert(pFace->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX);
		grRam_Free(pFace->FaceInfo);
	}
	else
	{
		assert(pFace->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX);
		assert(Brush->FaceInfoArray);

		grFaceInfo_ArrayRemoveFaceInfo(Brush->FaceInfoArray, &pFace->FaceInfoIndex);
	}

	grRam_Free(pFace);

	*Face = NULL;
			
	Brush->NumFaces--;
}

//========================================================================================
//	grBrush_GetFaceCount
//========================================================================================
GRAPI int32 GRCC grBrush_GetFaceCount(const grBrush *Brush)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	return Brush->NumFaces;
}

//========================================================================================
//	grBrush_GetNextFace
//========================================================================================
GRAPI grBrush_Face * GRCC grBrush_GetNextFace(const grBrush *Brush, const grBrush_Face *Start)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);

	if (!Start)
		return Brush->Faces;

	return Start->Next;
}

//========================================================================================
//	grBrush_GetPrevFace
//	Note that GetPrev face CANNOT take NULL as start.  Must be valid face...
//========================================================================================
GRAPI grBrush_Face * GRCC grBrush_GetPrevFace(const grBrush *Brush, const grBrush_Face *Start)
{
	assert(grBrush_IsValid(Brush) == GR_TRUE);
	assert(Start);

	return Start->Prev;
}

//========================================================================================
//	grBrush_GetFaceByIndex
//========================================================================================
GRAPI grBrush_Face * GRCC grBrush_GetFaceByIndex(const grBrush *Brush, int32 Index)
{
	grBrush_Face	*Face;

	assert(grBrush_IsValid(Brush) == GR_TRUE);
	assert(Index >= 0 && Index < Brush->NumFaces);

	// Jump to index in linked list
	for (Face = Brush->Faces; Face && Index > 0; Index--, Face=Face->Next);

	return Face;
}

//========================================================================================
//	grBrush_FaceGetBrush
//========================================================================================
GRAPI grBrush * GRCC grBrush_FaceGetBrush(const grBrush_Face *Face)
{
	assert(Face);
	assert(Face->Poly);

	return Face->Brush;
}

//========================================================================================
//	grBrush_FaceGetVertCount
//========================================================================================
GRAPI int32 GRCC grBrush_FaceGetVertCount(const grBrush_Face *Face)
{
	assert(Face);
	assert(Face->Poly);

	return Face->Poly->NumVerts;
}

//========================================================================================
//	grBrush_FaceSetVertByIndex
//========================================================================================
GRAPI void GRCC grBrush_FaceSetVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert)
{
	grVertArray_Index	*VIndex;

	assert(Face);
	assert(Index < GR_INDEXPOLY_MAX_VERTS);

	VIndex = &Face->Poly->Verts[Index];

	if (*VIndex != GR_VERTARRAY_NULL_INDEX)
		grVertArray_RemoveVert(Face->Brush->VertArray, VIndex);

	*VIndex = grVertArray_ShareVert(Face->Brush->VertArray, Vert);

	grBrush_FaceCalcPlane(Face);
}

//========================================================================================
//	grBrush_FaceMoveVertByIndex
//========================================================================================
GRAPI void GRCC grBrush_FaceMoveVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert)
{
	grVertArray_Index	VIndex;

	assert(Face);
	assert(Index < GR_INDEXPOLY_MAX_VERTS);

	VIndex = Face->Poly->Verts[Index];

	grVertArray_SetVertByIndex(Face->Brush->VertArray, VIndex, Vert);

	grBrush_FaceCalcPlane(Face);
}

//========================================================================================
//	grBrush_FaceGetVertByIndex
//========================================================================================
GRAPI const grVec3d * GRCC grBrush_FaceGetVertByIndex(const grBrush_Face *Face, int32 Index)
{
	assert(Face);
	assert(Index < GR_INDEXPOLY_MAX_VERTS);

	return grVertArray_GetVertByIndex(Face->Brush->VertArray, Face->Poly->Verts[Index]);
}

//========================================================================================
//	grBrush_FaceGetWorldSpaceVertByIndex
//========================================================================================
GRAPI grVec3d GRCC grBrush_FaceGetWorldSpaceVertByIndex(const grBrush_Face *Face, int32 Index)
{
	grVec3d		Vert;

	assert(Face);
	assert(Index < GR_INDEXPOLY_MAX_VERTS);

	Vert = *grVertArray_GetVertByIndex(Face->Brush->VertArray, Face->Poly->Verts[Index]);

	grXForm3d_Transform(&Face->Brush->XForm, &Vert, &Vert);

	return Vert;
}

//========================================================================================
//	grBrush_FaceCalcPlane
//========================================================================================
GRAPI void GRCC grBrush_FaceCalcPlane(grBrush_Face *Face)
{
	const grVec3d	*pVerts[3];
	int32			i;

	for (i=0; i<3; i++)
	{
		if (Face->Poly->Verts[i] == GR_VERTARRAY_NULL_INDEX)
			break;

		pVerts[i] = grVertArray_GetVertByIndex(Face->Brush->VertArray, Face->Poly->Verts[i]);
		assert(pVerts[i]);
	}

	if (i != 3)
	{
		memset(&Face->Plane, 0, sizeof(grPlane));
		return;
	}
	
	grPlane_SetFromVerts(&Face->Plane, pVerts[0], pVerts[1], pVerts[2]);
}

//========================================================================================
//	grBrush_FaceGetFaceInfoIndex
//========================================================================================
GRAPI grFaceInfo_ArrayIndex GRCC grBrush_FaceGetFaceInfoIndex(grBrush_Face *Face)
{
	return Face->FaceInfoIndex;
}

//========================================================================================
//	grBrush_FaceGetFaceInfo
//========================================================================================
GRAPI grBoolean GRCC grBrush_FaceGetFaceInfo(grBrush_Face *Face, grFaceInfo *FaceInfo)
{
	if (Face->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
	{
		assert(Face->FaceInfo);
		assert(!Face->Brush->FaceInfoArray);

		*FaceInfo = *Face->FaceInfo;
	}
	else
	{
		const grFaceInfo *pFaceInfo;

		assert(!Face->FaceInfo);
		assert(Face->Brush->FaceInfoArray);

		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(Face->Brush->FaceInfoArray, Face->FaceInfoIndex);

		if (!pFaceInfo)
			return GR_FALSE;

		*FaceInfo = *pFaceInfo;
	}

	return GR_TRUE;
}

//========================================================================================
//	grBrush_FaceSetFaceInfo
//========================================================================================
GRAPI grBoolean GRCC grBrush_FaceSetFaceInfo(grBrush_Face *Face, const grFaceInfo *FaceInfo)
{
	if (Face->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
	{
		assert(Face->FaceInfo);
		assert(!Face->Brush->FaceInfoArray);

		*Face->FaceInfo = *FaceInfo;
	}
	else
	{
		assert(!Face->FaceInfo);
		assert(Face->Brush->FaceInfoArray);

		// Remove any old faceinfo 
		grFaceInfo_ArrayRemoveFaceInfo(Face->Brush->FaceInfoArray, &Face->FaceInfoIndex);
		
		Face->FaceInfoIndex = grFaceInfo_ArrayShareFaceInfo(Face->Brush->FaceInfoArray, FaceInfo);

		if (Face->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
			return GR_FALSE;
	}

	return GR_TRUE;
}

static grBoolean grBrush_FaceRender(const grBrush_Face *Face, const DRV_Driver *Driver, const grCamera *Camera, const grFrustum *Frustum);

//========================================================================================
//	grBrush_Render
//========================================================================================
GRAPI grBoolean GRCC grBrush_Render(const grBrush *Brush, const grEngine *Engine, const grCamera *Camera)
{
	grBrush_Face		*Face;
	const DRV_Driver			*Driver;
	grFrustum			Frustum, WorldSpaceFrustum;

	Driver = grEngine_GetDriver(Engine);

	assert(Driver);

	grFrustum_SetFromCamera(&Frustum, Camera);
	grFrustum_TransformToWorldSpace(&Frustum, Camera, &WorldSpaceFrustum);
	
	for (Face = Brush->Faces; Face; Face = Face->Next)
	{
		if (!grBrush_FaceRender(Face, Driver, Camera, &WorldSpaceFrustum))
			return GR_FALSE;
	}

	return GR_TRUE;
}

#define MAX_TEMP_VERTS		64

//extern grPolyMgr		*HackPolyMgr;

//========================================================================================
//	grBrush_FaceRender
//========================================================================================
grBoolean grBrush_FaceRender(const grBrush_Face *Face, const DRV_Driver *Driver, const grCamera *Camera, const grFrustum *Frustum)
{
	int32				NumVerts;
	grLVertex			LVerts1[MAX_TEMP_VERTS], LVerts2[MAX_TEMP_VERTS];
	grLVertex			*pLVert;
	grTLVertex			TLVerts[MAX_TEMP_VERTS];
	int32				i;
	grFrustum_LClipInfo	ClipInfo;

	pLVert = LVerts1;				// Fill in this array with x,y,z,u,v,r,g,b...

	NumVerts = Face->Poly->NumVerts;
	assert(NumVerts+4 < MAX_TEMP_VERTS);

	// Copy the index verts, and the uvrgb's into a grLVertex structure
	for (i=0; i<NumVerts; i++)
	{
		grVec3d	pSrcVert;

		pSrcVert = grBrush_FaceGetWorldSpaceVertByIndex(Face, i);

		pLVert->X = pSrcVert.X;
		pLVert->Y = pSrcVert.Y;
		pLVert->Z = pSrcVert.Z;

		pLVert++;
	}
	
	// Fill LVerts2 with clipped LVerts1
	ClipInfo.SrcVerts = LVerts1;
	ClipInfo.NumSrcVerts = NumVerts;
	ClipInfo.Work1 = LVerts1;
	ClipInfo.Work2 = LVerts2;
	ClipInfo.ClipFlags = 0xffffffff;

	// Clip the verts against the frustum
	if (!grFrustum_ClipLVertsXYZUV(Frustum, &ClipInfo))
		return GR_TRUE;		// Poly was clipped away

	for (pLVert = ClipInfo.DstVerts, i=0; i<ClipInfo.NumDstVerts; i++, pLVert++)
	{
		pLVert->r = pLVert->g = 255.0f;
		pLVert->b = 0.0f;
		pLVert->a = 110.0f;
	}

	// Transform and project the point
	grCamera_TransformAndProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	Driver->RenderGouraudPoly(TLVerts, ClipInfo.NumDstVerts, GR_RENDER_FLAG_ALPHA);

	return GR_TRUE;
}

//========================================================================================
//	****** local static functions ********
//========================================================================================

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
		((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define GR_BRUSH_TAG				MAKEFOURCC('G', 'E', 'B', 'F')		// 'GE' 'B'rush 'F'ile
#define GR_BRUSH_VERSION			0x0000

//========================================================================================
//	grBrush_WriteHeader
//========================================================================================
static grBoolean grBrush_WriteHeader(const grBrush *Brush, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(Brush);
	assert(VFile);

	// Write TAG
	Tag = GR_BRUSH_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_BRUSH_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	// Write out the Brush flags
	if (!grVFile_Write(VFile, &Brush->Flags, sizeof(Brush->Flags)))
		return GR_FALSE;

	// Write out misc data
	if (!grVFile_Write(VFile, &Brush->XForm, sizeof(Brush->XForm)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &Brush->WorldToLocked, sizeof(Brush->WorldToLocked)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &Brush->Contents, sizeof(Brush->Contents)))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grBrush_WriteArrays
//========================================================================================
static grBoolean grBrush_WriteArrays(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMgr)
{
	if (!grVertArray_WriteToFile(Brush->VertArray, VFile))
		return GR_FALSE;

	if (Brush->FaceInfoArray)
	{
		assert(Brush->Flags & GR_BRUSH_HAS_FACEINFO_ARRAY);

		if (!grFaceInfo_ArrayWriteToFile(Brush->FaceInfoArray, VFile, NULL, NULL, PtrMgr))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
// grBrush_WriteFaces
//========================================================================================
static grBoolean grBrush_WriteFaces(const grBrush *Brush, grVFile *VFile)
{
	grBrush_Face		*Face;
	int32				NumFaces;

	// Count faces
	NumFaces = 0;
	for (Face = Brush->Faces; Face; Face = Face->Next)
		NumFaces++;

	// Write out number of faces
	if (!grVFile_Write(VFile, &NumFaces, sizeof(NumFaces)))
		return GR_FALSE;

	for (Face = Brush->Faces; Face; Face = Face->Next)
	{
		if (!grBrush_WriteFaceToFile(Brush, Face, VFile))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grBrush_ReadHeader
//========================================================================================
static grBoolean grBrush_ReadHeader(grBrush *Brush, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(Brush);
	assert(VFile);

	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	if (Tag != GR_BRUSH_TAG)
		return GR_FALSE;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	if (Version != GR_BRUSH_VERSION)
		return GR_FALSE;

	// Read the Brush flags
	if (!grVFile_Read(VFile, &Brush->Flags, sizeof(Brush->Flags)))
		return GR_FALSE;

	// Read misc data
	if (!grVFile_Read(VFile, &Brush->XForm, sizeof(Brush->XForm)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &Brush->WorldToLocked, sizeof(Brush->WorldToLocked)))
		return GR_FALSE;

	// Get the LockedToWorld XForm off the WorldToLocked XForm
	grXForm3d_GetInverse(&Brush->WorldToLocked, &Brush->LockedToWorld);

	if (!grVFile_Read(VFile, &Brush->Contents, sizeof(Brush->Contents)))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grBrush_ReadArrays
//========================================================================================
static grBoolean grBrush_ReadArrays(grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMgr)
{
	assert(Brush);
	assert(VFile);

	Brush->VertArray = grVertArray_CreateFromFile(VFile);

	if (!Brush->VertArray)
		goto ExitWithError;

	if (Brush->Flags & GR_BRUSH_HAS_FACEINFO_ARRAY)
	{
		Brush->FaceInfoArray = grFaceInfo_ArrayCreateFromFile(VFile, NULL, NULL, PtrMgr);

		if (!Brush->FaceInfoArray)
			goto ExitWithError;
	}

	return GR_TRUE;

	ExitWithError:
	{
		if (Brush->VertArray)
			grVertArray_Destroy(&Brush->VertArray);
		if (Brush->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&Brush->FaceInfoArray);

		return GR_FALSE;
	}
}

//========================================================================================
//	grBrush_ReadFaces
//========================================================================================
static grBoolean grBrush_ReadFaces(grBrush *Brush, grVFile *VFile)
{
	int32	NumFaces, i;

	assert(Brush);
	assert(VFile);

	// Read number of faces
	if (!grVFile_Read(VFile, &NumFaces, sizeof(NumFaces)))
		return GR_FALSE;

	for (i=0; i< NumFaces; i++)
	{
		grBrush_Face	*Face;

		Face = grBrush_CreateFaceFromFile(Brush, VFile);

		if (!Face)
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	DettachFaceInfoArray
//========================================================================================
static grBoolean DettachFaceInfoArray(grBrush *Brush)
{
	grBrush_Face	*Face;

	if (!Brush->FaceInfoArray)
	{
		assert(!(Brush->Flags & GR_BRUSH_HAS_FACEINFO_ARRAY));
		return GR_TRUE;				// Nothing to dettach
	}

	assert(Brush->Flags & GR_BRUSH_HAS_FACEINFO_ARRAY);

	// Save off the FaceInfo in a copy, and remove the index from the array for each face
	for (Face = Brush->Faces; Face; Face = Face->Next)
	{
		assert(!Face->FaceInfo);

		Face->FaceInfo = GR_RAM_ALLOCATE_STRUCT(grFaceInfo);

		if (!Face->FaceInfo)
			return GR_FALSE;

		// Save off the FaceInfo
		*(Face->FaceInfo) = *grFaceInfo_ArrayGetFaceInfoByIndex(Brush->FaceInfoArray, Face->FaceInfoIndex);

		// Remove the index from the current Array
		grFaceInfo_ArrayRemoveFaceInfo(Brush->FaceInfoArray, &Face->FaceInfoIndex);
	}

	// Destroy the ref on the array
	grFaceInfo_ArrayDestroy(&Brush->FaceInfoArray);

	Brush->FaceInfoArray = NULL;
	Brush->Flags &= ~GR_BRUSH_HAS_FACEINFO_ARRAY;

	return GR_TRUE;
}

//========================================================================================
//	AttachFaceInfoArray
//========================================================================================
static grBoolean AttachFaceInfoArray(grBrush *Brush, grFaceInfo_Array *Array)
{
	grBrush_Face	*Face;

	if (!Array)
		return GR_TRUE;

	assert(!Brush->FaceInfoArray);
	assert(!(Brush->Flags & GR_BRUSH_HAS_FACEINFO_ARRAY));

	for (Face = Brush->Faces; Face; Face = Face->Next)
	{
		assert(Face->FaceInfo);

		// Get the index fromt he faceinfo
		Face->FaceInfoIndex = grFaceInfo_ArrayShareFaceInfo(Array, Face->FaceInfo);

		if (Face->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
			return GR_FALSE;

		// Free the faceinfo
		grRam_Free(Face->FaceInfo);
		Face->FaceInfo = NULL;
	}

	// Ref new array
	if (!grFaceInfo_ArrayCreateRef(Array))
		return GR_FALSE;

	Brush->FaceInfoArray = Array;
	Brush->Flags |= GR_BRUSH_HAS_FACEINFO_ARRAY;

	return GR_TRUE;
}
