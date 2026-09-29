/*!
	@file TERRAIN.H

	@author Charles Bloom
	@brief  Terrain object interface

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

#ifndef GR_TERRAIN_H
#define GR_TERRAIN_H

#include "grTypes.h"
#include "VFile.h"
#include "Bitmap.h"
#include "grFrustum.h"
#include "Object.h"
#include "grPtrMgr.h"
#include "grLight.h"

//#include "engine.h"  // for grEngine
typedef struct grEngine	grEngine;

#ifdef __cplusplus
extern "C" {
#endif


typedef struct grTerrain		grTerrain;

enum
{
	TERRAIN_PROPERTY_HEIGHTMAP = PROPERTY_LOCAL_DATATYPE_START,
	TERRAIN_PROPERTY_MAXQUADS,
	TERRAIN_PROPERTY_MINERROR,
	TERRAIN_PROPERTY_TEXDIMLOG2,
//	TERRAIN_PROPERTY_VERTEXLIGHTING,
//	TERRAIN_PROPERTY_TEXTURELIGHTING,
//	TERRAIN_PROPERTY_NOLIGHTING,
	TERRAIN_PROPERTY_LIGHTING_LIST,
	TERRAIN_PROPERTY_SIZE,
	TERRAIN_PROPERTY_SIZE_X,
	TERRAIN_PROPERTY_SIZE_Y,
	TERRAIN_PROPERTY_SIZE_Z,
};

/*******************************************************/

GRAPI void *	GRCC grTerrain_Create(void);
GRAPI void *	GRCC grTerrain_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr);
GRAPI grTerrain *	GRCC grTerrain_CreateFromFileExt(grVFile * File,grVFile *ResourceBaseFS,grPtrMgr *PtrMgr);
GRAPI grBoolean 	GRCC grTerrain_Destroy(void ** ppTerrain);
GRAPI void 		GRCC grTerrain_CreateRef(void * pTerrain);

GRAPI grBoolean 	GRCC grTerrain_WriteToFile(const void *pTerrain,grVFile * File, grPtrMgr *PtrMgr);

/*******************************************************/

// NOTEZ : GetHeight and GetNormal are in "Terrain Space", a 1x1x1 cube

GRAPI grFloat		GRCC grTerrain_GetHeightAtXY(const grTerrain *T,grFloat X,grFloat Y);
GRAPI void 		GRCC grTerrain_GetNormalAtXY(const grTerrain *T,grFloat X,grFloat Y,grVec3d *pNormal);

GRAPI void			GRCC grTerrain_GetNormalAtXY_Rough(const grTerrain *Terrain,grFloat X,grFloat Y,grVec3d *pNormal);

GRAPI grFloat		GRCC grTerrain_GetHeightAtWorldSpaceVec(const grTerrain *Terrain,const grVec3d *pV);
GRAPI void			GRCC grTerrain_GetNormalAtWorldSpaceVec(const grTerrain *Terrain,const grVec3d *pV,grVec3d *pN);

GRAPI grBoolean	GRCC grTerrain_GetExtBox(const void *T,grExtBox * pBox);

/*******************************************************/

GRAPI grBoolean	GRCC grTerrain_IntersectsRay(const grTerrain *T,grVec3d *pStart,grVec3d *pDirection);

//GRAPI grBoolean	GRCC grTerrain_ExtBoxIsVis(const grTerrain *T,const grExtBox *pBox,const grCamera *pCamera);

GRAPI grBoolean GRCC grTerrain_SphereCollision(const grTerrain *T,
													const grVec3d *pFrom, const grVec3d *pTo, grFloat Radius,
													grVec3d *Impact, grPlane *Plane);

GRAPI grBoolean GRCC grTerrain_BoxCollision(const void *T, const grExtBox *Box, 
													const grVec3d *Front, const grVec3d *Back, 
													grVec3d *Impact, grPlane *Plane);

/*******************************************************/

GRAPI grBoolean	GRCC grTerrain_SetSize(grTerrain * T,grVec3d * pSize);

GRAPI grBoolean	GRCC grTerrain_SetXForm(void * T,const grXForm3d *pXF);
GRAPI grBoolean	GRCC grTerrain_GetXForm(const void * T,grXForm3d *pXF);

GRAPI grBoolean	GRCC grTerrain_SetHeightmap(grTerrain * T,grBitmap * Bmp);
GRAPI grBoolean	GRCC grTerrain_GetHeightmap(grTerrain * T,grBitmap ** pBmp);

GRAPI grBoolean	GRCC grTerrain_SetParameters(grTerrain *T,uint32 MaxQuads,float MinError);
														// 250 and 0.005 are reasonable numbers here

GRAPI grBoolean	GRCC grTerrain_SetLightsOnVertsFromWorld(grTerrain *T,grWorld *World);
										// LightsOnVerts & DLights are mutually exclusive (DLights overwrite vert lights)

GRAPI grBoolean GRCC grTerrain_SetLightsOnVerts(grTerrain *T, grLight **SrcLights, int Count);

GRAPI grBoolean	GRCC grTerrain_SetATexture(grTerrain *T,const grBitmap * Bmp,int tx,int ty);
										// tx & ty in [0,TexDim-1]

GRAPI grBoolean	GRCC grTerrain_GetTextureAtXY(const grTerrain *T,grFloat X,grFloat Y,const grBitmap ** pBmp,int *pTX,int *pTY);
GRAPI grBoolean	GRCC grTerrain_GetTextureAtWorldVec(const grTerrain *T,const grVec3d *pVec,const grBitmap ** pBmp,int *pTX,int *pTY);
									// fills out pBmp,pTX,pTY unless null; tx and ty are texture x & t's for use in SetATexture

GRAPI grBoolean	GRCC grTerrain_SetTexDim(grTerrain *T,int TexDim);
									// TexDim must be a power of two <= 8 (eg. 1,2,4,8)

/*******************************************************/

GRAPI grBoolean	GRCC grTerrain_RenderPrep(grTerrain * T,grEngine *Engine,grCamera *C);

GRAPI grBoolean	GRCC grTerrain_RenderThroughCamera( grTerrain *T,const grWorld *World, const grEngine *E,grCamera *Camera);

GRAPI grBoolean	GRCC grTerrain_RenderThroughFrustum(grTerrain *T,const grWorld *World, const grEngine *E,grCamera *Camera, const grFrustum *F);

// you can render & whatnot without attaching & detaching if you like
GRAPI grBoolean	GRCC grTerrain_AttachEngine(void *T,grEngine *Engine);
GRAPI grBoolean	GRCC grTerrain_DetachEngine(void *T,grEngine *Engine);
				   
/*******************************************************/

GRAPI void			GRCC grTerrain_InitObject(const grTerrain *T,grObject *O);

GRAPI grBoolean	GRCC grTerrain_RegisterObjectDef(void);

/*******************************************************/

/*!
	@fn void grTerrain_SetRenderNextTime(grTerrain* T, grBoolean RenderNext)
	@brief Enable/diable rendering in the next Object render loop

	@param[in] T The terrain object the flag applies
	@param[in] RenderNext The next rendering flag value (GR_TRUE means rendered, GR_FALSE means ignored)

	@author Bruno Pettorelli (krouer@jet3d.com)
    @see grObject_SetRenderNextPass
*/
GRAPI void			GRCC grTerrain_SetRenderNextTime(void* T, grBoolean RenderNext);

#ifdef __cplusplus
}
#endif

#endif // GR_TERRAIN_H


