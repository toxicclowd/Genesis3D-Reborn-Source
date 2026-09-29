/****************************************************************************************/
/*  SELECT.H                                                                            */
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

/* Open Source Revision -----------------------------------------------------------------
 By: Dennis Tierney (DJT) dtierney@oneoverz.com
 On: 12/27/99 8:50:01 PM
 Comments: 1) Select_KindsSelected() - Return selection mask.
           2) Select_All() - Select all.
----------------------------------------------------------------------------------------*/

#pragma once

#ifndef SELECT_H
#define SELECT_H

#include "Defs.h"
#include "ExtBox.h"
#include "Genesis3D.h"
#include "Level.h"
#include "jwObject.h"
#include "Ortho.h"
#include "Point.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SELECT_INVALID_NNUMBER	 -1
typedef	enum {
	SELECT_RESULT_NONE,
	SELECT_RESULT_CHANGED,
	SELECT_RESULT_SUBSELECT
} SELECT_RESULT;

grBoolean		Select_IsCorner( SELECT_HANDLE SelectHandle ) ;
grBoolean		Select_IsPointOverVertex( const Ortho * pOrtho, const Point * pViewPt, Level * pLevel ) ;

SELECT_RESULT	Select_ClosestThing( Level * pLevel, const Ortho * pOrtho, const Point * pViewPt, LEVEL_STATE eState, grExtBox * pWorldBounds, MODE eMode, grBoolean bControl_Held ) ;
grBoolean		Select_Face(Level * pLevel, const grCamera * pCamera,  const Point * pViewPt, uint32 *c1, uint32 *c2 ) ;
grBoolean		Select_CreateModel( Level * pLevel, const char * pszName ) ;
grBoolean		Select_CreateSelectedUndo( Level * pLevel, UNDO_TYPES Type ) ;
grBoolean		Select_Delete( Level * pLevel, grExtBox * pWorldBounds ) ;
grBoolean		Select_DeselectAll( Level * pLevel, grExtBox * pWorldBounds ) ;
grBoolean		Select_DupAndDeselectSelections( Level * pLevel ) ;
grBoolean		Select_Dup ( Level * pLevel ) ; // Added JH 25.03.2000
grBoolean		Select_DragBeginSub( Level * pLevel );
grBoolean		Select_DragEndSub( Level * pLevel );
grBoolean		Select_DragBegin( Level * pLevel );
grBoolean		Select_DragEnd( Level * pLevel );
grBoolean		Select_MoveSelectedVert( Level * pLevel, grVec3d * dWorldDist, grExtBox * WorldBounds );
// implemented new version - see below - DJT 
//grBoolean		Select_HasSelected( Level * pLevel, OBJECT_KIND eKinds ) ;
//
grBoolean		Select_HasSelectedVerts( Level * pLevel );
grBoolean		Select_DeselectAllVerts( Level *pLevel );
grBoolean		Select_DeselectAllFaces( Level *pLevel );
grBoolean		Select_AllFaces( Level * pLevel );
void			Select_NextFace( Level * pLevel );
void			Select_PrevFace( Level * pLevel );
void			Select_ApplyCurMaterial( Level * pLevel );
grBoolean		Select_GetEntityField( Level * pLevel, grSymbol *FieldSymbol, void *pData, int32 DataSize );
void			Select_GetFaceInfo( Level * pLevel, grFaceInfo *pFaceInfo, int32 *BlankFieldFlag );
void			Select_GetLightInfo( Level * pLevel, LightInfo *LightInfo, int32 *pBlankFieldFlag  );
//	Goes through the selection 
//  If the selection has differet types returns NULL
//	If the selection has same types but different names return NULL
//  If the selection has same type with same name return the name but nNumber set to SELECT_INVALID_NNUMBER
//  If the selctiion has only one thing it returns the name and the nNumber
const char  *	Select_GetName( Level * pLevel, int32 *nNumber );
SELECT_HANDLE	Select_NearestCornerHandle( Ortho * pOrtho, Point * pViewPt, grExtBox * pWorldBox ) ;
grBoolean		Select_Rectangle( Level * pLevel, grExtBox *pSelBox, grBoolean bSelEncompeses, int32 Mask, grExtBox *Bounds ); 
void			Select_SetFaceInfo( Level * pLevel, grFaceInfo *pFaceInfo, int32 BlankFieldFlag );
void			Select_SetLightInfo( Level * pLevel, LightInfo *pLightInfo, int32 BlankFieldFlag );
void			Select_SetEntityField( Level * pLevel, grSymbol *FieldSymbol, void *pData, int32 DataSize );
void			Select_SetName( Level * pLevel, const char * Name ) ;
grBoolean		Select_VertsInRectangle( Level * pLevel, grExtBox *pSelBox, grBoolean bSelEncompeses, grExtBox *Bounds ) ;
SELECT_HANDLE	Select_ViewPointHandle( Ortho * pOrtho, Point * pViewPt, grExtBox * pWorldBox ) ;
const char	*   Select_GetFirstEntityType( Level * pLevel );
grBoolean		Select_IsEdge( SELECT_HANDLE SelectHandle );
void			Select_GetBrushInfo( Level * pLevel, uint32 *Contents, int32 *pBlankFieldFlag );
void			Select_SetBrushInfo( Level * pLevel, uint32 Contents, int32 FieldFlag );
grProperty_List * Select_BuildDescriptor( Level * pLevel );
void			Select_SetProperty( int DataId, int DataType, grProperty_Data * pData );

//---------------------------------------------------
// Added DJT
//---------------------------------------------------
grBoolean       Select_All(Level * pLevel, int32 Mask, grExtBox *Bounds);
int32           Select_KindsSelected(Level * pLevel);
//---------------------------------------------------
// End DJT
//---------------------------------------------------


#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: Select.h */