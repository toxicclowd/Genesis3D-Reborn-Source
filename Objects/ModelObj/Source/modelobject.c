/****************************************************************************************/
/*  MODELOBJECT.C                                                                       */
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
#ifdef WIN32
#include "windows.h"
#endif

#ifdef BUILD_BE
#include <OS.h>
#endif

#include <memory.h>
#include <assert.h>
#include <string.h>

#include "ModelObject.h"

#include "grTypes.h"
#include "grProperty.h"
#include "grModel.h"
#include "grBrush.h"
#include "Genesis3D.h"
#include "Ram.h"
#include "Bitmap.h"
#include "VFile.h"
#include "ModelInstance.h"
#include "Resource.h"
#include "Errorlog.h"

char *NameList[3];
#ifdef WIN32
HINSTANCE ghInstance;
#endif

#ifdef BUILD_BE
#include <Resources.h>
image_id ghInstance;
#endif

#define MODELOBJECT_VERSION 1


#define UTIL_MAX_RESOURCE_LENGTH	(128)
static char stringbuffer[UTIL_MAX_RESOURCE_LENGTH + 1];

#define DEFAULT_SIZE 16.0f

enum {
	MODEL_STATS_ID = PROPERTY_LOCAL_DATATYPE_START,
	MODEL_AREAS_ID,
	MODEL_VIS_PORTALS_ID,
	MODEL_PORTALS_ID,
	MODEL_SUB_FACES_ID,
	MODEL_DRAW_FACES_ID,
	MODEL_SPLITS_ID,
	MODEL_LEAFS_ID,
	MODEL_NODES_ID,
	MODEL_BRUSH_FACES_ID,
	MODEL_BRUSHES_ID,
	MODEL_VISABLE_FACES_ID,
	MODEL_STATS_END_ID
};



#ifdef WIN32

////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_LoadLibraryString()
//
////////////////////////////////////////////////////////////////////////////////////////
static char * Util_LoadLibraryString(
	HINSTANCE		hInstance,
	unsigned int	ID )
{

	// locals
	#define		MAX_STRING_SIZE	255
	static char	StringBuf[MAX_STRING_SIZE];
	char		*NewString;
	int			Size;

	// ensure valid data
	assert( hInstance != NULL );
	assert( ID >= 0 );

	// get resource string
	Size = LoadString( hInstance, ID, StringBuf, MAX_STRING_SIZE );
	if ( Size <= 0 )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		return NULL;
	}

	// copy resource string
	NewString = grRam_Allocate( Size + 1 );
	if ( NewString == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return NULL;
	}
	strcpy( NewString, StringBuf );

	// all done
	return NewString;

} // Util_LoadLibraryString()

#endif

#ifdef BUILD_BE

static char *Util_LoadLibraryString(image_id libhinst, int32 resid)
{
	BResources resourcefile;
	int result;
	char *rcbuffer;
 	image_info info;
	size_t outSize;
	
	// locals
	#define		MAX_STRING_SIZE	255
	static char	stringbuffer[MAX_STRING_SIZE];


	assert(libhinst > 0);
	assert(resid);

///	hResources = (image_id)hStringResources ;
	
	if(get_image_info(libhinst,&info) != B_OK)
		return NULL;
		
	BFile* resFile = new BFile(info.name , B_READ_ONLY);
	
	resourcefile.SetTo(resFile,false);

	char* loadedString = (char *)resourcefile.FindResource((int)'DATA', 		/*** DEPRECATED ***/
								  resid, 
								  &outSize);
	
	//
	//	Note that if we did't allocate space and copy the string, then we
	//	would be limited to having one string loaded at a time. Or we would
	//	setup some kind of revolving buffer.  Either of these options is
	//	risky and could eventually cause a problem elsewhere... 	 LF
	//
 
	// Allocate memory for the string
	rcbuffer = (char*)grRam_Allocate(strlen(loadedString) + 1);
	strcpy(rcbuffer, loadedString);
 
#ifndef NDEBUG
	memset(stringbuffer, 0xFF, MAX_STRING_SIZE + 1);
#endif
 
	// return the allocated string
	return (rcbuffer);
}//Util_LoadLibraryString

#endif

#ifdef WIN32
int Util_GetAppPath(
	char	*Buf,		// where to store path name
	int		BufSize )	// size of buf
{

	// locals
	int	Count;

	// get exe full path name
	Count = GetModuleFileName( NULL, Buf, BufSize );
	if ( Count == 0 )
	{
		return 0;
	}

	// eliminate the exe from the path name
	while ( Count >= 0 )
	{
		if ( Buf[Count] == '\\' )
		{
			break;
		}
		Buf[Count] = '\0';
		Count--;
	}

	// all done
	return Count;

} // Util_GetAppPath()
#endif

static grBoolean BrushExtBox( grXForm3d * pModelXF, grBrush * pBrush, grExtBox * pExtBox )
{
	grBrush_Face *	pFace ;
	int				nVerts ;
	int				i ;
	grXForm3d		XForm ;
	grVec3d			Vert;
	const grVec3d	*pVert;

	assert( pBrush != NULL ) ;
	assert( pExtBox != NULL ) ;

	grXForm3d_Copy( grBrush_GetXForm( pBrush ), &XForm ) ;
	pFace = grBrush_GetNextFace( pBrush, NULL ) ;
	//Set pExtBox Invalid
	pExtBox->Max.X  = -1.0f;
	pExtBox->Max.Y  = -1.0f;
	pExtBox->Max.Z  = -1.0f;
	pExtBox->Min.X  = 1.0f;
	pExtBox->Min.Y  = 1.0f;
	pExtBox->Min.Z  = 1.0f;

	while( pFace != NULL )
	{
		nVerts = grBrush_FaceGetVertCount( pFace );
		for( i=0; i<nVerts; i++ )
		{
			pVert = grBrush_FaceGetVertByIndex( pFace, i) ;
			grXForm3d_Transform( &XForm, pVert, &Vert ) ;
			grXForm3d_Transform( pModelXF, &Vert, &Vert ) ;
			if( grExtBox_IsValid( pExtBox ) )
				grExtBox_ExtendToEnclose( pExtBox, &Vert );
			else
				grExtBox_SetToPoint ( pExtBox, &Vert );
		}
		pFace = grBrush_GetNextFace( pBrush, pFace ) ;
	}  
	return( GR_TRUE );
}// BrushExtBox


#ifdef WIN32
void Init_Class( HINSTANCE hInstance )
#endif
#ifdef BUILD_BE
void Init_Class( image_id hInstance )
#endif
{
	ghInstance = hInstance;
}



void * GRCC CreateInstance(void)
{
	ModelInstance * pModelInstance;
	
	pModelInstance = GR_RAM_ALLOCATE_STRUCT( ModelInstance );
	if( pModelInstance == NULL )
		return( NULL );

	pModelInstance->pModel = grModel_Create();
	pModelInstance->RefCnt = 1;
	if( pModelInstance->pModel == NULL )
	{
		grRam_Free( pModelInstance );
		return( NULL );
	}
		
	return( pModelInstance );

}


void GRCC CreateRef(void * Instance)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );

	pModelInstance->RefCnt++;
}

grBoolean GRCC Destroy(void **pInstance)
{
	ModelInstance **hModelInstance = (ModelInstance**)pInstance;
	ModelInstance *pModelInstance = *hModelInstance;

	assert( pInstance );
	assert( pModelInstance->RefCnt > 0 );

	pModelInstance->RefCnt--;
	if( pModelInstance->RefCnt == 0 )
	{
		grModel_Destroy( &pModelInstance->pModel );
		grRam_Free( pModelInstance );
	}
	else
		return( GR_FALSE );
	return( GR_TRUE );
}


grBoolean GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	ModelInstance		*pModelInstance = (ModelInstance*)Instance;
	
	if (!grModel_Render(pModelInstance->pModel, (grCamera*)Camera, (grFrustum*)CameraSpaceFrustum))
		return GR_FALSE;

	return GR_TRUE;
}

grBoolean	GRCC AttachWorld( void * Instance, grWorld * pWorld )
{
	ModelInstance		*pModelInstance = (ModelInstance*)Instance;
	grFaceInfo_Array	*FArray;
	grMaterial_Array	*MArray;
	grChain				*LChain;
	grChain				*DLChain;

	assert( Instance );
	assert( pWorld );
	
	FArray = grWorld_GetFaceInfoArray(pWorld);
	assert(FArray);

	MArray = grWorld_GetMaterialArray(pWorld);
	assert(MArray);

	LChain = grWorld_GetLightChain(pWorld);
	assert(LChain);

	DLChain = grWorld_GetDLightChain(pWorld);
	assert(DLChain);

   	// Krouer - distribute the World pointer to the model
   	grModel_SetWorld(pModelInstance->pModel, pWorld);
	grModel_SetArrays(pModelInstance->pModel, FArray, MArray, LChain, DLChain);

	return GR_TRUE;
}

grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld )
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
	assert( pWorld );

   	// Krouer - distribute the World NULL pointer to the model
   	grModel_SetWorld(pModelInstance->pModel, NULL);
	return GR_TRUE;
}
				
grBoolean	GRCC AttachEngine ( void * Instance, grEngine *Engine )
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
	assert( Engine);

	return grModel_SetEngine(pModelInstance->pModel, Engine);
}

grBoolean	GRCC DettachEngine( void * Instance, grEngine *Engine )
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
	assert( Engine);

	grModel_SetEngine(pModelInstance->pModel, NULL);

	return( GR_TRUE );
}

grBoolean	GRCC AttachSoundSystem( void * Instance, grSound_System *SoundSystem )
{
	return( GR_TRUE );
	Instance;
	SoundSystem;
}

grBoolean	GRCC DettachSoundSystem( void * Instance, grSound_System *SoundSystem )
{
	return( GR_TRUE );
	Instance;
	SoundSystem;
}

grBoolean	GRCC Collision(const void * Instance, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{	
	ModelInstance		*pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
		
	// Incarnadine
	return( grModel_Collision( 	pModelInstance->pModel, Box, Front, Back, Impact, Plane) );		
}

grBoolean GRCC GetExtBox(const void * Instance, grExtBox *BBox)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;
	grBrush		*pBrush;
	grExtBox	ExtBox;
	grXForm3d	ModelXF;

	
	GetXForm( Instance, &ModelXF );
	pBrush = grModel_GetNextBrush( pModelInstance->pModel, NULL );
	if( pBrush == NULL )
		return( GR_FALSE );
	BrushExtBox( &ModelXF, pBrush, BBox );
	while( pBrush )
	{
		BrushExtBox( &ModelXF, pBrush, &ExtBox );
		if( grExtBox_IsValid( &ExtBox ) )
		{
			if( grExtBox_IsValid( BBox ) )
				grExtBox_Union ( BBox, &ExtBox, BBox );
			else
				*BBox = ExtBox;
		}
		pBrush = grModel_GetNextBrush( pModelInstance->pModel, pBrush );
	}
	return( GR_TRUE );
}


void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	ModelInstance * pModelInstance;
	BYTE Version;
	uint32 Tag;
	

	pModelInstance = GR_RAM_ALLOCATE_STRUCT( ModelInstance );
	if( pModelInstance == NULL )
		return( NULL );

	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "ModelObject_CreateFromFile:Tag" );
		goto CFF_ERROR;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "ModelObject_CreateFromFile:Version" );
	       	goto CFF_ERROR;
		}
	}
	else
	{
		//for backwards compatibility with old object format
		Version = 1;
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
	}
	

	if (Version >= 1)
	{
    	pModelInstance->pModel = grModel_CreateFromFile( File, PtrMgr);
	    if( pModelInstance->pModel == NULL )
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ, "grModel_CreateFromFile" );
		    goto CFF_ERROR;
		}
	}
    
	pModelInstance->RefCnt = 1;
	OutputDebugString("END: ModelObject_CreateFromFile()\n");

	return( pModelInstance );

CFF_ERROR:
	OutputDebugString("ERROR: ModelObject_CreateFromFile()\n");

	grRam_Free( pModelInstance );
	return( NULL );
}


grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *PtrMgr)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;
	BYTE Version = MODELOBJECT_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	assert( Instance );


	if (!grVFile_Write( File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "ModelObject_WriteToFile:Tag" );
	    return( GR_FALSE );
	}

	if (!grVFile_Write( File, &Version, sizeof(Version)))
	{
    	grErrorLog_Add( GR_ERR_FILEIO_WRITE, "ModelObject_WriteToFile:VersionString" );
	    return( GR_FALSE );
	}

	if( !grModel_WriteToFile(pModelInstance->pModel, File, PtrMgr ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "grModel_WriteToFile" );
		return( GR_FALSE );
	}

	return( GR_TRUE );
	PtrMgr;
}


grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;
	char * Name = NULL;
	grProperty_List *PropertyList;
	grProperty		 Property;
	const		grBSP_DebugInfo * DebugInfo;

	assert( Instance );
	*List = NULL;

	DebugInfo = grModel_GetBSPDebugInfo( pModelInstance->pModel );
	PropertyList = grProperty_ListCreate( 0 );
	if( PropertyList == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillGroup");
		return( GR_FALSE );
	}

	Name = Util_LoadLibraryString( ghInstance, IDS_STATS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillGroup( &Property, Name, MODEL_STATS_ID ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillGroup");
		goto fail;
	}
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );


	Name = Util_LoadLibraryString( ghInstance, IDS_AREAS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumAreas, MODEL_AREAS_ID ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_VIS_PORTALS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumVisPortals, MODEL_VIS_PORTALS_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_PORTALS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumPortals, MODEL_PORTALS_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_SUB_FACES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumSubdividedDrawFaces, MODEL_SUB_FACES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_DRAW_FACES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumDrawFaces, MODEL_DRAW_FACES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_SPLITS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumSplits, MODEL_SPLITS_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_LEAFS );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumLeafs, MODEL_LEAFS_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_NODES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumNodes, MODEL_NODES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_BRUSH_FACES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumTotalBrushFaces, MODEL_BRUSH_FACES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_BRUSHES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumBrushes, MODEL_BRUSHES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	Name = Util_LoadLibraryString( ghInstance, IDS_VISABLE_FACES );
	if( Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "GetPropertyList:Util_LoadLibraryString");
		goto fail;
	}
	if( !grProperty_FillStaticInt( &Property, Name, DebugInfo->NumVisibleBrushFaces, MODEL_VISABLE_FACES_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillStaticInt");
		goto fail;
	}
	grProperty_SetDisabled( &Property, GR_TRUE );
	grProperty_Append( PropertyList, &Property );
	grRam_Free( Name );

	if( !grProperty_FillGroupEnd( &Property, MODEL_STATS_END_ID  ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "GetPropertyList:grProperty_FillGroupEnd");
		goto fail;
	}
	grProperty_Append( PropertyList, &Property );

	*List = PropertyList;
	return( GR_TRUE );
fail:
	if( Name != NULL )
		grRam_Free( Name );
	grProperty_ListDestroy( &PropertyList );
	*List = NULL;
	return( GR_FALSE );
}

grBoolean	GRCC SetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{

	assert( Instance );
	return( GR_TRUE );
}

grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );

	return( grModel_SetXForm( pModelInstance->pModel, XF ) );
}

grBoolean GRCC GetXForm(const void * Instance,grXForm3d *XF)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
	grXForm3d_Copy( grModel_GetXForm(pModelInstance->pModel ), XF  );
	return( GR_TRUE );
}

int	GRCC GetXFormModFlags( const void * Instance )
{
	Instance;
	return( GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE );
}

grBoolean GRCC GetChildren(const void * Instance,grObject * Children,int MaxNumChildren)
{
	return( GR_TRUE );
}

grBoolean GRCC AddChild(void * Instance,const grObject * Child)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );

	return grModel_AddObject(pModelInstance->pModel,(grObject*) Child);
}

grBoolean GRCC RemoveChild(void * Instance,const grObject * Child)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert( Instance );

	return grModel_RemoveObject(pModelInstance->pModel,(grObject*) Child);
}

#ifdef WIN32
grBoolean GRCC EditDialog (void * Instance,HWND Parent)
#endif
#ifdef BUILD_BE
grBoolean GRCC EditDialog (void * Instance, class G3DView *Parent)
#endif
{
	return( GR_TRUE );
}

grBoolean GRCC SendModelMessage(void *Instance, int32 Msg, void *Data)
{
	ModelInstance *pModelInstance = (ModelInstance*)Instance;

	assert(Instance);
	assert(pModelInstance->pModel);

	switch (Msg)
	{
		// Incarnadine Begin	
	    case GR_OBJECT_MSG_WORLD_REBUILDBSP:  
		{
			grBSPSetup *BSPSetup;

			assert(Data);
	
			BSPSetup = (grBSPSetup*)Data;
			return grModel_RebuildBSP(pModelInstance->pModel, BSPSetup->Options, BSPSetup->Logic, BSPSetup->LogicBalance);					
		} 
		case GR_OBJECT_MSG_WORLD_REBUILDLIGHTS:
		{
			return grModel_RebuildLights(pModelInstance->pModel);			
		}
		// Incarnadine End
		case GR_OBJECT_MSG_WORLD_ADD_SLIGHT_UPDATE:
		case GR_OBJECT_MSG_WORLD_REMOVE_SLIGHT_UPDATE:
		{
			grVec3d		Pos;
			grFloat		Radius;

			assert(Data);

			if (!grLight_GetAttributes((grLight*)Data, &Pos, NULL, &Radius, NULL, NULL))
				return GR_FALSE;

			if (!grModel_RebuildLightsFromPoint(pModelInstance->pModel, &Pos, Radius))
				return GR_FALSE;

			break;
		}
	}

	return GR_TRUE;
}

//Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{	
	ModelInstance		*pModelInstance = (ModelInstance*)Instance;

	assert( Instance );
		
	return( grModel_ChangeBoxCollision(pModelInstance->pModel, Pos, FrontBox, BackBox, ImpactBox, Plane) );		
}