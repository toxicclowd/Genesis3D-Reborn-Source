/****************************************************************************************/
/*  JEPROPERTY.C                                                                        */
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

#include <assert.h>
#include <string.h>

#include "grProperty.h"
#include "Ram.h"
#include "Errorlog.h"
#include "Util.h"


//========================================================================================================
//	grProperty_ListCreate
//========================================================================================================
GRAPI grProperty_List * GRCC grProperty_ListCreate( int FieldN )
{
	grProperty_List * pArray = NULL;
	grProperty *pgrProperty = NULL;

	pArray = GR_RAM_ALLOCATE_STRUCT( grProperty_List );
	if( pArray == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "grProperty_List" );
		return( NULL );
	}

	pArray->pgrProperty = GR_RAM_ALLOCATE_ARRAY_CLEAR( grProperty, FieldN );
	if( pArray->pgrProperty == NULL )
		return( NULL );
	pArray->grPropertyN =  FieldN;
	pArray->bDirty = GR_FALSE;

	return( pArray );
}

//========================================================================================================
//	grProperty_ListCreate
//========================================================================================================
GRAPI grProperty_List * GRCC grProperty_ListCreateEmpty( )
{
	grProperty_List * pArray = NULL;
	grProperty *pgrProperty = NULL;

	pArray = GR_RAM_ALLOCATE_STRUCT( grProperty_List );
	if( pArray == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "grProperty_List" );
		return( NULL );
	}

	pArray->grPropertyN =  0;
	pArray->pgrProperty = NULL;
	pArray->bDirty = GR_FALSE;

	return( pArray );
}

//========================================================================================================
//	grProperty_ListCopy
//========================================================================================================
GRAPI grProperty_List * GRCC grProperty_ListCopy( grProperty_List * pArray)
{
	grProperty_List * pNewArray;
	int i;

	pNewArray = grProperty_ListCreate( pArray->grPropertyN );
	if( pNewArray == NULL )
		return( NULL );

	for( i = 0; i < pArray->grPropertyN; i++ )
	{
		pNewArray->pgrProperty[i] = pArray->pgrProperty[i];
		if( pArray->pgrProperty[i].FieldName != NULL )
		{
			pNewArray->pgrProperty[i].FieldName = Util_StrDup( pArray->pgrProperty[i].FieldName );
			if( pNewArray->pgrProperty[i].FieldName == NULL )
				goto PLC_ERROR;
		}
		pNewArray->grPropertyN = i+1;
	}
	pNewArray->bDirty = pArray->bDirty;

	return( pNewArray );
PLC_ERROR:
	grProperty_ListDestroy( &pNewArray );
	return( NULL );
}

GRAPI grProperty_List * GRCC grProperty_ListConCat( grProperty_List * pArray, grProperty_List *pArray2 )
{
	grProperty_List * pNewArray;
	int i;
	int Array1End;

	pNewArray = grProperty_ListCreate( pArray->grPropertyN + pArray2->grPropertyN );
	if( pNewArray == NULL )
		return( NULL );
	pNewArray->grPropertyN = 0;
	for( i = 0; i < pArray->grPropertyN; i++ )
	{
		pNewArray->pgrProperty[i] = pArray->pgrProperty[i];
		if( pArray->pgrProperty[i].FieldName != NULL )
		{
			pNewArray->pgrProperty[i].FieldName = Util_StrDup( pArray->pgrProperty[i].FieldName );
			if( pNewArray->pgrProperty[i].FieldName == NULL )
				goto PLC_ERROR;
		}
		pNewArray->grPropertyN++;
	}
	Array1End = i;
	for( i = 0; i < pArray2->grPropertyN; i++ )
	{
		pNewArray->pgrProperty[Array1End+i] = pArray2->pgrProperty[i];
		if( pArray2->pgrProperty[i].FieldName != NULL )
		{
			pNewArray->pgrProperty[Array1End+i].FieldName = Util_StrDup( pArray2->pgrProperty[i].FieldName );
			if( pNewArray->pgrProperty[Array1End+i].FieldName == NULL )
				goto PLC_ERROR;
		}
		pNewArray->grPropertyN++;
	}
	pNewArray->bDirty = pArray->bDirty | pArray2->bDirty ;
	return( pNewArray );
PLC_ERROR:
	grProperty_ListDestroy( &pNewArray );
	return( NULL );
}
//========================================================================================================
//	grProperty_Append
//========================================================================================================
GRAPI grBoolean GRCC grProperty_Append( grProperty_List *pArray, grProperty *pgrProperty )
{
	grProperty *pgrPropertyArray = NULL;

	if( pArray->pgrProperty == NULL )
	{
		pArray->pgrProperty = GR_RAM_ALLOCATE_STRUCT( grProperty );
		if( pArray->pgrProperty == NULL )
			return( GR_FALSE );
		pArray->grPropertyN = 0;
	}
	else
	{
		pgrPropertyArray = GR_RAM_REALLOC_ARRAY( pArray->pgrProperty, grProperty, pArray->grPropertyN+1);
		if( pgrPropertyArray == NULL )
			return( GR_FALSE );
		pArray->pgrProperty = pgrPropertyArray;
	}
	pArray->pgrProperty[pArray->grPropertyN] = *pgrProperty;
	pArray->grPropertyN += 1;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillCheck
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillCheck( grProperty * pgrProperty, char *Name, int Value, int FieldId )
{
	assert( pgrProperty != NULL );
	assert( Name != NULL );
	
	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_CHECK_TYPE;
	pgrProperty->Data.Bool = Value;
	pgrProperty->DataSize = sizeof( int );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillRadio
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillRadio( grProperty * pgrProperty, char *Name, int Value, int FieldId )
{
	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_RADIO_TYPE;
	pgrProperty->Data.Bool = Value;
	pgrProperty->DataSize = sizeof( int );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillVec3dGroup
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillVec3dGroup( grProperty * pgrProperty, char *Name, const grVec3d *Vector, int FieldId )
{

	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_VEC3D_GROUP_TYPE;
	pgrProperty->Data.Vector = *Vector;
	pgrProperty->DataSize = sizeof( grVec3d );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillColorGroup
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillColorGroup( grProperty * pgrProperty, char *Name, const grVec3d *Vector, int FieldId )
{

	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_COLOR_GROUP_TYPE;
	pgrProperty->Data.Vector = *Vector;
	pgrProperty->DataSize = sizeof( grVec3d );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillFloat
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillFloat( grProperty * pgrProperty, char *Name, float Float, int FieldId, float Min, float Max, float Increment )
{

	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_FLOAT_TYPE;
	pgrProperty->Data.Float = Float;
	pgrProperty->DataSize = sizeof( float );
	pgrProperty->DataId = FieldId;
	pgrProperty->TypeInfo.NumInfo.Min = Min;
	pgrProperty->TypeInfo.NumInfo.Max = Max;
	pgrProperty->TypeInfo.NumInfo.Increment = Increment;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillInt
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillInt( grProperty * pgrProperty, char *Name, int Int, int FieldId, float Min, float Max, float Increment )
{
	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_INT_TYPE;
	pgrProperty->Data.Int = Int;
	pgrProperty->DataSize = sizeof( int );
	pgrProperty->DataId = FieldId;
	pgrProperty->TypeInfo.NumInfo.Min = Min;
	pgrProperty->TypeInfo.NumInfo.Max = Max;
	pgrProperty->TypeInfo.NumInfo.Increment = Increment;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillInt
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillStaticInt( grProperty * pgrProperty, char *Name, int Int, int FieldId )
{
	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_STATIC_INT_TYPE;
	pgrProperty->Data.Int = Int;
	pgrProperty->DataSize = sizeof( int );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillString
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillString( grProperty * pgrProperty, char *Name, char *String, int FieldId )
{
	assert( pgrProperty != NULL );
	assert( Name != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_STRING_TYPE;
	pgrProperty->Data.String = String;
	pgrProperty->DataSize = sizeof( char* );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}


//========================================================================================================
//	grProperty_FillGroupEnd
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillGroupEnd( grProperty * pgrProperty, int FieldId )
{
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = NULL;
	pgrProperty->Type = PROPERTY_GROUP_END_TYPE;
	pgrProperty->DataSize = PROPERTY_DATA_INVALID;
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillColorPicker
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillColorPicker( grProperty * pgrProperty, char *Name,  grVec3d *Vector, int FieldId )
{
	assert( Name != NULL );
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_COLOR_PICKER_TYPE;
	pgrProperty->Data.Vector = *Vector;
	pgrProperty->DataSize = sizeof( grVec3d );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}
//========================================================================================================
//	grProperty_FillCombo
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillCombo( grProperty *pgrProperty, char *Name,  char * Select, int FieldId, int StringN, char **StringList )
{
	assert( Name != NULL );
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = 	PROPERTY_COMBO_TYPE	;
	pgrProperty->Data.String = Select;
	pgrProperty->DataSize = sizeof( int );
	pgrProperty->DataId = FieldId;
	pgrProperty->TypeInfo.ComboInfo.StringN = StringN;
	pgrProperty->TypeInfo.ComboInfo.StringList = StringList;

	return( GR_TRUE );
}
//========================================================================================================
//	grProperty_FillGroup
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillGroup( grProperty * pgrProperty, char *Name, int FieldId )
{

	assert( Name != NULL );
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_GROUP_TYPE;
	pgrProperty->DataSize = PROPERTY_DATA_INVALID;
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillTimeGroup
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillTimeGroup( grProperty *pgrProperty, char *Name, int FieldId )
{

	assert( Name != NULL );
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_TIME_GROUP_TYPE;
	pgrProperty->DataSize = PROPERTY_DATA_INVALID;
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillButton
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillButton( grProperty *pgrProperty, char *Name, int FieldId )
{

	assert( Name != NULL );
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->FieldName = Util_StrDup( Name );
	pgrProperty->Type = PROPERTY_BUTTON_TYPE;
	pgrProperty->DataSize = PROPERTY_DATA_INVALID;
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillVoid
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillVoid( grProperty *pgrProperty, PROPERTY_FIELD_TYPE Type, void *Pointer, int FieldId )
{
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->Type = Type;
	pgrProperty->Data.Ptr = Pointer;
	pgrProperty->DataSize = sizeof( void * );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}

//========================================================================================================
//	grProperty_FillCurTime
//========================================================================================================
GRAPI grBoolean GRCC grProperty_FillCurTime( grProperty *pgrProperty, float Time, int FieldId )
{
	assert( pgrProperty != NULL );

	memset( pgrProperty, 0, sizeof( grProperty ) );
	pgrProperty->Type = PROPERTY_CURTIME_TYPE;
	pgrProperty->Data.Float = Time;
	pgrProperty->DataSize = sizeof( float );
	pgrProperty->DataId = FieldId;
	return( GR_TRUE );
}


//========================================================================================================
//	grProperty_DataEqual
//========================================================================================================
static grBoolean grProperty_DataEqual( grProperty *pgrProperty, grProperty *pgrProperty2 )
{
	grBoolean Result = GR_FALSE;

	if( pgrProperty->Type != pgrProperty2->Type )
		return( GR_FALSE );

	if( pgrProperty->DataSize == PROPERTY_DATA_INVALID )
		return( GR_FALSE );

	if( pgrProperty2->DataSize == PROPERTY_DATA_INVALID )
		return( GR_FALSE );

	switch( pgrProperty->Type )
	{
	case PROPERTY_STRING_TYPE:
	case PROPERTY_COMBO_TYPE:
		Result = !strcmp( pgrProperty->Data.String, pgrProperty2->Data.String);
		break;

	case PROPERTY_INT_TYPE:
		Result = (pgrProperty->Data.Int == pgrProperty2->Data.Int);
		break;

	case PROPERTY_FLOAT_TYPE:
		Result = (pgrProperty->Data.Float == pgrProperty2->Data.Float);
		break;

	case PROPERTY_CHECK_TYPE:
	case PROPERTY_RADIO_TYPE:
		Result = (pgrProperty->Data.Bool == pgrProperty2->Data.Bool);
		break;

	case PROPERTY_VEC3D_GROUP_TYPE:
	case PROPERTY_COLOR_GROUP_TYPE:
		Result = grVec3d_Compare( &pgrProperty->Data.Vector, &pgrProperty->Data.Vector, 0.0);
		break;

	case PROPERTY_GROUP_TYPE:
	case PROPERTY_GROUP_END_TYPE:
	case PROPERTY_COLOR_PICKER_TYPE:
	case PROPERTY_TIME_GROUP_TYPE:
	case PROPERTY_CHANNEL_POS_TYPE:
	case PROPERTY_CHANNEL_EVENT_TYPE:
	case PROPERTY_CHANNEL_ROT_TYPE:
	case PROPERTY_CURTIME_TYPE:
	case PROPERTY_STATIC_INT_TYPE:
		Result = GR_FALSE;
		break;

	default:
		assert( 0);
	}
	return( Result );
}

//========================================================================================================
//	grProperty_ListDestroy
//========================================================================================================
GRAPI void GRCC grProperty_ListDestroy( grProperty_List **pArray )
{
	int i;

	assert(pArray);
	assert(*pArray);

	if((*pArray)->pgrProperty != NULL )
	{
		for( i = 0; i < (*pArray)->grPropertyN; i++ )
		{
			if((*pArray)->pgrProperty[i].FieldName != NULL )
				grRam_Free((*pArray)->pgrProperty[i].FieldName );
		}
		grRam_Free((*pArray)->pgrProperty );
	}

	grRam_Free(*pArray);

	*pArray = NULL;
}

//========================================================================================================
//	grProperty_ListMerge
//========================================================================================================
GRAPI grProperty_List * GRCC grProperty_ListMerge( grProperty_List *pArray, grProperty_List *pArray2, int bSameType )
{
	int i, j;
	int NewArrayCnt = 0;
	int NewArraySize;
	grProperty_List *pNewArray;
	grProperty *pgrProperty;
	grProperty *pgrProperty2;
	grProperty *pNewgrProperty;

	if( pArray->grPropertyN > pArray2->grPropertyN )
		NewArraySize =  pArray->grPropertyN;
	else
		NewArraySize =  pArray2->grPropertyN;

	pNewArray = grProperty_ListCreate( NewArraySize );
	if( pNewArray == NULL )
		return( NULL );

	pgrProperty  = pArray->pgrProperty;
	pgrProperty2 = pArray2->pgrProperty;
	pNewgrProperty = pNewArray->pgrProperty;
	for( i = 0;i < pArray->grPropertyN ; i++ )
	{
		for( j = 0; j <  pArray2->grPropertyN ; j++ )
		{
			if( pgrProperty[i].DataId == pgrProperty2[j].DataId )
			{
				if( !bSameType && (pgrProperty[i].DataId >= PROPERTY_LOCAL_DATATYPE_START) )
					break;
				pNewgrProperty[NewArrayCnt] = pgrProperty[i];
				if( pgrProperty[i].FieldName  != NULL )
					pNewgrProperty[NewArrayCnt].FieldName = Util_StrDup( pgrProperty[i].FieldName );
				if( !grProperty_DataEqual( &pgrProperty[i], &pgrProperty2[j] ) )
					pNewgrProperty[NewArrayCnt].DataSize = PROPERTY_DATA_INVALID;
				NewArrayCnt++;
				break;
			}
		}
	}
	pNewArray->grPropertyN = NewArrayCnt;
	pNewArray->bDirty = pArray->bDirty | pArray2->bDirty ;
	return( pNewArray );

}

//========================================================================================================
//	grProperty_ListFindByDataId
//========================================================================================================
GRAPI grProperty * GRCC grProperty_ListFindByDataId(  grProperty_List *pArray, int FieldId )
{
	int i;

	for( i = 0; i < pArray->grPropertyN; i++ )
	{
		if( pArray->pgrProperty[i].DataId == FieldId )
			return( &pArray->pgrProperty[i] );
	}
	return( NULL );
}

//========================================================================================================
//	grProperty_SetDataInvalid
//========================================================================================================
GRAPI void GRCC grProperty_SetDataInvalid( grProperty *pgrProperty )
{
	pgrProperty->DataSize = PROPERTY_DATA_INVALID;
}

//========================================================================================================
//	grProperty_SetDisabled
//========================================================================================================
GRAPI void GRCC grProperty_SetDisabled( grProperty *pgrProperty, grBoolean bDisable )
{
	pgrProperty->bDisabled = bDisable;
}
