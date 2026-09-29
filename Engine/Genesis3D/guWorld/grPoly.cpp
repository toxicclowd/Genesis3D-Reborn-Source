/****************************************************************************************/
/*  JEPOLY.C                                                                            */
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

// Public dependents
#include "grPoly.h"

// Private dependents
#include "Ram.h"
#include "Errorlog.h"

#define COLINEAR_EPSILON		0.0001f		// For checking if merged polys are still convex

//=======================================================================================
//	grPoly_Create
//=======================================================================================
grPoly *grPoly_Create(int32 NumVerts)
{
	grPoly	*Poly;

	assert(NumVerts < GR_POLY_MAX_VERTS);

	Poly = GR_RAM_ALLOCATE_STRUCT(grPoly);

	if (!Poly)
		return NULL;
		  
	Poly->Verts = GR_RAM_ALLOCATE_ARRAY(grPoly_VertType, NumVerts);

	if (!Poly->Verts)
	{
		grRam_Free(Poly);
		return NULL;
	}
	
	Poly->NumVerts = (grPoly_NumVertType)NumVerts;

#ifdef _DEBUG
	Poly->Self = Poly;
#endif

	return Poly;
}

//====================================================================================
//	grPoly_CreateFromPoly
//	Copy's a poly
//====================================================================================
grPoly *grPoly_CreateFromPoly(const grPoly *Poly, grBoolean Reverse)
{
	int32		i;

	grPoly	*NewPoly;

	NewPoly = grPoly_Create(Poly->NumVerts);

	if (!NewPoly)
		return NULL;

	if (Reverse)
	{
		for (i=0; i< Poly->NumVerts; i++)
			NewPoly->Verts[i] = Poly->Verts[Poly->NumVerts-i-1];
	}
	else
	{
		for (i=0; i< Poly->NumVerts; i++)
			NewPoly->Verts[i] = Poly->Verts[i];
	}

	return NewPoly;
}

//====================================================================================
//	grPoly_CreateFromPlane	
//	Create a huge poly with normal pointing in same direction as Plane
//====================================================================================
grPoly *grPoly_CreateFromPlane(const grPlane *Plane, float Scale)
{
	grVec3d		Normal;
	grVec3d		UpVect;
	grVec3d		RightVect, Start;
	grPoly		*Poly;
	
	Normal = Plane->Normal;

	// Get a good "Up" vector that is not pointing in the same direction as the plane
	if (!grPlane_GetAAVectors(Plane, &RightVect, &UpVect))
		return NULL;

	// Produce some walk vectors
	grVec3d_CrossProduct(&Normal, &UpVect, &RightVect);	// Get right vector
	grVec3d_CrossProduct(&Normal, &RightVect, &UpVect);	// Get new Up vector from correct Right vector

	grVec3d_Normalize(&UpVect);
	grVec3d_Normalize(&RightVect);

	// Create the poly with 4 verts
	Poly = grPoly_Create(4);

	if (!Poly)
		return NULL;

	// Get the start position, and scale the walk vectors
	grVec3d_Scale(&Normal, Plane->Dist, &Start);				
	grVec3d_Scale(&UpVect, Scale, &UpVect);				
	grVec3d_Scale(&RightVect, Scale, &RightVect);

	grVec3d_Subtract(&Start, &RightVect, &Poly->Verts[0]);
	grVec3d_Add(&Poly->Verts[0], &UpVect, &Poly->Verts[0]);

	grVec3d_Add(&Start, &RightVect, &Poly->Verts[1]);
	grVec3d_Add(&Poly->Verts[1], &UpVect, &Poly->Verts[1]);

	grVec3d_Add(&Start, &RightVect, &Poly->Verts[2]);
	grVec3d_Subtract(&Poly->Verts[2], &UpVect, &Poly->Verts[2]);

	grVec3d_Subtract(&Start, &RightVect, &Poly->Verts[3]);
	grVec3d_Subtract(&Poly->Verts[3], &UpVect, &Poly->Verts[3]);

	return Poly;
}

//=======================================================================================
//	grPoly_Destroy
//=======================================================================================
void grPoly_Destroy(grPoly **Poly)
{
	grPoly		*pPoly;

	assert(Poly);
	pPoly = *Poly;
	assert(pPoly);

	if (pPoly->Verts)
	{
		assert(pPoly->NumVerts > 0);
		grRam_Free(pPoly->Verts);
	}
	else
	{
		assert(pPoly->NumVerts == 0);
	}

	grRam_Free(pPoly);

	*Poly = NULL;
}

//====================================================================================
//	grPoly_Area
//====================================================================================
float grPoly_Area(const grPoly *Poly)
{
	int32		i;
	grVec3d		Vect1, Vect2, Cross;
	float		Total;

	Total = 0.0f;

	assert(Poly->NumVerts >= 3);
	assert(grPoly_IsTiny(Poly) == GR_FALSE);

	// Add up the areas of all the tris on the poly
	for (i=2 ; i<Poly->NumVerts ; i++)
	{
		grVec3d_Subtract(&Poly->Verts[i-1], &Poly->Verts[0], &Vect1);
		grVec3d_Subtract(&Poly->Verts[i]  , &Poly->Verts[0], &Vect2);

		//assert(grVec3d_Length(&Vect1) > 0.0001f);
		//assert(grVec3d_Length(&Vect2) > 0.0001f);

		grVec3d_CrossProduct (&Vect1, &Vect2, &Cross);
		Total += 0.5f * grVec3d_Length(&Cross);
	}

	return (float)Total;
}

#define MAX_TEMP_VERTS	128

grVec3d TempVerts[MAX_TEMP_VERTS];
grVec3d TempVerts2[MAX_TEMP_VERTS];

//====================================================================================
//	grPoly_ClipEpsilon
//	Poly will be NULL if clipped away.  Poly pointer will change if clipped...
//====================================================================================
grBoolean grPoly_ClipEpsilon(grPoly **Poly, float Epsilon, const grPlane *Plane, grBoolean FlipSide)
{
	grPoly	*NewPoly = NULL;
	grVec3d	Vert1, Vert2;
	grVec3d	*pFrontVert, *Verts;
	int32	i, NextVert, NewNumVerts = 0;
	float	Scale;
	grVec3d	Normal;
	float	Dist;
	int32	NumVerts;
	int32	VSides[MAX_TEMP_VERTS];
	float	VDist[MAX_TEMP_VERTS];
	int32	CountSides[3];
	grPoly	*pPoly;

	assert(Poly);
	assert(*Poly);
	assert(Plane);
	
	pPoly = *Poly;

	NumVerts = pPoly->NumVerts;
	assert(NumVerts < MAX_TEMP_VERTS);

	Normal = Plane->Normal;
	Dist = Plane->Dist;

	memset(CountSides, 0, sizeof(CountSides));

	Verts = pPoly->Verts;

	pFrontVert = TempVerts;
	
	if (FlipSide)		// Flip the normal and dist
	{	
		grVec3d_Inverse(&Normal);
		Dist = -Dist;
	}
	
	// See what side of plane each vert is on
	for (i=0; i< NumVerts; i++)
	{
		VDist[i] = grVec3d_DotProduct(&Verts[i], &Normal) - Dist;
		if (VDist[i] > Epsilon)
			VSides[i] = 0;
		else if (VDist[i] < -Epsilon)
			VSides[i] = 1;
		else
			VSides[i] = 2;

		CountSides[VSides[i]]++;
	}

	if (!CountSides[0])				// Nothing on the front
	{
		grPoly_Destroy(Poly);
		return GR_TRUE;
	}

	if (!CountSides[1])
	{
		// Does not need to split
		return GR_TRUE;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;

		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->X = Vert1.X + (Vert2.X - Vert1.X) * Scale;
		pFrontVert->Y = Vert1.Y + (Vert2.Y - Vert1.Y) * Scale;
		pFrontVert->Z = Vert1.Z + (Vert2.Z - Vert1.Z) * Scale;

		pFrontVert++;
	}

	grPoly_Destroy(Poly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)		// clipped away
		return GR_TRUE;
	
	NewPoly = grPoly_Create((grPoly_NumVertType)NewNumVerts);

	if (!NewPoly)
		return GR_FALSE;

	for (i = 0; i< NewNumVerts; i++)
		NewPoly->Verts[i] =	TempVerts[i];

	*Poly = NewPoly;

	return GR_TRUE;
}

//====================================================================================
//	grPoly_SplitEpsilon
//====================================================================================
grBoolean grPoly_SplitEpsilon(grPoly **InPoly, float Epsilon, const grPlane *Plane, grBoolean FlipSide, grPoly **Front, grPoly **Back)
{
	grPoly		*NewPoly = NULL;
	grVec3d		Vert1, Vert2;
	grVec3d		*pFrontVert, *pBackVert, *Verts;
	int32		i, NextVert, NewNumVerts = 0;
	float		Scale;
	grPoly		*pInPoly;

	grVec3d		Normal;
	float		Dist;//, *pDist;
	int32		NumVerts;
	int32		VSides[100];
	float		VDist[100];
	int32		CountSides[3];

	assert(InPoly);
	assert(*InPoly);
	assert(Plane);
	assert(Front && Back);

	pInPoly = *InPoly;

	NumVerts = pInPoly->NumVerts;
	assert(NumVerts < MAX_TEMP_VERTS);
	
	Normal = Plane->Normal;
	Dist = Plane->Dist;

	memset(CountSides, 0, sizeof(CountSides));

	Verts = pInPoly->Verts;

	pFrontVert = TempVerts;
	pBackVert = TempVerts2;
	
	// See what side of plane each vert is on
	if (Plane->Type < Type_AnyX)			
	{
		//pDist = grVec3d_GetElement(&Verts[0], Plane->Type);
		for (i=0; i< NumVerts ; i++)
		{
			Dist = grVec3d_GetElement(&Verts[i], Plane->Type);
			
			//VDist[i] = *pDist - Plane->Dist;
			VDist[i] = Dist - Plane->Dist;

			if (FlipSide)
				VDist[i] = -VDist[i];

			if (VDist[i] > Epsilon)
				VSides[i] = 0;
			else if (VDist[i] < -Epsilon)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;

			//pDist += 3;
		}
	}
	else
	{
		if (FlipSide)		// Flip the normal and dist
		{
			Normal.X = -Normal.X;
			Normal.Y = -Normal.Y;
			Normal.Z = -Normal.Z;
			Dist = -Dist;
		}

		for (i=0; i< NumVerts; i++)
		{
			VDist[i] = grVec3d_DotProduct(&Verts[i], &Normal) - Dist;
			if (VDist[i] > Epsilon)
				VSides[i] = 0;
			else if (VDist[i] < -Epsilon)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;
		}
	}

	// Get out quick, if no splitting is neccesary
	if (!CountSides[0])
	{
		*InPoly = NULL;
		*Front = NULL;
		*Back = pInPoly;
		return GR_TRUE;
	}
	
	if (!CountSides[1])
	{
		*InPoly = NULL;
		*Back = NULL;
		*Front = pInPoly;
		return GR_TRUE;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
            *pBackVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;
		else if (VSides[i] == 1)		// Back side, put on back list
            *pBackVert++ = Vert1;


		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->X = Vert1.X + (Vert2.X - Vert1.X) * Scale;
		pFrontVert->Y = Vert1.Y + (Vert2.Y - Vert1.Y) * Scale;
		pFrontVert->Z = Vert1.Z + (Vert2.Z - Vert1.Z) * Scale;

		*pBackVert = *pFrontVert;
		pFrontVert++;
		pBackVert++;
	}

	grPoly_Destroy(InPoly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)
		*Front = NULL;
	else
	{
		NewPoly = grPoly_Create((grPoly_NumVertType)NewNumVerts);

		if (!NewPoly)
			return GR_FALSE;

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] =	TempVerts[i];

		*Front = NewPoly;
	}
    
	NewNumVerts = pBackVert - TempVerts2;
	
	if (NewNumVerts < 3)
		*Back = NULL;
	else
	{
		NewPoly = grPoly_Create((grPoly_NumVertType)NewNumVerts);

		if (!NewPoly)
		{
			grPoly_Destroy(Front);
			return GR_FALSE;
		}

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] = TempVerts2[i];

		*Back = NewPoly;
	}

	return GR_TRUE;
}

#define EDGE_LENGTH		0.1f
//====================================================================================
//	grPoly_IsTiny
//====================================================================================
grBoolean grPoly_IsTiny (const grPoly *Poly)
{
	int32	i, j;
	float	Len;
	grVec3d	Delta;
	int32	Edges;

	Edges = 0;

	for (i=0 ; i<Poly->NumVerts ; i++)
	{
		j = i == Poly->NumVerts - 1 ? 0 : i+1;

		grVec3d_Subtract(&Poly->Verts[j], &Poly->Verts[i], &Delta);
		Len = grVec3d_Length (&Delta);

		if (Len > EDGE_LENGTH)
		{
			if (++Edges == 3)
				return GR_FALSE;
		}
	}

	return GR_TRUE;
}

//====================================================================================
//	grPoly_IsValid
//====================================================================================
grBoolean grPoly_IsValid(const grPoly *Poly)
{
	int32	i, j;

	if (!Poly)
		return GR_FALSE;

#ifdef _DEBUG
	if (Poly->Self != Poly)
		return GR_FALSE;
#endif

	for (i=0 ; i<Poly->NumVerts ; i++)
	{
		grVec3d	Delta;
		grFloat	Len;

		j = i == Poly->NumVerts - 1 ? 0 : i+1;
		
		grVec3d_Subtract(&Poly->Verts[j], &Poly->Verts[i], &Delta);
		Len = grVec3d_Length (&Delta);

		if (Len < EDGE_LENGTH)
			return GR_FALSE;
	}

	return GR_TRUE;
}

//====================================================================================
//	grPoly_EdgeExist
//====================================================================================
grBoolean grPoly_EdgeExist(const grPoly *Poly, const grVec3d *v1, const grVec3d *v2, int32 *i1, int32 *i2)
{
	int32	i;
	grVec3d	*Verts, *v3, *v4;
	int32	NumVerts;

	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	for (i=0; i< NumVerts; i++)
	{
		int32	Next;

		Next = (i+1)%NumVerts;

		v3 = &Verts[i];
		v4 = &Verts[Next];

		if (grVec3d_Compare(v1, v3, 0.05f) && grVec3d_Compare(v2, v4, 0.05f))
		{
			*i1 = i;
			*i2 = Next;
			return GR_TRUE;
		}
	}

	return GR_FALSE;
}

//====================================================================================
//	grPoly_Merge
//	Merges 2 polys.  Does NOT free originals.  Out will == NULL if polys can't merge
//====================================================================================
grBoolean grPoly_Merge(const grPoly *Poly1, const grPoly *Poly2, const grVec3d *Normal, grPoly **Out)
{
	grVec3d		*Verts1, *Verts2;
	int32		i, k, NumVerts1, NumVerts2, EdgeIndex[2], NumNewVerts;
	grPoly		*NewPoly;
	grVec3d		Vec1, Vec2;
	grFloat		Dot;
	grVec3d		*v1, *v2, Normal2;
	grBoolean	Keep1, Keep2;

	assert(Poly1);
	assert(Normal);
	assert(Poly2);
	assert(Out);

	assert(*Out != Poly1);		
	assert(*Out != Poly2);

	*Out = NULL;

	NewPoly = NULL;
	Keep1 = Keep2 = GR_TRUE;

	Verts1 = Poly1->Verts;
	NumVerts1 = Poly1->NumVerts;

	// Go through each edge of Poly1, and see if the reverse of it exist in Poly2
	for (i=0; i< NumVerts1; i++)		
	{
		v2 = &Verts1[i];
		v1 = &Verts1[(i+1)%NumVerts1];

		if (grPoly_EdgeExist(Poly2, v1, v2, &EdgeIndex[0], &EdgeIndex[1]))	
			break;		// Found one, break out
	}

	if (i >= NumVerts1)							// Did'nt find an edge, can't merge
		return GR_TRUE;

	Verts2 = Poly2->Verts;
	NumVerts2 = Poly2->NumVerts;

	// See if the 2 joined make a convex poly, connect them, and return new one
	Vec1 = Verts1[(i+NumVerts1-1)%NumVerts1];	// Get the normal of the edge just behind edge1
	grVec3d_Subtract(&Vec1, v2, &Vec1);
	grVec3d_CrossProduct(Normal, &Vec1, &Normal2);
	grVec3d_Normalize(&Normal2);
	grVec3d_Subtract(&Verts2[(EdgeIndex[1]+1)%NumVerts2], &Verts2[EdgeIndex[1]], &Vec2);
	Dot = grVec3d_DotProduct(&Vec2, &Normal2);
	if (Dot > COLINEAR_EPSILON)
		return GR_TRUE;							// Edge makes a non-convex poly
	if (Dot >=-COLINEAR_EPSILON)				// Drop point, on colinear edge
		Keep1 = GR_FALSE;

	Vec1 = Verts1[(i+2)%NumVerts1];				// Get the normal of the edge just behind edge1
	grVec3d_Subtract(&Vec1, v1, &Vec1);
	grVec3d_CrossProduct(Normal, &Vec1, &Normal2);
	grVec3d_Normalize(&Normal2);
	grVec3d_Subtract(&Verts2[(EdgeIndex[0]+NumVerts2-1)%NumVerts2], &Verts2[EdgeIndex[0]], &Vec2);
	Dot = grVec3d_DotProduct(&Vec2, &Normal2);
	if (Dot > COLINEAR_EPSILON)
		return GR_TRUE;							// Edge makes a non-convex poly
	if (Dot >=-COLINEAR_EPSILON)				// Drop point, on colinear edge
		Keep2 = GR_FALSE;

	NewPoly = grPoly_Create(NumVerts1+NumVerts2);

	if (!NewPoly)
		return GR_FALSE;		// This is an error!!!

	// Make a new poly...
	NumNewVerts = 0;

	for (k = (i+1)%NumVerts1; k != i ; k = (k+1)%NumVerts1)
	{
		if (k == (i+1)%NumVerts1 && ! Keep2)
			continue;
		NewPoly->Verts[NumNewVerts++] = Verts1[k];
	}

	i = EdgeIndex[0];

	for (k = (i+1)%NumVerts2; k != i ; k = (k+1)%NumVerts2)
	{
		if (k == (i+1)%NumVerts2 && ! Keep1)
			continue;
		NewPoly->Verts[NumNewVerts++] = Verts2[k];
	}

	NewPoly->NumVerts = (grPoly_NumVertType)NumNewVerts;

	*Out = NewPoly;		// Give them the new poly

	return GR_TRUE;
}

//====================================================================================
//	grPoly_RemoveDegenerateEdges
//====================================================================================
grBoolean grPoly_RemoveDegenerateEdges(grPoly *Poly, grFloat Epsilon)
{
	int32		i, NumVerts;
	grVec3d		*Verts, *V1, *V2, NVerts[1024], Vec;
	grBoolean	Bad;
	int32		NumNVerts;

	assert(Poly);
	assert(Poly->NumVerts < 1024);

	NumNVerts = 0;
	Bad = GR_FALSE;

	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	for (i=0; i< NumVerts; i++)
	{
		V1 = &Verts[i];
		V2 = &Verts[(i+1)%NumVerts];

		grVec3d_Subtract(V1, V2, &Vec);

		if (grVec3d_Length(&Vec) > Epsilon)
		{
			NVerts[NumNVerts++] = *V1;	
		}
		else
			Bad = GR_TRUE;
	}

	if (Bad)
	{
		Poly->NumVerts = (grPoly_NumVertType)NumNVerts;
		for (i=0; i< NumNVerts; i++)
		{
			Verts[i] = NVerts[i];
		}
	}

	return GR_TRUE;
}
