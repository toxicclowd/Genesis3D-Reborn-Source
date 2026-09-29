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

typedef struct jeProperty_NumInfo {
	float	  Min;
	float	  Max;
	float	  Increment;
} grProperty_NumInfo;

typedef struct jeProperty_ComboInfo {
	int StringN;
	char ** StringList;
} grProperty_ComboInfo;

typedef union grProperty_TypeInfo {
	grProperty_NumInfo NumInfo;
	grProperty_ComboInfo ComboInfo;
} grProperty_TypeInfo;

typedef struct jeProperty {
	char * FieldName;
	PROPERTY_FIELD_TYPE Type;
	int	DataSize;
	grProperty_Data Data;
	int		  DataId;
	grBoolean bDisabled;
	grProperty_TypeInfo TypeInfo;
} grProperty;

typedef struct jeProperty_List {
	union {
		int	grPropertyN;
		int	jePropertyN;
	};
	union {
		grProperty * pgrProperty;
		grProperty * pjeProperty;
	};
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
GRAPI grBoolean GRCC grProperty_Append( grProperty_List *pArray, grProperty *pjeProperty );
GRAPI grProperty_List * GRCC grProperty_ListMerge( grProperty_List *pArray, grProperty_List *pArray2, int bSameType );
GRAPI grProperty * GRCC grProperty_ListFindByDataId(  grProperty_List *pArray, int FieldId );
GRAPI void GRCC grProperty_ListDestroy( grProperty_List **pArray );
GRAPI grBoolean GRCC grProperty_FillGroup( grProperty *pjeProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillButton( grProperty *pjeProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillCheck( grProperty *pjeProperty, char *Name, int Value, int FieldId );
GRAPI grBoolean GRCC grProperty_FillRadio( grProperty *pjeProperty, char *Name, int Value, int FieldId );
GRAPI grBoolean GRCC grProperty_FillVec3dGroup( grProperty *pjeProperty, char *Name, const grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillColorGroup( grProperty *pjeProperty, char *Name, const grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillTimeGroup( grProperty *pjeProperty, char *Name, int FieldId );
GRAPI grBoolean GRCC grProperty_FillFloat( grProperty *pjeProperty, char *Name, float Float, int FieldId, float Min, float Max, float Increment );
GRAPI grBoolean GRCC grProperty_FillInt( grProperty *pjeProperty, char *Name, int Int, int FieldId, float Min, float Max, float Increment );
GRAPI grBoolean GRCC grProperty_FillStaticInt( grProperty *pjeProperty, char *Name, int Int, int FieldId );
GRAPI grBoolean GRCC grProperty_FillString( grProperty *pjeProperty, char *Name, char *String, int FieldId );
GRAPI grBoolean GRCC grProperty_FillVoid( grProperty *pjeProperty, PROPERTY_FIELD_TYPE Type, void *Pointer, int FieldId );
GRAPI grBoolean GRCC grProperty_FillGroupEnd( grProperty *pjeProperty, int FieldId);
GRAPI grBoolean GRCC grProperty_FillColorPicker( grProperty *pjeProperty, char *Name,  grVec3d *Vector, int FieldId );
GRAPI grBoolean GRCC grProperty_FillCombo( grProperty *pjeProperty, char *Name,  char * Select, int FieldId, int StringN, char **StringList );
GRAPI grBoolean GRCC grProperty_FillCurTime( grProperty *pjeProperty, float Time, int FieldId );
GRAPI void GRCC grProperty_SetDataInvalid( grProperty *pjeProperty );
GRAPI void GRCC grProperty_SetDisabled( grProperty *pjeProperty, grBoolean bDisable );

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

typedef grProperty_Data jeProperty_Data;
typedef grProperty_NumInfo jeProperty_NumInfo;
typedef grProperty_ComboInfo jeProperty_ComboInfo;
typedef grProperty_TypeInfo jeProperty_TypeInfo;
typedef grProperty jeProperty;
typedef grProperty_List jeProperty_List;

#define jeProperty_Append                        grProperty_Append
#define jeProperty_FillButton                    grProperty_FillButton
#define jeProperty_FillCheck                     grProperty_FillCheck
#define jeProperty_FillColorGroup                grProperty_FillColorGroup
#define jeProperty_FillColorPicker               grProperty_FillColorPicker
#define jeProperty_FillCombo                     grProperty_FillCombo
#define jeProperty_FillCurTime                   grProperty_FillCurTime
#define jeProperty_FillFloat                     grProperty_FillFloat
#define jeProperty_FillGroup                     grProperty_FillGroup
#define jeProperty_FillGroupEnd                  grProperty_FillGroupEnd
#define jeProperty_FillInt                       grProperty_FillInt
#define jeProperty_FillRadio                     grProperty_FillRadio
#define jeProperty_FillStaticInt                 grProperty_FillStaticInt
#define jeProperty_FillString                    grProperty_FillString
#define jeProperty_FillTimeGroup                 grProperty_FillTimeGroup
#define jeProperty_FillVec3dGroup                grProperty_FillVec3dGroup
#define jeProperty_FillVoid                      grProperty_FillVoid
#define jeProperty_ListConCat                    grProperty_ListConCat
#define jeProperty_ListCopy                      grProperty_ListCopy
#define jeProperty_ListCreate                    grProperty_ListCreate
#define jeProperty_ListCreateEmpty               grProperty_ListCreateEmpty
#define jeProperty_ListDestroy                   grProperty_ListDestroy
#define jeProperty_ListFindByDataId              grProperty_ListFindByDataId
#define jeProperty_ListMerge                     grProperty_ListMerge
#define jeProperty_SetDataInvalid                grProperty_SetDataInvalid
#define jeProperty_SetDisabled                   grProperty_SetDisabled

#endif // GENESIS_NO_JET_COMPAT

#endif 
