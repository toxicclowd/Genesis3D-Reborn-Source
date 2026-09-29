/****************************************************************************************/
/*  VEC2D.C                                                                             */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description:                                                                        */
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
#include <stdlib.h>

#include "Vec2d.h"

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#endif

void grVec2d_Set( grVec2d * V, grFloat X, grFloat Y )
{
	assert( V ) ;

	V->X = X ;
	V->Y = Y ;
}

void grVec2d_Add( const grVec2d * pV1, const grVec2d * pV2, grVec2d * pV1PlusV2 )
{
	assert ( pV1 );
	assert ( pV2 );
	assert ( pV1PlusV2 );
	
	pV1PlusV2->X = pV1->X + pV2->X;
	pV1PlusV2->Y = pV1->Y + pV2->Y;
}/* grVec2d_Add */

void grVec2d_Copy(const grVec2d *VSrc, grVec2d *VDst)
{
	assert ( VSrc );
	assert ( VDst );
	
	*VDst = *VSrc;
}//grVec2d_Copy

void grVec2d_Clear( grVec2d *V)
{
	assert ( V );
	
	V->X = 0.0f;
	V->Y = 0.0f;
}//grVec2d_Clear


grFloat grVec2d_DotProduct(const grVec2d *V1, const grVec2d *V2)
{
	assert ( V1 );
	assert ( V2 );
	
	return(V1->X*V2->X + V1->Y*V2->Y);
}//grVec2d_DotProduct


grFloat grVec2d_DistanceBetween(const grVec2d *V1, const grVec2d *V2)	// returns length of V1-V2	
{
	grVec2d B;
	
	assert( V1 );
	assert( V2 );

	grVec2d_Subtract(V1,V2,&B);
	return grVec2d_Length(&B);
}// grVec2d_DistanceBetween

// returns length of V1-V2 Squared
grFloat grVec2d_DistBetweenSquared(const grVec2d *V1, const grVec2d *V2)
{
	grFloat f, d;
	
	assert( V1 );
	assert( V2 );

	f = V2->Y - V1->Y;
	f *= f;
	d = V2->X - V1->X;
	d *= d;
	d += f;

	return(d);

} // grVec2d_DistanceBetween


grFloat grVec2d_Length(const grVec2d *V1)
{	
float Len;

#ifdef WIN32
	__asm
	{
		mov		eax,V1
		fld		[eax+0]		//	st(0) = vec->X
		fmul	[eax+0]		//	st(0) = vecX ^2

		fld		[eax+4]
		fmul	[eax+4]
							// now st(0),st(1) are Y^2,X^2
	    fxch    st(1)       // ST(0) mul is still in progress, so let's add
							//	ST(1) and ST(2)
        faddp   st(1),st(0) 
		fsqrt
        fstp    [Len]
	}
#endif
#ifdef BUILD_BE
	__asm__ __volatile__ ("
		movl %1, %%eax //; %%eax, %1  ;$1, %%eax
		fldl 0(%%eax) //[%%eax] ;// st(0) = vec->X
		fmull 0(%%eax) // [%%eax] ;// st(0) = vecX ^2
		
		fldl		4(%%eax) //[%%eax+4]
		fmull	4(%%eax) // [%%eax+4]
							//;// now st(0),st(1) are Y^2,X^2
	    fxch    %%st(1)     // ; // ST(0) mul is still in progress, so let's add
							//;//	ST(1) and ST(2)
        faddp   %%st(1),%%st(0) 
		fsqrt
        fstp    %0 //;[Len]
        "
        : "=m" (Len)// outputs
        : "g" (V1)
        : "%eax" , "%st(1)"// registers
        );
#endif

return Len;
}
//Vec2d_Length


grFloat grVec2d_Normalize( grVec2d *V1 )
{
	grFloat	OneOverDist;
	grFloat	Dist;

	assert( V1 );

	Dist = grVec2d_Length(V1);

	OneOverDist = 1.0f/( Dist + 0.000000001f);
	
	V1->X *= OneOverDist;
	V1->Y *= OneOverDist;

	return Dist;
}// grVec2d_Normalize

void grVec2d_Scale( const grVec2d *VSrc, grFloat fScale, grVec2d *VDst)
{
	assert ( VSrc );
	assert ( VDst );

	VDst->X = VSrc->X * fScale;
	VDst->Y = VSrc->Y * fScale;
}// grVec2d_Scale


void grVec2d_Subtract(const grVec2d *V1, const grVec2d *V2, grVec2d *V1MinusV2)
{
	assert ( V1 );
	assert ( V2 );
	assert ( V1MinusV2 );

	V1MinusV2->X = V1->X - V2->X;
	V1MinusV2->Y = V1->Y - V2->Y;
}// grVec2d_Subtract

void	grVec2d_Perp_Clockwise( const grVec2d *pVec, grVec2d *pDest)
{
grVec2d Vec;
	// rotates by 90 clockwise:
	assert(pVec && pDest);
	Vec = *pVec;
	pDest->X =	 Vec.Y;
	pDest->Y = - Vec.X;
}

void	grVec2d_Perp_CClockwise( const grVec2d *pVec, grVec2d *pDest)
{
grVec2d Vec;
	assert(pVec && pDest);
	Vec = *pVec;
	// rotates by 90 counterclockwise:
	pDest->X = - Vec.Y;
	pDest->Y =   Vec.X;
}

void	grVec2d_Rotate( const grVec2d *pVec, grFloat Radians, grVec2d *pDest)
{
grVec2d Vec;
grFloat c,s;
	
	assert(pVec);
	Vec = *pVec;

	// rotates clockwise:
	//	(really? seems true)

	c = grFloat_Cos(Radians);
	s = grFloat_Sin(Radians);

	pDest->X = + c * Vec.X + s * Vec.Y;
	pDest->Y = - s * Vec.X + c * Vec.Y;
}

int		grVec2d_SideX(const grVec2d *pSeg1,const grVec2d *pSeg2,const grVec2d *pPoint)
{
grFloat minY,maxY,minX,maxX;
	assert( pSeg1 && pSeg2 && pPoint );

	minY = min(pSeg1->Y,pSeg2->Y);
	maxY = max(pSeg1->Y,pSeg2->Y);
	minX = min(pSeg1->X,pSeg2->X);
	maxX = max(pSeg1->X,pSeg2->X);

	if ( pPoint->Y < minY || pPoint->Y >= maxY )
		return 0;

	if ( pPoint->X < minX )
		return -1;
	if ( pPoint->X >= maxX )
		return +1;

	{
	grFloat testX;

	// y = mx + b	(with x and y swapped)

	testX = pSeg1->X + ( pPoint->Y - pSeg1->Y ) * ( pSeg2->X - pSeg1->X ) / ( pSeg2->Y - pSeg1->Y );

	if ( pPoint->X < testX )
		return -1;
	else
		return 1;
	}

}

/* EOF: grVec2d.c */
