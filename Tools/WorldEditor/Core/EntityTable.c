/****************************************************************************************/
/*  ENTITYTABLE.C                                                                       */
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

#include <Assert.h>
#include <Math.h>
#include <Stdio.h>
#include <StdLib.h>
#include <String.h>

#include "grTypes.h"
#include "EclipseNames.h"
#include "Vec3d.h"

#include "EntityTable.h"

typedef struct tagFieldDef
{
	char		*	pszName ;
	grSymbol_Type	Type ;
	char		*	pszDefault ;
} FieldDef ;

const static FieldDef TestFields[] = 
{
	{ "MinRadius", 	GR_SYMBOL_TYPE_INT, 	"20" },
	{ "MaxRadius", 	GR_SYMBOL_TYPE_INT, 	"40" },
	{ "FadeTime", 	GR_SYMBOL_TYPE_FLOAT, 	"0.5" },
	{ "Color", 		GR_SYMBOL_TYPE_COLOR, 	"255 255 255" },
	{ "Origin", 	GR_SYMBOL_TYPE_VEC3D, 	"5 0 5" },
	{ "Description", GR_SYMBOL_TYPE_STRING, "Hello there!" },
} ;

const static FieldDef PlayerStart[] = 
{
	{ "Origin",		GR_SYMBOL_TYPE_VEC3D,	"12 0 12" }
} ;

typedef struct tagDefaultsTypes
{
	char			*	pszName ;
	const FieldDef	*	pFields ;
	int					nFields ;
} DefaultTypes ;

const static DefaultTypes Defaults[] = 
{
	{ "Corona", TestFields, sizeof(TestFields)/sizeof(TestFields[0]) },
	{ "PlayerStart", PlayerStart, sizeof(PlayerStart)/sizeof(PlayerStart[0]) }
} ;

grSymbol_Table * EntityTable_Create( void )
{
	return grSymbol_TableCreate() ;
}// EntityTable_Create


void EntityTable_Destroy( grSymbol_Table ** ppSymbols )
{
	assert( ppSymbols != NULL ) ;
	assert( *ppSymbols != NULL ) ;

	grSymbol_TableDestroy( ppSymbols ) ;

}// EntityTable_Destroy

grBoolean EntityTable_AddField( grSymbol_Table * pSymbols, grSymbol * pTypeSym, const char *Name, grSymbol_Type Type, void *DefaultValue )
{
	grSymbol_List *	pFieldList;
	grSymbol *		pFieldSym;
	grBoolean		bFound;

	bFound = grSymbol_GetProperty
	(
		pTypeSym,
		grEclipseNames(pSymbols, GR_ECLIPSENAMES_STRUCTUREFIELDS),
		&pFieldList, 
		sizeof(pFieldList), 
		GR_SYMBOL_TYPE_LIST
	);

	if( GR_FALSE == bFound )
	{
		pFieldList = grSymbol_ListCreate( pSymbols );
		if( !pFieldList )
			return GR_FALSE ;
		if( grSymbol_SetProperty( pTypeSym,
							grEclipseNames(pSymbols, GR_ECLIPSENAMES_STRUCTUREFIELDS),
							&pFieldList, sizeof(pFieldList), GR_SYMBOL_TYPE_LIST) == GR_FALSE)
		{
			grSymbol_ListDestroy( &pFieldList ) ;
			return GR_FALSE;
		}
	}

	pFieldSym = grSymbol_Create(pSymbols, pTypeSym, Name, Type ) ;
	if( !pFieldSym )
		return GR_FALSE ;
	
	if( EntityTable_SetDefaultValue( pSymbols, pFieldSym, DefaultValue ) == GR_FALSE )
	{
		grSymbol_Destroy( &pFieldSym ) ;
		return GR_FALSE;
	}

	if( grSymbol_ListAddSymbol( pFieldList, pFieldSym ) == GR_FALSE )
	{
		grSymbol_Destroy( &pFieldSym ) ;
		return GR_FALSE;
	}

	grSymbol_Destroy( &pFieldSym ) ;
	grSymbol_ListDestroy( &pFieldList ) ;

	return GR_TRUE;
}// EntityTable_AddField

grBoolean EntityTable_AddFieldToInstances( grSymbol_Table * pSymbols, grSymbol * pDef, const char * pszName, grSymbol_Type Type, void * DefaultValue )
{
	grSymbol_List	*	pList ;
	grSymbol		*	pEntity ;
	grBoolean			bContinue ;
	int					iIndex ;
	assert( pSymbols != NULL ) ;
	assert( pDef != NULL ) ;
	assert( pszName != NULL ) ;
	assert( strlen(pszName) < ENTITY_MAXNAMELENGTH ) ;
	assert( DefaultValue != NULL ) ;

	pList = grSymbol_TableGetQualifiedSymbolList( pSymbols, grSymbol_GetQualifier( pDef ) ) ;
	if( pList == NULL )
		return GR_FALSE ;

	iIndex = 0 ;
	bContinue = GR_TRUE ;
	while( bContinue && (pEntity = grSymbol_ListGetSymbol( pList, iIndex )) != NULL )
	{
		iIndex++ ;
		if( grSymbol_Compare( pEntity, pDef ) == GR_FALSE )
		{
			bContinue = EntityTable_AddField( pSymbols, pDef, pszName, Type, DefaultValue ) ;
		}
	}
	grSymbol_ListDestroy( &pList ) ;

	return bContinue ;
}// EntityTable_AddFieldToInstances

grSymbol * EntityTable_CreateType( grSymbol_Table * pSymbols, const char * pszName )
{
	grSymbol		*	pTypeSym ;
	grSymbol		*	pQualifier ;
	grSymbol_List	*	pDefTypeList ;
	grSymbol		*	pGlobalTypesSymbol ;
	grSymbol		*	pDefinitionsProperty ;
	grBoolean			bSuccess ;

	pGlobalTypesSymbol = grEclipseNames( pSymbols, GR_ECLIPSENAMES_TYPES ) ;
	pDefinitionsProperty = grEclipseNames( pSymbols, GR_ECLIPSENAMES_TYPEDEFINITIONS ) ;
	if( pGlobalTypesSymbol == NULL || pDefinitionsProperty == NULL )
		return NULL ;
	
	// Can't do it if the symbol already exists.
	if( grSymbol_TableFindSymbol( pSymbols, NULL, pszName ) )
		return NULL ;

	// Create a package for the type, and intern the type symbol in that package
	pQualifier = grSymbol_Create( pSymbols, NULL, pszName, GR_SYMBOL_TYPE_VOID ) ;
	if( !pQualifier )
		return NULL ;

	pTypeSym = grSymbol_Create( pSymbols, pQualifier, pszName, GR_SYMBOL_TYPE_VOID ) ;
	grSymbol_Destroy( &pQualifier ) ;

	if( grSymbol_GetProperty( pGlobalTypesSymbol, pDefinitionsProperty, &pDefTypeList, sizeof pDefTypeList, GR_SYMBOL_TYPE_LIST ) == GR_FALSE )
	{
		// First time thru--create list and set it's property
		pDefTypeList = grSymbol_ListCreate( pSymbols ) ;
		bSuccess = GR_FALSE ;
		if( pDefTypeList != NULL )
		{
			bSuccess = grSymbol_SetProperty( pGlobalTypesSymbol, pDefinitionsProperty, &pDefTypeList, sizeof( pDefTypeList ), GR_SYMBOL_TYPE_LIST ) ;
		}

		if( pDefTypeList == NULL || bSuccess == GR_FALSE )
		{
			grSymbol_TableRemoveSymbol( pSymbols, pTypeSym ) ;
			grSymbol_Destroy( &pTypeSym ) ;
			return NULL ;
		}
	}
	
	if( grSymbol_ListAddSymbol( pDefTypeList, pTypeSym ) == GR_FALSE )
	{
		grSymbol_TableRemoveSymbol( pSymbols, pTypeSym ) ;
		grSymbol_Destroy( &pTypeSym ) ;
		return NULL ;
	}

	grSymbol_ListDestroy( &pDefTypeList ) ;

	return pTypeSym ;
}// EntityTable_CreateType

int32 EntityTable_ListGetNumItems( grSymbol_List * pList )
{
	grSymbol *	p ;
	int32		nCount ;
	assert( pList != NULL ) ;

	nCount = 0 ;
	while( (p = grSymbol_ListGetSymbol( pList, nCount )) != NULL )
	{
		nCount++ ;
	}
	return nCount ;
}// EntityTable_ListGetNumItems

grBoolean EntityTable_EnumDefinitions( grSymbol_Table * pSymbols, void * pVoid, EntityTable_ForEachCallback Callback )
{
	grSymbol_List	*	pDefTypeList ;
	grSymbol		*	pGlobalTypesSymbol ;
	grSymbol		*	pDefinitionsProperty ;
	grSymbol		*	pSymbol ;
	int					Index = 0 ;
	grBoolean			bContinue ;

	assert( pSymbols != NULL ) ;

	pGlobalTypesSymbol = grEclipseNames( pSymbols, GR_ECLIPSENAMES_TYPES ) ;
	pDefinitionsProperty = grEclipseNames( pSymbols, GR_ECLIPSENAMES_TYPEDEFINITIONS ) ;
	if( pGlobalTypesSymbol == NULL || pDefinitionsProperty == NULL )
		return GR_FALSE ;

	grSymbol_GetProperty( pGlobalTypesSymbol, pDefinitionsProperty, &pDefTypeList, sizeof pDefTypeList, GR_SYMBOL_TYPE_LIST ) ;
	assert( pDefTypeList != NULL ) ;

	bContinue = GR_TRUE ;
	while( bContinue && (pSymbol = grSymbol_ListGetSymbol( pDefTypeList, Index )) != NULL )
	{
		Index++ ;
		bContinue = Callback( pSymbol, pVoid ) ;
	}

	return GR_TRUE ;

}// EntityTable_Enum

grBoolean EntityTable_EnumFields( grSymbol_Table * pST, const char * pszType, void * pVoid, EntityTable_ForEachCallback Callback )
{
	grSymbol		*	pTypeSym ;
	grSymbol_List	*	pFieldList ;
	grSymbol		*	pFieldSym ;
	grSymbol		*	pEntityDef ;
	int					iField ;
	grBoolean			b ;

	assert( pST != NULL ) ;

	pTypeSym = grSymbol_TableFindSymbol( pST, NULL, pszType ) ;
	assert( pTypeSym != NULL ) ;

	pEntityDef = grSymbol_TableFindSymbol( pST, pTypeSym, grSymbol_GetName( pTypeSym ) ) ;	// TYPE::TYPE

	b = grSymbol_GetProperty
	(
		pEntityDef,
		grEclipseNames(pST, GR_ECLIPSENAMES_STRUCTUREFIELDS),
		&pFieldList, 
		sizeof(pFieldList), 
		GR_SYMBOL_TYPE_LIST
	);
	assert( b ) ;
	if( GR_FALSE == b )
		return GR_FALSE ;
	
	iField = 0 ;
	while( b && (pFieldSym = grSymbol_ListGetSymbol( pFieldList, iField )) != NULL )
	{
		b = Callback( pFieldSym, pVoid ) ;
		iField++ ;
	}
	grSymbol_ListDestroy( &pFieldList ) ;

	return b ;

}// EntityTable_EnumFields

grBoolean EntityTable_InitDefault( grSymbol_Table * pSymbols )
{
	int					i ;
	int					j ;
	int					nFields ;
	grSymbol		*	pDef ;
	assert( pSymbols != NULL ) ;

	for( i=0; i<sizeof(Defaults)/sizeof(Defaults[0]); i++ )
	{
		pDef = EntityTable_CreateType( pSymbols, Defaults[i].pszName ) ;
		if( pDef != NULL )
		{
			nFields = Defaults[i].nFields ;
			for( j=0; j<nFields; j++ )
			{
				EntityTable_AddField
				( 
					pSymbols, 
					pDef, 
					Defaults[i].pFields[j].pszName,
					Defaults[i].pFields[j].Type,
					Defaults[i].pFields[j].pszDefault
				) ;
			}
		}
	}
	
	return GR_TRUE ;
}// EntityTable_InitDefault

void EntityTable_RemoveDefaultEntityField( grSymbol_Table * pST, grSymbol * pSymbol )
{
	grSymbol		*	pEntityDef ;
	grSymbol_List	*	pFieldList ;
	grBoolean			b ;
	assert( pST != NULL ) ;
	assert( pSymbol != NULL ) ;
	
	// Incoming ENTITY::ENTITY::FIELD
	pEntityDef = grSymbol_GetQualifier( pSymbol ) ;	// ENTITY::ENTITY

	// Remove from structure fields
	b = grSymbol_GetProperty
	(
		pEntityDef,
		grEclipseNames(pST, GR_ECLIPSENAMES_STRUCTUREFIELDS),
		&pFieldList, 
		sizeof(pFieldList), 
		GR_SYMBOL_TYPE_LIST
	);
	if( b )
	{
		grSymbol_ListRemoveSymbol( pFieldList, pSymbol ) ;
		grSymbol_ListDestroy( &pFieldList ) ;
	}

	grSymbol_TableRemoveSymbol( pST, pSymbol ) ;

}// EntityTable_RemoveDefaultEntityField

void EntityTable_RemoveEntityAndInstances( grSymbol_Table * pST, grSymbol * pEntityDef )
{
	grSymbol_List	*	pList ;
	grSymbol		*	p ;
	int32				nIndex ;

	assert( pST != NULL ) ;
	assert( pEntityDef != NULL ) ;

	// List will have the "definition" and all instances
	pList = grSymbol_TableGetQualifiedSymbolList( pST, grSymbol_GetQualifier(pEntityDef) ) ;
	assert( pList != NULL ) ;
	nIndex = 0 ;
	while( (p = grSymbol_ListGetSymbol( pList, nIndex )) != NULL )
	{
		nIndex++ ;
		grSymbol_TableRemoveSymbol( pST, p ) ;
		grSymbol_Destroy( &p ) ;
	}
	grSymbol_ListDestroy( &pList ) ;

//	EntityTable_RemoveDefaultEntityField( pST, pEntityDef ) ;
//	grSymbol_Destroy( &pEntityDef ) ;

}// EntityTable_RemoveEntityAndInstances


grBoolean EntityTable_SetDefaultValue( grSymbol_Table *pST, grSymbol *pFieldSym, void *DefaultValue )
{
	grSymbol_Type	Type;
	grSymbol *		pDefaultValueSym;

	pDefaultValueSym = pFieldSym ;

//	DefaultValueSym = grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE);
//	if	(!DefaultValueSym)
//		return GR_FALSE;

	Type = grSymbol_GetType( pFieldSym ) ;
	switch( Type )
	{
		int		Integer;
		grFloat	Float;
		grVec3d	Vector;
		GR_RGBA	Color;

	case GR_SYMBOL_TYPE_INT:
		Integer = atoi(DefaultValue);
		return grSymbol_SetProperty(pFieldSym,
								  pDefaultValueSym,
								  &Integer,
								  sizeof(Integer),
								  GR_SYMBOL_TYPE_INT);

	case GR_SYMBOL_TYPE_FLOAT:
		Float = (grFloat)atof(DefaultValue);
		return grSymbol_SetProperty(pFieldSym, 
								pDefaultValueSym,
								&Float, sizeof(Float), GR_SYMBOL_TYPE_FLOAT);

	case GR_SYMBOL_TYPE_COLOR:
		sscanf( DefaultValue, "%f %f %f", &Color.r, &Color.g, &Color.b);
		return grSymbol_SetProperty( pFieldSym, 
								pDefaultValueSym,
								&Color, sizeof(Color), GR_SYMBOL_TYPE_COLOR);

	case GR_SYMBOL_TYPE_VEC3D:
		sscanf(DefaultValue, "%f %f %f", &Vector.X, &Vector.Y, &Vector.Z);
		return grSymbol_SetProperty(pFieldSym, 
								pDefaultValueSym,
								&Vector, sizeof(Vector), GR_SYMBOL_TYPE_VEC3D);
		break;

	case GR_SYMBOL_TYPE_STRING:
		return grSymbol_SetProperty(pFieldSym, 
								pDefaultValueSym,
								DefaultValue, sizeof(DefaultValue), GR_SYMBOL_TYPE_STRING);
		break;

	default:
		assert(!"Not finished here");
		return GR_FALSE;
	}

	assert(!"Shouldn't get here");
	pST;
}// EntityTable_SetDefaultValue

//grSymbol * Entity_TableGetType( 

grSymbol * EntityTable_GetField( grSymbol_Table * pST, grSymbol * pEntity, const char * pszName )
{
	grSymbol * pQualifier ;
	grSymbol * pEntityDef ;
	grSymbol * pField = NULL ;

	assert( pST != NULL ) ;
	assert( pEntity != NULL ) ;
	assert( pszName != NULL ) ;
	assert( strlen( pszName ) < ENTITY_MAXNAMELENGTH ) ;

	pQualifier = grSymbol_GetQualifier( pEntity ) ;	// TYPE:
	pEntityDef = grSymbol_TableFindSymbol( pST, pQualifier, grSymbol_GetName( pQualifier ) ) ;	// TYPE::TYPE
	if( pEntityDef != NULL )
		pField = grSymbol_TableFindSymbol( pST, pEntityDef, pszName ) ;	// TYPE::TYPE::FIELD

	return pField ;

}// EntityTable_GetField

grSymbol * EntityTable_AddEntity( grSymbol_Table * pST, const char * pszType, const char * pszName )
{
	grSymbol		*	pType ;
	grSymbol		*	pEntityDef ;
	grSymbol		*	pEntity ;
	grSymbol		*	pProperty ;
	grSymbol_List	*	pFieldList ;
	int					iProperty ;
	grBoolean			b ;

	assert( pST != NULL ) ;
	assert( pszType != NULL ) ;
	assert( strlen( pszType ) < ENTITY_MAXNAMELENGTH ) ;
	assert( pszName != NULL ) ;
	assert( strlen( pszName ) < ENTITY_MAXNAMELENGTH ) ;

	pType = grSymbol_TableFindSymbol( pST, NULL, pszType ) ;
	assert( pType != NULL ) ;

	pEntityDef = grSymbol_Create( pST, pType/*grSymbol_GetQualifier( pType )*/, pszType, GR_SYMBOL_TYPE_SYMBOL ) ;
	if( pEntityDef == NULL )
		return NULL ;

	pEntity = grSymbol_Create( pST, pType, pszName, GR_SYMBOL_TYPE_SYMBOL ) ;
	if( pEntity == NULL )
		return NULL ;

	// Get the list of fields for this Entity
	b = grSymbol_GetProperty
	( 
		pEntityDef, 
		grEclipseNames( pST, GR_ECLIPSENAMES_STRUCTUREFIELDS), 
		&pFieldList, 
		sizeof pFieldList, 
		GR_SYMBOL_TYPE_LIST
	) ;
	if( b == GR_FALSE )
		return pEntity ;

	// For each field, set the default value from the def entity
	iProperty = 0 ;
	b = GR_TRUE ;
	while( b && (pProperty = grSymbol_ListGetSymbol( pFieldList, iProperty )) != NULL )
	{
		b = grSymbol_CopyProperty( pEntity, pProperty, pProperty, pProperty ) ;
		if( b == GR_FALSE )
		{
			grSymbol_Destroy( &pEntity ) ;
			pEntity = NULL ;		// Just to be sure
			break ;
		}
		iProperty++ ;
	}

	grSymbol_ListDestroy( &pFieldList ) ;

	return pEntity ;
}// EntityTable_AddEntity


grSymbol * EntityTable_CopyEntity( grSymbol_Table * pST, grSymbol * pEntity, const char * pszName )
{
	grSymbol		*	pType ;
	grSymbol		*	pEntityDef ;
	grSymbol		*	pNewEntity ;
	grSymbol		*	pProperty ;
	grSymbol_List	*	pFieldList ;
	int					iProperty ;
	grBoolean			b ;

	assert( pST != NULL ) ;
	assert( pszName != NULL ) ;
	assert( strlen( pszName ) < ENTITY_MAXNAMELENGTH ) ;

	pType = grSymbol_GetQualifier( pEntity ) ;	// TYPE:
	assert( pType != NULL ) ;

	pEntityDef = grSymbol_TableFindSymbol( pST, pType, grSymbol_GetName( pType ) ) ;
	if( pEntityDef == NULL )
		return NULL ;

	pNewEntity = grSymbol_Create( pST, pType, pszName, GR_SYMBOL_TYPE_SYMBOL ) ;
	if( pNewEntity == NULL )
		return NULL ;

	// Get the list of fields for this Entity
	b = grSymbol_GetProperty
	( 
		pEntityDef, 
		grEclipseNames( pST, GR_ECLIPSENAMES_STRUCTUREFIELDS), 
		&pFieldList, 
		sizeof pFieldList, 
		GR_SYMBOL_TYPE_LIST
	) ;
	if( b == GR_FALSE )
		return pNewEntity ;

	// For each field, set the default value from the def entity
	iProperty = 0 ;
	b = GR_TRUE ;
	while( b && (pProperty = grSymbol_ListGetSymbol( pFieldList, iProperty )) != NULL )
	{
		b = grSymbol_CopyProperty( pNewEntity, pProperty, pEntity, pProperty ) ;
		if( b == GR_FALSE )
		{
			grSymbol_Destroy( &pNewEntity ) ;
			pNewEntity = NULL ;		// Just to be sure
			break ;
		}
		iProperty++ ;
	}

	grSymbol_ListDestroy( &pFieldList ) ;

	return pNewEntity ;
}// EntityTable_CopyEntity


grSymbol * EntityTable_FindSymbol( grSymbol_Table * pST, const char * pszType, const char * pszName )
{
	grSymbol * pTypeSym ;
	grSymbol * pEntity ;

	assert( pST != NULL ) ;
	assert( pszType != NULL ) ;
	assert( strlen(pszType) < ENTITY_MAXNAMELENGTH ) ;
	assert( pszName != NULL ) ;
	assert( strlen(pszName) < ENTITY_MAXNAMELENGTH ) ;

	pTypeSym = grSymbol_TableFindSymbol( pST, NULL, pszType ) ;
	assert( pTypeSym != NULL ) ;

	pEntity = grSymbol_TableFindSymbol( pST, pTypeSym, pszName ) ;
	assert( pEntity != NULL ) ;

	return pEntity ;

}// EntityTable_FindSymbol


/* EOF: EntityTable.c */