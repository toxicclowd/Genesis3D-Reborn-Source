/****************************************************************************************/
/*  JEBSPNODE.C                                                                         */
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

#include "Dcommon.h"
#include "grBSP._h"
#include "grBSP.h"

#include "Errorlog.h"
#include "Log.h"
#include "Ram.h"
#include "Quatern.h"


// Added by chrisjp : Engine now looks at driver preferences when creating lightmaps.
extern grPixelFormat bestSupportedLightmapPixelFormat;

void DetermineSupportedLightmapFormat(grPixelFormat goalFormat, DRV_Driver *Driver);
// End added by chrisjp

//****** NOTES!!!
//
//	Functions that end in _r, will be recursively re-entered.
//	Functions that end in _Callr, will call the _r function of the SAME TYPE ONLY
//	Example:
//		grBSP_MakeBSPFaces(Node);		// This will make the faces
//		grBSP_MakeBSPFaces_r(Node);		// This will call grBSP_MakeBSPFaces
//		grBSP_MakeBSPFaces_Callr(Node);	// This will call grBSP_MakeFaces_r
//

//
//	Ok, thus far, this is how the grBSPNode_Portal, grBSPNode_Face, and grBSPNode_DrawFace system works on the nodes.
//
//	First, an explanation on how the BSP is created/maintained.  The BSP is basically a bunch of partitioned
//	grBSP_Brush's.  They are partitioned to constantly mantain a tree of convex pieces.  These peices form the
//	grBSPNode_Leaf's.  
//
//	During the life of the tree, it maintains list of grBSPNode_Portal's that are passages
//	from one leaf to another.  At the same time, it keeps a list of grBSPNode_Faces on portals that seperate
//	visible passages (i.e. walls, etc...).  
//
//	The grBSPNode_Face is kind of tricky though.  When it's created on the portal, it adds itself to the list
//	of faces on the node that created the portal.  Then, it goes through all the grBSPNode_Face's and merges them
//	till they can no longer merge (they remain convex).  Lastly, they are split up to prepare the face to have a 
//	lightmap.  During this entire process, these new grBSPNode_Face's are maintained in the same list, but either
//	set their "merged" member, or the 2 "split" members in the face.  These new faces, will have the 
//	BSPFACE_MERGED_SPLIT flag set.  This simply means that these faces were a result from a merge or split.
//	
//	After the merge/split precess has been preformed, all the faces that don't have the "merged", or "split" member
//	set are then converted to a list of grBSPNode_DrawFace's on the node the the face's portal was on.
//	The main reason that we keep this chain, is so leafs can find out what faces are looking into them.  
//	We just simply have to start on the portals, then go out to the grBSPNode_Face's on that portal, then skip
//	all faces that have the "merged" or "split" member set, and we now have the list of faces that look into that leaf.
//
//	After everything is done with the face creation process, all the faces that were not originally created by portals
//	are destroyed (face with the BSPFACE_MERGED_SPLIT flag are destroyed).  The originals are kept on the portals.
//	
//	When a new brush is added, it is partitioned down to the leafs.  It then takes the leafs it lands in,
//	and paritions that leaf by the brush that landed in it.  It then takes all the portals that wre on that leaf,
//	and partitions them down to the new leafs.  BEFORE it partitions the portals though, it first takes the nodes
//	that the portals were on and destroys all drawfaces, then destroys all the grBSPNode_Face's on all the portals
//	that bound the new leaf that is going to get cut up.
//
//		-John Pollard

#define	OUTSIDE_PADDING			128.0f		// Space between world box, and outside leaf

#define ADD_PORTAL_TO_NODES(p, n1, n2) do {  grBSPNode_AddPortal(n1, p, 0); grBSPNode_AddPortal(n2, p, 1);} while (0)

//#define NODE_USE_JE_RAM
int TotalNumberCollisions=0;

//=======================================================================================
//	grBSPNode_Create
//=======================================================================================
grBSPNode *grBSPNode_Create(grBSP *BSP)
{
	grBSPNode		*Node;

#ifdef NODE_USE_JE_RAM
	Node = GR_RAM_ALLOCATE_STRUCT(grBSPNode);
#else
	Node = (grBSPNode *)grArray_GetNewElement(BSP->NodeArray);
#endif
	if (!Node)
		return NULL;

	ZeroMem(Node);

	Node->PlaneIndex = GR_PLANEARRAY_NULL_INDEX;

	return Node;
}

//=======================================================================================
//	grBSPNode_Destroy
//=======================================================================================
void grBSPNode_Destroy_r(grBSPNode **Node, grBSP *BSP)
{
	grBSPNode		*Node2;

	assert(Node);
	assert(grBSPNode_IsValid(*Node) == GR_TRUE);

	Node2 = *Node;

	if (!(Node2->Flags & NODE_LEAF))
	{
		assert(!Node2->Leaf);
		
		grBSPNode_Destroy_r(&Node2->Children[NODE_FRONT], BSP);
		grBSPNode_Destroy_r(&Node2->Children[NODE_BACK], BSP);

		// Remove the reference to the plane that this node was using
		grPlaneArray_RemovePlane(BSP->PlaneArray, &Node2->PlaneIndex);

		// Destroy any draw faces on node
		grBSPNode_DestroyDrawFaces(Node2, BSP);
	}
	else
	{
		assert(Node2->Leaf);
		assert(Node2->Leaf->Node);
		assert(Node2->Leaf->Node == Node2);
		assert(!Node2->DrawFaces);
		assert(!Node2->NumDrawFaces);
		assert(Node2->PlaneIndex == GR_PLANEARRAY_NULL_INDEX);
		grBSPNode_LeafDestroy(&Node2->Leaf, BSP);
	}

	// Free all the portals on this node
	grBSPNode_DestroyPortals(Node2, BSP);
	// Free all the faces
	grBSPNode_DestroyBSPFaces(Node2, BSP, GR_FALSE);

#ifdef NODE_USE_JE_RAM
	grRam_Free(*Node);
#else
	{
		grBoolean	Ret;
		Ret = grArray_FreeElement(BSP->NodeArray, *Node);
		assert(Ret == GR_TRUE);
	}
#endif

	*Node = NULL;
}

//=======================================================================================
//	grBSPNode_IsValid
//=======================================================================================
grBoolean grBSPNode_IsValid(const grBSPNode *Node)
{
	if (!Node)
		return GR_FALSE;

	if (Node->Leaf)
	{
		if (!(Node->Flags & NODE_LEAF))
		{
			grErrorLog_AddString(-1, "grBSPNode_IsValid:  !(Node->Flags & NODE_LEAF)", NULL);
			return GR_FALSE;
		}

		if (Node->Leaf->Node != Node)		// Make sure they still point to each other
		{
			grErrorLog_AddString(-1, "grBSPNode_IsValid:  Node->Leaf->Node != Node", NULL);
			return GR_FALSE;
		}

	#if 0	// Sometimes portals get clipped away before they recurse to the leafs.  
		if (!Node->Portals)					// Leafs should have portals
		{
			grErrorLog_AddString(-1, "grBSPNode_IsValid:  !Node->Portals", NULL);
			return GR_FALSE;
		}
	#endif
	}
	else
	{
		if (Node->Flags & NODE_LEAF)
		{
			grErrorLog_AddString(-1, "grBSPNode_IsValid:  Node->Flags & NODE_LEAF", NULL);
			return GR_FALSE;
		}

		if (Node->Portals)					// Nodes should NOT have portals
		{
			grErrorLog_AddString(-1, "grBSPNode_IsValid:  Node->Portals", NULL);
			return GR_FALSE;
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_DestroyPortals_r
//=======================================================================================
void grBSPNode_DestroyPortals_r(grBSPNode *Node, grBSP *BSP)
{
	assert(Node);

	if (!(Node->Flags & NODE_LEAF))		// Recurse to leafs
	{
		assert(!Node->Leaf);
		grBSPNode_DestroyPortals_r(Node->Children[NODE_FRONT], BSP);
		grBSPNode_DestroyPortals_r(Node->Children[NODE_BACK], BSP);

		return;
	}
	assert(Node->Leaf);

	// Destroy the portals only
	grBSPNode_DestroyPortals(Node, BSP);
}

//=======================================================================================
//	grBSPNode_DestroyBSPFaces_r
//=======================================================================================
void grBSPNode_DestroyBSPFaces_r(grBSPNode *Node, grBSP *BSP, grBoolean KeepOriginal)
{
	assert(Node);

	if (Node->Flags & NODE_LEAF)
	{
		assert(Node->Leaf);
		return;
	}

	assert(!Node->Leaf);

	grBSPNode_DestroyBSPFaces_r(Node->Children[NODE_FRONT], BSP, KeepOriginal);
	grBSPNode_DestroyBSPFaces_r(Node->Children[NODE_BACK], BSP, KeepOriginal);

	// Destroy the faces only
	grBSPNode_DestroyBSPFaces(Node, BSP, KeepOriginal);
}

//=======================================================================================
//	grBSPNode_DestroyBSPFaces
//	Destroys ALL BSP faces, unless KeepOriginal is set...
//=======================================================================================
void grBSPNode_DestroyBSPFaces(grBSPNode *Node, grBSP *BSP, grBoolean KeepOriginal)
{
	grBSPNode_Face		*Face, *NextFace, *OriginalFaces;

	OriginalFaces = NULL;
	
	// Destroy node faces
	for (Face = Node->Faces; Face; Face = NextFace)
	{
		NextFace = Face->Next;

		if (!(Face->Flags & BSPFACE_MERGED_SPLIT) && KeepOriginal)
		{
			// This face no longer has a merge target or split target...
			Face->Split[0] = NULL;
			Face->Split[1] = NULL;
			Face->Merged = NULL;

			Face->Next = OriginalFaces;		// Make new list of original faces
			OriginalFaces = Face;
			continue;
		}
		
		grBSPNode_FaceDestroy(&Face);
	}

	Node->Faces = OriginalFaces;
}

//=======================================================================================
//	PropogateFaceInfoToDrawFaces_r
//=======================================================================================
static grBoolean PropogateFaceInfoToDrawFaces_r(grBSPNode_Face *Face, grBSP *BSP, grBSP_TopSide *Side)
{
	assert(Face);
	assert(Side);
	assert(!Face->Merged);		// Faces should only be split once we get passed the merged chain

	if (Face->Split[0])
	{
		if (!PropogateFaceInfoToDrawFaces_r(Face->Split[0], BSP, Side))
			return GR_FALSE;
		if (!PropogateFaceInfoToDrawFaces_r(Face->Split[1], BSP, Side))
			return GR_FALSE;

		return GR_TRUE;
	}

	if (Face->DrawFace)
	{
		grBSPNode_DrawFace	*DFace = Face->DrawFace;

		DFace->TopSideFlags = Side->TopSideFlags;
		
		grBSPNode_DrawFaceSetFaceInfoIndex(DFace, BSP, Side->FaceInfoIndex);

		// We need to get smarter about when we destroy the faces lightmap
		//	We should only destroy the lightmap if it depends on data that just changed in faceinfo...
		if (DFace->Lightmap)
			grBSPNode_LightmapDestroy(&DFace->Lightmap, BSP);
	}

	return GR_TRUE;
}

//=======================================================================================
//	PropogateFaceInfoToDrawFaces
//=======================================================================================
static grBoolean PropogateFaceInfoToDrawFaces(grBSPNode_Face *Face, grBSP *BSP, grBSP_TopSide *Side)
{
	assert(Face);
	assert(Side);

	while (Face->Merged)
	{
		if (Face->Portal->Side->grBrushFace != Face->Merged->Portal->Side->grBrushFace)
			return GR_FALSE;	// Can't propogate, face merged accros multiple brush faces
	}

	if (!PropogateFaceInfoToDrawFaces_r(Face, BSP, Side))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_DiryNodes_r
//=======================================================================================
grBoolean grBSPNode_DirtyNodes_r(grBSPNode *Node, grBSP *BSP, grBSP_TopSide *Side)
{
	int32				PSide;
	grBSPNode_Portal	*Portal;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Side);

	if (!Node->Leaf)		// Recurse to leafs
	{
		if (!grBSPNode_DirtyNodes_r(Node->Children[NODE_FRONT], BSP, Side))
			return GR_FALSE;
		if (!grBSPNode_DirtyNodes_r(Node->Children[NODE_BACK], BSP, Side))
			return GR_FALSE;

		return GR_TRUE;
	}

	// At leaf, find portals that use this side
	for (Portal = Node->Portals; Portal; Portal = Portal->Next[PSide])
	{
		PSide = (Portal->Nodes[1] == Node);
		
		if (Portal->Side == Side)
		{
			assert(Portal->OnNode);

			#if 1
				Portal->OnNode->Flags |= NODE_REBUILD_FACES;		// Flag node to rebuild face
			#else
			{
				int32		i;

				// First, try to propogate this info to the drawfaces
				//	If DrawFaces merged across multiple brush faces, then we must rebuild the node faces
				for (i=0; i<2; i++)
				{
					if (!Portal->Face[i])
						continue;

					if (!PropogateFaceInfoToDrawFaces(Portal->Face[i], BSP, Side))
					{
						Portal->OnNode->Flags |= NODE_REBUILD_FACES;		// Flag node to rebuild face
						break;
					}
					// At least one face got propogated, so update lights on node
					Portal->OnNode->Flags |= NODE_UPDATELIGHTS;
				}
			}
			#endif
		}

	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_PropogateTopSideFlags
//=======================================================================================
grBoolean grBSPNode_PropogateTopSideFlags_r(grBSPNode *Node, grBSP *BSP, grBSP_TopSide *Side, int32 *NumPropogated)
{
	int32		i;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Side);

	if (Node->Leaf)
		return GR_TRUE;

	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace		*DFace;

		DFace = Node->DrawFaces[i];

		if (DFace->grBrushFace == Side->grBrushFace)
		{
			(*NumPropogated)++;
			DFace->TopSideFlags = Side->TopSideFlags;
		}
	}

	if (!grBSPNode_PropogateTopSideFlags_r(Node->Children[NODE_FRONT], BSP, Side, NumPropogated))
		return GR_FALSE;
	if (!grBSPNode_PropogateTopSideFlags_r(Node->Children[NODE_BACK], BSP, Side, NumPropogated))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_DestroyDrawFaces
//=======================================================================================
void grBSPNode_DestroyDrawFaces(grBSPNode *Node, grBSP *BSP)
{
	int32				i;

	assert(Node);
	assert(!(Node->Flags & NODE_LEAF));
	assert(!Node->Leaf);

	if (!Node->DrawFaces)
	{
		assert(Node->NumDrawFaces == 0);
		return;
	}

	for (i=0; i<Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*DFace;
		
		DFace = Node->DrawFaces[i];

		assert(DFace->Poly);		// DFace is bad if no poly
		grBSPNode_DrawFaceDestroy(&DFace, BSP);
	}

	grRam_Free(Node->DrawFaces);

	Node->DrawFaces = NULL;
	Node->NumDrawFaces = 0;
}

//=======================================================================================
//	grBSPNode_DestroyPortals
//=======================================================================================
void grBSPNode_DestroyPortals(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Portal	*Portal, *NextPortal;
	int32				Side;

	assert(Node);

	// Free all the portals on this node
	for (Portal = Node->Portals; Portal; Portal = NextPortal)
	{
		assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));	// Should be on one side or the other (not both)
		assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);		// Should be on at least one side

		Side = (Portal->Nodes[1] == Node);

		NextPortal = Portal->Next[Side];

		// Remove the portal from BOTH nodes that it seperates...
		grBSPNode_RemovePortal(Portal->Nodes[0], Portal);
		grBSPNode_RemovePortal(Portal->Nodes[1], Portal);

		grBSPNode_PortalDestroy(&Portal, BSP);
	}

	assert(Node->Portals == NULL);		// Above code SHOULD have removed all portals
}

//=======================================================================================
//	grBSPNode_InitializeRootPortals
//	Create portals on nodes box, and points front side of portals to node, back side to OutsideNode
//=======================================================================================
grBSPNode *grBSPNode_InitializeRootPortals(grBSPNode *Node, grBSP *BSP)
{
	int32				k, i, Index;
	grPlane				Planes[6];
	grBSPNode_Portal	*Portals[6];
	grVec3d				Mins, Maxs;
	grBSPNode			*OutsideNode;

	OutsideNode = grBSPNode_Create(BSP);

	if (!OutsideNode)
		return NULL;

	OutsideNode->Leaf = grBSPNode_LeafCreate(BSP);

	if (!OutsideNode->Leaf)
		goto ExitWithError;

	// The outside node
	OutsideNode->Flags = NODE_LEAF | NODE_OUTSIDE;
	OutsideNode->Leaf->Contents = GR_BSP_CONTENTS_SOLID;
	OutsideNode->Leaf->Node = OutsideNode;

	memset(Planes, 0, 6*sizeof(grPlane));
	memset(Portals, 0, 6*sizeof(grBSPNode_Portal*));

	// Get extents of node
	assert(grExtBox_IsValid(&Node->Box));

	Mins = Node->Box.Min;
	Maxs = Node->Box.Max;

#if 0
	// Get the box of the node, and expand it a little
	for (k=0; k< 3; k++)
	{
		float	Min, Max;

		Min = grVec3d_GetElement(&Mins, k) - OUTSIDE_PADDING;
		Max = grVec3d_GetElement(&Maxs, k) + OUTSIDE_PADDING;

		if (Min <= -GR_BSP_MINMAX_BOUNDS)
			goto ExitWithError;	// Not enough room to expand
		if (Max >= GR_BSP_MINMAX_BOUNDS)
			goto ExitWithError;	// Not enough room to expand

		grVec3d_SetElement(&Mins, k, Min);
		grVec3d_SetElement(&Maxs, k, Max);
	}
#else
	Mins.X = (-GR_BSP_MINMAX_BOUNDS)+1.0f;
	Mins.Y = (-GR_BSP_MINMAX_BOUNDS)+1.0f;
	Mins.Z = (-GR_BSP_MINMAX_BOUNDS)+1.0f;
	
	Maxs.X =  GR_BSP_MINMAX_BOUNDS-1.0f;
	Maxs.Y =  GR_BSP_MINMAX_BOUNDS-1.0f;
	Maxs.Z =  GR_BSP_MINMAX_BOUNDS-1.0f;
#endif
	// Create 6 portals on this box, and point to the outsidenode and the Node
	for (i=0; i<3; i++)
	{
		for (k=0; k<2; k++)
		{
			grPoly				*Poly;
			grBSPNode_Portal	*Portal;

			Index = k*3 + i;

			grVec3d_Clear(&Planes[Index].Normal);

			if (k == 0)
			{
				grVec3d_SetElement(&Planes[Index].Normal, i, 1.0f);
				Planes[Index].Dist = grVec3d_GetElement(&Mins, i);
			}
			else
			{
				grVec3d_SetElement(&Planes[Index].Normal, i, -1.0f);
				Planes[Index].Dist = -grVec3d_GetElement(&Maxs, i);
			}
			
			Planes[Index].Type = Type_Any;	// Not found yet

			Poly = grPoly_CreateFromPlane(&Planes[Index], GR_BSP_MINMAX_BOUNDS);

			if (!Poly)
				goto ExitWithError;

			Portal = grBSPNode_PortalCreate(Poly, BSP);

			if (!Portal)
				goto ExitWithError;
			
			Portal->PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Planes[Index]);

			if (Portal->PlaneIndex == GR_PLANEARRAY_NULL_INDEX)
				goto ExitWithError;

			if (!grPlaneArray_IndexSided(Portal->PlaneIndex))
				ADD_PORTAL_TO_NODES(Portal, Node, OutsideNode);
			else
				ADD_PORTAL_TO_NODES(Portal, OutsideNode, Node);

			Portals[Index] = Portal;
		}
	}
							  
	// Clip the portals against each other, to form a perfect skin of the box
	for (i=0; i< 6; i++)
	{
		for (k=0; k< 6; k++)
		{
			if (k == i)
				continue;

			if (!grPoly_ClipEpsilon(&Portals[i]->Poly, GR_BSP_PLANESIDE_EPSILON, &Planes[k], GR_FALSE))
			{
				grErrorLog_AddString(-1, "grBSPNode_InitializeRootPortals:  grPoly_ClipEpsilon failed.", NULL);
				goto ExitWithError;
			}

			if (!Portals[i]->Poly)
			{
				grErrorLog_AddString(-1, "grBSPNode_InitializeRootPortals:  Portal was clipped away.", NULL);
				goto ExitWithError;
			}
		}
	}

	return OutsideNode;

	ExitWithError:
	{
		if (OutsideNode)	// Don't destroy leaf, node will do that for us...
			grBSPNode_Destroy_r(&OutsideNode, BSP);

		for (i=0; i<6; i++)
			if (Portals[i])
				grBSPNode_PortalDestroy(&Portals[i], BSP);

		return NULL;
	}
}

//=======================================================================================
//	grBSPNode_CalcBoundsFromPortals
//	Calcs bounds for nodes, and leafs
//=======================================================================================
void grBSPNode_CalcBoundsFromPortals(grBSPNode *Node)
{
	grBSPNode_Portal	*p;
	grBoolean			Set;
	int32				s, i;

	assert(Node);

	Set = GR_FALSE;

	for (p=Node->Portals; p; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		for (i=0; i<p->Poly->NumVerts; i++)
		{
			if (!Set)
				grExtBox_SetToPoint(&Node->Box, &p->Poly->Verts[i]);
			else
				grExtBox_ExtendToEnclose(&Node->Box, &p->Poly->Verts[i]);

			Set = GR_TRUE;
		}
	}
}

//=======================================================================================
//	grBSPNode_PartitionPortals_r
//	Calcs the bounds for node by taking bounds of the current portal set
//	Then takes the portals on a node, and distributes them to the nodes children
//=======================================================================================
grBoolean grBSPNode_PartitionPortals_r(grBSPNode *Node, grBSP *BSP, grBoolean IncludeDetail)
{
	grBSPNode_CalcBoundsFromPortals(Node);	// Calcs bounds for nodes and leafs
	
	if (Node->Flags & NODE_LEAF)			// At leaf, no more recursing
		return GR_TRUE;

	if (!IncludeDetail && (Node->Flags & NODE_DETAIL))	// Stop at detail, if told to do so
		return GR_TRUE;

	// Initialize the portal on this node
	if (!grBSPNode_InitializePortal(Node, BSP))
		return GR_FALSE;

	// Distribute the portals to this nodes children
	if (!grBSPNode_DistributePortalsToChildren(Node, BSP))
		return GR_FALSE;

	// Take the portals on the 2 children, and ditribute those as well
	if (!grBSPNode_PartitionPortals_r(Node->Children[NODE_FRONT], BSP, IncludeDetail))
		return GR_FALSE;

	if (!grBSPNode_PartitionPortals_r(Node->Children[NODE_BACK], BSP, IncludeDetail))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_CreatePoly
//	Creates a huge poly on the node, and clips it against all the nodes parents...
//	Poly can come back NULL, and not be an error.  The poly just got clipped away...
//=======================================================================================
grBoolean grBSPNode_CreatePoly(const grBSPNode *Node, grBSP *BSP, grPoly **PolyOut)
{
	grPoly			*Poly;
	const grPlane	*Plane;
	grBSPNode		*n;

	Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	
	Poly = grPoly_CreateFromPlane(Plane, GR_BSP_MINMAX_BOUNDS);

	if (!Poly)
		return GR_FALSE;

	// Clip this poly by all the parents of this node
	for (n = Node->Parent ; n && Poly ; )
	{
		grBoolean	Side;

		Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, n->PlaneIndex);

		Side = (n->Children[0] == Node) ? GR_FALSE : GR_TRUE;
		
		if (!grPoly_ClipEpsilon(&Poly, 0.001f, Plane, Side))
			return GR_FALSE;

		Node = n;
		n = n->Parent;
	}

	*PolyOut = Poly;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_InitializePortal
//	Create a portal on the node.  Clips it by all nodes parents, and current portals.
//	Then points the portal to the nodes 2 children
//=======================================================================================
grBoolean grBSPNode_InitializePortal(grBSPNode *Node, grBSP *BSP)
{
	grPoly				*Poly;
	grBSPNode_Portal	*Portal;
	int32				Side;

	// Create a new portal
	if (!grBSPNode_CreatePoly(Node, BSP, &Poly))
		return GR_FALSE;

	// Clip it against all other portals attached to this node
	for (Portal = Node->Portals; Portal && Poly; Portal = Portal->Next[Side])
	{
		const grPlane	*pPlane;

		assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);
		assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
		
		Side = (Portal->Nodes[1] == Node);

		pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Portal->PlaneIndex);

		if (!grPoly_ClipEpsilon(&Poly, 0.001f, pPlane, Side))
			return GR_FALSE;

		if (!Poly)
		{
			break;
		}
	}
	
	if (Poly && grPoly_IsTiny(Poly))
		grPoly_Destroy(&Poly);

	if (Poly)
	{
		Portal = grBSPNode_PortalCreate(Poly, BSP);

		if (!Portal)
			return GR_FALSE;

		Portal->PlaneIndex = Node->PlaneIndex;
		Portal->OnNode = Node;

		if (!grBSPNode_PortalIsValid(Portal))
		{
			grErrorLog_AddString(-1, "grBSPNode_InitializePortal:  grBSPNode_PortalIsValidGeometry failed.\n", NULL);
			return GR_FALSE;
		}
		
		// Make the portal look at nodes children
		ADD_PORTAL_TO_NODES(Portal, Node->Children[0], Node->Children[1]);
	}
	else
	{
		Log_Printf("grBSPNode_InitializePortal:  Portal was cut away.\n");
	}


	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_DistributePortalsToChildren
//	Take all the portals that look at this node, and distributes to the nodes children
//=======================================================================================
grBoolean grBSPNode_DistributePortalsToChildren(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Portal	*Portal, *Next;
	const grPlane		*pPlane;
	grBSPNode			*Front, *Back;
	
	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	assert(pPlane);

	Front = Node->Children[0];
	Back = Node->Children[1];

	// Partition all portals by this node
	for (Portal = Node->Portals; Portal; Portal = Next)
	{
		int32				Side;
		grPoly				*FPoly, *BPoly;
		grBSPNode_Portal	*NewPortal;
		grBSPNode			*OppositeNode;

		assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);
		assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
		
		Side = (Portal->Nodes[1] == Node);

		Next = Portal->Next[Side];
		
		// Remember the node on the opposite side
		OppositeNode = Portal->Nodes[!Side];

		// Remove both nodes from this portal, split it, then add the split portals back to the nodes
		// that they truly look at
		grBSPNode_RemovePortal(Portal->Nodes[0], Portal);
		grBSPNode_RemovePortal(Portal->Nodes[1], Portal);

		if (!grPoly_SplitEpsilon(&Portal->Poly, 0.001f, pPlane, GR_FALSE, &FPoly, &BPoly))
		{
			grErrorLog_AddString(-1, "grBSPNode_DistributePortalsToChildren:  grPoly_SplitEpsilon failed.\n", NULL);
			return GR_FALSE;
		}
		
		if (FPoly && grPoly_IsTiny(FPoly))
			grPoly_Destroy(&FPoly);

		if (BPoly && grPoly_IsTiny(BPoly))
			grPoly_Destroy(&BPoly);
		
		if (!FPoly && !BPoly)			// Both tiny, or clipped away
			continue;
		
		if (!FPoly)						// On back side
		{
			Portal->Poly = BPoly;
			if (Side)
				ADD_PORTAL_TO_NODES(Portal, OppositeNode, Back);
			else
				ADD_PORTAL_TO_NODES(Portal, Back, OppositeNode);
			continue;
		}

		if (!BPoly)						// On front side
		{
			Portal->Poly = FPoly;
			if (Side)
				ADD_PORTAL_TO_NODES(Portal, OppositeNode, Front);
			else
				ADD_PORTAL_TO_NODES(Portal, Front, OppositeNode);
			continue;
		}

		// Portal was split
		NewPortal = grBSPNode_PortalCreate(BPoly, BSP);
		
		if (!NewPortal)
			return GR_FALSE;

		*NewPortal = *Portal;
		NewPortal->Poly = BPoly;

		Portal->Poly = FPoly;
		
		if (Side)
		{
			ADD_PORTAL_TO_NODES(Portal, OppositeNode, Front);
			ADD_PORTAL_TO_NODES(NewPortal, OppositeNode, Back);
		}
		else
		{
			ADD_PORTAL_TO_NODES(Portal, Front, OppositeNode);
			ADD_PORTAL_TO_NODES(NewPortal, Back, OppositeNode);
		}
	}

	assert(Node->Portals == NULL);	// All portals SHOULD have been removed, and distributed to the nodes children!!!

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_AddPortal
//=====================================================================================
void grBSPNode_AddPortal(grBSPNode *Node, grBSPNode_Portal *Portal, int32 Side)
{
	assert(!Portal->Nodes[Side]);

	Portal->Nodes[Side] = Node;
	Portal->Next[Side] = Node->Portals;
	Node->Portals = Portal;
}

//=====================================================================================
//	grBSPNode_RemovePortal
//	Finds a portal on the node, and removes it from the list of portals on the node
//=====================================================================================
void grBSPNode_RemovePortal(grBSPNode *Node, grBSPNode_Portal *Portal)
{
	int32				Side;
	grBSPNode_Portal	*p, **p2;
	
	assert(Node->Portals);		// Better have some portals on this node
	assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
	assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);	

	grBSPNode_PortalResetTopSide(Portal);

	// Find the portal on this node
	for (p2 = &Node->Portals, p = *p2; p; p2 = &p->Next[Side], p = *p2)
	{
		assert(!(p->Nodes[0] == Node && p->Nodes[1] == Node));
		assert(p->Nodes[0] == Node || p->Nodes[1] == Node);

		Side = (p->Nodes[1] == Node);	// Get the side of the portal that this node is on

		if (p == Portal)
			break;			// Got it
	}
	 
	assert(p && p2 && *p2);

	Side = (Portal->Nodes[1] == Node);	// Get the side of the portal that the node was on
	
	*p2 = Portal->Next[Side];
	Portal->Nodes[Side] = NULL;
}


//=======================================================================================
//	grBSPNode_BuildContents
//=======================================================================================
grBoolean grBSPNode_BuildContents(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Leaf	*Leaf;
	grBSP_Brush		*Brush;
	uint32			BestOrder;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);

	Leaf = Node->Leaf;

	// Reset the leaf contents
	Leaf->BrushContents = 0;
	Leaf->Contents = BSP->DefaultContents;

	BestOrder = 0;

	// Find the highest cut order
	for (Brush = Leaf->Brushes; Brush; Brush = Brush->Next)
	{
		grBSP_TopBrush		*TopBrush;

		TopBrush = Brush->Original;

		if (TopBrush->Contents & GR_BSP_CONTENTS_AIR)
		{
			if (TopBrush->Order >= BestOrder)			// Remember the cut brush with the highest order
				BestOrder = TopBrush->Order;

			Leaf->Contents &= ~GR_BSP_CONTENTS_SOLID;	// Since there is a cut, remove all solid from default contents
		}
	}

	// "or" together all the contents of the brushes that make up this leaf
	for (Brush = Leaf->Brushes; Brush; Brush = Brush->Next)
	{
		grBSP_TopBrush		*TopBrush;
		uint32				Contents;

		TopBrush = Brush->Original;
		Contents = TopBrush->Contents;

		// Only keep contents if it is higher than the highest cut contents
		if (TopBrush->Order >= BestOrder || (Contents & GR_BSP_CONTENTS_EMPTY))
		{
			Leaf->Contents |= Contents;
			Leaf->BrushContents |= Contents;
		}
	}

	// Solid overrides everything
	if ((Leaf->BrushContents|Leaf->Contents) & GR_BSP_CONTENTS_SOLID)
		Leaf->Contents = GR_BSP_CONTENTS_SOLID;

	return GR_TRUE;
}

int32	NumAirLeafs = 0;
int32	NumSolidLeafs = 0;

//=======================================================================================
//	grBSPNode_InitializeLeaf
//	Converts a node into a leaf, and create the contents
//=======================================================================================
grBoolean grBSPNode_InitializeLeaf(grBSPNode *Node, grBSP *BSP, grBSP_Brush *Brushes)
{
	grBSPNode_Leaf	*Leaf;

	assert(!Node->Leaf);

	Leaf = grBSPNode_LeafCreate(BSP);

	if (!Leaf)
		return GR_FALSE;

	Node->Flags = NODE_LEAF;	// Node is a leaf
	Node->PlaneIndex = GR_PLANEARRAY_NULL_INDEX;
	Node->Side = NULL;

	Node->Leaf = Leaf;
	Leaf->Node = Node;
	Leaf->Brushes = Brushes;		// Remember the list

	// Build the contents of this leaf
	grBSPNode_BuildContents(Node, BSP);

	if (Leaf->Contents & GR_BSP_CONTENTS_AIR)
		NumAirLeafs++;
	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		NumSolidLeafs++;

#if 0
	// Create the leaf sides from the first brush
	if (!grBSPNode_LeafInitializeSides(Leaf))
	{
		grBSPNode_LeafDestroy(&Node->Leaf);
		return GR_FALSE;
	}
#endif

	// Free up some memory
	{
		grBSP_Brush	*Brush;
		int32		i;

		// Don't need to keep polygons anymore...
		for (Brush = Brushes; Brush; Brush = Brush->Next)
		{
			for (i=0; i< Brush->NumSides; i++)
			{
				if (Brush->Sides[i].Poly)
					grPoly_Destroy(&Brush->Sides[i].Poly);
			}

			Brush->Flags &= ~BSPBRUSH_FORCEBOTH;		// Remove FORCEBOTH flag...
		}
	}

	return GR_TRUE;
}

static int32 NumMergedLeafs;

//=======================================================================================
//	grBSPNode_MergeLeafs_r
//=======================================================================================
void grBSPNode_MergeLeafs_r(grBSPNode *Node, grBSP *BSP)
{
	grBSP_Brush		*b, *Next;
	grBSPNode_Leaf	*lf, *lb;

	if (Node->Flags & NODE_LEAF)		
	{
		assert(Node->Leaf);
		return;
	}
	
	assert(!Node->Leaf);

	// Recurse to leafs, then start merging back up the stack...
	grBSPNode_MergeLeafs_r(Node->Children[0], BSP);
	grBSPNode_MergeLeafs_r(Node->Children[1], BSP);

	lf = Node->Children[0]->Leaf;
	lb = Node->Children[1]->Leaf;

	// If there are leafs on both sides, and...
	// If contents are the same on both sides (or solid on both), nodes can be merged
	if ((lf && lb) && ((lf->Contents == lb->Contents) || 
		((lf->Contents & GR_BSP_CONTENTS_SOLID) && (lb->Contents & GR_BSP_CONTENTS_SOLID))) )
	{
		grBSPNode_Leaf	*NewLeaf;			

		assert(!Node->Faces);				// Same contents should NOT have faces
		assert(!Node->Children[0]->Faces);
		assert(!Node->Children[1]->Faces);

		// Create a new leaf
		NewLeaf = grBSPNode_LeafCreate(BSP);

		// Remove the reference to this plane the node was using
		grPlaneArray_RemovePlane(BSP->PlaneArray, &Node->PlaneIndex);

		Node->Flags = NODE_LEAF;
		Node->Leaf = NewLeaf;			

		NewLeaf->Node = Node;
		NewLeaf->Contents = lb->Contents;
		NewLeaf->Brushes = lb->Brushes;

		// Combine front brushlist with back
		for (b=lf->Brushes ; b ; b=Next)
		{
			Next = b->Next;
			b->Next = NewLeaf->Brushes;
			NewLeaf->Brushes = b;
		}

		// front back leaf no longer contain brushes, since this new leaf took them
		lf->Brushes = NULL;
		lb->Brushes = NULL;

		// Destroy the nodes children
		grBSPNode_Destroy_r(&Node->Children[0], BSP);
		grBSPNode_Destroy_r(&Node->Children[1], BSP);

		NumMergedLeafs++;
	}
}

//=======================================================================================
//	grBSPNode_MergeLeafs
//=======================================================================================
void grBSPNode_MergeLeafs(grBSPNode *Node, grBSP *BSP)
{
	NumMergedLeafs = 0;

	Log_Printf("--- grBSPNode_MergeLeafs ---\n");

	grBSPNode_MergeLeafs_r(Node, BSP);

	Log_Printf("Num Merged Leafs       : %5i\n", NumMergedLeafs);
}

//=====================================================================================
//	grBSPNode_FindLeaf
//=====================================================================================
grBSPNode_Leaf *grBSPNode_FindLeaf(const grBSPNode *Node, grBSP *BSP, const grVec3d *Pos)
{
	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Pos);

	while(!(Node->Flags & NODE_LEAF))		// Go to leafs
	{
		const grPlane	*Plane;
		grFloat			Dist;

		Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
		
		Dist = grPlane_PointDistanceFast(Plane, Pos);	// We can use fast check, since node planes face positive

		if (Dist > 0)
			Node = Node->Children[0];
		else
			Node = Node->Children[1];
	}

	return Node->Leaf;
}

int32 NumFilledLeafs;

//=====================================================================================
//	grBSPNode_FillUnTouchedLeafs_r
//=====================================================================================
void grBSPNode_FillUnTouchedLeafs_r(grBSPNode *Node, int32 Fill)
{
	if (!(Node->Flags & NODE_LEAF))		// Recurse to leafs
	{
		assert(!Node->Leaf);
		grBSPNode_FillUnTouchedLeafs_r(Node->Children[0], Fill);
		grBSPNode_FillUnTouchedLeafs_r(Node->Children[1], Fill);
		return;
	}
	
	assert(Node->Leaf);

	if ((Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID))
		return;		// Already solid or removed...

	if (Node->Leaf->CurrentFrame != Fill)
	{
		// Fill er in with solid so it does not show up...(Preserve user contents)
		Node->Leaf->Contents &= (0xffff0000);
		Node->Leaf->Contents |= GR_BSP_CONTENTS_SOLID;
		NumFilledLeafs++;
	}
}

int32 NumMakeFaces;
int32 NumMergedFaces;
int32 NumSubdividedFaces;

//=======================================================================================
//	grBSPNode_SubdivideFaces
//=======================================================================================
grBoolean grBSPNode_SubdivideFaces(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Face	*Face;
	int32			NumSubdivided;

	NumSubdivided = 0;

	for (Face = Node->Faces; Face; Face = Face->Next)
	{
		if (!grBSPNode_FaceSubdivide(Face, BSP, Node, 230.0f, &NumSubdivided))
			return GR_FALSE;
	}

	NumSubdividedFaces += NumSubdivided;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeFaces
//=======================================================================================
grBoolean grBSPNode_MakeFaces(grBSPNode *Node, grBSP *BSP, grBoolean FreeMergedSplit)
{
	if (!(Node->Flags & NODE_REBUILD_FACES))
		return GR_TRUE;		// Node is not marked to rebuild any faces
		
	// Destroy all but the original faces created off the portals on this node
	grBSPNode_DestroyBSPFaces(Node, BSP, GR_TRUE);

	// Destroy any previous DrawFaces on the node
	grBSPNode_DestroyDrawFaces(Node, BSP);

	// Merge list
	if (Node->Faces)
	{
		int32		NumMerged = 0;

		#if 1
		{
			// Merge faces 
			if (!grBSPNode_FaceMergeList(Node->Faces, BSP, &NumMerged))
				return GR_FALSE;
			}
		#endif

		NumMergedFaces += NumMerged;

		// Subdivide them up for lightmaps... 
		if (!grBSPNode_SubdivideFaces(Node, BSP))
			return GR_FALSE;
	}

	// Make the new DrawFaces on this node from the final BSP faces
	if (!grBSPNode_MakeDrawFaces(Node, BSP))
		return GR_FALSE;

	if (FreeMergedSplit)
	{
		// Keep only originals
		grBSPNode_DestroyBSPFaces(Node, BSP, GR_TRUE);
	}

	Node->Flags &= ~NODE_REBUILD_FACES;		// This node is up to date with the faces
	Node->Flags |= NODE_UPDATELIGHTS;		// This node needs light

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeFaces_r
//	Examines all portals on leafs, and checks to see if any portal needs a face...
//=======================================================================================
grBoolean grBSPNode_MakeFaces_r(grBSPNode *Node, grBSP *BSP, grBoolean FreeMergedSplit)
{
	grBSPNode_Portal	*p;
	int32				s;

	// Recurse down to leafs
	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);
		assert(!Node->Portals);		// At this point, portals should be at leafs ONLY

		if (!grBSPNode_MakeFaces_r(Node->Children[NODE_FRONT], BSP, FreeMergedSplit))
			return GR_FALSE;

		if (!grBSPNode_MakeFaces_r(Node->Children[NODE_BACK], BSP, FreeMergedSplit))
			return GR_FALSE;
		
		if (!grBSPNode_MakeFaces(Node, BSP, FreeMergedSplit))
			return GR_FALSE;

		return GR_TRUE;
	}
	assert(Node->Leaf);

	// Solid leafs never have visible faces
	if (Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;

	// See which portals are valid, and need faces
	// If any portals needs faces, create the face on the side of the portal that
	// looks into this leaf...
	for (p = Node->Portals; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->Side)		// Portal does not seperate visible contents
		{
			assert(!p->Face[0] && !p->Face[1]);	// portal should have NO faces in this case
			continue;
		}

		if (p->Face[s])		// Portal already has a face that looks into this leaf
		{
			// Keep topsideflags up to date
			p->Face[s]->TopSideFlags = p->Side->TopSideFlags;
			continue;
		}

		// Create a face on the portal that looks into this leaf
		p->Face[s] = grBSPNode_FaceCreateFromPortal(p, s);

		if (!p->Face[s])
			return GR_FALSE;
		
		// Record the contents that the face looks into (this leaf)
		p->Face[s]->Contents = Node->Leaf->Contents;			// Front side contents is this leaf

		// Add the face to the list of faces on the node that originaly created the portal
		p->Face[s]->Next = p->OnNode->Faces;
		p->OnNode->Faces = p->Face[s];

		// When we recurse back up the stack, nodes need to know if they need
		// an update.  IOW, we don't want to try to merge the list, if NO new faces were
		// created on the node...
		p->OnNode->Flags |= NODE_REBUILD_FACES;		// Flag the node that faces need to be rebuilt

		NumMakeFaces++;
	}

	return GR_TRUE;
}


//=======================================================================================
//	grBSPNode_MakeFaces_Callr
//=======================================================================================
grBoolean grBSPNode_MakeFaces_Callr(grBSPNode *Node, grBSP *BSP, grBoolean FreeMergedSplit)
{
	Log_Printf("--- grBSPNode_MakeFaces	---\n");

	NumMergedFaces = 0;
	NumSubdividedFaces = 0;
	NumMakeFaces = 0;

	if (!grBSPNode_MakeFaces_r(Node, BSP, FreeMergedSplit))
		return GR_FALSE;

	Log_Printf("TotalFaces             : %5i\n", NumMakeFaces);
	Log_Printf("Merged Faces           : %5i\n", NumMergedFaces);
	Log_Printf("Subdivided Faces       : %5i\n", NumSubdividedFaces);
	Log_Printf("FinalFaces             : %5i\n", (NumMakeFaces-NumMergedFaces)+NumSubdividedFaces);
	
	BSP->DebugInfo.NumMakeFaces = NumMakeFaces;
	BSP->DebugInfo.NumMergedFaces = NumMergedFaces;
	BSP->DebugInfo.NumSubdividedDrawFaces = NumSubdividedFaces;
	BSP->DebugInfo.NumDrawFaces = (NumMakeFaces-NumMergedFaces)+NumSubdividedFaces;

	return GR_TRUE;
}

grVec3d VecTable[6] = { 
	{-1.0f, 0.0f, 0.0f, 0.0f},		// Left Wall
	{ 1.0f, 0.0f, 0.0f, 0.0f},		// Right Wall
	{ 0.0f, 0.0f,-1.0f, 0.0f},		// Back Wall
	{ 0.0f, 0.0f, 1.0f, 0.0f},		// Front Wall
	{ 0.0f, 1.0f, 0.0f, 0.0f},		// Ceiling
	{ 0.0f,-1.0f, 0.0f, 0.0f}		// Floor
};

//=======================================================================================
//	BuildDrawFaceXForm
//=======================================================================================
static grBoolean BuildDrawFaceXForm(grBSPNode_DrawFace *DFace, grBSP *BSP)
{
	grFloat		BestDot;
	int32		i, BestIndex;
	grVec3d		Left, Up, In;
	grPlane		Plane;

	assert(DFace);
	assert(DFace->PortalXForm);
	assert(DFace->grBrushFace);

	BestDot = -1.0f;
	BestIndex = -1;

	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, DFace->PlaneIndex);

	if (!grPlaneArray_IndexSided(DFace->PlaneIndex))
		grPlane_Inverse(&Plane);

	for (i=0; i<6; i++)
	{
		grFloat		Dot;

		Dot = grVec3d_DotProduct(&Plane.Normal, &VecTable[i]);

		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestIndex = i;
		}

	}

	switch (BestIndex)
	{
		case 0:				// Left Wall
			grVec3d_Set(&Left, 0.0f, 0.0f, 1.0f);
			grVec3d_Set(&Up, 0.0f, 1.0f, 0.0f);
			break;
		case 1:				// Right Wall
			grVec3d_Set(&Left, 0.0f, 0.0f,-1.0f);
			grVec3d_Set(&Up, 0.0f, 1.0f, 0.0f);
			break;
		case 2:				// Back Wall
			grVec3d_Set(&Left, -1.0f, 0.0f, 0.0f);
			grVec3d_Set(&Up, 0.0f, 1.0f, 0.0f);
			break;
		case 3:				// Front Wall
			grVec3d_Set(&Left,  1.0f, 0.0f, 0.0f);
			grVec3d_Set(&Up, 0.0f, 1.0f, 0.0f);
			break;
		case 4:				// Ceiling
			grVec3d_Set(&Left,  -1.0f, 0.0f, 0.0f);
			grVec3d_Set(&Up, 0.0f, 0.0f, 1.0f);
			break;
		case 5:				// Floor
			grVec3d_Set(&Left,  -1.0f, 0.0f, 0.0f);
			grVec3d_Set(&Up, 0.0f, 0.0f,-1.0f);
			break;
		default:
			assert(0);
	}
	
	grVec3d_Copy(&Plane.Normal, &In);

	grVec3d_CrossProduct(&Up, &In, &Left);
	grVec3d_Normalize(&Left);

	grVec3d_CrossProduct(&In, &Left, &Up);
	grVec3d_Normalize(&Up);

	grXForm3d_SetFromLeftUpIn(DFace->PortalXForm, &Left, &Up, &In);

	// Calculate center of original brush face, and use that as reference point
	{
		grVec3d		Center, Vert;
		int32		NumVerts;

		NumVerts = grBrush_FaceGetVertCount(DFace->grBrushFace);

		grVec3d_Clear(&Center);

		for (i=0; i< NumVerts; i++)
		{
			Vert = grBrush_FaceGetWorldSpaceVertByIndex(DFace->grBrushFace, i);

			grVec3d_Add(&Center, &Vert, &Center);
		}

		grVec3d_Scale(&Center, 1.0f/(grFloat)NumVerts, &Center);

		grXForm3d_Translate(DFace->PortalXForm, Center.X, Center.Y, Center.Z);

	}

	// Rotate, and translate the reference point by using texture rotate/shift info
	#if 0
	{
		grFloat				ShiftU, ShiftV;
		const grFaceInfo	*pFaceInfo;
		grXForm3d			XForm;
		grQuaternion		Quat;

		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, DFace->FaceInfoIndex);
		assert(pFaceInfo);

		ShiftU = -pFaceInfo->ShiftU;
		ShiftV = pFaceInfo->ShiftV;

		grXForm3d_Translate(DFace->PortalXForm, Left.X*ShiftU, Left.Y*ShiftU, Left.Z*ShiftU);
		grXForm3d_Translate(DFace->PortalXForm, Up.X*ShiftV, Up.Y*ShiftV, Up.Z*ShiftV);

		grQuaternion_SetFromAxisAngle(&Quat, &In, (pFaceInfo->Rotate/180.0f)*GR_PI);
		grQuaternion_ToMatrix(&Quat, &XForm);

		grXForm3d_Multiply(&XForm, DFace->PortalXForm, DFace->PortalXForm);
	}
	#endif

	return GR_TRUE;
}

static	int32	NumDrawFaces;
static	int32	RGBHack = 0;

//=======================================================================================
//	grBSPNode_MakeDrawFaces
//=======================================================================================
grBoolean grBSPNode_MakeDrawFaces(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Face		*Face;
	int32				NumFaces;
	grBSPNode_DrawFace	*DFace;

	assert(Node);
	assert(!(Node->Flags&NODE_LEAF));
	assert(!Node->Leaf);

	// Free old draw faces... (if any)
	grBSPNode_DestroyDrawFaces(Node, BSP);

	// Count the bsp faces
	NumFaces = 0;
	for (Face = Node->Faces; Face; Face = Face->Next)
	{
		if (Face->Merged || Face->Split[0] || Face->Split[1])
			continue;		// Only interested in final faces

		NumFaces++;		
	}

	// Create the draw faces (array of pointers to drawfaces, in the DrawFace Array)
	assert(NumFaces < GR_BSP_MAX_DRAW_FACES);
	
	Node->DrawFaces = GR_RAM_ALLOCATE_ARRAY(grpBSPNode_DrawFace, NumFaces);

	if (!Node->DrawFaces)
	{
		grErrorLog_AddString(-1, "grBSPNode_MakeDrawFaces:  Out of memory for DrawFaces.", NULL);
		return GR_FALSE;
	}

	// Clear out entire array of pointers
	ZeroMemArray(Node->DrawFaces, NumFaces);

	// Now, build the draw faces from the BSP faces
	NumFaces = 0;
	for (Face = Node->Faces; Face; Face = Face->Next)
	{
		int32					v;
		const grFaceInfo		*pFaceInfo;
		grVec3d					*pVert;
		grExtBox				Box;
		grFaceInfo_ArrayIndex	FaceInfoIndex;
		grBSP_TopSide			*TopSide;

		assert(Face->Portal);
		assert(Face->Portal->Side);

		if (Face->Merged || Face->Split[0] || Face->Split[1])
			continue;		// Only interested in final faces

		// Grab the TopSide
		TopSide = Face->Portal->Side;

		FaceInfoIndex = TopSide->FaceInfoIndex;

		pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, FaceInfoIndex);
		assert(pFaceInfo);

	#if 0
		if (pFaceInfo->Flags & FACEINFO_NO_DRAWFACE || (TopSide->Flags & SIDE_VIS_PORTAL))
			continue;		// Don't create draw faces for vis portal faces...
	#endif

		Node->DrawFaces[NumFaces] = grBSPNode_DrawFaceCreate(BSP);
		DFace = Node->DrawFaces[NumFaces];

		if (!DFace)
			return GR_FALSE;

		NumFaces++;

		// If face does not span multiple brush faces, mark the brush face it used
		if (!(Face->Flags & BSPFACE_SPAN_MULTIPLE_BRUSH_FACES))
			DFace->grBrushFace = TopSide->grBrushFace;

		DFace->Contents = Face->Contents;

		// Point them to each other
		Face->DrawFace = DFace;
		DFace->TopSideFlags = Face->TopSideFlags;

		DFace->Poly = grIndexPoly_Create(Face->Poly->NumVerts);

		if (!DFace->Poly)
			return GR_FALSE;

		pVert = &Face->Poly->Verts[0];
		grExtBox_SetToPoint(&Box, pVert);

		for (v=0; v< DFace->Poly->NumVerts; v++, pVert++)
		{
			//DFace->Poly->Verts[v] = grVertArray_ShareVert(BSP->VertArray, pVert);
			DFace->Poly->Verts[v] = grVertArray_AddVert(BSP->VertArray, pVert);

			grExtBox_ExtendToEnclose(&Box, pVert);
		}

		DFace->Radius = grVec3d_DistanceBetween(&Box.Max, &Box.Min) * 0.5f;
		grVec3d_Add(&Box.Max, &Box.Min, &DFace->Center);
		grVec3d_Scale(&DFace->Center, 0.5f, &DFace->Center);
		
		if (grPlaneArray_IndexSided(Face->PlaneIndex))
			DFace->NodeSide = 1;
		else
			DFace->NodeSide = 0;
						   
		// Assign faceinfo index
		if (!grBSPNode_DrawFaceSetFaceInfoIndex(DFace, BSP, FaceInfoIndex))
			return GR_FALSE;

		// Assign PlaneIndex
		DFace->PlaneIndex = Face->PlaneIndex;
		// Ref plane index so we can use it outside of this scope safely
		if (!grPlaneArray_RefPlaneByIndex(BSP->PlaneArray, DFace->PlaneIndex))
			return GR_FALSE;

		// Get the TexVec index
		DFace->TexVecIndex = TopSide->TexVecIndex;
		// Ref the TexVec index, since we will carry it out of this scope
		if (!grTexVec_ArrayRefTexVecByIndex(BSP->TexVecArray, DFace->TexVecIndex))
			return GR_FALSE;

		// Create the texture uv's
		if (!grBSPNode_DrawFaceCreateUVInfo(DFace, BSP))
			return GR_FALSE;

		// Set up the faces portal
		if (pFaceInfo->PortalCamera)
		{
			DFace->PortalObject = pFaceInfo->PortalCamera;
			
			grObject_CreateRef(DFace->PortalObject);
			
			DFace->PortalXForm = GR_RAM_ALLOCATE_STRUCT(grXForm3d);

			if (!DFace->PortalXForm)
				return GR_FALSE;

			BuildDrawFaceXForm(DFace, BSP);
		}

		Node->NumDrawFaces++;
		DFace++;
	}

	assert(Node->NumDrawFaces == NumFaces);

	NumDrawFaces += NumFaces;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeDrawFaces_r
//=======================================================================================
grBoolean grBSPNode_MakeDrawFaces_r(grBSPNode *Node, grBSP *BSP)
{
	assert(Node);

	if (Node->Flags & NODE_LEAF)
	{
		assert(Node->Leaf);
		return GR_TRUE;
	}

	assert(!Node->Leaf);

	if (!grBSPNode_MakeDrawFaces(Node, BSP))
	{
		grErrorLog_AddString(-1, "grBSPNode_MakeDrawFaces_r:  grBSPNode_MakeDrawFaces failed.", NULL);
		return GR_FALSE;
	}

	if (!grBSPNode_MakeDrawFaces_r(Node->Children[NODE_FRONT], BSP))
		return GR_FALSE;
	if (!grBSPNode_MakeDrawFaces_r(Node->Children[NODE_BACK], BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeDrawFaces_Callr
//=======================================================================================
grBoolean grBSPNode_MakeDrawFaces_Callr(grBSPNode *Node, grBSP *BSP)
{
	assert(Node);

	Log_Printf("--- grBSPNode_MakeDrawFaces_Callr --- \n");

	NumDrawFaces = 0;

	if (!grBSPNode_MakeDrawFaces_r(Node, BSP))
	{
		grErrorLog_AddString(-1, "grBSPNode_MakeDrawFaces_Callr:  grBSPNode_MakeDrawFaces_r failed.", NULL);
		return GR_FALSE;
	}

	Log_Printf("Num DrawFaces          : %5i\n", NumDrawFaces);

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_UpdateLeafSides_r
//=======================================================================================
grBoolean grBSPNode_UpdateLeafSides_r(grBSPNode *Node, grBSP *BSP)
{
	assert(grBSPNode_IsValid(Node));

	if (Node->Leaf)
	{
		if (Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		{
			if (!grBSPNode_LeafInitializeSides(Node->Leaf, BSP))
				return GR_FALSE;
		}

		return GR_TRUE;
	}

	if (!grBSPNode_UpdateLeafSides_r(Node->Children[NODE_FRONT], BSP))
		return GR_FALSE;
	if (!grBSPNode_UpdateLeafSides_r(Node->Children[NODE_BACK], BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_RemoveRecursionBit_r
//=======================================================================================
static void grBSPNode_RemoveRecursionBit_r(grBSPNode *Node, uint32 RecursionBit)
{
	int32		i;

	// Check recursion bit
	if (!(Node->RecursionBits & RecursionBit))
		return;

	g_WorldDebugInfo.NumNodes++;

	// Remove the recursion bit
	Node->RecursionBits ^= RecursionBit;

	if (Node->Leaf)		// At leaf, start recursing back up stack
	{
		g_WorldDebugInfo.NumLeaves++;
		return;
	}

	// Render polys on node
	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*DFace;

		DFace = Node->DrawFaces[i];

		// Remove recursion bit from face
		DFace->RecursionBits ^= RecursionBit;
	}

	grBSPNode_RemoveRecursionBit_r(Node->Children[NODE_FRONT], RecursionBit);
	grBSPNode_RemoveRecursionBit_r(Node->Children[NODE_BACK], RecursionBit);
}

//=======================================================================================
//	grBSPNode_RenderFrontToBack_r
//		Traverses down the side the camera is on to the opposite side the camera is on
//=======================================================================================
void grBSPNode_RenderFrontToBack_r(grBSPNode *Node, grBSP *BSP, grBSPNode_SceneInfo *SceneInfo, uint32 ClipFlags)
{
	grFloat			Dist;
	int32			i, Side;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);

	// Check recursion bit
	if (SceneInfo->RecursionBit)
	{
		if (!(Node->RecursionBits & SceneInfo->RecursionBit))
		{
			return;
		}
	}

	// Remove the recursion bit
	Node->RecursionBits ^= SceneInfo->RecursionBit;

	g_WorldDebugInfo.NumNodes++;

	if (Node->Leaf)		// At leaf, start recursing back up stack
	{
		g_WorldDebugInfo.NumLeaves++;
		return;
	}

#if 1
	if (ClipFlags)
	{
		grFloat		*MinMaxs;
		int32		*Index, p;
		grFloat		Dist2;
		grVec3d		Pnt;
		grPlane		*pPlane;
		grFrustum	*Frustum;

		Frustum = SceneInfo->Frustum;

		MinMaxs = (float*)&Node->Box.Min;

		for (pPlane = Frustum->Planes, p=0; p< Frustum->NumPlanes; p++, pPlane++)
		{
			if (!(ClipFlags & (1<<p)) )
				continue;

			Index = Frustum->pFrustumBBoxIndexes[p];

			Pnt.X = MinMaxs[Index[0]];
			Pnt.Y = MinMaxs[Index[1]];
			Pnt.Z = MinMaxs[Index[2]];
			
			Dist2 = grVec3d_DotProduct(&Pnt, &pPlane->Normal);
			Dist2 -= pPlane->Dist;

			if (Dist2 <= 0)
			{
				// We have no more visible nodes from this POV, 
				//	so just traverse to leafs to remove the recursion bit
				if (SceneInfo->RecursionBit)
				{
					grBSPNode_RemoveRecursionBit_r(Node->Children[NODE_FRONT], SceneInfo->RecursionBit);
					grBSPNode_RemoveRecursionBit_r(Node->Children[NODE_BACK], SceneInfo->RecursionBit);
				}

				return;
			}

			Pnt.X = MinMaxs[Index[3+0]];
			Pnt.Y = MinMaxs[Index[3+1]];
			Pnt.Z = MinMaxs[Index[3+2]];

			Dist2 = grVec3d_DotProduct(&Pnt, &pPlane->Normal);
			Dist2 -= pPlane->Dist;

			if (Dist2 >= 0)		
				ClipFlags ^= (1<<p);		// Don't need to clip to this plane anymore
		}
	}			
#endif

	// Get the distance that the eye is from this plane
	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	Dist = grPlane_PointDistanceFast(pPlane, &SceneInfo->POV);

	if (Dist < 0)		
		Side = 1;	// On the back side, go BACK->FRONT
	else
		Side = 0;	// On the front side, go FRONT->BACK

	grBSPNode_RenderFrontToBack_r(Node->Children[Side], BSP, SceneInfo, ClipFlags);

	// Render polys on node
	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grDeviceCaps devcaps;
		grBSPNode_DrawFace	*DFace;

		DFace = Node->DrawFaces[i];

		if (!(DFace->RecursionBits & SceneInfo->RecursionBit) && SceneInfo->RecursionBit)
			continue;

		// Remove recursion bit from face
		DFace->RecursionBits ^= SceneInfo->RecursionBit;

		if (DFace->NodeSide != Side)
			continue;		// Backfaced from node dir

		BSP->Driver->GetDeviceCaps(&devcaps);
		if ((devcaps.SuggestedDefaultRenderFlags & GR_RENDER_FLAG_HWTRANSFORM) == 0) {
			grBSPNode_DrawFaceRender(DFace, BSP, SceneInfo, ClipFlags);
		} else {
			grBSPNode_DrawFaceRenderPortal(DFace, BSP, SceneInfo, ClipFlags);
		}
		NumMakeFaces++;
	}

	grBSPNode_RenderFrontToBack_r(Node->Children[!Side], BSP, SceneInfo, ClipFlags);
}

//=======================================================================================
//	grBSPNode_RemoveTopBrush_r
//=======================================================================================
grBoolean grBSPNode_RemoveTopBrush_r(grBSPNode *Node, grBSP *BSP, grBSP_TopBrush *TopBrush)
{
	grBSP_Brush		*BSPBrush, *Next, *NewBrushes;
	grBoolean		Rebuild;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);

	if (!(Node->Flags & NODE_LEAF))		// Recurse to leafs
	{
		if (!grBSPNode_RemoveTopBrush_r(Node->Children[NODE_FRONT], BSP, TopBrush))
			return GR_FALSE;
		if (!grBSPNode_RemoveTopBrush_r(Node->Children[NODE_BACK], BSP, TopBrush))
			return GR_FALSE;

		return GR_TRUE;
	}

	grBSPNode_LeafDestroyDrawFaceList(Node->Leaf, BSP);

	// Find the brushes that point to the top brush, and remove them
	NewBrushes = NULL;
	Rebuild = GR_FALSE;

	for (BSPBrush = Node->Leaf->Brushes; BSPBrush; BSPBrush = Next)
	{
		Next = BSPBrush->Next;

		if (BSPBrush->Original == TopBrush)
		{
			Rebuild = GR_TRUE;
			grBSP_BrushDestroy(&BSPBrush);
			continue;
		}

		BSPBrush->Next = NewBrushes;
		NewBrushes = BSPBrush;
	}

	Node->Leaf->Brushes = NewBrushes;

	if (Rebuild)		// If we removed any brushes, rebuild the contents in this leaf, and call for an update on faces
	{
		grBSPNode_Portal	*p;
		int32				Side;

		// Rebuild the contents for this node...
		grBSPNode_BuildContents(Node, BSP);

		// Go through all the portals that look into this leaf, and remove their polys
		// from the nodes they were created on
		for (p = Node->Portals; p; p = p->Next[Side])
		{
			Side = (p->Nodes[1] == Node);

			assert(!(p->Nodes[0] == Node && p->Nodes[1] == Node));
			assert(p->Nodes[0] == Node || p->Nodes[1] == Node);

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

				// Force an update on the node that the portal was on
				p->OnNode->Flags |= NODE_REBUILD_FACES;
			}

			grBSPNode_PortalResetTopSide(p);
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_RebuildFaces_r
//=======================================================================================
void grBSPNode_RebuildFaces_r(grBSPNode *Node, grBSP *BSP)
{
	grBSPNode_Portal	*p;
	int32				Side;

	assert(Node);

	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);
		
		grBSPNode_RebuildFaces_r(Node->Children[NODE_FRONT], BSP);
		grBSPNode_RebuildFaces_r(Node->Children[NODE_BACK], BSP);

		// Destroy any draw faces on node
		grBSPNode_DestroyDrawFaces(Node, BSP);
	}
	else
	{
		assert(Node->Leaf);
		assert(Node->Leaf->Node);
		assert(Node->Leaf->Node == Node);
		assert(!Node->DrawFaces);
		assert(!Node->NumDrawFaces);
		
		// Reset portal sides
		for (p=Node->Portals; p; p = p->Next[Side])
		{
			Side = (p->Nodes[1] == Node);

			assert(!(p->Nodes[0] == Node && p->Nodes[1] == Node));
			assert(p->Nodes[0] == Node || p->Nodes[1] == Node);

			grBSPNode_PortalResetTopSide(p);

			p->Face[0] = NULL;
			p->Face[1] = NULL;
		}
	}

	assert(!Node->DrawFaces);
	assert(!Node->NumDrawFaces);

	// Free all the faces
	grBSPNode_DestroyBSPFaces(Node, BSP, GR_FALSE);
}

//=======================================================================================
//	grBSPNode_SplitBrushList
//=======================================================================================
grBoolean grBSPNode_SplitBrushList(grBSPNode *Node, grBSP *BSP, grBSP_Brush *Brushes, grBSP_Brush **Front, grBSP_Brush **Back)
{
	grBSP_Brush			*Brush, *NewBrush, *NewBrush2, *Next;
	grBSP_Side			*Side;
	grPlane_Side		Sides;
	int32				i;
	
	*Front = *Back = NULL;

	for (Brush = Brushes ; Brush ; Brush = Next)
	{
		Next = Brush->Next;

		Sides = Brush->Side;

		if (Sides == PSIDE_BOTH)
		{	
			// Split into two brushes
			if (!grBSP_BrushSplit(Brush, BSP, Node->PlaneIndex, SIDE_NODE, &NewBrush, &NewBrush2))
				return GR_FALSE;

			if (NewBrush)
			{
				NewBrush->Next = *Front;
				*Front = NewBrush;
			}
			if (NewBrush2)
			{
				NewBrush2->Next = *Back;
				*Back = NewBrush2;
			}

			BSP->DebugInfo.NumSplits++;

			continue;
		}
		
		// Copy the brush
		NewBrush = grBSP_BrushCreateFromBSPBrush(Brush);

		// If the planenum is actualy a part of the brush
		// find the plane and flag it as used so it won't be tried
		// as a splitter again
		if (Sides & PSIDE_FACING)
		{
			for (Side = NewBrush->Sides, i=0 ; i<NewBrush->NumSides ; i++, Side++)
			{
				if (grPlaneArray_IndexIsCoplanar(Side->PlaneIndex, Node->PlaneIndex))
					Side->Flags |= SIDE_NODE;
			}
		}

		if (Sides & PSIDE_FRONT)
		{
			NewBrush->Next = *Front;
			*Front = NewBrush;
			continue;
		}
		if (Sides & PSIDE_BACK)
		{
			NewBrush->Next = *Back;
			*Back = NewBrush;
			continue;
		}
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeAreas_r
//=======================================================================================
grBoolean grBSPNode_MakeAreas_r(grBSPNode *Node, grBSP *BSP, grChain *AreaChain)
{
	assert(Node);

	// Recurse to leafs
	while (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);

		if (!grBSPNode_MakeAreas_r(Node->Children[0], BSP, AreaChain))
			return GR_FALSE;

		Node = Node->Children[1];
	}
	
	assert(Node->Leaf);

	// Find all VIS_PORTALS
	if (!grBSPNode_LeafMakeAreas(Node->Leaf, BSP, AreaChain))
		return GR_FALSE;

	return GR_TRUE;
}

#ifdef AREA_DRAWFACE_TEST
//=======================================================================================
//	grBSPNode_MakeLeafList_r
//=======================================================================================
grBoolean grBSPNode_MakeLeafList_r(grBSPNode *Node,LinkNode *pList)
{
	assert(Node);

	// Recurse to leafs
	while (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);

		if (!grBSPNode_MakeLeafList_r(Node->Children[0],pList))
			return GR_FALSE;

		Node = Node->Children[1];
	}
	
	assert(Node->Leaf);

	assert(LN_ListLen(&(Node->Leaf->LN))==0);
	LN_AddTail(pList,Node->Leaf);

	return GR_TRUE;
}
#endif

//=======================================================================================
//	grBSPNode_CountAreaVisPortals_r
//=======================================================================================
grBoolean grBSPNode_CountAreaVisPortals_r(grBSPNode *Node)
{
	assert(Node);
	
	// recurse to leafs
	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);
		if (!grBSPNode_CountAreaVisPortals_r(Node->Children[NODE_FRONT]))
			return GR_FALSE;
		if (!grBSPNode_CountAreaVisPortals_r(Node->Children[NODE_BACK]))
			return GR_FALSE;

		return GR_TRUE;
	}
	
	assert(Node->Leaf);

	if (!grBSPNode_LeafCountAreaVisPortals(Node->Leaf))
		return GR_FALSE;
	
	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_MakeAreaVisPortals_r
//=======================================================================================
grBoolean grBSPNode_MakeAreaVisPortals_r(grBSPNode *Node, grBSP *BSP)
{
	assert(Node);
	
	// recurse to leafs
	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);
		if (!grBSPNode_MakeAreaVisPortals_r(Node->Children[NODE_FRONT], BSP))
			return GR_FALSE;
		if (!grBSPNode_MakeAreaVisPortals_r(Node->Children[NODE_BACK], BSP))
			return GR_FALSE;

		return GR_TRUE;
	}
	
	assert(Node->Leaf);

	if (!grBSPNode_LeafMakeAreaVisPortals(Node->Leaf, BSP))
		return GR_FALSE;
	
	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_DestroyAreas_r
//=======================================================================================
grBoolean grBSPNode_DestroyAreas_r(grBSPNode *Node)
{
	assert(Node);

	// Recurse to leafs
	if (!(Node->Flags & NODE_LEAF))
	{
		assert(!Node->Leaf);

		if (!grBSPNode_DestroyAreas_r(Node->Children[NODE_FRONT]))
			return GR_FALSE;
		if (!grBSPNode_DestroyAreas_r(Node->Children[NODE_BACK]))
			return GR_FALSE;

		return GR_TRUE;
	}
	
	assert(Node->Leaf);

	if (Node->Leaf->Area)
		grBSPNode_AreaDestroy(&Node->Leaf->Area);

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_DoAllAreasInRadius
//=====================================================================================
void grBSPNode_DoAllAreasInRadius(grBSPNode *Node, grBSP *BSP, grVec3d *Pos,grFloat Radius,
												grBSP_DoAreaFunc CB,void * Context)
{
	const grPlane	*Plane;
	grFloat			Dist;

	assert(Node);
	assert(CB);
	assert(Pos);

	while(!(Node->Flags & NODE_LEAF))		// Go to leafs
	{
		assert(!Node->Leaf);

		Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
		
		Dist = grVec3d_DotProduct(Pos, &(Plane->Normal)) - Plane->Dist;

		if (Dist > Radius)
		{
			Node = Node->Children[0];
		}
		else if ( Dist < (- Radius) )
		{
			Node = Node->Children[1];
		}
		else
		{
			// must go down both

			grBSPNode_DoAllAreasInRadius(Node->Children[0], BSP, Pos,Radius,CB,Context);
												
			Node = Node->Children[1];
		}

		assert(Node);
	}

	assert(Node->Leaf);

	if ( Node->Leaf->Area )
	{
		CB(Node->Leaf->Area,Context);
	}
}

//=====================================================================================
//	grBSPNode_DoAllAreasInBox
//=====================================================================================
grBoolean grBSPNode_DoAllAreasInBox(grBSPNode *Node, grBSP *BSP, grExtBox *BBox,grBSP_DoAreaFunc CB,void * Context)
{
	grFloat R;
	grVec3d Center;

	R = grVec3d_DistanceBetween(&(BBox->Max),&(BBox->Min)) * 0.5f;
	grVec3d_Add(&(BBox->Max),&(BBox->Min),&Center);
	grVec3d_Scale(&Center,0.5f,&Center);

	// <> approximate, fast !
   	grBSPNode_DoAllAreasInRadius(Node,BSP,&Center,R,CB,Context);

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_FindArea
//=====================================================================================
grBSPNode_Area *grBSPNode_FindArea(grBSPNode *Node, grBSP *BSP, const grVec3d *Pos)
{

	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Pos);

	while(!(Node->Flags & NODE_LEAF))		// Go to leafs
	{
		const grPlane	*Plane;
		grFloat			Dist;

		assert(!Node->Leaf);

		Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
		
		Dist = grPlane_PointDistanceFast(Plane, Pos);	// We can use fast check, since node planes face positive

		if (Dist > 0)
			Node = Node->Children[0];
		else
			Node = Node->Children[1];

		assert(Node);
	}

	assert(Node->Leaf);

	return Node->Leaf->Area;
}

// not really the closest; it's the first non-solid leaf we
//	can find, walking backwards up the tree
grBSPNode_Area *grBSPNode_FindClosestArea(grBSPNode *Node, grBSP *BSP, const grVec3d *Pos)
{
	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Pos);

	while(!(Node->Flags & NODE_LEAF))		// Go to leafs
	{
		const grPlane	*Plane;
		grFloat			Dist;
		grBSPNode_Area	*Area;
		int32			Side;

		assert(!Node->Leaf);

		Plane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
		
		Dist = grPlane_PointDistanceFast(Plane, Pos);	// We can use fast check, since node planes face positive

		Side = (Dist > 0) ? 0 : 1;

		Area = grBSPNode_FindClosestArea(Node->Children[Side], BSP, Pos);

		if ( Area )
			return Area;

		Node = Node->Children[!Side];

		assert(Node);
	}

	return Node->Leaf->Area;
}

//====================================================================================
//	grBSPNode_RayIntersects_r
//====================================================================================
#define NODE_CLIPPLANE_EPSILON 0.0001f  // DarkRift/Incarnadine
grBoolean grBSPNode_RayIntersects_r(const grBSPNode *Node, grBSP *BSP, const grVec3d *Front, const grVec3d *Back)
{
    grFloat			Fd, Bd, Dist;
    int32			Side;
    grVec3d			I;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);

	if (Node->Leaf)
	{
		if (Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID)
			return GR_TRUE;						// Ray collided with solid space
		else 
			return GR_FALSE;					// Ray collided with empty space
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);

    Fd = grPlane_PointDistanceFast(pPlane, Front);
    Bd = grPlane_PointDistanceFast(pPlane, Back);

    //if (Fd >= -1.0f && Bd >= -1.0f) 
	if (Fd >= -NODE_CLIPPLANE_EPSILON && Bd >= -NODE_CLIPPLANE_EPSILON) //DarkRift/Incarnadine
        return(grBSPNode_RayIntersects_r(Node->Children[NODE_FRONT], BSP, Front, Back));
    //if (Fd < 1.0f && Bd < 1.0f) 
	if (Fd < NODE_CLIPPLANE_EPSILON && Bd < NODE_CLIPPLANE_EPSILON) //DarkRift/Incarnadine
        return(grBSPNode_RayIntersects_r(Node->Children[NODE_BACK], BSP, Front, Back));

    Side = (Fd < 0);
    Dist = Fd / (Fd - Bd);

    I.X = Front->X + Dist * (Back->X - Front->X);
    I.Y = Front->Y + Dist * (Back->Y - Front->Y);
    I.Z = Front->Z + Dist * (Back->Z - Front->Z);

	if (grBSPNode_RayIntersects_r(Node->Children[Side], BSP, Front, &I))
        return GR_TRUE;
    else if (grBSPNode_RayIntersects_r(Node->Children[!Side], BSP, &I, Back))
		return GR_TRUE;

	return GR_FALSE;
}

//====================================================================================
//	grBSPNode_CollisionExact_r
//====================================================================================
grBoolean grBSPNode_CollisionExact_r(const grBSPNode *Node, grBSP *BSP, const grVec3d *Front, const grVec3d *Back, grBSPNode_CollisionInfo *Info)
{
    grFloat			Fd, Bd, Dist;
    int32			Side;
    grVec3d			I;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);

	if (Node->Leaf)
	{
		if (Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID )
			return GR_TRUE;						// Ray collided with solid space
		else 
			return GR_FALSE;					// Ray collided with empty space
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);

    Fd = grPlane_PointDistanceFast(pPlane, Front);

    Bd = grPlane_PointDistanceFast(pPlane, Back);

    //if (Fd >= -1.0f && Bd >= -1.0f)
	if (Fd >= -NODE_CLIPPLANE_EPSILON && Bd >= -NODE_CLIPPLANE_EPSILON) //DarkRift/Incarnadine
        return(grBSPNode_CollisionExact_r(Node->Children[NODE_FRONT], BSP, Front, Back, Info));
    //if (Fd < 1.0f && Bd < 1.0f)
	if (Fd < NODE_CLIPPLANE_EPSILON && Bd < NODE_CLIPPLANE_EPSILON) //DarkRift/Incarnadine
        return(grBSPNode_CollisionExact_r(Node->Children[NODE_BACK], BSP, Front, Back, Info));

    Side = (Fd < 0);
    Dist = Fd / (Fd - Bd);

    I.X = Front->X + Dist * (Back->X - Front->X);
    I.Y = Front->Y + Dist * (Back->Y - Front->Y);
    I.Z = Front->Z + Dist * (Back->Z - Front->Z);

	if (grBSPNode_CollisionExact_r(Node->Children[Side], BSP, Front, &I, Info))
	{
		if (Info && !Info->HitSet) // Icestorm
		{
			*(Info->Plane) = *pPlane;
			
			if (Side)
				grPlane_Inverse(Info->Plane);

			*(Info->Impact) = I;
			Info->HitSet = GR_TRUE;
		}
		TotalNumberCollisions++;
        return GR_TRUE;
	}
    else if (grBSPNode_CollisionExact_r(Node->Children[!Side], BSP, &I, Back, Info))
	{
		if (Info && !Info->HitSet) // Icestorm
		{
			*(Info->Plane) = *pPlane;
			
			if (Side)
				grPlane_Inverse(Info->Plane);

			*(Info->Impact) = I;
			Info->HitSet = GR_TRUE;
		}
		TotalNumberCollisions++;
		return GR_TRUE;
	}

	return GR_FALSE;
}

//=====================================================================================
//	grBSPNode_CollisionBBox_r
//=====================================================================================
grBoolean grBSPNode_CollisionBBox_r(const grBSPNode *Node, grBSP *BSP, const grExtBox *Box1, const grExtBox *Box2, const grVec3d *Front, const grVec3d *Back, grBSPNode_CollisionInfo2 *Info)
{
	grPlane_Side	Side;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node));

	if (Node->Leaf)
	{
		grBSPNode_CollisionInfo2 DummyInfo;
		if (!(Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID))
			return GR_FALSE;
		
		if (!Node->Leaf->NumSides)
			return GR_FALSE;

		if (!Info) // Icestorm: Ignore precise details
		{
			Info=&DummyInfo;
			Info->HitSet = GR_FALSE;
			Info->HitLeaf = GR_TRUE;	
		} else
			Info->HitLeaf = GR_FALSE;

		grBSPNode_LeafCollision_r(Node->Leaf, BSP, 0, 1, Box2, Front, Back, Info);

		return (Info->HitSet);
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	assert(pPlane);

	Side = grPlane_BoxSide(pPlane, Box1, 0.01f);

	if (Info)
	{
		// Go down the sides that the box lands in
		if (Side & PSIDE_FRONT)
			grBSPNode_CollisionBBox_r(Node->Children[NODE_FRONT], BSP, Box1, Box2, Front, Back, Info);
		if (Side & PSIDE_BACK)
			grBSPNode_CollisionBBox_r(Node->Children[NODE_BACK], BSP, Box1, Box2, Front, Back, Info);
		return (Info->HitSet);
	} else
	{
		// Go down the sides that the box lands in
		if (Side & PSIDE_FRONT)
			if (grBSPNode_CollisionBBox_r(Node->Children[NODE_FRONT], BSP, Box1, Box2, Front, Back, Info))
				return GR_TRUE;
		if (Side & PSIDE_BACK)
			if (grBSPNode_CollisionBBox_r(Node->Children[NODE_BACK], BSP, Box1, Box2, Front, Back, Info))
				return GR_TRUE;
		return GR_FALSE;
	}
}

// Added by Icestorm
//=====================================================================================
//	grBSPNode_ChangeBoxCollisionBBox_r
//=====================================================================================
grBoolean grBSPNode_ChangeBoxCollisionBBox_r(const grBSPNode *Node, grBSP *BSP, const grExtBox *Box1, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grBSPNode_CollisionInfo3 *Info)
{
	grPlane_Side	Side;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node));

	if (Node->Leaf)
	{
		grBSPNode_CollisionInfo3 DummyInfo;
		if (!(Node->Leaf->Contents & GR_BSP_CONTENTS_SOLID))
			return GR_FALSE;
		
		if (!Node->Leaf->NumSides)
			return GR_FALSE;

		if (!Info) // Icestorm: Ignore precise details
		{
			Info=&DummyInfo;
			Info->HitLeaf = GR_TRUE;	
			Info->HitSet  = GR_FALSE;
		} else
			Info->HitLeaf = GR_FALSE;

		grBSPNode_LeafChangeBoxCollision_r(Node->Leaf, BSP, 0, 1, Pos, FrontBox, BackBox, Info);

		return (Info->HitSet);
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	assert(pPlane);

	Side = grPlane_BoxSide(pPlane, Box1, 0.01f);

	if (Info)
	{
		// Go down the sides that the box lands in
		if (Side & PSIDE_FRONT)
			grBSPNode_ChangeBoxCollisionBBox_r(Node->Children[NODE_FRONT], BSP, Box1, Pos, FrontBox, BackBox, Info);
		if (Side & PSIDE_BACK)
			grBSPNode_ChangeBoxCollisionBBox_r(Node->Children[NODE_BACK], BSP, Box1, Pos, FrontBox, BackBox, Info);
		return (Info->HitSet);
	} else
	{
		// Go down the sides that the box lands in
		if (Side & PSIDE_FRONT)
			if (grBSPNode_ChangeBoxCollisionBBox_r(Node->Children[NODE_FRONT], BSP, Box1, Pos, FrontBox, BackBox, Info))
				return GR_TRUE;
		if (Side & PSIDE_BACK)
			if (grBSPNode_ChangeBoxCollisionBBox_r(Node->Children[NODE_BACK], BSP, Box1, Pos, FrontBox, BackBox, Info))
				return GR_TRUE;
		return GR_FALSE;
	}
}

//=======================================================================================
//	grBSPNode_MakeDrawFaceListOnLeafs_r
//=======================================================================================
grBoolean grBSPNode_MakeDrawFaceListOnLeafs_r(grBSPNode *Node, grBSP *BSP)
{
	if (!(Node->Flags & NODE_LEAF))		// Recurse to leafs
	{
		if (!grBSPNode_MakeDrawFaceListOnLeafs_r(Node->Children[NODE_FRONT], BSP))
			return GR_FALSE;
		if (!grBSPNode_MakeDrawFaceListOnLeafs_r(Node->Children[NODE_BACK], BSP))
			return GR_FALSE;

		return GR_TRUE;
	}

	if (!grBSPNode_LeafMakeDrawFaceList(Node->Leaf, BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_VisibleContents
//=====================================================================================
uint32 grBSPNode_VisibleContents(uint32 Contents)
{
	int32		i;

	Contents &= GR_BSP_VISIBLE_CONTENTS;

	if (!Contents)
		return 0;		// Early out

	for (i=0; i<32; i++)
	{
		uint32	Bit = (1<<i);

		if (Contents & Bit)
			return Bit;
	}

	return 0;		// No visible contents
}

//=====================================================================================
//	grBSPNode_GetTopSideSeperatingLeafs
//=====================================================================================
void grBSPNode_GetTopSideSeperatingLeafs(grBSP *BSP, const grBSPNode_Leaf *Leaf1, const grBSPNode_Leaf *Leaf2, grPlaneArray_Index PlaneIndex, grBSP_TopBrush **DstTopBrush, grBSP_TopSide **DstTopSide)
{
	uint32					Contents, MajorContents, BestOrder;
	grBSP_Brush				*Brush;
	int32					i,j;
	grBSP_TopSide			*Side, *BestSide, *ExactSide;
	grBSP_TopBrush			*ExactBrush, *BestBrush;
	grFloat					Dot, BestDot;
	const grPlane			*p1;
	grBrush_Contents		c1, c2;
	const grBSPNode_Leaf	*Leafs[2];

	assert(Leaf1);
	assert(Leaf2);
	assert(DstTopBrush);
	assert(DstTopSide);

	*DstTopBrush = NULL;
	*DstTopSide = NULL;

	// Portal only visible, if it is seperating different contents (or either is sheet)
	c1 = Leaf1->Contents;
	c2 = Leaf2->Contents;

	Contents = c1^c2;

	if ((c1&c2)&GR_BSP_CONTENTS_SHEET)
		Contents |= GR_BSP_CONTENTS_SHEET;	// If sheet was on both side, put it back in

	// There must be a visible contents on at least one side of the portal...
	//MajorContents = grBSPNode_VisibleContents(Contents);
	MajorContents = Contents & GR_BSP_VISIBLE_CONTENTS;

	if (MajorContents & (GR_BSP_CONTENTS_SOLID | GR_BSP_CONTENTS_AIR))
		MajorContents &= ~GR_BSP_CONTENTS_EMPTY;	// Solid/Cut overides empty when picking textures

	if (!MajorContents)		
		return;				// Portal doea not seperate visible contents

	p1 = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, PlaneIndex);

	BestOrder = 0;
	ExactSide = BestSide = NULL;
	ExactBrush = BestBrush = NULL;
	BestDot = 0.0f;

	Leafs[0] = Leaf1;
	Leafs[1] = Leaf2;

	for (j=0 ; j<2 ; j++)
	{
		const grBSPNode_Leaf	*Leaf;

		Leaf = Leafs[j];

		// First try and find an exact match
		for (Brush= Leaf->Brushes ; Brush; Brush=Brush->Next)
		{
			grBSP_TopBrush		*TopBrush;

			TopBrush = Brush->Original;

			// Only use the brush that contains a major contents (solid)
			if (!(TopBrush->Contents & MajorContents))
				continue;

			if (TopBrush->Order < BestOrder)
				continue;

		#if 0
			for (i=0 ; i< Brush->NumSides ; i++)
			{
				Side = &Brush->Sides[i];
			
				if (!(Side->Flags & SIDE_SPLIT))
					break;
			}

			if (i == Brush->NumSides)
				continue;		// Brush was split on all sides, just a contents changer
		#endif

			for (i=0 ; i<TopBrush->NumSides ; i++)
			{
				const grPlane		*p2;

				Side = &TopBrush->TopSides[i];
			
				if (Side->Flags & SIDE_NODE)
					continue;		

				if (Side->Flags & SIDE_SPLIT)
					continue;

				if (grPlaneArray_IndexIsCoplanar(Side->PlaneIndex,PlaneIndex))
				{	
					// Exact match
					ExactSide = &TopBrush->TopSides[i];
					ExactBrush = TopBrush;
					BestOrder = TopBrush->Order;
				}
				
				if (!(Contents & GR_BSP_CONTENTS_SHEET) && !ExactSide)
				{
					// Until we find an exact match, 
					// keep looking for the closest match just in case
					p2 = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Side->PlaneIndex);
					Dot = grVec3d_DotProduct(&p1->Normal, &p2->Normal);

					if (Dot > BestDot)
					{
						BestDot = Dot;
						BestSide = Side;
						BestOrder = TopBrush->Order;
						BestBrush = TopBrush;
					}
				}
			}
		}
	}
	
	if (ExactSide)
	{
		// ExactSide/Brush overides BestSide
		*DstTopBrush = ExactBrush;
		*DstTopSide = ExactSide;
	}
	else
	{
		// If no exact, just take the best
		*DstTopBrush = BestBrush;
		*DstTopSide = BestSide;
	}
}

#define NODESTACK_MAX_NODES		1024

typedef struct
{
	int32			NumNodes;
	const grBSPNode	*Nodes[NODESTACK_MAX_NODES];
} NodeStack;

//====================================================================================
//	grBSPNode_FillNodeStack_r
//	Fills a node stack using the segment from front to back of segment
//====================================================================================
static grBoolean grBSPNode_FillNodeStack_r(const grBSPNode *Node, grBSP *BSP, const grVec3d *Front, const grVec3d *Back, NodeStack *Stack)
{
    grFloat			Fd, Bd, Dist;
    int32			Side;
    grVec3d			I;
	const grPlane	*pPlane;

	assert(grBSPNode_IsValid(Node) == GR_TRUE);
	assert(Front);
	assert(Back);

	if (Node->Leaf)
	{
		if (Stack->NumNodes >= NODESTACK_MAX_NODES)
			return GR_FALSE;		// Oh well...

		Stack->Nodes[Stack->NumNodes++] = Node;

		return GR_TRUE;
	}

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);

    Fd = grPlane_PointDistanceFast(pPlane, Front);
    Bd = grPlane_PointDistanceFast(pPlane, Back);

    if (Fd >= 0.0f && Bd >= 0.0f) 
        return(grBSPNode_FillNodeStack_r(Node->Children[NODE_FRONT], BSP, Front, Back, Stack));
    if (Fd <= 0.0f && Bd <= 0.0f)
        return(grBSPNode_FillNodeStack_r(Node->Children[NODE_BACK], BSP, Front, Back, Stack));

    // The segment is split
	Side = (Fd < 0);
    Dist = Fd / (Fd - Bd);

    I.X = Front->X + Dist * (Back->X - Front->X);
    I.Y = Front->Y + Dist * (Back->Y - Front->Y);
    I.Z = Front->Z + Dist * (Back->Z - Front->Z);

	// Traverse form front of segment to back
	if (!grBSPNode_FillNodeStack_r(Node->Children[Side], BSP, Front, &I, Stack))
		return GR_FALSE;

	if (Stack->NumNodes >= NODESTACK_MAX_NODES)
		return GR_FALSE;		// Oh well...

	Stack->Nodes[Stack->NumNodes++] = Node;

	if (!grBSPNode_FillNodeStack_r(Node->Children[!Side], BSP, &I, Back, Stack))
		return GR_FALSE;

	return GR_TRUE;
}

//====================================================================================
//	grBSPNode_RayIntersectsBrushes
//====================================================================================
grBoolean grBSPNode_RayIntersectsBrushes(const grBSPNode *Node, grBSP *BSP, const grVec3d *Front, const grVec3d *Back, grBrushRayInfo *Info)
{
	NodeStack		Stack;
	int32			i;

	assert(Node);
	assert(Front);
	assert(Back);
	assert(Info);

	ZeroMem(Info);

	Stack.NumNodes = 0;

	if (!grBSPNode_FillNodeStack_r(Node, BSP, Front, Back, &Stack))
		return GR_FALSE;		// Node stack must have filled up, just act like there was no collision

	for (i=0; i< Stack.NumNodes-2; i += 2)
	{
		const grBSPNode		*Node1, *Node2, *Node3;

		Node1 = Stack.Nodes[i+0];
		Node2 = Stack.Nodes[i+1];
		Node3 = Stack.Nodes[i+2];

		assert(Node1->Leaf);			// The left leaf
		assert(!Node2->Leaf);
		assert(Node3->Leaf);

		if (!(Node1->Leaf->Contents & GR_BSP_CONTENTS_SOLID))
		{
			grBSP_TopBrush		*TopBrush;
			grBSP_TopSide		*TopSide;

			grBSPNode_GetTopSideSeperatingLeafs(BSP, Node1->Leaf, Node3->Leaf, Node2->PlaneIndex, &TopBrush, &TopSide);

			if (TopBrush && TopSide)
			{
				grFloat			Fd, Bd, Ratio;

				Info->Brush = TopBrush->Original;
				Info->BrushFace = TopSide->grBrushFace;

				Info->Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node2->PlaneIndex);

				Fd = grPlane_PointDistanceFast(&Info->Plane, Front);
				Bd = grPlane_PointDistanceFast(&Info->Plane, Back);

				Ratio = Fd / (Fd - Bd);

				Info->Impact.X = Front->X + Ratio * (Back->X - Front->X);
				Info->Impact.Y = Front->Y + Ratio * (Back->Y - Front->Y);
				Info->Impact.Z = Front->Z + Ratio * (Back->Z - Front->Z);

				Info->c1 = Node1->Leaf->BrushContents;
				Info->c2 = Node3->Leaf->BrushContents;

				return GR_TRUE;
			}
		}
	}

	return GR_FALSE;
}

//====================================================================================
//	grBSPNode_SetDLight_r
//====================================================================================
grBoolean grBSPNode_SetDLight_r(grBSPNode *Node, grBSP *BSP, grBSPNode_Light *Light, uint32 LBit, uint32 DLightVisFrame)
{
	const grPlane	*pPlane;
	int32			i;
	grFloat			Dist;

	assert(grBSPNode_IsValid(Node));

	if (Node->Leaf)
		return GR_TRUE;			// At leaf no more recursing

	pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, Node->PlaneIndex);
	assert(pPlane);

	Dist = grPlane_PointDistanceFast(pPlane, &Light->Pos);

	if (Dist > Light->Radius)			// Only on front
		return grBSPNode_SetDLight_r(Node->Children[NODE_FRONT], BSP, Light, LBit, DLightVisFrame);
	else if (Dist < -Light->Radius)		// Only on back
		return grBSPNode_SetDLight_r(Node->Children[NODE_BACK], BSP, Light, LBit, DLightVisFrame);

	// Light is touching node, find faces that touch light on the node
	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*DFace;

		DFace = Node->DrawFaces[i];

		if (grVec3d_DistanceBetween(&DFace->Center, &Light->Pos) > Light->Radius + DFace->Radius)
			continue;		// Light is NOT in radius of face

		if (DFace->DLightVisFrame != DLightVisFrame)
		{
			// Reset the face if this is the first light to touch it this frame...
			DFace->DLights = 0;
			DFace->DLightVisFrame = DLightVisFrame;
		}

		// This face will be lit by the light, so mark it
		DFace->DLights |= LBit;
	}

	// Go down both
	if (!grBSPNode_SetDLight_r(Node->Children[NODE_FRONT], BSP, Light, LBit, DLightVisFrame))
		return GR_FALSE;

	if (!grBSPNode_SetDLight_r(Node->Children[NODE_BACK], BSP, Light, LBit, DLightVisFrame))
		return GR_FALSE;

	return GR_TRUE;
}

//====================================================================================
//	grBSPNode_CreateLightmapTHandles_r
//====================================================================================
grBoolean grBSPNode_CreateLightmapTHandles_r(grBSPNode *Node, DRV_Driver *Driver)
{
	int32		i;

	assert(grBSPNode_IsValid(Node));
	assert(Driver);

	if (Node->Leaf)
		return GR_TRUE;		// At leaf, start recursing back up

	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace		*pDFace;
		int32					Width, Height;
		grRDriver_PixelFormat	PixelFormat;
		grBSPNode_Lightmap		*Lightmap;

		pDFace = Node->DrawFaces[i];

		Lightmap = pDFace->Lightmap;

		if (!Lightmap)
			continue;

		assert(!Lightmap->THandle);

		// Create the lightmap THandle 
		Width = Lightmap->Width;
		Height = Lightmap->Height;

		// Modified by chrisjp.
		if(bestSupportedLightmapPixelFormat == 0)
			DetermineSupportedLightmapFormat(GR_PIXELFORMAT_16BIT_565_RGB , Driver);

		PixelFormat.Flags = RDRIVER_PF_LIGHTMAP;
		PixelFormat.PixelFormat = bestSupportedLightmapPixelFormat;

		Lightmap->THandle = Driver->THandle_Create(Width, Height, 1, &PixelFormat);

		if (!Lightmap->THandle)
			return GR_FALSE;
	}

	if (!grBSPNode_CreateLightmapTHandles_r(Node->Children[NODE_FRONT], Driver))
		return GR_FALSE;
	if (!grBSPNode_CreateLightmapTHandles_r(Node->Children[NODE_BACK], Driver))
		return GR_FALSE;

	return GR_TRUE;
}

//====================================================================================
//	grBSPNode_DestroyLightmapTHandles_r
//====================================================================================
grBoolean grBSPNode_DestroyLightmapTHandles_r(grBSPNode *Node, DRV_Driver *Driver)
{
	int32		i;

	assert(grBSPNode_IsValid(Node));
	assert(Driver);

	if (Node->Leaf)
		return GR_TRUE;		// At leaf, start recursing back up

	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace		*pDFace;
		grBSPNode_Lightmap		*Lightmap;

		pDFace = Node->DrawFaces[i];

		Lightmap = pDFace->Lightmap;

		if (!Lightmap)
			continue;

		assert(Lightmap->THandle);

		Driver->THandle_Destroy(Lightmap->THandle);
		Lightmap->THandle = NULL;
	}

	if (!grBSPNode_DestroyLightmapTHandles_r(Node->Children[NODE_FRONT], Driver))
		return GR_FALSE;
	if (!grBSPNode_DestroyLightmapTHandles_r(Node->Children[NODE_BACK], Driver))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_WeldDrawFaceVerts_r
//=======================================================================================
grBoolean grBSPNode_WeldDrawFaceVerts_r(grBSPNode *Node, grBSP *BSP, grVertArray_Optimizer *Optimizer)
{
	int32		i;

	assert(grBSPNode_IsValid(Node));

	if (Node->Leaf)
		return GR_TRUE;		// At leaf, return

	for (i=0; i< Node->NumDrawFaces; i++)
	{
		grBSPNode_DrawFace	*pDrawFace;
		grVertArray_Index	*pIVert;
		int32				v;

		pDrawFace = Node->DrawFaces[i];
		assert(pDrawFace);
		
		for (pIVert = pDrawFace->Poly->Verts, v=0; v< pDrawFace->Poly->NumVerts; v++, pIVert++)
		{
			(*pIVert) = grVertArray_GetOptimizedIndex(BSP->VertArray, Optimizer, (*pIVert));
			assert((*pIVert) != GR_VERTARRAY_NULL_INDEX);
		}
	}

	if (!grBSPNode_WeldDrawFaceVerts_r(Node->Children[NODE_FRONT], BSP, Optimizer))
		return GR_FALSE;

	if (!grBSPNode_WeldDrawFaceVerts_r(Node->Children[NODE_BACK], BSP, Optimizer))
		return GR_FALSE;

	return GR_TRUE;
}

//=====================================================================================
//	TJunct code (put most of this in grBSPNode_DrawFace???)
//=====================================================================================

// FIXME:	Dynamically allocate these arrays

#define OFF_EPSILON				(0.05f)
#define	MAX_TEMP_INDEX_VERTS	(1024)

static int32					NumTempIndexVerts;
static grVertArray_Index		TempIndexVerts[MAX_TEMP_INDEX_VERTS];

static int32					NumEdgeVerts;
static grVertArray_Index		EdgeVerts[GR_VERTARRAY_MAX_VERTS];

static const grVec3d			*pEdgeV1;
static const grVec3d			*pEdgeV2;
static grVec3d					EdgeDir;
static grBSP					*g_BSP;

//=====================================================================================
//	FinalizeFace
//=====================================================================================
static grBoolean FinalizeFace(grBSPNode_DrawFace *Face, int32 Base, grBSP *BSP)
{
	int32				i;
	grIndexPoly			*Poly;
	const grFaceInfo	*pFaceInfo;

	if (NumTempIndexVerts == Face->Poly->NumVerts)
		return GR_TRUE;			// No TJunctions were added, leave face alone...

	// Grab the faceinfo
	pFaceInfo = grFaceInfo_ArrayGetFaceInfoByIndex(BSP->FaceInfoArray, Face->FaceInfoIndex);
	assert(pFaceInfo);
	
	if (pFaceInfo->PortalCamera)
		return GR_TRUE;				// Don't fix tjuncts for faces that use portals, it just complicates things...

	// Create a new poly with the new number of vertices
	Poly = grIndexPoly_Create((grIndexPoly_NumVertType)NumTempIndexVerts);

	if (!Poly)
		return GR_FALSE;
		
	// Copy the new vertices over
	for (i=0; i< NumTempIndexVerts; i++)
	{
		// Assign the index to the new poly
		Poly->Verts[i] = TempIndexVerts[(i+Base)%NumTempIndexVerts];
		// Ref the index
		if (!grVertArray_RefVertByIndex(BSP->VertArray, Poly->Verts[i]))
			return GR_FALSE;
	}

	// De-ref the verts in the old poly
	for (i=0; i< Face->Poly->NumVerts; i++)
		grVertArray_RemoveVert(BSP->VertArray, &Face->Poly->Verts[i]);

	// Destroy the old poly on the face
	grIndexPoly_Destroy(&Face->Poly);

	// Assign the new poly (with fixed TJuncts)
	Face->Poly = Poly;

	// Create the texture uv's
	if (!grBSPNode_DrawFaceCreateUVInfo(Face, BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=====================================================================================
//	TestEdge_r
//=====================================================================================
static grBoolean TestEdge_r(grFloat Start, grFloat End, grVertArray_Index p1, grVertArray_Index p2, int32 StartVert)
{
	int32		k;

	if (p1 == p2)
		return GR_TRUE;			// Degenerate edge

	for (k=StartVert ; k<NumEdgeVerts ; k++)
	{
		const grVec3d		*pVert;
		grVertArray_Index	j;
		grFloat				Dist;
		grVec3d				Vert2;
		grVec3d				Exact;
		grVec3d				Off;
		grFloat				Error;

		j = EdgeVerts[k];

		if (j == p1 || j == p2)
			continue;

		pVert = grVertArray_GetVertByIndex(g_BSP->VertArray, j);
		assert(pVert);

		// Translate the point by the amount it would take to put the edge at the origin...
		grVec3d_Subtract(pVert, pEdgeV1, &Vert2);
		Dist = grVec3d_DotProduct(&Vert2, &EdgeDir);
		
		if (Dist <= Start || Dist >= End)
			continue;						// Point does not lie in between edge end points
		
		// Put the point along the edge by projecting it along the edge dir by the amount of the dist perpendicular from the start of the edge
		grVec3d_AddScaled(pEdgeV1, &EdgeDir, Dist, &Exact);
		grVec3d_Subtract(pVert, &Exact, &Off);
		Error = grVec3d_Length(&Off);

		if (fabs(Error) > OFF_EPSILON)
			continue;		// Point does NOT lie on the edge

		// break the edge
		//NumTJunctions++;

		// Recursively go down the back side, re-constructing the poly while preserving the winding order...
		if (!TestEdge_r(Start, Dist, p1, j, k+1))
			return GR_FALSE;

		// Now go down the front side, and add the TJunct point...
		if (!TestEdge_r(Dist, End, j, p2, k+1))
			return GR_FALSE;

		return GR_TRUE;
	}
							
	if (NumTempIndexVerts >= MAX_TEMP_INDEX_VERTS)
		return GR_FALSE;		// Whoops...

	// Insert the index in the temp index array, the poly will use this to re-construct itself
	TempIndexVerts[NumTempIndexVerts++] = p1;

	return GR_TRUE;
}

//=====================================================================================
//	FixFaceTJunctions
//=====================================================================================
static grBoolean FixFaceTJunctions(grBSPNode *Node, grBSP *BSP, grBSPNode_DrawFace *Face, grVertArray_Optimizer *Optimizer)
{
	int32				i, NumVerts;
	grVertArray_Index	v1, v2;
	int32				Start[MAX_TEMP_INDEX_VERTS];
	int32				Count[MAX_TEMP_INDEX_VERTS];
	grFloat				Len;
	int32				Base;

	NumTempIndexVerts = 0;
	
	NumVerts = Face->Poly->NumVerts;

	g_BSP = BSP;

	for (i=0; i< NumVerts; i++)
	{
		v1 = Face->Poly->Verts[i];
		v2 = Face->Poly->Verts[(i+1)%NumVerts];

		// Get the 2 verts that define the edge
		pEdgeV1 = grVertArray_GetVertByIndex(BSP->VertArray, v1);
		pEdgeV2 = grVertArray_GetVertByIndex(BSP->VertArray, v2);

		// Find all the verts that lie inside the box formed by the edge
		grVertArray_GetEdgeVerts(Optimizer, pEdgeV1, pEdgeV2, EdgeVerts, &NumEdgeVerts, GR_VERTARRAY_MAX_VERTS);

		// Get the edge dir
		grVec3d_Subtract(pEdgeV2, pEdgeV1, &EdgeDir);
		// Get the length of the edge, and normalize the edge dir
		Len = grVec3d_Normalize(&EdgeDir);

		Start[i] = NumTempIndexVerts;

		// Recursively start constructing the poly while inserting TJuncts, and preserve winding order
		if (!TestEdge_r(0.0f, Len, v1, v2, 0))
			return GR_FALSE;

		Count[i] = NumTempIndexVerts - Start[i];
	}

	if (NumTempIndexVerts < 3)
	{
		//grIndexPoly_Destroy(&Face->Poly);
		return GR_TRUE;				// Face callapsed, mark it to be removed (by setting the poly to null)
	}

	// Try to find an edge that does not have any TJuncts on it, and use that as the first edge
	for (i=0; i< NumVerts; i++)
	{
		if (Count[i] == 1 && Count[(i+NumVerts-1)%NumVerts] == 1)
			break;
	}

	if (i == NumVerts)
		Base = 0;
	else
		Base = Start[i];

	if (!FinalizeFace(Face, Base, BSP))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_FixDrawFaceTJuncts_r
//=======================================================================================
grBoolean grBSPNode_FixDrawFaceTJuncts_r(grBSPNode *Node, grBSP *BSP, grVertArray_Optimizer *Optimizer)
{
	int32				i, GoodFaces;
	grBSPNode_DrawFace	*pFace;

	if (Node->Leaf)
		return GR_TRUE;

	GoodFaces = 0;

	// Go through each face, and fix TJuncts...
	for (i=0; i< Node->NumDrawFaces; i++)
	{
		pFace = Node->DrawFaces[i];

		if (!FixFaceTJunctions(Node, BSP, pFace, Optimizer))
			return GR_FALSE;

		if (pFace->Poly)
			GoodFaces++;
	}

	assert(GoodFaces <= Node->NumDrawFaces);		// NumFaces should have ONLY gotten smaller, NOT bigger

	// If the number of faces changed (faces got removed...), allocate a new array, and re-assign faces to the node
	if (GoodFaces != Node->NumDrawFaces)
	{
		grpBSPNode_DrawFace	*NewDrawFaces;
		int32				NumNewDrawFaces;

		NumNewDrawFaces = 0;

		NewDrawFaces = GR_RAM_ALLOCATE_ARRAY(grpBSPNode_DrawFace, GoodFaces);

		if (!NewDrawFaces)
			return GR_FALSE;

		// Faces got removed, reflect that change in the nodes DrawFace array
		for (i=0; i< Node->NumDrawFaces; i++)
		{
			pFace = Node->DrawFaces[i];

			if (!pFace->Poly)
			{
				grBSPNode_DrawFaceDestroy(&pFace, BSP);
				continue;
			}

			NewDrawFaces[NumNewDrawFaces++] = pFace;
		}
		
		assert(NumNewDrawFaces == GoodFaces);		// They should be the same or something went wrong...

		grRam_Free(Node->DrawFaces);
		Node->DrawFaces = NewDrawFaces;
	}	

	if (!grBSPNode_FixDrawFaceTJuncts_r(Node->Children[NODE_FRONT], BSP, Optimizer))
		return GR_FALSE;

	if (!grBSPNode_FixDrawFaceTJuncts_r(Node->Children[NODE_BACK], BSP, Optimizer))
		return GR_FALSE;

	return GR_TRUE;
}