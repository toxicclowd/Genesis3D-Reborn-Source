/****************************************************************************************/
/*  PORTALOBJECT.C                                                                      */
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

#include "PortalObject.h"
#include "grTypes.h"
#include "grProperty.h"
#include "grUserPoly.h"
#include "Genesis3D.h"
#include "Camera.h"
#include "Ram.h"
#include "Bitmap.h"
#include "VFile.h"
#include "Errorlog.h"
//#include "resource.h"
#include "EditMsg.h"


#define PORTALOBJECT_VERSION 1


typedef struct 
{
	const grPlane		*Plane;
	const grXForm3d		*FaceXForm;
	grWorld				*World;
	grCamera			*Camera;
	grFrustum			*Frustum;
} PortalMsgData;

enum {
	PORTAL_SKYBOX_CHECK_ID = PROPERTY_LOCAL_DATATYPE_START,
	PORTAL_SPEED_ID,
	PORTAL_RADIOX_ID,
	PORTAL_RADIOY_ID,
	PORTAL_RADIOZ_ID,
	PORTAL_FOV_ID,  // Jeff: For FOV property
	PORTAL_LAST_ID
};

enum {
	PORTAL_SKYBOX_CHECK_INDEX,
	PORTAL_SPEED_INDEX,
	PORTAL_RADIOX_INDEX,
	PORTAL_RADIOY_INDEX,
	PORTAL_RADIOZ_INDEX,
	PORTAL_FOV_INDEX,     // Jeff: For FOV property
	PORTAL_LAST_INDEX
};

grBrush *	Brush;

typedef struct PortalObj {

	grFloat				FOV;

	grPortal		*Portal;

	grBoolean		SkyBox;
	grFloat			RotateSpeed;
	grFloat			Rotation;
	int32			RAxis;

	int					RefCnt;

	grBoolean		RenderNextFlag;
} PortalObj;


grProperty PortalProperties[PORTAL_LAST_INDEX];
grProperty_List PortalPropertyList = { PORTAL_LAST_INDEX, &PortalProperties[0] };

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

static grBoolean Portal_CreateFace( grBrush * Brush, grVec3d *Verts, int32 nVerts, grFaceInfo * pFaceInfo)
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

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[4];
	FaceVerts[2]	=Verts[5];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[7];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[1];
	FaceVerts[2]	=Verts[7];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[2];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[0];
	FaceVerts[2]	=Verts[3];
	FaceVerts[1]	=Verts[5];
	FaceVerts[0]	=Verts[4];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[0];
	FaceVerts[2]	=Verts[4];
	FaceVerts[1]	=Verts[7];
	FaceVerts[0]	=Verts[1];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[3];
	FaceVerts[2]	=Verts[2];
	FaceVerts[1]	=Verts[6];
	FaceVerts[0]	=Verts[5];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}



	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[9];
	FaceVerts[1]	=Verts[10];
	FaceVerts[0]	=Verts[11];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[12];
	FaceVerts[2]	=Verts[13];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[15];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[9];
	FaceVerts[2]	=Verts[15];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[13];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[11];
	FaceVerts[1]	=Verts[13];
	FaceVerts[0]	=Verts[12];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[8];
	FaceVerts[2]	=Verts[12];
	FaceVerts[1]	=Verts[15];
	FaceVerts[0]	=Verts[9];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grBrush_Destroy( &Brush);
	}

	FaceVerts[3]	=Verts[11];
	FaceVerts[2]	=Verts[10];
	FaceVerts[1]	=Verts[14];
	FaceVerts[0]	=Verts[13];

	if( !Portal_CreateFace( Brush, FaceVerts, 4, &FaceInfo ) )
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
void Init_Class( HINSTANCE hInstance )
#endif
#ifdef BUILD_BE
void Init_Class( image_id hInstance )
#endif
{
	grProperty *Property;

	Property = &PortalProperties[PORTAL_SKYBOX_CHECK_INDEX];
	grProperty_FillCheck(Property, "SkyBox", 1, PORTAL_SKYBOX_CHECK_ID);

	Property = &PortalProperties[PORTAL_SPEED_INDEX];
	grProperty_FillFloat(Property, "Speed", 1.0f, PORTAL_SPEED_ID, 0.0f, 100.0f, 1.0f);

	Property = &PortalProperties[PORTAL_RADIOX_INDEX];
	grProperty_FillRadio(Property, "X Axis", GR_TRUE, PORTAL_RADIOX_ID);

	Property = &PortalProperties[PORTAL_RADIOY_INDEX];
	grProperty_FillRadio(Property, "Y Axis", GR_FALSE, PORTAL_RADIOY_ID);

	Property = &PortalProperties[PORTAL_RADIOZ_INDEX];
	grProperty_FillRadio(Property, "Z Axis", GR_FALSE, PORTAL_RADIOZ_ID);

	// Jeff:  Init FOV property
	Property = &PortalProperties[PORTAL_FOV_INDEX];
	grProperty_FillFloat(Property, "FOV", 2.0f, PORTAL_FOV_ID, -GR_PI, GR_PI, 0.1f);

}



void * GRCC CreateInstance( void )
{
	PortalObj *pPortalObj;
	grPortal *Portal;

	Portal = grPortal_Create();

	if (!Portal)
	{
		return NULL;
	}

	pPortalObj = GR_RAM_ALLOCATE_STRUCT_CLEAR( PortalObj );
	if( pPortalObj == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "CreateInstance");
		return( NULL );
	}
	pPortalObj->FOV = 2.0f;

	Portal->Recursion = 0;

	pPortalObj->Portal = Portal;
	pPortalObj->RAxis = 1;

	pPortalObj->RenderNextFlag = GR_TRUE;
	
	pPortalObj->RefCnt = 1;

	if( Brush == NULL )
	{
		if( !CreateGlobalBrush(16) )
			return(NULL);
	}

	return( pPortalObj );

}

void * GRCC DuplicateInstance(void * Instance)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;
	PortalObj *pNewPortalObj;

	pNewPortalObj = (PortalObj *)CreateInstance( );
	if( pNewPortalObj == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "DuplicateInstance:CreateInstance");
		return( NULL );
	}
	grXForm3d_Copy( &pPortalObj->Portal->XForm, &pNewPortalObj->Portal->XForm );
	pNewPortalObj->FOV			= pPortalObj->FOV;		
	pNewPortalObj->RefCnt		= pPortalObj->RefCnt;		

	pNewPortalObj->SkyBox		= pPortalObj->SkyBox;
	pNewPortalObj->RotateSpeed	= pPortalObj->RotateSpeed;
	pNewPortalObj->Rotation		= pPortalObj->Rotation;
	pNewPortalObj->RAxis		= pPortalObj->RAxis;

	return( pNewPortalObj );
}

void GRCC CreateRef(void * Instance)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );

	pPortalObj->RefCnt++;
}

grBoolean GRCC Destroy(void **pInstance)
{
	PortalObj **hPortalObj = (PortalObj**)pInstance;
	PortalObj *pPortalObj = *hPortalObj;

	assert( pInstance );
	assert( pPortalObj->RefCnt > 0 );

	pPortalObj->RefCnt--;
	if( pPortalObj->RefCnt == 0 )
	{
		grPortal_Destroy(&pPortalObj->Portal);
		grRam_Free( pPortalObj );
	}
	else
		return( GR_FALSE );
	return( GR_TRUE );
}


grBoolean GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	// TODO : implement it with Vertex/Index Section data call
	return( GR_TRUE );
}

grBoolean	GRCC AttachWorld( void * Instance, grWorld * pWorld )
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );
	return( GR_TRUE );
}

grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld )
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

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
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );

	grBrush_SetXForm( Brush,  &pPortalObj->Portal->XForm, GR_FALSE );
	BrushExtBox( Brush, BBox );
	return( GR_TRUE );
}


void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	PortalObj * pPortalObj;
	grPortal *Portal;
	BYTE Version;
	uint32 Tag;

	Portal = grPortal_Create();

	if (!Portal)
	{
		return NULL;
	}

	pPortalObj = GR_RAM_ALLOCATE_STRUCT( PortalObj );
	if( pPortalObj == NULL )
		return( NULL );

	pPortalObj->Portal = Portal;


	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "PortalObject_CreateFromFile:VersionString" );
		goto CFF_ERROR;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "PortalObject_CreateFromFile:Version" );
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
	
	    if( !grVFile_Read(	File, &pPortalObj->FOV, sizeof( pPortalObj->FOV) ) )
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CreateFromFile:FOV" );
		    goto CFF_ERROR;
		}

	    if (!grVFile_Read(File, &pPortalObj->Portal->XForm, sizeof(pPortalObj->Portal->XForm)))
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CreateFromFile:XForm" );
		    goto CFF_ERROR;
		}

	    if (!grVFile_Read(File, &pPortalObj->SkyBox, sizeof(pPortalObj->SkyBox)))
		{
		    grErrorLog_Add( GR_ERR_FILEIO_READ, "CreateFromFile:SkyBox" );
		    goto CFF_ERROR;
		}

	}

	pPortalObj->RenderNextFlag = GR_TRUE;

	pPortalObj->RefCnt = 1;

	if( Brush == NULL )
	{
		if( !CreateGlobalBrush(16) )
			return(NULL);
	}

	return( pPortalObj );

CFF_ERROR:
	grRam_Free( pPortalObj );
	return( NULL );
	PtrMgr;
}

grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *PtrMgr)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;
	BYTE Version = PORTALOBJECT_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	assert( Instance );

	if( !grVFile_Write(	File, &Tag, sizeof(Tag)))
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "PortalObject_WriteToFile:Tag");
	    return( GR_FALSE );
	}
	
	if( !grVFile_Write(	File, &Version, sizeof(Version) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "PortalObject_WriteToFile:Version");
	    return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pPortalObj->FOV, sizeof( pPortalObj->FOV) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "CreateFromFile:FOV" );
		return( GR_FALSE );
	}

	if (!grVFile_Write(File, &pPortalObj->Portal->XForm, sizeof(pPortalObj->Portal->XForm)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "CreateFromFile:XForm" );
		return( GR_FALSE );
	}

	if (!grVFile_Write(File, &pPortalObj->SkyBox, sizeof(pPortalObj->SkyBox)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "CreateFromFile:SkyBox" );
		return( GR_FALSE );
	}

	return( GR_TRUE );
	PtrMgr;
}


// Jeff:  Rewrote function to update all properties - 02/09/05
grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List)
{
	PortalObj		*pPortalObj;
	
	assert( Instance );
	pPortalObj = (PortalObj*)Instance;
	
	// setup property list
	PortalProperties[PORTAL_SKYBOX_CHECK_INDEX].Data.Bool = pPortalObj->SkyBox;
	PortalProperties[PORTAL_SPEED_INDEX].Data.Float = pPortalObj->RotateSpeed;
	PortalProperties[PORTAL_RADIOX_INDEX].Data.Bool = (pPortalObj->RAxis == 0);
	PortalProperties[PORTAL_RADIOY_INDEX].Data.Bool = (pPortalObj->RAxis == 1);
	PortalProperties[PORTAL_RADIOZ_INDEX].Data.Bool = (pPortalObj->RAxis == 2);
	PortalProperties[PORTAL_FOV_INDEX].Data.Float   = pPortalObj->FOV;


	// copy property list
	*List = grProperty_ListCopy( &PortalPropertyList );
	if ( *List == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Unable to create portal property list");
		return GR_FALSE;
	}

	return( GR_TRUE );
}

grBoolean	GRCC SetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );
	switch( FieldID  )
	{
		case PORTAL_SKYBOX_CHECK_ID:
		{
			assert(DataType == PROPERTY_CHECK_TYPE);
			pPortalObj->SkyBox = pData->Bool;
			break;
		}

		case PORTAL_SPEED_ID:
		{
			assert(DataType == PROPERTY_FLOAT_TYPE);
			pPortalObj->RotateSpeed = *(float*)pData;
			break;
		}

		case PORTAL_RADIOX_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			pPortalObj->RAxis = 0;
			break;
		}

		case PORTAL_RADIOY_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			pPortalObj->RAxis = 1;
			break;
		}

		case PORTAL_RADIOZ_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			pPortalObj->RAxis = 2;
			break;
		}

		// Jeff:  Update FOV 
		case PORTAL_FOV_ID:
		{
			assert(DataType == PROPERTY_FLOAT_TYPE);
			pPortalObj->FOV = *(float*)pData;
			break;
		}
	}
	return( GR_TRUE );
}

grBoolean	GRCC GetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );
	switch( FieldID  )
	{
	default:
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );
	grXForm3d_Copy( XF, &pPortalObj->Portal->XForm );

	pPortalObj->Portal->XForm.Flags = XFORM3D_NONORTHOGONALISOK;
	grXForm3d_Orthonormalize(&pPortalObj->Portal->XForm);
	return( GR_TRUE);
}

grBoolean GRCC GetXForm(const void * Instance,grXForm3d *XF)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	assert( Instance );
	grXForm3d_Copy( &pPortalObj->Portal->XForm, XF  );
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
static grBoolean RenderPortalObjectInstance2(PortalObj *pPortalObj, grPortal *Portal, const grPlane *Plane, const grXForm3d *FaceXForm, grWorld *World, grCamera *Camera, grFrustum *Frustum)
{
	grXForm3d		XForm, InvXForm, NewXForm;
	grBoolean		Ret;
	grFloat			ZScale;
	grFloat         FOV;    // Jeff: used to save and restore current camera's FOV
	grRect          Rect;   // Jeff: used to store current camera's rect       
	

	if (Portal->Recursion > 0)
		return GR_TRUE;

	// Get Camera XForm
	grCamera_GetXForm(Camera, &XForm);

	if (pPortalObj->SkyBox)
	{
		// Skymode
		NewXForm = XForm;

		grVec3d_Clear(&NewXForm.Translation);
		grXForm3d_Multiply(&Portal->XForm, &NewXForm, &NewXForm);
		
	// Jeff:  Active Skybox rotation code - 02/09/05	
	#if 1
		if (pPortalObj->Rotation)
		{
			switch (pPortalObj->RAxis)
			{
				case 0:
					grXForm3d_RotateX(&NewXForm, pPortalObj->Rotation);
					break;

				case 1:
					grXForm3d_RotateY(&NewXForm, pPortalObj->Rotation);
					break;

				case 2:
					grXForm3d_RotateZ(&NewXForm, pPortalObj->Rotation);
					break;
			}
		}
	#endif

		NewXForm.Translation = Portal->XForm.Translation;

		#pragma message ("Find the correct amount to scale by to put the camera on the new frustum front plane")
		ZScale = grCamera_GetZScale(Camera);
		grCamera_SetZScale(Camera, ZScale*20.0f);
	}
	else
	{
		grPlane		FrontPlane;
		grXForm3d	WorldToCameraXForm;

		//	XForm the Camera to the Dest Portal location
		// This is the equation we need to XForm the camera from the FaceXForm to the PortalXForm
		//	P' = PortalXForm*InvFaceXForm*CameraXForm
		//	What this does, is take the point, and XForm against the camera like normal, then XForm by the amount
		//	it would take to get the FaceXForm to line up with the origin, then XForm into the Portal XForm...
		grXForm3d_GetTranspose(FaceXForm, &InvXForm);

		grXForm3d_Multiply(&Portal->XForm, &InvXForm, &NewXForm);
		grXForm3d_Multiply(&NewXForm, &XForm, &NewXForm);
		
		// Get the WorldToCameraXForm
		grCamera_GetTransposeXForm(Camera, &WorldToCameraXForm);

		// Transform the FacePlane to camera space
		grPlane_Transform(Plane, &WorldToCameraXForm, &FrontPlane);

		// Add the Plane to the Frustum
		if (!grFrustum_AddPlane(Frustum, &FrontPlane, GR_TRUE))
			return GR_FALSE;
	}

	// Put the new XForm into the camera
	grCamera_SetXForm((grCamera*)Camera, &NewXForm);

	// Jeff: Save current camera's FOV and Rect
	grCamera_GetAttributes(Camera,&FOV,&Rect);

	// Jeff: Set camera's FOV 
	grCamera_SetAttributes(Camera,pPortalObj->FOV,&Rect);


	Portal->Recursion++;

	// Render the scene from this camera
	Ret = grWorld_Render(World, Camera, Frustum);

	Portal->Recursion--;

	// Restore Camera XForm
	grCamera_SetXForm((grCamera*)Camera, &XForm);

	// Jeff:  restore Camera's FOV and Rect
	grCamera_SetAttributes(Camera,FOV,&Rect);

	if (pPortalObj->SkyBox)
		grCamera_SetZScale(Camera, ZScale);

	return Ret;
}

grBoolean GRCC SendMsg(void * Instance, int32 Msg, void * Data)
{
	PortalObj *pPortalObj = (PortalObj*)Instance;

	switch( Msg)
	{
		case G3DEDITOR_GET_GRBRUSH:
		{
			grBrush **hBrush = (grBrush**)Data;

			assert(Brush);
			grBrush_SetXForm( Brush, &pPortalObj->Portal->XForm, GR_FALSE);
			*hBrush = Brush;
			return( GR_TRUE );
		}

		case 0:
		{
			PortalMsgData		*MData;
			
			MData = (PortalMsgData*)Data;

			if (MData->Frustum) {
				return RenderPortalObjectInstance2(	pPortalObj, 
													pPortalObj->Portal, 
													MData->Plane, 
													MData->FaceXForm, 
													MData->World, 
													MData->Camera, 
													MData->Frustum);
			} else {
				// could be change to a better suited function in next future
				return RenderPortalObjectInstance2(	pPortalObj, 
													pPortalObj->Portal, 
													MData->Plane, 
													MData->FaceXForm, 
													MData->World, 
													MData->Camera, 
													MData->Frustum);
			}
		}

	}
	return( GR_FALSE );
}

grBoolean GRCC PortalFrame( void *Instance, grFloat Time)
{
	PortalObj *pPortalObj = (PortalObj *)Instance;

	pPortalObj->Rotation += pPortalObj->RotateSpeed*0.01f;

	return GR_TRUE;
}

// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );
}

// Krouer
void GRCC SetRenderNextFlag(void *Instance, grBoolean NextFlag)
{
	PortalObj *pPortalObj = (PortalObj *)Instance;
	pPortalObj->RenderNextFlag = NextFlag;
}