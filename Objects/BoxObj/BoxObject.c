/****************************************************************************************/
/*  BOXOBJECT.C                                                                         */
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
#include <image.h>
#include <Resources.h>
#endif

#include <string.h>
#include <memory.h>
#include <assert.h>

#include "BoxObject.h"
#include "grTypes.h"
#include "grProperty.h"
#include "grUserPoly.h"
#include "Genesis3D.h"
#include "Ram.h"
#include "Bitmap.h"
#include "VFile.h"
#include "Resource.h"
#include "EditMsg.h"
#include "Errorlog.h"


#define BOXOBJ_VERSION 1 


enum {
	BOXOBJ_SIZE_ID = PROPERTY_LOCAL_DATATYPE_START,
	BOXOBJ_NAMLIST_ID
};

grBrush *	pBrush;

typedef struct BoxObj {
	grUserPoly	*Faces[6];
	grLVertex	Vertex[8];
	grXForm3d	XForm;
	float		Size;
	int			RefCnt;
} BoxObj;

enum {
	BOX_SIZE_INDEX,
	BOX_LAST_INDEX
};

grProperty BoxProperties[BOX_LAST_INDEX];
grProperty_List BoxPropertyList = { 1, &BoxProperties[0] };

char *NameList[3];

#define UTIL_MAX_RESOURCE_LENGTH	(128)
static char stringbuffer[UTIL_MAX_RESOURCE_LENGTH + 1];

#define DEFAULT_SIZE 16.0f


static grBoolean CreateGlobalBrush()
{
	pBrush = grBrush_Create( 8 );
	if( pBrush )
	{
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
		if( !grBrush_CreateFace( pBrush, 4 ) )
			goto CGB_ERROR;
	}
	return( GR_TRUE );
CGB_ERROR:
	grBrush_Destroy( &pBrush );
	pBrush = NULL;
	return( GR_FALSE );
}

static grBoolean UpdateGlobalBrush( BoxObj *pBoxObject )
{
	grBrush_Face *pFace;
	grVec3d		  Vertex;

	pFace = grBrush_GetNextFace( pBrush, NULL );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[3].X, pBoxObject->Vertex[3].Y, pBoxObject->Vertex[3].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[2].X, pBoxObject->Vertex[2].Y, pBoxObject->Vertex[2].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[1].X, pBoxObject->Vertex[1].Y, pBoxObject->Vertex[1].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[0].X, pBoxObject->Vertex[0].Y, pBoxObject->Vertex[0].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	pFace = grBrush_GetNextFace( pBrush, pFace );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[0].X, pBoxObject->Vertex[0].Y, pBoxObject->Vertex[0].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[1].X, pBoxObject->Vertex[1].Y, pBoxObject->Vertex[1].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[5].X, pBoxObject->Vertex[5].Y, pBoxObject->Vertex[5].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[4].X, pBoxObject->Vertex[4].Y, pBoxObject->Vertex[4].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	pFace = grBrush_GetNextFace( pBrush, pFace );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[1].X, pBoxObject->Vertex[1].Y, pBoxObject->Vertex[1].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[2].X, pBoxObject->Vertex[2].Y, pBoxObject->Vertex[2].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[6].X, pBoxObject->Vertex[6].Y, pBoxObject->Vertex[6].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[5].X, pBoxObject->Vertex[5].Y, pBoxObject->Vertex[5].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	pFace = grBrush_GetNextFace( pBrush, pFace );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[2].X, pBoxObject->Vertex[2].Y, pBoxObject->Vertex[2].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[3].X, pBoxObject->Vertex[3].Y, pBoxObject->Vertex[3].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[7].X, pBoxObject->Vertex[7].Y, pBoxObject->Vertex[7].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[6].X, pBoxObject->Vertex[6].Y, pBoxObject->Vertex[6].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	pFace = grBrush_GetNextFace( pBrush, pFace );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[3].X, pBoxObject->Vertex[3].Y, pBoxObject->Vertex[3].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[0].X, pBoxObject->Vertex[0].Y, pBoxObject->Vertex[0].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[4].X, pBoxObject->Vertex[4].Y, pBoxObject->Vertex[4].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[7].X, pBoxObject->Vertex[7].Y, pBoxObject->Vertex[7].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	pFace = grBrush_GetNextFace( pBrush, pFace );
	if( pFace == NULL )
		return(GR_FALSE );

	grVec3d_Set( &Vertex, pBoxObject->Vertex[4].X, pBoxObject->Vertex[4].Y, pBoxObject->Vertex[4].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 0, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[5].X, pBoxObject->Vertex[5].Y, pBoxObject->Vertex[5].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 1, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[6].X, pBoxObject->Vertex[6].Y, pBoxObject->Vertex[6].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 2, &Vertex);
	grVec3d_Set( &Vertex, pBoxObject->Vertex[7].X, pBoxObject->Vertex[7].Y, pBoxObject->Vertex[7].Z );
	grVec3d_Scale( &Vertex, pBoxObject->Size, &Vertex );
	grBrush_FaceSetVertByIndex( pFace, 3, &Vertex);

	grBrush_SetXForm( pBrush, &pBoxObject->XForm, GR_FALSE );
	return( GR_TRUE );
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

static grBoolean BoxObject_InitBox( BoxObj * pBoxObj )
{
	int i;
	assert( pBoxObj );
	assert( pBoxObj->Size > 0.0f );
	for( i = 0; i < 8; i++ )
	{
		pBoxObj->Vertex[i].r = 255.0f;
		pBoxObj->Vertex[i].g = 255.0f;
		pBoxObj->Vertex[i].b = 255.0f;
		pBoxObj->Vertex[i].a = 255.0f;
		pBoxObj->Vertex[i].u = 0.0f;
		pBoxObj->Vertex[i].v = 0.0f;
		pBoxObj->Vertex[i].sr = 255.0f;
		pBoxObj->Vertex[i].sg = 255.0f;
		pBoxObj->Vertex[i].sb = 255.0f;
	}

	pBoxObj->Vertex[0].X = 0.5f;
	pBoxObj->Vertex[0].Y = 0.5f;
	pBoxObj->Vertex[0].Z = 0.5f;

	pBoxObj->Vertex[1].X = 0.5f;
	pBoxObj->Vertex[1].Y = 0.5f;
	pBoxObj->Vertex[1].Z = -0.5f;

	pBoxObj->Vertex[2].X = -0.5f;
	pBoxObj->Vertex[2].Y = 0.5f;
	pBoxObj->Vertex[2].Z = -0.5f;

	pBoxObj->Vertex[3].X = -0.5f;
	pBoxObj->Vertex[3].Y = 0.5f;
	pBoxObj->Vertex[3].Z = 0.5f;


	pBoxObj->Vertex[4].X = 0.5f;
	pBoxObj->Vertex[4].Y = -0.5f;
	pBoxObj->Vertex[4].Z = 0.5f;

	pBoxObj->Vertex[5].X = 0.5f;
	pBoxObj->Vertex[5].Y = -0.5f;
	pBoxObj->Vertex[5].Z = -0.5f;

	pBoxObj->Vertex[6].X = -0.5f;
	pBoxObj->Vertex[6].Y = -0.5f;
	pBoxObj->Vertex[6].Z = -0.5f;

	pBoxObj->Vertex[7].X = -0.5f;
	pBoxObj->Vertex[7].Y = -0.5f;
	pBoxObj->Vertex[7].Z = 0.5f;


	memset( pBoxObj->Faces, sizeof( grUserPoly	*) * 6, 0 );
	pBoxObj->Faces[0] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[3], 
												&pBoxObj->Vertex[2],
												&pBoxObj->Vertex[1],
												&pBoxObj->Vertex[0] ,
												NULL,
												0 );
	if( pBoxObj->Faces[0] == NULL )
		goto INITBOX_ERR;

	pBoxObj->Faces[1] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[0], 
												&pBoxObj->Vertex[1],
												&pBoxObj->Vertex[5],
												&pBoxObj->Vertex[4] ,
												NULL,
												0 );
	if( pBoxObj->Faces[1] == NULL )
		goto INITBOX_ERR;

	pBoxObj->Faces[2] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[1], 
												&pBoxObj->Vertex[2],
												&pBoxObj->Vertex[6],
												&pBoxObj->Vertex[5] ,
												NULL,
												0 );
	if( pBoxObj->Faces[2] == NULL )
		goto INITBOX_ERR;

	pBoxObj->Faces[3] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[2], 
												&pBoxObj->Vertex[3],
												&pBoxObj->Vertex[7],
												&pBoxObj->Vertex[6] ,
												NULL,
												0 );
	if( pBoxObj->Faces[3] == NULL )
		goto INITBOX_ERR;

	pBoxObj->Faces[4] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[3], 
												&pBoxObj->Vertex[0],
												&pBoxObj->Vertex[4],
												&pBoxObj->Vertex[7] ,
												NULL,
												0 );
	if( pBoxObj->Faces[4] == NULL )
		goto INITBOX_ERR;

	pBoxObj->Faces[5] = grUserPoly_CreateQuad(	&pBoxObj->Vertex[4], 
												&pBoxObj->Vertex[5],
												&pBoxObj->Vertex[6],
												&pBoxObj->Vertex[7] ,
												NULL,
												0 );
	if( pBoxObj->Faces[5] == NULL )
		goto INITBOX_ERR;

	return( GR_TRUE );

INITBOX_ERR:
	for( i = 0; i < 6; i++ )
		if( pBoxObj->Faces[i] != NULL )
			grUserPoly_Destroy(&pBoxObj->Faces[i]);
	return( GR_FALSE );
}				

static void BoxObject_TransformVert( grLVertex * pVertex, grXForm3d * pXForm )
{
	grVec3d Point;
	assert( pVertex );
	assert( pXForm );

	//I guess for speed I could cheat and recognize that the structure from
	//pVertex->X matches Vec3d Struct.
	Point.X = pVertex->X;
	Point.Y = pVertex->Y;
	Point.Z = pVertex->Z;

	grXForm3d_Transform( pXForm, &Point, &Point );

	pVertex->X = Point.X;
	pVertex->Y = Point.Y;
	pVertex->Z = Point.Z;
}

static grBoolean BoxObject_UpdateFaces( BoxObj * pBoxObj )
{

	grLVertex	Vertex[4];
	grXForm3d	ScaleXForm;

	grXForm3d_SetIdentity(&ScaleXForm);
	grXForm3d_Scale(&ScaleXForm, pBoxObj->Size, pBoxObj->Size, pBoxObj->Size);

	Vertex[0] = pBoxObj->Vertex[3];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[2];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[1];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[0];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[0],	
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	Vertex[0] = pBoxObj->Vertex[0];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[1];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[5];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[4];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[1],	
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	Vertex[0] = pBoxObj->Vertex[1];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[2];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[6];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[5];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[2],	
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	Vertex[0] = pBoxObj->Vertex[2];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[3];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[7];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[6];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[3],	
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	Vertex[0] = pBoxObj->Vertex[3];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[0];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[4];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[7];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[4],
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	Vertex[0] = pBoxObj->Vertex[4];
	BoxObject_TransformVert( &Vertex[0], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[0], &pBoxObj->XForm );
	Vertex[1] = pBoxObj->Vertex[5];
	BoxObject_TransformVert( &Vertex[1], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[1], &pBoxObj->XForm );
	Vertex[2] = pBoxObj->Vertex[6];
	BoxObject_TransformVert( &Vertex[2], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[2], &pBoxObj->XForm );
	Vertex[3] = pBoxObj->Vertex[7];
	BoxObject_TransformVert( &Vertex[3], &ScaleXForm );
	BoxObject_TransformVert( &Vertex[3], &pBoxObj->XForm );

	 if( !grUserPoly_UpdateQuad(				pBoxObj->Faces[5],	
												&Vertex[0], 
												&Vertex[1],
												&Vertex[2],
												&Vertex[3] ,
												NULL ) )
	 {
		 return( GR_FALSE );
	 }

	return( GR_TRUE );

}				

#ifdef WIN32
void Init_Class( HINSTANCE hInstance )
#endif
#ifdef BUILD_BE
void Init_Class( image_id hInstance )
#endif
{
	BoxProperties[BOX_SIZE_INDEX].Type = PROPERTY_FLOAT_TYPE;
	BoxProperties[BOX_SIZE_INDEX].bDisabled = GR_FALSE;
	BoxProperties[BOX_SIZE_INDEX].Data.Float = 16.0f;
	BoxProperties[BOX_SIZE_INDEX].DataId = BOXOBJ_SIZE_ID;
	BoxProperties[BOX_SIZE_INDEX].DataSize = sizeof( float );
	BoxProperties[BOX_SIZE_INDEX].FieldName = Util_LoadLibraryString(hInstance, IDS_SIZE );
    BoxProperties[BOX_SIZE_INDEX].TypeInfo.NumInfo.Min = 1.0f;
	BoxProperties[BOX_SIZE_INDEX].TypeInfo.NumInfo.Max = 64.0f;
	BoxProperties[BOX_SIZE_INDEX].TypeInfo.NumInfo.Increment = 1.0f;

}



void * GRCC CreateInstance( void )
{
	BoxObj *pBoxObj;

	pBoxObj = GR_RAM_ALLOCATE_STRUCT( BoxObj );
	if( pBoxObj == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "CreateInstance");
		return( NULL );
	}
	grXForm3d_SetIdentity( &pBoxObj->XForm );
	pBoxObj->Size = DEFAULT_SIZE;
	pBoxObj->RefCnt = 1;
	if( !BoxObject_InitBox( pBoxObj ) )
	{
		grRam_Free( pBoxObj );
		return( NULL );
	}
	return( pBoxObj );

}

void * GRCC DuplicateInstance(void * Instance)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;
	BoxObj *pNewBoxObj;

	pNewBoxObj = (BoxObj *)CreateInstance( );
	if( pNewBoxObj == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "DuplicateInstance:CreateInstance");
		return( NULL );
	}
	SetXForm( pNewBoxObj, &pBoxObj->XForm );
	pNewBoxObj->Size = pBoxObj->Size;
	if( !BoxObject_UpdateFaces( pNewBoxObj ) )
	{
		Destroy( (void **)&pNewBoxObj );
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "DuplicateInstance:BoxObject_UpdateFaces");
		return( NULL );
	}
	return( pNewBoxObj );
}

void GRCC CreateRef(void * Instance)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );

	pBoxObj->RefCnt++;
}

grBoolean GRCC Destroy(void **pInstance)
{
	int i;
	BoxObj **hBoxObj = (BoxObj**)pInstance;
	BoxObj *pBoxObj = *hBoxObj;

	assert( pInstance );
	assert( pBoxObj->RefCnt > 0 );

	pBoxObj->RefCnt--;
	if( pBoxObj->RefCnt == 0 )
	{
		for( i = 0; i < 6; i++ )
			if( pBoxObj->Faces[i] != NULL )
				grUserPoly_Destroy(&pBoxObj->Faces[i]);
		grRam_Free( pBoxObj );
	}
	else
		return( GR_FALSE );
	return( GR_TRUE );
}


grBoolean GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{

	return( GR_TRUE );

}

grBoolean	GRCC AttachWorld( void * Instance, grWorld * pWorld )
{
	int i;
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );
	for( i = 0; i < 6; i++ )
		if( !grWorld_AddUserPoly(pWorld ,pBoxObj->Faces[i], GR_FALSE) )
			return( GR_FALSE );
	return( GR_TRUE );
}

grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld )
{
	int i;
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );
	for( i = 0; i < 6; i++ )
		if( !grWorld_RemoveUserPoly(pWorld ,pBoxObj->Faces[i]) )
			return( GR_FALSE );
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

grBoolean	GRCC Collision(const void * Instance, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{
	grExtBox BBox;
	grVec3d  Normal;
	grFloat		T;
	
	GetExtBox(Instance,&BBox);
	if (Impact)
	{
		if( grExtBox_RayCollision( &BBox, Front, Back, &T, &Normal ) )
		{
			grVec3d_Subtract( Back, Front, Impact );
			grVec3d_Scale( Impact, T, Impact );
			grVec3d_Add( Back, Impact, Impact );
			return( GR_TRUE );
		}
		return( GR_FALSE );
	} else
		return grExtBox_RayCollision( &BBox, Front, Back, NULL, NULL );
}


grBoolean GRCC GetExtBox(const void * Instance,grExtBox *BBox)
{
	grVec3d Point;
	BoxObj *pBoxObj = (BoxObj*)Instance;
	int i;
	grXForm3d	ScaleXForm;


	assert( Instance );
	assert( BBox );

	grXForm3d_SetIdentity(&ScaleXForm);
	grXForm3d_Scale(&ScaleXForm, pBoxObj->Size, pBoxObj->Size, pBoxObj->Size);

	Point.X = pBoxObj->Vertex[0].X;
	Point.Y = pBoxObj->Vertex[0].Y;
	Point.Z = pBoxObj->Vertex[0].Z;
	grXForm3d_Transform( &ScaleXForm, &Point, &Point );
	grXForm3d_Transform( &pBoxObj->XForm, &Point, &Point );

	grExtBox_SetToPoint ( BBox, &Point );
	for( i = 1; i < 8 ; i ++ )
	{
		Point.X = pBoxObj->Vertex[i].X;
		Point.Y = pBoxObj->Vertex[i].Y;
		Point.Z = pBoxObj->Vertex[i].Z;
		grXForm3d_Transform( &ScaleXForm, &Point, &Point );
		grXForm3d_Transform( &pBoxObj->XForm, &Point, &Point );
		grExtBox_ExtendToEnclose( BBox, &Point );
	}
	return( GR_TRUE );
}


void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	BoxObj * pBoxObj;
	BYTE Version;
    uint32 Tag;

	pBoxObj = GR_RAM_ALLOCATE_STRUCT( BoxObj );
	if( pBoxObj == NULL )
		return( NULL );

 	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "BoxObject_CreateFromFile:Tag" );
		goto CFF_ERROR;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "BoxObject_CreateFromFile:Version" );
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
		if( !grVFile_Read(	File, &pBoxObj->XForm, sizeof( pBoxObj->XForm) ) )
		{
	   	    grErrorLog_Add(GR_ERR_FILEIO_READ, "BoxObject_CreateFromFile:XForm");
        	goto CFF_ERROR;
		}

	    if( !grVFile_Read(	File, &pBoxObj->Size, sizeof( pBoxObj->Size) ) )
		{
	   	    grErrorLog_Add(GR_ERR_FILEIO_READ, "BoxObject_CreateFromFile:Size");
		    goto CFF_ERROR;
		}
	}

	pBoxObj->RefCnt = 1;
	if( !BoxObject_InitBox( pBoxObj ) )
		goto CFF_ERROR;
	if( !BoxObject_UpdateFaces( pBoxObj ) )
		goto CFF_ERROR;

	return( pBoxObj );

CFF_ERROR:

	grRam_Free( pBoxObj );
	return( NULL );
}


grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *PtrMgr)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;
	BYTE Version = BOXOBJ_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	assert( Instance );

	if(!grVFile_Write(File,&Tag, sizeof(Tag)))
	{
	   	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "BoxObject_WriteToFile:Tag");
		return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &Version, sizeof(Version) ) )
	{
	   	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "BoxObject_WriteToFile:Version");
		return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pBoxObj->XForm, sizeof( pBoxObj->XForm) ) )
	{
		grErrorLog_Add(GR_ERR_FILEIO_WRITE, "BoxObject_WriteToFile:XForm");
		return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pBoxObj->Size, sizeof( pBoxObj->Size) ) )
	{
		grErrorLog_Add(GR_ERR_FILEIO_WRITE, "BoxObject_WriteToFile:Size");
		return( GR_FALSE );
	}


	return( GR_TRUE );
}


grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );

	BoxProperties[BOX_SIZE_INDEX].Data.Float = pBoxObj->Size;

	*List = grProperty_ListCopy( &BoxPropertyList);
	if( *List == NULL )
		return( GR_FALSE );
	return( GR_TRUE );
}

grBoolean	GRCC SetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );
	if( FieldID == BOXOBJ_SIZE_ID )
	{
		pBoxObj->Size = pData->Float;
		BoxObject_UpdateFaces( pBoxObj );
	}
	return( GR_TRUE );
}

grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );

	pBoxObj->XForm = *XF;
	if( !BoxObject_UpdateFaces( pBoxObj ) )
		return GR_FALSE;
	return( GR_TRUE );
}

grBoolean GRCC GetXForm(const void * Instance,grXForm3d *XF)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	assert( Instance );
	*XF = pBoxObj->XForm;
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
grBoolean GRCC EditDialog (void * Instance, class G3DView *Parent)
#endif
{
	return( GR_TRUE );
}

grBoolean GRCC SendMsg(void * Instance, int32 Msg, void * Data)
{
	BoxObj *pBoxObj = (BoxObj*)Instance;

	if( Msg == G3DEDITOR_GET_GRBRUSH )
	{
		grBrush **hBrush = (grBrush**)Data;
		if( pBrush == NULL )
			if( !CreateGlobalBrush() )
				return(GR_FALSE);
		if( !UpdateGlobalBrush( pBoxObj ) )
			return(GR_FALSE );
		*hBrush = pBrush;
		return( GR_TRUE );
	}
	return( GR_FALSE );
}

// Icestorm: Collision ignores Box=>ChangeBoxCollision ignores all
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );
}