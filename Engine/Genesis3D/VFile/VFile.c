/****************************************************************************************/
/*  VFILE.C                                                                             */
/*                                                                                      */
/*  Author: Eli Boling                                                                  */
/*  Description: Virtual file implementation                                            */
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

#define LN_SEARCHLIST	// works? this lets us avoid a malloc
						// what exactly is the point of the searchlist?
//#define NO_INET

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include	<windows.h>

#endif

#include	<stdio.h>
#include	<assert.h>
#include	<stdarg.h>
#include	<string.h>

#include	"BaseType.h"
#include	"Ram.h"
#include	"ThreadQueue.h"
#include	"Log.h"

#include	"VFile.h"
#include	"VFile._h"

#ifdef WIN32
#include	"fsdos.h"
#endif

#ifdef BUILD_BE
#include <OS.h>
#include 	"FSBeos.h"
#endif

#include	"FSMemory.h"
#include	"FSVFS.h"
#include	"FSLZ.h"
#include	"fsfakenet.h"

#ifndef NO_INET
#include	"fsinet.h"
#endif

#ifdef LN_SEARCHLIST
#include	"List.h"
#endif

#ifndef LN_SEARCHLIST
typedef	struct	FSSearchList
{
	grVFile *				FS;
	struct FSSearchList *	Next;
}	FSSearchList;
#endif

typedef	struct	grVFile
{
#ifdef LN_SEARCHLIST
	LinkNode					LN;
	LinkNode					LN_Children;
#else
	FSSearchList *				SearchList;
#endif

	grVFile_TypeIdentifier		SystemType;
	const grVFile_SystemAPIs *	APIs;
	void *						FSData;
	grVFile *					Context;
	grThreadQueue_Semaphore *	Semaphore;	
	int							RefCount;
	grBoolean					IsHintsFile;
}	grVFile;

typedef struct	grVFile_Finder
{
	const grVFile_SystemAPIs *	APIs;
	void *						Data;
#ifdef WIN32
        CRITICAL_SECTION                        CriticalSection;
#endif // WIN32
#ifdef BUILD_BE
        sem_id                                  CriticalSection;
#endif // BUILD_BE  
}	grVFile_Finder;

/*}{ ******* Statics *******/

#ifdef _DEBUG
static uint32 VFiles_Open = 0;
#endif

static uint32 						grVFile_RefCount = 0;

static	grVFile_SystemAPIs  **RegisteredAPIs = NULL;
static	int							SystemCount;

#define grVFile_Lock(File)		grThreadQueue_Semaphore_Lock(	((grVFile *)(File))->Semaphore)
#define grVFile_UnLock(File)	grThreadQueue_Semaphore_UnLock(	((grVFile *)(File))->Semaphore)

static grVFile* GRCC	grVFile_New(void);
static void		GRCC	grVFile_Free(grVFile * File);

static grBoolean grVFile_PathIsSane(const char * Path);
static grBoolean GRCC CheckOpenFlags(unsigned int OpenModeFlags);

#ifdef WIN32
#define LOCK_CRITICALSECTION(a) EnterCriticalSection(a);
#define UNLOCK_CRITICALSECTION(a) LeaveCriticalSection(a);
#define DELETE_CRITICALSECTION(a) DeleteCriticalSection(a);
#endif

#ifdef BUILD_BE
#define LOCK_CRITICALSECTION(a) acquire_sem(*a); // passes a pointer, so convert
#define UNLOCK_CRITICALSECTION(a) release_sem(*a);
#define DELETE_CRITICALSECTION(a) delete_sem(*a);
#endif

/*}{ ******* File System Functions *******/

static	grBoolean GRCC grVFile_RegisterFileSystemInternal(const grVFile_SystemAPIs *APIs, grVFile_TypeIdentifier *Type)
{
	grVFile_SystemAPIs **	NewList;

	NewList = (grVFile_SystemAPIs **)grRam_Realloc((void *)RegisteredAPIs, sizeof(*RegisteredAPIs) * (SystemCount + 1));
	if(!NewList)
	 return GR_FALSE;

	RegisteredAPIs = NewList;
	RegisteredAPIs[SystemCount++] = (grVFile_SystemAPIs *)APIs;
	*Type = (grVFile_TypeIdentifier)SystemCount;

	return GR_TRUE;
}

static	grBoolean grVFile_Enter(void)
{
	grVFile_TypeIdentifier 	Type;

	if ( grVFile_RefCount > 0 )
	{
		grVFile_RefCount ++;
		return GR_TRUE;
	}

#ifdef WIN32
	if	(grVFile_RegisterFileSystemInternal(FSDos_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_DOS)
		return GR_FALSE;
#endif

#ifdef BUILD_BE
	if	(grVFile_RegisterFileSystemInternal(FSBeOS_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_DOS)
		return GR_FALSE;
#endif

	if	(grVFile_RegisterFileSystemInternal(FSMemory_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_MEMORY)
		return GR_FALSE;

	if	(grVFile_RegisterFileSystemInternal(FSVFS_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_VIRTUAL)
		return GR_FALSE;

	if	(grVFile_RegisterFileSystemInternal(FSLZ_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_LZ)
		return GR_FALSE;

	if	(grVFile_RegisterFileSystemInternal(FSFakeNet_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_FAKENET)
		return GR_FALSE;

	// INET must be last

#ifndef NO_INET
	if	(grVFile_RegisterFileSystemInternal(FSINet_GetAPIs(), &Type) == GR_FALSE)
		return GR_FALSE;
	if	(Type != GR_VFILE_TYPE_INTERNET)
		return GR_FALSE;
#endif

	grVFile_RefCount ++;

	return GR_TRUE;
}

static void grVFile_Leave(void)
{
	grVFile_RefCount --;
	if ( grVFile_RefCount == 0 )
	{
		assert(RegisteredAPIs);
		#ifdef WIN32 // hack....
		grRam_Free((void *)RegisteredAPIs);
		#endif
		#ifdef BUILD_BE
		grRam_Free(RegisteredAPIs);
		#endif
		
		RegisteredAPIs = NULL;
	}
}

/*
//CB : this function is neither exposed or used, so ignore it
GRAPI grBoolean GRCC grVFile_RegisterFileSystem(const grVFile_SystemAPIs *APIs, grVFile_TypeIdentifier *Type)
{
	grBoolean	Result;

	assert(APIs);
	assert(Type);

// @@ is this important ?
//	if	(RegisterBuiltInAPIs() == GR_FALSE)
//		return GR_FALSE;

	Result = grVFile_RegisterFileSystemInternal(APIs, Type);
	return Result;
}
*/

/*}{ ******* Open Functions *******/

GRAPI grVFile * GRCC grVFile_OpenNewSystem(
	grVFile *				FS,
	grVFile_TypeIdentifier 	FileSystemType,
	const char *			Name,
	void *					Context,
	unsigned int 			OpenModeFlags)
{
	const grVFile_SystemAPIs *	APIs;
	grVFile *					File;
	void *						FSData;

	assert( ! Name || grVFile_PathIsSane(Name) );

	if ( ! grVFile_Enter() )
		return NULL;

	if	((FileSystemType == 0) || (FileSystemType > SystemCount))
		goto fail;

	if	(CheckOpenFlags(OpenModeFlags) == GR_FALSE)
		goto fail;

	// Sugarcoating support for a taste test
	if	(FS == NULL && FileSystemType == GR_VFILE_TYPE_VIRTUAL)
	{
		assert(Name);
		FS = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS, Name, NULL,
									 OpenModeFlags & ~GR_VFILE_OPEN_DIRECTORY);
		if	(! FS)
			goto fail;
		Name = NULL;
	}
	else if	(FS)
	{
		grVFile_CreateRef(FS);
	}

	if	(FS)
		grVFile_Lock(FS);

	APIs = RegisteredAPIs[FileSystemType - 1];
	assert(APIs);
	FSData = APIs->OpenNewSystem(FS, Name, Context, OpenModeFlags);
	if	(FS)
		grVFile_UnLock(FS);

	if	(!FSData)
	{
		if	(FS)
			grVFile_Close(FS);
		goto fail;
	}

	File = grVFile_New();
	if	(!File)
	{
		if	(FS)
			grVFile_Close(FS);
		APIs->Close(FSData);
		goto fail;
	}

	File->SystemType =	FileSystemType;
	File->APIs = 		APIs;
	File->FSData = 		FSData;
	
#ifndef LN_SEARCHLIST
	File->SearchList = 	grRam_Allocate(sizeof(*File->SearchList));
#endif

	File->RefCount = 	0;
	File->Context =		FS;

#ifdef LN_SEARCHLIST
	LN_InitList(File);
	File->LN_Children.Next = (LinkNode *)File;
	File->LN_Children.Prev = (LinkNode *)File;
#endif

	File->Semaphore = grThreadQueue_Semaphore_Create();
	if ( ! File->Semaphore )
	{
		grVFile_Close(File);
		goto fail;	
	}

	assert( File->Context != File );

#ifndef LN_SEARCHLIST

	if	(!File->SearchList)
	{
		grVFile_Close(File);
		goto fail;
	}

	File->SearchList->FS 	= File;
	File->SearchList->Next	= NULL;
#endif

#ifdef _DEBUG
	VFiles_Open ++;
#endif

	assert( grVFile_IsValid(File) );

	return File;

fail:

	grVFile_Leave();

	return NULL;
}

GRAPI grVFile * GRCC grVFile_Open(
	grVFile *		FS,
	const char *	Name,
	unsigned int 	OpenModeFlags)
{
	grVFile *		StartContext;
	grVFile *		File;
	void *			FSData;
#ifndef LN_SEARCHLIST
	FSSearchList *	SearchList;
#endif

	assert( grVFile_IsValid(FS) );
	assert( grVFile_PathIsSane(Name) );

	if	(!FS)
		return NULL;

	if	(CheckOpenFlags(OpenModeFlags) == GR_FALSE)
		return NULL;

	StartContext = FS;

#ifdef KROUERDEBUG
	{
		char msg[80];
		sprintf(msg, "grVFile::Open %s\n", Name);
		OutputDebugString(msg);
	}
#endif

	// you can't lock FS up here, cuz FS is
	//	about to change!

#ifdef LN_SEARCHLIST
	if	(!(OpenModeFlags & GR_VFILE_OPEN_CREATE))
	{
	LinkNode * Head;	
		Head = &(StartContext->LN_Children);
		for( FS = (grVFile *)(Head->Next); FS != StartContext ; FS = (grVFile *)(FS->LN.Next) ) 
		{
			if	(FS->APIs->FileExists(FS, FS->FSData, Name))
				break;
		}

		if ( ! FS )
			return NULL;
	}
#else
	SearchList = FS->SearchList;
	assert(SearchList);
	assert(SearchList->FS == FS);
	if	(!(OpenModeFlags & GR_VFILE_OPEN_CREATE))
	{
		while	(SearchList)
		{
			FS = SearchList->FS;
			if	(FS->APIs->FileExists(FS, FS->FSData, Name))
				break;
			SearchList = SearchList->Next;
		}
	}

	if	(!SearchList)
		return NULL;
#endif

#if 1 //@@ CB debug : 
	{
		if ( FS != StartContext )
			Log_Printf("Chose FS != StartContext\n");
		assert(grVFile_IsValid(FS));
		assert(grVFile_IsValid(StartContext));
	}
#endif

	grVFile_Lock(FS);

	FSData = FS->APIs->Open(FS, FS->FSData, Name, NULL, OpenModeFlags);
	if	(!FSData)
	{
		grVFile_UnLock(FS);
		return NULL;
	}

	File = grVFile_New();
	if	(!File)
	{
		FS->APIs->Close(FSData);
		grVFile_UnLock(FS);
		return NULL;
	}

	File->SystemType =	GR_VFILE_TYPE_INVALID;
//	File->SystemType  = FS->SystemType;
	File->APIs = 		FS->APIs;
	File->FSData = 		FSData;	
#ifndef LN_SEARCHLIST
	File->SearchList = 	grRam_Allocate(sizeof(*File->SearchList));
#endif
	File->Context =		FS;
	File->RefCount = 	0;

	grVFile_CreateRef(FS); 

	grVFile_UnLock(FS);

#ifdef _DEBUG
	VFiles_Open ++;
#endif
		
#ifdef LN_SEARCHLIST
	LN_InitList(File);
	File->LN_Children.Next = (LinkNode *)File;
	File->LN_Children.Prev = (LinkNode *)File;
	LN_AddTail(&(StartContext->LN_Children),File);
#endif

	File->Semaphore = grThreadQueue_Semaphore_Create();
	if ( ! File->Semaphore )
	{
		grVFile_Close(File);
		return NULL;		
	}

#ifndef LN_SEARCHLIST
	if	(!File->SearchList)
	{
		grVFile_Close(File);
		return NULL;
	}

	File->SearchList->FS 	= File;
	File->SearchList->Next	= StartContext->SearchList;
#endif

#ifdef KROUERDEBUG
	{
		char msg[80];
		sprintf(msg, "grVFile::Open %s - open is ok %p\n", Name, File);
		OutputDebugString(msg);
	}
#endif

	assert( grVFile_IsValid(File) );

	return File;
}

GRAPI grBoolean GRCC grVFile_Close(grVFile *File)
{
grBoolean	Result = GR_TRUE;
grVFile * Context;
int RefCount;

	assert( grVFile_IsValid(File) );

#ifdef _DEBUG
	VFiles_Open --;
#endif

	RefCount = File->RefCount;
	Context = File->Context;

#ifdef KROUERDEBUG
	{
		char msg[80];
		strcpy(msg, "grVFile::Close      ");
		grVFile_GetName(File, msg+16, 60);
		strcat(msg, "\n");
		OutputDebugString(msg);
	}
#endif

	if ( ! RefCount )
	{

		if ( File->Semaphore )
			grVFile_Lock(File);

#ifdef LN_SEARCHLIST
		LN_Cut(File);
		LN_Cut(&(File->LN_Children));
#endif

		if ( File->FSData )
			if ( ! File->APIs->Close(File->FSData) )
				Result = GR_FALSE;
			
		if ( File->Semaphore )
		{
			grVFile_UnLock(File);
			grThreadQueue_Semaphore_Destroy(&(File->Semaphore));
		}

#ifndef LN_SEARCHLIST
		// <> never cuts self from parents' list !?
		if ( File->SearchList )
			grRam_Free(File->SearchList);
#endif

		grVFile_Free(File);
		File = NULL;
	}
	else
	{
		File->RefCount --;
	}

	if	( Context)
	{
		assert(Context->RefCount >= RefCount);
		if ( ! grVFile_Close(Context) )
			Result = GR_FALSE;
	}

return Result;
}

GRAPI grBoolean GRCC grVFile_Destroy(grVFile **pFile)
{
grVFile * File;
	assert(pFile);
	File = *pFile;
	if ( ! File )
		return GR_TRUE;
	*pFile = NULL;
return grVFile_Close(File);
}

#ifdef _DEBUG
GRAPI uint32 GRCC grVFile_OpenCount(void)
{
return VFiles_Open;
}
#endif

GRAPI void		 GRCC grVFile_CreateRef(grVFile *File)
{
	assert( grVFile_IsValid(File) );

	if	( File->Context)
	{
		grVFile_CreateRef(File->Context);
	}

#ifdef _DEBUG
	VFiles_Open ++;
#endif

	File->RefCount++;
}

GRAPI grBoolean GRCC grVFile_UpdateContext(grVFile *FS, void *Context, int ContextSize)
{
	grBoolean	Result;

	assert(Context);

	assert( grVFile_IsValid(FS) );

	grVFile_Lock(FS);
	Result = FS->APIs->UpdateContext(FS, FS->FSData, Context, ContextSize);
	grVFile_UnLock(FS);
	return Result;
}

GRAPI grVFile * GRCC grVFile_GetContext(const grVFile *File)
{
	assert( grVFile_IsValid(File) );

	return File->Context;
}

#ifndef LN_SEARCHLIST //{
static	void			DestroySearchList(FSSearchList *SearchList)
{
	while	(SearchList)
	{
		FSSearchList *	Temp;

		Temp = SearchList;
		SearchList = SearchList->Next;
		grRam_Free(Temp);
	}
}

static	FSSearchList *	CopySearchList(const FSSearchList *SearchList)
{
	FSSearchList *	NewList;
	FSSearchList *	Tail;

	NewList = Tail = NULL;
	while	(SearchList)
	{
		FSSearchList *	Temp;

		Temp = grRam_Allocate(sizeof(*Tail));
		if	(!Temp)
		{
			DestroySearchList(NewList);
			return NULL;
		}
		if	(Tail)
			Tail->Next = Temp;
		else
		{
			assert(!NewList);
			NewList = Temp;
		}
		Tail = Temp;
		Tail->FS = SearchList->FS;
		Tail->Next = NULL;
		SearchList = SearchList->Next;
	}
	return NewList;
}

GRAPI grBoolean GRCC grVFile_AddPath(grVFile *FS1, const grVFile *FS2, grBoolean Append)
{
	FSSearchList *	SearchList;

	assert(FS1);
	assert(FS2);
	assert( grVFile_IsValid(FS1) );
	assert( grVFile_IsValid(FS2) );

	grVFile_Lock(FS1);
	grVFile_Lock(FS2);

	SearchList = CopySearchList(FS2->SearchList);
	if	(!SearchList)
		goto fail;

	if	(Append == GR_FALSE)
	{
		SearchList->Next = FS1->SearchList;
		FS1->SearchList = SearchList;
	}
	else
	{
		FSSearchList	Temp;
		FSSearchList *	pTemp;

		Temp.Next = FS1->SearchList;
		pTemp = &Temp;
		while	(pTemp->Next)
		{
			pTemp = pTemp->Next;
		}

		pTemp->Next = SearchList;
//		SearchList->Next = NULL;
	}

	grVFile_UnLock(FS2);
	grVFile_UnLock(FS1);
	return GR_TRUE;

fail:
	grVFile_UnLock(FS2);
	grVFile_UnLock(FS1);
	return GR_FALSE;
}
#endif //}

GRAPI grBoolean GRCC grVFile_DeleteFile(grVFile *FS, const char *FileName)
{
	grBoolean	Result;

	assert( grVFile_IsValid(FS) );
	assert( grVFile_PathIsSane(FileName) );

	grVFile_Lock(FS);
	Result = FS->APIs->DeleteFile(FS, FS->FSData, FileName);
	grVFile_UnLock(FS);
	return Result;
}

GRAPI grBoolean GRCC grVFile_RenameFile(grVFile *FS, const char *FileName, const char *NewName)
{
	grBoolean	Result;

	assert( grVFile_IsValid(FS) );
	assert( grVFile_PathIsSane(FileName) );
	assert( grVFile_PathIsSane(NewName) );

	grVFile_Lock(FS);
	Result = FS->APIs->RenameFile(FS, FS->FSData, FileName, NewName);
	grVFile_UnLock(FS);
	return Result;
}

GRAPI grBoolean GRCC grVFile_FileExists(grVFile *FS, const char *FileName)
{
	assert( grVFile_PathIsSane(FileName) );
	assert( grVFile_IsValid(FS) );
	return FS->APIs->FileExists(FS, FS->FSData, FileName);
}

GRAPI grBoolean GRCC grVFile_Disperse(grVFile *FS, const char *Directory)
{
	grBoolean	Result;

	assert(Directory);
	assert( grVFile_IsValid(FS) );
	assert( grVFile_PathIsSane(Directory) );

	grVFile_Lock(FS);
	Result = FS->APIs->Disperse(FS, FS->FSData, Directory);
	grVFile_UnLock(FS);
	return Result;
}

GRAPI grBoolean GRCC grVFile_GetS(grVFile *File, void *Buff, int MaxLen)
{
	grBoolean	Result;

	assert(Buff);
	assert( grVFile_IsValid(File) );

	if	(MaxLen == 0)
		return GR_FALSE;

	grVFile_Lock(File);
	Result = File->APIs->GetS(File->FSData, Buff, MaxLen);
	grVFile_UnLock(File);
	
	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_BytesAvailable(grVFile *File, long *Count)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->BytesAvailable(File->FSData, Count);
	grVFile_UnLock(File);
	
	assert( grVFile_IsValid(File) );

	return Result;
}

grVFile * Hack_File;
grBoolean Hack_Used = 0;

GRAPI grBoolean GRCC grVFile_Read(grVFile *File, void *Buff, uint32 Count)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );

	if	(Count == 0) // <> CB 2/10
		return GR_TRUE;

	assert(Buff);

	grVFile_Lock(File);
//	assert( ! Hack_Used );
	Hack_File = File;
	Hack_Used ++;
	Result = File->APIs->Read(File->FSData, Buff, Count);
	Hack_Used --;
	grVFile_UnLock(File);
	
	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_Write(grVFile *File, const void *Buff, int Count)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );

	if	(Count == 0) // <> CB 2/10
		return GR_TRUE;

	assert(Buff);

#ifdef KROUERDEBUG
	{
		char msg[80];
		sprintf(msg, "grVFile::Write %d bytes\n", Count);
		OutputDebugString(msg);
	}
#endif

	grVFile_Lock(File);
	Result = File->APIs->Write(File->FSData, Buff, Count);
	grVFile_UnLock(File);
	
#ifdef KROUERDEBUG
	{
		char msg[80];
		sprintf(msg, "grVFile::Write result %d\n", Result);
		OutputDebugString(msg);
	}
#endif
	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_Rewind(grVFile *File)
{
grVFile * HintsFile;

	assert( grVFile_IsValid(File) );

	if ( ! grVFile_Seek(File,0,GR_VFILE_SEEKSET) )
		return GR_FALSE;

	HintsFile = grVFile_GetHintsFile(File);
	if ( HintsFile )
	{
		if ( ! grVFile_Seek(HintsFile,0,GR_VFILE_SEEKSET) )
			return GR_FALSE;
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grVFile_Seek(grVFile *File, int Where, grVFile_Whence Whence)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->Seek(File->FSData, Where, Whence);
	grVFile_UnLock(File);
	
	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_Printf(grVFile *File, const char *Format, ...)
{
	grBoolean	Result;
	char		Temp[8096];
	va_list		ArgPtr;

	assert( grVFile_IsValid(File) );
	assert(Format);

	va_start(ArgPtr, Format);
	vsprintf(Temp, Format, ArgPtr);
	va_end(ArgPtr);

	grVFile_Lock(File);
	Result = File->APIs->Write(File->FSData, &Temp[0], strlen(Temp));
	grVFile_UnLock(File);
	return Result;
}

GRAPI grBoolean GRCC grVFile_EOF   (const grVFile *File)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->Eof(File->FSData);
	grVFile_UnLock(File);
	return Result;
}

GRAPI grBoolean GRCC grVFile_Tell  (const grVFile *File, long *Position)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->Tell(File->FSData, Position);
	grVFile_UnLock(File);
	return Result;
}

GRAPI grBoolean GRCC grVFile_Size  (const grVFile *File, long *Size)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->Size(File->FSData, Size);
	grVFile_UnLock(File);

	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_GetProperties(const grVFile *File, grVFile_Properties *Properties)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->GetProperties(File->FSData, Properties);
	grVFile_UnLock(File);

	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_GetName(const grVFile *File, char *Buff, int MaxBuffLen)
{
grVFile_Properties Properties;
	if ( ! grVFile_GetProperties(File,&Properties) )
		return GR_FALSE;

	strncpy(Buff,Properties.Name,MaxBuffLen);

return GR_TRUE;
}

GRAPI grBoolean GRCC grVFile_SetSize(grVFile *File, long Size)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->SetSize(File->FSData, Size);
	grVFile_UnLock(File);

	assert( grVFile_IsValid(File) );

	return Result;
}

GRAPI grBoolean GRCC grVFile_SetAttributes(grVFile *File, grVFile_Attributes Attributes)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->SetAttributes(File->FSData, Attributes);
	grVFile_UnLock(File);
	return Result;
}

GRAPI grBoolean GRCC grVFile_SetTime(grVFile *File, const grVFile_Time *Time)
{
	grBoolean	Result;

	assert( grVFile_IsValid(File) );
	
	grVFile_Lock(File);
	Result = File->APIs->SetTime(File->FSData, Time);
	grVFile_UnLock(File);
	return Result;
}

GRAPI grVFile * GRCC grVFile_GetHintsFile(grVFile *File)
{
	grVFile *	Result;

#pragma message("grVFile_GetHintsFile : remove me for CreateHintsFile")

	assert( grVFile_IsValid(File) );

	if	(File->IsHintsFile == GR_TRUE)
		return NULL;

	grVFile_Lock(File);
	Result = File->APIs->GetHintsFile(File->FSData);
	if	(Result)
		Result->IsHintsFile = GR_TRUE;
	grVFile_UnLock(File);
	
	return Result;
}

GRAPI grVFile * GRCC grVFile_CreateHintsFile(grVFile *File)
{
	grVFile *	Result;

	assert( grVFile_IsValid(File) );

	if	(File->IsHintsFile == GR_TRUE)
		return NULL;

	grVFile_Lock(File);
	Result = File->APIs->GetHintsFile(File->FSData);
	if	(Result)
		Result->IsHintsFile = GR_TRUE;
	grVFile_UnLock(File);
	
	if ( Result )
	{
	/*
	// no; you can't do this: 
	// Context will call Close on the HintsFile
	// and then HintsFile will Close its context, which will ...
		assert( Result->Context == NULL || Result->Context == File );
		if ( Result->Context != File )
		{
		int r;
			for(r = File->RefCount;r>=0;r--)
				grVFile_CreateRef(File);
		}
		Result->Context = File;
	*/
		grVFile_CreateRef(Result);
	}

	return Result;
}

GRAPI grVFile_Finder * GRCC grVFile_CreateFinder(
	grVFile *FileSystem,
	const char *FileSpec)
{
	grVFile_Finder *	Finder;

	assert(FileSystem);
	assert(FileSpec);
	assert( grVFile_IsValid(FileSystem) );
	assert( grVFile_PathIsSane(FileSpec) );

	// CB : I don't think we use Finders enough to justify a MemPool

	Finder = (grVFile_Finder *)grRam_Allocate(sizeof(grVFile_Finder));
	if	(!Finder)
		return Finder;

	grVFile_Lock(FileSystem);
	Finder->Data = FileSystem->APIs->FinderCreate(FileSystem, FileSystem->FSData, FileSpec);
	grVFile_UnLock(FileSystem);
	if	(!Finder->Data)
	{
		grRam_Free(Finder);
		return NULL;
	}

	Finder->APIs = FileSystem->APIs;
       
    #ifdef WIN32
    InitializeCriticalSection(&Finder->CriticalSection);
    #endif
        
    #ifdef BUILD_BE
    Finder->CriticalSection = create_sem(1,NULL);
    #endif
        
	return Finder;
}

GRAPI void GRCC grVFile_DestroyFinder(grVFile_Finder *Finder)
{
	assert(Finder);
	assert(Finder->APIs);

	LOCK_CRITICALSECTION(&Finder->CriticalSection);
	Finder->APIs->FinderDestroy(Finder->Data);
	UNLOCK_CRITICALSECTION(&Finder->CriticalSection);
	DELETE_CRITICALSECTION(&Finder->CriticalSection);
	grRam_Free(Finder);
}

GRAPI grBoolean GRCC grVFile_FinderGetNextFile(grVFile_Finder *Finder)
{
	grBoolean	Result;

	assert(Finder);
	assert(Finder->APIs);
	assert(Finder->Data);
	
	LOCK_CRITICALSECTION(&Finder->CriticalSection);
	Result = Finder->APIs->FinderGetNextFile(Finder->Data);
	UNLOCK_CRITICALSECTION(&Finder->CriticalSection);
	return Result;
}

GRAPI grBoolean GRCC grVFile_FinderGetProperties(const grVFile_Finder *Finder, grVFile_Properties *Properties)
{
	grBoolean	Result;

	assert(Finder);
	assert(Finder->APIs);
	assert(Finder->Data);

	LOCK_CRITICALSECTION(&((grVFile_Finder *)Finder)->CriticalSection);
	Result = Finder->APIs->FinderGetProperties(Finder->Data, Properties);
	UNLOCK_CRITICALSECTION(&((grVFile_Finder *)Finder)->CriticalSection);

	return Result;
}

#ifdef WIN32
GRAPI void GRCC grVFile_TimeToWin32FileTime(const grVFile_Time *Time, LPFILETIME Win32FileTime)
{
        *Win32FileTime = *(LPFILETIME)Time;
}
#endif

#ifdef BUILD_BE
GRAPI void GRCC grVFile_TimeToTime_TFileTime(const grVFile_Time *Time, bigtime_t* fileTime)
{
        // not sure how we do this yet..
        memcpy(fileTime,Time,sizeof(int64));
}
#endif

static	grBoolean	GRCC	CheckOpenFlags(unsigned int OpenModeFlags)
{
	int 			FlagCount;
	unsigned int	AccessFlags;

	// Test to see that the open mode for this thing is mutually exclusive in
	// the proper flags.
	FlagCount = 0;
	AccessFlags = OpenModeFlags & (GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_UPDATE | GR_VFILE_OPEN_CREATE);
	if	(AccessFlags & GR_VFILE_OPEN_READONLY)
		FlagCount++;
	if	(AccessFlags & GR_VFILE_OPEN_UPDATE)
		FlagCount++;
	if	(AccessFlags & GR_VFILE_OPEN_CREATE)
		FlagCount++;

	if	(FlagCount != 1 && !(OpenModeFlags & GR_VFILE_OPEN_DIRECTORY))
		return GR_FALSE;

	return GR_TRUE;
}

static grBoolean grVFile_PathIsSane(const char * Path)
{
char * SubStr; 

	// look for paths that will crash your computer

	assert(Path);

	if ( (SubStr = strstr(Path,"//")) != NULL )
	{
		assert( SubStr[0] == '/' && SubStr[1] == '/' );
		if ( SubStr[-1] != ':' )
			return GR_FALSE;
	}
	if ( strstr(Path,"///") != NULL )
		return GR_FALSE;
	if ( (SubStr = strstr(Path,"\\\\")) != NULL )
	{
		if ( SubStr != Path )
			return GR_FALSE;
		if ( Path[2] == 0 )
			return GR_FALSE;
	}
return GR_TRUE;
}

#ifndef NDEBUG
GRAPI grBoolean GRCC grVFile_IsValid(const grVFile * F)
{
	assert( F );
	assert( F->RefCount >= 0 );
//	assert( grRam_IsValidPtr(F) );
	
#ifndef LN_SEARCHLIST
	assert( grRam_IsValidPtr(F->SearchList) );
#else
	assert( F->LN.Next );
#endif
	assert( F->Semaphore );
	assert( F->IsHintsFile == GR_TRUE || F->IsHintsFile == GR_FALSE );
//	assert( F->SystemType != GR_VFILE_TYPE_INVALID ); // why is this commented out?
	assert( F->SystemType < GR_VFILE_TYPE_COUNT );
	if ( F->Context )
	{
		assert(F->Context->RefCount >= F->RefCount);
		if ( ! grVFile_IsValid(F->Context) )
			return GR_FALSE;
	}
return GR_TRUE;
}
#endif

/*}{**** VFile Ram *********/

#include	"MemPool.h"

static MemPool * VFilePool = NULL;
static int VFilesAllocated = 0;

static grVFile * GRCC grVFile_New(void)
{
grVFile * File;

	if ( ! VFilesAllocated )
	{
		assert( ! VFilePool );
		VFilePool = MemPool_Create(sizeof(grVFile),32,32);
		if ( ! VFilePool )
			return NULL;
	}

	File = (grVFile *)MemPool_GetHunk(VFilePool);
	assert(File);
	VFilesAllocated++;

return File;
}

static void GRCC grVFile_Free(grVFile * File)
{
	assert(File);
	assert(VFilePool);
	MemPool_FreeHunk(VFilePool,File);
	VFilesAllocated--;
	if ( ! VFilesAllocated )
	{
		MemPool_Destroy(&VFilePool);
		VFilePool = NULL;
	} 
}
