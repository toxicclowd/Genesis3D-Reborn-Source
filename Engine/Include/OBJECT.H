/*!
	@file Object.h 
	
	@author
	@brief Object definition

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
*/
#ifndef GR_OBJECT_H
#define GR_OBJECT_H

#include "grTypes.h"
#include "VFile.h"
#include "ExtBox.h"
#include "Bitmap.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grProperty.h"
#include "grChain.h"
#include "grPtrMgr.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINVER
#define HWND void *
#endif

#include "Sound.h"

/*}{********* grObject ********/

#include "grPtrMgr.h"
typedef struct grEngine		grEngine;
typedef struct grWorld		grWorld;

typedef struct grObject		grObject;
typedef struct grObjectDef	grObjectDef;

#define FILE_UNIQUE_ID 3132205802


#define CONTENTS_EMPTY 0
#define CONTENTS_SOLID 1
#define CONTENTS_INVISIBLE 2

/*! struct grObject
	@brief The grObject data structure
*/
struct grObject
{
	char *				Name;		//!< Name of the object in clear
	const grObjectDef	*Methods;	//!< Array of functions any object of this kind can use
	void				*Instance;	//!< Value of one object of this grObject kind
	grObject			*Parent;	//!< Parent object of the object Instance
	grChain				*Children;	//!< List of all childrens of this object Instance
	grWorld	*			pWorld;		//!< grWorld attached to this object Instance
	grEngine *			pEngine;	//!< grEngine attached to this object Instance
	grSound_System *	pSoundSystem;	//!< grSound_System attached to this object Instance
	/*! Collision contents 
		@author Incarnadine
	*/
	int32				Contents;					
	int32				RefCnt;		//!< The grObject reference counter
	grObject*			Self;
};

/*! @name grObject XForm mod flags */
/*@{ */
/*! @def GR_OBJECT_XFORM_TRANSLATE
	@brief The translation of an XForm can be modifyed
*/
#define GR_OBJECT_XFORM_TRANSLATE		(1<<0)
/*! @def GR_OBJECT_XFORM_SCALE
	@brief The XForm can be scaled
*/
#define GR_OBJECT_XFORM_SCALE			(1<<1)
/*! @def GR_OBJECT_XFORM_ROTATE
	@brief The XForm can be rotated
*/
#define GR_OBJECT_XFORM_ROTATE			(1<<2)
/*! @def GR_OBJECT_XFORM_SHEAR
	@brief The XForm can be sheared
*/
#define GR_OBJECT_XFORM_SHEAR			(1<<3)
/*! @def GR_OBJECT_XFORM_ALL
	@brief All flags altogether
*/
#define GR_OBJECT_XFORM_ALL				( GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_SCALE |GR_OBJECT_XFORM_ROTATE|GR_OBJECT_XFORM_SHEAR)
/*@} */

/*! @name grObject flag possible values */
/*@{ */
#define GR_OBJECT_HIDDEN				0x0001		//!< This object is can not be created by user
#define GR_OBJECT_VISRENDER				0x0002		//!< This object must be rendered only if visible
/*@} */

/*! @enum grObject_Type
	@brief grObject kind possible values */
typedef enum
{
	GR_OBJECT_TYPE_OBJECTLIST,
	GR_OBJECT_TYPE_ACTOR,
	GR_OBJECT_TYPE_MODEL,
	GR_OBJECT_TYPE_TERRAIN,
	GR_OBJECT_TYPE_CURVE,
	GR_OBJECT_TYPE_PORTAL,
	GR_OBJECT_TYPE_UNKNOWN
} grObject_Type;

	// the _UNKNOWN type cannot be cast & must be accessed through its ->Methods virtual methods
	// note : we (or the user) can add types after _UNKNOWN
	// you should always check >= _UNKNOWN to see if you can't cast it; (never use == _UNKNOWN)

// Render Flags
typedef uint32											grObject_RenderFlags;

#define GR_OBJECT_RENDER_FLAG_CAMERA_FRUSTUM			(1<<0)

// Built in system messages 
//	(Should these be in grWorld.h? World is really the object mgr, and should define these types of msg's...)
#define GR_OBJECT_MSG_WORLD_ADD_SLIGHT_UPDATE			(-1)		// SLIGHT = Static Light
#define GR_OBJECT_MSG_WORLD_REMOVE_SLIGHT_UPDATE		(-2)
#define GR_OBJECT_MSG_WORLD_REBUILDBSP		(-3)  // Incarnadine
#define GR_OBJECT_MSG_WORLD_REBUILDLIGHTS (-4) // Incarnadine

/*}{********************** Object-Manager Functions ******************/

GRAPI grBoolean	GRCC grObject_RegisterGlobalObjectDef(const grObjectDef * Methods);
							// you must Register your Def if you want _CreateFromFile
							//	to be able to make objects of your type.
							// this means Registering user objects *before* calling grWorld_Create()

GRAPI int32		GRCC grObject_GetRegisteredN();
							// Returns the number of Registered Defs

GRAPI const char*	GRCC grObject_GetRegisteredDefName( int Index );
GRAPI uint32		GRCC grObject_GetRegisteredFlags( int Index );
GRAPI grBoolean	GRCC grObject_GetRegisteredPropertyList(const char * TypeName, grProperty_List **List);
GRAPI grBoolean	GRCC grObject_SetRegisteredProperty( const char * TypeName, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );

							// Index must be less than the number of Registered Defs 
/*}{********************** Object Functions ******************/

GRAPI grObject *	GRCC grObject_Create		( const char * TypeName );					// mallocs a blank object
GRAPI grObject *	GRCC grObject_Duplicate( grObject *pObject );

GRAPI void			GRCC grObject_Destroy	(grObject ** pObject);	// frees & destroys an object
GRAPI void			GRCC grObject_SetName( grObject * pObject, const char * Name );
GRAPI const char  *GRCC grObject_GetName( const grObject * pObject );

GRAPI grBoolean	GRCC grObject_WriteToFile(const grObject * Object,grVFile * File, grPtrMgr *PtrMgr);
GRAPI grObject *	GRCC grObject_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr);

GRAPI grObject_Type	GRCC grObject_GetType(const grObject * Object);
GRAPI const char	*GRCC grObject_GetTypeName	(const grObject * Object);

GRAPI grBoolean	GRCC grObject_GetPropertyList(const grObject *Object, grProperty_List **List);
GRAPI grBoolean	GRCC grObject_SetProperty(grObject *Object, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );
GRAPI grBoolean	GRCC grObject_GetProperty(const grObject *Object, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );

GRAPI void   *		GRCC grObject_GetInstance( const grObject *Object );
GRAPI void			GRCC grObject_CreateInstanceRef(grObject * Object);
GRAPI grBoolean	GRCC grObject_Free(grObject * Object);	
						// destroys the *Instance*, not the object's memory
						// returns GR_FALSE if RefCnt is not 0
GRAPI void			GRCC grObject_CreateRef(grObject * Object);

GRAPI grBoolean	GRCC grObject_Render(	const grObject			*Object,
													const grWorld			*World, 
													const grEngine			*Engine,
													const grCamera			*Camera, 
													const grFrustum			*CameraSpaceFrustum, 
													grObject_RenderFlags	RenderFlags);

GRAPI grBoolean	GRCC grObject_AttachWorld( grObject *Object, grWorld * pWorld );
GRAPI grBoolean	GRCC grObject_DettachWorld( grObject *Object, grWorld * pWorld );
GRAPI grBoolean	GRCC grObject_AttachEngine( grObject *Object, grEngine *Engine );
GRAPI grBoolean	GRCC grObject_DettachEngine(grObject *Object, grEngine *Engine );
GRAPI grBoolean	GRCC grObject_AttachSoundSystem( grObject *Object, grSound_System *pSoundSystem );
GRAPI grBoolean	GRCC grObject_DettachSoundSystem(grObject *Object, grSound_System *pSoundSystem );

GRAPI grBoolean	GRCC grObject_Collision(const grObject *Object, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane, grObject ** SubObject);

GRAPI grBoolean	GRCC grObject_GetExtBox	(const grObject * Object,grExtBox *BBox);

GRAPI grBoolean	GRCC grObject_SetXForm	(grObject * Object,const grXForm3d *XF);
GRAPI grBoolean	GRCC grObject_GetXForm	(const grObject * Object,grXForm3d *XF);
GRAPI int			GRCC grObject_GetXFormModFlags( const grObject * Object );

GRAPI grBoolean	GRCC grObject_GetChildren(const grObject * Object,grObject * Children,int MaxNumChildren);
GRAPI grObject		*GRCC grObject_GetNextChild(const grObject *Object, grObject *Start);
GRAPI grObject		*GRCC grObject_GetParent( const grObject *Object );
GRAPI grBoolean	GRCC grObject_AddChild	(grObject * Object,   grObject * Child);
GRAPI grBoolean	GRCC grObject_RemoveChild(grObject * Object, grObject * Child);

#ifdef WIN32
GRAPI grBoolean	GRCC grObject_EditDialog (grObject * Object,HWND Parent);
#endif

#ifdef BUILD_BE
GRAPI grBoolean	GRCC grObject_EditDialog (grObject * Object,class G3DView * Parent);
#endif

GRAPI grBoolean	GRCC grObject_SendMessage (grObject * Object,int32 Msg, void * Data);

GRAPI grBoolean	GRCC grObject_Frame (grObject * Object,float TimeDelta );

GRAPI int32 GRCC grObject_GetContents(const grObject * Object);
GRAPI void GRCC grObject_SetContents(grObject * Object, int32 Contents);

typedef struct grCollisionInfo grCollisionInfo;
GRAPI grBoolean GRCC grObject_MoveCollision(grObject * Object, 											
											const grVec3d *TargetPos,
											grCollisionInfo *CollisionInfo);

// Added by Icestorm
GRAPI grBoolean	GRCC grObject_ChangeBoxCollision(const grObject	*Object,
													  const grVec3d		*Pos,
													  const grExtBox	*FrontBox,
													  const grExtBox	*BackBox,
													  grExtBox			*ImpactBox,
													  grPlane			*Plane,
													  grObject			** SubObject);

// KROUER: Actor/Object visibility
/*! @fn void grObject_SetRenderNextPass(const grObject	*Object, grBoolean RenderNext)
	@brief Toggle the render of the object in the next rendering pass
    
	This function is used mainly for visibility definition.<br>To render the objects in 
	the next objects rendering loop, you must set the RenderNextTime flag to GR_TRUE.
	To make the object rendering loop ignoring it, you must set the RenderingNextTime 
	flag to GR_FALSE.
	JetEngine rendering is done in 2 passes:<br>
	1. Define which part of the BSP to render and render them<br>
	2. Render all objects<br>
	<br>
	To be more accurate, default BSP is embed in the head object of the object list. By this way,
	default BSP object is always rendered first.
	@author Bruno Pettorelli (krouer@jet3d.com)

	@param[in] Object The object on which apply the RenderNext value
	@param[in] RenderNext A boolean value to set or reset the Render nex pass value
*/
GRAPI void			GRCC grObject_SetRenderNextPass(const grObject	*Object, grBoolean RenderNext);

/*! @fn uint32 grObject_GetFlags(const grObject * Object);
	@brief Read the Flag member of the Object

    @param[in] Object The object from which to read the flag
	@return The reading flag
*/
GRAPI uint32		GRCC grObject_GetFlags(const grObject * Object);

/***

todos :

	1. make a _TYPE_LIST which is just a container for Add & Remove calls;
		Render & such call the function on all children

***/

/*}{******** The Object Def *********/

struct grObjectDef
{
	grObject_Type	Type;
	const char *	Name;		// Defs MUST have unique names!
	uint32			Flags;
	
	void	*	( GRCC * CreateInstance)(void);
	void		( GRCC * CreateRef)	(void * Instance);
	grBoolean	( GRCC * Destroy)		(void **pInstance);
				//Returns GR_TRUE if refcnt is 0 and instance is destroyed

	grBoolean	( GRCC * AttachWorld ) ( void * Instance, grWorld * pWorld );
	grBoolean	( GRCC * DettachWorld ) ( void * Instance, grWorld * pWorld );
				
	grBoolean	( GRCC * AttachEngine ) ( void * Instance, grEngine *Engine );
	grBoolean	( GRCC * DettachEngine ) ( void * Instance, grEngine *Engine );

	grBoolean	( GRCC * AttachSoundSystem ) ( void * Instance, grSound_System *SoundSystem );
	grBoolean	( GRCC * DettachSoundSystem ) ( void * Instance, grSound_System *SoundSystem );

	grBoolean	( GRCC * Render) (const void * Instance, const grWorld *World, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags);

	grBoolean	( GRCC * Collision)	(const void * Instance, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane);

	grBoolean	( GRCC * GetExtBox)	(const void * Instance,grExtBox *BBox);

	//--- the stuff above this point is pretty secure

	void *		( GRCC * CreateFromFile)(grVFile * File, grPtrMgr *PtrMgr);
	grBoolean	( GRCC * WriteToFile)	(const void * Instance,grVFile * File, grPtrMgr *PtrMgr);

	grBoolean	( GRCC * GetPropertyList)	(void * Instance, grProperty_List **List);
	grBoolean	( GRCC * SetProperty)      (void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );
	grBoolean	( GRCC * GetProperty )( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );

	grBoolean	( GRCC * SetXForm)	(void * Instance,const grXForm3d *XF);
	grBoolean	( GRCC * GetXForm)	(const void * Instance,grXForm3d *XF);
								// if your object is added to a world, you *MUST* call World_UpdateObject if
								//	you call Object_SetXForm
	int			( GRCC * GetXFormModFlags )( const void * Instace );
								//The flags determine in what way is it valid to modify the transform

	grBoolean	( GRCC * GetChildren)	(const void * Instance,grObject * Children,int MaxNumChildren);
	grBoolean	( GRCC * AddChild)	(void * Instance,const grObject * Child);
	grBoolean	( GRCC * RemoveChild)	(void * Instance,const grObject * Child);

	grBoolean	( GRCC * EditDialog )	(void * Instance, HWND Parent);
	grBoolean	( GRCC * SendMessage )	(void * Instance, int32 Msg, void * Data);
	grBoolean	( GRCC * Frame ) (void * Instance, float Time );
	void *		( GRCC * DuplicateInstance )( void * Instance );

	// Added by Icestorm
	grBoolean	( GRCC * ChangeBoxCollision )(const void * Instance, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane);

	grBoolean	( GRCC * GetGlobalPropertyList)	(grProperty_List **List);
	grBoolean	( GRCC * SetGlobalProperty)      (int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );

	// Added by KROUER
	/*! @brief Enable the draw for the next render pass
		@see grObject_SetRenderNextPass
	*/
	void        ( GRCC * SetRenderNextPass) (void * Instance, grBoolean RenderNext);
};

#ifndef WINVER
#undef HWND
#endif


#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grObjectDef grObject_Def;
typedef grObject_Type grObject_Type;
typedef grObject_RenderFlags grObject_RenderFlags;


#endif

