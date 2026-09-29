/****************************************************************************************/
/*  BitmapList.c                                                                        */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description: Maintains a pool of bitmap pointers.                                   */
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

#ifdef _DEBUG
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#else
#include <memory.h>
#endif
#endif

#include "BitmapList.h"
#include "Dcommon.h"
#include "Bitmap.h"
#include "Bitmap._h"
#include "List.h"
#include "MemPool.h"
#include "Errorlog.h"
#include "Ram.h"
//#include "tsc.h"

struct BitmapList
{
	Hash * HashPtr; // CJP : Modified to not be Hash* Hash
	int Members,Adds;
#ifdef _DEBUG
	BitmapList * MySelf;
#endif
};


grBoolean BitmapList_IsValid(BitmapList *pList);

//================================================================================
//	BitmapList_Create
//================================================================================
BitmapList *BitmapList_Create(void)
{
BitmapList * pList;
	pList = (BitmapList *)grRam_Allocate(sizeof(*pList));
	if (! pList )
		return NULL;
	memset(pList,0,sizeof(*pList));
	pList->HashPtr = Hash_Create();
	if ( ! pList->HashPtr )
	{
		grRam_Free(pList);
		return NULL;
	}
	#ifdef _DEBUG
	pList->MySelf = pList;
	#endif
return pList;
}

//================================================================================
//	BitmapList_Destroy
//================================================================================
grBoolean BitmapList_Destroy(BitmapList *pList)
{
grBoolean	Ret = GR_TRUE;

	if ( ! pList )
		return GR_TRUE;

	if ( pList->HashPtr )
	{
	HashNode	*pNode;
		pNode = NULL;
		
		while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
		{
		grBitmap *Bmp;
		uint32 TimesAdded;

			HashNode_GetData(pNode,(uint32 *)&Bmp,&TimesAdded);

			if (!grBitmap_DetachDriver(Bmp, GR_TRUE))
				Ret = GR_FALSE;

			assert( pList->Members >= 1 && pList->Adds >= (int)TimesAdded );

			pList->Members --;

			assert( TimesAdded >= 1 );

			while(TimesAdded --)
			{
				assert(Bmp);
				grBitmap_Destroy(&Bmp);
				pList->Adds --;
			}
		}

		// Finally, destroy the entire hash table
		Hash_Destroy(pList->HashPtr);
	}

	grRam_Free(pList);

	return Ret;
}

//================================================================================
//	BitmapList_SetGamma
//================================================================================
grBoolean BitmapList_SetGamma(BitmapList *pList, grFloat Gamma)
{
HashNode *pNode;

	assert(BitmapList_IsValid(pList));

#ifdef _DEBUG
	//pushTSC();
#endif

	pNode = NULL;
	while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
	{
	grBitmap *Bmp;
		Bmp = (grBitmap *)HashNode_Key(pNode);

		if (!grBitmap_SetGammaCorrection(Bmp, Gamma, GR_TRUE) )
		{
			grErrorLog_AddString(-1,"BitmapList_SetGamma : SetGamma failed.", NULL);
			return GR_FALSE;
		}
	}
	
#ifdef _DEBUG
	//showPopTSCper("BitmapList_SetGamma",pList->MembersAttached,"bitmap");
#endif

return GR_TRUE;
}

//================================================================================
//	BitmapList_AttachAll
//================================================================================
grBoolean BitmapList_AttachAll(BitmapList *pList, DRV_Driver *Driver, grFloat Gamma)
{
HashNode *pNode;
int MembersAttached;

	assert(BitmapList_IsValid(pList));

	//pushTSC();

	pNode = NULL;
	MembersAttached = 0;
	while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
	{
	grBitmap *Bmp;

		Bmp = (grBitmap *)HashNode_Key(pNode);

		if (!grBitmap_SetGammaCorrection_DontChange(Bmp, Gamma) )
		{
			grErrorLog_AddString(-1,"BitmapList_AttachAll : SetGamma failed", NULL);
			return GR_FALSE;
		}

		if (!grBitmap_AttachToDriver(Bmp, Driver, 0) )
		{
			grErrorLog_AddString(-1,"BitmapList_AttachAll : AttachToDriver failed", NULL);
			return GR_FALSE;
		}

		MembersAttached ++;
	}

	//showPopTSC("BitmapList_AttachAll");

	assert( MembersAttached == pList->Members );

	return GR_TRUE;
}

//================================================================================
//	BitmapList_DetachAll
//================================================================================
grBoolean BitmapList_DetachAll(BitmapList *pList)
{
HashNode	*pNode;
grBoolean	Ret = GR_TRUE;
int MembersAttached;

	assert(BitmapList_IsValid(pList));

	pNode = NULL;
	while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
	{
	grBitmap *Bmp;
	uint32 TimesAdded;

		HashNode_GetData(pNode,(uint32 *)&Bmp,&TimesAdded);

		if (!grBitmap_DetachDriver(Bmp, GR_TRUE))
			Ret = GR_FALSE;
	}

	MembersAttached = 0;

	return Ret;
}

//================================================================================
//	BitmapList_CountMembers
//================================================================================
int BitmapList_CountMembers(BitmapList *pList)
{
#ifdef NDEBUG
	return pList->Members;
#else
HashNode *pNode;
int Count;

	Count = 0;
	pNode = NULL;
	while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
	{
		Count ++;
	}

	assert( Count == pList->Members );
	assert( pList->Adds >= pList->Members );

return Count;
#endif
}
int BitmapList_CountMembersAttached(BitmapList *pList)
{
HashNode *pNode;
int Count;

	Count = 0;
	pNode = NULL;
	while( (pNode = Hash_WalkNext(pList->HashPtr,pNode)) != NULL )
	{
	grBitmap *Bmp;
	uint32 TimesAdded;

		HashNode_GetData(pNode,(uint32 *)&Bmp,&TimesAdded);

		if ( grBitmap_GetTHandle(Bmp) )
			Count ++;
	}

	assert( pList->Adds >= pList->Members && pList->Members >= Count );

return Count;
}

//================================================================================
//	BitmapList_Has
//================================================================================
grBoolean BitmapList_Has(BitmapList *pList, grBitmap *Bitmap)
{
HashNode *pNode;
uint32 TimesAdded;

	assert(pList && Bitmap);

	pNode = Hash_Get(pList->HashPtr,(uint32)Bitmap,&TimesAdded);

	assert( pList->Adds >= (int)TimesAdded );

return (pNode && TimesAdded) ? GR_TRUE : GR_FALSE;
}

//================================================================================
//	BitmapList_Add
//================================================================================
grBoolean BitmapList_Add(BitmapList *pList, grBitmap *Bitmap)
{	
HashNode *pNode;
uint32 TimesAdded;

	assert(BitmapList_IsValid(pList));
	assert(Bitmap);

	// Increase reference count on this Bitmap
	grBitmap_CreateRef(Bitmap);

	pList->Adds ++;

	if ( (pNode = Hash_Get(pList->HashPtr, (uint32)Bitmap, &TimesAdded)) != NULL )
	{
		HashNode_SetData(pNode,TimesAdded+1);
		return GR_FALSE;
	}
	else
	{
		pList->Members ++;
		Hash_Add(pList->HashPtr,(uint32)Bitmap,1);
		return GR_TRUE;
	}
}

//================================================================================
//	BitmapList_Remove
//================================================================================
grBoolean BitmapList_Remove(BitmapList *pList,grBitmap *Bitmap)
{
HashNode *pNode;
uint32 TimesAdded;
uint32 Key;

	assert(BitmapList_IsValid(pList));
	assert(Bitmap);

	Key = (uint32) Bitmap;
	pNode = Hash_Get(pList->HashPtr,Key,&TimesAdded);

	assert(pNode);

	pList->Adds --;
	TimesAdded --;

	if ( TimesAdded <= 0 )
	{
		if ( ! grBitmap_DetachDriver(Bitmap, GR_TRUE) )
		{
			grErrorLog_AddString(-1, "BitmapList_Remove:  grBitmap_DetachDriver failed.", NULL);
			return GR_FALSE;
		}
	}

	grBitmap_Destroy(&Bitmap);

	if ( TimesAdded <= 0 )
	{
		pList->Members --;
		Hash_DeleteNode(pList->HashPtr,pNode);
		return GR_TRUE;
	}
	else
	{
		HashNode_SetData(pNode,TimesAdded);
		return GR_FALSE;
	}
}


grBoolean BitmapList_IsValid(BitmapList *pList)
{
	if ( ! pList ) 
		return GR_FALSE;
		
	if ( pList->Adds < pList->Members )
		return GR_FALSE;

#ifdef _DEBUG
	if ( pList->MySelf != pList )
		return GR_FALSE;
#endif

	if ( pList->Members != BitmapList_CountMembers(pList) )
		return GR_FALSE;

return GR_TRUE;
}
