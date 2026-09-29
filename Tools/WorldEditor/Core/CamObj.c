/****************************************************************************************/
/*  CAMOBJ.C                                                                            */
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
#include <float.h>
#include "Ram.h"
#include "Errorlog.h"
#include "Util.h"
#include "../resource.h"
#include "XForm3d.h"
#include "CamObj.h"
#include "ObjectDef.h"
#include "units.h"
#define SIGNATURE			'CAMR'
#include "Brush.h"
#include "EditMsg.h"
#include "CamFieldID.h"


typedef struct tagCamera
{
	Object				ObjectData ;
#ifdef _DEBUG
	int					nSignature ;
#endif
	float				XRotation;
	float				YRotation;
	int32				Flags;
	Brush				* pCamBrush;
	grObject			*pgeObject;
	grExtBox			WorldBounds;
} Camera ;

#define CAMERA_SIZE	 16
#define CAMERA_FLAG_DIRTY			0x0001
#define CAMERA_FLAG_WBOUNDSDIRTY		0x0002
#define CAMERA_FLAG_DIRTYALL			CAMERA_FLAG_DIRTY | CAMERA_FLAG_WBOUNDSDIRTY
#define CAMERA_MAXNAMELENGTH	(31)

Camera * Camera_Create( const char * const pszName, Group * pGroup, int32 nNumber) 
{
	Camera	*	pCamera;
	grBrush	* pgeBrush;
	char * Name;

	assert( pszName );
	pCamera = GR_RAM_ALLOCATE_STRUCT( Camera );
	if( pCamera == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Camera" );
		return( NULL );
	}
	memset( pCamera, 0, sizeof( Camera ) );
	assert( (pCamera->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	if( !Object_Init( &pCamera->ObjectData, pGroup, KIND_CAMERA, pszName, nNumber ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Camera_Create:Object_Init" );
		grRam_Free( pCamera );
		return( NULL );
	}
	pCamera->pgeObject = grObject_Create( "Camera" );
	if( pCamera->pgeObject  == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Camera_Create:grObject_Create" );
		grRam_Free( pCamera );
		return( NULL );
	}
	if( grObject_SendMessage( pCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
		pCamera->pCamBrush = Brush_Create( pszName, NULL, 0 );

	Name = Object_GetNameAndTag( &pCamera->ObjectData );
	if( Name )
	{
		grObject_SetName( pCamera->pgeObject, Name );
		grRam_Free( Name );
	}
	pCamera->XRotation = 0.0f;
	pCamera->YRotation = 0.0f;

	pCamera->Flags |= CAMERA_FLAG_WBOUNDSDIRTY;

	return( pCamera );
}// Camera_Create

Camera *	Camera_Copy( Camera *	pCamera, int32 nNumber )
{
	Camera *pNewCamera;
	grBrush	* pgeBrush;
	char * Name;

	pNewCamera = GR_RAM_ALLOCATE_STRUCT( Camera );
	if( pNewCamera == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Camera" );
		return( NULL );
	}
	memset( pNewCamera, 0, sizeof( Camera ) );
	assert( (pNewCamera->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	if( !Object_Init( &pNewCamera->ObjectData, pCamera->ObjectData.pGroup, KIND_CAMERA, pCamera->ObjectData.pszName, nNumber ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Camera_Copy:Object_Init" );
		grRam_Free( pCamera );
		return( NULL );
	}
	pNewCamera->pgeObject = grObject_Duplicate( pCamera->pgeObject );
	if( pNewCamera->pgeObject  == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Camera_Copy:grObject_Create" );
		grRam_Free( pCamera );
		return( NULL );
	}
	if( grObject_SendMessage( pNewCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
		pNewCamera->pCamBrush = Brush_Create( pCamera->ObjectData.pszName, NULL, 0 );
	Name = Object_GetNameAndTag( &pCamera->ObjectData );
	if( Name )
	{
		grObject_SetName( pCamera->pgeObject, Name );
		grRam_Free( Name );
	}
	pNewCamera->Flags |= CAMERA_FLAG_WBOUNDSDIRTY;
	pNewCamera->XRotation = pCamera->XRotation;
	pNewCamera->YRotation = pCamera->YRotation;
	return( pNewCamera );
}


char *	Camera_CreateDefaultName(  )
{
	return( Util_LoadLocalRcString( IDS_CAMERA ) );
}

void Camera_Destroy( Camera ** ppCamera ) 
{
	assert( ppCamera );
	assert( *ppCamera );
	if( (*ppCamera)->pgeObject )
		grObject_Destroy( &(*ppCamera)->pgeObject );
	if( (*ppCamera)->pCamBrush )
	{
		Brush_SetGeBrush( (*ppCamera)->pCamBrush, KIND_BRUSH, NULL );
		Brush_Destroy( &(*ppCamera)->pCamBrush );
	}
	grRam_Free( (*ppCamera) );
}// Camera_Destroy


// MODIFIERS
grBoolean Camera_Move( Camera * pCamera, const grVec3d * pWorldDistance )
{
	grXForm3d XF;
	assert( pCamera != NULL ) ;
	assert( pCamera->pgeObject );
	assert( SIGNATURE == pCamera->nSignature ) ;

	grObject_GetXForm( pCamera->pgeObject, &XF );

	Camera_SetModified( pCamera );
	grVec3d_Add( &XF.Translation, pWorldDistance, &XF.Translation );
	grObject_SetXForm( pCamera->pgeObject, &XF );

	return( GR_TRUE );

}// Camera_Move

void Camera_Rotate( Camera * pCamera, ORTHO_AXIS RAxis, grFloat RadianAngle, const grVec3d * pRotationCenter )
{
	grXForm3d	XForm ;
	grXForm3d	XRot_XForm ;
	grXForm3d	CamXForm;
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	switch( RAxis )
	{
	case Ortho_Axis_X :
		pCamera->XRotation -=  RadianAngle;
		pCamera->XRotation = (float)fmod( pCamera->XRotation, (2*M_PI));
		break;

	case Ortho_Axis_Z :
		pCamera->XRotation +=  RadianAngle;
		pCamera->XRotation = (float)fmod( pCamera->XRotation, (2*M_PI));
		break;

	case Ortho_Axis_Y :
		pCamera->YRotation +=  RadianAngle;
		pCamera->YRotation = (float)fmod( pCamera->YRotation, (2*M_PI));
		break ;
	}
	grXForm3d_SetYRotation( &XForm, pCamera->YRotation );
	grXForm3d_SetXRotation( &XRot_XForm, pCamera->XRotation );
	grXForm3d_Multiply( &XForm, &XRot_XForm, &XForm );
	Camera_GetXForm( pCamera, &CamXForm );
	grXForm3d_Translate( &XForm, CamXForm.Translation.X, CamXForm.Translation.Y, CamXForm.Translation.Z ) ; 

	Camera_SetXForm( pCamera, &XForm) ;
	pRotationCenter;
}// Camera_Rotate

static grBoolean Camera_SizeEdge( Camera * pCamera, const grVec3d * pStillEdge, const grFloat fScale, ORTHO_AXIS Axis )
{
	float	fTemp;
	grXForm3d	CamXForm;

	Camera_GetXForm( pCamera, &CamXForm );
	fTemp = grVec3d_GetElement( &CamXForm.Translation, Axis ) - grVec3d_GetElement( pStillEdge, Axis ) ;
	fTemp = fTemp * fScale ;
	fTemp = fTemp + grVec3d_GetElement( pStillEdge, Axis ) ;
	grVec3d_SetElement( &CamXForm.Translation, Axis, fTemp ) ;
	Camera_SetXForm( pCamera, &CamXForm );
	return( GR_TRUE );
}

grBoolean Camera_Size( Camera * pCamera, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis )
{
	grBoolean bResult = GR_TRUE;

	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;

	// The Z axis is flipped (down is positive)
	// We just determine the edge and call _SizeEdge once or twice to keep this
	// as simple as possible
	switch( eSizeType )
	{
	case Select_Top :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		break ;

	case Select_Bottom :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		break ;

	case Select_Left :
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;

	case Select_Right :
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_TopLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;		

	case Select_TopRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_BottomLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;
	
	case Select_BottomRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		bResult = Camera_SizeEdge( pCamera, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;
	}
	Camera_SetModified( pCamera ) ;
	if( bResult == GR_FALSE )
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Camera_Size:Camera_SizeEdge" );

	return( bResult );
}// Camera_Size

grBoolean Camera_SetXForm( Camera * pCamera, const grXForm3d * XForm )
{

	assert( pCamera );
	assert( SIGNATURE == pCamera->nSignature ) ;
	assert( XForm );

	grObject_SetXForm( pCamera->pgeObject, XForm );
	Camera_SetModified( pCamera );
	return( GR_TRUE );
}// Camera_SetXForm


void Camera_UpdateBounds( Camera * pCamera )
{

	assert( pCamera );

	grObject_GetExtBox( pCamera->pgeObject, &pCamera->WorldBounds );

}

void Camera_SetModified( Camera * pCamera )
{
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	pCamera->Flags |= CAMERA_FLAG_DIRTYALL ;	
}// Camera_SetModified




// ACCESSORS
void Camera_GetXForm( const Camera * pCamera, grXForm3d * XForm )
{
	grObject_GetXForm( pCamera->pgeObject, XForm );
}

const grExtBox * Camera_GetWorldAxialBounds( const Camera * pCamera )
{
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	if( pCamera->Flags & CAMERA_FLAG_WBOUNDSDIRTY )
	{	
		Camera * pEvalCamera = (Camera*)pCamera ;			// Lazy Evaluation requires removing the const
		Camera_UpdateBounds( pEvalCamera ) ;
	}

	return &pCamera->WorldBounds ;

}// Camera_GetWorldAxialBounds

void Camera_GetWorldDrawBounds( const Camera * pCamera, grExtBox *DrawBounds )
{
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	if( pCamera->Flags & CAMERA_FLAG_WBOUNDSDIRTY )
	{	
		Camera * pEvalCamera = (Camera*)pCamera ;			// Lazy Evaluation requires removing the const
		Camera_UpdateBounds( pEvalCamera ) ;
	}
	*DrawBounds = pCamera->WorldBounds;

}// Camera_GetWorldDrawBounds


grBoolean Camera_SelectClosest( Camera * pCamera, FindInfo *	pFindInfo )
{
	grBrush *pgeBrush;
	if( pCamera->pCamBrush )
	{
		if( !grObject_SendMessage( pCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
			return( GR_TRUE );
		Brush_SetGeBrush( pCamera->pCamBrush, 0, pgeBrush );
		Brush_SelectClosest( pCamera->pCamBrush, pFindInfo );
		if( pFindInfo->pObject == (Object*)pCamera->pCamBrush )
			pFindInfo->pObject = (Object*)pCamera;
	}
	return(  GR_TRUE );
}

grBoolean Camera_FillPositionDescriptor( Camera * pCamera, grProperty_List * pArray )
{
	grXForm3d XForm;
	char * Name;

	grProperty Property;
	Camera_GetXForm( pCamera, &XForm );

	Name = Util_LoadLocalRcString( IDS_POSITION_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillVec3dGroup( &Property, Name, &XForm.Translation,	OBJECT_POSITION_FIELD  );
	if( !grProperty_Append( pArray,  &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONX_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat(  &Property, Name, XForm.Translation.X, OBJECT_POSITION_FIELDX, -FLT_MAX, FLT_MAX, 1.0f );
	if( !grProperty_Append( pArray,  &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONY_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat(  &Property, Name, XForm.Translation.Y,	OBJECT_POSITION_FIELDY, -FLT_MAX, FLT_MAX, 1.0f );
	if( !grProperty_Append( pArray, &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONZ_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat( &Property, Name, XForm.Translation.Z, OBJECT_POSITION_FIELDZ, -FLT_MAX, FLT_MAX, 1.0f );
	if( !grProperty_Append( pArray, &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	grProperty_FillGroupEnd( &Property, OBJECT_POSITION_FIELD_END );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

grProperty_List *	Camera_BuildDescriptor( Camera * pCamera )
{
	grProperty_List * pPropertyArray = NULL;
	grProperty_List * pObjectArray;
	grProperty_List * pArray = NULL;
	grProperty		  Property;
	char		*	  Name;


	pObjectArray = grProperty_ListCreateEmpty();

	Name = Util_LoadLocalRcString( IDS_NAME_FIELD );
	if( Name == NULL )
		goto UOBD_ERROR;
	grProperty_FillString( &Property, Name, pCamera->ObjectData.pszName, OBJECT_POSITION_FIELD );
	grRam_Free( Name );
	if( !grProperty_Append( pObjectArray,  &Property ) )
	{
		grRam_Free( Name );
		return( NULL );
	}
	
	if( !Camera_FillPositionDescriptor( pCamera, pObjectArray ) )
		goto UOBD_ERROR;

	if( !grObject_GetPropertyList(pCamera->pgeObject, &pPropertyArray) )
		goto UOBD_ERROR;


	 pArray = grProperty_ListConCat( pObjectArray, pPropertyArray );
	 if( pArray == NULL )
		 goto UOBD_ERROR;

	grProperty_ListDestroy( &pObjectArray );
	grProperty_ListDestroy( &pPropertyArray );

	 return( pArray );
UOBD_ERROR:
	 if( pObjectArray )
		 grProperty_ListDestroy( &pObjectArray );

	 if( pPropertyArray )
		 grProperty_ListDestroy( &pPropertyArray );

	 if( pArray )
		 grProperty_ListDestroy( &pArray );
	 return( NULL );
}


//IS
grBoolean	Camera_IsInRect( const Camera * pCamera, grExtBox *pSelRect, grBoolean bSelEncompeses )
{
	const grExtBox *pWorldBounds;
	grExtBox		Result;

	assert( pCamera );
	assert( pSelRect );
	
	pWorldBounds = Camera_GetWorldAxialBounds( pCamera ) ;
	if( bSelEncompeses )
	{
		if( pSelRect->Max.X >= pWorldBounds->Max.X &&
			pSelRect->Max.Y >= pWorldBounds->Max.Y &&
			pSelRect->Max.Z >= pWorldBounds->Max.Z &&
			pSelRect->Min.X <= pWorldBounds->Min.X &&
			pSelRect->Min.Y <= pWorldBounds->Min.Y &&
			pSelRect->Min.Z <= pWorldBounds->Min.Z )
			 return( GR_TRUE );
	}
	else
	{
		return( Util_geExtBox_Intersection ( pSelRect, pWorldBounds, &Result	) );
	}
	return( GR_FALSE );
}//Camera_IsInRect

grBoolean Camera_TranslateCurCam( Camera * pCamera, grVec3d * Offset )
{
	grXForm3d	XForm ;
	grXForm3d	CamXForm;

	Camera_GetXForm( pCamera, &CamXForm );
	XForm = CamXForm;
	grVec3d_Set( &XForm.Translation, 0.0f, 0.0f, 0.0f );

	grXForm3d_Transform(&XForm, Offset, Offset );
	grVec3d_Add( Offset, &CamXForm.Translation, &XForm.Translation );

	Camera_SetXForm( pCamera, &XForm) ;
	Camera_SetModified( pCamera ) ;
	return( GR_TRUE );
}

grBoolean Camera_RotCurCamY( Camera * pCamera, float Radians )
{
	grXForm3d	CamXForm;
	grXForm3d	XForm ;
	grXForm3d	XRot_XForm ;
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	Camera_GetXForm( pCamera, &CamXForm );
	pCamera->YRotation += Radians;
	grXForm3d_SetYRotation( &XForm, pCamera->YRotation );
	grXForm3d_SetXRotation( &XRot_XForm, pCamera->XRotation );
	grXForm3d_Multiply( &XForm, &XRot_XForm, &XForm );
	grXForm3d_Translate( &XForm, CamXForm.Translation.X, CamXForm.Translation.Y, CamXForm.Translation.Z ) ; 

	Camera_SetXForm( pCamera, &XForm) ;
	Camera_SetModified( pCamera ) ;
	return( GR_TRUE );
}

grBoolean Camera_RotCurCamX( Camera * pCamera, float Radians )
{
	grXForm3d	XForm ;
	grXForm3d	XRot_XForm ;
	grXForm3d	CamXForm;

	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	
	Camera_GetXForm( pCamera, &CamXForm );
	pCamera->XRotation += Radians;
	grXForm3d_SetYRotation( &XForm, pCamera->YRotation );
	grXForm3d_SetXRotation( &XRot_XForm, pCamera->XRotation );
	grXForm3d_Multiply( &XForm, &XRot_XForm, &XForm );
	grXForm3d_Translate( &XForm, CamXForm.Translation.X, CamXForm.Translation.Y, CamXForm.Translation.Z ) ; 

	Camera_SetXForm( pCamera, &XForm) ;
	Camera_SetModified( pCamera ) ;
	return( GR_TRUE );
}


Camera * Camera_CreateFromFile( grVFile * pF, grPtrMgr *PtrMgr )
{
	Camera	*	pCamera = NULL ;
	grBrush * pgeBrush;

	assert( grVFile_IsValid( pF ) ) ;

	pCamera = GR_RAM_ALLOCATE_STRUCT( Camera );
	if( pCamera == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Camera" );
		return( NULL );
	}
	memset( pCamera, 0, sizeof( Camera ) );
	assert( (pCamera->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN

	if( !Object_InitFromFile( pF , &pCamera->ObjectData ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Object_InitFromFile.\n", NULL);
		return NULL;
	}

	pCamera->pgeObject = grObject_CreateFromFile( pF, PtrMgr );
	if( !pCamera->pgeObject )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Camera_ReadFromFile.\n", NULL);
		return NULL;
	}

	if( !grVFile_Read(	pF, &pCamera->XRotation, sizeof( pCamera->XRotation) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "CreateFromFile:XRotation" );
		return NULL;
	}
	if( !grVFile_Read(	pF, &pCamera->YRotation, sizeof( pCamera->YRotation) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "CreateFromFile:YRotation" );
		return NULL;
	}

	pCamera->Flags |= CAMERA_FLAG_DIRTYALL ;
	if( grObject_SendMessage( pCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
		pCamera->pCamBrush = Brush_Create( pCamera->ObjectData.pszName, NULL, 0 ) ;
	Camera_UpdateBounds( pCamera );
	Object_SetInLevel( (Object*)pCamera, GR_TRUE );
	return( pCamera );
}


grBoolean Camera_WriteToFile( Camera * pCamera, grVFile * pF, grPtrMgr *PtrMgr )
{
	assert( pCamera != NULL ) ;
	assert( SIGNATURE == pCamera->nSignature ) ;
	assert( grVFile_IsValid( pF ) ) ;

	if( !Object_WriteToFile( &pCamera->ObjectData, pF ) )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Object_WriteToFile.", NULL);
		return GR_FALSE;
	}

	if( !grObject_WriteToFile( pCamera->pgeObject, pF, PtrMgr ) )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Object_WriteToFile.", NULL);
		return GR_FALSE;
	}

	if( !grVFile_Write(	pF, &pCamera->XRotation, sizeof( pCamera->XRotation) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "Camera_WriteToFile:XRotation" );
		return( GR_FALSE );
	}
	if( !grVFile_Write(	pF, &pCamera->YRotation, sizeof( pCamera->YRotation) ) )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, "Camera_WriteToFile:YRotation" );
		return( GR_FALSE );
	}

	return GR_TRUE ;

}// Camera_WriteToFile


//PRESENTATION
void Camera_RenderOrtho( const Ortho * pOrtho, Camera *pCamera, int32 hDC, grBoolean bColorOveride)
{
/*	grBrush * pgeBrush;
	jwePen  * pPen = NULL;

	if( ! bColorOveride )
	{
		pPen = Pen_SelectColor( hDC, 0, 255, 0);
	}
	if( !pCamera->pCamBrush )
		return;
	if( !grObject_SendMessage( pCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
		return;
	Brush_SetGeBrush( pCamera->pCamBrush, 0, pgeBrush );

	Brush_RenderOrthoFaces(  pCamera->pCamBrush, pOrtho, hDC, GR_FALSE, GR_FALSE, GR_TRUE );
	if( pPen )
		Pen_Release( pPen, hDC );
*/

	//	by TOM
	grBrush * pgeBrush = NULL;
	jwePen  * pPen = NULL;

	if( ! bColorOveride )
	{
		pPen = Pen_SelectColor( hDC, 0, 255, 0);
	}
	if( !pCamera->pCamBrush )
		return;
	grObject_SendMessage( pCamera->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush );

	if (!pgeBrush)
	{
		return;
	}
	Brush_SetGeBrush( pCamera->pCamBrush, 0, pgeBrush );

	Brush_RenderOrthoFaces(  pCamera->pCamBrush, pOrtho, hDC, GR_FALSE, GR_FALSE, GR_TRUE );
	if( pPen )
		Pen_Release( pPen, hDC );
}

grObject *	Camera_GetgrObject( Camera * pCamera )
{
	assert( pCamera );

	return( pCamera->pgeObject );
}

float	Camera_GetFOV( Camera * pCamera )
{
	float FOV = 1.0f;

	assert( pCamera );
	assert( pCamera->pgeObject );

	grObject_GetProperty( pCamera->pgeObject, CAMREA_FOV_ID, PROPERTY_FLOAT_TYPE, (grProperty_Data*)&FOV );
	return( FOV );
}

float Camera_GetCurCamY( const Camera * pCamera )
{
	assert( pCamera );

	return( pCamera->YRotation );
}

float Camera_GetCurCamX( const Camera * pCamera )
{
	assert( pCamera );

	return( pCamera->XRotation );
}

void Camera_SetCurCamY( Camera * pCamera, float YRot )
{
	assert( pCamera );

	pCamera->YRotation = YRot;
}

void Camera_SetCurCamX( Camera * pCamera, float XRot )
{
	assert( pCamera );

	pCamera->XRotation  = XRot;
}

void	Camera_SetProperty( Camera * pCamera, int DataId, int DataType, grProperty_Data * pData, grBoolean bUpdate )
{
	assert( pCamera );
	assert( pCamera->pgeObject );

	bUpdate;
	grObject_SetProperty( pCamera->pgeObject, DataId, DataType,pData );
}

