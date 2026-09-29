/****************************************************************************************/
/*  JEUSERPOLY.C                                                                        */
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
#include <string.h>

// Public Dependents
#include "grUserPoly.h"

// Private dependents
#include "Ram.h"

#include "Dcommon.h"
#include "Camera._h"

#include "grMaterial.h"

#pragma message ("All the mallocs should eventually use grMemPool...")

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define GR_USERPOLY_MAX_REFCOUNTS			(0xffffffff>>1)

// We should eventually make structures for each kind, and use grUserPoly as an interface to it
typedef struct grUserPoly
{
	// Info about the poly
	grUserPoly_Type		Type;
	uint32				Flags;
	const grMaterialSpec* Material;

	// The verts
	grLVertex			Verts[4];
	grFloat				Scale;				// Scale of sprite

	int32				RefCount;
} grUserPoly;

//========================================================================================
//	Local statics
//========================================================================================
static grBoolean RenderQuad(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);
static grBoolean RenderTri(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);
static grBoolean RenderSprite(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);
static grBoolean RenderLine(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);
//========================================================================================
//	grUserPoly_CreateTri
//	Create a user poly of type tri
//  Vizard/Void/Jeff Muizelaar
//========================================================================================
GRAPI grUserPoly * GRCC grUserPoly_CreateTri(	const grLVertex		*v1, 
												const grLVertex		*v2, 
												const grLVertex		*v3, 
												const grMaterialSpec	*Material,
												uint32				Flags)
{
	grUserPoly		*Tri;

	assert(v1);
	assert(v2);
	assert(v3);

	Tri = GR_RAM_ALLOCATE_STRUCT(grUserPoly);
	//We are allocating enough space for 4 vertexes instead of the 3 we need for a tri
	//Don't know how this would be remedied
	if (!Tri)
		return NULL;

	// Clear the memory
	ZeroMem(Tri);

	Tri->RefCount = 1;
	
	// Set the type
	Tri->Type = Type_Tri;

	Tri->Material = Material;

	Tri->Flags = Flags;

	// Copy the verts
	Tri->Verts[0] = *v1;
	Tri->Verts[1] = *v2;
	Tri->Verts[2] = *v3;

	return Tri;
}

//========================================================================================
//	grUserPoly_CreateQuad
//	Create a user poly of type quad
//========================================================================================
GRAPI grUserPoly * GRCC grUserPoly_CreateQuad(	const grLVertex		*v1, 
												const grLVertex		*v2, 
												const grLVertex		*v3, 
												const grLVertex		*v4, 
												const grMaterialSpec *Material,
												uint32				Flags)
{
	grUserPoly		*Quad;

	assert(v1);
	assert(v2);
	assert(v3);
	assert(v4);

	Quad = GR_RAM_ALLOCATE_STRUCT(grUserPoly);

	if (!Quad)
		return NULL;

	// Clear the memory
	ZeroMem(Quad);

	Quad->RefCount = 1;
	
	// Set the type
	Quad->Type = Type_Quad;

	Quad->Material = Material;

	Quad->Flags = Flags;

	// Copy the verts
	Quad->Verts[0] = *v1;
	Quad->Verts[1] = *v2;
	Quad->Verts[2] = *v3;
	Quad->Verts[3] = *v4;

	return Quad;
}

//========================================================================================
//	grUserPoly_CreateSprite
//	Create a user poly of type Sprite
//========================================================================================
GRAPI grUserPoly * GRCC grUserPoly_CreateSprite(	const grLVertex		*v1, 
												const grMaterialSpec *Material,
												grFloat				Scale,
												uint32				Flags)
{
	grUserPoly		*Sprite;

	assert(v1);
	assert(Material);		// For now, you need a bitmap

	Sprite = GR_RAM_ALLOCATE_STRUCT(grUserPoly);

	if (!Sprite)
		return NULL;

	// Clear the memory
	ZeroMem(Sprite);
	
	Sprite->RefCount = 1;

	// Set the type
	Sprite->Type = Type_Sprite;

	Sprite->Material = Material;

	Sprite->Scale = Scale;

	Sprite->Flags = Flags;

	// Copy the vert that will be the center of the sprite
	Sprite->Verts[0] = *v1;

	return Sprite;
}

//========================================================================================
//	grUserPoly_CreateLine
//========================================================================================
GRAPI grUserPoly * GRCC grUserPoly_CreateLine(const grLVertex *v1, const grLVertex *v2, grFloat Scale, uint32 Flags)
{
	grUserPoly		*Line;

	assert(v1);
	assert(v2);

	Line = GR_RAM_ALLOCATE_STRUCT(grUserPoly);

	if (!Line)
		return NULL;

	// Clear the memory
	ZeroMem(Line);
	
	Line->RefCount = 1;

	// Set the type
	Line->Type = Type_Line;

	Line->Scale = Scale;

	Line->Flags = Flags;

	Line->Verts[0] = *v1;
	Line->Verts[1] = *v2;

	return Line;
}

//========================================================================================
//	grUserPoly_IsValid
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_IsValid(const grUserPoly *Poly)
{
	if (!Poly)
		return GR_FALSE;

	if (Poly->RefCount <= 0)		// At least 1 person must be referencing the object
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_CreateRef(grUserPoly *Poly)
{
	assert(grUserPoly_IsValid(Poly) == GR_TRUE);

	if (Poly->RefCount >= GR_USERPOLY_MAX_REFCOUNTS)
	{
		assert(0);	// I know this is bad, but it is a common mistake to ignore the return values of grObject_CreateRef!!!!
		return GR_FALSE;
	}

	Poly->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_Destroy
//========================================================================================
GRAPI void GRCC grUserPoly_Destroy(grUserPoly **Poly)
{
	assert(Poly);
	assert(grUserPoly_IsValid(*Poly) == GR_TRUE);

	(*Poly)->RefCount--;

	if ((*Poly)->RefCount == 0)
		grRam_Free(*Poly);

	*Poly = NULL;
}

//========================================================================================
//	grUserPoly_UpdateTri
//  Vizard/Void/Jeff Muizelaar
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_UpdateTri(	grUserPoly *Poly, 
											const grLVertex *v1, 
											const grLVertex *v2, 
											const grLVertex *v3, 
											const grMaterialSpec *Material)
{
	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Poly->Type == Type_Tri);
	assert(v1);
	assert(v2);
	assert(v3);

	Poly->Verts[0] = *v1;
	Poly->Verts[1] = *v2;
	Poly->Verts[2] = *v3;

	Poly->Material = Material;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_UpdateQuad
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_UpdateQuad(	grUserPoly *Poly, 
											const grLVertex *v1, 
											const grLVertex *v2, 
											const grLVertex *v3, 
											const grLVertex *v4, 
											const grMaterialSpec *Material)
{
	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Poly->Type == Type_Quad);
	assert(v1);
	assert(v2);
	assert(v3);
	assert(v4);

	Poly->Verts[0] = *v1;
	Poly->Verts[1] = *v2;
	Poly->Verts[2] = *v3;
	Poly->Verts[3] = *v4;

	Poly->Material = Material;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_UpdateSprite
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_UpdateSprite(grUserPoly *Poly, const grLVertex *v1, const grMaterialSpec *Material, grFloat Scale)
{

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Poly->Type == Type_Sprite);
	assert(v1);

	Poly->Verts[0] = *v1;
	Poly->Material = Material;
	Poly->Scale = Scale;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_UpdateLine
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_UpdateLine(grUserPoly *Poly, const grLVertex *v1, const grLVertex *v2, grFloat Scale)
{

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Poly->Type == Type_Line);
	assert(v1);

	Poly->Verts[0] = *v1;
	Poly->Verts[1] = *v2;
	Poly->Scale = Scale;

	return GR_TRUE;
}

//========================================================================================
//	grUserPoly_Render
//========================================================================================
GRAPI grBoolean GRCC grUserPoly_Render(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Engine);
	assert(Camera);
	assert(Frustum);

	switch (Poly->Type)
	{
		//Vizard/Void/Jeff Muizelaar
		case Type_Tri:
			return RenderTri(Poly, Engine, Camera, Frustum);

		case Type_Quad:
			return RenderQuad(Poly, Engine, Camera, Frustum);

		case Type_Sprite:
			return RenderSprite(Poly, Engine, Camera, Frustum);

		case Type_Line:
			return RenderLine(Poly, Engine, Camera, Frustum);

		default:
			assert(0);		// Illegal!!!
	}

	return GR_TRUE;
}

// As long as the poly coming in does not have more planes than the frustum, then the buffer only needs
//	to be as big as the number of clip planes*2
#define GR_USERPOLY_MAX_CLIPPLANES		(GR_FRUSTUM_MAX_PLANES) 
#define GR_USERPOLY_MAX_CLIPVERTS		(GR_USERPOLY_MAX_CLIPPLANES*2)

//========================================================================================
//	RenderTri
//  Vizard/Void/Jeff Muizelaar
//========================================================================================
static grBoolean RenderTri(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	grFrustum_LClipInfo		ClipInfo;
	grLVertex				Work1[GR_USERPOLY_MAX_CLIPVERTS], Work2[GR_USERPOLY_MAX_CLIPVERTS];
	grTLVertex				TLVerts[GR_USERPOLY_MAX_CLIPVERTS];

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Engine);
	assert(Camera);
	assert(Frustum);
	assert(Poly->Type == Type_Tri);
	assert(Frustum->NumPlanes <= GR_USERPOLY_MAX_CLIPPLANES);

	ClipInfo.SrcVerts = Poly->Verts;
	ClipInfo.NumSrcVerts = 3;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;
	ClipInfo.ClipFlags = 0xffff;

	// Clip the verts against the frustum
	// Clip UV, and RGB
	if (!grFrustum_ClipLVertsXYZUVRGBA(Frustum, &ClipInfo))
		return GR_TRUE;		// Poly was clipped away
	
	// Transform and project the poly
	grCamera_TransformAndProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	// Render it
	grEngine_RenderPoly(Engine, TLVerts, ClipInfo.NumDstVerts, Poly->Material, Poly->Flags);

	return GR_TRUE;
}

//========================================================================================
//	RenderQuad
//========================================================================================
static grBoolean RenderQuad(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	grFrustum_LClipInfo		ClipInfo;
	grLVertex				Work1[GR_USERPOLY_MAX_CLIPVERTS], Work2[GR_USERPOLY_MAX_CLIPVERTS];
	grTLVertex				TLVerts[GR_USERPOLY_MAX_CLIPVERTS];

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Engine);
	assert(Camera);
	assert(Frustum);
	assert(Poly->Type == Type_Quad);
	assert(Frustum->NumPlanes <= GR_USERPOLY_MAX_CLIPPLANES);

	ClipInfo.SrcVerts = Poly->Verts;
	ClipInfo.NumSrcVerts = 4;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;
	ClipInfo.ClipFlags = 0xffff;

	// Clip the verts against the frustum
	// Clip UV, and RGB
	if (!grFrustum_ClipLVertsXYZUVRGBA(Frustum, &ClipInfo))
		return GR_TRUE;		// Poly was clipped away
	
	// Transform and project the poly
	grCamera_TransformAndProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	// Render it
	grEngine_RenderPoly(Engine, TLVerts, ClipInfo.NumDstVerts, Poly->Material, Poly->Flags);

	return GR_TRUE;
}

//========================================================================================
//	RenderSprite
//========================================================================================
static grBoolean RenderSprite(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	grFrustum_LClipInfo		ClipInfo;
	grLVertex				Work1[GR_USERPOLY_MAX_CLIPVERTS], Work2[GR_USERPOLY_MAX_CLIPVERTS];
	grTLVertex				TLVerts[GR_USERPOLY_MAX_CLIPVERTS];
	grLVertex				*pVerts;
	grVec3d					Up, Left, Start;
	grFloat					XScale, YScale, UShift, VShift;
	const grXForm3d			*MXForm;

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Engine);
	assert(Camera);
	assert(Frustum);
	assert(Poly->Type == Type_Sprite);
	assert(Frustum->NumPlanes <= GR_USERPOLY_MAX_CLIPPLANES);

	pVerts = Work1;

	pVerts[0] = pVerts[1] = pVerts[2] = pVerts[3] = Poly->Verts[0];

	UShift = pVerts[0].u;
	VShift = pVerts[0].v;

	Start.X = pVerts[0].X;
	Start.Y = pVerts[0].Y;
	Start.Z = pVerts[0].Z;

	MXForm = grCamera_WorldXForm(Camera);

	grXForm3d_GetLeft(MXForm, &Left);
	grXForm3d_GetUp(MXForm, &Up);

	XScale = (float)grMaterialSpec_Width(Poly->Material) * Poly->Scale;
	YScale = (float)grMaterialSpec_Height(Poly->Material) * Poly->Scale;

	grVec3d_Scale(&Left, XScale*0.2f, &Left);
	grVec3d_Scale(&Up, YScale*0.2f, &Up);

	pVerts->X = Start.X + Left.X + Up.X;
	pVerts->Y = Start.Y + Left.Y + Up.Y;
	pVerts->Z = Start.Z + Left.Z + Up.Z;
	pVerts->u = 0.0f + UShift;
	pVerts->v = 0.0f + VShift;

	pVerts++;
	
	pVerts->X = Start.X - Left.X + Up.X;
	pVerts->Y = Start.Y - Left.Y + Up.Y;
	pVerts->Z = Start.Z - Left.Z + Up.Z;
	pVerts->u = 1.0f + UShift;
	pVerts->v = 0.0f + VShift;
	
	pVerts++;
	
	pVerts->X = Start.X - Left.X - Up.X;
	pVerts->Y = Start.Y - Left.Y - Up.Y;
	pVerts->Z = Start.Z - Left.Z - Up.Z;
	pVerts->u = 1.0f + UShift;
	pVerts->v = 1.0f + VShift;

	pVerts++;
	
	pVerts->X = Start.X + Left.X - Up.X;
	pVerts->Y = Start.Y + Left.Y - Up.Y;
	pVerts->Z = Start.Z + Left.Z - Up.Z;
	pVerts->u = 0.0f + UShift;
	pVerts->v = 1.0f + VShift;

	//  Setup ClipInfo
	ClipInfo.SrcVerts = Work1;
	ClipInfo.NumSrcVerts = 4;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;
	ClipInfo.ClipFlags = 0xffff;

	// Clip the verts against the frustum
	// Clip UV, and RGBA
	if (!grFrustum_ClipLVertsXYZUVRGBA(Frustum, &ClipInfo))
		return GR_TRUE;		// Poly was clipped away
	
	// Transform and project the poly
	grCamera_TransformAndProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	// Render it
	grEngine_RenderPoly(Engine, TLVerts, ClipInfo.NumDstVerts, Poly->Material, Poly->Flags);

	return GR_TRUE;
}

//========================================================================================
//	RenderLine
//========================================================================================
static grBoolean RenderLine(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum)
{
	grFrustum_LClipInfo		ClipInfo;
	grLVertex				Work1[GR_USERPOLY_MAX_CLIPVERTS], Work2[GR_USERPOLY_MAX_CLIPVERTS];
	const grLVertex			*v1, *v2;
	grTLVertex				TLVerts[GR_USERPOLY_MAX_CLIPVERTS];
	grLVertex				*pVerts;
	grVec3d					Left;
	const grXForm3d			*MXForm;

	assert(grUserPoly_IsValid(Poly) == GR_TRUE);
	assert(Engine);
	assert(Camera);
	assert(Frustum);
	assert(Poly->Type == Type_Line);
	assert(Frustum->NumPlanes <= GR_USERPOLY_MAX_CLIPPLANES);

	// Get camera info
	MXForm = grCamera_WorldXForm(Camera);
	grXForm3d_GetLeft(MXForm, &Left);

	v1 = &Poly->Verts[0];
	v2 = &Poly->Verts[1];

	pVerts = Work1;
	pVerts[0] = pVerts[1] = *v1;
	pVerts[2] = pVerts[3] = *v2;

	grVec3d_Scale(&Left, Poly->Scale, &Left);

	if (v2->Y < v1->Y)
		grVec3d_Inverse(&Left);

	pVerts->X = v1->X + Left.X;
	pVerts->Y = v1->Y + Left.Y;
	pVerts->Z = v1->Z + Left.Z;

	pVerts++;
	
	pVerts->X = v1->X - Left.X;
	pVerts->Y = v1->Y - Left.Y;
	pVerts->Z = v1->Z - Left.Z;
	
	pVerts++;

	pVerts->X = v2->X - Left.X;
	pVerts->Y = v2->Y - Left.Y;
	pVerts->Z = v2->Z - Left.Z;

	pVerts++;
	
	pVerts->X = v2->X + Left.X;
	pVerts->Y = v2->Y + Left.Y;
	pVerts->Z = v2->Z + Left.Z;

	//  Setup ClipInfo
	ClipInfo.SrcVerts = Work1;
	ClipInfo.NumSrcVerts = 4;
	ClipInfo.Work1 = Work1;
	ClipInfo.Work2 = Work2;
	ClipInfo.ClipFlags = 0xffff;

	// Clip the verts against the frustum
	// Clip UV, and RGB
	if (!grFrustum_ClipLVertsXYZUVRGB(Frustum, &ClipInfo))
		return GR_TRUE;		// Poly was clipped away
	
	// Transform and project the poly
	grCamera_TransformAndProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

	// Render it
	grEngine_RenderPoly(Engine, TLVerts, ClipInfo.NumDstVerts, NULL, Poly->Flags);

	return GR_TRUE;
}
