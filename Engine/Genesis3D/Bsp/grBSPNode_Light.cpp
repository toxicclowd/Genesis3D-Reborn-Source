/****************************************************************************************/
/*  JEBSPNODE_LIGHT.C                                                                   */
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
#include <math.h>

#include "grBSP._h"

#include "Errorlog.h"
#include "Ram.h"
#include "grLight.h"
#include "Bitmap.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))  

// Added by chrisjp : Engine now looks at driver preferences when creating lightmaps.
// Modified again 4.8.00 should speed up level loading..

#include "Dcommon.h" // Neccesary as we use several of the driver thandle functions

//grTexture * grBitmap_CreateTHandle(DRV_Driver *Driver,int Width,int Height,int NumMipLevels,
//			grPixelFormat SeekFormat1,grPixelFormat SeekFormat2,grBoolean SeekCK,grBoolean SeekAlpha,grBoolean SeekSeparates,uint32 DriverFlags);

grPixelFormat bestSupportedLightmapPixelFormat = GR_PIXELFORMAT_NO_DATA;

//void DetermineSupportedLightmapFormat(grPixelFormat goalFormat, DRV_Driver	*Driver);
// End added by chrisjp

//=======================================================================================
//			*** Lightmap *** In here for now, till it finds a home...
//=======================================================================================

#define LIGHTMAP_CACHE_POINTS

grBoolean grBSPNode_LightmapFillPoints(const grBSPNode_Lightmap *Lightmap, grBSP *BSP, const grBSPNode *RootNode, grVec3d *Points, grFloat ShiftU, grFloat ShiftV);

//=======================================================================================
//	grBSPNode_LightmapCreate
//=======================================================================================
grBSPNode_Lightmap *grBSPNode_LightmapCreate(grBSP *BSP, const grTexVert *TVerts, int32 NumVerts, const grPlane *Plane, const grTexVec *TexVec, grBSPNode *RootNode, grFloat ShiftU, grFloat ShiftV)
{
	int32				i;
	const grVec3d		*pVecs[2];
	grFloat				Mins[2], Maxs[2];
	grFloat				Width, Height, DistScale;
	grVec3d				*pTex2WorldVecs[2], TexNormal;
	grFloat				Len, Dist;
	grBSPNode_Lightmap	*Lightmap;
	grPlane				WorldSpacePlane;

	assert(TVerts);
	assert(NumVerts > 2);
	assert(Plane);
	assert(TexVec);

	Lightmap = GR_RAM_ALLOCATE_STRUCT(grBSPNode_Lightmap);

	if (!Lightmap)
		return NULL;

	ZeroMem(Lightmap);

	//grPlane_Transform(Plane, &BSP->ModelToWorldXForm, &WorldSpacePlane);
	WorldSpacePlane = *Plane;

	for (i=0; i<2; i++)
	{
		Mins[i] =  99999.0f;
		Maxs[i] = -99999.0f;
	}

	// Get the mins/maxs of the u,v's to obtain the width/height of the lightmap
	for (i=0; i<NumVerts; i++, TVerts++)
	{
		int32		j;
		grFloat		Val[2];

		Val[0] = TVerts->u;
		Val[1] = TVerts->v;

		for (j=0; j<2; j++)
		{
			if (Val[j] < Mins[j])
				Mins[j] = Val[j];
			if (Val[j] > Maxs[j])
				Maxs[j] = Val[j];
		}
	}

	pVecs[0] = &(TexVec->VecU);
	pVecs[1] = &(TexVec->VecV);

	Lightmap->MinU = Mins[0];
	Lightmap->MinV = Mins[1];
	Lightmap->MaxU = Maxs[0];
	Lightmap->MaxV = Maxs[1];
	
	// Snap the uv rect to a LGRID_SIZExLGRID_SIZE aligned grid
	for (i=0; i<2; i++)
	{
		Mins[i] = (grFloat)floor(Mins[i]/LGRID_SIZE);
		Maxs[i] = (grFloat)ceil(Maxs[i]/LGRID_SIZE);
	}

	// Get the upper left hand corner of the lightmap (in texel space)
	Lightmap->StartU = Mins[0]*LGRID_SIZE;
	Lightmap->StartV = Mins[1]*LGRID_SIZE;

	// Get the width of the lightmap (in lightmap space)
	Width = (Maxs[0] - Mins[0])+1;
	Height = (Maxs[1] - Mins[1])+1;

	assert(Width > 0 && Width <= MAX_LIGHTMAP_WH);
	assert(Height > 0 && Height <= MAX_LIGHTMAP_WH);

	Lightmap->Width = (uint16)Width;
	Lightmap->Height = (uint16)Height;

	// Get the texture normal from the texture vecs
	grVec3d_CrossProduct(pVecs[0], pVecs[1], &TexNormal);
	grVec3d_Normalize(&TexNormal);
	
	// Flip it towards plane normal
	DistScale = grVec3d_DotProduct(&TexNormal, &WorldSpacePlane.Normal);
	assert(DistScale);

	if (DistScale < 0)
	{
		DistScale = -DistScale;
		grVec3d_Inverse(&TexNormal);
	}	
	
	// Inverse DistScale
	DistScale = 1.0f/DistScale;

	pTex2WorldVecs[0] = &Lightmap->Tex2WorldVecU;
	pTex2WorldVecs[1] = &Lightmap->Tex2WorldVecV;

	// Get the Tex to World vectors
	for (i=0 ; i<2 ; i++)
	{
		Len = grVec3d_Length(pVecs[i]);
		Dist = grVec3d_DotProduct(pVecs[i], &WorldSpacePlane.Normal);
		Dist *= DistScale;
		grVec3d_AddScaled(pVecs[i], &TexNormal, -Dist, pTex2WorldVecs[i]);
		grVec3d_Scale(pTex2WorldVecs[i], (1/Len)*(1/Len), pTex2WorldVecs[i]);
	}

	for (i=0 ; i<3 ; i++)
	{
		grFloat		Val;

		Val = -pVecs[0]->Z * grVec3d_GetElement(pTex2WorldVecs[0], i)-pVecs[1]->Z * grVec3d_GetElement(pTex2WorldVecs[1], i);
		grVec3d_SetElement(&Lightmap->TexOrigin, i, Val);
	}

	Dist = grVec3d_DotProduct(&Lightmap->TexOrigin, &WorldSpacePlane.Normal) - WorldSpacePlane.Dist - 1.0f;
	grVec3d_AddScaled(&Lightmap->TexOrigin, &TexNormal, -Dist*DistScale, &Lightmap->TexOrigin);

	
	// Create the lightmap THandle 
	if (BSP->Driver)
	{
		int32					Width, Height;
		grRDriver_PixelFormat	PixelFormat;
		
		Width = Lightmap->Width;
		Height = Lightmap->Height;


		if( bestSupportedLightmapPixelFormat == 0)
			DetermineSupportedLightmapFormat(GR_PIXELFORMAT_16BIT_565_RGB , BSP->Driver);

		PixelFormat.Flags = RDRIVER_PF_LIGHTMAP;
		PixelFormat.PixelFormat = bestSupportedLightmapPixelFormat;

		Lightmap->THandle = BSP->Driver->THandle_Create(Width, Height, 1, &PixelFormat);
	}

#ifdef LIGHTMAP_CACHE_POINTS
	Lightmap->Points = GR_RAM_ALLOCATE_ARRAY(grVec3d, Lightmap->Width*Lightmap->Height);

	if (!Lightmap->Points)
		goto ExitWithError;

	// Fill the array of points with the world space lightmap verts
	if (!grBSPNode_LightmapFillPoints(Lightmap, BSP, RootNode, Lightmap->Points, ShiftU, ShiftV))
		return NULL;
#endif

	// Setup lightmap scale values for dynamic lights
	{
		grFloat		Len;

		Len = grVec3d_Length(&TexVec->VecU);
		Lightmap->XStep = (int32)((16.0f/Len) * (1<<10));
		Lightmap->XScale = (int32)((1.0f/Len) * (1<<10));

		Len = grVec3d_Length(&TexVec->VecV);
		Lightmap->YStep = (int32)((16.0f/Len) * (1<<10));
		Lightmap->YScale = (int32)((1.0f/Len) * (1<<10));
	}

	return Lightmap;

	ExitWithError:
	{
		if (Lightmap)
		{
			if (Lightmap->Points)
				grRam_Free(Lightmap->Points);

			grRam_Free(Lightmap);
		}
		return NULL;
	}
}

//=======================================================================================
//	grBSPNode_LightmapDestroy
//=======================================================================================
void grBSPNode_LightmapDestroy(grBSPNode_Lightmap **Lightmap, grBSP *BSP)
{
	int32		i;

	assert(Lightmap);
	assert(*Lightmap);

	if ((*Lightmap)->Points)
		grRam_Free((*Lightmap)->Points);

	if ((*Lightmap)->THandle)
	{
		assert(BSP->Driver);		// Driver should still be valid if there are any THandles left about
		BSP->Driver->THandle_Destroy((*Lightmap)->THandle);
	}

	for (i=0; i< GR_LIGHTMAP_MAX_STYLES; i++)
	{
		if ((*Lightmap)->RGBData[i])
			grRam_Free((*Lightmap)->RGBData[i]);
	}
	
	grRam_Free(*Lightmap);

	*Lightmap = NULL;
}

//=======================================================================================
//	grBSPNode_LightmapFillPoints
//	Takes an array of allocated points (of size Lightmap->Width*Lightmap->Height),
//	and fills the array with texture space verts projected onto the face of the lightmap
//=======================================================================================
grBoolean grBSPNode_LightmapFillPoints(const grBSPNode_Lightmap *Lightmap, grBSP *BSP, const grBSPNode *RootNode, grVec3d *Points, grFloat ShiftU, grFloat ShiftV)
{
	grFloat			MidU, MidV, StartU, StartV;
	int32			u, v, NumPoints;
	grVec3d			*pPoint, FaceMid;
	const grVec3d	*pTex2WorldVecU, *pTex2WorldVecV, *pTexOrigin;
	uint8			InSolid[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH], *pInSolid;

	assert(Lightmap);
	assert(Points);

	NumPoints = Lightmap->Width*Lightmap->Height;

	assert(NumPoints < MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH);

	pTexOrigin = &Lightmap->TexOrigin;
	pTex2WorldVecU = &Lightmap->Tex2WorldVecU;
	pTex2WorldVecV = &Lightmap->Tex2WorldVecV;

	// Get texture space mid uv
	MidU = (Lightmap->MaxU + Lightmap->MinU)*0.5f;
	MidV = (Lightmap->MaxV + Lightmap->MinV)*0.5f;

	// Get world space mid vert off this uv
	FaceMid.X = pTex2WorldVecU->X * MidU + pTex2WorldVecV->X * MidV + pTexOrigin->X;
	FaceMid.Y = pTex2WorldVecU->Y * MidU + pTex2WorldVecV->Y * MidV + pTexOrigin->Y;
	FaceMid.Z = pTex2WorldVecU->Z * MidU + pTex2WorldVecV->Z * MidV + pTexOrigin->Z;
	
	pPoint = Points;
	pInSolid = InSolid;

	StartU = Lightmap->StartU + ShiftU;
	StartV = Lightmap->StartV + ShiftV;

	// Fill in the array of points
	for (v=0; v < Lightmap->Height; v++)
	{
		for (u=0; u < Lightmap->Width; u++, pPoint++, pInSolid++)
		{
			grFloat			CurU, CurV;
			grBSPNode_Leaf	*Leaf;

			CurU = StartU + u * LGRID_SIZE;
			CurV = StartV + v * LGRID_SIZE;

			// Project the CurU/CurV along the Tex2World vectors to get the WorldSpace vert for line tracing
			pPoint->X = pTex2WorldVecU->X * CurU + pTex2WorldVecV->X * CurV + pTexOrigin->X;
			pPoint->Y = pTex2WorldVecU->Y * CurU + pTex2WorldVecV->Y * CurV + pTexOrigin->Y;
			pPoint->Z = pTex2WorldVecU->Z * CurU + pTex2WorldVecV->Z * CurV + pTexOrigin->Z;

			// Pre-compute if the point is in solid space, so we don't
			// have to keep doing it in the check below...
			Leaf = grBSPNode_FindLeaf(RootNode, BSP, pPoint);

			if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
				*pInSolid = 1;
			else
				*pInSolid = 0;
		}
	}

	//
	//	Find all points in solid space, and set them to the closest point 
	//	thats not in solid space (there has got to be a better way to do this)
	//
	
	pPoint = Points;
	pInSolid = InSolid;

#if 1
	for (v=0; v< NumPoints; v++, pPoint++, pInSolid++)
	{
		grVec3d			*pPoint2, *pBestPoint;
		grFloat			Dist, BestDist;
		uint8			*pInSolid2;

		if (!(*pInSolid))
			continue;
		
		pPoint2 = Points;
		pInSolid2 = InSolid;
		pBestPoint = &FaceMid;		// Default it to something good
		BestDist = 99999.0f;

		for (u=0; u< NumPoints; u++, pPoint2++, pInSolid2++)
		{
			grVec3d		Vect;

			if (pPoint == pPoint2)
				continue;			// We know we don't want this point

			if (*pInSolid2)			
				continue;			// Skip points that are in solid
			
			grVec3d_Subtract(pPoint2, pPoint, &Vect);
			Dist = grVec3d_Length(&Vect);

			if (Dist < BestDist)
			{
				BestDist = Dist;
				pBestPoint = pPoint2;
			}
		}

		*pPoint = *pBestPoint;		// This is it, copy it over
		*pInSolid = 0;				// No longer in solid at this point
	}
#endif

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LightmapCalcLight
//	Fills a lightmap with data from the list of lights supplied.
//	If lightmap already has light, it is destroyed, and recreated...
//=======================================================================================
grBoolean grBSPNode_LightmapCalcLight(grBSPNode_Lightmap *Lightmap, grBSP *BSP, const grPlane *Plane, grBSPNode *RootNode, const grVec3d *Pos, grFloat Radius, const grChain *LightArray, grBoolean DoubleSided)
{
	int32				i, p, NumPoints;
#ifdef LIGHTMAP_CACHE_POINTS
	grVec3d				*Points, *pPoint;		// 5k
#else
	grVec3d				Points[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH], *pPoint;	// 5k
#endif
	grVec3d				RGB[MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH], *pRGB;		// 5k 
	uint8				*pLData;
	grChain_Link		*Link;

	assert(Lightmap);
	assert(LightArray);

	NumPoints = Lightmap->Width*Lightmap->Height;

	assert(NumPoints < MAX_LIGHTMAP_WH*MAX_LIGHTMAP_WH);

#ifdef LIGHTMAP_CACHE_POINTS
	Points = Lightmap->Points;
#else
	// Fill the array of points with the world space lightmap verts
	if (!grBSPNode_LightmapFillPoints(Lightmap, BSP, RootNode, Points, 0.0f, 0.0f))
		return GR_FALSE;
#endif

	memset(RGB, 0, sizeof(grVec3d)*NumPoints);

	// Go through all the lights
	for (Link = grChain_GetFirstLink(LightArray); Link; Link = grChain_LinkGetNext(Link))
	{
		grLight			*pLight;
		grVec3d			Color;
		grFloat			LRadius, Brightness;
		grVec3d			LPos;
		uint32			Flags;

		pLight = (grLight*)grChain_LinkGetLinkData(Link);

		grLight_GetAttributes(pLight, &LPos, &Color, &LRadius, &Brightness, &Flags);

		// Transform the light into the bsp (FIXME:  XForm all lights before they get here!  Maintain a XForm'ed list...)
		grXForm3d_Transform(&BSP->WorldToModelXForm, &LPos, &LPos);

		if (!DoubleSided)
		{
			if (grPlane_PointDistance(Plane, &LPos) < 0.001f)
				continue;		// Light behind surface
		}

		if (grVec3d_DistanceBetween(Pos, &LPos) > (Radius + LRadius + 1.0f))
			continue;		// Light is NOT in radius of face

		if (Color.X > 1.0f || Color.Y > 1.0f || Color.Z > 1.0f)
			grVec3d_Scale(&Color, 1.0f/255.0f, &Color);

		pPoint = Points;
		pRGB = RGB;

		for (p=0; p< NumPoints; p++, pPoint++, pRGB++)
		{
			grVec3d		Vect;
			grFloat		Dist, Angle, Val;

			grVec3d_Subtract(&LPos, pPoint, &Vect);
			Dist = grVec3d_Normalize(&Vect);

			Angle = grVec3d_DotProduct(&Vect, &Plane->Normal);

			if (Angle <= 0.0f)							
			{
				if (DoubleSided)
					Angle = (grFloat)fabs(Angle);
				else
					continue;
			}
			
			Val = (LRadius - Dist) * Angle;

			if (Val <= 0.0f)
				continue;	// Light out of radius for this point
			
			if ((Flags & GR_LIGHT_FLAG_TYPEMASK) == GR_LIGHT_FLAG_SPOT)
			{
				grVec3d		WorldPoint;

				grXForm3d_Transform(&BSP->ModelToWorldXForm, pPoint, &WorldPoint);
				Val *= grLight_GetSpotFactor(pLight, &WorldPoint);
				if (Val <= 0.0f)
					continue;	// Outside the cone
			}

			if (grBSPNode_RayIntersects_r(RootNode, BSP, pPoint, &LPos))
				continue;	// Ray is in shadow

			// Add this lights color to the lightmap data
			grVec3d_AddScaled(pRGB, &Color, Val*Brightness, pRGB);
		}
	}
	
	// If space for the lightdata has not been allocated, allocate it now
	if (!Lightmap->RGBData[0])	 
	{
		Lightmap->RGBData[0] = GR_RAM_ALLOCATE_ARRAY(uint8, NumPoints*3);
		if (!Lightmap->RGBData[0])
			return GR_FALSE;
	}

	pRGB = RGB;
	pLData = Lightmap->RGBData[0];

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
		for (i=0; i<3; i++, pLData++)
			*pLData = (uint8)(grVec3d_GetElement(pRGB, i)*Max2/Max);
	#else
		for (i=0; i<3; i++, pLData++)
		{
			grFloat	Val;
			
			Val = min(grVec3d_GetElement(pRGB, i), MaxLight);

			*pLData = (uint8)Val;
		}
	#endif
	}

	Lightmap->Dynamic = 1;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LightUpdate_r
//=======================================================================================
grBoolean grBSPNode_LightUpdate_r(grBSPNode *Node, grBSP *BSP, grBSPNode *RootNode, grBoolean UpdateAll)
{
	assert(Node);

	if (Node->Flags & NODE_LEAF)
	{
		assert(Node->Leaf);
		return GR_TRUE;
	}

	if ((Node->Flags & NODE_UPDATELIGHTS) || UpdateAll)
	{
		int32				i;

		for (i=0; i< Node->NumDrawFaces; i++)
		{
			grBSPNode_DrawFace	*pDrawFace;
			grPlane				Plane;
			const grFaceInfo	*pFaceInfo;
			grBoolean			DoubleSided;

			pDrawFace = Node->DrawFaces[i];
			assert(pDrawFace);

			if (!(pDrawFace->TopSideFlags & TOPSIDE_UPDATE_LIGHT) && !UpdateAll)
				continue;		// Face does not need or want light

			pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, pDrawFace->FaceInfoIndex);
			assert(pFaceInfo);

			if (!grFaceInfo_NeedsLightmap(pFaceInfo))
				continue;

			// Copt the plane for this face
			Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pDrawFace->PlaneIndex);

			// Reverse it if needed
			if (grPlaneArray_IndexSided(pDrawFace->PlaneIndex))
				grPlane_Inverse(&Plane);

			// Create a lightmap if needed
			if (!pDrawFace->Lightmap)
			{
				const grTexVec			*pTexVec;

				assert(pDrawFace->Poly);

				// Get the texture vector for this face
				pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, pDrawFace->TexVecIndex);

				pDrawFace->Lightmap = grBSPNode_LightmapCreate(BSP, pDrawFace->TVerts, pDrawFace->Poly->NumVerts, &Plane, pTexVec, RootNode, pDrawFace->FixShiftU, pDrawFace->FixShiftV);

				if (!pDrawFace->Lightmap)
					return GR_FALSE;
			}

			// Fill the lightmap in with the data
			if (pDrawFace->Contents & GR_BSP_CONTENTS_EMPTY)
				DoubleSided = GR_TRUE;
			else
				DoubleSided = GR_FALSE;

			if (!grBSPNode_LightmapCalcLight(	pDrawFace->Lightmap, 
												BSP, 
												&Plane, RootNode, 
												&pDrawFace->Center, 
												pDrawFace->Radius, 
												BSP->LightChain, 
												DoubleSided))
				return GR_FALSE;

		}

		Node->Flags &= ~NODE_UPDATELIGHTS;
	}

	if (!grBSPNode_LightUpdate_r(Node->Children[NODE_FRONT], BSP, RootNode, UpdateAll))
		return GR_FALSE;
	if (!grBSPNode_LightUpdate_r(Node->Children[NODE_BACK], BSP, RootNode, UpdateAll))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LightPatch_r
//	Lights all faces withought lightmaps...
//=======================================================================================
grBoolean grBSPNode_LightPatch_r(grBSPNode *Node, grBSP *BSP, grBSPNode *RootNode)
{
	assert(Node);

	if (Node->Flags & NODE_LEAF)
	{
		assert(Node->Leaf);
		return GR_TRUE;
	}

	{
		int32				i;

		for (i=0; i< Node->NumDrawFaces; i++)
		{
			grBSPNode_DrawFace	*pDrawFace;
			grPlane				Plane;
			const grFaceInfo	*pFaceInfo;
			grBoolean			DoubleSided;

			pDrawFace = Node->DrawFaces[i];
			assert(pDrawFace);

			pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, pDrawFace->FaceInfoIndex);
			assert(pFaceInfo);

			if (!grFaceInfo_NeedsLightmap(pFaceInfo))
			{
				assert(!pDrawFace->Lightmap);
				continue;
			}

			if (pDrawFace->Lightmap)
			{
				assert(pDrawFace->TopSideFlags & TOPSIDE_UPDATE_LIGHT);
				continue;				// Only light faces withough lightmaps
			}

			assert(!(pDrawFace->TopSideFlags & TOPSIDE_UPDATE_LIGHT));

			// Copt the plane for this face
			Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pDrawFace->PlaneIndex);

			// Reverse it if needed
			if (grPlaneArray_IndexSided(pDrawFace->PlaneIndex))
				grPlane_Inverse(&Plane);

			// Create a lightmap if needed
			if (!pDrawFace->Lightmap)
			{
				const grTexVec			*pTexVec;

				assert(pDrawFace->Poly);

				// Get the texture vector for this face
				pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, pDrawFace->TexVecIndex);

				pDrawFace->Lightmap = grBSPNode_LightmapCreate(BSP, pDrawFace->TVerts, pDrawFace->Poly->NumVerts, &Plane, pTexVec, RootNode, pDrawFace->FixShiftU, pDrawFace->FixShiftV);

				if (!pDrawFace->Lightmap)
					return GR_FALSE;
			}

			// Fill the lightmap in with the data
			if (pDrawFace->Contents & GR_BSP_CONTENTS_EMPTY)
				DoubleSided = GR_TRUE;
			else
				DoubleSided = GR_FALSE;

			if (!grBSPNode_LightmapCalcLight(	pDrawFace->Lightmap, 
												BSP, 
												&Plane, 
												RootNode, 
												&pDrawFace->Center, 
												pDrawFace->Radius, 
												BSP->LightChain,
												DoubleSided))
				return GR_FALSE;

		}

		Node->Flags &= ~NODE_UPDATELIGHTS;
	}

	if (!grBSPNode_LightPatch_r(Node->Children[NODE_FRONT], BSP, RootNode))
		return GR_FALSE;
	if (!grBSPNode_LightPatch_r(Node->Children[NODE_BACK], BSP, RootNode))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LightUpdateFromPoint_r
//=======================================================================================
grBoolean grBSPNode_LightUpdateFromPoint_r(grBSPNode *Node, grBSP *BSP, grBSPNode *RootNode, const grVec3d *Pos, grFloat Radius)
{
	grFloat			Dist;
	int32			i;
	const grPlane	*pPlane;

	assert(Node);

	if (Node->Flags & NODE_LEAF)
	{
		assert(Node->Leaf);
		return GR_TRUE;
	}

	// Get the distance that the eye is from this plane
	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	Dist = grPlane_PointDistanceFast(pPlane, Pos);

	if (Dist > Radius)		// Front side
		return grBSPNode_LightUpdateFromPoint_r(Node->Children[NODE_FRONT], BSP, RootNode, Pos, Radius);
	if (Dist < -Radius)		// Back side
		return grBSPNode_LightUpdateFromPoint_r(Node->Children[NODE_BACK], BSP, RootNode, Pos, Radius);
	
	// Both sides (On node)
	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*pDrawFace;
		grPlane				Plane;
		const grFaceInfo	*pFaceInfo;
		grBoolean			DoubleSided;

		pDrawFace = Node->DrawFaces[i];
		assert(pDrawFace);

		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, pDrawFace->FaceInfoIndex);
		assert(pFaceInfo);

		if (!grFaceInfo_NeedsLightmap(pFaceInfo))
		{
			assert(!pDrawFace->Lightmap);
			continue;
		}

		if (!pDrawFace->Lightmap)
			continue;				// Only update lighting on faces that have lightmaps

		// Copy the plane for this face
		Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pDrawFace->PlaneIndex);

		// Reverse it if needed
		if (grPlaneArray_IndexSided(pDrawFace->PlaneIndex))
			grPlane_Inverse(&Plane);

		// Create a lightmap if needed
		if (!pDrawFace->Lightmap)
		{
			const grTexVec			*pTexVec;

			assert(pDrawFace->Poly);

			// Get the texture vector for this face
			pTexVec = grTexVec_ArrayGetTexVecByIndex(BSP->TexVecArray, pDrawFace->TexVecIndex);

			pDrawFace->Lightmap = grBSPNode_LightmapCreate(BSP, pDrawFace->TVerts, pDrawFace->Poly->NumVerts, &Plane, pTexVec, RootNode, pDrawFace->FixShiftU, pDrawFace->FixShiftV);

			if (!pDrawFace->Lightmap)
				return GR_FALSE;
		}

		// Fill the lightmap in with the data
		if (pDrawFace->Contents & GR_BSP_CONTENTS_EMPTY)
			DoubleSided = GR_TRUE;
		else
			DoubleSided = GR_FALSE;

		if (!grBSPNode_LightmapCalcLight(	pDrawFace->Lightmap, 
											BSP,
											&Plane, RootNode, 
											&pDrawFace->Center, 
											pDrawFace->Radius, 
											BSP->LightChain,
											DoubleSided))
			return GR_FALSE;
	}
	
	Node->Flags &= ~NODE_UPDATELIGHTS;

	if (!grBSPNode_LightUpdateFromPoint_r(Node->Children[NODE_FRONT], BSP, RootNode, Pos, Radius))
		return GR_FALSE;
	if (!grBSPNode_LightUpdateFromPoint_r(Node->Children[NODE_BACK], BSP, RootNode, Pos, Radius))
		return GR_FALSE;

	return GR_TRUE;
}
//grTexture * grBitmap_CreateTHandle(DRV_Driver *Driver,int Width,int Height,int NumMipLevels,
//			grPixelFormat SeekFormat1,grPixelFormat SeekFormat2,grBoolean SeekCK,grBoolean SeekAlpha,grBoolean SeekSeparates,uint32 DriverFlags);

// Added by chrisjp.. this should speed up level loading a good bit..
void DetermineSupportedLightmapFormat(grPixelFormat goalFormat, DRV_Driver *Driver)
{
	grTexture_Info lightmapFormatInfo;
	grTexture* tempTestHandle = grBitmap_CreateTHandle(Driver,16,16,1,goalFormat,GR_PIXELFORMAT_NO_DATA,GR_FALSE,GR_FALSE,GR_FALSE,RDRIVER_PF_LIGHTMAP);

	Driver->THandle_GetInfo(tempTestHandle,0,&lightmapFormatInfo);

	bestSupportedLightmapPixelFormat = lightmapFormatInfo.PixelFormat.PixelFormat;

	Driver->THandle_Destroy(tempTestHandle);
}


