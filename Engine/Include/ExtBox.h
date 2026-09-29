/****************************************************************************************/
/*  EXTBOX.H                                                                            */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: Axial aligned bounding box (extent box) support                        */
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
#ifndef GR_EXTBOX_H
#define GR_EXTBOX_H

#include "BaseType.h"
#include "Vec3d.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct grExtBox
{
	grVec3d Min;
	grVec3d Max;
} grExtBox;

// Set the values in a box
GRAPI void GRCC grExtBox_Set (  grExtBox *B,
				  grFloat X1,	  grFloat Y1,	  grFloat Z1,
				  grFloat X2,	  grFloat Y2,	  grFloat Z2 );

// Test a box for validity ( non NULL and max >= min )
GRAPI grBoolean GRCC grExtBox_IsValid(  const grExtBox *B );

// Added by Icestorm
// Test a box for point-degeneration: Min=Max
GRAPI grBoolean GRCC grExtBox_IsPoint(  const grExtBox *B );

// Set box Min and Max to the passed point
GRAPI void GRCC grExtBox_SetToPoint ( grExtBox *B, const grVec3d *Point );

// Extend a box to encompass the passed point
GRAPI void GRCC grExtBox_ExtendToEnclose( grExtBox *B, const grVec3d *Point );

// Return result of box intersection.
// If no intersection, returns GR_FALSE and bResult is not modified.
// If intersection, returns GR_TRUE and fills bResult (if not NULL)
// with the intersected box,
// bResult may be one of b1 or b2.
// 
GRAPI grBoolean GRCC grExtBox_Intersection ( const grExtBox *B1, const grExtBox *B2, grExtBox *Result	);

// computes union of b1 and b2 and returns in bResult.
GRAPI void GRCC grExtBox_Union ( const grExtBox *B1, const grExtBox *B2, grExtBox *Result );

GRAPI grBoolean GRCC grExtBox_ContainsPoint ( const grExtBox *B, const grVec3d  *Point );

// CB note : there are some really bad names in here.  GetTranslation ? Scaling ? what?
GRAPI void GRCC grExtBox_GetTranslation ( const grExtBox *B,       grVec3d *pCenter );
GRAPI void GRCC grExtBox_SetTranslation (       grExtBox *B, const grVec3d *pCenter );
GRAPI void GRCC grExtBox_Translate      (       grExtBox *B, grFloat DX, grFloat DY, grFloat DZ );

// Icestorm Begin

// Make pOrigin the new origin of the box => B relative to pOrigin
GRAPI void GRCC grExtBox_SetNewOrigin( grExtBox *B, const grVec3d *pOrigin);

// Make B's center the new origin of the box => Make B realtive to it's center
// Also if OldCenter is not NULL, save old position/center
GRAPI void GRCC grExtBox_MoveToOrigin( grExtBox *B, grVec3d *OldCenter );

// Same as: First grExtBox_Translate by vMove then grExtBox_MoveToOrigin
GRAPI void GRCC grExtBox_TranslateAndMoveToOrigin( grExtBox *B, const grVec3d *vMove, grVec3d *MovedCenter );

// Icestorm End

GRAPI void GRCC grExtBox_GetScaling     ( const grExtBox *B,       grVec3d *pScale );
GRAPI void GRCC grExtBox_SetScaling     (       grExtBox *B, const grVec3d *pScale );
GRAPI void GRCC grExtBox_Scale          (       grExtBox *B, grFloat DX, grFloat DY,grFloat DZ );

//  Creates a box that encloses the entire area of a box that moves along linear path
GRAPI void GRCC grExtBox_LinearSweep(	const grExtBox *BoxToSweep, 
						const grVec3d *StartPoint, 
						const grVec3d *EndPoint, 
						grExtBox *EnclosingBox );

// Collides a ray with box B.  The ray is directed, from Start to End.  
//   Only returns a ray hitting the outside of the box.  
//     on success, GR_TRUE is returned, and 
//       if T is non-NULL, T is returned as 0..1 where 0 is a collision at Start, and 1 is a collision at End
//       if Normal is non-NULL, Normal is the surface normal of the box where the collision occured.
GRAPI grBoolean GRCC grExtBox_RayCollision( const grExtBox *B, const grVec3d *Start, const grVec3d *End, 
								grFloat *T, grVec3d *Normal );

GRAPI void GRCC grExtBox_GetPoint( const grExtBox *B, const int iPoint, grVec3d *vPoint);

// Added by Icestorm (fast, rewritten version of Incarnadine's one)
// (ca. 7-12 times faster than old ver.)
// ----------------------------------------
// Collides a moving box (or ray) against a stationary box.  The moving box
// must be relative to the path and move from Start to End.
//   Only returns a ray/box hitting the outside of the box.  
//     on success, GR_TRUE is returned, and 
//       if T is non-NULL, T is returned as 0..1 where 0 is a collision at Start, and 1 is a collision at End
//       if Normal is non-NULL, Normal is the surface normal of the box where the collision occured.
GRAPI grBoolean GRCC grExtBox_Collision(	const grExtBox *B, const grExtBox *MovingBox,
											const grVec3d *Start, const grVec3d *End, 
											grFloat *T, grVec3d *Normal );

// Added by Icestorm
// ----------------------------------------
// Collides a changing box against a stationary box.  The changing box
// must be relative to Pos.
//   Only returns a box hitting the outside of the box.  
//     on success, GR_TRUE is returned, and 
//       if T is non-NULL, T is returned as 0..1 where 0 is a collision at Start, and 1 is a collision at End
//       if Normal is non-NULL, Normal is the surfacenormal of the box where the collision occured.
//       if Point is non-NULL, Point is a point of the surface where the collision occured.
GRAPI grBoolean GRCC grExtBox_ChangeBoxCollision(	const grExtBox *B, const grVec3d *Pos,
													const grExtBox *StartBox, const grExtBox *EndBox,
													grFloat *T, grVec3d *Normal, grVec3d *Point );

#ifdef __cplusplus
	}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grExtBox grExtBox;


#endif
		