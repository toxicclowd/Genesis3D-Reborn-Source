/****************************************************************************************/
/*  QUATERN.C                                                                           */
/*                                                                                      */
/*  Author: Mike Sandige                                                                */
/*  Description: Quaternion mathematical system implementation                          */
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
#include <math.h>

#include "BaseType.h"
#include "Quatern.h"


#ifndef NDEBUG
	static grBoolean grQuaternion_MaximalAssertionMode = GR_TRUE;
	#define grQuaternion_Assert if (grQuaternion_MaximalAssertionMode) assert

	GRAPI void GRCC grQuaternion_SetMaximalAssertionMode( grBoolean Enable )
	{
		assert( (Enable == GR_TRUE) || (Enable == GR_FALSE) );
		grQuaternion_MaximalAssertionMode = Enable;
	}
#else
	#define grQuaternion_Assert assert
#endif

#define UNIT_TOLERANCE 0.001  
	// Quaternion magnitude must be closer than this tolerance to 1.0 to be 
	// considered a unit quaternion

#define QZERO_TOLERANCE 0.00001 
	// quaternion magnitude must be farther from this tolerance to 0.0 to be 
	// normalized

#define TRACE_QZERO_TOLERANCE 0.1
	// trace of matrix must be greater than this to be used for converting a matrix
	// to a quaternion.

#define AA_QZERO_TOLERANCE 0.0001
	

GRAPI grBoolean GRCC grQuaternion_IsValid(const grQuaternion *Q)
{
	if (Q == NULL)
		return GR_FALSE;
	if ((Q->W * Q->W) < 0.0f)
		return GR_FALSE;
	if ((Q->X * Q->X) < 0.0f)
		return GR_FALSE;
	if ((Q->Y * Q->Y) < 0.0f)
		return GR_FALSE;
	if ((Q->Z * Q->Z) < 0.0f)
		return GR_FALSE;
	return GR_TRUE;
}

GRAPI void GRCC grQuaternion_Set( 
	grQuaternion *Q, grFloat W, grFloat X, grFloat Y, grFloat Z)
{
	assert( Q != NULL );

	Q->W = W;
	Q->X = X;
	Q->Y = Y;
	Q->Z = Z;
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
}

GRAPI void GRCC grQuaternion_SetVec3d(
	grQuaternion *Q, grFloat W, const grVec3d *V)
{
	assert( Q != NULL );
	assert( grVec3d_IsValid(V) != GR_FALSE );

	Q->W = W;
	Q->X = V->X;
	Q->Y = V->Y;
	Q->Z = V->Z;
}	

GRAPI void GRCC grQuaternion_Get( 
	const grQuaternion *Q, 
	grFloat *W, 
	grFloat *X, 
	grFloat *Y, 
	grFloat *Z)
	// get quaternion components into W,X,Y,Z
{
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( W != NULL );
	assert( X != NULL );
	assert( Y != NULL );
	assert( Z != NULL );

	*W = Q->W;
	*X = Q->X;
	*Y = Q->Y;
	*Z = Q->Z;
}

GRAPI void GRCC grQuaternion_SetFromAxisAngle(grQuaternion *Q, const grVec3d *Axis, grFloat Theta)
	// set a quaternion from an axis and a rotation around the axis
{
	grFloat sinTheta;
	assert( Q != NULL);
	assert( grVec3d_IsValid(Axis) != GR_FALSE);
	assert( (Theta * Theta) >= 0.0f );
	assert( ( fabs(grVec3d_Length(Axis)-1.0f) < AA_QZERO_TOLERANCE) );
	
	Theta = Theta * (grFloat)0.5f;
	Q->W     = (grFloat) cos(Theta);
	sinTheta = (grFloat) sin(Theta);
	Q->X = sinTheta * Axis->X;
	Q->Y = sinTheta * Axis->Y;
	Q->Z = sinTheta * Axis->Z;

	grQuaternion_Assert( grQuaternion_IsUnit(Q) == GR_TRUE );
}


GRAPI grBoolean GRCC grQuaternion_GetAxisAngle(const grQuaternion *Q, grVec3d *Axis, grFloat *Theta)
{	
	float OneOverSinTheta;
	float HalfTheta;
	assert( Q != NULL );
	assert( Axis != NULL );
	assert( Theta != NULL );
	grQuaternion_Assert( grQuaternion_IsUnit(Q) != GR_FALSE );
	
	HalfTheta  = (grFloat)acos( Q->W );
	if (HalfTheta>QZERO_TOLERANCE)
		{
			OneOverSinTheta = 1.0f / (grFloat)sin( HalfTheta );
			Axis->X = OneOverSinTheta * Q->X;
			Axis->Y = OneOverSinTheta * Q->Y;
			Axis->Z = OneOverSinTheta * Q->Z;
			*Theta = 2.0f * HalfTheta;
			grQuaternion_Assert( grVec3d_IsValid(Axis) != GR_FALSE );
			grQuaternion_Assert( (*Theta * *Theta) >= 0.0f);
			return GR_TRUE;
		}
	else
		{
			Axis->X = Axis->Y = Axis->Z = 0.0f;
			*Theta = 0.0f;
			return GR_FALSE;
		}
}


GRAPI void GRCC grQuaternion_GetVec3d( 
	const grQuaternion *Q, 
	grFloat *W, 
	grVec3d *V)
	// get quaternion components into W and V
{
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( W != NULL );
	assert( V != NULL );
	
	*W   = Q->W;
	V->X = Q->X;
	V->Y = Q->Y;
	V->Z = Q->Z;
}


GRAPI void GRCC grQuaternion_FromMatrix(
	const grXForm3d		*M,
	      grQuaternion	*Q)
	// takes upper 3 by 3 portion of matrix (rotation sub matrix) 
	// and generates a quaternion
{
	grFloat trace,s;

	assert( M != NULL );
	assert( Q != NULL );
	grQuaternion_Assert( grXForm3d_IsOrthonormal(M)==GR_TRUE );

	trace = M->AX + M->BY + M->CZ;
	if (trace > 0.0f)
		{
			s = (grFloat)sqrt(trace + 1.0f);
			Q->W = s * 0.5f;
			s = 0.5f / s;

			Q->X = (M->CY - M->BZ) * s;
			Q->Y = (M->AZ - M->CX) * s;
			Q->Z = (M->BX - M->AY) * s;
		}
	else
		{
			int biggest;
			enum {A,E,I};
			if (M->AX > M->BY)
				{
					if (M->CZ > M->AX)
						biggest = I;	
					else
						biggest = A;
				}
			else
				{
					if (M->CZ > M->AX)
						biggest = I;
					else
						biggest = E;
				}

			// in the unusual case the original trace fails to produce a good sqrt, try others...
			switch (biggest)
				{
				case A:
					s = (grFloat)sqrt( M->AX - (M->BY + M->CZ) + 1.0);
					if (s > TRACE_QZERO_TOLERANCE)
						{
							Q->X = s * 0.5f;
							s = 0.5f / s;
							Q->W = (M->CY - M->BZ) * s;
							Q->Y = (M->AY + M->BX) * s;
							Q->Z = (M->AZ + M->CX) * s;
							break;
						}
							// I
							s = (grFloat)sqrt( M->CZ - (M->AX + M->BY) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->Z = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->BX - M->AY) * s;
									Q->X = (M->CX + M->AZ) * s;
									Q->Y = (M->CY + M->BZ) * s;
									break;
								}
							// E
							s = (grFloat)sqrt( M->BY - (M->CZ + M->AX) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->Y = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->AZ - M->CX) * s;
									Q->Z = (M->BZ + M->CY) * s;
									Q->X = (M->BX + M->AY) * s;
									break;
								}
							break;
				case E:
					s = (grFloat)sqrt( M->BY - (M->CZ + M->AX) + 1.0);
					if (s > TRACE_QZERO_TOLERANCE)
						{
							Q->Y = s * 0.5f;
							s = 0.5f / s;
							Q->W = (M->AZ - M->CX) * s;
							Q->Z = (M->BZ + M->CY) * s;
							Q->X = (M->BX + M->AY) * s;
							break;
						}
							// I
							s = (grFloat)sqrt( M->CZ - (M->AX + M->BY) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->Z = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->BX - M->AY) * s;
									Q->X = (M->CX + M->AZ) * s;
									Q->Y = (M->CY + M->BZ) * s;
									break;
								}
							// A
							s = (grFloat)sqrt( M->AX - (M->BY + M->CZ) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->X = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->CY - M->BZ) * s;
									Q->Y = (M->AY + M->BX) * s;
									Q->Z = (M->AZ + M->CX) * s;
									break;
								}
					break;
				case I:
					s = (grFloat)sqrt( M->CZ - (M->AX + M->BY) + 1.0);
					if (s > TRACE_QZERO_TOLERANCE)
						{
							Q->Z = s * 0.5f;
							s = 0.5f / s;
							Q->W = (M->BX - M->AY) * s;
							Q->X = (M->CX + M->AZ) * s;
							Q->Y = (M->CY + M->BZ) * s;
							break;
						}
							// A
							s = (grFloat)sqrt( M->AX - (M->BY + M->CZ) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->X = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->CY - M->BZ) * s;
									Q->Y = (M->AY + M->BX) * s;
									Q->Z = (M->AZ + M->CX) * s;
									break;
								}
							// E
							s = (grFloat)sqrt( M->BY - (M->CZ + M->AX) + 1.0);
							if (s > TRACE_QZERO_TOLERANCE)
								{
									Q->Y = s * 0.5f;
									s = 0.5f / s;
									Q->W = (M->AZ - M->CX) * s;
									Q->Z = (M->BZ + M->CY) * s;
									Q->X = (M->BX + M->AY) * s;
									break;
								}
					break;
				default:
					assert(0);
				}
		}
	grQuaternion_Assert( grQuaternion_IsUnit(Q) == GR_TRUE );
}

GRAPI void GRCC grQuaternion_ToMatrix(
	const grQuaternion	*Q, 
		  grXForm3d		*M)
	// takes a unit quaternion and fills out an equivelant rotation
	// portion of a xform
{
	grFloat X2,Y2,Z2;		//2*QX, 2*QY, 2*QZ
	grFloat XX2,YY2,ZZ2;	//2*QX*QX, 2*QY*QY, 2*QZ*QZ
	grFloat XY2,XZ2,XW2;	//2*QX*QY, 2*QX*QZ, 2*QX*QW
	grFloat YZ2,YW2,ZW2;	// ...

	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( M != NULL );
	grQuaternion_Assert( grQuaternion_IsUnit(Q) == GR_TRUE );
	
	
	X2  = 2.0f * Q->X;
	XX2 = X2   * Q->X;
	XY2 = X2   * Q->Y;
	XZ2 = X2   * Q->Z;
	XW2 = X2   * Q->W;

	Y2  = 2.0f * Q->Y;
	YY2 = Y2   * Q->Y;
	YZ2 = Y2   * Q->Z;
	YW2 = Y2   * Q->W;
	
	Z2  = 2.0f * Q->Z;
	ZZ2 = Z2   * Q->Z;
	ZW2 = Z2   * Q->W;
	
	M->AX = 1.0f - YY2 - ZZ2;
	M->AY = XY2  - ZW2;
	M->AZ = XZ2  + YW2;

	M->BX = XY2  + ZW2;
	M->BY = 1.0f - XX2 - ZZ2;
	M->BZ = YZ2  - XW2;

	M->CX = XZ2  - YW2;
	M->CY = YZ2  + XW2;
	M->CZ = 1.0f - XX2 - YY2;

	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;

#ifdef USE_CONVENTIONS
	M->Convention = GR_XFORM3D_RIGHT_HANDED;
#endif

	grQuaternion_Assert( grXForm3d_IsOrthonormal(M)==GR_TRUE );

}


#define EPSILON (0.00001)



GRAPI void GRCC grQuaternion_Slerp(
	const grQuaternion		*Q0, 
	const grQuaternion		*Q1, 
	grFloat					T,		
	grQuaternion			*QT)
	// spherical interpolation between q0 and q1.   0<=t<=1 
	// resulting quaternion is 'between' q0 and q1
	// with t==0 being all q0, and t==1 being all q1.
{
	grFloat omega,cosom,sinom,Scale0,Scale1;
	grQuaternion QL;
	assert( Q0 != NULL );
	assert( Q1 != NULL );
	assert( QT  != NULL );
	assert( ( 0 <= T ) && ( T <= 1.0f ) );
	grQuaternion_Assert( grQuaternion_IsUnit(Q0) == GR_TRUE );
	grQuaternion_Assert( grQuaternion_IsUnit(Q1) == GR_TRUE );

	cosom =		(Q0->W * Q1->W) + (Q0->X * Q1->X) 
			  + (Q0->Y * Q1->Y) + (Q0->Z * Q1->Z);

	if (cosom < 0)
		{
			cosom = -cosom;
			QL.X = -Q1->X;
			QL.Y = -Q1->Y;
			QL.Z = -Q1->Z;
			QL.W = -Q1->W;
		}
	else
		{
			QL = *Q1;
		}
			

	if ( (1.0f - cosom) > EPSILON )
		{
			omega  = (grFloat) acos( cosom );
			sinom  = (grFloat) sin( omega );
			Scale0 = (grFloat) sin( (1.0f-T) * omega) / sinom;
			Scale1 = (grFloat) sin( T*omega) / sinom;
		}
	else
		{
			// has numerical difficulties around cosom == 0
			// in this case degenerate to linear interpolation
		
			Scale0 = 1.0f - T;
			Scale1 = T;
		}


	QT-> X = Scale0 * Q0->X + Scale1 * QL.X;
	QT-> Y = Scale0 * Q0->Y + Scale1 * QL.Y;
	QT-> Z = Scale0 * Q0->Z + Scale1 * QL.Z;
	QT-> W = Scale0 * Q0->W + Scale1 * QL.W;
	grQuaternion_Assert( grQuaternion_IsUnit(QT) == GR_TRUE );
}




GRAPI void GRCC grQuaternion_SlerpNotShortest(
	const grQuaternion		*Q0, 
	const grQuaternion		*Q1, 
	grFloat					T,		
	grQuaternion			*QT)
	// spherical interpolation between q0 and q1.   0<=t<=1 
	// resulting quaternion is 'between' q0 and q1
	// with t==0 being all q0, and t==1 being all q1.
{
	grFloat omega,cosom,sinom,Scale0,Scale1;
	assert( Q0 != NULL );
	assert( Q1 != NULL );
	assert( QT  != NULL );
	assert( ( 0 <= T ) && ( T <= 1.0f ) );
	grQuaternion_Assert( grQuaternion_IsUnit(Q0) == GR_TRUE );
	grQuaternion_Assert( grQuaternion_IsUnit(Q1) == GR_TRUE );

	cosom =		(Q0->W * Q1->W) + (Q0->X * Q1->X) 
			  + (Q0->Y * Q1->Y) + (Q0->Z * Q1->Z);
	if ( (1.0f + cosom) > EPSILON )
		{
			if ( (1.0f - cosom) > EPSILON )
				{
					omega  = (grFloat) acos( cosom );
					sinom  = (grFloat) sin( omega );
					// has numerical difficulties around cosom == nPI/2
					// in this case everything is up for grabs... 
					//  ...degenerate to linear interpolation
					if (sinom < EPSILON)
						{
							Scale0 = 1.0f - T;
							Scale1 = T;	
						}
					else
						{
							Scale0 = (grFloat) sin( (1.0f-T) * omega) / sinom;
							Scale1 = (grFloat) sin( T*omega) / sinom;
						}
				}
			else
				{
					// has numerical difficulties around cosom == 0
					// in this case degenerate to linear interpolation
				
					Scale0 = 1.0f - T;
					Scale1 = T;
				}
			QT-> X = Scale0 * Q0->X + Scale1 * Q1->X;
			QT-> Y = Scale0 * Q0->Y + Scale1 * Q1->Y;
			QT-> Z = Scale0 * Q0->Z + Scale1 * Q1->Z;
			QT-> W = Scale0 * Q0->W + Scale1 * Q1->W;
			//#pragma message (" ack:!!!!!!")
			//grQuaternionNormalize(QT); 
			grQuaternion_Assert( grQuaternion_IsUnit(QT));
		}
	else
		{
			QT->X = -Q0->Y; 
			QT->Y =  Q0->X;
			QT->Z = -Q0->W;
			QT->W =  Q0->Z;
			Scale0 = (grFloat) sin( (1.0f - T) * (QUATERNION_PI*0.5) );
			Scale1 = (grFloat) sin( T * (QUATERNION_PI*0.5) );
			QT-> X = Scale0 * Q0->X + Scale1 * QT->X;
			QT-> Y = Scale0 * Q0->Y + Scale1 * QT->Y;
			QT-> Z = Scale0 * Q0->Z + Scale1 * QT->Z;
			QT-> W = Scale0 * Q0->W + Scale1 * QT->W;
			grQuaternion_Assert( grQuaternion_IsUnit(QT));
		}
}

GRAPI void GRCC grQuaternion_Multiply(
	const grQuaternion	*Q1, 
	const grQuaternion	*Q2, 
	grQuaternion		*Q)
	// multiplies q1 * q2, and places the result in q.
	// no failure. 	renormalization not automatic

{
	grQuaternion Q1L,Q2L;
	assert( grQuaternion_IsValid(Q1) != GR_FALSE );
	assert( grQuaternion_IsValid(Q2) != GR_FALSE );
	assert( Q  != NULL );
	Q1L = *Q1;
	Q2L = *Q2;

	Q->W  =	(  (Q1L.W*Q2L.W) - (Q1L.X*Q2L.X) 
			 - (Q1L.Y*Q2L.Y) - (Q1L.Z*Q2L.Z) );

	Q->X  =	(  (Q1L.W*Q2L.X) + (Q1L.X*Q2L.W) 
			 + (Q1L.Y*Q2L.Z) - (Q1L.Z*Q2L.Y) );

	Q->Y  =	(  (Q1L.W*Q2L.Y) - (Q1L.X*Q2L.Z) 
			 + (Q1L.Y*Q2L.W) + (Q1L.Z*Q2L.X) );

	Q->Z  = (  (Q1L.W*Q2L.Z) + (Q1L.X*Q2L.Y) 
			 - (Q1L.Y*Q2L.X) + (Q1L.Z*Q2L.W) );
	grQuaternion_Assert( grQuaternion_IsValid(Q) != GR_FALSE );

}


GRAPI void GRCC grQuaternion_Rotate(
	const grQuaternion	*Q, 
	const grVec3d         *V, 
	grVec3d				*VRotated)
	// Rotates V by the quaternion Q, places the result in VRotated.
{
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( grVec3d_IsValid(V)  != GR_FALSE );
	assert( VRotated  != NULL );

	grQuaternion_Assert( grQuaternion_IsUnit(Q) == GR_TRUE );

	{
		grQuaternion Qinv,QV,QRotated, QT;
		grFloat zero;
		grQuaternion_SetVec3d(&QV ,0.0f,V);
		grQuaternion_Inverse (Q,&Qinv);
		grQuaternion_Multiply(Q,&QV,&QT);
		grQuaternion_Multiply(&QT,&Qinv,&QRotated);
		grQuaternion_GetVec3d(&QRotated,&zero,VRotated);
	}
	
}



GRAPI grBoolean GRCC grQuaternion_IsUnit(const grQuaternion *Q)
	// returns GR_TRUE if Q is a unit grQuaternion.  GR_FALSE otherwise.
{
	grFloat magnitude;
	assert( Q != NULL );

	magnitude  =   (Q->W * Q->W) + (Q->X * Q->X) 
					  + (Q->Y * Q->Y) + (Q->Z * Q->Z);

	if (( magnitude < 1.0+UNIT_TOLERANCE ) && ( magnitude > 1.0-UNIT_TOLERANCE ))
		return GR_TRUE;
	return GR_FALSE;
}

GRAPI grFloat GRCC grQuaternion_Magnitude(const grQuaternion *Q)
	// returns Magnitude of Q.  
{

	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	return   (Q->W * Q->W) + (Q->X * Q->X)  + (Q->Y * Q->Y) + (Q->Z * Q->Z);
}


GRAPI grFloat GRCC grQuaternion_Normalize(grQuaternion *Q)
	// normalizes Q to be a unit grQuaternion
{
	grFloat magnitude,one_over_magnitude;
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	
	magnitude =   (grFloat) sqrt ((Q->W * Q->W) + (Q->X * Q->X) 
							  + (Q->Y * Q->Y) + (Q->Z * Q->Z));

	if (( magnitude < QZERO_TOLERANCE ) && ( magnitude > -QZERO_TOLERANCE ))
		{
			return 0.0f;
		}

	one_over_magnitude = 1.0f / magnitude;

	Q->W *= one_over_magnitude;
	Q->X *= one_over_magnitude;
	Q->Y *= one_over_magnitude;
	Q->Z *= one_over_magnitude;
	return magnitude;
}


GRAPI void GRCC grQuaternion_Copy(const grQuaternion *QSrc, grQuaternion *QDst)
	// copies quaternion QSrc into QDst
{
	assert( grQuaternion_IsValid(QSrc) != GR_FALSE );
	assert( QDst != NULL );
	*QDst = *QSrc;
}

GRAPI void GRCC grQuaternion_Inverse(const grQuaternion *Q, grQuaternion *QInv)
	// sets QInv to the inverse of Q.  
{
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( QInv != NULL );

	QInv->W =  Q->W;
	QInv->X = -Q->X;
	QInv->Y = -Q->Y;
	QInv->Z = -Q->Z;
}


GRAPI void GRCC grQuaternion_Add(
	const grQuaternion *Q1, 
	const grQuaternion *Q2, 
	grQuaternion *QSum)
	// QSum = Q1 + Q2  (result is not generally a unit quaternion!)
{
	assert( grQuaternion_IsValid(Q1) != GR_FALSE );
	assert( grQuaternion_IsValid(Q2) != GR_FALSE );
	assert( QSum != NULL );
	QSum->W = Q1->W + Q2->W;
	QSum->X = Q1->X + Q2->X;
	QSum->Y = Q1->Y + Q2->Y;
	QSum->Z = Q1->Z + Q2->Z;
}

GRAPI void GRCC grQuaternion_Subtract(
	const grQuaternion *Q1, 
	const grQuaternion *Q2, 
	grQuaternion *QSum)
	// QSum = Q1 - Q2  (result is not generally a unit quaternion!)
{
	assert( grQuaternion_IsValid(Q1) != GR_FALSE );
	assert( grQuaternion_IsValid(Q2) != GR_FALSE );
	assert( QSum != NULL );
	QSum->W = Q1->W - Q2->W;
	QSum->X = Q1->X - Q2->X;
	QSum->Y = Q1->Y - Q2->Y;
	QSum->Z = Q1->Z - Q2->Z;
}


#define ZERO_EPSILON (0.0001f)
 
GRAPI void GRCC grQuaternion_Ln(
	const grQuaternion *Q, 
	grQuaternion *LnQ)
	// ln(Q) for unit quaternion only!
{
	grFloat Theta;
	grQuaternion QL;
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( LnQ != NULL );
	grQuaternion_Assert( grQuaternion_IsUnit(Q) == GR_TRUE );
	
	if (Q->W < 0.0f)
		{
			QL.W = -Q->W;
			QL.X = -Q->X;
			QL.Y = -Q->Y;
			QL.Z = -Q->Z;
		}
	else
		{
			QL = *Q;
		}
	Theta    = (grFloat)  acos( QL.W  );
	 //  0 < Theta < pi
	if (Theta< ZERO_EPSILON)
		{
			// lim(t->0) of t/sin(t) = 1, so:
			LnQ->W = 0.0f;
			LnQ->X = QL.X;
			LnQ->Y = QL.Y;
			LnQ->Z = QL.Z;
		}
	else
		{
			grFloat Theta_Over_sin_Theta =  Theta / (grFloat) sin ( Theta );
			LnQ->W = 0.0f;
			LnQ->X = Theta_Over_sin_Theta * QL.X;
			LnQ->Y = Theta_Over_sin_Theta * QL.Y;
			LnQ->Z = Theta_Over_sin_Theta * QL.Z;
		}

}
	
GRAPI void GRCC grQuaternion_Exp(
	const grQuaternion *Q,
	grQuaternion *ExpQ)
	// exp(Q) for pure quaternion only!  (zero scalar part (W))
{
	grFloat Theta;
	grFloat sin_Theta_over_Theta;

	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( ExpQ != NULL);
	assert( Q->W == 0.0 );	//check a range?

	Theta = (grFloat) sqrt(Q->X*Q->X  +  Q->Y*Q->Y  +  Q->Z*Q->Z);
	if (Theta > ZERO_EPSILON)
		{
			sin_Theta_over_Theta = (grFloat) sin(Theta) / Theta;
		}
	else
		{
			sin_Theta_over_Theta = (grFloat) 1.0f;
		}

	ExpQ->W   = (grFloat) cos(Theta);
	ExpQ->X   = sin_Theta_over_Theta * Q->X;
	ExpQ->Y   = sin_Theta_over_Theta * Q->Y;
	ExpQ->Z   = sin_Theta_over_Theta * Q->Z;
}	

GRAPI void GRCC grQuaternion_Scale(
	const grQuaternion *Q,
	grFloat Scale,
	grQuaternion *QScaled)
	// Q = Q * Scale  (result is not generally a unit quaternion!)
{
	assert( grQuaternion_IsValid(Q) != GR_FALSE );
	assert( (Scale * Scale) >=0.0f );
	assert( QScaled != NULL);

	QScaled->W = Q->W * Scale;
	QScaled->X = Q->X * Scale;
	QScaled->Y = Q->Y * Scale;
	QScaled->Z = Q->Z * Scale;
}

GRAPI void GRCC grQuaternion_SetNoRotation(grQuaternion *Q)
	// sets Q to be a quaternion with no rotation (like an identity matrix)
{
	Q->W = 1.0f;
	Q->X = Q->Y = Q->Z = 0.0f;
	
	/* this is equivalent to 
		{	
			grXForm3d M;
			grXForm3d_SetIdentity(&M);
			grQuaternionFromMatrix(&M,Q);
		}
	*/
}



GRAPI grBoolean GRCC grQuaternion_Compare( grQuaternion *Q1, grQuaternion *Q2, grFloat Tolerance )
{
	assert( grQuaternion_IsValid(Q1) != GR_FALSE );
	assert( grQuaternion_IsValid(Q2) != GR_FALSE );
	assert ( Tolerance >= 0.0 );

	if (	// they are the same but with opposite signs
			(		(fabs(Q1->X + Q2->X) <= Tolerance )  
				&&  (fabs(Q1->Y + Q2->Y) <= Tolerance )  
				&&  (fabs(Q1->Z + Q2->Z) <= Tolerance )  
				&&  (fabs(Q1->W + Q2->W) <= Tolerance )  
			)
		  ||  // they are the same with same signs
			(		(fabs(Q1->X - Q2->X) <= Tolerance )  
				&&  (fabs(Q1->Y - Q2->Y) <= Tolerance )  
				&&  (fabs(Q1->Z - Q2->Z) <= Tolerance )  
				&&  (fabs(Q1->W - Q2->W) <= Tolerance )  
			)
		)
		return GR_TRUE;
	else
		return GR_FALSE;


	
}
