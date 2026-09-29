/****************************************************************************************/
/*  JEPROPERTY.H                                                                        */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D: Reborn Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D: Reborn, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
// grProperty.h : header file
//
//

#ifndef GR_PROPERTY_H
#define GR_PROPERTY_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include "Vec3d.h"
#define PROPERTY_LOCAL_DATATYPE_START 1000
#define PROPERTY_DATA_INVALID			0
typedef union grProperty_Data {
	char * String;
	int   Bool;
	float Float;
	int	  Int;
	void * Ptr;
	grVec3d Vector;
} grProperty_Data;

typedef enum {
	PROPERTY_STRING_TYPE,	//Uses no Type Info
	PROPERTY_INT_TYPE,		//Uses NumInfo
	PROPERTY_FLOAT_TYPE,	//Uses NumIn
	PROPERTY_CHECK_TYPE,	//Uses no Type Info
	PROPERTY_RADIO_TYPE,	//Uses no Type Info
	PROPERTY_GROUP_TYPE,	//Uses no Type Info
	PROPERTY_VEC3D_GROUP_TYPE,	//Uses no Type Info
	PROPERTY_COLOR_GROUP_TYPE,	//Uses no Type Info
	PROPERTY_GROUP_END_TYPE,	//Uses no Type Info
	PROPERTY_COLOR_PICKER_TYPE,	//Uses no Type Info
	PROPERTY_BUTTON_TYPE,		//Uses no Type Info
	PROPERTY_COMBO_TYPE	,		//Uses ComboInfo
	PROPERTY_TIME_GROUP_TYPE,	//Uses no Type Info
	PROPERTY_CHANNEL_POS_TYPE,		//Uses no Type Info
	PROPERTY_CHANNEL_EVENT_TYPE,	//Uses no Type Info
	PROPERTY_CHANNEL_ROT_TYPE,		//Uses no Type Info
	PROPERTY_CURTIME_TYPE,			//Uses no Type Info
	PROPERTY_STATIC_INT_TYPE,		//Uses no Type Info
	PROPERTY_LAST
} PROPERTY_FIELD_TYPE;

typedef struct grProperty_NumInfo {
	float	  Min;
	float	  Max;
	float	  Increment;
} grProperty_NumInfo;

typedef struct grProperty_ComboInfo {
	int StringN;
	char ** StringList;
} grProperty_ComboInfo;

typedef union grProperty_TypeInfo {
	grProperty_NumInfo NumInfo;
	grProperty_ComboInfo ComboInfo;
} grProperty_TypeInfo;

typedef struct grProperty {
	char * FieldName;
	PROPERTY_FIELD_TYPE Type;
	int	DataSize;
	grProperty_Data Data;
	int		  DataId;
	grBoolean bDisabled;
	grProperty_TypeInfo TypeInfo;
} grProperty;

typedef struct grProperty_List {
	int	grPropertyN;
	grProperty * pgrProperty;
	grBoolean	bDirty;			//This is used to comunicate that the list has been changed.
								//It is used by the editor when a property list is built to update data.
								//if the the bDirty is set it rebuild dialog instead of just updating data
} grProperty_List;

//========================================================================================================
//========================================================================================================
GRAPI grProperty_List * GRCC grProperty_ListCreate( int FieldN );
GRAPI grProperty_List * GRCC grProperty_ListCreateEmpty();
GRAPI grProperty_List * GRCC grProperty_ListCopy( grProperty_List * pArray);
GRAPI grProperty_List * GRCC grProperty_ListConCat( grProperty_List * pArray, grProperty_List *pArray2 );
GRAPI grBoolean GRCC grProperty_Append( grProperty_List *pArray, grProperty *pgrProperty );
GRAPI grProperty_List * GRCC grProperty_ListMerge( grProperty_List *pArray, grProperty_List *pArray2, int bSameType );
GRAPI grProperty * GRCC grProperty_ListFindByDataId(  grProperty_List *pArray, int FieldId );
GRAPI void GRCC grProperty_ListDestroy( grProperty_List **pArray );
GRAPI grBoolean GRCC grProperty_FillGroup( grProperty *pgrProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillButton( grProperty *pgrProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillCheck( grProperty *pgrProperty, char *Name, int Value, int FieldId );
GRAPI grBoolean GRCC grProperty_FillRadio( grProperty *pgrProperty, char *Name, int Value, int FieldId );
GRAPI grBoolean GRCC grProperty_FillVec3dGroup( grProperty *pgrProperty, char *Name, const grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillColorGroup( grProperty *pgrProperty, char *Name, const grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillTimeGroup( grProperty *pgrProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillFloat( grProperty *pgrProperty, char *Name, float Float, int FieldId, float Min, float Max, float Increment );
GRAPI grBoolean GRCC grProperty_FillInt( grProperty *pgrProperty, char *Name, int Int, int FieldId, float Min, float Max, float Increment );
GRAPI grBoolean GRCC grProperty_FillStaticInt( grProperty *pgrProperty, char *Name, int Int, int FieldId );
GRAPI grBoolean GRCC grProperty_FillString( grProperty *pgrProperty, char *Name, char *String, int FieldId );
GRAPI grBoolean GRCC grProperty_FillVoid( grProperty *pgrProperty, PROPERTY_FIELD_TYPE Type, void *Pointer, int FieldId );
GRAPI grBoolean GRCC grProperty_FillGroupEnd( grProperty *pgrProperty, int FieldId);
GRAPI grBoolean GRCC grProperty_FillColorPicker( grProperty *pgrProperty, char *Name,  grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillCombo( grProperty *pgrProperty, char *Name,  char * Select, int FieldId, int StringN, char **StringList );
GRAPI grBoolean GRCC grProperty_FillCurTime( grProperty *pgrProperty, float Time, int FieldId );
GRAPI void GRCC grProperty_SetDataInvalid( grProperty *pgrProperty );
GRAPI void GRCC grProperty_SetDisabled( grProperty *pgrProperty, grBoolean bDisable );

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif 
