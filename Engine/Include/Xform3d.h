/****************************************************************************************/
/*  XFORM3D.H                                                                           */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: 3D transform interface                                                 */
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
#ifndef GR_XFORM_H
#define GR_XFORM_H

#include "Vec3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/*{****

// CB note <>
// this would be mighty nice :

typedef struct
{
	union {
		struct Matrix {
			grFloat AA,AB,AC,AD;
			grFloat BA,BB,BC,BD;
			grFloat CA,CB,CC,CD;
		} Matrix;

		struct Matrix {
			grVec3d X;
			grVec3d Y;
			grVec3d Z;
		} Rows;
	}
	grVec3d Translation;
} grXForm3d;

***}*/

#define	XFORM3D_NONORTHOGONALISOK	0x00000001

typedef struct grXForm3d
{	
	grFloat 	AX,AY,AZ;			// e[0][0],e[0][1],e[0][2]
	uint32		Flags;				// Be careful!  We've sandwiched this in
									//  where Katmai instructions will deal with it
	grFloat 	BX,BY,BZ, BPad;		// e[1][0],e[1][1],e[1][2]
	grFloat 	CX,CY,CZ, CPad;		// e[2][0],e[2][1],e[2][2]
	grVec3d 	Translation;		// e[0][3],e[1][3],e[2][3]
	//	  0,0,0,1					// e[3][0],e[3][1],e[3][2]
} grXForm3d;

/*   this is essentially a 'standard' 4x4 transform matrix,
     with the bottom row always 0,0,0,1

	| AX, AY, AZ, Translation.X |  
	| BX, BY, BZ, Translation.Y |  
	| CX, CY, CZ, Translation.Z |  
	|  0,  0,  0,      1        |  
*/

//  all grXForm3d_Set* functions return a right-handed transform.

#define GEXFORM3D_MINIMUM_SCALE (0.00001f)

//is katmai active?
GRAPI	grBoolean	GRCC	grXForm3d_UsingKatmai(void);
GRAPI	grBoolean	GRCC	grXForm3d_EnableKatmai(grBoolean useit);

GRAPI void GRCC grXForm3d_Copy(
	const grXForm3d *Src, 
	grXForm3d *Dst);
	// copies Src to Dst.  

GRAPI grBoolean GRCC grXForm3d_IsValid(const grXForm3d *M);
	// returns GR_TRUE if M is 'valid'  
	// 'valid' means that M is non NULL, and there are no NAN's in the matrix.

GRAPI grBoolean GRCC grXForm3d_IsOrthonormal(const grXForm3d *M);
	// returns GR_TRUE if M is orthonormal 
	// (if the rows and columns are all normalized (transform has no scaling or shearing)
	// and is orthogonal (row1 cross row2 = row3 & col1 cross col2 = col3)
	// * does not check for right-handed convention *

GRAPI grBoolean GRCC grXForm3d_IsOrthogonal(const grXForm3d *M);
	// returns GR_TRUE if M is orthogonal
	// (row1 cross row2 = row3 & col1 cross col2 = col3)
	// * does not check for right-handed convention *

GRAPI void GRCC grXForm3d_Orthonormalize(grXForm3d *M);
	// essentially removes scaling (or other distortions) from 
	// an orthogonal (or nearly orthogonal) matrix 
	// returns a right-handed matrix


GRAPI void GRCC grXForm3d_SetIdentity(grXForm3d *M);			
	// sets M to an identity matrix (clears it)
	
GRAPI void GRCC grXForm3d_SetXRotation(grXForm3d *M,grFloat RadianAngle);
	// sets up a transform that rotates RadianAngle about X axis
	// all existing contents of M are replaced
	
GRAPI void GRCC grXForm3d_SetYRotation(grXForm3d *M,grFloat RadianAngle);
	// sets up a transform that rotates RadianAngle about Y axis
	// all existing contents of M are replaced

GRAPI void GRCC grXForm3d_SetZRotation(grXForm3d *M,grFloat RadianAngle);
	// sets up a transform that rotates RadianAngle about Z axis
	// all existing contents of M are replaced

GRAPI void GRCC grXForm3d_SetTranslation(grXForm3d *M,grFloat x, grFloat y, grFloat z);
	// sets up a transform that translates x,y,z
	// all existing contents of M are replaced

GRAPI void GRCC grXForm3d_SetScaling(grXForm3d *M,grFloat x, grFloat y, grFloat z);
	// sets up a transform that scales by x,y,z
	// all existing contents of M are replaced

GRAPI void GRCC grXForm3d_RotateX(grXForm3d *M,grFloat RadianAngle);  
	// Rotates M by RadianAngle about X axis   
	// applies the rotation to the existing contents of M

GRAPI void GRCC grXForm3d_RotateY(grXForm3d *M,grFloat RadianAngle);
	// Rotates M by RadianAngle about Y axis
	// applies the rotation to the existing contents of M

GRAPI void GRCC grXForm3d_RotateZ(grXForm3d *M,grFloat RadianAngle);
	// Rotates M by RadianAngle about Z axis
	// applies the rotation to the existing contents of M

GRAPI void GRCC grXForm3d_PostRotateX(grXForm3d *M,grFloat RadianAngle);  
GRAPI void GRCC grXForm3d_PostRotateY(grXForm3d *M,grFloat RadianAngle);
GRAPI void GRCC grXForm3d_PostRotateZ(grXForm3d *M,grFloat RadianAngle);
	// appends the rotation to M
	// this operation does not change the translation in M, unlike the "_Rotate" functions

GRAPI void GRCC grXForm3d_Translate(grXForm3d *M,grFloat x, grFloat y, grFloat z);	
	// Translates M by x,y,z
	// applies the translation to the existing contents of M

GRAPI void GRCC grXForm3d_Scale(grXForm3d *M,grFloat x, grFloat y, grFloat z);		
	// Scales M by x,y,z
	// applies the scale to the existing contents of M

GRAPI void GRCC grXForm3d_Multiply(
	const grXForm3d *M1, 
	const grXForm3d *M2, 
	grXForm3d *MProduct);
	// MProduct = matrix multiply of M1*M2
	// Concatenates the transformation in the M2 matrix onto the transformation in M1

GRAPI void GRCC grXForm3d_Transform(
	const grXForm3d *M,
	const grVec3d *V, 
	grVec3d *Result);
	// Result is Matrix M * Vector V:  V Tranformed by M

GRAPI void GRCC grXForm3d_TransformVecArray(const grXForm3d *XForm, 
								const grVec3d *Source, 
								grVec3d *Dest, 
								int32 Count);

GRAPI void GRCC grXForm3d_TransformArray(	const grXForm3d *XForm, 
								const grVec3d *Source, 
									int32 SourceStride,
								grVec3d *Dest,
									int32 DestStride,
								int32 Count);

GRAPI void GRCC grXForm3d_Rotate(
	const grXForm3d *M,
	const grVec3d *V, 
	grVec3d *Result);
	// Result is Matrix M * Vector V:  V Rotated by M (no translation)


/***
*
	"Left,Up,In" are just the basis vectors in the new coordinate space.
	You can get them by multiplying the unit bases into the transforms.
*
******/

GRAPI void GRCC grXForm3d_GetLeft(const grXForm3d *M, grVec3d *Left);
	// Gets a vector that is 'left' in the frame of reference of M (facing -Z)

GRAPI void GRCC grXForm3d_GetUp(const grXForm3d *M,    grVec3d *Up);
	// Gets a vector that is 'up' in the frame of reference of M (facing -Z)

GRAPI void GRCC grXForm3d_GetIn(const grXForm3d *M,  grVec3d *In);
	// Gets a vector that is 'in' in the frame of reference of M (facing -Z)

GRAPI void GRCC grXForm3d_GetInverse(const grXForm3d *M, grXForm3d *MInv);
	// Gets the inverse transform of M   (M^T) 

GRAPI void GRCC grXForm3d_GetTranspose(const grXForm3d *M, grXForm3d *MTranspose);
	// Gets the Transpose transform of M   (M^T) 
	// Transpose of a matrix is the switch of the rows and columns
	// The transpose is usefull because it is rapidly computed and is equal to the inverse 
	// transform for orthonormal transforms    [inverse is (M')  where M*M' = Identity ]

GRAPI void GRCC grXForm3d_TransposeTransform(
	const grXForm3d *M, 
	const grVec3d *V, 
	grVec3d *Result);
	// applies the transpose transform of M to V.  Result = (M^T) * V

/*****
*
	the Euler angles are subsequent rotations :
		by Angles->Z around the Z axis
		then by Angles->Y around the Y axis, in the newly rotate coordinates
		then by Angles->X around the X axis
*
******/	

GRAPI void GRCC grXForm3d_GetEulerAngles(const grXForm3d *M, grVec3d *Angles);
	// Finds Euler angles from M and puts them into Angles
	
GRAPI void GRCC grXForm3d_SetEulerAngles(grXForm3d *M, const grVec3d *Angles);
	// Applies Euler angles to build M

GRAPI void GRCC grXForm3d_SetFromLeftUpIn(
	grXForm3d *M,
	const grVec3d *Left, 
	const grVec3d *Up, 
	const grVec3d *In);
	// Builds an grXForm3d from orthonormal Left, Up and In vectors

GRAPI void GRCC grXForm3d_Mirror(
	const		grXForm3d *Source, 
	const		grVec3d *PlaneNormal, 
	float		PlaneDist, 
	grXForm3d	*Dest);
	// Mirrors a XForm3d about a plane


//--------------

#ifdef NDEBUG
	#define grXForm3d_SetMaximalAssertionMode(Enable )
#else
	GRAPI 	void GRCC grXForm3d_SetMaximalAssertionMode( grBoolean Enable );
#endif


#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grXForm3d grXForm3d;

#define GR_XFORM3D_NONORTHOGONALISOK     XFORM3D_NONORTHOGONALISOK

#endif
