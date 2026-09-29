/****************************************************************************************/
/*  VEC3D.H                                                                             */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: 3D Vector interface                                                    */
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
#ifndef GR_VEC3D_H
#define GR_VEC3D_H

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grVec3d
{
	grFloat X, Y, Z, Pad;
} grVec3d;

#ifndef NDEBUG
GRAPI	grFloat GRCC   grVec3d_GetElement(const grVec3d *V, int32 Index);
GRAPI	void GRCC		grVec3d_SetElement(grVec3d *V, int32 Index, grFloat Value);
#else
	#define grVec3d_GetElement(Vector,Index)  (* ((&((Vector)->X)) +  (Index) ))
	#define grVec3d_SetElement(Vector,Index, Value) ((* ((&((Vector)->X)) +  (Index) )) = Value)
#endif

GRAPI void GRCC		grVec3d_Set(grVec3d *V, grFloat X, grFloat Y, grFloat Z);
GRAPI void GRCC		grVec3d_Get(const grVec3d *V, grFloat *X, grFloat *Y, grFloat *Z);

GRAPI grFloat GRCC	grVec3d_DotProduct(const grVec3d *V1, const grVec3d *V2);
GRAPI void GRCC		grVec3d_CrossProduct(const grVec3d *V1, const grVec3d *V2, grVec3d *VResult);
GRAPI grBoolean GRCC	grVec3d_Compare(const grVec3d *V1, const grVec3d *V2,grFloat tolarance);
GRAPI grFloat GRCC	grVec3d_Normalize(grVec3d *V1);
GRAPI grBoolean GRCC 	grVec3d_IsNormalized(const grVec3d *V);
GRAPI void GRCC		grVec3d_Scale(const grVec3d *VSrc, grFloat Scale, grVec3d *VDst);
GRAPI grFloat GRCC	grVec3d_Length(const grVec3d *V1); 
GRAPI grFloat GRCC	grVec3d_LengthSquared(const grVec3d *V1); 
GRAPI void GRCC		grVec3d_Subtract(const grVec3d *V1, const grVec3d *V2, grVec3d *V1MinusV2);
GRAPI void GRCC		grVec3d_Add(const grVec3d *V1, const grVec3d *V2,  grVec3d *VSum);
GRAPI void GRCC		grVec3d_Copy(const grVec3d *Vsrc, grVec3d *Vdst);
GRAPI void GRCC		grVec3d_Clear(grVec3d *V);
GRAPI void GRCC		grVec3d_Inverse(grVec3d *V);
GRAPI void GRCC		grVec3d_MA(grVec3d *V1, grFloat Scale, const grVec3d *V2, grVec3d *V1PlusV2Scaled);
GRAPI void GRCC		grVec3d_AddScaled(const grVec3d *V1, const grVec3d *V2, grFloat Scale, grVec3d *V1PlusV2Scaled);

GRAPI grFloat GRCC	grVec3d_DistanceBetween(const grVec3d *V1, const grVec3d *V2);	// returns length of V1-V2	
GRAPI grFloat GRCC	grVec3d_DistanceBetweenSquared(const grVec3d *V1, const grVec3d *V2);

GRAPI grBoolean GRCC grVec3d_IsValid(const grVec3d *V);

#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grVec3d grVec3d;


#endif
