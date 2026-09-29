/****************************************************************************************/
/*  JEBSPNODE_FACE.C                                                                    */
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
#include <Assert.h>
#include <Math.h>

#include "grBSP._h"

#include "Errorlog.h"
#include "Ram.h"

static int32 g_ActiveFaces;
static int32 g_PeekFaces;

//=======================================================================================
//	grBSPNode_FaceCreate
//=======================================================================================
grBSPNode_Face *grBSPNode_FaceCreate(void)
{
	grBSPNode_Face		*Face;

	Face = GR_RAM_ALLOCATE_STRUCT(grBSPNode_Face);

	if (!Face)
		return NULL;

	ZeroMem(Face);

	g_ActiveFaces++;

	if (g_ActiveFaces > g_PeekFaces)
		g_PeekFaces = g_ActiveFaces;

	return Face;
}

//=======================================================================================
//	grBSPNode_FaceCreateFromPortal
//=======================================================================================
grBSPNode_Face *grBSPNode_FaceCreateFromPortal(grBSPNode_Portal *p, int32 s)
{
	grBSPNode_Face	*Face;
	grBSP_TopSide	*Side;
	
	assert(p);

	Side = p->Side;
	
	if (!Side)
		return NULL;	// Portal does not bridge different visible contents

	Face = grBSPNode_FaceCreate();

	if (!Face)
		return NULL;

	assert(Side->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX);
	Face->PlaneIndex = grPlaneArray_IndexGetPositive(Side->PlaneIndex);
	Face->Portal = p;

	Face->TopSideFlags = Side->TopSideFlags;

	if (s)
	{
		Face->PlaneIndex = grPlaneArray_IndexReverse(Face->PlaneIndex);
		Face->Poly = grPoly_CreateFromPoly(p->Poly, GR_TRUE);
	}
	else
	{
		Face->Poly = grPoly_CreateFromPoly(p->Poly, GR_FALSE);
	}

	if (!Face->Poly)
	{
		grRam_Free(Face);
		return NULL;
	}

	return Face;
}

//=======================================================================================
//	grBSPNode_FaceDestroy
//=======================================================================================
void grBSPNode_FaceDestroy(grBSPNode_Face **Face)
{
	assert(Face);
	assert(*Face);

	if ((*Face)->Poly)
		grPoly_Destroy(&(*Face)->Poly);

	grRam_Free(*Face);

	g_ActiveFaces--;

	*Face = NULL;
}

//=======================================================================================
//	grBSPNode_FaceGetActiveCount
//=======================================================================================
int32 grBSPNode_FaceGetActiveCount(void)
{
	return g_ActiveFaces;
}

//=======================================================================================
//	grBSPNode_FaceGetPeekCount
//=======================================================================================
int32 grBSPNode_FaceGetPeekCount(void)
{
	return g_PeekFaces;
}

//=======================================================================================
//	grBSPNode_FaceMerge
//=======================================================================================
grBoolean grBSPNode_FaceMerge(grBSPNode_Face *Face1, grBSPNode_Face *Face2, grBSP *BSP, grBSPNode_Face **Out)
{
	grPoly			*NewPoly;
	grBSPNode_Face	*NewFace;
	const grPlane	*pPlane;
	grVec3d			Normal;

	assert(Face1);
	assert(Face2);
	assert(Face1->Portal);
	assert(Face1->Portal->Side);
	assert(Face2->Portal);
	assert(Face2->Portal->Side);
	assert(Out);
	assert(Face1 != *Out);
	assert(Face2 != *Out);

	*Out = NULL;

	if (!grPlaneArray_IndexIsCoplanarAndFacing(Face1->PlaneIndex, Face2->PlaneIndex))
		return GR_TRUE;			// Planes don't match

	if ( Face1->Contents != Face2->Contents )
		return GR_TRUE;			// Don't merge faces across different contents

	if (Face1->Portal->Side->FaceInfoIndex != Face2->Portal->Side->FaceInfoIndex)
		return GR_TRUE;			// Different faceinfo

	if (Face1->TopSideFlags != Face2->TopSideFlags)
		return GR_TRUE;			// Different TopSideFlags

	#if 1
	//if (!(g_Options & GR_BSP_OPTIONS_MERJE_ACROSS_MULTIPLE_BRUSH_FACES))
	{
		// Don't merge across multiple brush faces
		if (Face1->Portal->Side->grBrushFace != Face2->Portal->Side->grBrushFace)
			return GR_TRUE;
	}
	#endif

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face1->PlaneIndex);

	Normal = pPlane->Normal;

	if (grPlaneArray_IndexSided(Face1->PlaneIndex))
		grVec3d_Inverse(&Normal);

	if (!grPoly_Merge(Face1->Poly, Face2->Poly, &Normal, &NewPoly))
	{
		grErrorLog_AddString(-1, "grBSPNode_FaceMerge:  grPoly_Merge failed.", NULL);
		return GR_FALSE;		 
	}

	if (!NewPoly)
		return GR_TRUE;		// Can't merge

	NewFace = grBSPNode_FaceCreate();

	if (!NewFace)
	{
		grPoly_Destroy(&NewPoly);
		return GR_FALSE;
	}

	*NewFace = *Face1;		// Copy face

	NewFace->Flags = 0;

	// Mark this face as a face in the chain, so we can find the originals
	//	(Original faces won't have the BSPFACE_MERGED_SPLIT flag)
	NewFace->Flags |= BSPFACE_MERGED_SPLIT;	

	NewFace->Poly = NewPoly;

	// Let face1, and face2 know who they merged with
	Face1->Merged = NewFace;
	Face2->Merged = NewFace;

	if (Face1->Portal->Side->grBrushFace != Face2->Portal->Side->grBrushFace)
		NewFace->Flags |= BSPFACE_SPAN_MULTIPLE_BRUSH_FACES;

	*Out = NewFace;			// This is the new face

	return GR_TRUE;
}

#define DEGENERATE_EPSILON		0.001f

//====================================================================================
//	grBSPNode_FaceIsValidGeometry
//====================================================================================
grBoolean grBSPNode_FaceIsValidGeometry(const grBSPNode_Face *Face, grBSP *BSP)
{
	int32		i, j, NumVerts;
	grVec3d		Vect1, *Verts, *V1, *V2, EdgeNormal;
	grPoly		*Poly;
	grFloat		Dist, EdgeDist;
	grPlane		Plane;
	
	Poly = Face->Poly;
	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	if (NumVerts < 3)
		return GR_FALSE;
	
	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);

	if (grPlaneArray_IndexSided(Face->PlaneIndex))
		grPlane_Inverse(&Plane);

	//	Check for degenerate edges, convexity, and make sure it's planar
	for (i=0; i< NumVerts; i++)
	{
		V1 = &Verts[i];
		V2 = &Verts[(i+1)%NumVerts];

		//	Check for degenreate edge
		grVec3d_Subtract(V2, V1, &Vect1);
		Dist = grVec3d_Length(&Vect1);
	
		if (fabs(Dist) < DEGENERATE_EPSILON)
			return GR_FALSE;

		// Check for planar
		Dist = grVec3d_DotProduct(V1, &Plane.Normal) - Plane.Dist;

		if (Dist > GR_BSP_ON_EPSILON || Dist <-GR_BSP_ON_EPSILON)
			return GR_FALSE;

		grVec3d_CrossProduct(&Plane.Normal, &Vect1, &EdgeNormal);
		grVec3d_Normalize(&EdgeNormal);
		EdgeDist = grVec3d_DotProduct(V1, &EdgeNormal);
		
		// Check for convexity
		for (j=0; j< NumVerts; j++)
		{
			Dist = grVec3d_DotProduct(&Verts[j], &EdgeNormal) - EdgeDist;
			
			if (Dist > GR_BSP_ON_EPSILON)
				return GR_FALSE;
		}
	}

	return GR_TRUE;
}

//====================================================================================
//	grBSPNode_FaceMergeList
//====================================================================================
grBoolean grBSPNode_FaceMergeList(grBSPNode_Face *Faces, grBSP *BSP, int32 *NumMerged)
{
	grBSPNode_Face	*Face1, *Face2, *End, *Merged;
	
	assert(Faces);
	assert(NumMerged);

	(*NumMerged) = 0;

	for (Face1 = Faces ; Face1 ; Face1 = Face1->Next)
	{
		if (Face1->Merged || Face1->Split[0] || Face1->Split[1])
			continue;

		for (Face2 = Faces ; Face2 != Face1 ; Face2 = Face2->Next)
		{
			if (Face2->Merged || Face2->Split[0] || Face2->Split[1])
				continue;
			
			Merged = NULL;
			if (!grBSPNode_FaceMerge(Face1, Face2, BSP, &Merged))
				return GR_FALSE;

			if (!Merged)
				continue;

			if (!grPoly_RemoveDegenerateEdges(Merged->Poly, 0.001f))
				return GR_FALSE;
			
			if (!grBSPNode_FaceIsValidGeometry(Merged, BSP))
			{
				grBSPNode_FaceDestroy(&Merged);
				Face1->Merged = NULL;		// Cancel out merge
				Face2->Merged = NULL;
				continue;
			}

			(*NumMerged)++;

			// Add the Merged to the end of the face list 
			// so it will be checked against all the faces again
			for (End = Faces ; End->Next ; End = End->Next);
				
			Merged->Next = NULL;
			End->Next = Merged;
			break;
		}
	}

	return GR_TRUE;
}

//====================================================================================
//	NewFaceFromFaceForSplit
//====================================================================================
static grBSPNode_Face *NewFaceFromFaceForSplit(grBSPNode_Face *f)
{
	grBSPNode_Face	*Newf;

	Newf = grBSPNode_FaceCreate();

	if (!Newf)
		return NULL;

	*Newf = *f;
	
	// Clear this faces split pointer/poly
	Newf->Split[0] = Newf->Split[1] = NULL;
	Newf->Poly = NULL;

	// Make sure that we remember that this is NOT an original face.
	// Mark it as a face in the chain, so we can destroy it once the drawfaces
	// are created from it!!!!!
	Newf->Flags |= BSPFACE_MERGED_SPLIT;
	
	return Newf;
}

//====================================================================================
//	grBSPNode_FaceSubdivide
//====================================================================================
grBoolean grBSPNode_FaceSubdivide(grBSPNode_Face *Face, grBSP *BSP, grBSPNode *Node, grFloat SubdivideSize, int32 *NumSubdivided)
{
	grFloat				Mins, Maxs, v;
	int32				Axis, i;
	grVec3d				Temp;
	grFloat				Dist;
	grPoly				*p, *Frontp, *Backp;
	grPlane				Plane;
	const grFaceInfo	*pFaceInfo;
	const grTexVec		*pTexVec;

	assert(Face);
	assert(Node);
	assert(Face->Portal);
	assert(Face->Portal->Side);

	if (Face->Merged)
		return GR_TRUE;
	
	pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, Face->Portal->Side->FaceInfoIndex);
	assert(pFaceInfo);

	// Faces that don't have lightmaps DON"T need to be subdivided
	if (!grFaceInfo_NeedsLightmap(pFaceInfo))
		return GR_TRUE;

	pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, Face->Portal->Side->TexVecIndex);
	assert(pTexVec);

	for (Axis = 0 ; Axis < 2 ; Axis++)
	{
		while (1)
		{
			Mins = 999999.0f;
			Maxs = -999999.0f;
			
			if (!Axis)
				grVec3d_Copy(&pTexVec->VecU, &Temp);
			else
				grVec3d_Copy(&pTexVec->VecV, &Temp);
			
			for (i=0 ; i<Face->Poly->NumVerts ; i++)
			{
				v = grVec3d_DotProduct(&Face->Poly->Verts[i], &Temp);
				if (v < Mins)
					Mins = v;
				if (v > Maxs)
					Maxs = v;
			}
			
			if (Maxs - Mins <= SubdivideSize)
				break;
			
			// Split it
			(*NumSubdivided)++;
			
			v = grVec3d_Normalize(&Temp);	

			Dist = (Mins + SubdivideSize - 16.0f)/v;

			Plane.Normal = Temp;
			Plane.Dist = Dist;
			Plane.Type = Type_Any;
			
			p = grPoly_CreateFromPoly(Face->Poly, GR_FALSE);

			if (!p)
				return GR_FALSE;

			if (!grPoly_SplitEpsilon(&p, 0.0f, &Plane, GR_FALSE, &Frontp, &Backp))
				return GR_FALSE;

			assert(Frontp || Backp);

			Face->Split[0] = NewFaceFromFaceForSplit(Face);
			Face->Split[0]->Poly = Frontp;
			Face->Split[0]->Next = Node->Faces;
			Node->Faces = Face->Split[0];

			Face->Split[1] = NewFaceFromFaceForSplit(Face);
			Face->Split[1]->Poly = Backp;
			Face->Split[1]->Next = Node->Faces;
			Node->Faces = Face->Split[1];

			if (!grBSPNode_FaceSubdivide(Face->Split[0], BSP, Node, SubdivideSize, NumSubdivided))
				return GR_FALSE;

			if (!grBSPNode_FaceSubdivide(Face->Split[1], BSP, Node, SubdivideSize, NumSubdivided))
				return GR_FALSE;

			return GR_TRUE;
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_FaceCullList
//=======================================================================================
grBSPNode_Face *grBSPNode_FaceCullList(grBSPNode_Face *List, grBSPNode_Face **Skip1)
{
	grBSPNode_Face	*NewList;
	grBSPNode_Face	*Next;

	NewList = NULL;

	for ( ; List ; List = Next)
	{
		Next = List->Next;

		if (List == *Skip1)
		{
			grBSPNode_FaceDestroy(Skip1);
			continue;
		}

		List->Next = NewList;
		NewList = List;
	}

	return NewList;
}
