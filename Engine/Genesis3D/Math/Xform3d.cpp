/****************************************************************************************/
/*  XFORM3D.C                                                                           */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: 3D transform implementation                                            */
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

#include "XForm3d.h"
#include "asmXForm3d.h"
#include "cpu.h"

#include "memory.h"
#pragma intrinsic(memcpy)

// Krouer: file to monitor cycle perfs
//#include "iaperf.h"


#ifndef NDEBUG
	static grBoolean grXForm3d_MaximalAssertionMode = GR_TRUE;
	#define grXForm3d_Assert if (grXForm3d_MaximalAssertionMode) assert

GRAPI 	void GRCC grXForm3d_SetMaximalAssertionMode( grBoolean Enable )
	{
		assert( (Enable == GR_TRUE) || (Enable == GR_FALSE) );
		grXForm3d_MaximalAssertionMode = Enable;
	}
#else
	#define grXForm3d_Assert(x)
#endif


GRAPI grBoolean GRCC grXForm3d_IsValid(const grXForm3d *M)
	// returns GR_TRUE if M is 'valid'  
	// 'valid' means that M is non NULL, and there are no NAN's in the matrix.
{

	if (M == NULL)
		return GR_FALSE;
	if (grVec3d_IsValid(&(M->Translation)) == GR_FALSE)
		return GR_FALSE;

	if ((M->AX * M->AX) < 0.0f) 
		return GR_FALSE;
	if ((M->AY * M->AY) < 0.0f) 
		return GR_FALSE;
	if ((M->AZ * M->AZ) < 0.0f) 
		return GR_FALSE;

	if ((M->BX * M->BX) < 0.0f) 
		return GR_FALSE;
	if ((M->BY * M->BY) < 0.0f) 
		return GR_FALSE;
	if ((M->BZ * M->BZ) < 0.0f) 
		return GR_FALSE;
	
	if ((M->CX * M->CX) < 0.0f) 
		return GR_FALSE;
	if ((M->CY * M->CY) < 0.0f) 
		return GR_FALSE;
	if ((M->CZ * M->CZ) < 0.0f) 
		return GR_FALSE;

	return GR_TRUE;
}




GRAPI void GRCC grXForm3d_SetIdentity(grXForm3d *M)
	// sets M to an identity matrix (clears it)
{
	assert( M != NULL );			
	
	M->AX = M->BY = M->CZ = 1.0f;
	M->AY = M->AZ = M->BX = M->BZ = M->CX = M->CY = 0.0f;
	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}
	
GRAPI void GRCC grXForm3d_SetXRotation(grXForm3d *M,grFloat RadianAngle)
	// sets up a transform that rotates RadianAngle about X axis
{
	grFloat Cos,Sin;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );

	Cos = (grFloat)cos(RadianAngle);
	Sin = (grFloat)sin(RadianAngle);
	M->BY =  Cos;
	M->BZ = -Sin;
	M->CY =  Sin;
	M->CZ =  Cos;
	M->AX = 1.0f;
	M->AY = M->AZ = M->BX = M->CX = 0.0f;
	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}  
	
GRAPI void GRCC grXForm3d_SetYRotation(grXForm3d *M,grFloat RadianAngle)
	// sets up a transform that rotates RadianAngle about Y axis
{
	grFloat Cos,Sin;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );

	Cos = (grFloat)cos(RadianAngle);
	Sin = (grFloat)sin(RadianAngle);
	
	M->AX =  Cos;
	M->AZ =  Sin;
	M->CX = -Sin;
	M->CZ =  Cos;
	M->BY = 1.0f;
	M->AY = M->BX = M->BZ = M->CY = 0.0f;
	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_SetZRotation(grXForm3d *M,grFloat RadianAngle)
	// sets up a transform that rotates RadianAngle about Z axis
{
	grFloat Cos,Sin;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );

	Cos = (grFloat)cos(RadianAngle);
	Sin = (grFloat)sin(RadianAngle);
	
	M->AX =  Cos;
	M->AY = -Sin;
	M->BX =  Sin;
	M->BY =  Cos;
	M->CZ = 1.0f;
	M->AZ = M->BZ = M->CX = M->CY = 0.0f;
	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_SetTranslation(grXForm3d *M,grFloat x, grFloat y, grFloat z)
	// sets up a transform that translates x,y,z
{
	assert( M != NULL );

	M->Translation.X = x;
	M->Translation.Y = y;
	M->Translation.Z = z;
	assert( grVec3d_IsValid(&M->Translation)!=GR_FALSE);

	M->AX = M->BY = M->CZ = 1.0f;
	M->AY = M->AZ = 0.0f;
	M->BX = M->BZ = 0.0f;
	M->CX = M->CY = 0.0f;
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_SetScaling(grXForm3d *M,grFloat x, grFloat y, grFloat z)
	// sets up a transform that scales by x,y,z
{
	assert( M != NULL );
	assert( x * x >= 0.0f);
	assert( y * y >= 0.0f);
	assert( z * z >= 0.0f);
	assert( x > GEXFORM3D_MINIMUM_SCALE );
	assert( y > GEXFORM3D_MINIMUM_SCALE );
	assert( z > GEXFORM3D_MINIMUM_SCALE );


	M->AX = x;
	M->BY = y;
	M->CZ = z;

	M->AY = M->AZ = 0.0f;
	M->BX = M->BZ = 0.0f;
	M->CX = M->CY = 0.0f;
	M->Translation.X = M->Translation.Y = M->Translation.Z = 0.0f;

	//If any of the scale values are non-equal than the transform is nonorthogonal
	if( x != y  )
		M->Flags = XFORM3D_NONORTHOGONALISOK;	
	else
	if( x != z )
		M->Flags = XFORM3D_NONORTHOGONALISOK;
	else
	{
		M->Flags = 0;
		grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
	}
}

GRAPI void GRCC grXForm3d_RotateX(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about X axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetXRotation(&R,RadianAngle);
	grXForm3d_Multiply(&R, M, M);
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_RotateY(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about Y axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetYRotation(&R,RadianAngle);
	grXForm3d_Multiply(&R, M, M);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_RotateZ(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about Z axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetZRotation(&R,RadianAngle);
	grXForm3d_Multiply(&R,M,M);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_PostRotateX(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about X axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetXRotation(&R,RadianAngle);
	grXForm3d_Multiply(M,&R,M);
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_PostRotateY(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about Y axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetYRotation(&R,RadianAngle);
	grXForm3d_Multiply(M,&R,M);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_PostRotateZ(grXForm3d *M,grFloat RadianAngle)
	// Rotates M by RadianAngle about Z axis
{
	grXForm3d R;
	assert( M != NULL );
	assert( RadianAngle * RadianAngle >= 0.0f );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetZRotation(&R,RadianAngle);
	grXForm3d_Multiply(M,&R,M);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_Translate(grXForm3d *M,grFloat x, grFloat y, grFloat z)
	// Translates M by x,y,z
{
	grXForm3d T;
	assert( M != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetTranslation(&T,x,y,z);
	grXForm3d_Multiply(&T, M, M);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_Scale(grXForm3d *M,grFloat x, grFloat y, grFloat z)
	// Scales M by x,y,z
{
	grXForm3d S;
	assert( M != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	grXForm3d_SetScaling(&S,x,y,z);
	grXForm3d_Multiply(&S, M, M);
	//If any of the scale values are non-equal than the transform is nonorthogonal
	if( x != y  )
		M->Flags = XFORM3D_NONORTHOGONALISOK;	
	else
	if( x != z )
		M->Flags = XFORM3D_NONORTHOGONALISOK;
	else
	{
		grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
	}

}

GRAPI void GRCC grXForm3d_Multiply(
	const grXForm3d *M1, 
	const grXForm3d *M2, 
	grXForm3d *MProduct)
	// MProduct = matrix multiply of M1*M2
{
grXForm3d MProductL;
	assert( M1       != NULL );
	assert( M2       != NULL );
	assert( MProduct != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M1) == GR_TRUE );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M2) == GR_TRUE );

	MProductL.AX = M1->AX * M2->AX + M1->AY * M2->BX + M1->AZ * M2->CX;
	MProductL.AY = M1->AX * M2->AY + M1->AY * M2->BY + M1->AZ * M2->CY;
	MProductL.AZ = M1->AX * M2->AZ + M1->AY * M2->BZ + M1->AZ * M2->CZ;

	MProductL.BX = M1->BX * M2->AX + M1->BY * M2->BX + M1->BZ * M2->CX;
	MProductL.BY = M1->BX * M2->AY + M1->BY * M2->BY + M1->BZ * M2->CY;
	MProductL.BZ = M1->BX * M2->AZ + M1->BY * M2->BZ + M1->BZ * M2->CZ;

	MProductL.CX = M1->CX * M2->AX + M1->CY * M2->BX + M1->CZ * M2->CX;
	MProductL.CY = M1->CX * M2->AY + M1->CY * M2->BY + M1->CZ * M2->CY;
	MProductL.CZ = M1->CX * M2->AZ + M1->CY * M2->BZ + M1->CZ * M2->CZ;

	MProductL.Translation.X =  M1->AX * M2->Translation.X
							 + M1->AY * M2->Translation.Y
							 + M1->AZ * M2->Translation.Z
							 + M1->Translation.X;

	MProductL.Translation.Y =  M1->BX * M2->Translation.X
							 + M1->BY * M2->Translation.Y
							 + M1->BZ * M2->Translation.Z
							 + M1->Translation.Y;

	MProductL.Translation.Z =  M1->CX * M2->Translation.X
							 + M1->CY * M2->Translation.Y
							 + M1->CZ * M2->Translation.Z
							 + M1->Translation.Z;

	MProductL.Flags = ( ( M1->Flags | M2->Flags ) & XFORM3D_NONORTHOGONALISOK );
	
	*MProduct = MProductL;

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(MProduct) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_Transform(
	const grXForm3d *M,
	const grVec3d *V, 
	grVec3d *Result)
	// Result is Matrix M * Vector V:  V Tranformed by M 
{
	grVec3d VL;
	assert( M != NULL );
	assert( grVec3d_IsValid(V)!=GR_FALSE);

	assert( Result != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	VL = *V;

	Result->X = (VL.X * M->AX) + (VL.Y * M->AY) + (VL.Z * M->AZ) + M->Translation.X;
	Result->Y = (VL.X * M->BX) + (VL.Y * M->BY) + (VL.Z * M->BZ) + M->Translation.Y;
	Result->Z = (VL.X * M->CX) + (VL.Y * M->CY) + (VL.Z * M->CZ) + M->Translation.Z;
	grXForm3d_Assert( grVec3d_IsValid(Result)!=GR_FALSE);

}


typedef void (GRCC *XFMVECARRAY)(const grXForm3d *XForm, const grVec3d *Source, grVec3d *Dest, int32 Count);
static	XFMVECARRAY	XFormVecArray	=NULL;	//does this guarantee first time null?

typedef void (GRCC *XFMARRAY)(const grXForm3d *XForm, const grVec3d *Source, grVec3d *Dest, int32 SourceStride, int32 DestStride, int32 Count);
static	XFMARRAY	XFormArray	=NULL;	//does this guarantee first time null?

GRAPI	grBoolean	GRCC	grXForm3d_UsingKatmai(void) 
{
	return (XFormVecArray==grXForm3d_TransformVecArrayKatmai);
}

GRAPI	grBoolean	GRCC	grXForm3d_EnableKatmai(grBoolean useit) 
{
	if(grCPU_Features & GR_CPU_HAS_KATMAI)
	{
		if(useit)
		{
			XFormVecArray	=grXForm3d_TransformVecArrayKatmai;
		}
		else
		{
			XFormVecArray	=grXForm3d_TransformVecArrayX86;
		}
		return	GR_TRUE;
	}

	return	GR_FALSE;
}

//========================================================================================
//	grXForm3d_TransformVecArray
//	Calls the correct version (if no flags are set goes to x86)
//========================================================================================
GRAPI void GRCC grXForm3d_TransformVecArray(const grXForm3d *XForm, 
	const grVec3d *Source, grVec3d *Dest, int32 Count)
{

#if 0 // @@
	grXForm3d_TransformArray(XForm,Source,sizeof(grVec3d),Dest,sizeof(grVec3d),Count);
#endif

	if(XFormVecArray)
	{
		XFormVecArray(XForm, Source, Dest, Count);	
	}
	else
	{
		if(grCPU_Features & GR_CPU_HAS_KATMAI)
		{
			XFormVecArray	=grXForm3d_TransformVecArrayKatmai;
		}
#if 0	//darn no native 3dnow 
		else if(grCPU_Features & GR_CPU_HAS_3DNOW)
		{
			XFormVecArray	=grXForm3d_TransformVecArray3DNow;
		}
#endif
		else
		{
			XFormVecArray	=grXForm3d_TransformVecArrayX86;
		}
		
		XFormVecArray(XForm, Source, Dest, Count);
	}
}

//========================================================================================
//	grXForm3d_TransformArray (allows strides)
//	Calls the correct version (if no flags are set goes to x86)
//========================================================================================
GRAPI void GRCC grXForm3d_TransformArray(const grXForm3d *XForm,
												   const grVec3d *Source,
													   int32 SourceStride,
												   grVec3d *Dest,
													   int32 DestStride,
												   int32 Count)
{
	if(XFormArray)
	{
		XFormArray(XForm, Source, Dest, SourceStride, DestStride, Count);	
	}
	else
	{
		if(grCPU_Features & GR_CPU_HAS_KATMAI)
		{
			XFormArray	=grXForm3d_TransformArrayKatmai;
		}
		#pragma message("XForm3d : Get the 3dnow XFormArray integrated!")
#if 0	//darn no native 3dnow 
		else if(grCPU_Features & GR_CPU_HAS_3DNOW)
		{
			XFormArray	=grXForm3d_TransformArray3DNow;
		}
#endif
		else
		{
			XFormArray	=grXForm3d_TransformArrayX86;
		}
		
		XFormArray(XForm, Source, Dest, SourceStride, DestStride, Count);
	}
}

GRAPI void GRCC grXForm3d_Rotate(
	const grXForm3d *M,
	const grVec3d *V, 
	grVec3d *Result)
	// Result is Matrix M * Vector V:  V Rotated by M (no translation)
{
	grVec3d VL;
	assert( M != NULL );
	assert( Result != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
	assert( grVec3d_IsValid(V)!=GR_FALSE);

	VL = *V;

	Result->X = (VL.X * M->AX) + (VL.Y * M->AY) + (VL.Z * M->AZ);
	Result->Y = (VL.X * M->BX) + (VL.Y * M->BY) + (VL.Z * M->BZ);
	Result->Z = (VL.X * M->CX) + (VL.Y * M->CY) + (VL.Z * M->CZ);
	grXForm3d_Assert( grVec3d_IsValid(Result)!=GR_FALSE);
}


GRAPI void GRCC grXForm3d_GetLeft(const grXForm3d *M, grVec3d *Left)
	// Gets a vector that is 'left' in the frame of reference of M (facing -Z)
{
	assert( M     != NULL );
	assert( Left != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
	
	Left->X = -M->AX;
	Left->Y = -M->BX;
	Left->Z = -M->CX;
	grXForm3d_Assert( grVec3d_IsValid(Left)!=GR_FALSE);
}

GRAPI void GRCC grXForm3d_GetUp(const grXForm3d *M,    grVec3d *Up)
	// Gets a vector that is 'up' in the frame of reference of M (facing -Z)
{
	assert( M  != NULL );
	assert( Up != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );
	
	Up->X = M->AY;
	Up->Y = M->BY;
	Up->Z = M->CY;
	grXForm3d_Assert( grVec3d_IsValid(Up)!=GR_FALSE);
}

GRAPI void GRCC grXForm3d_GetIn(const grXForm3d *M,  grVec3d *In)
	// Gets a vector that is 'in' in the frame of reference of M (facing -Z)
{
	assert( M    != NULL );
	assert( In != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	In->X = -M->AZ;
	In->Y = -M->BZ;
	In->Z = -M->CZ;
	grXForm3d_Assert( grVec3d_IsValid(In)!=GR_FALSE);
}

// KROUER : start of inverse matrix replace
#define _USE_OLD_INVERSE 0	// set it to 1 to use Gaussian method
#if _USE_OLD_INVERSE		// old inverse matrice using Gaussian and 3x3 matrice
typedef struct
{
	float x[3][3];
}	Matrix33;

void Matrix33_Copy(const Matrix33* m, Matrix33* c)
{
//	int i, j;

	assert(m != NULL);
	assert(c != NULL);

/*  // Krouer: optimisation
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			c->x[i][j] = m->x[i][j];
*/
	memcpy(c->x, m->x, 9 * sizeof(float));
}

void Matrix33_SetIdentity(Matrix33* m)
{
//	int i, j;

	assert(m != NULL);

/*  // Krouer: optimisation
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
		{
			if (i == j) 
				m->x[i][j] = 1.f;
			else m->x[i][j] = 0.f;
		}
*/
	m->x[0][0] = m->x[1][1] = m->x[2][2] = 1.f;
	m->x[0][1] = m->x[0][2] = 0.f;
	m->x[1][0] = m->x[1][2] = 0.f;
	m->x[2][0] = m->x[2][1] = 0.f;
}

void Matrix33_SwapRows(Matrix33* m,int r1,int r2)
{
int i;
	for(i=0;i<3;i++)
	{
	float temp;
		temp		= m->x[r1][i];
		m->x[r1][i] = m->x[r2][i];
		m->x[r2][i] = temp;
	}
}

void Matrix33_GetInverse(const Matrix33* m, Matrix33* inv)
{
	int i, j, k;
	Matrix33 copy;

	assert(m != NULL);
	assert(inv != NULL);

	Matrix33_Copy(m, &copy);
	Matrix33_SetIdentity(inv);

	for (i = 0; i < 3; i++)
	{
	float bigv;
		// first find the row with the largest coefficient
		k = i;
		bigv = copy.x[i][i];
		bigv = GR_ABS(bigv);
		for(j = i+1;j<3;j++)
		{
		float v;
			v = copy.x[j][i];
			if ( GR_ABS(v) > GR_ABS(bigv) )
			{
				k = j;
				bigv = v;
			}
		}

		// now row k has the largest value (bigv) in column i
		if ( k != i )
		{
			Matrix33_SwapRows(&copy,i,k);
			Matrix33_SwapRows(inv,i,k);
		}

		assert(GR_ABS(bigv) >= 1e-5);

		for (j = 0; j < 3; j++)
		{
			inv->x[i][j] /= bigv;
			copy.x[i][j] /= bigv;
		}

		for (j = 0; j < 3; j++)
		{
			if (j != i)
			{
				float mulby = copy.x[j][i];
				if ( mulby != 0.0f)
				{	
					for (k = 0; k < 3; k++)
					{
						copy.x[j][k] -= mulby * copy.x[i][k];
						inv->x[j][k] -= mulby * inv->x[i][k];
					}
				}
			}
		}
	}
}

void Matrix33_ExtractFromXForm3d(Matrix33* m, const grXForm3d* xform)
{
	assert(xform != NULL);
	assert(m != NULL);

	m->x[0][0] = xform->AX; m->x[0][1] = xform->AY; m->x[0][2] = xform->AZ;
	m->x[1][0] = xform->BX; m->x[1][1] = xform->BY; m->x[1][2] = xform->BZ;
	m->x[2][0] = xform->CX; m->x[2][1] = xform->CY; m->x[2][2] = xform->CZ;
}

void grXForm3d_ExtractFromMatrix33(grXForm3d* xform, const Matrix33* m)
{
	assert(xform != NULL);
	assert(m != NULL);

	grVec3d_Clear(&xform->Translation);

	xform->AX = m->x[0][0]; xform->AY = m->x[0][1]; xform->AZ = m->x[0][2];
	xform->BX = m->x[1][0]; xform->BY = m->x[1][1]; xform->BZ = m->x[1][2];
	xform->CX = m->x[2][0]; xform->CY = m->x[2][1]; xform->CZ = m->x[2][2];
}

GRAPI void GRCC grXForm3d_GetInverse(const grXForm3d *M, grXForm3d *MInv)
{
	Matrix33	Matrix, InvMatrix;

	MInv->Flags = M->Flags;
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	Matrix33_ExtractFromXForm3d(&Matrix, M);
	Matrix33_GetInverse(&Matrix, &InvMatrix);

	grXForm3d_ExtractFromMatrix33(MInv, &InvMatrix);

	{
		grXForm3d T;
		grXForm3d_SetTranslation(&T,-M->Translation.X,-M->Translation.Y,-M->Translation.Z);
		grXForm3d_Multiply(MInv,&T,MInv);
	}
}
#else // New inverse matrice algo using Cramer method

GRAPI void GRCC grXForm3d_GetInverse(const grXForm3d *M, grXForm3d *MInv)
{
	float det;
	float cofactors[9];

	// initialize the cofactors
	cofactors[0] = M->BY*M->CZ - M->BZ*M->CY;
	cofactors[1] = M->AZ*M->CY - M->AY*M->CZ;
	cofactors[2] = M->AY*M->BZ - M->AZ*M->BY;
	cofactors[3] = M->BZ*M->CX - M->BX*M->CZ;
	cofactors[4] = M->AX*M->CZ - M->AZ*M->CX;
	cofactors[5] = M->AZ*M->BX - M->AX*M->BZ;
	cofactors[6] = M->BX*M->CY - M->BY*M->CX;
	cofactors[7] = M->AY*M->CX - M->AX*M->CY;
	cofactors[8] = M->AX*M->BY - M->AY*M->BX;

	det = M->AX*cofactors[0] + M->BX*cofactors[1] + M->CX*cofactors[2];
	det = 1.f / det;

	MInv->AX = cofactors[0]*det;
	MInv->AY = cofactors[1]*det;
	MInv->AZ = cofactors[2]*det;

	MInv->BX = cofactors[3]*det;
	MInv->BY = cofactors[4]*det;
	MInv->BZ = cofactors[5]*det;

	MInv->CX = cofactors[6]*det;
	MInv->CY = cofactors[7]*det;
	MInv->CZ = cofactors[8]*det;

	MInv->Translation.X = 0;//-M->Translation.X;
	MInv->Translation.Y = 0;//-M->Translation.Y;
	MInv->Translation.Z = 0;//-M->Translation.Z;

	{
		grXForm3d T;
		grXForm3d_SetTranslation(&T,-M->Translation.X,-M->Translation.Y,-M->Translation.Z);
		grXForm3d_Multiply(MInv,&T,MInv);
	}
}
#endif // End Inverse matrix by KROUER

GRAPI void GRCC grXForm3d_GetTranspose(const grXForm3d *M, grXForm3d *MInv)
{
	grXForm3d M1;
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	M1 = *M;

	MInv->AX = M1.AX;
	MInv->AY = M1.BX;
	MInv->AZ = M1.CX;

	MInv->BX = M1.AY;
	MInv->BY = M1.BY;
	MInv->BZ = M1.CY;

	MInv->CX = M1.AZ;
	MInv->CY = M1.BZ;
	MInv->CZ = M1.CZ;

	MInv->Translation.X = 0.0f;
	MInv->Translation.Y = 0.0f;
	MInv->Translation.Z = 0.0f;

	MInv->Flags = M->Flags;

/*****

this is the same as:

	CXForm->Translation = MXForm->Translation;

	grVec3d_Inverse(&CXForm->Translation);

	// Rotate the translation in the new camera matrix
	grXForm3d_Rotate(CXForm, &CXForm->Translation, &CXForm->Translation);


******/
	{
		grXForm3d T;
		grXForm3d_SetTranslation(&T,-M1.Translation.X,-M1.Translation.Y,-M1.Translation.Z);
		grXForm3d_Multiply(MInv,&T,MInv);
	}

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(MInv) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_TransposeTransform(
	const grXForm3d *M, 
	const grVec3d *V, 
	grVec3d *Result)
	// applies the Transpose transform of M to V.  Result = (M^T) * V
{
	grVec3d V1;

	assert( M      != NULL );
	assert( grVec3d_IsValid(V)!=GR_FALSE);

	assert( Result != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(M) == GR_TRUE );

	V1.X = V->X - M->Translation.X;
	V1.Y = V->Y - M->Translation.Y;
	V1.Z = V->Z - M->Translation.Z;

	Result->X = (V1.X * M->AX) + (V1.Y * M->BX) + (V1.Z * M->CX);
	Result->Y = (V1.X * M->AY) + (V1.Y * M->BY) + (V1.Z * M->CY);
	Result->Z = (V1.X * M->AZ) + (V1.Y * M->BZ) + (V1.Z * M->CZ);
	grXForm3d_Assert( grVec3d_IsValid(Result)!=GR_FALSE);
}


GRAPI void GRCC grXForm3d_Copy(
	const grXForm3d *Src, 
	grXForm3d *Dst)
{	
	assert( Src != NULL );
	assert( Dst != NULL );
	grXForm3d_Assert ( grXForm3d_IsOrthogonal(Src) == GR_TRUE );

	*Dst = *Src;
}    

GRAPI void GRCC grXForm3d_GetEulerAngles(const grXForm3d *M, grVec3d *Angles)
	// order of angles z,y,x
{
	grFloat AZ;
	assert( M      != NULL );
	assert( Angles != NULL );

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
	
	//ack.  due to floating point error, the value can drift away from 1.0 a bit
	//      this will clamp it.  The _IsOrthonormal test will pass because it allows
	//      for a tolerance.

	AZ = M->AZ;
	if (AZ > 1.0f) 
		AZ = 1.0f;
	if (AZ < -1.0f) 
		AZ = -1.0f;

	Angles->Y = -(grFloat)asin(-AZ);

	if ( cos(Angles->Y) != 0 )
	{
		Angles->X = -(grFloat)atan2(M->BZ, M->CZ);
		Angles->Z = -(grFloat)atan2(M->AY, M->AX);
	}
	else
	{
		Angles->X = -(grFloat)atan2(M->BX, M->BY);
		Angles->Z = 0.0f;
	}
	assert( grVec3d_IsValid(Angles)!=GR_FALSE);
}


GRAPI void GRCC grXForm3d_SetEulerAngles(grXForm3d *M, const grVec3d *Angles)
	// order of angles z,y,x
{
	grXForm3d XM, YM, ZM;							            

	assert( M      != NULL );
	assert( grVec3d_IsValid(Angles)!=GR_FALSE);
	
	grXForm3d_SetXRotation(&XM,Angles->X);
	grXForm3d_SetYRotation(&YM,Angles->Y);
	grXForm3d_SetZRotation(&ZM,Angles->Z);
	
	grXForm3d_Multiply(&XM, &YM, M);
	grXForm3d_Multiply(M, &ZM, M);
	

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );

}

GRAPI grBoolean GRCC grXForm3d_IsOrthonormal(const grXForm3d *M)
	// returns GR_TRUE if M is orthonormal 
	// (if the rows and columns are all normalized (transform has no scaling or shearing)
	// and is orthogonal (row1 cross row2 = row3 & col1 cross col2 = col3)
{
#define ORTHONORMAL_TOLERANCE ((grFloat)(0.001f))
	grVec3d Col1,Col2,Col3;
	grVec3d Col1CrossCol2;
	grBoolean IsOrthonormal;
	assert( M != NULL );

#pragma message ("This test for non-orthogonal is not quite correct")
	if	(M->Flags & XFORM3D_NONORTHOGONALISOK)
		return GR_TRUE;

	grXForm3d_Assert ( grXForm3d_IsValid(M) == GR_TRUE );

	Col1.X = M->AX;
	Col1.Y = M->BX;
	Col1.Z = M->CX;
	
	Col2.X = M->AY;
	Col2.Y = M->BY;
	Col2.Z = M->CY;

	Col3.X = M->AZ;
	Col3.Y = M->BZ;
	Col3.Z = M->CZ;

	grVec3d_CrossProduct(&Col1,&Col2,&Col1CrossCol2);

	IsOrthonormal = grVec3d_Compare(&Col1CrossCol2,&Col3,ORTHONORMAL_TOLERANCE);
	if (IsOrthonormal == GR_FALSE)
		{
			grVec3d_Inverse(&Col3);
			IsOrthonormal = grVec3d_Compare(&Col1CrossCol2,&Col3,ORTHONORMAL_TOLERANCE);
		}

	if ( grVec3d_IsValid(&(M->Translation)) ==GR_FALSE)
		return GR_FALSE;

	return IsOrthonormal;
}


GRAPI void GRCC grXForm3d_Orthonormalize(grXForm3d *M)
	// essentially removes scaling (or other distortions) from 
	// an orthogonal (or nearly orthogonal) matrix 
{
	grVec3d Col1,Col2,Col3;
	assert( M != NULL );
	grXForm3d_Assert ( grXForm3d_IsValid(M) == GR_TRUE );

#pragma message ("This test for non-orthogonal is not quite correct")
	if	(M->Flags & XFORM3D_NONORTHOGONALISOK)
		return;

	// Normalize Col 1 & 2
	Col1.X = M->AX;
	Col1.Y = M->BX;
	Col1.Z = M->CX;
	grVec3d_Normalize(&Col1);
	M->AX = Col1.X;
	M->BX = Col1.Y;
	M->CX = Col1.Z;
	
	Col2.X = M->AY;
	Col2.Y = M->BY;
	Col2.Z = M->CY;
	grVec3d_Normalize(&Col2);
	M->AY = Col2.X;
	M->BY = Col2.Y;
	M->CY = Col2.Z;

	// Cross Col 1 & 2 to get 3
	grVec3d_CrossProduct(&Col1,&Col2,&Col3);

	M->AZ = Col3.X;
	M->BZ = Col3.Y;
	M->CZ = Col3.Z;

	// Cross Col 3 and 1 to get 2
	grVec3d_CrossProduct(&Col3,&Col1,&Col2);

	M->AY = Col2.X;
	M->BY = Col2.Y;
	M->CY = Col2.Z;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}




GRAPI grBoolean GRCC grXForm3d_IsOrthogonal(const grXForm3d *M)
	// returns GR_TRUE if M is orthogonal
	// (row1 cross row2 = row3 & col1 cross col2 = col3)
{
#define ORTHOGONAL_TOLERANCE ((grFloat)(0.001f))
	grVec3d Col1,Col2,Col3;
	grVec3d Col1CrossCol2;
	grBoolean IsOrthogonal;
	assert( M != NULL );
	grXForm3d_Assert ( grXForm3d_IsValid(M) == GR_TRUE );

#pragma message ("This test for non-orthogonal is not quite correct")
	if	(M->Flags & XFORM3D_NONORTHOGONALISOK)
		return GR_TRUE;

	//return GR_TRUE;

	Col1.X = M->AX;
	Col1.Y = M->BX;
	Col1.Z = M->CX;
	//grVec3d_Normalize(&Col1);
	
	Col2.X = M->AY;
	Col2.Y = M->BY;
	Col2.Z = M->CY;
	//grVec3d_Normalize(&Col2);
	
	Col3.X = M->AZ;
	Col3.Y = M->BZ;
	Col3.Z = M->CZ;
	grVec3d_Normalize(&Col3);
	
	grVec3d_CrossProduct(&Col1,&Col2,&Col1CrossCol2);
		
	grVec3d_Normalize(&Col1CrossCol2);
	
	IsOrthogonal = grVec3d_Compare(&Col1CrossCol2,&Col3,ORTHOGONAL_TOLERANCE);
	if (IsOrthogonal == GR_FALSE)
		{
			grVec3d_Inverse(&Col3);
			IsOrthogonal = grVec3d_Compare(&Col1CrossCol2,&Col3,ORTHOGONAL_TOLERANCE);
		}

	if ( grVec3d_IsValid(&(M->Translation)) ==GR_FALSE)
		return GR_FALSE;

	return IsOrthogonal;
}

GRAPI void GRCC grXForm3d_LookAt (grXForm3d* M, const grVec3d *Target)
{
	/*
	grVec3d Pos, InVect;
	grVec3d			LVect = {1.0f,0.0f,0.0f}, UpVect = {0.0f,1.0f,0.0f};

	grVec3d_Subtract(Target, &M->Translation, &InVect);

	Pos = M->Translation;

	if (grVec3d_Length(&InVect) == 0.0f)
	{
		grXForm3d_SetIdentity(M);
	}
	else
	{
		grVec3d_Normalize(&InVect);
		if ((1.0f - fabs(grVec3d_DotProduct(&InVect, &UpVect))) < 0.01f)
		{
			grVec3d_CrossProduct(&LVect, &InVect, &UpVect);
			grVec3d_Normalize(&UpVect);
			grVec3d_CrossProduct(&UpVect, &InVect, &LVect);
			grXForm3d_SetFromLeftUpIn(M, &LVect, &UpVect, &InVect);
		}
		else
		{
			grVec3d_CrossProduct(&UpVect, &InVect, &LVect);
			grVec3d_Normalize(&LVect);
			grVec3d_CrossProduct(&InVect, &LVect, &UpVect);
			grXForm3d_SetFromLeftUpIn(M, &LVect, &UpVect, &InVect);
		}
	}

	M->Translation = Pos;
*/
	
	grVec3d		vecNewIn, vecNewUp, vecNewLeft;
	grVec3d_Subtract(&M->Translation, Target, &vecNewIn);
	grVec3d_Normalize(&vecNewIn);

	vecNewUp.X = 0.0f;
	vecNewUp.Y = 1.0f;
	vecNewUp.Z = 0.0f;

	grVec3d_CrossProduct(&vecNewUp, &vecNewIn, &vecNewLeft);
	grVec3d_Normalize(&vecNewLeft);

	M->AX = vecNewLeft.X;
	M->BX = vecNewLeft.Y;
	M->CX = vecNewLeft.Z;

	M->AY = vecNewUp.X;
	M->BY = vecNewUp.Y;
	M->CY = vecNewUp.Z;

	M->AZ = vecNewIn.X;
	M->BZ = vecNewIn.Y;
	M->CZ = vecNewIn.Z;

	if (!grXForm3d_IsOrthonormal(M))
		grXForm3d_Orthonormalize(M);

	grVec3d_Subtract(&M->Translation, Target, &vecNewIn);
	grVec3d_Normalize(&vecNewIn);

	grVec3d_CrossProduct(&vecNewIn, &vecNewLeft,&vecNewUp);
	grVec3d_Normalize(&vecNewUp);

	M->AX = vecNewLeft.X;
	M->BX = vecNewLeft.Y;
	M->CX = vecNewLeft.Z;

	M->AY = vecNewUp.X;
	M->BY = vecNewUp.Y;
	M->CY = vecNewUp.Z;

	M->AZ = vecNewIn.X;
	M->BZ = vecNewIn.Y;
	M->CZ = vecNewIn.Z;

	if (!grXForm3d_IsOrthonormal(M))
		grXForm3d_Orthonormalize(M);

}

GRAPI void GRCC grXForm3d_RotateAboutLeft (grXForm3d* M, grFloat RadianAngle)
{
     grVec3d Angles, Pos;

     Pos = M->Translation;
     grXForm3d_Translate (M, -Pos.X, -Pos.Y, -Pos.Z); 
     grXForm3d_GetEulerAngles (M, &Angles); 
     grXForm3d_SetIdentity (M); 
     grXForm3d_RotateX (M, -RadianAngle); 
     grXForm3d_RotateZ (M, Angles.Z); 
     grXForm3d_RotateY (M, Angles.Y);
     grXForm3d_RotateX (M, Angles.X);

     grXForm3d_Translate (M, Pos.X, Pos.Y, Pos.Z);
}



GRAPI void GRCC grXForm3d_SetFromLeftUpIn(
	grXForm3d *M,
	const grVec3d *Left, 
	const grVec3d *Up, 
	const grVec3d *In)
{
	assert(M);
	assert(Left);
	assert(Up);
	assert(In);
	grXForm3d_Assert(grVec3d_IsNormalized(Left));
	grXForm3d_Assert(grVec3d_IsNormalized(Up));
	grXForm3d_Assert(grVec3d_IsNormalized(In));

	M->AX = -Left->X;
	M->BX = -Left->Y;
	M->CX = -Left->Z;
	M->AY =  Up->X;
	M->BY =  Up->Y;
	M->CY =  Up->Z;
	M->AZ = -In->X;
	M->BZ = -In->Y;
	M->CZ = -In->Z;

	grVec3d_Clear(&M->Translation);
	M->Flags = 0;

	grXForm3d_Assert ( grXForm3d_IsOrthonormal(M) == GR_TRUE );
}

GRAPI void GRCC grXForm3d_Mirror(
	const		grXForm3d *Source, 
	const		grVec3d *PlaneNormal, 
	float		PlaneDist, 
	grXForm3d	*Dest)
{
	float			Dist;
	grVec3d			In, Left, Up;
	grXForm3d		Original;
	grVec3d			MirrorTranslation;

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(Source) == GR_TRUE );
	assert( PlaneNormal != NULL );
	assert( Dest        != NULL );

	grXForm3d_Copy(Source, &Original);

	// Mirror the translation portion of the matrix
	Dist = grVec3d_DotProduct(&Original.Translation, PlaneNormal) - PlaneDist;
	grVec3d_AddScaled(&Original.Translation, PlaneNormal, -Dist*2.0f, &MirrorTranslation);

	// Mirror the Rotational portion of the xform first
	grXForm3d_GetIn(&Original, &In);
	grVec3d_Add(&Original.Translation, &In, &In);
	Dist = grVec3d_DotProduct(&In, PlaneNormal) - PlaneDist;
	grVec3d_AddScaled(&In, PlaneNormal, -Dist*2.0f, &In);
	grVec3d_Subtract(&In, &MirrorTranslation, &In);
	grVec3d_Normalize(&In);

	grXForm3d_GetLeft(&Original, &Left);
	grVec3d_Add(&Original.Translation, &Left, &Left);
	Dist = grVec3d_DotProduct(&Left, PlaneNormal) - PlaneDist;
	grVec3d_AddScaled(&Left, PlaneNormal, -Dist*2.0f, &Left);
	grVec3d_Subtract(&Left, &MirrorTranslation, &Left);
	grVec3d_Normalize(&Left);

	grXForm3d_GetUp(&Original, &Up);
	grVec3d_Add(&Original.Translation, &Up, &Up);
	Dist = grVec3d_DotProduct(&Up, PlaneNormal) - PlaneDist;
	grVec3d_AddScaled(&Up, PlaneNormal, -Dist*2.0f, &Up);
	grVec3d_Subtract(&Up, &MirrorTranslation, &Up);
	grVec3d_Normalize(&Up);

	grXForm3d_SetFromLeftUpIn(Dest, &Left, &Up, &In);

	// Must set the mirror translation here since grXForm3d_SetFromLeftUpIn cleared the translation portion
	grVec3d_Set(&Dest->Translation, MirrorTranslation.X, MirrorTranslation.Y, MirrorTranslation.Z);

	grXForm3d_Assert ( grXForm3d_IsOrthogonal(Dest) == GR_TRUE );
}
