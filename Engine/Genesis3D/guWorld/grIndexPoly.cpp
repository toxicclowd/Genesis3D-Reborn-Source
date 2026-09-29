/****************************************************************************************/
/*  JEINDEXPOLY.C                                                                       */
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
#include <stdio.h>

// Public dependents
#include "grIndexPoly.h"

// Private dependents
#include "Ram.h"

//=======================================================================================
//	grIndexPoly_Create
//=======================================================================================
grIndexPoly *grIndexPoly_Create(grIndexPoly_NumVertType NumVerts)
{
	grIndexPoly	*Poly;
	int32		i;

	assert(NumVerts < GR_INDEXPOLY_MAX_VERTS);

	Poly = GR_RAM_ALLOCATE_STRUCT(grIndexPoly);

	if (!Poly)
		return NULL;

	Poly->Verts = GR_RAM_ALLOCATE_ARRAY(grVertArray_Index, NumVerts);

	if (!Poly->Verts)
	{
		grRam_Free(Poly);
		return NULL;
	}

	// Invalidate all the indexes
	for (i=0; i<NumVerts; i++)
	{
		Poly->Verts[i] = GR_VERTARRAY_NULL_INDEX;
	}
	
	Poly->NumVerts = NumVerts;

	return Poly;
}

//=======================================================================================
//	grIndexPoly_CreateFromFile
//=======================================================================================
grIndexPoly *grIndexPoly_CreateFromFile(grVFile *VFile)
{
	grIndexPoly	*Poly;

	Poly = GR_RAM_ALLOCATE_STRUCT(grIndexPoly);

	if (!Poly)
		return NULL;

	// Read the number of verts
	if (!grVFile_Read(VFile, &Poly->NumVerts, sizeof(Poly->NumVerts)))
		return NULL;

	// Allocate the verts
	Poly->Verts = GR_RAM_ALLOCATE_ARRAY(grVertArray_Index, Poly->NumVerts);

	if (!Poly->Verts)
	{
		grRam_Free(Poly);
		return NULL;
	}

	// Read the verts
	if (!grVFile_Read(VFile, Poly->Verts, sizeof(Poly->Verts[0])*Poly->NumVerts))
		return NULL;

	return Poly;
}

//=======================================================================================
//	grIndexPoly_WriteToFile
//=======================================================================================
grBoolean grIndexPoly_WriteToFile(const grIndexPoly *Poly, grVFile *VFile)
{
	// Write out the number of verts
	if (!grVFile_Write(VFile, &Poly->NumVerts, sizeof(Poly->NumVerts)))
		return GR_FALSE;

	// Write out the verts
	if (!grVFile_Write(VFile, Poly->Verts, sizeof(Poly->Verts[0])*Poly->NumVerts))
		return GR_FALSE;

	return GR_TRUE;
}

//=======================================================================================
//	grIndexPoly_Destroy
//=======================================================================================
void grIndexPoly_Destroy(grIndexPoly **Poly)
{
	grIndexPoly		*pPoly;

	assert(Poly);
	pPoly = *Poly;
	assert(pPoly);

	if (pPoly->Verts)
	{
		assert(pPoly->NumVerts > 0);
		grRam_Free(pPoly->Verts);
	}
	else
	{
		assert(pPoly->NumVerts == 0);
	}

	grRam_Free(pPoly);

	*Poly = NULL;
}

//=======================================================================================
//	grIndexPoly_IsConvex
//=======================================================================================
grBoolean grIndexPoly_IsConvex(const grIndexPoly *Poly, const grVec3d *Normal, const grVertArray *Array)
{
	int32		i;

	for (i=0; i<Poly->NumVerts; i++)
	{
		grVec3d			Edge, EdgeNormal;
		const grVec3d	*v1, *v2;
		int32			j, i2;

		i2 = (i == Poly->NumVerts - 1) ? 0 : i+1;

		v1 = grVertArray_GetVertByIndex(Array, (grVertArray_Index)i);
		v2 = grVertArray_GetVertByIndex(Array, (grVertArray_Index)i2);

		grVec3d_Subtract(v1, v2, &Edge);

		grVec3d_CrossProduct(&Edge, Normal, &EdgeNormal);
		grVec3d_Normalize(&EdgeNormal);

		for (j=0; j<Poly->NumVerts; j++)
		{
			grFloat			Val;
			const grVec3d	*v3;

			if (j == i || j == i2)
				continue;

			v3 = grVertArray_GetVertByIndex(Array, (grVertArray_Index)j);

			Val = grVec3d_DotProduct(v2, &EdgeNormal);

			if (Val < 0)			// Point behind edge, poly is non-convex
				return GR_FALSE;
		}
	}

	return GR_TRUE;
}
