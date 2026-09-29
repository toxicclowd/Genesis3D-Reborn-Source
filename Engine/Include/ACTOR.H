/**
   @file ACTOR.H                                                                       
                                                                                      
   @author Mike Sandige	                                                          
   @brief Actor interface      		                                          
                                                                                      
	@par Licence
	The contents of this file are subject to the Jet3D Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.jet3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Jet3D, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved

	@par Documentation
	@ref actors "The Actors page"
*/

/** @page actors The Actors
*	@section infActor Presentation
* 	This object is designed to support character animation.
*	There are two basic objects to deal with.  
*	
* 	@par Actor Definition (grActor_Def)
* 		A #grActor_Def embodies the geometry (polygon, and bone information), 
* 		and a library of motions that can be applied to that geometry.
*
* 	@par Actor
*		A #grActor is an instance of an actor definition.  The definition is used for 
*		the geometry, but all additional settings, such as the bone pose, lighting information,
*		and cuing information is unique for a #grActor.<br>
*	An Actor Definition is created either from an existing Actor Definition file, or from scratch by 
*	first creating a #grBody and #grMotions and selecting these into an Actor.  If the Actor Definition
*	is constructed from scratch, the objects selected into it (via SetBody and AddMotion) are
*	then 'owned' by the actor and will be destroyed along with the Actor when it is destroyed.
*    Of course, when the Actor is loaded from a file, the Body and Motion it creates as it is
*	loaded are cleaned up when the Actor is destroyed.<br><br>
*	Once an Actor is created, prepare it for rendering and animating by calling 
*	Actor_RenderPrep().  This must be called (and it must succeed) before any render or
*	pose setting functions can be called.
*
*	@section useActor There are two ways to use an Actor.
*	@par Direct Control
*		One method is to directly control the skeleton configuration.  Use _SetPose() to set its 
*		skeleton using a grMotion animation.  The pose is positioned in world space relative to the 
*		transform given in SetPose().  Whenever a new skeleton pose is required, call _SetPose() 
*		to reposition the skeleton for a new point in time.<br>
*		More complex positioning can be achieved by blending more than one animation.  Use
*		_BlendPose() after a _SetPose() to blend the second grMotion into the first.  Additional
*		blends can be applied by additional _BlendPose() calls.  Each blend is performed on the
*		the existing skeleton (the results of any previous blends).
*	@par Cuing
*		Another method is to 'cue' up motions that are applied with parameterized blending over time.
*		A cued motion takes effect 'now' in time.  The Actor advances in time and repositions itself
*		according to its currently cued motions with a call to _AnimationStep().  AnimationStep() 
*		redefines what the actor thinks 'now' is.  This causes historical cues to be forgotten, and 
*		motions that are no longer valid are cleaned up.  AnimationTestStep() can be used to position 
*		the actor for potential queries with its currently cued motions at some arbitrary future time -
*		relative to the last AnimationTestStep() call.  AnimationNudge() applies a given transform 
*		'instantly' to the current actor's cue list.  This is usefull for moving the actor as a 
*		result of a collision with another object.
*	@par
*	If a motion contains joint information that does not exactly match the Actor's skeleton 
*	joints, only the joints that match by name are applied.  So a grMotion can be applied to
*	a portion of the Actor, or a grMotion that has more joint information than the skeleton can
*	be applied and the extra joint information is ignored.  
*	@par
*	Examples of this:  If the Actor is a biped and has no tail, but the motion is for a 
*	biped with a tail, the grMotion can be applied, but the tail information will be ignored.
*	Also if there is a grMotion for only a left arm, it can be applied and it will only affect
*	the left arm of the Actor, and consequently its left hand and fingers, but no other 
*	bones that are not children of the affected bones will be changed.
*
*	@note
*	*call setscale and setshadow after preparing the actor for rendering (renderprep)
*	 	
*/

#ifndef GR_ACTOR_H
#define GR_ACTOR_H

#include "BaseType.h"
#include "ExtBox.h"
#include "Bitmap.h"

#include "Motion.h"
#include "Camera.h"
#include "grFrustum.h"

#include "Body.h"

#include "grWorld.h"
#include "Engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GR_ACTOR_ENUMS
#define GR_ACTOR_ENUMS
	
/////////////////////////////////////////////////////////
/// @enum grActor_BlendingType
/// @brief Type of blending to perform on animations
/////////////////////////////////////////////////////////
typedef enum grActor_BlendingType 
{
	/// @brief Treats the blending amount as a linear value
	GR_ACTOR_BLEND_LINEAR,

	/// @brief Applies a parametric smoothing curve to the blending amount
	///
	///  Applies a parametric smoothing curve to the blending amount
	///  so that a linear change in BlendAmount parameters will
	///  result in a smooth (non-linear) change in blending.
	GR_ACTOR_BLEND_HERMITE 
} grActor_BlendingType;

#endif

////////////////////////////////////////////////////////////
/// @typedef grActor
/// @brief An instance of an actor
////////////////////////////////////////////////////////////
typedef struct grActor grActor;

////////////////////////////////////////////////////////////
/// @typedef grActor_Def
/// @brief The definition of an actor's geometry and bone structure
////////////////////////////////////////////////////////////
typedef struct grActor_Def grActor_Def;

///////////////////////////////////////////////////////////
/// @fn float grActor_GetAlpha(const grActor *A)
/// @brief Gets the actor's alpha value for transparency
/// @param[in] A The actor to get the alpha value on
/// @return The alpha value of the actor
//////////////////////////////////////////////////////////
GRAPI float GRCC grActor_GetAlpha(const grActor *A);

//////////////////////////////////////////////////////////
/// @fn void grActor_SetAlpha(grActor *A, float Alpha)
/// @brief Sets the actor's alpha value for transparency
/// @param[in] A The actor to set the alpha value on
/// @param[in] Alpha The alpha value
//////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetAlpha(grActor *A, float Alpha);

//---------------------------------------------------------------------------------
//   Creation/Destruction functions
//---------------------------------------------------------------------------------

//////////////////////////////////////////////////////////
/// @fn grActor_Def *grActor_DefCreate(void)
/// @brief Create an empty Actor Definition
/// @return A newly created actor definition
/////////////////////////////////////////////////////////
GRAPI grActor_Def* GRCC grActor_DefCreate(void);

/////////////////////////////////////////////////////////
/// @fn grActor_Def *grActor_DefCreateFromFile(grVFile *pFile)
/// @brief Create an Actor Definition from a file
/// @param[in] pFile The file to create the actor from
/// @return A newly created actor definition
/////////////////////////////////////////////////////////
GRAPI grActor_Def* GRCC grActor_DefCreateFromFile(grVFile *pFile);

/////////////////////////////////////////////////////////
/// @fn void grActor_DefCreateRef(grActor_Def *pActorDefinition)
/// @brief Create an additional reference (owner) for the Actor_Definition
/// @param[in] pActorDefinition The actor definition to reference
/////////////////////////////////////////////////////////
GRAPI void GRCC grActor_DefCreateRef(grActor_Def *pActorDefinition);

/////////////////////////////////////////////////////////
/// @fn grBoolean grActor_DefDestroy(grActor_Def **pActorDefinition)
/// @brief Destroy a grActor_Def (its grBody and its grMotions)  Actors that rely on this definition become invalid can fail if there are actors still referencing this definition.
/// @param[in] pActorDefinition The actor definition to destroy
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_DefDestroy(grActor_Def **pActorDefinition);

////////////////////////////////////////////////////////
/// @fn void grActor_CreateRef(grActor *Actor)
/// @brief Create an additional reference (owner) for the Actor
/// @param[in] Actor The actor to reference
///////////////////////////////////////////////////////
GRAPI void GRCC grActor_CreateRef(grActor *Actor);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetBody(grActor_Def *ActorDefinition, grBody *grBodyGeometry)
/// @brief Give the Actor Definition a Body.  grActor becomes responsible for its destruction and sets up default materials as referenced by the Body
/// @param[in] ActorDefinition The actor def to set the geometry for
/// @param[in] grBodyGeometry The geometry for the actor definition
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetBody( grActor_Def *ActorDefinition, grBody *grBodyGeometry);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_AddMotion(grActor_Def *ActorDefinition, grMotion *M, int32 *Index)
/// @brief Adds a grMotion to the Actor Definition's library.  The ActorDefinition becomes responsible for its destruction.
/// returns the library index to the new grMotion
/// @param[in] ActorDefinition The actor definition to add the motion to
/// @param[in] M The motion to add
/// @param[out] Index The index of the new motion in the actor definition
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AddMotion(grActor_Def *ActorDefinition, grMotion *M, int32 *Index);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_Destroy(grActor **pA)
/// @brief Destroy an Actor
/// @param[in] pA The actor to destroy
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note A return value of GR_FALSE would result in a memory leak
///////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_Destroy(grActor **pA);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_DefIsValid(const grActor_Def *A)
/// @brief Checks if the given actor definition is valid
/// @param[in] A The actor definition to validate
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_DefIsValid(const grActor_Def *A);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_IsValid(const grActor *A)
/// @brief Checks to see if an actor is valid
/// @param[in] A The actor to validate
/// @return GR_TRUE on success, GR_FALSE on failure
//////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_IsValid(const grActor *A);

//---------------------------------------------------------------------------------
//   Queries 
//---------------------------------------------------------------------------------

	// In general: Objects retuned from Get functions should not not be destroyed. 
	// if ownership is desired, call the objects _CreateRef() function to create another owner. 
	// (An 'owner' has access to the object regardless of the number of other owners, and 
	// an owner must call the object's _Destroy() function to relinquish ownership )

////////////////////////////////////////////////////////
/// @fn grActor_Def *grActor_GetActorDef(const grActor *A)
/// @brief Returns the Actor Definition associated with Actor A
/// @param[in] A The actor to get the definition from
/// @return The definition of the given actor
////////////////////////////////////////////////////////
GRAPI grActor_Def* GRCC grActor_GetActorDef(const grActor *A);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_DefWriteToFile(const grActor_Def *A, grVFile *pFile)
/// @brief Writes an existing grActor to a file image
/// @param[in] A The actor definition to write
/// @param[in] pFile The file to write to
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_DefWriteToFile(const grActor_Def *A, grVFile *pFile);

////////////////////////////////////////////////////////
/// @fn grBody *grActor_GetBody(const grActor_Def *ActorDefinition)
/// @brief Gets the body geometry from an actor definition
/// @param[in] ActorDefinition The actor definition to retrieve the body from
/// @return The geometry from the actor, NULL if actor has no polys
////////////////////////////////////////////////////////
GRAPI grBody* GRCC grActor_GetBody(const grActor_Def *ActorDefinition);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_DefHasBoneNamed(const grActor_Def *Ad, const char *Name)
/// @brief Checks if an actor has a bone named with the given name
/// @param[in] Ad The actor definition to query
/// @param[in] Name The name of the bone to query for
/// @return GR_TRUE if the actor definition as the named bone, GR_FALSE if not
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_DefHasBoneNamed(const grActor_Def *Ad, const char *Name );

////////////////////////////////////////////////////////
/// @fn void grActor_SetBlendingType(grActor *A, grActor_BlendingType BlendingType)
/// @brief Selects a blending type.  
/// @param[in] A The actor to set the blending type for
/// @param[in] BlendingType The blending type
/// @note BlendingType only affects the meaning of the BlendAmount parameter for the blend functions.  Can be changed anytime.
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetBlendingType( grActor *A, grActor_BlendingType BlendingType );

////////////////////////////////////////////////////////
/// @fn int32 grActor_GetMotionCount(const grActor_Def *ActorDefinition)
/// @brief Gets the number of grMotions in the grActors grMotion library.
/// @param[in] ActorDefinition The actor definition to query
/// @return The amount of motions in the actor's motion library
////////////////////////////////////////////////////////
GRAPI int32 GRCC grActor_GetMotionCount(const grActor_Def *ActorDefinition);

////////////////////////////////////////////////////////
/// @fn grMotion *grActor_GetMotionByIndex(const grActor_Def *ActorDefinition, int32 Index)
/// @brief Gets a grMotion pointer from the grActors grMotion library referenced by index
/// @param[in] ActorDefinition The actor definition to query
/// @param[in] Index The index to return
/// @return The motion at the given index, NULL if it doesn't exist.
/// @note  This is an aliased pointer - Not a copy.  Changes to this motion will be reflected
///   in the actor.  Destroying this return motion will confuse the actor.
/// Index must be in range [0..grActor_GetMotionCount-1]
////////////////////////////////////////////////////////
GRAPI grMotion* GRCC grActor_GetMotionByIndex(const grActor_Def *ActorDefinition, int32 Index );

////////////////////////////////////////////////////////
/// @fn grMotion *grActor_GetMotionByName(const grActor_Def *ActorDefinition, const char *Name)
/// @brief Gets a grMotion pointer from the grActors grMotion library referenced by name
/// @param[in] ActorDefinition The actor definition to query
/// @param[in] Name The name of the motion to get
/// @return The motion with the given name, NULL if it doesn't exist
/// @note  This is an aliased pointer - Not a copy.  Changes to this motion will be reflected
///   in the actor.  Destroying this return motion will confuse the actor.
/// if there is no motion that matches the given name, the return value will be NULL
////////////////////////////////////////////////////////
GRAPI grMotion* GRCC grActor_GetMotionByName(const grActor_Def *ActorDefinition, const char *Name );

////////////////////////////////////////////////////////
/// @fn const char *grActor_GetMotionName(const grActor_Def *ActorDefinition, int32 Index)
/// @brief Gets a motion name given an ActorDef and a motion index.
/// @param[in] ActorDefinition The actor definition to query
/// @param[in] Index The inded of the motion to query
/// @return The name of the motion at the given index, NULL if it doesn't exist
////////////////////////////////////////////////////////
GRAPI const char* GRCC grActor_GetMotionName(const grActor_Def *ActorDefinition, int32 Index );

////////////////////////////////////////////////////////
/// @fn int32 grActor_GetMaterialCount(const grActor *A)
/// @brief Gets the number of materials for an instance of an actor.
/// @param[in] A The actor to query
/// @return The amount of materials for the actor given
////////////////////////////////////////////////////////
GRAPI int32 GRCC grActor_GetMaterialCount(const grActor *A);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetMaterial(const grActor *Actor, int32 MaterialIndex, grMaterialSpec **Bitmap, grFloat *Red, grFloat *Green, grFloat *Blue, grUVMapper *pMapper)
/// @brief Gets the current material for an instance of an actor
/// @param[in] Actor The actor to query
/// @param[in] MaterialIndex The index of the material
/// @param[out] Bitmap The texture to the material
/// @param[out] Red The red color value
/// @param[out] Green The green color value
/// @param[out] Blue The blue color value
/// @param[out] pMapper The UV Mapper
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetMaterial(const grActor *Actor, int32 MaterialIndex, grMaterialSpec **Bitmap, grFloat *Red, grFloat *Green, grFloat *Blue, grUVMapper* pMapper);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetMaterial(grActor *Actor, int32 MaterialIndex, grMaterialSpec *Bitmap, grFloat Red, grFloat Green, grFloat Blue, grUVMapper Mapper)
/// @brief Allows a material to be overriden in an actor instance
/// @param[in] Actor The actor to override the material for
/// @param[in] MaterialIndex The index of the material to override
/// @param[in] Bitmap The texture
/// @param[in] Red The red color value
/// @param[in] Green The green color value
/// @param[in] Blue The blue color value
/// @param[in] Mapper The UV mapper
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetMaterial(grActor *Actor, int32 MaterialIndex, grMaterialSpec *Bitmap,  grFloat Red,  grFloat Green,  grFloat Blue, grUVMapper Mapper);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetBoneTransform(const grActor *A, const char *BoneName, grXForm3d *Transform)
/// @brief Gets the current transform for a single bone in A.
/// @param[in] A The actor to get the bone transform from
/// @param[in] BoneName The name of the bone
/// @param[out] Transform The transform of the given bone
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note (actor space->world space transform) with a NULL BoneName, this returns the current 'root' transform
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetBoneTransform(const grActor *A, const char *BoneName, grXForm3d *Transform);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetBoneExtBox(const grActor *A, const char *BoneName, grExtBox *ExtBox)
/// @brief Gets the extent box (axial-aligned bounding box) for a given bone (for the current pose)
/// @param[in] A The actor to query
/// @param[in] BoneName The name of the bone to query
/// @param[out] ExtBox The bone's axial aligned bounding box
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note if BoneName is NULL, gets the a general bounding box from the body of the actor if it has been set.
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetBoneExtBox(const grActor *A,
										 const char *BoneName,grExtBox *ExtBox);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetBoneBoundingBox(const grActor *A, const char *BoneName, grVec3d *Corner, grVec3d *DX, grVec3d *DY, grVec3d *DZ)
/// @brief Gets the non-axial-aligned bounding box for a given bone (for the current pose)
/// @param[in] A The actor to query
/// @param[in] BoneName The name of the bone to query
/// @param[out] Corner The corner of the box
/// @param[out] DX The X axis orientation
/// @param[out] DY The Y axis orientation
/// @param[out] DZ The Z axis orientation
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note The box is specified by a corner, and
///  a non-normalized orientation transform.  Add DX,DY,DZ components 
///  of the orientation to get other corners of the box
/// @note if BoneName is NULL, gets the a general bounding box from the body of the actor if it has been set.
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetBoneBoundingBox(const grActor *A, const char *BoneName, grVec3d *Corner, grVec3d *DX, grVec3d *DY, grVec3d *DZ);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetDynamicExtBox(const grActor *A, grExtBox *ExtBox)
/// @brief Gets the current axial-aligned bounding box for an actor's bone configuration takes all bones into account
/// @param[in] A The actor to query
/// @param[out] ExtBox The actor's dynamic axial-aligned bounding box
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Takes all bones into account
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetDynamicExtBox( const grActor *A, grExtBox *ExtBox);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetExtBox(const grActor *A, grExtBox *ExtBox)
/// @brief Gets an assigned general non changing bounding box from the actor
/// @param[in] A The actor to query
/// @param[out] ExtBox The actor's non changing bounding box
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetExtBox(const grActor *A, grExtBox *ExtBox);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetExtBox(grActor *A, const grExtBox *ExtBox, const char *CenterBoxOnThisNamedBone)
/// @brief Sets an assigned general non changing bounding box from the actor
/// @param[in] A The actor to set the bounding box of
/// @param[in] ExtBox The new bounding box
/// @param[in] CenterBoxOnThisNamedBone Name of the bone to center the box on
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note NULL uses root position of actor
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetExtBox(grActor *A, const grExtBox *ExtBox,
					const char *CenterBoxOnThisNamedBone);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetRenderHintExtBox(const grActor *A, grExtBox *Box, grBoolean *Enabled)
/// @brief Gets the rendering hint bounding box from the actor
/// @param[in] A The actor to query
/// @param[out] Box The render hint bounding box
/// @param[out] Enabled Whether or not the render hint box is enabled
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note if the RenderHintExtBox is disabled, Enabled is GR_FALSE, and the box returned has zero dimensions, 
///   centered at the root position of the actor.  If the RenderHintExtBox is enabled, Enabled is
///   GR_TRUE, and the box returned is the one set with _SetRenderHintExtBox, offset by the 
///   bone position of the bone named in _SetRenderHintExtBox().
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetRenderHintExtBox(const grActor *A, grExtBox *Box, grBoolean *Enabled);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetRenderHintExtBox(grActor *A, const grExtBox *Box, const char *CenterBoxOnThisNamedBone)
/// @brief Sets a rendering hint bounding box from the actor.  
/// @param[in] A The actor to set the render hint box for
/// @param[in] Box The new render hint box
/// @param[in] CenterBoxOnThisNamedBone Centers the box on the bone with the given name
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Increases performance by 
///   enabling the rendering of the actor to occur only if the box is visible.
///   If the box is not visible, a detailed analysis of the actor's current geometry is avoided.
///   This does allow errors to occur: 
///   If the actor has a bit of geometry that extends outside this box for some
///   animation, that extended geometry may not be drawn, if the box if off-screen.   
///   If the render hint box is not set, the engine will make no conservative assumptions 
///   about the visibility of an actor - it will always be drawn if any portion of it is
///   visible.
///   To attach the box to the 'root' bone, pass NULL for CenterBoxOnThisNamedBone
///   For disabling the hint box: (disabled is default) pass Box with zero mins and maxs
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetRenderHintExtBox(grActor *A, const grExtBox *Box,
												const char *CenterBoxOnThisNamedBone );

////////////////////////////////////////////////////////
/// @fn void *grActor_GetUserData(const grActor *A)
/// @brief Returns the data set by a user
/// @param[in] A The actor to get the user data from
/// @return The user data, NULL if not set
////////////////////////////////////////////////////////
GRAPI void* GRCC grActor_GetUserData(const grActor *A);

////////////////////////////////////////////////////////
/// @fn void grActor_SetUserData(grActor *A, void *UserData)
/// @brief Sets the actors user data pointer to the given value.  For clients only.
/// @param[in] A The actor to store the user data in
/// @param[in] UserData The user data to store
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetUserData(grActor *A, void *UserData);


//--------------------------------------------------------------------------------
//   Posing and Rendering
//--------------------------------------------------------------------------------

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_AttachEngine(grActor *A, grEngine *Engine)
/// @brief Prepares the grActor for rendering and posing.  
/// @param[in] A The actor to attach the engine to
/// @param[in] Engine The Engine to attach
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Call Once once the actor is fully created. Must be called prior to render/pose/setworldtransform 
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AttachEngine( grActor *A, grEngine *pEngine);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_DetachEngine(grActor *A, grEngine *pEngine)
/// @brief Detaches the engine from an actor
/// @param[in] A The actor to detach the engine from
/// @param[in] pEngine The engine to detach
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_DetachEngine(grActor *A, grEngine *pEngine);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_RenderThroughFrustum(const grActor *A, grEngine *Engine, grWorld *World, grCamera *Camera, const grFrustum *Frustum)
/// @brief Draws the grActor  
/// @param[in] A The actor to render
/// @param[in] Engine The engine to render with
/// @param[in] World The world the actor is in
/// @param[in] Camera The camera to render through
/// @param[in] Frustum The frustum to cull with
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note RenderPrep must be called first
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_RenderThroughFrustum(
		const grActor	*A, 
		grEngine		*Engine, 
		grWorld			*World, 
		grCamera		*Camera, 
		const grFrustum *Frustum);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_Render(const grActor *A, grEngine *Engine, grWorld *World, const grCamera *Camera)
/// @brief Renders an actor without a frustum
/// @param[in] A The actor to render
/// @param[in] Engine The engine to render with
/// @param[in] World The world the actor is in
/// @param[in] Camera The camera to render through
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_Render(
		const grActor	*A, 
		grEngine		*Engine, 
		grWorld			*World, 
		const grCamera	*Camera);

////////////////////////////////////////////////////////
/// @fn void grActor_ForceStaticRelighting(grActor *pActor)
/// @brief Forces an actor to be relit with static lighting
/// @param[in] pActor The actor to relight
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_ForceStaticRelighting(grActor* pActor);

////////////////////////////////////////////////////////
/// @fn void grActor_ClearPose(grActor *A, const grXForm3d *Transform)
/// @brief Poses the actor in its default pose
/// @param[in] A The actor to pose
/// @param[in] Transform Where to position the root for this pose
/// @note If Transform is NULL, the root for the pose is assumed to be the root of the actor
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_ClearPose(grActor *A, const grXForm3d *Transform);

////////////////////////////////////////////////////////
/// @fn void grActor_SetPose(grActor *A, const grMotion *Motion, grFloat Time, const grXForm3d *Transform)
/// @brief Poses the actor using given motion M at a time offset of Time
/// @param[in] A The actor to pose
/// @param[in] Motion The motion to pose it in
/// @param[in] Time The keyframe to use
/// @param[in] Transform Where to position the root for this pose.
/// @note If Transform is NULL, the root for the pose is assumed to be the root of the actor
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetPose(grActor *A, const grMotion *Motion, grFloat Time, const grXForm3d *Transform);

////////////////////////////////////////////////////////
/// @fn void grActor_BlendPose(grActor *A, const grMotion *Motion, grFloat Time, const grXForm3d *Transform, grFloat BlendAmount)
/// @brief Blends the current pose of the grActor with a new pose using motion M at a time offset of Time
/// @param[in] A The actor to pose
/// @param[in] Motion The motion to pose it in
/// @param[in] Time The keyframe to use
/// @param[in] Transform Where to position the root for the pose
/// @param[in] BlendAmount The percentage of the mixed pose to use
/// @note A BlendAmount of 0 will result in the existing pose, A BlendAmount of 1 will
/// result in the new pose from M.  The BlendingType set by _SetBlendingType() determines
/// the blending function between 0 and 1
/// If Transform is NULL, the root for the pose is assumed to be the root of the actor.
////////////////////////////////////////////////////////
GRAPI void GRCC grActor_BlendPose(grActor *A, const grMotion *Motion, grFloat Time,
						const grXForm3d *Transform, grFloat BlendAmount);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetBoneAttachment(const grActor *A, const char *BoneName, grXForm3d *Transform)
/// @brief Gets the requested bone's attachment matrix
/// @param[in] A The actor to query
/// @param[in] BoneName The name of the bone to query
/// @param[out] Transform The attachement matrix
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetBoneAttachment(const grActor *A, const char *BoneName, grXForm3d *Transform);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetBoneAttachment(grActor *A, const char *BoneName, grXForm3d *Transform)
/// @brief Sets the attachment matrix for a bone
/// @param[in] A The actor to set the matrix to
/// @param[in] BoneName The name of the bone to set it on
/// @param[in] Transform The attachment matrix
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetBoneAttachment(grActor *A, const char *BoneName, grXForm3d *Transform);

////////////////////////////////////////////////////////
/// @fn grBoolean grActor_Attach(grActor *Slave, const char *SlaveBoneName, const grActor *Master, const char *MasterBoneName, const grXForm3d *Attachment)
/// @brief Attaches one actor to another
/// @param[in] Slave The actor that is being attached to
/// @param[in] SlaveBoneName The bone to attach the actor to
/// @param[in] Master The actor to attach
/// @param[in] MasterBoneName The bone to attach to the other actor
/// @param[in] Attachment The attachment matrix
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_Attach( grActor *Slave,  const char *SlaveBoneName,
						const grActor *Master, const char *MasterBoneName, 
						const grXForm3d *Attachment);

///////////////////////////////////////////////////////
/// @fn grActor_Detach(grActor *Slave)
/// @brief Detaches all actors from the given actor
/// @param[in] Slave The actor to detach all actors from
///////////////////////////////////////////////////////
GRAPI void GRCC grActor_Detach(grActor *Slave);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetLightingOptions(grActor *A, grBoolean UseFillLight, const grVec3d *FillLightNormal, grFloat FillLightRed, grFloat FillLightGreen, grFloat FillLightBlue, grFloat AmbientLightRed, grFloat AmbientLightGreen, grFloat AmbientLightBlue, grBoolean AmbientLightFromFloor, int32 MaximumDynamicLightsToUse, int32 MaximumStaticLightsToUse, const char *LightReferenceBoneName, grBoolean PerBoneLighting);
/// @brief Lights up and actor
/// @param[in] A The actor to light
/// @param[in] UseFillLight Flag for whether or not to use fill lights
/// @param[in] FillLightNormal Normalized vector for fill light
/// @param[in] FillLightRed The red color component for the fill light
/// @param[in] FillLightGreen The green color component for the fill light
/// @param[in] FillLightBlue The blue color component for the fill light
/// @param[in] AmbientLightRed The red color component for the ambient light
/// @param[in] AmbientLightGreen The green color component for the ambient light
/// @param[in] AmbientLightBlue The blue color component for the ambient light
/// @param[in] AmbientLightFromFloor Whether or not to use the ambient light from below the actor
/// @param[in] MaximumDynamicLightsToUse The maximum amount of dynamic lights to use
/// @param[in] MaximumStaticLightsToUse The maximum amount of static lights to use
/// @param[in] LightReferenceBoneName The bone that emits the light
/// @param[in] PerBoneLighting Turn on/Turn off per bone lighting
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetLightingOptions(grActor *A,
									grBoolean UseFillLight,				// GR_TRUE or GR_FALSE
									const grVec3d *FillLightNormal,		// normalized vector
									grFloat FillLightRed,				// 0 .. 255
									grFloat FillLightGreen,				// 0 .. 255
									grFloat FillLightBlue,				// 0 .. 255
									grFloat AmbientLightRed,			// 0 .. 255
									grFloat AmbientLightGreen,			// 0 .. 255
									grFloat AmbientLightBlue,			// 0 .. 255
									grBoolean AmbientLightFromFloor,	// GR_TRUE or GR_FALSE
									int32 MaximumDynamicLightsToUse,		// 0 for none
									int32 MaximumStaticLightsToUse, // 0 for none
									const char *LightReferenceBoneName,	//NULL for root
									grBoolean PerBoneLighting);			
									// if GR_TRUE, then dynamic lighting attenuation and direction is computed
									// for each bone.  if GR_FALSE, then the computations are relative to the 
									// single bone named by the LightReferenceBoneName


////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetLightingOptions(const grActor *A, grBoolean *UseFillLight, grVec3d *FillLightNormal, grFloat *FillLightRed, grFloat *FillLightGreen, grFloat *FillLightBlue, grFloat *AmbientLightRed, grFloat *AmbientLightGreen, grFloat *AmbientLightBlue, grBoolean *UseAmbientLightFromFloor, int32 *MaximumDynamicLightsToUse, int32 *MaximumStaticLightsToUse, const char **LightReferenceBoneName, grBoolean *PerBoneLighting)
/// @brief Gets the light information from an actor
/// @param[in] A The actor to query
/// @param[out] UseFillLight Flags for whether or not to use fill lights
/// @param[out] FillLightNormal Normalized vector for fill light
/// @param[out] FillLightRed The red color component of the fill light
/// @param[out] FillLightGreen The green color component of the fill light
/// @param[out] FillLightBlue The blue color component of the fill light
/// @param[out] AmbientLightRed The red color component of the ambient light
/// @param[out] AmbientLightGreen The green color component of the ambient light
/// @param[out] AmbientLightBlue The blue color component of the ambient light
/// @param[out] UseAmbientLightFromFloor Whether or not to use the ambient light from below the actor
/// @param[out] MaximumDynamicLightsToUse The maximum amount of dynamic lights to use
/// @param[out] MaximumStaticLightsToUse The maximum amount of dynamic lights to use
/// @param[out] LightReferenceBoneName The bone emitting the light
/// @param[out] PerBoneLighting Turn on/Turn off per bone lighting
////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetLightingOptions(const grActor *A,
									grBoolean *UseFillLight,			// GR_TRUE or GR_FALSE
									grVec3d *FillLightNormal,			// normalized vector
									grFloat *FillLightRed,				// 0 .. 255
									grFloat *FillLightGreen,			// 0 .. 255
									grFloat *FillLightBlue,				// 0 .. 255
									grFloat *AmbientLightRed,			// 0 .. 255
									grFloat *AmbientLightGreen,			// 0 .. 255
									grFloat *AmbientLightBlue,			// 0 .. 255
									grBoolean *UseAmbientLightFromFloor,// GR_TRUE or GR_FALSE
									int32 *MaximumDynamicLightsToUse,
									int32 *MaximumStaticLightsToUse,		
									const char **LightReferenceBoneName,
									grBoolean *PerBoneLighting);		// NULL for root


////////////////////////////////////////////////////////
/// @fn void grActor_SetScale(grActor *A, grFloat ScaleX, grFloat ScaleY, grFloat ScaleZ)
/// @brief Sets the actor's scale value
/// @param[in] A The actor to scale
/// @param[in] ScaleX The X axis scale value
/// @param[in] ScaleY The Y axis scale value
/// @param[in] ScaleZ The Z axis scale value
///////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetScale(grActor *A, grFloat ScaleX,grFloat ScaleY,grFloat ScaleZ);

///////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetShadow(grActor *A, grBoolean DoShadow, grFloat Radius, const grMaterialSpec *ShadowMap, const char *BoneName)
/// @brief Sets an actor's shadow map
/// @param[in] A The actor to set the shadow map on
/// @param[in] DoShadow Turn on/Turn off shadow
/// @param[in] Radius The radius of the shadow
/// @param[in] ShadowMap The shadow map to set
/// @param[in] BoneName The bone to center the shadow from
/// @return GR_TRUE on success, GR_FALSE on failure
//////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetShadow(grActor *A, grBoolean DoShadow, grFloat Radius, const grMaterialSpec *ShadowMap, const char * BoneName);

/// @note Animation Cuing API:
/// high level Actor animation:  The principle is that motions can be applied to an actor
/// and the actor will keep track of which motions are currently appropriate.  Call 
///	_AnimationStep() to compute a new pose for an elapsed time interval.  The new pose
///  will take into account all motions that are 'currently' cued up to be set or blended.


//////////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationCue(grActor *A, grMotion *Motion, grFloat TimeScaleFactor, grFloat TimeIntoMotion, grFloat BlendTime, grFloat BlendFromAmount, grFloat BlendToAmount, const grXForm3d *MotionTransform)
/// @brief Cues up a new motion
/// @param[in] A The actor to cue the motion for
/// @param[in] Motion The motion to cue up
/// @param[in] TimeScaleFactor Time scale to apply to cued motion
/// @param[in] TimeIntoMotion Time offset to begin motion with
/// @param[in] BlendTime Time to apply the blend
/// @param[in] BlendFromAmount Blend value at current time
/// @param[in] BlendToAmount Blend value after BlendTime has elapsed
/// @param[in] MotionTransform Local transform to adjust motion by
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note  The motion begins at the current time.  The motion can be 
/// blended in or out over time and time scaled.  If the return value is GR_FALSE, the 
/// animation was not cued up (failure implies Actor is incompletely initialized).
/////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationCue( 
		grActor *A,						// actor to apply animation to
		grMotion *Motion,				// motion to Cue
		grFloat TimeScaleFactor,		// time scale to apply to cued motion
		grFloat TimeIntoMotion,			// time offset to begin motion with (Not TimeScaled)
		grFloat BlendTime,				// time to apply a blend. 
		grFloat BlendFromAmount,		// blend value at current time
		grFloat BlendToAmount,			// blend value after BlendTime time has elapsed
		const grXForm3d *MotionTransform);	// local transform to adjust motion by (NULL implies NO transform)

////////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationRemoveLastCue(grActor *A)
/// @brief Removes the last animation
/// @param[in] A The actor to remove the cue from
/// @return GR_TRUE if the cue was removed, GR_FALSE if there are no cues to remove
/// @note  Can be called repeatedly to successively remove older and older cues
///////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationRemoveLastCue( grActor *A );

///////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationStep(grActor *A, grFloat DeltaTime)
/// @brief Applies a time step
/// @param[in] A The actor to apply the time step to
/// @param[in] DeltaTime The time to apply
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Re-poses the actor according to all currently applicable Animation Cues. (failure implies Actor is incompletely initialized)
//////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationStep(grActor *A, grFloat DeltaTime );

//////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationTestStep(grActor *A, grFloat DeltaTime)
/// @brief Applies a 'temporary' time step
/// @param[in] A Actor to apply step to
/// @param[in] DeltaTime The time to apply
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Re-poses the actor according to all currently appliciable cues.  (failure implies Actor is incompletely initialized)
/// DeltaTime is always relative to the the last AnimationStep()
//////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationTestStep(grActor *A, grFloat DeltaTime);

//////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName)
/// @brief Optimized version of grActor_AnimationStep 
/// @param[in] A The actor to apply the animation step to
/// @param[in] DeltaTime The time to apply
/// @param[in] BoneName The bone to apply the step to
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Limits calculations to the bone named BoneName, and it's 
/// parents.  BoneName will be correctly computed, but the other bones will be wrong.  This is usefull for 
/// moving and animating an actor that is not actually visible.  Rendering and queries will be 'optimized'
/// until the actor is given any pose or animation that doesn't go through grActor_AnimationStepBoneOptimized() or 
///  grActor_AnimationTestStepBoneOptimized().  BoneName can be NULL to compute only 'root' bone.
//////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName );

//////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationTestStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName)
/// @brief Optimized version of grActor_AnimationTestStep
/// @param[in] A The actor to apply the step to
/// @param[in] DeltaTime The time to apply
/// @param[in] BoneName The bone to apply the step to
/// @return GR_TRUE on success, GR_FALSE on failure
/// @note Limits calculations to the bone named BoneName, and it's 
/// parents.  BoneName will be correctly computed, but the other bones will be wrong.  This is usefull for 
/// moving and animating an actor that is not actually visible.  Rendering and queries will be 'optimized'
/// until the actor is given any pose or animation that doesn't go through grActor_AnimationStepBoneOptimized() or 	
/// grActor_AnimationTestStepBoneOptimized().  BoneName can be NULL to compute only 'root' bone.
//////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationTestStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName);

/////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationNudge(grActor *A, grXForm3d *Offset)
/// @brief Applies an 'immediate' offset to the animated actor
/// @param[in] A The actor to apply the offset to
/// @param[in] Offset The offset to apply
/// @return GR_TRUE on success, GR_FALSE on failure
/////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationNudge(grActor *A, grXForm3d *Offset);

/////////////////////////////////////////////////
/// @fn grBoolean grActor_GetAnimationEvent(grActor *A, const char **ppEventString)
/// @brief Gets the current animation event from an actor
/// @param[in] A The actor to query
/// @param[out] ppEventString The current event string
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetAnimationEvent(grActor *A,						
	const char **ppEventString);		// Return data, if return value is GR_TRUE

////////////////////////////////////////////////
/// @fn grBoolean grActor_GetIndexedBoneWorldSpaceVertexLocations(const grActor *Actor, int32 boneIndex, int32 aSize, grVec3d *pVerts)
/// @brief Gets the vertices of a bone in world coordinates
/// @param[in] Actor The actor to query
/// @param[in] boneIndex The index of the bone that has the vertices
/// @param[in] aSize The size of the vertex array
/// @param[out] pVerts The vertex array
/// @return GR_TRUE on success, GR_FALSE on failure
////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetIndexedBoneWorldSpaceVertexLocations(const grActor* pActor, int32 boneIndex, int32 aSize,
	grVec3d* pVerts);

////////////////////////////////////////////////
/// @fn grBoolean grActor_GetNamedBoneWorldSpaceVertexLocations(const grActor *pActor, const char *pBoneName, int32 aSize, grVec3d *pVerts)
/// @brief Gets the vertices of a bone in world coordinates
/// @param[in] pActor The actor to query
/// @param[in] pBoneName The name of the bone that has the vertices
/// @param[in] aSize The size of the vertex array
/// @param[out] pVerts The vertex array
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetNamedBoneWorldSpaceVertexLocations(const grActor* pActor, const char* pBoneName, int32 aSize,
	grVec3d* pVerts);

///////////////////////////////////////////////
/// @fn int32 grActor_GetCount(void)
/// @brief Returns number of actors that are currently created
/// @return The number of actors that are created
///////////////////////////////////////////////
GRAPI int32 GRCC grActor_GetCount(void);

//////////////////////////////////////////////////////////////////////////////////////////
/// @fn	grBoolean grActor_MoveCollision(grActor *Actor, const grWorld *World, const grVec3d *Front, const grVec3d *Back, grCollisionInfo *CollisionInfo)
/// @brief This function checks an Actor to see if it collides with other Actors in the world or with the world itself
/// @param[in] Actor The actor to test collision for
/// @param[in] World The world to test collision with
/// @param[in] Front The forward vector
/// @param[in] Back The back vector
/// @param[out] CollisionInfo The resulting information from the collision
/// @return GR_TRUE if a collision happened, GR_FALSE if no collision
/// @note The current Actor position is irrelevant as it assumes the actor
/// is moving on a path from Front to Back.  The actor's collision box must have previously
/// been properly set.
//////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_MoveCollision(grActor *Actor, const grWorld *World, const grVec3d *Front, const grVec3d *Back, grCollisionInfo *CollisionInfo);

//////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_RotateCollision(grActor *Actor, const grWorld *World, const grXForm3d *NewTransform, grCollisionInfo *CollisionInfo)
/// @brief This function checks an Actor to see if it collides with other Actors in the world or with the world itself during a rotation (orientation) change
/// @param[in] Actor The actor to test collision for
/// @param[in] World The world to test collision with
/// @param[out] NewTransform The resulting transform from the collision
/// @param[out] CollisionInfo The resulting information from the collision
/// @return GR_TRUE if there was a collision, GR_FALSE if no collision
/// @note This isn't really needed if you're not using bone level collisions.  If you are, it's very necessary in order to keep 
/// your actors in the world.
//////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_RotateCollision(grActor *Actor, const grWorld *World, const grXForm3d *NewTransform, grCollisionInfo *CollisionInfo);

//////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_AnimationCollision(grActor *Actor, const grWorld *World, const grFloat DeltaTime, grCollisionInfo *CollisionInfo)
/// @brief This function checks an Actor to see if it collides with other Actors in the world orwith the world itself during an animation change
/// @param[in] Actor The actor to test collision for
/// @param[in] World The world to test collision with
/// @param[in] DeltaTime The animation keyframe to test collision with
/// @param[out] CollisionInfo The resulting information from the collision
/// @return GR_TRUE if there was a collision, GR_FALSE if no collision
/// @note  This isn't really needed if you're not using bone level collisions.  If you are, it's very necessary in order to keep 
/// your actors in the world.
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_AnimationCollision(grActor *Actor, const grWorld *World,	const grFloat DeltaTime, grCollisionInfo *CollisionInfo);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_Collision(grActor *Actor, const grWorld *World, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grCollisionInfo *CollisionInfo)
/// @brief Tests to see if a Box moving along a path from Front to Back collides with the Actor
/// @param[in] Actor The actor to test collision for
/// @param[in] World The world to test collision with
/// @param[in] Box The bounding box to test collision with
/// @param[in] Front The forward vector
/// @param[in] Back The back vector
/// @param[out] CollisionInfo The resulting information from the collision
/// @return GR_TRUE if there was a collision, GR_FALSE if no collision
/// @note The Actor's bounding box must have previously been set and Box must be relative to the path (meaning not in world-space coordinates).
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_Collision(grActor *Actor, const grWorld* World, const grExtBox *Box, const grVec3d *Front, const grVec3d	*Back, grCollisionInfo *CollisionInfo);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_ChangeBoxCollision(grActor *Actor, const grWorld *World, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grChangeBoxCollisionInfo *CollisionInfo)
/// @brief Tests to see if a Box changing from FrontBox to BackBox collides with the Actor
/// @param[in] Actor The actor to test collision for
/// @param[in] World The world to test collision with
/// @param[in] Pos The actor's position
/// @param[in] FrontBox The forward axis-aligned bounding box
/// @param[in] BackBox The back axis-aligned bounding box
/// @param[out] CollisionInfo The resulting collision information
/// @return GR_TRUE if there was a collision, GR_FALSE if no collision
/// @note The Actor's bounding box must have previously been set and Box must be relative to
///  the path (meaning not in world-space coordinates)
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_ChangeBoxCollision(grActor *Actor, const grWorld *World, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grChangeBoxCollisionInfo *CollisionInfo);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_AddCollisionBone(grActor *Actor, const char *BoneName)
/// @brief Adds a bone to be tested for collision
/// @param[in] Actor The actor to modify
/// @param[in] BoneName The name of the bone to test for collision
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_AddCollisionBone(grActor *Actor, const char* BoneName);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_RemoveCollisionBone(grActor *Actor, const char *BoneName)
/// @brief Removes a bone from the collision list
/// @param[in] Actor The actor to modify
/// @param[in] BoneName The name of the bone to remove
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_RemoveCollisionBone(grActor *Actor, const char* BoneName);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_HasCollisionBone(const grActor *Actor, const char *BoneName)
/// @brief Checks to see if the given bone name is in the collision list
/// @param[in] Actor The actor to query
/// @param[in] BoneName The bone name to query for
/// @return GR_TRUE if the bone is in the list, GR_FALSE if not
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_HasCollisionBone(const grActor *Actor, const char* BoneName);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn char *grActor_GetNextCollisionBone(grActor *Actor, char *BoneName)
/// @brief Returns the next bone name in the collision list
/// @param[in] Actor The actor to query
/// @param[in] BoneName The name of the bone to start with.  If NULL, it will start with the first bone in the list
/// @return The next bone name in the list, NULL if at the end of the list
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI char* GRCC grActor_GetNextCollisionBone(grActor *Actor, char *BoneName);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn char *grActor_GetNextCollisionBoneWithExtBoxes(grActor *Actor, char *BoneName, grExtBox *PrevBox, grExtBox *CurrBox)
/// @brief Gets the next collision bone in the list with it's axis-aligned bounding boxes
/// @param[in] Actor The actor to query
/// @param[in] BoneName The name of the bone to start with, If NULL, it will start with the first bone in the list
/// @param[out] PrevBox The previous bounding box
/// @param[out] CurrBox The current bounding box
/// @return The next bone name in the list, NULL if at the end of the list
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI char* GRCC grActor_GetNextCollisionBoneWithExtBoxes(grActor *Actor, char *BoneName, grExtBox *PrevBox, grExtBox *CurrBox);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_RecalcCollisionBones(grActor *Actor)
/// @brief Recalculates the collision bones
/// @param[in] Actor The actor to modify
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_RecalcCollisionBones(grActor *Actor);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_SetCollidedBone(grActor *Actor, char *BoneName)
/// @brief Sets the collided bone for an actor
/// @param[in] Actor The actor to modify
/// @param[in] BoneName The name of the bone to set
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetCollidedBone(grActor* Actor, char *BoneName);

/////////////////////////////////////////////////////////////////////////////////////////
/// @fn char *grActor_GetCollidedBone(const grActor *Actor)
/// @brief Gets a bone that was collided with
/// @param[in] Actor The actor to query
/// @return The name of the bone that was collided with
/////////////////////////////////////////////////////////////////////////////////////////
GRAPI char* GRCC grActor_GetCollidedBone(const grActor* Actor);

/////////////////////////////////////////////////////////////////////////////////////////////
/// @brief This function is used to set whether or not a particular Actor should be checked during a collision test.  Three Flags are possible, you should set ONLY one:
/// @note Actors are initialized with a COLLIDE_SOLID state.
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
/// @def COLLIDE_EMPTY
/// @brief This actor can not collide against anything or have other others collide with it
////////////////////////////////////////////////////////////////////////////////////////////
#define COLLIDE_EMPTY (1<<12)

////////////////////////////////////////////////////////////////////////////////////////////
/// @def COLLIDE_SOLID
/// @brief This actor can collide with other objects and have other objects collide with it
////////////////////////////////////////////////////////////////////////////////////////////
#define COLLIDE_SOLID  (1<<13)

////////////////////////////////////////////////////////////////////////////////////////////
/// @def COLLIDE_INVISIBLE
/// @brief This actor can collide with other objects, but other objects can not collide with it
////////////////////////////////////////////////////////////////////////////////////////////
#define COLLIDE_INVISIBLE (1<<14)	

////////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_SetCollisionFlags(grActor *Actor, const uint32 Flags)
/// @brief Sets the actor's collision flags
/// @param[in] Actor The Actor to set collision flags for
/// @param[in] Flags The flags to set
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetCollisionFlags(grActor* Actor, const uint32 Flags);

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn uint32 grActor_GetCollisionFlags(const grActor *Actor)
/// @brief Gets the collision flags for an actor
/// @param[in] Actor The actor to query
/// @return The collision flags
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI uint32 GRCC grActor_GetCollisionFlags(const grActor* Actor);

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_InitObject(const grActor *A, grObject *O)
/// @brief Initializes the actor object
/// @param[in] A The actor to initialize the object with
/// @param[in] O The object to initialize
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_InitObject(const grActor *A,grObject *O);

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_RegisterObjectDef(void)
/// @brief Registers the actor object with the engine
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_RegisterObjectDef(void);

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn void grActor_SetActorDef(grActor *A, grActor_Def *Def)
/// @brief Sets the actors definition
/// @param[in] A The actor to modify
/// @param[in] Def The actor definition to set
/// @note Must be called prior to using your actor, imediately after Create!
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grActor_SetActorDef(grActor *A, grActor_Def *Def);

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_GetXForm(const grActor *Actor, grXForm3d *Xf)
/// @brief Gets the actor's transform
/// @param[in] Actor The actor to query
/// @param[out] Xf The actor's transform
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_GetXForm(const grActor* Actor,	// object instance data
	grXForm3d	*Xf );		// where to store xform

///////////////////////////////////////////////////////////////////////////////////////////
/// @fn grBoolean grActor_SetXForm(grActor *Actor, const grXForm3d *Xf)
/// @brief Sets an actor's transform
/// @param[in] Actor The actor to modify
/// @param[in] Xf The transform
/// @return GR_TRUE on success, GR_FALSE on failure
///////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grActor_SetXForm(
	grActor* Actor,	// object instance data
	const grXForm3d	*Xf );		// where to store xform


/*! @fn void grActor_SetRenderNextTime(grActor *Actor, grBoolean RenderNextTime)
	@brief Sets an actor's render next pass flag
	@author Bruno Pettorelli (krouer3d@yahoo.fr)
	
	@param[in] Actor The actor to modify
	@param[in] RenderNextTime The next rendering pass flag

    @see grObject_SetRenderNextPass
*/
GRAPI void GRCC grActor_SetRenderNextTime(grActor* Actor, grBoolean RenderNextTime);

///////////////////////////////////////////////////////////////////////////////////////////
/// @enum ActorObjectProperties
///////////////////////////////////////////////////////////////////////////////////////////
enum ActorObjectProperties
{

	// scale
	//ACTOROBJ_SCALEGROUP_ID = PROPERTY_LOCAL_DATATYPE_START,
	ACTOROBJ_SCALEX_ID = PROPERTY_LOCAL_DATATYPE_START,
	/*ACTOROBJ_SCALEY_ID,
	ACTOROBJ_SCALEZ_ID,*/
	//ACTOROBJ_SCALEGROUPEND_ID,

	// ambient color
	ACTOROBJ_AMBIENTLIGHTGROUP_ID,
	ACTOROBJ_AMBIENTLIGHT_ID,
	ACTOROBJ_AMBIENTLIGHTRED_ID,
	ACTOROBJ_AMBIENTLIGHTGREEN_ID,
	ACTOROBJ_AMBIENTLIGHTBLUE_ID,
	ACTOROBJ_AMBIENTLIGHTGROUPEND_ID,

	// fill light
	ACTOROBJ_FILLIGHTGROUP_ID,
	ACTOROBJ_FILLLIGHT_ID,
	ACTOROBJ_FILLLIGHTRED_ID,
	ACTOROBJ_FILLLIGHTGREEN_ID,
	ACTOROBJ_FILLLIGHTBLUE_ID,
	ACTOROBJ_FILLLIGHTUSE_ID,
	ACTOROBJ_FILLLIGHTNORMALACTORRELATIVE_ID,
	ACTOROBJ_FILLLIGHTNORMAL_ID,
	ACTOROBJ_FILLLIGHTNORMALX_ID,
	ACTOROBJ_FILLLIGHTNORMALY_ID,
	ACTOROBJ_FILLLIGHTNORMALZ_ID,
	ACTOROBJ_FILLLIGHTNORMALEND_ID,
	ACTOROBJ_FILLIGHTGROUPEND_ID,

	// collision box
	ACTOROBJ_COLLISIONEXTBOXGROUP_ID,
	ACTOROBJ_COLLISIONEXTBOXDISPLAY_ID,
	ACTOROBJ_COLLISIONEXTBOXMINX_ID,
	ACTOROBJ_COLLISIONEXTBOXMINY_ID,
	ACTOROBJ_COLLISIONEXTBOXMINZ_ID,
	ACTOROBJ_COLLISIONEXTBOXMAXX_ID,
	ACTOROBJ_COLLISIONEXTBOXMAXY_ID,
	ACTOROBJ_COLLISIONEXTBOXMAXZ_ID,
	ACTOROBJ_COLLISIONEXTBOXGROUPEND_ID,

	// render box
	ACTOROBJ_RENDEREXTBOXGROUP_ID,
	ACTOROBJ_RENDEREXTBOXDISPLAY_ID,
	ACTOROBJ_RENDEREXTBOXMINX_ID,
	ACTOROBJ_RENDEREXTBOXMINY_ID,
	ACTOROBJ_RENDEREXTBOXMINZ_ID,
	ACTOROBJ_RENDEREXTBOXMAXX_ID,
	ACTOROBJ_RENDEREXTBOXMAXY_ID,
	ACTOROBJ_RENDEREXTBOXMAXZ_ID,
	ACTOROBJ_RENDEREXTBOXGROUPEND_ID,

	//
	ACTOROBJ_PERBONELIGHTING_ID,
	ACTOROBJ_USEAMBIENTLIGHTFROMFLOOR_ID,
	ACTOROBJ_MAXIMUMDYNAMICLIGHTSTOUSE_ID,
	ACTOROBJ_MAXIMUMSTATICLIGHTSTOUSE_ID,
	ACTOROBJ_MOTIONTIMESCALE_ID,

	// lists
	ACTOROBJ_LIST_ID,
	ACTOROBJ_MOTIONLIST_ID,
	ACTOROBJ_LIGHTREFERENCEBONENAMELIST_ID,

	// materials
	ACTOROBJ_MATERIALGROUP_ID,
	ACTOROBJ_MATERIALLIST_ID,
	/*ACTOROBJ_MATERIALRED_ID,
	ACTOROBJ_MATERIALGREEN_ID,
	ACTOROBJ_MATERIALBLUE_ID,*/
	ACTOROBJ_MATERIALOVERIDE_ID,
	ACTOROBJ_MATERIALMAPPER_ID,
	ACTOROBJ_MATERIALGROUPEND_ID,

	//
	ACTOROBJ_LAST_ID
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @enum ActorObjectIndexProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ActorObjectIndexProperties
{

	// scale
	//ACTOROBJ_SCALEGROUP_INDEX = 0,
	ACTOROBJ_SCALEX_INDEX = 0,
	/*ACTOROBJ_SCALEY_INDEX,
	ACTOROBJ_SCALEZ_INDEX,*/
	//ACTOROBJ_SCALEGROUPEND_INDEX,

	// ambient color
	ACTOROBJ_AMBIENTLIGHTGROUP_INDEX,
	ACTOROBJ_AMBIENTLIGHT_INDEX,
	ACTOROBJ_AMBIENTLIGHTRED_INDEX,
	ACTOROBJ_AMBIENTLIGHTGREEN_INDEX,
	ACTOROBJ_AMBIENTLIGHTBLUE_INDEX,
	ACTOROBJ_AMBIENTLIGHTGROUPEND_INDEX,

	// fill light
	ACTOROBJ_FILLIGHTGROUP_INDEX,
	ACTOROBJ_FILLLIGHT_INDEX,
	ACTOROBJ_FILLLIGHTRED_INDEX,
	ACTOROBJ_FILLLIGHTGREEN_INDEX,
	ACTOROBJ_FILLLIGHTBLUE_INDEX,
	ACTOROBJ_FILLLIGHTUSE_INDEX,
	ACTOROBJ_FILLLIGHTNORMALACTORRELATIVE_INDEX,
	ACTOROBJ_FILLLIGHTNORMAL_INDEX,
	ACTOROBJ_FILLLIGHTNORMALX_INDEX,
	ACTOROBJ_FILLLIGHTNORMALY_INDEX,
	ACTOROBJ_FILLLIGHTNORMALZ_INDEX,
	ACTOROBJ_FILLLIGHTNORMALEND_INDEX,
	ACTOROBJ_FILLIGHTGROUPEND_INDEX,

	// collision box
	ACTOROBJ_COLLISIONEXTBOXGROUP_INDEX,
	ACTOROBJ_COLLISIONEXTBOXDISPLAY_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMINX_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMINY_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMINZ_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMAXX_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMAXY_INDEX,
	ACTOROBJ_COLLISIONEXTBOXMAXZ_INDEX,
	ACTOROBJ_COLLISIONEXTBOXGROUPEND_INDEX,

	// render box
	ACTOROBJ_RENDEREXTBOXGROUP_INDEX,
	ACTOROBJ_RENDEREXTBOXDISPLAY_INDEX,
	ACTOROBJ_RENDEREXTBOXMINX_INDEX,
	ACTOROBJ_RENDEREXTBOXMINY_INDEX,
	ACTOROBJ_RENDEREXTBOXMINZ_INDEX,
	ACTOROBJ_RENDEREXTBOXMAXX_INDEX,
	ACTOROBJ_RENDEREXTBOXMAXY_INDEX,
	ACTOROBJ_RENDEREXTBOXMAXZ_INDEX,
	ACTOROBJ_RENDEREXTBOXGROUPEND_INDEX,

	//
	ACTOROBJ_PERBONELIGHTING_INDEX,
	ACTOROBJ_USEAMBIENTLIGHTFROMFLOOR_INDEX,
	ACTOROBJ_MAXIMUMDYNAMICLIGHTSTOUSE_INDEX,
	ACTOROBJ_MAXIMUMSTATICLIGHTSTOUSE_INDEX,
	ACTOROBJ_MOTIONTIMESCALE_INDEX,

	// lists	
	ACTOROBJ_LIST_INDEX,
	ACTOROBJ_MOTIONLIST_INDEX,
	ACTOROBJ_LIGHTREFERENCEBONENAMELIST_INDEX,

	// materials
	ACTOROBJ_MATERIALGROUP_INDEX,
	ACTOROBJ_MATERIALLIST_INDEX,
	/*ACTOROBJ_MATERIALRED_INDEX,
	ACTOROBJ_MATERIALGREEN_INDEX,
	ACTOROBJ_MATERIALBLUE_INDEX,*/
	ACTOROBJ_MATERIALOVERIDE_INDEX,
	ACTOROBJ_MATERIALMAPPER_INDEX,
	ACTOROBJ_MATERIALGROUPEND_INDEX,

	// end marker
	ACTOROBJ_LAST_INDEX
};

#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grActor_TransformContext grActor_TransformContext;
typedef struct grActor_RenderHint grActor_RenderHint;


#endif
