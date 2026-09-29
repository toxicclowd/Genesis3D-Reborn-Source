/****************************************************************************************/
/*  JEBSPNODE_DRAWFACE.C                                                                */
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

#include "grBSP._h"
#include "Dcommon.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grIndexPoly.h"
#include "grFaceInfo.h"
#include "grMaterial.h"

#include "Bitmap._h"
#include "Ram.h"


//=======================================================================================
//	grBSPNode_DrawFaceCreate
//=======================================================================================
grBSPNode_DrawFace *grBSPNode_DrawFaceCreate(grBSP *BSP)
{
	grBSPNode_DrawFace		*Face;

#ifdef DRAWFACE_USE_JE_RAM
	Face = GR_RAM_ALLOCATE_STRUCT(grBSPNode_DrawFace);
#else
	Face = (grBSPNode_DrawFace *)grArray_GetNewElement(BSP->DrawFaceArray);
#endif

	if (!Face)
		return NULL;

	ZeroMem(Face);

	Face->FaceInfoIndex = GR_FACEINFO_ARRAY_NULL_INDEX;
	Face->TexVecIndex = GR_TEXVEC_ARRAY_NULL_INDEX;

	Face->BSP = BSP;

	return Face;
}

//=======================================================================================
//	grBSPNode_DrawFaceDestroy
//=======================================================================================
void grBSPNode_DrawFaceDestroy(grBSPNode_DrawFace **Face, grBSP *BSP)
{
	grBSPNode_DrawFace		*DFace;

	assert(Face);
	assert(*Face);

	DFace = *Face;

	if (DFace->PortalObject)
		grObject_Destroy(&DFace->PortalObject);

	if (DFace->PortalXForm)
	{
		grRam_Free(DFace->PortalXForm);
		DFace->PortalXForm = NULL;
	}

	if (DFace->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX)
		grFaceInfo_ArrayRemoveFaceInfo(BSP->FaceInfoArray, &DFace->FaceInfoIndex);

	if (DFace->PlaneIndex != GR_PLANEARRAY_NULL_INDEX)
		grPlaneArray_RemovePlane(BSP->PlaneArray, &DFace->PlaneIndex);

	if (DFace->TexVecIndex != GR_TEXVEC_ARRAY_NULL_INDEX)
		grTexVec_ArrayRemoveTexVec(BSP->TexVecArray, &DFace->TexVecIndex);

	if (DFace->Poly)
	{
		int32		v;

		// Remove all the verts from the global vertarray 
		for (v=0; v< DFace->Poly->NumVerts; v++)
			grVertArray_RemoveVert(BSP->VertArray, &DFace->Poly->Verts[v]);

		grIndexPoly_Destroy(&DFace->Poly);
	}

	if (DFace->TVerts)
	{
		grRam_Free(DFace->TVerts);
		DFace->TVerts = NULL;
	}

	// BEGIN - Hardware T&L - paradoxnj 4/7/2005
	//if (DFace->Lightmap)
	//	grBSPNode_LightmapDestroy(&DFace->Lightmap, BSP);
	// END - Hardware T&L - paradoxnj 4/7/2005

#ifdef DRAWFACE_USE_JE_RAM
	grRam_Free(*Face);
#else
	{
		grBoolean	Ret;
		Ret = grArray_FreeElement(BSP->DrawFaceArray, *Face);
		assert(Ret == GR_TRUE);
	}
#endif

	*Face = NULL;
}

//=======================================================================================
//	grBSPNode_DrawFaceSetFaceInfoIndex
//=======================================================================================
grBoolean grBSPNode_DrawFaceSetFaceInfoIndex(grBSPNode_DrawFace *DFace, grBSP *BSP, grFaceInfo_ArrayIndex Index)
{
	assert(DFace);
	assert(Index != GR_FACEINFO_ARRAY_NULL_INDEX);

	if (DFace->FaceInfoIndex != GR_FACEINFO_ARRAY_NULL_INDEX)
		grFaceInfo_ArrayRemoveFaceInfo(BSP->FaceInfoArray, &DFace->FaceInfoIndex);

	if (!grFaceInfo_ArrayRefFaceInfoIndex(BSP->FaceInfoArray, Index))
		return GR_FALSE;

	DFace->FaceInfoIndex = Index;
	
	return GR_TRUE;
}

typedef struct 
{
	const grPlane		*Plane;
	const grXForm3d		*FaceXForm;
	grWorld				*World;
	grCamera			*Camera;
	grFrustum			*Frustum;
} PortalMsgData;

//=======================================================================================
//	grBSPNode_DrawFaceCreateUVInfo
//=======================================================================================
grBoolean grBSPNode_DrawFaceCreateUVInfo(grBSPNode_DrawFace *Face, grBSP *BSP)
{
	const grTexVec		*pTexVec;
	grTexVert			*pTVert;
	int32				v;

	if (Face->TVerts)
		grRam_Free(Face->TVerts);

	Face->TVerts = GR_RAM_ALLOCATE_ARRAY(grTexVert, Face->Poly->NumVerts);

	if (!Face->TVerts)	
		return GR_FALSE;

	// Grab the locked texture vectors for uv calculations
	pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, Face->TexVecIndex);
	assert(pTexVec);

	for (pTVert = Face->TVerts, v=0; v< Face->Poly->NumVerts; v++, pTVert++)
	{
		const grVec3d	*pVert;

		pVert = grVertArray_GetVertByIndex(BSP->VertArray, Face->Poly->Verts[v]);
		assert(pVert);

		// Get the U,V's by projecting the vert onto the texture axis that for this face
		pTVert->u = grVec3d_DotProduct(pVert, &pTexVec->VecU);
		pTVert->v = grVec3d_DotProduct(pVert, &pTexVec->VecV);
	}

	// Adjust the uv's as close to the origin as possible without effecting their appearance
	#if 1
	{
		grFloat				ShiftU, ShiftV;
		int32				Width, Height;
		const grMaterial	*pMaterial;
		const grBitmap		*pBitmap;
#ifndef _USE_BITMAPS
		const grMaterialSpec *pMatSpec;
		const grTexture*	pTexture;
#endif
		const grFaceInfo	*pFaceInfo;

		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, Face->FaceInfoIndex);
		assert(pFaceInfo);
		pMaterial = grMaterial_ArrayGetMaterialByIndex(BSP->MaterialArray, pFaceInfo->MaterialIndex);
#ifdef _USE_BITMAPS
		pBitmap = grMaterial_GetBitmap(pMaterial);

		if (pBitmap)
		{
			Width = grBitmap_Width(pBitmap);
			Height = grBitmap_Width(pBitmap);

			ShiftU = (grFloat)(((int32)(Face->TVerts[0].u / (grFloat)Width))*Width);
			ShiftV = (grFloat)(((int32)(Face->TVerts[0].v / (grFloat)Height))*Height);

			ShiftU *= (pFaceInfo->DrawScaleU/pFaceInfo->LMapScaleU);
			ShiftV *= (pFaceInfo->DrawScaleV/pFaceInfo->LMapScaleV);

			Face->FixShiftU = ShiftU;
			Face->FixShiftV = ShiftV;

			for (pTVert = Face->TVerts, v=0; v< Face->Poly->NumVerts; v++, pTVert++)
			{
				pTVert->u -= ShiftU;
				pTVert->v -= ShiftV;
			}
		}
#else
		pMatSpec = grMaterial_GetMaterialSpec(pMaterial);

		if (pMatSpec)
		{
			pTexture = grMaterialSpec_GetLayerTexture(pMatSpec, 0);
			if (pTexture) {
			} else { // Keep backward compatibility
				pBitmap = grMaterialSpec_GetLayerBitmap(pMatSpec, 0);
				if (pBitmap) {
					Width = grBitmap_Width(pBitmap);
					Height = grBitmap_Width(pBitmap);

					ShiftU = (grFloat)(((int32)(Face->TVerts[0].u / (grFloat)Width))*Width);
					ShiftV = (grFloat)(((int32)(Face->TVerts[0].v / (grFloat)Height))*Height);

					ShiftU *= (pFaceInfo->DrawScaleU/pFaceInfo->LMapScaleU);
					ShiftV *= (pFaceInfo->DrawScaleV/pFaceInfo->LMapScaleV);

					Face->FixShiftU = ShiftU;
					Face->FixShiftV = ShiftV;

					for (pTVert = Face->TVerts, v=0; v< Face->Poly->NumVerts; v++, pTVert++)
					{
						pTVert->u -= ShiftU;
						pTVert->v -= ShiftV;
					}
				}
			}
		}
#endif
	}
	#endif
	
	return GR_TRUE;
}

extern grBoolean h_LeftHanded;

//=======================================================================================
//	grBSPNode_DrawFaceRender
//=======================================================================================
void grBSPNode_DrawFaceRender(const grBSPNode_DrawFace *Face, grBSP *BSP, grBSPNode_SceneInfo *SceneInfo, uint32 ClipFlags)
{
	grIndexPoly			*Poly;
	grVertArray_Index	*pIVert;
	grLVertex			LVerts1[MAX_TEMP_VERTS], LVerts2[MAX_TEMP_VERTS];
	grLVertex			*pLVert;
	grTLVertex			TLVerts[MAX_TEMP_VERTS];
	int32				i, NumVerts1;
	grTexVert			*pTVert;
	grFrustum_LClipInfo	ClipInfo;
	const grFaceInfo	*pFaceInfo;
	const grMaterial	*pMaterial;
	const grBitmap		*pBitmap;
	uint32				Flags;
#ifndef _USE_BITMAPS
	const grMaterialSpec*		pMatSpec;
//	grTexture*			pTexture;
#endif

	assert(Face);
	assert(BSP);
	assert(SceneInfo);

	g_WorldDebugInfo.NumTransformedPolys++;

	// Get the poly pointer, and num verts to start with
	Poly = Face->Poly;
	NumVerts1 = Poly->NumVerts;

	assert(NumVerts1+4 < MAX_TEMP_VERTS);

	// Copy the verts into a nice linear array
	pLVert = LVerts1;				// Fill in this array with x,y,z,u,v,r,g,b...
	pIVert = Poly->Verts;			// index in to VertArray
	pTVert = Face->TVerts;			// Source u,v,r,g,b

	// Copy the index verts, and the uvrgb's into a grLVertex structure
	for (i=0; i<NumVerts1; i++)
	{
		const grVec3d	*pSrcVert;

		// FIXME:  Get rid of this crap-shit, and look directly into the array
		pSrcVert = grVertArray_GetVertByIndex(BSP->VertArray, *pIVert);

		pLVert->X = pSrcVert->X;
		pLVert->Y = pSrcVert->Y;
		pLVert->Z = pSrcVert->Z;
	#if 0
		pLVert->r = pTVert->r;
		pLVert->g = pTVert->g;
		pLVert->b = pTVert->b;
	#endif
		
		pLVert->u = pTVert->u;
		pLVert->v = pTVert->v;

		pLVert++;
		pTVert++;
		pIVert++;
	}

	ClipInfo.NumSrcVerts = NumVerts1;
	ClipInfo.SrcVerts = LVerts1;

	ClipInfo.Work1 = LVerts1;
	ClipInfo.Work2 = LVerts2;

	ClipInfo.ClipFlags = ClipFlags;

#if 0
	// Clip the verts against the frustum
	if (0)//pFaceInfo->Flags & FACEINFO_FLAGS_GOURAUD)
	{
		// Clip UV, and RGB
		if (!grFrustum_ClipLVertsXYZUVRGB(SceneInfo->Frustum, &ClipInfo))
			return;		// Poly was clipped away
	}
	else
#endif
	{
		// Just clip UV
		if (!grFrustum_ClipLVertsXYZUV(SceneInfo->Frustum, &ClipInfo))
			return;		// Poly was clipped away
	}

	// BEGIN - Hardware T&L - paradoxnj 4/7/2005

	// Transform from Model to Camera Space 
	//	(The ModelToWorld and WorldToCamera XForms are combined here)
	for (pLVert = ClipInfo.DstVerts, i=0; i< ClipInfo.NumDstVerts; i++, pLVert++)
		grXForm3d_Transform(&SceneInfo->ModelToCameraXForm, (grVec3d*)pLVert, (grVec3d*)pLVert);

	// END - Hardware T&L - paradoxnj 4/7/2005

	// FIXME:  All this shit can be cached out (maybe cache out bitmaps on the drawfaces?)
	pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, Face->FaceInfoIndex);
	pMaterial = grMaterial_ArrayGetMaterialByIndex(BSP->MaterialArray, pFaceInfo->MaterialIndex);
#ifdef _USE_BITMAPS
	pBitmap = grMaterial_GetBitmap(pMaterial);
#else
	pMatSpec = grMaterial_GetMaterialSpec(pMaterial);
    if (pMatSpec == NULL) return;
	pBitmap = grMaterialSpec_GetLayerBitmap(pMatSpec, 0);
#pragma message("Krouer: think to add the grTexture support here")
#endif

	Flags = SceneInfo->DefaultRenderFlags;

#if 1
	if (Face->PortalObject)		// Special portal face
	{
		grFrustum			PortalFrustum;
		grVec3d				Origin;
		grLVertex			*pVerts2;
		grPlane				FacePlane;
		PortalMsgData		MsgData;
		grXForm3d			WorldSpaceFaceXForm;

		assert(Face->PortalObject->Methods->Type == GR_OBJECT_TYPE_PORTAL);
		assert(Face->PortalXForm);

		g_WorldDebugInfo.NumPortals++;

		grVec3d_Clear(&Origin);

		pVerts2 = ClipInfo.DstVerts;

		// Create the frustum from the portal in camera space (This way the frustum is in camera space)
		grFrustum_SetFromLVerts2(&PortalFrustum, pVerts2, ClipInfo.NumDstVerts, h_LeftHanded);

		// Get the face plane
		FacePlane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);

		if (!grPlaneArray_IndexSided(Face->PlaneIndex))
			grPlane_Inverse(&FacePlane);	// Point the Normal opposite of face

		// BEGIN - Hardware T&L - paradoxnj 4/7/2005

		// Transform the plane into world space 
		grPlane_Transform(&FacePlane, &BSP->ModelToWorldXForm, &FacePlane);
		// Transform the FaceXForm into WorldSpace
		grXForm3d_Multiply(&BSP->ModelToWorldXForm, Face->PortalXForm, &WorldSpaceFaceXForm);

		// END - Hardware T&L - paradoxnj 4/7/2005

		// Setup the message data
		//MsgData.World = (grWorld*)Face->PortalObject->Instance;
		MsgData.Plane = &FacePlane;
		MsgData.FaceXForm = &WorldSpaceFaceXForm;
		MsgData.World = BSP->World;
		MsgData.Camera = SceneInfo->Camera;
		MsgData.Frustum = &PortalFrustum;
			
		BSP->Driver->BeginBatch();

		// Render the portal
		grObject_SendMessage(Face->PortalObject, 0, &MsgData);
	}
#endif

	// Set up the verts
	{
		grFloat		Alpha;

		if (pFaceInfo->Flags & FACEINFO_TRANSPARENT)
		{
			Flags |= GR_RENDER_FLAG_ALPHA;
			Alpha = pFaceInfo->Alpha;
		}
		else
		{
			// Only non transparent polys use spans
			if (BSP->DefaultContents & GR_BSP_CONTENTS_SOLID)
				Flags |= (GR_RENDER_FLAG_SWRITE | GR_RENDER_FLAG_STEST);

			Alpha = 255.0f;
		}

		for (pLVert = ClipInfo.DstVerts, i=0; i<ClipInfo.NumDstVerts; i++, pLVert++)
		{
		#if 0
			grFloat			Val;

			Val = (grFloat)(((uint32)Face<<1)&255);

			pLVert->r = pLVert->g = pLVert->b = Val;
		#else
			pLVert->r = pLVert->g = pLVert->b = 255.0f;
		#endif
			pLVert->a = Alpha;
		}
	}

	{
		if (h_LeftHanded)		// Big hack-a-rama
			Flags |= GR_RENDER_FLAG_COUNTER_CLOCKWISE;
	}

	// BEGIN - Hardware T&L - paradoxnj 4/7/2005

	// Transform and project the point
	grCamera_ProjectAndClampLArray(SceneInfo->Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	// END - Hardware T&L - paradoxnj 4/7/2005

	if (!(pFaceInfo->Flags & FACEINFO_RENDER_PORTAL_ONLY))
	{
		g_WorldDebugInfo.NumRenderedPolys++;
	
		if (pBitmap)
		{
			grTexture	*THandle;
			grRDriver_Layer		Layers[2];
	
			THandle = grBitmap_GetTHandle(pBitmap);
			assert(THandle);
	
			Layers[0].THandle = THandle;
			Layers[0].Rop = Rop_Multiply;
			Layers[0].ShiftU = pFaceInfo->ShiftU;
			Layers[0].ShiftV = pFaceInfo->ShiftV;
			Layers[0].ScaleU = pFaceInfo->DrawScaleU/pFaceInfo->LMapScaleU;
			Layers[0].ScaleV = pFaceInfo->DrawScaleV/pFaceInfo->LMapScaleV;
	
			// BEGIN - Hardware T&L - paradoxnj 4/7/2005
			if (Face->Lightmap && BSP->RenderMode == RenderMode_TexturedAndLit)
			{
				
				grBSPNode_Lightmap	*pLightmap;
	
				pLightmap = Face->Lightmap;
				assert(pLightmap->THandle);
	
				Layers[1].THandle = pLightmap->THandle;
				Layers[1].Rop = Rop_None;
				Layers[1].ShiftU = pLightmap->StartU;
				Layers[1].ShiftV = pLightmap->StartV;
				Layers[1].ScaleU = 16.0f;
				Layers[1].ScaleV = 16.0f;

				
				// BEGIN - Hardware T&L - paradoxnj 4/7/2005
				BSP->Driver->RenderWorldPoly(TLVerts, ClipInfo.NumDstVerts, Layers, 2, (void*)Face, Flags);
				//BSP->Driver->RenderWorldPoly(ClipInfo.DstVerts, ClipInfo.NumDstVerts, Layers, 1, (void*)Face, Flags);
				// END - Hardware T&L - paradoxnj 4/7/2005
			}
			else
			{
				// BEGIN - Hardware T&L - paradoxnj 4/7/2005
				BSP->Driver->RenderWorldPoly(TLVerts, ClipInfo.NumDstVerts, Layers, 1, NULL, Flags);
				//BSP->Driver->RenderWorldPoly(ClipInfo.DstVerts, ClipInfo.NumDstVerts, Layers, 1, NULL, Flags);
				// END - Hardware T&L - paradoxnj 4/7/2005
			}
		}
		else
		{
			// BEGIN - Hardware T&L - paradoxnj 4/7/2005
			BSP->Driver->RenderGouraudPoly(TLVerts, ClipInfo.NumDstVerts, Flags);
			//BSP->Driver->RenderGouraudPoly(ClipInfo.DstVerts, ClipInfo.NumDstVerts, Flags);
			// END - Hardware T&L - paradoxnj 4/7/2005
		}
	}

	if (Face->PortalObject)		
		BSP->Driver->EndBatch();

	if ((Face->TopSideFlags & TOPSIDE_CALL_CB) && BSP->DrawFaceCB)
		BSP->DrawFaceCB(TLVerts, ClipInfo.NumDstVerts, BSP->DrawFaceCBContext);
}


void grBSPNode_DrawFaceRenderPortal(const grBSPNode_DrawFace *Face, grBSP *BSP, grBSPNode_SceneInfo *SceneInfo, uint32 ClipFlags)
{
	if (Face->PortalObject)		// Special portal face
	{
		grPlane				FacePlane;
		PortalMsgData		MsgData;
		grXForm3d			WorldSpaceFaceXForm;

		assert(Face->PortalObject->Methods->Type == GR_OBJECT_TYPE_PORTAL);
		assert(Face->PortalXForm);

		g_WorldDebugInfo.NumPortals++;

		// Get the face plane
		FacePlane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Face->PlaneIndex);

		if (!grPlaneArray_IndexSided(Face->PlaneIndex))
			grPlane_Inverse(&FacePlane);	// Point the Normal opposite of face

		// BEGIN - Hardware T&L - paradoxnj 4/7/2005

		// Transform the plane into world space 
		grPlane_Transform(&FacePlane, &BSP->ModelToWorldXForm, &FacePlane);
		// Transform the FaceXForm into WorldSpace
		grXForm3d_Multiply(&BSP->ModelToWorldXForm, Face->PortalXForm, &WorldSpaceFaceXForm);

		// END - Hardware T&L - paradoxnj 4/7/2005

		// Setup the message data
		//MsgData.World = (grWorld*)Face->PortalObject->Instance;
		MsgData.Plane = &FacePlane;
		MsgData.FaceXForm = &WorldSpaceFaceXForm;
		MsgData.World = BSP->World;
		MsgData.Camera = SceneInfo->Camera;
		MsgData.Frustum = NULL;
			
		//BSP->Driver->BeginBatch();

		// Render the portal
		grObject_SendMessage(Face->PortalObject, 1, &MsgData);

		//BSP->Driver->EndBatch();

		grObject_SetRenderNextPass(Face->PortalObject, GR_TRUE);
	}
}