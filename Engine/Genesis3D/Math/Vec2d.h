/****************************************************************************************/
/*  VEC2D.H                                                                             */
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
#ifndef VEC2D_H
#define VEC2D_H

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grVec2d
{
	grFloat	X;
	grFloat	Y;
} grVec2d;

void	grVec2d_Set( grVec2d * V, grFloat X, grFloat Y ) ;

void	grVec2d_Add( const grVec2d * pV1, const grVec2d * pV2, grVec2d * pV1PlusV2 ) ;
void	grVec2d_Copy( const grVec2d *VSrc, grVec2d *VDst ) ;
void	grVec2d_Clear( grVec2d *V ) ;
grFloat	grVec2d_DistanceBetween( const grVec2d *V1, const grVec2d *V2 ) ;
grFloat	grVec2d_DistBetweenSquared( const grVec2d *V1, const grVec2d *V2 );
grFloat	grVec2d_DotProduct( const grVec2d *V1, const grVec2d *V2 ) ;
grFloat	grVec2d_Length( const grVec2d *V1 ) ;
grFloat	grVec2d_Normalize( grVec2d *V1 ) ;
void	grVec2d_Scale( const grVec2d *VSrc, grFloat fScale, grVec2d *VDst) ;
void	grVec2d_Subtract( const grVec2d *V1, const grVec2d *V2, grVec2d *V1MinusV2 ) ;

			//(assuming positive X is along 3 o'clock and Y is along '12')
void	grVec2d_Perp_Clockwise( const grVec2d *Src, grVec2d *Dst);
void	grVec2d_Perp_CClockwise( const grVec2d *Src, grVec2d *Dst);
			// makes a perpendicular vector (as close to cross product as you get in 2d)
void	grVec2d_Rotate( const grVec2d *pVec, grFloat Radians, grVec2d *pDest);
			// rotates clockwise (!?)

int		grVec2d_SideX(const grVec2d *pSeg1,const grVec2d *pSeg2,const grVec2d *pPoint);
			// -1 if point is to the left, +1 to the right, 0 is not in Y range
			// this is not oriented, its absolte X-lower is left

#define grVec2d_LengthSquared(V)	grVec2d_DotProduct(V,V)

#ifdef __cplusplus
}
#endif

#endif // Prevent Multiple Inclusion
/* EOF: grVec2d.h */
