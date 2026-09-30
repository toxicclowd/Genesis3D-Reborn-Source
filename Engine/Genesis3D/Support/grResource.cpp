/****************************************************************************************/
/*  JERESOURCE.C                                                                        */
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
//
//	Used to manage a list of resources.
//
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <string.h>
#include "Ram.h"
#include "grChain.h"
#include "Util.h"
#include "grResource.h"
#include "Errorlog.h" // Added by Incarnadine
#include "Log.h"
#include "Engine.h"

#ifdef BUILD_BE
#define stricmp strcasecmp
#endif

////////////////////////////////////////////////////////////////////////////////////////
//	grResourceMgr struct
////////////////////////////////////////////////////////////////////////////////////////
typedef struct grResourceMgr
{
	grChain	*List;
	int		RefCount;

	grEngine* Engine;
	struct grResourceMgr* NextLive;	// every live manager, for grResource_RemapTextures
} grResourceMgr;


////////////////////////////////////////////////////////////////////////////////////////
//	grResource struct
////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	char	*Name;
    uint32  Type;
	void	*Data;
	int32	RefCount;
	grBoolean OpenDir;	// [MLB-ICE] Added by Icestorm

} grResource;


static grResourceMgr* g_pSingleResource = NULL;
static grResourceMgr* g_pLiveMgrs = NULL;

GRAPI grResourceMgr* GRCC grResourceMgr_GetSingleton()
{
	return g_pSingleResource;
}


////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_MgrCreate()
//
//	Create a resource manager.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grResourceMgr * GRCC grResource_MgrCreate(grEngine* pEngine)
{
	// locals
	grResourceMgr	*ResourceMgr;

	// create resource list struct
	ResourceMgr = (grResourceMgr *)grRam_AllocateClear( sizeof( *ResourceMgr ) );
	if ( ResourceMgr == NULL )
	{
		return NULL;
	}

	if (g_pSingleResource == NULL)
		g_pSingleResource = ResourceMgr;
	ResourceMgr->NextLive = g_pLiveMgrs;
	g_pLiveMgrs = ResourceMgr;

	// init struct
	ResourceMgr->List = grChain_Create();
	if ( ResourceMgr->List == NULL )
	{
		grResource_MgrDestroy( &ResourceMgr );
		return NULL;
	}

	// set ref count
	ResourceMgr->RefCount = 1;

	ResourceMgr->Engine = pEngine;

	// all done
	return ResourceMgr;

} // grResource_MgrCreate()

GRAPI grBoolean GRCC grResourceMgr_SetEngine(grResourceMgr *ResourceMgr, grEngine* pEngine)
{
	ResourceMgr->Engine = pEngine;
	return GR_TRUE;
}

GRAPI grEngine* GRCC grResourceMgr_GetEngine(const grResourceMgr *ResourceMgr)
{
	return ResourceMgr->Engine;
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_MgrIncRefcount()
//
//	Increment a resource managers ref count.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int32 GRCC grResource_MgrIncRefcount(grResourceMgr *ResourceMgr )	// manager whose ref count will be incremented
{
	// ensure valid data
	assert( ResourceMgr != NULL );

	// increment ref count
	ResourceMgr->RefCount++;

	// all done
	return ResourceMgr->RefCount;

} // grResource_MgrIncRefcount()


// [MLB-ICE]
////////////////////////////////////////////////////////////////////////////////////////
//
//  Close a vfile(directory) of Resource Manager
//     by Icestorm
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean GRCC grResource_CloseDirectory(grResource* Resource)
{
	grVFile* pFS = NULL;

	// clean up open directories
	pFS = (grVFile *)Resource->Data;
	if((pFS == NULL) || (!grVFile_Destroy(&pFS)))
	{
		grErrorLog_AddString(GR_ERR_SYSTEM_RESOURCE, ":grResource_CloseDirectory: grVFile_Destroy", Resource->Name);
		return GR_FALSE;
	}
	return GR_TRUE;
}
// [MLB-ICE] EOB


////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_MgrDestroy()
//
//	Destroy a resource manager.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grResource_MgrDestroy(
	grResourceMgr	**DeadResourceMgr )	// manager to zap
{

	// locals
	grResourceMgr	*ResourceMgr;
	grResource		*CurResource;
	grChain_Link	*CurNode;
	grChain_Link	*NextNode;	// [MLB-ICE] Added by Icestorm

	// ensure valid data
	assert( DeadResourceMgr != NULL );
	assert( *DeadResourceMgr != NULL );

	// get list
	ResourceMgr = *DeadResourceMgr;

	// dont destroy it if ref count is not zero
	ResourceMgr->RefCount--;
	assert( ResourceMgr->RefCount >= 0 );
	if ( ResourceMgr->RefCount > 0 )
	{
		*DeadResourceMgr = NULL;
		return;
	}

	// destroy list
	if ( ResourceMgr->List != NULL )
	{

		// free data for each node
		CurNode = grChain_GetFirstLink( ResourceMgr->List );
		while ( CurNode != NULL )
		{
			// get node data
			CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
			assert( CurResource != NULL );

			// [MLB-ICE]
			NextNode = grChain_LinkGetNext( CurNode );	
			if ( CurResource->OpenDir==GR_TRUE )		// Cleanup open dirs
				grResource_CloseDirectory(CurResource);
			// [MLB-ICE] EOB

			// free name
			if ( CurResource->Name != NULL )
			{
				grRam_Free( CurResource->Name );
			}

			// free resource struct
			grRam_Free( CurResource );

			// get next node
			CurNode = NextNode;  //CurNode = grChain_LinkGetNext( CurNode );  [MLB-ICE]
		}

		// destroy the list itself
		grChain_Destroy( &( ResourceMgr->List ) );

		ResourceMgr->List = NULL;
	}

	if (g_pSingleResource == ResourceMgr)
		g_pSingleResource = NULL;
	{
		grResourceMgr	**Link;

		for (Link = &g_pLiveMgrs; *Link; Link = &(*Link)->NextLive) {
			if (*Link == ResourceMgr) {
				*Link = ResourceMgr->NextLive;
				break;
			}
		}
	}

	// free main struct
	grRam_Free( ResourceMgr );

	// zap pointer
	*DeadResourceMgr = NULL;

} // grResource_MgrDestroy()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_GetNode()
//
//	Get a node by name.
//
////////////////////////////////////////////////////////////////////////////////////////
static grChain_Link * grResource_GetNode(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
    uint32          Type,           // resource type
	char			*Name )			// resource name
{

	// locals
	uint32			Count;
	grChain_Link	*CurNode;

	if (g_pSingleResource == NULL) {
		return NULL;
	}

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// fail if resource list is empty
	Count = grChain_GetLinkCount( ResourceMgr->List );
	if ( Count == 0 )
	{
		return NULL;
	}

	CurNode = grChain_GetFirstLink( ResourceMgr->List );
    while ( CurNode )
	{
		// locals
		grResource	*Resource;
		int			Compare;

		// get data
		assert( CurNode != NULL );
		Resource = (grResource *)grChain_LinkGetLinkData( CurNode );
		assert( Resource != NULL );

		// get compare value
		Compare = stricmp( Name, Resource->Name );

		// if we have found it then return it
        if ( Compare == 0 && (Type==0 || (Type > 0 && Type == Resource->Type)))
		{
			return CurNode;
		}

        CurNode = grChain_LinkGetNext( CurNode );
	}

	// if we got to here then it was not found
	return NULL;

} // grResource_GetNode()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_Add()
//
//	Add a new resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grResource_Add(
	grResourceMgr	*ResourceMgr,	// resource list to add it to
	char			*Name,			// name
    uint32          Type,           // type
	void			*Data )			// data
{
	// locals
	grChain_Link	*CurNode;
	grChain_Link	*NewNode;
	grResource		*CurResource;
	grResource		*NewResource;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );
	assert( Data != NULL );

	// create new resource struct
	NewResource = (grResource *)grRam_Allocate( sizeof( *NewResource ) );
	if ( NewResource == NULL )
	{
		return GR_FALSE;
	}
	NewResource->Name = Util_StrDup( Name );
	NewResource->Data = Data;
	NewResource->RefCount = 1;
    NewResource->Type = Type;
	NewResource->OpenDir = GR_FALSE;	// [MLB-ICE]

	// search for correct node
	CurNode = grChain_GetFirstLink( ResourceMgr->List );
	while ( CurNode != NULL )
	{
		// get resource
		CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
		assert( CurResource != NULL );

		// data should not already exist
        if (stricmp( Name, CurResource->Name ) == 0 && Type == CurResource->Type) {
            CurResource->RefCount++;
            return GR_TRUE; // resource is present, don't add it a second time
        }

		// if we just passed our spot then break out
		if ( stricmp( Name, CurResource->Name ) < 0 )
		{
			break;
		}

		// get next node
		CurNode = grChain_LinkGetNext( CurNode );
	}

	// if we reached end of list or if list is empty them just at the new link
	if ( CurNode == NULL )
	{
		return grChain_AddLinkData( ResourceMgr->List, NewResource );
	}

	// if we got to here then just prepend this link to the current one
	NewNode = grChain_LinkCreate( NewResource );
	if ( NewNode == NULL )
	{
		return GR_FALSE;
	}
	if ( grChain_InsertLinkBefore( ResourceMgr->List, CurNode, NewNode ) == GR_FALSE )
	{
		grChain_LinkDestroy( &NewNode );
		return GR_FALSE;
	}
	return GR_TRUE;

} // grResource_Add()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_Get()
//
//	Get an existing resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void * GRCC grResource_Get(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
	char			*Name )			// resource name
{

	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// fail if node doesn't exist
	CurNode = grResource_GetNode( ResourceMgr, 0, Name );
	if ( CurNode == NULL )
	{
		return NULL;
	}

	// get data
	CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
	assert( CurResource != NULL );
	CurResource->RefCount++;
	return CurResource->Data;

} // grResource_Get()


////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_Delete()
//
//	Delete an existing resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grResource_Delete(
	grResourceMgr	*ResourceMgr,	// resource list to delete it from
	char			*Name )			// resource name
{
	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;

	if (g_pSingleResource == NULL) {
		return -1;
	}

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// fail if node doesn't exist
	CurNode = grResource_GetNode( ResourceMgr, 0, Name );
	if ( CurNode == NULL )
	{
		return -1;
	}

	// decrement ref count
	CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
	assert( CurResource != NULL );
	CurResource->RefCount--;
	assert( CurResource->RefCount >= 0 );

	// if ref count if zero then remove it from the list
	if ( CurResource->RefCount == 0 )
	{
		grChain_RemoveLink( ResourceMgr->List, CurNode );

		// [MLB-ICE]
		// NOTE : THIS WAS NEVER HERE!!!

		// free name
		if ( CurResource->Name != NULL )
		{
			grRam_Free( CurResource->Name );
		}

		// free resource struct
		grRam_Free( CurResource );
		// [MLB-ICE] EOB

		return 0;
	}

	// return current ref count
	return CurResource->RefCount;

} // grResource_Delete()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_CreateVFileName()
//
//	Create a VFile resource name.
//
////////////////////////////////////////////////////////////////////////////////////////
static char * grResource_CreateVFileName(
	char	*Name )	// name to base it on
{

	// locals
	char	*VFilePrefix = "__VFILE__";
	char	*NewName;

	// ensure valid data
	assert( Name != NULL );

	// build new name
	NewName = (char *)grRam_Allocate( strlen( VFilePrefix ) + strlen( Name ) + 1 );
	if ( NewName == NULL )
	{
		return NULL;
	}
	strcpy( NewName, VFilePrefix );
	strcat( NewName, Name );

	// all done
	return NewName;

} // grResource_CreateVFileName()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_AddVFile()
//
//	Add a new VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grResource_AddVFile(
	grResourceMgr	*ResourceMgr,	// resource list to add it to
	char			*Name,			// name
	grVFile			*Data )			// data
{

	// locals
	char		*NewName;
	grBoolean	Result;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );
	assert( Data != NULL );

	// build new name
	NewName = grResource_CreateVFileName( Name );
	if ( NewName == NULL )
	{
		return GR_FALSE;
	}

	// add it to the list
    Result = grResource_Add( ResourceMgr, NewName, GR_RESOURCE_VFS, Data );
	grRam_Free( NewName );

	// all done
	return Result;

} // grResource_AddVFile()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_GetVFile()
//
//	Get an existing VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grVFile * GRCC grResource_GetVFile(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
	char			*Name )			// name
{

	// locals
	char		*NewName;
	grVFile		*Data;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// build new name
	NewName = grResource_CreateVFileName( Name );
	if ( NewName == NULL )
	{
		return NULL;
	}

	// get data
	Data = (grVFile *)grResource_Get( ResourceMgr, NewName );
	grRam_Free( NewName );

	// all done
	return Data;

} // grResource_GetVFile()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_DeleteVFile()
//
//	Delete an existing VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grResource_DeleteVFile(
	grResourceMgr	*ResourceMgr,	// resource list to delete it from
	char			*Name )			// name
{

	// locals
	char		*NewName;
	int			RefCount;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// build new name
	NewName = grResource_CreateVFileName( Name );
	if ( NewName == NULL )
	{
		return -1;
	}

	// delete data
	RefCount = grResource_Delete( ResourceMgr, NewName );
	grRam_Free( NewName );

	// all done
	return RefCount;

} // grResource_DeleteVFile()


// [MLB-ICE]

////////////////////////////////////////////////////////////////////////////////////////
//
//	Set OpenDir flag of an existing resource 
//    by Icestorm
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean grResource_SetOpenDir(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
	char			*Name )			// resource name
{

	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;
	char			*NewName;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// build new name
	NewName = grResource_CreateVFileName( Name );
	if ( NewName == NULL )
	{
		return GR_FALSE;
	}

	// fail if node doesn't exist
    CurNode = grResource_GetNode( ResourceMgr, GR_RESOURCE_VFS, NewName );
	grRam_Free( NewName );

	if ( CurNode == NULL )
	{
		return GR_FALSE;
	}

	// get data
	CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
	assert( CurResource != NULL );
	CurResource->OpenDir=GR_TRUE;
	return GR_TRUE;

}

////////////////////////////////////////////////////////////////////////////////////////
//
//  Open a vfile(directory) for Resource Manager (WITH AutoRemove on ResMgrDestroy)
//  DirName = Path of directory
//  ResName = Alias of this resource
//     by Icestorm
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grResource_OpenDirectory(grResourceMgr* pResourceMgr, char* DirName, char* ResName)
{
	grVFile* pFS = NULL ;
	assert( pResourceMgr != NULL );

	pFS = grVFile_OpenNewSystem
	(
		NULL, 
		GR_VFILE_TYPE_DOS,
		DirName,
		NULL,
		GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY
	);
	if( pFS == NULL )
	{
		grErrorLog_AddString(GR_ERR_SYSTEM_RESOURCE, "grResource_OpenDirectory:grVFile_OpenNewSystem", DirName);
		return GR_FALSE;
	}
	else
	{
		grResource_AddVFile( pResourceMgr, ResName, pFS );
		grResource_SetOpenDir( pResourceMgr, ResName);
	}
	return GR_TRUE;
}

//	Create a Resource Manager using the default paths.
//     by Incarnadine, modified by Icestorm [Added autoremove]
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grResourceMgr* GRCC grResource_MgrCreateDefault(grEngine* pEngine)
{
	grResourceMgr *	pResourceMgr;
//	grVFile			*	pFS = NULL ;

	pResourceMgr = grResource_MgrCreate(pEngine);
	if( pResourceMgr == NULL )
		return NULL;

	// open sound vfile
	grResource_OpenDirectory(pResourceMgr, "Sounds", "Sounds");

	// open bitmap vfile	
	grResource_OpenDirectory(pResourceMgr, "GlobalMaterials", "GlobalMaterials");
		
	// open actors vfile	
	grResource_OpenDirectory(pResourceMgr, "Actors", "Actors");

	// open shaders vfile
	grResource_OpenDirectory(pResourceMgr, "Shaders", "Shaders");
	
/*	pFS = grVFile_OpenNewSystem
	(
		NULL, 
		GR_VFILE_TYPE_DOS,
		"Sounds",
		NULL,
		GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY
	);
	if( pFS == NULL )
	{
		grErrorLog_AddString(GR_ERR_SYSTEM_RESOURCE, "Level_CreateResourceMgr:grVFile_OpenNewSystem", "");
	}
	else
	{
		grResource_AddVFile( pResourceMgr, "Sounds", pFS );
	}


	// open bitmap vfile	
	pFS = grVFile_OpenNewSystem(	NULL,
									GR_VFILE_TYPE_DOS,
									"GlobalMaterials",
									NULL,
									GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY );
	if ( pFS == NULL )
	{
		grErrorLog_AddString( GR_ERR_SYSTEM_RESOURCE, "Level_CreateResourceMgr:grVFile_OpenNewSystem", "" );
	}
	else
	{
		grResource_AddVFile( pResourceMgr, "GlobalMaterials", pFS );
	}

	// open actors vfile	
	pFS = grVFile_OpenNewSystem(	NULL,
									GR_VFILE_TYPE_DOS,
									"Actors",
									NULL,
									GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY );
	if ( pFS == NULL )
	{
		grErrorLog_AddString( GR_ERR_SYSTEM_RESOURCE, "Level_CreateResourceMgr:grVFile_OpenNewSystem", "");
	}
	else
	{
		grResource_AddVFile( pResourceMgr, "Actors", pFS );
	}*/

	return pResourceMgr;
}
// [MLB-ICE] EOB

// Krouer ;: add usefull function to read resource from disk and create them from here
// First declare all functions need to create data
#include "Bitmap.h"
#include "grMaterial.h"

//=====================================================================================
//	OpenMaterialOverride
//	G3D_MATERIAL_OVERRIDES=<directory>: a .jmat there replaces the one of the same name,
//	e.g. to try PBR versions of a level's materials without changing the level or the
//	shipped materials. Its layers still load from GlobalMaterials (or a pak under it).
//=====================================================================================
static const char *MaterialOverrideDir(void)
{
	static grBoolean	Checked = GR_FALSE;
	static char			Dir[_MAX_PATH];
	const char			*Env;

	if (!Checked)
	{
		Env = getenv("G3D_MATERIAL_OVERRIDES");
		if (Env)
		{
			strncpy(Dir, Env, sizeof(Dir) - 1);
			Dir[sizeof(Dir) - 1] = 0;
		}
		Checked = GR_TRUE;
	}
	return Dir;
}

static grBoolean MaterialOverridePath(const char *FileName, char *Path, int32 PathSize)
{
	const char			*Dir = MaterialOverrideDir();

	if (!Dir[0])
		return GR_FALSE;
	_snprintf(Path, PathSize - 1, "%s\\%s", Dir, FileName);
	Path[PathSize - 1] = 0;
	return (GetFileAttributesA(Path) != INVALID_FILE_ATTRIBUTES) ? GR_TRUE : GR_FALSE;
}

static grVFile *OpenMaterialOverride(const char *FileName)
{
	char				Path[_MAX_PATH * 2];

	if (!MaterialOverridePath(FileName, Path, sizeof(Path)))
		return NULL;
	return grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS, Path, NULL, GR_VFILE_OPEN_READONLY);
}

GRAPI grBoolean GRCC grResource_HasMaterialOverride(const char *Name)
{
	char				FileName[_MAX_PATH];
	char				Path[_MAX_PATH * 2];
	const char			*Colon;

	if (!Name || !MaterialOverrideDir()[0])
		return GR_FALSE;
	Colon = strrchr(Name, ':');
	_snprintf(FileName, sizeof(FileName) - 1, "%s.jmat", Colon ? Colon + 1 : Name);
	FileName[sizeof(FileName) - 1] = 0;
	return MaterialOverridePath(FileName, Path, sizeof(Path));
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	grResource_GetResource()
//
//	Get an existing resource of a directory or VFS
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void * GRCC grResource_GetResource(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
	int32			Type,			// resource kind
	char			*Name )			// resource name
{
	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// fail if node doesn't exist
	CurNode = grResource_GetNode( ResourceMgr, Type, Name );
	if ( CurNode == NULL ) // if failed, new behavior
	{
		void* Data;
		char* Paks;
		char* ResName;
		char* CopyName;
		grResource* DirRes;
		grVFile* Directory;
		grVFile* ResFile;

		CopyName = Util_StrDup(Name);

		// the resource was not already open
		Paks = strtok(CopyName, ":.");
		ResName = strtok(NULL, ":.");

		// exchange the paks and the resource if no resource found
		if (ResName==NULL) {
			ResName=Paks;
			Paks= NULL;
		}
		if (Paks && strcmp(Paks, "GlobalMaterials") == 0) {
			Paks = NULL;
		}

		if (Paks) {
			// open the first part of the name separate by :
            CurNode = grResource_GetNode( ResourceMgr, GR_RESOURCE_VFS, grResource_CreateVFileName(Paks) );

			// fail to find the dir/pak resource
			if (CurNode == NULL) {
				// the resource was not already open
				// open the first part of the name separate by :
				CurNode = grResource_GetNode( ResourceMgr, GR_RESOURCE_VFS, grResource_CreateVFileName("GlobalMaterials") );
				DirRes = (grResource *)grChain_LinkGetLinkData( CurNode ); 

				// now, we have to add a grVFile to the ResourceMgr - we add a directory because normally it's already done by the editor or the game
				// (a subdirectory of GlobalMaterials: grVFile_OpenNewSystem would ignore the
				// parent and look in the current directory)
				Directory = grVFile_Open((grVFile*) DirRes->Data, Paks,
										 GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);
				if (Directory == NULL) {
					grErrorLog_AddString(GR_ERR_SYSTEM_RESOURCE, "grResource_GetResource: no such pak directory", Paks);
					grRam_Free(CopyName);
					return NULL;
				}

				// add it to the resource for the next time
				grResource_AddVFile(ResourceMgr, Paks, Directory);
				grResource_SetOpenDir( ResourceMgr, Paks);
			} else {
				DirRes = (grResource *)grChain_LinkGetLinkData( CurNode );
				Directory = (grVFile*) DirRes->Data;
			}
		} else {
			CurNode = grResource_GetNode( ResourceMgr, GR_RESOURCE_VFS, grResource_CreateVFileName("GlobalMaterials") );
			// open the resource container
			DirRes = (grResource *)grChain_LinkGetLinkData( CurNode ); 
			Directory = (grVFile*) DirRes->Data;
		}

		// get the name of the sub resource
		if (ResName==NULL) {
			return NULL;
		}

		// Try to locate already 
		CurNode = grResource_GetNode( ResourceMgr, Type, ResName );
		if (CurNode==NULL) {
			char* ResNameCopy;
			ResNameCopy = (char*) grRam_Allocate(strlen(ResName)+10);
			strcpy(ResNameCopy, ResName);
            Data = NULL;
			switch (Type) {
			case GR_RESOURCE_BITMAP:
				strcat(ResNameCopy, ".bmp");
				ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
				if (ResFile) {
					Data = grBitmap_CreateFromFile(ResFile);
				} else {
#ifdef _DEBUG
					Log_Printf("Replace %s by jet3d.bmp\n", ResNameCopy);
#endif
					ResFile = grVFile_Open(Directory, "Jet3D.bmp", GR_VFILE_OPEN_READONLY);
					Data = grBitmap_CreateFromFile(ResFile);
				}
				break;
			case GR_RESOURCE_MATERIAL:
				strcat(ResNameCopy, ".jmat");
				ResFile = OpenMaterialOverride(ResNameCopy);
				if (!ResFile)
					ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
				if (ResFile) {
					Data = grMaterialSpec_CreateFromFile(ResFile, ResourceMgr->Engine, ResourceMgr);
                } else {
#ifdef _DEBUG
					Log_Printf("Replace %s by jet3d.jmat\n", ResNameCopy);
#endif
/*
                    ResFile = grVFile_Open(Directory, "Jet3D.jmat", GR_VFILE_OPEN_READONLY);
                    Data = grMaterialSpec_CreateFromFile(ResFile, ResourceMgr->Engine, ResourceMgr);
*/
                    Data = grResource_GetResource(ResourceMgr, GR_RESOURCE_MATERIAL, "jet3d");
                }
				break;
			case GR_RESOURCE_TEXTURE:
				{
				char* extension = ResNameCopy + strlen(ResName);
				// DDS first: G3DTexImport's compressed, mipmapped output
				strcpy(extension, ".dds");
				ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
				if (ResFile == NULL) {
					strcpy(extension, ".png");
					ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
				}
				if (ResFile == NULL) {
					strcpy(extension, ".bmp");
					ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
				}
				if (ResFile != NULL) {
					Data = grEngine_CreateTextureFromFile(ResourceMgr->Engine, ResFile);
                } else {
#ifdef _DEBUG
					Log_Printf("Replace %s by jet3d.bmp\n", ResNameCopy);
#endif
					ResFile = grVFile_Open(Directory, "Jet3D.bmp", GR_VFILE_OPEN_READONLY);
                    if (ResFile) {
					    Data = grEngine_CreateTextureFromFile(ResourceMgr->Engine, ResFile);
                    }
                }
				}
				break;
			default:
				ResFile = grVFile_Open(Directory, "Jet3D.bmp", GR_VFILE_OPEN_READONLY);
                if (ResFile) {
				    Data = grBitmap_CreateFromFile(ResFile);
                }
				break;
			}
			if (ResFile) {
				grVFile_Close(ResFile);
            }
            if (Data) {
				grResource_Add(ResourceMgr, Name, Type, Data);
			}
			grRam_Free(ResNameCopy);
		} else {
			DirRes = (grResource *)grChain_LinkGetLinkData( CurNode );
			DirRes->RefCount++;
			Data = DirRes->Data;
		}

		grRam_Free(CopyName);
		return Data;
	}

	// fallback in old behavior of grResource_Get

	// get data
	CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
	assert( CurResource != NULL );
	CurResource->RefCount++;
	switch (Type) {
	case GR_RESOURCE_BITMAP:
        {
            grBitmap* pBitmap = (grBitmap*) CurResource->Data;
            grBitmap_CreateRef(pBitmap);
        }
        break;
	case GR_RESOURCE_MATERIAL:
        {
            grMaterialSpec* pMatSpec = (grMaterialSpec*) CurResource->Data;
            grMaterialSpec_CreateRef(pMatSpec);
        }
        break;
	case GR_RESOURCE_TEXTURE:
        break;
    }
	return CurResource->Data;

} // grResource_GetResource()

// export to jet 3D bitmap format
GRAPI void GRCC grResource_ExportResource(grResourceMgr *ResourceMgr, int32 Type, char *Name, void* Data)
{
	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;
	
	grVFile* Directory;

	CurNode = grResource_GetNode( ResourceMgr, GR_RESOURCE_VFS, grResource_CreateVFileName("GlobalMaterials") );
	CurResource = (grResource *)grChain_LinkGetLinkData( CurNode ); 
	if (CurResource->OpenDir) {
		char* ResNameCopy;
		grVFile* ResFile;
		ResNameCopy = (char*) grRam_Allocate(strlen(Name)+10);
		strcpy(ResNameCopy, Name);
		Directory = (grVFile*) CurResource->Data;
		switch (Type) {
		case GR_RESOURCE_BITMAP:
			strcat(ResNameCopy, ".bmp");
			ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_READONLY);
			if (!ResFile) {
				ResFile = grVFile_Open(Directory, ResNameCopy, GR_VFILE_OPEN_CREATE);
				grBitmap_WriteToFile((grBitmap*)Data, ResFile);
				grVFile_Close(ResFile);
			}
			break;
		}
		grRam_Free(ResNameCopy);
	}
}

GRAPI void GRCC grResource_RemapTextures(grTexture* const* Old, grTexture* const* New, int32 Count)
{
	grResourceMgr	*ResourceMgr;
	grChain_Link	*Link;
	int32			i;

	for (ResourceMgr = g_pLiveMgrs; ResourceMgr; ResourceMgr = ResourceMgr->NextLive) {
		if (!ResourceMgr->List)
			continue;
		for (Link = grChain_GetFirstLink(ResourceMgr->List); Link; Link = grChain_LinkGetNext(Link)) {
			grResource	*Resource = (grResource *)grChain_LinkGetLinkData(Link);

			if (Resource->Type != GR_RESOURCE_TEXTURE || !Resource->Data)
				continue;
			for (i = 0; i < Count; i++) {
				if (Resource->Data == Old[i]) {
					Resource->Data = New[i];
					break;
				}
			}
		}
	}
}

// Krouer 08/16/2005
// release identified resource
GRAPI grBoolean GRCC grResource_ReleaseResource(grResourceMgr *ResourceMgr, int32 Type, char *Name)
{
	// locals
	grChain_Link	*CurNode;
	grResource		*CurResource;

	// a material outliving its manager (e.g. the editor's lists at exit)
	if (ResourceMgr == NULL) {
		return GR_FALSE;
	}

	// ensure valid data
	assert( ResourceMgr->List != NULL );
	assert( Name != NULL );

	// fail if node doesn't exist
	CurNode = grResource_GetNode( ResourceMgr, Type, Name );

	if (CurNode) {
		// Get access to the ressource
		CurResource = (grResource *)grChain_LinkGetLinkData( CurNode );
		CurResource->RefCount--;

		if (CurResource->RefCount == 0) {
			// Free the texture when fully managed from here
			if (Type == GR_RESOURCE_TEXTURE && CurResource->Data) {
				grEngine_DestroyTexture(ResourceMgr->Engine, (grTexture*) CurResource->Data);
				CurResource->Data = NULL;
			}

			// Remove the link
			grChain_RemoveLink( ResourceMgr->List, CurNode );

			// free name
			if ( CurResource->Name != NULL )
			{
				grRam_Free( CurResource->Name );
			}

			// free resource struct
			grRam_Free( CurResource );
		}

		return GR_TRUE;
	}

	return GR_FALSE;
}

