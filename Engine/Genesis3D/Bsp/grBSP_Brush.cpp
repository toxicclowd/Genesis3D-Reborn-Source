/****************************************************************************************/
/*  JEBSP_BRUSH.C                                                                       */
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
#include <memory.h>		// memset

// Private dependents
#include "grBSP._h"
#include "Errorlog.h"
#include "Ram.h"
#include "Vec3d.h"
#include "Log.h"

// Public dependents
#include "grBSP.h"

#define BRUSH_SIZE(s) ((sizeof(grBSP_Brush)-sizeof(grBSP_Side[GR_BSP_BRUSH_DEFAULT_SIDES]))+(sizeof(grBSP_Side)*(s)));

static int32	g_ActiveBrushes = 0;
static int32	g_PeekBrushes = 0;

//=======================================================================================
//	grBSP_BrushCreate
//=======================================================================================
grBSP_Brush *grBSP_BrushCreate(int32 NumSides)
{
	grBSP_Brush		*Brush;
	int32			c;
	int32			i;

	assert(NumSides < 65535);

	// Get the size needed to accomodate the number of sides
	c = BRUSH_SIZE(NumSides);

	// Allocate the brush
	Brush = (grBSP_Brush*)grRam_Allocate(c);

	if (!Brush)
		return NULL;

	memset (Brush, 0, c);

	Brush->NumSides = (uint16)NumSides;

	// Default all sides to NULL PlaneIndex
	for (i=0; i<NumSides; i++)
		Brush->Sides[i].PlaneIndex = GR_PLANEARRAY_NULL_INDEX;

	g_ActiveBrushes++;

	if (g_ActiveBrushes > g_PeekBrushes)
		g_PeekBrushes = g_ActiveBrushes;

	return Brush;
}

//=======================================================================================
//	grBSP_BrushCreateFromBox
//=======================================================================================
grBSP_Brush *grBSP_BrushCreateFromBox(grBSP *BSP, const grExtBox *Box)
{
	grBSP_Brush	*b;
	int32		i;
	grPlane		Plane;

	b = grBSP_BrushCreate(6);

	for (i=0 ; i<3 ; i++)
	{
		grVec3d_Clear(&Plane.Normal);
		grVec3d_SetElement(&Plane.Normal, i, 1.0f);
		Plane.Dist = grVec3d_GetElement(&((grExtBox*)Box)->Max, i)+1.0f;
		b->Sides[i].PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);

		grVec3d_SetElement(&Plane.Normal, i, -1.0f);
		Plane.Dist = -(grVec3d_GetElement(&((grExtBox*)Box)->Min, i)-1.0f);
		b->Sides[i+3].PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);
	}

	if (!grBSP_BrushCreatePolys(b, BSP))
	{
		grBSP_BrushDestroy(&b);
		return NULL;
	}

	return b;
}

//=======================================================================================
//	grBSP_BrushDestroy
//=======================================================================================
void grBSP_BrushDestroy(grBSP_Brush **Brush)
{
	grBSP_Brush		*pBrush;
	int32			i;

	assert(Brush);

	pBrush = *Brush;

	assert(pBrush);

	// Free all the polys on the brush
	for (i=0 ; i<pBrush->NumSides ; i++)
	{
		if (pBrush->Sides[i].Poly)
			grPoly_Destroy(&pBrush->Sides[i].Poly);
	}

	grRam_Free(pBrush);

	*Brush = NULL;

	g_ActiveBrushes--;
}

//=======================================================================================
//	grBSP_BrushGetActiveCount
//=======================================================================================
int32 grBSP_BrushGetActiveCount(void)
{
	return g_ActiveBrushes;
}

//=======================================================================================
//	grBSP_BrushGetPeekCount
//=======================================================================================
int32 grBSP_BrushGetPeekCount(void)
{
	return g_PeekBrushes;
}

//=======================================================================================
//	grBSP_BrushDestroyList
//=======================================================================================
void grBSP_BrushDestroyList(grBSP_Brush **Brushes)
{
	grBSP_Brush	*Brush, *Next;

	for (Brush = *Brushes ; Brush ; Brush = Next)
	{
		Next = Brush->Next;

		grBSP_BrushDestroy(&Brush);
	}
	
	*Brushes = NULL;		
}

//=======================================================================================
//	grBSP_BrushCullList
//=======================================================================================
grBSP_Brush *grBSP_BrushCullList(grBSP_Brush *List, grBSP_Brush *Skip1)
{
	grBSP_Brush	*NewList;
	grBSP_Brush	*Next;

	NewList = NULL;

	for ( ; List ; List = Next)
	{
		Next = List->Next;

		if (List == Skip1)
		{
			grBSP_BrushDestroy(&List);
			continue;
		}

		List->Next = NewList;
		NewList = List;
	}

	return NewList;
}

//=======================================================================================
//	grBSP_BrushCreateFromBSPBrush
//	Copys a BSPBrush
//=======================================================================================
grBSP_Brush *grBSP_BrushCreateFromBSPBrush(const grBSP_Brush *Brush)
{
	grBSP_Brush *NewBrush;
	int32		Size;
	int32		i;
	
	assert(Brush);
	assert(Brush->NumSides > 0);

	Size = BRUSH_SIZE(Brush->NumSides);

	NewBrush = grBSP_BrushCreate(Brush->NumSides);

	if (!NewBrush)
		return NULL;

	// Copy entire brush over
	memcpy (NewBrush, Brush, Size);

	// Copy all the polys over
	for (i=0 ; i<Brush->NumSides ; i++)
	{
		if (!Brush->Sides[i].Poly)
			continue;

		assert(Brush->Sides[i].Poly->NumVerts > 0);
		assert(Brush->Sides[i].Poly->NumVerts < 255);
		
		NewBrush->Sides[i].Poly = grPoly_CreateFromPoly(Brush->Sides[i].Poly, GR_FALSE);
		
		if (!NewBrush->Sides[i].Poly)
			return NULL;
	}

	return NewBrush;
}

//=======================================================================================
//	grBSP_SideSetFromTopSide
//=======================================================================================
void grBSP_SideSetFromTopSide(grBSP_Side *Side, const grBSP_TopSide *TopSide)
{
	assert(Side);
	assert(TopSide);
	assert(TopSide->PlaneIndex != GR_PLANEARRAY_NULL_INDEX);

	Side->PlaneIndex = TopSide->PlaneIndex;
	//Side->FaceInfoIndex = TopSide->FaceInfoIndex;
	Side->Flags = TopSide->Flags;

	if (TopSide->Flags & SIDE_HINT)
		Side->Flags |= SIDE_VISIBLE;		// Hint allways visible
}

//=======================================================================================
//	grBSP_BrushCreateFromTopBrush
//=======================================================================================
grBSP_Brush *grBSP_BrushCreateFromTopBrush(grBSP_TopBrush *TopBrush, grBSP *BSP)
{
	grBSP_Brush		*BSPBrush;
	int32			i;

	assert(TopBrush);

	// Create a BSPBrush with the same number of sides as the top brush
	BSPBrush = grBSP_BrushCreate(TopBrush->NumSides);

	if (!BSPBrush)
		return NULL;

	// Copy the sides over
	for (i=0; i< TopBrush->NumSides; i++)
	{
		grBSP_TopSide	*TopSide;
		grBSP_Side		*Side;

		TopSide = &TopBrush->TopSides[i];
		Side = &BSPBrush->Sides[i];

		// Copy the side
		grBSP_SideSetFromTopSide(Side, TopSide);
	}

	// Create the polys out of the sides on the BSPBrush.  It should be a perfect skin of the brush...
	if (!grBSP_BrushCreatePolys(BSPBrush, BSP))
		goto ExitWithError;

	BSPBrush->Original = TopBrush;		// Remember the top brush that created this brush

	return BSPBrush;

	// Error
	ExitWithError:
	{
		if (BSPBrush)
			grBSP_BrushDestroy(&BSPBrush);

		return NULL;
	}
}

//=======================================================================================
//	grBSP_BrushCreateListFromTopBrushList
//=======================================================================================
grBSP_Brush *grBSP_BrushCreateListFromTopBrushList(grBSP_TopBrush *TopBrushList, grBSP *BSP)
{
	grBSP_TopBrush	*TopBrush;
	grBSP_Brush		*BSPBrushList;

	BSPBrushList = NULL;

	for (TopBrush = TopBrushList; TopBrush; TopBrush = TopBrush->Next)
	{
		grBSP_Brush		*BSPBrush;

		BSPBrush = grBSP_BrushCreateFromTopBrush(TopBrush, BSP);

		if (!BSPBrush)
		{
			grBSP_BrushDestroyList(&BSPBrushList);
			return NULL;
		}

		assert(BSPBrush->Next == NULL);

		BSPBrush->Next = BSPBrushList;
		BSPBrushList = BSPBrush;
	}

	return BSPBrushList;
}

//=======================================================================================
//	grBSP_BrushCalcBounds
//=======================================================================================
grBoolean grBSP_BrushCalcBounds(grBSP_Brush *BSPBrush)
{
	int32		i;
	grBoolean	Set;

	Set = GR_FALSE;

	// Go through all the brush sides
	for (i=0; i< BSPBrush->NumSides; i++)
	{
		grPoly	*Poly;
		int32	v;

		Poly = BSPBrush->Sides[i].Poly;

		if (!Poly)
			continue;

		if (!Set)		// Take first valid point as box default
		{
			grExtBox_SetToPoint(&BSPBrush->Box, &BSPBrush->Sides[i].Poly->Verts[0]);
			Set = GR_TRUE;
		}

		// Extend the box
		for (v=0; v< Poly->NumVerts; v++)
			grExtBox_ExtendToEnclose(&BSPBrush->Box, &Poly->Verts[v]);
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BrushIsValidGeometry
//=======================================================================================
grBoolean grBSP_BrushIsValidGeometry(grBSP_Brush *BSPBrush)
{
	int32		j;

	if (BSPBrush->NumSides < 3)
	{
		grErrorLog_AddString(-1, "BSPBrush NumSides < 3", NULL);
		return GR_FALSE;
	}

	for (j=0 ; j<3 ; j++)
	{
		if (grVec3d_GetElement(&BSPBrush->Box.Min, j) >= grVec3d_GetElement(&BSPBrush->Box.Max, j))
			return GR_FALSE;

		if (grVec3d_GetElement(&BSPBrush->Box.Min, j) <= -GR_BSP_MINMAX_BOUNDS)
			return GR_FALSE;

		if (grVec3d_GetElement(&BSPBrush->Box.Max, j) >= GR_BSP_MINMAX_BOUNDS)
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grExtBox_Intersects
//=======================================================================================
static grBoolean grExtBox_Intersects(  const grExtBox *B1,  const grExtBox *B2 )
{
	assert ( grExtBox_IsValid (B1) != GR_FALSE );
	assert ( grExtBox_IsValid (B2) != GR_FALSE );

	if ((B1->Min.X >= B2->Max.X) || (B1->Max.X <= B2->Min.X)) return GR_FALSE;
	if ((B1->Min.Y >= B2->Max.Y) || (B1->Max.Y <= B2->Min.Y)) return GR_FALSE;
	if ((B1->Min.Z >= B2->Max.Z) || (B1->Max.Z <= B2->Min.Z)) return GR_FALSE;
	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BrushDisjoint
//	Returns GR_TRUE is 2 brushes are NOT overlapping
//=======================================================================================
grBoolean grBSP_BrushDisjoint(const grBSP_Brush *b1, const grBSP_Brush *b2)
{
	int32		i, j;

	// Check bounding boxes
	if (!grExtBox_Intersects(&b1->Box, &b2->Box))
		return GR_TRUE;		// Bounding boxes don't overlap
	
	// Check for opposing planes
	for (i=0 ; i<b1->NumSides ; i++)
	{
		grPlaneArray_Index	i1, i2;
		
		i1 = b1->Sides[i].PlaneIndex;
		
		for (j=0 ; j<b2->NumSides ; j++)
		{
			i2 = b2->Sides[j].PlaneIndex;

			if (grPlaneArray_IndexIsCoplanarAndNotFacing(i1, i2))
			{
				//Log_Printf("Disjoint by opposing planes...\n");
				return GR_TRUE;	// Opposite/Coplanar planes, so not overlapping
			}
		}
	}
	
	return GR_FALSE;	// Might intersect
}

//=======================================================================================
//	grBSP_BrushCanBite
//=======================================================================================
grBoolean grBSP_BrushCanBite(const grBSP_Brush *b1, const grBSP_Brush *b2)
{
	grBrush_Contents	c1, c2;
	grBSP_TopBrush		*t1, *t2;

	t1 = b1->Original;
	t2 = b2->Original;

	if (t1->Order < t2->Order)
		return GR_FALSE;

	c1 = t1->Contents;
	c2 = t2->Contents;

#ifdef USE_DETAIL
	// Detail brushes never bite structural brushes
	if ( (c1 & GR_BSP_CONTENTS_DETAIL) && !(c2 & GR_BSP_CONTENTS_DETAIL) )
		return GR_FALSE;
#endif

	//if (c1 & BSP_CONTENTS_FLOCKING)
	//	return GR_FALSE;
	//if (c2 & GR_BSP_CONTENTS_FLOCK)		// Nothing cuts a flock brush (they override)
	//	return GR_FALSE;

	//if (c1 & GR_BSP_CONTENTS_SOLID)	
	//	return GR_TRUE;

	if (c1 == c2)
		return GR_TRUE;

	return GR_FALSE;
}

//=======================================================================================
//	grBSP_BrushVolume
//=======================================================================================
float grBSP_BrushVolume(grBSP_Brush *Brush, grBSP *BSP)
{
	int32		i;
	grPoly		*p;
	grVec3d		Corner;
	float		d, Area, Volume;
	grBSP_Side	*pSide;

	assert(Brush);

	if (!Brush)
		return 0.0f;

	// Grab the first valid point as the corner
	for (p=NULL, i=0 ; i<Brush->NumSides ; i++)
	{
		p = Brush->Sides[i].Poly;
		if (p)
			break;
	}

	if (!p)				// No polys on the brush
		return 0.0f;

	grVec3d_Copy(&p->Verts[0], &Corner);

	// Make tetrahedrons to all other faces
	for (Volume = 0.0f, pSide = Brush->Sides; i<Brush->NumSides ; i++, pSide++)
	{
		const grPlane	*pPlane;

		p = pSide->Poly;

		if (!p)
			continue;

		pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex);

		d = grPlane_PointDistanceFast(pPlane, &Corner);

		if (grPlaneArray_IndexSided(pSide->PlaneIndex))
			d = -d;

		Area = grPoly_Area(p);
		Volume += -d*Area;
	}

	Volume /= 3.0f;

	return Volume;
}

//=======================================================================================
//	grBSP_BrushMostlyOnPlaneSide
//=======================================================================================
grPlane_Side grBSP_BrushMostlyOnPlaneSide(const grBSP_Brush *Brush, const grPlane *Plane)
{
	int32			i, j;
	grPoly			*p;
	float			d, Max;
	grPlane_Side	Side;

	Max = 0.0f;
	Side = PSIDE_FRONT;		// Default to front side

	for (i=0 ; i<Brush->NumSides ; i++)
	{
		p = Brush->Sides[i].Poly;

		if (!p)
			continue;

		for (j=0 ; j<p->NumVerts ; j++)
		{
			d = grVec3d_DotProduct(&p->Verts[j], &Plane->Normal) - Plane->Dist;

			if (d > Max)
			{
				Max = d;
				Side = PSIDE_FRONT;
			}
			if (-d > Max)
			{
				Max = -d;
				Side = PSIDE_BACK;
			}
		}
	}

	return Side;
}

//=======================================================================================
//	grBSP_BrushCountList
//=======================================================================================
int32 grBSP_BrushCountList(const grBSP_Brush *Brushes)
{
	int32	c;

	for (c=0 ; Brushes ; Brushes = Brushes->Next)
		c++;

	return c;
}

//=======================================================================================
//	grBSP_BrushCreatePolys
//=======================================================================================
grBoolean grBSP_BrushCreatePolys(grBSP_Brush *Brush, grBSP *BSP)
{
	int32			i, j;

	for (i=0 ; i<Brush->NumSides ; i++)
	{
		grPoly			*p;
		grBSP_Side		*Side;
		grPlane			Plane;
		const grPlane	*pPlane;

		Side = &Brush->Sides[i];

		if (Side->Poly)
			grPoly_Destroy(&Side->Poly);		// Destroy all old polys

		Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Side->PlaneIndex);
		Plane.Type = Type_Any;

		if (grPlaneArray_IndexSided(Side->PlaneIndex))
			grPlane_Inverse(&Plane);

		p = grPoly_CreateFromPlane(&Plane, GR_BSP_MINMAX_BOUNDS);

		if (!p)
			return GR_FALSE;

		for (j=0 ; j<Brush->NumSides && p; j++)
		{
			if (i == j)
				continue;

			pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Brush->Sides[j].PlaneIndex);

			if (!grPoly_ClipEpsilon(&p, 0.0f, pPlane, !grPlaneArray_IndexSided(Brush->Sides[j].PlaneIndex)))
				return GR_FALSE;
		}

		Side->Poly = p;
		
		if (!Side->Poly)					// If no poly on side, then make is invisible
			Side->Flags &= ~SIDE_VISIBLE;
	}

	// Calculate the bounds of the brush
	if (!grBSP_BrushCalcBounds(Brush))
		return GR_FALSE;

	// Make sure it's valid
	if (!grBSP_BrushIsValidGeometry(Brush))
	{
		grErrorLog_AddString(-1, "grBSP_BrushCreatePolys:  grBSP_BrushIsValidGeometry failed.", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BrushListAddToTail
//=======================================================================================
grBSP_Brush *grBSP_BrushListAddToTail(grBSP_Brush *List, grBSP_Brush *Tail)
{
	grBSP_Brush	*Walk, *Next;

	for (Walk=List ; Walk ; Walk=Next)
	{	// Add to end of list
		Next = Walk->Next;
		Walk->Next = NULL;
		Tail->Next = Walk;
		Tail = Walk;
	}

	return Tail;
}

//=======================================================================================
//	grBSP_BrushSplit
//	Splits brush into 2 brushes.  The input brush is NOT freed!  
//	Back and/or Front CAN be NULL if the brush was not split, or tiny...
//=======================================================================================
grBoolean grBSP_BrushSplit(grBSP_Brush *Brush, grBSP *BSP, grPlaneArray_Index Index, grBSP_SideFlag MidFlags, grBSP_Brush **Front, grBSP_Brush **Back)
{
	grBSP_Brush		*b[2];
	int32			i, j;
	grPoly			*p, *MidPoly;
	grPlane			Plane;
	const grPlane	*pPlane;
	grBSP_Side		*cs, *pSide;
	float			d, FrontD, BackD;

	if (Brush->Flags & BSPBRUSH_FORCEBOTH)		// Force it down both sides
	{
		*Front = grBSP_BrushCreateFromBSPBrush(Brush);
		*Back = grBSP_BrushCreateFromBSPBrush(Brush);
		return GR_TRUE;
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Index);

	Plane = *pPlane;		// Copy the plane into something we can modify
	Plane.Type = Type_Any;

	if (grPlaneArray_IndexSided(Index))
		grPlane_Inverse(&Plane);

	*Front = *Back = NULL;

	// Check all points
	FrontD = BackD = 0.0f;

	for (i=0 ; i<Brush->NumSides ; i++)
	{
		grVec3d	*pVert;

		p = Brush->Sides[i].Poly;

		if (!p)
			continue;

		for (pVert = p->Verts, j=0 ; j<p->NumVerts ; j++, pVert++)
		{
		#if 1
			d = grPlane_PointDistanceFast(pPlane, pVert);

			if (grPlaneArray_IndexSided(Index))
				d = -d;
		#else
			d = grVec3d_DotProduct (pVert, &Plane.Normal) - Plane.Dist;
		#endif

			if (d > FrontD)
				FrontD = d;
			if (d < BackD)
				BackD = d;
		}
	}
	
	if (FrontD < 0.1f)			// Only on back
	{	
		*Back = grBSP_BrushCreateFromBSPBrush(Brush);
		return GR_TRUE;
	}

	if (BackD > -0.1f)			// Only on front
	{	
		*Front = grBSP_BrushCreateFromBSPBrush(Brush);
		return GR_TRUE;
	}

	// create a new poly from the split plane
	p = grPoly_CreateFromPlane(&Plane, GR_BSP_MINMAX_BOUNDS);

	if (!p)
		return GR_FALSE;
	
	// Clip the poly by all the planes of the brush being split
	for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides && p ; i++, pSide++)
	{
		const grPlane		*pPlane2;

		pPlane2 = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex);
		
		// Keep the back side
		if (!grPoly_ClipEpsilon(&p, 0.0f, pPlane2, !grPlaneArray_IndexSided(pSide->PlaneIndex)))
			return GR_FALSE;
	}

	if (!p || grPoly_IsTiny(p))
	{	
		// The brush isn't really split
		grPlane_Side		Side;

		//Log_Printf("Dropping poly...\n");

		Side = grBSP_BrushMostlyOnPlaneSide(Brush, &Plane);
		
		if (Side == PSIDE_FRONT)
			*Front = grBSP_BrushCreateFromBSPBrush(Brush);
		if (Side == PSIDE_BACK)
			*Back = grBSP_BrushCreateFromBSPBrush(Brush);

		if (!(*Front) && !(*Back))
			return GR_FALSE;

		return GR_TRUE;
	}

	// Split it for real
	MidPoly = p;					// Store the mid poly

	// Create 2 brushes
	for (i=0 ; i<2 ; i++)
	{
		b[i] = grBSP_BrushCreate(Brush->NumSides+1);
		
		if (!b[i])		
			return GR_FALSE;		// FIXME:  Free brushes

		// Reset sides, so we can fill it with exact number of sides, after split
		b[i]->NumSides = 0;	
		b[i]->Original = Brush->Original;		// Save original
	}

	// Split all the current polys of the brush being split, 
	// and distribute them to the other 2 brushes
	for (i=0 ; i<Brush->NumSides ; i++)
	{
		grBSP_Side		*pDestSide, *pSrcSide;
		grPoly			*pPolys[2];

		pSrcSide = &Brush->Sides[i];
		
		if (!pSrcSide->Poly)	// Don't copy sides that don't have polys
			continue;

		p = grPoly_CreateFromPoly(pSrcSide->Poly, GR_FALSE);		// Copy the poly
		
		if (!p)
			return GR_FALSE;		// FIXME:  Free stuff

		// Split the poly on the split plane
		if (!grPoly_SplitEpsilon(&p, 0.0f, &Plane, GR_FALSE, &pPolys[0], &pPolys[1]))
			return GR_FALSE;

		// Distribute the polys to the split brushes
		for (j=0 ; j<2 ; j++)
		{
			if (!pPolys[j])
				continue;
		#if 1
			if (grPoly_IsTiny(pPolys[j]))
			{
				grPoly_Destroy(&pPolys[j]);
				continue;
			}
		#endif
			
			pDestSide = &b[j]->Sides[b[j]->NumSides];
			b[j]->NumSides++;
			
			*pDestSide = *pSrcSide;					// Copy the side
			
			pDestSide->Poly = pPolys[j];
			pDestSide->Flags &= ~SIDE_TESTED;		// Remove the tested flag
		}
	}

	// See if we have valid polygons on both sides
	for (i=0 ; i<2 ; i++)
	{
		if (!grBSP_BrushCalcBounds(b[i]))
		{
			grErrorLog_AddString(-1, "grBSP_BrushSplit:  grBSP_BrushCalcBounds failed.", NULL);
			return GR_FALSE;
		}
		//assert(grBSP_BrushIsValidGeometry(b[i]) == GR_TRUE);
		
		if (!grBSP_BrushIsValidGeometry(b[i]))
		{
			grBSP_BrushDestroy(&b[i]);
			//return GR_FALSE;
		}
		
	}


	if (!(b[0] && b[1]) )		// If either brush got removed
	{
		if (!b[0] && !b[1])		// If both brushes got removed
			Log_Printf("grBSP_BrushSplit: Split removed brush\n");
		else
			Log_Printf("grBSP_BrushSplit: Split not on both sides\n");
		
		if (b[0])
		{
			grBSP_BrushDestroy(&b[0]);
			*Front = grBSP_BrushCreateFromBSPBrush(Brush);
		}
		if (b[1])
		{
			grBSP_BrushDestroy(&b[1]);
			*Back = grBSP_BrushCreateFromBSPBrush(Brush);
		}
		return GR_TRUE;
	}

	// Add the midpoly to both sides
	for (i=0 ; i<2 ; i++)
	{
		cs = &b[i]->Sides[b[i]->NumSides];
		b[i]->NumSides++;

		cs->PlaneIndex = Index;		// The MidPolys plane will be the splitplane index
		cs->Flags = MidFlags;		// Store the mid flags
		cs->Flags |= SIDE_SPLIT;	// Remember that this side was a result of a split

		if (!i)		// Reverse MidPoly on front side of split plane
		{
			cs->PlaneIndex = grPlaneArray_IndexReverse(cs->PlaneIndex);
			cs->Poly = grPoly_CreateFromPoly(MidPoly, GR_TRUE);
		}
		else
		{
			cs->Poly = MidPoly;
		}

		if (!cs->Poly)
			return GR_FALSE;
	}

	// Check the brushes for tiny volumes
	for (i=0 ; i<2 ; i++)
	{
		grFloat		Val;

		Val = grBSP_BrushVolume(b[i], BSP);

		if (Val < GR_BSP_TINY_VOLUME)
			grBSP_BrushDestroy(&b[i]);
	}
	
	if (!b[0] && !b[1])
		Log_Printf("grBSP_BrushSplit: Split removed brush (2)\n");
		//return GR_FALSE;		// The above code should surely take care of bad brushes
	
	*Front = b[0];
	*Back = b[1];

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BrushSubtract
//	Frees a+b, returns new brush.  New brush might be unique, a, b, or NULL.
//=======================================================================================
grBoolean grBSP_BrushSubtract(grBSP_Brush *a, grBSP_Brush *b, grBSP *BSP, grBSP_Brush **Result)
{	
	int32		i;
	grBSP_Brush	*Front, *Back;
	grBSP_Brush	*Out, *In;
	grBSP_Side	*Side;

	In = a;
	Out = NULL;

	Side = b->Sides;

	for (i=0 ; i<b->NumSides && In ; i++, Side++)
	{
		if (!grBSP_BrushSplit(In, BSP, Side->PlaneIndex, SIDE_NODE, &Front, &Back))
		{
			*Result = NULL;
			return GR_FALSE;
		}

		if (In != a)
			grBSP_BrushDestroy(&In);

		if (Front)				// Keep front side
		{	
			Front->Next = Out;
			Out = Front;
		}
		In = Back;				// Keep splitting back side
	}
	if (In)
		grBSP_BrushDestroy(&In);
	else
	{	// didn't really intersect
		grBSP_BrushDestroyList(&Out);
		*Result = a;
		return GR_TRUE;
	}

	*Result = Out;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BrushCSGList
//	Input is freed, only keeps output...
//=======================================================================================
grBSP_Brush *grBSP_BrushCSGList(grBSP_Brush *List, grBSP *BSP)
{
	grBSP_Brush	*b1, *b2, *Next;
	grBSP_Brush	*Tail;
	grBSP_Brush	*Keep;
	grBSP_Brush	*Sub, *Sub2;
	int32		c1, c2;

	Log_Printf("--- grBSP_BrushCSGList --- \n");
	Log_Printf("Num brushes before CSG : %5i\n", grBSP_BrushCountList(List));
	
	Keep = NULL;

	NewList:

	// Find tail
	if (!List)
		return NULL;

	for (Tail=List ; Tail->Next ; Tail=Tail->Next);

	for (b1=List ; b1 ; b1=Next)
	{
		Next = b1->Next;
		
		for (b2=b1->Next ; b2 ; b2 = b2->Next)
		{
			if (grBSP_BrushDisjoint(b1, b2))
				continue;

			Sub = NULL;
			Sub2 = NULL;
			c1 = 999999;
			c2 = 999999;

			if (grBSP_BrushCanBite(b2, b1) )
			{
				if (!grBSP_BrushSubtract(b1, b2, BSP, &Sub))
					return NULL;

				if (Sub == b1)
					continue;		// Didn't really intersect

				if (!Sub)
				{	
					// b1 is swallowed by b2
					List = grBSP_BrushCullList(b1, b1);
					goto NewList;
				}

				c1 = grBSP_BrushCountList(Sub);
			}

			if ( grBSP_BrushCanBite(b1, b2) )
			{
				if (!grBSP_BrushSubtract(b2, b1, BSP, &Sub2))
					return NULL;

				if (Sub2 == b2)
					continue;		// didn't really intersect

				if (!Sub2)
				{	
					// b2 is swallowed by b1
					grBSP_BrushDestroyList(&Sub);
					List = grBSP_BrushCullList (b1, b2);
					goto NewList;
				}
				c2 = grBSP_BrushCountList(Sub2);
			}

			if (!Sub && !Sub2)
				continue;		// neither one can bite

			// only accept if it didn't fragment
			// (commenting this out allows full fragmentation)
		#if 0
			if (c1 > 4 && c2 > 4)
			{
				if (Sub2)
					grBSP_BrushDestroyList(&Sub2);
				if (Sub)
					grBSP_BrushDestroyList(&Sub);

				continue;
			}
		#endif
			
			if (c1 < c2)
			{
				if (Sub2)
					grBSP_BrushDestroyList(&Sub2);
				Tail = grBSP_BrushListAddToTail(Sub, Tail);
				List = grBSP_BrushCullList(b1, b1);
				goto NewList;
			}
			else
			{
				if (Sub)
					grBSP_BrushDestroyList(&Sub);
				Tail = grBSP_BrushListAddToTail (Sub2, Tail);
				List = grBSP_BrushCullList(b1, b2);
				goto NewList;
			}
			
			
		}

		if (!b2)
		{	// b1 is no longer intersecting anything, so keep it
			b1->Next = Keep;
			Keep = b1;
		}
	}

	Log_Printf("Num brushes after CSG  : %5i\n", grBSP_BrushCountList(Keep));

	return Keep;
}

