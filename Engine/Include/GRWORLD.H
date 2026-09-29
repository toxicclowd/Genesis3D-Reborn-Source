/****************************************************************************************/
/*  grWorld.H                                                                           */
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

#ifndef grWorld_H
#define grWorld_H

#include "GRMODEL.H"  // Added by Incarnadine

#include "grFaceInfo.h"		// grFaceInfo_Array
#include "grMaterial.h"		// grMaterial_Array
#include "BaseType.h"
#include "grPlane.h"
#include "grSoundSystem.h"
#include "VFile.h"
#include "grLight.h"
#include "Camera.h"
#include "grPtrMgr.h"
#include "grUserPoly.h"
#include "Object.h"
#include "Sound.h"
#include "grChain.h"
#include "grResource.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================

typedef struct grWorld grWorld;
typedef struct grActor grActor;
typedef struct grCollisionInfo
{
	grVec3d		Impact;			// Impact location
	grPlane		Plane;			// Impact plane
	grObject	*Object;		// Object that was hit
	grBoolean	IsValid;		// Infos are valid - Icestorm
} grCollisionInfo;

// Added by Icestorm
typedef struct
{
	grExtBox	ImpactBox;		// Impact box
	grPlane		Plane;			// Impact plane
	grObject	*Object;		// Object that was hit
	grBoolean	IsValid;		// Infos are valid - Icestorm
} grChangeBoxCollisionInfo;

// Added by Incarnadine
// Flags for collision functions
#define COLLIDE_EXTBOX 0
#define COLLIDE_BONES 1
#define COLLIDE_MESH 2 // Not currently implemented

// Temp hack structure of all mothers
typedef struct
{
	int32			NumObjects;
	int32			NumRenders;
	int32			NumPortals;
	int32			NumDLights;

	int32			NumTransformedPolys;
	int32			NumRenderedPolys;
	int32			NumActorPolys;
	int32			NumNodes;
	int32			NumLeaves;

	int32			NumVisibleAreas;
} grWorld_DebugInfo;

#define MAX_VISIBLE_DLIGHTS			32			// Max 32 visible dlights in the frustum

//========================================================================================
// Misc functions

GRAPI grWorld	*	GRCC grWorld_Create( grResourceMgr * pResourceMgr );
GRAPI grBoolean	GRCC grWorld_WriteToFile(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr);
GRAPI grWorld	*	GRCC grWorld_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr, grResourceMgr * pResourceMgr );

// =====================
// Incarnadine
// -------------------
// This function simplifies the loading process for a level.  Pass it the filename of a 
// level created in JEdit and it will automatically open the appropriate, file and fork, and 
// then read in the world.  The second two parameters here CAN be NULL.  If they are NULL,
// they will be created automatically.
GRAPI grWorld	*	GRCC grWorld_CreateFromEditorFile(const char* FileName, grPtrMgr *pPtrMgr, grResourceMgr * pResourceMgr );

GRAPI grBoolean	GRCC grWorld_CreateRef(grWorld *World);
GRAPI void			GRCC grWorld_Destroy(grWorld **World);

GRAPI grBoolean GRCC grWorld_RebuildBSP(grWorld *World, 
										grBSP_Options Options, 
										grBSP_Logic Logic, 
										grBSP_LogicBalance LogicBalance);

#ifdef USE_MODELS

//========================================================================================
// Model list manipulation

GRAPI grBoolean	GRCC grWorld_AddModel(grWorld *World, grModel *Model);
GRAPI grBoolean	GRCC grWorld_RemoveModel(grWorld *World, grModel *Model);
GRAPI grBoolean	GRCC grWorld_HasModel(grWorld *World, grModel *Model);
GRAPI grModel	*	GRCC grWorld_GetNextModel(const grWorld *World, const grModel *Start);

#endif

//========================================================================================
// Actor Manipulation Functions - Added by Incarnadine
//
// Actor list manipulation
//    Use these to manage adding/removing actors from the world.  When a world
// knows about actors, they get rendered during a call to grWorld_Render() and they
// get checked for collisions during a call to grWorld_Collision().
//GRAPI grBoolean	grWorld_AddActor(grWorld *World, grActor *Actor);
//GRAPI grBoolean	grWorld_RemoveActor(grWorld *World, grActor *Actor);
//GRAPI grBoolean	grWorld_HasActor(grWorld *World, grActor *Actor);
//GRAPI grActor	*	grWorld_GetNextActor(const grWorld *World, grActor *Start);

//========================================================================================


//========================================================================================
// All the sub-arrays

GRAPI grMaterial_Array		* GRCC grWorld_GetMaterialArray(const grWorld *World);
GRAPI grFaceInfo_Array		* GRCC grWorld_GetFaceInfoArray(const grWorld *World);
GRAPI grChain				* GRCC  grWorld_GetLightChain(const grWorld *World);
GRAPI grChain				* GRCC grWorld_GetDLightChain(const grWorld *World);

//========================================================================================
// SetEngine Then Render

GRAPI grBoolean	GRCC grWorld_SetEngine(grWorld *World, grEngine *Engine);

#ifdef USE_MODELS
GRAPI grBoolean	GRCC grWorld_SetRenderOptions(grWorld *World, grBSP_RenderMode RenderMode);
#endif

GRAPI	int32		GRCC grWorld_GetRenderRecursion( const grWorld *World );
GRAPI grBoolean	GRCC grWorld_Render(grWorld *World, grCamera *Camera, grFrustum *CameraSpaceFrustum);

// Lights
GRAPI grBoolean	GRCC grWorld_AddLight(grWorld *World, grLight *Light, grBoolean Update);
GRAPI grBoolean	GRCC grWorld_RemoveLight(grWorld *World, grLight *Light, grBoolean Update);
GRAPI grBoolean	GRCC grWorld_UpdateLight(grWorld *World, grLight *Light);
GRAPI grLight		* GRCC grWorld_GetNextLight(grWorld *World, grLight *Start);

#ifdef USE_MODELS
GRAPI grBoolean	GRCC grWorld_PatchLighting(grWorld *World);
#endif
GRAPI grBoolean	GRCC grWorld_RebuildLights(grWorld *World); // Moved by Incarnadine


GRAPI grBoolean	GRCC grWorld_AddDLight(grWorld *World, grLight *Light);
GRAPI grBoolean	GRCC grWorld_RemoveDLight(grWorld *World, grLight *Light);
GRAPI grLight		* GRCC grWorld_GetNextDLight(grWorld *World, grLight *Start);

GRAPI grBoolean	GRCC grWorld_AddUserPoly(grWorld *World, grUserPoly *Poly, grBoolean AutoRemove);
GRAPI grBoolean	GRCC grWorld_RemoveUserPoly(grWorld *World, grUserPoly *Poly);

// Objects
GRAPI grBoolean	GRCC grWorld_AddObject(grWorld *World,grObject *Object);
GRAPI grBoolean	GRCC grWorld_HasObject(const grWorld *World,const grObject *Object);
GRAPI grBoolean	GRCC grWorld_RemoveObject(grWorld *World,grObject *Object);
GRAPI grObject	* GRCC grWorld_GetNextObject(grWorld *World, grObject *Start);
GRAPI grObject	* GRCC grWorld_FindObjectByDefName(grWorld *World, const char *DefName);
GRAPI grBoolean	GRCC grWorld_Frame(grWorld *World, float TimeDelta );

// Icestorm: If CollisionInfo is NULL, they only test, whether there is an collision
GRAPI grBoolean	GRCC grWorld_Collision(	const grWorld *World, 
											const grExtBox *Box, 
											const grVec3d *Front, 
											const grVec3d *Back, 
											grCollisionInfo *CollisionInfo);

// Added by Icestorm
GRAPI grBoolean	GRCC grWorld_ChangeBoxCollision(	const grWorld *World, 
												const grVec3d *Pos, 
												const grExtBox *FrontBox, 
												const grExtBox *BackBox, 
												grChangeBoxCollisionInfo *CollisionInfo);

//External Attachments
GRAPI grBoolean	GRCC grWorld_AttachSoundSystem(grWorld *World, grSound_System * pSoundSystem );
GRAPI grSound_System *	GRCC grWorld_GetSoundSystem(grWorld *World );


// resource manager
GRAPI grResourceMgr * GRCC grWorld_GetResourceMgr(grWorld *World);


//========================================================================================
// Collision Manipulation Functions - Added by Incarnadine
//
GRAPI grBoolean	GRCC grWorld_CanCollide(const grWorld *World, const char* Type);
GRAPI grBoolean	GRCC grWorld_EnableCollision(grWorld *World, const char* Type);
GRAPI grBoolean  GRCC grWorld_DisableCollision(grWorld *World, const char *Type);
GRAPI char * GRCC grWorld_GetNextCollisionExclusion(const grWorld *World, const char *Start);
//GRAPI void grWorld_SetCollisionOptions(grWorld* World, int32 Level, const char *Include, const char *Exclude);
//GRAPI void grWorld_ResetCollisionOptions(grWorld* World);
GRAPI int32 GRCC grWorld_GetCollisionLevel(const grWorld* World);
GRAPI void GRCC grWorld_SetCollisionLevel(grWorld* World, int32 Level);
//========================================================================================

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
