/****************************************************************************************/
/*  CORONA.C                                                                            */
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
#pragma warning ( disable : 4115 )
#include <windows.h>
#pragma warning ( default : 4115 )
#include <float.h>
#include <assert.h>
#include <string.h>
#include "vfile.h"
#include "grProperty.h"
#include "ram.h"
#include "grResource.h"
#include "grWorld.h"
#include "Corona.h"
#include "resource.h"
#include "Errorlog.h"
#include "ObjectMsg.h"
#include "ObjUtil.h"
#include "grMaterial.h"
#include "grResource.h"

#define CORONA_VERSION_NUMBER 1


//Royce
#define OBJ_PERSIST_SIZE 5000
//---

////////////////////////////////////////////////////////////////////////////////////////
//	Property list stuff
////////////////////////////////////////////////////////////////////////////////////////
enum
{

	// misc stuff
	CORONA_FADETIME_ID = PROPERTY_LOCAL_DATATYPE_START,
	CORONA_MAXVISIBLEDISTANCE_ID,
	CORONA_MINRADIUS_ID,
	CORONA_MAXRADIUS_ID,
	CORONA_MINRADIUSDISTANCE_ID,
	CORONA_MAXRADIUSDISTANCE_ID,

	// texture group
	CORONA_ARTGROUP_ID,
	CORONA_ARTSIZE_ID,
	CORONA_ARTBITMAP_ID,
	CORONA_ARTALPHA_ID,
	CORONA_ARTGROUPEND_ID,

	// end marker
	CORONA_LAST_ID

};
enum
{

	// misc stuff
	CORONA_FADETIME_INDEX = 0,
	CORONA_MAXVISIBLEDISTANCE_INDEX,
	CORONA_MINRADIUS_INDEX,
	CORONA_MAXRADIUS_INDEX,
	CORONA_MINRADIUSDISTANCE_INDEX,
	CORONA_MAXRADIUSDISTANCE_INDEX,

	// texture group
	CORONA_ARTGROUP_INDEX,
	CORONA_ARTSIZE_INDEX,
	CORONA_ARTBITMAP_INDEX,
	CORONA_ARTALPHA_INDEX,
	CORONA_ARTGROUPEND_INDEX,

	// end marker
	CORONA_LAST_INDEX

};


////////////////////////////////////////////////////////////////////////////////////////
//	Globals
////////////////////////////////////////////////////////////////////////////////////////
static HINSTANCE		hClassInstance = NULL;
static BitmapList		*AvailableArt = NULL;
static grProperty		CoronaProperties[CORONA_LAST_INDEX];
static grProperty_List	CoronaPropertyList = { CORONA_LAST_INDEX, &( CoronaProperties[0] ) };
static char				*NoSelection = "< none >";
static grMaterialSpec *MatSpec;

////////////////////////////////////////////////////////////////////////////////////////
//	Defaults
////////////////////////////////////////////////////////////////////////////////////////

#define CORONA_DEFAULT_FADETIME				2.0f
#define CORONA_DEFAULT_MINRADIUS			1.0f
#define CORONA_DEFAULT_MAXRADIUS			4.0f
#define	CORONA_DEFAULT_MINRADIUSDISTANCE	100.0f
#define	CORONA_DEFAULT_MAXRADIUSDISTANCE	1000.0f
#define CORONA_DEFAULT_MAXVISIBLEDISTANCE	1000.0f


////////////////////////////////////////////////////////////////////////////////////////
//	Object data
////////////////////////////////////////////////////////////////////////////////////////
typedef struct Corona
{

	// standard stuff
	grWorld			*World;
	grResourceMgr	*ResourceMgr;
	grEngine		*Engine;
	int				RefCount;
	grBoolean		LoadedFromDisk;

	// internal stuff
	grBitmap	*Art;				// current art
	char		*ArtName;			// current art name
	char		*SizeString;		// size string loaded from disk
	float		LastVisibleRadius;	// last visible radius
	grFloat		DistanceToCorona;	// distance from camera to corona
	grBoolean	Visible;			// whether or not its visible

	// user adjustable stuff
	grXForm3d	Xf;
	char		*BitmapName;		// name of chosen bitmap
	char		*AlphaName;			// name of chosen alpha
	grFloat		FadeTime;			// how many seconds to spend fading away the corona
    float		MinRadius;			// mix corona radius
    float		MaxRadius;			// max corona radius
	float		MinRadiusDistance;	// below this distance, corona is capped at MinRadius
	float		MaxRadiusDistance;	// above this distance, corona is capped at MaxRadius
	float		MaxVisibleDistance;	// beyond this distance the corona is not visible

} Corona;



////////////////////////////////////////////////////////////////////////////////////////
//
//	InitClass()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean InitClass(
	HINSTANCE	hInstance )	// dll instance handle
{

	// locals
	char	*String;

	// ensure valid data
	assert( hInstance != NULL );

	// save hinstance
	hClassInstance = hInstance;


	////////////////////////////////////////////////////////////////////////////////////////
	//	Setup start and end of texture group
	////////////////////////////////////////////////////////////////////////////////////////
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_ARTGROUP );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillGroup( &( CoronaProperties[CORONA_ARTGROUP_INDEX] ), String, CORONA_ARTGROUP_ID );
	grProperty_FillGroupEnd( &( CoronaProperties[CORONA_ARTGROUPEND_INDEX] ), CORONA_ARTGROUPEND_ID );


	////////////////////////////////////////////////////////////////////////////////////////
	//	Misc properties
	////////////////////////////////////////////////////////////////////////////////////////

	// fade time
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_FADETIME );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_FADETIME_INDEX] ), String,
							CORONA_DEFAULT_FADETIME, CORONA_FADETIME_ID, 0.0f, FLT_MAX, 0.1f );

	// max visible distance
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_MAXVISIBLEDISTANCE );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_MAXVISIBLEDISTANCE_INDEX] ), String,
							CORONA_DEFAULT_MAXVISIBLEDISTANCE, CORONA_MAXVISIBLEDISTANCE_ID, 1.0f, FLT_MAX, 100.0f );

	// min radius
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_MINRADIUS );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_MINRADIUS_INDEX] ), String,
							CORONA_DEFAULT_MINRADIUS, CORONA_MINRADIUS_ID, 0.1f, FLT_MAX, 0.1f );

	// max radius
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_MAXRADIUS );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_MAXRADIUS_INDEX] ), String,
							CORONA_DEFAULT_MAXRADIUS, CORONA_MAXRADIUS_ID, 0.1f, FLT_MAX, 0.1f );

	// min radius distance
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_MINRADIUSDISTANCE );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_MINRADIUSDISTANCE_INDEX] ), String,
							CORONA_DEFAULT_MINRADIUSDISTANCE, CORONA_MINRADIUSDISTANCE_ID, 0.0f, FLT_MAX, 100.0f );

	// max radius distance
	String = ObjUtil_LoadLibraryString( hClassInstance, IDS_MAXRADIUSDISTANCE );
	if ( String == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
		goto ERROR_InitClass;
	}
	grProperty_FillFloat(	&( CoronaProperties[CORONA_MAXRADIUSDISTANCE_INDEX] ), String,
							CORONA_DEFAULT_MAXRADIUSDISTANCE, CORONA_MAXRADIUSDISTANCE_ID, 0.0f, FLT_MAX, 100.0f );


	////////////////////////////////////////////////////////////////////////////////////////
	//	End marker
	////////////////////////////////////////////////////////////////////////////////////////
	CoronaPropertyList.grPropertyN = CORONA_LAST_INDEX;

	// all done
	return GR_TRUE;


	//
	//	error handling
	//
	ERROR_InitClass:

	// reset misc stuff
	hClassInstance = NULL;
	CoronaPropertyList.grPropertyN = 0;

	// return failure
	ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
	return GR_FALSE;

} // InitClass()



////////////////////////////////////////////////////////////////////////////////////////
//
//	DeInitClass()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean DeInitClass(
	void )	// no parameters
{

	// free available art list
	if ( AvailableArt != NULL )
	{
		ObjUtil_DestroyBitmapList( &AvailableArt );
	}

	// zap instance pointer
	hClassInstance = NULL;

	// all done
	 return GR_TRUE;

} // DeInitClass()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CreateInstance()
//
////////////////////////////////////////////////////////////////////////////////////////
void * GRCC CreateInstance(
	void )	// no parameters
{

	// locals
	Corona	*Object;

	// allocate struct
	Object = grRam_AllocateClear( sizeof( *Object ) );
	if ( Object == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_MEMORY_RESOURCE, IDS_ERROR_CreateInstance_AllocateObject );
		goto ERROR_CreateInstance;
	}

	// set defaults
	grXForm3d_SetIdentity( &( Object->Xf ) );
	Object->RefCount = 1;
	Object->FadeTime = CORONA_DEFAULT_FADETIME;
	Object->MinRadius = CORONA_DEFAULT_MINRADIUS;
	Object->MaxRadius = CORONA_DEFAULT_MAXRADIUS;
	Object->MinRadiusDistance = CORONA_DEFAULT_MINRADIUSDISTANCE;
	Object->MaxRadiusDistance = CORONA_DEFAULT_MAXRADIUSDISTANCE;
	Object->MaxVisibleDistance = CORONA_DEFAULT_MAXVISIBLEDISTANCE;

	// all done
	return Object;


	//
	//	error handling
	//
	ERROR_CreateInstance:

	// free object
	if ( Object != NULL )
	{
		// free object itself
		grRam_Free( Object );
		Object = NULL;
	}

	// return failure
	ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateInstance_Failure );
	return NULL;

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
	Corona	*Object;
	
	// get object
	Object = (Corona *)Instance;
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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );

	// get object
	Object = (Corona *)*Instance;
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
	const void				*Instance,				// object instance data
	const grWorld			*World,					// world
	const grEngine			*Engine,				// engine
	const grCamera			*Camera,				// camera
	const grFrustum			*CameraSpaceFrustum,	// frustum 	
	grObject_RenderFlags	RenderFlags )			// render flags
{

	// locals
	Corona	*Object;
    grVec3d			Delta;
    grXForm3d		CameraXf;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );
	assert( Engine != NULL );
	assert( Camera != NULL );
	assert( CameraSpaceFrustum != NULL );

	// get object
	Object = (Corona *)Instance;
	assert( Object != NULL );

	// determine corona distance and visibility
	{

		// locals
		grCollisionInfo	CollisionInfo;
		grExtBox		ExtBox;

		// get camera xform
		grCamera_GetXForm( Camera, &CameraXf );

		// determine distance from corona to camera
		grVec3d_Subtract( &( Object->Xf.Translation ), &( CameraXf.Translation ), &Delta );
		Object->DistanceToCorona = grVec3d_Length( &Delta );

		// setup extent box
        ExtBox.Min.X = -1.0f;
        ExtBox.Min.Y = -1.0f;
        ExtBox.Min.Z = -1.0f;
        ExtBox.Max.X = 1.0f;
        ExtBox.Max.Y = 1.0f;
        ExtBox.Max.Z = 1.0f;
//!!		grExtBox_SetToPoint ( &ExtBox, &( Object->Xf.Translation ) );
//!!		grExtBox_ExtendToEnclose( &ExtBox, &( Object->Xf.Translation ) );

		// determine whether or not corona is visible
		if ( Object->DistanceToCorona > Object->MaxVisibleDistance )
		{
			Object->Visible = GR_FALSE;
		}
		else
		{
			Object->Visible = !(grWorld_Collision( World, &ExtBox, &( Object->Xf.Translation ), &( CameraXf.Translation ), &CollisionInfo ));
		}
	}

	// update the art
	if (	( Object->Art != NULL ) &&
			( Object->LastVisibleRadius > 0.0f ) )
	{

		// locals
		grUserPoly	*Poly;
		grLVertex	Vertex;
        grVec3d position;

		// setup vert
		Vertex.a = 255.0f;
		Vertex.r = 255.0f;
		Vertex.g = 255.0f;
		Vertex.b = 255.0f;
		Vertex.u = 0.0f;
		Vertex.v = 0.0f;
        grVec3d_Scale(&Delta,4.0f/Object->DistanceToCorona,&position);
        grVec3d_Add(&position,&(CameraXf.Translation),&position);
//!!		Vertex.X = Object->Xf.Translation.X;
//!!		Vertex.Y = Object->Xf.Translation.Y;
//!!		Vertex.Z = Object->Xf.Translation.Z;
        Vertex.X = position.X;
        Vertex.Y = position.Y;
        Vertex.Z = position.Z;

		// add poly
		if (!MatSpec)
    	{
	        MatSpec = grMaterialSpec_Create(grResourceMgr_GetEngine(grResourceMgr_GetSingleton()), grResourceMgr_GetSingleton());
#pragma message ("Krouer: change NULL to something better next time")
	        grMaterialSpec_AddLayerFromBitmap(MatSpec, 0, Object->Art, NULL);
	    }
  
		Poly = grUserPoly_CreateSprite( &Vertex, MatSpec, (Object->LastVisibleRadius * 4.0f)/Object->DistanceToCorona, 0 );
		if ( Poly == NULL )
		{
			ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_Render_AddPoly );
		}
		else
		{
			grWorld_AddUserPoly( (grWorld *)World, Poly, GR_TRUE );
			grUserPoly_Destroy( &Poly );
		}
	}

	// all done
	return GR_TRUE;

	// eliminate warnings
	CameraSpaceFrustum;
	RenderFlags;
	Engine;

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Object = (Corona *)Instance;

	// save world pointer
	Object->World = World;

	// save an instance of the resource manager
	Object->ResourceMgr = grWorld_GetResourceMgr( World );
	assert( Object->ResourceMgr != NULL );

	// build bitmap list if required
	if ( AvailableArt == NULL )
	{

		// build list
		AvailableArt = ObjUtil_CreateBitmapList( Object->ResourceMgr, "GlobalMaterials", "*.bmp" );
		if ( AvailableArt == NULL )
		{
			ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_AttachWorld_CreateBitmapList );
			goto ERROR_AttachWorld;
		}
	}

	// set object art defaults
	if ( Object->LoadedFromDisk == GR_FALSE )
	{
		Object->BitmapName = AvailableArt->Name[0];
		Object->AlphaName = AvailableArt->Name[0];
	}

	// all done
	return GR_TRUE;


	//
	//	error handling
	//
	ERROR_AttachWorld:

	// destroy bitmap list
	if ( AvailableArt != NULL )
	{
		ObjUtil_DestroyBitmapList( &AvailableArt );
	}

	// destroy our instance of the resource manager
	if ( Object->ResourceMgr != NULL )
	{
		grResource_MgrDestroy( &( Object->ResourceMgr ) );
	}

	// return failure
	ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_AttachWorld_Failure );
	return GR_FALSE;

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Object = (Corona *)Instance;
	assert( Object->World == World );

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Object = (Corona *)Instance;

	// save engine pointer
	Object->Engine = Engine;

	// set properties if object was loaded from disk
	if ( Object->LoadedFromDisk == GR_TRUE )
	{

		// locals
		grBoolean	Result = GR_TRUE;
		char		*LoadBitmapName, *LoadAlphaName;

		// reset loaded from disk flag
		Object->LoadedFromDisk = GR_FALSE;

		// save allocated string pointers
		LoadBitmapName = Object->BitmapName;
		LoadAlphaName = Object->AlphaName;
		Object->BitmapName = AvailableArt->Name[0];
		Object->AlphaName = AvailableArt->Name[0];

		// set texture group size
		ObjUtil_TextureGroupSetSize(	Object->Engine, Object->ResourceMgr,
										AvailableArt, Object->SizeString,
										&( Object->BitmapName ), &( Object->AlphaName ),
										&( Object->Art ), &( Object->ArtName ) );

		// set bitmap name
		Result &= ObjUtil_TextureGroupSetArt(	Object->Engine, Object->ResourceMgr,
												AvailableArt, LoadBitmapName, NULL,
												&( Object->BitmapName ), &( Object->AlphaName ),
												&( Object->Art ), &( Object->ArtName ) );

		// set alpha name
		Result &= ObjUtil_TextureGroupSetArt(	Object->Engine, Object->ResourceMgr,
												AvailableArt, NULL, LoadAlphaName,
												&( Object->BitmapName ), &( Object->AlphaName ),
												&( Object->Art ), &( Object->ArtName ) );

		// free strings allocated on load
		grRam_Free( LoadBitmapName );
		grRam_Free( LoadAlphaName );

		// log errors
		if ( Result == GR_FALSE )
		{
			goto ERROR_AttachEngine;
		}
	}

	// all done
	return GR_TRUE;


	//
	//	error handling
	//
	ERROR_AttachEngine:

	// return failure
	ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_AttachEngine );
	return GR_FALSE;

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Object = (Corona *)Instance;
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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( BBox != NULL );

	// get object data
	Object = (Corona *)Instance;

	// setup extent box	//hack
	{

		// locals
		grVec3d Pos;
		float	Size = 25.0f;

		// save extent box
		Pos = Object->Xf.Translation;
		grExtBox_Set (  BBox, 
						Pos.X - Size, Pos.Y - Size, Pos.Z - Size,
						Pos.X + Size, Pos.Y + Size, Pos.Z + Size );
	}

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
	grPtrMgr	*PtrMgr )	// pointer manager
{

	// locals
	grFloat	Ver;
	BYTE Version;
	uint32 Tag;
	Corona		*Object;
	grBoolean	Result = GR_TRUE;


	// ensure valid data
	assert( File != NULL );
	assert( PtrMgr != NULL );

	// create new object
	Object = CreateInstance();
	if ( Object == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_CreateObject );
		goto ERROR_CreateFromFile;
	}

	
	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_ReadData );
		goto ERROR_CreateFromFile;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read( File, &Version, sizeof( Version )))
		{
		    ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_ReadData );
		    goto ERROR_CreateFromFile;
		}

	}
	else
	{
		//for backwards compatibility with old object format
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
		if (!grVFile_Read( File, &Ver, sizeof( Ver )))
		{
		    ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_ReadData );
		    goto ERROR_CreateFromFile;
		}
		Version = 1;
		
	}
	
	if ( Version >= 1 )
	{
		//read data
		Result &= ObjUtil_ReadString( File, &( Object->SizeString ) );
	    Result &= ObjUtil_ReadString( File, &( Object->BitmapName ) );
	    Result &= ObjUtil_ReadString( File, &( Object->AlphaName ) );
	    Result &= grVFile_Read( File, &( Object->Xf ), sizeof( Object->Xf ) );
	    Result &= grVFile_Read( File, &( Object->FadeTime ), sizeof( Object->FadeTime ) );
	    Result &= grVFile_Read( File, &( Object->MinRadius ), sizeof( Object->MinRadius ) );
	    Result &= grVFile_Read( File, &( Object->MaxRadius ), sizeof( Object->MaxRadius ) );
	    Result &= grVFile_Read( File, &( Object->MinRadiusDistance ), sizeof( Object->MinRadiusDistance ) );
	    Result &= grVFile_Read( File, &( Object->MaxRadiusDistance ), sizeof( Object->MaxRadiusDistance ) );
	    Result &= grVFile_Read( File, &( Object->MaxVisibleDistance ), sizeof( Object->MaxVisibleDistance ) );

	    // log errors
	    if ( Result == GR_FALSE )
		{
		    ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_ReadData );
		    goto ERROR_CreateFromFile;
		}
	}
	
	// all done
	Object->LoadedFromDisk = GR_TRUE;

	return Object;


	//
	// error handling
	//
	ERROR_CreateFromFile:

	// free object
	if ( Object != NULL )
	{

		// free strings
		if ( Object->SizeString != NULL )
		{
			grRam_Free( Object->SizeString );
		}
		if ( Object->BitmapName != NULL )
		{
			grRam_Free( Object->BitmapName );
		}
		if ( Object->AlphaName != NULL )
		{
			grRam_Free( Object->AlphaName );
		}

		// free object itself
		grRam_Free( Object );
		Object = NULL;
	}

	// return error
	return NULL;

	// eliminate warnings
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
	grPtrMgr	*PtrMgr )
{

	// locals
    BYTE Version = CORONA_VERSION_NUMBER;
	uint32 Tag = FILE_UNIQUE_ID;
	Corona		*Object;
	grBoolean	Result = GR_TRUE;

	// ensure valid data
	assert( Instance != NULL );
	assert( File != NULL );
	assert( PtrMgr != NULL );

	// get object data
	Object = (Corona *)Instance;

	// write version number
	Result &= grVFile_Write( File, &Tag, sizeof(Tag));
	Result &= grVFile_Write( File, &Version, sizeof( Version ) );
	
	// write out data
	Result &= ObjUtil_WriteString( File, AvailableArt->StringSizes[AvailableArt->ActiveCurSize] );
	Result &= ObjUtil_WriteString( File, Object->BitmapName );
	Result &= ObjUtil_WriteString( File, Object->AlphaName );
	Result &= grVFile_Write( File, &( Object->Xf ), sizeof( Object->Xf ) );
	Result &= grVFile_Write( File, &( Object->FadeTime ), sizeof( Object->FadeTime ) );
	Result &= grVFile_Write( File, &( Object->MinRadius ), sizeof( Object->MinRadius ) );
	Result &= grVFile_Write( File, &( Object->MaxRadius ), sizeof( Object->MaxRadius ) );
	Result &= grVFile_Write( File, &( Object->MinRadiusDistance ), sizeof( Object->MinRadiusDistance ) );
	Result &= grVFile_Write( File, &( Object->MaxRadiusDistance ), sizeof( Object->MaxRadiusDistance ) );
	Result &= grVFile_Write( File, &( Object->MaxVisibleDistance ), sizeof( Object->MaxVisibleDistance ) );

	// log errors
	if ( Result == GR_FALSE )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_FILEIO_WRITE, IDS_ERROR_WriteToFile );
	}

	// all done
	return Result;

	// eliminate warnings
	PtrMgr;

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( List != NULL );

	// get object data
	Object = (Corona *)Instance;

	// set properties
	CoronaProperties[CORONA_FADETIME_INDEX].Data.Float = Object->FadeTime;
	CoronaProperties[CORONA_MAXVISIBLEDISTANCE_INDEX].Data.Float = Object->MaxVisibleDistance;
	CoronaProperties[CORONA_MINRADIUS_INDEX].Data.Float = Object->MinRadius;
	CoronaProperties[CORONA_MAXRADIUS_INDEX].Data.Float = Object->MaxRadius;
	CoronaProperties[CORONA_MINRADIUSDISTANCE_INDEX].Data.Float = Object->MinRadiusDistance;
	CoronaProperties[CORONA_MAXRADIUSDISTANCE_INDEX].Data.Float = Object->MaxRadiusDistance;

	// setup texture group properties
	{

		// locals
		char	*ArtSize, *ArtBitmap, *ArtAlpha;

		// get strings
		ArtSize = ObjUtil_LoadLibraryString( hClassInstance, IDS_ARTSIZE );
		ArtBitmap = ObjUtil_LoadLibraryString( hClassInstance, IDS_ARTBITMAP );
		ArtAlpha = ObjUtil_LoadLibraryString( hClassInstance, IDS_ARTALPHA );

		// check strings
		if ( ( ArtSize == NULL ) || ( ArtBitmap == NULL ) || ( ArtAlpha == NULL ) )
		{
			ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetString );
			goto ERROR_GetPropertyList;
		}

		// init properties
		grProperty_FillCombo(	&( CoronaProperties[CORONA_ARTSIZE_INDEX] ), ArtSize,
								AvailableArt->StringSizes[AvailableArt->ActiveCurSize],
								CORONA_ARTSIZE_ID, AvailableArt->SizesListSize, AvailableArt->StringSizes );
		grProperty_FillCombo(	&( CoronaProperties[CORONA_ARTBITMAP_INDEX] ), ArtBitmap,
								Object->BitmapName, CORONA_ARTBITMAP_ID, AvailableArt->ActiveCount, AvailableArt->ActiveList );
		grProperty_FillCombo(	&( CoronaProperties[CORONA_ARTALPHA_INDEX] ), ArtAlpha,
								Object->AlphaName, CORONA_ARTALPHA_ID, AvailableArt->ActiveCount, AvailableArt->ActiveList );
	}

	// copy property list
	*List = grProperty_ListCopy( &CoronaPropertyList );
	if ( *List == NULL )
	{
		ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_CreateFromFile_CreateObject );
		return GR_FALSE;
	}

	// reset dirty flag
	CoronaPropertyList.bDirty = GR_FALSE;

	// all done
	return GR_TRUE;


	//
	//	error handling
	//
	ERROR_GetPropertyList:

	// return failure
	ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_GetPropertyList );
	return GR_FALSE;

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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( pData != NULL );

	// get object data
	Object = (Corona *)Instance;

	// process field id
	switch ( FieldID )
	{

		// adjust fade time
		case CORONA_FADETIME_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->FadeTime = pData->Float;
			break;
		}

		// adjust max visible distance
		case CORONA_MAXVISIBLEDISTANCE_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			Object->MaxVisibleDistance = pData->Float;
			break;
		}

		// adjust min radius
		case CORONA_MINRADIUS_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			if ( pData->Float <= Object->MaxRadius )
			{
				Object->MinRadius = pData->Float;
			}
			break;
		}

		// adjust max radius
		case CORONA_MAXRADIUS_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			if ( pData->Float >= Object->MinRadius )
			{
				Object->MaxRadius = pData->Float;
			}
			break;
		}

		// adjust min radius distance
		case CORONA_MINRADIUSDISTANCE_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			if ( pData->Float <= Object->MaxRadiusDistance )
			{
				Object->MinRadiusDistance = pData->Float;
			}
			break;
		}

		// adjust max radius distance
		case CORONA_MAXRADIUSDISTANCE_ID:
		{
			assert( DataType == PROPERTY_FLOAT_TYPE );
			if ( pData->Float >= Object->MinRadiusDistance )
			{
				Object->MaxRadiusDistance = pData->Float;
			}
			break;
		}

		// process art size choice
		case CORONA_ARTSIZE_ID:
		{

			// ensure valid data
			assert( DataType == PROPERTY_COMBO_TYPE );
			assert( pData->String != NULL );

			// process new size choice
			ObjUtil_TextureGroupSetSize(	Object->Engine, Object->ResourceMgr,
											AvailableArt, pData->String,
											&( Object->BitmapName ), &( Object->AlphaName ),
											&( Object->Art ), &( Object->ArtName ) );

			// set dirty flag
			CoronaPropertyList.bDirty = GR_TRUE;
			break;
		}

		// process art choice
		case CORONA_ARTBITMAP_ID:
		case CORONA_ARTALPHA_ID:
		{

			// locals
			grBoolean	Result;

			// ensure valid data
			assert( DataType == PROPERTY_COMBO_TYPE );
			assert( pData->String != NULL );

			// process new bitmap choice...
			if ( FieldID == CORONA_ARTBITMAP_ID )
			{
				Result = ObjUtil_TextureGroupSetArt(	Object->Engine, Object->ResourceMgr,
														AvailableArt, pData->String, NULL,
														&( Object->BitmapName ), &( Object->AlphaName ),
														&( Object->Art ), &( Object->ArtName ) );
			}
			// ...or alpha choice
			else
			{
				Result = ObjUtil_TextureGroupSetArt(	Object->Engine, Object->ResourceMgr,
														AvailableArt, NULL, pData->String,
														&( Object->BitmapName ), &( Object->AlphaName ),
														&( Object->Art ), &( Object->ArtName ) );
			}

			// log errors
			if ( Result == GR_FALSE )
			{
				ObjUtil_LogError( hClassInstance, GR_ERR_SUBSYSTEM_FAILURE, IDS_ERROR_SetProperty );
				return GR_FALSE;
			}
			break;
		}

		// if we got to here then its an unsupported field
		default:
		{
			return GR_FALSE;
			break;
		}
	}

	// all done
	return GR_TRUE;

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
	Corona	*Object;
	grVec3d	Pos;

	// ensure valid data
	assert( Instance != NULL );
	assert( Xf != NULL );

	// get object data
	Object = (Corona *)Instance;

	// save xform
	Object->Xf = *Xf;
	grVec3d_Copy( &( Object->Xf.Translation ), &Pos );
	grXForm3d_Orthonormalize( &( Object->Xf ) );
	grVec3d_Copy( &Pos, &( Object->Xf.Translation ) );

	// all done
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
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Xf != NULL );

	// get object data
	Object = (Corona *)Instance;

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
	return ( GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE );

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
	HWND	Parent )
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

	// locals
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );

	// do nothing if no time has elapsed
	assert( TimeDelta >= 0.0f );
	if ( TimeDelta == 0.0f )
	{
		return GR_TRUE;
	}

	// get object
	Object = (Corona *)Instance;
	assert( Object != NULL );

	// set new radius
	if ( Object->Visible )
	{

		// locals
		float	DesiredRadius;

		// determine desired radius
		if ( Object->DistanceToCorona >= Object->MaxRadiusDistance )
		{
			DesiredRadius = Object->MaxRadius;
		}
		else if	( Object->DistanceToCorona <= Object->MinRadiusDistance )
		{
			DesiredRadius = Object->MinRadius;
		}
		else
		{

			// locals
			grFloat	Slope;

			// determine radius
			Slope = ( Object->MaxRadius - Object->MinRadius ) / ( Object->MaxRadiusDistance - Object->MinRadiusDistance );
			DesiredRadius = Object->MinRadius + Slope * ( Object->DistanceToCorona - Object->MinRadiusDistance );
		}

		// scale radius upwards
		if ( Object->FadeTime > 0.0f )
		{
			Object->LastVisibleRadius += ( ( TimeDelta * Object->MaxRadius ) / Object->FadeTime );
			if ( Object->LastVisibleRadius > DesiredRadius )
			{
				Object->LastVisibleRadius = DesiredRadius;
			}
		}
		else
		{
			Object->LastVisibleRadius = DesiredRadius;
		}
	}
	else if ( Object->LastVisibleRadius > 0.0f )
	{

		// scale radius down
		if ( Object->FadeTime > 0.0f )
		{
			Object->LastVisibleRadius -= ( ( TimeDelta * Object->MaxRadius ) / Object->FadeTime );
			if ( Object->LastVisibleRadius < 0.0f )
			{
				Object->LastVisibleRadius = 0.0f;
			}
		}
		else
		{
			Object->LastVisibleRadius = 0.0f;
		}
	}

	// all done
	return GR_TRUE;

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

	// locals
	Corona	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Data != NULL );

	// get object
	Object = (Corona *)Instance;
	assert( Object != NULL );

	// process message
	switch ( Msg )
	{

		// unsupported message
		default:
		{
			//return GR_FALSE;
			break;
		}
	}

	// all done
	return GR_FALSE;

	// Eliminate warnings
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
	grObject* newCorObj = NULL;
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
		GR_VFILE_TYPE_MEMORY|GR_VFILE_TYPE_VIRTUAL,
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

	newCorObj = CreateFromFile(ramfile, ptrMgr);
	if (!newCorObj) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to reade the object back from a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	grVFile_Close(ramfile);
	grVFile_Close(ramdisk);

	grRam_Free(vfsmemctx.Data);

	return( newCorObj );
}
//---

// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );Plane;ImpactBox;BackBox;FrontBox;Pos;Instance;
}