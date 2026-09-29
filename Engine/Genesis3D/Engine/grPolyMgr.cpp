/****************************************************************************************/
/*  JEPOLYMGR.C                                                                         */
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
#include <stdlib.h>		// QSort

// Public Dependents
#include "Dcommon.h"
#include "grPolyMgr.h"

// Private dependents
#include "Ram.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define GR_POLYMGR_MAX_REFCOUNTS			(0xffffffff>>1)

#define GR_POLYMGR_MAX_BATCHED_POLYS			256		// Max polys before an auto-flush is performed
#define GR_POLYMGR_MAX_BATCHED_VERTS			256		// Max verts before an auto-flush is performed

#define		Type_Gouraud			1
#define		Type_Misc				2
#define		Type_World				3
#define		Type_WorldWithLightmap	4

typedef struct grPolyMgr_Poly
{
	uint8				Type;

	int32				NumVerts;
	grTLVertex			*Verts;

	grFloat				CenterZ;

	grRDriver_Layer		Layers[2];
	int32				NumLayers;

	void				*LMapCBContext;

	uint32				Flags;
} grPolyMgr_Poly;

typedef struct grPolyMgr
{
	int32			TotalPolys;			// Num polys in batch
	int32			TotalVerts;			// Num verts of all polys

	grPolyMgr_Poly	Polys[GR_POLYMGR_MAX_BATCHED_POLYS];
	grTLVertex		Verts[GR_POLYMGR_MAX_BATCHED_VERTS];

	grPolyMgr_Poly	*SortedPolys[GR_POLYMGR_MAX_BATCHED_POLYS];

	int32		RefCount;
} grPolyMgr;

//========================================================================================
//	Local statics
//========================================================================================
static DRV_Driver			*g_Driver = NULL;

//========================================================================================
//	grPolyMgr_Create
//========================================================================================
grPolyMgr *grPolyMgr_Create(void)
{
	grPolyMgr	*PolyMgr;
	
	PolyMgr = GR_RAM_ALLOCATE_STRUCT(grPolyMgr);

	if (!PolyMgr)
		return NULL;

	ZeroMem(PolyMgr);

	PolyMgr->RefCount = 1;

	return PolyMgr;
}

//========================================================================================
//	grPolyMgr_IsValid
//========================================================================================
grBoolean grPolyMgr_IsValid(const grPolyMgr *Mgr)
{
	if (!Mgr)
		return GR_FALSE;

	if (Mgr->RefCount <= 0)		// At least 1 person must be referencing the object
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grPolyMgr_CreateRef
//========================================================================================
grBoolean grPolyMgr_CreateRef(grPolyMgr *Mgr)
{
	assert(grPolyMgr_IsValid(Mgr) == GR_TRUE);

	if (Mgr->RefCount >= GR_POLYMGR_MAX_REFCOUNTS)
	{
		assert(0);	// I know this is bad, but it is a common mistake to ignore the return values of grObject_CreateRef!!!!
		return GR_FALSE;
	}

	Mgr->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grPolyMgr_Destroy
//========================================================================================
void grPolyMgr_Destroy(grPolyMgr **Mgr)
{
	assert(Mgr);
	assert(grPolyMgr_IsValid(*Mgr) == GR_TRUE);

	(*Mgr)->RefCount--;

	if ((*Mgr)->RefCount == 0)
		grRam_Free(*Mgr);

	*Mgr = NULL;
}

//========================================================================================
//	grPolyMgr_SetDriver
//========================================================================================
void grPolyMgr_SetDriver(grPolyMgr *Mgr, DRV_Driver *Driver)
{
	assert(Driver);

	g_Driver = Driver;
}

//========================================================================================
//	BatchPoly
//========================================================================================
static void BatchPoly(	grPolyMgr				*Mgr,
						const grTLVertex		*Verts, 
						int32					NumVerts, 
						grRDriver_Layer			*Layers,
						int32					NumLayers,
						void					*LMapCBContext,
						uint32					Flags,
						uint8					Type)
{
	grTLVertex		*pVert;
	int32			i;
	grPolyMgr_Poly	*pPoly;

	if (Mgr->TotalPolys+1 > GR_POLYMGR_MAX_BATCHED_POLYS)
		grPolyMgr_FlushBatch(Mgr);
	else if (Mgr->TotalVerts+NumVerts > GR_POLYMGR_MAX_BATCHED_VERTS)
		grPolyMgr_FlushBatch(Mgr);

	pPoly = &Mgr->Polys[Mgr->TotalPolys];

	pPoly->NumVerts = NumVerts;
	pPoly->Verts = &Mgr->Verts[Mgr->TotalVerts];
	pPoly->LMapCBContext = LMapCBContext;
	pPoly->Flags = Flags;
	pPoly->Layers[0] = Layers[0];
	pPoly->Type = Type;

	switch (Type)
	{
		case Type_WorldWithLightmap:
		{
			assert(NumLayers == 2);
			pPoly->Layers[1] = Layers[1];
			break;
		}
		case Type_World:
		{
			assert(NumLayers == 1);
		}
	}

	pVert = pPoly->Verts;

	// Find the center of the poly to use for sorting
	pPoly->CenterZ = 0.0f;

	for (i=0; i< NumVerts; i++, pVert++, Verts++)
	{
		*pVert = *Verts;
	
		pPoly->CenterZ += pVert->z;
	}

	pPoly->CenterZ /= NumVerts;

	// Update counts
	Mgr->TotalPolys++;
	Mgr->TotalVerts += NumVerts;
}

//========================================================================================
//	grPolyMgr_RenderGouraudPoly
//========================================================================================
void grPolyMgr_RenderGouraudPoly(	grPolyMgr			*Mgr, 
									const grTLVertex	*Verts, 
									int32				NumVerts, 
									uint32				Flags)
{
#ifdef USE_BATCHING
	if (Flags & (GR_RENDER_FLAG_ALPHA|GR_RENDER_FLAG_COLORKEY))		// Snag alphas/colorkeys, so we can batch and sort them
	{
		// Cache them out, so we can sort them later
		BatchPoly(Mgr, Verts, NumVerts, NULL, NULL, NULL, Flags, Type_Gouraud);
		return;
	}
#endif

	// Other polys can just go straight to the driver
	g_Driver->RenderGouraudPoly((grTLVertex*)Verts, NumVerts, Flags);
}

//========================================================================================
//	grPolyMgr_RenderMiscPoly
//========================================================================================
void grPolyMgr_RenderMiscPoly(	grPolyMgr				*Mgr, 
								const grTLVertex		*Verts, 
								int32					NumVerts, 
								grRDriver_Layer			*Layers,
								int32					NumLayers,
								uint32					Flags)
{
#ifdef USE_BATCHING
	grBoolean				HasColorKey;
	grTexture_Info	Info;

	assert(NumLayers == 1);

	g_Driver->THandle_GetInfo((grTexture*)Layers->THandle, 0, &Info);

	HasColorKey = (Info.PixelFormat.PixelFormat == GR_PIXELFORMAT_16BIT_1555_ARGB || Info.PixelFormat.PixelFormat == GR_PIXELFORMAT_16BIT_4444_ARGB);

	if (Flags & (GR_RENDER_FLAG_ALPHA|GR_RENDER_FLAG_COLORKEY) || HasColorKey)		// Snag alphas/colorkeys, so we can batch and sort them
	{
		// Cache them out, so we can sort them later
		BatchPoly(Mgr, Verts, NumVerts, Layers, NumLayers, NULL, Flags, Type_Misc);

		if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
			grPolyMgr_FlushBatch(Mgr);
		return;
	}

	if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
		grPolyMgr_FlushBatch(Mgr);
#endif

	// Other polys can just go straight to the driver
	g_Driver->RenderMiscTexturePoly((grTLVertex*)Verts, NumVerts, Layers, NumLayers, Flags);
}

//========================================================================================
//	grPolyMgr_RenderWorldPoly
//========================================================================================
void grPolyMgr_RenderWorldPoly(	grPolyMgr				*Mgr, 
								const grTLVertex		*Verts, 
								int32					NumVerts, 
								grRDriver_Layer			*Layers,
								int32					NumLayers,
								void					*LMapCBContext,
								uint32					Flags)
{
#ifdef USE_BATCHING
	if (Flags & (GR_RENDER_FLAG_ALPHA|GR_RENDER_FLAG_COLORKEY))		// Snag alphas, so we can batch and sort them
	{
		// Cache them out, so we can sort them later
		if (LInfo)
			BatchPoly(Mgr, Verts, NumVerts, THandle, TexInfo, LInfo, Flags, Type_WorldWithLightmap);
		else
			BatchPoly(Mgr, Verts, NumVerts, THandle, TexInfo, LInfo, Flags, Type_World);

		if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
			grPolyMgr_FlushBatch(Mgr);

		return;
	}
	
	if (Flags & GR_RENDER_FLAG_FLUSHBATCH)
		grPolyMgr_FlushBatch(Mgr);
#endif

	// Other polys can just go straight to the driver
	g_Driver->RenderWorldPoly((grTLVertex*)Verts, NumVerts, Layers, NumLayers, LMapCBContext, Flags);
}

//====================================================================================
//====================================================================================
static int PolyComp(const void *a, const void *b)
{
	grPolyMgr_Poly	*pPoly1, *pPoly2;

	pPoly1 = (*(grPolyMgr_Poly**)a);
	pPoly2 = (*(grPolyMgr_Poly**)b);

	// We should be working in 1/Z space
	if (pPoly1->CenterZ == pPoly2->CenterZ)		
		return 0;

	if (pPoly1->CenterZ > pPoly2->CenterZ)
		return -1;

	return 1;
}

//====================================================================================
//====================================================================================
static void SortPolys(grPolyMgr *Mgr)
{
	grPolyMgr_Poly	*pPoly;
	int32			i;

	pPoly = Mgr->Polys;

	for (i=0; i<Mgr->TotalPolys; i++, pPoly++)
		Mgr->SortedPolys[i] = pPoly;
	
	// Sort the polys
	qsort(&Mgr->SortedPolys, Mgr->TotalPolys, sizeof(Mgr->SortedPolys[0]), PolyComp);
}

//========================================================================================
//	grPolyMgr_FlushBatch
//========================================================================================
void grPolyMgr_FlushBatch(grPolyMgr *Mgr)
{
	int32			i;

	SortPolys(Mgr);

	for (i=0; i< Mgr->TotalPolys; i++)
	{
		grPolyMgr_Poly		*pPoly;
		uint32				Flags;

		pPoly = Mgr->SortedPolys[i];

		Flags = pPoly->Flags;

		if (i == 0 || i == (Mgr->TotalPolys-1))
			Flags |= GR_RENDER_FLAG_FLUSHBATCH;

		switch (pPoly->Type)
		{
			case Type_Gouraud:
				g_Driver->RenderGouraudPoly((grTLVertex*)pPoly->Verts, pPoly->NumVerts, Flags);
				break;

			case Type_Misc:
				g_Driver->RenderMiscTexturePoly((grTLVertex*)pPoly->Verts, pPoly->NumVerts, pPoly->Layers, 1, Flags);
				break;

			case Type_World:
				g_Driver->RenderWorldPoly((grTLVertex*)pPoly->Verts, pPoly->NumVerts, pPoly->Layers, 1, NULL, Flags);
				break;

			case Type_WorldWithLightmap:
				g_Driver->RenderWorldPoly((grTLVertex*)pPoly->Verts, pPoly->NumVerts, pPoly->Layers, 2, pPoly->LMapCBContext, Flags);
				break;

			default:
				assert(0);
		}
	}

	// Update counts
	Mgr->TotalPolys = 0;
	Mgr->TotalVerts = 0;
}
