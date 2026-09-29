/****************************************************************************************/
/*  CAMOBJECT.H                                                                         */
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
#include "Engine.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grProperty.h"
#include "Object.h"
#include "grWorld.h"
#include "grPtrMgr.h"

#ifdef WIN32
void Init_Class( HINSTANCE hInstance );
#endif

#ifdef BUILD_BE
void Init_Class( image_id hInstance );
#endif

void * GRCC CreateInstance(void);
void * GRCC DuplicateInstance(void * Instance);
void  GRCC CreateRef(void * Instance);
grBoolean GRCC Destroy(void **pInstance);
grBoolean	GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags);
grBoolean	GRCC AttachWorld( void * Instance, grWorld * pWorld );
grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld );
grBoolean	GRCC AttachEngine ( void * Instance, grEngine *Engine );
grBoolean	GRCC DettachEngine( void * Instance, grEngine *Engine );
grBoolean	GRCC AttachSoundSystem( void * Instance, grSound_System *SoundSystem );
grBoolean	GRCC DettachSoundSystem( void * Instance, grSound_System *SoundSystem );
grBoolean	GRCC Collision(const void *Object, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane);
grBoolean GRCC GetExtBox(const void * Instance,grExtBox *BBox);

void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *);
grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *);

grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List);
grBoolean	GRCC SetProperty(void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );
grBoolean	GRCC GetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData );
grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF);
grBoolean GRCC GRCC GetXForm(const void * Instance,grXForm3d *XF);
int	GRCC GetXFormModFlags( const void * Instace );
grBoolean GRCC GetChildren(const void * Instance,grObject * Children,int MaxNumChildren);
grBoolean GRCC AddChild(void * Instance,const grObject * Child);
grBoolean GRCC RemoveChild(void * Instance,const grObject * Child);
#ifdef WIN32
grBoolean GRCC EditDialog (void * Instance,HWND Parent);
#endif

#ifdef BUILD_BE
grBoolean GRCC EditDialog (void * Instance, class G3DView* Parent );
#endif

grBoolean GRCC SendMsg(void * Instance, int32 Msg, void * Data);
grBoolean GRCC PortalFrame( void * Instance, grFloat Time);
// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane);