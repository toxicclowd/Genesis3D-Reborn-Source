/****************************************************************************************/
/*  TRANSFORM.H                                                                         */
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
#pragma once

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "BaseType.h"
#include "Brush.h"
#include "Level.h"
#include "XForm3d.h"

#ifdef __cplusplus
extern "C" {
#endif

void		Transform_GetHandlePoint( SELECT_HANDLE eCorner, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, const grExtBox * pBounds, grVec3d * pPoint ) ;
void		Transform_PlaceSnap( Level * pLevel, grVec3d *placePt, grVec3d * pSnapDelta );
void		Transform_MoveSelected( Level * pLevel, const grVec3d * pWorldDistance, grExtBox * pWorldBounds ) ;
void		Transform_MoveSelectedSub( Level * pLevel, const grVec3d * pWorldDistance, grExtBox * pWorldBounds );
void		Transform_MoveSnapSelected( Level * pLevel, SELECT_HANDLE eCorner, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds, grVec3d * pSnapDelta ) ;
void		Transform_PointToGrid( Level * pLevel, const grVec3d * pPoint, grVec3d * pGridPoint ) ;
void		Transform_RotateSelected( Level * pLevel, grFloat fRadianAngle, ORTHO_AXIS RAxis, grVec3d *pCenter3d, grExtBox * pWorldBounds ) ;
void		Transform_RotateSubSelected( Level * pLevel, grFloat fRadianAngle, ORTHO_AXIS RAxis, grExtBox * pWorldBounds ) ;
void		Transform_SizeSelected( Level * pLevel, const grVec3d * pWorldDistance, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds ) ;
void		Transform_SizeSnapSelected( Level *	pLevel, SELECT_HANDLE eCorner, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds, grVec3d * pSnapDelta ) ;
void		Transform_SnapBounds( const grExtBox * pBox, const grFloat fSnapSize, grVec3d * pDelta ) ;
void		Transform_SnapPoint( const grVec3d * pPoint, const grFloat fSnapSize, grVec3d * pDelta ) ;
void		Transform_SnapPointLR( const grVec3d * pPoint, const grFloat fSnapSize, grVec3d * pDelta ) ;
grBoolean	Transform_AddSelectedUndo( Level * pLevel, UNDO_TYPES Type  );
grBoolean	Transform_AddShearSelectedUndo( Level * pLevel );
void Transform_ShearSelected( Level * pLevel, const grVec3d * pWorldDistance,  SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds );
#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: Transform.h */