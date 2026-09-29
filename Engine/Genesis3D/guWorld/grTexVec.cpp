/****************************************************************************************/
/*  JETEXVEC.C                                                                          */
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
#include "grTexVec.h"

// Private dependents
#include "grGArray.h"
#include "Ram.h"
#include "Errorlog.h"

//=======================================================================================
//	grTexVec_Compare
//=======================================================================================
grBoolean grTexVec_Compare(const grTexVec *Tex1, const grTexVec *Tex2)
{
	if (!grVec3d_Compare(&Tex1->VecU, &Tex2->VecU, 0.01f))
		return GR_FALSE;
	if (!grVec3d_Compare(&Tex1->VecV, &Tex2->VecV, 0.01f))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grTexVec_ArrayCreate
//=======================================================================================
grTexVec_Array *grTexVec_ArrayCreate(int32 StartVecs)
{
	return (grTexVec_Array*)grGArray_Create(StartVecs, sizeof(grTexVec));
}

//=======================================================================================
//	grTexVec_ArrayDestroy
//=======================================================================================
void grTexVec_ArrayDestroy(grTexVec_Array **Array)
{
	grGArray_Destroy((grGArray**)Array);
}

//=======================================================================================
//	grTexVec_ArrayIndexIsValid
//=======================================================================================
grBoolean grTexVec_ArrayIndexIsValid(grTexVec_ArrayIndex Index)
{
	if (Index == GR_GARRAY_NULL_INDEX)
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grTexVec_ArrayAddTexVec
//=======================================================================================
grTexVec_ArrayIndex grTexVec_ArrayAddTexVec(grTexVec_Array *Array, const grTexVec *TexVec)
{
	grGArray_Index	Index;

	Index = grGArray_AddElement((grGArray*)Array, (grGArray_Element*)TexVec);

	return (grTexVec_ArrayIndex)Index;
}

//=======================================================================================
//	grTexVec_ArrayShareTexVec
//=======================================================================================
grTexVec_ArrayIndex grTexVec_ArrayShareTexVec(grTexVec_Array *Array, const grTexVec *TexVec)
{
	grTexVec			*pTexVec;
	grGArray_RefType	*pRef;
	int32				i;

	pTexVec = (grTexVec*)grGArray_GetElements((grGArray*)Array);
	pRef = grGArray_GetRefCounts((grGArray*)Array);

	for (i=0; i< grGArray_GetSize((grGArray*)Array); i++, pRef++, *pTexVec++)
	{
		if (*pRef == 0 || *pRef >= GR_GARRAY_MAX_ELEMENT_REFCOUNT)
			continue;					// If it's empty, or totally full, we can't share with it...

		if (grTexVec_Compare(pTexVec, TexVec))
		{
			assert(*pRef < GR_GARRAY_MAX_ELEMENT_REFCOUNT);
			(*pRef)++;
			return (grTexVec_ArrayIndex)i;
		}
	}

	return grTexVec_ArrayAddTexVec(Array, TexVec);
}

//=======================================================================================
//	grTexVec_ArrayRefTexVecByIndex
//=======================================================================================
grBoolean grTexVec_ArrayRefTexVecByIndex(grTexVec_Array *Array, grTexVec_ArrayIndex Index)
{
	return grGArray_RefElement((grGArray*)Array, Index);
}

//=======================================================================================
//	grTexVec_ArrayRemoveTexVec
//=======================================================================================
void grTexVec_ArrayRemoveTexVec(grTexVec_Array *Array, grTexVec_ArrayIndex *Index)
{
	grGArray_RemoveElement((grGArray*)Array, (grGArray_Index*)Index);
}

//=======================================================================================
//	grTexVec_ArraySetTexVecByIndex
//=======================================================================================
void grTexVec_ArraySetTexVecByIndex(grTexVec_Array *Array, grTexVec_ArrayIndex Index, const grTexVec *TexVec)
{
	grGArray_SetElementByIndex((grGArray*)Array, (grGArray_Index)Index, (grGArray_Element*)TexVec);
}

//=======================================================================================
//	grTexVec_ArrayGetTexVecByIndex
//=======================================================================================
const grTexVec *grTexVec_ArrayGetTexVecByIndex(const grTexVec_Array *Array, grTexVec_ArrayIndex Index)
{
	return (grTexVec*)grGArray_GetElementByIndex((grGArray*)Array, (grGArray_Index)Index);
}

