/****************************************************************************************/
/*  EXTBOX.C                                                                            */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: Axial aligned bounding box support                                     */
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
#include "ExtBox.h"
#include <assert.h>

#define MAX(aa,bb)   ( ((aa)>(bb))?(aa):(bb) )
#define MIN(aa,bb)   ( ((aa)<(bb))?(aa):(bb) )

// Added by Icestorm
GRAPI grBoolean GRCC grExtBox_IsPoint(  const grExtBox *B )
{
	assert (B != NULL);
	if (!GR_FLOATS_EQUAL(B->Min.X,B->Max.X))
		return GR_FALSE;
    if (!GR_FLOATS_EQUAL(B->Min.Y,B->Max.Y))
		return GR_FALSE;
	if (!GR_FLOATS_EQUAL(B->Min.Z,B->Max.Z))
		return GR_FALSE;
	else
		return GR_TRUE;
}


GRAPI grBoolean GRCC grExtBox_IsValid(  const grExtBox *B )
{
	if (B == NULL) return GR_FALSE;
	
	if (grVec3d_IsValid(&(B->Min)) == GR_FALSE)
		return GR_FALSE;
	if (grVec3d_IsValid(&(B->Max)) == GR_FALSE)
		return GR_FALSE;


	if (    (B->Min.X <= B->Max.X) &&
			(B->Min.Y <= B->Max.Y) &&
			(B->Min.Z <= B->Max.Z)   	)
		return GR_TRUE;
	else
		return GR_FALSE;
}

GRAPI void GRCC grExtBox_Set(  grExtBox *B,
					grFloat X1,grFloat Y1,grFloat Z1,
					grFloat X2,grFloat Y2,grFloat Z2)
{
	assert (B != NULL);

	//grVec3d_Set	(&B->Min, MIN (x1, x2),	MIN (y1, y2),MIN (z1, z2));
	//grVec3d_Set (&B->Max, MAX (x1, x2),	MAX (y1, y2),MAX (z1, z2));

	if ( X1 > X2 )
		{	B->Max.X = X1;	B->Min.X = X2;	}
	else
		{	B->Max.X = X2;  B->Min.X = X1;  }
	
	if ( Y1 > Y2 )
		{	B->Max.Y = Y1;	B->Min.Y = Y2;	}
	else
		{	B->Max.Y = Y2;  B->Min.Y = Y1;  }
	
	if ( Z1 > Z2 )
		{	B->Max.Z = Z1;	B->Min.Z = Z2;	}
	else
		{	B->Max.Z = Z2;  B->Min.Z = Z1;  }

	assert( grVec3d_IsValid(&(B->Min)) != GR_FALSE );
	assert( grVec3d_IsValid(&(B->Max)) != GR_FALSE );

}

// Set box Min and Max to the passed point
GRAPI void GRCC grExtBox_SetToPoint ( grExtBox *B, const grVec3d *Point )
{
	assert( B     != NULL );
	assert( Point != NULL );
	assert( grVec3d_IsValid(Point) != GR_FALSE );

	
	B->Max = *Point;
	B->Min = *Point;
}

// Extend a box to encompass the passed point
GRAPI void GRCC grExtBox_ExtendToEnclose( grExtBox *B, const grVec3d *Point )
{
	assert ( grExtBox_IsValid(B) != GR_FALSE );
	assert( Point != NULL );
	assert( grVec3d_IsValid(Point) != GR_FALSE );

	if (Point->X > B->Max.X ) B->Max.X = Point->X;
	if (Point->Y > B->Max.Y ) B->Max.Y = Point->Y;
	if (Point->Z > B->Max.Z ) B->Max.Z = Point->Z;

	if (Point->X < B->Min.X ) B->Min.X = Point->X;
	if (Point->Y < B->Min.Y ) B->Min.Y = Point->Y;
	if (Point->Z < B->Min.Z ) B->Min.Z = Point->Z;

}

static grBoolean GRCC grExtBox_Intersects(  const grExtBox *B1,  const grExtBox *B2 )
{
	assert ( grExtBox_IsValid (B1) != GR_FALSE );
	assert ( grExtBox_IsValid (B2) != GR_FALSE );

	if ((B1->Min.X > B2->Max.X) || (B1->Max.X < B2->Min.X)) return GR_FALSE;
	if ((B1->Min.Y > B2->Max.Y) || (B1->Max.Y < B2->Min.Y)) return GR_FALSE;
	if ((B1->Min.Z > B2->Max.Z) || (B1->Max.Z < B2->Min.Z)) return GR_FALSE;
	return GR_TRUE;
}


	
GRAPI grBoolean GRCC grExtBox_Intersection( const grExtBox *B1, const grExtBox *B2, grExtBox *Result )
{
	grBoolean rslt;

	assert ( grExtBox_IsValid (B1) != GR_FALSE );
	assert ( grExtBox_IsValid (B2) != GR_FALSE );

	rslt = grExtBox_Intersects (B1, B2);
	if ( (rslt != GR_FALSE) && (Result != NULL))
		{
			grExtBox_Set ( Result,
						MAX (B1->Min.X, B2->Min.X),
						MAX (B1->Min.Y, B2->Min.Y),
						MAX (B1->Min.Z, B2->Min.Z),
						MIN (B1->Max.X, B2->Max.X),
						MIN (B1->Max.Y, B2->Max.Y),
						MIN (B1->Max.Z, B2->Max.Z) );
		}
	return rslt;
}

GRAPI void GRCC grExtBox_Union( const grExtBox *B1, const grExtBox *B2, grExtBox *Result )
{
	assert ( grExtBox_IsValid (B1) != GR_FALSE );
	assert ( grExtBox_IsValid (B2) != GR_FALSE );
	assert (Result != NULL);

	grExtBox_Set (	Result,
				MIN (B1->Min.X, B2->Min.X),
				MIN (B1->Min.Y, B2->Min.Y),
				MIN (B1->Min.Z, B2->Min.Z),
				MAX (B1->Max.X, B2->Max.X),
				MAX (B1->Max.Y, B2->Max.Y),
				MAX (B1->Max.Z, B2->Max.Z) );
}

GRAPI grBoolean GRCC grExtBox_ContainsPoint(  const grExtBox *B,  const grVec3d *Point )
{
	assert (grExtBox_IsValid (B) != GR_FALSE);
	assert( grVec3d_IsValid(Point) != GR_FALSE );

	if (    (Point->X >= B->Min.X) && (Point->X <= B->Max.X) &&
			(Point->Y >= B->Min.Y) && (Point->Y <= B->Max.Y) &&
			(Point->Z >= B->Min.Z) && (Point->Z <= B->Max.Z)     )
		{
			return GR_TRUE;
		}
	else
		{
			return GR_FALSE;
		}
}


GRAPI void GRCC grExtBox_GetTranslation( const grExtBox *B, grVec3d *pCenter )
{
	assert (grExtBox_IsValid (B) != GR_FALSE);
	assert (pCenter != NULL);

	grVec3d_Set( pCenter,
				(B->Min.X + B->Max.X)/2.0f,
				(B->Min.Y + B->Max.Y)/2.0f,
				(B->Min.Z + B->Max.Z)/2.0f );
}

GRAPI void GRCC grExtBox_Translate(  grExtBox *B,  grFloat DX,  grFloat DY,  grFloat DZ	)
{
	grVec3d VecDelta;

	assert (grExtBox_IsValid (B) != GR_FALSE);

	grVec3d_Set (&VecDelta, DX, DY, DZ);
		assert( grVec3d_IsValid(&VecDelta) != GR_FALSE );
	grVec3d_Add (&B->Min, &VecDelta, &B->Min);
	grVec3d_Add (&B->Max, &VecDelta, &B->Max);
}

GRAPI void GRCC grExtBox_SetTranslation( grExtBox *B, const grVec3d *pCenter )
{
	grVec3d Center,Translation;

	assert (grExtBox_IsValid (B) != GR_FALSE);
	assert (pCenter != NULL);
	assert( grVec3d_IsValid(pCenter) != GR_FALSE );

	grExtBox_GetTranslation( B, &Center );
	grVec3d_Subtract( pCenter, &Center, &Translation);

	grExtBox_Translate( B, Translation.X, Translation.Y, Translation.Z );
}

// Icestorm Begin
GRAPI void GRCC grExtBox_SetNewOrigin( grExtBox *B, const grVec3d *pOrigin)
{
	assert (grExtBox_IsValid (B) != GR_FALSE);
	assert (pOrigin != NULL);
	assert( grVec3d_IsValid(pOrigin) != GR_FALSE );
	grVec3d_Subtract(&B->Min, pOrigin, &B->Min);
	grVec3d_Subtract(&B->Max, pOrigin, &B->Max);
}

GRAPI void GRCC grExtBox_MoveToOrigin( grExtBox *B, grVec3d *OldCenter )
{
	grFloat DX,DY,DZ;
	assert (grExtBox_IsValid (B) != GR_FALSE);

	DX=(B->Min.X+B->Max.X)*0.5f;
	B->Min.X-=DX;B->Max.X-=DX;

	DY=(B->Min.Y+B->Max.Y)*0.5f;
	B->Min.Y-=DY;B->Max.Y-=DY;

	DZ=(B->Min.Z+B->Max.Z)*0.5f;
	B->Min.Z-=DZ;B->Max.Z-=DZ;
	if (OldCenter)
		grVec3d_Set(OldCenter, DX, DY, DZ);
}

GRAPI void GRCC grExtBox_TranslateAndMoveToOrigin( grExtBox *B, const grVec3d *vMove, grVec3d *MovedCenter )
{
	grFloat DX,DY,DZ;
	assert (grExtBox_IsValid (B) != GR_FALSE);

	DX=(B->Min.X+B->Max.X)*0.5f;
	B->Min.X-=DX;B->Max.X-=DX;

	DY=(B->Min.Y+B->Max.Y)*0.5f;
	B->Min.Y-=DY;B->Max.Y-=DY;

	DZ=(B->Min.Z+B->Max.Z)*0.5f;
	B->Min.Z-=DZ;B->Max.Z-=DZ;
	if (MovedCenter)
	{ 
		MovedCenter->X=DX+vMove->X;
		MovedCenter->Y=DY+vMove->Y;
		MovedCenter->Z=DZ+vMove->Z;
	}
}
// Icestorm End

GRAPI void GRCC grExtBox_GetScaling( const grExtBox *B, grVec3d *pScale )
{
	assert (grExtBox_IsValid (B) != GR_FALSE );
	assert (pScale != NULL);

	grVec3d_Subtract( &(B->Max), &(B->Min), pScale );
}

GRAPI void GRCC grExtBox_Scale( grExtBox *B, grFloat ScaleX, grFloat ScaleY, grFloat ScaleZ )
{
	grVec3d Center;
	grVec3d Scale;
	grFloat DX,DY,DZ;

	assert (grExtBox_IsValid (B) != GR_FALSE );
	assert (ScaleX >= 0.0f );
	assert (ScaleY >= 0.0f );
	assert (ScaleZ >= 0.0f );
	assert (ScaleX * ScaleX >= 0.0f );		// check for NANS
	assert (ScaleY * ScaleY >= 0.0f );
	assert (ScaleZ * ScaleZ >= 0.0f );

	grExtBox_GetTranslation( B, &Center );
	grExtBox_GetScaling    ( B, &Scale  );
	
	DX = ScaleX * Scale.X * 0.5f;
	DY = ScaleY * Scale.Y * 0.5f;
	DZ = ScaleZ * Scale.Z * 0.5f;

	B->Min.X = Center.X - DX;
	B->Min.Y = Center.Y - DY;
	B->Min.Z = Center.Z - DZ;
	
	B->Max.X = Center.X + DX;
	B->Max.Y = Center.Y + DY;
	B->Max.Z = Center.Z + DZ;
	
	assert (grExtBox_IsValid (B) != GR_FALSE);
}

GRAPI void GRCC grExtBox_SetScaling( grExtBox *B, const grVec3d *pScale )
{
	grVec3d Center;
	grFloat DX,DY,DZ;

	assert (grExtBox_IsValid (B) != GR_FALSE );
	assert (pScale != NULL );
	assert (grVec3d_IsValid( pScale )!= GR_FALSE);
	assert (pScale->X >= 0.0f );
	assert (pScale->Y >= 0.0f );
	assert (pScale->Z >= 0.0f );

	grExtBox_GetTranslation( B, &Center );

	DX = pScale->X / 2.0f;
	DY = pScale->Y / 2.0f;
	DZ = pScale->Z / 2.0f;

	B->Min.X = Center.X - DX;
	B->Min.Y = Center.Y - DY;
	B->Min.Z = Center.Z - DZ;
	
	B->Max.X = Center.X + DX;
	B->Max.Y = Center.Y + DY;
	B->Max.Z = Center.Z + DZ;
}

GRAPI void GRCC grExtBox_LinearSweep(	const grExtBox *BoxToSweep, 
						const grVec3d *StartPoint, 
						const grVec3d *EndPoint, 
						grExtBox *EnclosingBox )
{

	assert (grExtBox_IsValid (BoxToSweep) != GR_FALSE );
	assert (StartPoint   != NULL );
	assert (EndPoint     != NULL );
	assert (grVec3d_IsValid( StartPoint )!= GR_FALSE);
	assert (grVec3d_IsValid( EndPoint   )!= GR_FALSE);
	assert (EnclosingBox != NULL );

	*EnclosingBox = *BoxToSweep;

	if (EndPoint->X > StartPoint->X)
		{
			EnclosingBox->Min.X += StartPoint->X; 
			EnclosingBox->Max.X += EndPoint->X; 
		}
	else
		{
			EnclosingBox->Min.X += EndPoint->X; 
			EnclosingBox->Max.X += StartPoint->X; 
		}

	if (EndPoint->Y > StartPoint->Y)
		{
			EnclosingBox->Min.Y += StartPoint->Y; 
			EnclosingBox->Max.Y += EndPoint->Y; 
		}
	else
		{
			EnclosingBox->Min.Y += EndPoint->Y; 
			EnclosingBox->Max.Y += StartPoint->Y; 
		}

	if (EndPoint->Z > StartPoint->Z)
		{
			EnclosingBox->Min.Z += StartPoint->Z; 
			EnclosingBox->Max.Z += EndPoint->Z; 
		}
	else
		{
			EnclosingBox->Min.Z += EndPoint->Z; 
			EnclosingBox->Max.Z += StartPoint->Z; 
		}
	assert (grExtBox_IsValid (EnclosingBox) != GR_FALSE );
}

static grBoolean GRCC grExtBox_XFaceDist(  const grVec3d *Start, 
												const grVec3d *Delta, const grExtBox *B, grFloat *T, grFloat X)
{
	grFloat t;
	grFloat Y,Z;
	assert( Start != NULL );
	assert( Delta != NULL );
	assert( B     != NULL );
	assert( T     != NULL );

	//if ( (Start->X <= X) && (X <= Delta->X + Start->X) )
		{
			t = (X - Start->X)/Delta->X;
			Y  = Start->Y + Delta->Y * t;
			if ( ( B->Min.Y <= Y) && (Y <= B->Max.Y) )
				{
					Z = Start->Z + Delta->Z * t;
					if ( ( B->Min.Z <= Z) && (Z <= B->Max.Z) )
						{
							*T = t;
							return GR_TRUE;
						}
				}
		}
	return GR_FALSE;
}

static grBoolean GRCC grExtBox_YFaceDist(  const grVec3d *Start, const grVec3d *Delta, const grExtBox *B, grFloat *T, grFloat Y)
{
	grFloat t;
	grFloat X,Z;
	assert( Start != NULL );
	assert( Delta != NULL );
	assert( B     != NULL );
	assert( T     != NULL );

	//if ( (Start->Y <= Y) && (Y <= Delta->Y + Start->Y) )
		{
			t = (Y - Start->Y)/Delta->Y;
			Z  = Start->Z + Delta->Z * t;
			if ( ( B->Min.Z <= Z) && (Z <= B->Max.Z) )
				{
					X = Start->X + Delta->X * t;
					if ( ( B->Min.X <= X) && (X <= B->Max.X) )
						{
							*T = t;
							return GR_TRUE;
						}
				}
		}
	return GR_FALSE;
}


static grBoolean GRCC grExtBox_ZFaceDist(  const grVec3d *Start, const grVec3d *Delta, const grExtBox *B, grFloat *T, grFloat Z)
{
	grFloat t;
	grFloat X,Y;
	assert( Start != NULL );
	assert( Delta != NULL );
	assert( B     != NULL );
	assert( T     != NULL );

	//if ( (Start->Z <= Z) && (Z <= Delta->Z + Start->Z) )
		{
			t = (Z - Start->Z)/Delta->Z;
			X  = Start->X + Delta->X * t;
			if ( ( B->Min.X <= X) && (X <= B->Max.X) )
				{
					Y = Start->Y + Delta->Y * t;
					if ( ( B->Min.Y <= Y) && (Y <= B->Max.Y) )
						{
							*T = t;
							return GR_TRUE;
						}
				}
		}
	return GR_FALSE;
}



GRAPI grBoolean GRCC grExtBox_RayCollision( const grExtBox *B, const grVec3d *Start, const grVec3d *End, 
								grFloat *T, grVec3d *Normal )
{
	// only detects rays going 'in' to the box
	grFloat t;
	grVec3d Delta;
	grVec3d LocalNormal;
	grFloat LocalT;

	assert( B != NULL );
	assert( Start != NULL );
	assert( End != NULL );
	assert (grVec3d_IsValid( Start )!= GR_FALSE);
	assert (grVec3d_IsValid( End   )!= GR_FALSE);
	assert (grExtBox_IsValid( B )!= GR_FALSE );

	grVec3d_Subtract(End,Start,&Delta);
	
	if (Normal == NULL)
		Normal = &LocalNormal;
	if (T == NULL)
		T = &LocalT;
	
	// test x end of box, facing away from ray direction.
	if (Delta.X > 0.0f)
		{
			if ( (Start->X <= B->Min.X) && (B->Min.X <= End->X) &&
				 (grExtBox_XFaceDist(  Start ,&Delta, B, &t, B->Min.X ) != GR_FALSE) )
					{
						grVec3d_Set( Normal,  -1.0f, 0.0f, 0.0f );
						*T = t;
						return GR_TRUE;
					}
		}
	else if (Delta.X < 0.0f)
		{
			if ( (End->X <= B->Max.X) && (B->Max.X <= Start->X) &&
				 (grExtBox_XFaceDist(  Start ,&Delta, B, &t, B->Max.X ) != GR_FALSE) )
					{
						grVec3d_Set( Normal,  1.0f, 0.0f, 0.0f );
						*T = t;
						return GR_TRUE;
					}
		}
	
	// test y end of box, facing away from ray direction.
	if (Delta.Y > 0.0f)
		{	
			if ( (Start->Y <= B->Min.Y) && (B->Min.Y <= End->Y) &&
				 (grExtBox_YFaceDist(  Start ,&Delta, B, &t, B->Min.Y ) != GR_FALSE) )
				{
					grVec3d_Set( Normal,  0.0f, -1.0f, 0.0f );
					*T = t;
					return GR_TRUE;
				}
		}
	else if (Delta.Y < 0.0f)
		{
			if ( (End->Y <= B->Max.Y) && (B->Max.Y <= Start->Y) &&
				 (grExtBox_YFaceDist(  Start ,&Delta, B, &t, B->Max.Y ) != GR_FALSE) )
				{
					grVec3d_Set( Normal,  0.0f, 1.0f, 0.0f );
					*T = t;
					return GR_TRUE;
				}
		}
	
	// test z end of box, facing away from ray direction.
	if (Delta.Z > 0.0f)
		{	
			if ( (Start->Z <= B->Min.Z) && (B->Min.Z <= End->Z) &&
			     (grExtBox_ZFaceDist(  Start ,&Delta, B, &t, B->Min.Z ) != GR_FALSE) )
				{
					grVec3d_Set( Normal,  0.0f, 0.0f, -1.0f );
					*T = t;
					return GR_TRUE;
				}
		}
	else if (Delta.Z < 0.0f)
		{			
			if ( (End->Z <= B->Max.Z) && (B->Max.Z <= Start->Z) &&
				 (grExtBox_ZFaceDist(  Start ,&Delta, B, &t, B->Max.Z ) != GR_FALSE) )
				{
					grVec3d_Set( Normal,  0.0f, 0.0f, 1.0f );
					*T = t;
					return GR_TRUE;
				}
		}
	return GR_FALSE;	
}

GRAPI void GRCC grExtBox_GetPoint( const grExtBox *B, const int iPoint, grVec3d *vPoint)
{
	assert(vPoint != NULL);

	switch(iPoint)
	{
	case 0:
		grVec3d_Set(vPoint, B->Min.X, B->Min.Y, B->Min.Z);
		break;
	case 1:
		grVec3d_Set(vPoint, B->Max.X, B->Min.Y, B->Min.Z);
		break;
	case 2:
		grVec3d_Set(vPoint, B->Min.X, B->Max.Y, B->Min.Z);
		break;
	case 3:
		grVec3d_Set(vPoint, B->Max.X, B->Max.Y, B->Min.Z);
		break;
	case 4:
		grVec3d_Set(vPoint, B->Min.X, B->Min.Y, B->Max.Z);
		break;
	case 5:
		grVec3d_Set(vPoint, B->Max.X, B->Min.Y, B->Max.Z);
		break;
	case 6:
		grVec3d_Set(vPoint, B->Min.X, B->Max.Y, B->Max.Z);
		break;
	case 7:
		grVec3d_Set(vPoint, B->Max.X, B->Max.Y, B->Max.Z);
		break;
	}
}

// Added by Icestorm:
// __inline functions & defines for an
// fast collision routine ;)
// NOTE:	Added EPSILON (slip through edge/corner bug)
//			Added more asm code (prevents unnecessary fdiv)
//			Flipped normals into right direction ;)
//			Fixed FPU-stack-overflow-bug
static  grFloat			GR_EXTBOX_FC_EPSILON = 0.00001f;
#define FSIZE			4
#define VSIZE			4*FSIZE
#define _X				+0*FSIZE]
#define _Y				+1*FSIZE]
#define _Z				+2*FSIZE]
#define _MIN			+0*VSIZE
#define _MAX			+1*VSIZE
#define _VPATH		dword ptr [esi
#define _VPATH2		dword ptr [ecx
#define _B			dword ptr [edi
#define _XMOVINGBOX dword ptr [ebx

#define _ASM_TEST_A(_P)																   \
__asm	fld   _VPATH _P				/* |   vPath->_P								*/ \
__asm	fmul  t						/* | t*vPath->_P								*/ \
__asm	fld   _B _MAX _P 			/* | t*vPath->_P | B->Max._P					*/ \
__asm	fsub  _XMOVINGBOX _MIN _P  	/* | t*vPath->_P | B->Max._P-xMovingBox->Min._P */ \
__asm   fadd  GR_EXTBOX_FC_EPSILON  /*   IMPROTANT!!								*/ \
__asm	fcomp						/* | t*vPath->_P								*/ \
__asm	fnstsw ax					/*												*/ \
__asm	test  ah,1h					/*												*/ \
__asm	je   TestCollision##_P		/* !(xMovingBox->Min._P+t*vPath->_P<=B->Max._P) */ \
__asm	fcomp st					/*	Clean up stack								*/ \
__asm	jmp  short NoCollision		/*												*/

#define _ASM_TEST_B(_P)																   \
__asm	fld   _B _MIN _P			/* | t*vPath->_P | B->Min._P					*/ \
__asm	fsub  _XMOVINGBOX _MAX _P	/* | t*vPath->_P | B->Min._P-xMovingBox->Max._P */ \
__asm   fsub  GR_EXTBOX_FC_EPSILON  /*   IMPROTANT!!								*/ \
__asm	fcompp						/*												*/ \
__asm	fnstsw ax					/*												*/ \
__asm	test  ah,41h				/*												*/ \
__asm	je    NoCollision			/* !(B->Min._P<=t*vPath->_P+xMovingBox->Max._P) */

static __inline grBoolean grExtBox_asmCollisionTestX_(grFloat t, grVec3d *vPath, const grExtBox *B, grExtBox *xMovingBox)
{
	__asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		_ASM_TEST_A(_Y)
TestCollision_Y:
		_ASM_TEST_B(_Y)
		_ASM_TEST_A(_Z)		
TestCollision_Z:
		_ASM_TEST_B(_Z)
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}

static __inline grBoolean grExtBox_asmCollisionTestY_(grFloat t, grVec3d *vPath, const grExtBox *B, grExtBox *xMovingBox)
{
	_asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		_ASM_TEST_A(_X)
TestCollision_X:
		_ASM_TEST_B(_X)
		_ASM_TEST_A(_Z)		
TestCollision_Z:
		_ASM_TEST_B(_Z)		
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}

static __inline grBoolean grExtBox_asmCollisionTestZ_(grFloat t, grVec3d *vPath, const grExtBox *B, grExtBox *xMovingBox)
{
	_asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		_ASM_TEST_A(_X)
TestCollision_X:
		_ASM_TEST_B(_X)
		_ASM_TEST_A(_Y)		
TestCollision_Y:
		_ASM_TEST_B(_Y)
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}

// Added by Icestorm (fast, rewritten version of Incarnadine's one)
// (ca. 7-12 times faster)
// ----------------------------------------
// Collides a moving box (or ray) against a stationary box.  The moving box
// must be relative to the path and move from Start to End.
//   Only returns a ray/box hitting the outside of the box.  
//     on success, GR_TRUE is returned, and 
//       if T is non-NULL, T is returned as 0..1 where 0 is a collision at Start, and 1 is a collision at End
//       if Normal is non-NULL, Normal is the surface normal of the box where the collision occured.
GRAPI grBoolean GRCC grExtBox_Collision(	const grExtBox *B, const grExtBox *MovingBox,
											const grVec3d *Start, const grVec3d *End, 
											grFloat *T, grVec3d *Normal )
{
	grFloat t;
	grExtBox xSweepBox,xMovingBox,*xMovingBoxPtr=&xMovingBox;
	grVec3d vPath,*vPathPtr=&vPath;
	grBoolean TestB;

	assert(B != NULL);

	// If there's no moving box, do a ray collision
	if(MovingBox == NULL)
		return grExtBox_RayCollision(B,Start,End,T,Normal);

	// If the boxes already overlap, we have to report no collision
	// to be consistent with the rest of the engine collision calls.
	xMovingBox = *MovingBox;  // Used later as well.
	grExtBox_Translate(&xMovingBox, Start->X, Start->Y, Start->Z);
	if(grExtBox_Intersection(B, &xMovingBox, NULL)) return GR_FALSE;	

	// Verify the sweepbox intersects this box
	grExtBox_LinearSweep(MovingBox, Start, End, &xSweepBox);
	if(!grExtBox_Intersection(B, &xSweepBox, NULL)) return GR_FALSE;

	grVec3d_Subtract(End,Start,&vPath);

	// CollisionTest X-Front
	if (vPath.X<0.0f)
	{
		//t=(B->Max.X-xMovingBox.Min.X)/vPath.X;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MAX _X
			fsub  _XMOVINGBOX _MIN _X
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne    NoCollisionX1
			fadd   _VPATH _X
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionX1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndX1
NoCollisionX1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndX1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xMovingBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xMovingBox.Max,&vPath,t,&vPoint2);
			//if (vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)		
			if (grExtBox_asmCollisionTestX_(t, &vPath, B, &xMovingBox) )
			{ 
				if (Normal) grVec3d_Set(Normal,1.0f,0.0f,0.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	} 
	// CollisionTest X-Back
	else if (vPath.X>0.0f)
	{
		//t=(B->Min.X-xMovingBox.Max.X)/vPath.X;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MIN _X
			fsub  _XMOVINGBOX _MAX _X
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionX2
			fadd  _VPATH _X
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionX2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndX2
NoCollisionX2:
			fcompp
			mov   TestB,0
CTEndX2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath,t,&vPoint2);
			//if (vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestX_(t, &vPath, B, &xMovingBox) )
			{
				if (Normal) grVec3d_Set(Normal,-1.0f,0.0f,0.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Y-Front
	if (vPath.Y<0.0f)
	{
		//t=(B->Max.Y-xMovingBox.Min.Y)/vPath.Y;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MAX _Y
			fsub  _XMOVINGBOX _MIN _Y
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne    NoCollisionY1
			fadd  _VPATH _Y
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionY1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndY1
NoCollisionY1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndY1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,bT,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath,bT,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestY_(t, &vPath, B, &xMovingBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,+1.0f,0.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Y-Back
	else if (vPath.Y>0.0f)
	{
		//t=(B->Min.Y-xMovingBox.Max.Y)/vPath.Y;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MIN _Y
			fsub  _XMOVINGBOX _MAX _Y
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionY2
			fadd   _VPATH _Y
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionY2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndY2
NoCollisionY2:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndY2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestY_(t, &vPath, B, &xMovingBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,-1.0f,0.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Z-Front
	if (vPath.Z<0.0f)
	{
		//t=(B->Max.Z-xMovingBox.Min.Z)/vPath.Z;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MAX _Z
			fsub  _XMOVINGBOX _MIN _Z
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne    NoCollisionZ1
			fadd   _VPATH _Z
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionZ1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndZ1
NoCollisionZ1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndZ1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y)
			if (grExtBox_asmCollisionTestZ_(t, &vPath, B, &xMovingBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,0.0f,+1.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Z-Back
	else if (vPath.Z>0.0f)
	{
		//t=(B->Min.Z-xMovingBox.Max.Z)/vPath.Z;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xMovingBoxPtr
			fld   _B _MIN _Z
			fsub  _XMOVINGBOX _MAX _Z
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionZ2
			fadd  _VPATH _Z
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionZ2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndZ2
NoCollisionZ2:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndZ2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y)
			if (grExtBox_asmCollisionTestZ_(t, &vPath, B, &xMovingBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,0.0f,-1.0f);
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	}


	return GR_FALSE;
}


#define _ASM_TEST2_(_P)																   \
__asm	fld   _VPATH _P				/* |   vPath->_P								*/ \
__asm	fmul  t						/* | t*vPath->_P								*/ \
__asm	fld   _B _MAX _P 			/* | t*vPath->_P | B->Max._P					*/ \
__asm	fsub  _XMOVINGBOX _MIN _P  	/* | t*vPath->_P | B->Max._P-xMovingBox->Min._P */ \
__asm   fadd  GR_EXTBOX_FC_EPSILON  /*   IMPROTANT!!								*/ \
__asm	fcompp						/*												*/ \
__asm	fnstsw ax					/*												*/ \
__asm	test  ah,1h					/*												*/ \
__asm	jne   NoCollision			/* !(xMovingBox->Min._P+t*vPath->_P<=B->Max._P) */ \
									/*												*/ \
__asm	fld   _VPATH2 _P			/* |   vPath2->_P								*/ \
__asm	fmul  t						/* | t*vPath2->_P								*/ \
__asm	fld   _B _MIN _P			/* | t*vPath2->_P | B->Min._P					*/ \
__asm	fsub  _XMOVINGBOX _MAX _P	/* | t*vPath2->_P | B->Min._P-xMovingBox->Max._P*/ \
__asm   fsub  GR_EXTBOX_FC_EPSILON  /*   IMPROTANT!!								*/ \
__asm	fcompp						/*												*/ \
__asm	fnstsw ax					/*												*/ \
__asm	test  ah,41h				/*												*/ \
__asm	je    NoCollision			/* !(B->Min._P<=t*vPath2->_P+xMovingBox->Max._P)*/

static __inline grBoolean grExtBox_asmCollisionTestX2_(grFloat t, grVec3d *vPath, grVec3d *vPath2,
													   const grExtBox *B, grExtBox *xMovingBox)
{
	__asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		mov   ecx,vPath2
		_ASM_TEST2_(_Y)
		_ASM_TEST2_(_Z)
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}

static __inline grBoolean grExtBox_asmCollisionTestY2_(grFloat t, grVec3d *vPath, grVec3d *vPath2,
													   const grExtBox *B, grExtBox *xMovingBox)
{
	_asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		mov	  ecx,vPath2
		_ASM_TEST2_(_X)
		_ASM_TEST2_(_Z)		
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}

static __inline grBoolean grExtBox_asmCollisionTestZ2_(grFloat t, grVec3d *vPath, grVec3d *vPath2,
													   const grExtBox *B, grExtBox *xMovingBox)
{
	_asm
	{
		mov   esi,vPath
		mov   edi,B
		mov   ebx,xMovingBox
		mov   ecx,vPath2
		_ASM_TEST2_(_X)
		_ASM_TEST2_(_Y)		
		mov   eax,1
		jmp short End
NoCollision:
		xor   eax,eax
End:
	}
}


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
													grFloat *T, grVec3d *Normal, grVec3d *Point )
{
	grFloat t;
	grExtBox xStartBox,xChangeBox,*xStartBoxPtr=&xStartBox;
	grVec3d vPath,vPath2,*vPathPtr=&vPath,*vPathPtr2=&vPath2;
	grBoolean TestB;

	assert(B != NULL);
	assert(StartBox != NULL);
	assert(EndBox != NULL);

	// If the boxes already overlap, we have to report no collision
	// to be consistent with the rest of the engine collision calls.
	xStartBox = *StartBox;  // Used later as well.
	grExtBox_Translate(&xStartBox, Pos->X, Pos->Y, Pos->Z);
	if(grExtBox_Intersection(B, &xStartBox, NULL)) return GR_FALSE;	

	// Verify the sweepbox intersects this box
	grExtBox_Union(StartBox, EndBox, &xChangeBox);
	if(!grExtBox_Intersection(B, &xChangeBox, NULL)) return GR_FALSE;

	grVec3d_Subtract(&(EndBox->Min),&(StartBox->Min),&vPath);
	grVec3d_Subtract(&(EndBox->Max),&(StartBox->Max),&vPath2);

	// CollisionTest X-Front
	if (vPath.X<0.0f)
	{
		//t=(B->Max.X-xStartBox.Min.X)/vPath.X;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xStartBoxPtr
			fld   _B _MAX _X
			fsub  _XMOVINGBOX _MIN _X
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionX1
			fadd  _VPATH _X
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionX1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndX1
NoCollisionX1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndX1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,t,&vPoint2);
			//if (vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)		
			if (grExtBox_asmCollisionTestX2_(t, &vPath, &vPath2, B, &xStartBox) )
			{ 
				if (Normal) grVec3d_Set(Normal,1.0f,0.0f,0.0f);
				if (Point) *Point=B->Max;
				if (T) *T=t;
				return GR_TRUE;
			}
		}
	} 
	// CollisionTest X-Back
	if (vPath2.X>0.0f)
	{
		//t=(B->Min.X-xStartBox.Max.X)/vPath2.X;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr2
			mov   ebx,xStartBoxPtr
			fld   _B _MIN _X
			fsub  _XMOVINGBOX _MAX _X
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionX2
			fadd  _VPATH _X
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionX2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndX2
NoCollisionX2:
			fcompp
			mov   TestB,0
CTEndX2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,t,&vPoint2);
			//if (vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestX2_(t, &vPath, &vPath2, B, &xStartBox) )
			{
				if (Normal) grVec3d_Set(Normal,-1.0f,0.0f,0.0f);
				if (T) *T=t;
				if (Point) *Point=B->Min;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Y-Front
	if (vPath.Y<0.0f)
	{
		//t=(B->Max.Y-xStartBox.Min.Y)/vPath.Y;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xStartBoxPtr
			fld   _B _MAX _Y
			fsub  _XMOVINGBOX _MIN _Y
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionY1
			fadd  _VPATH _Y
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionY1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndY1
NoCollisionY1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndY1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,bT,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,bT,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestY2_(t, &vPath, &vPath2, B, &xStartBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,+1.0f,0.0f);
				if (T) *T=t;
				if (Point) *Point=B->Max;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Y-Back
	if (vPath2.Y>0.0f)
	{
		//t=(B->Min.Y-xStartBox.Max.Y)/vPath2.Y;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr2
			mov   ebx,xStartBoxPtr
			fld   _B _MIN _Y
			fsub  _XMOVINGBOX _MAX _Y
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionY2
			fadd  _VPATH _Y
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionY2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndY2
NoCollisionY2:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndY2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Z<=B->Max.Z && B->Min.Z<=vPoint2.Z)
			if (grExtBox_asmCollisionTestY2_(t, &vPath, &vPath2, B, &xStartBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,-1.0f,0.0f);
				if (T) *T=t;
				if (Point) *Point=B->Min;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Z-Front
	if (vPath.Z<0.0f)
	{
		//t=(B->Max.Z-xStartBox.Min.Z)/vPath.Z;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr
			mov   ebx,xStartBoxPtr
			fld   _B _MAX _Z
			fsub  _XMOVINGBOX _MIN _Z
			fldz
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionZ1
			fadd  _VPATH _Z
			fcom
			fnstsw ax
			test  ah,41h
			je   NoCollisionZ1
			fdivp st(1),st
			fstp  t
			jmp   short CTEndZ1
NoCollisionZ1:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndZ1:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y)
			if (grExtBox_asmCollisionTestZ2_(t, &vPath, &vPath2, B, &xStartBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,0.0f,+1.0f);
				if (T) *T=t;
				if (Point) *Point=B->Max;
				return GR_TRUE;
			}
		}
	}
	// CollisionTest Z-Back
	if (vPath2.Z>0.0f)
	{
		//t=(B->Min.Z-xStartBox.Max.Z)/vPath2.Z;
		//if (t>=0 && t<=1)
		TestB=GR_TRUE;
		__asm 
		{
			mov   edi,B
			mov   esi,vPathPtr2
			mov   ebx,xStartBoxPtr
			fld   _B _MIN _Z
			fsub  _XMOVINGBOX _MAX _Z
			fldz
			fcom
			fnstsw ax
			test  ah,41h
			je    NoCollisionZ2
			fadd  _VPATH _Z
			fcom
			fnstsw ax
			test  ah,1h
			jne   NoCollisionZ2
			fdivp st(1),st
			fstp  t
			jmp   short CTEndZ2
NoCollisionZ2:
			fcompp
			xor   eax,eax
			mov   TestB,0
CTEndZ2:
		}
		if(TestB)
		{
			//grVec3d_AddScaled(&xStartBox.Min,&vPath,t,&vPoint);
			//grVec3d_AddScaled(&xStartBox.Max,&vPath2,t,&vPoint2);
			//if (vPoint.X<=B->Max.X && B->Min.X<=vPoint2.X &&
			//	vPoint.Y<=B->Max.Y && B->Min.Y<=vPoint2.Y)
			if (grExtBox_asmCollisionTestZ2_(t, &vPath, &vPath2, B, &xStartBox) )
			{
				if (Normal) grVec3d_Set(Normal,0.0f,0.0f,-1.0f);
				if (T) *T=t;
				if (Point) *Point=B->Min;
				return GR_TRUE;
			}
		}
	}


	return GR_FALSE;
}