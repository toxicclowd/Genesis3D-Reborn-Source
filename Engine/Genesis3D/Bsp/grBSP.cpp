/****************************************************************************************/
/*  JEBSP.C                                                                             */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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
#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef BUILD_BE
#define min(a,b) (((a)<(b))?(a):(b))
#endif
 
// Private dependents
#include "grBSP._h"
#include "Ram.h"
#include "Vec3d.h"
#include "Errorlog.h"
#include "Log.h"

#include "math.h"
#include "Camera._h"

// Public dependents
#include "grBSP.h"

static void grBSP_GpuGeometryDestroy(grBSP *BSP);

#ifdef _DEBUG
	#define GR_BSP_DEBUG_OUTPUT_LEVEL		1
	//#define GR_BSP_DEBUG_OUTPUT_LEVEL		2
#else
	#define GR_BSP_DEBUG_OUTPUT_LEVEL		0
#endif

//--- Local global statics
static grBSP_Logic			g_Logic;
static grBSP_LogicBalance	g_LogicBalance;		// (0..10), 0 = Less splits, 10 = Balanced tree
static int32				NumNonVisNodes;

// extern data for stats purpose - Krouer
extern int32 NumMakeFaces;
extern int32 NumMergedFaces;
extern int32 NumSubdividedFaces;

//=======================================================================================
// Local Static function prototypes (ALL Static functions at BOTTOM of file)
//=======================================================================================
static grBoolean	grBSP_AddBSPBrush_r(grBSP *BSP, grBSPNode *Node, grBSP_Brush **Brush);
static grBoolean	grBSP_AddBrushInternally(grBSP *BSPTree, grBrush *Brush, uint32 Order, grBoolean AutoLight);
static grBoolean	grBSP_BuildBSPTree(	grBSP *BSPTree, 
										grBSP_Brush **BrushList, 
										grBSP_Logic Logic, 
										grBSP_LogicBalance LogicBalance);
static grBoolean	grBSP_BuildBSPTree_r(grBSP *BSP, grBSPNode *Node, grBSP_Brush **Brushes);
static grPlane_Side grBSP_TestBrushToPlaneIndex(grBSP *BSP, grBSP_Brush *Brush, grPlaneArray_Index Index, int32 *NumSplits, 
												grBoolean *HintSplit, int32 *EpsilonBrush);

static grBSP_Side	*grBSP_GetSplitter(grBSP *BSP, grBSP_Brush *Brushes, grBSPNode *Node);
static grBoolean	grBSP_CheckPlaneAgainstVolume(grBSP *BSP, grPlaneArray_Index Index, grBSPNode *Node);
static grBoolean	grBSP_CheckPlaneAgainstParents(grPlaneArray_Index Index, grBSPNode *Node);
static grBoolean	grBSP_FillLeafsFromEntities(grBSP *Tree, int32 Fill);
static grBoolean	grBSP_RemoveHiddenLeafs(grBSP *Tree);
static grBoolean	grBSP_MarkVisibleTopSides(grBSP *Tree);
static grBoolean	grBSP_CreatePortals(grBSP *Tree, grBoolean IncludeDetail);
static void			grBSP_DestroyAllPortals(grBSP *Tree);
static void			grBSP_MarkVisibleTopSides_r(grBSP *BSP, grBSPNode *Node);

#define LIGHT_FRACT			8
#define CSCALE				(1.0f/195.0f)
//#define COLOR_TO_FIXED(c)	((int32)(((c)*CSCALE)*(1<<LIGHT_FRACT)))
#define COLOR_TO_FIXED(c)	((int32)((c)*(1<<LIGHT_FRACT)))

static grBoolean	CombineDLightWithRGBMapFastLightingModel(grBSP *BSP, int32 *LightData, grBSPNode_Light *Light, grBSPNode_DrawFace *Face);
static grBoolean	CombineDLightWithRGBMap(grBSP *BSP, int32 *LightData, grBSPNode_Light *Light, grBSPNode_DrawFace *Face);
static void			AddLightType1(int32 *LightDest, uint8 *LightData, int32 Size, int32 Intensity);
static void			AddLightType2(int32 *LightDest, uint8 *LightData, int32 Size, int32 Intensity);
static void	GRCC	grBSP_SetupLightmap(grRDriver_LMapCBInfo *Info, void *LMapCBContext);
static grBoolean	UpdateDLights(grBSP *BSP);
static grBoolean	UpdateObjects(grBSP *BSP);

static grBoolean	GRCC ShutdownDriverCB(DRV_Driver *Driver, void *Context);
static grBoolean	GRCC StartupDriverCB(DRV_Driver *Driver, void *Context);

static grBoolean	grBSP_UpdateWorldSpaceBox(grBSP *BSP);

static grBoolean	grBSP_CreateInternalArrays(grBSP *BSP);
static void			grBSP_DestroyInternalArrays(grBSP *BSP);
static void			grBSP_ResetObjects(grBSP *BSP);
static grBoolean	grBSP_ResetGeometry(grBSP *BSP);
static void			grBSP_DestroyExternalArrays(grBSP *BSP);
static grBoolean	grBSP_CreateExternalArrays(grBSP *BSP, grFaceInfo_Array *FArray, grMaterial_Array *MArray, grChain *LChain, grChain *DLChain);

static grBoolean	grBSP_OptimizeDrawFaceVerts(grBSP *BSP);
static grBoolean	grBSP_CreateVertexBuffer(grBSP* BSP);
static grBoolean	grBSP_RenderVertexBuffer(grBSP* BSP);

//=======================================================================================
//	grActor_SetRenderNextTime
//	An accessor the actor render flag
//=======================================================================================
extern GRAPI void GRCC grActor_SetRenderNextTime(grActor* Actor, grBoolean RenderNextTime);


//=======================================================================================
//	grBSP_Create 
//	Create an "empty" BSP tree
//=======================================================================================
grBSP *grBSP_Create(void)
{
	grBSP		*BSPTree;
	grXForm3d	IdentityXForm;

	BSPTree = GR_RAM_ALLOCATE_STRUCT(grBSP);

	if (!BSPTree)
		return NULL;

	ZeroMem(BSPTree);

#ifdef AREA_DRAWFACE_TEST
	LN_InitList(&(BSPTree->AreaList));
	LN_InitList(&(BSPTree->LeafList));
#endif

	if (!grBSP_CreateInternalArrays(BSPTree))
		goto ExitWithError;

	BSPTree->BSPObjectChain = grChain_Create();

	if (!BSPTree->BSPObjectChain)
		goto ExitWithError;

	// Set default XForm
	grXForm3d_SetIdentity(&IdentityXForm);
	grBSP_SetXForm(BSPTree, &IdentityXForm);

	// Set other default stuff
	BSPTree->RenderMode = RenderMode_TexturedAndLit;
	BSPTree->DefaultContents = GR_BSP_CONTENTS_SOLID;

	return BSPTree;

	// Error
	ExitWithError:
	{
		if (BSPTree)
		{
			if (BSPTree->BSPObjectChain)
				grChain_Destroy(&BSPTree->BSPObjectChain);

			grRam_Free(BSPTree);
		}

		return NULL;
	}
}

//=======================================================================================
//	grBSP_RebuildGeometry
//	Creates a fresh tree from a list of brushes
//=======================================================================================
grBSP *grBSP_RebuildGeometry(	grBSP				*BSP,
								grChain				*BrushChain, 
								grBSP_Options		Options,
								grBSP_Logic			Logic, 
								grBSP_LogicBalance	LogicBalance)
{
	grBSP_Brush		*BSPBrushList;

	assert(BSP);
	assert(BrushChain);

	// Destroy any geometry the BSP might have, and reset arrays
	if (!grBSP_ResetGeometry(BSP))
		return NULL;

	BSPBrushList = NULL;

	// Set some globals
	g_Logic = Logic;
	g_LogicBalance = LogicBalance;

	BSP->NumBrushes = 0;

	// Create the TopLevel brushes, from the editor grBrushes...
	BSP->TopBrushes = grBSP_TopBrushCreateListFromBrushChain(BrushChain, BSP, &BSP->NumBrushes);

	if (!BSP->TopBrushes)
		goto ExitWithError;

	// Create the brushes that will get cut up in the tree
	BSPBrushList = grBSP_BrushCreateListFromTopBrushList(BSP->TopBrushes, BSP);
	
	if (!BSPBrushList)
		goto ExitWithError;

	// CSG this list 
	if (Options & BSP_OPTIONS_CSG_BRUSHES)
	{
		BSPBrushList = grBSP_BrushCSGList(BSPBrushList, BSP);

		if (!BSPBrushList)
			goto ExitWithError;
	}

	// Now, build the tree
	if (!grBSP_BuildBSPTree(BSP, &BSPBrushList, Logic, LogicBalance))
		goto ExitWithError;

	// Create portals
	if (!grBSP_CreatePortals(BSP, GR_TRUE))
		goto ExitWithError;
		
	// Remove hidden leafs
	//if (!grBSP_RemoveHiddenLeafs(BSP))
	//	goto ExitWithError;

	// Use the tree to mark the visible sides
	if (!grBSP_MarkVisibleTopSides(BSP))
		goto ExitWithError;

	// Merge nodes
	//grBSPNode_MergeLeafs(BSP->RootNode);

	// Make collision hulls for leafs
	if (!grBSPNode_UpdateLeafSides_r(BSP->RootNode, BSP))
		goto ExitWithError;

	if (Options & BSP_OPTIONS_MAKE_VIS_AREAS)
	{
		if (!grBSPNode_MakeFaces_Callr(BSP->RootNode, BSP, GR_FALSE))
			goto ExitWithError;

		if (!grBSPNode_MakeDrawFaceListOnLeafs_r(BSP->RootNode, BSP))
			goto ExitWithError;

		// We MUST destroy the chain of merged/split faces fragments as soon as we are done with them
		grBSPNode_DestroyBSPFaces_r(BSP->RootNode, BSP, GR_TRUE);
		
		if (!grBSP_MakeVisAreas(BSP))
			goto ExitWithError;

#ifdef AREA_DRAWFACE_TEST
		if ( 1 ) // @@
		{
			if (!grBSP_MakeAreaDrawFaces(BSP))
				goto ExitWithError;
		}
#endif
	}
	else
	{
		if (!grBSPNode_MakeFaces_Callr(BSP->RootNode, BSP, GR_TRUE))
			goto ExitWithError;
	}

#ifdef AREA_DRAWFACE_TEST
	Log_Printf("Area List Len = %d\n",LN_ListLen(&(BSP->AreaList)));
#endif

	if (!grBSP_UpdateWorldSpaceBox(BSP))
		return NULL;

	if (!grBSP_OptimizeDrawFaceVerts(BSP))
		return NULL;

// Krouer: put here the call to the Vertex buffer creation function
	if (BSP->Driver) 
	{
		grDeviceCaps devcaps;
		BSP->Driver->GetDeviceCaps(&devcaps);
		if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == GR_RENDER_FLAG_HWTRANSFORM) {
			if (!grBSP_CreateVertexBuffer(BSP)) {
				return NULL;
			}
		}
	}
	return BSP;

	// Error
	ExitWithError:
	{
		if (BSP->RootNode)
			grBSPNode_Destroy_r(&BSP->RootNode, BSP);

		if (BSP->TopBrushes)
			grBSP_TopBrushDestroyList(&BSP->TopBrushes, BSP);

		if (BSPBrushList)
			grBSP_BrushDestroyList(&BSPBrushList);

		return NULL;
	}
}

//=======================================================================================
//	grBSP_Destroy
//=======================================================================================
void grBSP_Destroy(grBSP **BSPTree)
{
	assert(BSPTree);
	assert(*BSPTree);

	// Release the GPU geometry while the driver is still attached.
	grBSP_GpuGeometryDestroy(*BSPTree);

	// Detach from the engine (if any)
	if ((*BSPTree)->Engine)
	{
		assert((*BSPTree)->ChangeDriverCB);
		grEngine_DestroyChangeDriverCB((*BSPTree)->Engine, &(*BSPTree)->ChangeDriverCB);
		grEngine_Free((*BSPTree)->Engine);
		(*BSPTree)->Engine = NULL;
	}
	else
	{
		assert(!(*BSPTree)->ChangeDriverCB);
		assert(!(*BSPTree)->Driver);
	}

	// First, make sure no portals are in tree
	grBSP_DestroyAllPortals(*BSPTree);	

	// Destroy arrays, and anything that depends on them...
	grBSP_DestroyInternalArrays(*BSPTree);

	// Destroy all externally set arrays
	grBSP_DestroyExternalArrays(*BSPTree);

	// Destroy the BSPOBject list
	if ((*BSPTree)->BSPObjectChain)
	{
		grChain_Link	*Link;

		for (Link = grChain_GetFirstLink((*BSPTree)->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
		{
			grBSP_Object		*BSPObject;

			BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);

			grObject_Destroy(&BSPObject->Object);

			// [MLB-ICE]
			grRam_Free(BSPObject);	// Icestorm: "Container" should be freed!
			// [MLB-ICE] EOB

		}

		grChain_Destroy(&(*BSPTree)->BSPObjectChain);
	}

	// Free the bsp structure
	grRam_Free(*BSPTree);

	// NULL out their pointer
	*BSPTree = NULL;

	// Print some degbue info...
	Log_Printf("--- grBSP_Destroy ---\n");
	Log_Printf("Num Active BSPBrushes  : %5i\n", grBSP_BrushGetActiveCount());
	Log_Printf("Num Peek BSPBrushes    : %5i\n", grBSP_BrushGetPeekCount());
	Log_Printf("Num Active BSPFaces    : %5i\n", grBSPNode_FaceGetActiveCount());
	Log_Printf("Num Peek BSPFaces      : %5i\n", grBSPNode_FaceGetPeekCount());
	Log_Printf("Num Active Portals     : %5i\n", grBSPNode_PortalGetActiveCount());
	Log_Printf("Num Peek Portals       : %5i\n", grBSPNode_PortalGetPeekCount());
}

//=====================================================================================
//	grBSP_SetArrays
//=====================================================================================
grBoolean grBSP_SetArrays(grBSP *BSP, grFaceInfo_Array *FArray, grMaterial_Array *MArray, grChain *LChain, grChain *DLChain)
{
	assert(BSP);

	// Check to see if the arrays changed
	if (BSP->FaceInfoArray == FArray && 
		BSP->MaterialArray == MArray &&
		BSP->LightChain == LChain && 
		BSP->DLightChain == DLChain)
			return GR_TRUE;			// Nothing changed

	// Destroy ALL geometry, and arrays
	if (!grBSP_ResetGeometry(BSP))
		return GR_FALSE;			

	// Destroy all externally set arrays
	grBSP_DestroyExternalArrays(BSP);
		
	if (FArray)
		grBSP_CreateExternalArrays(BSP, FArray, MArray, LChain, DLChain);
	else
	{
		assert(!FArray);		
		assert(!MArray);
		assert(!LChain);
		assert(!LChain);
	}

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_AddBrush
//=====================================================================================
grBoolean grBSP_AddBrush(grBSP *BSPTree, grBrush *Brush, grBoolean AutoLight)
{
	assert(BSPTree);
	assert(Brush);

	if (!grBSP_AddBrushInternally(BSPTree, Brush, BSPTree->NumBrushes, AutoLight))
		return GR_FALSE;

	BSPTree->NumBrushes++;

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_HasBrush
//=====================================================================================
grBoolean grBSP_HasBrush(grBSP *BSPTree, grBrush *Brush)
{
	grBSP_TopBrush		*TopBrush;

	assert(BSPTree);
	assert(Brush);

	if (!BSPTree->RootNode)
		return GR_FALSE;

	for (TopBrush = BSPTree->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
	{
		if (TopBrush->Original == Brush)
			return GR_TRUE;
	}

	return GR_FALSE;
}

//=====================================================================================
//	grBSP_RemoveBrush
//=====================================================================================
grBoolean grBSP_RemoveBrush(grBSP *BSPTree, grBrush *Brush)
{
	grBSP_TopBrush		*TopBrush, *NewList, *Next;
	grBoolean			Found;

	assert(BSPTree);
	assert(Brush);
	assert(grBSP_HasBrush(BSPTree, Brush));

	if (BSPTree->AreaChain)
	{
		if (!grBSP_DestroyVisAreas(BSPTree))
			return GR_FALSE;
	}

	if (!BSPTree->RootNode)
		return GR_FALSE;

	NewList = NULL;
	Found = GR_FALSE;
	
	// Find all the topbrushes that are a apart of the editor brush, and remove them
	for (TopBrush = BSPTree->TopBrushes; TopBrush; TopBrush = Next)
	{
		Next = TopBrush->Next;

		if (TopBrush->Original == Brush)
		{
			Found = GR_TRUE;
			
			if (!grBSPNode_RemoveTopBrush_r(BSPTree->RootNode, BSPTree, TopBrush))
				return GR_FALSE;
			
			grBSP_TopBrushDestroy(&TopBrush, BSPTree);
			continue;
		}

		TopBrush->Next = NewList;
		NewList = TopBrush;
	}

	BSPTree->TopBrushes = NewList;
   	
	if (!Found)
		return GR_FALSE;		// Brush not in tree!!

	BSPTree->UpdateFlags |= BSP_UPDATE_FACES;
	BSPTree->UpdateFlags |= BSP_UPDATE_LIGHTS;

	BSPTree->DebugInfo.NumBrushes--;

	return GR_TRUE;
}

//=======================================================================================
//	AllocateAreaPortals
//=======================================================================================
static grBoolean AllocateAreaPortals(grBSP *BSP)
{
	grChain_Link		*Link;

	for (Link = grChain_GetFirstLink(BSP->AreaChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSPNode_Area	*Area;

		Area = (grBSPNode_Area*)grChain_LinkGetLinkData(Link);

		// If array has not been allocated, then allocate it now...
		assert(!Area->AreaPortals);
		assert(!Area->NumWorkAreaPortals);

		if (!Area->NumAreaPortals)
			continue;

		Area->AreaPortals = GR_RAM_ALLOCATE_ARRAY(grBSPNode_AreaPortal, Area->NumAreaPortals);

		if (!Area->AreaPortals)
			return GR_FALSE;

		ZeroMemArray(Area->AreaPortals, Area->NumAreaPortals);
	}

	return GR_TRUE;
}

//=======================================================================================
//	CheckAreaPortals
//=======================================================================================
static void CheckAreaPortals(grBSP *BSP)
{
	grChain_Link		*Link;

	for (Link = grChain_GetFirstLink(BSP->AreaChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSPNode_Area	*Area;

		Area = (grBSPNode_Area*)grChain_LinkGetLinkData(Link);

		assert(Area->NumAreaPortals == Area->NumWorkAreaPortals);
	}
}

//=======================================================================================
//	grBSP_MakeVisAreas
//=======================================================================================
grBoolean grBSP_MakeVisAreas(grBSP *BSPTree)
{
	assert(BSPTree);

	if (!BSPTree->RootNode)
		return GR_TRUE;

	Log_Printf(" --- grBSP_MakeVisAreas ---\n");

	BSPTree->DebugInfo.NumAreas = 0;

	assert(!BSPTree->AreaChain);

	BSPTree->AreaChain = grChain_Create();

	if (!BSPTree->AreaChain)
		return GR_FALSE;

	// Destroy any existing areas
	if (!grBSPNode_DestroyAreas_r(BSPTree->RootNode))
		return GR_FALSE;

	// Make the AreaChain
	if (!grBSPNode_MakeAreas_r(BSPTree->RootNode, BSPTree, BSPTree->AreaChain))
		return GR_FALSE;

	// First, count the portals on the areas
	if (!grBSPNode_CountAreaVisPortals_r(BSPTree->RootNode))
		return GR_FALSE;

	// Allocate portals arrays on the areas
	if (!AllocateAreaPortals(BSPTree))
		return GR_FALSE;

	// Then, make the portals on the areas
	if (!grBSPNode_MakeAreaVisPortals_r(BSPTree->RootNode, BSPTree))
		return GR_FALSE;

	// Finally, verify that counts match what was made (This code only asserts, so no error checking)
	CheckAreaPortals(BSPTree);

	Log_Printf("Num Vis Areas      : %5i\n", BSPTree->DebugInfo.NumAreas);

#ifdef AREA_DRAWFACE_TEST
	assert( BSPTree->DebugInfo.NumAreas == LN_ListLen(&(BSPTree->AreaList)));
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_DestroyVisAreas
//=======================================================================================
grBoolean grBSP_DestroyVisAreas(grBSP *BSP)
{
	grChain_Link		*Link;

	if (!BSP->AreaChain)
		return GR_TRUE;

	assert(BSP->RootNode);

	// Reset the BSP object list (they depend on the areas being valid)
	grBSP_ResetObjects(BSP);

	if (!grBSPNode_DestroyAreas_r(BSP->RootNode))
		return GR_FALSE;

	for (Link = grChain_GetFirstLink(BSP->AreaChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSPNode_Area	*Area;

		Area = (grBSPNode_Area*)grChain_LinkGetLinkData(Link);

		assert(Area->RefCount == 1);

		grBSPNode_AreaDestroy(&Area);
	}

	grChain_Destroy(&BSP->AreaChain);

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_AddObject
//=======================================================================================
grBoolean grBSP_AddObject(grBSP *BSP, grObject *Object)
{
	grBSP_Object	*BSPObject;
	grXForm3d		XForm;

	assert(!grBSP_HasObject(BSP, Object));

	BSPObject = GR_RAM_ALLOCATE_STRUCT(grBSP_Object);

	if (!BSPObject)
		return GR_FALSE;

	ZeroMem(BSPObject);

	BSPObject->Object = Object;

	// For now, just simply use the translation of the object to get an area
	//	we will need to eventually use the box, and possibly occupy more than one
	//	area, but this will do for now
	grObject_GetXForm(BSPObject->Object, &XForm);

	BSPObject->Area = grBSP_FindArea(BSP, &XForm.Translation);

	if (BSPObject->Area)
	{
		// Add the object to the list of objects in the area
		assert(!grChain_FindLink(BSPObject->Area->ObjectChain, Object));
		grChain_AddLinkData(BSPObject->Area->ObjectChain, Object);
	}

	grChain_AddLinkData(BSP->BSPObjectChain, BSPObject);

	grObject_CreateRef(Object);

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RemoveObject
//=======================================================================================
grBoolean grBSP_RemoveObject(grBSP *BSP, grObject *Object)
{
	grChain_Link	*Link;

	assert(grBSP_HasObject(BSP, Object));

	for (Link = grChain_GetFirstLink(BSP->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSP_Object		*BSPObject;

		BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);

		if (BSPObject->Object != Object)
			continue;

		// If the Object has an area, remove it from the areas list
		if (BSPObject->Area)
		{
			assert(grChain_FindLink(BSPObject->Area->ObjectChain, BSPObject->Object));
			grChain_RemoveLinkData(BSPObject->Area->ObjectChain, BSPObject->Object);
			BSPObject->Area = NULL;
		}

		// De-ref the object
		grObject_Destroy(&BSPObject->Object);

		// Remove the object from the chain
		assert(grChain_FindLink(BSP->BSPObjectChain, BSPObject));
		grChain_RemoveLinkData(BSP->BSPObjectChain, BSPObject);

		// Free the bsp object
		grRam_Free(BSPObject);

		return GR_TRUE;
	}
	
	assert(0);
	return GR_FALSE;		
}

//=======================================================================================
//	grBSP_HasObject
//=======================================================================================
grBoolean grBSP_HasObject(grBSP *BSP, grObject *Object)
{
	grChain_Link	*Link;

	for (Link = grChain_GetFirstLink(BSP->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSP_Object		*BSPObject;

		BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);

		if (BSPObject->Object == Object)
			return GR_TRUE;
	}

	return GR_FALSE;
}

//=======================================================================================
//	Rendering/Vis
//=======================================================================================

//=======================================================================================
//	grBSP_VisFrame
//=======================================================================================
grBoolean grBSP_VisFrame(grBSP *BSPTree, const grCamera *Camera, const grFrustum *ModelSpaceFrustum)
{
	grBSPNode_Area		*Area;
	grVec3d				POV;

	assert(BSPTree);

	if (!BSPTree->RootNode)
		return GR_TRUE;

	if (!BSPTree->AreaChain)
		return GR_TRUE;

	// Get the POV by taking WorldSpace camera pos, and rotaing into model space
	grXForm3d_Transform(&BSPTree->WorldToModelXForm, grCamera_GetPov(Camera), &POV);

	Area = grBSPNode_FindArea(BSPTree->RootNode, BSPTree, &POV);

	if (!Area)
	{
		// Not in a valid vis area
		// find *something* to mark as vis!
		Area = grBSPNode_FindClosestArea(BSPTree->RootNode, BSPTree, &POV);

		if (!Area) // no areas in the whole BSP !
			return GR_TRUE;
	}

	assert(BSPTree->AreaChain);

	if (!grBSPNode_AreaVisFlood_r(Area, &POV, ModelSpaceFrustum, (1<<BSPTree->RenderRecursion), Area))
	{
	
//		return GR_FALSE;
	}
	
	return GR_TRUE;
}

//=======================================================================================
//	GPU world geometry
//
//	Every draw face on a node is uploaded once as a small triangle fan (model-space
//	positions, face normal/tangent and the face's texture coordinates) so the driver
//	can draw it without the CPU clipping, transforming and projecting it each frame.
//=======================================================================================

// Nesting depth of face traversals. Only the outermost one uses the GPU path: nested
// traversals come from portal/mirror faces, whose views the CPU clips to the portal
// polygon, which the GPU path does not do yet.
static int32 g_BSPFaceTraversalDepth = 0;

typedef struct
{
	int32				NumFaces;
	int32				NumVerts;
	int32				NumIndices;
	DRV_WorldVertex		*Verts;
	uint32				*Indices;
	DRV_WorldFace		*Faces;
} grBSP_GpuBuild;

static grBoolean grBSP_GpuFaceUsable(const grBSPNode_DrawFace *Face)
{
	return (Face && Face->Poly && Face->TVerts && Face->Poly->NumVerts >= 3) ? GR_TRUE : GR_FALSE;
}

static void grBSP_GpuGather_r(grBSPNode *Node, grBSP *BSP, grBSP_GpuBuild *Build)
{
	int32		i;

	if (!Node || Node->Leaf)
		return;

	for (i = 0; i < Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*Face = Node->DrawFaces[i];
		int32				NumVerts, v;

		if (!Face)
			continue;
		Face->GpuFace = -1;
		if (!grBSP_GpuFaceUsable(Face))
			continue;

		NumVerts = Face->Poly->NumVerts;

		// First pass (no arrays yet) only counts.
		if (Build->Verts)
		{
			DRV_WorldFace		*pFace = &Build->Faces[Build->NumFaces];
			const grTexVec		*pTexVec;
			grPlane				Plane;
			grVec3d				Tangent, Bitangent, Cross;
			grFloat				Sign;

			// The side the face is seen (and lit) from, as grBSPNode_Light computes it
			Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);
			if (grPlaneArray_IndexSided(Face->PlaneIndex))
				grPlane_Inverse(&Plane);

			pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, Face->TexVecIndex);
			Tangent = pTexVec->VecU;
			Bitangent = pTexVec->VecV;
			grVec3d_Normalize(&Tangent);
			grVec3d_CrossProduct(&Plane.Normal, &Tangent, &Cross);
			Sign = (grVec3d_DotProduct(&Cross, &Bitangent) < 0.0f) ? -1.0f : 1.0f;

			for (v = 0; v < NumVerts; v++)
			{
				DRV_WorldVertex	*pVert = &Build->Verts[Build->NumVerts + v];
				const grVec3d	*pPos = grVertArray_GetVertByIndex(BSP->VertArray, Face->Poly->Verts[v]);

				pVert->Pos[0] = pPos->X;
				pVert->Pos[1] = pPos->Y;
				pVert->Pos[2] = pPos->Z;
				pVert->Normal[0] = Plane.Normal.X;
				pVert->Normal[1] = Plane.Normal.Y;
				pVert->Normal[2] = Plane.Normal.Z;
				pVert->Tangent[0] = Tangent.X;
				pVert->Tangent[1] = Tangent.Y;
				pVert->Tangent[2] = Tangent.Z;
				pVert->Tangent[3] = Sign;
				pVert->u = Face->TVerts[v].u;
				pVert->v = Face->TVerts[v].v;
				pVert->Face = (uint32)Build->NumFaces;
			}

			// Same fan as the transformed-poly path (0, i, i+1).
			pFace->FirstIndex = (uint32)Build->NumIndices;
			pFace->NumIndices = (uint32)((NumVerts - 2) * 3);
			for (v = 1; v < NumVerts - 1; v++)
			{
				uint32 *pIndex = &Build->Indices[Build->NumIndices + (v - 1) * 3];

				pIndex[0] = (uint32)(Build->NumVerts);
				pIndex[1] = (uint32)(Build->NumVerts + v);
				pIndex[2] = (uint32)(Build->NumVerts + v + 1);
			}

			Face->GpuFace = Build->NumFaces;
		}

		Build->NumFaces++;
		Build->NumVerts += NumVerts;
		Build->NumIndices += (NumVerts - 2) * 3;
	}

	grBSP_GpuGather_r(Node->Children[NODE_FRONT], BSP, Build);
	grBSP_GpuGather_r(Node->Children[NODE_BACK], BSP, Build);
}

//=======================================================================================
//	grBSP_GpuGeometryDestroy
//=======================================================================================
static void grBSP_GpuGeometryDestroy(grBSP *BSP)
{
	if (BSP->GpuGeometry && BSP->Driver && BSP->Driver->WorldGeometry_Destroy)
		BSP->Driver->WorldGeometry_Destroy(BSP->GpuGeometry);
	BSP->GpuGeometry = 0;
}

//=======================================================================================
//	grBSP_GpuGeometryBuild
//	(Re)creates the BSP's GPU world geometry. A driver without world geometry, or one
//	that declines it, leaves GpuGeometry at 0 and every face on the transformed-poly path.
//=======================================================================================
static void grBSP_GpuGeometryBuild(grBSP *BSP)
{
	grBSP_GpuBuild		Build;

	grBSP_GpuGeometryDestroy(BSP);
	BSP->GpuDirty = GR_FALSE;

	if (!BSP->RootNode || !BSP->Driver || !BSP->Driver->WorldGeometry_Create)
		return;

	memset(&Build, 0, sizeof(Build));
	grBSP_GpuGather_r(BSP->RootNode, BSP, &Build);		// count
	if (Build.NumFaces == 0)
		return;

	Build.Verts = GR_RAM_ALLOCATE_ARRAY(DRV_WorldVertex, Build.NumVerts);
	Build.Indices = GR_RAM_ALLOCATE_ARRAY(uint32, Build.NumIndices);
	Build.Faces = GR_RAM_ALLOCATE_ARRAY(DRV_WorldFace, Build.NumFaces);

	if (Build.Verts && Build.Indices && Build.Faces)
	{
		Build.NumFaces = Build.NumVerts = Build.NumIndices = 0;
		grBSP_GpuGather_r(BSP->RootNode, BSP, &Build);		// fill, assigning GpuFace

		BSP->GpuGeometry = BSP->Driver->WorldGeometry_Create(Build.Verts, Build.NumVerts,
			Build.Indices, Build.NumIndices, Build.Faces, Build.NumFaces);
	}

	if (Build.Verts)
		grRam_Free(Build.Verts);
	if (Build.Indices)
		grRam_Free(Build.Indices);
	if (Build.Faces)
		grRam_Free(Build.Faces);
}

//=======================================================================================
//	grBSP_RenderFrontToBack
//=======================================================================================
grBoolean grBSP_RenderFrontToBack(grBSP *Tree, grCamera *Camera, grFrustum *CameraSpaceFrustum, grFrustum *ModelSpaceFrustum, grXForm3d *ModelToCameraXForm)
{
	uint32				ClipFlags;
	grBSPNode_SceneInfo	SceneInfo;
	DRV_Driver			*Driver;

	assert(Tree);
	assert(Tree->Engine);
	assert(Camera);
	assert(ModelSpaceFrustum);

	if (!Tree->RootNode)
		return GR_TRUE;

	if (!Tree->Engine)
		return GR_TRUE;

#if (GR_BSP_DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("BEGIN grBSP_RenderFrontToBack\n");
#endif

	Driver = grEngine_GetDriver(Tree->Engine);
	assert(Driver);
	Driver->SetupLightmap = grBSP_SetupLightmap;

	// Fill in the SceneInfo struct
	SceneInfo.Camera = Camera;
	
	// Get ModelToCameraXForm by combining ModelToWorld, and WorldToCamera together
	SceneInfo.ModelToCameraXForm = *ModelToCameraXForm;

	// Get the POV by taking WorldSpace camera pos, and rotaing into model space
	grXForm3d_Transform(&Tree->WorldToModelXForm, grCamera_GetPov(Camera), &SceneInfo.POV);

	SceneInfo.Frustum = ModelSpaceFrustum;

	if (Tree->AreaChain)
		SceneInfo.RecursionBit = (1<<Tree->RenderRecursion);
	else
		SceneInfo.RecursionBit = 0;


	grEngine_GetDefaultRenderFlags(Tree->Engine, &SceneInfo.DefaultRenderFlags);

	// GPU world path. The geometry is only rebuilt in the outermost traversal (see
	// g_BSPFaceTraversalDepth). Nested traversals (portal and mirror views) clip to their
	// camera-space frustum on the GPU, so they need it to fit in the driver's clip planes.
	if (Tree->GpuDirty && g_BSPFaceTraversalDepth == 0)
		grBSP_GpuGeometryBuild(Tree);

	SceneInfo.GpuWorld = Tree->GpuGeometry ? GR_TRUE : GR_FALSE;
	if (SceneInfo.GpuWorld)
	{
		grBoolean	ZFarEnable;
		grFloat		ZFar;
		int32		NumPlanes;

		grCamera_GetFarClipPlane(Camera, &ZFarEnable, &ZFar);
		if (g_BSPFaceTraversalDepth > 0)
		{
			NumPlanes = CameraSpaceFrustum ? CameraSpaceFrustum->NumPlanes + (ZFarEnable ? 1 : 0) : 0;
			if (NumPlanes <= 0 || NumPlanes > DRV_WORLD_MAX_CLIP_PLANES)
				SceneInfo.GpuWorld = GR_FALSE;
		}
	}
	if (SceneInfo.GpuWorld)
	{
		grBoolean	ZFarEnable;
		grFloat		ZFar;
		int32		i;

		memset(&SceneInfo.WorldView, 0, sizeof(SceneInfo.WorldView));
		SceneInfo.WorldView.ModelToCamera = *ModelToCameraXForm;
		grCamera_GetScreenProjection(Camera, &SceneInfo.WorldView.Scale,
			&SceneInfo.WorldView.XCenter, &SceneInfo.WorldView.YCenter);
		SceneInfo.WorldView.ZScale = grCamera_GetZScale(Camera);
		grCamera_GetScreenSize(Camera, &SceneInfo.WorldView.HalfWidth, &SceneInfo.WorldView.HalfHeight);
		SceneInfo.WorldView.HalfWidth *= 0.5f;
		SceneInfo.WorldView.HalfHeight *= 0.5f;
		grCamera_GetFarClipPlane(Camera, &ZFarEnable, &ZFar);
		SceneInfo.WorldView.ZFar = ZFarEnable ? ZFar : 0.0f;

		if (g_BSPFaceTraversalDepth > 0)
		{
			// Same planes and sides as grFrustum_ClipLVerts* (inside: N.p - Dist >= 0)
			for (i = 0; i < CameraSpaceFrustum->NumPlanes; i++)
			{
				const grPlane	*Plane = &CameraSpaceFrustum->Planes[i];
				float			*Dst = SceneInfo.WorldView.ClipPlanes[i];

				Dst[0] = Plane->Normal.X;
				Dst[1] = Plane->Normal.Y;
				Dst[2] = Plane->Normal.Z;
				Dst[3] = -Plane->Dist;
			}
			if (ZFarEnable)
			{
				// Camera space looks down -Z: inside when -Z <= ZFar
				float	*Dst = SceneInfo.WorldView.ClipPlanes[i++];

				Dst[0] = 0.0f;
				Dst[1] = 0.0f;
				Dst[2] = 1.0f;
				Dst[3] = ZFar;
			}
			SceneInfo.WorldView.NumClipPlanes = i;
		}
	}

	// Setup clipflags
	ClipFlags = (1<<ModelSpaceFrustum->NumPlanes)-1;

	Tree->RenderRecursion++;

	NumMakeFaces = 0;

	g_BSPFaceTraversalDepth++;
	grBSPNode_RenderFrontToBack_r(Tree->RootNode, Tree, &SceneInfo, ClipFlags);
	g_BSPFaceTraversalDepth--;

	assert(Tree->RenderRecursion > 0);
	Tree->RenderRecursion--;

	if (Tree->AreaChain)
	{
		grChain_Link	*Link;

		for (Link = grChain_GetFirstLink(Tree->AreaChain); Link; Link = grChain_LinkGetNext(Link))
		{
			grBSPNode_Area		*Area;
			grChain_Link		*Link2;

			Area = (grBSPNode_Area*)grChain_LinkGetLinkData(Link);
			
			if (!(Area->RecursionBits & SceneInfo.RecursionBit))
				continue;

			// Krouer: I can render the area here
			{
				grDeviceCaps devcaps;
				Tree->Driver->GetDeviceCaps(&devcaps);
				if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == GR_RENDER_FLAG_HWTRANSFORM) {
					grBSPNode_AreaRenderVertexBuffer(Area, Tree);
				}
			}

			// The Area is visible, render all objects inside the area
			//	NOTE - For now, objects only live in one area at a time, so we
			//	always render them.  As soon as they are allowed to live in more than
			//	one area, we will have to take this into account, and make sure we only render
			//	them once...
			for (Link2 = grChain_GetFirstLink(Area->ObjectChain); Link2; Link2 = grChain_LinkGetNext(Link2))
			{
				grObject		*Object;
				grObject_Type	objectType;

				Object = (grObject*)grChain_LinkGetLinkData(Link2);
				objectType = grObject_GetType(Object);

				if (GR_OBJECT_TYPE_ACTOR==objectType || GR_OBJECT_TYPE_TERRAIN==objectType) {
					grObject_SetRenderNextPass(Object, GR_TRUE);
					//grActor_SetRenderNextTime((grActor*) Object->Instance, GR_TRUE);
				} else {
					// Render the object
					if (!grObject_Render(Object, Tree->World, Tree->Engine, Camera, CameraSpaceFrustum, 0))
						return GR_FALSE;
				}
			}

			// Remove the vis recursion bit
			Area->RecursionBits ^= SceneInfo.RecursionBit;
		}
	}
	else		// Render every object, since there is no areas...
	{
		grChain_Link	*Link;

		OutputDebugString("BSP: before render Objects - no AreaChain\n");
		{
			grDeviceCaps devcaps;
			Tree->Driver->GetDeviceCaps(&devcaps);
			if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == GR_RENDER_FLAG_HWTRANSFORM) {
				grBSP_RenderVertexBuffer(Tree);
			}
		}

		for (Link = grChain_GetFirstLink(Tree->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
		{
			//grObject		*Object;
			grBSP_Object		*BSPObject;

			BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);
		
			// Render the object
			if (!grObject_Render(BSPObject->Object, Tree->World, Tree->Engine, Camera, CameraSpaceFrustum, 0))
				return GR_FALSE;
		}
	}

#if (GR_BSP_DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("END grBSP_RenderFrontToBack\n");
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RenderAndVis
//=======================================================================================
grBoolean grBSP_RenderAndVis(grBSP *Tree, grCamera *Camera, grFrustum *Frustum)
{
	grFrustum		ModelSpaceFrustum;
	grXForm3d		ModelToCameraXForm, CameraToModelXForm;

	Tree->DebugInfo.NumVisibleAreas = 0;

	grXForm3d_Multiply(grCamera_XForm(Camera), &Tree->ModelToWorldXForm, &ModelToCameraXForm);
	grXForm3d_GetTranspose(&ModelToCameraXForm, &CameraToModelXForm);

	// Transform Frustum from camera space to model space
	grFrustum_Transform(Frustum, &CameraToModelXForm, &ModelSpaceFrustum);

	if (!grBSP_UpdateAll(Tree))
		return GR_FALSE;

	if (!grBSP_VisFrame(Tree, Camera, &ModelSpaceFrustum))
		return GR_FALSE;

	if (!UpdateDLights(Tree))
		return GR_FALSE;

	if (!UpdateObjects(Tree))
		return GR_FALSE;

	// Render the models BSP tree
	if (!grBSP_RenderFrontToBack(Tree, Camera, Frustum, &ModelSpaceFrustum, &ModelToCameraXForm))
		return GR_FALSE;

#if 0
	if (!Tree->AreaChain) // @@
	{
		// Render the models BSP tree
		if (!grBSP_RenderFrontToBack(Tree, Camera, &ModelSpaceFrustum))
			return GR_FALSE;

	}
	else
	{
		if (!grBSP_RenderAreas(Tree, Camera, Frustum))
			return GR_FALSE;
	}
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RenderAreas
//=======================================================================================
grBoolean grBSP_RenderAreas(grBSP *Tree, grCamera *Camera, grFrustum *CameraSpaceFrustum, grFrustum *ModelSpaceFrustum, grXForm3d *ModelToCameraXForm)
{
#ifdef AREA_DRAWFACE_TEST
	DRV_Driver		*Driver;

	grBSPNode_Area *Area;

	assert(Tree);
	assert(Tree->Engine);
	assert(Camera);
	assert(Frustum);

	if (!Tree->RootNode)
		return GR_TRUE;

	Driver = grEngine_GetDriver(Tree->Engine);
	assert(Driver);
	Driver->SetupLightmap = grBSP_SetupLightmap;
		
	Area = grBSPNode_FindArea(Tree->RootNode,Tree, grCamera_GetPov(Camera));
	if ( ! Area )
	{
		Area = grBSPNode_FindClosestArea(Tree->RootNode,Tree, grCamera_GetPov(Camera));
		if ( ! Area )
			return GR_FALSE;
	}

	if (! grBSPNode_AreaRenderFlood_r(Area,Tree, Camera,Frustum, Tree->RenderRecursion ,NULL) )
		return GR_FALSE;
#endif

	return GR_TRUE;
}

#ifdef AREA_DRAWFACE_TEST
//=======================================================================================
//	grBSP_MakeAreaDrawFaces
//=======================================================================================
grBoolean grBSP_MakeAreaDrawFaces(grBSP *BSP)
{
	grBSPNode_Area * pArea;
	LinkNode *CurNode=NULL;

	grChain_Link	*Link;

	assert(BSP);

//	LN_Walk(pArea,&(BSP->AreaList))
	for (Link = grChain_GetFirstLink(BSP->AreaChain); Link; Link = grChain_LinkGetNext(Link))

	{
		pArea = (grBSPNode_Area*)grChain_LinkGetLinkData(Link);
		if ( ! grBSPNode_AreaMakeDrawFaces(pArea) )
			return GR_FALSE;
	}

   return GR_TRUE;
}
#endif

//=======================================================================================
//	Update lights/faces/brushes
//=======================================================================================

//=====================================================================================
//	grBSP_UpdateBrush
//	Update the geometry of a brush, autolights if told to do so
//=====================================================================================
grBoolean grBSP_UpdateBrush(grBSP *BSPTree, grBrush *Brush, grBoolean AutoLight)
{
	grBSP_TopBrush		*TopBrush, *NewList, *Next;
	grBoolean			Found;
	uint32				Order;

	assert(BSPTree);
	assert(Brush);

	if (BSPTree->AreaChain)
	{
		if (!grBSP_DestroyVisAreas(BSPTree))
			return GR_FALSE;
	}

	NewList = NULL;
	Found = GR_FALSE;

	if (BSPTree->RootNode)
	{
		// Find all the topbrushes that are a part of the editor brush, and remove them
		for (TopBrush = BSPTree->TopBrushes; TopBrush; TopBrush = Next)
		{
			Next = TopBrush->Next;

			if (TopBrush->Original == Brush)
			{
				if (!Found)
				{
					Order = TopBrush->Order;
					Found = GR_TRUE;
				}
				else
				{
					// All topbrushes resulting from the same grBrush should have the same order number!!
					assert(Order == TopBrush->Order);
				}
		
				if (!grBSPNode_RemoveTopBrush_r(BSPTree->RootNode, BSPTree, TopBrush))
					return GR_FALSE;
		
				grBSP_TopBrushDestroy(&TopBrush, BSPTree);
				continue;
			}

			TopBrush->Next = NewList;
			NewList = TopBrush;
		}

		BSPTree->TopBrushes = NewList;
	}

	if (!Found)
		Order = BSPTree->NumBrushes++;
   	
	// Add the brush back into the tree with the same order number it had before
	if (!grBSP_AddBrushInternally(BSPTree, Brush, Order, AutoLight))
		return GR_FALSE;

	BSPTree->UpdateFlags |= BSP_UPDATE_FACES;
	BSPTree->UpdateFlags |= BSP_UPDATE_LIGHTS;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_UpdateBrushFace
//	Update the properties of a face, autolights if told to do so
//=======================================================================================
grBoolean grBSP_UpdateBrushFace(grBSP *BSP, const grBrush_Face *Face, grBoolean AutoLight)
{
	grBSP_TopBrush	*TopBrush;
	grBrush			*Original;

	assert(BSP);

	if (BSP->AreaChain)
	{
		if (!grBSP_DestroyVisAreas(BSP))
			return GR_FALSE;
	}

	if (!BSP->RootNode)
		return GR_TRUE;

	Original = grBrush_FaceGetBrush(Face);

	// Find the TopBrush that contains the pointer to the grBrushFace
	//	This is kind of slow, but we could use hash tables if it got out of hand...
	for (TopBrush = BSP->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
	{
		int32			i;
		grBSP_TopSide	*Side;

		if (TopBrush->Original != Original)
			continue;

		for (Side = TopBrush->TopSides, i=0; i< TopBrush->NumSides; i++, Side++)
		{
			if (Side->grBrushFace != Face)
				continue;

			if (AutoLight)
				Side->TopSideFlags |= TOPSIDE_UPDATE_LIGHT;
			else
				Side->TopSideFlags &= ~TOPSIDE_UPDATE_LIGHT;

			if (!grBSP_TopBrushSideCalcFaceInfo(TopBrush, BSP, Side, Face))
				return GR_FALSE;

			// Mark all nodes that touch this side to update their faces
			if (!grBSPNode_DirtyNodes_r(BSP->RootNode, BSP, Side))
				return GR_FALSE;
					
			BSP->UpdateFlags |= BSP_UPDATE_FACES;
			BSP->UpdateFlags |= BSP_UPDATE_LIGHTS;
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_PatchLights
//=======================================================================================
grBoolean grBSP_PatchLighting(grBSP *Tree)
{
	if (!Tree->RootNode)
		return GR_TRUE;

	if (!grBSPNode_LightPatch_r(Tree->RootNode, Tree, Tree->RootNode))
		return GR_FALSE;

	{
		grBSP_TopBrush		*TopBrush;

		for (TopBrush = Tree->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
		{
			int32		i;

			for (i=0; i< TopBrush->NumSides; i++)
			{
				TopBrush->TopSides[i].TopSideFlags |= TOPSIDE_UPDATE_LIGHT;
			}
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RebuildLights
//=======================================================================================
grBoolean grBSP_RebuildLights(grBSP *Tree)
{
	if (!Tree->RootNode)
		return GR_TRUE;

	if (!grBSPNode_LightUpdate_r(Tree->RootNode, Tree, Tree->RootNode, GR_TRUE))
		return GR_FALSE;

	{
		grBSP_TopBrush		*TopBrush;

		for (TopBrush = Tree->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
		{
			int32		i;

			for (i=0; i< TopBrush->NumSides; i++)
			{
				TopBrush->TopSides[i].TopSideFlags |= TOPSIDE_UPDATE_LIGHT;
			}
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RebuildLightsFromPoint
//=======================================================================================
grBoolean grBSP_RebuildLightsFromPoint(grBSP *Tree, const grVec3d *Pos, grFloat Radius)
{
	grVec3d		NewPos;

	if (!Tree->RootNode)
		return GR_TRUE;

	grXForm3d_Transform(&Tree->WorldToModelXForm, Pos, &NewPos);
	
	if (!grBSPNode_LightUpdateFromPoint_r(Tree->RootNode, Tree, Tree->RootNode, &NewPos, Radius))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_UpdateAll
//=======================================================================================
grBoolean grBSP_UpdateAll(grBSP *BSP)
{
	assert(BSP);

#if (GR_BSP_DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("BEGIN grBSP_UpdateAll\n");
#endif

	if (!BSP->RootNode)
		return GR_TRUE;

	if (BSP->UpdateFlags & BSP_UPDATE_FACES)
	{
		if (!grBSP_MarkVisibleTopSides(BSP))
			return GR_FALSE;

		NumMakeFaces = 0;
		NumMergedFaces = 0;
		NumSubdividedFaces = 0;

		if (!grBSPNode_MakeFaces_r(BSP->RootNode, BSP, GR_TRUE))
			return GR_FALSE;

		BSP->UpdateFlags &= ~BSP_UPDATE_FACES;

		if (BSP->Driver) 
		{
			grDeviceCaps devcaps;
			BSP->Driver->GetDeviceCaps(&devcaps);
			if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == GR_RENDER_FLAG_HWTRANSFORM) {
				grBSP_CreateVertexBuffer(BSP);
			}
		}
	}

	if (BSP->UpdateFlags & BSP_UPDATE_LIGHTS)
	{
		if (!grBSPNode_LightUpdate_r(BSP->RootNode, BSP, BSP->RootNode, GR_FALSE))
			return GR_FALSE;

		BSP->UpdateFlags &= ~BSP_UPDATE_LIGHTS;
	}

	{
		grDeviceCaps devcaps;
		BSP->Driver->GetDeviceCaps(&devcaps);
		if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == GR_RENDER_FLAG_HWTRANSFORM && BSP->pVertexBuffer == NULL) {
			grBSP_CreateVertexBuffer(BSP);
		}
	}

#if (GR_BSP_DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("END grBSP_UpdateAll\n");
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_RebuildFaces
//=======================================================================================
grBoolean grBSP_RebuildFaces(grBSP *BSPTree)
{
	if (!BSPTree->RootNode)
		return GR_TRUE;

	grBSPNode_RebuildFaces_r(BSPTree->RootNode, BSPTree);

	// Use the tree to mark the visible sides
	if (!grBSP_MarkVisibleTopSides(BSPTree))
		return GR_FALSE;

	if (!grBSPNode_MakeFaces_Callr(BSPTree->RootNode, BSPTree, GR_TRUE))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetBrushFaceCB
//=======================================================================================
grBoolean grBSP_SetBrushFaceCB(grBSP *BSP, grBSPNode_DrawFaceCB *CB, void *Context)
{
	BSP->DrawFaceCB = CB;
	BSP->DrawFaceCBContext = Context;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetBrushFaceCBOnOff
//=======================================================================================
grBoolean grBSP_SetBrushFaceCBOnOff(grBSP *BSP, const grBrush_Face *Face, grBoolean OnOff)
{
	grBSP_TopBrush	*TopBrush;
	grBrush			*Original;

	assert(BSP);

	if (!BSP->RootNode)
		return GR_TRUE;

	Original = grBrush_FaceGetBrush(Face);

	// Find the TopBrush that contains the pointer to the grBrushFace
	//	This is kind of slow, but we could use hash tables if it got out of hand...
	for (TopBrush = BSP->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
	{
		int32			i;
		grBSP_TopSide	*Side;

		if (TopBrush->Original != Original)
			continue;

		for (Side = TopBrush->TopSides, i=0; i< TopBrush->NumSides; i++, Side++)
		{
			int32		Num;

			if (Side->grBrushFace != Face)
				continue;

			if (OnOff)
				Side->TopSideFlags |= TOPSIDE_CALL_CB;
			else
				Side->TopSideFlags &= ~TOPSIDE_CALL_CB;

			Num = 0;
			if (!grBSPNode_PropogateTopSideFlags_r(BSP->RootNode, BSP, Side, &Num))
				return GR_FALSE;

			if (!Num)
			{
				if (!grBSPNode_DirtyNodes_r(BSP->RootNode, BSP, Side))
					return GR_FALSE;
			}

			// Update faces and lights just in case anything got destroyed
			BSP->UpdateFlags |= BSP_UPDATE_FACES;
			BSP->UpdateFlags |= BSP_UPDATE_LIGHTS;
		}
	}

	return GR_TRUE;
}

//================================================================================================
//	Misc
//================================================================================================

//=======================================================================================
//	grBSP_SetXForm
//=======================================================================================
grBoolean grBSP_SetXForm(grBSP *BSP, const grXForm3d *XForm)
{
	// Save off ModelToWorld XForm
	BSP->ModelToWorldXForm = *XForm;

	// Get the Transpose of that to get WorldToModelXForm
	grXForm3d_GetTranspose(&BSP->ModelToWorldXForm, &BSP->WorldToModelXForm);

	// Maintain the bounding WorldSpace ExtBox of the bsp
	if (!grBSP_UpdateWorldSpaceBox(BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetEngine
//=======================================================================================
grBoolean grBSP_SetEngine(grBSP *Tree, grEngine *Engine)
{
	assert(Tree);
	if (Engine == Tree->Engine) {
		return GR_TRUE;
	}
	
	if (Tree->Engine)
	{
		assert(Tree->ChangeDriverCB);
		grEngine_DestroyChangeDriverCB(Tree->Engine, &Tree->ChangeDriverCB);
		grEngine_Free(Tree->Engine);
		Tree->Engine = NULL;
	}
	else
	{
		assert(!Tree->ChangeDriverCB);
		assert(!Tree->Driver);
	}

	Tree->Engine = Engine;

	if (Engine)
	{
		Tree->ChangeDriverCB = grEngine_CreateChangeDriverCB(Engine, ShutdownDriverCB, StartupDriverCB, Tree);

		if (!Tree->ChangeDriverCB)
			return GR_FALSE;

		grEngine_CreateRef(Engine);
		grEngine_SetRenderMode(Engine, Tree->RenderMode);
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetWorld
//=======================================================================================
grBoolean grBSP_SetWorld(grBSP *Tree, grWorld *World)
{
	assert(Tree);

	// Weak back-pointer: the world owns the objects whose models own this tree and
	// detaches them (setting this to NULL) before it is freed. Taking a reference here
	// made a cycle that kept the world, and through it the engine and driver, alive forever.
	Tree->World = World;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetRenderMode
//=======================================================================================
grBoolean grBSP_SetRenderMode(grBSP *BSP, grBSP_RenderMode RenderMode)
{
	BSP->RenderMode = RenderMode;
   grEngine_SetRenderMode(BSP->Engine, RenderMode);

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_SetDefaultContents
//=======================================================================================
grBoolean grBSP_SetDefaultContents(grBSP *BSP, grBrush_Contents DefaultContents)
{
	BSP->DefaultContents = DefaultContents;

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_FindArea
//=======================================================================================
grBSPNode_Area *grBSP_FindArea(grBSP *BSP, const grVec3d *Pos)
{
	if (!BSP->RootNode)
		return NULL;

	return grBSPNode_FindArea(BSP->RootNode,BSP, Pos);
}

//=======================================================================================
//	grBSP_DoAllAreasInBox
//=======================================================================================
grBoolean grBSP_DoAllAreasInBox(grBSP *BSP,grExtBox *BBox,grBSP_DoAreaFunc CB,void * Context)
{
	return grBSPNode_DoAllAreasInBox(BSP->RootNode,BSP, BBox,CB,Context);
}

//=====================================================================================
//	grBSP_GetModelSpaceBox
//=====================================================================================
grBoolean grBSP_GetModelSpaceBox(const grBSP *BSP, grExtBox *Box)
{
	*Box = BSP->Box;

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_GetWorldSpaceBox
//=====================================================================================
grBoolean grBSP_GetWorldSpaceBox(const grBSP *BSP, grExtBox *Box)
{
	*Box = BSP->WorldSpaceBox;

	return GR_TRUE;
}

//=====================================================================================
//	GetSegmentBox
//	Creates a box around the entire segment
//=====================================================================================
static void GetSegmentBox(const grVec3d *Mins, const grVec3d *Maxs, const grVec3d *Front, const grVec3d *Back, grVec3d *SMins, grVec3d *SMaxs)
{
	int32		i;

	assert(Mins);
	assert(Maxs);
	assert(Front);
	assert(Back);
	
	for (i=0 ; i<3 ; i++)
	{
		if (grVec3d_GetElement(Back, i) > grVec3d_GetElement(Front, i))
		{
			grVec3d_SetElement(SMins, i, grVec3d_GetElement(Front, i) + grVec3d_GetElement(Mins, i) - 1.0f);
			grVec3d_SetElement(SMaxs, i, grVec3d_GetElement(Back, i) + grVec3d_GetElement(Maxs, i) + 1.0f);
		}
		else
		{
			grVec3d_SetElement(SMins, i, grVec3d_GetElement(Back, i) + grVec3d_GetElement(Mins, i) - 1.0f);
			grVec3d_SetElement(SMaxs, i, grVec3d_GetElement(Front, i) + grVec3d_GetElement(Maxs, i) + 1.0f);
		}
	}
}

//=======================================================================================
//	grBSP_Collision
//	Returns GR_TRUE if there was a collision, GR_FALSE otherwise
//=======================================================================================
grBoolean grBSP_Collision(	const grBSP		*BSP, 
							const grExtBox	*Box, 
							const grVec3d	*Front, 
							const grVec3d	*Back, 
							grVec3d			*Impact, 
							grPlane			*Plane)
{
	int32			i;
	grVec3d			SegmentMins, SegmentMaxs, Front2, Back2, vPath;
	grExtBox		FakeBox;
	const grExtBox	*RejectBox;

	// The code below will act as if there was a box, so if box is NULL, pretend like there is a box
	if (Box)
	{
		RejectBox = Box;
	}
	else
	{
		grVec3d_Set(&FakeBox.Min, -1.0f, -1.0f, -1.0f);
		grVec3d_Set(&FakeBox.Max,  1.0f,  1.0f,  1.0f);
		RejectBox = &FakeBox;
	}

	// Get a box that enscribes the entire segment
	GetSegmentBox(&RejectBox->Min, &RejectBox->Max, Front, Back, &SegmentMins, &SegmentMaxs);

	// Do some trivial rejection stuff here
	for (i=0; i<3; i++)
	{
		if (grVec3d_GetElement(&SegmentMaxs, i) < grVec3d_GetElement(&BSP->WorldSpaceBox.Min, i))
			break;
		if (grVec3d_GetElement(&SegmentMins, i) > grVec3d_GetElement(&BSP->WorldSpaceBox.Max, i))
			break;
	}

	if (i != 3)
		return GR_FALSE;		// Segment does not collide with mnode's dynamic box (box is already transformed into world space)


	// Transform the ray into the bsptree
	grXForm3d_Transform(&BSP->WorldToModelXForm, Front, &Front2);
	grXForm3d_Transform(&BSP->WorldToModelXForm, Back , &Back2);

	if (!Box)
	{
		// Icestorm: Check only on collision, no further details
		if (!Plane || !Impact)
			return (grBSPNode_CollisionExact_r(BSP->RootNode, (grBSP*)BSP, &Front2, &Back2, NULL));
		else
		{
			grBSPNode_CollisionInfo			Info;

			Info.HitSet = GR_FALSE;
			Info.Impact = Impact;
			Info.Plane = Plane;

			if (grBSPNode_CollisionExact_r(BSP->RootNode, (grBSP*)BSP, &Front2, &Back2, &Info))
			{
	            grXForm3d_Transform(&BSP->ModelToWorldXForm, Info.Impact, Info.Impact);
				grPlane_Transform(Plane, &BSP->ModelToWorldXForm, Plane);	// Transform Plane into world space
				return GR_TRUE;
			}
		}
	}
	else
	{
		grExtBox					SegmentBox;

		GetSegmentBox(&Box->Min, &Box->Max, &Front2, &Back2, &SegmentBox.Min, &SegmentBox.Max);

		if (!Plane || !Impact)
			return grBSPNode_CollisionBBox_r(BSP->RootNode, (grBSP*)BSP, &SegmentBox, Box, &Front2, &Back2, NULL);
		else
		{
			grBSPNode_CollisionInfo2	Info;
			Info.HitSet = GR_FALSE;
			Info.Front = &Front2;
			Info.Plane = Plane;
			Info.Impact = Impact;
			Info.BestDist = 9999999.0f;

			grBSPNode_CollisionBBox_r(BSP->RootNode, (grBSP*)BSP, &SegmentBox, Box, &Front2, &Back2, &Info);

			if (Info.HitSet)
			{
				grPlane_Transform(Plane, &BSP->ModelToWorldXForm, Plane);	// Transform Plane into world space
				grXForm3d_Transform(&BSP->ModelToWorldXForm, Info.Impact, Info.Impact);

				grVec3d_Subtract(Front, Back, &vPath); 
				grVec3d_Normalize(&vPath); 
				grVec3d_AddScaled(Info.Impact, &vPath, 1.5f, Info.Impact); 
				Plane->Dist = grVec3d_DotProduct(&vPath, Info.Impact); 
				Plane->Type = Type_Any; 

				return GR_TRUE;
			}
		}
	}

	return GR_FALSE;			
}

// Added by Icestorm
//=======================================================================================
//	grBSP_ChangeBoxCollision
//	Returns GR_TRUE if there was a collision, while box changes, GR_FALSE otherwise
//=======================================================================================
grBoolean grBSP_ChangeBoxCollision(	const grBSP		*BSP, 
									const grVec3d	*Pos,
									const grExtBox	*FrontBox, 
									const grExtBox	*BackBox, 
									grExtBox		*ImpactBox, 
									grPlane			*Plane)
{
	int32			i;
	grVec3d			SegmentMins, SegmentMaxs, Pos2;
	grExtBox		RejectBox;

	assert(grExtBox_IsValid(FrontBox)&&grExtBox_IsValid(BackBox));
	
	// Get a box that enscribes the entire segment
	grExtBox_Union(FrontBox, BackBox, &RejectBox);
	// extend to a safe distance
	RejectBox.Min.X-=1.0f;RejectBox.Min.Y-=1.0f;RejectBox.Min.Z-=1.0f;
	RejectBox.Max.X+=1.0f;RejectBox.Max.Y+=1.0f;RejectBox.Max.Z+=1.0f;
	grExtBox_SetTranslation(&RejectBox,Pos);
	SegmentMins=RejectBox.Min;SegmentMaxs=RejectBox.Max;

	// Do some trivial rejection stuff here
	for (i=0; i<3; i++)
	{
		if (grVec3d_GetElement(&SegmentMaxs, i) < grVec3d_GetElement(&BSP->WorldSpaceBox.Min, i))
			break;
		if (grVec3d_GetElement(&SegmentMins, i) > grVec3d_GetElement(&BSP->WorldSpaceBox.Max, i))
			break;
	}

	if (i != 3)
		return GR_FALSE;		// Segment does not collide with node's dynamic box (box is already transformed into world space)


	// Transform the ray into the bsptree
	grXForm3d_Transform(&BSP->WorldToModelXForm, Pos, &Pos2);

	{
		grExtBox_SetTranslation(&RejectBox,&Pos2);
		
		if (!Plane || !ImpactBox)
			return grBSPNode_ChangeBoxCollisionBBox_r(BSP->RootNode, (grBSP*)BSP, &RejectBox, &Pos2,
					FrontBox, BackBox, NULL);
		else
		{
			grBSPNode_CollisionInfo3	Info;

			Info.HitSet = GR_FALSE;
			Info.FrontBox = FrontBox;
			Info.Plane = Plane;
			Info.ImpactBox = ImpactBox;
			Info.BestDist = 9999999.0f;

			grBSPNode_ChangeBoxCollisionBBox_r(BSP->RootNode, (grBSP*)BSP, &RejectBox, &Pos2, FrontBox, BackBox, &Info);

			if (Info.HitSet)
			{
				grPlane_Transform(Plane, &BSP->ModelToWorldXForm, Plane);	// Transform Plane into world space
				Plane->Dist += 0.1f;
				return GR_TRUE;
			}
		}
	}

	return GR_FALSE;			
}

//========================================================================================
//	grBSP_RayIntersectsBrushes
//========================================================================================
grBoolean grBSP_RayIntersectsBrushes(const grBSP *BSP, const grVec3d *Front, const grVec3d *Back, grBrushRayInfo *Info)
{
	grVec3d		Front2, Back2;

	assert(BSP);

	if (!BSP->RootNode)
		return GR_FALSE;

	// Transform the ray into the bsptree
	grXForm3d_Transform(&BSP->WorldToModelXForm, Front, &Front2);
	grXForm3d_Transform(&BSP->WorldToModelXForm, Back , &Back2);

	if (grBSPNode_RayIntersectsBrushes(((grBSP*)BSP)->RootNode, (grBSP*)BSP, &Front2, &Back2, Info))
	{
		grPlane_Transform(&Info->Plane, &BSP->ModelToWorldXForm, &Info->Plane);	// Transform Plane into world space
		return GR_TRUE;
	}

	return GR_FALSE;
}

//=======================================================================================
//	grBSP_GetDebugInfo
//=======================================================================================
const grBSP_DebugInfo *grBSP_GetDebugInfo(const grBSP *BSPTree)
{
	assert(BSPTree);

	return &BSPTree->DebugInfo;
}

//=======================================================================================
//	 Local static functions
//=======================================================================================

//=====================================================================================
//	grBSP_AddBSPBrush_r
//	Takes the brush, and distributes it to the leafs, where it then takes the leafs
//	the brush lands in, and partitions up the resulting brushes to make more leafs...
//=====================================================================================
static grBoolean grBSP_AddBSPBrush_r(grBSP *BSP, grBSPNode *Node, grBSP_Brush **Brush)
{
	grBSP_Brush		*Front, *Back, *Brush2;
	int32			Splits,  EpsilonBrush;
	int HintSplit;

	assert(Node);

	Brush2 = *Brush;

	if (!Brush2)				// Brush was clipped away...
		return GR_TRUE;

	if (Node->Flags & NODE_LEAF)
	{
		// At leaf...
		// Convert this leaf into a node, and take the brush fragment that landed in this partition, and 
		// partition it into new leafs starting from the new node (the converted leaf)
		grBSPNode_Portal	*p;
		int32				Side;
		grBSP_Brush			*BSPBrush;

		assert(Node->Leaf);
		assert(Node->Portals);

		grBSPNode_LeafDestroyDrawFaceList(Node->Leaf, BSP);

	#if 1
		for (BSPBrush = Node->Leaf->Brushes; BSPBrush; BSPBrush=BSPBrush->Next)
		{
			// Since this brush is "inside" of this leaf, we can force the brushes of this leaf
			// down both sides (unless the leaf brush has a plane that shares a plane in this brush, 
			// then we send it down the correct side)
			BSPBrush->Flags = BSPBRUSH_FORCEBOTH;
			//grBSP_BrushCreatePolys(BSPBrush);
		}
	#endif

		Brush2->Next = Node->Leaf->Brushes;	// Add brush to front of leafs list of brushes
		Node->Leaf->Brushes = NULL;			// So grBSPNode_LeafDestroy won't destroy them
		Node->Flags &= ~NODE_LEAF;			// This node is no longer a leaf

		grBSPNode_LeafDestroy(&Node->Leaf, BSP);

		BSP->DebugInfo.NumLeafs--;			// Take a leaf away from the num total leafs in tree

		// Go through all the portals that look into this leaf, and remove their polys
		// from the nodes they were created on
		for (p = Node->Portals; p; p = p->Next[Side])
		{
			Side = (p->Nodes[1] == Node);

			if (p->OnNode)
			{
				int32		i;

				for (i=0; i<2; i++)
				{
					if (p->Face[i])
					{
						assert(p->Face[i]->Portal == p);
						p->OnNode->Faces = grBSPNode_FaceCullList(p->OnNode->Faces, &p->Face[i]);
					}
				}
				// Force an update on nodes that share this portal
				p->OnNode->Flags |= NODE_REBUILD_FACES;
			}

			grBSPNode_PortalResetTopSide(p);
		}

		// Start from this list, and start building the tree from here to form the new leafs
		if (!grBSP_BuildBSPTree_r(BSP, Node, Brush))
		{
			grErrorLog_AddString(-1, "grBSP_AddBSPBrush_r:  grBSPNode_PartitionPortals_r failed.", NULL);
			return GR_FALSE;
		}

		// Take the portals that were on this old leaf, and distribute them to the new leafs...
		if (!grBSPNode_PartitionPortals_r(Node, BSP, GR_TRUE))
		{
			grErrorLog_AddString(-1, "grBSP_AddBSPBrush_r:  grBSPNode_PartitionPortals_r failed.", NULL);
			return GR_FALSE;
		}

		// Update the leaf sides
		if (!grBSPNode_UpdateLeafSides_r(Node, BSP))
		{
			grErrorLog_AddString(-1, "grBSP_AddBSPBrush_r:  grBSPNode_UpdateLeafSides_r failed.", NULL);
			return GR_FALSE;
		}

		return GR_TRUE;
	}

	assert(!Node->Leaf);

	// Set the brush side, grBSP_BrushSplitListByNode uses it as a first test
	Brush2->Side = grBSP_TestBrushToPlaneIndex(BSP, Brush2, Node->PlaneIndex, &Splits, &HintSplit, &EpsilonBrush);

	if (!grBSPNode_SplitBrushList(Node, BSP, Brush2, &Front, &Back))
		return GR_FALSE;

	grBSP_BrushDestroy(Brush);		// Don't need this sucka anymore...

	if (!grBSP_AddBSPBrush_r(BSP, Node->Children[NODE_FRONT], &Front))
		return GR_FALSE;
	if (!grBSP_AddBSPBrush_r(BSP, Node->Children[NODE_BACK], &Back))
		return GR_FALSE;

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_AddBrushInternally
//=====================================================================================
static grBoolean grBSP_AddBrushInternally(grBSP *BSPTree, grBrush *Brush, uint32 Order, grBoolean AutoLight)
{
	grBSP_TopBrush	*TopBrush = NULL;
	grBSP_Brush		*BSPBrush = NULL;
	grBoolean		Ret;

	assert(BSPTree);
	assert(Brush);
	assert(!grBSP_HasBrush(BSPTree, Brush));

	if (BSPTree->AreaChain)
	{
		if (!grBSP_DestroyVisAreas(BSPTree))
			return GR_FALSE;
	}

	// Create a Top level Brush from the editor brush
	TopBrush = grBSP_TopBrushCreateFromBrush(Brush, BSPTree, Order);

	if (!TopBrush)
		goto ExitWithError;

	if (AutoLight)
	{
		grBSP_TopSide		*TopSide;
		int32				i;

		for (TopSide = TopBrush->TopSides, i = 0; i< TopBrush->NumSides; i++, TopSide++)
			TopSide->TopSideFlags |= TOPSIDE_UPDATE_LIGHT;
	}

	// Add the top brush to the list of top brushes in the tree
	TopBrush->Next = BSPTree->TopBrushes;
	BSPTree->TopBrushes = TopBrush;

	// Create a bsp brush
	BSPBrush = grBSP_BrushCreateFromTopBrush(TopBrush, BSPTree);

	if (!BSPBrush)
		goto ExitWithError;

	Ret = GR_TRUE;

	// Start at the top of the tree, and filter the BSPBrush down...
	if (!BSPTree->RootNode)
	{
		if (!grBSP_BuildBSPTree(BSPTree, &BSPBrush, Logic_Lazy, 0))
			goto ExitWithError;
			
		if (!grBSP_CreatePortals(BSPTree, GR_TRUE))
			goto ExitWithError;
	}
	else
	{
		// Update the bsptrees bbox
		grExtBox_Union(&BSPTree->Box, &BSPBrush->Box, &BSPTree->Box);

		if (!grBSP_AddBSPBrush_r(BSPTree, BSPTree->RootNode, &BSPBrush))		// Add brush to existing tree
		{
			goto ExitWithError;
		}
	}

	BSPTree->UpdateFlags |= BSP_UPDATE_FACES;
	BSPTree->UpdateFlags |= BSP_UPDATE_LIGHTS;

	if (!grBSP_UpdateWorldSpaceBox(BSPTree))
		return GR_FALSE;

	BSPTree->DebugInfo.NumBrushes++;

	return GR_TRUE;

	// Error
	ExitWithError:
	{
		if (TopBrush)
			BSPTree->TopBrushes = grBSP_TopBrushCullList(BSPTree->TopBrushes, BSPTree, TopBrush);

		if (BSPBrush)
			grBSP_BrushDestroy(&BSPBrush);

		return GR_FALSE;
	}
}

// FIXME: Remove!!!!
extern int32	NumAirLeafs;
extern int32	NumSolidLeafs;

//=======================================================================================
//	grBSP_BuildBSPTree
//=======================================================================================
static grBoolean grBSP_BuildBSPTree(grBSP *BSPTree, 
									grBSP_Brush **BrushList, 
									grBSP_Logic Logic, 
									grBSP_LogicBalance LogicBalance)
{
	grBSP_Brush	*b;
	int32		NumNonVisFaces;
	int32		i;
	grFloat		Volume;
	grBoolean	Set;

	assert(BSPTree->RootNode == NULL);

	Log_Printf("--- grBSP_BuildBSPTree ---\n");

	// Set static globals
	g_Logic = Logic;
	g_LogicBalance = LogicBalance;

	NumNonVisFaces = 0;

	Set = GR_FALSE;

	for (b=*BrushList ; b ; b=b->Next)
	{
		BSPTree->DebugInfo.NumBrushes++;

		Volume = grBSP_BrushVolume(b, BSPTree);

		if (Volume < GR_BSP_TINY_VOLUME)
			Log_Printf("**WARNING** grBSP_BuildBSPTree: Brush with NULL volume\n");
		
		for (i=0 ; i<b->NumSides ; i++)
		{
			if (!b->Sides[i].Poly)
				continue;

			BSPTree->DebugInfo.NumTotalBrushFaces++;

			if (b->Sides[i].Flags & SIDE_NODE)
				continue;

			if (b->Sides[i].Flags & SIDE_VISIBLE)
				BSPTree->DebugInfo.NumVisibleBrushFaces++;
		}

		if (!Set)
		{
			BSPTree->Box = b->Box;
			Set = GR_TRUE;
		}
		else
		{
			grExtBox_Union(&BSPTree->Box, &b->Box, &BSPTree->Box);
		}
	}
	
	Log_Printf("Total Brushes          : %5i\n", BSPTree->DebugInfo.NumBrushes);
	Log_Printf("Visible Faces          : %5i\n", BSPTree->DebugInfo.NumVisibleBrushFaces);
	Log_Printf("Total Faces            : %5i\n", BSPTree->DebugInfo.NumTotalBrushFaces);

	NumNonVisNodes = 0;
	
	// Create the very first node
	BSPTree->RootNode = grBSPNode_Create(BSPTree);

	BSPTree->RootNode->Box = BSPTree->Box;

	if (Logic >= Logic_Smart)
	{
		BSPTree->RootNode->Volume = grBSP_BrushCreateFromBox(BSPTree, &BSPTree->Box);

		if (grBSP_BrushVolume(BSPTree->RootNode->Volume, BSPTree) < GR_BSP_TINY_VOLUME)
			Log_Printf("**WARNING** grBSP_BuildBSPTree: BAD Tree Volume.\n");
	}
	else
		BSPTree->RootNode->Volume = NULL;

	if (!grBSP_BuildBSPTree_r(BSPTree, BSPTree->RootNode, BrushList))
		return GR_FALSE;

	Log_Printf("Total Nodes            : %5i\n", BSPTree->DebugInfo.NumNodes);
	Log_Printf("Nodes Removed          : %5i\n", NumNonVisNodes);
	Log_Printf("Total Leafs            : %5i\n", BSPTree->DebugInfo.NumLeafs);

	//Log_Printf("Num Air Leafs          : %5i\n", NumAirLeafs);
	//Log_Printf("Num Solid Leafs        : %5i\n", NumSolidLeafs);

#ifdef AREA_DRAWFACE_TEST
	LN_InitList(&(BSPTree->LeafList));

	if (!grBSPNode_MakeLeafList_r(BSPTree->RootNode,&(BSPTree->LeafList)))
		return GR_FALSE;

	Log_Printf("Leaf List Len = %d\n",LN_ListLen(&(BSPTree->LeafList)));
	assert( ((BSPTree->DebugInfo.NumNodes+1)/2) == LN_ListLen(&(BSPTree->LeafList)));
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_BuildBSPTree_r
//=======================================================================================
static grBoolean grBSP_BuildBSPTree_r(grBSP *BSP, grBSPNode *Node, grBSP_Brush **Brushes)
{
	grBSPNode	*NewNode;
	grBSP_Side	*BestSide;
	int32		i;
	grBSP_Brush	*Children[2];

	// find the best plane to use as a splitter
	BestSide = grBSP_GetSplitter(BSP, *Brushes, Node);
	
	if (!BestSide)
	{
		BSP->DebugInfo.NumLeafs++;

		if (!grBSPNode_InitializeLeaf(Node, BSP, *Brushes))
			return GR_FALSE;

		if (Node->Volume)
		{
			assert(g_Logic >= Logic_Smart);
			grBSP_BrushDestroy(&Node->Volume);
		}

		return GR_TRUE;
	}

	BSP->DebugInfo.NumNodes++;

	// This is a splitplane node
	Node->Side = BestSide;
	// Nodes are ALWAYS positive facing
	Node->PlaneIndex = grPlaneArray_IndexGetPositive(BestSide->PlaneIndex);

	// Ref the plane
	if (!grPlaneArray_RefPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex))
		return GR_FALSE;

	// Split the brushes on this node
	if (!grBSPNode_SplitBrushList(Node, BSP, *Brushes, &Children[0], &Children[1]))
   {
		return GR_FALSE;
   }

	// Don't need this list anymore
	grBSP_BrushDestroyList(Brushes);
	
   // Set pointer to zero
   Node->Children[0] = NULL;
   Node->Children[1] = NULL;
	// Allocate children before recursing
	for (i=0 ; i<2 ; i++)
	{
		NewNode = grBSPNode_Create(BSP);

		if (!NewNode)
			goto ExitError;

		NewNode->Parent = Node;				// And a child is born...
		Node->Children[i] = NewNode;
	}
	
	if (Node->Volume)
	{
		assert(g_Logic >= Logic_Smart);

		// Distribute this nodes volume to its children
		grBSP_BrushSplit(Node->Volume, BSP, Node->PlaneIndex, SIDE_NODE, &Node->Children[0]->Volume, &Node->Children[1]->Volume);

		if (!Node->Children[0]->Volume || !Node->Children[1]->Volume)
			Log_Printf("*WARNING* grBSP_BuildBSPTree_r:  Volume was not split on both sides...\n");
	
		grBSP_BrushDestroy(&Node->Volume);
	}

	// Recursively process children
	for (i=0 ; i<2 ; i++)
	{
      if (!grBSP_BuildBSPTree_r(BSP, Node->Children[i], &Children[i]))
      {
			goto ExitError;
      }
	}

	return GR_TRUE;

ExitError:
	for (i=0 ; i<2 ; i++)
	{
      if (Node->Children[i]) {
         grBSPNode_Destroy_r(&Node->Children[i], BSP);
      }
   }
   return GR_FALSE;
}

//=======================================================================================
//	grBSP_TestBrushToPlaneIndex
//=======================================================================================
static grPlane_Side grBSP_TestBrushToPlaneIndex(grBSP *BSP, grBSP_Brush *Brush, grPlaneArray_Index Index, int32 *NumSplits, 
												grBoolean *HintSplit, int32 *EpsilonBrush)
{
	int32			i, j;
	const grPlane	*pPlane;
	grPlane_Side	s;
	grPoly			*p;
	grFloat			d, d_front, d_back;
	int32			Front, Back;
	grBSP_Side		*pSide;

	*NumSplits = 0;
	*HintSplit = GR_FALSE;

#if 0		// Experimental
	// Once a brushes sides has all been picked as nodes, we can assume that it totally
	//	encapsulates all brushes under it in the tree.  In this case, we can start forcing
	//	down both sides of it's children
	if (!(Brush->Flags & BSPBRUSH_FORCEBOTH))
	{
		for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides ; i++, pSide++)
		{
			if (!(pSide->Flags & SIDE_NODE))
				break;
		}

		if (i == Brush->NumSides)
		{
			Brush->Flags |= BSPBRUSH_FORCEBOTH;

			for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides ; i++, pSide++)
				grPoly_Destroy(&pSide->Poly);

			Brush->NumSides = 0;
		}
	}
#endif

	// If the brush uses this planenum, then send it down the side it's oppositely facing
	for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides ; i++, pSide++)
	{
		if (grPlaneArray_IndexIsCoplanarAndFacing(Index, pSide->PlaneIndex))
			return PSIDE_BACK|PSIDE_FACING;
		if (grPlaneArray_IndexIsCoplanarAndNotFacing(Index, pSide->PlaneIndex))
			return PSIDE_FRONT|PSIDE_FACING;
	}
		
	if (Brush->Flags & BSPBRUSH_FORCEBOTH)
		return PSIDE_BOTH;			// If not facing, check to see if forceboth is set...

	// Box on plane side
	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Index);

	assert(pPlane);

	s = grPlane_BoxSide(pPlane, &Brush->Box, GR_BSP_PLANESIDE_EPSILON);

	if (s != PSIDE_BOTH)
		return s;
					   	
	// If on both sides, count the visible faces split
	d_front = d_back = 0.0f;

	for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides ; i++, pSide++)
	{
		grVec3d	*pVert;

		if (pSide->Flags & SIDE_NODE)
			continue;	
	#if 1
		if (!(pSide->Flags & SIDE_VISIBLE))
			continue;		
	#endif

		p = pSide->Poly;

		if (!p)
			continue;

		Front = Back = 0;

		for (pVert = p->Verts, j=0 ; j<p->NumVerts; j++, pVert++)
		{
		#if 1
			d = grPlane_PointDistanceFast(pPlane, pVert);
		#else
			d = DotProduct (pVert, &Plane->Normal) - Plane->Dist;
		#endif

			if (d > d_front)
				d_front = d;
			else if (d < d_back)
				d_back = d;

			if (d > 0.1) 
				Front = 1;
			else if (d < -0.1) 
				Back = 1;
		}

		if (Front && Back)
		{
			if (!(pSide->Flags & SIDE_SKIP))
			{
				(*NumSplits)++;
				if (pSide->Flags & SIDE_HINT)
					*HintSplit = GR_TRUE;
			}
		}
	}

	if ( (d_front > 0.0 && d_front < 1.0) || (d_back < 0.0 && d_back > -1.0) )
		(*EpsilonBrush)++;

#if 0
	if (*NumSplits == 0)
	{	
		if (Front)
			s = PSIDE_FRONT;
		else if (Back)
			s = PSIDE_BACK;
		else
			s = 0;
	}
#endif

	return s;
}

//=======================================================================================
//	grBSP_CheckPlaneAgainstParents
//	Makes sure no plane gets used twice in the tree, from the children up.  
//=======================================================================================
static grBoolean grBSP_CheckPlaneAgainstParents(grPlaneArray_Index Index, grBSPNode *Node)
{
	grBSPNode	*p;

	for (p=Node->Parent ; p ; p=p->Parent)
	{
		if (grPlaneArray_IndexIsCoplanar(p->PlaneIndex, Index))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_CheckPlaneAgainstVolume
//	Makes sure that a potential splitter does not make tiny volumes from the parent volume
//=======================================================================================
static grBoolean grBSP_CheckPlaneAgainstVolume(grBSP *BSP, grPlaneArray_Index Index, grBSPNode *Node)
{
	grBSP_Brush	*Front, *Back;
	grBoolean	Good;

	grBSP_BrushSplit(Node->Volume, BSP, Index, SIDE_NODE, &Front, &Back);

	Good = (Front && Back);

	if (Front)
		grBSP_BrushDestroy(&Front);
	if (Back)
		grBSP_BrushDestroy(&Back);

	return Good;
}

//=======================================================================================
//	grBSP_GetSplitter
//=======================================================================================
static grBSP_Side *grBSP_GetSplitter(grBSP *BSP, grBSP_Brush *Brushes, grBSPNode *Node)
{
	int32			Value, BestValue;
	grBSP_Brush		*Brush, *Test;
	grBSP_Side		*Side, *BestSide;
	int32			i, j, Pass, NumPasses;
	grPlane_Side	s;
	int32			Front, Back, Both, Facing, Splits;
	int32			BSplits;
	int32			BestSplits;
	int32			EpsilonBrush;
	grBoolean		HintSplit;

	if (!Brushes)
		return NULL;

	BestSide	= NULL;
	BestValue	= -999999;
	BestSplits	= 0;

	// The search order goes: 
	//		Visible-Structural, Visible-Detail,
	//		NonVisible-Structural, NonVisible-Detail.
	// If any valid plane is available in a pass, no further passes will be tried.
	NumPasses = 4;
	for (Pass = 0 ; Pass < NumPasses ; Pass++)
	{
		for (Brush = Brushes ; Brush ; Brush=Brush->Next)
		{
			const grPlane		*pPlane;

		#ifdef USE_DETAIL
			if ( (Pass & 1) && !(Brush->Original->Contents & GR_BSP_CONTENTS_DETAIL))
				continue;
			if ( !(Pass & 1) && (Brush->Original->Contents & GR_BSP_CONTENTS_DETAIL))
				continue;
		#endif
			
			for (i=0 ; i<Brush->NumSides ; i++)
			{
				grPlaneArray_Index		Index;

				Side = &Brush->Sides[i];
				
				if (!Side->Poly)
					continue;	// No Poly, so it can't split
				if (Side->Flags & (SIDE_NODE|SIDE_TESTED|SIDE_SPLIT))
					continue;	// Already a node splitter/Or already tested
				if (Side->Flags & SIDE_SKIP)
					continue;
				if (!(Side->Flags & SIDE_VISIBLE) && Pass<2)
					continue;	// Only check visible faces on first pass

				// Treat index as a node (always positive)
				Index = grPlaneArray_IndexGetPositive(Side->PlaneIndex);

				assert(grBSP_CheckPlaneAgainstParents(Index, Node) == GR_TRUE);
				
				if (Node->Volume)
				{
					assert(g_Logic >= Logic_Smart);

					if (!grBSP_CheckPlaneAgainstVolume(BSP, Index, Node))
						continue;	// Would produce a tiny volume
				}
				
				Front = 0;
				Back = 0;
				Both = 0;
				Facing = 0;
				Splits = 0;
				EpsilonBrush = 0;

				for (Test = Brushes ; Test ; Test=Test->Next)
				{
					s = grBSP_TestBrushToPlaneIndex(BSP, Test, Index, &BSplits, &HintSplit, &EpsilonBrush);

					Splits += BSplits;

					if (BSplits && (s&PSIDE_FACING) )
					{
						grErrorLog_AddString(-1, "grBSP_GetSplitter:  Brush non-convex.", NULL);
						return NULL;
					}

					Test->TestSide = s;

					// If the brush shares this face, don't bother
					// testing that facenum as a splitter again
					if (s & PSIDE_FACING)
					{
						Facing++;
						for (j=0 ; j<Test->NumSides ; j++)
						{
							if (grPlaneArray_IndexIsCoplanar(Test->Sides[j].PlaneIndex, Index))
								Test->Sides[j].Flags |= SIDE_TESTED;
						}
					}

					if (s & PSIDE_FRONT)
						Front++;
					if (s & PSIDE_BACK)
						Back++;
					if (s == PSIDE_BOTH)
						Both++;
				}

				// Give a value estimate for using this plane
				Value = 10*Facing - ((10-g_LogicBalance)*Splits) - (g_LogicBalance*abs(Front-Back));
				//Value = 5*Facing - 5*Splits - abs(Front-Back);
				//Value = 0 - ((10-g_LogicBalance)*Splits) - (g_LogicBalance*abs(Front-Back));
				//Value = -10*Splits;
				
				pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Index);

				if (pPlane->Type < 3)
					Value+=10;					// Try to get axial aligned planes at top of tree
				
				Value -= EpsilonBrush*1000;		// Try to avoid splits that will cause tiny leafs

				// Never split a hint side except with another hint
				if (HintSplit && !(Side->Flags & SIDE_HINT) )
					Value = -999999;

				// Save off the side test so we don't need
				// to recalculate it when we actually seperate
				// the brushes
				if (Value > BestValue)
				{
					BestValue = Value;
					BestSide = Side;
					BestSplits = Splits;

					for (Test = Brushes ; Test ; Test=Test->Next)
						Test->Side = Test->TestSide;

					//if (g_Logic == Logic_Lazy)
					//	break;
				}
			}

			if (g_Logic == Logic_Lazy)
			{
				if (BestSide)		// Just take the first side in lazy mode
					break;
			}
		}

		// Bail out now, if we found a good side
		if (BestSide)
		{
			if (Pass > 1)
			{
				//BSP->DebugInfo.NumNodes--;
				NumNonVisNodes++;
			}
			
			if (Pass > 0)						// The node is not visible, or detail, so just mark it detail...
				Node->Flags |= NODE_DETAIL;	
			
			break;
		}
	}

	// Clear all the tested flags we set
	for (Brush = Brushes ; Brush ; Brush=Brush->Next)
	{
		for (i=0, Side = Brush->Sides ; i<Brush->NumSides ; i++, Side++)
			Side->Flags &= ~SIDE_TESTED;
	}

	return BestSide;
}

//=====================================================================================
//	grBSP_FillLeafsFromEntities
//=====================================================================================
static grBoolean grBSP_FillLeafsFromEntities(grBSP *Tree, int32 Fill)
{
	grBoolean	Empty;
	int32		i;

	assert(Tree);

	Empty = GR_FALSE;
	//Entity = NULL;

	//while (Entity = grEntity_SetGetNextEntity(EntitySet, Entity))
	for (i=0; i<1; i++)
	{
		grVec3d			Pos;
		grBSPNode_Leaf	*Leaf;

		//grEntity_GetPosition(Entity, &Pos);
		grVec3d_Set(&Pos, 0.0f, 0.0f, 0.0f);

		// Get the leaf to fill from
		Leaf = grBSPNode_FindLeaf(Tree->RootNode, Tree, &Pos);
		assert(Leaf);

		if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		{
			//Log_Printf("grBSP_FillLeafsFromEntities:  Entity in solid space.\n");
			continue;
		}

		Empty = GR_TRUE;

		// Fill from this leaf
		grBSPNode_LeafFill_r(Leaf, Fill);
	}

	if (!Empty)
	{
		grErrorLog_AddString(-1, "grBSP_FillLeafsFromEntities:  No entities in empty space.\n", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

extern int32 NumFilledLeafs;

//=====================================================================================
//	grBSP_RemoveHiddenLeafs
//=====================================================================================
static grBoolean grBSP_RemoveHiddenLeafs(grBSP *Tree)
{
	assert(Tree);

	Log_Printf("--- grBSP_RemoveHiddenLeafs --- \n");

	if (!grBSP_FillLeafsFromEntities(Tree, 1))
		return GR_FALSE;

	grBSPNode_FillUnTouchedLeafs_r(Tree->RootNode, 1);

	Log_Printf("Num Filled Leafs       : %5i\n", NumFilledLeafs);

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_MarkVisibleTopSides
//=======================================================================================
static grBoolean grBSP_MarkVisibleTopSides(grBSP *Tree)
{
	int32			j;
	grBSP_TopBrush	*TopBrush;

	Log_Printf("--- grBSP_MarkVisibleTopSides--- \n");

	// Clear all the visible flags
	for (TopBrush = Tree->TopBrushes; TopBrush; TopBrush = TopBrush->Next)
	{
		for (j=0 ; j<TopBrush->NumSides ; j++)
			TopBrush->TopSides[j].Flags &= ~SIDE_VISIBLE;
	}
	
	// Set visible flags on the sides that are used by portals
	grBSP_MarkVisibleTopSides_r (Tree, Tree->RootNode);

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_MarkVisibleTopSides_r
//=======================================================================================
static void grBSP_MarkVisibleTopSides_r(grBSP *BSP, grBSPNode *Node)
{
	grBSPNode_Portal	*p;
	int32				s;

	// Recurse to leafs 
	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);
		grBSP_MarkVisibleTopSides_r(BSP, Node->Children[0]);
		grBSP_MarkVisibleTopSides_r(BSP, Node->Children[1]);
		return;
	}
	assert(Node->Leaf);

	if (!Node->Leaf->Contents)
		return;

	for (p=Node->Portals ; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->OnNode)
		{
			assert(p->Nodes[!s]->Flags & NODE_OUTSIDE);	// Only outside nodes don't set onnode for portals
			assert(p->Nodes[!s]->Flags & NODE_LEAF);
			assert(p->Nodes[!s]->Leaf);
			assert(p->Nodes[!s]->Leaf->Node == p->Nodes[!s]);
			continue;
		}

		// For all sides not found for portals, try to find a side
		if (!(p->Flags & PORTAL_SIDE_FOUND))
			grBSPNode_PortalFindTopSide(p, BSP, s);

		// If the portal has a side, mark the side as visible
		if (p->Side)
			p->Side->Flags |= SIDE_VISIBLE;		
	}

}

//=======================================================================================
//	grBSP_CreatePortals
//=======================================================================================
static grBoolean grBSP_CreatePortals(grBSP *Tree, grBoolean IncludeDetail)
{
	assert(Tree);

	grBSP_DestroyAllPortals(Tree);		// First, make sure no portals are in tree

	// Create portals
	Tree->OutsideNode = grBSPNode_InitializeRootPortals(Tree->RootNode, Tree);

	if (!Tree->OutsideNode)
	{
		grErrorLog_AddString(-1, "grBSP_CreatePortals:  grBSPNode_InitializeRootPortals failed.", NULL);
		return GR_FALSE;
	}

	if (!grBSPNode_PartitionPortals_r(Tree->RootNode, Tree, IncludeDetail))
	{
		grErrorLog_AddString(-1, "grBSP_CreatePortals:  grBSPNode_PartitionPortals_r failed.", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSP_DestroyAllPortals
//=======================================================================================
static void grBSP_DestroyAllPortals(grBSP *Tree)
{
	assert(Tree);

	if (!Tree->RootNode)
	{
		assert(!Tree->OutsideNode);
		return;
	}

	grBSPNode_DestroyPortals_r(Tree->RootNode, Tree);

	if (Tree->OutsideNode)
		grBSPNode_Destroy_r(&Tree->OutsideNode, Tree);
}

//=====================================================================================
//	Dynamic Lights
//=====================================================================================

//=====================================================================================
//	CombineDLightWithRGBMapFastLightingModel
//=====================================================================================
static grBoolean CombineDLightWithRGBMapFastLightingModel(grBSP *BSP, int32 *LightData, grBSPNode_Light *Light, grBSPNode_DrawFace *Face)
{
	grBoolean			Hit;
	grFloat				Radius, Dist;
	const grPlane		*pPlane;
	const grTexVec		*pTexVec;
	int32				Sx, Sy, x, y, u, v, Val;
	int32				ColorR, ColorG, ColorB, Radius2, Dist2;
	int32				FixedX, FixedY, XStep, YStep;
	grBSPNode_Lightmap	*Lightmap;
	
	assert(LightData);
	assert(Light);
	assert(Face);
	assert(Face->Lightmap);

	Lightmap = Face->Lightmap;

	pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, Face->TexVecIndex);

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);
	assert(pPlane);

	Radius = Light->Radius;

	Dist = grPlane_PointDistanceFast(pPlane, &Light->Pos);

	// Subtract dist from radius, so we can project light on surface and calculate in 2d
	Radius -= (float)fabs(Dist);

	if (Radius <= 0.0f)		// We can leave now if the dist is > radius
		return GR_FALSE;

	// Calculate where light is projected onto the 2d-plane
	Sx = (int32)(grVec3d_DotProduct(&Light->Pos, &pTexVec->VecU));
	Sy = (int32)(grVec3d_DotProduct(&Light->Pos, &pTexVec->VecV));

	// Align with upper-left of lightmap in 2d space
	Sx -= (int32)(Lightmap->StartU+Face->FixShiftU);
	Sy -= (int32)(Lightmap->StartV+Face->FixShiftV);

	// Scale by the texture scaling (1:21:10 fixed)
	Sx *= Lightmap->XScale;
	Sy *= Lightmap->YScale;

	Hit = GR_FALSE;
	
	ColorR = Light->R;
	ColorG = Light->G;
	ColorB = Light->B;

	Radius2 = (int32)Radius;
	
	XStep = Lightmap->XStep;
	YStep = Lightmap->YStep;

	FixedY = Sy;

	for (v=0; v< Lightmap->Height; v++)
	{
		y = FixedY >> 10;

		if (y < 0)
			y = -y;

		FixedX = Sx;
		
		for (u=0; u< Lightmap->Width; u++)
		{
			x = FixedX >> 10;

			if (x<0)
				x = -x;

			if (x > y)
				Dist2 = (x + (y>>1));
			else
				Dist2 = (y + (x>>1));
			
			if (Dist2 < Radius2)
			{
				Hit = GR_TRUE;
				
				Val = (Radius2 - Dist2);

				*LightData++ += (int32)(Val * ColorR);
				*LightData++ += (int32)(Val * ColorG);
				*LightData++ += (int32)(Val * ColorB);
			}
			else
				LightData+=3;

			FixedX -= XStep;
		}

		FixedY -= YStep;
	}

	return Hit;
}

//=======================================================================================
//	CombineDLightWithRGBMap
//=======================================================================================
static grBoolean CombineDLightWithRGBMap(grBSP *BSP, int32 *LightData, grBSPNode_Light *Light, grBSPNode_DrawFace *Face)
{
	int32				i, p, NumPoints;
	grVec3d				*Points, *pPoint;	
	grVec3d				RGB[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH], *pRGB;		// 5k 
	grBSPNode_Lightmap	*Lightmap;
	grPlane				Plane;
	grFloat				Dist;
	grBoolean			DoubleSided;

	Lightmap = Face->Lightmap;
	assert(Lightmap);

	NumPoints = Lightmap->Width*Lightmap->Height;

	assert(NumPoints < MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH);

	Points = Lightmap->Points;

	if (!Points)
		return GR_FALSE;

	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);

	if (grPlaneArray_IndexSided(Face->PlaneIndex))
		grPlane_Inverse(&Plane);

	DoubleSided = (Face->Contents & GR_BSP_CONTENTS_EMPTY);

	Dist = grPlane_PointDistance(&Plane, &Light->Pos);

	if (!DoubleSided)
	{
		if (Dist < 0.001f)
			return GR_FALSE;		// Light behind surface
	}

	if (grVec3d_DistanceBetween(&Face->Center, &Light->Pos) > Face->Radius + Light->Radius)
		return GR_FALSE;		// Light is NOT in radius of face

	pPoint = Points;
	pRGB = RGB;

	for (p=0; p< NumPoints; p++, pPoint++, pRGB++)
	{
		grVec3d		Vect;
		grFloat		Dist, Angle, Val;

		grVec3d_Subtract(&Light->Pos, pPoint, &Vect);
		Dist = grVec3d_Normalize(&Vect);

		Angle = grVec3d_DotProduct(&Vect, &Plane.Normal);

		if (Angle <= 0.001f)							
		{
			if (DoubleSided)
				Angle = (grFloat)fabs(Angle);
			else 
				goto Skip;
		}
			
		Val = (Light->Radius - Dist) * Angle;

		if (Val <= 0.0f)
			goto Skip;	// Light out of radius for this point

		if (BSP->RootNode)
		{
			if (grBSPNode_RayIntersects_r(BSP->RootNode, BSP, pPoint, &Light->Pos))
				goto Skip;	// Ray is in shadow
		}

		// Add this lights color to the lightmap data
		grVec3d_Scale(&Light->Color, Val, pRGB);
		continue;

		Skip:
		{
			grVec3d_Clear(pRGB);
		}
	}

	pRGB = RGB;

	// Put the light into the lightmaps data, scaling, then clamping to MaxLight
	for (p=0; p< NumPoints; p++, pRGB++)
	{
	#if 0
		grFloat		Max, Max2;

		// Clamp light to a min of 1.0f, and find max
		Max = 0.0f;
		for (i=0; i<3; i++)
		{
			grFloat		Val;

			Val = grVec3d_GetElement(pRGB, i);
			if (Val < 1.0f)
				grVec3d_SetElement(pRGB, i, 1.0f);
			if (Val > Max)
				Max = Val;
		}

		Max2 = min(Max, MaxLight);
			
		// Copy work RGB into real lightmap, clamping to MaxLight
		for (i=0; i<3; i++)
			*LightData++ += (uint32)(grVec3d_GetElement(pRGB, i)*Max2/Max)<<LIGHT_FRACT;
	#else	
		for (i=0; i<3; i++, LightData++)
		{
			grFloat	Val;
			
			Val = min(grVec3d_GetElement(pRGB, i), MaxLight);

			(*LightData) += (uint32)(Val*(1<<LIGHT_FRACT));
		}
	#endif
	}

	return GR_TRUE;
}

//=====================================================================================
//	AddLightType1
//=====================================================================================
static void AddLightType1(int32 *LightDest, uint8 *LightData, int32 Size, int32 Intensity)
{	
	int32	h;

	assert(LightDest != NULL);
	assert(LightData != NULL);

	for (h = 0; h < Size; h++)
	{
		*LightDest++ = *LightData++ * Intensity;
		*LightDest++ = *LightData++ * Intensity;
		*LightDest++ = *LightData++ * Intensity;
	}
}

//=====================================================================================
//	AddLightType2
//=====================================================================================
static void AddLightType2(int32 *LightDest, uint8 *LightData, int32 Size, int32 Intensity)
{	
	int32	h;

	assert(LightDest != NULL);
	assert(LightData != NULL);

	for (h = 0; h < Size; h++)
	{
		*LightDest++ += *LightData++ * Intensity;
		*LightDest++ += *LightData++ * Intensity;
		*LightDest++ += *LightData++ * Intensity;
	}
}

DRV_RGB		BlankLight[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH];
int32		TempRGB32[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH*3];
DRV_RGB		FinalRGB[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH];

//=======================================================================================
//	grBSP_SetupLightmap
//=======================================================================================
static void GRCC grBSP_SetupLightmap(grRDriver_LMapCBInfo *Info, void *LMapCBContext)
{
	int32				Intensity;
	grBSPNode_DrawFace	*pFace;
	int32				i;
	grBSP				*BSP;
	int32				Size;
	grBSPNode_Lightmap	*pLightmap;
	int32				*pRGB1;
	DRV_RGB				*pRGB2;

	pFace = (grBSPNode_DrawFace *)LMapCBContext;
	assert( pFace->Lightmap );
	BSP = pFace->BSP;
	assert(BSP);

	pLightmap = pFace->Lightmap;

	assert(pLightmap->Width <= MAX_LIGHTMAP_WH);
	assert(pLightmap->Height <= MAX_LIGHTMAP_WH);

	Info->Dynamic = (grBoolean)pFace->Lightmap->Dynamic;
	pFace->Lightmap->Dynamic = 0;
	
	if (pFace->DLightVisFrame != BSP->DLightVisFrame /* && Lightmap->NumStyles == 1*/)
	{
		// If there is no dynamic light and only 1 layer, then we can short-curcuit the layering/clamping process
		if (pFace->Lightmap->RGBData[0])
			Info->RGBLight[0] = (DRV_RGB*)pLightmap->RGBData[0];		// Style 0
		else
			Info->RGBLight[0] = BlankLight;

		goto FogOnly;
	}

	// Get the lightmap size
	Size = pLightmap->Width*pLightmap->Height;

	// Layer all the Styles together
	//for (i=0; i< Lightmap->NumStyles; i++)
	for (i=0; i< 1; i++)
	{
		if (!i)
			AddLightType1(TempRGB32, pLightmap->RGBData[i], Size, 1<<LIGHT_FRACT);	// Set for first time
		else	
			AddLightType2(TempRGB32, pLightmap->RGBData[i], Size, 1<<LIGHT_FRACT);	// Merge in with light
	}

	// Merge in the dynamic lights
	if (pFace->DLightVisFrame == BSP->DLightVisFrame)			// Face has some dlights
	{
		for (i=0; i< BSP->NumDLights; i++)
		{
			grBSPNode_Light		*Light;

			if (!(pFace->DLights & (1<<i)))
				continue;		// Light was not touching the face

			Light = &BSP->DLights[i];

			if (Light->Flags & GR_LIGHT_FLAG_FAST_LIGHTING_MODEL)
			{
				if (CombineDLightWithRGBMapFastLightingModel(BSP, TempRGB32, Light, pFace))
					Info->Dynamic = GR_TRUE;
			}
			else 
			{
				if (CombineDLightWithRGBMap(BSP, TempRGB32, Light, pFace))
					Info->Dynamic = GR_TRUE;
			}
		}
	}

	// Clamp and Copy 32-bit data over to 8-bit data
	pRGB1 = TempRGB32;
	pRGB2 = FinalRGB;
	for (i=0; i< Size; i++)
	{
		Intensity = (*pRGB1++) >> LIGHT_FRACT;
		if (Intensity > 255)
			Intensity = 255;
		else if (Intensity < 0 )
			Intensity = 0;

		pRGB2->r = (uint8)Intensity;

		Intensity = (*pRGB1++) >> LIGHT_FRACT;
		if (Intensity > 255)
			Intensity = 255;
		else if (Intensity < 0 )
			Intensity = 0;

		pRGB2->g = (uint8)Intensity;

		Intensity = (*pRGB1++) >> LIGHT_FRACT;
		if (Intensity > 255)
			Intensity = 255;
		else if (Intensity < 0 )
			Intensity = 0;

		pRGB2->b = (uint8)Intensity;

		pRGB2++;
	}

	// Point the lightmap to the 8-bit data
	Info->RGBLight[0] = FinalRGB;

	FogOnly:
	
	// Set fog to NULL for now
	//Lamex
	Info->RGBLight[1] = NULL;
	//Info->RGBLight[1] = Info->RGBLight[1];
}

//========================================================================================
//	UpdateDLights
//========================================================================================
static grBoolean UpdateDLights(grBSP *BSP)
{
	grChain_Link		*Link;

	assert(BSP);

	if (!BSP->RootNode)
	{
		BSP->NumDLights = 0;
		return GR_TRUE;
	}

	if (BSP->RenderRecursion)		
		return GR_TRUE;

	BSP->NumDLights = 0;

	BSP->DLightVisFrame++;

	for (Link = grChain_GetFirstLink(BSP->DLightChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grLight			*Light;
		grBSPNode_Light	*BSPLight;
		grFloat			Brightness;

		Light = (grLight*)grChain_LinkGetLinkData(Link);
		BSPLight = &BSP->DLights[BSP->NumDLights];

		if (!grLight_GetAttributes(Light, &BSPLight->Pos, &BSPLight->Color, &BSPLight->Radius, &Brightness, &BSPLight->Flags))
			return GR_FALSE;

		// Rotate the light into the BSP, so we can send it down the tree
		grXForm3d_Transform(&BSP->WorldToModelXForm, &BSPLight->Pos, &BSPLight->Pos);

		if (BSPLight->Color.X > 1.0f || BSPLight->Color.Y > 1.0f || BSPLight->Color.Z > 1.0f)
			grVec3d_Scale(&BSPLight->Color, 1.0f/255.0f, &BSPLight->Color);

		grVec3d_Scale(&BSPLight->Color, Brightness, &BSPLight->Color);

		BSPLight->R = COLOR_TO_FIXED(BSPLight->Color.X);
		BSPLight->G = COLOR_TO_FIXED(BSPLight->Color.Y);
		BSPLight->B = COLOR_TO_FIXED(BSPLight->Color.Z);

		if (!grBSPNode_SetDLight_r(BSP->RootNode, BSP, BSPLight, 1<<BSP->NumDLights, BSP->DLightVisFrame))
			return GR_FALSE;
		
		BSP->NumDLights++;

		if (BSP->NumDLights >= MAX_VISIBLE_DLIGHTS)
			break;
	}

	return GR_TRUE;
}

//=======================================================================================
//	UpdateObjects
//=======================================================================================
static grBoolean UpdateObjects(grBSP *BSP)
{
	grChain_Link	*Link;

	for (Link = grChain_GetFirstLink(BSP->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSP_Object		*BSPObject;
		grBSPNode_Area		*Area;
		grXForm3d			XForm;

		BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);
		
		grObject_GetXForm(BSPObject->Object, &XForm);

		Area = grBSP_FindArea(BSP, &XForm.Translation);

		// KROUER: use a render flag
		if (Area != NULL) { // only if a valid area exist
			if (BSPObject->Object->Methods->Type == GR_OBJECT_TYPE_ACTOR || BSPObject->Object->Methods->Type == GR_OBJECT_TYPE_TERRAIN) {
				grObject_SetRenderNextPass(BSPObject->Object, GR_FALSE);
				//grActor_SetRenderNextTime((grActor*) BSPObject->Object->Instance, GR_FALSE);
			}
		}

		if (BSPObject->Area == Area)
			continue;		// Area has not changed, so do nothing

		// Remove the object from the old area (if any)
		if (BSPObject->Area)
		{
			assert(grChain_FindLink(BSPObject->Area->ObjectChain, BSPObject->Object));
			grChain_RemoveLinkData(BSPObject->Area->ObjectChain, BSPObject->Object);
		}

		// Then, add it to the new one
		if (Area)
		{
			assert(!grChain_FindLink(Area->ObjectChain, BSPObject->Object));
			grChain_AddLinkData(Area->ObjectChain, BSPObject->Object);
		}

		BSPObject->Area = Area;		// Remember the area
	}

	return GR_TRUE;
}

//=======================================================================================
//	ShutdownDriverCB
//=======================================================================================
static grBoolean GRCC ShutdownDriverCB(DRV_Driver *Driver, void *Context)
{
	grBSP	*Tree = (grBSP*)Context;

	// The GPU geometry belongs to this driver; build it again for the next one.
	grBSP_GpuGeometryDestroy(Tree);
	Tree->GpuDirty = GR_TRUE;

	if (Tree->RootNode)
	{
		if (!grBSPNode_DestroyLightmapTHandles_r(Tree->RootNode, Driver))
			return GR_FALSE;
	}

	Tree->Driver = NULL;

	return GR_TRUE;
}

//=======================================================================================
//	StartupDriverCB
//=======================================================================================
static grBoolean GRCC StartupDriverCB(DRV_Driver *Driver, void *Context)
{
	grBSP	*Tree = (grBSP*)Context;

	if (Tree->RootNode)
	{
		if (!grBSPNode_CreateLightmapTHandles_r(Tree->RootNode, Driver))
			return GR_FALSE;
	}

	Tree->Driver = Driver;		// This is the current render driver (used to manage lightmaps, rendering, etc)

	return GR_TRUE;
}

//========================================================================================
//	grBSP_UpdateWorldSpaceBox
//========================================================================================
static grBoolean grBSP_UpdateWorldSpaceBox(grBSP *BSP)

{
	grExtBox	*Box, *WorldSpaceBox;
	grVec3d		AxisVecs[3], Center;
	int32		i;

	Box = &BSP->Box;

	WorldSpaceBox = &BSP->WorldSpaceBox;

	// First, caclulate center of bsp
	grVec3d_Add(&Box->Min, &Box->Max, &Center);
	grVec3d_Scale(&Center, 0.5f, &Center);

	// Then build some vectors along the box's 3 axis so we can rotate them and get new box
	for(i=0;i < 3;i++)
	{
		grVec3d_SetElement(&AxisVecs[i], i, grVec3d_GetElement(&Box->Max, i) - grVec3d_GetElement(&Center, i));
		grXForm3d_Rotate(&BSP->ModelToWorldXForm, &AxisVecs[i], &AxisVecs[i]);
	}
		
	// Mask off sign bits
#if 0
	for(i=0;i < 3; i++)
	{
		AxisVecs[i].X = fabs(AxisVecs[i].X);
		AxisVecs[i].Y = fabs(AxisVecs[i].Y);
		AxisVecs[i].Z = fabs(AxisVecs[i].Z);
	}
#else
	for(i=0;i < 3;i++)
	{
		int32		j;

		for(j=0;j < 3;j++)
		{
			*((int32*)(&AxisVecs[i])+j)	= *((int32*)(&AxisVecs[i])+j) & 0x7fffffff;
		}
	}
#endif

	// Get new box by summing up the 3 vectors
	for(i=0;i < 3;i++)
	{
		grVec3d_SetElement(&WorldSpaceBox->Max, i,
			  grVec3d_GetElement(&AxisVecs[0], i)
			+ grVec3d_GetElement(&AxisVecs[1], i)
			+ grVec3d_GetElement(&AxisVecs[2], i));
	}

	// Inverse max to get min
	WorldSpaceBox->Min = WorldSpaceBox->Max;
	grVec3d_Inverse(&WorldSpaceBox->Min);

	// Transform to world space
	grVec3d_Add(&BSP->ModelToWorldXForm.Translation, &Center, &Center);
	grVec3d_Add(&WorldSpaceBox->Min, &Center, &WorldSpaceBox->Min);
	grVec3d_Add(&WorldSpaceBox->Max, &Center, &WorldSpaceBox->Max);
		
	// Add some epsilon back in
	for(i=0;i < 3;i++)
	{
		grVec3d_SetElement(&WorldSpaceBox->Min, i, grVec3d_GetElement(&WorldSpaceBox->Min, i) - 1.0f);
		grVec3d_SetElement(&WorldSpaceBox->Max, i, grVec3d_GetElement(&WorldSpaceBox->Max, i) + 1.0f);
	}

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_CreateInternalArrays
//=====================================================================================
static grBoolean grBSP_CreateInternalArrays(grBSP *BSP)
{
	BSP->PlaneArray = grPlaneArray_Create(128);		// 128 planes should be good to start with

	if (!BSP->PlaneArray)
		goto ExitWithError;

	BSP->VertArray = grVertArray_Create(128);		// 128 verts should be good to start with

	if (!BSP->VertArray)
		goto ExitWithError;

	BSP->TexVecArray = grTexVec_ArrayCreate(6);

	if (!BSP->TexVecArray)
		goto ExitWithError;

	BSP->NodeArray = grArray_Create(sizeof(grBSPNode), 128, 128);

	if (!BSP->NodeArray)
		goto ExitWithError;
	
	BSP->DrawFaceArray = grArray_Create(sizeof(grBSPNode_DrawFace), 128, 128);

	if (!BSP->DrawFaceArray)
		goto ExitWithError;
		
	return GR_TRUE;
			
	ExitWithError:
	{
		if (BSP->PlaneArray)
			grPlaneArray_Destroy(&BSP->PlaneArray);

		if (BSP->VertArray)
			grVertArray_Destroy(&BSP->VertArray);

		if (BSP->TexVecArray)
			grTexVec_ArrayDestroy(&BSP->TexVecArray);

		if (BSP->NodeArray)
			grArray_Destroy(&BSP->NodeArray);

		if (BSP->DrawFaceArray)
			grArray_Destroy(&BSP->DrawFaceArray);

		return GR_FALSE;
	}
}

//=====================================================================================
//	grBSP_DestroyInternalArrays
//		NOTE - This will actually destroy anything that depends on the arrays as well...
//=====================================================================================
static void grBSP_DestroyInternalArrays(grBSP *BSP)
{
	assert(BSP);

	// Destroy all vis areas in the BSP
	grBSP_DestroyVisAreas(BSP);

	// Destroy the stuff the depends on the arrays FIRST...
	if (BSP->RootNode)
		grBSPNode_Destroy_r(&BSP->RootNode, BSP);

	if (BSP->TopBrushes)
		grBSP_TopBrushDestroyList(&BSP->TopBrushes, BSP);

	// Then destroy thr actuall arrays
	if (BSP->TexVecArray)
		grTexVec_ArrayDestroy(&BSP->TexVecArray);

	if (BSP->NodeArray)
		grArray_Destroy(&BSP->NodeArray);

	if (BSP->DrawFaceArray)
		grArray_Destroy(&BSP->DrawFaceArray);

	if (BSP->PlaneArray)
		grPlaneArray_Destroy(&BSP->PlaneArray);

	if (BSP->VertArray)
		grVertArray_Destroy(&BSP->VertArray);
}

//=====================================================================================
//	grBSP_ResetObjects
//=====================================================================================
static void grBSP_ResetObjects(grBSP *BSP)
{
	grChain_Link	*Link;

	for (Link = grChain_GetFirstLink(BSP->BSPObjectChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grBSP_Object		*BSPObject;

		BSPObject = (grBSP_Object*)grChain_LinkGetLinkData(Link);

		// If the Object has an area, remove it from the areas list
		if (BSPObject->Area)
		{
			assert(grChain_FindLink(BSPObject->Area->ObjectChain, BSPObject->Object));
			grChain_RemoveLinkData(BSPObject->Area->ObjectChain, BSPObject->Object);
			BSPObject->Area = NULL;
		}
	}
}

//=====================================================================================
//	grBSP_ResetGeometry
//		Destroy ALL geometry, TopBrushes, and any internal arrays that the geometry uses
//=====================================================================================
static grBoolean grBSP_ResetGeometry(grBSP *BSP)
{
	assert(BSP);

	assert(BSP->RenderRecursion == 0);

	// Reset the BSP object list
	grBSP_ResetObjects(BSP);
	// Make sure no portals are in tree
	grBSP_DestroyAllPortals(BSP);
	// Destroy the current set of internal arrays
	grBSP_DestroyInternalArrays(BSP);
	// Create new internal arrays
	grBSP_CreateInternalArrays(BSP);

	// Clear the BSP's Box
	grVec3d_Clear(&BSP->Box.Min);	
	grVec3d_Clear(&BSP->Box.Max);	
	grVec3d_Clear(&BSP->WorldSpaceBox.Min);
	grVec3d_Clear(&BSP->WorldSpaceBox.Max);

	// Clear other various stuff
	BSP->UpdateFlags = 0;

	BSP->NumDLights = 0;
	BSP->DLightVisFrame = 0;

	BSP->NumBrushes = 0;
	memset(&BSP->DebugInfo, 0, sizeof(BSP->DebugInfo));

	return GR_TRUE;
}

//=====================================================================================
//	grBSP_DestroyExternalArrays
//=====================================================================================
static void grBSP_DestroyExternalArrays(grBSP *BSP)
{
	assert(BSP);

	// Un-ref old arrays (if any)
	if (BSP->FaceInfoArray)
	{
		// If one array is set, then they ALL MUST BE SET
		assert(BSP->FaceInfoArray);
		assert(BSP->MaterialArray);
		assert(BSP->LightChain);
		assert(BSP->DLightChain);

		grFaceInfo_ArrayDestroy(&BSP->FaceInfoArray);
		grMaterial_ArrayDestroy(&BSP->MaterialArray);
		grChain_Destroy(&BSP->LightChain);
		grChain_Destroy(&BSP->DLightChain);
	}
}

//=====================================================================================
//	grBSP_CreateExternalArrays
//=====================================================================================
static grBoolean grBSP_CreateExternalArrays(grBSP *BSP, grFaceInfo_Array *FArray, grMaterial_Array *MArray, grChain *LChain, grChain *DLChain)
{
	// If one array is set, they ALL must be set
	assert(FArray);		
	assert(MArray);
	assert(LChain);
	assert(DLChain);

	// There should be no arrays at this point in the bsp
	assert(!BSP->FaceInfoArray);
	assert(!BSP->MaterialArray);
	assert(!BSP->LightChain);
	assert(!BSP->DLightChain);

	// Increase refs on new incoming arrays
	if (!grFaceInfo_ArrayCreateRef(FArray))
		goto ExitWithError;

	if (!grMaterial_ArrayCreateRef(MArray))
		goto ExitWithError;

	if (!grChain_CreateRef(LChain))
		goto ExitWithError;

	if (!grChain_CreateRef(DLChain))
		goto ExitWithError;

	// Save off the arrays
	BSP->FaceInfoArray = FArray;
	BSP->MaterialArray = MArray;
	BSP->LightChain = LChain;
	BSP->DLightChain = DLChain;

	return GR_TRUE;

	ExitWithError:
	{
		if (BSP->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&BSP->FaceInfoArray);
		if (BSP->MaterialArray)
			grMaterial_ArrayDestroy(&BSP->MaterialArray);
		if (BSP->LightChain)
			grChain_Destroy(&BSP->LightChain);
		if (BSP->DLightChain)
			grChain_Destroy(&BSP->DLightChain);

		return GR_FALSE;
	}
}

//=====================================================================================
//	grBSP_OptimizeDrawFaceVerts
//=====================================================================================
static grBoolean grBSP_OptimizeDrawFaceVerts(grBSP *BSP)
{
	grVertArray_Optimizer	*Optimizer;

	if (!BSP->RootNode)
		return GR_TRUE;

	Optimizer = grVertArray_CreateOptimizer(BSP->VertArray);

	if (!Optimizer)
		return GR_FALSE;

	if (!grBSPNode_WeldDrawFaceVerts_r(BSP->RootNode, BSP, Optimizer))
	{
		grVertArray_DestroyOptimizer(BSP->VertArray, &Optimizer);
		return GR_FALSE;
	}
	
	if (!grBSPNode_FixDrawFaceTJuncts_r(BSP->RootNode, BSP, Optimizer))
	{
		grVertArray_DestroyOptimizer(BSP->VertArray, &Optimizer);
		return GR_FALSE;
	}

	grVertArray_DestroyOptimizer(BSP->VertArray, &Optimizer);
	
	return GR_TRUE;
}


// Krouer : VertexBuffer sort functions

typedef struct
{
	grMaterialSpec	*pSpec;
	grChain			*pFaces;
} grFacesPerMaterial;

grChain_Link* grBSP_FindFacesByMaterial(grChain* Chain, const grMaterialSpec* pMatSpec, grFacesPerMaterial** ppSearch)
{
	grChain_Link* pLink;
	grFacesPerMaterial* pSearch;

	pSearch = NULL;

	for (pLink = grChain_GetFirstLink(Chain); pLink; pLink = grChain_LinkGetNext(pLink))
	{
		pSearch = (grFacesPerMaterial*) grChain_LinkGetLinkData(pLink);
		if (pSearch->pSpec == pMatSpec) {
			*ppSearch = pSearch;
			return pLink;
		}
	}

	return NULL;
}

grChain_Link* grBSP_FindRenderSection(grChain* Chain, const grMaterialSpec* pMatSpec, grRenderSectionData** ppSearch)
{
	grChain_Link* pLink;
	grRenderSectionData* pSearch;

	pSearch = NULL;

	for (pLink = grChain_GetFirstLink(Chain); pLink; pLink = grChain_LinkGetNext(pLink))
	{
		pSearch = (grRenderSectionData*) grChain_LinkGetLinkData(pLink);
		if (pSearch->Material == pMatSpec) {
			*ppSearch = pSearch;
			return pLink;
		}
	}

	return NULL;
}

static grBoolean grBSP_CreateVertexBuffer(grBSP* Tree)
{
	unsigned long nVertexCount;
	unsigned long nIndexCount;
	unsigned long nIndexOffset;
	unsigned long nVertexOffset;

	grFacesPerMaterial* pFacesSort;
	grRenderSectionData* pSectionData;

	grChain_Link* pDataLink;
	grChain_Link* pSortedLink;

	// Vertex data
	grTexVert			*pTexVerts;
	grVertex			pTLVerts[MAX_TEMP_VERTS];
	grVertex			*pCurVerts;
	grIndexPoly			*pPoly;
	grVertArray_Index	*pIdxVerts;

	grVertex* pVertexData;
	uint16* pIndexData;
	const grVec3d			*pVert;

	// Matertial Data
	const grMaterial		*pMaterial;
	const grMaterialSpec	*pMatSpec;
	const grFaceInfo		*pFaceInfo;

	grBSPNode_DrawFace* pDrawFace;
	grBSPNode_Leaf* pLeaf;

	nVertexCount = 0;
	nIndexCount = 0;
	nIndexOffset = 0;
	nVertexOffset = 0;

	if (Tree->AreaChain) {
		// Parse all AreaChain
		grChain_Link* pAreaLink;
		grBSPNode_Area* pArea;
		grBSPNode_AreaLeaf* AreaLeaf;

		for (pAreaLink = grChain_GetFirstLink(Tree->AreaChain); pAreaLink; pAreaLink = grChain_LinkGetNext(pAreaLink))
		{
			int f;			

			AreaLeaf = NULL;
			// Parse all faces of the Area
			pArea = (grBSPNode_Area*) grChain_LinkGetLinkData(pAreaLink);
			// Sort faces per Material
			while (AreaLeaf = (grBSPNode_AreaLeaf*)grArray_GetNextElement(pArea->LeafArray, AreaLeaf))
			{
				pLeaf = AreaLeaf->Leaf;
				for(f=0;f<pLeaf->NumDrawFaces;f++)
				{
					pDrawFace = pLeaf->DrawFaces[f];
					pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(Tree->FaceInfoArray, pDrawFace->FaceInfoIndex);
					pMaterial = grMaterial_ArrayGetMaterialByIndex(Tree->MaterialArray, pFaceInfo->MaterialIndex);
					pMatSpec = grMaterial_GetMaterialSpec(pMaterial);

					pDataLink = grBSP_FindFacesByMaterial(pArea->FacesPerMaterialList, pMatSpec, &pFacesSort);
					if (pDataLink)
					{
						// add the face to the material list
						grChain_AddLinkData(pFacesSort->pFaces, pDrawFace);
					}
					else 
					{
						// Create a new items
						pFacesSort = GR_RAM_ALLOCATE_STRUCT(grFacesPerMaterial);
						pFacesSort->pSpec = (grMaterialSpec*) pMatSpec;
						pFacesSort->pFaces = grChain_Create();

						// Add the face to the material list
						grChain_AddLinkData(pFacesSort->pFaces, pDrawFace);

						// Add the material key to the Area list
						grChain_AddLinkData(pArea->FacesPerMaterialList, pFacesSort);
					}

					// Add Portal of Faces has children of the BSP and the Area
					if (pDrawFace->PortalObject)
					{
						grChain_AddLinkData(pArea->ObjectChain, pDrawFace->PortalObject);
						grObject_CreateRef(pDrawFace->PortalObject);
					}

					nVertexCount += pDrawFace->Poly->NumVerts; //grBrush_FaceGetVertCount(pDrawFace->grBrushFace);
					nIndexCount += pDrawFace->Poly->NumVerts;
				}
			}
		}

		// now all faces are sorted
		//if (Tree->pVertexBuffer) {
		//	grEngine_DestroyVertexBuffer(Tree->Engine, &Tree->pVertexBuffer);
		//}
		//if (Tree->pIndexBuffer) {
		//	grEngine_DestroyIndexBuffer(Tree->Engine, &Tree->pIndexBuffer);
		//}
		// create the 2 Buffers
		//Tree->pVertexBuffer = grEngine_CreateVertexBuffer(Tree->Engine, nVertexCount);
		//Tree->pIndexBuffer = grEngine_CreateIndexBuffer(Tree->Engine, nIndexCount);

		// lock the 2 buffers
		//grEngine_LockVertexBuffer(Tree->Engine, Tree->pVertexBuffer, 0, nVertexCount, (void**)&pVertexData);
		//grEngine_LockIndexBuffer(Tree->Engine, Tree->pIndexBuffer, 0, nIndexCount, (void**)&pIndexData);

		// reparse the areas and fill the vertex and index buffer
		for (pAreaLink = grChain_GetFirstLink(Tree->AreaChain); pAreaLink; pAreaLink = grChain_LinkGetNext(pAreaLink))
		{
			pArea = (grBSPNode_Area*) grChain_LinkGetLinkData(pAreaLink);

			for (pSortedLink = grChain_GetFirstLink(pArea->FacesPerMaterialList); pSortedLink; pSortedLink = grChain_LinkGetNext(pSortedLink))
			{
				pFacesSort = (grFacesPerMaterial*) grChain_LinkGetLinkData(pSortedLink);

				pSectionData = GR_RAM_ALLOCATE_STRUCT(grRenderSectionData);
				pSectionData->StartIndex = nIndexOffset;
				pSectionData->StartVertex = nIndexOffset;
				pSectionData->VertexCount = 0;
				pSectionData->IndexCount = 0;
				pSectionData->Material = pFacesSort->pSpec;

				for (pDataLink = grChain_GetFirstLink(pFacesSort->pFaces); pDataLink; pDataLink = grChain_LinkGetNext(pDataLink))
				{
					long IdxVert = 0;
					pDrawFace = (grBSPNode_DrawFace*) grChain_LinkGetLinkData(pDataLink);
					//pBrushFace = pDrawFace->grBrushFace;

					// Fill the TexturedVertex Array
					pPoly = pDrawFace->Poly;
					pIdxVerts = pPoly->Verts;
					pCurVerts = pTLVerts;
					ZeroMemArray(pTLVerts, MAX_TEMP_VERTS);
					pTexVerts = pDrawFace->TVerts;

					for (IdxVert=0; IdxVert<pPoly->NumVerts;IdxVert++)
					{
						pVert = grVertArray_GetVertByIndex(Tree->VertArray, *pIdxVerts);

						pCurVerts->World.X = pVert->X;
						pCurVerts->World.Y = pVert->Y;
						pCurVerts->World.Z = pVert->Z;

						pCurVerts->Color.r = 255;
						pCurVerts->Color.g = 255;
						pCurVerts->Color.b = 255;
						pCurVerts->Color.a = 255;
						
						pCurVerts->u = pTexVerts->u;
						pCurVerts->v = pTexVerts->v;

						// now offset the index of the value already set
						// now copy the index into the index buffer
						pIndexData[nIndexOffset+IdxVert] = (uint16) (nVertexOffset+IdxVert);
	
						pCurVerts++;
						pTexVerts++;
						pIdxVerts++;
					}

					pSectionData->IndexCount += pPoly->NumVerts;

					// now copy the buffer into the vertex buffer - must be formatted to driver wish
					memcpy(pVertexData+nVertexOffset, pTLVerts, pPoly->NumVerts*sizeof(grVertex));
					pSectionData->VertexCount += pPoly->NumVerts;

					// update the render section
					nVertexOffset += pPoly->NumVerts;
				}
			}

			grChain_Destroy(&pArea->FacesPerMaterialList);
		}

		// unlock the buffer
		//grEngine_UnlockIndexBuffer(Tree->Engine, Tree->pIndexBuffer);
		//grEngine_UnlockVertexBuffer(Tree->Engine, Tree->pVertexBuffer);
	} else {
		pDrawFace = NULL;
		// Parse all Faces
		while (pDrawFace = (grBSPNode_DrawFace*) grArray_GetNextElement(Tree->DrawFaceArray, pDrawFace))
		{
			// Sort faces per Material
			pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(Tree->FaceInfoArray, pDrawFace->FaceInfoIndex);
			pMaterial = grMaterial_ArrayGetMaterialByIndex(Tree->MaterialArray, pFaceInfo->MaterialIndex);
			pMatSpec = grMaterial_GetMaterialSpec(pMaterial);

			pDataLink = grBSP_FindFacesByMaterial(Tree->FacesPerMaterialList, pMatSpec, &pFacesSort);
			if (pDataLink)
			{
				// add the face to the material list
				grChain_AddLinkData(pFacesSort->pFaces, pDrawFace);
			}
			else
			{
				// Create a new items
				pFacesSort = GR_RAM_ALLOCATE_STRUCT(grFacesPerMaterial);
				pFacesSort->pSpec = (grMaterialSpec*) pMatSpec;
				pFacesSort->pFaces = grChain_Create();

				// Add the face to the material list
				grChain_AddLinkData(pFacesSort->pFaces, pDrawFace);

				// Add the material key to the Area list
				grChain_AddLinkData(Tree->FacesPerMaterialList, pFacesSort);
			}

			// Add Portal of Faces has children of the BSP
			if (pDrawFace->PortalObject)
			{
				grBSP_AddObject(Tree, pDrawFace->PortalObject);
			}

			nVertexCount += pDrawFace->Poly->NumVerts; //grBrush_FaceGetVertCount(pDrawFace->grBrushFace);
			nIndexCount += pDrawFace->Poly->NumVerts;
		}

		// lock the 2 buffers
		//if (Tree->pVertexBuffer) {
		//	grEngine_DestroyVertexBuffer(Tree->Engine, &Tree->pVertexBuffer);
		//}
		//if (Tree->pIndexBuffer) {
		//	grEngine_DestroyIndexBuffer(Tree->Engine, &Tree->pIndexBuffer);
		//}
		// create the 2 Buffers
		//Tree->pVertexBuffer = grEngine_CreateVertexBuffer(Tree->Engine, nVertexCount);
		//Tree->pIndexBuffer = grEngine_CreateIndexBuffer(Tree->Engine, nIndexCount);

		//grEngine_LockVertexBuffer(Tree->Engine, Tree->pVertexBuffer, 0, nVertexCount, (void**)&pVertexData);
		//grEngine_LockIndexBuffer(Tree->Engine, Tree->pIndexBuffer, 0, nIndexCount, (void**)&pIndexData);
		
		// Fill the buffers
		for (pSortedLink = grChain_GetFirstLink(Tree->FacesPerMaterialList); pSortedLink; pSortedLink = grChain_LinkGetNext(pSortedLink))
		{
			pFacesSort = (grFacesPerMaterial*) grChain_LinkGetLinkData(pSortedLink);

			pSectionData = GR_RAM_ALLOCATE_STRUCT(grRenderSectionData);
			pSectionData->StartIndex = 0;
			pSectionData->StartVertex = 0;
			pSectionData->VertexCount = 0;
			pSectionData->IndexCount = 0;
			pSectionData->Material = pFacesSort->pSpec;

			for (pDataLink = grChain_GetFirstLink(pFacesSort->pFaces); pDataLink; pDataLink = grChain_LinkGetNext(pDataLink))
			{
				long IdxVert = 0;
				pDrawFace = (grBSPNode_DrawFace*) grChain_LinkGetLinkData(pDataLink);

				// Fill the TexturedVertex Array
				pPoly = pDrawFace->Poly;
				pIdxVerts = pPoly->Verts;
				pCurVerts = pTLVerts;
				ZeroMemArray(pTLVerts, MAX_TEMP_VERTS);
				pTexVerts = pDrawFace->TVerts;

				for (IdxVert=0; IdxVert<pPoly->NumVerts;IdxVert++)
				{
					pVert = grVertArray_GetVertByIndex(Tree->VertArray, *pIdxVerts);

					pCurVerts->World.X = pVert->X;
					pCurVerts->World.Y = pVert->Y;
					pCurVerts->World.Z = pVert->Z;

					pCurVerts->Color.r = 255;
					pCurVerts->Color.g = 255;
					pCurVerts->Color.b = 255;
					pCurVerts->Color.a = 255;
					
					pCurVerts->u = pTexVerts->u;
					pCurVerts->v = pTexVerts->v;

					// now offset the index of the value already set
					// now copy the index into the index buffer
					pIndexData[nIndexOffset+IdxVert] = (uint16) (nVertexOffset+IdxVert);

					pCurVerts++;
					pTexVerts++;
					pIdxVerts++;
				}

				pSectionData->IndexCount += pPoly->NumVerts;

				// now copy the buffer into the vertex buffer - must be formatted to driver wish
				memcpy(pVertexData+nVertexOffset, pTLVerts, pPoly->NumVerts*sizeof(grVertex));
				pSectionData->VertexCount += pPoly->NumVerts;

				// update the render section
				nVertexOffset += pPoly->NumVerts;
			}
		}

		// unlock the buffer
		//grEngine_UnlockIndexBuffer(Tree->Engine, Tree->pIndexBuffer);
		//grEngine_UnlockVertexBuffer(Tree->Engine, Tree->pVertexBuffer);

		grChain_Destroy(&Tree->FacesPerMaterialList);
	}
	return GR_TRUE;
}

static grBoolean grBSP_RenderVertexBuffer(grBSP* Tree)
{
	grChain_Link* pLink;
	
	// Enumerate all sections
	for (pLink = grChain_GetFirstLink(Tree->RenderDataList); pLink; pLink = grChain_LinkGetNext(pLink))
	{
		grRenderSectionData* pSection;
		pSection = (grRenderSectionData*) grChain_LinkGetLinkData(pLink);

		// Render the current section
		//Tree->Driver->Render(Tree->pVertexBuffer, Tree->pIndexBuffer, pSection); // TBD
	}
	return GR_TRUE;
}

