/****************************************************************************************/
/*  CAMOBJECT.C                                                                         */
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
#include <Resources.h>
#include <image.h>
#endif

#include <string.h>
#include <memory.h>
#include <assert.h>

#include "CamObject.h"
#include "grTypes.h"
#include "grProperty.h"
#include "grUserPoly.h"
#include "Genesis3D.h"
#include "Camera.h"
#include "Ram.h"

#include "Bitmap.h"
#include "VFile.h"

#include "Errorlog.h"
#include "Resource.h"
#include "EditMsg.h"
#include "CamFieldID.h"


#define CAMOBJECT_VERSION 2


enum {
	CAMREA_FOV_INDEX,

	// BEGIN - Far clip plane editor box - paradoxnj 3/9/2005
	CAMREA_FARCLIP_INDEX,
	CAMREA_FARCLIPENABLED_INDEX,
	// END - Far clip plane editor box - paradoxnj 3/9/2005

	CAMREA_LAST_INDEX
};

grBrush *	Brush;

typedef struct CamObj {

	grFloat				FOV;
	grXForm3d			XForm;

	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	grBoolean			FarClipEnabled;
	grFloat				FarClip;
	// END - Far clip plane box - paradoxnj 3/9/2005

	int					RefCnt;
} CamObj;


grProperty CamProperties[CAMREA_LAST_INDEX];
grProperty_List CamPropertyList = { CAMREA_LAST_INDEX, &CamProperties[0], GR_FALSE };

char *NameList[3];

#define UTIL_MAX_RESOURCE_LENGTH	(128)
static char stringbuffer[UTIL_MAX_RESOURCE_LENGTH + 1];

#define DEFAULT_SIZE 16.0f

static grBoolean BrushExtBox( grBrush * pBrush, grExtBox * pExtBox )
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
			if( grExtBox_IsValid( pExtBox ) )
				grExtBox_ExtendToEnclose( pExtBox, &Vert );
			else
				grExtBox_SetToPoint ( pExtBox, &Vert );
		}
		pFace = grBrush_GetNextFace( pBrush, pFace ) ;
	}  
	return( GR_TRUE );
}// BrushExtBox

static grBoolean CamObj_CreateFace( grBrush * Brush, grVec3d *Verts, int32 nVerts, grFaceInfo * pFaceInfo)
{
	grBrush_Face *Face;
	int i;

	assert( Brush );
	assert( Verts );

	Face = grBrush_CreateFace(Brush, nVerts);
	if( Face == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Unable to create brush face." );
		return(GR_FALSE );
	}
	for( i = 0; i < nVerts ; i++)
		grBrush_FaceSetVertByIndex(Face, i, &Verts[i] );
	grBrush_FaceSetFaceInfo(Face, pFaceInfo);
	return(GR_TRUE );
}

grBoolean CreateGlobalBrush (int BoxSize  )
{
	//revisit for error handling when merged
	grVec3d		Verts[16];
	grVec3d		FaceVerts[4];
	grFaceInfo  FaceInfo;


	grFaceInfo_SetDefaults( &FaceInfo );
	Brush = grBrush_Create(11);
	if(Brush == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Unable to create grBrush" );
		return( GR_FALSE );
	}

	// Vertices 0 to 3 are the 4 corners of the top face
	grVec3d_Set (&Verts[0], (float)-(BoxSize/2), (float)(BoxSize/2), (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[1], (float)-(BoxSize/2), (float)(BoxSize/2), (float)(BoxSize*0.75f));
	grVec3d_Set (&Verts[2], (float)(BoxSize/2), (float)(BoxSize/2), (float)(BoxSize*0.75f));
	grVec3d_Set (&Verts[3], (float)(BoxSize/2), (float)(BoxSize/2), (float)-(BoxSize*0.38f));

	// Vertices 4 to 7 are the 4 corners of the bottom face
	grVec3d_Set (&Verts[4], (float)-(BoxSize/2), (float)-(BoxSize/2), (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[5], (float)(BoxSize/2), (float)-(BoxSize/2), (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[6], (float)(BoxSize/2), (float)-(BoxSize/2), (float)(BoxSize*0.75f));
	grVec3d_Set (&Verts[7], (float)-(BoxSize/2), (float)-(BoxSize/2), (float)(BoxSize*0.75f));

	// Vertices 8 to 11 are the 4 corners of the Lens bottom
	grVec3d_Set (&Verts[8], (float)-(BoxSize/4), (float)-(BoxSize/4), (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[9], (float)-(BoxSize/4), (float)(BoxSize/4) , (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[10], (float)(BoxSize/4), (float)(BoxSize/4) , (float)-(BoxSize*0.38f));
	grVec3d_Set (&Verts[11], (float)(BoxSize/4), (float)-(BoxSize/4), (float)-(BoxSize*0.38f));

	// Vertices 12 to 11 are the 4 corners of the Lens top
	grVec3d_Set (&Verts[12], (float)-(BoxSize/3), (float)-(BoxSize/3), (float)-(BoxSize*0.75f));
	grVec3d_Set (&Verts[13], (float)-(BoxSize/3), (float)(BoxSize/3) , (float)-(BoxSize*0.75f));
	grVec3d_Set (&Verts[14], (float)(BoxSize/3) , (float)(BoxSize/3) , (float)-(BoxSize*0.75f) );
	grVec3d_Set (&Verts[15], (float)(BoxSize/3) , (float)-(BoxSize/3), (float)-(BoxSize*0.75f) );

	FaceVerts[3]	=Verts[0];
	FaceVerts[2]	=Verts[1];
	FaceVerts[1]	=Verts[2];
	FaceVerts[0]	=Verts[3];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[4];
	FaceVerts[2]	=Verts[5];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[7];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[1];
	FaceVerts[2]	=Verts[7];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[2];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[0];
	FaceVerts[2]	=Verts[3];
	FaceVerts[1]	=Verts[5];
	FaceVerts[0]	=Verts[4];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[0];
	FaceVerts[2]	=Verts[4];
	FaceVerts[1]	=Verts[7];
	FaceVerts[0]	=Verts[1];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[3];
	FaceVerts[2]	=Verts[2];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[5];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}



	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[9];
	FaceVerts[1]	=Verts[10];
	FaceVerts[0]	=Verts[11];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[12];
	FaceVerts[2]	=Verts[13];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[15];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[9];
	FaceVerts[2]	=Verts[15];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[13];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[11];
	FaceVerts[1]	=Verts[13];
	FaceVerts[0]	=Verts[12];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[12];
	FaceVerts[1]	=Verts[15];
	FaceVerts[0]	=Verts[9];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[11];
	FaceVerts[2]	=Verts[10];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[13];

	if( !CamObj_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	return	GR_TRUE;
}

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

#ifdef WIN32
void Init_Class( HINSTANCE hInstance )
#endif
#ifdef BUILD_BE
void Init_Class( image_id hInstance )
#endif
{
	char * FieldName;

	FieldName = Util_LoadLibraryString(hInstance, IDS_FOV );
	if( FieldName )
	{
		grProperty_FillFloat( &CamProperties[CAMREA_FOV_INDEX],FieldName,  1.0f, CAMREA_FOV_ID, 0.1f, 4.0f, 0.1f );
		grRam_Free( FieldName );
	}

	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	FieldName = Util_LoadLibraryString(hInstance, IDS_FARCLIP);
	if (FieldName)
	{
		grProperty_FillFloat( &CamProperties[CAMREA_FARCLIP_INDEX], FieldName, 10000.0f, CAMREA_FARCLIP_ID, 1.0f, 99999.0f, 1.0f);
		grRam_Free(FieldName);
	}

	FieldName = Util_LoadLibraryString(hInstance, IDS_FARCLIPENABLED);
	if (FieldName)
	{
		grProperty_FillCheck(&CamProperties[CAMREA_FARCLIPENABLED_INDEX], FieldName, GR_TRUE, CAMREA_FARCLIPENABLE_ID);
		grRam_Free(FieldName);
	}
	// END - Far clip plane box - paradoxnj 3/9/2005
}



void * GRCC CreateInstance( void )
{
	CamObj *pCamObj;

	pCamObj = GR_RAM_ALLOCATE_STRUCT_CLEAR( CamObj );
	if( pCamObj == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "CreateInstance");
		return( NULL );
	}
	pCamObj->FOV = 2.0f;
	grXForm3d_SetIdentity( &pCamObj->XForm );

	
	pCamObj->RefCnt = 1;
	return( pCamObj );

}

void * GRCC DuplicateInstance(void * Instance)
{
	CamObj *pCamObj = (CamObj*)Instance;
	CamObj *pNewCamObj;

	pNewCamObj = (CamObj *)CreateInstance( );
	if( pNewCamObj == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "DuplicateInstance:CreateInstance");
		return( NULL );
	}
	grXForm3d_Copy( &pCamObj->XForm, &pNewCamObj->XForm );
	pNewCamObj->FOV			= pCamObj->FOV;		
	pNewCamObj->RefCnt		= pCamObj->RefCnt;		

	return( pNewCamObj );
}

void GRCC CreateRef(void * Instance)
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );

	pCamObj->RefCnt++;
}

grBoolean GRCC Destroy(void **pInstance)
{
	CamObj **hCamObj = (CamObj**)pInstance;
	CamObj *pCamObj = *hCamObj;

	assert( pInstance );
	assert( pCamObj->RefCnt > 0 );

	pCamObj->RefCnt--;
	if( pCamObj->RefCnt == 0 )
	{
		grRam_Free( pCamObj );
	}
	else
		return( GR_FALSE );
	return( GR_TRUE );
}


grBoolean GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{

	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	CamObj					*pCamObj = (CamObj*)Instance;
	grRect					Rect;
	grFloat					FOV;

	grCamera_GetAttributes((grCamera*)Camera, &FOV, &Rect);
	grCamera_SetFarClipPlane((grCamera*)Camera, pCamObj->FarClipEnabled, pCamObj->FarClip);
	grCamera_SetAttributes((grCamera*)Camera, pCamObj->FOV, &Rect);
	//END - Far clip plane box - paradoxnj 3/9/2005

	return( GR_TRUE );

}

grBoolean	GRCC AttachWorld( void * Instance, grWorld * pWorld )
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	return( GR_TRUE );
}

grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld )
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	return( GR_TRUE );
}
				
grBoolean	GRCC AttachEngine ( void * Instance, grEngine *Engine )
{
 return( GR_TRUE );
 Engine;
 Instance;
}

grBoolean	GRCC DettachEngine( void * Instance, grEngine *Engine )
{
	return( GR_TRUE );
	Instance;
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

grBoolean	GRCC Collision(const grObject *Object, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{
	return( GR_FALSE );
}


grBoolean GRCC GetExtBox(const void * Instance,grExtBox *BBox)
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );

	grBrush_SetXForm( Brush,  &pCamObj->XForm, GR_FALSE );
	BrushExtBox( Brush, BBox );
	return( GR_TRUE );
}


void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	CamObj	*pCamObj;
	BYTE Version;
	uint32 Tag;
	OutputDebugString("CamObject\n");
	pCamObj = GR_RAM_ALLOCATE_STRUCT( CamObj );
	
	if( pCamObj == NULL )
		return( NULL );

	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:Tag" );
		goto CFF_ERROR;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:Version" );
	       	goto CFF_ERROR;
		}
	}
	else
	{
		Version = 1;
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
	}
	
	if (Version >= 1)
	{
    	if( !grVFile_Read(	File, &pCamObj->FOV, sizeof( pCamObj->FOV) ) )
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:FOV" );
		    goto CFF_ERROR;
		}

		if (!grVFile_Read(File, &pCamObj->XForm, sizeof(pCamObj->XForm)))
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:XForm" );
		    goto CFF_ERROR;
		}

	}

	if (Version >= 2)
	{
    	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	    if (!grVFile_Read(File, &pCamObj->FarClipEnabled, sizeof(grBoolean)))
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:FarClipEnable");
		    goto CFF_ERROR;
		}

	    if (!grVFile_Read(File, &pCamObj->FarClip, sizeof(grFloat)))
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CamObject_CreateFromFile:FarClip");
		    goto CFF_ERROR;
		}
	    // END - Far clip plane box - paradoxnj 3/9/2005
	}
	else
	{
		// Defualt Values
		pCamObj->FarClipEnabled = GR_FALSE;
        pCamObj->FarClip = 10000.00f;
	}


	pCamObj->RefCnt = 1;
	return( pCamObj );

CFF_ERROR:

	grRam_Free( pCamObj );
	return( NULL );
}



grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *PtrMgr)
{
	BYTE Version = CAMOBJECT_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	CamObj *pCamObj = (CamObj*)Instance;
	
	
	assert( Instance );

	if( !grVFile_Write(	File, &Tag,sizeof(Tag)))
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "CamObject_WriteToFile:Tag");
	    return( GR_FALSE );
	}
	
	if( !grVFile_Write(	File, &Version, sizeof(Version) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "CamObject_WriteToFile:Version");
	    return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pCamObj->FOV, sizeof( pCamObj->FOV) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "CreateFromFile:FOV" );
		return( GR_FALSE );
	}

	if (!grVFile_Write(File, &pCamObj->XForm, sizeof(pCamObj->XForm)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "CreateFromFile:XForm" );
		return( GR_FALSE );
	}

	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	if (!grVFile_Write(File, &pCamObj->FarClipEnabled, sizeof(grBoolean)))
	{
		grErrorLog_Add(GR_ERR_FILEIO_WRITE, "WriteToFile:FarClipEnabled");
		return GR_FALSE;
	}

	if (!grVFile_Write(File, &pCamObj->FarClip, sizeof(grFloat)))
	{
		grErrorLog_Add(GR_ERR_FILEIO_WRITE, "WriteToFile:FarClip");
		return GR_FALSE;
	}
	// END - Far clip plane box - paradoxnj 3/9/2005

	return( GR_TRUE );
}

grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List)
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );

	CamProperties[CAMREA_FOV_INDEX].Data.Float = pCamObj->FOV;
	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	CamProperties[CAMREA_FARCLIP_INDEX].Data.Float = pCamObj->FarClip;
	CamProperties[CAMREA_FARCLIPENABLED_INDEX].Data.Bool = pCamObj->FarClipEnabled;
	// END - Far clip plane box - paradoxnj 3/9/2005

	*List = grProperty_ListCopy( &CamPropertyList );
	if( *List == NULL )
		return( GR_FALSE );
	return( GR_TRUE );
}

grBoolean	GRCC SetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	switch( FieldID  )
	{
	case CAMREA_FOV_ID:
		assert( DataType == PROPERTY_FLOAT_TYPE );
		pCamObj->FOV = pData->Float;
		break;
		
	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	case CAMREA_FARCLIPENABLE_ID:
		assert( DataType == PROPERTY_CHECK_TYPE );
		pCamObj->FarClipEnabled = pData->Bool;
		break;

	case CAMREA_FARCLIP_ID:
		assert(DataType == PROPERTY_FLOAT_TYPE);
		pCamObj->FarClip = pData->Float;
		break;
	// END - Far clip plane box - paradoxnj 3/9/2005

	}
	return( GR_TRUE );
}

grBoolean	GRCC GetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	switch( FieldID  )
	{
	case CAMREA_FOV_ID:
		assert( DataType == PROPERTY_FLOAT_TYPE );
		pData->Float = pCamObj->FOV;
		break;

	// BEGIN - Far clip plane box - paradoxnj 3/9/2005
	case CAMREA_FARCLIPENABLE_ID:
		assert(DataType == PROPERTY_CHECK_TYPE);
		pData->Bool = pCamObj->FarClipEnabled;
		break;

	case CAMREA_FARCLIP_ID:
		assert(DataType == PROPERTY_FLOAT_TYPE);
		pData->Float = pCamObj->FarClip;
		break;
	// END - Far clip plane box - paradoxnj 3/9/2005
	}
	return( GR_TRUE );
}

grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF)
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	grXForm3d_Copy( XF, &pCamObj->XForm );

	pCamObj->XForm.Flags = XFORM3D_NONORTHOGONALISOK;
	grXForm3d_Orthonormalize(&pCamObj->XForm);
	return( GR_TRUE);
}

grBoolean GRCC GetXForm(const void * Instance,grXForm3d *XF)
{
	CamObj *pCamObj = (CamObj*)Instance;

	assert( Instance );
	grXForm3d_Copy( &pCamObj->XForm, XF  );
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
	return( GR_TRUE );
}

grBoolean GRCC RemoveChild(void * Instance,const grObject * Child)
{
	return( GR_TRUE );
}

#ifdef WIN32
grBoolean GRCC EditDialog (void * Instance,HWND Parent)
#endif
#ifdef BUILD_BE
grBoolean GRCC EditDialog (void * Instance, class G3DView* Parent)
#endif
{
	return( GR_TRUE );
}

grBoolean GRCC SendMsg(void * Instance, int32 Msg, void * Data)
{
	CamObj *pCamObj = (CamObj*)Instance;

	switch( Msg)
	{
		case G3DEDITOR_GET_GRBRUSH:
		{
			grBrush **hBrush = (grBrush**)Data;
			if( Brush == NULL )
				if( !CreateGlobalBrush(16) )
					return(GR_FALSE);
			grBrush_SetXForm( Brush, &pCamObj->XForm, GR_FALSE);
			*hBrush = Brush;
			return( GR_TRUE );
		}


	}
	return( GR_FALSE );
}

grBoolean GRCC PortalFrame( void *Instance, grFloat Time)
{
	Instance;
	Time;
	return GR_TRUE;
}

// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );
}