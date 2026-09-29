/****************************************************************************************/
/*  ENTITYTABLE.H                                                                       */
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

#ifndef ENTITYTABLE_H
#define ENTITYTABLE_H

// NOTE: EntityTable is "wrap" and utility functions for Symbol.h
#include "Symbol.h"
#include "Entity.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef grBoolean (*EntityTable_ForEachCallback)(grSymbol *pSymbol, void *lParam);

grSymbol_Table *	EntityTable_Create( void ) ;
void				EntityTable_Destroy( grSymbol_Table ** ppSymbols ) ;

grSymbol *			EntityTable_AddEntity( grSymbol_Table * pST, const char * pszType, const char * pszName ) ;
grBoolean			EntityTable_AddField( grSymbol_Table * pSymbols, grSymbol * pTypeSym, const char *Name, grSymbol_Type Type, void *DefaultValue ) ;
grBoolean			EntityTable_AddFieldToInstances( grSymbol_Table * pSymbols, grSymbol * pDef, const char * pszName, grSymbol_Type Type, void * DefaultValue ) ;
grSymbol *			EntityTable_CopyEntity( grSymbol_Table * pST, grSymbol * pEntity, const char * pszName ) ;
grSymbol *			EntityTable_CreateType( grSymbol_Table * pSymbols, const char * pszName ) ;
grBoolean			EntityTable_EnumDefinitions( grSymbol_Table * pSymbols, void * pVoid, EntityTable_ForEachCallback Callback ) ;
grBoolean			EntityTable_EnumFields( grSymbol_Table * pST, const char * pszType, void * pVoid, EntityTable_ForEachCallback Callback ) ;
grSymbol *			EntityTable_FindSymbol( grSymbol_Table * pST, const char * pszType, const char * pszName ) ;
grSymbol *			EntityTable_GetField( grSymbol_Table * pST, grSymbol * pEntity, const char * pszName ) ;
grBoolean			EntityTable_InitDefault( grSymbol_Table * pSymbols ) ;
int32				EntityTable_ListGetNumItems( grSymbol_List * pList ) ;
void				EntityTable_RemoveDefaultEntityField( grSymbol_Table * pST, grSymbol * pSymbol ) ;
void				EntityTable_RemoveEntityAndInstances( grSymbol_Table * pST, grSymbol * pEntityDef ) ;
grBoolean			EntityTable_SetDefaultValue( grSymbol_Table *pST, grSymbol *pFieldSym, void *DefaultValue ) ;

#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: EntityTable.h */