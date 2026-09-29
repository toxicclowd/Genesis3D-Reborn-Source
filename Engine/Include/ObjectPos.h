/****************************************************************************************/
/*  OBJECTPOS.H                                                                         */
/*                                                                                      */
/*  Author: David Eisele	                                                        */
/*  Description: Simplify and improve rotation and tranlation of objects                */
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
/*  This file was not part of the original Jet3D, released December 12, 1999.           */
/*                                                                                      */
/****************************************************************************************/

#ifndef GR_POSITION_H
#define GR_POSITION_H

#include "Vec3d.h"
#include "XForm3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	grFloat x[3][4];
} Matrix34;

typedef union MatrixXForm {
	struct{ 
		Matrix34	Matrix;
		grVec3d		Translation;
	};
	grXForm3d	XForm;
} MatrixXForm;

typedef struct grObjectPos {
	grFloat		Phi;							// Spin-Angle
	grFloat		Psi;							// Slope-Angle
	grFloat		Rho;							// Tilt-Angle
	grFloat     CPhi;							// Their sin/cos values
	grFloat     SPhi;
	grFloat     CPsi;
	grFloat     SPsi;
	grFloat     CRho;
	grFloat     SRho;
	Matrix34	RMatrix;						// Relative rotationmatrix
	Matrix34	BMatrix; 						// Base     rotationmatrix
	union { //  Matrix34 & grVec3d <=> grXForm3d
		struct { 
			Matrix34	Matrix;					// Resulting rotationmatrix
			grVec3d		Translation;			// Translation
		};
		grXForm3d	XForm;						// Resulting rotation- and translationmatrix
	};
	int         Flags;							// Normal  = BMatrix is identity
												// XFormed = BMatrix is NOT identity
												// Used internally for rotationacceleration
} grObjectPos;



// Initialization-Methods

GRAPI void GRCC grObjectPos_SetIdentity(grObjectPos *APos);
// Initialization of APos:  Do nothing

GRAPI void GRCC grObjectPos_SetTranslation(grObjectPos *APos, grFloat X, grFloat Y, grFloat Z);
// Initialization of APos:  Set up a translation by (X,Y,Z)

GRAPI void GRCC grObjectPos_SetTranslationByVec(grObjectPos *APos, grVec3d *T);
// Initialization of APos:  Set up a translation by T
							
GRAPI void GRCC grObjectPos_SetRotation(grObjectPos *APos, grFloat Phi, grFloat Psi, grFloat Rho);
// Initialization of APos:  Set up a rotation:
//							Spin by Phi, slope by Psi, tilt by Rho
//							Same as RotateZ(Rho), RotateX(Psi), RotateY(Phi)

GRAPI void GRCC grObjectPos_SetBaseXForm(grObjectPos *APos, const grXForm3d *BaseXF);
// Initialization of APos:  Set up relative Identity:
//							Rotate by BaseXF,no translation or further rotation

GRAPI void GRCC grObjectPos_SetBaseXFormByAxis(grObjectPos *APos, const grVec3d *X, const grVec3d *Y, const grVec3d *Z);
// Same as above, but instead of a XForm the axis are given:
//							X = "Left"-axisvector
//							Y = "Up"-axisvector
//							Z = "Forward"-axisvector



// Set-Methods

void __inline grObjectPos_SetNewTranslationByVec(grObjectPos *APos, grVec3d *V) { APos->Translation=*V; }
// Moveto V

void __inline grObjectPos_SetNewTranslationVecFromMatrix(grObjectPos *APos, grXForm3d *XForm) { APos->Translation=XForm->Translation; }
// Get translation of XForm and moveto there

GRAPI void GRCC grObjectPos_SetNewRotation(grObjectPos *APos, grFloat Phi, grFloat Psi, grFloat Rho);
// Rotate to the new angles

GRAPI void GRCC grObjectPos_SetNewBaseXForm(grObjectPos *APos, const grXForm3d *BaseXF);
// Setup a new baseorientation by a XForm

GRAPI void GRCC grObjectPos_SetNewBaseXFormByAxis(grObjectPos *APos, const grVec3d *X, const grVec3d *Y, const grVec3d *Z);
// Setup a new baseorientation by left/up/forward vectors




// Transform-Methods

GRAPI void GRCC grObjectPos_Spin(grObjectPos *APos, grFloat DPhi);
// Rotate CounterClockWise around the "up/Y-axis" by DPhi
// pos. : "rotate right"
// neg. : "rotate left"

GRAPI void GRCC grObjectPos_Slope(grObjectPos *APos, grFloat DPsi);
// Rotate CCW around the "right/X-axis" by DPsi
// pos. : "rotate up"
// neg. : "rotate down"

GRAPI void GRCC grObjectPos_Tilt(grObjectPos *APos, grFloat DRho);
// Rotate CCW around the "back/Z-axis" by DRho
// pos. : "fall left"
// neg. : "fall right"

GRAPI void GRCC grObjectPos_Rotate(grObjectPos *APos, grFloat DPhi, grFloat DPsi, grFloat DRho);
// Rotate CCW around all axis by DPhi,DPsi,DRho

GRAPI void GRCC grObjectPos_Translate(grObjectPos *APos, grFloat DX, grFloat DY, grFloat DZ);
// Move by (DX,DY,DZ)

GRAPI void GRCC grObjectPos_Move(grObjectPos *APos, grVec3d *Dir, grFloat Dist);
// Move by scaled Dir vector
// Normally used for normalized vectors

GRAPI void GRCC grObjectPos_MoveIn(grObjectPos *APos, grFloat Dist);
// Move forward by Dist

GRAPI void GRCC grObjectPos_MoveLeft(grObjectPos *APos, grFloat Dist);
// Move left by Dist

GRAPI void GRCC grObjectPos_MoveUp(grObjectPos *APos, grFloat Dist);
// Move up by Dist

GRAPI void GRCC grObjectPos_MoveByDifference(const grObjectPos *OPos, const grObjectPos *NPos, grObjectPos *APos);
// Get difference translation vector of OPos and NPos  and move APos by it

GRAPI void GRCC grObjectPos_TransformByDifference(const grObjectPos *OPos, const grObjectPos *NPos, grObjectPos *APos);
// Same as above, but additionally spin/slope/tilt APos by difference


// Query-Methods

void __inline grObjectPos_GetTranslation(const grObjectPos *APos, grVec3d *V) { *V=APos->Translation; }
// return translation

void __inline grObjectPos_ToMatrix(const grObjectPos *APos, grXForm3d *XForm) { *XForm=APos->XForm; }
// return rotation and translation

GRAPI void GRCC grObjectPos_GetIn(const grObjectPos *APos, grVec3d *V);
// Get relative forwardvector (to baseorientation)

GRAPI void GRCC grObjectPos_GetLeft(const grObjectPos *APos, grVec3d *V);
// Get relative leftvector

GRAPI void	GRCC grObjectPos_GetUp(const grObjectPos *APos, grVec3d *V);
// Get relative upwardvector

GRAPI grBoolean GRCC grObjectPos_IsValid(const grObjectPos *APos);
// Are all variables correct initialized?



#ifdef NDEBUG
	#define grObjectPos_SetMaximalAssertionMode(Enable )
#else
	GRAPI 	void GRCC grObjectPos_SetMaximalAssertionMode( grBoolean Enable );
#endif

#ifdef __cplusplus
}
#endif

#endif
