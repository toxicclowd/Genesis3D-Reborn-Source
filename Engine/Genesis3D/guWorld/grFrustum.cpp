/****************************************************************************************/
/*  JEFRUSTUM.C                                                                         */
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
#include <memory.h>		// memcpy

#include "grFrustum.h"
#include "Camera._h"

static void SetUpFrustumBBox(grFrustum *Info);


//================================================================================
//	grFrustum_SetFromCamera
//================================================================================
GRAPI void GRCC grFrustum_SetFromCamera(grFrustum *Frustum, const grCamera *Camera)
{
    grFloat	s, c;
    grVec3d	Normal;
	int32	i;

	// BEGIN - Far clip plane - paradoxnj 2/9/2005
	grBoolean		ZFarEnable;
	grFloat			ZFar;
	// END - Far clip plane - paradoxnj 2/9/2005

    grCamera_GetViewAngleXSinCos(Camera,&s,&c);

    // Left clip plane
    Normal.X = s;
    Normal.Y = 0.0f;
    Normal.Z = -c;
	grVec3d_Normalize(&Normal);
	Frustum->Planes[0].Normal = Normal;

    // Right clip plane
    Normal.X = -s;
	grVec3d_Normalize(&Normal);
	Frustum->Planes[1].Normal = Normal;

    grCamera_GetViewAngleYSinCos(Camera,&s,&c);

    // Bottom clip plane
    Normal.X = 0.0f;
    Normal.Y = s;
    Normal.Z = -c;
	grVec3d_Normalize(&Normal);
	Frustum->Planes[2].Normal = Normal;

    // Top clip plane
    Normal.Y = -s;
	grVec3d_Normalize(&Normal);
	Frustum->Planes[3].Normal = Normal;

	Frustum->FrontPlane = NULL;
	Frustum->NumPlanes = 4;

	// Clear all distances
	for (i=0; i<Frustum->NumPlanes; i++)
	{
		Frustum->Planes[i].Dist = 0.0f;
		Frustum->Planes[i].Type = Type_Any;
	}

	// BEGIN - Far clip plane - paradoxnj MODIFIED 3/9/2005
	grCamera_GetFarClipPlane(Camera, &ZFarEnable, &ZFar);

	if (ZFarEnable)
	{
		// Farclip plane
		Normal.X = 0.0f;
		Normal.Y = 0.0f;
		Normal.Z = 1.0f;
		grVec3d_Normalize(&Normal);
		Frustum->Planes[4].Normal = Normal;

		Frustum->Planes[4].Dist = -(ZFar/grCamera_GetZScale(Camera));
//		Frustum->Planes[4].Dist = -grCamera_GetZScale(Camera);
		Frustum->Planes[4].Type = Type_Any;

		Frustum->NumPlanes = 5;
	}
	// END - Far clip plane - paradoxnj MODIFIED 3/9/2005

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Frustum);
}

//================================================================================
//	grFrustum_SetFromCamera
//================================================================================
GRAPI void GRCC grFrustum_SetWorldSpaceFromCamera(grFrustum *pFrustum, const grCamera *Camera)
{
	grFrustum CamFrustum;

	grFrustum_SetFromCamera(&CamFrustum,Camera);
	grFrustum_TransformToWorldSpace(&CamFrustum,Camera,pFrustum);
	pFrustum->FrontPlane = NULL;
}

//================================================================================
//	grFrustum_SetFromVerts
//	Create a frustum looking through a poly (from the POV)
//================================================================================
GRAPI grBoolean GRCC grFrustum_SetFromVerts(grFrustum *Frustum, const grVec3d *POV, const grVec3d *Verts, int32 NumVerts)
{
	int32			NextVert;
	const grVec3d	*Vert1, *Vert2;
	grVec3d			Vect1, Vect2;
	grPlane			*Planes;
	int32			i;

	if (NumVerts >= GR_FRUSTUM_MAX_PLANES)
		return GR_FALSE;		// Too many planes!!!
	
	Planes = Frustum->Planes;

	Frustum->NumPlanes = 0;

	for (i=0; i< NumVerts; i++)
	{
		NextVert = ((i+1) < NumVerts) ? (i+1) : 0;

		Vert1 = &Verts[i];
		Vert2 = &Verts[NextVert];

		// FIXME:  Check for coplanar edges???
		if (grVec3d_Compare(Vert1, Vert2, 0.1f))	// Degenerate edge...
			continue;	

		grVec3d_Subtract(Vert2, Vert1, &Vect1);
		grVec3d_Subtract(POV, Vert1, &Vect2);

		grVec3d_CrossProduct(&Vect2, &Vect1, &Planes->Normal);
		grVec3d_Normalize(&Planes->Normal);

		Planes->Dist = grVec3d_DotProduct(POV, &Planes->Normal);
		Planes->Type = Type_Any;

		Planes++;
		Frustum->NumPlanes++;
	}

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Frustum);
	
	return GR_TRUE;
}

//================================================================================
//	grFrustum_SetFromVerts2
//	Create a frustum looking through a poly (from the origin)
//================================================================================
GRAPI grBoolean GRCC grFrustum_SetFromVerts2(grFrustum *Frustum, const grVec3d *Verts, int32 NumVerts)
{
	int32			NextVert;
	const grVec3d	*pVert1, *pVert2;
	grVec3d			Vect;
	grPlane			*Planes;
	int32			i;

	if (NumVerts >= GR_FRUSTUM_MAX_PLANES)
		return GR_FALSE;		// Too many planes!!!
	
	Planes = Frustum->Planes;

	Frustum->FrontPlane = NULL;
	Frustum->NumPlanes = 0;

	for (i=0; i< NumVerts; i++)
	{
		NextVert = ((i+1) < NumVerts) ? (i+1) : 0;

		pVert1 = &Verts[i];
		pVert2 = &Verts[NextVert];

		// FIXME:  Check for coplanar edges???
		if (grVec3d_Compare(pVert1, pVert2, 0.1f))	// Degenerate edge...
			continue;	

		grVec3d_Subtract(pVert1, pVert2, &Vect);
		
		grVec3d_CrossProduct(&Vect, pVert2, &Planes->Normal);
		grVec3d_Normalize(&Planes->Normal);

		Planes->Dist = 0.0f;
		Planes->Type = Type_Any;

		Planes++;
		Frustum->NumPlanes++;
	}

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Frustum);
	
	return GR_TRUE;
}

//================================================================================
//	grFrustum_SetFromLVerts
//	Create a frustum looking through a poly (from the POV)
//================================================================================
GRAPI grBoolean GRCC grFrustum_SetFromLVerts(grFrustum *Frustum, const grVec3d *POV, const grLVertex *Verts, int32 NumVerts)
{
	int32			NextVert;
	grVec3d			*Vert1, *Vert2;
	grVec3d			Vect1, Vect2;
	grPlane			*Planes;
	int32			i;

	if (NumVerts >= GR_FRUSTUM_MAX_PLANES)
		return GR_FALSE;		// Too many planes!!!
	
	Planes = Frustum->Planes;

	Frustum->FrontPlane = NULL;
	Frustum->NumPlanes = 0;

	for (i=0; i< NumVerts; i++)
	{
		NextVert = ((i+1) < NumVerts) ? (i+1) : 0;

		Vert1 = (grVec3d*)&Verts[i];
		Vert2 = (grVec3d*)&Verts[NextVert];

		// FIXME:  Check for coplanar edges???
		if (grVec3d_Compare(Vert1, Vert2, 0.1f))	// Degenerate edge...
			continue;	

		grVec3d_Subtract(Vert2, Vert1, &Vect1);
		grVec3d_Subtract(POV, Vert1, &Vect2);

		grVec3d_CrossProduct(&Vect2, &Vect1, &Planes->Normal);
		//grVec3d_CrossProduct(&Vect1, &Vect2, &Planes->Normal);
		grVec3d_Normalize(&Planes->Normal);

		Planes->Dist = grVec3d_DotProduct(POV, &Planes->Normal);
		Planes->Type = Type_Any;

		Planes++;
		Frustum->NumPlanes++;
	}

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Frustum);
	
	return GR_TRUE;
}

//================================================================================
//	grFrustum_SetFromLVerts2
//	Create a frustum looking through a poly (from the origin)
//================================================================================
GRAPI grBoolean GRCC grFrustum_SetFromLVerts2(grFrustum *Frustum, const grLVertex *Verts, int32 NumVerts, grBoolean Flip)
{
	int32			NextVert;
	const grVec3d	*pVert1, *pVert2;
	grVec3d			Vect;
	grPlane			*Planes;
	int32			i;

	if (NumVerts >= GR_FRUSTUM_MAX_PLANES)
		return GR_FALSE;		// Too many planes!!!
	
	Planes = Frustum->Planes;

	Frustum->FrontPlane = NULL;
	Frustum->NumPlanes = 0;

	for (i=0; i< NumVerts; i++)
	{
		NextVert = ((i+1) < NumVerts) ? (i+1) : 0;

		pVert1 = (grVec3d*)&Verts[i];
		pVert2 = (grVec3d*)&Verts[NextVert];

		// FIXME:  Check for coplanar edges???
		if (grVec3d_Compare(pVert1, pVert2, 0.1f))	// Degenerate edge...
			continue;	

		grVec3d_Subtract(pVert1, pVert2, &Vect);
		
		if (Flip)
			grVec3d_CrossProduct(pVert2, &Vect, &Planes->Normal);
		else
			grVec3d_CrossProduct(&Vect, pVert2, &Planes->Normal);

		grVec3d_Normalize(&Planes->Normal);

		Planes->Dist = 0.0f;
		Planes->Type = Type_Any;

		Planes++;
		Frustum->NumPlanes++;
	}

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Frustum);
	
	return GR_TRUE;
}

//================================================================================
//	grFrustum_AddPlane
//================================================================================
GRAPI grBoolean GRCC grFrustum_AddPlane(grFrustum *Frustum, const grPlane *SrcPlane, grBoolean FrontPlane)
{
	grPlane			*pDstPlane;

	if (Frustum->NumPlanes >= GR_FRUSTUM_MAX_PLANES)
		return GR_FALSE;		// Out of space!

	pDstPlane = &Frustum->Planes[Frustum->NumPlanes++];

	*pDstPlane = *SrcPlane;

	if (FrontPlane)
		Frustum->FrontPlane = pDstPlane;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_RotateToWorldSpace
//================================================================================
GRAPI void GRCC grFrustum_RotateToWorldSpace(const grFrustum *In, const grCamera *Camera, grFrustum *Out)
{
    int32			i;
	const grPlane	*InPlane;
	grPlane			*OutPlane;
	const grXForm3d	*InvXForm;

	InvXForm = grCamera_WorldXForm(Camera);			// Get CameraToWorldXForm

	InPlane = In->Planes;
	OutPlane = Out->Planes;

	// Rotate all the planes
	for (i=0; i<In->NumPlanes; i++, InPlane++, OutPlane++)
	{
		OutPlane->Type = InPlane->Type;

		grPlane_Rotate(InPlane, InvXForm, OutPlane);

		// If this InPlane is the front plane in the In frustum, then assign the 
		// corresponding OutPlane as the frontplane in the Out Frustum
		if (InPlane == In->FrontPlane)
			Out->FrontPlane = OutPlane;
	}

	Out->NumPlanes = In->NumPlanes;

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Out);
}

//================================================================================
//	grFrustum_TransformToWorldSpace
//================================================================================
GRAPI void GRCC grFrustum_TransformToWorldSpace(const grFrustum *In, const grCamera *Camera, grFrustum *Out)
{
    int32			i;
	const grPlane	*InPlane;
	grPlane			*OutPlane;
	const grXForm3d	*InvXForm;

	InvXForm = grCamera_WorldXForm(Camera);			// Get CameraToWorldXForm

	InPlane = In->Planes;
	OutPlane = Out->Planes;

	// Transform all the planes
	for (i=0; i<In->NumPlanes; i++, InPlane++, OutPlane++)
	{
		if (InPlane->Dist)
			grPlane_Transform(InPlane, InvXForm, OutPlane);
		else
		{
			// Transformation for plane is easy if plane is at the origin
			grPlane_Rotate(InPlane, InvXForm, OutPlane);		
			OutPlane->Dist = grVec3d_DotProduct(grCamera_GetPov(Camera), &OutPlane->Normal) - CLIP_PLANE_EPSILON;
		}
			
		// If this InPlane is the front plane in the In frustum, then assign the 
		// corresponding OutPlane as the frontplane in the Out Frustum
		if (InPlane == In->FrontPlane)
			Out->FrontPlane = OutPlane;
	}

	Out->NumPlanes = In->NumPlanes;

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Out);
}

//================================================================================
//	grFrustum_Rotate
//================================================================================
GRAPI void GRCC grFrustum_Rotate(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out)
{
    int32			i;
	const grPlane	*InPlane;
	grPlane			*OutPlane;

	InPlane = In->Planes;
	OutPlane = Out->Planes;

	// Transform all the planes
	for (i=0; i<In->NumPlanes; i++, InPlane++, OutPlane++)
	{
		grPlane_Rotate(InPlane, XForm, OutPlane);		

		// If this InPlane is the front plane in the In frustum, then assign the 
		// corresponding OutPlane as the frontplane in the Out Frustum
		if (InPlane == In->FrontPlane)
			Out->FrontPlane = OutPlane;
	}

	Out->NumPlanes = In->NumPlanes;

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Out);
}

//================================================================================
//	grFrustum_Transform
//================================================================================
GRAPI void GRCC grFrustum_Transform(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out)
{
    int32			i;
	const grPlane	*InPlane;
	grPlane			*OutPlane;

	InPlane = In->Planes;
	OutPlane = Out->Planes;

	// Transform all the planes
	for (i=0; i<In->NumPlanes; i++, InPlane++, OutPlane++)
	{
		if (InPlane->Dist)
			grPlane_Transform(InPlane, XForm, OutPlane);
		else
		{
			// Transformation for plane is easy if plane is at the origin
			grPlane_Rotate(InPlane, XForm, OutPlane);		
			OutPlane->Dist = grVec3d_DotProduct(&XForm->Translation, &OutPlane->Normal) - CLIP_PLANE_EPSILON;
		}
			
		// If this InPlane is the front plane in the In frustum, then assign the 
		// corresponding OutPlane as the frontplane in the Out Frustum
		if (InPlane == In->FrontPlane)
			Out->FrontPlane = OutPlane;
	}

	Out->NumPlanes = In->NumPlanes;

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Out);
}

//================================================================================
//	grFrustum_Transform
//================================================================================
GRAPI void GRCC grFrustum_TransformRenorm(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out)
{
    int32			i;
	const grPlane	*InPlane;
	grPlane			*OutPlane;

	InPlane = In->Planes;
	OutPlane = Out->Planes;

	// Transform all the planes
	for (i=0; i<In->NumPlanes; i++, InPlane++, OutPlane++)
	{
		grPlane_TransformRenorm(InPlane, XForm, OutPlane);
			
		// If this InPlane is the front plane in the In frustum, then assign the 
		// corresponding OutPlane as the frontplane in the Out Frustum
		if (InPlane == In->FrontPlane)
			Out->FrontPlane = OutPlane;
	}

	Out->NumPlanes = In->NumPlanes;

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(Out);
}

//================================================================================
//	grFrustum_Transform
//================================================================================
GRAPI void GRCC grFrustum_TransformAnchored(grFrustum *F, const grXForm3d *XForm, const grVec3d * Anchor)
{
int32			i;
grPlane			*Plane;

	Plane = F->Planes;

	// Transform all the planes
	for (i=0; i<F->NumPlanes; i++, Plane++)
	{
		grXForm3d_Rotate(XForm, &Plane->Normal, &Plane->Normal);
		grVec3d_Normalize(&Plane->Normal); // if XForm isn't normalized, need to renorm here
		// Find the Dist of the new plane by projecting the transformed point on the new plane
		Plane->Dist = grVec3d_DotProduct(&Plane->Normal, Anchor);
		Plane->Type = Type_Any;
	}

	// Get BBox info for fast BBox rejection against frustum...
	SetUpFrustumBBox(F);
}

//================================================================================
//	SetUpFrustumBBox
//	Setup bbox min/max test for the quadrant the frustum planes are in...
//================================================================================
static void SetUpFrustumBBox(grFrustum *Info)
{
	int32		i, *Index;

	Index = Info->FrustumBBoxIndexes;

	for (i=0 ; i<Info->NumPlanes ; i++)
	{
		if (Info->Planes[i].Normal.X < 0)			// Plane is facing left
		{
			Index[0] = 0;							// Take LEFT side of box for complete rejection
			Index[3] = 4;							// Take RIGHT side of box for totally accept
		}
		else										// Plane is facing right
		{
			Index[0] = 4;							// Take RIGHT side of box for complete rejection
			Index[3] = 0;							// Take LEFT side of box for totally accept
		}
		if (Info->Planes[i].Normal.Y < 0)			// Same rules apply for below...
		{
			Index[1] = 1;
			Index[4] = 5;
		}
		else
		{
			Index[1] = 5;
			Index[4] = 1;
		}
		if (Info->Planes[i].Normal.Z < 0)
		{
			Index[2] = 2;
			Index[5] = 6;
		}
		else
		{
			Index[2] = 6;
			Index[5] = 2;
		}

		Info->pFrustumBBoxIndexes[i] = Index;
		Index += 6;
	}
}

//================================================================================
//	grFrustum_SetClipFlagsFromBBox
//================================================================================
GRAPI grBoolean GRCC grFrustum_SetClipFlagsFromExtBox(const grFrustum *Frustum,const grExtBox *BBox,uint32 ClipFlags,uint32 *pClipFlags)
{
const grFloat *MinMaxs;
int32		*Index, p;
uint32		mask;
grFloat		Dist2;
grVec3d		Pnt;
const grPlane *pPlane;

	assert(Frustum);
	assert(BBox);

	MinMaxs = (const grFloat *)&(BBox->Min);

	for (pPlane = Frustum->Planes, p=0; ; p++, pPlane++)
	{
		mask = 1UL<<p;
		if ( mask > ClipFlags )
			break;
		if (!(ClipFlags & mask))
			continue;

		Index = Frustum->pFrustumBBoxIndexes[p];

		Pnt.X = MinMaxs[Index[0]];
		Pnt.Y = MinMaxs[Index[1]];
		Pnt.Z = MinMaxs[Index[2]];
		
		Dist2 = grVec3d_DotProduct(&Pnt, &pPlane->Normal);
		Dist2 -= pPlane->Dist;

		if (Dist2 <= 0)
		{
			// box is totally outside
			return GR_FALSE;
		}

		Pnt.X = MinMaxs[Index[3]];
		Pnt.Y = MinMaxs[Index[4]];
		Pnt.Z = MinMaxs[Index[5]];

		Dist2 = grVec3d_DotProduct(&Pnt, &pPlane->Normal);
		Dist2 -= pPlane->Dist;

		if (Dist2 >= 0)		
			ClipFlags ^= mask;		// Don't need to clip to this plane anymore
	}

	if ( pClipFlags )
		*pClipFlags = ClipFlags;

   return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsToPlaneXYZUV
//	Clips X, Y, Z, u, v
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUV(	const grPlane *pPlane, 
											const grLVertex *pIn, grLVertex *pOut,
											int32 NumVerts, int32 *OutVerts)
{
    int32			i, CurIn, NextIn;
    float			CurDot, NextDot, Scale;
    const grLVertex	*pIn2, *pNext;
	grLVertex		*pOut2;
	const grVec3d	*pNormal;

    pIn2 = pIn;
    pOut2= pOut;
	pNormal = &pPlane->Normal;

	CurDot = (pIn->X * pNormal->X) + (pIn->Y * pNormal->Y) + (pIn->Z * pNormal->Z);
    CurIn = (CurDot >= pPlane->Dist);

    for (i=0 ; i<NumVerts ; i++)
    {
		pNext = ((i+1) < NumVerts) ? (pIn2+1): pIn;

        // Keep the current vertex if it's inside the plane
        if (CurIn) 
		{
            pOut2->X = pIn2->X;
            pOut2->Y = pIn2->Y;
            pOut2->Z = pIn2->Z;
            pOut2->u = pIn2->u;
            pOut2->v = pIn2->v;
			pOut2++;
		}

		NextDot = (pNext->X * pNormal->X) + (pNext->Y * pNormal->Y) + (pNext->Z * pNormal->Z);
		NextIn = (NextDot >= pPlane->Dist);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (CurIn != NextIn)
        {
			Scale = (pPlane->Dist - CurDot) / (NextDot - CurDot);

            pOut2->X = pIn2->X + (pNext->X - pIn2->X) * Scale;
            pOut2->Y = pIn2->Y + (pNext->Y - pIn2->Y) * Scale;
            pOut2->Z = pIn2->Z + (pNext->Z - pIn2->Z) * Scale;

            pOut2->u = pIn2->u + (pNext->u - pIn2->u) * Scale;
            pOut2->v = pIn2->v + (pNext->v - pIn2->v) * Scale;

            pOut2++;
        }

        CurDot = NextDot;
        CurIn = NextIn;
        pIn2++;
    }

    *OutVerts = pOut2 - pOut;

    if (*OutVerts < 3)
        return GR_FALSE;

    return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsToPlaneXYZUVRGB
//	Clips X, Y, Z, u, v
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGB(	const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts)
{
    int32			i, CurIn, NextIn;
    float			CurDot, NextDot, Scale;
    const grLVertex	*pIn2, *pNext;
	grLVertex		*pOut2;
	const grVec3d	*pNormal;

    pIn2 = pIn;
    pOut2= pOut;
	pNormal = &pPlane->Normal;

	CurDot = (pIn->X * pNormal->X) + (pIn->Y * pNormal->Y) + (pIn->Z * pNormal->Z);
    CurIn = (CurDot >= pPlane->Dist);

    for (i=0 ; i<NumVerts ; i++)
    {
		pNext = ((i+1) < NumVerts) ? (pIn2+1): pIn;

        // Keep the current vertex if it's inside the plane
        if (CurIn) 
		{
            memcpy(pOut2, pIn2, sizeof(grLVertex));
			pOut2++;
		}

		NextDot = (pNext->X * pNormal->X) + (pNext->Y * pNormal->Y) + (pNext->Z * pNormal->Z);
		NextIn = (NextDot >= pPlane->Dist);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (CurIn != NextIn)
        {
			Scale = (pPlane->Dist - CurDot) / (NextDot - CurDot);

            pOut2->X = pIn2->X + (pNext->X - pIn2->X) * Scale;
            pOut2->Y = pIn2->Y + (pNext->Y - pIn2->Y) * Scale;
            pOut2->Z = pIn2->Z + (pNext->Z - pIn2->Z) * Scale;

            pOut2->u = pIn2->u + (pNext->u - pIn2->u) * Scale;
            pOut2->v = pIn2->v + (pNext->v - pIn2->v) * Scale;

            pOut2->r = pIn2->r + (pNext->r - pIn2->r) * Scale;
            pOut2->g = pIn2->g + (pNext->g - pIn2->g) * Scale;
            pOut2->b = pIn2->b + (pNext->b - pIn2->b) * Scale;

            pOut2++;
        }

        CurDot = NextDot;
        CurIn = NextIn;
        pIn2++;
    }

    *OutVerts = pOut2 - pOut;

    if (*OutVerts < 3)
        return GR_FALSE;

    return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsToPlaneXYZUVRGBA
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGBA(	const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts)
{
    int32			i, CurIn, NextIn;
    float			CurDot, NextDot, Scale;
    const grLVertex	*pIn2, *pNext;
	grLVertex		*pOut2;
	const grVec3d	*pNormal;

    pIn2 = pIn;
    pOut2= pOut;
	pNormal = &pPlane->Normal;

	CurDot = (pIn->X * pNormal->X) + (pIn->Y * pNormal->Y) + (pIn->Z * pNormal->Z);
    CurIn = (CurDot >= pPlane->Dist);

    for (i=0 ; i<NumVerts ; i++)
    {
		pNext = ((i+1) < NumVerts) ? (pIn2+1): pIn;

        // Keep the current vertex if it's inside the plane
        if (CurIn) 
		{
            *pOut2 = *pIn2;
			pOut2++;
		}

		NextDot = (pNext->X * pNormal->X) + (pNext->Y * pNormal->Y) + (pNext->Z * pNormal->Z);
		NextIn = (NextDot >= pPlane->Dist);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (CurIn != NextIn)
        {
			Scale = (pPlane->Dist - CurDot) / (NextDot - CurDot);

            pOut2->X = pIn2->X + (pNext->X - pIn2->X) * Scale;
            pOut2->Y = pIn2->Y + (pNext->Y - pIn2->Y) * Scale;
            pOut2->Z = pIn2->Z + (pNext->Z - pIn2->Z) * Scale;

            pOut2->u = pIn2->u + (pNext->u - pIn2->u) * Scale;
            pOut2->v = pIn2->v + (pNext->v - pIn2->v) * Scale;

            pOut2->r = pIn2->r + (pNext->r - pIn2->r) * Scale;
            pOut2->g = pIn2->g + (pNext->g - pIn2->g) * Scale;
            pOut2->b = pIn2->b + (pNext->b - pIn2->b) * Scale;
            pOut2->a = pIn2->a + (pNext->a - pIn2->a) * Scale;

            pOut2++;
        }

        CurDot = NextDot;
        CurIn = NextIn;
        pIn2++;
    }

    *OutVerts = pOut2 - pOut;

    if (*OutVerts < 3)
        return GR_FALSE;

    return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsToPlaneXYZUVRGBAS
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGBAS(const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts)
{
    int32			i, CurIn, NextIn;
    float			CurDot, NextDot, Scale;
    const grLVertex	*pIn2, *pNext;
	grLVertex		*pOut2;
	const grVec3d	*pNormal;

    pIn2 = pIn;
    pOut2= pOut;
	pNormal = &pPlane->Normal;

	CurDot = (pIn->X * pNormal->X) + (pIn->Y * pNormal->Y) + (pIn->Z * pNormal->Z);
    CurIn = (CurDot >= pPlane->Dist);

    for (i=0 ; i<NumVerts ; i++)
    {
		pNext = ((i+1) < NumVerts) ? (pIn2+1): pIn;

        // Keep the current vertex if it's inside the plane
        if (CurIn) 
		{
            *pOut2 = *pIn2;
			pOut2++;
		}

		NextDot = (pNext->X * pNormal->X) + (pNext->Y * pNormal->Y) + (pNext->Z * pNormal->Z);
		NextIn = (NextDot >= pPlane->Dist);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (CurIn != NextIn)
        {
			Scale = (pPlane->Dist - CurDot) / (NextDot - CurDot);

            pOut2->X = pIn2->X + (pNext->X - pIn2->X) * Scale;
            pOut2->Y = pIn2->Y + (pNext->Y - pIn2->Y) * Scale;
            pOut2->Z = pIn2->Z + (pNext->Z - pIn2->Z) * Scale;

            pOut2->u = pIn2->u + (pNext->u - pIn2->u) * Scale;
            pOut2->v = pIn2->v + (pNext->v - pIn2->v) * Scale;

            pOut2->r = pIn2->r + (pNext->r - pIn2->r) * Scale;
            pOut2->g = pIn2->g + (pNext->g - pIn2->g) * Scale;
            pOut2->b = pIn2->b + (pNext->b - pIn2->b) * Scale;
            pOut2->a = pIn2->a + (pNext->a - pIn2->a) * Scale;

            pOut2->sr = pIn2->sr + (pNext->sr - pIn2->sr) * Scale;
            pOut2->sg = pIn2->sg + (pNext->sg - pIn2->sg) * Scale;
            pOut2->sb = pIn2->sb + (pNext->sb - pIn2->sb) * Scale;

            pOut2++;
        }

        CurDot = NextDot;
        CurIn = NextIn;
        pIn2++;
    }

    *OutVerts = pOut2 - pOut;

    if (*OutVerts < 3)
        return GR_FALSE;

    return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsXYZUV
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUV(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo)
{
	grLVertex		*pSrc, *pDst;
	int32			NumVerts, i;
	const grPlane	*pPlane;
	uint32			ClipFlags;

	assert(Frustum);
	assert(ClipInfo);
	assert(ClipInfo->Work2 != ClipInfo->SrcVerts);

	ClipFlags = ClipInfo->ClipFlags;

	if (!ClipFlags)		// Early out if possible
	{
		ClipInfo->NumDstVerts = ClipInfo->NumSrcVerts;
		ClipInfo->DstVerts = (grLVertex*)ClipInfo->SrcVerts;
		return GR_TRUE;
	}

	pSrc = (grLVertex*)ClipInfo->SrcVerts;
	pDst = ClipInfo->Work2;

	NumVerts = ClipInfo->NumSrcVerts;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (!(ClipFlags & (1<<i)))
			continue;

		if (!grFrustum_ClipLVertsToPlaneXYZUV(pPlane, pSrc, pDst, NumVerts, &NumVerts))
			return GR_FALSE;

		if (pDst == ClipInfo->Work2)
		{
			pSrc = ClipInfo->Work2;
			pDst = ClipInfo->Work1;
		}
		else
		{
			pSrc = ClipInfo->Work1;
			pDst = ClipInfo->Work2;
		}
	}

	ClipInfo->NumDstVerts = NumVerts;
	ClipInfo->DstVerts = pSrc;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsXYZUVRGB
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGB(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo)
{
	grLVertex		*pSrc, *pDst;
	int32			NumVerts, i;
	const grPlane	*pPlane;
	uint32			ClipFlags;

	assert(Frustum);
	assert(ClipInfo);
	assert(ClipInfo->Work2 != ClipInfo->SrcVerts);

	ClipFlags = ClipInfo->ClipFlags;

	if (!ClipFlags)		// Early out if possible
	{
		ClipInfo->NumDstVerts = ClipInfo->NumSrcVerts;
		ClipInfo->DstVerts = (grLVertex*)ClipInfo->SrcVerts;
		return GR_TRUE;
	}

	pSrc = (grLVertex*)ClipInfo->SrcVerts;
	pDst = ClipInfo->Work2;

	NumVerts = ClipInfo->NumSrcVerts;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (!(ClipFlags & (1<<i)))
			continue;

		if (!grFrustum_ClipLVertsToPlaneXYZUVRGB(pPlane, pSrc, pDst, NumVerts, &NumVerts))
			return GR_FALSE;

		if (pDst == ClipInfo->Work2)
		{
			pSrc = ClipInfo->Work2;
			pDst = ClipInfo->Work1;
		}
		else
		{
			pSrc = ClipInfo->Work1;
			pDst = ClipInfo->Work2;
		}
	}

	ClipInfo->NumDstVerts = NumVerts;
	ClipInfo->DstVerts = pSrc;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsXYZUVRGBA
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGBA(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo)
{
	grLVertex		*pSrc, *pDst;
	int32			NumVerts, i;
	const grPlane	*pPlane;
	uint32			ClipFlags;

	assert(Frustum);
	assert(ClipInfo);
	assert(ClipInfo->Work2 != ClipInfo->SrcVerts);

	ClipFlags = ClipInfo->ClipFlags;

	if (!ClipFlags)		// Early out if possible
	{
		ClipInfo->NumDstVerts = ClipInfo->NumSrcVerts;
		ClipInfo->DstVerts = (grLVertex*)ClipInfo->SrcVerts;
		return GR_TRUE;
	}

	pSrc = (grLVertex*)ClipInfo->SrcVerts;
	pDst = ClipInfo->Work2;

	NumVerts = ClipInfo->NumSrcVerts;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (!(ClipFlags & (1<<i)))
			continue;

		if (!grFrustum_ClipLVertsToPlaneXYZUVRGBA(pPlane, pSrc, pDst, NumVerts, &NumVerts))
			return GR_FALSE;

		if (pDst == ClipInfo->Work2)
		{
			pSrc = ClipInfo->Work2;
			pDst = ClipInfo->Work1;
		}
		else
		{
			pSrc = ClipInfo->Work1;
			pDst = ClipInfo->Work2;
		}
	}

	ClipInfo->NumDstVerts = NumVerts;
	ClipInfo->DstVerts = pSrc;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipLVertsXYZUVRGBAS
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGBAS(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo)
{
	grLVertex		*pSrc, *pDst;
	int32			NumVerts, i;
	const grPlane	*pPlane;
	uint32			ClipFlags;

	assert(Frustum);
	assert(ClipInfo);
	assert(ClipInfo->Work2 != ClipInfo->SrcVerts);

	ClipFlags = ClipInfo->ClipFlags;

	if (!ClipFlags)		// Early out if possible
	{
		ClipInfo->NumDstVerts = ClipInfo->NumSrcVerts;
		ClipInfo->DstVerts = (grLVertex*)ClipInfo->SrcVerts;
		return GR_TRUE;
	}

	pSrc = (grLVertex*)ClipInfo->SrcVerts;
	pDst = ClipInfo->Work2;

	NumVerts = ClipInfo->NumSrcVerts;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (!(ClipFlags & (1<<i)))
			continue;

		if (!grFrustum_ClipLVertsToPlaneXYZUVRGBAS(pPlane, pSrc, pDst, NumVerts, &NumVerts))
			return GR_FALSE;

		if (pDst == ClipInfo->Work2)
		{
			pSrc = ClipInfo->Work2;
			pDst = ClipInfo->Work1;
		}
		else
		{
			pSrc = ClipInfo->Work1;
			pDst = ClipInfo->Work2;
		}
	}

	ClipInfo->NumDstVerts = NumVerts;
	ClipInfo->DstVerts = pSrc;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipVertsToPlane
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipVertsToPlane(	const grPlane *pPlane, 
										const grVec3d *pIn, grVec3d *pOut,
										int32 NumVerts, int32 *OutVerts)
{
    int32			i, CurIn, NextIn;
    float			CurDot, NextDot, Scale;
    const grVec3d	*pIn2, *pNext;
	grVec3d			*pOut2;
	const grVec3d	*pNormal;

    pIn2 = pIn;
    pOut2= pOut;
	pNormal = &pPlane->Normal;

	CurDot = (pIn->X * pNormal->X) + (pIn->Y * pNormal->Y) + (pIn->Z * pNormal->Z);
    CurIn = (CurDot >= pPlane->Dist);

    for (i=0 ; i<NumVerts ; i++)
    {
		pNext = ((i+1) < NumVerts) ? (pIn2+1): pIn;

        // Keep the current vertex if it's inside the plane
        if (CurIn) 
		{
            pOut2->X = pIn2->X;
            pOut2->Y = pIn2->Y;
            pOut2->Z = pIn2->Z;
			pOut2++;
		}

		NextDot = (pNext->X * pNormal->X) + (pNext->Y * pNormal->Y) + (pNext->Z * pNormal->Z);
		NextIn = (NextDot >= pPlane->Dist);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (CurIn != NextIn)
        {
			Scale = (pPlane->Dist - CurDot) / (NextDot - CurDot);

            pOut2->X = pIn2->X + (pNext->X - pIn2->X) * Scale;
            pOut2->Y = pIn2->Y + (pNext->Y - pIn2->Y) * Scale;
            pOut2->Z = pIn2->Z + (pNext->Z - pIn2->Z) * Scale;

            pOut2++;
        }

        CurDot = NextDot;
        CurIn = NextIn;
        pIn2++;
    }

    *OutVerts = pOut2 - pOut;

    if (*OutVerts < 3)
        return GR_FALSE;

    return GR_TRUE;
}

//================================================================================
//	grFrustum_ClipVerts
//================================================================================
GRAPI grBoolean GRCC grFrustum_ClipVerts(const grFrustum *Frustum, grFrustum_ClipInfo *ClipInfo)
{
	grVec3d			*pSrc, *pDst;
	int32			i, NumVerts;
	const grPlane	*pPlane;

	assert(Frustum);
	assert(ClipInfo);
	assert(ClipInfo->Work2 != ClipInfo->SrcVerts);

	if (!ClipInfo->ClipFlags)		// Early out if possible
	{
		ClipInfo->NumDstVerts = ClipInfo->NumSrcVerts;
		ClipInfo->DstVerts = (grVec3d*)ClipInfo->SrcVerts;
		return GR_TRUE;
	}

	pSrc = (grVec3d*)ClipInfo->SrcVerts;
	pDst = ClipInfo->Work2;

	NumVerts = ClipInfo->NumSrcVerts;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (!(ClipInfo->ClipFlags & (1<<i)))
			continue;

		if (!grFrustum_ClipVertsToPlane(pPlane, pSrc, pDst, NumVerts, &NumVerts))
			return GR_FALSE;

		if (pDst == ClipInfo->Work2)
		{
			pSrc = ClipInfo->Work2;
			pDst = ClipInfo->Work1;
		}
		else
		{
			pSrc = ClipInfo->Work1;
			pDst = ClipInfo->Work2;
		}
	}

	ClipInfo->NumDstVerts = NumVerts;
	ClipInfo->DstVerts = pSrc;

	return GR_TRUE;
}

//================================================================================
//	grFrustum_PointCollision
//================================================================================
GRAPI grBoolean GRCC grFrustum_PointCollision(const grFrustum *Frustum, const grVec3d *Point, grFloat Radius)
{
	int32			i;
	const grPlane	*pPlane;

	for (pPlane = Frustum->Planes, i=0; i< Frustum->NumPlanes; i++, pPlane++)
	{
		if (grPlane_PointDistance(pPlane, Point) < -Radius)
			return GR_FALSE;		// Point behind plane (outside of frustum)
	}	

	return GR_TRUE;
}
