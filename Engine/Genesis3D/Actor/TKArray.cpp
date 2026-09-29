/****************************************************************************************/
/*  TKARRAY.C																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Time-keyed array implementation.										*/
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
/*TKArray
	(Time-Keyed-Array)
	This module is designed primarily to support path.c

	The idea is that there are these packed arrays of elements,
	sorted by a grTKArray_TimeType key.  The key is assumed to be the 
	first field in each element.

	the grTKArray functions operate on this very specific array type.

	Error conditions are reported to errorlog

	Michael Sandige
*/

#include <assert.h>
#include <stddef.h> // offsetof
#include <string.h>

#include "TKArray.h"
#include "Errorlog.h"
#include "Ram.h"

typedef struct grTKArray
{
	int32 NumElements;		// number of elements in use
	int32 ElementSize;		// size of each element
	char Elements[1];		// array elements.  This list will be expanded by changing
							// the allocated size of the entire grTKArray object
}	grTKArray;

typedef struct 
{
	int32 NumElements;		// number of elements in use
	int32 ElementSize;		// size of each element
} grTKArray_FileHeader;


#define TK_MAX_ARRAY_LENGTH (0x7FFFFFFF)  // NumElements is (signed) 32 bit int


#define TK_ARRAYSIZE (offsetof(grTKArray, Elements))	// gets rid of the extra element char in the def.

// General validity test.
// Use TK_ASSERT_VALID to test array for reasonable data.
#ifdef _DEBUG

#define TK_ASSERT_VALID(A) grTKArray_Asserts(A)

// Do not call this function directly.  Use TK_ASSERT_VALID
static void GRCC grTKArray_Asserts(const grTKArray* A)
{
	assert( (A) != NULL );
	assert( ((A)->NumElements == 0) ||
			(((A)->NumElements > 0) && ((A)->Elements != NULL)) );
	assert( (A)->NumElements >= 0 );
	assert( (A)->NumElements <= TK_MAX_ARRAY_LENGTH );
	assert( (A)->ElementSize > 0 );
}

#else // !_DEBUG

#define TK_ASSERT_VALID(A) ((void)0)

#endif // _DEBUG


grTKArray *GRCC grTKArray_Create(				
	int ElementSize)				// element size
	// Creates new array with given attributes.  The first field of the element
	// is assumed to be the grTKArray_TimeType key.
{
	grTKArray *A;

	// first item in each element must be the time key
	assert( ElementSize >= sizeof(grTKArray_TimeType) );

	A = (grTKArray *)grRam_AllocateClear(TK_ARRAYSIZE);
	if ( A == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKArray_Create.");
		return NULL;
	}

	A->ElementSize = ElementSize;
	A->NumElements = 0;

	TK_ASSERT_VALID(A);

	return A;	
}

grTKArray *GRCC grTKArray_CreateEmpty(				
	int ElementSize,int ElementCount)				// element size
	// Creates new array with given size and count.  The first field of the element
	// is assumed to be the grTKArray_TimeType key.
{
	grTKArray *A;
	int32 size = TK_ARRAYSIZE + ElementCount * ElementSize;
	A = (grTKArray*)grRam_AllocateClear(size);
	if( A == NULL )
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grTKArray_CreateEmpty.");
		return NULL;
	}
	A->ElementSize = ElementSize;
	A->NumElements = ElementCount;

	TK_ASSERT_VALID(A);

	return A;	
}

grTKArray* GRCC grTKArray_CreateFromFile(
	grVFile* pFile)					// stream positioned at array data
	// Creates a new array from the given stream.
{
	int32 size;
	grTKArray* A;
	grTKArray_FileHeader Header;

	if (grVFile_Read(pFile, &Header, sizeof(grTKArray_FileHeader)) == GR_FALSE)
	{
		grErrorLog_Add(GR_ERR_FILEIO_READ,"grTKArray_CreateFromFile: Failed to read header");
		return NULL;
	}

	size = TK_ARRAYSIZE + Header.NumElements * Header.ElementSize;
	A = (grTKArray*)grRam_AllocateClear(size);
	if( A == NULL )
	{
		grErrorLog_Add(GR_ERR_FILEIO_READ,"grTKArray_CreateFromFile.");
		return NULL;
	}


	if(grVFile_Read(pFile, A->Elements, size - sizeof(grTKArray_FileHeader)) == GR_FALSE)
		{
			grRam_Free(A);
			grErrorLog_Add(GR_ERR_FILEIO_READ,"grTKArray_CreateFromFile.");
			return NULL;
		}

	A->NumElements = Header.NumElements;
	A->ElementSize = Header.ElementSize;

	return A;
}


grBoolean GRCC grTKArray_SamplesAreTimeLinear(const grTKArray *Array,grFloat Tolerance)
{
	int i;

	grTKArray_TimeType Delta,Nth,LastNth,NthDelta;
			
	if (Array->NumElements < 2)
		return GR_TRUE;

	LastNth = grTKArray_ElementTime(Array, 0);
	Nth     = grTKArray_ElementTime(Array, 1);
	Delta   =  Nth - LastNth;
	LastNth = Nth;
	
	for (i=2; i< Array->NumElements; i++)
		{
			Nth = grTKArray_ElementTime(Array, i);
			NthDelta = (Nth-LastNth)-Delta;
			if (NthDelta<0.0f) NthDelta = -NthDelta;
			if (NthDelta>Tolerance)
				{
					return GR_FALSE;
				}
			LastNth = Nth;
		}
	return GR_TRUE;
}

grBoolean GRCC grTKArray_WriteToFile(
	const grTKArray* Array,			// sorted array to write
	grVFile* pFile)					// stream positioned for writing
	// Writes the array to the given stream.
{
	int size;
	
	size = TK_ARRAYSIZE + Array->NumElements * Array->ElementSize;
	if(grVFile_Write(pFile, Array, size) == GR_FALSE)
	{
		grErrorLog_Add(GR_ERR_FILEIO_WRITE,"grTKArray_WriteToFile.");
		return GR_FALSE;
	}
	return GR_TRUE;
}

void GRCC grTKArray_Destroy(grTKArray **PA)
	// destroys array
{
	assert( PA  != NULL );
	TK_ASSERT_VALID(*PA);

	grRam_Free(*PA);
	*PA = NULL;
}


int GRCC grTKArray_BSearch(
	const grTKArray *A,				// sorted array to search
	grTKArray_TimeType Key)			// searching for this key
	// Searches for key in the Array.   A is assumed to be sorted
	// if key is found (within +-tolarance), the index to that element is returned.
	// if key is not found, the index to the key just smaller than the 
	// given key is returned.  (-1 if the key is smaller than the first element)
{
	int low,hi,mid;
	int ElementSize;
	const char *Array;
	grTKArray_TimeType test;

	TK_ASSERT_VALID(A);
	
	low = 0;
	hi = A->NumElements - 1;
	Array = A->Elements;
	ElementSize = A->ElementSize;
	
	while ( low<=hi )
		{
			mid = (low+hi)/2;
			test = *(grTKArray_TimeType *)(Array + mid*ElementSize);
			if ( Key > test )
				{
					low = mid+1;
				}
			else
				{
					if ( Key < test )
						{
							hi = mid-1;
						}
					else
						{
							return mid;
						}
				}
		}
	return hi;
}


grBoolean GRCC grTKArray_Insert(
	grTKArray **PtrA,				// sorted array to insert into
	grTKArray_TimeType Key,			// key to insert
	int *Index)						// new element index
	// inserts a new element into Array.
	// sets only the key for the new element - the rest is junk
	// returns GR_TRUE if the insertion was successful.
	// returns GR_FALSE if the insertion failed. 
	// if Array is empty (no elements, NULL pointer) it is allocated and filled 
	// with the one Key element
	// Index is the index of the new element 
{
	int n;
	grTKArray *ChangedA;
	grTKArray *A;
	grTKArray_TimeType Found;

	assert( PtrA );
	A = *PtrA;
	TK_ASSERT_VALID(A);

	n = grTKArray_BSearch(A,Key);
	// n is the element just prior to the location of the new element

	if(Index)
		*Index = n+1;

	if (n >= 0)
	{
		Found =  *(grTKArray_TimeType *)(A->Elements + (n * (A->ElementSize)) );
		// Found <= Key  (within +-GR_TKA_TIME_TOLERANCE)
		if (Found > Key - GR_TKA_TIME_TOLERANCE)
		{	// if Found==Key, bail.  Can't have two identical keys.
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grTKArray_Insert: Identical keys not allowed.");
			return GR_FALSE;
		}
	}

	if (A->NumElements >= TK_MAX_ARRAY_LENGTH)
	{
		grErrorLog_Add(GR_ERR_LIST_FULL, "grTKArray_Insert: Too many keys.");
		return GR_FALSE;
	}

	ChangedA = (grTKArray *)grRam_Realloc(A, 
				TK_ARRAYSIZE + (A->NumElements + 1) * A->ElementSize);

	if ( ChangedA == NULL )
	{	
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKArray_Insert.");
		return GR_FALSE;
	}
	A = ChangedA;

	// advance n to new element's position
	n++;
	
	// move elements as necessary
	if(n < A->NumElements)
	{
		memmove( A->Elements + (n + 1) * A->ElementSize,	// dest
				 A->Elements + n * A->ElementSize,			// src
				 (A->NumElements - n) * A->ElementSize);	// count
	}

	*(grTKArray_TimeType *)((A->Elements) + ((n) * (A->ElementSize)) ) = Key;
	A->NumElements++;
	*PtrA = A;

	return GR_TRUE;
}


grBoolean GRCC grTKArray_DeleteElement(
	grTKArray **PtrA,				// sorted array to delete from
	int N)							// element to delete
	// deletes an element from Array.
	// returns GR_TRUE if the deletion was successful. 
	// returns GR_FALSE if the deletion failed. (key not found or realloc failed)
{
	grTKArray *A;
	grTKArray *ChangedA;
	
	assert( PtrA != NULL);
	A = *PtrA;
	TK_ASSERT_VALID(A);
	assert(N >= 0);
	assert(N < A->NumElements);
	
	memmove( (A->Elements) + (N) * (A->ElementSize),  //dest
			 (A->Elements) + (N+1) * (A->ElementSize),  //src
			 ((A->NumElements) - (N+1))* (A->ElementSize) );

	A->NumElements--;
	ChangedA = (grTKArray *)grRam_Realloc(A, 
				TK_ARRAYSIZE + A->NumElements * A->ElementSize);
	if ( ChangedA != NULL ) 
	{	
		// if realloc fails to shrink block. no real error.
		A = ChangedA;
	}

	*PtrA = A;

	return GR_TRUE;
}


void *GRCC grTKArray_Element(const grTKArray *A, int N)
	// returns the Nth element 
{
	TK_ASSERT_VALID(A);
	assert(N >= 0);
	assert(N < A->NumElements);

	return (void *)( (A->Elements) + (N * (A->ElementSize)) );
}


grTKArray_TimeType GRCC grTKArray_ElementTime(const grTKArray *A, int N)
	// returns the time key for the Nth element 
{
	TK_ASSERT_VALID(A);
	assert(N >= 0);
	assert(N < A->NumElements);
	
	return *(grTKArray_TimeType *)((A->Elements) + (N * (A->ElementSize)) );
}


int GRCC grTKArray_NumElements(const grTKArray *A)
	// returns the number of elements in the array
{
	TK_ASSERT_VALID(A);
	return A->NumElements;
}


int GRCC grTKArray_ElementSize(const grTKArray *A)
	// returns the size of each element in the array
{
	TK_ASSERT_VALID(A);
	return A->ElementSize;
}
