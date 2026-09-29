/****************************************************************************************/
/*  FSFAKENET.C                                                                         */
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

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"BaseType.h"
#include	"Ram.h"
#include	"ThreadQueue.h"

#include	"VFile.h"
#include	"VFile._h"

#include	"fsfakenet.h"
#include	"Log.h"
#include	"Tsc.h"

double FakeNet_ByPerSec		= 100.0f;
double FakeNet_LagSec		= 0.5f;
double FakeNet_FirstLagSec	= 1.0f;

typedef struct	FakeNetFile
{
	uint32		Pos;
	double		OpenTime,AvailTime;

	uint32		RefCount;
	grVFile *	BaseFile;
} FakeNetFile;

/*}{******************* Protos ******************************/

static	grBoolean	GRCC FSFakeNet_Close(void *Handle);
static	grBoolean	GRCC FSFakeNet_BytesAvailable(void *Handle, long *pCount);
static	grBoolean	GRCC FSFakeNet_Seek(void *Handle, int Where, grVFile_Whence Whence);

/*}{******************* Open/Close ******************************/

static	void *	GRCC FSFakeNet_OpenNewSystem(
	grVFile *		FS,
	const char *	Name,
	void *			Context,
	unsigned int	OpenModeFlags)
{
FakeNetFile * File;

	if ( Name || Context || !FS )
		return NULL;

	File = (FakeNetFile *)grRam_AllocateClear(sizeof(*File));
	if	(!File)
		return NULL;

	File->BaseFile = FS;
	grVFile_CreateRef(File->BaseFile);

	File->OpenTime = timeTSC();
	File->AvailTime = File->OpenTime;

return File;
}

static	grBoolean GRCC FSFakeNet_Close(void *Handle)
{
FakeNetFile * File;
	File = (FakeNetFile *)Handle;

	if ( File->RefCount > 0 )
	{
		File->RefCount--;
		return GR_TRUE;
	}

	if ( File->BaseFile )
		grVFile_Close(File->BaseFile);

	grRam_Free(File);

return GR_TRUE;
}

/*}{******************* The Fake Net ******************************/

static	grBoolean	GRCC FSFakeNet_BytesAvailable(void *Handle, long *pCount)
{
FakeNetFile * File;
int32 AllowLen,CurLen;
double CurTime,LagTime;

	File = (FakeNetFile *)Handle;

	if ( ! grVFile_BytesAvailable(File->BaseFile,pCount) )
		return GR_FALSE;

	CurTime = timeTSC();

	if ( File->Pos == 0 )
		LagTime = FakeNet_FirstLagSec;
	else
		LagTime = FakeNet_LagSec;

	if ( CurTime < (File->AvailTime + LagTime) )
	{
		*pCount = 0;
		return GR_TRUE;
	}

	if ( ! grVFile_Tell(File->BaseFile,(long *)&CurLen) )
		return GR_FALSE;

	File->AvailTime = CurTime;

	AllowLen = (int)((CurTime - File->OpenTime) * FakeNet_ByPerSec) - CurLen;

	if ( *pCount > AllowLen )
		*pCount = AllowLen;

return GR_TRUE;
}

static	grBoolean	GRCC FSFakeNet_Read(void *Handle, char *Buff, uint32 Count)
{
FakeNetFile * File;
uint32 Avail;

	File = (FakeNetFile *)Handle;
	
	do
	{
		if ( ! FSFakeNet_BytesAvailable(Handle,&Avail) )
			return GR_FALSE;

		if ( Avail == 0 )
			grThreadQueue_Sleep(10);
		else if ( Avail < Count )
		{
			if ( ! grVFile_Read(File->BaseFile,Buff,Avail) )
				return GR_FALSE;
			Buff 		+= Avail;
			Count 		-= Avail;

			Avail = 0;

			if ( grVFile_EOF(File->BaseFile) )
				return GR_FALSE;
		}

	} while( Avail < Count );

return grVFile_Read(File->BaseFile,Buff,Count);
}

static	int GRCC FSFakeNet_GetC(FakeNetFile * File)
{
 char C;
	if ( ! FSFakeNet_Read(File,&C,1) )
		return -1;
return C;
}

static	grBoolean	GRCC FSFakeNet_GetS(void *Handle, char *Buff, int MaxLen)
{
FakeNetFile *	File;
int C;
char * Ptr;

	File = (FakeNetFile *)Handle;

	Ptr = Buff;
	while( MaxLen > 1 && (C = FSFakeNet_GetC(File)) != -1 )
	{	
		*Ptr++ = C;
		MaxLen--;
		if (C == '\n' || C == '\r' || C == 0 )
			break;		
	}
	
	while( (C = FSFakeNet_GetC(File)) != -1 )
	{
		if (C == '\n' || C == '\r' || C == 0 )
			continue;

		if ( ! FSFakeNet_Seek(File,-1,GR_VFILE_SEEKCUR) )
			return GR_FALSE;
		break;
	}

	*Ptr = 0;

return GR_TRUE;
}

static	grBoolean	GRCC FSFakeNet_GetProperties(const void *Handle, grVFile_Properties *Properties)
{
const FakeNetFile * File;

	File = (FakeNetFile *)Handle;

	if ( ! grVFile_GetProperties(File->BaseFile,Properties) )
		return GR_FALSE;
	
	Properties->AttributeFlags |= GR_VFILE_ATTRIB_REMOTE;

return GR_TRUE;
}

/*}{******************* Pass-Throughs ******************************/

static	grBoolean	GRCC FSFakeNet_Write(void *Handle, const void *Buff, int Count)
{
FakeNetFile * File;
	File = (FakeNetFile *)Handle;

return grVFile_Write(File->BaseFile,Buff,Count);
}

static	grBoolean	GRCC FSFakeNet_Seek(void *Handle, int Where, grVFile_Whence Whence)
{
FakeNetFile * File;
	File = (FakeNetFile *)Handle;

return grVFile_Seek(File->BaseFile,Where,Whence);
}

static	grBoolean	GRCC FSFakeNet_EOF(const void *Handle)
{
const FakeNetFile *	File;
	File = (FakeNetFile *)Handle;

return grVFile_EOF(File->BaseFile);
}

static	grBoolean	GRCC FSFakeNet_Tell(const void *Handle, long *pPosition)
{
const FakeNetFile *	File;
	File = (FakeNetFile *)Handle;

return grVFile_Tell(File->BaseFile,pPosition);
}

static	grBoolean	GRCC FSFakeNet_Size(const void *Handle, long *pSize)
{
const FakeNetFile *	File;
	File = (FakeNetFile *)Handle;

return grVFile_Size(File->BaseFile,pSize);
}

static	grVFile *	GRCC FSFakeNet_GetHintsFile(void *Handle)
{
FakeNetFile *	File;
	File = (FakeNetFile *)Handle;

return grVFile_GetHintsFile(File->BaseFile);
}

/*}{******************* UnImplemented Bullshit ******************************/

static	void *	GRCC FSFakeNet_FinderCreate(
	grVFile *			FS,
	void *			Handle,
	const char *	FileSpec)
{
	return NULL;
}

static	grBoolean	GRCC FSFakeNet_FinderGetNextFile(void *Handle)
{
	assert(!Handle);
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_FinderGetProperties(void *Handle, grVFile_Properties *Props)
{
	assert(!Handle);
	return GR_FALSE;
}

static	void GRCC FSFakeNet_FinderDestroy(void *Handle)
{
	assert(!Handle);
}

static	void *	GRCC FSFakeNet_Open(
	grVFile *		FS,
	void *			Handle,
	const char *	Name,
	void *			Context,
	unsigned int	OpenModeFlags)
{
	return NULL;
}


static	grBoolean	GRCC FSFakeNet_SetSize(void *Handle, long Size)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_SetAttributes(void *Handle, grVFile_Attributes Attributes)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_SetTime(void *Handle, const grVFile_Time *Time)
{
	assert(!"Not implemented");
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_FileExists(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_Disperse(
	grVFile *	FS,
	void *		Handle,
	const char *Directory)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_DeleteFile(grVFile *FS, void *Handle, const char *Name)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_RenameFile(grVFile *FS, void *Handle, const char *Name, const char *NewName)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSFakeNet_UpdateContext(
	grVFile *		FS,
	void *			Handle,
	void *			Context,
	int 			ContextSize)
{
	return GR_FALSE;
}

/*}{******************* The FSFakeNet Struct ******************************/

#pragma warning (disable : 4113 4028)

static	grVFile_SystemAPIs	FSFakeNet_APIs =
{
	FSFakeNet_FinderCreate,
	FSFakeNet_FinderGetNextFile,
	FSFakeNet_FinderGetProperties,
	FSFakeNet_FinderDestroy,

	FSFakeNet_OpenNewSystem,
	FSFakeNet_UpdateContext,
	FSFakeNet_Open,
	FSFakeNet_DeleteFile,
	FSFakeNet_RenameFile,
	FSFakeNet_FileExists,
	FSFakeNet_Disperse,
	FSFakeNet_Close,

	FSFakeNet_GetS,
	FSFakeNet_BytesAvailable,
	FSFakeNet_Read,
	FSFakeNet_Write,
	FSFakeNet_Seek,
	FSFakeNet_EOF,
	FSFakeNet_Tell,
	FSFakeNet_Size,

	FSFakeNet_GetProperties,

	FSFakeNet_SetSize,
	FSFakeNet_SetAttributes,
	FSFakeNet_SetTime,

	FSFakeNet_GetHintsFile,
};

const grVFile_SystemAPIs * GRCC FSFakeNet_GetAPIs(void)
{
	return &FSFakeNet_APIs;
}

/*}{******************* EOF ******************************/
