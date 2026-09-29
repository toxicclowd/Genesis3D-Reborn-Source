/****************************************************************************************/
/*  MATERIALS.C                                                                         */
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
#include "vfile.h"
#include "bitmap.h"
#include <string.h>
#include "assert.h"
#include "errorlog.h"
#include "ram.h"
#include "util.h"
#include "grWorld.h"
#include <stdio.h>
//#include "grShader.h"
#include "bmp.h"

/* This structure contains the binding of the grBitmaps to the editable bmps */
typedef struct Material_Struct {
	char* Name;
	char* PrimaryMaterialPath;
	union {
		grBitmap * PrimaryMaterial;
		grMaterialSpec* MaterialSpec;
	};
} Material_Struct;



//Creates a Material_Stuct
//Loads the bitmap specifed in the properties
//Intializes the Material struct
//Returns NULL on failure
Material_Struct *Materials_Load( grEngine* pEngine, grResourceMgr* pResMgr, char* DirPath, char* Name )
{
	Material_Struct *Material;
	grVFile *MaterialFile;
	char* extStart;

	assert( DirPath != NULL );
	assert( Name != NULL );


	Material = GR_RAM_ALLOCATE_STRUCT( Material_Struct );
	if( Material == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	//Temporarily remove ".bmp" from file name to copy to Material Name
	extStart = strchr( Name, '.' );
	if( extStart == NULL )
	{
		grErrorLog_Add( GR_ERR_DATA_FORMAT, NULL );
		return( NULL );
	}
	*extStart = '\0';

	Material->Name = Util_StrDup( Name );
	if( Material->Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	*extStart = '.';

	// Build full path to material
	//allocate enough for path, back slash, file name, terminating char
	Material->PrimaryMaterialPath = grRam_Allocate( strlen( DirPath ) + strlen( Name ) + 2 );
	if( Material->PrimaryMaterialPath == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	strcpy( Material->PrimaryMaterialPath, DirPath );
	strcat( Material->PrimaryMaterialPath, "\\" );
	strcat( Material->PrimaryMaterialPath, Name );

	//Load grBitmap
	MaterialFile = grVFile_OpenNewSystem(
		NULL, 
		GR_VFILE_TYPE_DOS, 
		Material->PrimaryMaterialPath, 
		NULL,
		GR_VFILE_OPEN_READONLY  );

	if( MaterialFile == NULL )
	{
		grErrorLog_Add( GR_ERR_FILEIO_OPEN, Material->PrimaryMaterialPath );
		return( NULL );
	}

	Material->PrimaryMaterial = grBitmap_CreateFromFile( MaterialFile );
	if( Material->PrimaryMaterial == NULL )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Failed to create bitmap", Material->PrimaryMaterialPath );
		grVFile_Close( MaterialFile );
		return( NULL );
	}
	if( !grBitmap_SetMipCount(Material->PrimaryMaterial, 4 ) )
	{
		grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE, "Failed to create mips", Material->PrimaryMaterialPath );
		grBitmap_Destroy( &Material->PrimaryMaterial );
		grVFile_Close( MaterialFile );
		return( NULL );
	}

	grVFile_Close( MaterialFile );

	return( Material );
}

Material_Struct *Materials_ConvertToJMAT( grEngine* pEngine, grResourceMgr* pResMgr, char* DirPath, char* Name )
{
	Material_Struct *Material;
	grVFile *MaterialFile;
	char* extStart;
	grBitmap* pBmps;
	grMaterialSpec_Thumbnail tumbs;

	assert( DirPath != NULL );
	assert( Name != NULL );

	Material = GR_RAM_ALLOCATE_STRUCT( Material_Struct );
	if( Material == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	//Temporarily remove ".bmp" from file name to copy to Material Name
	extStart = strchr( Name, '.' );
	if( extStart == NULL )
	{
		grErrorLog_Add( GR_ERR_DATA_FORMAT, NULL );
		return( NULL );
	}
	*extStart = '\0';

	Material->Name = Util_StrDup( Name );
	if( Material->Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	*extStart = '.';

	// Build full path to material
	//allocate enough for path, back slash, file name, terminating char
	Material->PrimaryMaterialPath = grRam_Allocate( strlen( DirPath ) + strlen( Name ) + 4 );
	if( Material->PrimaryMaterialPath == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	strcpy( Material->PrimaryMaterialPath, DirPath );
	strcat( Material->PrimaryMaterialPath, "\\" );
	strcat( Material->PrimaryMaterialPath, Name );

	//Load grBitmap
	MaterialFile = grVFile_OpenNewSystem(
		NULL, 
		GR_VFILE_TYPE_DOS, 
		Material->PrimaryMaterialPath, 
		NULL,
		GR_VFILE_OPEN_READONLY  );

	extStart = strchr( Material->PrimaryMaterialPath, '.' );
	strcpy(extStart, ".jmat");

	if( MaterialFile == NULL )
	{
		grErrorLog_Add( GR_ERR_FILEIO_OPEN, Material->PrimaryMaterialPath );
		return( NULL );
	}

	pBmps = grBitmap_CreateFromFile( MaterialFile );
	if( pBmps == NULL )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Failed to create bitmap", Material->PrimaryMaterialPath );
		grVFile_Close( MaterialFile );
		return( NULL );
	}
	if( !grBitmap_SetMipCount(pBmps, 4 ) )
	{
		grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE, "Failed to create mips", Material->PrimaryMaterialPath );
		grBitmap_Destroy( &pBmps );
		grVFile_Close( MaterialFile );
		return( NULL );
	}

	// Create an empty material spec
	Material->MaterialSpec = grMaterialSpec_Create(pEngine, pResMgr);
	grMaterialSpec_AddLayerFromBitmap(Material->MaterialSpec, 0, pBmps, Material->Name);

	//now create the thumbnail from the bmps
	if (CreateThumbnails(pBmps, &tumbs))
	    grMaterialSpec_SetThumbnail(Material->MaterialSpec, &tumbs);

	grBitmap_Destroy( &pBmps );
	grRam_Free(tumbs.contents);

	grVFile_Close( MaterialFile );

	//Create JMAT
	MaterialFile = grVFile_OpenNewSystem(
		NULL, 
		GR_VFILE_TYPE_DOS, 
		Material->PrimaryMaterialPath, 
		NULL,
		GR_VFILE_OPEN_CREATE  );

	grMaterialSpec_WriteToFile(Material->MaterialSpec, MaterialFile);

	grVFile_Close( MaterialFile );

	return( Material );
}

// Same as above but for loading grMaterialSpec
Material_Struct *Materials_LoadEx( grEngine* pEngine, grResourceMgr* pResMgr, char* DirPath, char* Name )
{
	Material_Struct *Material;
	grVFile *MaterialFile;
	char* extStart;

	assert( DirPath != NULL );
	assert( Name != NULL );


	Material = GR_RAM_ALLOCATE_STRUCT( Material_Struct );
	if( Material == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	//Temporarily remove ".bmp" from file name to copy to Material Name
	extStart = strchr( Name, '.' );
	if( extStart == NULL )
	{
		grErrorLog_Add( GR_ERR_DATA_FORMAT, NULL );
		return( NULL );
	}
	*extStart = '\0';

	Material->Name = Util_StrDup( Name );
	if( Material->Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	*extStart = '.';

	// Build full path to material
	//allocate enough for path, back slash, file name, terminating char
	Material->PrimaryMaterialPath = grRam_Allocate( strlen( DirPath ) + strlen( Name ) + 2 );
	if( Material->PrimaryMaterialPath == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return( NULL );
	}

	strcpy( Material->PrimaryMaterialPath, DirPath );
	strcat( Material->PrimaryMaterialPath, "\\" );
	strcat( Material->PrimaryMaterialPath, Name );

	//Load grBitmap
	MaterialFile = grVFile_OpenNewSystem(
		NULL, 
		GR_VFILE_TYPE_DOS, 
		Material->PrimaryMaterialPath, 
		NULL,
		GR_VFILE_OPEN_READONLY  );

	if( MaterialFile == NULL )
	{
		grErrorLog_Add( GR_ERR_FILEIO_OPEN, Material->PrimaryMaterialPath );
		return( NULL );
	}

	Material->MaterialSpec = grMaterialSpec_CreateFromFile( MaterialFile, pEngine, pResMgr );
	if( Material->MaterialSpec == NULL )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Failed to create bitmap", Material->PrimaryMaterialPath );
		grVFile_Close( MaterialFile );
		return( NULL );
	}

	grVFile_Close( MaterialFile );

	return( Material );
}

const char* Materials_GetName( Material_Struct* Material )
{
	assert( Material );
	assert( Material->Name );
	return( Material->Name );
}

const grBitmap	*	Materials_GetBitmap( Material_Struct* Material )
{
	assert( Material );
	assert( Material->PrimaryMaterial);
	return( Material->PrimaryMaterial );
}

// Krouer: move slightly from BMP to JMAT
const grMaterialSpec*	Materials_GetMaterialSpec( Material_Struct* Material )
{
	assert( Material );
	assert( Material->MaterialSpec );
	return( Material->MaterialSpec );
}

void Materials_Destroy( Material_Struct* Material )
{
	assert( Material );

	if( Material->Name != NULL )
		grRam_Free( Material->Name );

	if( Material->PrimaryMaterialPath != NULL )
		grRam_Free( Material->PrimaryMaterialPath );
#ifdef _USE_BITMAPS
	if( Material->PrimaryMaterial != NULL )
		grBitmap_Destroy( &Material->PrimaryMaterial );
#else
	if (Material->MaterialSpec != NULL) {
		grMaterialSpec_Destroy(&Material->MaterialSpec);
	}
#endif
	grRam_Free( Material );
}
	

/* EOF: Materials.c */
