/****************************************************************************************/
/*  VEC3D.C                                                                             */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: 3D Vector implementation                                               */
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
#include <math.h>
#include <assert.h>

#include "grVec3d_Katmai.h"
#include "Vec3d.h"
#include "CPU.h"

#ifndef NDEBUG
GRAPI grFloat GRCC   grVec3d_GetElement(const grVec3d *V, int32 Index)
{
	assert( V != NULL );
	assert( Index >= 0 );
	assert( Index <  3 );
	return (* ((&((V)->X)) +  (Index) ));
}

GRAPI void GRCC grVec3d_SetElement(grVec3d *V, int32 Index, grFloat Value)
{
	assert( V != NULL );
	assert( Index >= 0 );
	assert( Index <  3 );
	
	(* ((&((V)->X)) +  (Index) )) = Value;
}

#endif

GRAPI grBoolean GRCC grVec3d_IsValid(const grVec3d *V)
{
	if (V == NULL)
		return GR_FALSE;
	if ((V->X * V->X) < 0.0f) 
		return GR_FALSE;
	if ((V->Y * V->Y) < 0.0f) 
		return GR_FALSE;
	if ((V->Z * V->Z) < 0.0f) 
		return GR_FALSE;
	return GR_TRUE;
}


GRAPI void GRCC		grVec3d_Set(grVec3d *V, grFloat X, grFloat Y, grFloat Z)
{
	assert ( V != NULL );
	V->X = X;
	V->Y = Y;
	V->Z = Z;
	assert( grVec3d_IsValid(V) );
}

GRAPI void GRCC		grVec3d_Get(const grVec3d *V, grFloat *X, grFloat *Y, grFloat *Z)
{
	assert ( V != NULL );
	assert ( X != NULL );
	assert ( Y != NULL );
	assert ( Z != NULL );
	assert( grVec3d_IsValid(V) );
	
	*X = V->X;
	*Y = V->Y;
	*Z = V->Z;
}


GRAPI grFloat GRCC	grVec3d_DotProduct(const grVec3d *V1, const grVec3d *V2)
{
	assert ( V1 != NULL );
	assert ( V2 != NULL );
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );
	
	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		return grVec3d_DotProduct_SSE(V1, V2);
		
	return(V1->X*V2->X + V1->Y*V2->Y + V1->Z*V2->Z);
}

GRAPI void GRCC grVec3d_CrossProduct(const grVec3d *V1, const grVec3d *V2, grVec3d *VResult)
{
	grVec3d Result;

	assert ( V1 != NULL );
	assert ( V2 != NULL );
	assert ( VResult != NULL );
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );

	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		grVec3d_CrossProduct_SSE(V1, V2, VResult);
	else
	{
		Result.X = V1->Y*V2->Z - V1->Z*V2->Y;
		Result.Y = V1->Z*V2->X - V1->X*V2->Z;
		Result.Z = V1->X*V2->Y - V1->Y*V2->X;

		*VResult = Result;
	}
}

GRAPI grBoolean GRCC grVec3d_Compare(const grVec3d *V1, const grVec3d *V2, grFloat Tolerance)
{
	assert ( V1 != NULL );
	assert ( V2 != NULL );
	assert ( Tolerance >= 0.0 );
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );

	if (fabs(V2->X - V1->X) > Tolerance)
		return GR_FALSE;
	if (fabs(V2->Y - V1->Y) > Tolerance)
		return GR_FALSE;
	if (fabs(V2->Z - V1->Z) > Tolerance)
		return GR_FALSE;

	return GR_TRUE;
}

GRAPI grFloat GRCC grVec3d_Normalize(grVec3d *V1)
{
	if (grCPU_Features & GR_CPU_HAS_KATMAI)
	{
		float len = grVec3d_Length(V1);
		grVec3d_Normalize_SSE(V1);
		
		return len;
	}
	else
	{
		grFloat OneOverDist;
		grFloat Dist;

		assert( grVec3d_IsValid(V1) );

		Dist = grVec3d_Length(V1);
		if (Dist == 0.0f)
			return 0.0f;

		OneOverDist = 1.0f/Dist;
	
		V1->X *= OneOverDist;
		V1->Y *= OneOverDist;
		V1->Z *= OneOverDist;

		return Dist;
	}
}

GRAPI grBoolean GRCC	grVec3d_IsNormalized(const grVec3d *V)
{
	grFloat	length;

	assert( grVec3d_IsValid(V) );

	length = grVec3d_Length(V);
	if ( fabs(length - 1.0f) < GR_EPSILON )
		return GR_TRUE;

	return GR_FALSE;
}

GRAPI void GRCC grVec3d_Scale(const grVec3d *VSrc, grFloat Scale, grVec3d *VDst)
{
	assert ( VDst != NULL );
	assert( grVec3d_IsValid(VSrc) );

	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		grVec3d_Scale_SSE(VSrc, Scale, VDst);
	else
	{
		VDst->X = VSrc->X * Scale;
		VDst->Y = VSrc->Y * Scale;
		VDst->Z = VSrc->Z * Scale;
	}

	assert( grVec3d_IsValid(VDst) );
}

GRAPI grFloat GRCC grVec3d_LengthSquared(const grVec3d *V1)
{
	return ( (V1)->X * (V1)->X + (V1)->Y * (V1)->Y + (V1)->Z * (V1)->Z );
}

GRAPI grFloat GRCC grVec3d_Length(const grVec3d *V1)
{	
	assert( grVec3d_IsValid(V1) );

	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		return grVec3d_Length_SSE(V1);
	else
		return grFloat_Sqrt(grVec3d_LengthSquared(V1));
}

GRAPI void GRCC grVec3d_Subtract(const grVec3d *V1, const grVec3d *V2, grVec3d *V1MinusV2)
{
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );
	assert ( V1MinusV2 != NULL );

	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		grVec3d_Subtract_SSE(V1, V2, V1MinusV2);
	else
	{
		V1MinusV2->X = V1->X - V2->X;
		V1MinusV2->Y = V1->Y - V2->Y;
		V1MinusV2->Z = V1->Z - V2->Z;
	}
}

GRAPI void GRCC grVec3d_Add(const grVec3d *V1, const grVec3d *V2, grVec3d *V1PlusV2)
{
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );
	assert ( V1PlusV2 != NULL );
	
	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		grVec3d_Add_SSE(V1, V2, V1PlusV2);
	else
	{
		V1PlusV2->X = V1->X + V2->X;
		V1PlusV2->Y = V1->Y + V2->Y;
		V1PlusV2->Z = V1->Z + V2->Z;
	}
}

GRAPI void GRCC grVec3d_MA(grVec3d *V1, grFloat Scale, const grVec3d *V2, grVec3d *V1PlusV2Scaled)
{
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );
	assert ( V1PlusV2Scaled != NULL );
	
	V1PlusV2Scaled->X = V1->X + V2->X*Scale;
	V1PlusV2Scaled->Y = V1->Y + V2->Y*Scale;
	V1PlusV2Scaled->Z = V1->Z + V2->Z*Scale;
}

GRAPI void GRCC grVec3d_AddScaled(const grVec3d *V1, const grVec3d *V2, grFloat Scale, grVec3d *V1PlusV2Scaled)
{
	assert( grVec3d_IsValid(V1) );
	assert( grVec3d_IsValid(V2) );
	assert ( V1PlusV2Scaled != NULL );
	
	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		grVec3d_AddScaled_SSE(V1, V2, Scale, V1PlusV2Scaled);
	else
	{
		V1PlusV2Scaled->X = V1->X + V2->X*Scale;
		V1PlusV2Scaled->Y = V1->Y + V2->Y*Scale;
		V1PlusV2Scaled->Z = V1->Z + V2->Z*Scale;
	}
}

GRAPI void GRCC grVec3d_Copy(const grVec3d *VSrc, grVec3d *VDst)
{
	assert ( VDst != NULL );
	assert( grVec3d_IsValid(VSrc) );
	
	*VDst = *VSrc;
}

GRAPI void GRCC grVec3d_Clear(grVec3d *V)
{
	assert ( V != NULL );
	
	V->X = 0.0f;
	V->Y = 0.0f;
	V->Z = 0.0f;
}

GRAPI void GRCC grVec3d_Inverse(grVec3d *V)
{
	assert( grVec3d_IsValid(V) );
	
	V->X = -V->X;
	V->Y = -V->Y;
	V->Z = -V->Z;
}

GRAPI grFloat GRCC	grVec3d_DistanceBetweenSquared(const grVec3d *V1, const grVec3d *V2)
{
float d,x;
	x = (V1->X - V2->X);
	d = x*x;
	x = (V1->Y - V2->Y);
	d+= x*x;
	x = (V1->Z - V2->Z);
	d+= x*x;
return d;
}

GRAPI grFloat GRCC	grVec3d_DistanceBetween(const grVec3d *V1, const grVec3d *V2)	// returns length of V1-V2	
{
	if (grCPU_Features & GR_CPU_HAS_KATMAI)
		return grVec3d_DistanceBetween_SSE(V1, V2);
	else
		return grFloat_Sqrt( grVec3d_DistanceBetweenSquared(V1,V2) );
}
