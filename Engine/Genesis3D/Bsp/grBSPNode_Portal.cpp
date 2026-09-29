/****************************************************************************************/
/*  JEBSPNODE_PORTAL.C                                                                  */
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


#include "grBSP._h"

#include "Errorlog.h"
#include "Log.h"
#include "Ram.h"

static		int32 ActivePortals;
static		int32 PeekPortals;

//=====================================================================================
//	grBSPNode_PortalCreate
//=====================================================================================
grBSPNode_Portal *grBSPNode_PortalCreate(grPoly *Poly, grBSP *BSP)
{
	grBSPNode_Portal	*Portal;

	Portal = GR_RAM_ALLOCATE_STRUCT(grBSPNode_Portal);

	if (!Portal)
		return NULL;

	memset(Portal, 0, sizeof(grBSPNode_Portal));

	Portal->Poly = Poly;

	ActivePortals++;

	if (ActivePortals > PeekPortals)
		PeekPortals++;

	BSP->DebugInfo.NumPortals++;

	return Portal;
}

//=====================================================================================
//	grBSPNode_PortalDestroy
//=====================================================================================
void grBSPNode_PortalDestroy(grBSPNode_Portal **Portal, grBSP *BSP)
{
	assert(Portal);
	assert(*Portal);

	if ((*Portal)->Poly)
		grPoly_Destroy(&(*Portal)->Poly);

	//grBSPNode_PortalResetTopSide(*Portal);

	grRam_Free(*Portal);

	ActivePortals--;

	BSP->DebugInfo.NumPortals--;

	*Portal = NULL;
}

//=====================================================================================
//	grBSPNode_PortalIsValid
//=====================================================================================
grBoolean grBSPNode_PortalIsValid(const grBSPNode_Portal *Portal)
{
	grPoly		*Poly;
	grVec3d		*Verts;
	int32		i, k;
	grFloat		Val;

	Poly = Portal->Poly;
	Verts = Poly->Verts;

	if (Poly->NumVerts < 3)
		return GR_FALSE;

	for (i=0; i< Poly->NumVerts; i++)
	{
		for (k=0; k<3; k++)
		{
			Val = grVec3d_GetElement(&Verts[i], k);

			if (Val >= GR_BSP_MINMAX_BOUNDS)
				return GR_FALSE;
		
			if (Val <= -GR_BSP_MINMAX_BOUNDS)
				return GR_FALSE;
		}
	}

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_PortalFindTopSide
//	Examines the brushes on each side of the portal, and finde the side best suited to use
//	for the portal
//=====================================================================================
void grBSPNode_PortalFindTopSide(grBSPNode_Portal *Portal, grBSP *BSP, int32 s)
{
	grBSP_TopBrush		*TopBrush;

	assert(Portal);
	assert(Portal->Nodes[0]->Leaf);		// Portals SHOULD seperate leafs!
	assert(Portal->Nodes[1]->Leaf);
	assert(Portal->OnNode);

	if (Portal->Flags & PORTAL_SIDE_FOUND)
		return;		// Don't check portals more than once, sice each side checks both leafs

	assert(!Portal->Side);		// There should be no side set yet

	grBSPNode_GetTopSideSeperatingLeafs(BSP, Portal->Nodes[0]->Leaf, Portal->Nodes[1]->Leaf, Portal->OnNode->PlaneIndex, &TopBrush, &Portal->Side);

	if (Portal->Side)
	{
		Portal->Flags |= PORTAL_SIDE_FOUND;

		if (Portal->Side->FaceInfoIndex == GR_FACEINFO_ARRAY_NULL_INDEX)
			Portal->Side = NULL;	// Cancel out side for this portal, if it has no FaceInfo
	}
}

//=====================================================================================
//	grBSPNode_PortalResetTopSide
//=====================================================================================
void grBSPNode_PortalResetTopSide(grBSPNode_Portal *Portal)
{
	if (Portal->Side)
	{
		assert(Portal->Flags & PORTAL_SIDE_FOUND);
		Portal->Side = NULL;		// Reset the portal side...
	}

	Portal->Flags &= ~PORTAL_SIDE_FOUND;
}

//=====================================================================================
//	grBSPNode_PortalGetActiveCount
//=====================================================================================
int32 grBSPNode_PortalGetActiveCount(void)
{
	return ActivePortals;
}

//=====================================================================================
//	grBSPNode_PortalGetPeekCount
//=====================================================================================
int32 grBSPNode_PortalGetPeekCount(void)
{
	return PeekPortals;
}

