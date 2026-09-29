/****************************************************************************************/
/*  JETEXVEC.H                                                                          */
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

#ifndef GR_TEXVEC_H
#define GR_TEXVEC_H

#include "Vec3d.h"
#include "grGArray.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef struct	grTexVec_Array			grTexVec_Array;
					
#define grTexVec_ArrayIndex				grGArray_Index

#define GR_TEXVEC_ARRAY_MAX_TEXVECS		(GR_GARRAY_MAX_ELEMENTS)
#define	GR_TEXVEC_ARRAY_NULL_INDEX		(GR_GARRAY_NULL_INDEX)

//========================================================================================
//	Structure defs
//========================================================================================
typedef struct
{
	grVec3d		VecU;
	grVec3d		VecV;
} grTexVec;

//Begin CyRiuS (Timothy Roff)
typedef struct Surf_TexVert
{
	float		u, v;
	float		r, g, b, a;
} Surf_TexVert;
//End CyRiuS

//========================================================================================
//	Function prototypes
//========================================================================================
grBoolean grTexVec_Compare(const grTexVec *Tex1, const grTexVec *Tex2);
grTexVec_Array *grTexVec_ArrayCreate(int32 StartVecs);
void grTexVec_ArrayDestroy(grTexVec_Array **Array);
grBoolean grTexVec_ArrayIndexIsValid(grTexVec_ArrayIndex Index);
grTexVec_ArrayIndex grTexVec_ArrayAddTexVec(grTexVec_Array *Array, const grTexVec *TexVec);
grTexVec_ArrayIndex grTexVec_ArrayShareTexVec(grTexVec_Array *Array, const grTexVec *TexVec);
grBoolean grTexVec_ArrayRefTexVecByIndex(grTexVec_Array *Array, grTexVec_ArrayIndex Index);
void grTexVec_ArrayRemoveTexVec(grTexVec_Array *Array, grTexVec_ArrayIndex *Index);
void grTexVec_ArraySetTexVecByIndex(grTexVec_Array *Array, grTexVec_ArrayIndex Index, const grTexVec *TexVec);
const grTexVec *grTexVec_ArrayGetTexVecByIndex(const grTexVec_Array *Array, grTexVec_ArrayIndex Index);

#ifdef __cplusplus
}
#endif

#endif
