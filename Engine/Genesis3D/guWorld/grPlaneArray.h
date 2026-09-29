/****************************************************************************************/
/*  JEPLANEARRAY.H                                                                      */
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

#ifndef GEPLANEARRAY_H
#define GEPLANEARRAY_H

#include "BaseType.h"
#include "grPlane.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct		grPlaneArray		grPlaneArray;

typedef uint32		grPlaneArray_Index;

#define GR_PLANEARRAY_MAX_PLANES		((0xffffffff>>1)-1)
#define	GR_PLANEARRAY_NULL_INDEX		(GR_PLANEARRAY_MAX_PLANES+1)

#define grPlaneArray_IndexSided(p) (p&1)
#define grPlaneArray_IndexIsCoplanar(p1, p2) ((p1&~1) == (p2&~1))
#define grPlaneArray_IndexIsCoplanarAndFacing(p1, p2) (p1 == p2)
#define grPlaneArray_IndexIsCoplanarAndNotFacing(p1, p2) (p1 == (p2^1))
#define grPlaneArray_IndexReverse(p) (p^1)
#define	grPlaneArray_IndexGetPositive(p) (p&(~1))

grPlaneArray	*grPlaneArray_Create(int32 StartPlanes);
void			grPlaneArray_Destroy(grPlaneArray **Array);
grBoolean		grPlaneArray_IsValid(const grPlaneArray *Array);
grPlaneArray_Index grPlaneArray_SharePlane(grPlaneArray *Array, const grPlane *Plane);
void			grPlaneArray_RemovePlane(grPlaneArray *Array, grPlaneArray_Index *Index);
grBoolean		grPlaneArray_RefPlaneByIndex(grPlaneArray *Array, grPlaneArray_Index Index);

const grPlane * PLANE_CC grPlaneArray_GetPlaneByIndex(const grPlaneArray *Array, grPlaneArray_Index Index);

#ifdef __cplusplus
}
#endif

#endif
