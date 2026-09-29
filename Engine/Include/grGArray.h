/****************************************************************************************/
/*  JEGARRAY.H                                                                          */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D: Reborn Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D: Reborn, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/

#ifndef GR_GARRAY_H
#define GR_GARRAY_H

#include "BaseType.h"
#include "VFile.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef struct grGArray grGArray;
typedef			uint16				grGArray_Index;
									
typedef			void				grGArray_Element;
typedef			uint16				grGArray_RefType;

typedef			grBoolean			grGArray_IOFunc(grVFile *File, grGArray_Element *Element, void *Context);


#define GR_GARRAY_MAX_ELEMENTS		(0xffff-1)
#define	GR_GARRAY_NULL_INDEX		(GR_GARRAY_MAX_ELEMENTS+1)
#define GR_GARRAY_MAX_ELEMENT_SIZE	0xffff

#define	GR_GARRAY_MAX_ELEMENT_REFCOUNT	0xffff
//========================================================================================
//	Structure defs
//========================================================================================

//========================================================================================
//	Function prototypes
//========================================================================================
grGArray	*grGArray_Create(int32 StartElements, int32 ElementSize);


grBoolean	grGArray_WriteToFile(const grGArray *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grGArray	*grGArray_CreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);

grBoolean	grGArray_CreateRef(grGArray *Array);
void		grGArray_Destroy(grGArray **Array);
grBoolean	grGArray_IsValid(const grGArray *Array);
grGArray_Index grGArray_AddElement(grGArray *Array, const grGArray_Element *Element);
grBoolean	grGArray_RefElement(grGArray *Array, grGArray_Index Index);
void		grGArray_RemoveElement(grGArray *Array, grGArray_Index *Index);
int32		grGArray_GetSize(const grGArray *Array);
grGArray_Element *grGArray_GetElements(const grGArray *Array);
grGArray_RefType *grGArray_GetRefCounts(const grGArray *Array);
const		grGArray_RefType grGArray_GetElementRefCountByIndex(const grGArray *Array, grGArray_Index Index);
void		grGArray_SetElementByIndex(grGArray *Array, grGArray_Index Index, const grGArray_Element *Element);
const		grGArray_Element *grGArray_GetElementByIndex(const grGArray *Array, grGArray_Index Index);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
