/****************************************************************************************/
/*  JECHAIN.C                                                                           */
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
#include <memory.h>

#include "grChain.h"
#include "Ram.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |  ((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define GR_CHAIN_TAG			MAKEFOURCC('G', 'E', 'C', 'F')		// 'GE' 'C'hain 'F'ile
#define GR_CHAIN_VERSION		0x0000

// The grChain keeps a list of grChain_Links, current number of links, etc...
typedef struct grChain
{
	int32					RefCount;

	uint32					NumLinks;
	grChain_Link			*Links;

	const void				*LastLinkData;
	grChain_Link			*LastLink;

	#ifdef _DEBUG
		struct grChain		*Self;
	#endif
} grChain;

// The grChain_Link is used to keep linked list of items in the grChain object
typedef struct grChain_Link
{
	// I wanted to make this a const, but sonce we need to return it,
	//	I didn't want to cause any confusion by casting it to a non-const... sigh...
	void					*LinkData;				// LinkData is the user data the caller store on links (It CANNOT be NULL!)

	struct grChain_Link		*Next;
	struct grChain_Link		*Prev;

	#ifdef _DEBUG
		struct grChain_Link	*Self;
	#endif
} grChain_Link;

//========================================================================================
//	grChain_Create
//========================================================================================
grChain *grChain_Create(void)
{
	grChain		*Chain;

	Chain = (grChain *)GR_RAM_ALLOCATE_STRUCT(grChain);

	if (!Chain)
		return NULL;

	ZeroMem(Chain);

	Chain->RefCount = 1;

#ifdef _DEBUG
	Chain->Self = Chain;
#endif

	return Chain;
}

//========================================================================================
//	grChain_CreateRef
//========================================================================================
grBoolean grChain_CreateRef(grChain *Chain)
{
	assert(grChain_IsValid(Chain) == GR_TRUE);

	Chain->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grChain_CreateFromFile
//========================================================================================
grChain *grChain_CreateFromFile(grVFile *VFile, grChain_ReadIOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr)
{
	grChain		*Chain = NULL;
	uint32		NumLinks, i;
	uint32		Tag;
	uint16		Version;

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Chain))
			return NULL;

		if (Chain)
		{				
			if (!grChain_CreateRef(Chain))
				return NULL;

			return Chain;
		}
	}

	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return NULL;

	if (Tag != GR_CHAIN_TAG)
		return NULL;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return NULL;

	if (Version != GR_CHAIN_VERSION)
		return NULL;

	Chain = GR_RAM_ALLOCATE_STRUCT(grChain);

	if (!Chain)
		return NULL;

	ZeroMem(Chain);
	Chain->RefCount = 1;

#ifdef _DEBUG
	Chain->Self = Chain;
#endif

	// Load the links
	if (!grVFile_Read(VFile, &NumLinks, sizeof(NumLinks)))
		goto ExitWithError;

 	for (i=0; i< NumLinks; i++)
	{
		grChain_Link		*Link;
		void				*LinkData;

		if (!IOFunc(VFile, &LinkData, Context, PtrMgr))
			goto ExitWithError;

		Link = grChain_LinkCreate(LinkData);

		if (!Link)
			goto ExitWithError;

		if (!grChain_AddLink(Chain, Link))
		{
			grChain_LinkDestroy(&Link);
			goto ExitWithError;
		}
	}

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Chain))
			goto ExitWithError;
	}

	return Chain;

	ExitWithError:
	{
		if (Chain)
			grChain_Destroy(&Chain);

		return NULL;
	}
}


//========================================================================================
//	grChain_WriteToFile
//========================================================================================
grBoolean grChain_WriteToFile(const grChain *Chain, grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr)
{
	uint32			Tag;
	uint16			Version;
	grChain_Link	*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);

	if (PtrMgr)	
	{
		uint32		Count;
			
		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)Chain, &Count))
			return GR_FALSE;

		if (Count)		// Ptr was on stack, so return 
			return GR_TRUE;
	}

	Tag = GR_CHAIN_TAG;
	Version = GR_CHAIN_VERSION;
	
	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;

	if (!grVFile_Write(VFile, &Chain->NumLinks, sizeof(Chain->NumLinks)))
		return GR_FALSE;

	for (Link = Chain->Links; Link; Link = Link->Next)
	{
		if (!IOFunc(VFile, &Link->LinkData, Context, PtrMgr))
			return GR_FALSE;
	}

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)Chain))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grChain_Destroy
//========================================================================================
void grChain_Destroy(grChain **Chain)
{
	grChain_Link		*Link, *Next;

	assert(Chain);
	assert(grChain_IsValid(*Chain) == GR_TRUE);

	(*Chain)->RefCount --;

	if ((*Chain)->RefCount == 0)
	{
		// Free all the links
		for (Link = (*Chain)->Links; Link; Link = Next)
		{
			Next = Link->Next;

			assert(grChain_LinkIsValid(Link) == GR_TRUE);

			if (Link->Next)
				Link->Next->Prev = Link->Prev;

			if (Link == (*Chain)->Links)
			{
				assert(Link->Prev == NULL);
				(*Chain)->Links = Link->Next;
			}
			else
			{
				assert(Link->Prev != NULL);
				Link->Prev->Next = Link->Next;
			}

			grChain_LinkDestroy(&Link);
		}

		grRam_Free(*Chain);
	}

	*Chain = NULL;
}

//========================================================================================
//	grChain_IsValid
//========================================================================================
grBoolean grChain_IsValid(const grChain *Chain)
{
	uint32			NumLinks;
	grChain_Link	*Link, *LastLink;

	if (!Chain)
		return GR_FALSE;

#ifdef _DEBUG
	if (Chain->Self != Chain)
		return GR_FALSE;
#endif
	if (Chain->RefCount <= 0)
		return GR_FALSE;

	NumLinks = 0;

	LastLink = NULL;
	for (Link = Chain->Links; Link; Link = Link->Next)
	{
		if (!grChain_LinkIsValid(Link))
			return GR_FALSE;
	
		assert(Link->Prev == LastLink);
		LastLink = Link;

		NumLinks++;
	}

	if (Chain->NumLinks != NumLinks)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grChain_FindLink
//========================================================================================
grChain_Link *grChain_FindLink(const grChain *Chain, void *LinkData)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);

	for (Link = Chain->Links; Link; Link = Link->Next)
	{
		if (Link->LinkData == LinkData)
			return Link;
	}

	return NULL;
}

//========================================================================================
//	grChain_AddLink
//	Create a new link at the end of the current chain
//========================================================================================
grBoolean grChain_AddLink(grChain *Chain, grChain_Link *Link)
{
	grChain_Link		*Tail;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_LinkIsValid(Link) == GR_TRUE);

	assert(Link->Next == NULL);			// This is a booboo if they have already added this link to another grChain
	assert(Link->Prev == NULL);

	assert(Chain->NumLinks < 0xffffffff);
	Chain->NumLinks++;

	if (!Chain->Links)		// This case is easy
	{
		Chain->Links = Link;
		return GR_TRUE;
	}

	// Add to tail of chain
	for (Tail = Chain->Links; Tail->Next; Tail = Tail->Next);

	Tail->Next = Link;
	Link->Prev = Tail;

	return GR_TRUE;
}

//========================================================================================
//	grChain_InsertLinkAfter
//	Create a new link and inserts after InsertAfter
//========================================================================================
grBoolean grChain_InsertLinkAfter(grChain *Chain, grChain_Link *InsertAfter, grChain_Link *Link)
{
	grChain_Link		*Current;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_LinkIsValid(Link) == GR_TRUE);

	assert(Link->Next == NULL);			// This is a booboo if they have already added this link to another grChain
	assert(Link->Prev == NULL);
	assert(Chain->NumLinks < 0xffffffff);
	assert(Chain->Links);

	Chain->NumLinks++;

	// Find InsertAfter, and insert new link there
	for (Current = Chain->Links; Current; Current = Current->Next)
	{
		if (Current == InsertAfter)
			break;
	}

	assert(Current);

	if (!Current)
		return GR_FALSE;

	if (Current->Next)
		Current->Next->Prev = Link;

	Link->Next = Current->Next;
	Current->Next = Link;
	Link->Prev = Current;

	return GR_TRUE;
}

//========================================================================================
//	grChain_InsertLinkBefore
//	Create a new link and inserts before InsertBefore
//========================================================================================
grBoolean grChain_InsertLinkBefore(grChain *Chain, grChain_Link *InsertBefore, grChain_Link *Link)
{
	grChain_Link		*Current;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_LinkIsValid(Link) == GR_TRUE);

	assert(Link->Next == NULL);			// This is a booboo if they have already added this link to another grChain
	assert(Link->Prev == NULL);
	assert(Chain->NumLinks < 0xffffffff);
	assert(Chain->Links);

	Chain->NumLinks++;

	// Find InsertBefore, and insert new link there
	for (Current = Chain->Links; Current; Current = Current->Next)
	{
		if (Current == InsertBefore)
			break;
	}

	assert(Current);

	if (!Current)
		return GR_FALSE;

	if ( Current->Prev != NULL )
	{
		Current->Prev->Next = Link;
	}
	else
	{
		Chain->Links = Link;
	}

	Link->Next = Current;
	Link->Prev = Current->Prev;
	Current->Prev = Link;

	return GR_TRUE;
}

//========================================================================================
//	grChain_AddLinkData
//	Create a new link at the end of the current chain, and sets Link->LinkData to LinkData
//========================================================================================
grBoolean grChain_AddLinkData(grChain *Chain, void *LinkData)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(LinkData);

	Link = grChain_LinkCreate(LinkData);

	if (!Link)
		return GR_FALSE;

	return grChain_AddLink(Chain, Link);
}

//========================================================================================
//	grChain_InsertLinkData
//	Create a new link and inserts after InsertAfter, and sets Link->LinkData to LinkData
//========================================================================================
grBoolean grChain_InsertLinkData(grChain *Chain, grChain_Link *InsertAfter, void *LinkData)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(LinkData);

	Link = grChain_LinkCreate(LinkData);

	if (!Link)
		return GR_FALSE;

	return grChain_InsertLinkBefore(Chain, InsertAfter, Link);
}

//========================================================================================
//	grChain_RemoveLink
//========================================================================================
grBoolean grChain_RemoveLink(grChain *Chain, grChain_Link *Link)
{
	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_LinkIsValid(Link) == GR_TRUE);
	assert(Chain->NumLinks > 0);

	if (Link->Next)
		Link->Next->Prev = Link->Prev;

	if (Link == Chain->Links)
	{
		assert(Link->Prev == NULL);
		Chain->Links = Link->Next;
	}
	else
	{
		assert(Link->Prev != NULL);
		Link->Prev->Next = Link->Next;
	}

	// Assert code expects Next/Prev fields to be NULL when you add a link, so lets make it happy
	Link->Next = NULL;
	Link->Prev = NULL;

	Chain->NumLinks--;

	return GR_TRUE;
}

//========================================================================================
//	grChain_RemoveLinkData
//========================================================================================
grBoolean grChain_RemoveLinkData(grChain *Chain, void *LinkData)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(LinkData);

	// Find the link
	Link = grChain_FindLink(Chain, LinkData);

	if (!Link)
		return GR_FALSE;

	// Icestorm Begin
	// NOTE: Be sure, LastLink(Data) will be valid or NULL.
	if (Chain->LastLink==Link)
	{
		Chain->LastLink=Link->Next;
		if (Link->Next!=NULL) 
			Chain->LastLinkData=Link->Next->LinkData;
		else
			Chain->LastLinkData=NULL;
	}
	// Icestorm End

	// Remove it
	if (!grChain_RemoveLink(Chain, Link))
		return GR_FALSE;

	// Destroy it
	grChain_LinkDestroy(&Link);

	return GR_TRUE;
}

//========================================================================================
//	grChain_GetLinkCount
//========================================================================================
uint32 grChain_GetLinkCount(const grChain *Chain)
{
	assert(grChain_IsValid(Chain) == GR_TRUE);

	return Chain->NumLinks;
}

//========================================================================================
//	grChain_GetFirstLink
//	This function is useful, when they want to iterate through the links themselves.
//========================================================================================
grChain_Link *grChain_GetFirstLink(const grChain *Chain)
{
	assert(Chain);

	return Chain->Links;
}

//========================================================================================
//	grChain_GetLinkByIndex
//========================================================================================
grChain_Link *grChain_GetLinkByIndex(const grChain *Chain, uint32 Index)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_GetLinkCount(Chain) >= Index);

	for (Link = Chain->Links; Link; Link = Link->Next, Index--)
	{
		if (Index == 0)
			return Link;
	}

	assert(0);		// Invalid index if we get here
	return NULL;
}

//========================================================================================
//	grChain_GetLinkDataByIndex
//========================================================================================
void *grChain_GetLinkDataByIndex(const grChain *Chain, uint32 Index)
{
	grChain_Link		*Link;

	assert(grChain_IsValid(Chain) == GR_TRUE);
	assert(grChain_GetLinkCount(Chain) >= Index);

	for (Link = Chain->Links; Link; Link = Link->Next, Index--)
	{
		if (Index == 0)
			return Link->LinkData;
	}

	assert(0);		// Invalid index if we get here
	return NULL;
}

//========================================================================================
//	grChain_GetNextLinkData
//	NULL returns the first in t e list.
//	Caches out the last item, so linear searches are faster
//========================================================================================
void *grChain_GetNextLinkData(grChain *Chain, void *Start)
{
	grChain_Link	*Link;
	void			*LinkData;

	assert(Chain);

	if (!Start)								// This case is really easy
	{
		Link = Chain->Links;
	}
	else if (Chain->LastLinkData == Start)	// This case is easy
	{
		Link = Chain->LastLink;

		if (Link)
			Link = Link->Next;				// Get next link (NOTE that this next link CAN be NULL)
		else
			Link = Chain->Links;			// If link is NULL, wrap to first
	}
	else for (Link = Chain->Links; Link; Link = Link->Next)		// We will have to search now...
	{
		if (Link->LinkData == Start)
			break;
	}

	if (Link)
		LinkData = Link->LinkData;
	else
		LinkData = NULL;

	// Remember the last brush/link returned...
	Chain->LastLinkData = LinkData;
	Chain->LastLink = Link;

	return LinkData;
}

//========================================================================================
//	grChain_LinkCreate
//========================================================================================
grChain_Link *grChain_LinkCreate(void *LinkData)
{
	grChain_Link	*Link;

	assert(LinkData);

	Link = GR_RAM_ALLOCATE_STRUCT(grChain_Link);

	if (!Link)
		return NULL;

	ZeroMem(Link);

#ifdef _DEBUG
	Link->Self = Link;
#endif

	Link->LinkData = LinkData;

	return Link;
}

//========================================================================================
//	grChain_LinkDestroy
//========================================================================================
void grChain_LinkDestroy(grChain_Link **Link)
{
	assert(Link);
	assert(grChain_LinkIsValid(*Link) == GR_TRUE);

	grRam_Free(*Link);

	*Link = NULL;
}

//========================================================================================
//	grChain_LinkIsValid
//========================================================================================
grBoolean grChain_LinkIsValid(const grChain_Link *Link)
{
	if (!Link)
		return GR_FALSE;

#ifdef _DEBUG
	if (Link->Self != Link)
		return GR_FALSE;
#endif

	if (!Link->LinkData)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
//	grChain_LinkGetLinkData
//========================================================================================
void *grChain_LinkGetLinkData(const grChain_Link *Link)
{
	return Link->LinkData;
}

//========================================================================================
//	grChain_LinkGetNext
//========================================================================================
grChain_Link *grChain_LinkGetNext(const grChain_Link *Link)
{
	assert(Link);

	return Link->Next;
}

//========================================================================================
//	grChain_LinkGetPrev
//========================================================================================
grChain_Link *grChain_LinkGetPrev(const grChain_Link *Link)
{
	assert(Link);

	return Link->Prev;
}

//========================================================================================
//	grChain_LinkDataGetIndex
//========================================================================================
uint32 grChain_LinkDataGetIndex(const grChain *Chain, void *LinkData)
{
	grChain_Link		*Link;
	uint32				Index;

	assert(grChain_IsValid(Chain) == GR_TRUE);

	for (Index = 0, Link = Chain->Links; Link; Link = Link->Next, Index++)
	{
		if (Link->LinkData == LinkData)
			return Index;
	}

	assert(0);		// Invalid linkdata if we get here
	return 0;
}

