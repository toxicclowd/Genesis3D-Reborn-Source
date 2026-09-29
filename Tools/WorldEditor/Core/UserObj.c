/****************************************************************************************/
/*  USEROBJ.C                                                                           */
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

#include "ErrorLog.h"
#include "Genesis3D.h"
#include "Ram.h"
#include "Transform.h"
#include "Util.h"
#include "../resource.h"
#include "Point.h"
#include "Brush.h"
#include "EditMsg.h"

#pragma warning(disable : 4201 4214 4115)
#include <Windows.h>
#include <Windowsx.h>
#pragma warning(default : 4201 4214 4115; disable : 4514)

#include "UserObj.h"
#include "ObjectDef.h"
#define SIGNATURE			'UOBJ'

#define USEROBJ_BOX_MIN				-0.1f
#define USEROBJ_BOX_MAX				0.1f
#define USEROBJ_DRAW_MIN				-3.0f
#define USEROBJ_DRAW_MAX				3.0f
#define USEROBJ_FLAG_DIRTY			0x0001
#define USEROBJ_FLAG_WBOUNDSDIRTY		0x0002
#define USEROBJ_FLAG_DIRTYALL			USEROBJ_FLAG_DIRTY | USEROBJ_FLAG_WBOUNDSDIRTY
#define USEROBJ_MAXNAMELENGTH	(31)

typedef struct tagUserObj
{
	Object				ObjectData ;
#ifdef _DEBUG
	int					nSignature ;
#endif
	grObject			*	pgeObject;
	Brush				*   pDrawBrush;
} UserObj ;

//STATIC FUNCTIONS

grBoolean UserObj_AddToObject( UserObj * pUserObj, grObject * pParent )
{
	assert( pUserObj );
	assert( pUserObj->pgeObject );
	assert( pParent );

	return( grObject_AddChild( pParent, pUserObj->pgeObject) );
}

static grBoolean UserObj_SizeEdge( UserObj * pUserObj, const grVec3d * pStillEdge, const grFloat fScale, ORTHO_AXIS Axis )
{
	float	fTemp;
	grXForm3d	XForm;

	grVec3d		Scale ;
	grVec3d		Temp ;
	int			ModFlags;


	if( !UserObj_GetXForm( pUserObj, &XForm ) )
		return( GR_TRUE );

	ModFlags = grObject_GetXFormModFlags( pUserObj->pgeObject );

	if( ModFlags & GR_OBJECT_XFORM_SCALE )
	{
		grVec3d_Set( &Scale, 1.0f, 1.0f, 1.0f ) ;
		grVec3d_SetElement( &Scale, Axis, fScale ) ;
		Temp = XForm.Translation ;	
		grVec3d_Clear( &XForm.Translation ) ;
		grXForm3d_Scale( &XForm, Scale.X, Scale.Y, Scale.Z ) ;
		XForm.Translation = Temp ;
	}

	
	if( ModFlags & GR_OBJECT_XFORM_TRANSLATE )
	{
		fTemp = grVec3d_GetElement( &XForm.Translation, Axis ) - grVec3d_GetElement( pStillEdge, Axis ) ;
		fTemp = fTemp * fScale ;
		fTemp = fTemp + grVec3d_GetElement( pStillEdge, Axis ) ;
		grVec3d_SetElement( &XForm.Translation, Axis, fTemp ) ;
	}
	UserObj_SetXForm( pUserObj, &XForm );
	return( GR_TRUE );
}

// CREATORS
UserObj *	UserObj_Create( const char * const pszName, Group * pGroup, int32 nNumber, grObject	* pgeObject )
{
	UserObj	*	pUserObj;
	char * CombName;
	grBrush * pgeBrush;

	assert( pszName );
	pUserObj = GR_RAM_ALLOCATE_STRUCT_CLEAR( UserObj );
	if( pUserObj == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate UserObj" );
		return( NULL );
	}
	assert( (pUserObj->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	if( !Object_Init( &pUserObj->ObjectData, pGroup, KIND_USEROBJ, pszName, nNumber ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grRam_Free( pUserObj );
		return( NULL );
	}

	pUserObj->pgeObject = pgeObject;
	if( pUserObj->pgeObject == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "UserObj_Create:grUserObj_Create" );
		grRam_Free( pUserObj );
		return( NULL );
	}


	if( grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
	{
		pUserObj->pDrawBrush = Brush_Create( pszName, NULL, 0 );
	}
	else
		pUserObj->pDrawBrush = NULL;
	CombName = Object_GetNameAndTag( &pUserObj->ObjectData );

	grObject_SetName( pgeObject, CombName );
	grRam_Free( CombName );

	return( pUserObj );
}// UserObj_Create


char  *	 UserObj_CreateKindName( )
{
	return( Util_LoadLocalRcString( IDS_USEROBJ ) );
}



UserObj *	UserObj_Copy( UserObj *	pUserObj, int32 nNumber )
{
	UserObj *pNewUserObj;
	grObject *pgeObject;


	
	assert( pUserObj );
	assert( SIGNATURE == pUserObj->nSignature ) ;

	pgeObject = grObject_Duplicate( pUserObj->pgeObject );
	if( pgeObject == NULL )
	{
		grErrorLog_AddString( GR_ERR_INTERNAL_RESOURCE, "UserObj_Copy:grObject_Duplicate", "Object does not support clone." );
		return( NULL );
	}


	pNewUserObj = UserObj_Create( pUserObj->ObjectData.pszName, pUserObj->ObjectData.pGroup, nNumber, pgeObject );
	if( pNewUserObj == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		return( NULL );
	}

	return( pNewUserObj );
}



char *	UserObj_CreateDefaultName( grObject	* pgeObject )
{
	return( Util_StrDup( grObject_GetTypeName(pgeObject)));
}

void UserObj_Destroy( UserObj ** ppUserObj ) 
{
	assert( ppUserObj );
	assert( *ppUserObj );


	if( (*ppUserObj)->pgeObject  != NULL )
	{
		grObject_Destroy( &(*ppUserObj)->pgeObject );
	}
	if( (*ppUserObj)->pDrawBrush )
	{
		Brush_SetGeBrush( (*ppUserObj)->pDrawBrush, KIND_BRUSH, NULL );
		Brush_Destroy( &(*ppUserObj)->pDrawBrush );
	}
	grRam_Free( (*ppUserObj) );
}// UserObj_Destroy


// MODIFIERS
grBoolean UserObj_Move( UserObj * pUserObj, const grVec3d * pWorldDistance )
{
	grXForm3d XF;
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;

	UserObj_SetModified( pUserObj );
	grObject_GetXForm(pUserObj->pgeObject,&XF);
	grVec3d_Add( &XF.Translation, pWorldDistance, &XF.Translation );
	grObject_SetXForm(pUserObj->pgeObject,&XF);
	return( GR_TRUE );

}// UserObj_Move

grBoolean UserObj_Size( UserObj * pUserObj, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis )
{
	grBoolean bResult = GR_TRUE;

	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;

	// The Z axis is flipped (down is positive)
	// We just determine the edge and call _SizeEdge once or twice to keep this
	// as simple as possible
	switch( eSizeType )
	{
	case Select_Top :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		break ;

	case Select_Bottom :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		break ;

	case Select_Left :
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, hScale, HAxis ) ;
		break ;

	case Select_Right :
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_TopLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, hScale, HAxis ) ;
		break ;		

	case Select_TopRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_BottomLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, hScale, HAxis ) ;
		break ;
	
	case Select_BottomRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Max, vScale, VAxis ) ;
		bResult = UserObj_SizeEdge( pUserObj, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;
	}
	UserObj_SetModified( pUserObj ) ;
	if( bResult == GR_FALSE )
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "UserObj_Size:UserObj_SizeEdge" );

	return( bResult );
}// UserObj_Size

void UserObj_Rotate( UserObj * pUserObj, ORTHO_AXIS RAxis, grFloat RadianAngle, const grVec3d * pRotationCenter )
{
	grXForm3d	XForm ;
	grXForm3d	OrgXForm;
	int			ModFlags;
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	
	if( !UserObj_GetXForm( pUserObj, &OrgXForm ) )
		return;

	XForm = OrgXForm;
	ModFlags = grObject_GetXFormModFlags( pUserObj->pgeObject );

	//If it cant be translated or rotated return.
	if( (ModFlags & ( GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE)) == 0 )
		return;
	grXForm3d_Translate( &XForm, -pRotationCenter->X, -pRotationCenter->Y, -pRotationCenter->Z ) ;
	switch( RAxis )
	{
	case Ortho_Axis_X :
		grXForm3d_RotateX( &XForm, RadianAngle ) ;	break ;
	case Ortho_Axis_Y :
		grXForm3d_RotateY( &XForm, RadianAngle ) ;	break ;
	case Ortho_Axis_Z :
		grXForm3d_RotateZ( &XForm, RadianAngle ) ;	break ;
	}
	grXForm3d_Translate( &XForm, pRotationCenter->X, pRotationCenter->Y, pRotationCenter->Z ) ; 

	// If cant be rotated then just translate it.
	if( !(ModFlags & GR_OBJECT_XFORM_ROTATE ) )
	{
		OrgXForm.Translation = XForm.Translation;
		XForm = OrgXForm;
	}
	UserObj_SetXForm( pUserObj, &XForm) ;
	UserObj_SetModified( pUserObj ) ;


}// UserObj_Rotate

grBoolean UserObj_SendMessage( UserObj * pUserObj, int32 message, void * data )
{
	assert( pUserObj );
	assert( SIGNATURE == pUserObj->nSignature ) ;
	assert( pUserObj->pgeObject );

	return( grObject_SendMessage(pUserObj->pgeObject, message, data ) );
}

int32 UserObj_GetXFormModFlag( UserObj * pUserObj )
{
	assert( pUserObj );
	assert( SIGNATURE == pUserObj->nSignature ) ;
	assert( pUserObj->pgeObject );

	return( grObject_GetXFormModFlags(pUserObj->pgeObject ) );
}

void UserObj_Select3d( UserObj* pUserObj, grVec3d * Front, grVec3d * Back, grVec3d * Impact )
{
	Select3dContextDef Context;

	assert( pUserObj );
	assert( Front );
	assert( Back );
	assert( pUserObj->pgeObject );


	Context.Front = *Front;
	Context.Back = *Back;
	Context.Impact = *Impact;

	grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_SELECT3D,	&Context );
}

#ifdef _USE_BITMAPS
void UserObj_ApplyMatr( UserObj* pUserObj, grBitmap * pBitmap )
{
	assert( pUserObj );
	assert( pBitmap );
	assert( pUserObj->pgeObject );


	grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_APPLYMATERIAL,	pBitmap );
}
#else
void UserObj_ApplyMatr( UserObj* pUserObj, grMaterialSpec * pMatSpec )
{
	assert( pUserObj );
	assert( pMatSpec );
	assert( pUserObj->pgeObject );

	grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_APPLYMATERIALSPEC,	pMatSpec );
}
#endif


grBoolean UserObj_SetXForm( UserObj * pUserObj, const grXForm3d * XForm )
{

	assert( pUserObj );
	assert( SIGNATURE == pUserObj->nSignature ) ;
	assert( pUserObj->pgeObject );
	assert( XForm );

	grObject_SetXForm(pUserObj->pgeObject, XForm);
	UserObj_SetModified( pUserObj );
	return( GR_TRUE );
}// UserObj_SetXForm

 
grBoolean UserObj_RemoveFromWorld( UserObj * pUserObj, grWorld * pWorld)
{
	assert( pUserObj );
	assert( pUserObj->pgeObject );
	assert( pWorld );

	return( grWorld_RemoveObject( pWorld, pUserObj->pgeObject) );
}

grBoolean UserObj_AddToWorld( UserObj * pUserObj, grWorld * pWorld )
{
	assert( pUserObj );
	assert( pUserObj->pgeObject );
	assert( pWorld );

	return( grWorld_AddObject( pWorld, pUserObj->pgeObject) );
}

void UserObj_SetModified( UserObj * pUserObj )
{
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	
	Object_Dirty( &pUserObj->ObjectData );
}// UserObj_SetModified


// ACCESSORS
grBoolean UserObj_GetXForm( const UserObj * pUserObj, grXForm3d * XForm )
{
	return( grObject_GetXForm(pUserObj->pgeObject,XForm) );

}

grBoolean UserObj_GetWorldAxialBounds( const UserObj * pUserObj, grExtBox * BBox)
{
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	
	return( grObject_GetExtBox	( pUserObj->pgeObject, BBox) );

}// UserObj_GetWorldAxialBounds

grBoolean UserObj_GetWorldDrawBounds( const UserObj * pUserObj, grExtBox *DrawBounds )
{
	grBrush *pgeBrush;
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	
	if( pUserObj->pDrawBrush )
	{
		if( !grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
			return( GR_FALSE );
		Brush_SetGeBrush( pUserObj->pDrawBrush, 0, pgeBrush );
		*DrawBounds = *Brush_GetWorldAxialBounds( pUserObj->pDrawBrush );
		return( GR_TRUE );
	}
	return( grObject_GetExtBox	( pUserObj->pgeObject, DrawBounds) );

}// UserObj_GetWorldDrawBounds


grBoolean UserObj_SelectClosest( UserObj * pUserObj, FindInfo	*	pFindInfo )
{
	Point				pt1;
	Point				pt2;
	grFloat				DistSq ;
	grVec3d			Vert1 ;
	grVec3d			Vert2 ;
	grExtBox		Bounds;
	int32			y ;
	grBrush		*	pgeBrush;

	assert( pUserObj != NULL );
	assert( pFindInfo != NULL );
	assert( pFindInfo->pOrtho  != NULL ) ;
			
	
	if( pUserObj->pDrawBrush )
	{
		if( !grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
			return( GR_FALSE );
		Brush_SetGeBrush( pUserObj->pDrawBrush, 0, pgeBrush );
		Brush_SelectClosest( pUserObj->pDrawBrush, pFindInfo );
		if( pFindInfo->pObject == (Object*)pUserObj->pDrawBrush )
			pFindInfo->pObject = (Object*)pUserObj;
		return( GR_TRUE );
	}



	if( !UserObj_GetWorldDrawBounds( pUserObj, &Bounds )  )
		return( GR_TRUE );
	Vert1 = Bounds.Min ;
	Vert2 = Bounds.Max ;
	Ortho_WorldToView( pFindInfo->pOrtho, &Vert1, &pt1 ) ;
	Ortho_WorldToView( pFindInfo->pOrtho, &Vert2, &pt2 ) ;
	DistSq = Util_PointToLineDistanceSquared( &pt1, &pt2, pFindInfo->pViewPt ) ;
	if( DistSq < pFindInfo->fMinDistance )
	{
		pFindInfo->fMinDistance = DistSq ;
		pFindInfo->pObject = (Object*)pUserObj ;
		pFindInfo->nFace = 0 ;
		pFindInfo->nFaceEdge = 0;
	}

	y = pt1.Y ;
	pt1.Y = pt2.Y ;
	pt2.Y = y ;

	DistSq = Util_PointToLineDistanceSquared( &pt1, &pt2, pFindInfo->pViewPt ) ;
	if( DistSq < pFindInfo->fMinDistance )
	{
		pFindInfo->fMinDistance = DistSq ;
		pFindInfo->pObject = (Object*)pUserObj ;
		pFindInfo->nFace = 0 ;
		pFindInfo->nFaceEdge = 0;
	}
	return( GR_TRUE );
}

grObject * UserObj_GetgrObject( UserObj * pUserObj )
{
	assert( pUserObj != NULL );
	assert( SIGNATURE == pUserObj->nSignature ) ;

	return( pUserObj->pgeObject );
}

grBoolean UserObj_FillPositionDescriptor( UserObj * pUserObj, grProperty_List * pArray )
{
	grXForm3d XForm;
	char * Name;

	grProperty Property;
	if( !UserObj_GetXForm( pUserObj, &XForm ) )
		return( GR_TRUE );

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

grProperty_List *	UserObj_BuildDescriptor( UserObj * pUserObj )
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
	grProperty_FillString( &Property, Name, pUserObj->ObjectData.pszName, OBJECT_NAME_FIELD );
	grRam_Free( Name );
	if( !grProperty_Append( pObjectArray,  &Property ) )
	{
		grRam_Free( Name );
		return( NULL );
	}
	
	if( !UserObj_FillPositionDescriptor( pUserObj, pObjectArray ) )
		goto UOBD_ERROR;

	if( !grObject_GetPropertyList(pUserObj->pgeObject, &pPropertyArray) )
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

grProperty_List *	UserObj_GlobalPropertyList( const char * TypeName )
{
	grProperty_List * pPropertyArray = NULL;

	assert( TypeName );

	if( ! grObject_GetRegisteredPropertyList( TypeName, &pPropertyArray ) )
		return( NULL );

	return( pPropertyArray );
}

void UserObj_SetGlobalProperty( const char * TypeName, int DataId, int DataType, grProperty_Data * pData )
{
	grObject_SetRegisteredProperty( TypeName, DataId, DataType, pData );
}


void UserObj_SetProperty( UserObj * pUserObj, int DataId, int DataType, grProperty_Data * pData, grBoolean bUpdate )
{
	char * CombName;

	assert( pUserObj );
	assert( pData );

	if( DataId == OBJECT_NAME_FIELD )
	{
		CombName = Object_GetNameAndTag( &pUserObj->ObjectData );

		grObject_SetName( pUserObj->pgeObject, CombName );
		grRam_Free( CombName );
		return;
	}

	grObject_SetProperty( pUserObj->pgeObject, DataId, DataType, (grProperty_Data*)pData );
	bUpdate;
}


void UserObj_Update( UserObj * pUserObj, int Update_Type )
{
	pUserObj;
	Update_Type;
}

//IS
grBoolean	UserObj_IsInRect( const UserObj * pUserObj, grExtBox *pSelRect, grBoolean bSelEncompeses )
{
	grExtBox WorldBounds;
	grExtBox		Result;

	assert( pUserObj );
	assert( pSelRect );
	
	if( !UserObj_GetWorldDrawBounds( pUserObj, &WorldBounds)  )
		return( GR_FALSE );
	if( bSelEncompeses )
	{
		if( pSelRect->Max.X >= WorldBounds.Max.X &&
			pSelRect->Max.Y >= WorldBounds.Max.Y &&
			pSelRect->Max.Z >= WorldBounds.Max.Z &&
			pSelRect->Min.X <= WorldBounds.Min.X &&
			pSelRect->Min.Y <= WorldBounds.Min.Y &&
			pSelRect->Min.Z <= WorldBounds.Min.Z )
			 return( GR_TRUE );
	}
	else
	{
		return( Util_geExtBox_Intersection ( pSelRect, &WorldBounds, &Result	) );
	}
	return( GR_FALSE );
}//UserObj_IsInRect


//FILE
UserObj * UserObj_CreateFromFile( grVFile * pF, grPtrMgr * pPtrMgr )
{
	UserObj	*	pUserObj = NULL ;
	grBrush	*	pgeBrush;

	assert( grVFile_IsValid( pF ) ) ;

	pUserObj = GR_RAM_ALLOCATE_STRUCT( UserObj );
	if( pUserObj == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate UserObj" );
		return( NULL );
	}
	memset( pUserObj, 0, sizeof( UserObj ) );
	assert( (pUserObj->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN

	if( !Object_InitFromFile( pF , &pUserObj->ObjectData ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Object_InitFromFile.", NULL);
		return NULL;
	}

	pUserObj->pgeObject = grObject_CreateFromFile( pF, pPtrMgr );
	if( pUserObj->pgeObject == NULL )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "grObject_CreateFromFile.", NULL);
		return NULL;
	}
	if( grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
	{
		pUserObj->pDrawBrush = Brush_Create( pUserObj->ObjectData.pszName, NULL, 0 );
	}
	else
		pUserObj->pDrawBrush = NULL;
	Object_SetInLevel( (Object*)pUserObj, GR_TRUE );
	return( pUserObj );
}




grBoolean UserObj_WriteToFile( UserObj * pUserObj, grVFile * pF, grPtrMgr * pPtrMgr )
{
	assert( pUserObj != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	assert( grVFile_IsValid( pF ) ) ;

	if( !Object_WriteToFile( &pUserObj->ObjectData, pF ) )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Object_WriteToFile.", NULL);
		return GR_FALSE;
	}
	if( grObject_WriteToFile( pUserObj->pgeObject, pF, pPtrMgr ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "grUserObj_WriteToFile.", NULL);
		return GR_FALSE;
	}
	return GR_TRUE ;

}// UserObj_WriteToFile


//DISPLAY
#define USEROBJ_MAXPOINTSPERFACE (64)
void UserObj_RenderOrtho( const Ortho * pOrtho, UserObj *pUserObj, int32 hDC, grBoolean bColorOveride )
{
	Point			points[USEROBJ_MAXPOINTSPERFACE];
	grVec3d			Vert1 ;
	grVec3d			Vert2 ;
	grExtBox  Bounds;
	int32			y ;
	grXForm3d		XF;
	HPEN				hOldPen = NULL ;
	HPEN				hPen ;
	grBrush *			pgeBrush;

	assert( pOrtho != NULL ) ;
	assert( SIGNATURE == pUserObj->nSignature ) ;
	assert( pUserObj != NULL ) ;

	if( pUserObj->pDrawBrush )
	{
		if( !bColorOveride )
		{
			hPen = CreatePen( PS_SOLID, 1, RGB( 255, 0, 128 ) ) ;		// Selected objects
			hOldPen = SelectPen( (HDC)hDC, hPen ) ;
		}
		if( !grObject_SendMessage( pUserObj->pgeObject, G3DEDITOR_GET_GRBRUSH, &pgeBrush ) )
			return;
		Brush_SetGeBrush( pUserObj->pDrawBrush, 0, pgeBrush );
		Brush_RenderOrthoFaces(  pUserObj->pDrawBrush, pOrtho, hDC, GR_FALSE, GR_FALSE, GR_TRUE );
		if( !bColorOveride )
		{
			hPen = SelectPen( (HDC)hDC, hOldPen ) ;
			DeletePen( hPen ) ;
		}
		return;
	}

	if( !UserObj_GetWorldDrawBounds( pUserObj, &Bounds )  )
		return;

	if( !UserObj_GetXForm( pUserObj, &XF ) )
		return;
/*
	hPen = CreatePen( PS_SOLID, 1, RGB( 255, 255, 255 ) ) ;		// Selected objects
	hOldPen = SelectPen( (HDC)hDC, hPen ) ;
	Vert1 = XF.Translation ;
	grXForm3d_GetIn( &XF, &Vert2 );
	grVec3d_Scale( &Vert2, 8.0f, &Vert2 );
	grVec3d_Add( &Vert2, &Vert1, &Vert2 );
	Ortho_WorldToView( pOrtho, &Vert1, &points[0] ) ;
	Ortho_WorldToView( pOrtho, &Vert2, &points[1] ) ;
	Pen_Polyline( hDC, points, 2 ) ;
	hPen = SelectPen( (HDC)hDC, hOldPen ) ;
	DeletePen( hPen ) ;
	//TextOut( (HDC)hDC, points[1].X, points[1].Y, "Z", 1 ) ;

	hPen = CreatePen( PS_SOLID, 1, RGB( 255, 0, 0 ) ) ;		// Selected objects
	hOldPen = SelectPen( (HDC)hDC, hPen ) ;
	Vert1 = XF.Translation ;
	grXForm3d_GetUp( &XF, &Vert2 );
	grVec3d_Scale( &Vert2, 8.0f, &Vert2 );
	grVec3d_Add( &Vert2, &Vert1, &Vert2 );
	Ortho_WorldToView( pOrtho, &Vert1, &points[0] ) ;
	Ortho_WorldToView( pOrtho, &Vert2, &points[1] ) ;
	Pen_Polyline( hDC, points, 2 ) ;
	hPen = SelectPen( (HDC)hDC, hOldPen ) ;
	DeletePen( hPen ) ;
	//TextOut( (HDC)hDC, points[1].X, points[1].Y, "Y", 1 ) ;

	hPen = CreatePen( PS_SOLID, 1, RGB( 0, 255, 0 ) ) ;		// Selected objects
	hOldPen = SelectPen( (HDC)hDC, hPen ) ;
	Vert1 = XF.Translation ;
	grXForm3d_GetLeft( &XF, &Vert2 );
	grVec3d_Scale( &Vert2, 8.0f, &Vert2 );
	grVec3d_Add( &Vert2, &Vert1, &Vert2 );
	Ortho_WorldToView( pOrtho, &Vert1, &points[0] ) ;
	Ortho_WorldToView( pOrtho, &Vert2, &points[1] ) ;
	Pen_Polyline( hDC, points, 2 ) ;
	hPen = SelectPen( (HDC)hDC, hOldPen ) ;
	DeletePen( hPen ) ;
	//TextOut( (HDC)hDC, points[1].X, points[1].Y, "X", 1 ) ;
*/
  
	if( !bColorOveride )
	{
		hPen = CreatePen( PS_SOLID, 1, RGB( 255, 0, 128 ) ) ;		// Selected objects
		hOldPen = SelectPen( (HDC)hDC, hPen ) ;
	}
	 Vert1 = Bounds.Min ;
	Vert2 = Bounds.Max ;
	Ortho_WorldToView( pOrtho, &Vert1, &points[0] ) ;
	Ortho_WorldToView( pOrtho, &Vert2, &points[1] ) ;

	// It would appear we just let GDI clip...
	Pen_Polyline( hDC, points, 2 ) ;
	y = points[0].Y ;
	points[0].Y = points[1].Y ;
	points[1].Y = y ;
	Pen_Polyline( hDC, points, 2 ) ;
	if( !bColorOveride )
	{
		hPen = SelectPen( (HDC)hDC, hOldPen ) ;
		DeletePen( hPen ) ;
	}


}// UserObj_RenderOrtho



