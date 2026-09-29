/****************************************************************************************/
/*  TKARRAY.H																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Time-keyed array interface.											*/
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
#ifndef GR_TKARRAY_H
#define GR_TKARRAY_H
/* TKArray
	(Time-Keyed-Array)
	This module is designed primarily to support path.c

	The idea is that there are these packed arrays of elements,
	sorted by a grTKArray_TimeType key.  The key is assumed to be the 
	first field in each element.

	the TKArray functions operate on this very specific array type.

	Error conditions are reported to errorlog
	
	Michael Sandige

	01-28-98 [SLB]: style consistency changes, added grTKArray_CreateFromFile

*/

#include "BaseType.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef grFloat grTKArray_TimeType;

#define GR_TKA_TIME_TOLERANCE (0.00001f)

typedef struct grTKArray grTKArray;

grTKArray *GRCC grTKArray_Create(int ElementSize);
	// creates new array with given attributes

grTKArray *GRCC grTKArray_CreateEmpty(int ElementSize,int ElementCount);
	// creates new array with given element size and given count of uninitialized members

grTKArray* GRCC grTKArray_CreateFromFile(
	grVFile* pFile);					// stream positioned at array data
	// Creates a new array from the given stream.

grBoolean GRCC grTKArray_WriteToFile(
	const grTKArray* Array,			// sorted array to write
	grVFile* pFile);					// stream positioned for writing
	// Writes the array to the given stream.


int GRCC grTKArray_BSearch(
	const grTKArray *Array,			// sorted array to search
	grTKArray_TimeType Key);		// searching for this time
	// Searches for key in the Array. (assumes array is sorted) 
	// if key is found (within +-tolerance), the index to that element is returned.
	// if key is not found, the index to the key just smaller than the 
	// given key is returned.  (-1 if the key is smaller than the first element)
	// search is only accurate to 2*TKA_TIME_TOLERANCE.  
	// if multiple keys exist within 2*TKA_TIME_TOLERANCE, this will find an arbitrary one of them.

grBoolean GRCC grTKArray_Insert(
	grTKArray **Array,
	grTKArray_TimeType Key,			// time to insert
	int *Index);					// new element index
	// inserts a new element into Array.
	// sets only the key for the new element - the rest is junk
	// returns TRUE if the insertion was successful.
	// returns FALSE if the insertion failed. 
	// if Array is empty (no elements, NULL pointer) it is allocated and filled 
	// with the one Key element
	// Index is the index of the new element 

grBoolean GRCC grTKArray_DeleteElement(
	grTKArray **Array,
	int N);							// element to delete
	// deletes an element from Array.
	// returns TRUE if the deletion was successful. 
	// returns FALSE if the deletion failed. (key not found or realloc failed)

void GRCC grTKArray_Destroy(
	grTKArray **Array);	
	// destroys array

void *GRCC grTKArray_Element(
	const grTKArray *Array,
	int N);
	// returns a pointer to the Nth element of the array.

int GRCC grTKArray_NumElements(
	const grTKArray *Array);
	// returns the number of elements in the array

grTKArray_TimeType GRCC grTKArray_ElementTime(
	const grTKArray *Array, 
	int N);
	// returns the Time associated with the Nth element of the array

int GRCC grTKArray_ElementSize(
	const grTKArray *A);
	// returns the size of each element in the array

grBoolean GRCC grTKArray_SamplesAreTimeLinear(const grTKArray *Array,grFloat Tolerance);
	// returns true if the samples are linear in time within a tolerance

#ifdef __cplusplus
}
#endif



#endif
