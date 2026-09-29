/****************************************************************************************/
/*  JEPLANE.H                                                                           */
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

#ifndef GEPLANE_H
#define GEPLANE_H

#include "BaseType.h"
#include "Vec3d.h"
#include "ExtBox.h"
#include "Xform3d.h"

#ifdef __cplusplus
extern "C" {
#endif

// Update by cjp. defining PLANE_CC as __fastcall on win32
#ifdef WIN32
#define PLANE_CC _fastcall
#else
#define PLANE_CC // unknown platform
#endif

//	grPlane_Type just simply says what axis the plane is aligned on.
//	It does not say what direction it is facing on the aligned axis.

//	DON'T change these numbers!!!!  Doing so will result in BAD things!!!
//	The reason, is code uses these enums to index into the X,Y,Z elements:
//		Val = grVec3d_GetElement(&Plane->Normal, Plane->Type%Type_AnyX);
typedef enum
{
	Type_X=0,							// X aligned
	Type_Y=1,							// Y aligned
	Type_Z=2,							// Z aligned
	Type_AnyX=3,						// Arbitrary, X dominent axis
	Type_AnyY=4,						// Arbitrary, Y dominent axis
	Type_AnyZ=5,						// Arbitrary, Z dominent axis
	Type_Any=6,							// Arbitrary (Any axis)
} grPlane_Type;
typedef grPlane_Type grPlane_Type;

typedef uint8			grPlane_Side;
typedef grPlane_Side	grPlane_Side;

// flags for plane on side types
#define PSIDE_FRONT		(1<<0)
#define PSIDE_BACK		(1<<1)
#define PSIDE_FACING	(1<<2)

#define PSIDE_BOTH		(PSIDE_FRONT|PSIDE_BACK)

typedef struct grPlane
{
	grVec3d			Normal;				// Unit Orientation
	float			Dist;				// Distance from origin
	grPlane_Type	Type;				// grPlane_Type
} grPlane;
typedef struct grPlane grPlane;



GRAPI void	GRCC grPlane_SetFromVerts(grPlane *Plane, const grVec3d *V1, const grVec3d *V2, const grVec3d *V3);
GRAPI void	GRCC grPlane_Inverse(grPlane *Plane);
GRAPI void	GRCC grPlane_Rotate(const grPlane *In, const grXForm3d *XForm, grPlane *Out);
GRAPI void	GRCC grPlane_Transform(const grPlane *In, const grXForm3d *XForm, grPlane *Out);
GRAPI void GRCC grPlane_TransformRenorm(const grPlane *In, const grXForm3d *XForm, grPlane *Out);
grPlane_Type	grPlane_TypeFromUnitVector(const grVec3d *V1);
GRAPI grBoolean GRCC grPlane_GetAAVectors(const grPlane *Plane, grVec3d *Xv, grVec3d *Yv);

float PLANE_CC grPlane_PointDistance(const grPlane *Plane, const grVec3d *Point);
float PLANE_CC grPlane_PointDistanceFast(const grPlane *Plane, const grVec3d *Point);

grPlane_Side	grPlane_BoxSide(const grPlane *Plane, const grExtBox *Box, grFloat Epsilon);
grBoolean		grPlane_Compare(const grPlane *Plane1, const grPlane *Plane2, float NEpsilon, float DEpsilon);
	// NEpsilon = Normal Epsilon, DEpsilon = Dist Epsilon
#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif

