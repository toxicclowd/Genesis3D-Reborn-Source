/****************************************************************************************/
/*  JEPOLY.H                                                                            */
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

#ifndef GEPOLY_H
#define GEPOLY_H

#include "Vec3d.h"
#include "grPlane.h"

#ifdef __cplusplus
extern "C" {
#endif

//=======================================================================================
//=======================================================================================
typedef uint16	grPoly_NumVertType;
typedef grPoly_NumVertType jePoly_NumVertType;
typedef grVec3d	grPoly_VertType;
typedef grPoly_VertType jePoly_VertType;

#define GR_POLY_MAX_VERTS	((uint32)1<<(sizeof(grPoly_NumVertType)*8))

//=======================================================================================
//=======================================================================================

typedef struct jePoly
{
	grPoly_NumVertType	NumVerts;
	grPoly_VertType		*Verts;

#ifdef _DEBUG
	struct jePoly		*Self;
#endif

} grPoly;
typedef struct jePoly jePoly;


//=======================================================================================
//	Function prototypes
//=======================================================================================
grPoly		*grPoly_Create(int32 NumVerts);
grPoly		*grPoly_CreateFromPoly(const grPoly *Poly, grBoolean Reverse);
grPoly		*grPoly_CreateFromPlane(const grPlane *Plane, float Scale);
void		grPoly_Destroy(grPoly **Poly);
float		grPoly_Area(const grPoly *Poly);
grBoolean	grPoly_ClipEpsilon(grPoly **Poly, float Epsilon, const grPlane *Plane, grBoolean FlipSide);
				// Poly will be NULL if clipped away.  Poly pointer will chanje if clipped...
grBoolean	grPoly_SplitEpsilon(grPoly **InPoly, float Epsilon, const grPlane *Plane, grBoolean FlipSide, grPoly **Front, grPoly **Back);
				// InPoly is freed
grBoolean	grPoly_IsTiny (const grPoly *Poly);
grBoolean	grPoly_IsValid(const grPoly *Poly);
grBoolean	grPoly_EdgeExist(const grPoly *Poly, const grVec3d *v1, const grVec3d *v2, int32 *i1, int32 *i2);
grBoolean	grPoly_Merge(const grPoly *Poly1, const grPoly *Poly2, const grVec3d *Normal, grPoly **Out);
grBoolean	grPoly_RemoveDegenerateEdges(grPoly *Poly, grFloat Epsilon);
#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define JE_POLY_MAX_VERTS                        GR_POLY_MAX_VERTS
#define jePoly_Area                              grPoly_Area
#define jePoly_ClipEpsilon                       grPoly_ClipEpsilon
#define jePoly_Create                            grPoly_Create
#define jePoly_CreateFromPlane                   grPoly_CreateFromPlane
#define jePoly_CreateFromPoly                    grPoly_CreateFromPoly
#define jePoly_Destroy                           grPoly_Destroy
#define jePoly_EdgeExist                         grPoly_EdgeExist
#define jePoly_IsTiny                            grPoly_IsTiny
#define jePoly_IsValid                           grPoly_IsValid
#define jePoly_Merge                             grPoly_Merge
#define jePoly_RemoveDegenerateEdges             grPoly_RemoveDegenerateEdges
#define jePoly_SplitEpsilon                      grPoly_SplitEpsilon

#endif // GENESIS_NO_JET_COMPAT

#endif
