/****************************************************************************************/
/*  JEBSPNODE_LEAF.C                                                                    */
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

//=======================================================================================
//	grBSPNode_LeafCreate
//=======================================================================================
grBSPNode_Leaf *grBSPNode_LeafCreate(grBSP *BSP)
{
	grBSPNode_Leaf		*Leaf;

	Leaf = GR_RAM_ALLOCATE_STRUCT(grBSPNode_Leaf);

	if (!Leaf)
		return NULL;

	ZeroMem(Leaf);

	LN_Null(Leaf);

	return Leaf;
}

//=======================================================================================
//	grBSPNode_LeafDestroy
//=======================================================================================
void grBSPNode_LeafDestroy(grBSPNode_Leaf **Leaf, grBSP *BSP)
{
	grBSP_Brush		*Brush, *Next;

	assert(Leaf);
	assert(*Leaf);

	for (Brush = (*Leaf)->Brushes; Brush; Brush = Next)
	{
		Next = Brush->Next;

		grBSP_BrushDestroy(&Brush);
	}

	if ((*Leaf)->Sides)
	{
		int32		i;

		for (i=0; i< (*Leaf)->NumSides; i++)
		{
			grPlaneArray_RemovePlane(BSP->PlaneArray, &(*Leaf)->Sides[i].PlaneIndex);
		}

		grRam_Free((*Leaf)->Sides);
	}

	if ((*Leaf)->Area)
		grBSPNode_AreaDestroy(&(*Leaf)->Area);

	grBSPNode_LeafDestroyDrawFaceList(*Leaf, BSP);

	grRam_Free(*Leaf);

	*Leaf = NULL;
}

#define MAX_TEMP_LEAF_SIDES			(256)

//=====================================================================================
//	grBSPNode_LeafInitializeSides
//=====================================================================================
grBoolean grBSPNode_LeafInitializeSides(grBSPNode_Leaf *Leaf, grBSP *BSP)
{
	grPlane				Plane;
	int32				Axis, i, Dir;
	grBSPNode			*Node;
	grBSPNode_Portal	*Portal;
	grBSPNode_LeafSide	TempLeafSides[MAX_TEMP_LEAF_SIDES];
	int32				NumTempLeafSides, Side;
	grBSPNode_LeafSide	*pSide;

	assert(Leaf);
	assert(Leaf->Node);
	assert(!Leaf->Sides);
	assert(!Leaf->NumSides);

	if (!Leaf->Node->Portals)
		return GR_TRUE;			// If no portals, then can't make sides

	Node = Leaf->Node;

	NumTempLeafSides = 0;
	pSide = TempLeafSides;

#if 1
	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		grBSPNode_LeafSide	*pSide2;
		grPlaneArray_Index	PlaneIndex;

		assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
		assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);

		Side = (Portal->Nodes[1] == Node);

		PlaneIndex = Portal->PlaneIndex;

		// Don't add planes already in the list
		for (pSide2 = TempLeafSides, i=0; i< NumTempLeafSides; i++, pSide2++)
		{
			if (grPlaneArray_IndexIsCoplanar(pSide2->PlaneIndex, PlaneIndex))
				break;
		}

		if (i != NumTempLeafSides)
			continue;				// Plane already in list

		// Make sure we don't overflow our TempSides array
		if (NumTempLeafSides >= MAX_TEMP_LEAF_SIDES)
		{
			//grErrorLog_AddString(-1, "grBSPNode_LeafInitializeSides:  NumTempLeafSides >= MAX_TEMP_LEAF_SIDES.", NULL);
			//return GR_FALSE;
			return GR_TRUE;
		}

		pSide->PlaneIndex = PlaneIndex;

		// Reverse BEFORE we ref count so it will ref the correct plane
		// NOTE - Portals without nodes are portals on the outside, these are allways facing in, flip them outwards...
		if (!Side || !Portal->OnNode)
			pSide->PlaneIndex = grPlaneArray_IndexReverse(pSide->PlaneIndex);

		// Ref the planeindex
		if (!grPlaneArray_RefPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex))
			return GR_FALSE;

		NumTempLeafSides++;
		pSide++;
	}
#endif

#if 1
	// Add any bevel planes to the sides so we can expand them for axial box collisions
	for (Axis=0 ; Axis <3 ; Axis++)
	{
		for (Dir=-1 ; Dir <= 1 ; Dir+=2)
		{
			// See if the plane is allready in the sides
			for (pSide = TempLeafSides, i=0; i< NumTempLeafSides; i++, pSide++)
			{
				const grPlane		*pPlane;
				grFloat				FloatDir;

				pPlane = grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex);
				assert(pPlane);

				if (grPlaneArray_IndexSided(pSide->PlaneIndex))
					FloatDir = (grFloat)-Dir;
				else
					FloatDir = (grFloat)Dir;

				if (grVec3d_GetElement(&pPlane->Normal, Axis) == FloatDir)
					break;
			}

			if (i != NumTempLeafSides)
				continue;		// Side exist, don't add

			if (NumTempLeafSides >= MAX_TEMP_LEAF_SIDES)
			{
				//grErrorLog_AddString(-1, "grBSPNode_LeafInitializeSides:  NumTempLeafSides >= MAX_TEMP_LEAF_SIDES.", NULL);
				//return GR_FALSE;
				return GR_TRUE;
			}

			// Add a new axial aligned side
			grVec3d_Clear(&Plane.Normal);
			grVec3d_SetElement(&Plane.Normal, Axis, (grFloat)Dir);
				
			// get the mins/maxs from the gbsp brush
			if (Dir == 1)
				Plane.Dist = grVec3d_GetElement(&Node->Box.Max, Axis);
			else
				Plane.Dist = -grVec3d_GetElement(&Node->Box.Min, Axis);
				
			TempLeafSides[NumTempLeafSides].PlaneIndex = grPlaneArray_SharePlane(BSP->PlaneArray, &Plane);

			if (TempLeafSides[NumTempLeafSides].PlaneIndex == GR_PLANEARRAY_NULL_INDEX)
				return GR_FALSE;

			NumTempLeafSides++;
			//NumLeafBevels++;
		}
	}		
#endif

	if (NumTempLeafSides < 3)
		return GR_TRUE;

	// Allocate the sides for the leaf and copy them over
	Leaf->Sides = GR_RAM_ALLOCATE_ARRAY(grBSPNode_LeafSide, NumTempLeafSides);

	if (!Leaf->Sides)
		return GR_FALSE;

	// Copy over the sides in the allocated sides for the leaf
	for (i=0; i< NumTempLeafSides; i++)
		Leaf->Sides[i] = TempLeafSides[i];

	Leaf->NumSides = NumTempLeafSides;

	return GR_TRUE;
}

//=====================================================================================
//	grBSPNode_LeafFill_r
//=====================================================================================
grBoolean grBSPNode_LeafFill_r(grBSPNode_Leaf *Leaf, int32 Fill)
{
	grBSPNode_Portal	*Portal;
	int32				Side;
	grBSPNode			*Node;

	assert(Leaf);
	assert(Leaf->Node);		

	if (Leaf->Node->Flags & NODE_OUTSIDE)
		Log_Printf("**** Leak ****\n");

	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;

	if (Leaf->CurrentFrame == Fill)					// Only visit leafs once
		return GR_TRUE;

	Leaf->CurrentFrame = Fill;						// Mark the leaf as visitied

	Node = Leaf->Node;		// Get node that this leaf is on

	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		grBSPNode_Leaf	*LeafTo;

		assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
		assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);

		Side = (Portal->Nodes[1] == Node);

		LeafTo = Portal->Nodes[!Side]->Leaf;
		assert(LeafTo);

		// Flood to the leaf on the other side of the portal (!side)
		if (!grBSPNode_LeafFill_r(LeafTo, Fill))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LeafFloodAreas_r
//		Marks the current leaf with the current area, and adds the leaf to the areas LeafArray.
//		It then proceeds to keep flooding till it hits solid wall, or another vis portal
//=======================================================================================
grBoolean grBSPNode_LeafFloodAreas_r(grBSPNode_Leaf *Leaf, grBSPNode_Area *Area)
{
	grBSPNode			*Node;
	grBSPNode_Portal	*p;
	int32				Side;
	grBSPNode_AreaLeaf	*AreaLeaf;

	assert(Leaf);
	assert(Area);
	
	if (Leaf->Area)
		return GR_TRUE;		// Area already found

	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;		// Solid, can't cross portal

	// Add this leaf to the list of leafs on the Areas LeafArray...
	AreaLeaf = (grBSPNode_AreaLeaf*)grArray_GetNewElement(Area->LeafArray);
	AreaLeaf->Leaf = Leaf;

	// This leaf now has an area
	Leaf->Area = Area;

	// Ref the area (Area should have at least as many refs, as the number of leafs touching it)
	if (!grBSPNode_AreaCreateRef(Area))
		return GR_FALSE;

	// Grab the node of the leaf
	Node = Leaf->Node;
	assert(Node);

	for (p = Node->Portals; p; p = p->Next[Side])
	{
		Side = (p->Nodes[1] == Node);

		if (p->Side && p->Side->Flags & SIDE_VIS_PORTAL)
			continue;		// Don't flood out vis portals

		// Flood to the leaf on the opposite side of the portal
		if (!grBSPNode_LeafFloodAreas_r(p->Nodes[!Side]->Leaf, Area))
			return GR_FALSE;
	}
	
	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LeafMakeAreas
//		Starts at the current leaf, and floods out (grBSPNode_LeafFloodAreas_r) and marks 
//		all areas touched with the same area.  
//=======================================================================================
grBoolean grBSPNode_LeafMakeAreas(grBSPNode_Leaf *Leaf, grBSP *BSP, grChain *AreaChain)
{
	grBSPNode		*Node;
	grBSPNode_Area	*Area;

	assert(Leaf);
	assert(AreaChain);

	Node = Leaf->Node;
	assert(Node);

	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;		// Solids can't have areas

	if (Leaf->Area)
		return GR_TRUE;		// Area already set
	
	BSP->DebugInfo.NumAreas++;

	// Create an Area, and start flooding out of the current leaf using grBSPNode_LeafFloodAreas_r
	Area = grBSPNode_AreaCreate();

	if (!Area)
		return GR_FALSE;

	if (!grChain_AddLinkData(AreaChain, Area))
		return GR_FALSE;
		
	// Flood from this leaf, setting all touched leafs to this area
	if (!grBSPNode_LeafFloodAreas_r(Leaf, Area))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LeafCountAreaVisPortals
//=======================================================================================
grBoolean grBSPNode_LeafCountAreaVisPortals(grBSPNode_Leaf *Leaf)
{
	grBSPNode			*Node;
	grBSPNode_Portal	*p;
	int32				Side;

	assert(Leaf);

	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;

	Node = Leaf->Node;
	assert(Node);

	for (p = Node->Portals; p; p = p->Next[Side])
	{
		grBSPNode_Area			*Area, *OtherArea;
		grBSPNode_Leaf			*OtherLeaf;

		Side = (p->Nodes[1] == Node);

		if (!p->Side)		
			continue;		// Portal didn't have side, not visible

		if (!(p->Side->Flags & SIDE_VIS_PORTAL))
			continue;		// Only interested in VIS_PORTALS's

		Area = p->Nodes[Side]->Leaf->Area;
		assert(Area);
		assert(Area == Leaf->Area);

		OtherLeaf = p->Nodes[!Side]->Leaf;
		assert(OtherLeaf);
		assert(OtherLeaf != Leaf);

		if (OtherLeaf->Contents & GR_BSP_CONTENTS_SOLID)
			continue;		// Ignore VisPortals that flood into solid

		OtherArea = OtherLeaf->Area;
		assert(OtherArea);

		if (Area == OtherArea)		
			continue;		// Invalid vis portal! Seperates same area...
		
		Area->NumAreaPortals++;
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LeafMakeAreaVisPortals
//=======================================================================================
grBoolean grBSPNode_LeafMakeAreaVisPortals(grBSPNode_Leaf *Leaf, grBSP *BSP)
{
	grBSPNode			*Node;
	grBSPNode_Portal	*p;
	int32				Side;

	assert(Leaf);

	if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
		return GR_TRUE;

	Node = Leaf->Node;
	assert(Node);

	for (p = Node->Portals; p; p = p->Next[Side])
	{
		grBSPNode_AreaPortal	*AreaPortal;
		grBSPNode_Area			*Area, *OtherArea;
		grBSPNode_Leaf			*OtherLeaf;

		Side = (p->Nodes[1] == Node);

		if (!p->Side)		
			continue;		

		if (!(p->Side->Flags & SIDE_VIS_PORTAL))
			continue;		// Only interested in VIS_PORTALS's

		Area = p->Nodes[Side]->Leaf->Area;
		assert(Area);
		assert(Area == Leaf->Area);

		OtherLeaf = p->Nodes[!Side]->Leaf;
		assert(OtherLeaf);
		assert(OtherLeaf != Leaf);

		if (OtherLeaf->Contents & GR_BSP_CONTENTS_SOLID)
			continue;		// Ignore VisPortals that flood into solid

		OtherArea = OtherLeaf->Area;
		assert(OtherArea);

		if (Area == OtherArea)		
			continue;		// Invalid vis portal! Seperates same area...

		assert(Area->NumWorkAreaPortals < Area->NumAreaPortals);
		assert(Area->AreaPortals);
	
		AreaPortal = &Area->AreaPortals[Area->NumWorkAreaPortals++];

		AreaPortal->Poly = grPoly_CreateFromPoly(p->Poly, !Side);

		if (!AreaPortal->Poly)
			return GR_FALSE;

		AreaPortal->Target = OtherArea;		// This portal looks into the other area

		AreaPortal->Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, p->OnNode->PlaneIndex);

		if (!Side)
			grPlane_Inverse(&AreaPortal->Plane);
	}

	return GR_TRUE;
}

//=======================================================================================
//	CountFaces_r
//=======================================================================================
static void CountLeafFaces_r(grBSPNode_Leaf *Leaf, grBSPNode_Face *Face)
{
	assert(Face);

	while (Face->Merged)
		Face = Face->Merged;

	if (Face->Split[0])
	{
		CountLeafFaces_r(Leaf, Face->Split[0]);
		CountLeafFaces_r(Leaf, Face->Split[1]);
		return;
	}

	if ( Face->DrawFace )
	{
		Leaf->NumDrawFaces++;
	}
}

//=======================================================================================
//	GetLeafFaces_r
//=======================================================================================
static void GetLeafFaces_r(grBSPNode_Leaf *Leaf, grBSPNode_Face *Face)
{
	assert(Face);

	while (Face->Merged)
	{
		assert(!Face->DrawFace);
		Face = Face->Merged;
	}

	if (Face->Split[0])
	{
		assert(!Face->DrawFace);
		GetLeafFaces_r(Leaf, Face->Split[0]);
		GetLeafFaces_r(Leaf, Face->Split[1]);
		return;
	}

//	assert(Face->DrawFace); // portal Faces don't face DrawFaces !

	if ( Face->DrawFace )
	{
		Leaf->DrawFaces[Leaf->NumDrawFaces++] = Face->DrawFace;
	}
}

//=======================================================================================
//	grBSPNode_LeafMakeDrawFaceList
//=======================================================================================
grBoolean grBSPNode_LeafMakeDrawFaceList(grBSPNode_Leaf *Leaf, grBSP *BSP)
{
	int32				s;
	grBSPNode_Portal	*p;
	grBSPNode			*Node;

	assert(Leaf);

	Node = Leaf->Node;

	assert(Node);
	assert(Node->Leaf == Leaf);

	// Destroy any previous list of drawfaces on this leaf
	grBSPNode_LeafDestroyDrawFaceList(Leaf, BSP);

	// Count number of faces on this leaf
	Leaf->NumDrawFaces = 0;
	for (p = Node->Portals; p; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->Face[s])
			continue;

		CountLeafFaces_r(Leaf, p->Face[s]);
	}

	if (!Leaf->NumDrawFaces)			// Don't bother making anything
		return GR_TRUE;

	// Allocate space for the array of pointers to the faces
	Leaf->DrawFaces = GR_RAM_ALLOCATE_ARRAY(grpBSPNode_DrawFace, Leaf->NumDrawFaces);

	Leaf->NumDrawFaces = 0;
	for (p = Node->Portals; p; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->Face[s])
			continue;

		GetLeafFaces_r(Leaf, p->Face[s]);
	}

	return GR_TRUE;
}

//=======================================================================================
//	grBSPNode_LeafDestroyDrawFaceList
//=======================================================================================
void grBSPNode_LeafDestroyDrawFaceList(grBSPNode_Leaf *Leaf, grBSP *BSP)
{
	assert(Leaf);

	if (Leaf->DrawFaces)
	{
		assert(Leaf->NumDrawFaces);
		grRam_Free(Leaf->DrawFaces);
		Leaf->DrawFaces = NULL;
		Leaf->NumDrawFaces = 0;
	}
	else
	{
		assert(!Leaf->NumDrawFaces);
	}
}

//=======================================================================================
//	grBSPNode_BubbleRecursionBit
//=======================================================================================
void grBSPNode_BubbleRecursionBit(grBSPNode *Node, uint32 RecursionBit)
{
	assert(grBSPNode_IsValid(Node));

	while (Node)
	{
		Node->RecursionBits |= RecursionBit;
		Node = Node->Parent;
	}
}

//=======================================================================================
//	grBSPNode_LeafSetRecursionBit
//=======================================================================================
void grBSPNode_LeafSetRecursionBit(grBSPNode_Leaf *Leaf, uint32 RecursionBit)
{
	int32		i;

	assert(Leaf);

	if (Leaf->Node->RecursionBits & RecursionBit)
		return;		// Don't bother messing with it again...

	Leaf->Node->RecursionBits |= RecursionBit;

	// Set all faces on the leaf as visible...
	for (i=0; i< Leaf->NumDrawFaces; i++)
	{
		Leaf->DrawFaces[i]->RecursionBits |= RecursionBit;
	}

	// Bubble vis info up to parents
	grBSPNode_BubbleRecursionBit(Leaf->Node->Parent, RecursionBit);
}

//#define LEAF_COLLISION_EPSILON	(0.1f)
//#define LEAF_COLLISION_EPSILON	(0.0001f)  // DarkRift/Incarnadine
#define LEAF_COLLISION_EPSILON	(0.01f)  // Incarnadine

//=====================================================================================
//	ExpandPlaneForBox
//	Pushes a plane out by the side of the box it is looking at
//=====================================================================================
static void ExpandPlaneForBox(grPlane *Plane, const grVec3d *Mins, const grVec3d *Maxs)
{
	grVec3d		*Normal;

	Normal = &Plane->Normal;
	
	if (Normal->X > 0)
		Plane->Dist -= Normal->X * Mins->X;
	else	 
		Plane->Dist -= Normal->X * Maxs->X;
	
	if (Normal->Y > 0)
		Plane->Dist -= Normal->Y * Mins->Y;
	else
		Plane->Dist -= Normal->Y * Maxs->Y;

	if (Normal->Z > 0)
		Plane->Dist -= Normal->Z * Mins->Z;
	else							 
		Plane->Dist -= Normal->Z * Maxs->Z;
}

//=====================================================================================
//	grBSPNode_LeafCollision_r
//=====================================================================================
grBoolean grBSPNode_LeafCollision_r(const grBSPNode_Leaf *Leaf, grBSP *BSP, int32 Side, int32 PSide, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grBSPNode_CollisionInfo2 *Info)
{
	grFloat				Fd, Bd, Dist;
	grBSPNode_LeafSide	*pSide;
	grPlane				Plane;
	int32				Side2;
	grVec3d				I;

	assert(Leaf);
	assert(Box);
	assert(Front);
	assert(Back);
	assert(Info);
	assert(Leaf->Sides);

	if (!PSide)
		return GR_FALSE;		// Ray was on front side, not a collision when dealing with convex hulls

	if (!(Leaf->Contents & GR_BSP_CONTENTS_SOLID)) // Icestorm/Incarnadine
		return GR_FALSE;

	if (Side >= Leaf->NumSides)
	{
		// We did the solid test above already, so we already know the answer. - Icestorm
		return GR_TRUE;

		/*
		if (Leaf->Contents & GR_BSP_CONTENTS_SOLID)
			return GR_TRUE;		// If it lands behind all sides, it is inside
		else
			return GR_FALSE;
		*/
	}

	pSide = &Leaf->Sides[Side];

	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex);
	Plane.Type = Type_Any;
	
	if (grPlaneArray_IndexSided(pSide->PlaneIndex))
		grPlane_Inverse(&Plane);
	
	// Simulate the point having a box, by pushing the plane out by the box size
	ExpandPlaneForBox(&Plane, &Box->Min, &Box->Max);

	Fd = grPlane_PointDistanceFast(&Plane, Front);
	Bd = grPlane_PointDistanceFast(&Plane, Back);

	if (Fd >= 0 && Bd >= 0)	// Leaf sides are convex hulls, so front side is totally outside
		return GR_FALSE;

	if (Fd < 0 && Bd < 0)
		return grBSPNode_LeafCollision_r(Leaf, BSP, Side+1, 1, Box, Front, Back, Info);

	// We have an intersection
    Side2 = Fd < 0;
	
	/* Icestorm/Incarnadine	
	if (Fd < 0)
		Dist = (Fd + LEAF_COLLISION_EPSILON)/(Fd-Bd);
	else
		Dist = (Fd - LEAF_COLLISION_EPSILON)/(Fd-Bd);
	*/

	Dist = (Fd - LEAF_COLLISION_EPSILON)/(Fd-Bd);

	if (Dist < 0.0f)
		Dist = 0.0f;
	
	if (Dist > 1.0f)
		Dist = 1.0f;

    I.X = Front->X + Dist * (Back->X - Front->X);
    I.Y = Front->Y + Dist * (Back->Y - Front->Y);
    I.Z = Front->Z + Dist * (Back->Z - Front->Z);

	// Only go down the back side, since the front side is empty in a convex tree
	if (grBSPNode_LeafCollision_r(Leaf, BSP, Side+1, Side2, Box, Front, &I, Info))
	{
		// No collision info needs to be set here or the wrong impact
		// point could be returned.  - Icestorm/Incarnadine
		return GR_TRUE;
	}
	else if (grBSPNode_LeafCollision_r(Leaf, BSP, Side+1, !Side2, Box, &I, Back, Info))
	{
		// Record the intersection closest to the start of ray
		if (!Info->HitLeaf)
		{
			grFloat		Dist;

			Dist = grVec3d_DistanceBetween(Info->Front, &I);

			if (Dist < Info->BestDist)
			{
				Info->BestDist = Dist;
				*(Info->Impact) = I;
				*(Info->Plane) = Plane;				
			}	
			Info->HitLeaf = GR_TRUE;	// Icestorm: should prevent unnecessary tests
		}
		Info->HitSet = GR_TRUE;	// For collisioncheck only
		return GR_TRUE;
	}
	
	return GR_FALSE;	
}

// Added by Icestorm
//=====================================================================================
//	GetChangeBoxPlaneImpact
//	Pushes a plane out by the side of the box it is looking at
//  and get this point
//=====================================================================================
static void GetChangeBoxPlaneImpact(grPlane *Plane, const grExtBox *Box, const grVec3d *Pos)
{
	grVec3d Impact;

	if (Plane->Normal.X > 0)
		Impact.X=Box->Min.X+Pos->X;
	else	 
		Impact.X=Box->Max.X+Pos->X;	

	if (Plane->Normal.Y > 0)
		Impact.Y=Box->Min.Y+Pos->Y;
	else
		Impact.Y=Box->Max.Y+Pos->Y;

	if (Plane->Normal.Z > 0)
		Impact.Z=Box->Min.Z+Pos->Z;
	else				
		Impact.Z=Box->Max.Z+Pos->Z;

	Plane->Dist = Plane->Normal.X * Impact.X;
	Plane->Dist += Plane->Normal.Y * Impact.Y;
	Plane->Dist += Plane->Normal.Z * Impact.Z;
}

//=====================================================================================
//	grBSPNode_LeafChangeBoxCollision_r
//=====================================================================================
grBoolean grBSPNode_LeafChangeBoxCollision_r(const grBSPNode_Leaf *Leaf, grBSP *BSP, int32 Side, int32 PSide, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grBSPNode_CollisionInfo3 *Info)
{
	grFloat				Fd, Bd,Fd2,Bd2, Dist;
	grBSPNode_LeafSide	*pSide;
	grPlane				Plane,FPlane,FPlane2,BPlane,BPlane2;
	int32				Side2;
	grExtBox			IBox;

	assert(Leaf);
	assert(Pos);
	assert(FrontBox);
	assert(BackBox);
	assert(Info);
	assert(Leaf->Sides);

	if (!PSide)
		return GR_FALSE;		// Box was on front side, not a collision when dealing with convex hulls

	if (!(Leaf->Contents & GR_BSP_CONTENTS_SOLID))
		return GR_FALSE;

	// We did the solid test above already, so we already know the answer.
	if (Side >= Leaf->NumSides)
		return GR_TRUE;

	pSide = &Leaf->Sides[Side];

	Plane = *grPlaneArray_GetPlaneByIndex(BSP->PlaneArray, pSide->PlaneIndex);
	Plane.Type = Type_Any;
	
	if (grPlaneArray_IndexSided(pSide->PlaneIndex))
		grPlane_Inverse(&Plane);
	
	FPlane=BPlane=FPlane2=BPlane2=Plane;
	// Push the plane to all four corners (front/back and biggest/smallest dist.)
	// and get the dists.
	ExpandPlaneForBox(&FPlane,  &FrontBox->Min, &FrontBox->Max);
	ExpandPlaneForBox(&FPlane2, &FrontBox->Max, &FrontBox->Min);

	Fd  = grPlane_PointDistanceFast(&FPlane,  Pos);
	Fd2 = grPlane_PointDistanceFast(&FPlane2, Pos);

	ExpandPlaneForBox(&BPlane,  &BackBox->Min, &BackBox->Max);
	ExpandPlaneForBox(&BPlane2, &BackBox->Max, &BackBox->Min);

	Bd  = grPlane_PointDistanceFast(&BPlane,  Pos);
	Bd2 = grPlane_PointDistanceFast(&BPlane2, Pos);

	if (Fd >= 0 && Bd >= 0 && Fd2>=0 && Bd2>=0)	// Leaf sides are convex hulls, so boxes are totally outside
		return GR_FALSE;

	if (Fd < 0 && Bd < 0 && Fd2<0 && Bd2<0)  // all are inside
		return grBSPNode_LeafChangeBoxCollision_r(Leaf, BSP, Side+1, 1, Pos, FrontBox, BackBox, Info);

	// both are in- and outside: test rest of planes
	// same as Frontbox inside,except there is no IBox 
	if ((Fd < 0 && Fd2 >= 0) || (Fd >= 0 && Fd2 < 0)) 
		return grBSPNode_LeafChangeBoxCollision_r(Leaf, BSP, Side+1, 1, Pos, FrontBox, BackBox, Info);

	// We have an intersection
    Side2 = Fd < 0;
	
	if (Side2) { Fd=Fd2;Bd=Bd2; } // Get side of collision

	Dist = (Fd - LEAF_COLLISION_EPSILON)/(Fd-Bd);

	if (Dist < 0.0f)
		Dist = 0.0f;
	
	if (Dist > 1.0f)
		Dist = 1.0f;

	// Calc. new changed box, a little bit outside
    IBox.Min.X = FrontBox->Min.X + Dist * (BackBox->Min.X - FrontBox->Min.X);
    IBox.Min.Y = FrontBox->Min.Y + Dist * (BackBox->Min.Y - FrontBox->Min.Y);
    IBox.Min.Z = FrontBox->Min.Z + Dist * (BackBox->Min.Z - FrontBox->Min.Z);

	IBox.Max.X = FrontBox->Max.X + Dist * (BackBox->Max.X - FrontBox->Max.X);
    IBox.Max.Y = FrontBox->Max.Y + Dist * (BackBox->Max.Y - FrontBox->Max.Y);
    IBox.Max.Z = FrontBox->Max.Z + Dist * (BackBox->Max.Z - FrontBox->Max.Z);

	// Only go down the back side, since the front side is empty in a convex tree
	if (grBSPNode_LeafChangeBoxCollision_r(Leaf, BSP, Side+1, Side2, Pos, FrontBox, &IBox, Info))
	{
		// No collision info needs to be set here or the wrong impact
		// box could be returned.
		return GR_TRUE;
	}
	else if (grBSPNode_LeafChangeBoxCollision_r(Leaf, BSP, Side+1, !Side2, Pos, &IBox, BackBox, Info))
	{
		// Record the intersection closest to the start of ray
		if (!Info->HitLeaf)
		{
			grFloat		Dist;

			GetChangeBoxPlaneImpact(&Plane,&IBox,Pos);
			Dist = grVec3d_DistanceBetween(&Info->FrontBox->Min, &IBox.Min);

			if (Dist < Info->BestDist)
			{
				Info->BestDist = Dist;
				*(Info->ImpactBox) = IBox;
				*(Info->Plane) = Plane;
			}	
			
			Info->HitLeaf = GR_TRUE;
						
		}
		Info->HitSet = GR_TRUE;		// Icestorm: For collisioncheck only
		return GR_TRUE;
	}
	
	return GR_FALSE;	
}