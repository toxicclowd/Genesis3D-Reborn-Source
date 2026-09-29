/**
   @file grStaticMesh.h                                                                       
                                                                                      
   @author Anthony Rufrano	                                                          
   @brief Static mesh code     		                                          
                                                                                      
	@par Licence
	The contents of this file are subject to the Genesis3D: Reborn Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.genesis3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Genesis3D: Reborn, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/
#ifndef GR_STATIC_MESH_H
#define GR_STATIC_MESH_H

#include "BaseType.h"
#include "grTypes.h"
#include "XForm3d.h"
#include "Vec3d.h"
#include "VFile.h"
#include "grResource.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jeCamera grCamera;
typedef struct jeEngine grEngine;
typedef struct jeFrustum							grFrustum;
typedef struct jeExtBox grExtBox;
typedef struct jePlane								grPlane;

/*!
	@typedef grStaticMesh
	@brief A static mesh
*/
typedef struct jeStaticMesh grStaticMesh;
typedef struct jeStaticMesh jeStaticMesh;

/*!
	@fn grStaticMesh *grStaticMesh_Create(const char *Name)
	@brief Creates a static mesh
	@param[in] MeshName Name of the static mesh (Directory/PAK.FileName (no extension))
	@param[in] ResMgr The resource manager
	@return The new mesh
*/
GRAPI grStaticMesh * GRCC grStaticMesh_Create(const char *MeshName, grResourceMgr *ResMgr);

/*!
	@fn uint32 grStaticMesh_Destroy(grStaticMesh **Mesh)
	@brief Decrements the reference counter to the mesh.  If it's 0, the mesh is destroyed
	@param[in] Mesh The mesh to dereference
	@return The number of references to the mesh
*/
GRAPI uint32 GRCC grStaticMesh_Destroy(grStaticMesh **Mesh);

/*!
	@fn uint32 grStaticMesh_CreateRef(grStaticMesh *Mesh)
	@brief Increases the reference count of the mesh
	@param[in] Mesh The mesh to reference
	@return The number of references to the mesh
*/
GRAPI uint32 GRCC grStaticMesh_CreateRef(grStaticMesh *Mesh);

/*!
	@fn grBoolean grStaticMesh_Render(grStaticMesh *Mesh, grEngine *Engine, grCamera *Camera, grFrustum *Frustum, grXForm3d *XForm)
	@brief Renders the mesh at a given location
	@param[in] Mesh The mesh to render
	@param[in] Engine The engine to render with
	@param[in] Camera The camera to render through
	@param[in] Frustum The frustum to cull with
	@param[in] XForm The location to render to
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grStaticMesh_Render(grStaticMesh *Mesh, grEngine *Engine, grCamera *Camera, grFrustum *Frustum, grXForm3d *XForm);

/*!
	@fn grBoolean grStaticMesh_GetExtBox(grStaticMesh *Mesh, grExtBox *BBox)
	@brief Gets the mesh's bounding box
	@param[in] Mesh The mesh to get the box from
	@param[out] BBox The mesh's bounding box
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grStaticMesh_GetExtBox(grStaticMesh *Mesh, grExtBox *BBox);

/*!
	@fn grBoolean grStaticMesh_SetExtBox(grStaticMesh *Mesh, grExtBox *BBox)
	@brief Sets the mesh's bounding box
	@param[in] Mesh The mesh to set the bounding box for
	@param[in] BBox The bounding box to set
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grStaticMesh_SetExtBox(grStaticMesh *Mesh, grExtBox *BBox);

/*!
	@fn grBoolean grStaticMesh_Collision(grStaticMesh *Mesh, grExtBox *BBox, grVec3d *Front, grVec3d *Back, grVec3d *Impact, grPlane *Plane)
	@brief Performs collision testing on the mesh
	@param[in] Mesh The mesh to test
	@param[in] BBox The mesh's bounding box
	@param[in] Front The forward vector to test against
	@param[in] Back The back vector to test against
	@param[out] Impact The point of impact
	@param[out] Plane The plane at which the collision occurred
	@return GR_TRUE if there was a collision, GR_FALSE if not
*/
GRAPI grBoolean GRCC grStaticMesh_Collision(grStaticMesh *Mesh, grExtBox *BBox, grVec3d *Front, grVec3d *Back, grVec3d *Impact, grPlane *Plane);

#ifdef __cplusplus
}
#endif

// End of header

//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define jeStaticMesh_Collision                   grStaticMesh_Collision
#define jeStaticMesh_Create                      grStaticMesh_Create
#define jeStaticMesh_CreateRef                   grStaticMesh_CreateRef
#define jeStaticMesh_Destroy                     grStaticMesh_Destroy
#define jeStaticMesh_GetExtBox                   grStaticMesh_GetExtBox
#define jeStaticMesh_Render                      grStaticMesh_Render
#define jeStaticMesh_SetExtBox                   grStaticMesh_SetExtBox

#endif // GENESIS_NO_JET_COMPAT

#endif
