/****************************************************************************************/
/*  LIGHT.C                                                                             */
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

#include "Light.h"
#include "ObjectDef.h"
#define SIGNATURE			'LITE'

#define LIGHT_BOX_MIN				-0.1f
#define LIGHT_BOX_MAX				0.1f
#define LIGHT_DRAW_MIN				-3.0f
#define LIGHT_DRAW_MAX				3.0f
#define LIGHT_FLAG_DIRTY			0x0001
#define LIGHT_FLAG_WBOUNDSDIRTY		0x0002
#define LIGHT_FLAG_DIRTYALL			LIGHT_FLAG_DIRTY | LIGHT_FLAG_WBOUNDSDIRTY
#define LIGHT_MAXNAMELENGTH	(31)

static int gLight_Update = OBJECT_UPDATE_REALTIME;


typedef enum
{
	LIGHT_GLOBAL_UPDATEGROUP_ID = PROPERTY_LOCAL_DATATYPE_START,
	LIGHT_GLOBAL_UPDATE_MANUEL_ID,
	LIGHT_GLOBAL_UPDATE_CHANGE_ID,
	LIGHT_GLOBAL_UPDATE_REALTIME_ID,
	LIGHT_GLOBAL_UPDATEGROUP_END_ID,
	LIGHT_GLOBAL_MAINTAIN_LIGHING_ID
};

typedef struct tagLight
{
	Object				ObjectData ;
	uint32				nIndexTag ;		// Used only during load
#ifdef _DEBUG
	int					nSignature ;
#endif
	int32				Flags;
	grExtBox			WorldBounds ;
	LightInfo			LightData ;
	grLight			*	pgeLight;
	grWorld			*	pWorld; //Array that owns this light
	grBoolean			bInWorld;
	grBoolean			bDLight;
} Light ;

//STATIC FUNCTIONS

static grBoolean Light_SetData( Light * pLight )
{
	if( pLight->pgeLight != NULL )
	{
		if( !grLight_SetAttributes(	pLight->pgeLight,
									&pLight->LightData.Pos, 
									&pLight->LightData.Color, 
									pLight->LightData.Radius, 
									pLight->LightData.Brightness, 
									pLight->LightData.Flags) )
		{
			grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_SetData:grLight_SetAttributes" );
			return( GR_FALSE );
		}
		Object_Dirty( &pLight->ObjectData );
	}
	return( GR_TRUE );
}

grBoolean Light_UpdateData( Light * pLight )
{
	if( pLight->pgeLight != NULL )
	{
		if( !grLight_SetAttributes(	pLight->pgeLight,
									&pLight->LightData.Pos, 
									&pLight->LightData.Color, 
									pLight->LightData.Radius, 
									pLight->LightData.Brightness, 
									pLight->LightData.Flags) )
		{
			grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_SetData:grLight_SetAttributes" );
			return( GR_FALSE );
		}

	}
	return( GR_TRUE );
}

static grBoolean Light_SizeEdge( Light * pLight, const grVec3d * pStillEdge, const grFloat fScale, ORTHO_AXIS Axis )
{
	float	fTemp;


	fTemp = grVec3d_GetElement( &pLight->LightData.Pos, Axis ) - grVec3d_GetElement( pStillEdge, Axis ) ;
	fTemp = fTemp * fScale ;
	fTemp = fTemp + grVec3d_GetElement( pStillEdge, Axis ) ;
	grVec3d_SetElement( &pLight->LightData.Pos, Axis, fTemp ) ;
	if( !Light_SetData( pLight ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_SizeEdge:Light_SetData" );
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

// CREATORS
Light *	Light_Create( const char * const pszName, Group * pGroup, int32 nNumber,grWorld	* pWorld )
{
	Light	*	pLight;
	assert( pszName );
	assert( pWorld );

	pLight = GR_RAM_ALLOCATE_STRUCT( Light );
	if( pLight == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Light" );
		return( NULL );
	}
	memset( pLight, 0, sizeof( Light ) );
	assert( (pLight->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	if( !Object_Init( &pLight->ObjectData, pGroup, KIND_LIGHT, pszName, nNumber ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		grRam_Free( pLight );
		return( NULL );
	}

	pLight->pgeLight = grLight_Create();
	if( pLight->pgeLight == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_Create:grLight_Create" );
		grRam_Free( pLight );
		return( NULL );
	}
	pLight->pWorld = pWorld;
	pLight->bInWorld = GR_FALSE;
	pLight->bDLight = GR_FALSE;

	if( !grLight_GetAttributes(	pLight->pgeLight, 
									&pLight->LightData.Pos, 
									&pLight->LightData.Color, 
									&pLight->LightData.Radius, 
									&pLight->LightData.Brightness, 
									&pLight->LightData.Flags ) 
	  )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_Create:grLight_GetAttributes" );
		grRam_Free( pLight );
		return( NULL );
	}
	grExtBox_Set( &pLight->WorldBounds, LIGHT_BOX_MIN, LIGHT_BOX_MIN, LIGHT_BOX_MIN,
										LIGHT_BOX_MAX, LIGHT_BOX_MAX, LIGHT_BOX_MAX );
	return( pLight );
}// Light_Create


Light *	Light_Copy( Light *	pLight, int32 nNumber )
{
	Light *pNewLight;

	
	assert( pLight );
	assert( SIGNATURE == pLight->nSignature ) ;

	pNewLight = Light_Create( pLight->ObjectData.pszName, pLight->ObjectData.pGroup, nNumber,pLight->pWorld );
	if( pNewLight == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		return( NULL );
	}
	pNewLight->LightData = pLight->LightData;
	if( !Light_UpdateData( pNewLight ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_Copy::Light_SetData" );
		return( NULL );
	}

	return( pNewLight );
}

Light *	Light_FromTemplate( char * pszName, Group * pGroup, Light *	pLight, int32 nNumber, grBoolean bUpdate )
{
	Light *pNewLight;

	
	assert( pszName );
	assert( pLight );
	assert( SIGNATURE == pLight->nSignature ) ;

	pNewLight = Light_Copy( pLight, nNumber );
	if( pNewLight == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Trace" );
		return( NULL );
	}
	if( pNewLight->ObjectData.pszName != NULL )
	{
		grRam_Free( pNewLight->ObjectData.pszName );
	}
	 pNewLight->ObjectData.pszName = pszName;
	 pNewLight->ObjectData.pGroup = pGroup ;

	Light_UpdateBounds( pNewLight );
	if( !grWorld_AddLight(pNewLight->pWorld, pNewLight->pgeLight, bUpdate ) )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_SetData:grWorld_AddLight" );
		return NULL;
	}
	pNewLight->bInWorld = GR_TRUE;
	return( pNewLight );
}// Light_FromTemplate


char *	Light_CreateDefaultName(  )
{
	return( Util_LoadLocalRcString( IDS_LIGHT ) );
}

void Light_Destroy( Light ** ppLight ) 
{
	assert( ppLight );
	assert( *ppLight );
	assert( (*ppLight)->pWorld );


	if( (*ppLight)->pgeLight  != NULL )
	{
		if( (*ppLight)->bInWorld )
		{
			grWorld_RemoveLight((*ppLight)->pWorld, (*ppLight)->pgeLight, GR_TRUE );
			(*ppLight)->bInWorld = GR_FALSE;
		}
		grLight_Destroy( &(*ppLight)->pgeLight );
	}

	// [MLB-ICE] Comment: Same as in Brush/Level/... ;)
	if( (*ppLight)->ObjectData.pszName != NULL )
		grRam_Free( (*ppLight)->ObjectData.pszName );
	// [MLB-ICE] EOB

	grRam_Free( (*ppLight) );
}// Light_Destroy

Light *	Light_CreateTemplate(  grWorld * pWorld )
{
	Light * pLight;
	assert( pWorld );

	pLight = GR_RAM_ALLOCATE_STRUCT( Light );
	if( pLight == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Light" );
		return( NULL );
	}
	memset( pLight, 0, sizeof( Light) );
	assert( (pLight->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	Object_Init( (Object*)pLight, NULL, KIND_LIGHT, "Template", 0 );
	pLight->pWorld = pWorld;
	pLight->LightData.Brightness = 3.0f;
	grVec3d_Set( &pLight->LightData.Color, 255.0f, 255.0f, 255.0f );
	grVec3d_Set( &pLight->LightData.Pos, 0.0f, 0.0f, 0.0f );
	pLight->LightData.Radius = 200.0f;
	Light_UpdateBounds( pLight );
	pLight->pgeLight = NULL;
	grExtBox_Set( &pLight->WorldBounds, LIGHT_BOX_MIN, LIGHT_BOX_MIN, LIGHT_BOX_MIN,
										LIGHT_BOX_MAX, LIGHT_BOX_MAX, LIGHT_BOX_MAX );
	return( pLight );
}

// MODIFIERS
grBoolean Light_Move( Light * pLight, const grVec3d * pWorldDistance )
{
	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;

	Light_SetModified( pLight );
	grVec3d_Add( &pLight->LightData.Pos, pWorldDistance, &pLight->LightData.Pos );
	Light_SetData( pLight );
	return( GR_TRUE );

}// Light_Move

grBoolean Light_Size( Light * pLight, const grExtBox * pSelectedBounds, const grFloat hScale, const grFloat vScale, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis )
{
	grBoolean bResult = GR_TRUE;

	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;

	// The Z axis is flipped (down is positive)
	// We just determine the edge and call _SizeEdge once or twice to keep this
	// as simple as possible
	switch( eSizeType )
	{
	case Select_Top :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		break ;

	case Select_Bottom :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		break ;

	case Select_Left :
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;

	case Select_Right :
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_TopLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;		

	case Select_TopRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;

	case Select_BottomLeft :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/hScale, HAxis ) ;
		break ;
	
	case Select_BottomRight :
		if( Ortho_Axis_Z == VAxis )
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, vScale, VAxis ) ;
		else
			bResult = Light_SizeEdge( pLight, &pSelectedBounds->Max, 1.0f/vScale, VAxis ) ;
		bResult = Light_SizeEdge( pLight, &pSelectedBounds->Min, hScale, HAxis ) ;
		break ;
	}
	Light_SetModified( pLight ) ;
	if( bResult == GR_FALSE )
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_Size:Light_SizeEdge" );

	Light_SetData( pLight );
	return( bResult );
}// Light_Size

grBoolean Light_SetXForm( Light * pLight, const grXForm3d * XForm )
{

	assert( pLight );
	assert( SIGNATURE == pLight->nSignature ) ;
	assert( XForm );

	pLight->LightData.Pos = XForm->Translation;
	Light_SetModified( pLight );
	return( Light_SetData( pLight ) );
}// Light_SetXForm


void Light_UpdateBounds( Light * pLight )
{

	assert( pLight );

	grExtBox_SetTranslation ( &pLight->WorldBounds, &pLight->LightData.Pos );

}

void Light_SetModified( Light * pLight )
{
	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;
	
	pLight->Flags |= LIGHT_FLAG_DIRTYALL ;	
}// Light_SetModified

grBoolean Light_SetInfo( Light * pLight, LightInfo *pLightInfo, int32 BlankFieldFlag )
{
	assert( pLight );
	assert( pLightInfo );

	if( BlankFieldFlag  & LIGHT_FIELD_POS )
	{
		pLight->LightData.Pos = pLightInfo->Pos;
		Light_SetModified( pLight );
	}
	if( BlankFieldFlag  & LIGHT_FIELD_BRIGHTNESS )
		pLight->LightData.Brightness = pLightInfo->Brightness;
	if( BlankFieldFlag  & LIGHT_FIELD_RADIUS )
		pLight->LightData.Radius = pLightInfo->Radius;
	if( BlankFieldFlag  & LIGHT_FIELD_COLOR )
		pLight->LightData.Color = pLightInfo->Color;
	return( Light_SetData( pLight ) );
}//Light_SetInfo

void Light_SetIndexTag( Light * pLight, const uint32 nIndex ) 
{
	pLight->nIndexTag = nIndex;
	//grLight_SetIndexTAG( pLight->pgeLight, nIndex );
}//Light_SetIndexTag

void Light_RemoveFromWorld( Light * pLight )
{
	grWorld_RemoveLight( pLight->pWorld, pLight->pgeLight, GR_TRUE );
	pLight->bInWorld = GR_FALSE;
}

void Light_AddToWorld( Light * pLight )
{
	grWorld_AddLight(pLight->pWorld, pLight->pgeLight, GR_TRUE);
	pLight->bInWorld = GR_TRUE;
}

// ACCESSORS
void Light_GetXForm( const Light * pLight, grXForm3d * XForm )
{
	grXForm3d_SetIdentity( XForm );
	XForm->Translation = pLight->LightData.Pos;
}

const grExtBox * Light_GetWorldAxialBounds( const Light * pLight )
{
	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;
	
	if( pLight->Flags & LIGHT_FLAG_WBOUNDSDIRTY )
	{	
		Light * pEvalLight = (Light*)pLight ;			// Lazy Evaluation requires removing the const
		Light_UpdateBounds( pEvalLight ) ;
	}

	return &pLight->WorldBounds ;

}// Light_GetWorldAxialBounds

void Light_GetWorldDrawBounds( const Light * pLight, grExtBox *DrawBounds )
{
	grVec3d Center;
	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;
	
	if( pLight->Flags & LIGHT_FLAG_WBOUNDSDIRTY )
	{	
		Light * pEvalLight = (Light*)pLight ;			// Lazy Evaluation requires removing the const
		Light_UpdateBounds( pEvalLight ) ;
	}
	grExtBox_GetTranslation ( &pLight->WorldBounds, &Center );
	grExtBox_Set (  DrawBounds,
				  LIGHT_DRAW_MIN,	  LIGHT_DRAW_MIN,	  LIGHT_DRAW_MIN,
				  LIGHT_DRAW_MAX,	  LIGHT_DRAW_MAX,	  LIGHT_DRAW_MAX );
	grExtBox_SetTranslation ( DrawBounds, &Center );

}// Light_GetWorldDrawBounds

void Light_GetInfo( const Light * pLight, LightInfo *pLightInfo, int32 *BlankFieldFlag )
{
	assert( pLight );
	assert( pLightInfo );
	assert( BlankFieldFlag );

	if( *BlankFieldFlag == LIGHT_INIT_ALL )
	{
		*(pLightInfo) = pLight->LightData;
		*BlankFieldFlag = 0;
		return;
	}
	if( !grVec3d_Compare( &pLightInfo->Pos,&pLight->LightData.Pos, 0.0f) )
		*BlankFieldFlag |= LIGHT_FIELD_POS;

	if( pLightInfo->Brightness != pLight->LightData.Brightness )
		*BlankFieldFlag |= LIGHT_FIELD_BRIGHTNESS;

	if( pLightInfo->Radius != pLight->LightData.Radius )
		*BlankFieldFlag |= LIGHT_FIELD_RADIUS;

	if( !grVec3d_Compare( &pLightInfo->Color,&pLight->LightData.Color, 0.0f) )
		*BlankFieldFlag |= LIGHT_FIELD_COLOR;
	return;
}//Light_GetInfo

grBoolean Light_SelectClosest( Light * pLight, FindInfo	*	pFindInfo )
{
	Point				pt1;
	Point				pt2;
	grFloat				DistSq ;
	grVec3d			Vert1 ;
	grVec3d			Vert2 ;
	grExtBox		Bounds;
	int32			y ;

	assert( pLight != NULL );
	assert( pFindInfo != NULL );
	assert( pFindInfo->pOrtho  != NULL ) ;
			
	


	Light_GetWorldDrawBounds( pLight, &Bounds ) ;
	Vert1 = Bounds.Min ;
	Vert2 = Bounds.Max ;
	Ortho_WorldToView( pFindInfo->pOrtho, &Vert1, &pt1 ) ;
	Ortho_WorldToView( pFindInfo->pOrtho, &Vert2, &pt2 ) ;
	DistSq = Util_PointToLineDistanceSquared( &pt1, &pt2, pFindInfo->pViewPt ) ;
	if( DistSq < pFindInfo->fMinDistance )
	{
		pFindInfo->fMinDistance = DistSq ;
		pFindInfo->pObject = (Object*)pLight ;
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
		pFindInfo->pObject = (Object*)pLight ;
		pFindInfo->nFace = 0 ;
		pFindInfo->nFaceEdge = 0;
	}
	return( GR_TRUE );
}


grBoolean Light_FillPositionDescriptor( Light * pLight, grProperty_List * pArray )
{
	char * Name;
	grProperty Property;

	Name = Util_LoadLocalRcString( IDS_POSITION_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillVec3dGroup( &Property, Name, &pLight->LightData.Pos,	OBJECT_POSITION_FIELD  );
	if( !grProperty_Append( pArray,  &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONX_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat(  &Property, Name, pLight->LightData.Pos.X, OBJECT_POSITION_FIELDX, -FLT_MAX, FLT_MAX, 1.0f );
	if( !grProperty_Append( pArray,  &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONY_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat(  &Property, Name, pLight->LightData.Pos.Y,	OBJECT_POSITION_FIELDY, -FLT_MAX, FLT_MAX, 1.0f );
	if( !grProperty_Append( pArray, &Property ) )
	{
		grRam_Free( Name );
		return( GR_FALSE );
	}
	grRam_Free( Name );

	Name = Util_LoadLocalRcString( IDS_POSITIONZ_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat( &Property, Name, pLight->LightData.Pos.Z, OBJECT_POSITION_FIELDZ, -FLT_MAX, FLT_MAX, 1.0f );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}

	grProperty_FillGroupEnd( &Property, OBJECT_POSITION_FIELD_END );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

grBoolean Light_FillRGBDescriptor( Light * pLight, grProperty_List * pArray )
{
	char * Name;
	grProperty Property;

	Name = Util_LoadLocalRcString( IDS_COLOR_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillColorGroup( &Property, Name, &pLight->LightData.Color,	LIGHT_COLOR_FIELD  );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}

	Name = Util_LoadLocalRcString( IDS_RED_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat( &Property, Name, pLight->LightData.Color.X,	LIGHT_RED_FIELD, 0, 255.0f, 2.0f );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}

	Name = Util_LoadLocalRcString( IDS_GREEN_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat( &Property, Name, pLight->LightData.Color.Y,	LIGHT_GREEN_FIELD, 0, 255.0f, 1.0f );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}

	Name = Util_LoadLocalRcString( IDS_BLUE_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillFloat( &Property, Name, pLight->LightData.Color.Z,	LIGHT_BLUE_FIELD, 0, 255.0, 1.0f );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}


	Name = Util_LoadLocalRcString( IDS_PICKER_FIELD );
	if( Name == NULL )
		return( GR_FALSE );
	grProperty_FillColorPicker( &Property, Name, &pLight->LightData.Color,	LIGHT_PICKER_FIELD );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}

	grProperty_FillGroupEnd( &Property, LIGHT_COLOR_FIELD_END );
	if( !grProperty_Append( pArray, &Property ) )
	{
		return( GR_FALSE );
	}
	return( GR_TRUE );
}

grProperty_List *	Light_BuildDescriptor( Light * pLight )
{
	grProperty_List * pArray = NULL;
	char * Name;
	grProperty Property;


	assert( pLight );

	pArray = grProperty_ListCreateEmpty();
	if( pArray == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "DescriptorArray" );
		return NULL;
	}

	Name = Util_LoadLocalRcString( IDS_NAME_FIELD );
	if( Name == NULL )
		goto LBD_ERROR;
	grProperty_FillString( &Property, Name, pLight->ObjectData.pszName, OBJECT_NAME_FIELD );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
		goto LBD_ERROR;

	Light_FillPositionDescriptor( pLight, pArray );

	Name = Util_LoadLocalRcString( IDS_BRIGHTNESS_FIELD );
	if( Name == NULL )
		goto LBD_ERROR;
	grProperty_FillFloat( &Property, Name, pLight->LightData.Brightness,	LIGHT_BRIGHTNESS_FIELD, 0, FLT_MAX, 1.0f);
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
		goto LBD_ERROR;

	Name = Util_LoadLocalRcString( IDS_RADIUS_FIELD );
	if( Name == NULL )
		goto LBD_ERROR;
	grProperty_FillFloat( &Property, Name, pLight->LightData.Radius, LIGHT_RADIUS_FIELD, 1, FLT_MAX, 1.0f );
	Light_FillRGBDescriptor( pLight, pArray );
	grRam_Free( Name );
	if( !grProperty_Append( pArray, &Property ) )
		goto LBD_ERROR;
	
	return( pArray );

LBD_ERROR:
	grProperty_ListDestroy( &pArray );
	return( NULL );
}

void Light_SetProperty( Light * pLight, int DataId, int DataType, grProperty_Data * pData, grBoolean bUpdate )
{
	DataType;
	switch( DataId )
	{
	case LIGHT_BRIGHTNESS_FIELD:
		pLight->LightData.Brightness = pData->Float;
		break;

	case LIGHT_RADIUS_FIELD:
		pLight->LightData.Radius = pData->Float;
		break;
	case LIGHT_RED_FIELD:
		pLight->LightData.Color.X = pData->Float;
		break;

	case LIGHT_GREEN_FIELD:
		pLight->LightData.Color.Y = pData->Float;
		break;

	case LIGHT_BLUE_FIELD:
		pLight->LightData.Color.Z = pData->Float;
		break;

	case LIGHT_PICKER_FIELD:
		pLight->LightData.Color = pData->Vector;
		break;
	}
	Light_SetData( pLight );
	if( bUpdate )
		Light_Update( pLight, OBJECT_UPDATE_CHANGE );
}

void Light_ChangeToDLight( Light * pLight )
{
	Light_RemoveFromWorld( pLight );
	grWorld_AddDLight(pLight->pWorld, pLight->pgeLight );
	pLight->bDLight = GR_TRUE;
}

void Light_ChangeFromDLight( Light * pLight )
{
	grWorld_RemoveDLight( pLight->pWorld, pLight->pgeLight);
	Light_AddToWorld( pLight );
	pLight->bDLight = GR_FALSE;
}

void Light_Update( Light * pLight, int Update_Type )
{
#pragma message( "Temporary Add and Remove from world to update lights." )

	if( Update_Type >= OBJECT_UPDATE_CHANGE )
		Object_Dirty( (Object*)pLight );

	if( Update_Type > gLight_Update )
		return;
	if( !(pLight->ObjectData.miscFlags & OBJECT_DIRTY  ) )
		return;

	if( Update_Type == OBJECT_UPDATE_REALTIME && pLight->bDLight == GR_FALSE )
	{
		Light_ChangeToDLight( pLight );
	}

	if( Update_Type == OBJECT_UPDATE_CHANGE && pLight->bDLight )
	{
		Light_ChangeFromDLight( pLight );
	}

	if( pLight->bInWorld )
	{
		grWorld_RemoveLight(pLight->pWorld, pLight->pgeLight, GR_TRUE );
		pLight->bInWorld = GR_FALSE;
		if( !grWorld_AddLight(pLight->pWorld, pLight->pgeLight, GR_TRUE ) )
		{
			grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_SetData:grWorld_AddLight" );
			return;
		}
		pLight->bInWorld = GR_TRUE;
	}
	pLight->ObjectData.miscFlags &= ~OBJECT_DIRTY;
}

//IS
grBoolean	Light_IsInRect( const Light * pLight, grExtBox *pSelRect, grBoolean bSelEncompeses )
{
	const grExtBox *pWorldBounds;
	grExtBox		Result;

	assert( pLight );
	assert( pSelRect );
	
	pWorldBounds = Light_GetWorldAxialBounds( pLight ) ;
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
}//Light_IsInRect


//FILE
Light * Light_CreateFromFile( grVFile * pF, grWorld * pWorld, grPtrMgr * pPtrMgr )
{
	Light	*	pLight = NULL ;

	assert( grVFile_IsValid( pF ) ) ;

	pLight = GR_RAM_ALLOCATE_STRUCT( Light );
	if( pLight == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Unable to allocate Light" );
		return( NULL );
	}
	memset( pLight, 0, sizeof( Light ) );
	assert( (pLight->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN

	if( !Object_InitFromFile( pF , &pLight->ObjectData ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Object_InitFromFile.\n", NULL);
		return NULL;
	}

	if( !grVFile_Read(  pF, &pLight->nIndexTag, sizeof pLight->nIndexTag ) )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "Light_ReadFromFile.\n", NULL);
		return NULL;
	}
	pLight->pgeLight = grLight_CreateFromFile(pF, pPtrMgr);
	if( pLight->pgeLight == NULL )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_READ, "grLight_CreateFromFile.\n", NULL);
		return NULL;
	}
	if( !grLight_GetAttributes(	pLight->pgeLight, 
									&pLight->LightData.Pos, 
									&pLight->LightData.Color, 
									&pLight->LightData.Radius, 
									&pLight->LightData.Brightness, 
									&pLight->LightData.Flags ) 
	  )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "Light_ReattachCB:grLight_GetAttributes" );
		return NULL;
	}

	pLight->pWorld = pWorld;
	grExtBox_Set( &pLight->WorldBounds, LIGHT_BOX_MIN, LIGHT_BOX_MIN, LIGHT_BOX_MIN,
											LIGHT_BOX_MAX, LIGHT_BOX_MAX, LIGHT_BOX_MAX );
	Light_UpdateBounds( pLight );

	pLight->Flags |= LIGHT_FLAG_DIRTYALL ;	
	Object_SetInLevel( (Object*)pLight, GR_TRUE );
	pLight->bInWorld = GR_TRUE;

	return( pLight );
}


grBoolean Light_WriteToFile( Light * pLight, grVFile * pF, grPtrMgr * pPtrMgr )
{
	assert( pLight != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;
	assert( grVFile_IsValid( pF ) ) ;

	if( !Object_WriteToFile( &pLight->ObjectData, pF ) )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Object_WriteToFile.", NULL);
		return GR_FALSE;
	}
	if( grVFile_Write( pF, &pLight->nIndexTag, sizeof pLight->nIndexTag ) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "Light_WriteToFile.", NULL);
		return GR_FALSE;
	}
	if( grLight_WriteToFile( pLight->pgeLight, pF, pPtrMgr) == GR_FALSE )
	{
		grErrorLog_AddString(GR_ERR_FILEIO_WRITE, "grLight_WriteToFile.", NULL);
		return GR_FALSE;
	}
	return GR_TRUE ;

}// Light_WriteToFile

//DISPLAY
#define LIGHT_MAXPOINTSPERFACE (64)
void Light_RenderOrtho( const Ortho * pOrtho, Light *pLight, int32 hDC, grBoolean bColorOveride )
{
	Point			points[LIGHT_MAXPOINTSPERFACE];
	grVec3d			Vert1 ;
	grVec3d			Vert2 ;
	grExtBox  Bounds;
	int32			y ;

	assert( pOrtho != NULL ) ;
	assert( SIGNATURE == pLight->nSignature ) ;
	assert( pLight != NULL ) ;

	 Light_GetWorldDrawBounds( pLight, &Bounds ) ;
	
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
	bColorOveride;

}// Light_RenderOrtho


grProperty_List *	Light_GlobalPropertyList()
{
	grProperty_List * pList;
	grProperty	Property;
	char *	Name;
	grBoolean bCheck;

	pList  =  grProperty_ListCreate(0);
	if( pList == NULL )
		return( NULL );
	
	Name = Util_LoadLocalRcString( IDS_UPDATE ) ;
	grProperty_FillGroup( &Property, Name, LIGHT_GLOBAL_UPDATEGROUP_ID );
	if( !grProperty_Append( pList, &Property ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Light_GlobalPropertyList:grProperty_Append");
		grProperty_ListDestroy( &pList );
		return( NULL );
	}

	Name = Util_LoadLocalRcString( IDS_UPDATE_MANUEL ) ;
	bCheck = (gLight_Update == OBJECT_UPDATE_MANUEL );
	grProperty_FillRadio( &Property, Name, bCheck, LIGHT_GLOBAL_UPDATE_MANUEL_ID );
	if( !grProperty_Append( pList, &Property ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Light_GlobalPropertyList:grProperty_Append");
		grProperty_ListDestroy( &pList );
		return( NULL );
	}
	
	Name = Util_LoadLocalRcString( IDS_UPDATE_CHANGE ) ;
	bCheck = (gLight_Update == OBJECT_UPDATE_CHANGE );
	grProperty_FillRadio( &Property, Name, bCheck, LIGHT_GLOBAL_UPDATE_CHANGE_ID );
	if( !grProperty_Append( pList, &Property ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Light_GlobalPropertyList:grProperty_Append");
		grProperty_ListDestroy( &pList );
		return( NULL );
	}

	Name = Util_LoadLocalRcString( IDS_UPDATE_REALTIME ) ;
	bCheck = (gLight_Update == OBJECT_UPDATE_REALTIME );
	grProperty_FillRadio( &Property, Name, bCheck, LIGHT_GLOBAL_UPDATE_REALTIME_ID );
	if( !grProperty_Append( pList, &Property ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Light_GlobalPropertyList:grProperty_Append");
		grProperty_ListDestroy( &pList );
		return( NULL );
	}

	grProperty_FillGroupEnd( &Property, LIGHT_GLOBAL_UPDATEGROUP_END_ID );
	if( !grProperty_Append( pList, &Property ) )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Light_GlobalPropertyList:grProperty_Append");
		grProperty_ListDestroy( &pList );
		return( NULL );
	}

	
	return( pList );
}

void Light_SetGlobalProperty( int DataId, int DataType, grProperty_Data * pData )
{
	switch( DataId )
	{
		case LIGHT_GLOBAL_UPDATE_MANUEL_ID:
			if( pData->Bool )
			{
				gLight_Update = OBJECT_UPDATE_MANUEL;
			}
			break;

		case LIGHT_GLOBAL_UPDATE_CHANGE_ID:
			if( pData->Bool )
			{
				gLight_Update = OBJECT_UPDATE_CHANGE;
			}
			break;

		case LIGHT_GLOBAL_UPDATE_REALTIME_ID:
			if( pData->Bool )
			{
				gLight_Update = OBJECT_UPDATE_REALTIME;
			}
			break;

	}
	DataType;
}
