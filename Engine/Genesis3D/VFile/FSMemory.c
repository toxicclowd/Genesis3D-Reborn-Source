/****************************************************************************************/
/*  FSMEMORY.C                                                                          */
/*                                                                                      */
/*  Author: Eli Boling                                                                  */
/*  Description: Memory file system implementation                                      */
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
/******

Jan/Feb 99 : cbloom : hints-related bug-fix

*******/

#ifdef WIN32
#include	<windows.h>
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"BaseType.h"
#include	"Ram.h"

#include	"VFile.h"
#include	"VFile._h"

#include	"FSMemory.h"

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

//	"MF01"
#define	MEMORYFILE_SIGNATURE	0x3130464D

//	"MF02"
#define	MEMORYFINDER_SIGNATURE	0x3230464D

#define	CHECK_HANDLE(H)	assert(H);assert(H->Signature == MEMORYFILE_SIGNATURE);
#define	CHECK_FINDER(F)	assert(F);assert(F->Signature == MEMORYFINDER_SIGNATURE);

#define	MEMORY_FILE_GROW	0x2000

typedef struct	MemoryFile
{
	unsigned int	Signature;
	char *			Memory;
	int				Size;
	int				AllocatedSize;
	int				Position;
	int				PositionAdjust;				// Adjustment for old hints data
//	grVFile_Hints	Hints;
	grBoolean		WeOwnMemory;
	grBoolean		ReadOnly;
	grVFile *		HintsFile;
	unsigned int	OpenModeFlags;
}	MemoryFile;

static char * GRCC DataPtr(const MemoryFile *File)
{
	return File->Memory + File->Position + File->PositionAdjust;
}

static	grBoolean	GRCC TestForExpansion(MemoryFile *File, int Size)
{
	assert(File);
	assert(File->ReadOnly == GR_FALSE);
	assert(File->WeOwnMemory == GR_TRUE);

	assert(File->AllocatedSize >= File->Size);
	assert(File->AllocatedSize >= File->Position);

	if	(File->AllocatedSize - File->Position < Size)
	{
		int		NewSize;
		char *	NewBlock;

		NewSize = ((File->AllocatedSize + Size + (MEMORY_FILE_GROW - 1)) / MEMORY_FILE_GROW) * MEMORY_FILE_GROW;
		NewBlock = (char *)grRam_Realloc(File->Memory, NewSize);
		if	(!NewBlock)
			return GR_FALSE;
		File->Memory = NewBlock;
		File->AllocatedSize = NewSize;
//printf("FSMemory: Expanded file to %d bytes\n", NewSize);
	}

	return GR_TRUE;
}

#pragma warning (disable:4100)
static	void *	GRCC FSMemory_FinderCreate(
	grVFile *		FS,
	void *			Handle,
	const char *	FileSpec)
{
	return NULL;
}
#pragma warning (default:4100)

static	grBoolean	GRCC FSMemory_FinderGetNextFile(void *Handle)
{
	assert(!Handle);
	return GR_FALSE;
}

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_FinderGetProperties(void *Handle, grVFile_Properties *Props)
{
	assert(!Handle);
	return GR_FALSE;
}
#pragma warning (default:4100)

static	void GRCC FSMemory_FinderDestroy(void *Handle)
{
	assert(!Handle);
}

#pragma warning (disable:4100)
static	void *	GRCC FSMemory_Open(
	grVFile *		FS,
	void *			Handle,
	const char *	Name,
	void *			Context,
	unsigned int 	OpenModeFlags)
{
	return NULL;
}
#pragma warning (default:4100)

static	void *	GRCC FSMemory_OpenNewSystem(
	grVFile *		FS,
	const char *	Name,
	void *			Context,
	unsigned int 	OpenModeFlags)
{
	MemoryFile *			NewFS;
	grVFile_MemoryContext *	MemContext;

	if	(FS || Name || !Context)
		return NULL;

	MemContext = (grVFile_MemoryContext *)Context;

	// Don't allow the user to pass in memory pointer if we're updating or creating, because
	// we don't know what allocation functions we should use to resize their block if
	// necessary.  If you want to create a new file, you have to pass in NULL and let
	// us manage the allocations.
	if	(MemContext->Data && (OpenModeFlags & (GR_VFILE_OPEN_UPDATE | GR_VFILE_OPEN_CREATE)))
		return NULL;

	if	(OpenModeFlags & GR_VFILE_OPEN_DIRECTORY)
		return NULL;

	NewFS = (MemoryFile *)grRam_Allocate(sizeof(*NewFS));
	if	(!NewFS)
		return NewFS;
	memset(NewFS, 0, sizeof(*NewFS));

	NewFS->Memory = (char *)MemContext->Data;
	NewFS->Size = MemContext->DataLength;
	NewFS->AllocatedSize = NewFS->Size;
	NewFS->OpenModeFlags = OpenModeFlags;

	if	(NewFS->Memory)
	{
		grVFile_HintsFileHeader *	HintsHeader;

		NewFS->ReadOnly = GR_TRUE;
		NewFS->WeOwnMemory = GR_FALSE;

		HintsHeader = (grVFile_HintsFileHeader *)NewFS->Memory;
		if	(HintsHeader->Signature == GR_VFILE_HINTSFILEHEADER_SIGNATURE &&
			 !(OpenModeFlags & GR_VFILE_OPEN_RAW))
		{
#if 1
			grVFile_MemoryContext	MemoryContext;
			if	((uint32)HintsHeader->HintDataLength + sizeof(*HintsHeader) > (uint32)NewFS->Size)
			{
				//  Oops.  Something is wrong with this file
				grRam_Free(NewFS);
				return NULL;
			}
			MemoryContext.Data = (void *)(HintsHeader + 1);
			MemoryContext.DataLength = HintsHeader->HintDataLength;
			NewFS->HintsFile = grVFile_OpenNewSystem(NULL,
													 GR_VFILE_TYPE_MEMORY,
													 NULL, 
													 &MemoryContext,
													 GR_VFILE_OPEN_READONLY);
			NewFS->PositionAdjust = HintsHeader->HintDataLength + sizeof(*HintsHeader);

			NewFS->Size -= NewFS->PositionAdjust; 
#else
			NewFS->Hints.HintData = (void *)(HintsHeader + 1);
			NewFS->Hints.HintDataLength = HintsHeader->HintDataLength;
#endif
		}
	}
	else
	{
		NewFS->ReadOnly = GR_FALSE;
		NewFS->WeOwnMemory = GR_TRUE;
	}

	NewFS->Signature = MEMORYFILE_SIGNATURE;

	return NewFS;
}

static	grBoolean	GRCC FSMemory_UpdateContext(
	grVFile *		FS,
	void *			Handle,
	void *			Context,
	int 			ContextSize)
{
MemoryFile *			File;
grVFile_MemoryContext *	MemoryContext;
int HintsLength;

	assert(FS);
	assert(Context);
	
	File = (MemoryFile *)Handle;
	
	CHECK_HANDLE(File);

	if	(ContextSize != sizeof(grVFile_MemoryContext))
		return GR_FALSE;

	HintsLength = 0;	
	if	(File->HintsFile)
	{
		grVFile_MemoryContext	HintsContext;
		grVFile_HintsFileHeader	HintsHeader;

		grVFile_UpdateContext(File->HintsFile, &HintsContext, sizeof(HintsContext));
		if	(TestForExpansion(File, HintsContext.DataLength) == GR_FALSE)
			return GR_FALSE;

		HintsLength = sizeof(HintsHeader) + HintsContext.DataLength;
		memmove(File->Memory + HintsLength, File->Memory + File->PositionAdjust, File->Size); 
		HintsHeader.Signature = GR_VFILE_HINTSFILEHEADER_SIGNATURE;
		HintsHeader.HintDataLength = HintsContext.DataLength;
		memcpy(File->Memory, &HintsHeader, sizeof(HintsHeader));
		memcpy(File->Memory + sizeof(HintsHeader), HintsContext.Data, HintsContext.DataLength);
		File->PositionAdjust = HintsLength;
	}

	MemoryContext = (grVFile_MemoryContext *)Context;
	
	MemoryContext->Data		  = File->Memory;
	MemoryContext->DataLength = File->Size + HintsLength;

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_Close(void *Handle)
{
	MemoryFile *	File;
	
	File = (MemoryFile *)Handle;
	
	CHECK_HANDLE(File);

	if	(File->WeOwnMemory == GR_TRUE && File->Memory)
		grRam_Free(File->Memory);

	if	(File->HintsFile)
		grVFile_Close(File->HintsFile);

	grRam_Free(File);

	return GR_TRUE;
}

static uint32 GRCC ClampOperationSize(const MemoryFile *File, int Size)
{
	return min(File->Size - File->Position, Size);
}

static	grBoolean	GRCC FSMemory_GetS(void *Handle, void *Buff, int MaxLen)
{
	MemoryFile *	File;
	char *			p;
	char *			Start;
	char *			pBuff;

	assert(Buff);
	assert(MaxLen != 0);

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	MaxLen = ClampOperationSize(File, MaxLen);
	if	(MaxLen == 0)
		return GR_FALSE;

	p = DataPtr(File);
	pBuff = (char *)Buff;
	Start = p;
	while	(*p != '\n' && MaxLen > 0)
	{
		*pBuff++ = *p++;
		MaxLen--;
	}

	File->Position += p - Start + 1;
	assert(File->Position <= File->Size);
	assert(File->Size <= File->AllocatedSize);

	if	(MaxLen != 0)
	{
		*pBuff = *p;
		return GR_TRUE;
	}

	return GR_FALSE;
}

static	grBoolean	GRCC FSMemory_BytesAvailable(void *Handle, long *Count)
{
	MemoryFile *	File;

	assert(Count);

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	*Count = File->Size - File->Position;

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_Read(void *Handle, void *Buff, uint32 Count)
{
	MemoryFile *	File;

	assert(Buff);
	assert(Count != 0);

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	if	(ClampOperationSize(File, Count) != Count)
		return GR_FALSE;

	memcpy(Buff, DataPtr(File), Count);

	File->Position += Count;
	assert(File->Position <= File->Size);
	assert(File->Size <= File->AllocatedSize);

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_Write(void *Handle, const void *Buff, int Count)
{
	MemoryFile *	File;

	assert(Buff);
	assert(Count != 0);

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->ReadOnly == GR_TRUE)
		return GR_FALSE;

	if	(TestForExpansion(File, Count) == GR_FALSE)
		return GR_FALSE;

	memcpy(DataPtr(File), Buff, Count);
	
	File->Position += Count;
	if	(File->Size < File->Position)
		File->Size = File->Position;

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_Seek(void *Handle, int Where, grVFile_Whence Whence)
{
	MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	switch	(Whence)
	{
	int		NewPos;

	case	GR_VFILE_SEEKCUR:
		NewPos = File->Position + Where;
		if	(NewPos > File->AllocatedSize)
		{
			if	(File->ReadOnly == GR_TRUE)
				return GR_FALSE;
			if	(TestForExpansion(File, Where) == GR_FALSE)
				return GR_FALSE;
		}
		File->Position = NewPos;
		break;

	case	GR_VFILE_SEEKEND:
		if	(File->Size < Where)
			return GR_FALSE;
		File->Position = File->Size - Where;
		break;

	case	GR_VFILE_SEEKSET:
		if	(Where > File->AllocatedSize)
		{
			if	(File->ReadOnly == GR_TRUE)
				return GR_FALSE;
			if	(TestForExpansion(File, Where - File->Position) == GR_FALSE)
				return GR_FALSE;
		}
		File->Position = Where;
		break;

	default:
		assert(!"Unknown seek kind");
	}

	if	(File->Position > File->Size)
		File->Size = File->Position;
	
	assert(File->Size <= File->AllocatedSize);
	assert(File->Position <= File->AllocatedSize);

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_EOF(const void *Handle)
{
	const MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->Position == File->Size)
		return GR_TRUE;

	return GR_FALSE;
}

static	grBoolean	GRCC FSMemory_Tell(const void *Handle, long *Position)
{
	const MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	*Position = File->Position;

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_Size(const void *Handle, long *Size)
{
	const MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);
	
	*Size = File->Size;

	return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_GetProperties(const void *Handle, grVFile_Properties *Properties)
{
	const MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);
	
	assert(Properties);

	Properties->Time.Time1 = Properties->Time.Time2 = 0;

	Properties->Size = File->Size;
	Properties->Name[0] = 0;
		
	Properties->AttributeFlags = 0;
	if ( File->ReadOnly )
		Properties->AttributeFlags |= GR_VFILE_ATTRIB_READONLY;

return GR_TRUE;
}

static	grBoolean	GRCC FSMemory_SetSize(void *Handle, long Size)
{
	MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	if	(Size < File->Size)
		return GR_FALSE;

	if	(!TestForExpansion(File, (Size - File->Position)))
		return GR_FALSE;

	File->Size = Size;

	return GR_TRUE;
}

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_SetAttributes(void *Handle, grVFile_Attributes Attributes)
{
	assert(!"Not implemented");
	return GR_FALSE;
}
#pragma warning (default:4100)

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_SetTime(void *Handle, const grVFile_Time *Time)
{
	assert(!"Not implemented");
	return GR_FALSE;
}
#pragma warning (default:4100)

static	grVFile *	GRCC FSMemory_GetHintsFile(void *Handle)
{
	MemoryFile *	File;

	File = (MemoryFile *)Handle;

	CHECK_HANDLE(File);

	if	(!File->HintsFile && !(File->OpenModeFlags & GR_VFILE_OPEN_RAW))
	{
		grVFile_MemoryContext	MemoryContext;

		// Can't create a hints file for a readonly memory file
		if	(File->ReadOnly )
			return NULL;

		MemoryContext.Data = NULL;
		MemoryContext.DataLength = 0;

		File->HintsFile = grVFile_OpenNewSystem(NULL,
												GR_VFILE_TYPE_MEMORY,
												NULL,
												&MemoryContext,
												GR_VFILE_OPEN_CREATE);

	}

	return File->HintsFile;
}

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_FileExists(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}
#pragma warning (default:4100)

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_Disperse(
	grVFile *	FS,
	void *		Handle,
	const char *Directory)
{
	return GR_FALSE;
}
#pragma warning (default:4100)

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_DeleteFile(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}
#pragma warning (default:4100)

#pragma warning (disable:4100)
static	grBoolean	GRCC FSMemory_RenameFile(grVFile *FS, void *Handle, const char *Name, const char *NewName)
{
	return GR_FALSE;
}
#pragma warning (default:4100)

static	grVFile_SystemAPIs	FSMemory_APIs =
{
	FSMemory_FinderCreate,
	FSMemory_FinderGetNextFile,
	FSMemory_FinderGetProperties,
	FSMemory_FinderDestroy,

	FSMemory_OpenNewSystem,
	FSMemory_UpdateContext,
	FSMemory_Open,
	FSMemory_DeleteFile,
	FSMemory_RenameFile,
	FSMemory_FileExists,
	FSMemory_Disperse,
	FSMemory_Close,

	FSMemory_GetS,
	FSMemory_BytesAvailable,
	FSMemory_Read,
	FSMemory_Write,
	FSMemory_Seek,
	FSMemory_EOF,
	FSMemory_Tell,
	FSMemory_Size,

	FSMemory_GetProperties,

	FSMemory_SetSize,
	FSMemory_SetAttributes,
	FSMemory_SetTime,

#if 0
	FSMemory_ReadHints,
	FSMemory_WriteHints,
	FSMemory_HintsSize,
#endif
	FSMemory_GetHintsFile,
};

const grVFile_SystemAPIs * GRCC FSMemory_GetAPIs(void)
{
	return &FSMemory_APIs;
}
