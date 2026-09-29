/****************************************************************************************/
/*  LIGHTLIST.C                                                                         */
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
// LightList contains a list to all the lights of a level as
// well a conatainer of all data needed to manupulate the lights.
// It also holds the Templete Light which is Dummy light structure
// which has not been allocated to the world light array.

#include <Assert.h>

#include "ErrorLog.h"
#include "Util.h"
#include "ram.h"

#include "LightList.h"

typedef struct LightList{
	List			*	pList;
	grWorld			*	pWorld;
} LightList;

static void LightList_DestroyLightCB( void *p1 )
{
	Light * pLight = (Light*)p1 ;
	assert( pLight != NULL ) ;

	Light_RemoveFromWorld( pLight );
	Object_Free( (Object**)&pLight ) ;
}// LightList_DestroyLightCB

static grBoolean LightList_FindCB( void *p1, void *lParam )
{
	return ( p1 == lParam ) ;
}// LightList_FindCB



LightList * LightList_Create( grWorld *pWorld )
{
	LightList *pLightList;

	pLightList = GR_RAM_ALLOCATE_STRUCT( LightList );
	if( pLightList == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate LightList" );
		return( NULL );
	}
	pLightList->pList = List_Create( );
	if( pLightList->pList == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grRam_Free( pLightList );
		return( NULL );
	}
	pLightList->pWorld = pWorld;

	return(pLightList) ;
}// LightList_Create


void LightList_Destroy( LightList **ppLightList )
{
	List_Destroy( &(*ppLightList)->pList, LightList_DestroyLightCB ) ;
	grRam_Free( (*ppLightList) );
}// LightList_Destroy

//
// ACCESSORS
//
int32 LightList_GetNumItems( const LightList * pLightList )
{
	assert( pLightList != NULL ) ;

	return List_GetNumItems( pLightList->pList ) ;
}// LightList_GetNumItems

Light * LightList_GetFirst( LightList * pLightList, LightIterator	*pBI )
{
	 ;
	assert( pLightList != NULL ) ;

	return (Light*)List_GetFirst( pLightList->pList, pBI ) ;
}// LightList_GetFirst

Light * LightList_GetNext( LightList * pLightList, LightIterator	*pBI )
{
	 ;
	assert( pLightList != NULL ) ;

	return (Light*)List_GetNext( pLightList->pList, pBI ) ;
}// LightList_GetFirst


LightIterator LightList_Find( LightList * pLightList, Light * pLight )
{
	LightIterator	pBI ;
	Light	*		pFoundLight ;

	assert( pLightList != NULL ) ;

	List_Search( pLightList->pList, LightList_FindCB, pLight, &pFoundLight, &pBI ) ;
	return pBI ;

}// LightList_Find

// MODIFIERS
LightIterator LightList_Append( LightList * pLightList, Light * pLight )
{
	assert( pLightList != NULL ) ;

	Object_AddRef( (Object*)pLight );
	return List_Append( pLightList->pList, pLight ) ;
}// LightList_Append

void LightList_Remove( LightList * pLightList, Light * pLight )
{
	LightIterator	pBI ;
	grBoolean		bFound ;
	Light	*		pFoundLight ;

	assert( pLightList != NULL ) ;

	bFound = List_Search( pLightList->pList, LightList_FindCB, pLight, &pFoundLight, &pBI ) ;
	assert( GR_TRUE == bFound ) ;

	List_Remove( pLightList->pList, pBI, NULL ) ;
}// LightList_Remove

void LightList_DeleteLight( LightList * pLightList, Light * pLight )
{
	LightList_Remove( pLightList, pLight );
	Object_Free( (Object**)&pLight );
}// LightList_DeleteLight

// ENUMERATION

int32 LightList_EnumLights( LightList * pLightList, void * pVoid, LightListCB Callback )
{
	assert( pLightList != NULL ) ;

	return List_ForEach( pLightList->pList, Callback, pVoid ) ;

}// LightList_EnumLightes



LightList * LightList_CreateFromFile( grVFile * pF, grWorld * pWorld, grPtrMgr * pPtrMgr  )
{
	LightList	*	pLightList = NULL ;
	Light		*	pLight ;
	int32			i ;
	int32			nItems ;
	int32			nVersion ;
	assert( grVFile_IsValid( pF ) ) ;

	if( !grVFile_Read( pF, &nVersion, sizeof nVersion ) )
		return NULL ;
	if( nVersion != LIGHT_VERSION )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "BrushList_CreateFromFile Version.\n", NULL);
		return NULL ;
	}

	if( !grVFile_Read( pF, &nItems, sizeof nItems ) )
		return NULL ;

	pLightList = LightList_Create( pWorld ) ;
	if( pLightList == NULL )
	{
		grErrorLog_AddString(GR_ERR_INTERNAL_RESOURCE, "Lightist_CreateFromFile\n", NULL);
		return NULL ;
	}

	for( i=0; i<nItems; i++ )
	{
		pLight = Light_CreateFromFile( pF, pWorld, pPtrMgr ) ;
		if( pLight == NULL )
		{
			grErrorLog_AddString(GR_ERR_INTERNAL_RESOURCE, "Lightist_CreateFromFile\n", NULL);
			return NULL ;
		}
		if( LightList_Append( pLightList, pLight ) == NULL )
		{	
			Light_Destroy( &pLight ) ;
			return NULL ;
		}
		Object_Free( (Object**)&pLight );
		Object_SetInLevel( (Object*)pLight, GR_TRUE );
	}
	return pLightList ;


}// LightList_CreateFromFile


grBoolean LightList_WriteToFile( LightList * pList, grVFile * pF, grPtrMgr * pPtrMgr )
{
	int32	nVersion ;
	int32	nItems ;
	Light	* pLight;
	ListIterator pli;

	assert( pList != NULL ) ;
	assert( grVFile_IsValid( pF ) ) ;

	nVersion = LIGHT_VERSION ;
	if( grVFile_Write( pF, &nVersion, sizeof nVersion ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "BrushList_WriteToFile.\n", NULL);
		return GR_FALSE;
	}
	
	nItems = LightList_GetNumItems( pList ) ;
	if( grVFile_Write( pF, &nItems, sizeof nItems ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "BrushList_WriteToFile.\n", NULL);
		return GR_FALSE;
	}
	
	pLight = (Light	*)List_GetFirst (pList->pList, &pli);
	while( pLight )
	{
		if( !Light_WriteToFile( pLight, pF, pPtrMgr ) )
		{
			grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Light_WriteToFile.\n", NULL);
			return GR_FALSE;
		}

		pLight = (Light*)List_GetNext( pList->pList, &pli );
	}
	return  GR_TRUE;

}// LightList_WriteToFile


/* EOF: LightList.h */
