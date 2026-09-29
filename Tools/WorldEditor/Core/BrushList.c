/****************************************************************************************/
/*  BRUSHLIST.C                                                                         */
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
// BrushList is a thin wrapper on List, a linked-list module.
// It provides casting and some simple validation

#include <Assert.h>

#include "ErrorLog.h"
#include "Util.h"

#include "BrushList.h"
#include "Model.h"
#include "jwObject.h"

typedef struct tagBrushModelReattachInfo
{
	BrushList	* pList ;
	grWorld		* pWorld ;
} BrushModelReattachInfo ;

static grBoolean BrushList_ClearMiscFlagsCB( Brush *pBrush, void* lParam )
{
	Object_ClearMiscFlags( (Object*)pBrush, (const uint32)lParam ) ;
	return GR_TRUE ;
}// BrushList_ClearMiscFlagsCB

static void BrushList_DestroyBrushCB( void *p1 )
{
	Brush * pBrush = (Brush*)p1 ;
	assert( pBrush != NULL ) ;

	Object_Free( (Object**)&pBrush ) ;
}// BrushList_DestroyBrushCB

static grBoolean BrushList_FindCB( void *p1, void *lParam )
{
	return ( p1 == lParam ) ;
}// BrushList_FindCB

static grBoolean BrushList_FindGECB( void *p1, void *lParam )
{
	Brush * pBrush = (Brush*)p1 ;
	assert( pBrush != NULL ) ;

	return ( Brush_GetgrBrush(pBrush) == lParam ) ;
}// BrushList_FindCB


static grBoolean BrushList_SetMiscFlagsCB( Brush *pBrush, void* lParam )
{
	Object_SetMiscFlags( (Object*)pBrush, (const uint32)lParam ) ;
	return GR_TRUE ;
}// BrushList_SetMiscFlagsCB

static grBoolean BrushList_WriteCB( Brush *pBrush, void* lParam )
{
	return Brush_WriteToFile( pBrush, (Brush_WriteInfo *)lParam ) ;
}// BrushList_ClearMiscFlagsCB


BrushList * BrushList_Create( void )
{
	return (BrushList*)List_Create( ) ;
}// BrushList_Create

void BrushList_Destroy( BrushList **ppList, BrushList_DestroyCallback DestroyFcn )
{
	List_Destroy( (List**)ppList, DestroyFcn ) ;
}// BrushList_Destroy

//
// ACCESSORS
//
int32 BrushList_GetNumItems( const BrushList * pList )
{
	assert( pList != NULL ) ;

	return List_GetNumItems( pList ) ;
}// BrushList_GetNumItems

Brush * BrushList_GetFirst( BrushList * pList, BrushIterator * pBI )
{
	assert( pList != NULL ) ;

	return (Brush*)List_GetFirst( pList, pBI ) ;
}// BrushList_GetFirst

Brush * BrushList_GetNext( BrushList * pList, BrushIterator * pBI )
{
	assert( pList != NULL ) ;

	return (Brush*)List_GetNext( pList, pBI ) ;
}// BrushList_GetFirst


BrushIterator BrushList_Find( BrushList * pList, Brush * pBrush )
{
	BrushIterator	pBI ;
	Brush	*		pFoundBrush ;

	assert( pList != NULL ) ;
	assert( GR_TRUE == Brush_IsValid( pBrush ) ) ;

	List_Search( pList, BrushList_FindCB, pBrush, &pFoundBrush, &pBI ) ;
	return pBI ;

}// BrushList_Find

Brush *	BrushList_FindByGeBrush( BrushList * pList, BrushIterator *Interator, grBrush * pgeBrush )
{
	Brush	*		pFoundBrush ;

	assert( pList != NULL ) ;

	List_Search( pList, BrushList_FindGECB, pgeBrush, &pFoundBrush, Interator ) ;
	return pFoundBrush ;
}

// MODIFIERS
BrushIterator BrushList_Append( BrushList * pList, Brush * pBrush )
{
	assert( pList != NULL ) ;
	assert( GR_TRUE == Brush_IsValid( pBrush ) ) ;

	Object_AddRef( (Object*)pBrush );
	return List_Append( pList, pBrush ) ;
}// BrushList_Append

grBoolean BrushList_AppendNoDup( BrushList * pList, Brush * pBrush )
{
	BrushIterator	pBI ;
	grBoolean		bFound ;
	Brush	*		pFoundBrush ;
	assert( pList != NULL ) ;
	assert( GR_TRUE == Brush_IsValid( pBrush ) ) ;

	bFound = List_Search( pList, BrushList_FindCB, pBrush, &pFoundBrush, &pBI ) ;
	if( bFound == GR_FALSE )
	{
		if( BrushList_Append( pList, pBrush ) == NULL )
			return GR_FALSE ;
	}
	return GR_TRUE ;
}// BrushList_AppendNoDup

void BrushList_ClearMiscFlags( BrushList * pList, const uint32 nFlags )
{
	assert( pList != NULL ) ;

	BrushList_EnumBrushes( pList, (void*)nFlags, BrushList_ClearMiscFlagsCB ) ;

}// BrushList_ClearMiscFlags

void BrushList_Remove( BrushList * pList, Brush * pBrush )
{
	BrushIterator	pBI ;
	grBoolean		bFound ;
	Brush	*		pFoundBrush ;

	assert( pList != NULL ) ;
	assert( GR_TRUE == Brush_IsValid( pBrush ) ) ;

	bFound = List_Search( pList, BrushList_FindCB, pBrush, &pFoundBrush, &pBI ) ;
	assert( GR_TRUE == bFound ) ;

	List_Remove( pList, pBI, NULL ) ;
}// BrushList_Remove

void BrushList_SetMiscFlags( BrushList * pList, const uint32 nFlags )
{
	assert( pList != NULL ) ;

	BrushList_EnumBrushes( pList, (void*)nFlags, BrushList_SetMiscFlagsCB ) ;

}// BrushList_SetMiscFlags

// ENUMERATION

int32 BrushList_EnumBrushes( BrushList * pList, void * pVoid, BrushListCB Callback )
{
	assert( pList != NULL ) ;

	return List_ForEach( pList, Callback, pVoid ) ;

}// BrushList_EnumBrushes


// FILE HANDLING
BrushList * BrushList_CreateFromFile( grVFile * pF, grPtrMgr * pPtrMgr )
{
	BrushList	*	pList = NULL ;
	Brush		*	pBrush ;
	int32			i ;
	int32			nItems ;
	int32			nVersion ;

	assert( grVFile_IsValid( pF ) ) ;

	if( !grVFile_Read( pF, &nVersion, sizeof nVersion ) )
		return NULL ;
	if( nVersion != BRUSH_VERSION )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "BrushList_CreateFromFile Version.\n", NULL);
		return NULL ;
	}

	if( !grVFile_Read( pF, &nItems, sizeof nItems ) )
		return NULL ;

	pList = BrushList_Create( ) ;
	if( pList == NULL )
		goto BLCFF_FAILURE ;

	for( i=0; i<nItems; i++ )
	{
		pBrush = Brush_CreateFromFile( pF, nVersion, pPtrMgr ) ;
		if( pBrush == NULL )
			goto BLCFF_FAILURE ;
		if( BrushList_Append( pList, pBrush ) == NULL )
		{	
			Object_Free( (Object**)&pBrush ) ;
			goto BLCFF_FAILURE ;
		}
		Object_SetInLevel( (Object*)pBrush, GR_TRUE );
		Object_Free( (Object**)&pBrush ) ;
	}
	return pList ;

BLCFF_FAILURE :
	if( pList != NULL )
		BrushList_Destroy( &pList, BrushList_DestroyBrushCB ) ;
	return NULL ;

}// BrushList_CreateFromFile


grBoolean BrushList_WriteToFile( BrushList * pList, Brush_WriteInfo * pWriteInfo  )
{
	int32	nVersion ;
	int32	nItems ;
	grVFile * pF = pWriteInfo->pF;
	assert( pList != NULL ) ;
	assert( grVFile_IsValid( pF ) ) ;

	nVersion = BRUSH_VERSION ;
	if( grVFile_Write( pF, &nVersion, sizeof nVersion ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "BrushList_WriteToFile.\n", NULL);
		return GR_FALSE;
	}
	
	nItems = BrushList_GetNumItems( pList ) ;
	if( grVFile_Write( pF, &nItems, sizeof nItems ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "BrushList_WriteToFile.\n", NULL);
		return GR_FALSE;
	}

	return BrushList_EnumBrushes( pList, pWriteInfo, BrushList_WriteCB ) ;

}// BrushList_WriteToFile


grBoolean BrushList_Reattach( BrushList * pList, Model * pModel, grWorld *pWorld )
{
	BrushReattachInfo	bri ;
	grBrush			*	pgeBrush ;
	grModel			*	pguModel = Model_GetguModel( pModel ) ;

	assert( pList != NULL ) ;
	assert( pModel != NULL ) ;

	// We have a model, look thru all its brushes and attach them

	bri.pModel = pModel ;
	bri.pWorld = pWorld;

	pgeBrush = NULL ;
	while( (pgeBrush = grModel_GetNextBrush( pguModel, pgeBrush )) != NULL )
	{
		//bri.nIndexTag = grBrush_GetIndexTag( pgeBrush ) ;
		bri.pgeBrush = pgeBrush ;
		BrushList_EnumBrushes( pList, &bri, Brush_ReattachCB ) ;
		assert( bri.nIndexTag == BRUSH_REATTACH_GOOD ) ;
	}
	return GR_TRUE ;

}// ModelList_Reattach


/* EOF: BrushList.h */
