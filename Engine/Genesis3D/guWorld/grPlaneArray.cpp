/****************************************************************************************/
/*  JEPLANEARRAY.C                                                                      */
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
#include <math.h>
#include <memory.h>

// Public Dependents
#include "grPlaneArray.h"

// Private Dependents
#include "Ram.h"
#include "Errorlog.h"

#define COMPOSE_INDEX(i, s) (((grPlaneArray_Index)i<<1)|(grPlaneArray_Index)s)
#define PLANENUM_FROM_INDEX(i) (i>>1)
#define PLANESIDE_FROM_INDEX(i) (i&1)

#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define GR_PLANEARRAY_MAX_REFCOUNT			65535
#define GR_PLANEARRAY_MAX_PLANE_REFCOUNT	65535

//=======================================================================================
//=======================================================================================
typedef struct grPlaneArray_Plane
{
	uint16					RefCount;		// Max of 65535 shared refs!!!
	grPlane					Plane;
} grPlaneArray_Plane;

typedef struct grPlaneArray
{
	uint16				RefCount;

	grPlaneArray_Index	ActivePlanes;		// Total active Planes
	grPlaneArray_Index	MaxPlanes;			// Plane array size
	grPlaneArray_Plane	*Planes;			// Array of Planes

	grPlaneArray_Plane	*LastPlane;			//

#ifdef _DEBUG
	grPlaneArray		*Self;
#endif
} grPlaneArray;

static grPlaneArray_Plane *grPlaneArray_Extend(grPlaneArray *Array);

//=======================================================================================
//	grPlaneArray_Create
//=======================================================================================
grPlaneArray *grPlaneArray_Create(int32 StartPlanes)
{
	grPlaneArray		*Array;

	assert(StartPlanes < GR_PLANEARRAY_MAX_PLANES);		// This must be true

	Array = GR_RAM_ALLOCATE_STRUCT(grPlaneArray);

	if (! Array)	// Assume not enough ram
		return NULL;

	// Clear mem for array
	ZeroMem(Array);

	// Now, create the Planes
	Array->Planes = GR_RAM_ALLOCATE_ARRAY(grPlaneArray_Plane, StartPlanes);

	if (!Array->Planes)
		goto ExitWithError;

	// Clear the verts in this vert Link
	ZeroMemArray(Array->Planes, StartPlanes);

	// Store the number of verts that the first link in the Link has
	Array->MaxPlanes = (grPlaneArray_Index)StartPlanes;
	Array->ActivePlanes = 0;
	Array->LastPlane = Array->Planes;

	Array->RefCount = 1;

#ifdef _DEBUG
	Array->Self = Array;
#endif
   
	return Array;			// Done

	// Error
	ExitWithError:
	{
		if (Array)
		{
			if (Array->Planes)
				grRam_Free(Array->Planes);
			grRam_Free(Array);
		}

		return NULL;
	}
}

//=======================================================================================
//	grPlaneArray_Destroy
//=======================================================================================
void grPlaneArray_Destroy(grPlaneArray **Array)
{
	assert(Array);
	assert(*Array);
	assert((*Array)->RefCount > 0);

	(*Array)->RefCount--;

	if ((*Array)->RefCount == 0)			// Don't destroy until refcount == 0
	{
		if ((*Array)->Planes)				// Free the planes
		{
			assert((*Array)->MaxPlanes > 0);
			grRam_Free((*Array)->Planes);
		}
		else
		{
			assert((*Array)->MaxPlanes == 0);
		}
	
		grRam_Free(*Array);				// Finally, free the VArray itself
	}

	*Array = NULL;
}

//======================================================================================
//	grPlaneArray_IsValid
//=======================================================================================
grBoolean grPlaneArray_IsValid(const grPlaneArray *Array)
{
	if (!Array)
		return GR_FALSE;

#ifdef _DEBUG
	if (Array->Self != Array)
		return GR_FALSE;
#endif
	return GR_TRUE;
}

//=======================================================================================
//	grPlaneArray_Extend
//=======================================================================================
grPlaneArray_Plane *grPlaneArray_Extend(grPlaneArray *Array)
{
	int32				NewSize;
	grPlaneArray_Plane	*v;

	NewSize = Array->MaxPlanes<<1;

	if (NewSize > GR_PLANEARRAY_MAX_PLANES)
	{
		NewSize = GR_PLANEARRAY_MAX_PLANES;

		if (NewSize <= (int32)Array->MaxPlanes)	// Make sure it grows past original size!
		{
			assert(0);							// No more verts available
			return NULL;
		}
	}
	
	Array->Planes = (grPlaneArray_Plane *)grRam_Realloc(Array->Planes, NewSize*sizeof(grPlaneArray_Plane));

	assert(Array->Planes);
	if (!Array->Planes)
		return NULL;

	v = &Array->Planes[Array->MaxPlanes];				// Get the first vert in the new space

	// Clear new memory allocated...
	memset(v, 0, sizeof(grPlaneArray_Plane)*(NewSize-Array->MaxPlanes));

	Array->MaxPlanes = (grPlaneArray_Index)NewSize;	// Get the new number of max verts

	return v;										// Return the first vert in the new space
}

#define	DIST_EPSILON		0.01f
#define	ANGLE_EPSILON		0.00001f
#define NORMAL_EPSILON		0.00001f

//====================================================================================
//	SnapVector
//====================================================================================
static void SnapVector(grVec3d *Normal)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if ( fabs(grVec3d_GetElement(Normal,i) - 1.0f) < ANGLE_EPSILON )
		{
			grVec3d_Clear(Normal);
			grVec3d_SetElement(Normal,i, 1.0f);
			break;
		}

		if ( fabs(grVec3d_GetElement(Normal,i) - -1.0f) < ANGLE_EPSILON )
		{
			grVec3d_Clear(Normal);
			grVec3d_SetElement(Normal,i, -1.0f);
			break;
		}
	}
}

//====================================================================================
//	Rint (round up int)
//====================================================================================
static float RInt(float In)
{
	return (float)floor(In + 0.5f);
}

//====================================================================================
//	SnapPlane
//====================================================================================
static void SnapPlane(grPlane *Plane)
{
	SnapVector(&Plane->Normal);

	if (fabs(Plane->Dist-RInt(Plane->Dist)) < DIST_EPSILON)
		Plane->Dist = RInt(Plane->Dist);
}

//====================================================================================
//	SidePlane
//====================================================================================
static grBoolean SidePlane(grPlane *Plane)
{
	grPlane_Type	Type;

	Plane->Type = grPlane_TypeFromUnitVector(&Plane->Normal);

	// Get the index into the major axis
	Type = (grPlane_Type)((int)Plane->Type % 3);

	// If the major axis is negated, flip plane, so the dominent axis is positive
	if (grVec3d_GetElement(&Plane->Normal, Type) < 0.0f)
	{
		grPlane_Inverse(Plane);
		return GR_TRUE;				// Plane was sided
	}

	return GR_FALSE;
}

//=======================================================================================
//	grPlaneArray_AddPlane
//=======================================================================================
static grPlaneArray_Index grPlaneArray_AddPlane(grPlaneArray *Array, const grPlane *Plane)
{
	grPlaneArray_Plane	*p;

	assert(grPlaneArray_IsValid(Array) == GR_TRUE);
	assert(Plane);

	if (Array->ActivePlanes >= Array->MaxPlanes)
	{
		p = grPlaneArray_Extend(Array);

		if (!p)
			return GR_PLANEARRAY_NULL_INDEX;
	}
	else 
	{
		int32				i;
		grPlaneArray_Plane	*PEnd;

		PEnd = &Array->Planes[Array->MaxPlanes];

		for (p = Array->LastPlane, i=0; i<(int32)Array->MaxPlanes; i++, p++)
		{
			if (p == PEnd)				// Wrap
				p = Array->Planes;

			if (!p->RefCount)
				break;
		}
	}

	assert(p->RefCount == 0);

	p->Plane = *Plane;
	p->RefCount++;

	Array->ActivePlanes++;

	Array->LastPlane = p;
	Array->LastPlane++;

	return (grPlaneArray_Index)((p-Array->Planes));
}

//=======================================================================================
//	grPlaneArray_SharePlane
//=======================================================================================
grPlaneArray_Index grPlaneArray_SharePlane(grPlaneArray *Array, const grPlane *Plane)
{
	grPlaneArray_Plane	*pPlane;
	grPlane				Plane2;
	int32				i;
	grBoolean			Sided;
	grPlaneArray_Index	Index;


	Plane2 = *Plane;	// Preserve plane

	SnapPlane(&Plane2);
	
	// Flip plane if needed...
	if (SidePlane(&Plane2))
		Sided = 1;
	else
		Sided = 0;

	for (pPlane = Array->Planes, i=0; i< (int32)Array->MaxPlanes; i++, pPlane++)
	{
		if (pPlane->RefCount == 0)		// Planes must share!!!!
			continue;

		if (grPlane_Compare(&pPlane->Plane, &Plane2, NORMAL_EPSILON, DIST_EPSILON))
		{
			assert(pPlane->RefCount < GR_PLANEARRAY_MAX_PLANE_REFCOUNT);
			pPlane->RefCount++;
			return COMPOSE_INDEX(i, Sided);		// Got it...
		}
	}

	Index = grPlaneArray_AddPlane(Array, &Plane2);

	return COMPOSE_INDEX(Index, Sided);
}

//=======================================================================================
//	grPlaneArray_RefPlaneByIndex
//=======================================================================================
grBoolean grPlaneArray_RefPlaneByIndex(grPlaneArray *Array, grPlaneArray_Index Index)
{
	grPlaneArray_Plane	*Plane2;

	assert(grPlaneArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_PLANEARRAY_NULL_INDEX);

	Index = PLANENUM_FROM_INDEX(Index);

	assert(Index >= 0 && Index < GR_PLANEARRAY_MAX_PLANES);
	assert(Index >= 0 && Index < Array->MaxPlanes);

	Plane2 = &Array->Planes[Index];	
	assert(Plane2->RefCount > 0);
	assert(Plane2->RefCount < GR_PLANEARRAY_MAX_PLANE_REFCOUNT);

	if (Plane2->RefCount >= GR_PLANEARRAY_MAX_PLANE_REFCOUNT)
		return GR_FALSE;

	Plane2->RefCount++;

	return GR_TRUE;
}

//=======================================================================================
//	grPlaneArray_RemovePlane
//=======================================================================================
void grPlaneArray_RemovePlane(grPlaneArray *Array, grPlaneArray_Index *Index)
{
	grPlaneArray_Plane	*Plane;

	assert(grPlaneArray_IsValid(Array) == GR_TRUE);
	assert(*Index != GR_PLANEARRAY_NULL_INDEX);
	assert(Array->ActivePlanes > 0);

	Plane = &Array->Planes[PLANENUM_FROM_INDEX(*Index)];

	assert(Plane->RefCount > 0);

	Plane->RefCount--;

	if (Plane->RefCount == 0)
		Array->ActivePlanes--;

	*Index = GR_PLANEARRAY_NULL_INDEX;		// They should not use this plane again
}

//=======================================================================================
//	grPlaneArray_GetPlaneByIndex
//=======================================================================================
const grPlane * PLANE_CC grPlaneArray_GetPlaneByIndex(const grPlaneArray *Array, grPlaneArray_Index Index)
{
	grPlaneArray_Plane	*Plane2;

	assert(grPlaneArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_PLANEARRAY_NULL_INDEX);

	Index = PLANENUM_FROM_INDEX(Index);

	assert(Index >= 0 && Index < GR_PLANEARRAY_MAX_PLANES);
	assert(Index >= 0 && Index < Array->MaxPlanes);

	Plane2 = &Array->Planes[Index];	
	assert(Plane2->RefCount > 0);

	return &Plane2->Plane;
}
