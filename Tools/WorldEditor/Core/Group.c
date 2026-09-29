/****************************************************************************************/
/*  GROUP.C                                                                             */
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
#include <Memory.h>
#include <String.h>

#include "Ram.h"
#include "Util.h"
#include "Errorlog.h"

#include "Group.h"

#define SIGNATURE	(0x85296307)
#define GROUP_MAXNAMELENGTH (31)

typedef struct tagGroup
{
	int32			Kind;
#ifdef _DEBUG
	int				nSignature ;
#endif
	ObjectList		*	pObjects ;
	struct tagGroup	*	Parent ;
	grBoolean			bVisible ;
	grBoolean			bLocked;
	int32				nIndex ;
	char			*	pszName ;
	uint32				Color ;
} Group ;


Group * Group_Create( const char * const pszName )
{
	Group * pGroup ;
	pGroup = GR_RAM_ALLOCATE_STRUCT( Group ) ;
	if( pGroup == NULL )
		goto GC_FAILURE ;

	memset( pGroup, 0, sizeof *pGroup ) ;
	assert( SIGNATURE == (pGroup->nSignature = SIGNATURE) ) ;	// ASSIGN

	pGroup->Color = 0x00FFFFFF ;

	pGroup->pszName = Util_StrDup( pszName ) ;
	if( pGroup->pszName == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Could not allocate group name" );
		goto GC_FAILURE ;
	}

	pGroup->pObjects = ObjectList_Create();
	if( pGroup->pObjects == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Failed to create group object list");
		goto GC_FAILURE  ;
	}

	pGroup->Kind = KIND_GROUP;
	pGroup->bVisible = GR_TRUE ;
	pGroup->bLocked	= GR_FALSE ;
	pGroup->Parent = NULL;

	return pGroup ;
	
GC_FAILURE :
	if( pGroup->pszName != NULL )
		grRam_Free( pGroup->pszName ) ;

	if( pGroup->pObjects != NULL )
		ObjectList_Destroy( &pGroup->pObjects, NULL );

	if( pGroup != NULL )
		Group_Destroy( &pGroup ) ;

	return NULL ;
}// Group_Create


void Group_ObjectDestroy( void * data )
{
	Object * pObject = (Object*)data;

	Object_Free( &pObject );
}

void Group_Destroy( Group ** ppGroup )
{
	assert( ppGroup != NULL ) ;
	assert( SIGNATURE == (*ppGroup)->nSignature ) ;

	if( (*ppGroup)->pszName != NULL )
	grRam_Free( (*ppGroup)->pszName ) ;

	if( (*ppGroup)->pObjects != NULL )
		ObjectList_Destroy( &(*ppGroup)->pObjects, Group_ObjectDestroy );

	assert( ((*ppGroup)->nSignature = 0) == 0 ) ;	// CLEAR
	grRam_Free( *ppGroup ) ;

}// Group_Destroy

// ACCESSORS

const char * Group_GetName( const Group * pGroup )
{
	return( pGroup->pszName );
}

ObjectList * Group_GetObjectList( const Group * pGroup )
{
	return( pGroup->pObjects );
}

const uint32 Group_GetColor( const Group * pGroup )
{
	assert( pGroup != NULL ) ;
	assert( SIGNATURE == pGroup->nSignature ) ;

	return pGroup->Color ;
}// Group_GetColor

Group *	Group_GetParent( const Group * pGroup )
{
	Group *pParent;

	assert( pGroup );

	pParent = pGroup->Parent;

	if( pParent != NULL )
	{
		while( pParent->Parent != NULL )
			pParent = pParent->Parent;
	}

	return( pParent );
}

grBoolean Group_IsLocked( const Group * pGroup )
{
	assert( pGroup );

	return( pGroup->bLocked );
}

uint32 Group_GetIndexTag( Group * pGroup ) 
{
	if( pGroup == NULL )
		return( (uint32)-1 );
	return( pGroup->nIndex );
}


// MODIFIERS

grBoolean Group_AddObject( Group * pGroup, Object * pObject )
{
	assert( pGroup );
	assert( pObject );

	Object_AddRef( pObject );
	if( ObjectList_Append( pGroup->pObjects, pObject ) == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Group_AddObject:Failed to append object" );
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

// Krouer: add to sorted list
grBoolean Group_AddObjectSorted( Group * pGroup, Object * pObject, ObjectList_SortCB Callback)
{
	assert( pGroup );
	assert( pObject );

	Object_AddRef( pObject );
	if( ObjectList_AppendSort( pGroup->pObjects, pObject, Callback ) == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Group_AddObject:Failed to append object" );
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

void Group_SetLocked( Group * pGroup, grBoolean bLocked )
{
	assert( pGroup );

	pGroup->bLocked = bLocked;
}

void Group_SetIndexTag( Group * pGroup, const uint32 nIndex ) 
{
	assert( pGroup );
	
	pGroup->nIndex = nIndex;
}

Group *	Group_FindLockedParent( Group * pGroup  )
{
	Group *pFoundGroup = NULL;
	Group *pCurGroup = pGroup;

	while( pCurGroup )
	{
		if( pCurGroup->bLocked )
			pFoundGroup = pCurGroup;
		pCurGroup = pCurGroup->Parent;
	}

	return( pFoundGroup );
}

grBoolean Group_AddGroup( Group * pGroup, Group * pChildGroup )
{
	assert( pGroup );
	assert( pChildGroup );

	if( pChildGroup->Parent )
		Group_RemoveGroup( pChildGroup->Parent, pChildGroup );
	pChildGroup->Parent = pGroup;
	if( ObjectList_Append( pGroup->pObjects, (Object*)pChildGroup ) == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Group_AddGroup:Failed to append group" );
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

void Group_RemoveObject( Group * pGroup, Object * pObject )
{
	ObjectList_Remove( pGroup->pObjects, pObject ) ;
	Object_Free( &pObject ) ;
}

void Group_RemoveGroup( Group * pGroup, Group * pChildGroup  )
{
	ObjectList_Remove( pGroup->pObjects, (Object*)pChildGroup ) ;
	pChildGroup->Parent = NULL;
}

// IS 
grBoolean Group_IsVisible( const Group * pGroup )
{
	assert( pGroup != NULL ) ;
	assert( SIGNATURE == pGroup->nSignature ) ;

	return pGroup->bVisible ;
}// Group_IsVisible

// Krouer : try to hide a group 
void Group_Show( Group * pGroup, grBoolean Visible )
{
	assert( pGroup != NULL ) ;
	assert( SIGNATURE == pGroup->nSignature ) ;

	pGroup->bVisible = Visible;
}// Group_IsVisible


//FILE
Group * Group_CreateFromFile( grVFile * pF )
{
	uint32 ParentIdx;
	Group	*	pGroup = NULL ;
	char		szName[ 30 ] ;

	assert( grVFile_IsValid( pF ) ) ;

	if( !Util_geVFile_ReadString( pF, szName, GROUP_MAXNAMELENGTH ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Group_CreateFromFile:Util_geVFile_ReadString", NULL);
		return NULL;
	}

	pGroup = Group_Create( szName );
	if( pGroup == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Group" );
		return( NULL );
	}
	if( grVFile_Read( pF, &pGroup->bLocked, sizeof(pGroup->bLocked) )  == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "Group_WriteToFile:grVFile_Read", NULL);
		return NULL;
	}

	if( grVFile_Read( pF, &pGroup->bVisible, sizeof(pGroup->bVisible) )  == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "Group_WriteToFile:grVFile_Read", NULL);
		return NULL;
	}

	if( grVFile_Read( pF, &pGroup->nIndex, sizeof(pGroup->nIndex) ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "Group_WriteToFile:grVFile_Read", NULL);
		return NULL;
	}
	if( grVFile_Read( pF, &ParentIdx, sizeof( ParentIdx ) )  == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ, "Group_WriteToFile:grVFile_Read", NULL);
		return NULL;
	}
	return pGroup ;
}// Group_CreateFromFile

grBoolean Group_WriteToFile( const Group * pGroup, grVFile * pF )
{
	uint32 ParentIdx;
	assert( pGroup != NULL ) ;
	assert( Group_IsValid( pGroup ) ) ;
	assert( grVFile_IsValid( pF ) ) ;

	if( grVFile_Write( pF, pGroup->pszName, strlen( pGroup->pszName )+1 ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Group_WriteToFile:grVFile_Write", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pGroup->bLocked, sizeof(pGroup->bLocked) ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Group_WriteToFile:grVFile_Write", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pGroup->bVisible, sizeof(pGroup->bVisible) ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Group_WriteToFile:grVFile_Write", NULL);
		return GR_FALSE;
	}

	if( grVFile_Write( pF, &pGroup->nIndex, sizeof(pGroup->nIndex) ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Group_WriteToFile:grVFile_Write", NULL);
		return GR_FALSE;
	}
	ParentIdx = Group_GetIndexTag( pGroup->Parent );
	if( grVFile_Write( pF, &ParentIdx, sizeof( ParentIdx ) )  == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Group_WriteToFile:grVFile_Write", NULL);
		return GR_FALSE;
	}
	return GR_TRUE ;

}// Group_WriteToFile

// DEBUGGING
#ifdef _DEBUG
grBoolean Group_IsValid( const Group * pGroup )
{
	assert( pGroup != NULL ) ;
	return SIGNATURE == pGroup->nSignature ;
}
#endif //_DEBUG


// KROUER: prefab extension
// Write callback
#include "brush.h"
#include "MaterialIdentList.h"

typedef struct {
	MaterialIdentList* pList;
	grMaterial_Array* pMatArray;
} BrushMaterialData;

grBoolean Group_WriteObjectCB(Object* pObj, void* pParam)
{
	OBJECT_KIND objKind;
	Brush_WriteInfo* pgwi = (Brush_WriteInfo*) pParam;

	objKind = Object_GetKind(pObj);

	// write the object kind first
	grVFile_Write(pgwi->pF, &objKind, sizeof(objKind));
	// copy brushes
	if (KIND_BRUSH == objKind) {
		Brush_WriteToFile((Brush*)pObj, pgwi);
	}

	return GR_TRUE;
}

grBoolean Group_EnumBrushTextureCB(Object* pObj, void* pParam)
{
	OBJECT_KIND objKind;
	MaterialIdentList* pList;

	BrushMaterialData* pbmd = (BrushMaterialData*) pParam;
	
	pList = pbmd->pList;

	objKind = Object_GetKind(pObj);

	if (KIND_BRUSH == objKind) {
		grBrush_Face* pJeFace;
		grBrush* pJeBrush;

		Brush* pBrush = (Brush*) pObj;

		pJeBrush= Brush_GetgrBrush(pBrush);
		pJeFace = grBrush_GetNextFace(pJeBrush, NULL);
		while (pJeFace) {
			grFaceInfo faceinfo;
			if (grBrush_FaceGetFaceInfo(pJeFace, &faceinfo) == GR_TRUE) {
				// find a way to add only once each material index
				// and recover the name associate with
				const grMaterial* pMaterial = grMaterial_ArrayGetMaterialByIndex(pbmd->pMatArray, faceinfo.MaterialIndex);
				if (pMaterial) {
					MaterialIdent* pMaterialIdent;
					const char* szName = grMaterial_GetName(pMaterial);
					
					pMaterialIdent = GR_RAM_ALLOCATE_STRUCT(MaterialIdent);
					pMaterialIdent->FileMatIdx = faceinfo.MaterialIndex;
					pMaterialIdent->WorldMatIdx = -1;
					strcpy(pMaterialIdent->MaterialName, szName);
					strcpy(pMaterialIdent->BitmapName, grMaterial_GetBitmapName(pMaterial));
					if (MaterialIdentList_Append(pList, pMaterialIdent) == NULL) {
						grRam_Free(pMaterialIdent);
					}
				}
			}
			pJeFace = grBrush_GetNextFace(pJeBrush, pJeFace);
		}
	}

	return GR_TRUE;
}

grBoolean Group_WriteMaterialIdentCB(MaterialIdent* pMat, void* pParam)
{
	grVFile* pF = (grVFile*) pParam;

	grVFile_Write(pF, pMat, sizeof(MaterialIdent));

	return GR_TRUE;
}

// Write all data about the group and its brushes
grBoolean Group_WriteToPrefabFile(const Group * pGroup, grVFile * pF, grPtrMgr* pPtrMgr, grMaterial_Array* pMatArray)
{
	long size;
	grBoolean bRetVal;
	Brush_WriteInfo gwi;

	// write the Group header
	bRetVal = Group_WriteToFile(pGroup, pF);

	if (bRetVal == GR_TRUE)
	{
		BrushMaterialData  BrushMatData;
		MaterialIdentList* pList;

		gwi.pF = pF;
		gwi.pPtrMgr = pPtrMgr;
		gwi.pWorld = NULL;

		pList = MaterialIdentList_Create();

		BrushMatData.pList = pList;
		BrushMatData.pMatArray = pMatArray;

		// write each group object
		ObjectList_EnumObjects(pGroup->pObjects, &BrushMatData, Group_EnumBrushTextureCB);

		size = MaterialIdentList_GetNumItems(pList);
		grVFile_Write(pF, &size, sizeof(size));

		MaterialIdentList_EnumMaterialIdents(pList, pF, Group_WriteMaterialIdentCB);

		MaterialIdentList_Destroy(&pList);

		size = ObjectList_GetNumItems(pGroup->pObjects);
		grVFile_Write(pF, &size, sizeof(size));

		// write each group object
		ObjectList_EnumObjects(pGroup->pObjects, &gwi, Group_WriteObjectCB);
	}
	return bRetVal;
}

grBoolean Group_BrushSortCB(Object* p1, Object* p2)
{
/*
	grBrush_Contents c1;
	grBrush_Contents c2;
	grBrush* b1 = Brush_GetgrBrush((Brush*)p1);
	grBrush* b2 = Brush_GetgrBrush((Brush*)p2);

	c1 = grBrush_GetContents(b1);
	c2 = grBrush_GetContents(b2);

	if (c1 == GR_BSP_CONTENTS_AIR && (c2 == GR_BSP_CONTENTS_SOLID || c2 == GR_BSP_CONTENTS_EMPTY)) {
		return GR_TRUE;
	}

	if (c1 == GR_BSP_CONTENTS_SOLID && c2 == GR_BSP_CONTENTS_EMPTY) {
		return GR_TRUE;
	}
*/
	//const char* name1 = Object_GetName(p1);
	//const char* name2 = Object_GetName(p2);
	//return (strcmp(name1, name2) < 0);
	return GR_FALSE;
}

/*! @brief Read the group and its brushes and add them to the current world
    @param pF The virtual file
    @param pPtrMgr The pointer manager instance
    @param pMatList The material list of the group
    @result The new created group
*/
Group	* Group_CreateFromPrefabFile(grVFile * pF, grPtrMgr* pPtrMgr, MaterialIdentList* pMatList)
{
	Group* pGroup = Group_CreateFromFile(pF);

	if (pGroup != NULL) {
		long idx;
		long size;
		MaterialIdent* pMatid;

		// Read the number of textures
		grVFile_Read(pF, &size, sizeof(size));
		for (idx=0; idx<size; idx++) {
			pMatid = GR_RAM_ALLOCATE_STRUCT(MaterialIdent);
			grVFile_Read(pF, pMatid, sizeof(MaterialIdent));
			MaterialIdentList_Append(pMatList, pMatid);
		}
		
		// Read the number of objects
		grVFile_Read(pF, &size, sizeof(size));

		for (idx=0; idx<size; idx++) {
			OBJECT_KIND objKind;
			// read object kind first
			grVFile_Read(pF, &objKind, sizeof(objKind));
			if (objKind == KIND_BRUSH) {
				// read one brush
				Brush* pBrush = Brush_CreateFromFile(pF, BRUSH_VERSION, pPtrMgr);
				// add the read brush to group
				Object_SetGroup((Object*)pBrush, pGroup);
				Group_AddObjectSorted(pGroup, (Object*)pBrush, Group_BrushSortCB);
			}
		}
	}
	return pGroup;
}


/* EOF: Group.c */