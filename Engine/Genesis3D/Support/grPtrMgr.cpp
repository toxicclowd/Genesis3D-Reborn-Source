/****************************************************************************************/
/*  JEPTRMGR.C                                                                          */
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
#include <string.h>
#include <assert.h>

// Public Dependents
#include "grPtrMgr.h"

// Private dependents
#include "Ram.h"
#include "Errorlog.h"
#include "grPtrMgr._h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

static grPtrMgr_SEntry *PushSEntry(grPtrMgr *PtrMgr);
static grPtrMgr_Index FindIndexFromPtr(const grPtrMgr *PtrMgr, void *Ptr);

//========================================================================================
//	grPtrMgr_Create
//========================================================================================
GRAPI grPtrMgr * GRCC grPtrMgr_Create(void)
{
	grPtrMgr		*PtrMgr;

	PtrMgr = GR_RAM_ALLOCATE_STRUCT(grPtrMgr);

	if (!PtrMgr)
	{
		grErrorLog_AddString(-1, "grPtrMgr_Create:  GR_RAM_ALLOCATE_STRUCT(grPtrMgr) failed.", "PtrMgr");
		return NULL;
	}

	ZeroMem(PtrMgr);

	PtrMgr->StackSize = GR_PTRMGR_START_SIZE;

	PtrMgr->PtrStack = GR_RAM_ALLOCATE_ARRAY(grPtrMgr_SEntry, PtrMgr->StackSize);

	if (!PtrMgr->PtrStack)
	{
		grErrorLog_AddString(-1, "grPtrMgr_Create:  GR_RAM_ALLOCATE_ARRAY failed.", "PtrMgr->PtrStack");
		grRam_Free(PtrMgr);
		return NULL;
	}

#ifdef _DEBUG
	PtrMgr->Signature1 = PtrMgr;
	PtrMgr->Signature2 = PtrMgr;
#endif

	PtrMgr->RefCount = 1;

	return PtrMgr;
}

//========================================================================================
//	grPtrMgr_IsValid
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_IsValid(const grPtrMgr *PtrMgr)
{
	if (!PtrMgr)
	{
		grErrorLog_AddString(-1, "grPtrMgr_IsValid:  PtrMgr == NULL", NULL);
		return GR_FALSE;
	}

#ifdef _DEBUG
	if (PtrMgr->Signature1 != PtrMgr)
	{
		grErrorLog_AddString(-1, "grPtrMgr_IsValid:  PtrMgr->Signature1 != PtrMgr", NULL);
		return GR_FALSE;
	}

	if (PtrMgr->Signature2 != PtrMgr)
	{
		grErrorLog_AddString(-1, "grPtrMgr_IsValid:  PtrMgr->Signature2 != PtrMgr", NULL);
		return GR_FALSE;
	}
#endif

	if (PtrMgr->RefCount <= 0)
	{
		grErrorLog_AddString(-1, "grPtrMgr_IsValid:  PtrMgr->RefCount <= 0", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_CreateRef(grPtrMgr *PtrMgr)
{
	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);

	if (PtrMgr->RefCount >= GR_PTRMGR_MAX_REF_COUNTS)
	{
		grErrorLog_AddString(-1, "grPtrMgr_CreateRef:  PtrMgr->RefCount >= ((0xFFFFFFFF>>1)-1)", NULL);
		assert(0);		// just in case they are not checking return value (I dunno if this is legit though...)
		return GR_FALSE;
	}

	PtrMgr->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_Destroy
//========================================================================================
GRAPI void GRCC grPtrMgr_Destroy(grPtrMgr **PtrMgr)
{
	assert(PtrMgr);
	assert(grPtrMgr_IsValid(*PtrMgr) == GR_TRUE);

	(*PtrMgr)->RefCount--;

	if ((*PtrMgr)->RefCount == 0)
	{
		assert((*PtrMgr)->PtrStack);
		grRam_Free((*PtrMgr)->PtrStack);

		grRam_Free(*PtrMgr);
	}

	*PtrMgr = NULL;
}

//========================================================================================
//	grPtrMgr_ReadPtr
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_ReadPtr(grPtrMgr *PtrMgr, grVFile *VFile, void **Ptr)
{
	grPtrMgr_Header		Header;
	grPtrMgr_SEntry		*SEntry;

	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);
	assert(VFile);
	assert(Ptr);

	// Read the header
	if (!grVFile_Read(VFile, &Header, sizeof(Header)))
	{
		grErrorLog_AddString(-1, "grPtrMgr_Read:  grVFile_Read failed.", "Header");
		return GR_FALSE;
	}

	if (Header.Index == GR_PTRMGR_NULL_INDEX)
	{
		*Ptr = NULL;		// They need to load this object
		return GR_TRUE;
	}
	
	// Object has been loaded by a previous read, return a ptr to it
	assert(Header.Index >= 0 && Header.Index < PtrMgr->StackLoc);	// This assert usually means that the ptr has not actually been read yet

	SEntry = &PtrMgr->PtrStack[Header.Index];

	// Assert that it HAS been loaded (assuming they are loading in the EXACT order they saved)
	assert(SEntry->Ptr);
	assert(SEntry->RefCount > 0);
	
	SEntry->RefCount++;		// Ref it

	PtrMgr->TotalPtrRefs++;

	*Ptr = SEntry->Ptr;

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_PushPtr
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_PushPtr(grPtrMgr *PtrMgr, void *Ptr)
{
	grPtrMgr_SEntry		*SEntry;

	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);
	assert(Ptr);

	SEntry = PushSEntry(PtrMgr);

	if (!SEntry)
		return GR_FALSE;

	assert(SEntry->RefCount == 0);
	assert(!SEntry->Ptr);

	// Ref the entry
	SEntry->RefCount++;
	SEntry->Ptr = Ptr;

	PtrMgr->TotalPtrRefs++;

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_WritePtr
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_WritePtr(grPtrMgr *PtrMgr, grVFile *VFile, void *Ptr, uint32 *Count)
{
	grPtrMgr_Header		Header;
	grPtrMgr_SEntry		*SEntry;

	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);
	assert(VFile);
	assert(Ptr);
	assert(Count);

	// Try to find the ptr in the PtrStack
	Header.Index = FindIndexFromPtr(PtrMgr, Ptr);
	
	if (Header.Index != GR_PTRMGR_NULL_INDEX)
	{
		SEntry = &PtrMgr->PtrStack[Header.Index];
		assert(SEntry->RefCount > 0);
		assert(SEntry->Ptr == Ptr);

		// Get the count of this entry, BEFORE it was ref'd
		*Count = SEntry->RefCount++;
		PtrMgr->TotalPtrRefs++;
	}
	else
		*Count = 0;		// Not in PtrStack yet

	// Save the header
	if (!grVFile_Write(VFile, &Header, sizeof(Header)))
	{
		grErrorLog_AddString(-1, "grPtrMgr_WritePtr:  grVFile_Read failed.", "Header");
		return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_PopPtr
//========================================================================================
GRAPI void GRCC grPtrMgr_PopPtr(grPtrMgr *PtrMgr, void *Ptr)
{
	grPtrMgr_SEntry		*SEntry;

	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);
	assert(Ptr);
	
	assert(PtrMgr->StackLoc > 0);
	PtrMgr->StackLoc--;

	SEntry = &PtrMgr->PtrStack[PtrMgr->StackLoc];

	assert(SEntry->RefCount > 0);
	assert(SEntry->Ptr == Ptr);
		
	SEntry->RefCount--;
	PtrMgr->TotalPtrRefs--;
}

//========================================================================================
//	grPtrMgr_GetPtrCount
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_GetPtrCount(const grPtrMgr *PtrMgr, int32 *PtrCount)
{
	*PtrCount = PtrMgr->StackLoc;		// Ptr count is current stack loc...

	return GR_TRUE;
}

//========================================================================================
//	grPtrMgr_GetPtrRefs
//========================================================================================
GRAPI grBoolean GRCC grPtrMgr_GetPtrRefs(const grPtrMgr *PtrMgr, int32 *PtrRefs)
{
	*PtrRefs = PtrMgr->TotalPtrRefs;

	return GR_TRUE;
}

//========================================================================================
//	PushSEntry
//========================================================================================
static grPtrMgr_SEntry *PushSEntry(grPtrMgr *PtrMgr)
{
	grPtrMgr_SEntry		*SEntry;
	
	if (PtrMgr->StackLoc >= PtrMgr->StackSize)
	{
		uint32		NewSize;

		if (PtrMgr->StackSize >= GR_PTRMGR_MAX_STACK_SIZE)
			return NULL;		// No room to grow anymore...

		// At this point, there is room to grow at least one element, so it should not fail
		//	unless we run out of memory

		NewSize = PtrMgr->StackSize + GR_PTRMGR_EXTEND_AMOUNT;

		if (NewSize < PtrMgr->StackSize)				// Must have wrapped, clamp to MAXSIZE
			NewSize = GR_PTRMGR_MAX_STACK_SIZE;

		if (NewSize > GR_PTRMGR_MAX_STACK_SIZE)
			NewSize = GR_PTRMGR_MAX_STACK_SIZE;

		assert(NewSize > PtrMgr->StackSize);

		PtrMgr->StackSize = NewSize;

		PtrMgr->PtrStack = (grPtrMgr_SEntry *)grRam_Realloc(PtrMgr->PtrStack, PtrMgr->StackSize*sizeof(grPtrMgr_SEntry));

		if (!PtrMgr->PtrStack)
			return NULL;			// Out of memory
	}

	SEntry = &PtrMgr->PtrStack[PtrMgr->StackLoc++];

	ZeroMem(SEntry);

	return SEntry;
}

//========================================================================================
//	FindIndexFromPtr
//========================================================================================
static grPtrMgr_Index FindIndexFromPtr(const grPtrMgr *PtrMgr, void *Ptr)
{
	uint32				i;
	grPtrMgr_SEntry		*SEntry;

	assert(grPtrMgr_IsValid(PtrMgr) == GR_TRUE);
	assert(Ptr);

	for (SEntry = PtrMgr->PtrStack, i=0; i< PtrMgr->StackLoc; i++, SEntry++)
	{
		if (SEntry->Ptr == Ptr)
		{
			assert(SEntry->RefCount > 0);
			return i;
		}
	}

	return GR_PTRMGR_NULL_INDEX;
}

// Krouer : ResourceMgr accessor
GRAPI grResourceMgr* GRCC grPtrMgr_GetResourceMgr(const grPtrMgr *PtrMgr)
{
	assert(PtrMgr);
	return PtrMgr->pResMgr;
}

