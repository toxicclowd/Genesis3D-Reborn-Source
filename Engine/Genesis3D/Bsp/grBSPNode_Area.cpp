/****************************************************************************************/
/*  JEBSPNODE_AREA.C                                                                    */
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

// Private dependents
#include "grBSP._h"
#include "Errorlog.h"
#include "Ram.h"
#include "VisObject.h"
#include "Log.h"

// Public dependents
#include "grBSP.h"

//=======================================================================================
//	grBSPNode_AreaCreate
//=======================================================================================
grBSPNode_Area *grBSPNode_AreaCreate(void)
{
	grBSPNode_Area		*Area;

	Area = GR_RAM_ALLOCATE_STRUCT(grBSPNode_Area);

	if (!Area)
		return NULL;

	ZeroMem(Area);

#ifdef AREA_DRAWFACE_TEST
	LN_Null(Area);

	Area->VisObjectList = NULL;
#endif

	if (!grBSPNode_AreaCreateRef(Area))
	{
		grRam_Free(Area);
		return NULL;
	}

	Area->LeafArray = grArray_Create(sizeof(grBSPNode_AreaLeaf), 10, 10);

	if (!Area->LeafArray)
		goto ExitWithError;

	Area->ObjectChain = grChain_Create();

	if (!Area->ObjectChain)
		goto ExitWithError;

	return Area;

	ExitWithError:
	{
		if (Area)
		{
			if (Area->LeafArray)
				grArray_Destroy(&Area->LeafArray);

			if (Area->ObjectChain)
				grChain_Destroy(&Area->ObjectChain);

			grRam_Free(Area);
		}

		return NULL;
	}
}

//=======================================================================================
//	grBSPNode_AreaCreateRef
//=======================================================================================
grBoolean grBSPNode_AreaCreateRef(grBSPNode_Area *Area)
{
	assert(Area);
	assert(Area->RefCount >= 0);		// 0 because this could be the first ref

	if (Area->RefCount >= (~(uint32)(0)))	// Good God!  Help them if they need more than this...
		return GR_FALSE;

	Area->RefCount++;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_AreaDestroy
//=======================================================================================
void grBSPNode_AreaDestroy(grBSPNode_Area **pArea)
{
	grBSPNode_Area	*Area;

	assert(pArea);

	Area = *pArea;

	if (!Area)
		return;

	assert(Area->RefCount > 0);

	Area->RefCount--;

	if (Area->RefCount == 0)
	{
		if (Area->AreaPortals)
		{
			assert(Area->NumAreaPortals > 0);
			assert(Area->NumAreaPortals == Area->NumWorkAreaPortals);
		
			grRam_Free(Area->AreaPortals);

			Area->AreaPortals = NULL;
			Area->NumAreaPortals = 0;
			Area->NumWorkAreaPortals = 0;
		}

		assert(Area->LeafArray);
		grArray_Destroy(&(Area->LeafArray));

		assert(Area->ObjectChain);
		grChain_Destroy(&Area->ObjectChain);

#ifdef AREA_DRAWFACE_TEST
		if ( Area->VisObjectList )
			List_Destroy(Area->VisObjectList);

		if ( Area->DrawFaces )
			grRam_Free(Area->DrawFaces);
#endif
		grRam_Free(Area);
	}

	*pArea = NULL;
}

#define MAX_PORTAL_VERTS	(GR_FRUSTUM_MAX_PLANES*2)

//=======================================================================================
//	grBSPNode_AreaVisFlood
//=======================================================================================
grBoolean grBSPNode_AreaVisFlood_r(grBSPNode_Area *Area, const grVec3d *Pos, const grFrustum *Frustum, uint32 RecursionBit, grBSPNode_Area *FromArea)
{
	grBSPNode_AreaPortal	*pPortal;
	int32					i;
	grFrustum_ClipInfo		ClipInfo;
	grVec3d					Work1[MAX_PORTAL_VERTS];		// Add padding
	grVec3d					Work2[MAX_PORTAL_VERTS];
	grBSPNode_AreaLeaf		*AreaLeaf;

	if (!(Area->RecursionBits & RecursionBit))
	{
		// Mark all leafs in the area to the current RecursionBit
		AreaLeaf = NULL;
		while (AreaLeaf = (grBSPNode_AreaLeaf*)grArray_GetNextElement(Area->LeafArray, AreaLeaf))
		{
			// Mark the leaf as visible
			grBSPNode_LeafSetRecursionBit(AreaLeaf->Leaf, RecursionBit);
		}

		g_WorldDebugInfo.NumVisibleAreas++;	
	
	#if 0
		// Mark grObjects as Vis
		if (Area->VisObjectList)
		{
			List			*Node;
			grVisObject		*VO;

			for(Node = List_Next(Area->VisObjectList); Node != Area->VisObjectList; Node = List_Next(Node) )
			{
				assert(Node);

				VO = (grVisObject *)List_NodeData(Node);
				assert(VO);

				//grVisObject_SetRecursionBit(VO,Frustum,RecursionBit);
			}
		}
	#endif

		// Mark this area with the current RecursionBit
		Area->RecursionBits |= RecursionBit;
	}

	// Setup some of the clip info that won't change
	ClipInfo.ClipFlags = 0xFFFFFF;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;

	pPortal = Area->AreaPortals;

	for (i=0; i< Area->NumAreaPortals; i++, pPortal++)
	{
		grFrustum			NewFrustum;
		grBSPNode_Area		*OtherArea;
		float				Dist;

		OtherArea = (grBSPNode_Area*)pPortal->Target;

		assert(OtherArea);
		assert(OtherArea != Area);

		if ( OtherArea == FromArea ) // <> don't go back to the source
			continue;

		Dist = grVec3d_DotProduct(Pos, &pPortal->Plane.Normal) - pPortal->Plane.Dist;

		if (Dist >= 0)	
			continue;			// Don't go out front facing portals
		
		assert(pPortal->Poly->NumVerts <= GR_FRUSTUM_MAX_PLANES);

		ClipInfo.NumSrcVerts = pPortal->Poly->NumVerts;
		ClipInfo.SrcVerts = pPortal->Poly->Verts;

		// Clip the portal by all the frustum planes
		if (!grFrustum_ClipVerts(Frustum, &ClipInfo))
			continue;		// Portal was clipped away

		// Create a new frustum from this new portal, and flood through it...
		grFrustum_SetFromVerts(&NewFrustum, Pos, ClipInfo.DstVerts, ClipInfo.NumDstVerts);

		// Flow through the portals target (the area on the other side of the portal)
		if (!grBSPNode_AreaVisFlood_r(OtherArea, Pos, &NewFrustum, RecursionBit, Area))
			return GR_FALSE;
	}

	return GR_TRUE;
}

#ifdef AREA_DRAWFACE_TEST
//=======================================================================================
//	grBSPNode_AreaAddObject
//=======================================================================================
void grBSPNode_AreaAddObject(grBSPNode_Area *Area,grVisObject *VO)
{

	if ( ! Area->VisObjectList )
	{
		Area->VisObjectList = List_Create();
		assert(Area->VisObjectList);
	}
	else
	{
		if ( List_Find(Area->VisObjectList,(uint32)VO) )
			return; // already have it !
	}

	grVisObject_AddArea(VO,(uint32)Area);

	#ifdef _DEBUG
	{
		int l1,l2;

		l1 = List_Length(Area->VisObjectList);

		List_AddTail(Area->VisObjectList,(uint32)VO);

		l2 = List_Length(Area->VisObjectList);
		assert( l2 == (l1 + 1) );
	}
	#else
		List_AddTail(Area->VisObjectList,(uint32)VO);
	#endif
}

//=======================================================================================
//	grBSPNode_AreaRemoveObject
//=======================================================================================
grBoolean grBSPNode_AreaRemoveObject(grBSPNode_Area *Area,grVisObject *VO)
{
List * VONode;

	if ( Area->VisObjectList == NULL )
		return GR_FALSE;

	#ifdef _DEBUG
	{
		int l1,l2;

		l1 = List_Length(Area->VisObjectList);
		assert( l1 > 0 );

		VONode = List_Find(Area->VisObjectList,(uint32)VO);
	
		if ( ! VONode )
			return GR_FALSE;

		assert(List_NodeData(VONode) == (uint32)VO);

		List_DeleteNode(VONode);

		l2 = List_Length(Area->VisObjectList);
		assert( l2 == (l1 - 1) );
	}
	#else
		VONode = List_Find(Area->VisObjectList,(uint32)VO);

		if ( ! VONode )
			return GR_FALSE;

		List_DeleteNode(VONode);
	#endif

return GR_TRUE;
}


//=======================================================================================
//	grBSPNode_AreaMakeDrawFaces
//=======================================================================================
grBoolean grBSPNode_AreaMakeDrawFaces(grBSPNode_Area *Area)
{
	grBSPNode_AreaLeaf	*AreaLeaf;
	grBSPNode_Leaf		*pLeaf;
	Hash				*DrawFaceHash;
	HashNode			*hn;
	grBSPNode_DrawFace	*pFace;
	int					di;

	DrawFaceHash = Hash_Create();
	if (  ! DrawFaceHash )
		return GR_FALSE;

	assert(Area->NumDrawFaces == 0);
	assert(Area->DrawFaces == NULL);

	AreaLeaf = NULL;
	while( (AreaLeaf = (grBSPNode_AreaLeaf *)grArray_GetNextElement(Area->LeafArray,AreaLeaf) ) != NULL )
	{
	int f;
		pLeaf = AreaLeaf->Leaf;
		for(f=0;f<pLeaf->NumDrawFaces;f++)
		{
			pFace = pLeaf->DrawFaces[f];
			if ( ! Hash_Get(DrawFaceHash,(uint32)pFace,NULL) )
			{
				Hash_Add(DrawFaceHash,(uint32)pFace,(uint32)pFace);
				Area->NumDrawFaces ++;
			}
		}
	}

	Area->DrawFaces = grRam_Allocate(Area->NumDrawFaces*sizeof(void *));
	if ( ! Area->DrawFaces )
	{
		Hash_Destroy(DrawFaceHash);
		return GR_FALSE;
	}

	hn = NULL;
	di = 0;
	while(hn = Hash_WalkNext(DrawFaceHash,hn) )
	{
		pFace = (grBSPNode_DrawFace *) HashNode_Data(hn);
		assert( di < Area->NumDrawFaces );
		Area->DrawFaces[di++] = pFace;
	}

	Hash_Destroy(DrawFaceHash);

	Log_Printf("Area Drawfaces : %d\n",Area->NumDrawFaces);

	return GR_TRUE;
}
#endif

#ifdef AREA_DRAWFACE_TEST

//=======================================================================================
//	grBSPNode_AreaVisFlood
//=======================================================================================
grBoolean grBSPNode_AreaRenderFlood_r(grBSPNode_Area *Area, grBSP *BSP, const grCamera *Camera, const grFrustum *Frustum, uint32 RecursionBit, grBSPNode_Area *FromArea)
{
	grBSPNode_AreaPortal	*pPortal;
	int32					i;
	grFrustum_ClipInfo		ClipInfo;
	grVec3d					Work1[MAX_PORTAL_VERTS];		// Add padding
	grVec3d					Work2[MAX_PORTAL_VERTS];
	uint32					ClipFlags;
	grBSPNode_SceneInfo		SceneInfo;

	assert(0);		// This stuff is not ready for prime time...

	if (!(Area->RecursionBits & RecursionBit))
		g_WorldDebugInfo.NumVisibleAreas++;	

	// Mark this area with the current RecursionBit
	Area->RecursionBits |= RecursionBit;

	// Render it !
	SceneInfo.Camera = (grCamera*)Camera;
	SceneInfo.Frustum = (grFrustum*)Frustum;
	SceneInfo.RecursionBit = RecursionBit;
	SceneInfo.GpuWorld = GR_FALSE;

	//@@ set up area clip flags!
	ClipFlags = (1UL<<Frustum->NumPlanes)-1;

	for(i=0;i<Area->NumDrawFaces;i++)
	{
		/**

		disadvantages :
			1. we don't have the heirarchical clipflags
			2. backfacing the drawfaces

		***/
		// @@ do ExtBox clipflags thing for each draw face !!
		//	grFrustum_SetClipFlagsFromExtBox()
		//	faster than doing the frustum clips cuz we avoid the memcpys of all the verts
		grBSPNode_DrawFaceRender(Area->DrawFaces[i], BSP, &SceneInfo, ClipFlags);
	}

	// Render grObjects
	if ( Area->VisObjectList )
	{
		List		*Node;
		grVisObject	*VO;
	
		for(Node = List_Next(Area->VisObjectList); Node != Area->VisObjectList; Node = List_Next(Node) )
		{
			assert(Node);

			VO = (grVisObject *)List_NodeData(Node);
			assert(VO);

			grVisObject_Render(VO, Frustum, RecursionBit);
		}
	}

	// Setup some of the clip info that won't change
	ClipInfo.ClipFlags = 0xFFFFFF;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;

	pPortal = Area->AreaPortals;

	for (i=0; i< Area->NumAreaPortals; i++, pPortal++)
	{
		grFrustum			NewFrustum;
		grBSPNode_Area		*OtherArea;
		float				Dist;

		OtherArea = (grBSPNode_Area*)pPortal->Target;

		assert(OtherArea);
		assert(OtherArea != Area);

		if ( OtherArea == FromArea ) // don't go back to the source
			continue;

		Dist = grVec3d_DotProduct(grCamera_GetPov(Camera), &pPortal->Plane.Normal) - pPortal->Plane.Dist;

		if (Dist >= 0)	
			continue;			// Don't go out front facing portals
		
		assert(pPortal->Poly->NumVerts <= MAX_PORTAL_VERTS);

		ClipInfo.NumSrcVerts = pPortal->Poly->NumVerts;
		ClipInfo.SrcVerts = pPortal->Poly->Verts;

		// Clip the portal by all the frustum planes
		if (!grFrustum_ClipVerts(Frustum, &ClipInfo))
			continue;		// Portal was clipped away

		// Create a new frustum from this new portal, and flood though it...
		grFrustum_SetFromVerts(&NewFrustum, grCamera_GetPov(Camera), ClipInfo.DstVerts, ClipInfo.NumDstVerts);

		// Flow through the portals target (the area on the other side of the portal)
		if (!grBSPNode_AreaRenderFlood_r(OtherArea, BSP, Camera, &NewFrustum, RecursionBit, Area))
			return GR_FALSE;
	}

	return GR_TRUE;
}
#endif

grBoolean grBSPNode_AreaRenderVertexBuffer(grBSPNode_Area* Area, grBSP* Tree)
{
	grChain_Link* pLink;
	
	// Enumerate all sections
	for (pLink = grChain_GetFirstLink(Area->RenderDataList); pLink; pLink = grChain_LinkGetNext(pLink))
	{
		grRenderSectionData* pSection;
		pSection = (grRenderSectionData*) grChain_LinkGetLinkData(pLink);

		// Render the current section
		//Tree->Driver->Render(Tree->pVertexBuffer, Tree->pIndexBuffer, pSection); // TBD
	}
	return GR_TRUE;
}
