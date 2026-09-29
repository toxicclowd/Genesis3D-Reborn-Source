
#ifdef WIN32
#include <windows.h>
#endif

#include <math.h>

#include <assert.h>
#include <stdlib.h>

#include "Ram.h"
#include "Puppet.h"
#include "Body.h"
#include "Motion.h"
#include "Errorlog.h"
#include "StrBlock.h"
#include "Log.h"

#include "Actor.h"
#include "Actor._h"
#include "ActorObj.h"

#include "ActorUtil.h"
#include "ActorPropertyList.h"


#define ACTOROBJECT_VERSION 1


GRAPI grActor *GRCC grActor_Create();

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
	grActor	*Actor;
	ActorObj *Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Actor = (grActor *)Instance;
	Object = Actor->Object;

	if (Object->World == World)
	{
		return GR_TRUE;
	}
	// save world pointer
	Object->World = World;

	// save an instance of the resource manager
	Object->ResourceMgr = grWorld_GetResourceMgr( World );
	assert( Object->ResourceMgr != NULL );

	// build mapper name list
	if ( MaterialMapperNameList == NULL )
	{

		// locals
		int	i;

		// allocate list
		MaterialMapperNameList = (char **)grRam_AllocateClear( sizeof ( char * ) * MaterialMapperTableSize );
		if ( MaterialMapperNameList == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			return GR_FALSE;
		}

		// init list
		for ( i = 0; i < MaterialMapperTableSize; i++ )
		{
			MaterialMapperNameList[i] = MaterialMapperTable[i].Name;
		}
	}


	// build bitmap list if required
	if ( Bitmaps == NULL )
	{

		// create bitmap list
		Bitmaps = Util_CreateBitmapList( Object->ResourceMgr, "GlobalMaterials", "*.bmp" );
		if ( Bitmaps == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			return GR_FALSE;
		}
	}

	// prepare actor list combo box
	if ( ActorDefList != NULL )
	{
		Util_DestroyFileList( &ActorDefList, &ActorDefListSize );
	}
	ActorDefList = Util_BuildFileList( Object->ResourceMgr, "Actors", "*.act", &ActorDefListSize );
	if ( ActorDefList == NULL )
	{
		Object->World = NULL;
		Object->ResourceMgr = NULL;
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		return GR_FALSE;
	}
	grProperty_FillCombo(	&( ActorObjPropertyList.pgrProperty[ACTOROBJ_LIST_INDEX] ),
							IDS_ACTORLIST,
							ActorDefList[0],
							ACTOROBJ_LIST_ID,
							ActorDefListSize,
							ActorDefList );

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
	grActor *Actor;
	ActorObj *Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( World != NULL );

	// get object
	Actor = (grActor *)Instance;
	Object = Actor->Object;

	//assert( Object->World == World );

	if (Object->ResourceMgr) { // destroy our instance of the resource manager
		grResource_MgrDestroy( &( Object->ResourceMgr ) );
	}

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
	grActor	*Actor;
	ActorObj *Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Actor = (grActor *)Instance;
	Object = Actor->Object;

	// save engine pointer
	Actor->Object->Engine = Engine;

	// set properties
	if ( Object->LoadedFromDisk == GR_TRUE )
	{

		// locals
		grProperty_Data	Data;
		char *RememberMotionName;
		grBoolean ret;

		// reset loaded from disk flag
		//Object->LoadedFromDisk = GR_FALSE;  // Krouer: move this line later

		// compensate for hack rotation that will occur
		{
			grVec3d	Pos;
			grVec3d_Copy( &( Actor->Xf.Translation ), &Pos );
			grVec3d_Set( &( Actor->Xf.Translation ), 0.0f, 0.0f, 0.0f );
			grXForm3d_RotateX( &( Actor->Xf ), GR_HALFPI );
			grVec3d_Copy( &Pos, &( Actor->Xf.Translation ) );
		}

		RememberMotionName = Util_StrDup( Object->MotionName );

		// set actor def
		Data.String = Util_StrDup( Object->ActorDefName );
		ret = SetProperty( Actor, ACTOROBJ_LIST_ID, PROPERTY_COMBO_TYPE, &Data );
		grRam_Free( Data.String );

		if (!ret)
		{
            // Krouer: restore the previous commented line
    	    Object->LoadedFromDisk = GR_FALSE;
			grRam_Free(Object->ActorDefName);
			grRam_Free(Object->MotionName);
			grRam_Free(Object->LightReferenceBoneName);
			Object->ActorDefName = Util_StrDup( NoSelection );
			Object->MotionName = Util_StrDup( NoSelection );
			Object->LightReferenceBoneName = Util_StrDup( NoSelection );
			return GR_TRUE;
		}

		// set motion name
		Data.String = Util_StrDup( RememberMotionName );
		ret = SetProperty( Actor, ACTOROBJ_MOTIONLIST_ID, PROPERTY_COMBO_TYPE, &Data );
		grRam_Free( Data.String );
		grRam_Free( RememberMotionName );

		if (!ret)
		{
            // Krouer: restore the previous commented line
    	    Object->LoadedFromDisk = GR_FALSE;
			grRam_Free(Object->MotionName);
			Object->MotionName = Util_StrDup( NoSelection );
		}

		// set light reference bone name
		Data.String = Util_StrDup( Object->LightReferenceBoneName );
		ret = SetProperty( Actor, ACTOROBJ_LIGHTREFERENCEBONENAMELIST_ID, PROPERTY_COMBO_TYPE, &Data );
		grRam_Free( Data.String );

		if (!ret)
		{
			grRam_Free(Object->LightReferenceBoneName);
			Object->LightReferenceBoneName = Util_StrDup( NoSelection );
		}

        // Krouer: restore the previous commented line
    	Object->LoadedFromDisk = GR_FALSE;
	}
	else
	{
		if(Actor->ActorDefinition != NULL)
			grActor_AttachEngine( Actor, Object->Engine );		
	}


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
	grActor *Actor;
	ActorObj* Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Engine != NULL );

	// get object data
	Actor = (grActor *)Instance;
	Object = Actor->Object;
#ifdef _DEBUG
	if (Object->Engine) assert( Object->Engine == Engine );
#endif

	// destroy motion list
	if ( Object->MotionListSize > 0 )
	{
		ActorObj_DestroyMotionList( Actor );
	}

	// free actor def name
	if ( Object->ActorDefName != NULL )
	{
		grRam_Free( Object->ActorDefName );
		Object->ActorDefName = NULL;
	}
		
	// zap engine pointer	
	if(Object->Engine && Actor->ActorDefinition)
	{
		grActor_DetachEngine(Actor,Engine);
		Object->Engine = NULL;
	}


	// all done
	return GR_TRUE;
} // DettachEngine()

void GRCC CreateRef(void *Actor)
{
	grActor_CreateRef((grActor*)Actor);
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Render()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Render(
	const void				*ActorPtr,				// object instance data
	const grWorld			*World,					// world
	const grEngine			*Engine,				// engine
	const grCamera			*Camera,				// camera
	const grFrustum			*CameraSpaceFrustum,	// frustum 	
	grObject_RenderFlags	RenderFlags )			// render flags
{

	// locals
	//ActorObj	*Object;
	grActor *Actor = (grActor *)ActorPtr;
	
	grBody		*Body;
	grFloat		floate;
	grCamera	*Cam2;
	ActorObj *Object;

	// ensure valid data
	assert( Actor != NULL );
	assert( World != NULL );
	assert( Engine != NULL );
	assert( Camera != NULL );
	assert( CameraSpaceFrustum != NULL );

	Object = Actor->Object;

	// peform rendering if an actor exists
	if ( grActor_IsValid(Actor) == GR_TRUE && grActor_DefIsValid(Actor->ActorDefinition) == GR_TRUE)
	{
		grVec3d	Pos;
		grVec3d Vector;
		const grVec3d * POV;
		int lod=0;
		grXForm3d Xf;
		grActor_GetXForm(Actor,&Xf);
				
		//Calculate LOD
		// get actor def body
		Body = grActor_GetBody( Actor->ActorDefinition );
		grVec3d_Copy( &( Xf.Translation ), &Pos );
		Cam2 = (grCamera*)Camera;
		POV = grCamera_GetPov2(Cam2);
		grVec3d_Copy(POV, &Vector);
		floate = grVec3d_Length(&Vector);
		
		if(floate >=0.0f)
			lod = 0;
#if 0
		if(floate >=200.0f)
			lod = 1;
		if(floate >= 400.0f)
			lod = 2;
		if(floate >=800.0f)
			lod = 3;
		if(floate >=1600.0f)
			lod = 4;
		if(floate >=3200.0f)
			lod = 5;
		if(floate >=6400.0f)
			lod = 6;
#endif

		grBody_ComputeLevelsOfDetail(Body, lod);
		
		
		// render the actor
		if ( RenderFlags & GR_OBJECT_RENDER_FLAG_CAMERA_FRUSTUM )
		{
			grActor_Render( Actor, (grEngine *)Engine, (grWorld *)World, Camera );
		}
		else
		{
			grActor_RenderThroughFrustum( Actor, (grEngine *)Engine, (grWorld *)World, (grCamera*)Camera , CameraSpaceFrustum );
		}

		// display collision ext box
		if ( Object->CollisionExtBoxDisplay == GR_TRUE )
		{

			// locals
			GR_RGBA		Color = { 0.0f, 255.0f, 0.0f, 64.0f };
			grExtBox	ExtBox;
			grVec3d Translation;

			// copy ext box
			ExtBox = Object->CollisionExtBox;

			// adjust extent box
			GetBoxTranslation(&ExtBox, Actor, &Translation);
			grExtBox_Translate( &ExtBox, Translation.X, Translation.Y, Translation.Z );
			grExtBox_Scale( &ExtBox, Object->ScaleX, Object->ScaleZ, Object->ScaleY );

			// draw it
			Util_DrawExtBox( (grWorld *)World, &Color, &ExtBox );
		}

		// display render ext box
		if ( Object->RenderExtBoxDisplay == GR_TRUE )
		{

			// locals
			GR_RGBA		Color = { 255.0f, 0.0f, 0.0f, 64.0f };
			grExtBox	ExtBox;
			grVec3d Translation;

			// copy ext box
			ExtBox = Object->RenderHintExtBox;

			// adjust extent box
			GetBoxTranslation(&ExtBox, Actor, &Translation);
			grExtBox_Translate( &ExtBox, Translation.X, Translation.Y, Translation.Z );
			grExtBox_Scale( &ExtBox, Object->ScaleX, Object->ScaleZ, Object->ScaleY );

			// draw it
			Util_DrawExtBox( (grWorld *)World, &Color, &ExtBox );
		}
	}


	// all done
	return GR_TRUE;

} // Render()

////////////////////////////////////////////////////////////////////////////////////////
//
//	Collision()
//
///////////////////////////////////////////////////////////////////////////////////////
grBoolean GRCC Collision(
	const void* ActorPtr,
	const grExtBox	*Box,
	const grVec3d	*Front,
	const grVec3d	*Back,	
	grVec3d			*Impact,
	grPlane			*Plane )
{
	grActor* Actor = (grActor *)ActorPtr;
	grCollisionInfo CollisionInfo;
	ActorObj	*Object;

	// ensure valid data
	assert( Actor != NULL );
	// Box CAN be null
	assert( Front != NULL );
	assert( Back != NULL );
	//assert( Impact != NULL );  Removed by Incarnadine. Impact&Plane CAN be NULL.
	//assert( Plane != NULL );
	
	Object = Actor->Object;
	if(!Actor->ActorDefinition) return GR_FALSE;

	if(grActor_Collision((grActor*)Actor,Object->World,Box,Front,Back,&CollisionInfo))
	{
		if(Impact != NULL)	*Impact = CollisionInfo.Impact;
		if(Plane != NULL) *Plane = CollisionInfo.Plane;
		return GR_TRUE;
	}
	return GR_FALSE;
} // Collision()

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
	grActor	*Actor;
	ActorObj *Object;
		
	// ensure valid data
	assert( Instance != NULL );

	// dp nothing if no time has elapsed
	if ( TimeDelta == 0.0f )
	{
		return GR_TRUE;
	}

	// get object
	Actor = (grActor *)Instance;
	assert( Actor != NULL );
	Object = Actor->Object;

	// get actor def body
	/* MOVED TO RENDER()
	Body = grActor_GetBody( Object->ActorDef );
	grBody_ComputeLevelsOfDetail(Body, 1); //TODO replace 1*/

	// adjust actors motion
	if ( Object->Motion != NULL )
	{

		// locals
		grBoolean	Result;
		float		TimeScale;
		float		MotionTime;
		float		Start, End, Total;

		// get time scale
		TimeScale = (float)fabs( Object->MotionTimeScale );

		// adjust motion time
		Object->MotionTime += ( TimeDelta * TimeScale );

		// get total motion time
		Result = grMotion_GetTimeExtents( Object->Motion, &Start, &End );
		assert( Result == GR_TRUE );
		Total = End - Start;

		// adjust motion time if required
		if( Object->MotionTime > Total )
		{
			Object->MotionTime = (float)fmod( Object->MotionTime, Total );
		}

		// set motion time
		if ( Object->MotionTimeScale >= 0.0f )
		{
			MotionTime = Object->MotionTime;
		}
		else
		{
			MotionTime = Total - Object->MotionTime;
		}

		// set new pose
		grActor_SetPose( Actor, Object->Motion, Object->MotionTime, &Actor->Xf );
	}

	// all done
	return GR_TRUE;

} // Frame()

grBoolean GRCC GetExtBox(
	const void	*Instance,	// object instance data
	grExtBox	*BBox )		// where to store extent box
{
		// locals
		grVec3d Pos;
		ActorObj *Object;
		grExtBox	ExtBox;
		grActor *Actor = (grActor*)Instance;
		Object = Actor->Object;
		

		// now update the collision boxes
		// copy ext box
		ExtBox = Object->CollisionExtBox;
		if(!grExtBox_IsValid(&ExtBox))
		{
			// save extent box
			Pos = Actor->Xf.Translation;
			grExtBox_Set (  BBox, 
						Pos.X - 5.0f, Pos.Y - 5.0f, Pos.Z - 5.0f,
						Pos.X + 5.0f, Pos.Y + 5.0f, Pos.Z + 5.0f );
		}
		else
		{
			grVec3d Translation;

			// Dist from box actor pos to box center
			GetBoxTranslation(&ExtBox, Actor, &Translation);
			grExtBox_Translate( &ExtBox, Translation.X, Translation.Y, Translation.Z );
			grExtBox_Scale( &ExtBox, Object->ScaleX, Object->ScaleZ, Object->ScaleY );
			*BBox = ExtBox;
		}

		return GR_TRUE;
}

int	GRCC GetXFormModFlags( const void * Instance )
{
	Instance;
	return( GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE);
}

#ifdef WIN32
grBoolean GRCC EditDialog (void * Instance,HWND Parent)
#endif
#ifdef BUILD_BE
grBoolean GRCC EditDialog (void * Instance,class G3DView* Parent)
#endif
{
	return( GR_TRUE );
}

grBoolean GRCC SendObjMessage(void * Instance, int32 Msg, void * Data)
{
	// locals
	grActor *Actor;
	ActorObj	*Object;

	// ensure valid data
	assert( Instance != NULL );
	assert( Data != NULL );

	// get object
	Actor = (grActor *)Instance;
	Object = Actor->Object;
	assert( Object != NULL );

	// process message
	switch ( Msg )
	{
	case 1:
/*		case G3DEDITOR_GET_GRBRUSH:
		{
			grBrush **hBrush = (grBrush**)Data;
			if( pBrush == NULL )
			{
				if( !CreateGlobalBrush() )
					return(GR_FALSE);
			}
			UpdateGlobalBrush( Object );
			*hBrush = pBrush;
			return( GR_TRUE );
		}
		break;

		// event mesage
		case OBJECT_EVENT_MSG:
		{

			// get event data struct
			Object_EventData *ed = Data;

			// process event type
			switch ( ed->EventType )
			{

				// change motion
				case 0:
				{

					// locals
					grProperty_Data	Data;

					// get motion name
					assert( ed->Args != NULL );
					Data.String = Util_StrDup( ed->Args );

					// apply motion change
					SetProperty( Actor, ACTOROBJ_MOTIONLIST_ID, PROPERTY_COMBO_TYPE, &Data );

					// free temporary string
					grRam_Free( Data.String );
					break;
				}
			}
			break;
		}*/

		// unsupported message
		default:
		{
			return GR_FALSE;
			break;
		}
	}

	// all done
	return GR_FALSE;
}

void* GRCC CreateInstance(void)
{
	grActor* A;
	A = grActor_Create();
	A->Object = GR_RAM_ALLOCATE_STRUCT_CLEAR( ActorObj );	
	A->Object->Engine = NULL;
	InitObjectProperties(A);
	FillProperties();	
	return A;
}

GRAPI grBoolean GRCC Destroy(void **pActor) //grActor **pA)
{
	grActor** pA = (grActor **)pActor;
	grRam_Free((*pA)->Object);
	(*pA)->Object = NULL;
	grActor_Destroy(pA);	
	return GR_TRUE;
}

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
	grActor *Actor;
	ActorObj		*Object;
	int				Size;
	grBoolean		Result = GR_TRUE;
	BYTE Version;
	uint32 Tag;

	OutputDebugString("ActorObject\n");

	// ensure valid data
	assert( File != NULL );
	assert( PtrMgr != NULL );

	// create new object
	Actor = (grActor *)CreateInstance();
	if( Actor == NULL ) return NULL;

	Object = Actor->Object;
	if ( Object == NULL )
	{
		return NULL;
	}

	//read version 
 	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "ActorObject_CreateFromFile:Tag" );
		goto ERROR_CreateFromFile;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "ActorObject_CreateFromFile:Version" );
	       	goto ERROR_CreateFromFile;
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
	    // read actor def name
	    Result &= grVFile_Read( File, &( Size ), sizeof( Size ) );
	    if ( ( Size > 0 ) && ( Result == GR_TRUE ) )
		{
		    Object->ActorDefName = (char *)grRam_Allocate( Size );
		    if ( Object->ActorDefName == NULL )
			{
			    grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
			    goto ERROR_CreateFromFile;
			}
		    Result &= grVFile_Read( File, Object->ActorDefName, Size );
		}

	    // read motion name
	    Result &= grVFile_Read( File, &( Size ), sizeof( Size ) );
	    if ( ( Size > 0 ) && ( Result == GR_TRUE ) )
		{
		    Object->MotionName = (char *)grRam_Allocate( Size );
		    if ( Object->MotionName == NULL )
			{
			    grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
			    goto ERROR_CreateFromFile;
			}
		    Result &= grVFile_Read( File, Object->MotionName, Size );
		}

	    // read light reference bone name
	    Result &= grVFile_Read( File, &( Size ), sizeof( Size ) );
	    if ( ( Size > 0 ) && ( Result == GR_TRUE ) )
		{
		    Object->LightReferenceBoneName = (char *)grRam_Allocate( Size );
		    if ( Object->LightReferenceBoneName == NULL )
			{
			    grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
			    goto ERROR_CreateFromFile;
			}
		    Result &= grVFile_Read( File, Object->LightReferenceBoneName, Size );
		}

	    // read xform
	    Result &= grVFile_Read( File, &( Actor->Xf ), sizeof( Actor->Xf ) );

	    // read scales
	    Result &= grVFile_Read( File, &( Object->ScaleX ), sizeof( Object->ScaleX ) );
	    Result &= grVFile_Read( File, &( Object->ScaleY ), sizeof( Object->ScaleY ) );
	    Result &= grVFile_Read( File, &( Object->ScaleZ ), sizeof( Object->ScaleZ ) );

	    // read fill light color
	    Result &= grVFile_Read( File, &( Object->FillLightRed ), sizeof( Object->FillLightRed ) );
	    Result &= grVFile_Read( File, &( Object->FillLightGreen ), sizeof( Object->FillLightGreen ) );
	    Result &= grVFile_Read( File, &( Object->FillLightBlue ), sizeof( Object->FillLightBlue ) );

	    // read ambient light color
	    Result &= grVFile_Read( File, &( Object->AmbientLightRed ), sizeof( Object->AmbientLightRed ) );
	    Result &= grVFile_Read( File, &( Object->AmbientLightGreen ), sizeof( Object->AmbientLightGreen ) );
	    Result &= grVFile_Read( File, &( Object->AmbientLightBlue ), sizeof( Object->AmbientLightBlue ) );

	    // read per bone lighting flag
	    Result &= grVFile_Read( File, &( Object->PerBoneLighting ), sizeof( Object->PerBoneLighting ) );

	    // read fill light normal
	    Result &= grVFile_Read( File, &( Object->FillLightNormal ), sizeof( Object->FillLightNormal ) );

	    // read use fill light flag
	    Result &= grVFile_Read( File, &( Object->UseFillLight ), sizeof( Object->UseFillLight ) );

	    // read use ambient light from floor flag
	    Result &= grVFile_Read( File, &( Object->UseAmbientLightFromFloor ), sizeof( Object->UseAmbientLightFromFloor ) );

	    // read max dynamic lights amount
	    Result &= grVFile_Read( File, &( Object->MaximumDynamicLightsToUse ), sizeof( Object->MaximumDynamicLightsToUse ) );

	    // read collision ext box info
	    Result &= grVFile_Read( File, &( Object->CollisionExtBox ), sizeof( Object->CollisionExtBox ) );

	    // read draw ext box adjust info
	    Result &= grVFile_Read( File, &( Object->RenderHintExtBox ), sizeof( Object->RenderHintExtBox ) );

	    // read fill normal actor relative flag
	    Result &= grVFile_Read( File, &( Object->FillNormalActorRelative ), sizeof( Object->FillNormalActorRelative ) );

	    // read motion time scale
	    Result &= grVFile_Read( File, &( Object->MotionTimeScale ), sizeof( Object->MotionTimeScale ) );
	}

	// fail if there was an error
	if ( Result == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, NULL );
		goto ERROR_CreateFromFile;
	}

	// all done
	Object->LoadedFromDisk = GR_TRUE;
	return Actor;

	// handle errors
	ERROR_CreateFromFile:

	// free all strings
	if ( Object->ActorDefName != NULL )
	{
		grRam_Free( Object->ActorDefName );
	}
	if ( Object->MotionName != NULL )
	{
		grRam_Free( Object->MotionName );
	}
	if ( Object->LightReferenceBoneName != NULL )
	{
		grRam_Free( Object->LightReferenceBoneName );
	}

	// free object
	grRam_Free( Object );

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
	grActor *Actor;
	BYTE Version = ACTOROBJECT_VERSION;
	ActorObj	*Object;
	grBoolean	Result = GR_TRUE;
	int			Size;
	uint32 Tag = FILE_UNIQUE_ID;

	// ensure valid data
	assert( Instance != NULL );
	assert( File != NULL );
	assert( PtrMgr != NULL );

	// get object data
	Actor = (grActor *)Instance;
	Object = Actor->Object;

	if( !grVFile_Write(	File, &Tag, sizeof(Tag)))
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "ActorObject_WriteToFile:Tag");
	    return( GR_FALSE );
	}
	
	if( !grVFile_Write(	File, &Version, sizeof(Version) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "ActorObject_WriteToFile:Version");
	    return( GR_FALSE );
	}

	// write actor def name
	if ( Object->ActorDefName != NULL )
	{
		Util_WriteString( File, Object->ActorDefName );
	}
	else
	{
		Size = 0;
		Result &= grVFile_Write( File, &Size, sizeof( Size ) );
	}

	// write motion name
	if ( Object->MotionName != NULL )
	{
		Util_WriteString( File, Object->MotionName );
	}
	else
	{
		Size = 0;
		Result &= grVFile_Write( File, &Size, sizeof( Size ) );
	}

	// write light reference bone name
	if ( Object->LightReferenceBoneName != NULL )
	{
		Util_WriteString( File, Object->LightReferenceBoneName );
	}
	else
	{
		Size = 0;
		Result &= grVFile_Write( File, &Size, sizeof( Size ) );
	}

	// write xform
	Result &= grVFile_Write( File, &( Actor->Xf ), sizeof( Actor->Xf ) );

	// write scales
	Result &= grVFile_Write( File, &( Object->ScaleX ), sizeof( Object->ScaleX ) );
	Result &= grVFile_Write( File, &( Object->ScaleY ), sizeof( Object->ScaleY ) );
	Result &= grVFile_Write( File, &( Object->ScaleZ ), sizeof( Object->ScaleZ ) );

	// write fill light color
	Result &= grVFile_Write( File, &( Object->FillLightRed ), sizeof( Object->FillLightRed ) );
	Result &= grVFile_Write( File, &( Object->FillLightGreen ), sizeof( Object->FillLightGreen ) );
	Result &= grVFile_Write( File, &( Object->FillLightBlue ), sizeof( Object->FillLightBlue ) );

	// write ambient light color
	Result &= grVFile_Write( File, &( Object->AmbientLightRed ), sizeof( Object->AmbientLightRed ) );
	Result &= grVFile_Write( File, &( Object->AmbientLightGreen ), sizeof( Object->AmbientLightGreen ) );
	Result &= grVFile_Write( File, &( Object->AmbientLightBlue ), sizeof( Object->AmbientLightBlue ) );

	// write per bone lighting flag
	Result &= grVFile_Write( File, &( Object->PerBoneLighting ), sizeof( Object->PerBoneLighting ) );

	// write fill light normal
	Result &= grVFile_Write( File, &( Object->FillLightNormal ), sizeof( Object->FillLightNormal ) );

	// write use fill light flag
	Result &= grVFile_Write( File, &( Object->UseFillLight ), sizeof( Object->UseFillLight ) );

	// write use ambient light from floor flag
	Result &= grVFile_Write( File, &( Object->UseAmbientLightFromFloor ), sizeof( Object->UseAmbientLightFromFloor ) );

	// write max dynamic lights amount
	Result &= grVFile_Write( File, &( Object->MaximumDynamicLightsToUse ), sizeof( Object->MaximumDynamicLightsToUse ) );

	// write collision ext box adjust info
	Result &= grVFile_Write( File, &( Object->CollisionExtBox ), sizeof( Object->CollisionExtBox ) );

	// write draw ext box adjust info
	Result &= grVFile_Write( File, &( Object->RenderHintExtBox ), sizeof( Object->RenderHintExtBox ) );

	// write fill normal actor relative flag
	Result &= grVFile_Write( File, &( Object->FillNormalActorRelative ), sizeof( Object->FillNormalActorRelative ) );

	// write motion time scale
	Result &= grVFile_Write( File, &( Object->MotionTimeScale ), sizeof( Object->MotionTimeScale ) );

	// log errors
	if ( Result != GR_TRUE )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, NULL );
	}

	// all done
	return Result;

	// eliminate warnings
	PtrMgr;

} // WriteToFile()

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
	grObject* newActor = NULL;
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
		(grVFile_TypeIdentifier)(GR_VFILE_TYPE_MEMORY|GR_VFILE_TYPE_VIRTUAL),
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

	newActor = (grObject *)CreateFromFile(ramfile, ptrMgr);
	if (!newActor) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to reade the object back from a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	grVFile_Close(ramfile);
	grVFile_Close(ramdisk);

	grRam_Free(vfsmemctx.Data);

	return( newActor );
}

void GRCC SetRenderNextTime(void * Instance, grBoolean RenderNextTime)
{
	grActor_SetRenderNextTime((grActor*)Instance, RenderNextTime);
}

grBoolean GRCC SetXForm(void *Actor, const grXForm3d *XF)
{
	grActor_SetXForm((grActor*)Actor, XF);
	return GR_TRUE;
}

grBoolean GRCC GetXForm(const void *Actor, grXForm3d *XF)
{
	grActor_GetXForm((grActor*)Actor, XF);
	return GR_TRUE;
}

//#pragma warning(disable : 4028 4090)
grObjectDef grActor_ObjectDef =
{
	GR_OBJECT_TYPE_ACTOR,
	"Actor",
	GR_OBJECT_VISRENDER,
	CreateInstance,
	CreateRef,
	Destroy,

	AttachWorld,
	DettachWorld,
	AttachEngine,
	DettachEngine,

	NULL, // Soundsystem
	NULL, // Soundsystem
	Render,//grActor_Render,
	Collision,//grActor_Collision,
	GetExtBox,
	CreateFromFile,
	WriteToFile,
	GetPropertyList,
	SetProperty,
	NULL,// GetProperty
	
	SetXForm,
	GetXForm,

	GetXFormModFlags,
	NULL,//grActor_GetChildren,
	NULL,//grActor_AddChild,
	NULL,//grActor_RemoveChild,
	EditDialog,
	SendObjMessage,
	Frame,
	//Royce
	DuplicateInstance,
	//---
	NULL,	// ChangeBoxCollision
	NULL,	// GetGlobalPropertyList
	NULL,	// SetGlobalProperty
	SetRenderNextTime,
};
