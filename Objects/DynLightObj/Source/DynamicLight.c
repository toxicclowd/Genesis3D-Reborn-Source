/****************************************************************************************/
/*  DYNAMICLIGHT.C                                                                      */
/*                                                                                      */
/*  Author:  Peter Siamidis                                                             */
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
#pragma warning ( disable : 4115 )
#include <windows.h>
#pragma warning ( default : 4115 )
#endif

#ifdef BUILD_BE
#include <image.h>
#include <Resources.h>
#endif

#include <assert.h>
#include <float.h>
#include "VFile.h"
#include "grProperty.h"
#include "Ram.h"
#include "grResource.h"
#include "grWorld.h"
#include "DynamicLight.h"
#include "Resource.h"
#include "Errorlog.h"


#define DYNAMICLIGHT_VERSION 1

//Royce
#define OBJ_PERSIST_SIZE 5000
//---

////////////////////////////////////////////////////////////////////////////////////////
//	Property list stuff
////////////////////////////////////////////////////////////////////////////////////////
enum
{

	//
	DYNAMICLIGHT_RADIUS_ID = PROPERTY_LOCAL_DATATYPE_START,
	DYNAMICLIGHT_BRIGHTNESS_ID,

	// color
	DYNAMICLIGHT_COLORGROUP_ID,
	DYNAMICLIGHT_COLOR_ID,
	DYNAMICLIGHT_COLORRED_ID,
	DYNAMICLIGHT_COLORGREEN_ID,
	DYNAMICLIGHT_COLORBLUE_ID,
	DYNAMICLIGHT_COLORGROUPEND_ID,

	//
	DYNAMICLIGHT_CASTSHADOW_ID,
	DYNAMICLIGHT_LAST_ID

};
enum
{

	//
	DYNAMICLIGHT_RADIUS_INDEX = 0,
	DYNAMICLIGHT_BRIGHTNESS_INDEX,

	// color
	DYNAMICLIGHT_COLORGROUP_INDEX,
	DYNAMICLIGHT_COLOR_INDEX,
	DYNAMICLIGHT_COLORRED_INDEX,
	DYNAMICLIGHT_COLORGREEN_INDEX,
	DYNAMICLIGHT_COLORBLUE_INDEX,
	DYNAMICLIGHT_COLORGROUPEND_INDEX,

	//
	DYNAMICLIGHT_CASTSHADOW_INDEX,
	DYNAMICLIGHT_LAST_INDEX

};


////////////////////////////////////////////////////////////////////////////////////////
//	Globals
////////////////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
static HINSTANCE		hClassInstance = NULL;
#endif

#ifdef BUILD_BE
static image_id			hClassInstance = NULL;
#endif

static grProperty		DynamicLightProperties[DYNAMICLIGHT_LAST_INDEX];
static grProperty_List	DynamicLightPropertyList = { DYNAMICLIGHT_LAST_INDEX, &( DynamicLightProperties[0] ) };


////////////////////////////////////////////////////////////////////////////////////////
//	Defaults
////////////////////////////////////////////////////////////////////////////////////////
#define DYNAMICLIGHT_DEFAULT_RADIUS			50.0f
#define DYNAMICLIGHT_DEFAULT_BRIGHTNESS		20.0f
#define DYNAMICLIGHT_DEFAULT_COLORRED		128.0f
#define DYNAMICLIGHT_DEFAULT_COLORGREEN		128.0f
#define DYNAMICLIGHT_DEFAULT_COLORBLUE		128.0f
#define DYNAMICLIGHT_DEFAULT_CASTSHADOW		GR_FALSE


////////////////////////////////////////////////////////////////////////////////////////
//	Object data
////////////////////////////////////////////////////////////////////////////////////////
typedef struct DynamicLight
{
	grWorld			*World;
	grResourceMgr	*ResourceMgr;
	grEngine		*Engine;
	int				RefCount;
	grXForm3d		Xf;
	grLight			*Light;
	grVec3d			Color;
	float			Radius;
	float			Brightness;
	grBoolean		CastShadow;
	grBoolean		LoadedFromDisk;

} DynamicLight;



////////////////////////////////////////////////////////////////////////////////////////
//
//	DynamicLight_Destroy()
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean DynamicLight_Destroy(
	DynamicLight	*Object )	// object from which light will be created
{

	// ensure valid data
	assert( Object != NULL );

	// remove light from world
	assert( Object->Light != NULL );
	assert( Object->World != NULL );
	if ( grWorld_RemoveDLight( Object->World, Object->Light ) == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		return GR_FALSE;
	}

	// destroy light
	grLight_Destroy( &Object->Light );

	// all done
	return GR_TRUE;

} // DynamicLight_Destroy()



////////////////////////////////////////////////////////////////////////////////////////
//
//	DynamicLight_Create()
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean DynamicLight_Create(
	DynamicLight	*Object )	// object from which light will be created
{

	// locals
	grBoolean	Result;

	// ensure valid data
	assert( Object != NULL );

	// create light
	Object->Light = grLight_Create();
	if ( Object->Light == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		return GR_FALSE;
	}

	// set light attributes
	Result = grLight_SetAttributes(	Object->Light,
									&( Object->Xf.Translation ),
									&( Object->Color ),
									Object->Radius, 
									Object->Brightness, 
									GR_LIGHT_FLAG_FAST_LIGHTING_MODEL | (Object->CastShadow ? GR_LIGHT_FLAG_CAST_SHADOWS : 0) );
	

	if ( Result == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		grLight_Destroy( &( Object->Light ) );
		return GR_FALSE;
	}

	// add it to the world
	Result = grWorld_AddDLight( Object->World, Object->Light );
	if ( Result == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		grLight_Destroy( &( Object->Light ) );
		return GR_FALSE;
	}

	// all done
	return GR_TRUE;

} // DynamicLight_Create()




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

#include <stdio.h>
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
 
	printf("Read %s\n" , rcbuffer);
	// return the allocated string
	return (rcbuffer);
}//Util_LoadLibraryString

#endif


////////////////////////////////////////////////////////////////////////////////////////
//
//	Init_Class()
//
////////////////////////////////////////////////////////////////////////////////////////
void Init_Class(
#ifdef WIN32
	HINSTANCE	hInstance )	// dll instance handle
#endif
#ifdef BUILD_BE
	image_id	hInstance )	// dll instance handle
#endif
{
#ifdef WIN32
	// ensure valid data
	assert( hInstance != NULL );
#endif
#ifdef BUILD_BE
	// ensure valid data
	assert( hInstance > 0 );
#endif
	// save hinstance
	hClassInstance = hInstance;

	// setup radius property
	grProperty_FillFloat(	&( DynamicLightProperties[DYNAMICLIGHT_RADIUS_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_RADIUS ),
							DYNAMICLIGHT_DEFAULT_RADIUS,
							DYNAMICLIGHT_RADIUS_ID,
							0.1f, FLT_MAX, 5.0f );

	// setup brightness property
	grProperty_FillFloat(	&( DynamicLightProperties[DYNAMICLIGHT_BRIGHTNESS_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_BRIGHTNESS ),
							DYNAMICLIGHT_DEFAULT_BRIGHTNESS,
							DYNAMICLIGHT_BRIGHTNESS_ID,
							0.1f, FLT_MAX, 5.0f );


	////////////////////////////////////////////////////////////////////////////////////////
	//	Color properties
	////////////////////////////////////////////////////////////////////////////////////////

	// start color group
	grProperty_FillGroup(	&( DynamicLightProperties[DYNAMICLIGHT_COLORGROUP_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_COLORGROUP ),
							DYNAMICLIGHT_COLORGROUP_INDEX );

	// setup color property
	{
		grVec3d	Color = { DYNAMICLIGHT_DEFAULT_COLORRED, DYNAMICLIGHT_DEFAULT_COLORGREEN, DYNAMICLIGHT_DEFAULT_COLORBLUE };
		grProperty_FillColorPicker(	&( DynamicLightProperties[DYNAMICLIGHT_COLOR_INDEX] ),
									Util_LoadLibraryString( hClassInstance, IDS_COLOR ),
									&Color,
									DYNAMICLIGHT_COLOR_ID );
	}

	// setup color red property
	grProperty_FillFloat(	&( DynamicLightProperties[DYNAMICLIGHT_COLORRED_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_COLORRED ),
							DYNAMICLIGHT_DEFAULT_COLORRED,
							DYNAMICLIGHT_COLORRED_ID,
							0.0f, 255.0f, 1.0f );

	// setup ambient light green property
	grProperty_FillFloat(	&( DynamicLightProperties[DYNAMICLIGHT_COLORGREEN_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_COLORGREEN ),
							DYNAMICLIGHT_DEFAULT_COLORGREEN,
							DYNAMICLIGHT_COLORGREEN_ID,
							0.0f, 255.0f, 1.0f );

	// setup ambient light blue property
	grProperty_FillFloat(	&( DynamicLightProperties[DYNAMICLIGHT_COLORBLUE_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_COLORBLUE ),
							DYNAMICLIGHT_DEFAULT_COLORBLUE,
							DYNAMICLIGHT_COLORBLUE_ID,
							0.0f, 255.0f, 1.0f );

	// end color group
	grProperty_FillGroupEnd( &( DynamicLightProperties[DYNAMICLIGHT_COLORGROUPEND_INDEX] ), DYNAMICLIGHT_COLORGROUPEND_ID );


	////////////////////////////////////////////////////////////////////////////////////////
	//	Misc properties
	////////////////////////////////////////////////////////////////////////////////////////

	// setup cast shadow flag
	grProperty_FillCheck(	&( DynamicLightProperties[DYNAMICLIGHT_CASTSHADOW_INDEX] ),
							Util_LoadLibraryString( hClassInstance, IDS_CASTSHADOW ),
							DYNAMICLIGHT_DEFAULT_CASTSHADOW,
							DYNAMICLIGHT_CASTSHADOW_ID );

	// final init
	DynamicLightPropertyList.grPropertyN = DYNAMICLIGHT_LAST_INDEX;

} // Init_Class()

////////////////////////////////////////////////////////////////////////////////////////
//
//	DeInit_Class()
//
////////////////////////////////////////////////////////////////////////////////////////
void DeInit_Class(
	void )	// no parameters
{

	// zap instance pointer
	hClassInstance = NULL;

} // DeInit_Class()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CreateInstance()
//
////////////////////////////////////////////////////////////////////////////////////////
void * GRCC CreateInstance(
	void )	// no parameters
{

	// locals
	DynamicLight	*Object;

	// allocate struct
	Object = (DynamicLight *)grRam_AllocateClear( sizeof( *Object ) );
	if ( Object == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return NULL;
	}

	// get default settings
	Object->Radius = DYNAMICLIGHT_DEFAULT_RADIUS;
	Object->Brightness = DYNAMICLIGHT_DEFAULT_BRIGHTNESS;
	Object->Color.X = DYNAMICLIGHT_DEFAULT_COLORRED;
	Object->Color.Y = DYNAMICLIGHT_DEFAULT_COLORGREEN;
	Object->Color.Z = DYNAMICLIGHT_DEFAULT_COLORBLUE;
	Object->CastShadow = DYNAMICLIGHT_DEFAULT_CASTSHADOW;

	// init remaining fields
	grXForm3d_SetIdentity( &Object->Xf );
	Object->RefCount = 1;

	// all done
	return Object;

} // CreateInstance()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CreateRef()
//
////////////////////////////////////////////////////////////////////////////////////////
void GRCC CreateRef(
	void	*Instance )	// instance data
{

	// locals
	DynamicLight	*Object;
	
	// get object
	Object = (DynamicLight *)Instance;
	assert( Object != NULL );

	// adjust object ref count
	Object->RefCount++;

} // CreateRef()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Destroy()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Destroy(
	void	**Instance )	// pointer to instance data
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );

	// get object
	Object = (DynamicLight *)*Instance;
	assert( Object != NULL );
	assert( Object->RefCount > 0 );

	// do nothing if ref count is not at zero
	Object->RefCount--;
	if ( Object->RefCount > 0 )
	{
		return GR_FALSE;
	}

	// make sure everything has been properly destroyed
	assert( Object->World == NULL );
	assert( Object->Engine == NULL );
	assert( Object->ResourceMgr == NULL );
	assert( Object->Light == NULL );

	// free struct
	grRam_Free( Object );

	// zap pointer
	*Instance = NULL;

	// all done
	return GR_TRUE;

} // Destroy()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Render()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Render(
	const void				*Instance,	// object instance data
	const grWorld			*World,		// world
	const grEngine			*Engine,	// engine
	const grCamera			*Camera,				// camera
	const grFrustum			*CameraSpaceFrustum, 	// frustum
	grObject_RenderFlags	RenderFlags)
{

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	World;
	Engine;
	Camera;
	CameraSpaceFrustum;
	RenderFlags;
} // Render()



////////////////////////////////////////////////////////////////////////////////////////
//
//	AttachWorld()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC AttachWorld(
	void	*Instance,	// object instance data
	grWorld	*World )	// world
{

	// locals
	DynamicLight	*Object;

	
	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Object = (DynamicLight *)Instance;

	// save world pointer
	Object->World = World;

	// save an instance of the resource manager
	Object->ResourceMgr = grWorld_GetResourceMgr( World );
	assert( Object->ResourceMgr != NULL );

	// create light
	if ( DynamicLight_Create( Object ) == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		grResource_MgrDestroy( &( Object->ResourceMgr ) );
		Object->World = NULL;
		return GR_FALSE;
	}

	// all done
	return GR_TRUE;

} // AttachWorld()



////////////////////////////////////////////////////////////////////////////////////////
//
//	DettachWorld()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC DettachWorld(
	void	*Instance,	// object instance data
	grWorld	*World )	// world
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Object = (DynamicLight *)Instance;
	assert( Object->World == World );

	// destroy light
	DynamicLight_Destroy( Object );

	// destroy our instance of the resource manager
	grResource_MgrDestroy( &( Object->ResourceMgr ) );

	// zap world pointer
	Object->World = NULL;

	// all done
	return GR_TRUE;

	// eliminate warnings
	World;

} // DettachWorld()



////////////////////////////////////////////////////////////////////////////////////////
//
//	AttachEngine()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC AttachEngine(
	void		*Instance,	// object instance data
	grEngine	*Engine )	// engine
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// save engine pointer
	Object->Engine = Engine;

	// all done
	return GR_TRUE;

} // AttachEngine()



////////////////////////////////////////////////////////////////////////////////////////
//
//	DettachEngine()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC DettachEngine(
	void		*Instance,	// object instance data
	grEngine	*Engine )	// engine
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Object = (DynamicLight *)Instance;
	assert( Object->Engine == Engine );

	// zap engine pointer
	Object->Engine = NULL;

	// all done
	return GR_TRUE;

	// eliminate warnings
	Engine;

} // DettachEngine()



////////////////////////////////////////////////////////////////////////////////////////
//
//	AttachSoundSystem()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC AttachSoundSystem(
	void			*Instance,		// object instance data
	grSound_System	*SoundSystem )	// sound system
{

	// ensure valid data
	assert( Instance != NULL );
	assert( SoundSystem != NULL );

	// all done
	return GR_TRUE;

	// elminate warnings
	Instance;
	SoundSystem;

} // AttachSoundSystem()



////////////////////////////////////////////////////////////////////////////////////////
//
//	DettachSoundSystem()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC DettachSoundSystem(
	void			*Instance,		// object instance data
	grSound_System	*SoundSystem )	// sound system
{

	// ensure valid data
	assert( Instance != NULL );
	assert( SoundSystem != NULL );

	// all done
	return GR_TRUE;

	// elminate warnings
	Instance;
	SoundSystem;

} // DettachSoundSystem()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Collision()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Collision(
	const grObject	*Object,
	const grExtBox	*Box,
	const grVec3d	*Front,
	const grVec3d	*Back,
	grVec3d			*Impact,
	grPlane			*Plane )
{

	// ensure valid data
	assert( Object != NULL );
	//assert( Box != NULL );  Removed by Incarnadine.  Box CAN be NULL.
	assert( Front != NULL );
	assert( Back != NULL );
	//assert( Impact != NULL ); Removed by Icestorm. Impact&Plane CAN be NULL.
	//assert( Plane != NULL );

	// all done
	return GR_FALSE;

	// eliminate warnings
	Object;
	Box;
	Front;
	Back;
	Impact;
	Plane;

} // Collision()



////////////////////////////////////////////////////////////////////////////////////////
//
//	GetExtBox()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC GetExtBox(
	const void	*Instance,	// object instance data
	grExtBox	*BBox )		// where to store extent box
{

	// locals
	DynamicLight	*Object;
	grVec3d			Pos;

	// ensure valid data
	assert( Instance != NULL );
	assert( BBox != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// save extent box
	Pos = Object->Xf.Translation;
	grExtBox_Set (  BBox, 
					Pos.X - 5.0f, Pos.Y - 5.0f, Pos.Z - 5.0f,
					Pos.X + 5.0f, Pos.Y + 5.0f, Pos.Z + 5.0f );

	// all done
	return GR_TRUE;

} // GetExtBox()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CreateFromFile()
//
///////////////////////////////////////////////////////////////////////////////////////
void * GRCC CreateFromFile(
	grVFile		*File,		// vfile to use
	grPtrMgr *PtrMgr )	// pointer manager
{

	// locals
	DynamicLight	*Object;
	grBoolean		Result = GR_TRUE;
	BYTE Version;
	uint32 Tag;
	
	// ensure valid data
	assert( File != NULL );

	// allocate struct
	Object = (DynamicLight *)grRam_AllocateClear( sizeof( *Object ) );
	if ( Object == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return NULL;
	}

	// init struct
	Object->RefCount = 1;
	Object->LoadedFromDisk = GR_TRUE;

	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
		goto ERROR_CreateFromFile;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
		    goto ERROR_CreateFromFile;
		}
	}
	else
	{
		Version = 1;
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
	}

	if (Version >= 1)
	{
	    // read data
	    Result &= grVFile_Read( File, &( Object->Xf ), sizeof( Object->Xf ) );
	    Result &= grVFile_Read( File, &( Object->Color ), sizeof( Object->Color ) );
        Result &= grVFile_Read( File, &( Object->Radius ), sizeof( Object->Radius ) );
        Result &= grVFile_Read( File, &( Object->Brightness ), sizeof( Object->Brightness ) );

		// fail if there was an error
	    if ( Result == GR_FALSE )
		{
		    grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
		    goto ERROR_CreateFromFile;
		}
	}

	// all done
	return Object;

	// handle errors
	ERROR_CreateFromFile:

	// free object
	grRam_Free( Object );

	// return error
	return NULL;
	PtrMgr;

} // CreateFromFile()


////////////////////////////////////////////////////////////////////////////////////////
//
//	WriteToFile()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC WriteToFile(
	const void	*Instance,
	grVFile		*File,
	grPtrMgr *PtrMgr )
{

	// locals
	DynamicLight	*Object;
	grBoolean		Result = GR_TRUE;
	BYTE Version = DYNAMICLIGHT_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	// ensure valid data
	assert( Instance != NULL );
	assert( File != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// write Version
	Result &= grVFile_Write( File, &Tag, sizeof(Tag));
	Result &= grVFile_Write( File, &Version, sizeof(Version) );


	// write xform
	Result &= grVFile_Write( File, &( Object->Xf ), sizeof( Object->Xf ) );

	// write color
	Result &= grVFile_Write( File, &( Object->Color ), sizeof( Object->Color ) );

	// write radius
	Result &= grVFile_Write( File, &( Object->Radius ), sizeof( Object->Radius ) );

	// write brightness
	Result &= grVFile_Write( File, &( Object->Brightness ), sizeof( Object->Brightness ) );

	// log errors
	if ( Result != GR_TRUE )
	{
		grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
	}

	// all done
	return Result;
	PtrMgr;

	// eliminate warnings
} // WriteToFile()


////////////////////////////////////////////////////////////////////////////////////////
//
//	GetPropertyList()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC GetPropertyList(
	void			*Instance,	// object instance data
	grProperty_List	**List)		// where to save property list pointer
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( List != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// setup property list
	DynamicLightProperties[DYNAMICLIGHT_RADIUS_INDEX].Data.Float = Object->Radius;
	DynamicLightProperties[DYNAMICLIGHT_BRIGHTNESS_INDEX].Data.Float = Object->Brightness;
	DynamicLightProperties[DYNAMICLIGHT_COLOR_INDEX].Data.Vector.X = Object->Color.X;
	DynamicLightProperties[DYNAMICLIGHT_COLOR_INDEX].Data.Vector.X = Object->Color.Y;
	DynamicLightProperties[DYNAMICLIGHT_COLOR_INDEX].Data.Vector.X = Object->Color.Z;
	DynamicLightProperties[DYNAMICLIGHT_COLORRED_INDEX].Data.Float = Object->Color.X;
	DynamicLightProperties[DYNAMICLIGHT_COLORGREEN_INDEX].Data.Float = Object->Color.Y;
	DynamicLightProperties[DYNAMICLIGHT_COLORBLUE_INDEX].Data.Float = Object->Color.Z;
	DynamicLightProperties[DYNAMICLIGHT_CASTSHADOW_INDEX].Data.Bool = Object->CastShadow;

	// copy property list
	*List = grProperty_ListCopy( &DynamicLightPropertyList );
	if ( *List == NULL )
	{
		grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, NULL );
		return GR_FALSE;
	}

	// all done
	return GR_TRUE;

} // GetPropertyList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	SetProperty()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC SetProperty(
	void				*Instance,	// object instance data
	int32				FieldID,	// id of field to be changed
	PROPERTY_FIELD_TYPE	DataType,	// type of data
	grProperty_Data		*pData )	// new data
{

	// locals
	DynamicLight	*Object;
	grBoolean		AdjustDynamicLightProperties = GR_FALSE;
	grBoolean		Result = GR_TRUE;

	// ensure valid data
	assert( Instance != NULL );
	assert( pData != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// process field id
	switch ( FieldID )
	{

		// adjust radius
		case DYNAMICLIGHT_RADIUS_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->Radius = pData->Float;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}

		// adjust brightness
		case DYNAMICLIGHT_BRIGHTNESS_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->Brightness = pData->Float;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}

		// adjust color
		case DYNAMICLIGHT_COLOR_ID:
		{
			assert( DataType == PROPERTY_COLOR_PICKER_TYPE );
			Object->Color.X = pData->Vector.X;
			Object->Color.Y = pData->Vector.Y;
			Object->Color.Z = pData->Vector.Z;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}
		case DYNAMICLIGHT_COLORRED_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->Color.X = pData->Float;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}
		case DYNAMICLIGHT_COLORGREEN_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->Color.Y = pData->Float;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}
		case DYNAMICLIGHT_COLORBLUE_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->Color.Z = pData->Float;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}

		// adjust cast shadow flag
		case DYNAMICLIGHT_CASTSHADOW_ID:
		{
			assert( DataType == PROPERTY_CHECK_TYPE );
			Object->CastShadow = pData->Bool;
			AdjustDynamicLightProperties = GR_TRUE;
			break;
		}

		// if we got to here then its an unsupported field
		default:
		{
			assert( 0 );
			return GR_FALSE;
			break;
		}
	}

	// adjust dynamic light properties if required
	if ( AdjustDynamicLightProperties == GR_TRUE )
	{
		if (Object->Light)
			{
			Result = grLight_SetAttributes(	Object->Light,
											&( Object->Xf.Translation ),
											&( Object->Color ),
											Object->Radius, 
											Object->Brightness, 
											GR_LIGHT_FLAG_FAST_LIGHTING_MODEL | (Object->CastShadow ? GR_LIGHT_FLAG_CAST_SHADOWS : 0) );
			}
	
	}

	// all done
	return Result;

	// eliminate warnings
	DataType;

} // SetProperty()



////////////////////////////////////////////////////////////////////////////////////////
//
//	SetXForm()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC SetXForm(
	void			*Instance,	// object instance data
	const grXForm3d	*Xf )		// new xform
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Xf != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// save xform
	Object->Xf = *Xf;

	// adjust light
	if (Object->Light)
		{
		return grLight_SetAttributes( Object->Light,
									&( Object->Xf.Translation ),
									&( Object->Color ),
									Object->Radius, 
									Object->Brightness, 
									GR_LIGHT_FLAG_FAST_LIGHTING_MODEL | (Object->CastShadow ? GR_LIGHT_FLAG_CAST_SHADOWS : 0) );
		}

	return GR_TRUE;
	

} // SetXForm()



////////////////////////////////////////////////////////////////////////////////////////
//
//	GetXForm()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC GetXForm(
	const void	*Instance,	// object instance data
	grXForm3d	*Xf )		// where to store xform
{

	// locals
	DynamicLight	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Xf != NULL );

	// get object data
	Object = (DynamicLight *)Instance;

	// save xform
	*Xf = Object->Xf;

	// all done
	return GR_TRUE;

} // GetXForm()



////////////////////////////////////////////////////////////////////////////////////////
//
//	GetXFormModFlags()
//
///////////////////////////////////////////////////////////////////////////////////////
int	GRCC GetXFormModFlags(
	const void	*Instance )	// object instance data
{

	// return xform mod flags
	return GR_OBJECT_XFORM_TRANSLATE;

	// eliminate warnings
	Instance;

} // GetXFormModFlags()



////////////////////////////////////////////////////////////////////////////////////////
//
//	GetChildren()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC GetChildren(
	const void	*Instance,
	grObject	*Children,
	int			MaxNumChildren )
{

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	Children;
	MaxNumChildren;

} // GetChildren()



////////////////////////////////////////////////////////////////////////////////////////
//
//	AddChild()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC AddChild(
	void			*Instance,
	const grObject	*Child )
{

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	Child;

} // AddChild()



////////////////////////////////////////////////////////////////////////////////////////
//
//	RemoveChild()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC RemoveChild(
	void			*Instance,
	const grObject	*Child )
{

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	Child;

} // RemoveChild()



////////////////////////////////////////////////////////////////////////////////////////
//
//	EditDialog()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC EditDialog(
	void	*Instance,
#ifdef WIN32
	HWND	Parent )
#endif
#ifdef BUILD_BE
	class G3DView* Parent)
#endif
{

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	Parent;

} // EditDialog()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Frame()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Frame(
	void	*Instance,
	float	TimeDelta )
{
	
	// ensure valid data
	assert( Instance != NULL );

	// all done
	return GR_TRUE;

	// eliminate warnings
	Instance;
	TimeDelta;

} // Frame()



////////////////////////////////////////////////////////////////////////////////////////
//
//	SendAMessage()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC SendAMessage(
	void	*Instance,	// object instance data
	int32	Msg,		// message id
	void	*Data )		// message data
{

	// all done
	return GR_FALSE;

	// eliminate warnings
	Instance;
	Msg;
	Data;

} // SendAMessage()

//Royce
////////////////////////////////////////////////////////////////////////////////////////
//
//	DuplicateInstance()
//
///////////////////////////////////////////////////////////////////////////////////////
void * GRCC DuplicateInstance(void * Instance)
{
	grVFile *ramdisk, *ramfile;
	grVFile_MemoryContext vfsmemctx;
	grObject* newDLight = NULL;
	grPtrMgr *ptrMgr = NULL;

	vfsmemctx.Data = grRam_Allocate(OBJ_PERSIST_SIZE); //"I dunno, 100K sounds good."
	vfsmemctx.DataLength = OBJ_PERSIST_SIZE;

	if (!vfsmemctx.Data) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to allocate enough RAM to duplicate this object", NULL);
		return NULL;
	}

	ramdisk = grVFile_OpenNewSystem
	(
		NULL, 
		(grVFile_TypeIdentifier) (GR_VFILE_TYPE_MEMORY|GR_VFILE_TYPE_VIRTUAL),
		"Memory",
		NULL,
		GR_VFILE_OPEN_CREATE|GR_VFILE_OPEN_DIRECTORY
	);

	if (!ramdisk) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a VFile Memory Directory", NULL);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	ramfile = grVFile_Open(ramdisk, "tempObject", GR_VFILE_OPEN_CREATE);

	if (!ramfile) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a VFile Memory File", NULL);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}
	ptrMgr = grPtrMgr_Create();

	if (!ptrMgr) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a Pointer Manager", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	if (!WriteToFile(Instance, ramfile, grPtrMgr_Create())) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to write the object to a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	if (!grVFile_Rewind(ramfile)) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to rewind the temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	newDLight = (grObject *)CreateFromFile(ramfile, ptrMgr);
	if (!newDLight) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to reade the object back from a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	grVFile_Close(ramfile);
	grVFile_Close(ramdisk);

	grRam_Free(vfsmemctx.Data);

	return( newDLight );
}
//---

// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );Plane;ImpactBox;BackBox;FrontBox;Pos;Instance;
}