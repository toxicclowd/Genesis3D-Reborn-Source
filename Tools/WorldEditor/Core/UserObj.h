/****************************************************************************************/
/*  USEROBJ.H                                                                           */
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

#ifndef USEROBJ_H
#define USEROBJ_H

#include "grWorld.h"
#include "Object.h"
#include "defs.h"
#include "Group.h"
#include "grPtrMgr.h"

typedef struct tagUserObj UserObj ;

#ifdef __cplusplus
extern "C" {
#endif

// CREATORS
UserObj *			UserObj_Create( const char * const pszName, Group * pGroup, int32 nNumber, grObject	* pgeObject) ;
UserObj *			UserObj_Copy( UserObj *	pUserObj, int32 nNumber );
void				UserObj_Destroy( UserObj ** ppUserObj ) ;
char  *				UserObj_CreateDefaultName( grObject	* pgeObject );
char  *				UserObj_CreateKindName( );

// MODIFIERS
grBoolean			UserObj_Move( UserObj * pUserObj, const grVec3d * pWorldDistance ) ;
void				UserObj_SetModified( UserObj * pUserObj ) ;
void				UserObj_Snap( UserObj * pUserObj, grFloat fSnapSize ) ;
grBoolean			UserObj_SetXForm( UserObj * pUserObj, const grXForm3d * XForm );
void				UserObj_UpdateBounds( UserObj * pUserObj ) ;
grBoolean			UserObj_Size( UserObj * pUserObj, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis ) ;
grBoolean			UserObj_RemoveFromWorld( UserObj * pUserObj, grWorld * pWorld );
grBoolean			UserObj_AddToWorld( UserObj * pUserObj, grWorld * pWorld );
grBoolean			UserObj_UpdateData( UserObj * pUserObj );
grProperty_List *	UserObj_BuildDescriptor( UserObj * pUserObj );
void				UserObj_SetProperty( UserObj * pUserObj, int DataId, int DataType, grProperty_Data * pData, grBoolean bUpdate );
grProperty_List *	UserObj_GlobalPropertyList( const char * TypeName );
void				UserObj_SetGlobalProperty( const char * TypeName, int DataId, int DataType, grProperty_Data * pData );

void				UserObj_Update( UserObj * pUserObj, int Update_Type );
void				UserObj_Rotate( UserObj * pUserObj, ORTHO_AXIS RAxis, grFloat RadianAngle, const grVec3d * pRotationCenter );
grBoolean			UserObj_SendMessage( UserObj * pUserObj, int32 message, void * data );
int32				UserObj_GetXFormModFlag( UserObj * pUserObj );
void				UserObj_Select3d( UserObj* pUserObj, grVec3d * Front, grVec3d * Back, grVec3d * Impact );
#ifdef _USE_BITMAPS
void				UserObj_ApplyMatr( UserObj* pUserObj, grBitmap * pBitmap );
#else
void				UserObj_ApplyMatr( UserObj* pUserObj, grMaterialSpec * pMatSpec );
#endif

// ACCESSORS
grBoolean			UserObj_GetXForm( const UserObj * pUserObj, grXForm3d * XForm );
grBoolean			UserObj_GetWorldAxialBounds( const UserObj * pUserObj, grExtBox * BBox);
grBoolean			UserObj_GetWorldDrawBounds( const UserObj * pUserObj, grExtBox *DrawBounds );
grBoolean			UserObj_SelectClosest( UserObj * pUserObj, FindInfo	*	pFindInfo );
grObject *			UserObj_GetgrObject( UserObj * pUserObj );

// IS
grBoolean	UserObj_IsInRect( const UserObj * pUserObj, grExtBox *pSelRect, grBoolean bSelEncompeses );

// FILE
UserObj * UserObj_CreateFromFile( grVFile * pF, grPtrMgr * pPtrMgr );
grBoolean UserObj_WriteToFile( UserObj * pUserObj, grVFile * pF, grPtrMgr * pPtrMgr );

//PRESENTATION
void UserObj_RenderOrtho( const Ortho * pOrtho, UserObj *pUserObj, int32 hDC, grBoolean bColorOveride );


grBoolean UserObj_AddToObject( UserObj * pUserObj, grObject * pParent );

#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: UserObj.h */
