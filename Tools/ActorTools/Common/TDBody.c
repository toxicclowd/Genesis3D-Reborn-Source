/****************************************************************************************/
/*  TDBODY.C																			*/
/*                                                                                      */
/*  Author: Stephen Balkum	                                                            */
/*  Description: Build-time internal body hierarchy format.								*/
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

#include "tdbody.h"

#include "ram.h"

static void TopDownBody_InitNode(TopDownBody* pTDNode, int ThisBone)
{
	assert(pTDNode != NULL);

	pTDNode->BoneIndex = ThisBone;
	pTDNode->NumChildren = 0;
	pTDNode->pChildren = NULL;
}

static grBoolean TopDownBody_AddChildren(TopDownBody* pTDNode, int ThisBone, grBody* pBody)
{
	int BoneCount;
	int i;
	const char* pName;
	grXForm3d dummyMatrix;
	int ParentIndex;

	assert(pTDNode != NULL);
	assert(pBody != NULL);
	assert(grBody_IsValid(pBody) != GR_FALSE);
	assert(pTDNode->NumChildren == 0);
	assert(pTDNode->pChildren == NULL);

	// Body's bone hierarchy links each bone to its parent.  This guarantees
	// all children to have a higher bone index or they would not have been
	// to be added.
	BoneCount = grBody_GetBoneCount(pBody);
	for(i=ThisBone+1;i<BoneCount;i++)
	{
		grBody_GetBone(pBody, i, &pName, &dummyMatrix, &ParentIndex);
		if(ParentIndex == ThisBone)
		{
			// found a child
			TopDownBody* pNewNode;

#ifdef _DEBUG
			if(pTDNode->NumChildren == 0)
			{
				assert(pTDNode->pChildren == NULL);
			}
#endif

			pNewNode = grRam_Realloc(	pTDNode->pChildren, 
										sizeof(TopDownBody) * (pTDNode->NumChildren + 1) );
			if(pNewNode == NULL)
			{
				return(GR_FALSE);
			}

			// Do not adjust the number of children until the child node
			// actually exists.
			pTDNode->pChildren = pNewNode;
			TopDownBody_InitNode(&pTDNode->pChildren[pTDNode->NumChildren], i);
			pTDNode->NumChildren++;

			if(TopDownBody_AddChildren(&pTDNode->pChildren[pTDNode->NumChildren - 1], i, pBody) == GR_FALSE)
			{
				// something failed
				return(GR_FALSE);
			}
		}
	}

	return(GR_TRUE);
}

static void TopDownBody_DestroyChildren(TopDownBody* pTDNode)
{
	int i;
	assert(pTDNode != NULL);

	if(pTDNode->NumChildren > 0)
	{
		for(i=0;i<pTDNode->NumChildren;i++)
		{
			TopDownBody_DestroyChildren(&pTDNode->pChildren[i]);
		}

		grRam_Free(pTDNode->pChildren);
		pTDNode->NumChildren = 0;
	}
}

TopDownBody* TopDownBody_CreateFromBody(grBody* pBody)
{
	TopDownBody* pTDBody = NULL;

	assert(pBody != NULL);
	assert(grBody_IsValid(pBody) != GR_FALSE);

	pTDBody = GR_RAM_ALLOCATE_STRUCT(TopDownBody);
	if(pTDBody == NULL)
	{
		return(NULL);
	}

	// Initialize for root which is guaranteed to be the first bone (0).
	TopDownBody_InitNode(pTDBody, 0);

	if(TopDownBody_AddChildren(pTDBody, 0, pBody) == GR_FALSE)
	{
		// something failed
		TopDownBody_Destroy(&pTDBody);
	}

	return(pTDBody);
}

void TopDownBody_Destroy(TopDownBody** ppTDBody)
{
	TopDownBody_DestroyChildren(*ppTDBody);

	grRam_Free(*ppTDBody);
}

const TopDownBody* TopDownBody_FindBoneIndex(const TopDownBody* pTDNode, int Index)
{
	const TopDownBody* pFoundNode = NULL;
	int i;

	if(pTDNode->BoneIndex == Index)
		return(pTDNode);

	for(i=0;i<pTDNode->NumChildren;i++)
	{
		pFoundNode = TopDownBody_FindBoneIndex(pTDNode->pChildren + i, Index);
		if(pFoundNode != NULL)
			break;
	}

	return(pFoundNode);
}

TDBodyHeritage TopDownBody_IsDescendentOf(const TopDownBody* pTDBody, int ParentIndex, int Index)
{
	const TopDownBody* pParentNode;
	const TopDownBody* pChildNode;

	pParentNode = TopDownBody_FindBoneIndex(pTDBody, ParentIndex);
	if(pParentNode == NULL)
	{
		assert(0);
	}
	else
	{
		pChildNode = TopDownBody_FindBoneIndex(pParentNode, Index);
		if(pChildNode != NULL)
			return(TDBODY_IS_DESCENDENT);
	}

	return(TDBODY_IS_NOT_DESCENDENT);
}

