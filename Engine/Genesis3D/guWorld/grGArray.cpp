/****************************************************************************************/
/*  JEGARRAY.C                                                                          */
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
#include "grGArray.h"

// Private dependents
#include "Ram.h"
#include "Errorlog.h"

//=======================================================================================
//=======================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define GR_GARRAY_MAX_REF_COUNT		65535

//=======================================================================================
//=======================================================================================
typedef struct grGArray
{
	uint16				RefCount;

	grGArray_Index		ActiveElements;		// Total active elements
	grGArray_Index		MaxElements;		// Element array size (in elements)
	uint16				ElementSize;
	grGArray_Index		LastIndex;			//
	
	uint8				*Elements;
	grGArray_RefType	*RefCounts;

#ifdef _DEBUG
	grGArray			*Self;
#endif

} grGArray;

static grBoolean grGArray_Extend(grGArray *Array);

//=======================================================================================
//	grGArray_Create
//=======================================================================================
grGArray *grGArray_Create(int32 StartElements, int32 ElementSize)
{
	grGArray	*Array;

	assert(StartElements < GR_GARRAY_MAX_ELEMENTS);		// This must be true
	assert(ElementSize < GR_GARRAY_MAX_ELEMENT_SIZE);

	Array = GR_RAM_ALLOCATE_STRUCT(grGArray);

	if (!Array)	// Assume not enough ram
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_Create:  Out of ram for array.", NULL);
		return NULL;
	}

	// Clear mem for array
	ZeroMem(Array);

	// Now, create the elements
	Array->Elements = GR_RAM_ALLOCATE_ARRAY(uint8, StartElements*ElementSize);

	if (!Array->Elements)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_Create:  Out of ram for elements.", NULL);
		goto ExitWithError;
	}

	// Clear the elements in this vert Link
	ZeroMemArray(Array->Elements, StartElements*ElementSize);

	// Create the element ref counts
	Array->RefCounts = GR_RAM_ALLOCATE_ARRAY(grGArray_RefType, StartElements);

	if (!Array->RefCounts)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_Create:  Out of ram for ref counts.", NULL);
		goto ExitWithError;
	}

	ZeroMemArray(Array->RefCounts, StartElements);

	// Store the number of verts that the first link in the Link has
	Array->MaxElements = (grGArray_Index)StartElements;
	Array->ElementSize = (grGArray_Index)ElementSize;
	Array->ActiveElements = 0;
	Array->LastIndex = 0;

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
			if (Array->Elements)
				grRam_Free(Array->Elements);
			if (Array->RefCounts)
				grRam_Free(Array->RefCounts);
			grRam_Free(Array);
		}

		return NULL;
	}
}

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
		((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define GR_GARRAY_TAG			MAKEFOURCC('G', 'E', 'G', 'A')		// 'GE' 'G'eneric 'A'rray
#define GR_GARRAY_VERSION		0

//=======================================================================================
//	grGArray_CreateFromFile
//=======================================================================================
grGArray *grGArray_CreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr)
{
	uint32		Tag;
	uint16		Version;
	grGArray	*Array;

	assert(VFile);

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Array))
			return NULL;

		if (Array)
		{
			if (!grGArray_CreateRef(Array))
				return NULL;

			return Array;		// Ptr found in stack, return it
		}
	}

	// Read header info
	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return NULL;

	if (Tag != GR_GARRAY_TAG)
		return NULL;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return NULL;

	if (Version != GR_GARRAY_VERSION)
		return NULL;

	// Read array
	Array = GR_RAM_ALLOCATE_STRUCT(grGArray);

	if (!Array)	// Assume not enough ram
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_CreateFromFile:  Out of ram for array.", NULL);
		return NULL;
	}

	// Clear mem for array
	ZeroMem(Array);

	// Read array size
	if (!grVFile_Read(VFile, &Array->MaxElements, sizeof(Array->MaxElements)))
		return NULL;

	// Read num active elements
	if (!grVFile_Read(VFile, &Array->ActiveElements, sizeof(Array->ActiveElements)))
		return NULL;

	// Read size of each element
	if (!grVFile_Read(VFile, &Array->ElementSize, sizeof(Array->ElementSize)))
		return NULL;

	// Allocate arrays
	Array->Elements = GR_RAM_ALLOCATE_ARRAY(uint8, Array->MaxElements*Array->ElementSize);

	if (!Array->Elements)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_CreateFromFile:  Out of ram for elements.", NULL);
		goto ExitWithError;
	}

	// Allocate ref count array
	Array->RefCounts = GR_RAM_ALLOCATE_ARRAY(grGArray_RefType, Array->MaxElements);

	if (!Array->RefCounts)
	{
		grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE, "grGArray_CreateFromFile:  Out of ram for ref counts.", NULL);
		goto ExitWithError;
	}

	// Load arrays
	// load refcount array
	if (!grVFile_Read(VFile, Array->RefCounts, sizeof(Array->RefCounts[0])*Array->MaxElements))
		return NULL;

	// Load element array
	if (IOFunc)
	{
		int32		i;

		for (i=0; i< Array->MaxElements; i++)
		{
			if (!Array->RefCounts[i])
				continue;

			if (!IOFunc(VFile, &Array->Elements[i*Array->ElementSize], IOFuncContext))
				goto ExitWithError;
		}
	}
	else
	{
		if (!grVFile_Read(VFile, Array->Elements, Array->ElementSize*Array->MaxElements))
			goto ExitWithError;
	}

	// Zero out the RefCount array because no one should be reffing any of the elements yet
	ZeroMemArray(Array->RefCounts, Array->MaxElements);

	Array->LastIndex = 0;

	Array->RefCount = 1;

#ifdef _DEBUG
	Array->Self = Array;
#endif
   
	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Array))
			goto ExitWithError;
	}

	return Array;			// Done

	// Error
	ExitWithError:
	{
		if (Array)
		{
			if (Array->Elements)
				grRam_Free(Array->Elements);
			if (Array->RefCounts)
				grRam_Free(Array->RefCounts);
			grRam_Free(Array);
		}

		return NULL;
	}
}


//=======================================================================================
//	grGArray_WriteToFile
//=======================================================================================
grBoolean grGArray_WriteToFile(const grGArray *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr)
{
	uint32		Tag;
	uint16		Version;

	assert(Array);
	assert(VFile);

	if (PtrMgr)
	{
		uint32		Count;

		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)Array, &Count))
			return GR_FALSE;

		if (Count)
			return GR_TRUE;		// Ptr was on stack, so return
	}

	// Write TAG
	Tag = GR_GARRAY_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_GARRAY_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	// Write out array size
	if (!grVFile_Write(VFile, &Array->MaxElements, sizeof(Array->MaxElements)))
		return GR_FALSE;

	// Write out num active elements
	if (!grVFile_Write(VFile, &Array->ActiveElements, sizeof(Array->ActiveElements)))
		return GR_FALSE;

	// Write out size of each element
	if (!grVFile_Write(VFile, &Array->ElementSize, sizeof(Array->ElementSize)))
		return GR_FALSE;

	// Write out ref count array
	if (!grVFile_Write(VFile, Array->RefCounts, sizeof(Array->RefCounts[0])*Array->MaxElements))
		return GR_FALSE;

	// Write out element array
	if (IOFunc)
	{
		int32		i;

		for (i=0; i< Array->MaxElements; i++)
		{
			if (!Array->RefCounts[i])
				continue;

			if (!IOFunc(VFile, &Array->Elements[i*Array->ElementSize], IOFuncContext))
				return GR_FALSE;
		}
	}
	else
	{
		if (!grVFile_Write(VFile, Array->Elements, Array->ElementSize*Array->MaxElements))
			return GR_FALSE;
	}

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)Array))
			return GR_FALSE;
	}

	return GR_TRUE;
}


//=======================================================================================
//	grGArray_CreateRef
//=======================================================================================
grBoolean grGArray_CreateRef(grGArray *Array)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Array->RefCount >= 0);

	Array->RefCount++;

	if (Array->RefCount >= GR_GARRAY_MAX_REF_COUNT)
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grGArray_Destroy
//=======================================================================================
void grGArray_Destroy(grGArray **Array)
{
	assert(Array);
	assert(grGArray_IsValid(*Array) == GR_TRUE);
	assert((*Array)->RefCount > 0);

	(*Array)->RefCount--;

	if ((*Array)->RefCount == 0)		// Don't destroy until refcount == 0
	{
		if ((*Array)->Elements)				// Free the elements
		{
			assert((*Array)->MaxElements > 0);
			assert((*Array)->RefCounts);

			#ifdef _DEBUG		
			{
				int32		i;
	
				for (i=0; i< (*Array)->MaxElements; i++)
				{
					assert((*Array)->RefCounts[i] == 0);
				}
			}
			#endif

			grRam_Free((*Array)->Elements);
			grRam_Free((*Array)->RefCounts);
		}
		else
		{
			assert((*Array)->MaxElements == 0);
			assert(!(*Array)->RefCounts);
		}

		grRam_Free(*Array);				// Finally, free the Array itself
	}

	*Array = NULL;
}

//=======================================================================================
//	grGArray_IsValid
//=======================================================================================
grBoolean grGArray_IsValid(const grGArray *Array)
{
	if (!Array)
		return GR_FALSE;

	if (Array->RefCount <= 0)
		return GR_FALSE;

#ifdef _DEBUG
	if (Array->Self != Array)
		return GR_FALSE;
#endif
	return GR_TRUE;
}

//=======================================================================================
//	grGArray_Extend
//=======================================================================================
static grBoolean grGArray_Extend(grGArray *Array)
{
	int32				NewSize;
	grGArray_RefType	*Refs;

	NewSize = Array->MaxElements<<1;

	if (NewSize > GR_GARRAY_MAX_ELEMENTS)
	{
		NewSize = GR_GARRAY_MAX_ELEMENTS;

		assert(NewSize > Array->MaxElements);	// Make sure it grows past original size!

		if (NewSize <= Array->MaxElements)		// Make sure it grows past original size!
			return GR_FALSE;			// Out of index space
	}
	
	Array->Elements = (uint8 *)grRam_Realloc(Array->Elements, NewSize*Array->ElementSize);

	if (!Array->Elements)
		return GR_FALSE;

	Array->RefCounts = (grGArray_RefType *)grRam_Realloc(Array->RefCounts, NewSize*sizeof(grGArray_RefType));

	if (!Array->RefCounts)
	{
		grRam_Free(Array->Elements);
		return GR_FALSE;
	}

	Refs = &Array->RefCounts[Array->MaxElements];

	// Clear new memory allocated in the refs section...
	memset(Refs, 0, sizeof(grGArray_RefType)*(NewSize-Array->MaxElements));

	Array->MaxElements = (grGArray_Index)NewSize;	// Get the new number of max elements

	return GR_TRUE;
}

//=======================================================================================
//	grGArray_AddElement
//=======================================================================================
grGArray_Index grGArray_AddElement(grGArray *Array, const grGArray_Element *Element)
{
	int32	i2;

	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Element);

	if (Array->ActiveElements >= Array->MaxElements)
	{
		i2 = Array->MaxElements;

		if (!grGArray_Extend(Array))
			return GR_GARRAY_NULL_INDEX;
	}
	else 
	{
		grGArray_Index			i;

		i2 = Array->LastIndex;

		for (i=0; i<Array->MaxElements; i++, i2++)
		{
			if (i2 == Array->MaxElements)				// Wrap
				i2 = 0;

			if (!Array->RefCounts[i2])
				break;
		}
	}

	assert(Array->RefCounts[i2] == 0);

	Array->RefCounts[i2]++;
	Array->ActiveElements++;

	memcpy(&Array->Elements[i2*Array->ElementSize], Element, Array->ElementSize);

	Array->LastIndex = (grGArray_Index)(i2+1);

	return (grGArray_Index)i2;
}

//=======================================================================================
//	grGArray_RefElement
//=======================================================================================
grBoolean grGArray_RefElement(grGArray *Array, grGArray_Index Index)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_GARRAY_NULL_INDEX);
	assert(Array->ActiveElements > 0);
	assert(Array->RefCounts[Index] >= 0);
	assert(Array->RefCounts[Index] < GR_GARRAY_MAX_ELEMENT_REFCOUNT);

	Array->RefCounts[Index]++;

	return GR_TRUE;
}

//=======================================================================================
//	grGArray_RemoveElement
//=======================================================================================
void grGArray_RemoveElement(grGArray *Array, grGArray_Index *Index)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(*Index != GR_GARRAY_NULL_INDEX);
	assert(Array->ActiveElements > 0);
	assert(Array->RefCounts[*Index] > 0);

	Array->RefCounts[*Index]--;

	if (Array->RefCounts[*Index] == 0)
		Array->ActiveElements--;

	*Index = GR_GARRAY_NULL_INDEX;	// They should not use this element index again
}

//=======================================================================================
//	grGArray_GetSize
//=======================================================================================
int32 grGArray_GetSize(const grGArray *Array)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);

	return (int32)Array->MaxElements;
}

//=======================================================================================
//	grGArray_GetElements
//=======================================================================================
grGArray_Element *grGArray_GetElements(const grGArray *Array)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	return Array->Elements;
}

//=======================================================================================
//	grGArray_GetRefCounts
//=======================================================================================
grGArray_RefType *grGArray_GetRefCounts(const grGArray *Array)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	return Array->RefCounts;
}

//=======================================================================================
//	grGArray_GetElementRefCountByIndex
//=======================================================================================
const grGArray_RefType grGArray_GetElementRefCountByIndex(const grGArray *Array, grGArray_Index Index)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Index >= 0 && Index < Array->MaxElements);
	assert(Index != GR_GARRAY_NULL_INDEX);
	assert(Array->RefCounts[Index] >= 0);

	return Array->RefCounts[Index];
}
/*
//=======================================================================================
//	grGArray_GetNextElement
//=======================================================================================
grGArray_Element *grGArray_GetNextElement(grGarray *Array, grGArray_Element *Start)
{
	assert(Array);
	assert(Start);

	if (!Start)
		Start = Array->Elements;

	Index = Array->Elements

	while (
}
*/
//=======================================================================================
//	grGArray_SetElementByIndex
//=======================================================================================
void grGArray_SetElementByIndex(grGArray *Array, grGArray_Index Index, const grGArray_Element *Element)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_GARRAY_NULL_INDEX);
	assert(Index >= 0 && Index < Array->MaxElements);
	assert(Array->RefCounts[Index] > 0);

	memcpy(&Array->Elements[Index*Array->ElementSize], Element, Array->ElementSize);
}

//=======================================================================================
//	grGArray_GetElementByIndex
//=======================================================================================
const grGArray_Element *grGArray_GetElementByIndex(const grGArray *Array, grGArray_Index Index)
{
	assert(grGArray_IsValid(Array) == GR_TRUE);
	assert(Index != GR_GARRAY_NULL_INDEX);
	assert(Index >= 0 && Index < Array->MaxElements);
	assert(Array->RefCounts[Index] > 0);

	return &Array->Elements[Index*Array->ElementSize];
}

