/****************************************************************************************/
/*  JEVERTARRAY.C                                                                       */
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
#include "grVertArray.h"

// Private dependents
#include "Ram.h"
#include "Errorlog.h"

//=======================================================================================
//=======================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define VERTARRAY_MAX_REFCOUNT			(65535)
#define VERTARRAY_MAX_VERT_REFCOUNT		(255)

#define VCOMPARE_EPSILON				(0.01f)

//=======================================================================================
//=======================================================================================
typedef struct grVertArray_Vert
{
	uint8					RefCount;	// Max of 255 shared verts in any one spot!!!
	grVec3d					Vert;
} grVertArray_Vert;

typedef struct grVertArray
{
	uint16				RefCount;

	grVertArray_Index	ActiveVerts;		// Total active verts
	grVertArray_Index	MaxVerts;			// Vert array size
	grVertArray_Vert	*Verts;				// Array of verts

	grVertArray_Vert	*LastVert;			//

#ifdef _DEBUG
	grVertArray			*Self;
#endif
} grVertArray;

static grVertArray_Vert *grVertArray_Extend(grVertArray *Array);

//=======================================================================================
//	grVertArray_Create
//=======================================================================================
GRAPI grVertArray * GRCC grVertArray_Create(int32 StartVerts)
{
	grVertArray		*VArray;

	assert(StartVerts < GR_VERTARRAY_MAX_VERTS);		// This must be true

	VArray = GR_RAM_ALLOCATE_STRUCT(grVertArray);

	if (!VArray)	// Assume not enough ram
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grVertArray_Create:  Out of ram for varray.", NULL);
		return NULL;
	}

	// Clear mem for varray
	ZeroMem(VArray);

	// Now, create the verts
	VArray->Verts = GR_RAM_ALLOCATE_ARRAY(grVertArray_Vert, StartVerts);

	if (!VArray->Verts)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grVertArray_Create:  Out of ram for verts.", NULL);
		goto ExitWithError;
	}

	// Clear the verts in this vert Link
	ZeroMemArray(VArray->Verts, StartVerts);

	// Store the number of verts that the first link in the Link has
	VArray->MaxVerts = (grVertArray_Index)StartVerts;
	VArray->ActiveVerts = 0;
	VArray->LastVert = VArray->Verts;

	VArray->RefCount = 1;

#ifdef _DEBUG
	VArray->Self = VArray;
#endif
   
	return VArray;			// Done

	// Error
	ExitWithError:
	{
		if (VArray)
		{
			if (VArray->Verts)
				grRam_Free(VArray->Verts);
			grRam_Free(VArray);
		}

		return NULL;
	}
}

//=======================================================================================
//	grVertArray_CreateFromFile
//=======================================================================================
GRAPI grVertArray * GRCC grVertArray_CreateFromFile(grVFile *VFile)
{
	grVertArray		*Array;

	assert(VFile);

	Array = GR_RAM_ALLOCATE_STRUCT(grVertArray);

	if (!Array)	// Assume not enough ram
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grVertArray_CreateFromFile:  Out of ram for varray.", NULL);
		return NULL;
	}

	// Clear mem for varray
	ZeroMem(Array);

	// Read in array size
	if (!grVFile_Read(VFile, &Array->MaxVerts, sizeof(Array->MaxVerts)))
		return NULL;
	
	// Read in number of active elements
	if (!grVFile_Read(VFile, &Array->ActiveVerts, sizeof(Array->MaxVerts)))
		return NULL;

	// Now, create the vert array off the array size
	Array->Verts = GR_RAM_ALLOCATE_ARRAY(grVertArray_Vert, Array->MaxVerts);

	if (!Array->Verts)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grVertArray_CreateFromFile:  Out of ram for verts.", NULL);
		goto ExitWithError;
	}

	// Clear the verts in this vert Link
	ZeroMemArray(Array->Verts, Array->MaxVerts);

	// Read in the array
	if (!grVFile_Read(VFile, Array->Verts, sizeof(Array->Verts[0])*Array->MaxVerts))
		return NULL;

	// Store the number of verts that the first link in the Link has
	Array->LastVert = Array->Verts;

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
			if (Array->Verts)
				grRam_Free(Array->Verts);
			grRam_Free(Array);
		}

		return NULL;
	}
}

//=======================================================================================
//	grVertArray_WriteToFile
//=======================================================================================
GRAPI grBoolean GRCC grVertArray_WriteToFile(const grVertArray *Array, grVFile *VFile)
{
	assert(Array);
	assert(VFile);

	// Write out total size of the ENTIRE array
	if(!grVFile_Write(VFile, &Array->MaxVerts, sizeof(Array->MaxVerts)))
		return GR_FALSE;

	// Write out number of active elements in the array
	if(!grVFile_Write(VFile, &Array->ActiveVerts, sizeof(Array->ActiveVerts)))
		return GR_FALSE;

	// Write out the array it self
	if (!grVFile_Write(VFile, Array->Verts, sizeof(Array->Verts[0])*Array->MaxVerts))
		return GR_FALSE;
	
	return GR_TRUE;
}

//=======================================================================================
//	grVertArray_Destroy
//=======================================================================================
GRAPI void GRCC grVertArray_Destroy(grVertArray **VArray)
{
	assert(VArray);
	assert(*VArray);
	assert((*VArray)->RefCount > 0);

	(*VArray)->RefCount--;

	if ((*VArray)->RefCount == 0)		// Don't destroy until refcount == 0
	{
		if ((*VArray)->Verts)				// Free the verts 
		{
			assert((*VArray)->MaxVerts > 0);
			grRam_Free((*VArray)->Verts);
		}
		else
		{
			assert((*VArray)->MaxVerts == 0);
		}

		grRam_Free(*VArray);				// Finally, free the VArray itself
	}

	*VArray = NULL;
}

//=======================================================================================
//	grVertArray_IsValid
//=======================================================================================
GRAPI grBoolean GRCC grVertArray_IsValid(const grVertArray *VArray)
{
	if (!VArray)
		return GR_FALSE;

#ifdef _DEBUG
	if (VArray->Self != VArray)
		return GR_FALSE;
#endif
	return GR_TRUE;
}

//=======================================================================================
//	grVertArray_Extend
//=======================================================================================
grVertArray_Vert * grVertArray_Extend(grVertArray *Array)
{
	uint32				NewSize;
	grVertArray_Vert	*v;

	if (Array->MaxVerts >= GR_VERTARRAY_MAX_VERTS)
		return NULL;		// No more space left	

	// At this point, there should be enough space to extend by at least one element
	NewSize = Array->MaxVerts<<1;

	if (NewSize <= Array->MaxVerts)		
		NewSize = GR_VERTARRAY_MAX_VERTS;		// Must have wrapped
	
	if (NewSize > GR_VERTARRAY_MAX_VERTS)		
		NewSize = GR_VERTARRAY_MAX_VERTS;	

	assert(NewSize > Array->MaxVerts);

	Array->Verts = (grVertArray_Vert *)grRam_Realloc(Array->Verts, NewSize*sizeof(grVertArray_Vert));

	assert(Array->Verts);
	if (!Array->Verts)
		return NULL;

	v = &Array->Verts[Array->MaxVerts];				// Get the first vert in the new space

	// Clear new memory allocated...
	memset(v, 0, sizeof(grVertArray_Vert)*(NewSize-Array->MaxVerts));

	Array->MaxVerts = (grVertArray_Index)NewSize;	// Get the new number of max verts

	return v;										// Return the first vert in the new space
}

//=======================================================================================
//	grVertArray_AddVert
//=======================================================================================
GRAPI grVertArray_Index GRCC grVertArray_AddVert(grVertArray *Array, const grVec3d *Vert)
{
	grVertArray_Vert	*v;

	assert(grVertArray_IsValid(Array) == GR_TRUE);
	assert(Vert);

	if (Array->ActiveVerts >= Array->MaxVerts)
	{
		v = grVertArray_Extend(Array);

		if (!v)
			return GR_VERTARRAY_NULL_INDEX;
	}
	else 
	{
		int32				i;
		grVertArray_Vert	*VEnd;

		VEnd = &Array->Verts[Array->MaxVerts];

		for (v = Array->LastVert, i=0; i<Array->MaxVerts; i++, v++)
		{
			if (v == VEnd)				// Wrap
				v = Array->Verts;

			if (!v->RefCount)
				break;
		}
	}

	assert(v->RefCount == 0);

	v->Vert = *Vert;
	v->RefCount++;

	Array->ActiveVerts++;

	Array->LastVert = v;
	Array->LastVert++;

	return (grVertArray_Index)(v-Array->Verts);
}

//=======================================================================================
//	grVertArray_ShareVert
//=======================================================================================
GRAPI grVertArray_Index GRCC grVertArray_ShareVert(grVertArray *Array, const grVec3d *Vert)
{
	grVertArray_Vert	*pVert;
	int32				i;

	for (pVert = Array->Verts, i=0; i< Array->MaxVerts; i++, pVert++)
	{
		if (pVert->RefCount == 0 || pVert->RefCount >= VERTARRAY_MAX_VERT_REFCOUNT)
			continue;

		if (grVec3d_Compare(&pVert->Vert, Vert, VCOMPARE_EPSILON))
		{
			assert(pVert->RefCount < VERTARRAY_MAX_VERT_REFCOUNT);
			pVert->RefCount++;
			return (grVertArray_Index)i;
		}
	}

	return grVertArray_AddVert(Array, Vert);
}

//=======================================================================================
//	grVertArray_RemoveVert
//=======================================================================================
GRAPI void GRCC grVertArray_RemoveVert(grVertArray *Array, grVertArray_Index *Index)
{
	grVertArray_Vert	*Vert;

	assert(grVertArray_IsValid(Array) == GR_TRUE);
	assert(*Index != GR_VERTARRAY_NULL_INDEX);
	assert(Array->ActiveVerts > 0);

	Vert = &Array->Verts[*Index];

	assert(Vert->RefCount > 0);

	Vert->RefCount--;

	if (Vert->RefCount == 0)
	{
		// Decement the global number of active verts in this array
		Array->ActiveVerts--;
	}

	*Index = GR_VERTARRAY_NULL_INDEX;		// They should not use this vert again
}

//=======================================================================================
//	grVertArray_RemoveVert
//=======================================================================================
GRAPI grBoolean GRCC grVertArray_RefVertByIndex(grVertArray *Array, grVertArray_Index Index)
{
	grVertArray_Vert	*Vert;

	assert(grVertArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_VERTARRAY_NULL_INDEX);
	assert(Array->ActiveVerts > 0);

	Vert = &Array->Verts[Index];

	assert(Vert->RefCount > 0);
	assert(Vert->RefCount < VERTARRAY_MAX_VERT_REFCOUNT);
	
	if (Vert->RefCount >= VERTARRAY_MAX_VERT_REFCOUNT)
		return GR_FALSE;

	Vert->RefCount++;

	return GR_TRUE;
}

//=======================================================================================
//	grVertArray_SetVertByIndex
//=======================================================================================
GRAPI void GRCC grVertArray_SetVertByIndex(grVertArray *VArray, grVertArray_Index Index, const grVec3d *Vert)
{
	grVertArray_Vert	*Vert2;

	assert(grVertArray_IsValid(VArray) == GR_TRUE);
	assert(Index >= 0 && Index < VArray->MaxVerts);
	assert(Index != GR_VERTARRAY_NULL_INDEX);

	Vert2 = &VArray->Verts[Index];
	assert(Vert2->RefCount > 0);

	Vert2->Vert = *Vert;
}

//=======================================================================================
//	grVertArray_GetVertByIndex
//=======================================================================================
GRAPI const grVec3d * GRCC grVertArray_GetVertByIndex(const grVertArray *VArray, grVertArray_Index Index)
{
	grVertArray_Vert	*Vert2;

	assert(grVertArray_IsValid(VArray) == GR_TRUE);
	assert(Index >= 0 && Index < VArray->MaxVerts);
	assert(Index != GR_VERTARRAY_NULL_INDEX);

	Vert2 = &VArray->Verts[Index];	
	assert(Vert2->RefCount > 0);

	return &Vert2->Vert;
}

//=======================================================================================
//	grVertArray_GetMaxIndex
//=======================================================================================
GRAPI int16 GRCC grVertArray_GetMaxIndex( const grVertArray *VArray )
{
	assert(grVertArray_IsValid(VArray) == GR_TRUE);
	return( VArray->ActiveVerts );
}

// Each X,Y,Z Element cannot be no more than +- HASH_SIZE2 (16384)

#define	HASH_SIZE		128							// Must be power of 2
#define	HASH_SIZE2		HASH_SIZE*HASH_SIZE			// Squared(HASH_SIZE)
#define HASH_SHIFT		8							// Log2(HASH_SIZE)+1

#define HASH_ELEMENT(x)	((HASH_SIZE2 + (int32)((x) + 0.5f)) >> HASH_SHIFT)

typedef struct grVertArray_Optimizer
{	
	grVertArray_Index	*VertexChain;		
	grVertArray_Index	HashVerts[HASH_SIZE2];			// A vertex number, or GR_VERTARRAY_NULL_INDEX for no verts

	grVertArray_Index	OptimizedIndexListSize;
	grVertArray_Index	*OptimizedIndexList;
} grVertArray_Optimizer;

//=====================================================================================
//	GetHashKeyFromVert
//=====================================================================================
static int32 GetHashKeyFromVert(grVec3d *Vert)
{
	int32	x, y;

	x = HASH_ELEMENT(Vert->X);
	y = HASH_ELEMENT(Vert->Z);

	assert (!( x < 0 || x >= HASH_SIZE || y < 0 || y >= HASH_SIZE ));
	
	return y*HASH_SIZE + x;
}

#define INTEGRAL_EPSILON	(0.01f)

//=====================================================================================
//	WeldVert
//=====================================================================================
static grVertArray_Index WeldVert(grVertArray *Array, grVertArray_Optimizer *Optimizer, grVertArray_Vert *Verts, int32 *NumVerts, grVec3d *Vert)
{
	grVertArray_Index		i;
	int32					h;

	// Snap the vert
	for (h=0; h<3; h++)
	{
		int32	FVert;

		FVert = (int32)(grVec3d_GetElement(Vert, h)+0.5f);

		if (fabs(grVec3d_GetElement(Vert, h) - FVert) < INTEGRAL_EPSILON)
			grVec3d_SetElement(Vert, h, (grFloat)FVert);
	}

	// Get the HashKey
	h = GetHashKeyFromVert(Vert);

	// Search through all the verts in this chain for a match
	for (i=Optimizer->HashVerts[h]; i != GR_VERTARRAY_NULL_INDEX; i = Optimizer->VertexChain[i])
	{
		assert(i <= (*NumVerts));

		if (grVec3d_Compare(Vert, &Verts[i].Vert, VCOMPARE_EPSILON))
		{
			assert(Verts[i].RefCount > 0 && Verts[i].RefCount < VERTARRAY_MAX_VERT_REFCOUNT);
			Verts[i].RefCount++;

			return i;
		}
	}

	// No match, add to tail

	assert((*NumVerts) < Array->ActiveVerts);

	Verts[(*NumVerts)].Vert = *Vert;
	assert(Verts[(*NumVerts)].RefCount == 0);
	Verts[(*NumVerts)].RefCount = 1;

	Optimizer->VertexChain[(*NumVerts)] = Optimizer->HashVerts[h];
	Optimizer->HashVerts[h] = (grVertArray_Index)(*NumVerts);

	(*NumVerts)++;

	return (grVertArray_Index)(*NumVerts)-1;
}

//=======================================================================================
//	BuildOptimizedIndexList
//=======================================================================================
static grBoolean BuildOptimizedIndexList(grVertArray *Array, grVertArray_Optimizer *Optimizer)
{
	grVertArray_Vert	*OptimizedVerts, *pVert;
	int32				NumVerts, i;
	
	// Build a new vert array, where everything vert is welded
	OptimizedVerts = GR_RAM_ALLOCATE_ARRAY(grVertArray_Vert, Array->ActiveVerts);

	if (!OptimizedVerts)
		return GR_FALSE;

	ZeroMemArray(OptimizedVerts, Array->ActiveVerts);

	NumVerts = 0;

	for (pVert = Array->Verts, i=0; i< Array->MaxVerts; i++, pVert++)
	{
		if (!pVert->RefCount)
			continue;		// Vert is not active, skip it

		Optimizer->OptimizedIndexList[i] = WeldVert(Array, Optimizer, OptimizedVerts, &NumVerts, &pVert->Vert);

		if (Optimizer->OptimizedIndexList[i] == GR_VERTARRAY_NULL_INDEX)
		{
			grRam_Free(OptimizedVerts);
			return GR_FALSE;
		}
	}

	// Free old vert array
	grRam_Free(Array->Verts);

	if (NumVerts < Array->ActiveVerts)
	{
		// Shrink the new array if it is smaller then the original # of verts
		//	(This is probably the case since we have welded them together...)
		OptimizedVerts = (grVertArray_Vert *)grRam_Realloc(OptimizedVerts, NumVerts*sizeof(grVertArray_Vert));
		Array->ActiveVerts = (grVertArray_Index)NumVerts;
	}

	// Assign new array to the welded optimized verts
	Array->Verts = OptimizedVerts;

	Array->MaxVerts = Array->ActiveVerts;
	Array->LastVert = Array->Verts;

	return GR_TRUE;
}

//=======================================================================================
//	grVertArray_CreateOptimizer
//	An Optimizer welds all the verts together in an array REALLY fast using a hash table
//		NOTE - Once you create an optimizer from an array, you MUST convert your indexes
//		over using the grVertArray_GetOptimizedIndex API function call before using the array again...
//=======================================================================================
GRAPI grVertArray_Optimizer * GRCC grVertArray_CreateOptimizer(grVertArray *Array)
{
	grVertArray_Optimizer	*Optimizer;
	int32					i;

	assert(grVertArray_IsValid(Array) == GR_TRUE);

	Optimizer = GR_RAM_ALLOCATE_STRUCT(grVertArray_Optimizer);

	if (!Optimizer)
		return NULL;
	
	ZeroMem(Optimizer);
	
	Optimizer->VertexChain = GR_RAM_ALLOCATE_ARRAY(grVertArray_Index, Array->MaxVerts);

	if (!Optimizer->VertexChain)
		goto ExitWithError;

	Optimizer->OptimizedIndexList = GR_RAM_ALLOCATE_ARRAY(grVertArray_Index, Array->MaxVerts);

	if (!Optimizer->OptimizedIndexList)
		goto ExitWithError;

	Optimizer->OptimizedIndexListSize = Array->MaxVerts;

	// Set the defaults
	for (i=0; i< Array->MaxVerts; i++)
	{
		Optimizer->VertexChain[i] = GR_VERTARRAY_NULL_INDEX;
		Optimizer->OptimizedIndexList[i] = GR_VERTARRAY_NULL_INDEX;
	}

	for (i=0; i< HASH_SIZE2; i++)
		Optimizer->HashVerts[i] = GR_VERTARRAY_NULL_INDEX;

	if (!BuildOptimizedIndexList(Array, Optimizer))
		goto ExitWithError;

	// At this point on, they MUST use the Optimizer to convert their indexes 
	//	over before they can use the new array

	return Optimizer;

	ExitWithError:
	{
		if (Optimizer)
		{
			if (Optimizer->VertexChain)
				grRam_Free(Optimizer->VertexChain);

			if (Optimizer->OptimizedIndexList)
				grRam_Free(Optimizer->OptimizedIndexList);

			grRam_Free(Optimizer);
		}
		return NULL;
	}
}

//=======================================================================================
//	grVertArray_DestroyOptimizer
//=======================================================================================
GRAPI void GRCC grVertArray_DestroyOptimizer(grVertArray *Array, grVertArray_Optimizer **Optimizer)
{
	assert(grVertArray_IsValid(Array) == GR_TRUE);
	assert(Optimizer);
	assert(*Optimizer);

	if ((*Optimizer)->VertexChain)
		grRam_Free((*Optimizer)->VertexChain);

	if ((*Optimizer)->OptimizedIndexList)
		grRam_Free((*Optimizer)->OptimizedIndexList);

	grRam_Free(*Optimizer);
	*Optimizer = NULL;
}

//=======================================================================================
//	grVertArray_GetOptimizedIndex
//=======================================================================================
GRAPI grVertArray_Index GRCC grVertArray_GetOptimizedIndex(grVertArray *Array, grVertArray_Optimizer *Optimizer, grVertArray_Index Index)
{
	assert(grVertArray_IsValid(Array) == GR_TRUE);
	assert(Optimizer);
	assert(Index != GR_VERTARRAY_NULL_INDEX);
	assert(Index >= 0 && Index < Optimizer->OptimizedIndexListSize);
	
	return Optimizer->OptimizedIndexList[Index];
}

//=======================================================================================
//	grVertArray_GetEdgeVerts
//=======================================================================================
GRAPI grBoolean GRCC grVertArray_GetEdgeVerts(grVertArray_Optimizer *Optimizer, const grVec3d *v1, const grVec3d *v2, grVertArray_Index *EdgeVerts, int32 *NumEdgeVerts, int32 MaxEdgeVerts)
{
	int32				x1, y1, x2, y2;
	int32				t, x, y;

	x1 = HASH_ELEMENT(v1->X);
	y1 = HASH_ELEMENT(v1->Z);

	x2 = HASH_ELEMENT(v2->X);
	y2 = HASH_ELEMENT(v2->Z);

	if (x1 > x2)
	{
		t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2)
	{
		t = y1;
		y1 = y2;
		y2 = t;
	}

	(*NumEdgeVerts) = 0;

	for (x=x1 ; x <= x2 ; x++)
	{
		for (y=y1 ; y <= y2 ; y++)
		{
			grVertArray_Index	Index;

			for (Index = Optimizer->HashVerts[y*HASH_SIZE+x] ; Index != GR_VERTARRAY_NULL_INDEX ; Index = Optimizer->VertexChain[Index])
			{
				if ((*NumEdgeVerts) > MaxEdgeVerts)
					return GR_FALSE;

				EdgeVerts[(*NumEdgeVerts)++] = Index;
			}
		}
	}

	return GR_TRUE;
}
