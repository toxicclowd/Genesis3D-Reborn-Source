/****************************************************************************************/
/*  grModel.H                                                                           */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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

#ifndef JEMODEL_H
#define JEMODEL_H

#include "grBrush.h"
#include "BaseType.h"
#include "VFile.h"
#include "grLight.h"
#include "grChain.h"
#include "grPtrMgr.h"
#include "grBSP.h"

#ifdef __cplusplus
extern "C" {
#endif

//**********
typedef struct grEngine		grEngine;
typedef struct grCamera		grCamera;
typedef struct grFrustum	grFrustum;
typedef struct grWorld		grWorld;
typedef struct grObject		grObject;
typedef struct grMaterial_Array grMaterial_Array;
typedef struct grFaceInfo_Array grFaceInfo_Array;
typedef struct grBrushRayInfo grBrushRayInfo;
typedef struct grBSP_DebugInfo grBSP_DebugInfo;

typedef struct grModel grModel;
typedef struct grBSPSetup { // Incarnadine
	grBSP_Options Options;
	grBSP_Logic Logic;
	grBSP_LogicBalance LogicBalance;
} grBSPSetup;
typedef struct grBSPSetup grBSPSetup;


GRAPI grModel		* GRCC grModel_Create(void);
GRAPI grBoolean	GRCC grModel_WriteToFile(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr);
GRAPI grModel		* GRCC grModel_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
GRAPI grBoolean	GRCC grModel_CreateRef(grModel *Model);
GRAPI void			GRCC grModel_Destroy(grModel **Model);
GRAPI void			GRCC grModel_SetArrays(grModel *Model, grFaceInfo_Array *FArray, grMaterial_Array *Marray, grChain *LChain, grChain *DLChain);
GRAPI grBoolean	GRCC grModel_AddBrush(grModel *Model, grBrush *Brush, grBoolean Update, grBoolean AutoLight);
GRAPI grBoolean	GRCC grModel_RemoveBrush(grModel *Model, grBrush *Brush, grBoolean Update);
GRAPI grBoolean	GRCC grModel_UpdateBrush(grModel *Model, grBrush *Brush, grBoolean AutoLight);
GRAPI grBrush		* GRCC grModel_GetNextBrush(const grModel *Model, const grBrush *Start);
GRAPI grBoolean	GRCC grModel_HasBrush(const grModel *Model, const grBrush *Brush);

GRAPI grBoolean	GRCC grModel_AddObject(grModel *Model, grObject *Object);
GRAPI grBoolean	GRCC grModel_RemoveObject(grModel *Model, grObject *Object);

GRAPI grBoolean	GRCC grModel_UpdateBrushFace(grModel *Model, const grBrush_Face *Face, grBoolean AutoLight);
GRAPI grBoolean	GRCC grModel_RebuildLightsFromPoint(grModel *Model, const grVec3d *Pos, grFloat Radius);

GRAPI grBoolean	GRCC grModel_RebuildBSP(	grModel *Model, 
								grBSP_Options Options, 
								grBSP_Logic Logic, 
								grBSP_LogicBalance LogicBalance);
GRAPI grBoolean	GRCC grModel_RebuildBSPFaces(grModel *Model);
GRAPI grBoolean	GRCC grModel_PatchLighting(grModel *Model);
GRAPI grBoolean	GRCC grModel_RebuildLights(grModel *Model);
GRAPI grBoolean	GRCC grModel_SetEngine(grModel *Model, grEngine *Engine);
GRAPI grBoolean	GRCC grModel_SetWorld(grModel *Model, grWorld *World);
GRAPI grBoolean	GRCC grModel_SetRenderOptions(grModel *Model, grBSP_RenderMode RenderMode);
GRAPI grBoolean	GRCC grModel_SetDefaultContents(grModel *Model, grBrush_Contents DefaultContents);
GRAPI grBoolean	GRCC grModel_Render(grModel *Model, grCamera *Camera, grFrustum *CameraSpaceFrustum);

GRAPI grBoolean	GRCC grModel_RayIntersectsBrushes(const grModel *Model, const grVec3d *Front, const grVec3d *Back, grBrushRayInfo *Info);

GRAPI grBoolean	GRCC grModel_SetBrushFaceCB(grModel *Model, grBSPNode_DrawFaceCB *CB, void *Context);
GRAPI grBoolean	GRCC grModel_SetBrushFaceCBOnOff(grModel *Model, const grBrush_Face *Face, grBoolean OnOff);

// Temporary??
GRAPI const		grBSP_DebugInfo *GRCC grModel_GetBSPDebugInfo(const grModel *Model);

GRAPI grBoolean	GRCC grModel_Collision(	const grModel	*Model, 
											const grExtBox	*Box, 
											const grVec3d	*Front, 
											const grVec3d	*Back, 
											grVec3d			*Impact, 
											grPlane			*Plane);

GRAPI grBoolean	GRCC grModel_ChangeBoxCollision(	const grModel	*Model, 
												const grVec3d	*Pos, 
												const grExtBox	*FrontBox, 
												const grExtBox	*BackBox, 
												grExtBox		*ImpactBox, 
												grPlane			*Plane);

GRAPI grBoolean	GRCC grModel_SetXForm(grModel *Model, const grXForm3d *XForm);
GRAPI const		grXForm3d * GRCC grModel_GetXForm(grModel *Model );

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
