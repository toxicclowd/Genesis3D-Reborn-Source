/****************************************************************************************/
/*  QUATERN.H                                                                           */
/*                                                                                      */
/*  Author: Mike Sandige                                                                */
/*  Description: Quaternion mathematical system interface                               */
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
#ifndef GR_QUATERNION_H
#define GR_QUATERNION_H

/***************************************************************************

	the quatern module contains basic support for a quaternion object.

	quaternions are an extension of complex numbers that allows an
	expression for rotation that can be easily interpolated.  grQuaternion_s are also 
	more numericaly stable for repeated rotations than matrices.

	
	A quaternion is a 4 element 'vector'  [w,x,y,z] where:

	q = w + xi + yj + zk
	i*i = -1
	j*j = -1
	k*k = -1
	i*j = -j*i = k
	j*k = -k*j = i
	k*i = -i*k = j
	q' (conjugate) = w - xi - yj - zk
	||q|| (magnitude) = sqrt(q*q') = sqrt(w*w + x*x + y*y + z*z)
	unit quaternion ||q|| == 1; this implies  q' == qinverse 
	quaternions are associative (q1*q2)*q3 == q1*(q2*q3)
	quaternions are not commutative  q1*q2 != q2*q1
	qinverse (inverse (1/q) ) = q'/(q*q')
	
	q can be expressed by w + xi + yj + zk or [w,x,y,z] 
	or as in this implementation (s,v) where s=w, and v=[x,y,z]

	quaternions can represent a rotation.  The rotation is an angle t, around a 
	unit vector u.   q=(s,v);  s= cos(t/2);   v= u*sin(t/2).

	quaternions can apply the rotation to a point.  let the point be p [px,py,pz],
	and let P be a quaternion(0,p).  Protated = q*P*qinverse 
	( Protated = q*P*q' if q is a unit quaternion)

	concatenation rotations is similar to matrix concatenation.  given two rotations
	q1 and q2,  to rotate by q1, then q2:  let qc = (q2*q1), then the combined 
	rotation is given by qc*P*qcinverse (= qc*P*qc' if q is a unit quaternion)

	multiplication: 
	q1 = w1 + x1i + y1j + z1k
	q2 = w2 + x2i + y2j + z2k
	q1*q2 = q3 =
			(w1*w2 - x1*x2 - y1*y2 - z1*z2)     {w3}
	        (w1*x2 + x1*w2 + y1*z2 - z1*y2)i	{x3}
			(w1*y2 - x1*z2 + y1*w2 + z1*x2)j    {y3}
			(w1*z2 + x1*y2 + y1*x2 + z1*w2)k	{z3}

	also, 
	q1 = (s1,v1) = [s1,(x1,y1,z1)]
	q2 = (s2,v2) = [s2,(x2,y2,z2)]
	q1*q2 = q3	=	(s1*s2 - dot_product(v1,v2),			{s3}
					(s1*v2 + s2*v1 + cross_product(v1,v2))	{v3}


	interpolation - it is possible (and sometimes reasonable) to interpolate between
	two quaternions by interpolating each component.  This does not quarantee a 
	resulting unit quaternion, and will result in an animation with non-linear 
	rotational velocity.

	spherical interpolation: (slerp) treat the quaternions as vectors 
	find the angle between them (w = arccos(q1 dot q2) ).
	given 0<=t<=1,  q(t) = q1*(sin((1-t)*w)/sin(w) + q2 * sin(t*w)/sin(w).
	since q == -q, care must be taken to rotate the proper way.  

	this implementation uses the notation quaternion q = (quatS,quatV) 
	  where quatS is a scalar, and quatV is a 3 element vector.

********************************************/

#include "BaseType.h"
#include "Xform3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
	grFloat	W;
	grFloat   X,Y,Z;
	//grVec3d   QuatV;
} grQuaternion;


#define	QUATERNION_PI	(GR_PI)

GRAPI grBoolean GRCC grQuaternion_IsValid( const grQuaternion *Q );
	// return GR_TRUE if Q is non null and for has no NAN's in its components

GRAPI void GRCC grQuaternion_Set( grQuaternion *Q, grFloat W, grFloat X, grFloat Y, grFloat Z);
	// set quaternion components.  Doesn't normalize
GRAPI void GRCC grQuaternion_SetVec3d( grQuaternion *Q, grFloat W, const grVec3d *V);
	// set quaternion components.  Doesn't normalize
GRAPI void GRCC grQuaternion_SetFromAxisAngle(grQuaternion *Q, const grVec3d *Axis, grFloat Theta);
	// set a quaternion from an axis and a rotation around the axis
GRAPI grBoolean GRCC grQuaternion_GetAxisAngle(const grQuaternion *Q, grVec3d *Axis, grFloat *Theta);
	// gets an axis and angle of rotation around the axis from a quaternion
	// returns GR_TRUE if there is an axis.  
	// returns GR_FALSE if there is no axis (and Axis is set to 0,0,0, and Theta is 0)

GRAPI void GRCC grQuaternion_Get( const grQuaternion *Q, 
					grFloat *W, grFloat *X, grFloat *Y, grFloat *Z);
	// get quaternion components into W,X,Y,Z
GRAPI void GRCC grQuaternion_GetVec3d( const grQuaternion *Q, grFloat *W, grVec3d *V);
	// get quaternion components into W and V

GRAPI void GRCC grQuaternion_FromMatrix(
	const grXForm3d		*RotationMatrix,
	      grQuaternion	*QDest);
	// takes upper 3 by 3 portion of matrix (rotation sub matrix) 
	// and generates a quaternion

GRAPI void GRCC grQuaternion_ToMatrix(
	const grQuaternion	*Q, 
		  grXForm3d		*RotationMatrixDest);
	// takes a unit quaternion and makes RotationMatrixDest an equivelant rotation xform.
	// (any translation in RotationMatrixDest will be list)

GRAPI void GRCC grQuaternion_Slerp(
	const grQuaternion		*Q0, 
	const grQuaternion		*Q1, 
	grFloat					T,		
	grQuaternion			*QT);
	// spherical interpolation between q0 and q1.   0<=t<=1 
	// resulting quaternion is 'between' q0 and q1
	// with t==0 being all q0, and t==1 being all q1.
	// returns a quaternion with a positive W - always takes shortest route
	// through the positive W domain.

GRAPI void GRCC grQuaternion_SlerpNotShortest(
	const grQuaternion		*Q0, 
	const grQuaternion		*Q1, 
	grFloat					T,		
	grQuaternion			*QT);
	// spherical interpolation between q0 and q1.   0<=t<=1 
	// resulting quaternion is 'between' q0 and q1
	// with t==0 being all q0, and t==1 being all q1.


GRAPI void GRCC grQuaternion_Multiply(
	const grQuaternion	*Q1, 
	const grQuaternion	*Q2, 
	grQuaternion			*QProduct);
	// multiplies q1 * q2, and places the result in q.
	// no failure. 	renormalization not automatic

GRAPI void GRCC grQuaternion_Rotate(
	const grQuaternion	*Q, 
	const grVec3d       *V, 
	grVec3d				*VRotated);
	// Rotates V by the quaternion Q, places the result in VRotated.

GRAPI grBoolean GRCC grQuaternion_IsUnit(const grQuaternion *Q);
	// returns GR_TRUE if q is a unit quaternion.  GR_FALSE otherwise.

GRAPI grFloat GRCC grQuaternion_Normalize(grQuaternion *Q);
	// normalizes q to be a unit quaternion.  returns original magnitude of q

GRAPI void GRCC grQuaternion_Copy(const grQuaternion *QSrc, grQuaternion *QDst);
	// copies quaternion QSrc into QDst

GRAPI void GRCC grQuaternion_SetNoRotation(grQuaternion *Q);
	// sets Q to be a quaternion with no rotation (like an identity matrix)

GRAPI void GRCC grQuaternion_Ln(
	const grQuaternion *Q, 
	grQuaternion *LnQ);
	// ln(Q) for unit quaternion only!

GRAPI void GRCC grQuaternion_Exp(
	const grQuaternion *Q,
	grQuaternion *ExpQ);
	// exp(Q) for pure quaternion only!  (zero scalar part (W))

GRAPI void GRCC grQuaternion_Scale(
	const grQuaternion *Q,
	grFloat Scale,
	grQuaternion *QScaled);
	// Q = Q * Scale  (result is not generally a unit quaternion!)

GRAPI void GRCC grQuaternion_Add(
	const grQuaternion *Q1,
	const grQuaternion *Q2,
	grQuaternion *QSum);
	// QSum = Q1 + Q2  (result is not generally a unit quaternion!)

GRAPI void GRCC grQuaternion_Subtract(
	const grQuaternion *Q1, 
	const grQuaternion *Q2, 
	grQuaternion *QSum);
	// QSum = Q1 - Q2  (result is not generally a unit quaternion!)

GRAPI void GRCC grQuaternion_Inverse(const grQuaternion *Q, grQuaternion *QInv);
	// sets QInv to the inverse of Q.  

GRAPI grFloat GRCC grQuaternion_Magnitude(const grQuaternion *Q);
	// returns Magnitude of Q.  

GRAPI grBoolean GRCC grQuaternion_Compare( grQuaternion *Q1, grQuaternion *Q2, grFloat Tolerance );
	// return GR_TRUE if quaternions differ elementwise by less than Tolerance.


#ifndef NDEBUG
GRAPI void GRCC grQuaternion_SetMaximalAssertionMode( grBoolean Enable );
#endif

#ifdef __cplusplus
}
#endif

#endif
