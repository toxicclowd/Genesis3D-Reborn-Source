/*!
	@file grBSP.h 
	
	@author John Pollard
	@brief Binary Space Partition Implementation

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

/*!  @note  BSP
*	This object represent the Binary Space Partition of a world. This is the basic entity of the rendering engine.
*/
#ifndef JEBSP_H
#define JEBSP_H

#include "Engine.h"

#include "grBrush.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grLight.h"
#include "grChain.h"
#include "BaseType.h"
#include "grTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// This looks like a big mess.  But these guys are sticking together for life.
// Might as well be in the same file, for speed!!!
typedef struct grWorld grWorld;
typedef struct grEngine grEngine;

/*! @brief Represent the Binary Space Partition data handler */
typedef struct grBSP grBSP;
typedef struct grBSP grBSP;				// Tree
/*! @brief Represent the Brush representation in the BSP */
typedef struct grBSP_Brush			grBSP_Brush;		// Brushes
typedef struct grBSP_Side			grBSP_Side;			// Brush sides
typedef struct grBSP_TopSide		grBSP_TopSide;		// Top level brush sides
typedef struct grBSP_TopBrush		grBSP_TopBrush;		// Top level brushes

typedef struct grBSPNode			grBSPNode;			// Node (subspace seperators)
typedef struct grBSPNode_Portal	grBSPNode_Portal;	// Portal (passage from leaf to leaf)
typedef struct grBSPNode_Face		grBSPNode_Face;		// Node face during construction
typedef struct grBSPNode_DrawFace	grBSPNode_DrawFace;	// Face that gets rendered on nodes
typedef struct grBSPNode_Area		grBSPNode_Area;

typedef void			grBSPNode_DrawFaceCB(const grTLVertex *Verts, int32 NumVerts, void *Context);

typedef enum
{
	Logic_Lazy=0,		//!< No optimizations
	Logic_Normal=1,		//!< Does num splits/balance test
	Logic_Smart=2,		//!< Does num splits/balance, and test volumes
	Logic_Super=3		//!< Final, does splits/balance, test voumes, csg, and solid fill
} grBSP_Logic;
typedef grBSP_Logic grBSP_Logic;

// Render/Build Options
typedef enum
{
	RenderMode_Lines,					//<! Renders using lines only
	RenderMode_Flat,					//<! Renders using unique color per brush face
	RenderMode_BSPSplits,				//<! Renders using unique color per drawface
	RenderMode_Textured,				//<! Renders using material assigned/no lighting
	RenderMode_TexturedAndLit,			//<! Renders using materials assigned, with lighting applied
} grBSP_RenderMode;
typedef grBSP_RenderMode grBSP_RenderMode;

typedef int32 grBSP_LogicBalance;		//<! (0...10), 0 = Less splits, 10 = Balanced tree
typedef grBSP_LogicBalance grBSP_LogicBalance;

/*! @typedef grBSP_Options
	@brief grBSP_Options is the type of Options parameters of function grBSP_RebuildGeometry()<br>
 */
typedef uint32 grBSP_Options;
typedef grBSP_Options grBSP_Options;

#define BSP_OPTIONS_MAKE_VIS_AREAS		(1<<0)
#define BSP_OPTIONS_CSG_BRUSHES			(1<<1)
#define BSP_OPTIONS_SOLID_FILL			(1<<3)

typedef struct grBSP_DebugInfo grBSP_DebugInfo;


struct grBSP_DebugInfo
{
	int32			NumBrushes;				// Total brushes in bsp
	int32			NumVisibleBrushFaces;
	int32			NumTotalBrushFaces;
	int32			NumNodes;
	int32			NumLeafs;
	int32			NumSplits;
	int32			NumDrawFaces;
	int32			NumSubdividedDrawFaces;
	int32			NumPortals;
	int32			NumVisPortals;
	int32			NumAreas;				// Number of VisAreas
	int32			NumVisibleAreas;	
	int32			NumMakeFaces;
	int32			NumMergedFaces;
};

//================================================================================================
//	Build/Rebuild
//================================================================================================
/*! @brief Create the BSP object
*/
grBSP			*grBSP_Create(void);

/*! @brief Rebuild the geomerty of the BSP with the brushes geometry specified in BrushChain

	@param[in] BSP The BSP instance on input of the BSP build algorithm
	@param[in] BrushChain The list of the brushes 
	@param[in] Options The BSP build algorithm options. This parameter can be a combination of BSP_Options. See Remarks
	@param[in] Logic   The BSP build algorithm logic.
	@param[in] LogicBalance The BSP build algorithm balance between logic request and complexity of the geometry.
	@return The BSP instance of the result of the BSP build algorithm if succeed, NULL otherwise

    @par Remarks
	@par Options
			The BSP_Options can be combined:
			<table border=1 bordercolor=#000000 cellspacing=0 cellpadding=2><tr><th width="40%">Value</th><th width="60%">Meaning</th></tr>
			<tr valign=top><td>BSP_OPTIONS_MAKE_VIS_AREAS</td><td>Indicate BSP rebuild to take care od the VIS area brushes. BSP splitter will create areas.</td></tr>
			<tr valign=top><td>BSP_OPTIONS_CSG_BRUSHES</td><td>Indicate BSP to build brushes</td></tr>
			<tr valign=top><td>BSP_OPTIONS_SOLID_FILL</td><td>&nbsp;</td></tr>
			</table>
 */
grBSP			*grBSP_RebuildGeometry(	grBSP				*BSP,
										grChain				*BrushChain, 
										grBSP_Options		Options,
										grBSP_Logic			Logic, 
										grBSP_LogicBalance	LogicBalance);

/*! @brief Destroy the BSP instance
    @param BSPTree The address of the BSP instance pointer
    
    Decrement the reference counter of the BSP instance and destroy it only when the counter reach 0.
    @note The address is set to NULL when returns
*/
void			grBSP_Destroy(grBSP **BSPTree);

grBoolean		grBSP_SetArrays(grBSP *BSP, grFaceInfo_Array *FArray, grMaterial_Array *MArray, grChain *LChain, grChain *DLChain);

grBoolean		grBSP_AddBrush(grBSP *BSPTree, grBrush *Brush, grBoolean AutoLight);
grBoolean		grBSP_HasBrush(grBSP *BSPTree, grBrush *Brush);
grBoolean		grBSP_RemoveBrush(grBSP *BSPTree, grBrush *Brush);

grBoolean		grBSP_MakeVisAreas(grBSP *BSPTree);
grBoolean		grBSP_DestroyVisAreas(grBSP *BSP);

grBoolean		grBSP_AddObject(grBSP *BSP, grObject *Object);
grBoolean		grBSP_RemoveObject(grBSP *BSP, grObject *Object);
grBoolean		grBSP_HasObject(grBSP *BSP, grObject *Object);

//================================================================================================
//	Render/Vis
//================================================================================================
grBoolean		grBSP_VisFrame(grBSP *BSPTree, const grCamera *Camera, const grFrustum *ModelSpaceFrustum);
grBoolean		grBSP_RenderFrontToBack(grBSP *Tree, grCamera *Camera, grFrustum *CameraSpaceFrustum, grFrustum *ModelSpaceFrustum, grXForm3d *ModelToCameraXForm);
grBoolean		grBSP_RenderAndVis(grBSP *Tree, grCamera *Camera, grFrustum *Frustum);
grBoolean		grBSP_RenderAreas(grBSP *Tree, grCamera *Camera, grFrustum *CameraSpaceFrustum, grFrustum *ModelSpaceFrustum, grXForm3d *ModelToCameraXForm);
#ifdef AREA_DRAWFACE_TEST
grBoolean 		grBSP_MakeAreaDrawFaces(grBSP *BSP);
#endif

//================================================================================================
// Update lights/faces/brushes
//================================================================================================
grBoolean		grBSP_UpdateBrush(grBSP *BSPTree, grBrush *Brush, grBoolean AutoLight);
grBoolean		grBSP_UpdateBrushFace(grBSP *BSP, const grBrush_Face *Face, grBoolean AutoLight);

grBoolean		grBSP_PatchLighting(grBSP *Tree);
grBoolean		grBSP_RebuildLights(grBSP *Tree);
grBoolean		grBSP_RebuildLightsFromPoint(grBSP *Tree, const grVec3d *Pos, grFloat Radius);

grBoolean		grBSP_UpdateAll(grBSP *BSP);
grBoolean		grBSP_RebuildFaces(grBSP *BSPTree);

grBoolean		grBSP_SetBrushFaceCB(grBSP *BSP, grBSPNode_DrawFaceCB *CB, void *Context);
grBoolean		grBSP_SetBrushFaceCBOnOff(grBSP *BSP, const grBrush_Face *Face, grBoolean OnOff);

//================================================================================================
//	Misc
//================================================================================================
grBoolean		grBSP_SetXForm(grBSP *BSP, const grXForm3d *XForm);
grBoolean		grBSP_SetEngine(grBSP *Tree, grEngine *Engine);
grBoolean		grBSP_SetWorld(grBSP *Tree, grWorld *World);
grBoolean		grBSP_SetRenderMode(grBSP *BSP, grBSP_RenderMode RenderMode);
grBoolean		grBSP_SetDefaultContents(grBSP *BSP, grBrush_Contents DefaultContents);
grBSPNode_Area	*grBSP_FindArea(grBSP * BSP, const grVec3d *Pos);

typedef void (* grBSP_DoAreaFunc) ( grBSPNode_Area * Area, void * Context );
typedef grBSP_DoAreaFunc grBSP_DoAreaFunc;
grBoolean		grBSP_DoAllAreasInBox(grBSP *BSP,grExtBox *BBox,grBSP_DoAreaFunc CB,void * Context);


grBoolean		grBSP_GetModelSpaceBox(const grBSP *BSP, grExtBox *Box);
grBoolean		grBSP_GetWorldSpaceBox(const grBSP *BSP, grExtBox *Box);

// Icestorm : If Impact(Box) or Plane are NULL: Tests only, whether there is an collision (it's a bit faster)
grBoolean		grBSP_Collision(const grBSP *BSP, 
								const grExtBox *Box, 
								const grVec3d *Front, 
								const grVec3d *Back, 
								grVec3d *Impact, 
								grPlane *Plane);

// Added by Icestorm
grBoolean		grBSP_ChangeBoxCollision(	const grBSP		*BSP, 
											const grVec3d	*Pos,
											const grExtBox	*FrontBox, 
											const grExtBox	*BackBox, 
											grExtBox		*ImpactBox, 
											grPlane			*Plane);

grBoolean		grBSP_RayIntersectsBrushes(const grBSP *BSP, const grVec3d *Front, const grVec3d *Back, grBrushRayInfo *Info);

const			grBSP_DebugInfo *grBSP_GetDebugInfo(const grBSP *BSPTree);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
