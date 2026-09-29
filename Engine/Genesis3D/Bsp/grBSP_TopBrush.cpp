/****************************************************************************************/
/*  JEBSP_TOPBRUSH.C                                                                    */
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

// The grBSP_Top brush is the top level brush in the BSP world.  grBSP_TopBrushes
// are created from grBrushes.  Once this happens, no dependency exist between
// grBSP_TopBrushes, and grBrushes.  This was done so grBSP's could be saved independent
// of grBrushes.  The actual tree is built by creating a list of grBSP_Brushes from
// grBSP_TopBrushes.  This is done, because grBSP_Brushes need to be as small as possible.
// The grBSP_Brushes are then partitioned up, and the tree is born...
//		-John Pollard

#include <assert.h>
#include <stdio.h>
#include <memory.h>		// memset

// Private dependents
#include "grBSP._h"
#include "Ram.h"
#include "Quatern.h"

// Public dependents
#include "grBSP.h"

//=======================================================================================
//	grBSP_TopBrushCreate
//=======================================================================================
grBSP_TopBrush *grBSP_TopBrushCreate(int32 NumSides, uint32 Order)
{
	grBSP_TopBrush	*Brush;
	int32			c, i;

	assert(NumSides < 65535);

	// Get the size needed to accomodate the number of sides
	c = (sizeof(grBSP_TopBrush)-sizeof(grBSP_TopSide[GR_BSP_BRUSH_DEFAULT_SIDES]))+(sizeof(grBSP_TopSide)*NumSides);

	// Allocate the brush
	Brush = (grBSP_TopBrush*)grRam_Allocate(c);

	if (!Brush)
		return NULL;

	memset (Brush, 0, c);

	Brush->NumSides = (uint16)NumSides;

	for (i=0; i<NumSides; i++)
	{
		Brush->TopSides[i].PlaneIndex = GR_PLANEARRAY_NULL_INDEX;
		Brush->TopSides[i].FaceInfoIndex = GR_FACEINFO_ARRAY_NULL_INDEX;
		Brush->TopSides[i].TexVecIndex = GR_TEXVEC_ARRAY_NULL_INDEX;
	}

	Brush->Order = Order;

	return Brush;
}

//=======================================================================================
//	grBSP_TopBrushDestroy
//=======================================================================================
void grBSP_TopBrushDestroy(grBSP_TopBrush **Brush, grBSP *BSP)
{
	grBSP_TopSide		*Side;
	int32				i;

	assert(Brush);
	assert(*Brush);

	#pragma message ("Need a grBrush_FaceDestroy here for all TopSides...")
	#pragma message ("Need a grBrush_Destroy here...")
	
	for (Side = (*Brush)->TopSides, i=0; i<(*Brush)->NumSides; i++, Side++)
	{
		if (Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX)
			grPlaneArray_RemovePlane(BSP->PlaneArray, &Side->PlaneIndex);

		if (Side->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX)
			grFaceInfo_ArrayRemoveFaceInfo(BSP->FaceInfoArray, &Side->FaceInfoIndex);

		if (Side->TexVecIndex != GR_TEXVEC_ARRAY_NULL_INDEX)
			grTexVec_ArrayRemoveTexVec(BSP->TexVecArray, &Side->TexVecIndex);
	}

	grRam_Free(*Brush);

	*Brush = NULL;
}

//=======================================================================================
//	grBSP_TopBrushDestroyList
//=======================================================================================
void grBSP_TopBrushDestroyList(grBSP_TopBrush **Brushes, grBSP *BSP)
{
	grBSP_TopBrush	*Brush, *Next;

	for (Brush = *Brushes ; Brush ; Brush = Next)
	{
		Next = Brush->Next;

		grBSP_TopBrushDestroy(&Brush, BSP);
	}
	
	*Brushes = NULL;		
}

#define SHEET_BRUSH_THICKNESS	5.0f

//=======================================================================================
//	grBSP_TopBrushCreateFromBrushSheet
//	Create a special TopBrush that is a sheet
//=======================================================================================
static grBSP_TopBrush *grBSP_TopBrushCreateFromBrushSheet(grBrush *Brush, grBSP *BSP, uint32 Order)
{
	grBrush_Face		*Face;
	grBSP_TopSide		*Side;
	int32				i, v, NumFaces, NumVerts;
	grVec3d				Tri[3];
	grPlane				Plane;
	grBSP_TopBrush		*TopBrush;
	grBrush_Contents	Contents;
//	grFaceInfo	      FaceInfo;

	NumFaces = grBrush_GetFaceCount(Brush);

	if (!NumFaces)
		return NULL;

	assert(NumFaces == 1);	// Only one face in a sheet brush

	Contents = grBrush_GetContents(Brush);

	Face = grBrush_GetNextFace(Brush, NULL);		// Get the sheet face
	assert(Face);
	
	NumVerts = grBrush_FaceGetVertCount(Face);
	assert(NumVerts >= 3);

	// Create the top brush
	NumFaces = NumVerts+2;	// Make enough faces to extrude the sheet (2 faces + poly edges)

	TopBrush = grBSP_TopBrushCreate(NumFaces, Order);

	if (!TopBrush)
		return NULL;

	TopBrush->Original = Brush;

	//
	// Create the first side
	//
	Side = TopBrush->TopSides;

	// Create a plane from the polys verts
	for (v=0; v< 3; v++)
	{
		Tri[v] = grBrush_FaceGetWorldSpaceVertByIndex(Face, v);
	}

	// Create the plane
	grPlane_SetFromVerts(&Plane, &Tri[0], &Tri[1], &Tri[2]);

	Side->PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);
	assert(Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);

	if (Side->PlaneIndex == GR_PLANEARRAY_NULL_INDEX)
		return NULL;

	// Assign the faceinfo index
	if (!grBSP_TopBrushSideCalcFaceInfo(TopBrush, BSP, Side, Face))
		return NULL;

	// This side is the actual sheet, so it's visible
	Side->Flags = SIDE_VISIBLE;

	Side->Flags |= SIDE_SHEET;	

	// KROUER: try to change the flag for the VIS Bug
    //grBrush_FaceGetFaceInfo(Face, &FaceInfo);
   	//if (FaceInfo.Flags & FACEINFO_VIS_PORTAL)
   	{
		Side->Flags |= (SIDE_VIS_PORTAL|SIDE_HINT);
		//Side->Flags |= SIDE_VIS_PORTAL;
		//Side->Flags |= SIDE_HINT;
   	}

	Side++;

	//
	// Extrude and flip the sheet to the other side
	//
	for (v=0; v< 3; v++)
		grVec3d_AddScaled(&Tri[v], &Plane.Normal, -SHEET_BRUSH_THICKNESS, &Tri[v]);

	// Create a reversed plane
	grPlane_SetFromVerts(&Plane, &Tri[0], &Tri[1], &Tri[2]);
	grPlane_Inverse(&Plane);

	Side->PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);
	assert(Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);
	Side->Flags = 0;		// Side is not visible
	Side->FaceInfoIndex = GR_FACEINFO_ARRAY_NULL_INDEX;
	Side++;

	// Create the brush sides from the poly edges
	for (i=0; i< NumVerts; i++, Side++)
	{
		int32				i2;
		grVec3d				Vert1, Vert2, Vec;
		grPlane				EdgePlane;
		grFloat				Length;

		i2 = (i+1 < NumVerts) ? (i+1): 0;

		Vert1 = grBrush_FaceGetWorldSpaceVertByIndex(Face, i);
		Vert2 = grBrush_FaceGetWorldSpaceVertByIndex(Face, i2);

		grVec3d_Subtract(&Vert1, &Vert2, &Vec);
		Length = grVec3d_Normalize(&Vec);

		assert(Length > 0.1);

		memset(&EdgePlane, 0, sizeof(grPlane));
		grVec3d_CrossProduct(&Plane.Normal, &Vec, &EdgePlane.Normal);
		EdgePlane.Dist = grVec3d_DotProduct(&Vert1, &EdgePlane.Normal);
		
		Side->PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &EdgePlane);
		assert(Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);
		Side->Flags = 0;		// Side is not visible
		Side->FaceInfoIndex = GR_FACEINFO_ARRAY_NULL_INDEX;
	}

	// Just in case they used other contents, just force it to be sheet contents.
	//	The only requirement is that they set it as sheet on the brush
	TopBrush->Contents = GR_BSP_CONTENTS_SHEET;

	assert(TopBrush->Contents & GR_BSP_CONTENTS_SHEET);

	#pragma message ("Need a grBrush_CreateRef here...")

	return TopBrush;
}

//=======================================================================================
//	grBSP_TopBrushCreateFromBrush
//	This function will return a list of convex grBSP_TopBrushes, if grBrush is NOT convex...
//	It is ok that a list is returned, because grBSP_TopBrush is ONLY used by grBSP
//	module and friends.  It is NOT publicized that there is a next pointer in the grBSP_TopBrush...
//=======================================================================================
grBSP_TopBrush *grBSP_TopBrushCreateFromBrush(grBrush *Brush, grBSP *BSP, uint32 Order)
{
	grBSP_TopBrush	*TopBrush;
	grBSP_TopSide	*Side;
	int32			NumFaces, NeededFaces;
	grBrush_Face	*Face;

	assert(Brush);

	NumFaces = grBrush_GetFaceCount(Brush);

	if (!NumFaces)
		return NULL;

	// If this is a sheet brush, send it down a different pipeline
	if (grBrush_GetContents(Brush) & GR_BSP_CONTENTS_SHEET)
		return grBSP_TopBrushCreateFromBrushSheet(Brush, BSP, Order);

	// Count how many faces we'll need to create this brush
	Face = NULL;
	NeededFaces = 0;
	while (Face = grBrush_GetNextFace(Brush, Face))
	{
		NeededFaces += grBrush_FaceGetVertCount(Face) - 2;
	}

	TopBrush = grBSP_TopBrushCreate(NeededFaces, Order);

	if (!TopBrush)
		return NULL;

	TopBrush->Original = Brush;

	Face = NULL;
	Side = TopBrush->TopSides;

	// Reset number of TopSides, so we can increment it as we actually create them, so it is accurate
	TopBrush->NumSides = 0;

	// Copy the polys over
	while (Face = grBrush_GetNextFace(Brush, Face))
	{
		int32				NumVerts, f;
		const grFaceInfo	*pFaceInfo;

		NumVerts = grBrush_FaceGetVertCount(Face);
		assert(NumVerts >= 3);

		// Triangulate the face out just in case it's not planar
		for (f = 0; f< NumVerts-2; f++)
		{
			int32				j;
			grPlane				Plane;
			grVec3d				Tri[3];

			// Create a plane from the current tri we are on in the face
			Tri[0] = grBrush_FaceGetWorldSpaceVertByIndex(Face, 0);
			Tri[1] = grBrush_FaceGetWorldSpaceVertByIndex(Face, f+1);
			Tri[2] = grBrush_FaceGetWorldSpaceVertByIndex(Face, f+2);

			// Create the plane
			grPlane_SetFromVerts(&Plane, &Tri[0], &Tri[1], &Tri[2]);

			// Find the plane index
			Side->PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);
			assert(Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);

			if (Side->PlaneIndex == GR_PLANEARRAY_NULL_INDEX)
				return NULL;

			// Check to see if the plane already exist in the brush, if so, remove it from the array, and do not
			//	create this face on the top brush...
			for (j=0; j< TopBrush->NumSides; j++)
			{
				if (grPlaneArray_IndexIsCoplanar(Side->PlaneIndex, TopBrush->TopSides[j].PlaneIndex))
				{
					// Remove this plane
					grPlaneArray_RemovePlane(BSP->PlaneArray, &Side->PlaneIndex);
					break;
				}
			}

			if (j != TopBrush->NumSides)
				continue;	// Plane already in list

			if (!grBSP_TopBrushSideCalcFaceInfo(TopBrush, BSP, Side, Face))
				return NULL;

			pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, Side->FaceInfoIndex);
			assert(pFaceInfo);

			// All TopSides default to being visible 
			Side->Flags = SIDE_VISIBLE;

			// KROUER: try to change the flag for the VIS Bug
         	if (pFaceInfo->Flags & FACEINFO_VIS_PORTAL) 
        	{
				Side->Flags |= (SIDE_VIS_PORTAL|SIDE_HINT);
				//Side->Flags |= SIDE_VIS_PORTAL;
            	//Side->Flags |= SIDE_HINT;
         	}
			if (pFaceInfo->PortalCamera)
				Side->Flags |= SIDE_HINT;

			Side++;
			TopBrush->NumSides++;

			assert(TopBrush->NumSides <= NeededFaces);
		}
	}

	// Get the contents from the editor Brush
	TopBrush->Contents = grBrush_GetContents(Brush);

	#pragma message ("Need a grBrush_CreateRef here...")

	return TopBrush;
}

//=======================================================================================
//	grBSP_TopBrushCreateListFromBrushChain
//=======================================================================================
grBSP_TopBrush *grBSP_TopBrushCreateListFromBrushChain(grChain *BrushChain, grBSP *BSP, uint32 *Order)
{
	grBSP_TopBrush	*TopBrushList;
	int32			BadBrushes;
	grBrush			*Brush;

	assert(BrushChain);

	BadBrushes = 0;
	TopBrushList = NULL;

	Brush = NULL;

	// Create the BSPBrushList
	while (Brush = (grBrush*)grChain_GetNextLinkData(BrushChain, Brush))
	{
		grBSP_TopBrush	*TopBrush;

		TopBrush = grBSP_TopBrushCreateFromBrush(Brush, BSP, (*Order));
		
		if (!TopBrush)
		{
			BadBrushes++;
			continue;
		}

		assert(TopBrush->Next == NULL);	// Only handle one brush at a time for now...

		// Add the BSPBrush to the beginning of the list
		TopBrush->Next = TopBrushList;
		TopBrushList = TopBrush;

		(*Order)++;
	}

	return TopBrushList;
}

//=======================================================================================
//	grBSP_TopBrushSideCalcFaceInfo
//=======================================================================================
grBoolean grBSP_TopBrushSideCalcFaceInfo(grBSP_TopBrush *TopBrush, grBSP *BSP, grBSP_TopSide *Side, const grBrush_Face *grFace)
{
	grPlane				Plane;
	const grFaceInfo	*pFaceInfo;
	
	assert(BSP->FaceInfoArray);
	assert(BSP->TexVecArray);
	assert(BSP->PlaneArray);
	
	// Remove any old FaceInfo/TexVec index data from the arrays
	if (Side->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX)
		grFaceInfo_ArrayRemoveFaceInfo(BSP->FaceInfoArray, &Side->FaceInfoIndex);
	
	if (Side->TexVecIndex != GR_TEXVEC_ARRAY_NULL_INDEX)
		grTexVec_ArrayRemoveTexVec(BSP->TexVecArray, &Side->TexVecIndex);
	
#pragma message ("Need grBrush_FaceCreateRef here...")
	Side->grBrushFace = (grBrush_Face*)grFace;
	
	assert(Side->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);
	
	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Side->PlaneIndex);
	
	// inverse the plane
	if (grPlaneArray_IndexSided(Side->PlaneIndex)) {
		grPlane_Inverse(&Plane);
	}
	
	// compute all face UV vertex coords 
	{
		grVec3d					LockedTranslation;
		const grXForm3d			*XForm, *WorldToLocked, *LockedToWorld;
		grTexVec				TexVec1, TexVec2;
		grFaceInfo				FaceInfo;
		grFaceInfo_ArrayIndex	FaceInfoIndex;
		grFloat					AddU, AddV, Len1, Len2;
		
		FaceInfoIndex = grBrush_FaceGetFaceInfoIndex((grBrush_Face*)grFace);
		
		// Get the faceinfo so we can access it
		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, FaceInfoIndex);
		assert(pFaceInfo);
		
		FaceInfo = *pFaceInfo;
		
		// Get the ModelToWorld, WorldToLocked, and LockedToWorld XForms
		XForm = grBrush_GetXForm(TopBrush->Original);
		assert(XForm);
		WorldToLocked = grBrush_GetWorldToLockedXForm(TopBrush->Original);
		assert(WorldToLocked);
		LockedToWorld = grBrush_GetLockedToWorldXForm(TopBrush->Original);
		assert(LockedToWorld);
		
		// Put the normal into locked space
		grXForm3d_Rotate(WorldToLocked, &Plane.Normal, &Plane.Normal);
		grVec3d_Normalize(&Plane.Normal);
		
		// Get the locked texture vectors from the locked normal
		grPlane_GetAAVectors(&Plane, &TexVec1.VecU, &TexVec1.VecV);
		
		// Scale and rotate the texture vectors
		assert(FaceInfo.DrawScaleU);
		assert(FaceInfo.DrawScaleV);
		
		assert(FaceInfo.LMapScaleU);
		assert(FaceInfo.LMapScaleV);
		
		grVec3d_Scale(&TexVec1.VecU, 1.0f/FaceInfo.LMapScaleU, &TexVec1.VecU);
		grVec3d_Scale(&TexVec1.VecV, 1.0f/FaceInfo.LMapScaleV, &TexVec1.VecV);
		
		// Rotate the texture vectors
		{
			grVec3d			Axis;
			grXForm3d		RotXForm;
			grQuaternion	Quat;
			
			grVec3d_CrossProduct(&TexVec1.VecU, &TexVec1.VecV, &Axis);
			
			grVec3d_Normalize(&Axis);
			
			grQuaternion_SetFromAxisAngle(&Quat, &Axis, (FaceInfo.Rotate/180.0f)*GR_PI);
			grQuaternion_ToMatrix(&Quat, &RotXForm);
			
			grXForm3d_Transform(&RotXForm, &TexVec1.VecU, &TexVec1.VecU);
			grXForm3d_Transform(&RotXForm, &TexVec1.VecV, &TexVec1.VecV);
		}
		
		
		// Rotate the locked texture vectors into world space
		grXForm3d_Rotate(LockedToWorld, &TexVec1.VecU, &TexVec2.VecU);
		grXForm3d_Rotate(LockedToWorld, &TexVec1.VecV, &TexVec2.VecV);
		
		// Inverse the scaling in the rotation
		//	This is done by first setting the length to the original locked length,
		//	then scaling this length by the ratio of the old length over the new length
		Len1 = grVec3d_Length(&TexVec1.VecU);
		Len2 = grVec3d_Length(&TexVec2.VecU);
		grVec3d_Normalize(&TexVec2.VecU);
		grVec3d_Scale(&TexVec2.VecU, Len1, &TexVec2.VecU);
		grVec3d_Scale(&TexVec2.VecU, Len1/Len2, &TexVec2.VecU);
		
		Len1 = grVec3d_Length(&TexVec1.VecV);
		Len2 = grVec3d_Length(&TexVec2.VecV);
		grVec3d_Normalize(&TexVec2.VecV);
		grVec3d_Scale(&TexVec2.VecV, Len1, &TexVec2.VecV);
		grVec3d_Scale(&TexVec2.VecV, Len1/Len2, &TexVec2.VecV);
		
		// Get the locked translation
		grXForm3d_Transform(WorldToLocked, &XForm->Translation, &LockedTranslation);
		
		// Get the difference in ShiftU/ShiftV, so we can correct for it
		//	We do this by projecting the translation of the face onto the corresponding new and locked texture vectors
		//	Then we take the difference...
		AddU = grVec3d_DotProduct(&TexVec2.VecU, &XForm->Translation) - grVec3d_DotProduct(&TexVec1.VecU, &LockedTranslation);
		AddV = grVec3d_DotProduct(&TexVec2.VecV, &XForm->Translation) - grVec3d_DotProduct(&TexVec1.VecV, &LockedTranslation);
		
		// Add it in 
		//	We must interpret the Shifts the same way the drivers do. 
		//	So we must scale the shift the same way the driver will scale, etc...
		FaceInfo.ShiftU -= AddU/(FaceInfo.DrawScaleU/FaceInfo.LMapScaleU);
		FaceInfo.ShiftV -= AddV/(FaceInfo.DrawScaleV/FaceInfo.LMapScaleV);
		
		// Obtain the FaceInfoIndex for this side, by re-sharing the FaceInfo into the array
		//	If nothing changed, then we should get the same faceinfo, otherwise, it will try to share with something else...
		Side->FaceInfoIndex = grFaceInfo_ArrayShareFaceInfo(BSP->FaceInfoArray, &FaceInfo);
		
		if (Side->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
			return GR_FALSE;
		
		// Get the texvec index by sharing the texture vectors into the texvec array
		Side->TexVecIndex = grTexVec_ArrayShareTexVec(BSP->TexVecArray, &TexVec2);
		
		if (Side->TexVecIndex == GR_TEXVEC_ARRAY_NULL_INDEX)
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_TopBrushCullList
//=======================================================================================
grBSP_TopBrush *grBSP_TopBrushCullList(grBSP_TopBrush *List, grBSP *BSP, grBSP_TopBrush *Skip1)
{
	grBSP_TopBrush	*NewList;
	grBSP_TopBrush	*Next;

	NewList = NULL;

	for ( ; List ; List = Next)
	{
		Next = List->Next;

		if (List == Skip1)
		{
			grBSP_TopBrushDestroy(&List, BSP);
			continue;
		}

		List->Next = NewList;
		NewList = List;
	}

	return NewList;
}
