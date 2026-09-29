/****************************************************************************************/
/*  GROUP.H                                                                             */
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

#ifndef GROUP_H
#define GROUP_H

#include "BaseType.h"
#include "grTypes.h"	// RGBA
#include "jwObject.h"
#include "ObjectList.h"
#include "MaterialIdentList.h"

#include "grMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagGroup Group ;
typedef int32 GroupID ;

#define GROUP_MAXNAMELENGTH (31) 
#define GROUP_NEXTID		(-1)
#define GROUP_LASTID		(-1)

Group *			Group_Create( const char * const pszName ) ;
void			Group_Destroy( Group ** ppGroup ) ;

// ACCESSORS
const char	*	Group_GetName( const Group * pGroup ) ;
ObjectList *	Group_GetObjectList( const Group * pGroup );
const uint32	Group_GetColor( const Group * pGroup ) ;
//Goes up the links and returns the highest level parent
Group*			Group_GetParent( const Group * pGroup );
grBoolean		Group_IsLocked( const Group * pGroup ) ;
Group *			Group_FindLockedParent( Group * pGroup  );
uint32		 	Group_GetIndexTag( Group * pGroup ) ;

// IS
grBoolean		Group_IsVisible( const Group * pGroup ) ;
grBoolean		Group_AddGroup( Group * pGroup, Group * pChildGroup );
void			Group_RemoveObject( Group * pGroup, Object * pObject );
void			Group_RemoveGroup( Group * pGroup, Group * pChildGroup  );
// Krouer: hide/show a group
void            Group_Show( Group * pGroup, grBoolean Visible );

// MODIFIERS

grBoolean		Group_AddObject( Group * pGroup, Object * pObject );
void			Group_SetLocked( Group * pGroup, grBoolean bLocked );
void			Group_SetIndexTag( Group * pGroup, const uint32 nIndex ) ;

//FILE
grBoolean Group_WriteToFile( const Group * pGroup, grVFile * pF );
Group	* Group_CreateFromFile( grVFile * pF );

// DEBUGGING
#ifdef _DEBUG
grBoolean		Group_IsValid( const Group * pGroup ) ;
#endif

// KROUER : prefab extension
grBoolean Group_WriteToPrefabFile(const Group * pGroup, grVFile * pF, grPtrMgr* pPtrMgr, grMaterial_Array* pMatArray);
Group	* Group_CreateFromPrefabFile(grVFile * pF, grPtrMgr* pPtrMgr, MaterialIdentList* pMatList);


#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: Group.h */
