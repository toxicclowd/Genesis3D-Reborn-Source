/****************************************************************************************/
/*  JWOBJECT.H                                                                          */
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

// Any World Object, Brush, Entity, Actor, etc.

#ifndef OBJECT_H
#define OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BaseType.h"
#include "XForm3d.h"
#include "ExtBox.h"
#include "Defs.h"
#include "vfile.h"
#include "grProperty.h"
#include "Ortho.h"
#include "object.h"


typedef struct tagGroup Group ;

typedef struct tagObject Object ;

typedef struct tagFindInfo
{
	const Ortho *	pOrtho ;
	const Point	*	pViewPt ;
	Object		*	pObject ;
	grFloat			fMinDistance ;
	int32			nFace ;
	int32			nFaceEdge ;
	MODE			eMode ;
	OBJECT_KIND		eSelKind ;
} FindInfo ;

#define BRUSH_REATTACH_GOOD	((uint32)(-1))
enum {
	OBJECT_NAME_FIELD = 1,
	OBJECT_POSITION_FIELD, 
	OBJECT_POSITION_FIELDX,
	OBJECT_POSITION_FIELDY,
	OBJECT_POSITION_FIELDZ,
	OBJECT_POSITION_FIELD_END
};

typedef grBoolean (*ObjectListCB)( Object *pObject, void * pVoid ) ;

void		Object_Free( Object ** ppObject ) ;
Object *	Object_Copy( Object * pObject, const int32 nNumber ) ;
void		Object_AddRef( Object * pObject );
char	*	Object_CreateDefaultName( OBJECT_KIND ObjectKind, int32 SubKind );
char	*	Object_CreateKindName(  Object * pObject );

// ACCESSORS
// Returns FALSE if object is wrong type.
OBJECT_KIND		Object_GetKind( const Object * pObject );
const char *	Object_GetName( const Object * pObject ) ;
int32			Object_GetNameTag( const Object * pObject  );
char		*	Object_GetNameAndTag( const Object * pObject ) ; //Allocates name
grBoolean		Object_GetTransform( Object * pObject, grXForm3d * pXForm );
grBoolean		Object_GetWorldAxialBounds( Object * pObject, grExtBox *ObjectBounds );
grBoolean		Object_GetWorldDrawBounds( Object * pObject, grExtBox *ObjectBounds );
uint32			Object_GetMiscFlags( const Object * pObject );
Group		*   Object_GetGroup( const Object * pObject );
uint32			Object_GetGroupTag( const Object * pObject );
grBoolean		Object_IsInLevel( const Object * pObject );
grBoolean		Object_SelectClosest(  Object * pObject, FindInfo	*	pFindInfo );
int32			Object_GetXFormModFlags( Object * pObject );
grObject	*	Object_GetgrObject( Object * pObject );

// MODIFY
void			Object_ClearMiscFlags( Object * pObject, uint32 nMiscFlags );
void			Object_SetMiscFlags( Object * pObject, uint32 nMiscFlags );
grBoolean		Object_Move(  Object * pObject, const grVec3d * pWorldDistance );
grBoolean		Object_Rotate( Object * pObject, ORTHO_AXIS RAxis, grFloat RadianAngle, const grVec3d * pRotationCenter ) ;
grBoolean		Object_Size( Object * pObject, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis ) ;
grBoolean		Object_SetTransform( Object *pObject, grXForm3d * pXForm );
grBoolean		Object_SetName( Object * pObject, const char * Name, int32 nNumber );
void			Object_Shear( Object * pObject, const grVec3d * pWorldDistance, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, const grExtBox * pSelectedBounds);
void			Object_SetGroupTag( Object * pObject, uint32 Tag  );
void			Object_SetGroup( Object * pObject, Group * pGroup );
grProperty_List * Object_BuildDescriptor( Object * pObject );
void			Object_SetProperty( Object * pObject, int DataId, int DataType, grProperty_Data * pData, grBoolean bLightUpdate, grBoolean bBrushUpdate, grBoolean bBrushLighting  );
void			Object_Update( Object *pObject, int Update_Type, grBoolean bOverideDirty );
void			Object_Dirty( Object *pObject );
void			Object_SetInLevel( Object *pObject, grBoolean bInLevel );
grBoolean		Object_SendMessage( Object *pObect, int32 message, void * data );

// CAN/IS
grBoolean		Object_IsInRect( const Object * pObject, grExtBox *pSelRect, grBoolean bSelEncompeses );
Group		*	Object_IsMemberOfLockedGroup( const Object * pObject );

//FILE

grBoolean		Object_WriteToFile( Object * pObject, grVFile * pF );
grBoolean		Object_InitFromFile( grVFile * pF , Object * pObject );
#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: Object.h */
