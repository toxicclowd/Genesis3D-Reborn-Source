/****************************************************************************************/
/*  grModel.C                                                                           */
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
#include <assert.h>
#include <memory.h> // memset

#pragma message (" Clean up Add/Remove code.  There are some leaks on error...")

#include "Dcommon.h"
#include "Engine.h"

// Public Dependents
#include "grModel.h"

// Private dependents
#include "Errorlog.h"
#include "Camera.h"
#include "grBSP.h"
#include "Ram.h"
#include "grFrustum.h"
#include "Bitmap._h"
#include "grChain.h"
#include "Errorlog.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

typedef struct grModel
{
	int32					RefCount;

	uint16					Flags;

	// The model creates these arrays...
	grBSP					*BSPTree;		// BSPTree for this model
	grChain					*Brushes;

	grFaceInfo_Array		*FaceInfoArray;

	grBSP_RenderMode		RenderMode;

	grBrush_Contents		DefaultContents;
	grXForm3d				XForm;			// Models current XForm

} grModel;

//========================================================================================
//========================================================================================
static grBoolean grModel_WriteHeader(const grModel *Model, grVFile *VFile);
static grBoolean grModel_ReadHeader(grModel *Model, grVFile *VFile);


static grBoolean WriteBrush(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr);
static grBoolean grModel_WriteBrushes(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr);

static grBoolean ReadBrush(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr);
static grBoolean grModel_ReadBrushes(grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr);

//========================================================================================
//	grModel_DestroyBrushes
//========================================================================================
static void grModel_DestroyBrushes(grModel *Model)
{
	grBrush		*Brush;

	assert(Model);
	assert(Model->Brushes);

	Brush = NULL;

	while(Brush = (grBrush*)grChain_GetNextLinkData(Model->Brushes, Brush))
	{
		grChain_RemoveLinkData(Model->Brushes, Brush);
		grBrush_Destroy(&Brush);
	}

	grChain_Destroy(&Model->Brushes);
}

//========================================================================================
//	grModel_Create
//========================================================================================
GRAPI grModel * GRCC grModel_Create(void)
{
	grModel		*Model;

	Model = GR_RAM_ALLOCATE_STRUCT(grModel);

	if (!Model)
		return NULL;

	ZeroMem(Model);

	Model->RefCount = 1;
	
	Model->RenderMode = RenderMode_TexturedAndLit;

	// Create the brush chain (model owns this chain)
	Model->Brushes = grChain_Create();

	if (!Model->Brushes)
		goto ExitWithError;

	// Set the models XForm to identity
	grXForm3d_SetIdentity(&Model->XForm);

	// Set the default contents
	Model->DefaultContents = GR_BSP_CONTENTS_SOLID;

	// Create an "empty" BSPTree
	Model->BSPTree = grBSP_Create();

	if (!Model->BSPTree)
		goto ExitWithError;

	return Model;

	ExitWithError:
	{
		if (Model)
		{
			if (Model->Brushes)
				grChain_Destroy(&Model->Brushes);

			if (Model->BSPTree)
				grBSP_Destroy(&Model->BSPTree);

			grRam_Free(Model);
		}

		return NULL;
	}
}

//========================================================================================
//	grModel_CreateFromFile
//========================================================================================
GRAPI grModel * GRCC grModel_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
{
	grModel		*Model = NULL;

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Model))
			return NULL;

		if (Model)
		{
			if (!grModel_CreateRef(Model))
				return NULL;

			return Model;		// Ptr found in stack, return it
		}
	}

	Model = GR_RAM_ALLOCATE_STRUCT(grModel);

	if (!Model)
		return NULL;

	ZeroMem(Model);

	Model->RefCount = 1;

	Model->RenderMode = RenderMode_TexturedAndLit;

	// Read in the header info
	if (!grModel_ReadHeader(Model, VFile))
		goto ExitWithError;

	if (!grModel_ReadBrushes(Model, VFile, PtrMgr))
		goto ExitWithError;

	// Create an empty BSPTree
	Model->BSPTree = grBSP_Create();

	if (!Model->BSPTree)
		goto ExitWithError;

	grBSP_SetDefaultContents(Model->BSPTree, Model->DefaultContents);
	grBSP_SetXForm(Model->BSPTree, &Model->XForm);

	if (PtrMgr) {
		grBSP_SetEngine(Model->BSPTree, grResourceMgr_GetEngine(grPtrMgr_GetResourceMgr(PtrMgr)));
	}

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Model))
			goto ExitWithError;
	}

	return Model;

	ExitWithError:
	{
		if (Model)
			grModel_Destroy(&Model);

		return NULL;
	}
}

//========================================================================================
//	grModel_WriteToFile
//========================================================================================
GRAPI grBoolean GRCC grModel_WriteToFile(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr)
{
	if (PtrMgr)
	{
		uint32		Count;

		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)Model, &Count))
			return GR_FALSE;

		if (Count)
			return GR_TRUE;		// Ptr was on stack, so return
	}

	// Write out header info
	if (!grModel_WriteHeader(Model, VFile))
	{
		grErrorLog_AddString(-1, "grModel_WriteToFile:  grModel_WriteHeader failed.\n", NULL);
		return GR_FALSE;
	}

	if (!grModel_WriteBrushes(Model, VFile, PtrMgr))
		return GR_FALSE;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)Model))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grModel_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grModel_CreateRef(grModel *Model)
{
	assert(Model);
	
	Model->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grModel_Destroy
//========================================================================================
GRAPI void GRCC grModel_Destroy(grModel **Model)
{
	assert(Model);
	assert(*Model);
	assert((*Model)->RefCount > 0);

	(*Model)->RefCount--;

	if ((*Model)->RefCount == 0)
	{
		// Destroy Brushes
		grModel_DestroyBrushes(*Model);

		// Destroy the bsp tree
		if ((*Model)->BSPTree)
			grBSP_Destroy(&(*Model)->BSPTree);

		if ((*Model)->FaceInfoArray)
			grFaceInfo_ArrayDestroy(&(*Model)->FaceInfoArray);
	}

	grRam_Free(*Model);
}

//========================================================================================
//	grModel_SetArrays
//========================================================================================
GRAPI void GRCC grModel_SetArrays(grModel *Model, grFaceInfo_Array *FArray, grMaterial_Array *MArray, grChain *LChain, grChain *DLChain)
{
	grChain_Link		*Link;

	assert(grFaceInfo_ArrayIsValid(FArray));

	// Tell the current set of brushes the new FaceInfoArray
	for (Link = grChain_GetFirstLink(Model->Brushes); Link; Link = grChain_LinkGetNext(Link))
	{
		grBrush		*Brush;

		Brush = (grBrush*)grChain_LinkGetLinkData(Link);

		if (!grBrush_SetFaceInfoArray(Brush, FArray))
			goto ExitWithError;
	}

	// Update the BSP
	if (!grBSP_SetArrays(Model->BSPTree, FArray, MArray, LChain, DLChain))
		goto ExitWithError;

	if (Model->FaceInfoArray)
		grFaceInfo_ArrayDestroy(&Model->FaceInfoArray);

	if (FArray)
	{
		// Ref the array so we can keep it out of this scope
		if (!grFaceInfo_ArrayCreateRef(FArray))
			goto ExitWithError;
		
		// Remember the array so we can set new incoming brushes to the array
		Model->FaceInfoArray = FArray;	
	}

	return;

	ExitWithError:
	{
		#pragma message ("This function needs to be able to fail, currently it CAN'T!")
		assert(0);
	}
}

//========================================================================================
//	grModel_AddBrush
//========================================================================================
GRAPI grBoolean GRCC grModel_AddBrush(grModel *Model, grBrush *Brush, grBoolean Update, grBoolean AutoLight)
{
	assert(grChain_FindLink(Model->Brushes, Brush) == NULL);

	if (!grBrush_SetFaceInfoArray(Brush, Model->FaceInfoArray))
		return GR_FALSE;

	if (!grChain_AddLinkData(Model->Brushes, Brush))
		return GR_FALSE;

	if (!grBrush_CreateRef(Brush))		// Ref it
		return GR_FALSE;

	if (Update)
	{
		if (!grBSP_AddBrush(Model->BSPTree, Brush, AutoLight))
		{
			grErrorLog_AddString(-1, "grModel_AddBrush:  grBSP_AddBrush failed.", NULL);
			return GR_FALSE;
		}
	}

	return GR_TRUE;
}

//========================================================================================
//	grModel_RemoveBrush
//========================================================================================
GRAPI grBoolean GRCC grModel_RemoveBrush(grModel *Model, grBrush *Brush, grBoolean Update)
{
	assert(grChain_FindLink(Model->Brushes, Brush));

	if (!grChain_RemoveLinkData(Model->Brushes, Brush))
		return GR_FALSE;

	if (Update)
	{
		assert(Model->BSPTree);
		
		if (grBSP_HasBrush(Model->BSPTree, Brush))
		{
			if (!grBSP_RemoveBrush(Model->BSPTree, Brush))
			{
				grErrorLog_AddString(-1, "grModel_RemoveBrush:  grModel_RemoveBrush failed.", NULL);
				assert(0);
				return GR_FALSE;
			}
		}
	}

	grBrush_Destroy(&Brush);	// De-Ref

	return GR_TRUE;
}

//========================================================================================
//	grModel_UpdateBrush
//		Makes the brush current in the bsp
//========================================================================================
GRAPI grBoolean GRCC grModel_UpdateBrush(grModel *Model, grBrush *Brush, grBoolean AutoLight)
{
	assert(Model);
	assert(grChain_FindLink(Model->Brushes, Brush));

	if (!grBSP_UpdateBrush(Model->BSPTree, Brush, AutoLight))
	{
		grErrorLog_AddString(-1, "grModel_UpdateBrush:  grBSP_UpdateBrush failed.", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grModel_GetNextBrush
//========================================================================================
GRAPI grBrush * GRCC grModel_GetNextBrush(const grModel *Model, const grBrush *Start)
{
	assert(Model);
	assert(Model->Brushes);

	return (grBrush *)grChain_GetNextLinkData(Model->Brushes, (void*)Start);
}

//========================================================================================
//	grModel_HasBrush
//========================================================================================
GRAPI grBoolean GRCC grModel_HasBrush(const grModel *Model, const grBrush *Brush)
{
	return (grChain_FindLink(Model->Brushes, (void*)Brush) != NULL);
}

//========================================================================================
//	grModel_UpdateBrushFace
//========================================================================================
GRAPI grBoolean GRCC grModel_UpdateBrushFace(grModel *Model, const grBrush_Face *Face, grBoolean AutoLight)
{
	assert(Model);
	assert(Face);

	return grBSP_UpdateBrushFace(Model->BSPTree, Face, AutoLight);
}

//=======================================================================================
//	grModel_AddObject
//	Adds an object to the Models BSP tree.  The visible objects are then rendered
//	upon calling grBSP_Render...
//=======================================================================================
GRAPI grBoolean GRCC grModel_AddObject(grModel *Model, grObject *Object)
{
	assert(Model);

	return grBSP_AddObject(Model->BSPTree, Object);
}

//=======================================================================================
//	grModel_RemoveObject
//=======================================================================================
GRAPI grBoolean GRCC grModel_RemoveObject(grModel *Model, grObject *Object)
{
	assert(Model);

	return grBSP_RemoveObject(Model->BSPTree, Object);
}

//=======================================================================================
//	grModel_RebuildightsFromPoint
//=======================================================================================
GRAPI grBoolean GRCC grModel_RebuildLightsFromPoint(grModel *Model, const grVec3d *Pos, grFloat Radius)
{
	assert(Model);
	assert(Pos);

	return grBSP_RebuildLightsFromPoint(Model->BSPTree, Pos, Radius);
}

//========================================================================================
//	grModel_RebuilBSP
//========================================================================================
GRAPI grBoolean GRCC grModel_RebuildBSP(grModel *Model, 
										grBSP_Options Options, 
										grBSP_Logic Logic, 
										grBSP_LogicBalance LogicBalance)
{
	if (!grChain_GetLinkCount(Model->Brushes))
		return GR_TRUE;		// No brushes in pool

	grBSP_RebuildGeometry(	Model->BSPTree, 
							Model->Brushes, 
							Options, 
							Logic, 
							LogicBalance);

	return GR_TRUE;
}

//========================================================================================
//	grModel_RebuildBSPFaces
//========================================================================================
GRAPI grBoolean GRCC grModel_RebuildBSPFaces(grModel *Model)
{
	assert(Model);

	if (!grBSP_RebuildFaces(Model->BSPTree))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grModel_PatchLighting
//========================================================================================
GRAPI grBoolean GRCC grModel_PatchLighting(grModel *Model)
{
	assert(Model);

	return grBSP_PatchLighting(Model->BSPTree);
}

//========================================================================================
//	grModel_Rebuildights
//========================================================================================
GRAPI grBoolean GRCC grModel_RebuildLights(grModel *Model)
{
	assert(Model);

	return grBSP_RebuildLights(Model->BSPTree);
}

//========================================================================================
//	grModel_SetEngine
//========================================================================================
GRAPI grBoolean GRCC grModel_SetEngine(grModel *Model, grEngine *Engine)
{
	grBSP_SetEngine(Model->BSPTree, Engine);

	return GR_TRUE;
}

//========================================================================================
//	grModel_SetWorld
//========================================================================================
GRAPI grBoolean	GRCC grModel_SetWorld(grModel *Model, grWorld *World)
{
	grBSP_SetWorld(Model->BSPTree, World);

	return GR_TRUE;
}


//========================================================================================
//	grModel_SetRenderOptions
//========================================================================================
GRAPI grBoolean GRCC grModel_SetRenderOptions(grModel *Model, grBSP_RenderMode RenderMode)
{
	Model->RenderMode = RenderMode;

	// Set the render options
	if (!grBSP_SetRenderMode(Model->BSPTree,Model->RenderMode))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grModel_SetDefaultContents
//========================================================================================
GRAPI grBoolean GRCC grModel_SetDefaultContents(grModel *Model, grBrush_Contents DefaultContents)
{
	Model->DefaultContents = DefaultContents;

	// Set the render options
	if (!grBSP_SetDefaultContents(Model->BSPTree, DefaultContents))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grModel_Render
//========================================================================================
GRAPI grBoolean GRCC grModel_Render(grModel *Model, grCamera *Camera, grFrustum *CameraSpaceFrustum)
{
	assert(Model);
	assert(Camera);
	assert(CameraSpaceFrustum);

	// Render the models BSP tree
	if (!grBSP_RenderAndVis(Model->BSPTree, Camera, CameraSpaceFrustum))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grModel_RayIntersectsBrushes
//========================================================================================
GRAPI grBoolean GRCC grModel_RayIntersectsBrushes(const grModel *Model, const grVec3d *Front, const grVec3d *Back, grBrushRayInfo *Info)
{
	assert(Model);

	return grBSP_RayIntersectsBrushes(Model->BSPTree, Front, Back, Info);
}

//========================================================================================
//	grModel_SetBrushFaceCB
//========================================================================================
GRAPI grBoolean GRCC grModel_SetBrushFaceCB(grModel *Model, grBSPNode_DrawFaceCB *CB, void *Context)
{
	assert(Model);

	return grBSP_SetBrushFaceCB(Model->BSPTree, CB, Context);
}

//========================================================================================
//	grModel_SetBrushFaceCBOnOff
//========================================================================================
GRAPI grBoolean GRCC grModel_SetBrushFaceCBOnOff(grModel *Model, const grBrush_Face *Face, grBoolean OnOff)
{
	assert(Model);
	assert(Face);

	return grBSP_SetBrushFaceCBOnOff(Model->BSPTree, Face, OnOff);
}

grBSP_DebugInfo	FakeBSPInfo;

//========================================================================================
//	grModel_GetBSPDebugInfo
//========================================================================================
GRAPI const grBSP_DebugInfo * GRCC grModel_GetBSPDebugInfo(const grModel *Model)
{
	assert(Model);

	return grBSP_GetDebugInfo(Model->BSPTree);
}

//========================================================================================
//	grModel_Collision
//	Returns GR_TRUe if there was a collision, GR_FALSE otherwise
//========================================================================================
GRAPI grBoolean GRCC grModel_Collision(	const grModel	*Model, 
										const grExtBox	*Box, 
										const grVec3d	*Front, 
										const grVec3d	*Back, 
										grVec3d			*Impact, 
										grPlane			*Plane)
{
	assert(Model);
	assert(Front && Back);
	//assert(Impact); // Icestorm: They CAN be NULL
	//assert(Plane);

	return grBSP_Collision(Model->BSPTree, Box, Front, Back, Impact, Plane);
}

// Added by Icestorm
//========================================================================================
//	grModel_ChangeBoxCollision
//	Returns GR_TRUE if there was a collision, GR_FALSE otherwise
//========================================================================================
GRAPI grBoolean GRCC grModel_ChangeBoxCollision(	const grModel	*Model, 
												const grVec3d	*Pos, 
												const grExtBox	*FrontBox, 
												const grExtBox	*BackBox, 
												grExtBox		*ImpactBox, 
												grPlane			*Plane)
{
	assert(Model);
	assert(FrontBox && BackBox);
	assert(Pos);
	//assert(ImpactBox);// Icestorm: They CAN be NULL
	//assert(Plane);

	return grBSP_ChangeBoxCollision(Model->BSPTree, Pos, FrontBox, BackBox, ImpactBox, Plane);
}

//========================================================================================
//	grModel_SetXForm
//========================================================================================
GRAPI grBoolean GRCC grModel_SetXForm(grModel *Model, const grXForm3d *XForm)
{
	// Save the XForm
	Model->XForm = *XForm;
	
	if (!grBSP_SetXForm(Model->BSPTree, XForm))
		return GR_FALSE;
	
	return GR_TRUE;
}

//========================================================================================
//	grModel_GetXForm
//========================================================================================
GRAPI const grXForm3d * GRCC grModel_GetXForm(grModel *Model )
{
	return &Model->XForm;
}

//========================================================================================
//	****** local static functions ********
//========================================================================================

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
		((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define GR_MODEL_TAG				MAKEFOURCC('G', 'E', 'M', 'F')		// 'GE' 'M'odel 'F'ile
#define GR_MODEL_VERSION			0x0000

//========================================================================================
//	grModel_WriteHeader
//========================================================================================
static grBoolean grModel_WriteHeader(const grModel *Model, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(Model);
	assert(VFile);

	// Write TAG
	Tag = GR_MODEL_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_MODEL_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	// Save off the flags
	if (!grVFile_Write(VFile, &Model->Flags, sizeof(Model->Flags)))
		return GR_FALSE;

	// Save DefaultContents
	if (!grVFile_Write(VFile, &Model->DefaultContents, sizeof(Model->DefaultContents)))
		return GR_FALSE;

	// Save XForm
	if (!grVFile_Write(VFile, &Model->XForm, sizeof(Model->XForm)))
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
// grModel_ReadHeader
//========================================================================================
static grBoolean grModel_ReadHeader(grModel *Model, grVFile *VFile)
{
	uint32		Tag;
	uint16		Version;

	assert(Model);
	assert(VFile);

	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	if (Tag != GR_MODEL_TAG)
		return GR_FALSE;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	if (Version != GR_MODEL_VERSION)
		return GR_FALSE;

	// Read Flags
	if (!grVFile_Read(VFile, &Model->Flags, sizeof(Model->Flags)))
		return GR_FALSE;

	// Read DefaultContents
	if (!grVFile_Read(VFile, &Model->DefaultContents, sizeof(Model->DefaultContents)))
		return GR_FALSE;

	// Read XForm
	if (!grVFile_Read(VFile, &Model->XForm, sizeof(Model->XForm)))
		return GR_FALSE;

	return GR_TRUE;

}

//========================================================================================
//	ReadBrush
//========================================================================================
static grBoolean ReadBrush(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	*LinkData = grBrush_CreateFromFile(VFile, PtrMgr);
		
	if (!(*LinkData))
		return GR_FALSE;

	// Icestorm: Double ref. (see grBrush_CreateFromFile)
	//if (!grBrush_CreateRef(*LinkData))		// Ref it
	//	return GR_FALSE;

	return GR_TRUE;
}


//========================================================================================
//	WriteBrush
//========================================================================================
static grBoolean WriteBrush(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr)
{
	if (!grBrush_WriteToFile((grBrush *)*LinkData, VFile, PtrMgr))
		return GR_FALSE;

	return GR_TRUE;
}



//========================================================================================
//	grModel_ReadBrushes
//========================================================================================
static grBoolean grModel_ReadBrushes(grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr)
{
	// Read in the brush chain (The model owns/created this chain)
	Model->Brushes = grChain_CreateFromFile(VFile, ReadBrush, NULL, PtrMgr);
	
	if (!Model->Brushes)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grModel_WriteBrushes
//========================================================================================
static grBoolean grModel_WriteBrushes(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr)
{
	// Write out brush chain (The model owns/created this chain)
	if (!grChain_WriteToFile(Model->Brushes, VFile, WriteBrush, NULL, PtrMgr))
	{
		grErrorLog_AddString(-1, "grModel_WriteToFile:  grChain_WriteToFile failed for Brushes.\n", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

