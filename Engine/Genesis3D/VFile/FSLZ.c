/****************************************************************************************/
/*  FSLZ.C                                                                              */
/*                                                                                      */
/*  Author:  Charles Bloom                                                              */
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

/***********

todos:

1. use the grErrorLog * for threads

************/

// Threading the decompressor is probably pointless, cuz its so fast
#define FSLZ_ALWAYS_THREAD_READER 0
//#define FSLZ_ALWAYS_THREAD_READER 1

#ifndef _LOG
#define _LOG
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"BaseType.h"
#include	"Ram.h"
#include	"ThreadQueue.h"

#include	"VFile.h"
#include	"VFile._h"

#include	"FSLZ.h"
#include	"lza.h"
#include	"Log.h"

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#define FSLZ_TAG_TYPE	uint32
#define FSLZ_TAG		((FSLZ_TAG_TYPE)0x5A4C5346)	// "FSLZ"
#define FSLZ_TAG_UNC	((FSLZ_TAG_TYPE)0x4E555A4C)	// "LZUN"

typedef struct	LZFile
{
	FSLZ_TAG_TYPE	Tag;

	uint32		Reading;
	uint32		Uncompressed;
	uint32		Pos;
	uint32		Size;
	int32		HintsSize;

	grThreadQueue_Semaphore * Lock;	// the Lock applies to all stuff below:

	uint32		RefCount;
	grVFile *	BaseFile;	// never touched by the main-thread accessor functions
	grVFile *	HintsBaseFile;
	grVFile *	MemFile;
	grVFile *	HintsMemFile;
	uint32		MemFileLen;
	grThreadQueue_Job * ReaderJob;

	// stuff only used by the ReaderJob:
	lzaDecoder * Decoder;
	uint32		CompLen,CompLenRead;
	uint8 *		CompArray;
} LZFile;

/*}{******************* Protos ******************************/

static void __inline FSLZ_Lock(LZFile * File);
static void __inline FSLZ_UnLock(LZFile * File);

static	grBoolean	GRCC FSLZ_Close(void *Handle);
static	grBoolean	GRCC FSLZ_Close2(void *Handle,grBoolean Reader);
static	grBoolean	GRCC FSLZ_BytesAvailable(void *Handle, int32 *pCount);

void FSLZReader_Func(grThreadQueue_Job * Job,void * Context);
void FSLZReader_Peek(LZFile * File);
static grBoolean grVFile_CopyData(grVFile * Fm,grVFile *To,int Size);

/*}{******************* Implemented ******************************/

static	void *	GRCC FSLZ_OpenNewSystem(
	grVFile *		FS,
	const char *	Name,
	void *			Context,
	unsigned int			OpenModeFlags)
{
LZFile * File;

	if ( Name || Context || !FS )
		return NULL;

	if	(OpenModeFlags & GR_VFILE_OPEN_DIRECTORY)
		return NULL;

	File = (LZFile *)grRam_AllocateClear(sizeof(*File));
	if	(!File)
		return NULL;

	File->BaseFile = FS;
	grVFile_CreateRef(File->BaseFile);

	File->HintsBaseFile = grVFile_GetHintsFile(File->BaseFile);
	if ( ! File->HintsBaseFile )
		File->HintsBaseFile = File->BaseFile;
	grVFile_CreateRef(File->HintsBaseFile);

	File->Tag = FSLZ_TAG;

	if ( OpenModeFlags & GR_VFILE_OPEN_READONLY)
		File->Reading = GR_TRUE;

	if ( File->Reading )
	{
		grVFile_Read(File->HintsBaseFile,&(File->Tag),sizeof(File->Tag));
		if ( File->Tag == FSLZ_TAG_UNC )
		{
		grVFile_MemoryContext MemContext;
		grVFile * NewBaseFile,*NewHintsBaseFile;

			if ( ! grVFile_Read(File->HintsBaseFile,&(File->Size),sizeof(File->Size)) )
			{
				grErrorLog_AddString(-1,"FSLZ : Read Hints failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}
			
			if ( ! grVFile_Read(File->HintsBaseFile,&(File->HintsSize),sizeof(File->HintsSize)) )
			{
				grErrorLog_AddString(-1,"FSLZ_OpenNew : Base Read failed!",NULL);
				assert(0);
				FSLZ_Close(File);
				return NULL;
			}

			MemContext.DataLength = File->HintsSize;
			MemContext.Data = grRam_Allocate(MemContext.DataLength);
			if ( ! MemContext.Data )
			{
				grErrorLog_AddString(-1,"FSLZ : Allocate failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}

			if ( ! grVFile_Read(File->HintsBaseFile,MemContext.Data,MemContext.DataLength) )
			{
				grErrorLog_AddString(-1,"FSLZ : Allocate failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}

			NewHintsBaseFile = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_MEMORY,NULL,&MemContext,GR_VFILE_OPEN_READONLY);
			if ( ! File->HintsBaseFile )
			{
				FSLZ_Close(File);
				return NULL;
			}

			MemContext.DataLength = File->Size;
			MemContext.Data = grRam_Allocate(MemContext.DataLength);
			if ( ! MemContext.Data )
			{
				grErrorLog_AddString(-1,"FSLZ : Allocate failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}

			if ( ! grVFile_Read(FS,MemContext.Data,MemContext.DataLength) )
			{
				grErrorLog_AddString(-1,"FSLZ : Allocate failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}
			
			NewBaseFile = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_MEMORY,NULL,&MemContext,GR_VFILE_OPEN_READONLY);
			if ( ! File->BaseFile )
			{
				FSLZ_Close(File);
				return NULL;
			}

			File->Uncompressed = GR_TRUE;

			grVFile_Close(File->HintsBaseFile);
			grVFile_Close(File->BaseFile);

			File->HintsBaseFile = NewHintsBaseFile;
			File->BaseFile = NewBaseFile;

			return File;
		}
		else if ( File->Tag != FSLZ_TAG )
		{
			grVFile_Seek(File->HintsBaseFile,- (int)sizeof(File->Tag),GR_VFILE_SEEKCUR);
			grErrorLog_AddString(-1,"FSLZ : Opening uncompressed without UNC header!",NULL);
			#pragma message("FSLZ : UNC NoHeader open may pass back hintsfile == file !")
			File->Uncompressed = GR_TRUE;

			return File;
		}
	}

	{
	grVFile_MemoryContext MemContext;
		MemContext.Data = NULL;
		MemContext.DataLength = 0;

		File->MemFile = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_MEMORY,NULL,&MemContext,OpenModeFlags);
		if ( ! File->MemFile )
		{
			FSLZ_Close(File);
			return NULL;
		}
		MemContext.Data = NULL;
		MemContext.DataLength = 0;

		File->HintsMemFile = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_MEMORY,NULL,&MemContext,OpenModeFlags);
		if ( ! File->MemFile )
		{
			FSLZ_Close(File);
			return NULL;
		}
	}

	if ( File->Reading )
	{
		if ( ! grVFile_Read(File->HintsBaseFile,&(File->Size),sizeof(File->Size)) )
		{
			grErrorLog_AddString(-1,"FSLZ_OpenNew : Base Read failed!",NULL);
			assert(0);
			FSLZ_Close(File);
			return NULL;
		}

		if ( ! grVFile_Read(File->HintsBaseFile,&(File->CompLen),sizeof(File->CompLen)) )
		{
			grErrorLog_AddString(-1,"FSLZ_OpenNew : Base Read failed!",NULL);
			assert(0);
			FSLZ_Close(File);
			return NULL;
		}

		if ( ! grVFile_Read(File->HintsBaseFile,&(File->HintsSize),sizeof(File->HintsSize)) )
		{
			grErrorLog_AddString(-1,"FSLZ_OpenNew : Base Read failed!",NULL);
			assert(0);
			FSLZ_Close(File);
			return NULL;
		}

		if ( File->HintsSize > 0 )
		{
			if ( ! grVFile_SetSize(File->HintsMemFile,File->HintsSize) )
			{
				grErrorLog_AddString(-1,"FSLZ_OpenNew : Hints SetSize failed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}
		}

		// read hints
		
		if ( ! grVFile_CopyData(File->HintsBaseFile,File->HintsMemFile,File->HintsSize) )
		{
			grErrorLog_AddString(-1,"FSLZ_OpenNew : copy hints!",NULL);
			FSLZ_Close(File);
			return NULL;
		}
		grVFile_Rewind(File->HintsMemFile);

		if ( File->Size > 0 )
		{
			if ( ! grVFile_SetSize(File->MemFile,File->Size) )
			{
				grErrorLog_AddString(-1,"FSLZ_OpenNew : SetSize ailed!",NULL);
				FSLZ_Close(File);
				return NULL;
			}
		}

		if ( ! (File->CompArray = (uint8 *)grRam_Allocate(File->CompLen) ) )
		{
			grErrorLog_AddString(-1,"FSLZ_OpenNew : Allocate CompLen failed!",NULL);
			FSLZ_Close(File);
			return NULL;
		}

		{
		grVFile_Properties Prop;
		memset(&Prop, 0 , sizeof(grVFile_Properties) );
		
			File->RefCount++;
			if ( grVFile_GetProperties(File->BaseFile,&Prop) && (Prop.AttributeFlags & GR_VFILE_ATTRIB_REMOTE) || ( FSLZ_ALWAYS_THREAD_READER == 1) )
			{
				File->Lock = grThreadQueue_Semaphore_Create();
				if ( ! File->Lock )
				{
					grErrorLog_AddString(-1,"FSLZ_OpenNew : Semaphore_Create failed!",NULL);
					FSLZ_Close(File);
					return NULL;
				}

				File->ReaderJob = grThreadQueue_JobCreate(FSLZReader_Func,File,NULL,16384);
				if ( ! File->ReaderJob )
				{
					grErrorLog_AddString(-1,"FSLZ_OpenNew : Thread_Create failed!",NULL);
					FSLZ_Close(File);
					return NULL;
				}
			}
			else
			{
				FSLZReader_Func(NULL,File);
			}
		}
	}

return File;
}

static	grBoolean GRCC FSLZ_Close2(void *Handle,grBoolean Reader)
{
LZFile * File;
grBoolean Ret = GR_TRUE;
	
	File = (LZFile*)Handle;
	
	if ( File->Reading )
	{

		assert( ! Reader || File->RefCount > 0 );

		// must wait on Job BEFORE you Lock !

		if ( ! Reader && File->ReaderJob )
		{
			grThreadQueue_WaitOnJob(File->ReaderJob,GR_THREADQUEUE_STATUS_COMPLETED);
			grThreadQueue_JobDestroy(&(File->ReaderJob));
			File->ReaderJob = NULL;

			if ( File->Lock )
				grThreadQueue_Semaphore_Destroy(&(File->Lock));
		}

		// reader job must be gone now

		if ( File->RefCount > 0 )
		{
			File->RefCount--;
			return GR_TRUE;
		}

		if ( File->CompArray )
		{
			grRam_Free(File->CompArray);
			File->CompArray = NULL;
		}
		if ( File->Decoder )
		{
			lzaDecoder_Destroy(&(File->Decoder));
			File->Decoder = NULL;
		}

		// we're done
	}
	else
	{
		if ( File->RefCount > 0 )
		{
			File->RefCount--;
			return GR_TRUE;
		}

		{
		grVFile_MemoryContext MemContext;
			// compresss & write to BaseFile !
			if ( grVFile_UpdateContext(File->MemFile,&MemContext,sizeof(MemContext)) )
			{
			uint8 * OutBuf;
			uint32 OutLen;
				assert((int)File->Size == MemContext.DataLength);
				lzaEncode((uint8*)MemContext.Data,MemContext.DataLength,&OutBuf,&OutLen);
				if ( OutBuf && OutLen )
				{
					if ( OutLen < (uint32)MemContext.DataLength )
					{
						File->Tag = FSLZ_TAG;
						if (! grVFile_Write(File->HintsBaseFile,&(File->Tag),sizeof(File->Tag)) ||
							! grVFile_Write(File->HintsBaseFile,&(File->Size),sizeof(File->Size)) ||
							! grVFile_Write(File->HintsBaseFile,&OutLen,sizeof(OutLen)) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile_WriteHints failed!",NULL);
							Ret = GR_FALSE;
						}
									// write hints
			
						if ( ! grVFile_Size(File->HintsMemFile,&(File->HintsSize)) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile Hints Size failed!",NULL);
							Ret = GR_FALSE;
						}
						else
						{
							if (! grVFile_Write(File->HintsBaseFile,&(File->HintsSize),sizeof(File->HintsSize)) )
							{
								grErrorLog_AddString(-1,"FSLZ_Close : VFile_WriteHints failed!",NULL);
								Ret = GR_FALSE;
							}

							grVFile_Rewind(File->HintsMemFile);
							if ( ! grVFile_CopyData(File->HintsMemFile,File->HintsBaseFile,File->HintsSize) )
							{
								grErrorLog_AddString(-1,"FSLZ_Close : copy hints!",NULL);
								Ret = GR_FALSE;
							}
						}

						if ( ! grVFile_Write(File->BaseFile,OutBuf,OutLen) )						
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile_Write failed!",NULL);
							Ret = GR_FALSE;
						}

						Log_Printf("FSLZ : compressed %d -> %d = %1.3f bpc\n",MemContext.DataLength,OutLen,(OutLen*8.0f/MemContext.DataLength));

						#ifdef COUNT_HEADER_SIZES
						Header_Sizes += 12;
						#endif
						
						grRam_Free(OutBuf);
					}
					else
					{
						grRam_Free(OutBuf);

						// these hints cost 12 bytes :
						// 4 hints header
						// 4 hints len
						// 4 hints bytes
						// we could avoid this if we had VFile_UnWriteHints
						
						#pragma message("FSLZ : try passing through uncompressed without UNC header")

						File->Tag = FSLZ_TAG_UNC;
						if (	! grVFile_Write(File->HintsBaseFile,&(File->Tag),sizeof(File->Tag))
							||	! grVFile_Write(File->HintsBaseFile,&(MemContext.DataLength),sizeof(MemContext.DataLength)) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile_WriteHints failed!",NULL);
							Ret = GR_FALSE;
						}
						else if ( ! grVFile_Size(File->HintsMemFile,&(File->HintsSize)) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile Hints Size failed!",NULL);
							Ret = GR_FALSE;
						}
						else if (! grVFile_Write(File->HintsBaseFile,&(File->HintsSize),sizeof(File->HintsSize)) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile_WriteHints failed!",NULL);
							Ret = GR_FALSE;
						}
						else if ( ! grVFile_Rewind(File->HintsMemFile) ||
								! grVFile_CopyData(File->HintsMemFile,File->HintsBaseFile,File->HintsSize) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : copy hints!",NULL);
							Ret = GR_FALSE;
						}
						else if ( ! grVFile_Write(File->BaseFile,MemContext.Data,MemContext.DataLength) )
						{
							grErrorLog_AddString(-1,"FSLZ_Close : VFile_Write failed!",NULL);
							Ret = GR_FALSE;
						}
					}
				}
				else
				{
					grErrorLog_AddString(-1,"FSLZ_Close : lzaEncode failed!",NULL);
					Ret = GR_FALSE;
				}
			}

		}
	}

	if ( File->BaseFile )
		grVFile_Close(File->BaseFile);
	if ( File->HintsBaseFile )
		grVFile_Close(File->HintsBaseFile);
	if ( File->MemFile )
		grVFile_Close(File->MemFile);
	if ( File->HintsMemFile )
		grVFile_Close(File->HintsMemFile);

	grRam_Free(File);

return Ret;
}

static	grBoolean GRCC FSLZ_Close(void *Handle)
{
return FSLZ_Close2(Handle,GR_FALSE);
}

/*}{******************* The Reader Job ******************************/

void FSLZReader_Func(grThreadQueue_Job * Job,void * Context)
{
LZFile * File;
uint32 CurLen;
uint32 NewMemFileLen;

	File = (LZFile*)Context;

	while( File->CompLenRead < File->CompLen )
	{

		grVFile_BytesAvailable(File->BaseFile,(int32 *)&CurLen);
		while ( CurLen < 10 && (CurLen + File->CompLenRead != File->CompLen) )
		{
			if ( grVFile_EOF(File->BaseFile) )
				goto fail;
			grThreadQueue_Sleep(10);
			grVFile_BytesAvailable(File->BaseFile,(int32 *)&CurLen);
		}

		CurLen = min(CurLen, File->CompLen - File->CompLenRead );

		FSLZ_Lock(File);
		grVFile_BytesAvailable(File->BaseFile,(int32 *)&CurLen);
		
		if ( grVFile_Read(File->BaseFile,File->CompArray + File->CompLenRead, CurLen ) )
		{
			if ( File->CompLenRead == 0 )
			{
			grVFile_MemoryContext MemContext;
				if ( ! grVFile_UpdateContext(File->MemFile,&MemContext,sizeof(MemContext)) )
				{
					FSLZ_UnLock(File);
					goto fail;
				}

				File->Decoder = lzaDecoder_Create(File->CompArray,File->CompLen,CurLen,(uint8*)MemContext.Data,MemContext.DataLength);
				if ( ! File->Decoder )
				{
					FSLZ_UnLock(File);
					goto fail;
				}
					
				lzaDecoder_Extend(File->Decoder,0,&NewMemFileLen);
				assert( NewMemFileLen >= File->MemFileLen );
				File->MemFileLen = NewMemFileLen;
					
			}
			else
			{
				lzaDecoder_Extend(File->Decoder,CurLen,&NewMemFileLen);
				assert( NewMemFileLen >= File->MemFileLen );
				File->MemFileLen = NewMemFileLen;
			}
			File->CompLenRead += CurLen;
			assert( File->CompLenRead <= File->CompLen );

			Log_Printf("FSLZ : decompressed %d -> %d\n",File->CompLenRead,File->MemFileLen);
		}

		// we don't have to tell the MemFile that we've added to it!

#if 0 // for debugging ; should be a "do nothing"
		if ( File->CompLenRead == File->CompLen )
		{
			lzaDecoder_Extend(File->Decoder,0,&NewMemFileLen);
		}
#endif

		FSLZ_UnLock(File);
	}

fail:

	FSLZ_Lock(File);

	if ( File->CompArray )
	{
		grRam_Free(File->CompArray);
		File->CompArray = NULL;
	}
	if ( File->Decoder )
	{
		lzaDecoder_Destroy(&(File->Decoder));
		File->Decoder = NULL;
	}
	
	FSLZ_UnLock(File);

	FSLZ_Close2(File,GR_TRUE);

return;
}

void FSLZReader_Peek(LZFile * File)
{
grThreadQueue_JobStatus Status;

	if ( ! File->ReaderJob )
		return;

	FSLZ_Lock(File);
	
	Status = grThreadQueue_JobGetStatus(File->ReaderJob);

	FSLZ_UnLock(File);

	if ( Status == GR_THREADQUEUE_STATUS_WAITINGFORTHREAD )
	{
		grThreadQueue_PollJobs();
	}
	else if ( Status == GR_THREADQUEUE_STATUS_COMPLETED )
	{
		FSLZ_Lock(File);
		grThreadQueue_JobDestroy(&(File->ReaderJob));
		File->ReaderJob = NULL;
		FSLZ_UnLock(File);
	}
}

/*}{******************* Utilities ******************************/

static grBoolean grVFile_CopyData(grVFile * Fm,grVFile *To,int CurSize)
{
char CopyBuff[1024];
int CurLen;
	while(CurSize)
	{
		CurLen = min(1024,CurSize);
		if ( ! grVFile_Read( Fm,CopyBuff,CurLen) )
			return GR_FALSE;
		if ( ! grVFile_Write(To,CopyBuff,CurLen) )
			return GR_FALSE;
		CurSize -= CurLen;
	}
return GR_TRUE;
}

static void __inline FSLZ_Lock(LZFile * File)
{
	if ( File->ReaderJob )
		grThreadQueue_Semaphore_Lock(File->Lock);
}

static void __inline FSLZ_UnLock(LZFile * File)
{
	if ( File->ReaderJob )
		grThreadQueue_Semaphore_UnLock(File->Lock);
}


/*}{******************* Pass-Throughs ******************************/

static	grBoolean	GRCC FSLZ_BytesAvailable(void *Handle, int32 *pCount)
{
	LZFile *	File;

	File = (LZFile*)Handle;

	if ( ! File->Reading )
		return GR_FALSE;

	if ( File->Uncompressed )
		return grVFile_BytesAvailable(File->BaseFile,pCount);

	FSLZReader_Peek(File);

	FSLZ_Lock(File);

	*pCount = (File->MemFileLen - File->Pos);

	FSLZ_UnLock(File);
	
return GR_TRUE;
}

static	grBoolean	GRCC FSLZ_Read(void *Handle, void *Buff, uint32 Count)
{
LZFile * File;
uint32 Avail;

	File = (LZFile*)Handle;

	if ( ! File->Reading )
		return GR_FALSE;

	if ( File->Uncompressed )
	{
		return grVFile_Read(File->BaseFile,Buff,Count);
	}
	
	if ( Count > (File->Size - File->Pos) )
		return GR_FALSE;
	if ( Count <= 0 )
		return GR_FALSE;

	FSLZ_BytesAvailable(Handle,(int32 *)&Avail);
	
	if ( Avail < Count )
	{
		if ( ! File->ReaderJob )
			return GR_FALSE;

		if ( ! grThreadQueue_WaitOnJob(File->ReaderJob,GR_THREADQUEUE_STATUS_RUNNING) )
			return GR_FALSE;
			
		do
		{
			grThreadQueue_Sleep(1);
			FSLZ_BytesAvailable(Handle,(int32 *)&Avail);
		} while( Avail < Count );
	}

	if ( File->ReaderJob )
	{
	grThreadQueue_JobStatus Status;	
		Status = grThreadQueue_JobGetStatus(File->ReaderJob);
		if ( Status == GR_THREADQUEUE_STATUS_COMPLETED )
		{
			FSLZ_Lock(File);
			grThreadQueue_JobDestroy(&File->ReaderJob);
			File->ReaderJob = NULL;
			FSLZ_UnLock(File);
		}
	}
	
	FSLZ_Lock(File);

	if ( ! grVFile_Read(File->MemFile,Buff,Count) )
		return GR_FALSE;

	FSLZ_UnLock(File);

	File->Pos += Count;
	return GR_TRUE;
}

static	grBoolean	GRCC FSLZ_Write(void *Handle, const void *Buff, int Count)
{
LZFile * File;

	File = (LZFile*)Handle;

	if ( File->Reading )
		return GR_FALSE;

	if ( ! grVFile_Write(File->MemFile,Buff,Count) )
		return GR_FALSE;

	File->Pos += Count;
	if ( File->Pos > File->Size )
		File->Size = File->Pos;
	File->MemFileLen = File->Size;

	return GR_TRUE;
}

static	grBoolean	GRCC FSLZ_Seek(void *Handle, int Where, grVFile_Whence Whence)
{
LZFile * File;
uint32 NewPos;

	File = (LZFile*)Handle;

	if ( File->Uncompressed )
		return grVFile_Seek(File->BaseFile,Where,Whence);
	
	switch(Whence)
	{
		case GR_VFILE_SEEKCUR:
			NewPos = File->Pos + Where;
			break;

		case GR_VFILE_SEEKEND:
			NewPos = File->Size + Where;
			break;

		case GR_VFILE_SEEKSET:
			NewPos = Where;
			break;
	}

	if ( File->Reading )
	{
	uint32 Len;
		FSLZ_Lock(File);
		Len = File->MemFileLen;
		FSLZ_UnLock(File);
		if ( NewPos > Len )
			 return GR_FALSE;
	}

	File->Pos = NewPos;

	return grVFile_Seek(File->MemFile,Where,Whence);
}

static	grBoolean	GRCC FSLZ_EOF(const void *Handle)
{
	const LZFile *	File;

	File = (LZFile*)Handle;

	if ( File->Uncompressed )
		return grVFile_EOF(File->BaseFile);

	if ( File->Pos == File->Size )
		return GR_TRUE;

	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_Tell(const void *Handle, int32 *pPosition)
{
	const LZFile *	File;

	File = (LZFile*)Handle;

	if ( File->Uncompressed )
		return grVFile_Tell(File->BaseFile,pPosition);

	*pPosition = File->Pos;

	return GR_TRUE;
}

static	grBoolean	GRCC FSLZ_Size(const void *Handle, int32 *pSize)
{
	const LZFile *	File;

	File = (LZFile*)Handle;

	if ( File->Uncompressed )
		return grVFile_Size(File->BaseFile,pSize);

	*pSize = File->Size;

	return GR_TRUE;
}


static	grVFile *	GRCC FSLZ_GetHintsFile(void *Handle)
{
LZFile *	File;

	File = (LZFile*)Handle;

	if ( File->Uncompressed )
		return File->HintsBaseFile;

	if ( File->Reading )
	{
	int32 Size;
		if ( ! grVFile_Size(File->HintsMemFile,&Size) )
			return NULL;
		if ( ! Size )
			return NULL;
	}

	return File->HintsMemFile;
}

static	int GRCC FSLZ_GetC(LZFile * File)
{
unsigned char C;
	if ( ! FSLZ_Read(File,&C,1) )
		return -1;
return C;
}

static	grBoolean	GRCC FSLZ_GetS(void *Handle, char *Buff, int MaxLen)
{
LZFile *	File;
int C;
char * Ptr;

	File = (LZFile*)Handle;

	if ( ! File->Reading )
		return GR_FALSE;

	Ptr = Buff;
	while( MaxLen > 1 && (C = FSLZ_GetC(File)) != -1 )
	{	
		*Ptr++ = C;
		MaxLen--;
		if (C == '\n' || C == '\r' || C == 0 )
			break;		
	}
	
	while( (C = FSLZ_GetC(File)) != -1 )
	{
		if (C == '\n' || C == '\r' || C == 0 )
			continue;

		if ( ! FSLZ_Seek(File,-1,GR_VFILE_SEEKCUR) )
			return GR_FALSE;
		break;
	}

	*Ptr = 0;

return GR_TRUE;
}

static	grBoolean	GRCC FSLZ_GetProperties(const void *Handle, grVFile_Properties *Properties)
{
const LZFile * File;

	File = (LZFile*)Handle;

	if ( ! grVFile_GetProperties(File->BaseFile,Properties) )
		return GR_FALSE;
	
	Properties->Size = File->Size;
return GR_TRUE;
}

/*}{******************* UnImplemented Bullshit ******************************/

static	void *	GRCC FSLZ_FinderCreate(
	grVFile *			FS,
	void *			Handle,
	const char *	FileSpec)
{
	return NULL;
}

static	grBoolean	GRCC FSLZ_FinderGetNextFile(void *Handle)
{
	assert(!Handle);
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_FinderGetProperties(void *Handle, grVFile_Properties *Props)
{
	assert(!Handle);
	return GR_FALSE;
}

static	void GRCC FSLZ_FinderDestroy(void *Handle)
{
	assert(!Handle);
}

static	void *	GRCC FSLZ_Open(
	grVFile *		FS,
	void *			Handle,
	const char *	Name,
	void *			Context,
	unsigned int			OpenModeFlags)
{
	return NULL;
}


static	grBoolean	GRCC FSLZ_SetSize(void *Handle, int32 Size)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_SetAttributes(void *Handle, grVFile_Attributes Attributes)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_SetTime(void *Handle, const grVFile_Time *Time)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_FileExists(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_Disperse(
	grVFile *	FS,
	void *		Handle,
	const char *Directory)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_DeleteFile(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_RenameFile(grVFile *FS, void *Handle, const char *Name, const char *NewName)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSLZ_UpdateContext(
	grVFile *		FS,
	void *			Handle,
	void *			Context,
	int 			ContextSize)
{
	return GR_FALSE;
}

/*}{******************* The FSLZ Struct ******************************/

#pragma warning (disable : 4113 4028)

static	grVFile_SystemAPIs	FSLZ_APIs =
{
	FSLZ_FinderCreate,
	FSLZ_FinderGetNextFile,
	FSLZ_FinderGetProperties,
	FSLZ_FinderDestroy,

	FSLZ_OpenNewSystem,
	FSLZ_UpdateContext,
	FSLZ_Open,
	FSLZ_DeleteFile,
	FSLZ_RenameFile,
	FSLZ_FileExists,
	FSLZ_Disperse,
	FSLZ_Close,

	FSLZ_GetS,
	FSLZ_BytesAvailable,
	FSLZ_Read,
	FSLZ_Write,
	FSLZ_Seek,
	FSLZ_EOF,
	FSLZ_Tell,
	FSLZ_Size,

	FSLZ_GetProperties,

	FSLZ_SetSize,
	FSLZ_SetAttributes,
	FSLZ_SetTime,

	FSLZ_GetHintsFile,
};

const grVFile_SystemAPIs * GRCC FSLZ_GetAPIs(void)
{
	return &FSLZ_APIs;
}

/*}{******************* EOF ******************************/
