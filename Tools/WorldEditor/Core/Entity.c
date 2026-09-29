/****************************************************************************************/
/*  ENTITY.C                                                                            */
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
#include <String.h>

#include "ErrorLog.h"
#include "ObjectDef.h"	// Should be a private header file!
#include "Ram.h"
#include "Util.h"
#include "Genesis3D.h"

#include "Entity.h"
#include "EntityTable.h"

#define ENTITY_BOX_MIN				-0.1f
#define ENTITY_BOX_MAX				0.1f
#define ENTITY_DRAW_MIN				-3.0f
#define ENTITY_DRAW_MAX				3.0f
#define SIGNATURE					'ENTY'
#define ENTITY_FLAG_DIRTY			0x0001
#define ENTITY_FLAG_WBOUNDSDIRTY	0x0002
#define ENTITY_FLAG_DIRTYALL		(ENTITY_FLAG_DIRTY | ENTITY_FLAG_WBOUNDSDIRTY)

typedef struct tagEntity
{
	Object			ObjectData ;
#ifdef _DEBUG
	int				nSignature ;
#endif
	grSymbol_Table * pSymbolTable ;
	int32			Flags;
	grExtBox		WorldBounds ;
	grVec3d			Origin	;
	grSymbol	*	pSymbol ;
	char		*	pszType;
} Entity ;

//STATIC FUNCTIONS


static void Entity_GetOrigin( Entity * pEntity )
{
	grSymbol *FieldSymbol;

	if( pEntity->pSymbol == NULL )
		return;
	FieldSymbol = EntityTable_GetField( pEntity->pSymbolTable, pEntity->pSymbol, "Origin" ) ;

	if( FieldSymbol  == NULL )
		return;

	grSymbol_GetProperty( pEntity->pSymbol, FieldSymbol, &pEntity->Origin, 
		sizeof( pEntity->Origin ), GR_SYMBOL_TYPE_VEC3D );
}

static void Entity_SetOrigin( Entity * pEntity )
{
	grSymbol *FieldSymbol;

	if( pEntity->pSymbol == NULL )
		return;
	FieldSymbol = EntityTable_GetField( pEntity->pSymbolTable, pEntity->pSymbol, "Origin" ) ;

	if( FieldSymbol  == NULL )
		return;

	grSymbol_SetProperty( pEntity->pSymbol, FieldSymbol, &pEntity->Origin, 
		sizeof( pEntity->Origin ), GR_SYMBOL_TYPE_VEC3D );
}

static void Entity_SizeEdge( Entity * pEntity, const grVec3d * pStillEdge, const grFloat fScale, ORTHO_AXIS Axis )
{
	float	fTemp;


	fTemp = grVec3d_GetElement( &pEntity->Origin, Axis ) - grVec3d_GetElement( pStillEdge, Axis ) ;
	fTemp = fTemp * fScale ;
	fTemp = fTemp + grVec3d_GetElement( pStillEdge, Axis ) ;
	grVec3d_SetElement( &pEntity->Origin, Axis, fTemp ) ;
	Entity_SetOrigin( pEntity );
}

static char * Entity_AllocateNameWithNumber( const Entity * pEntity )
{
	char	*	pszNameAndNumber ;

	pszNameAndNumber = grRam_Allocate( strlen( pEntity->ObjectData.pszName ) + ENTITY_MAXNUMBERLENGTH ) ;
	if( pszNameAndNumber != NULL )
	{
		sprintf( pszNameAndNumber, "%s %d", pEntity->ObjectData.pszName, pEntity->ObjectData.nNumber ) ;
	}

	return pszNameAndNumber ;
}// Entity_AllocateNameWithNumber


Entity * Entity_Create( grSymbol_Table * pSymbols, Group * pGroup, const char * pszType, const char * pszName, const int32 nNumber )
{
	Entity	*	pEntity ;
	char	*	pszNameAndNumber ;
	assert( pszName != NULL ) ;
	assert( strlen( pszName ) < ENTITY_MAXNAMELENGTH ) ;

	pEntity = GR_RAM_ALLOCATE_STRUCT( Entity ) ;
	if( pEntity == NULL )
		return NULL ;

	memset( pEntity, 0, sizeof *pEntity ) ;
	assert( (pEntity->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN

	if( !Object_Init( &pEntity->ObjectData, pGroup, KIND_ENTITY, pszName, nNumber )  )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_Create:Object_Init" );
		goto EC_FAILURE ;
	}

	pEntity->pSymbolTable = pSymbols;
	pEntity->pszType = Util_StrDup( pszType );
	if( pEntity->pszType == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_Create:Util_StrDup" );
		goto EC_FAILURE ;
	}

	pszNameAndNumber = Entity_AllocateNameWithNumber( pEntity ) ;
	if( pszNameAndNumber == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_Create:Entity_AllocateNameWithNumber" );
		goto EC_FAILURE ;
	}
	
	pEntity->pSymbol = EntityTable_AddEntity( pSymbols, pszType, pszNameAndNumber ) ;
	grRam_Free( pszNameAndNumber ) ;
	if( pEntity->pSymbol == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_Create Object_Init" );
		goto EC_FAILURE ;
	}
	grExtBox_Set( &pEntity->WorldBounds, ENTITY_BOX_MIN, ENTITY_BOX_MIN, ENTITY_BOX_MIN,
										ENTITY_BOX_MAX, ENTITY_BOX_MAX, ENTITY_BOX_MAX );
	Entity_GetOrigin( pEntity );

	return( pEntity );

EC_FAILURE :
	Entity_Destroy( &pEntity ) ;
	return NULL ;

}// Entity_Create

Entity * Entity_Copy(  Entity *	pEntity, int32 nNumber )
{
	Entity * pNewEntity;
	char	*	pszNameAndNumber ;

	pNewEntity = Entity_Create( pEntity->pSymbolTable, pEntity->ObjectData.pGroup, pEntity->pszType, pEntity->ObjectData.pszName, nNumber );
	if( pNewEntity == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_Copy:Entity_Create" );
		return( NULL );
	}
	
	//Detroy the default symbol
	if( pNewEntity->pSymbol != NULL )
		grSymbol_TableRemoveSymbol( pNewEntity->pSymbolTable, pNewEntity->pSymbol ) ;

	pszNameAndNumber = grRam_Allocate( strlen( pEntity->ObjectData.pszName ) + ENTITY_MAXNUMBERLENGTH ) ;
	if( pszNameAndNumber == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Entity_Copy:pszNameAndNumber" );
		goto ECP_FAILURE ;
	}
	sprintf( pszNameAndNumber, "%s %d", pEntity->ObjectData.pszName, nNumber ) ;

	//Create a copy of the old symbol
	pNewEntity->pSymbol = EntityTable_CopyEntity( pEntity->pSymbolTable, pEntity->pSymbol, pszNameAndNumber ) ;

	grRam_Free( pszNameAndNumber ) ;

	
	pNewEntity->Origin = pEntity->Origin;
	Entity_SetOrigin( pNewEntity );
	Entity_UpdateBounds( pNewEntity );

	return( pNewEntity );

ECP_FAILURE :
	Entity_Destroy( &pNewEntity ) ;
	return NULL ;
}

void Entity_Destroy( Entity ** ppEntity )
{
	assert( ppEntity != NULL ) ;
	assert( (*ppEntity)->nSignature == SIGNATURE ) ;

	if( (*ppEntity)->pSymbol != NULL )
		grSymbol_TableRemoveSymbol( (*ppEntity)->pSymbolTable, (*ppEntity)->pSymbol ) ;

	assert( ((*ppEntity)->nSignature = 0) == 0 ) ;	// CLEAR
	(*ppEntity)->ObjectData.ObjectKind = KIND_INVALID ;

	grRam_Free( *ppEntity ) ;
}// Entity_Destroy

Entity * Entity_CreateTemplate( const char * const pszType, grSymbol_Table * pSymbols )
{
	Entity  * pEntity;

	assert( pszType != NULL ) ;
	assert( strlen( pszType ) < ENTITY_MAXNAMELENGTH ) ;

	pEntity = GR_RAM_ALLOCATE_STRUCT( Entity ) ;
	if( pEntity == NULL )
		return NULL ;

	memset( pEntity, 0, sizeof *pEntity ) ;
	assert( (pEntity->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN

	if( !Object_Init( &pEntity->ObjectData, NULL, KIND_ENTITY, pszType, 0 )  )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_CreateTemplate:Object_Init" );
		goto EC_FAILURE ;
	}

	pEntity->pSymbolTable = pSymbols;
	pEntity->pszType = Util_StrDup( pszType );
	if( pEntity->pszType == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Entity_CreateTemplate:pszType" );
		goto EC_FAILURE ;
	}
	grExtBox_Set( &pEntity->WorldBounds, ENTITY_BOX_MIN, ENTITY_BOX_MIN, ENTITY_BOX_MIN,
										ENTITY_BOX_MAX, ENTITY_BOX_MAX, ENTITY_BOX_MAX );
	return( pEntity );

EC_FAILURE :
	Entity_Destroy( &pEntity ) ;
	return NULL ;
}// Entity_CreateTemplate

Entity * Entity_FromTemplate( const char * pszName, Group * pGroup, const Entity *	pEntity, int32 nNumber )
{
	Entity * pNewEntity;

	pNewEntity = Entity_Create( pEntity->pSymbolTable, pGroup, pEntity->pszType, pszName, nNumber );
	if( pNewEntity == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Entity_FromTemplate:Entity_Create" );
		return( NULL );
	}
	pNewEntity->Origin = pEntity->Origin;
	Entity_SetOrigin( pNewEntity );
	Entity_UpdateBounds( pNewEntity );

	return( pNewEntity );
}
// MODIFIERS
void Entity_Move( Entity * pEntity, const grVec3d * pWorldDistance )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;

	Entity_SetModified( pEntity );
	grVec3d_Add( &pEntity->Origin, pWorldDistance, &pEntity->Origin );
	Entity_SetOrigin( pEntity );

}// Entity_Move

void Entity_Size( Entity * pEntity, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;

	// The Z axis is flipped (down is positive)
	// We just determine the edge and call _SizeEdge once or twice to keep this
	// as simple as possible
	switch( eSizeType )
	{
	case Select_Top :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		break ;

	case Select_Bottom :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		break ;

	case Select_Left :
		Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;

	case Select_Right :
		Entity_SizeEdge( pEntity, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_TopLeft :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;		

	case Select_TopRight :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		Entity_SizeEdge( pEntity, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_BottomLeft :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;
	
	case Select_BottomRight :
		if( Ortho_Axis_Z == VAxis )
			Entity_SizeEdge( pEntity, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			Entity_SizeEdge( pEntity, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		Entity_SizeEdge( pEntity, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;
	}
	Entity_SetModified( pEntity ) ;

}// Entity_Size

void Enity_SetField( const Entity * pEntity , grSymbol *FieldSymbol, void *pData, int32 DataSize )
{
	grSymbol_Type Type;
	
	assert( pEntity );
	assert( FieldSymbol );
	assert( pData );


	Type = grSymbol_GetType( FieldSymbol );

	grSymbol_SetProperty( pEntity->pSymbol, FieldSymbol, pData, 
		DataSize, Type );
}

void Entity_SetXForm( Entity * pEntity, const grXForm3d * XForm )
{
	assert( pEntity );
	assert( SIGNATURE == pEntity->nSignature ) ;
	assert( XForm );

	pEntity->Origin = XForm->Translation;
	Entity_SetOrigin( pEntity );
	Entity_SetModified( pEntity );

}// Entity_SetXForm


void Entity_UpdateBounds( Entity * pEntity )
{
	assert( pEntity );

	grExtBox_SetTranslation ( &pEntity->WorldBounds, &pEntity->Origin );

}

void Entity_SetModified( Entity * pEntity )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	
	pEntity->Flags |= ENTITY_FLAG_DIRTYALL ;	
}// Entity_SetModified


// ACCESSORS
void Entity_GetXForm( const Entity * pEntity, grXForm3d * XForm )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	
	grXForm3d_SetIdentity( XForm );
	XForm->Translation = pEntity->Origin;
}

const grExtBox * Entity_GetWorldAxialBounds( const Entity * pEntity )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	
	if( pEntity->Flags & ENTITY_FLAG_WBOUNDSDIRTY )
	{	
		Entity * pEvalEntity = (Entity*)pEntity ;			// Lazy Evaluation requires removing the const
		Entity_UpdateBounds( pEvalEntity ) ;
	}

	return &pEntity->WorldBounds ;

}// Entity_GetWorldAxialBounds

void Entity_GetWorldDrawBounds( const Entity * pEntity, grExtBox *DrawBounds )
{
	grVec3d Center;
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	
	if( pEntity->Flags & ENTITY_FLAG_WBOUNDSDIRTY )
	{	
		Entity * pEvalEntity = (Entity*)pEntity ;			// Lazy Evaluation requires removing the const
		Entity_UpdateBounds( pEvalEntity ) ;
	}
	grExtBox_GetTranslation ( &pEntity->WorldBounds, &Center );
	grExtBox_Set (  DrawBounds,
				  ENTITY_DRAW_MIN,	  ENTITY_DRAW_MIN,	  ENTITY_DRAW_MIN,
				  ENTITY_DRAW_MAX,	  ENTITY_DRAW_MAX,	  ENTITY_DRAW_MAX );
	grExtBox_SetTranslation ( DrawBounds, &Center );

}// Entity_GetWorldDrawBounds


const char * Entity_GetType( const Entity * pEntity )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	
	return( pEntity->pszType );
} //Entity_GetType

//IS
grBoolean	Entity_IsInRect( const Entity * pEntity, grExtBox *pSelRect, grBoolean bSelEncompeses )
{
	const grExtBox *pWorldBounds;
	grExtBox		Result;

	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	assert( pSelRect );
	
	pWorldBounds = Entity_GetWorldAxialBounds( pEntity ) ;
	if( bSelEncompeses )
	{
		if( pSelRect->Max.X >= pWorldBounds->Max.X &&
			pSelRect->Max.Y >= pWorldBounds->Max.Y &&
			pSelRect->Max.Z >= pWorldBounds->Max.Z &&
			pSelRect->Min.X <= pWorldBounds->Min.X &&
			pSelRect->Min.Y <= pWorldBounds->Min.Y &&
			pSelRect->Min.Z <= pWorldBounds->Min.Z )
			 return( GR_TRUE );
	}
	else
	{
		return( Util_geExtBox_Intersection ( pSelRect, pWorldBounds, &Result	) );
	}
	return( GR_FALSE );
}//Entity_IsInRect

grBoolean	Entity_GetField( const Entity * pEntity , grSymbol *FieldSymbol, void *pData, int32 DataSize, grBoolean *pDataInited )
{
	grSymbol_Type Type;
	
	assert( pEntity );
	assert( FieldSymbol );
	assert( pData );
	assert( pDataInited );

	if( pEntity->pSymbol == NULL )
		return GR_TRUE ;

	Type = grSymbol_GetType( FieldSymbol );

	if( !(*pDataInited) )
	{
		grSymbol_GetProperty( pEntity->pSymbol, FieldSymbol, pData, 
			DataSize, Type );
		*pDataInited = GR_TRUE;
		return( GR_TRUE );
	}
	switch( Type )
	{
		int		Integer;
		int		*OldInteger;
		grFloat	Float;
		grFloat	*OldFloat;
		grVec3d	Vector;
		grVec3d	*OldVector;
		GR_RGBA	Color;
		GR_RGBA	*OldColor;
		char	*String;
		char	*OldString;

	case GR_SYMBOL_TYPE_INT:
		assert( DataSize == sizeof(Integer) );
		OldInteger = (int*)pData;
		grSymbol_GetProperty(pEntity->pSymbol,
								  FieldSymbol,
								  &Integer,
								  sizeof(Integer),
								  GR_SYMBOL_TYPE_INT);
		return( Integer == *OldInteger );

	case GR_SYMBOL_TYPE_FLOAT:
		assert( DataSize == sizeof(grFloat) );
		OldFloat = (grFloat*)pData;
		grSymbol_GetProperty(pEntity->pSymbol,
								  FieldSymbol,
								  &Float,
								  sizeof(grFloat),
								  GR_SYMBOL_TYPE_FLOAT);
		return( Float == *OldFloat );

	case GR_SYMBOL_TYPE_COLOR:
		assert( DataSize == sizeof(GR_RGBA) );
		OldColor = (GR_RGBA*)pData;
		grSymbol_GetProperty(pEntity->pSymbol,
								  FieldSymbol,
								  &Color,
								  sizeof(GR_RGBA),
								  GR_SYMBOL_TYPE_COLOR);
		return( Color.r == OldColor->r &&
				Color.g	== OldColor->g &&
				Color.b	== OldColor->b &&
				Color.a	== OldColor->a 	);

	case GR_SYMBOL_TYPE_VEC3D:
		assert( DataSize == sizeof(grVec3d) );
		OldVector = (grVec3d*)pData;
		grSymbol_GetProperty(pEntity->pSymbol,
								  FieldSymbol,
								  &Vector,
								  sizeof(grVec3d),
								  GR_SYMBOL_TYPE_VEC3D);
		return( grVec3d_Compare( &Vector, OldVector, 0.0f ) );

	case GR_SYMBOL_TYPE_STRING:
		assert( DataSize == sizeof(char	*) );
		OldString = (char	*)pData;
		grSymbol_GetProperty(pEntity->pSymbol,
								  FieldSymbol,
								  &String,
								  sizeof(char	*),
								  GR_SYMBOL_TYPE_STRING);
		return( !strcmp( String, OldString ) );

	default:
		assert(!"Not finished here");
		return GR_FALSE;
	}

	return GR_FALSE;
}


grBoolean Enity_SelectClosest(  Entity * pEntity, FindInfo	*	pFindInfo )
{
	Point				pt;
	grVec3d				wpt ;
	grFloat				DistSq ;
	grXForm3d			wXForm;

	assert( pEntity != NULL );
	assert( pFindInfo != NULL );
	assert( pFindInfo->pOrtho  != NULL ) ;
			

	Entity_GetXForm( pEntity, &wXForm );
	wpt = wXForm.Translation;
	Ortho_WorldToView( pFindInfo->pOrtho, &wpt, &pt ) ;
	DistSq = Util_PointDistanceSquared( &pt, pFindInfo->pViewPt );
	if( DistSq < pFindInfo->fMinDistance )
	{
		pFindInfo->fMinDistance = DistSq ;
		pFindInfo->pObject = (Object*)pEntity ;
		pFindInfo->nFace = 0 ;
		pFindInfo->nFaceEdge = 0;
	}
	return( GR_TRUE );
}

// FILE HANDLING

Entity * Entity_CreateFromFile( grVFile * pF, const int32 nVersion, grSymbol_Table * pEntities )
{
	Entity	*	pEntity = NULL ;
	char		szType[ ENTITY_MAXNAMELENGTH ] ;
	assert( grVFile_IsValid( pF ) ) ;
	assert( nVersion <= ENTITY_VERSION ) ;
	
	if( ENTITY_VERSION != nVersion )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "Entity_CreateFromFile.\n", NULL);
		return NULL ;
	}

	if( !Util_geVFile_ReadString( pF, szType, ENTITY_MAXNAMELENGTH ) )
		goto ECFF_FAILURE ;

	pEntity = Entity_CreateTemplate( szType, pEntities ) ;
	if( pEntity == NULL )
		goto ECFF_FAILURE ;

	grRam_Free( pEntity->ObjectData.pszName ) ;
	if( !Object_InitFromFile( pF , &pEntity->ObjectData ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Object_InitFromFile.\n", NULL);
		goto ECFF_FAILURE ;
	}
	if( !grVFile_Read( pF, &pEntity->Flags, sizeof pEntity->Flags ) )
		goto ECFF_FAILURE ;

	if( !grVFile_Read( pF, &pEntity->WorldBounds, sizeof pEntity->WorldBounds ) )
		goto ECFF_FAILURE ;

	if( !grVFile_Read( pF, &pEntity->Origin, sizeof pEntity->Origin ) )
		goto ECFF_FAILURE ;

	return pEntity ;

ECFF_FAILURE :
	if( pEntity != NULL )
		Object_Free( (Object**)pEntity ) ;

	grErrorLog_AddString(GR_ERR_FILEIO_READ, "Entity_CreateFromFile.\n", NULL);
	return NULL ;

}// Entity_CreateFromFile


grBoolean Entity_WriteToFile( Entity * pEntity, grVFile * pF )
{
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	assert( grVFile_IsValid( pF ) ) ;

	if( grVFile_Write( pF, pEntity->pszType, strlen( pEntity->pszType )+1 ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Entity_WriteToFile.\n", NULL);
		return GR_FALSE;
	}
	if( !Object_WriteToFile( &pEntity->ObjectData, pF ) )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Object_WriteToFile.\n", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pEntity->Flags, sizeof pEntity->Flags ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Entity_WriteToFile.\n", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pEntity->WorldBounds, sizeof pEntity->WorldBounds ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Entity_WriteToFile.\n", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pEntity->Origin, sizeof pEntity->Origin ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Entity_WriteToFile.\n", NULL);
		return GR_FALSE;
	}

	return GR_TRUE ;

}// Entity_WriteToFile

grBoolean Entity_Reattach( Entity * pEntity )
{
	char * pszNameAndNumber ;
	assert( pEntity != NULL ) ;
	assert( SIGNATURE == pEntity->nSignature ) ;
	assert( pEntity->pSymbol == NULL ) ;
	assert( pEntity->pSymbolTable != NULL ) ;

	pszNameAndNumber = Entity_AllocateNameWithNumber( pEntity ) ;
	if( pszNameAndNumber == NULL )
		return GR_FALSE ;

	pEntity->pSymbol = EntityTable_FindSymbol( pEntity->pSymbolTable, pEntity->pszType, pszNameAndNumber ) ;
	grRam_Free( pszNameAndNumber ) ;
	return ( pEntity->pSymbol == NULL ) ? GR_FALSE : GR_TRUE ;

}// Entity_Reattach

/* EOF: Entity.c */