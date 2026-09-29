/****************************************************************************************/
/*  MATRIDX.C                                                                           */
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
#include "grMaterial.h"
#include <string.h>
#include "assert.h"
#include "errorlog.h"
#include "ram.h"
#include "util.h"

/* This structure contains the binding of the grBitmaps to the editable bmps */
typedef struct MatrIdx_Struct {
	char					*	Name;
	grMaterial_ArrayIndex		MaterialIndex;
	int32						RefCnt;
} MatrIdx_Struct;


// Creates a new MatrIdx structure, addes pBitmap to array and initializes structure
MatrIdx_Struct *MatrIdx_Create( grMaterial_Array * pMatlArray, grBitmap * pBitmap, const char * Name )
{
	MatrIdx_Struct *pMatrIdx;

	pMatrIdx = GR_RAM_ALLOCATE_STRUCT( MatrIdx_Struct );
	if( pMatrIdx == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Failed allocate MatrIdx" );
		return( NULL );
	}
	pMatrIdx->Name = Util_StrDup( Name );
	if( pMatrIdx->Name == NULL )
	{
		grRam_Free( pMatrIdx );
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Failed allocate MatrIdx->Name");
		return( NULL );
	}


	pMatrIdx->MaterialIndex =  grMaterial_ArrayCreateMaterial(pMatlArray, Name );
	if( pMatrIdx->MaterialIndex  == GR_MATERIAL_ARRAY_NULL_INDEX )
	{
		grRam_Free( pMatrIdx->Name );
		grRam_Free( pMatrIdx );
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Failed to create Material to array.");
		return( NULL );
	}

	if( !grMaterial_ArraySetMaterialBitmap( pMatlArray, pMatrIdx->MaterialIndex, pBitmap, Name ) )
	{
		grRam_Free( pMatrIdx->Name );
		grRam_Free( pMatrIdx );
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Failed to add Material to array.");
		return( NULL );
	}
	pMatrIdx->RefCnt = 1;
	return( pMatrIdx );
}

const char* MatrIdx_GetName( MatrIdx_Struct* MatrIdx )
{
	assert( MatrIdx );
	assert( MatrIdx->Name );
	return( MatrIdx->Name );
}

const grBitmap	*	MatrIdx_GetBitmap( grMaterial_Array * pMatlArray, MatrIdx_Struct* MatrIdx )
{
	const grMaterial * Material;

	assert( MatrIdx );

	Material = grMaterial_ArrayGetMaterialByIndex( pMatlArray, MatrIdx->MaterialIndex );
	if( Material ==  NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Failed to add Material to array.");
		return( NULL );
	}
	return( grMaterial_GetBitmap( Material ) );

}

void MatrIdx_AddRef( MatrIdx_Struct* MatrIdx )
{
	assert( MatrIdx );

	MatrIdx->RefCnt++;
}

grMaterial_ArrayIndex	MatrIdx_GetIndex( MatrIdx_Struct* pMatrIdx )
{
	return( pMatrIdx->MaterialIndex );
}

//returns GR_TRUE if object was truly destroyed
//returns GR_FALSE if only RefCnt was decremented
grBoolean MatrIdx_Destroy( MatrIdx_Struct** hMatrIdx )
{
	assert( hMatrIdx );

	
	assert( (*hMatrIdx )->RefCnt > 0 );

	(*hMatrIdx )->RefCnt--;
	if( (*hMatrIdx )->RefCnt > 0 )
		return( GR_FALSE );

	if( (*hMatrIdx )->Name != NULL )
		grRam_Free( (*hMatrIdx )->Name );


	grRam_Free( (*hMatrIdx ) );
	return( GR_TRUE );
}
	

/* EOF: MatrIdx.c */
