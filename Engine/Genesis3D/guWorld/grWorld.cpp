/****************************************************************************************/
/*  grWorld.C                                                                           */
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
/****************************************************************************************/
/*      grWorld.c                                                                       */
/*                                                                                      */
/*      REVISION: 01-13-1999  8:32 p.m.                                                 */
/*			John Pollard : Created                                                      */
/*                                                                                      */
/*      Copyright (c) 1999, Eclipse Entertainment; All rights reserved.                 */
/*                                                                                      */
/****************************************************************************************/
#include <stdio.h>
#include <memory.h>		// memset
#include <assert.h>
#include <string.h>
#include <stdlib.h> //free

#include "Dcommon.h"
#include "Engine.h"

// Public dependents
#include "grWorld.h"
#include "Actor.h"  // Added by Incarnadine

// Private dependents
#include "Ram.h"
#include "Errorlog.h"
#include "grMaterial._h"		// grMaterial_ArraySetEngine
#include "grFrustum.h"
#include "grChain.h"
#include "grPortal.h"
#include "Util.h"			// Added by Icestorm [MLB-ICE]

#include "grPtrMgr._h"
#include "log.h"

//#define FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK

#ifdef _DEBUG 
	#define WORLD_DEBUG_OUTPUT_LEVEL	1
	//#define WORLD_DEBUG_OUTPUT_LEVEL	2
#else
	#define WORLD_DEBUG_OUTPUT_LEVEL	0
#endif

//
//	Please note that this module is called grWorld.  It is only temporary 
//	until grWorld replaces grWorld... (uh huh...)
//
void ProcUtil_Init(void);

#pragma message (" Clean up Add/Remove code.  There are some leaks on error...")

//========================================================================================
// Local #defines
//========================================================================================
#define	GR_WORLD_START_FACEINFO		16
#define	GR_WORLD_START_MATERIALS	16
#define GR_WORLD_START_LIGHTS		16

#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

//========================================================================================
//	Local static defines
//========================================================================================
static grBoolean grWorld_DestroyAutoRemoveUserPolys(grWorld *World);
static grBoolean grWorld_RenderUserPolys(const grWorld *World, const grCamera *Camera, const grFrustum *WorldSpaceFrustum);
static grBoolean grWorld_CreateArrays(grWorld *World);
static grBoolean grWorld_WriteHeader(const grWorld *World, grVFile *VFile);
static grBoolean grWorld_ReadHeader(grWorld *World, grVFile *VFile);

static grBoolean WriteObject(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *);
static grBoolean grWorld_WriteArrays(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr);
static grBoolean WriteLight(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr);
static grBoolean WritePtrMgrVerification(grVFile *VFile, const grPtrMgr *PtrMgr);

static grBoolean ReadObject(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr);
static grBoolean grWorld_ReadArrays(grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr);
static grBoolean ReadLight(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr);
static grBoolean ReadPtrMgrVerification(grVFile *VFile, const grPtrMgr *PtrMgr);


typedef struct grWorld
{
	int32						RefCount;

	// Various arrays shared by ALL objects
	grFaceInfo_Array			*FaceInfoArray;
	grMaterial_Array			*MaterialArray;

	grChain						*LightChain;		// Linked list of lights
	grChain						*OldLights;			// Linked list of backup lights

	grChain						*Objects;			// Linked list of objects

	grChain						*DLightChain;							// Dynamic light chain

	grChain						*UserPolys;
	grChain						*AutoRemoveUserPolys;

	//grChain						*Actors;  // Added by Incarnadine
	grChain *CollisionObjectTypes; // Incarnadine
	int32 CollisionLevel; // Incarnadine

	// paradoxnj - Useless and incomplete
	//grChain						*ShaderChain; // Added by CyRiuS (Timothy Roff)
	//grChain						*ActorScriptChain; //Added by cyrius (Timothy Roff)

	grEngine					*Engine;
	grSound_System				*SoundSystem;
	
	grResourceMgr				*ResourceMgr;

	int32						Recursion;

	grObject					*Model;
	
} grWorld;

grWorld_DebugInfo				g_WorldDebugInfo;

static grBoolean grWorld_RenderALL(grWorld *World, grCamera *Camera, grFrustum *CameraSpaceFrustum);
static grBoolean GRCC grWorld_RenderFromMirrorPortal(const grPortal *Portal, const grWorld *World, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);

/*
grVFile * grWorld_GetObjectDirectory(grWorld *World)
{
	assert(World);
	return World->FileDirectory;
}

grObject * grWorld_FindObjectFromName(grWorld *World, char *NameToFind)
{
	grObject *CurrObject;
	const char *Name;

	assert (World);
	assert (NameToFind);

	// loop initializers
	CurrObject = nullptr;

	while (GR_TRUE)
		{
		CurrObject = grWorld_GetNextObject(World, CurrObject);

		if (CurrObject == nullptr)
			break;

		Name = grObject_GetName( CurrObject );

		if (!Name) continue;

		if (stricmp(NameToFind, Name) == 0)
			return (CurrObject);
		}

	return nullptr;
}
*/

////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_StrDup()
//
////////////////////////////////////////////////////////////////////////////////////////
static char * Util_StrDup(
	const char	*const String )	// string to copy
{

	// locals
	char	*NewString{};

	// ensure valid data
	assert( String != nullptr );

	// copy string
	NewString = (char *)grRam_Allocate( strlen( String ) + 1 );
	if ( NewString ) 
	{
		strcpy( NewString, String );
	}
	else
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, nullptr );
	}

	// return string
	return NewString;

} // Util_StrDup()




// HACK of all mothers!  This should be the next thing cleaned up, this is just to get it working!!!
void					*h_World;

typedef struct 
{
	const grPlane		*Plane;
	const grXForm3d		*FaceXForm;
	grWorld				*World;
	grCamera			*Camera;
	grFrustum			*Frustum;
} PortalMsgData;

//========================================================================================
//	CreateMirrorObjectInstance
//========================================================================================
void * GRCC CreateMirrorObjectInstance(void)
{
	return grPortal_Create();
}

//========================================================================================
//	RefMirrorObjectInstance
//========================================================================================
void GRCC RefMirrorObjectInstance(void *Portal)
{
	grPortal_CreateRef((grPortal*)Portal);
}

//========================================================================================
//	DestroyMirrorObjectInstance
//========================================================================================
grBoolean GRCC DestroyMirrorObjectInstance(void **Portal)
{
	grPortal_Destroy((grPortal**)Portal);

	return GR_TRUE;
}

#define MAX_MIRROR_RECURSION		1

#pragma message ("Fix this big hack-a-rama (MirrorRecursion global)")
int32 MirrorRecursion		= 0;
grBoolean					h_LeftHanded;

//========================================================================================
//	RenderMirrorObjectInstance
//========================================================================================
static grBoolean GRCC RenderMirrorObjectInstance(const void *PortalInst, const grWorld *World, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	return GR_TRUE;
}


void *GRCC ReadMirrorObjectInstance(grVFile *VFile, grPtrMgr *PtrMgr)
{
	return grPortal_Create();
}

//========================================================================================
//	WriteMirrorObjectInstance
//========================================================================================
grBoolean GRCC WriteMirrorObjectInstance(const void *Instance, grVFile *VFile, grPtrMgr *PtrMgr)
{
	return GR_TRUE;
}


//========================================================================================
//	RenderMirrorObjectInstance2
//========================================================================================
static grBoolean RenderMirrorObjectInstance2(void *Portal, const grPlane *Plane, const grXForm3d *FaceXForm, grWorld *World, grCamera *Camera, grFrustum *Frustum)
{
	grXForm3d		XForm{};
	grXForm3d		MirrorXForm{};
	grPlane			FrontPlane{};
	grBoolean		Ret{};
	grXForm3d		WorldToCameraXForm{};
	grPortal		*P = (grPortal*)Portal;

	if (P->Recursion > 0)
		return GR_TRUE;

	// Get Camera XForm
	grCamera_GetXForm((grCamera*)Camera, &XForm);

	// Get the WorldToCameraXForm
	grCamera_GetTransposeXForm(Camera, &WorldToCameraXForm);

	// Transform the FacePlane to camera space
	grPlane_Transform(Plane, &WorldToCameraXForm, &FrontPlane);

	// Add the Plane to the Frustum
	if (!grFrustum_AddPlane(Frustum, &FrontPlane, GR_TRUE))
		return GR_FALSE;

	// Mirror the camera XForm
	grXForm3d_Mirror(&XForm, &Plane->Normal, Plane->Dist, &MirrorXForm);
	
	h_LeftHanded = !h_LeftHanded;

	// Put the new mirrored XForm into the camera
	grCamera_SetXForm(Camera, &MirrorXForm);

	// Increase the portal recursion count
	P->Recursion++;

	// Render the scene from this camera
	Ret = grWorld_Render(World, Camera, Frustum);

	// Decrease the portal recursion count
	assert(P->Recursion > 0);
	P->Recursion--;

	// Restore Camera XForm
	grCamera_SetXForm(Camera, &XForm);
	h_LeftHanded = !h_LeftHanded;

	return Ret;
}

//========================================================================================
//	SendMirrorMessage
//========================================================================================
static grBoolean GRCC SendMirrorMessage(void *Portal, int32 Msg, void *Data)
{
	switch (Msg)
	{
		case 0:
		{
			PortalMsgData		*MData;
			
			MData = (PortalMsgData*)Data;

			return RenderMirrorObjectInstance2(	Portal, 
												MData->Plane, 
												MData->FaceXForm, 
												MData->World, 
												MData->Camera, 
												MData->Frustum);
		}

		default:
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	MirrorObjectDef
//========================================================================================
grObjectDef MirrorObjectDef = 
{
	GR_OBJECT_TYPE_PORTAL,
	"MirrorObject",
	GR_OBJECT_HIDDEN,

	CreateMirrorObjectInstance,
	RefMirrorObjectInstance,
	DestroyMirrorObjectInstance,

	nullptr,
	nullptr,

	nullptr,
	nullptr,

	nullptr,
	nullptr,

	RenderMirrorObjectInstance,

	nullptr,
	nullptr,

	ReadMirrorObjectInstance,
	WriteMirrorObjectInstance,

	nullptr,
	nullptr,
	nullptr,

	nullptr,
	nullptr,

	nullptr,

	nullptr,
	nullptr,
	nullptr,

	nullptr,
	SendMirrorMessage,
	nullptr,
	nullptr,
	nullptr,	// Added by Icestorm: ChangeBoxCollision
	nullptr,	// GetGlobalPropertyList
	nullptr,	// SetGlobalProperty
	nullptr,	//SetRenderNextTime,
};

//========================================================================================
//	grWorld_AddDefaultObjects
//========================================================================================
static grBoolean grWorld_AddDefaultObjects(grWorld *World)
{
	grObject			*MirrorObject;

	// Add the mirror portal object
	MirrorObject = grObject_Create("MirrorObject");

	if (!MirrorObject)
		return GR_FALSE;

	// [MLB-ICE]
	//MirrorObject->Name = strdup("Mirror1");
	MirrorObject->Name = Util_StrDup("Mirror1");

	if (!grWorld_AddObject(World, MirrorObject))
	{
		grObject_Destroy(&MirrorObject);	// Added
		return GR_FALSE;
	}

	grObject_Destroy(&MirrorObject);	// Added
	// [MLB-ICE] EOB

	return GR_TRUE;
}

//========================================================================================
//	grWorld_CreateBase
//========================================================================================
static grWorld *grWorld_CreateBase(grResourceMgr *ResourceMgr)
{
	grWorld		*World{};

	assert(ResourceMgr);

	World = (grWorld *)grRam_AllocateClear(sizeof(*World));

	if (!World)
		return nullptr;

	World->RefCount = 1;

	World->Model = nullptr;

	// Added by Incarnadine
	// create the actor chain
	//World->Actors = grChain_Create();

	//if (!World->Actors)
	//	goto ExitWithError;
	World->CollisionObjectTypes = grChain_Create();

	if(!World->CollisionObjectTypes)
		goto ExitWithError;

	grWorld_SetCollisionLevel(World,COLLIDE_EXTBOX);	

	// paradoxnj - Useless and incomplete
	// create the shader chain (cyrius)
	/*World->ShaderChain = grChain_Create();

	if(!World->ShaderChain)
		goto ExitWithError;

	// create the actor script chain (cyrius)
	World->ActorScriptChain = grChain_Create();

	if(!World->ActorScriptChain)
		goto ExitWithError;
	*/
	
	// Create the Dyanamic Light Chain
	World->DLightChain = grChain_Create();

	if (!World->DLightChain)
		goto ExitWithError;

	// Create the UserPoly Chain
	World->UserPolys = grChain_Create();

	if (!World->UserPolys)
		goto ExitWithError;

	// Create the AutoRemoveUserPoly Chain
	World->AutoRemoveUserPolys = grChain_Create();

	if (!World->AutoRemoveUserPolys)
		goto ExitWithError;

	// Assign the resource mgr. The world holds its own reference (released in
	// grWorld_Destroy); callers keep and release theirs.
	World->ResourceMgr = ResourceMgr;

	if ( World->ResourceMgr == nullptr )
		goto ExitWithError;

	grResource_MgrIncRefcount( World->ResourceMgr );

	// Register the built-in objects
	{
		//extern grObjectDef	PortalObjectDef;	// Icestorm: Seems to be useless now...

		grObject_RegisterGlobalObjectDef(&MirrorObjectDef);
		//grObject_RegisterGlobalObjectDef(&PortalObjectDef);
	}

	// HACK of all mothers!
	h_World = World;

	return World;

	ExitWithError:
	{
		grErrorLog_AddString(-1, "grWorld_CreateBase failed...", nullptr);

		if (World)
		{
			if (World->DLightChain)
				grChain_Destroy(&World->DLightChain);

			if (World->UserPolys)
				grChain_Destroy(&World->UserPolys);

			if (World->AutoRemoveUserPolys)
				grChain_Destroy(&World->AutoRemoveUserPolys);

			if (World->Objects)
				grChain_Destroy(&World->Objects);

			if (World->ResourceMgr)
				grResource_MgrDestroy(&World->ResourceMgr);

			//if (World->Actors) // Added cause it was needed (cyrius)
			//	grChain_Destroy(&World->Actors);
			if(World->CollisionObjectTypes)
				grChain_Destroy(&World->CollisionObjectTypes);

			// paradoxnj - Useless and incomplete
			/*if(World->ShaderChain) //(cyrius)
				grChain_Destroy(&World->ShaderChain);

			if(World->ActorScriptChain) //(cyrius)
				grChain_Destroy(&World->ActorScriptChain);
			*/

			grRam_Free(World);
		}

		return nullptr;
	}
}

//========================================================================================
//	grWorld_Create
//========================================================================================
GRAPI grWorld * GRCC grWorld_Create(grResourceMgr *pResourceMgr)
{
	grWorld		*World{};

	assert(pResourceMgr);
	
	World = grWorld_CreateBase(pResourceMgr);

	if (!World)
		return nullptr;

	if (!grWorld_CreateArrays(World))
		goto ExitWithError;
	
	// Create the light chain
	World->LightChain = grChain_Create();

	if (!World->LightChain)
		goto ExitWithError;

	// Create the backup light chain
	World->OldLights = grChain_Create();

	if (!World->OldLights)
		goto ExitWithError;

	// create the object chain
	World->Objects = grChain_Create();

	if (!World->Objects)
		goto ExitWithError;

	// paradoxnj - Useless and incomplete
	//Init the ProcUtil routines (cyrius)
	//ProcUtil_Init();
	

#ifndef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	if (!grWorld_AddDefaultObjects(World))
		goto ExitWithError;
#endif

	return World;

	ExitWithError:
	{
		if (World)
			grWorld_Destroy(&World);

		return nullptr;
	}
}


//========================================================================================
//	grWorld_CreateFromFile
//========================================================================================
GRAPI grWorld * GRCC grWorld_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr, grResourceMgr *pResourceMgr)
{
	grWorld* World{};

	World = grWorld_CreateBase(pResourceMgr);

	if (!World)
		return nullptr;

	if (!PtrMgr)
	{
		PtrMgr = grPtrMgr_Create();
		
		if (!PtrMgr)
			goto ExitWithError;
	}
	else
	{
		// Ref it, so destroy code can be called in both cases
		if (!grPtrMgr_CreateRef(PtrMgr))
			goto ExitWithError;
	}

	// Krouer: inform the PtrMgr on the resource and world it behaves
	PtrMgr->pWorld = World;
	PtrMgr->pResMgr = pResourceMgr;

	// Read the header
	if (!grWorld_ReadHeader(World, VFile))
		goto ExitWithError;

	//if (!ReadPtrMgrVerification(VFile, PtrMgr))
	//	goto ExitWithError;

	// Load the world arrays from disk
	if (!grWorld_ReadArrays(World, VFile, PtrMgr))
		goto ExitWithError;

	// Load the lights off disk
	World->LightChain = grChain_CreateFromFile(VFile, ReadLight, nullptr, PtrMgr);

	if (!World->LightChain)
		goto ExitWithError;

	// Load the backup lights off disk
	World->OldLights = grChain_CreateFromFile(VFile, ReadLight, nullptr, PtrMgr);

	if (!World->OldLights)
		goto ExitWithError;

	// Load the objects off disk
	World->Objects = grChain_CreateFromFile(VFile, ReadObject, World, PtrMgr);

	if (!World->Objects)
		goto ExitWithError;

	//if (!ReadPtrMgrVerification(VFile, PtrMgr))
	//	goto ExitWithError;

	return World;
	
	ExitWithError:
	{
		if (World)
		{
			if (PtrMgr)
				grPtrMgr_Destroy(&PtrMgr);

			grWorld_Destroy(&World);
		}
		return nullptr;
	}
}



//========================================================================================
//	grWorld_WriteToFile
//========================================================================================
GRAPI grBoolean GRCC grWorld_WriteToFile(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr)
{
	if (!PtrMgr)		// The world needs an PtrMgr, so create one if one not supplied...
	{
		PtrMgr = grPtrMgr_Create();
		
		if (!PtrMgr)
			goto ExitWithError;
	}
	else
	{
		// Ref it, so destroy code can be called in both cases
		if (!grPtrMgr_CreateRef(PtrMgr))
			goto ExitWithError;
	}

	// Write out header info
	if (!grWorld_WriteHeader(World, VFile))
	{
		grErrorLog_AddString(-1, "grWorld_WriteToFile:  grWorld_WriteHeader failed.", nullptr);
		goto ExitWithError;
	}

	//if (!WritePtrMgrVerification(VFile, PtrMgr))
	//	goto ExitWithError;

	// Write the arrays
	if (!grWorld_WriteArrays(World, VFile, PtrMgr))
	{
		grErrorLog_AddString(-1, "grWorld_WriteToFile:  grWorld_WriteArrays failed.", nullptr);
		goto ExitWithError;
	}

	// Write out the Lights
	if (!grChain_WriteToFile(World->LightChain, VFile, WriteLight, nullptr, PtrMgr))
	{
		grErrorLog_AddString(-1, "grWorld_WriteToFile:  grChain_WriteToFile failed for lights.", nullptr);
		goto ExitWithError;
	}

	// Write out the backup Lights
	if (!grChain_WriteToFile(World->OldLights, VFile, WriteLight, nullptr, PtrMgr))
	{
		grErrorLog_AddString(-1, "grWorld_WriteToFile:  grChain_WriteToFile failed for lights.", nullptr);
		goto ExitWithError;
	}


	// Write out the objects
	if (!grChain_WriteToFile(World->Objects, VFile, WriteObject, nullptr, PtrMgr))
	{
		grErrorLog_AddString(-1, "grWorld_WriteToFile:  grChain_WriteToFile failed for objects.", nullptr);
		goto ExitWithError;
	}

	// This is for debugging, so you can run your mouse over, and examine contents
	{
		int32		NumPtrs, PtrRefs;

		grPtrMgr_GetPtrCount(PtrMgr, &NumPtrs);
		grPtrMgr_GetPtrRefs(PtrMgr, &PtrRefs);
	}

	//if (!WritePtrMgrVerification(VFile, PtrMgr))
	//	goto ExitWithError;

	if (PtrMgr)
		grPtrMgr_Destroy(&PtrMgr);

	return GR_TRUE;

	ExitWithError:
	{
		return GR_FALSE;
	}
}

//========================================================================================
//	grWorld_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grWorld_CreateRef(grWorld *World)
{
	assert(World);
	assert(World->RefCount >= 0);

	World->RefCount++;

	return GR_TRUE;
}


//========================================================================================
//	grWorld_Destroy
//========================================================================================
GRAPI void GRCC grWorld_Destroy(grWorld **pWorld)
{
	grChain_Link		*Link{};
	grWorld				*World{};

	assert(pWorld);
	assert(*pWorld);

	World =	*pWorld;

	//assert(World->RefCount > 0);
#ifdef _DEBUG
	Log_Printf("grWorld_Destroy %p, %d\n", World, World->RefCount);
#endif

	World->RefCount--;

	if (World->RefCount == 0)
	{
		// destroy all objects
		if (World->Objects)
		{
			// destroy each object
			for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
			{
				// locals
				grObject	*Object;

				// jet object pointer
				Object = (grObject *)grChain_LinkGetLinkData( Link );

				// BEGIN - Proper destruction of objects - paradoxnj 5/9/2005
				// Worlds made with grWorld_Create (no level loaded) have no model object
				if (World->Model != nullptr)
					grObject_RemoveChild(World->Model,Object);

				// Detach in the reverse of the attach order in grWorld_AddObject (and the
				// same order as grWorld_RemoveObject): objects free world-owned resources
				// in DettachWorld that their DettachEngine may still need.
				if (World->SoundSystem != nullptr)
					grObject_DettachSoundSystem(Object, World->SoundSystem);

				if (World->Engine != nullptr)
					grObject_DettachEngine(Object, World->Engine);

				grObject_DettachWorld(Object, World);
				// END - Proper destruction of objects - paradoxnj 5/9/2005

				grObject_Destroy( &Object );
			}

			// destroy object chain
			grChain_Destroy( &( World->Objects ) );
		}

		// destroy all actors -- Incarnadine
/*		
		if (World->Actors)
		{
			// destroy each object
			for (Link = grChain_GetFirstLink(World->Actors); Link; Link = grChain_LinkGetNext(Link))
			{
				// locals
				grActor *Actor;

				// jet object pointer
				Actor = (grActor *)grChain_LinkGetLinkData( Link );
				grActor_Destroy( &Actor );
			}

			// destroy object chain
			grChain_Destroy( &( World->Actors ) );
		}
*/
		if (World->CollisionObjectTypes)
		{
			// destroy each object
			for (Link = grChain_GetFirstLink(World->CollisionObjectTypes); Link; Link = grChain_LinkGetNext(Link))
			{
				// locals
				char *Data;

				// jet object pointer
				Data = (char *)grChain_LinkGetLinkData( Link );
				grRam_Free(Data);				
			}

			// destroy object chain
			grChain_Destroy( &( World->CollisionObjectTypes ) );
		}


		// paradoxnj - Useless and incomplete
		// destroy all shaders -- CyRiuS
		/*if (World->ShaderChain)
		{
			// destroy each shader
			for(Link = grChain_GetFirstLink(World->ShaderChain); Link; Link = grChain_LinkGetNext(Link))
			{
				// locals
				grShader	*Shader;

				// jet object pointer
				Shader = (grShader *)grChain_LinkGetLinkData( Link );
				grShader_Destroy( &Shader );
			}

			//destroy object chain
			grChain_Destroy( &( World->ShaderChain ) );
		}

		// destroy all ActorScripts -- CyRiuS
		if (World->ActorScriptChain)
		{
			// destroy each shader
			for(Link = grChain_GetFirstLink(World->ActorScriptChain); Link; Link = grChain_LinkGetNext(Link))
			{
				// locals
				grScript	*ActorScript;

				// jet object pointer
				ActorScript = (grScript *)grChain_LinkGetLinkData( Link );
				grScript_Destroy( &ActorScript );
			}

			//destroy object chain
			grChain_Destroy( &( World->ActorScriptChain ) );
		}
		*/

		// Destroy all dlights
		if (World->DLightChain)
		{
			for (Link = grChain_GetFirstLink(World->DLightChain); Link; Link = grChain_LinkGetNext(Link))
			{
				grLight		*Light;

				Light = (grLight*)grChain_LinkGetLinkData(Link);
				grLight_Destroy(&Light);
			}
			grChain_Destroy(&World->DLightChain);
		}

		// Destroy all Lights
		if (World->LightChain)
		{
			for (Link = grChain_GetFirstLink(World->LightChain); Link; Link = grChain_LinkGetNext(Link))
			{
				grLight		*Light;

				Light = (grLight*)grChain_LinkGetLinkData(Link);
				grLight_Destroy(&Light);
			}

			grChain_Destroy(&World->LightChain);
		}

		// Destroy all backup lights
		if (World->OldLights)
		{
			for (Link = grChain_GetFirstLink(World->OldLights); Link; Link = grChain_LinkGetNext(Link))
			{
				grLight		*Light;

				Light = (grLight*)grChain_LinkGetLinkData(Link);
				grLight_Destroy(&Light);
			}

			grChain_Destroy(&World->OldLights);
		}

		// Destroy all user polys
		if (World->UserPolys)
		{
			for (Link = grChain_GetFirstLink(World->UserPolys); Link; Link = grChain_LinkGetNext(Link))
			{
				grUserPoly		*Poly;

				Poly = (grUserPoly*)grChain_LinkGetLinkData(Link);
				grUserPoly_Destroy(&Poly);
			}

			grChain_Destroy(&World->UserPolys);
		}

		// Destroy all autoremove user polys
		if (World->AutoRemoveUserPolys)
		{
			for (Link = grChain_GetFirstLink(World->AutoRemoveUserPolys); Link; Link = grChain_LinkGetNext(Link))
			{
				grUserPoly		*Poly;
	
				Poly = (grUserPoly*)grChain_LinkGetLinkData(Link);
				grUserPoly_Destroy(&Poly);
			}
	
			grChain_Destroy(&World->AutoRemoveUserPolys);
		}

		// Destroy faceinfo array
		if (World->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&(World->FaceInfoArray));
	
		// destroy material array
		if (World->MaterialArray)
			grMaterial_ArrayDestroy(&(World->MaterialArray));

		// destroy resource manager
		if ( World->ResourceMgr != nullptr )
		{
			grResource_MgrDestroy( &( World->ResourceMgr ) );
		}

		if (World->Model != nullptr)
		{
			grObject_Destroy(&World->Model);
		}

		grRam_Free(World);
	}

	*pWorld = nullptr;
}

//========================================================================================
//	grWorld_SetEngine
//========================================================================================
GRAPI grBoolean GRCC grWorld_SetEngine(grWorld *World, grEngine *Engine)
{
	grChain_Link	*Link{};

	assert(World);

	if (!grMaterial_ArraySetEngine(World->MaterialArray, Engine))
	{
		grErrorLog_AddString(-1, "grWorld_SetEngine:  grMaterial_ArraySetEngine failed.", nullptr);
		return GR_FALSE;
	}

	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject* Object{};

		Object = (grObject*)grChain_LinkGetLinkData(Link);

		if (!grObject_AttachEngine( Object, Engine ))
		{
			grErrorLog_AddString(-1, "grWorld_SetEngine:  grObject_AttachEngine failed.", nullptr);
			return GR_FALSE;
		}
	}

	World->Engine = Engine;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_GetMaterialArray
//========================================================================================
GRAPI grMaterial_Array * GRCC grWorld_GetMaterialArray(const grWorld *World)
{
	assert(World);

	return World->MaterialArray;
}

//========================================================================================
//	grWorld_GetFaceInfoArray
//========================================================================================
GRAPI grFaceInfo_Array * GRCC grWorld_GetFaceInfoArray(const grWorld *World)
{
	assert(World);

	return World->FaceInfoArray;
}

//========================================================================================
//	grWorld_GetLightChain
//========================================================================================
GRAPI grChain * GRCC grWorld_GetLightChain(const grWorld *World)
{
	assert(World);

	return World->LightChain;
}

//========================================================================================
//	grWorld_GetDLightChain
//========================================================================================
GRAPI grChain * GRCC grWorld_GetDLightChain(const grWorld *World)
{
	assert(World);

	return World->DLightChain;
}

GRAPI	int32	GRCC grWorld_GetRenderRecursion( const grWorld *World )
{
	return( World->Recursion );
}

static uint32 WorldVisFrame = 0;


/*
//========================================================================================
//	grWorld_RenderSprite
//	Nothing much here.
//========================================================================================
GRAPI grBoolean GRCC grEngine_RenderSprite(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	grFrustum Fru,WorldSpaceFrustum;
	if (!Frustum)
	{
		grFrustum_SetFromCamera(&Fru, Camera);
		Frustum = &Fru;
	}

	grFrustum_TransformToWorldSpace(Frustum, Camera, &WorldSpaceFrustum);

	return RenderSprite(Poly, Engine, Camera, &WorldSpaceFrustum);
}
*/

//========================================================================================
//	grWorld_Render
//	This function can be recursively re-entered...
//========================================================================================
GRAPI grBoolean GRCC grWorld_Render(grWorld *World, grCamera *Camera, grFrustum *CameraSpaceFrustum)
{
	assert(World);
	assert(World->Engine);
	assert(Camera);

	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
		OutputDebugString("BEGIN grWorld_Render\n");
	#endif

	#ifdef _DEBUG
	{
		grEngine_FrameState	FrameState;

		grEngine_GetFrameState(World->Engine, &FrameState);
		assert(FrameState == FrameState_Begin);
	}
	#endif
	
	if (World->Recursion == 0)
	{
		memset(&g_WorldDebugInfo, 0, sizeof(g_WorldDebugInfo));
	}

	World->Recursion++;

	g_WorldDebugInfo.NumRenders++;

	// Render the scene
	if (!grWorld_RenderALL(World, Camera, CameraSpaceFrustum))
		return GR_FALSE;

	assert(World->Recursion > 0);
	World->Recursion--;

	if (World->Recursion == 0)
	{
		// Destroy all AutoRemove UserPolys
		if (!grWorld_DestroyAutoRemoveUserPolys(World))
			return GR_FALSE;

	#if 0
		grEngine_FlushScene(World->Engine);
		grEngine_Printf(World->Engine, 3, 0*17+10, "Renders: %2i, Objects: %2i, Portals: %2i", g_WorldDebugInfo.NumRenders, g_WorldDebugInfo.NumObjects, g_WorldDebugInfo.NumPortals);
		grEngine_Printf(World->Engine, 3, 1*17+10, "Model Polys: %3i/%3i, Actor Polys: %3i", g_WorldDebugInfo.NumTransformedPolys, g_WorldDebugInfo.NumRenderedPolys, g_WorldDebugInfo.NumActorPolys);
		grEngine_Printf(World->Engine, 3, 2*17+10, "Nodes: %4i, Leaves: %4i", g_WorldDebugInfo.NumNodes, g_WorldDebugInfo.NumRenderedPolys, g_WorldDebugInfo.NumLeaves);
	#endif
	}

	WorldVisFrame++;

	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
		OutputDebugString("END grWorld_Render\n");
	#endif

	return GR_TRUE;
}

//========================================================================================
//	grWorld_AddLight
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddLight(grWorld *World, grLight *Light, grBoolean Update)
{
	grLight* OldLight{};

	assert(World);
	assert(grLight_IsValid(Light) == GR_TRUE);
	assert(grChain_FindLink(World->LightChain, Light) == nullptr);
	
	// Add the light to the worlds light chain
	if (!grChain_AddLinkData(World->LightChain, Light))
		return GR_FALSE;

	// Ref the light
	grLight_CreateRef(Light);		

	// Make a backup copy of the light
	OldLight = grLight_CreateFromLight(Light);

	if (!OldLight)
		goto ExitWithError1;

	// Add the light to the worlds backup light chain
	if (!grChain_AddLinkData(World->OldLights, OldLight))
	{
		grLight_Destroy(&OldLight);
		goto ExitWithError1;
	}

	// Make sure indexes are the same
	assert(grChain_LinkDataGetIndex(World->LightChain, Light) == grChain_LinkDataGetIndex(World->OldLights, OldLight));

	if (Update)
	{
		grChain_Link		*Link;

		// Update is set, so go through all the objects, and notify them of a light update
		for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
		{
			grObject	*Object;
		
			Object = (grObject*)grChain_LinkGetLinkData(Link);

			grObject_SendMessage (Object, GR_OBJECT_MSG_WORLD_ADD_SLIGHT_UPDATE, Light);
		}
	}

	return GR_TRUE;

	ExitWithError1:
	{
		grBoolean Ret;

		Ret = grChain_RemoveLinkData(World->LightChain, Light);
		assert(Ret == GR_TRUE);

		return GR_FALSE;
	}
}

//========================================================================================
//	grWorld_RemoveLight
//========================================================================================
GRAPI grBoolean GRCC grWorld_RemoveLight(grWorld *World, grLight *Light, grBoolean Update)
{
	uint32		Index{};
	grLight* OldLight{};
	
	assert(World);
	assert(grLight_IsValid(Light) == GR_TRUE);
	assert(grChain_FindLink(World->LightChain, Light));

	Index = grChain_LinkDataGetIndex(World->LightChain, Light);

	if (!grChain_RemoveLinkData(World->LightChain, Light))
		return GR_FALSE;

	OldLight = (grLight *)grChain_GetLinkDataByIndex(World->OldLights, Index);
	assert(OldLight);

	if (!grChain_RemoveLinkData(World->OldLights, OldLight))
		return GR_FALSE;

	if (Update)
	{
		grChain_Link		*Link;

		// Update is set, so go through all the objects, and notify them of a light update
		for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
		{
			grObject	*Object;
		
			Object = (grObject*)grChain_LinkGetLinkData(Link);

			grObject_SendMessage (Object, GR_OBJECT_MSG_WORLD_REMOVE_SLIGHT_UPDATE, Light);
		}
	}

	grLight_Destroy(&Light);
	grLight_Destroy(&OldLight);

	return GR_FALSE;
}

//========================================================================================
//	grWorld_UpdateLight
//========================================================================================
GRAPI grBoolean GRCC grWorld_UpdateLight(grWorld *World, grLight *Light)
{
	if (!grWorld_RemoveLight(World, Light, GR_TRUE))
		return GR_FALSE;

	if (!grWorld_AddLight(World, Light, GR_TRUE))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_GetNextLight
//========================================================================================
GRAPI grLight * GRCC grWorld_GetNextLight(grWorld *World, grLight *Start)
{
	assert(World);

	return (grLight *)grChain_GetNextLinkData(World->LightChain, Start);
}

//========================================================================================
//	grWorld_AddDLight
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddDLight(grWorld *World, grLight *Light)
{
	assert(World);
	assert(grLight_IsValid(Light) == GR_TRUE);
	assert(grChain_FindLink(World->DLightChain, Light) == nullptr);
	
	// Add the light to the worlds dlight chain
	if (!grChain_AddLinkData(World->DLightChain, Light))
		return GR_FALSE;

	grLight_CreateRef(Light);		// Ref the light

	return GR_TRUE;
}

//========================================================================================
//	grWorld_RemoveDLight
//========================================================================================
GRAPI grBoolean GRCC grWorld_RemoveDLight(grWorld *World, grLight *Light)
{
	assert(World);
	assert(grLight_IsValid(Light) == GR_TRUE);
	assert(grChain_FindLink(World->DLightChain, Light));
	
	// Remove the light from the worlds dlight chain
	if (!grChain_RemoveLinkData(World->DLightChain, Light))
		return GR_FALSE;

	grLight_Destroy(&Light);		// De-Ref the light

	return GR_TRUE;
}

//========================================================================================
//	grWorld_GetNextDLight
//========================================================================================
GRAPI grLight * GRCC grWorld_GetNextDLight(grWorld *World, grLight *Start)
{
	assert(World);

	return (grLight *)grChain_GetNextLinkData(World->DLightChain, Start);
}

//========================================================================================
//	grWorld_AddUserPoly
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddUserPoly(grWorld *World, grUserPoly *Poly, grBoolean AutoRemove)
{
	assert(World);
	assert(Poly);

	if (AutoRemove)
	{
		assert(!grChain_FindLink(World->AutoRemoveUserPolys, Poly));
		if (!grChain_AddLinkData(World->AutoRemoveUserPolys, Poly))
			return GR_FALSE;
	}
	else
	{
		assert(!grChain_FindLink(World->UserPolys, Poly));
		if (!grChain_AddLinkData(World->UserPolys, Poly))
			return GR_FALSE;
	}
	
	if (!grUserPoly_CreateRef(Poly))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_RemoveUserPoly
//========================================================================================
GRAPI grBoolean GRCC grWorld_RemoveUserPoly(grWorld *World, grUserPoly *Poly)
{
	assert(World);
	assert(Poly);

	assert(grChain_FindLink(World->UserPolys, Poly));

	if (!grChain_RemoveLinkData(World->UserPolys, Poly))
		return GR_FALSE;

	grUserPoly_Destroy(&Poly);		// Re-ref

	return GR_TRUE;
}

//========================================================================================
//	Objects
//========================================================================================

#ifdef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	static grObject		*HackModelObject;
#endif

//========================================================================================
//	grWorld_AddObject
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddObject(grWorld *World, grObject *Object)
{
	grBoolean bAdded{};
	assert(World);
	assert(Object);
	assert(!grChain_FindLink(World->Objects, Object));

	bAdded = GR_FALSE;

#ifdef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	if (HackModelObject)
	{
		// Attach world to object
		if( !grObject_AttachWorld( Object, World ) )
			goto AO_ERROR;

		// Attach engine
		if( World->Engine != nullptr )
		{
			if( !grObject_AttachEngine( Object, World->Engine ) )
				goto AO_ERROR;
		}

		// Attach sound system
		if( World->SoundSystem != nullptr )
			grObject_AttachSoundSystem( Object, World->SoundSystem );

		return grObject_AddChild(HackModelObject, Object);
	}
#endif
	// Attach world to object
	if( !grObject_AttachWorld( Object, World ) )
	{
		goto AO_ERROR;
	}

	// Attach engine
	if( World->Engine != nullptr )
	{
		if( !grObject_AttachEngine( Object, World->Engine ) )
			goto AO_ERROR;
	}

	// Attach sound system
	if( World->SoundSystem != nullptr )
	{
		grObject_AttachSoundSystem( Object, World->SoundSystem );
	}
	
	// KROUER - Try to put actor into the BSP instead of the World
	{
		uint32 flags = grObject_GetFlags(Object);
		grObject_Type objectType = grObject_GetType(Object);
		if (GR_OBJECT_TYPE_MODEL == objectType && World->Model == nullptr)
		{
			World->Model = Object;
			grObject_CreateRef(Object);
		}
		if ((flags&GR_OBJECT_VISRENDER) && World->Model)
		{
			grObject_AddChild(World->Model, Object);
		}
	}

	// add object to the objects list
	if (!grChain_AddLinkData(World->Objects, Object))
	{
		return GR_FALSE;
	}
	
	// Ref the object
	grObject_CreateRef( Object );		
	
#ifdef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	HackModelObject = Object;
#endif

	return GR_TRUE;

AO_ERROR:
	grChain_RemoveLinkData( World->Objects, Object );
	return( GR_FALSE );
}

//========================================================================================
//	grWorld_HasObject
//========================================================================================
GRAPI grBoolean GRCC grWorld_HasObject(const grWorld *World, const grObject *Object)
{
	assert(World);
	assert(Object);

	if (grChain_FindLink(World->Objects, (void*)Object))
		return GR_TRUE;

	return GR_FALSE;
}

//========================================================================================
//	grWorld_RemoveObject
//========================================================================================
GRAPI grBoolean GRCC grWorld_RemoveObject(grWorld *World, grObject *Object)
{
	assert(World);
	assert(Object);

#ifdef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	if (Object != HackModelObject)
		return grObject_RemoveChild(HackModelObject, Object);
#endif

	if (World->Model != nullptr)
		grObject_RemoveChild(World->Model, Object);

	assert(grChain_FindLink(World->Objects, Object));

	if (!grChain_RemoveLinkData(World->Objects, Object))
		return GR_FALSE;

	if( World->Engine != nullptr )
		grObject_DettachEngine(Object, World->Engine);

	grObject_DettachWorld(Object, World);

	grObject_Destroy(&Object);

#ifdef FIRST_OBJECT_IN_HIERARCHY_IS_MODEL_HACK
	HackModelObject = nullptr;
#endif

	return GR_TRUE;
}

//========================================================================================
//	grWorld_GetNextObject
//========================================================================================
GRAPI grObject * GRCC grWorld_GetNextObject(grWorld *World, grObject *Start)
{
	assert(World);

	return (grObject *)grChain_GetNextLinkData(World->Objects, Start);
}

//========================================================================================
//	grWorld_FindObjectByDefName
//========================================================================================
GRAPI grObject * GRCC grWorld_FindObjectByDefName(grWorld *World, const char *DefName)
{
	grChain_Link		*Link{};

	assert(World);

	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject* Object{};

		Object = (grObject*)grChain_LinkGetLinkData(Link);

		if (!strcmp(Object->Methods->Name, DefName))
			return Object;		// Fount it
	}

	return nullptr;		// Not found
}

//========================================================================================
//	grWorld_Frame
//========================================================================================
GRAPI grBoolean GRCC grWorld_Frame(grWorld *World, float TimeDelta)
{
	grChain_Link		*Link{};

	assert(World);
	assert(World->Objects);


	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject* Object{};

		Object = (grObject*)grChain_LinkGetLinkData(Link);

		if (!grObject_Frame( Object, TimeDelta ))
			return GR_FALSE;
	}

	// paradoxnj - Useless and incomplete
	//get shader objects and run a shader frame for each (CyRiuS)
	/*for (Link = grChain_GetFirstLink(World->ShaderChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grShader	*Shader;

		Shader = (grShader*)grChain_LinkGetLinkData(Link);

		if (!grShader_Frame( Shader, World, TimeDelta ))
			return GR_FALSE;
	}

	//retrieve each Actor Scriptlet and send it out to be processed
	for (Link = grChain_GetFirstLink(World->ActorScriptChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grScript	*ActorScript;

		ActorScript = (grScript*)grChain_LinkGetLinkData(Link);

		if(!grScript_Frame( ActorScript, World, TimeDelta ))
			return GR_FALSE;
	}*/

	return( GR_TRUE );
}

//========================================================================================
//	grWorld_Collision
//	Returns GR_TRUE if there was a collision, GR_FALSE otherwise
//========================================================================================
GRAPI grBoolean GRCC grWorld_Collision(	const grWorld *World, 
										const grExtBox *Box, 
										const grVec3d *Front, 
										const grVec3d *Back, 
										grCollisionInfo *CollisionInfo)
{
	grChain_Link	*Link{};
	grFloat			BestDist{};
	grBoolean		Hit;
	
	Hit = GR_FALSE;
	BestDist = 999999.0f;

	if (CollisionInfo)	// Invalidate the collision info structure	
		memset(CollisionInfo, 0, sizeof(*CollisionInfo));

	// Call each objects collision function
	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject		* Object{}, * SubObject{};
		grVec3d			Impact{};
		grPlane			Plane{};
		const char		*ObjectType{};
	
		Object = (grObject*)grChain_LinkGetLinkData(Link);
		ObjectType = grObject_GetTypeName(Object);
		if(grWorld_CanCollide(World,ObjectType))
		{
			// Icestorm: Do we want to know further deatils?
			if (CollisionInfo)
			{
				if (grObject_Collision(Object, Box, Front, Back, &Impact, &Plane, &SubObject))
				{
					grFloat		Dist{};

					Dist = grVec3d_DistanceBetween(Front, &Impact);

					// Record the closest collision point
					if (Dist < BestDist)
					{
						BestDist = Dist; // Added by Incarnadine
						CollisionInfo->Impact = Impact;
						CollisionInfo->Plane = Plane;
						CollisionInfo->Object = SubObject;
						CollisionInfo->IsValid = GR_TRUE;
						Hit = GR_TRUE;
					}
				}
			} else
				if (grObject_Collision(Object, Box, Front, Back, nullptr, nullptr, &SubObject))
					return GR_TRUE;
		}
	}
	
	return Hit;
}

// Added by Icestorm
//========================================================================================
//	grWorld_ChangeBoxCollision
//	Returns GR_TRUE if there was a collision, while Box changes, GR_FALSE otherwise
//========================================================================================
GRAPI grBoolean GRCC grWorld_ChangeBoxCollision(	const grWorld *World, 
												const grVec3d *Pos, 
												const grExtBox *FrontBox, 
												const grExtBox *BackBox, 
												grChangeBoxCollisionInfo *CollisionInfo)
{
	grChain_Link	* Link{};
	grFloat			BestDist{};
	grBoolean		Hit{};
	
	Hit = GR_FALSE;
	BestDist = 999999.0f;

	// Invalidate the collision info structure
	if (CollisionInfo)	
		memset(CollisionInfo, 0, sizeof(*CollisionInfo));

	// Call each objects collision function
	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject* Object{}, * SubObject{};
		grExtBox		ImpactBox{};
		grPlane			Plane{};
		const char* ObjectType{};
	
		Object = (grObject*)grChain_LinkGetLinkData(Link);
		ObjectType = grObject_GetTypeName(Object);
		if(grWorld_CanCollide(World,ObjectType))
		{
			// Icestorm: Do we want to know further deatils?
			if (CollisionInfo)
			{
				if (grObject_ChangeBoxCollision(Object, Pos, FrontBox, BackBox, &ImpactBox, &Plane, &SubObject))
				{
					grFloat		Dist{};

					Dist = grVec3d_DistanceBetween(&FrontBox->Min, &ImpactBox.Min);
				
					// Record the closest collision point
					if (Dist < BestDist)
					{
						BestDist = Dist;
						CollisionInfo->ImpactBox = ImpactBox;
						CollisionInfo->Plane = Plane;
						CollisionInfo->Object = SubObject;
						Hit = GR_TRUE;
					}
				}
			} else
				if (grObject_ChangeBoxCollision(Object, Pos, FrontBox, BackBox, nullptr, nullptr, &SubObject))
					return GR_TRUE;
		}
	}
	
	return Hit;
}

//#include "grPolyMgr.h"
//extern grPolyMgr		*HackPolyMgr;

//========================================================================================
//	grWorld_RenderALL
//	Takes a camera space frustum
//========================================================================================
grBoolean grWorld_RenderALL(grWorld *World, grCamera *Camera, grFrustum *CameraSpaceFrustum)
{
	grChain_Link* Link{};
	grFrustum				Frustum{}, WorldSpaceFrustum{};
	grObject_RenderFlags	RenderFlags{};

//	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
//		OutputDebugString("BEGIN grWorld_RenderALL\n");
//	#endif

	RenderFlags = 0;

	if (!CameraSpaceFrustum)
	{
		// Prepare the default frustum
		grFrustum_SetFromCamera(&Frustum, Camera);

		RenderFlags |= GR_OBJECT_RENDER_FLAG_CAMERA_FRUSTUM;	// Frustum was created from camera

		CameraSpaceFrustum = &Frustum;
	}

	// Render objects
	for ( Link = grChain_GetFirstLink( World->Objects ); Link; Link = grChain_LinkGetNext( Link ) )
	{
		grObject* Object{};

		Object = (grObject*)grChain_LinkGetLinkData(Link);

		if (Object->Methods->Type == GR_OBJECT_TYPE_PORTAL)
			continue;	// Don't render portals like normal objects

//		if (Object->Methods->Type == GR_OBJECT_TYPE_ACTOR)
//			continue;

		if (!grObject_Render(Object, World, World->Engine, Camera, CameraSpaceFrustum, RenderFlags))
			return GR_FALSE;
	}

/*
	// Render actors
	for ( Link = grChain_GetFirstLink( World->Actors ); Link; Link = grChain_LinkGetNext( Link ) )
	{
		grActor		*Actor;

		Actor = (grActor*)grChain_LinkGetLinkData(Link);

		//if (!grActor_RenderThroughFrustum(Actor, World->Engine, World, Camera, CameraSpaceFrustum))
		//	return GR_FALSE;
		if (!grActor_Render(Actor, World->Engine, World, Camera))
			return GR_FALSE;
	}
*/

	// User polys need frustum in world space
	grFrustum_TransformToWorldSpace(CameraSpaceFrustum, Camera, &WorldSpaceFrustum);

	// Render user polys
	if (!grWorld_RenderUserPolys(World, Camera, &WorldSpaceFrustum))
		return GR_FALSE;

	// paradoxnj - Useless and incomplete
	// Update world based on shader variables (cyrius)
	// AKA Render the shaders
	/*for ( Link = grChain_GetFirstLink( World->ShaderChain ); Link; Link = grChain_LinkGetNext( Link ) )
	{
		grShader	*Shader;

		Shader = (grShader*)grChain_LinkGetLinkData(Link);

		// "render" the shader
		// this is different from grShader_Frame. the Frame call uses the "rendered" info
		// that is retrieved/generated here

		if (!grShader_Render(Shader, World, World->Engine, Camera, CameraSpaceFrustum))
			return GR_FALSE;
		
	}

	//render ActorScripts
	for ( Link = grChain_GetFirstLink( World->ActorScriptChain ); Link; Link = grChain_LinkGetNext( Link ) )
	{
		grScript	*ActorScript;

		ActorScript = (grScript*)grChain_LinkGetLinkData(Link);

		// render the ActorScript
		// This is where the changes to the actor and camera are applied
		// The info used here is update in grActorScript_Frame()

		if (!grScript_Render(ActorScript, World, World->Engine, Camera, CameraSpaceFrustum))
			return GR_FALSE;
	}*/

//	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
//		OutputDebugString("END grWorld_RenderALL\n");
//	#endif

	return GR_TRUE;
}

//========================================================================================
//	** LOCAL Static functions **
//========================================================================================

//========================================================================================
//	grWorld_RenderUserPolys
//	Frustum is assumed to be in WorldSpace already!!!
//========================================================================================
static grBoolean grWorld_RenderUserPolys(const grWorld *World, const grCamera *Camera, const grFrustum *WorldSpaceFrustum)
{
	grChain_Link	* Link{};
	//grChain_Link		*Link2;
	grUserPoly		* Poly{};

	//assert(World);
	//assert(Camera);
	//assert(WorldSpaceFrustum);

//	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
//		OutputDebugString("BEGIN grWorld_RenderUserPolys\n");
//	#endif

	// Render normal user polys first..or maybe not. Let's do it a bit faster shall we
	// Observe the legendary chinese super special tactic: DOUBLE RENDER PUUUNCH!...
	// yea riiiight....
	for (Link = grChain_GetFirstLink(World->UserPolys); Link; Link = grChain_LinkGetNext(Link))
	{
		Poly = (grUserPoly*)grChain_LinkGetLinkData(Link);
		//assert(Poly);

		if (Poly && !grUserPoly_Render(Poly, World->Engine, Camera, WorldSpaceFrustum))
			return GR_FALSE;
	}

	// Render normal user polys first
	for (Link = grChain_GetFirstLink(World->AutoRemoveUserPolys); Link; Link = grChain_LinkGetNext(Link))
	{
		Poly = (grUserPoly*)grChain_LinkGetLinkData(Link);
		//assert(Poly);

		if (Poly && !grUserPoly_Render(Poly, World->Engine, Camera, WorldSpaceFrustum))
			return GR_FALSE;
	}

//	#if (WORLD_DEBUG_OUTPUT_LEVEL >= 2)
//		OutputDebugString("END grWorld_RenderUserPolys\n");
//	#endif

	return GR_TRUE;
}

//========================================================================================
//	grWorld_DestroyAutoRemoveUserPolys
//========================================================================================
static grBoolean grWorld_DestroyAutoRemoveUserPolys(grWorld *World)
{
	grChain_Link	* Link{};
	grUserPoly		* Poly{};

	assert(World);

	// Keep getting links till there are no more
	while (Link = grChain_GetFirstLink(World->AutoRemoveUserPolys))
	{
		Poly = (grUserPoly*)grChain_LinkGetLinkData(Link);
		assert(Poly);

		if (!grChain_RemoveLink(World->AutoRemoveUserPolys, Link))
			return GR_FALSE;
		
		// Destroy the link
		grChain_LinkDestroy(&Link);

		// De-ref the poly
		grUserPoly_Destroy(&Poly);
	}

	assert(grChain_GetLinkCount(World->AutoRemoveUserPolys) == 0);

	return GR_TRUE;
}

//========================================================================================
//	ReadPtrMgrVerification
//========================================================================================
static grBoolean ReadPtrMgrVerification(grVFile *VFile, const grPtrMgr *PtrMgr)
{
	int32		PtrCount1, PtrRefs1;
	int32		PtrCount2, PtrRefs2;

	grPtrMgr_GetPtrCount(PtrMgr, &PtrCount1);
	grPtrMgr_GetPtrRefs(PtrMgr, &PtrRefs1);

	if (!grVFile_Read(VFile, &PtrCount2, sizeof(PtrCount2)))
		return GR_FALSE;

	if (PtrCount1 != PtrCount2)
		return GR_FALSE;

	if (!grVFile_Read(VFile, &PtrRefs2, sizeof(PtrRefs2)))
		return GR_FALSE;

	if (PtrRefs1 != PtrRefs2)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	WritePtrMgrVerification
//========================================================================================
static grBoolean WritePtrMgrVerification(grVFile *VFile, const grPtrMgr *PtrMgr)
{
	int32		PtrCount, PtrRefs;

	grPtrMgr_GetPtrCount(PtrMgr, &PtrCount);
	grPtrMgr_GetPtrRefs(PtrMgr, &PtrRefs);

	if (!grVFile_Write(VFile, &PtrCount, sizeof(PtrCount)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &PtrRefs, sizeof(PtrRefs)))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_CreateArrays
//========================================================================================
static grBoolean grWorld_CreateArrays(grWorld *World)
{
	assert(World);

	// Create the faceinfo array
	World->FaceInfoArray = grFaceInfo_ArrayCreate(GR_WORLD_START_FACEINFO);

	if (!World->FaceInfoArray)
	{
		grErrorLog_AddString(-1, "grWorld_CreateArrays:  grFaceInfo_ArrayCreate failed.", nullptr);
		goto ExitWithError;
	}

	// Create the material array
	World->MaterialArray = grMaterial_ArrayCreate(GR_WORLD_START_MATERIALS);

	if (!World->MaterialArray)
	{
		grErrorLog_AddString(-1, "grWorld_CreateArrays:  grMaterial_ArrayCreate failed.", nullptr);
		goto ExitWithError;
	}

	return GR_TRUE;

	ExitWithError:
	{
		if (World->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&World->FaceInfoArray);
		if (World->MaterialArray)
			grMaterial_ArrayDestroy(&World->MaterialArray);

		return GR_FALSE;
	}
}

#define FACEINFO_READWRITE_PORTALCAMERA		(1<<0)

//========================================================================================
//	ReadFaceInfo
//========================================================================================
static grBoolean ReadFaceInfo(grVFile *VFile, grGArray_Element *Element, void *Context)
{
	grFaceInfo		* pFaceInfo{};
	uint8			ReadWriteFlags;

	pFaceInfo = (grFaceInfo*)Element;

	//if (!grFaceInfo_Read(pFaceInfo, VFile))
	//	return GR_FALSE;

	if (!grVFile_Read(VFile, &ReadWriteFlags, sizeof(ReadWriteFlags)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->Flags, sizeof(pFaceInfo->Flags)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->Alpha, sizeof(pFaceInfo->Alpha)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->Rotate, sizeof(pFaceInfo->Rotate)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->ShiftU, sizeof(pFaceInfo->ShiftU)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->ShiftV, sizeof(pFaceInfo->ShiftV)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->DrawScaleU, sizeof(pFaceInfo->DrawScaleU)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->DrawScaleV, sizeof(pFaceInfo->DrawScaleV)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->LMapScaleU, sizeof(pFaceInfo->LMapScaleU)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->LMapScaleV, sizeof(pFaceInfo->LMapScaleV)))
		return GR_FALSE;

	if (!grVFile_Read(VFile, &pFaceInfo->MaterialIndex, sizeof(pFaceInfo->MaterialIndex)))
		return GR_FALSE;

	if (ReadWriteFlags & FACEINFO_READWRITE_PORTALCAMERA)
	{
		pFaceInfo->PortalCamera = grObject_CreateFromFile(VFile, (grPtrMgr *)Context);
		if (!pFaceInfo->PortalCamera)
			return GR_FALSE;
	}
	else
		pFaceInfo->PortalCamera = nullptr;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_ReadArrays
//========================================================================================
static grBoolean grWorld_ReadArrays(grWorld *World, grVFile *VFile, grPtrMgr* PtrMgr)
{
	assert(World);

	// Create the faceinfo array
	World->FaceInfoArray = grFaceInfo_ArrayCreateFromFile(VFile, ReadFaceInfo, PtrMgr, PtrMgr);

	if (!World->FaceInfoArray)
	{
		grErrorLog_AddString(-1, "grWorld_CreateArrays:  grFaceInfo_ArrayCreateFromFile failed.", nullptr);
		goto ExitWithError;
	}

	// Create the material array
	World->MaterialArray = grMaterial_ArrayCreateFromFile(VFile, PtrMgr);

	if (!World->MaterialArray)
	{
		grErrorLog_AddString(-1, "grWorld_CreateArrays:  grMaterial_ArrayCreateFromFile failed.", nullptr);
		goto ExitWithError;
	}

	return GR_TRUE;

	ExitWithError:
	{
		if (World->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&World->FaceInfoArray);
		if (World->MaterialArray)
			grMaterial_ArrayDestroy(&World->MaterialArray);

		return GR_FALSE;
	}
}



#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
		((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define GR_WORLD_TAG				MAKEFOURCC('G', 'E', 'W', 'F')		// 'GE' 'W'orld 'F'ile
#define GR_WORLD_VERSION			0x0000

//========================================================================================
//	grWorld_WriteHeader
//========================================================================================
static grBoolean grWorld_WriteHeader(const grWorld *World, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(World);
	assert(VFile);

	// Write TAG
	Tag = GR_WORLD_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_WORLD_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grWorld_ReadHeader
//========================================================================================
static grBoolean grWorld_ReadHeader(grWorld *World, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(World);
	assert(VFile);

	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	if (Tag != GR_WORLD_TAG)
		return GR_FALSE;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	if (Version != GR_WORLD_VERSION)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	WriteFaceInfo
//========================================================================================
static grBoolean WriteFaceInfo(grVFile *VFile, grGArray_Element *Element, void *Context)
{
	grFaceInfo		* pFaceInfo{};
	uint8			ReadWriteFlags;

	pFaceInfo = (grFaceInfo*)Element;

	ReadWriteFlags = 0;

	if (pFaceInfo->PortalCamera)
		ReadWriteFlags |= FACEINFO_READWRITE_PORTALCAMERA;

	if (!grVFile_Write(VFile, &ReadWriteFlags, sizeof(ReadWriteFlags)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->Flags, sizeof(pFaceInfo->Flags)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->Alpha, sizeof(pFaceInfo->Alpha)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->Rotate, sizeof(pFaceInfo->Rotate)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->ShiftU, sizeof(pFaceInfo->ShiftU)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->ShiftV, sizeof(pFaceInfo->ShiftV)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->DrawScaleU, sizeof(pFaceInfo->DrawScaleU)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->DrawScaleV, sizeof(pFaceInfo->DrawScaleV)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->LMapScaleU, sizeof(pFaceInfo->LMapScaleU)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->LMapScaleV, sizeof(pFaceInfo->LMapScaleV)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &pFaceInfo->MaterialIndex, sizeof(pFaceInfo->MaterialIndex)))
		return GR_FALSE;

	if (pFaceInfo->PortalCamera)
	{
		if (!grObject_WriteToFile(pFaceInfo->PortalCamera, VFile, (grPtrMgr *)Context))
			return GR_FALSE;
	}

	return GR_TRUE;
}


//========================================================================================
//	grWorld_WriteArrays
//========================================================================================
static grBoolean grWorld_WriteArrays(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr)
{
	assert(World);
	assert(VFile);

	if (!grFaceInfo_ArrayWriteToFile(World->FaceInfoArray, VFile, WriteFaceInfo, PtrMgr, PtrMgr))
		return GR_FALSE;

	if (!grMaterial_ArrayWriteToFile(World->MaterialArray, VFile, PtrMgr))
		return GR_FALSE;

	return GR_TRUE;
}


//========================================================================================
//	ReadLight
//========================================================================================
static grBoolean ReadLight(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	*LinkData = grLight_CreateFromFile(VFile, PtrMgr);
		
	if (!(*LinkData))
		return GR_FALSE;

	/*if (!grLight_CreateRef(*LinkData))		// Ref it [MLB-ICE] : Object was already refd. in CreateFromFile
		return GR_FALSE;*/

	return GR_TRUE;
}


//========================================================================================
//	WriteLight
//========================================================================================
static grBoolean WriteLight(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	if (!grLight_WriteToFile((grLight *)*LinkData, VFile, PtrMgr))
		return GR_FALSE;

	return GR_TRUE;
}


//========================================================================================
//	ReadObject
//========================================================================================
static grBoolean ReadObject(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	grObject	* Object{};
	grWorld		* World{};

	World = (grWorld*)Context;
	assert(World);

	Object = grObject_CreateFromFile(VFile, PtrMgr);
		
	if (!Object)
		return GR_FALSE;

	
	//grObject_CreateRef(Object);		// Ref it  [MLB-ICE] : Object was already refd. in CreateFromFile

	// Attach world to object
	if( !grObject_AttachWorld(Object, World) )
		goto RO_ERROR;

	// Attach engine
	if(World->Engine)
	{
		if( !grObject_AttachEngine(Object, World->Engine) )
		{
			grObject_DettachWorld( Object, World );
			goto RO_ERROR;
		}
	}

	// Attach sound system
	if(World->SoundSystem)
		grObject_AttachSoundSystem(Object, World->SoundSystem);

	// KROUER - Try to put actor into the BSP instead of the World
	{
		uint32 flags = grObject_GetFlags(Object);
		grObject_Type objectType = grObject_GetType(Object);
		if (GR_OBJECT_TYPE_MODEL == objectType && World->Model == nullptr)
		{
			World->Model = Object;
			grObject_CreateRef(Object);
		}
		if ((flags&GR_OBJECT_VISRENDER) && World->Model)
		{
			grObject_AddChild(World->Model, Object);
		}
	}

	*LinkData = Object;

	return GR_TRUE;
RO_ERROR:
	grObject_Destroy( &Object );
	return GR_FALSE;
}



//========================================================================================
//	WriteObject
//========================================================================================
static grBoolean WriteObject(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	if (!grObject_WriteToFile((grObject *)*LinkData, VFile, PtrMgr))
		return GR_FALSE;

	return GR_TRUE;
}


//========================================================================================
//	grWorld_AttachSoundSystem
//========================================================================================
GRAPI grBoolean GRCC grWorld_AttachSoundSystem(grWorld *World, grSound_System * pSoundSystem )
{
	grChain_Link	* Link{};

	assert( World );

	World->SoundSystem = pSoundSystem;
	for (Link = grChain_GetFirstLink(World->Objects); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject	* Object{};

		Object = (grObject*)grChain_LinkGetLinkData(Link);

		
		if (!grObject_AttachSoundSystem( Object, pSoundSystem ))
		{
			grErrorLog_AddString(-1, "grWorld_AttachSoundSystem:  grObject_AttachSoundSystem failed.", nullptr);
			return GR_FALSE;
		}
	}
	return( GR_TRUE );
}

//========================================================================================
//	grWorld_GetSoundSystem
//========================================================================================
GRAPI grSound_System *	GRCC grWorld_GetSoundSystem(grWorld *World )
{
	return( World->SoundSystem );
}

//========================================================================================
//	grWorld_GetResourceMgr
//========================================================================================
GRAPI grResourceMgr * GRCC grWorld_GetResourceMgr(grWorld *World)
{
	assert( World != nullptr );
	assert( World->ResourceMgr != nullptr );

	grResource_MgrIncRefcount( World->ResourceMgr );
	return( World->ResourceMgr );
}

//========================================================================================
//	TEST PORTAL OBJECT CODE
//========================================================================================

enum
{
	PORTAL_SKYBOX_CHECK_ID = PROPERTY_LOCAL_DATATYPE_START,
	PORTAL_SPEED_ID,
	PORTAL_RADIOX_ID,
	PORTAL_RADIOY_ID,
	PORTAL_RADIOZ_ID,
};

typedef struct
{
	grPortal		*Portal;

	grBoolean		SkyBox;
	grFloat			RotateSpeed;
	grFloat			Rotation;
	int32			RAxis;

	int32			RefCount;
}  grWorld_Portal;

static void * GRCC CreatePortalObjectInstance(void)
{
	grWorld_Portal		* WPortal{};
	grPortal			* Portal{};

	WPortal = GR_RAM_ALLOCATE_STRUCT(grWorld_Portal);

	if (!WPortal)
		return nullptr;

	ZeroMem(WPortal);

	WPortal->RefCount = 1;

	Portal = grPortal_Create();

	if (!Portal)
	{
		grRam_Free(WPortal);
		return nullptr;
	}

	Portal->Recursion = 0;

	WPortal->Portal = Portal;
	WPortal->RAxis = 1;

	return WPortal;
}

static void GRCC RefPortalObjectInstance(grWorld_Portal *WPortal)
{
	assert(WPortal);

	assert(WPortal->RefCount > 0);

	WPortal->RefCount++;
}

static grBoolean GRCC DestroyPortalObjectInstance(void **WPortal)
{
	grWorld_Portal	* WPortal2{};

	WPortal2 = *(grWorld_Portal **)WPortal;

	WPortal2->RefCount--;

	if (WPortal2->RefCount > 0)
		return GR_FALSE;

	grPortal_Destroy(&WPortal2->Portal);

	grRam_Free(WPortal2);
	*WPortal = nullptr;

	return GR_TRUE;
}

static grBoolean GRCC RenderPortalObjectInstance(const grWorld_Portal *WPortal, const grWorld *World, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	return GR_TRUE;
}

grBoolean GRCC GetPortalPropertyList(grWorld_Portal *WPortal, grProperty_List **List)
{
	grProperty_List		* PropertyList{};
	grProperty			* Property{};
	grPortal			* Portal{};

	Portal = WPortal->Portal;

#if 1
	PropertyList = grProperty_ListCreate(1);

	Property = &PropertyList->pgrProperty[0];
	grProperty_FillCheck(Property, "SkyBox", WPortal->SkyBox, PORTAL_SKYBOX_CHECK_ID);
#else
	PropertyList = grProperty_ListCreate(5);

	Property = &PropertyList->pgrProperty[0];
	grProperty_FillCheck(Property, "SkyBox", WPortal->SkyBox, PORTAL_SKYBOX_CHECK_ID);

	Property = &PropertyList->pgrProperty[1];
	grProperty_FillFloat(Property, "Speed", WPortal->RotateSpeed, PORTAL_SPEED_ID, 0.0f, 100.0f, 1.0f);

	Property = &PropertyList->pgrProperty[2];
	grProperty_FillRadio(Property, "X Axis", (WPortal->RAxis == 0), PORTAL_RADIOX_ID);

	Property = &PropertyList->pgrProperty[3];
	grProperty_FillRadio(Property, "Y Axis", (WPortal->RAxis == 1), PORTAL_RADIOY_ID);

	Property = &PropertyList->pgrProperty[4];
	grProperty_FillRadio(Property, "Z Axis", (WPortal->RAxis == 2), PORTAL_RADIOZ_ID);
#endif

	*List = PropertyList;

	return GR_TRUE;
}

grBoolean GRCC SetPortalProperty(grWorld_Portal *WPortal, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	switch (FieldID)
	{
		case PORTAL_SKYBOX_CHECK_ID:
		{
			assert(DataType == PROPERTY_CHECK_TYPE);
			WPortal->SkyBox = *(int32*)pData;
			break;
		}

		case PORTAL_SPEED_ID:
		{
			assert(DataType == PROPERTY_FLOAT_TYPE);
			WPortal->RotateSpeed = *(float*)pData;
			break;
		}

		case PORTAL_RADIOX_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			WPortal->RAxis = 0;
			break;
		}

		case PORTAL_RADIOY_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			WPortal->RAxis = 1;
			break;
		}

		case PORTAL_RADIOZ_ID:
		{
			assert(DataType == PROPERTY_RADIO_TYPE);
			WPortal->RAxis = 2;
			break;
		}
	}

	return GR_TRUE;
}

grBoolean GRCC SetPortalXForm(grWorld_Portal *WPortal, const grXForm3d *XForm)
{
	grPortal	* Portal{};

	Portal = WPortal->Portal;

	Portal->XForm = *XForm;

	Portal->XForm.Flags = XFORM3D_NONORTHOGONALISOK;
	grXForm3d_Orthonormalize(&Portal->XForm);

	return GR_TRUE;
}

grBoolean GRCC GetPortalXForm(const grWorld_Portal *WPortal, grXForm3d *XForm)
{
	grPortal	* Portal{};

	Portal = WPortal->Portal;

	*XForm = Portal->XForm;

	return GR_TRUE;
}

grBoolean GRCC GetPortalExtBox	(const grWorld_Portal *WPortal, grExtBox *BBox)
{
	grExtBox	Box{};
	grVec3d		* Pos{};
	grPortal	* Portal{};

	Portal = WPortal->Portal;

	Pos = &((grPortal*)Portal)->XForm.Translation;

	grVec3d_Set(&Box.Min, Pos->X-10.0f, Pos->Y-10.0f, Pos->Z-10.0f);
	grVec3d_Set(&Box.Max, Pos->X+10.0f, Pos->Y+10.0f, Pos->Z+10.0f);

	*BBox = Box;

	return GR_TRUE;
}


void * GRCC ReadPortalObjectInstance(grVFile *VFile, grPtrMgr *PtrMgr)
{
	grWorld_Portal	* WPortal{};

	WPortal = (grWorld_Portal *)CreatePortalObjectInstance();

	if (!WPortal)
		return nullptr;

	if (!grVFile_Read(VFile, &WPortal->Portal->XForm, sizeof(WPortal->Portal->XForm)))
	{
		DestroyPortalObjectInstance((void **)&WPortal);
		return nullptr;
	}

	if (!grVFile_Read(VFile, &WPortal->SkyBox, sizeof(WPortal->SkyBox)))
	{
		DestroyPortalObjectInstance((void **)&WPortal);
		return nullptr;
	}

	return WPortal;
}


grBoolean GRCC WritePortalObjectInstance(const void *Instance, grVFile *VFile, grPtrMgr *PtrMgr)
{
	grWorld_Portal	* WPortal{};

	WPortal = (grWorld_Portal*)Instance;

	if (!grVFile_Write(VFile, &WPortal->Portal->XForm, sizeof(WPortal->Portal->XForm)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &WPortal->SkyBox, sizeof(WPortal->SkyBox)))
		return GR_FALSE;

	return GR_TRUE;
}


int	GRCC GetPortalXFormModFlags(const grWorld_Portal *WPortal)
{
	return GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE;
}

static grBoolean RenderPortalObjectInstance2(grWorld_Portal *WPortal, grPortal *Portal, const grPlane *Plane, const grXForm3d *FaceXForm, grWorld *World, grCamera *Camera, grFrustum *Frustum)
{
	grXForm3d		XForm{}, InvXForm{}, NewXForm{};
	grBoolean		Ret{};
	grFloat			ZScale{};

	if (Portal->Recursion > 0)
		return GR_TRUE;

	// Get Camera XForm
	grCamera_GetXForm(Camera, &XForm);

	if (WPortal->SkyBox)
	{
		// Skymode
		NewXForm = XForm;

		grVec3d_Clear(&NewXForm.Translation);
		grXForm3d_Multiply(&Portal->XForm, &NewXForm, &NewXForm);

		#if 0													  
		if (WPortal->Rotation)
		{
			switch (WPortal->RAxis)
			{
				case 0:
					grXForm3d_RotateX(&NewXForm, WPortal->Rotation);
					break;

				case 1:
					grXForm3d_RotateY(&NewXForm, WPortal->Rotation);
					break;

				case 2:
					grXForm3d_RotateZ(&NewXForm, WPortal->Rotation);
					break;
			}
		}
		#endif

		NewXForm.Translation = Portal->XForm.Translation;

		ZScale = grCamera_GetZScale(Camera);
		grCamera_SetZScale(Camera, ZScale*20.0f);
	}
	else
	{
		grPlane		FrontPlane{};
		grXForm3d	WorldToCameraXForm{};

		//	XForm the Camera to the Dest Portal location
		// This is the equation we need to XForm the camera from the FaceXForm to the PortalXForm
		//	P' = PortalXForm*InvFaceXForm*CameraXForm
		//	What this does, is take the point, and XForm against the camera like normal, then XForm by the amount
		//	it would take to get the FaceXForm to line up with the origin, then XForm into the Portal XForm...
		grXForm3d_GetTranspose(FaceXForm, &InvXForm);

		grXForm3d_Multiply(&Portal->XForm, &InvXForm, &NewXForm);
		grXForm3d_Multiply(&NewXForm, &XForm, &NewXForm);
		
		// Get the WorldToCameraXForm
		grCamera_GetTransposeXForm(Camera, &WorldToCameraXForm);

		// Transform the FacePlane to camera space
		grPlane_Transform(Plane, &WorldToCameraXForm, &FrontPlane);

		// Add the Plane to the Frustum
		if (!grFrustum_AddPlane(Frustum, &FrontPlane, GR_TRUE))
			return GR_FALSE;
	}

	// Put the new XForm into the camera
	grCamera_SetXForm((grCamera*)Camera, &NewXForm);

	Portal->Recursion++;

	// Render the scene from this camera
	Ret = grWorld_Render(World, Camera, Frustum);

	Portal->Recursion--;

	// Restore Camera XForm
	grCamera_SetXForm((grCamera*)Camera, &XForm);

	if (WPortal->SkyBox)
		grCamera_SetZScale(Camera, ZScale);

	return Ret;
}

static grBoolean GRCC SendPortalMessage(grWorld_Portal *WPortal, int32 Msg, void *Data)
{
	switch (Msg)
	{
		case 0:
		{
			PortalMsgData	* MData{};
			
			MData = (PortalMsgData*)Data;

			return RenderPortalObjectInstance2(	WPortal, 
												WPortal->Portal, 
												MData->Plane, 
												MData->FaceXForm, 
												MData->World, 
												MData->Camera, 
												MData->Frustum);
		}

		default:
			return GR_FALSE;
	}

	return GR_TRUE;
}

grBoolean GRCC PortalFrame(grWorld_Portal *WPortal, grFloat Time)
{
	WPortal->Rotation += WPortal->RotateSpeed*0.01f;

	return GR_TRUE;
}

// Icestorm: This seems already done in Portals...
/*grObjectDef PortalObjectDef = 
{
	GR_OBJECT_TYPE_PORTAL,
	"Portal",
	0,

	CreatePortalObjectInstance,
	RefPortalObjectInstance,
	DestroyPortalObjectInstance,

	nullptr,
	nullptr,

	nullptr,
	nullptr,

	nullptr,
	nullptr,

	RenderPortalObjectInstance,

	nullptr,

	GetPortalExtBox,

	ReadPortalObjectInstance,
	WritePortalObjectInstance,

	GetPortalPropertyList,
	SetPortalProperty,
	nullptr,

	SetPortalXForm,			// SetXForm
	GetPortalXForm,			// Get XForm

	GetPortalXFormModFlags,
	
	nullptr,
	nullptr,
	nullptr,

	nullptr,
	SendPortalMessage,
	PortalFrame,
	nullptr
};*/

// Added by Incarnadine
//  -- Does this need to have additional logic to handle child objects?
GRAPI grBoolean GRCC grWorld_RebuildBSP(grWorld *World, 
										grBSP_Options Options, 
										grBSP_Logic Logic, 
										grBSP_LogicBalance LogicBalance)
{
	grBoolean bResult = GR_TRUE;
	grBSPSetup BSPSetup{};
	grObject *pCurrentObj = grWorld_GetNextObject(World, nullptr);
		
	// Find all models
	while(pCurrentObj)
	{
		if(strcmp(grObject_GetTypeName(pCurrentObj),"Model")==0)
		{
			// Send a message to the model to tell it to rebuild the BSP
			BSPSetup.Options = Options;
			BSPSetup.Logic = Logic;
			BSPSetup.LogicBalance = LogicBalance;
			if(!grObject_SendMessage(pCurrentObj, GR_OBJECT_MSG_WORLD_REBUILDBSP, &BSPSetup))
				bResult = GR_FALSE;
		}
		pCurrentObj = grWorld_GetNextObject(World,pCurrentObj);
	}
	return bResult;
}

// Added by Incarnadine
//  -- Does this need to have additional logic to handle child objects?
GRAPI grBoolean	GRCC  grWorld_RebuildLights(grWorld *World)
{
	grBoolean bResult = GR_TRUE;
	grObject *pCurrentObj = grWorld_GetNextObject(World, nullptr);
		
	// Find all models
	while(pCurrentObj)
	{
		if(strcmp(grObject_GetTypeName(pCurrentObj),"Model")==0)
		{
			// Send a message to the model to tell it to rebuild lights
			if(!grObject_SendMessage(pCurrentObj, GR_OBJECT_MSG_WORLD_REBUILDLIGHTS, nullptr))
				bResult = GR_FALSE;
		}
		pCurrentObj = grWorld_GetNextObject(World,pCurrentObj);	
	}
	return bResult;
}

// paradoxnj - Useless and incomplete
//========================================================================================
//	grWorld_AddShader - By CyRiuS
//========================================================================================
/*GRAPI grBoolean GRCC grWorld_AddShader(grWorld *World, grShader *Shader)
{
	assert(World);
	assert(grChain_FindLink(World->ShaderChain, Shader) == nullptr); //make sure shader doesnt exist in chain already
		
	// Add the shader to the world's shader chain
	if (!grChain_AddLinkData(World->ShaderChain, Shader))
		return GR_FALSE;

	//I dont think a ref for the shader is needed... might change though
	//grShader_CreateRef(Shader);		// Ref the shader

	return GR_TRUE;
}

//========================================================================================
//	grWorld_AddScript - By CyRiuS
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddScript(grWorld *World, grScript *Script)
{
	assert(World);
	
	//note: scripts can be added to the chain more than once
			
	// Add the script to the world's chain
	if (!grChain_AddLinkData(World->ActorScriptChain, Script))
		return GR_FALSE;

	return GR_TRUE;
}
*/
// ----------------------------------------------
//  ACTOR SUPPORT FUNCTIONS
// Added by Incarnadine
/*
//========================================================================================
//	grWorld_AddActor - By Incarnadine
//========================================================================================
GRAPI grBoolean GRCC grWorld_AddActor(grWorld *World, grActor *Actor)
{
	assert(World);
	assert(grActor_IsValid(Actor) == GR_TRUE);
	assert(grChain_FindLink(World->Actors, Actor) == nullptr);
	
	// Add the actor to the world's actor chain
	if (!grChain_AddLinkData(World->Actors, Actor))
		return GR_FALSE;

	grActor_CreateRef(Actor);		// Ref the actor

	return GR_TRUE;
}

//========================================================================================
//	grWorld_RemoveActor - By Incarnadine
//========================================================================================
GRAPI grBoolean GRCC grWorld_RemoveActor(grWorld *World, grActor *Actor)
{
	assert(World);
	assert(grActor_IsValid(Actor) == GR_TRUE);
	assert(grChain_FindLink(World->Actors, Actor));
	
	// Remove the actor from the world's actor chain
	if (!grChain_RemoveLinkData(World->Actors, Actor))
		return GR_FALSE;

	grActor_Destroy(&Actor);		// De-ref the actor

	return GR_TRUE;
}

//========================================================================================
//	grWorld_HasActor - By Incarnadine
//========================================================================================
GRAPI grBoolean	GRCC grWorld_HasActor(grWorld *World, grActor *Actor)
{
	assert(World);

	if(grChain_FindLink(World->Actors, Actor))
		return GR_TRUE;

	return GR_FALSE;
}

//========================================================================================
//	grWorld_GetNextActor - By Incarnadine
//========================================================================================
GRAPI grActor * GRCC grWorld_GetNextActor(const grWorld *World, grActor *Start)
{
	assert(World);

	return grChain_GetNextLinkData(World->Actors, Start);
}
*/

//========================================================================================
//	grWorld_CanCollide - By Incarnadine
//========================================================================================
GRAPI grBoolean GRCC grWorld_CanCollide(const grWorld *World, const char* Type)
{
	grChain_Link	* Link{};

	assert(World);
	assert(Type);
	
	for (Link = grChain_GetFirstLink(World->CollisionObjectTypes); Link; Link = grChain_LinkGetNext(Link))
	{
		char	* Data{};
	
		Data = (char*)grChain_LinkGetLinkData(Link);
		if(stricmp(Data,Type) == 0)
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grWorld_EnableCollision - By Incarnadine
//========================================================================================
GRAPI grBoolean	GRCC grWorld_EnableCollision(grWorld *World, const char* Type)
{
	grChain_Link	* Link{};

	assert(World);
	assert(Type);
	
	if(grWorld_CanCollide(World,Type)) return GR_TRUE;

	for (Link = grChain_GetFirstLink(World->CollisionObjectTypes); Link; Link = grChain_LinkGetNext(Link))
	{
		char	* Data{};
	
		Data = (char*)grChain_LinkGetLinkData(Link);
		if(stricmp(Data,Type) == 0)
		{
			grRam_Free(Data);
			return grChain_RemoveLink(World->CollisionObjectTypes, Link);			
		}
	}

	return GR_FALSE;
}

//========================================================================================
//	grWorld_DisableCollision - By Incarnadine
//========================================================================================
GRAPI grBoolean GRCC grWorld_DisableCollision(grWorld *World, const char *Type)
{
	assert(World);
	assert(Type);	
	
	if(!grWorld_CanCollide(World,Type)) return GR_TRUE;

	// Add the actor to the world's actor chain
	if (!grChain_AddLinkData(World->CollisionObjectTypes, Util_StrDup(Type)))
		return GR_FALSE;	

	return GR_TRUE;
}


//========================================================================================
//	grWorld_SetCollisionOptions - By Incarnadine
//========================================================================================
/*GRAPI void GRCC grWorld_SetCollisionOptions(grWorld* World, int32 Level, const char *Include, const char *Exclude)
{
	char *Parse;
	char seps[] = ", ";
	char *token;

	World->CollisionLevel = Level;		

	if(Include != nullptr)
	{		
		grChain_Link *Link;		

		Parse = strdup(Include);
		token = strtok(Parse,seps);
	
		for (Link = grChain_GetFirstLink(World->CollisionObjectTypes); Link; Link = grChain_LinkGetNext(Link))
		{
			char		*Data;		
		
			Data = (char*)grChain_LinkGetLinkData(Link);
			grRam_Free(Data);
			grChain_RemoveLink(World->CollisionObjectTypes, Link);					
		}

		while(token != nullptr)
		{
			grWorld_AddCollisionObjectType(World,token);
			token = strtok(nullptr,seps);
		}

		free(Parse);
	}
	if(Exclude != nullptr)
	{	
		Parse = strdup(Exclude);
		token = strtok(Parse,seps);
		while(token != nullptr)
		{
			grWorld_RemoveCollisionObjectType(World,token);
			token = strtok(nullptr,seps);
		}
		free(Parse);
	}
}
*/
GRAPI int32 GRCC grWorld_GetCollisionLevel(const grWorld* World)
{
	return World->CollisionLevel;
}

GRAPI void GRCC grWorld_SetCollisionLevel(grWorld* World, int32 Level)
{
	World->CollisionLevel = Level;
}

//========================================================================================
//	grWorld_GetNextCollisionExclusion - By Incarnadine
//========================================================================================
GRAPI char * GRCC grWorld_GetNextCollisionExclusion(const grWorld *World, const char *Start)
{
	grChain_Link	* Link{};

	assert(World);

	if(Start == nullptr)
	{
		Link = grChain_GetFirstLink(World->CollisionObjectTypes);
		if(Link == nullptr) return nullptr;
		return((char *)grChain_LinkGetLinkData(Link));		
	}

	for (Link = grChain_GetFirstLink(World->CollisionObjectTypes); Link; Link = grChain_LinkGetNext(Link))
	{
		// locals
		char	* LinkName{};
			
		LinkName = (char *)grChain_LinkGetLinkData( Link );		
		if(LinkName && stricmp(Start,LinkName)==0)
		{
			if(grChain_LinkGetNext(Link) == nullptr) return nullptr;			

			return((char *)grChain_LinkGetLinkData(grChain_LinkGetNext(Link)));
		}
	}
	return nullptr;
}

// =====================
// Incarnadine
// -------------------
// This function simplifies the loading process for a level.  Pass it the filename of a 
// level created in JEdit and it will automatically open the appropriate, file and fork, and 
// then read in the world.  The second two parameters here CAN be nullptr.  If they are nullptr,
// they will be created automatically.
GRAPI grWorld	* GRCC grWorld_CreateFromEditorFile(const char* FileName, grPtrMgr *pPtrMgr, grResourceMgr * pResourceMgr )
{
	grWorld* pWorld{};
	grVFile* pMapFile{};
	grVFile* pWorldFork{};
	grBoolean bCreated = GR_FALSE;
	
	if(pPtrMgr == nullptr)	
		pPtrMgr = grPtrMgr_Create();
	else grPtrMgr_CreateRef(pPtrMgr);

	if(pPtrMgr == nullptr )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "OnSaveDocument:grPtrMgr_Create");		
		return nullptr;
	}
	
	if(pResourceMgr == nullptr) 
	{
		bCreated = GR_TRUE;
		pResourceMgr = grResource_MgrCreateDefault(nullptr);	
	}
	else grResource_MgrIncRefcount(pResourceMgr);

	if(pResourceMgr == nullptr)
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "OnSaveDocument:grResource_MgrCreateDefault");
		grPtrMgr_Destroy( &pPtrMgr );		
		return nullptr;
	}
	
	pMapFile = grVFile_OpenNewSystem
	(
		nullptr, 
		GR_VFILE_TYPE_VIRTUAL,
		FileName,
		nullptr,
		GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY
	);  
	if( pMapFile == nullptr )
	{
		grErrorLog_AddString( GR_ERR_FILEIO_OPEN, "OnOpenDocument:grVFile_OpenNewSystem", FileName);		
		grPtrMgr_Destroy( &pPtrMgr );
		grResource_MgrDestroy(&pResourceMgr);
		return nullptr;
	}

	// Open the Jet3D Fork
	pWorldFork = grVFile_Open( pMapFile, "Jet3D", GR_VFILE_OPEN_READONLY) ;
	if( pWorldFork == nullptr )
	{
		grVFile_Close( pMapFile ) ;
		grErrorLog_AddString( GR_ERR_FILEIO_FORMAT, "OnOpenDocument:grVFile_Open", FileName);		
		grPtrMgr_Destroy( &pPtrMgr );
		grResource_MgrDestroy(&pResourceMgr);
		return nullptr;
	}		

	pWorld = grWorld_CreateFromFile( pWorldFork, pPtrMgr, pResourceMgr );

	grVFile_Close( pWorldFork ) ;
	if( pWorld == nullptr )
	{
		grVFile_Close( pMapFile ) ;
		grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE, "OnOpenDocument:grWorld_CreateFromFile", FileName);		
		grPtrMgr_Destroy( &pPtrMgr );
		grResource_MgrDestroy(&pResourceMgr);
		return nullptr;
	}

	grVFile_Close(pMapFile);			
	grPtrMgr_Destroy( &pPtrMgr );

	// The world took its own reference; drop the one this function holds
	// (either the one it created or the temporary one on the caller's manager).
	grResource_MgrDestroy(&pResourceMgr);

	return pWorld;
}
