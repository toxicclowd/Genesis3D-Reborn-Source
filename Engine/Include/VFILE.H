/*!
	@file VFile.h
	
	@author Eli Boling
	@brief Virtual file interface

	@par Licence
	The contents of this file are subject to the Jet3D Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.jet3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Jet3D, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/


#ifndef	VFILE_H
#define	VFILE_H

#include	"BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Krouer: enable/disable trace in VFile */
//#define KROUERDEBUG

/*! @typedef grVFile
	@brief A virtual file or filesystem, physic file or directory instance.
*/
typedef	struct	grVFile			grVFile;

//--------- Finder (Directory) --------------
typedef	struct	grVFile_Finder	grVFile_Finder;

typedef unsigned int    grVFile_Attributes;

// <> is this still used?
typedef struct	grVFile_Hints
{
	void *			HintData;
	unsigned int	HintDataLength;
}	grVFile_Hints;

typedef	struct	grVFile_Time
{
	unsigned long	Time1;
	unsigned long	Time2;
}	grVFile_Time;

#define	GR_VFILE_ATTRIB_READONLY	(1UL<<0)
#define	GR_VFILE_ATTRIB_DIRECTORY	(1UL<<1)
#define	GR_VFILE_ATTRIB_REMOTE		(1UL<<2)

typedef	struct	grVFile_Properties
{
	grVFile_Time			Time;
	grVFile_Attributes		AttributeFlags;
	long					Size;
//<>grVFile_Hints			Hints;
	char					Name[1024];
}	grVFile_Properties;

#ifdef _INC_WINDOWS
GRAPI void GRCC grVFile_TimeToWin32FileTime(const grVFile_Time *, LPFILETIME Win32FileTime);
	// Converts a grVFile time to a Win32 FILETIME structure.  This API is the
	// way to get the file time into a format to do standardized date/time
	// operations on.  We do not provide date/time operations natively.
#endif

#ifdef BUILD_BE
#ifdef _SUPPORT_DEFS_H
GRAPI void GRCC grVFile_TimeToTime_TFileTime(const grVFile_Time *Time, bigtime_t* fileTime);
#endif
#endif

GRAPI grVFile_Finder * GRCC grVFile_CreateFinder(
	grVFile *		FileSystem,
	const char *	FileSpec);
	// Creates a finder object from which you can get iterated file names.
	// This is findfirst/findnext functionality.  

GRAPI void GRCC grVFile_DestroyFinder(grVFile_Finder *Finder);
	// Destroys a Finder object

GRAPI grBoolean GRCC grVFile_FinderGetNextFile(grVFile_Finder *Finder);
	// Tracks to the next file in the finder directory

GRAPI grBoolean GRCC grVFile_FinderGetProperties(const grVFile_Finder *Finder, grVFile_Properties *Properties);
	// Gets the file properties from a grVFile_Finder.  You cannot set properties for
	// a file through a finder.  You have to set the properties through a grVFile.

//--------- File System Operations ----

typedef	struct	grVFile_MemoryContext
{
	void *	Data;
	int		DataLength;
}	grVFile_MemoryContext;

typedef	struct	grVFile_RemoteFileStatistics
{
	int		TotalBytesReceived;
	int		TotalBlocksReceived;
	int		MinBlockSize;
	int		MaxBlockSize;
}	grVFile_RemoteFileStatistics;

typedef enum {
	GR_VFILE_TYPE_INVALID = 0,
	GR_VFILE_TYPE_DOS,
	GR_VFILE_TYPE_MEMORY,
	GR_VFILE_TYPE_VIRTUAL,
	GR_VFILE_TYPE_LZ,
	GR_VFILE_TYPE_FAKENET,
	GR_VFILE_TYPE_INTERNET,
	GR_VFILE_TYPE_COUNT
} grVFile_TypeIdentifier;

// First three flags are mutually exclusive.  Combining them will result in failure
// returns for both grVFile_OpenNewSystem and grVFile_Open.
#define	GR_VFILE_OPEN_READONLY	 (1UL<<0)
#define	GR_VFILE_OPEN_UPDATE 	 (1UL<<1)
#define	GR_VFILE_OPEN_CREATE	 (1UL<<2)

#define GR_VFILE_OPEN_DIRECTORY  (1UL<<3)
#define	GR_VFILE_OPEN_RAW		 (1UL<<4)
				// Warning : VFile_Open_Raw is a new flag, 2/14/99 : it is not fully supported yet!
				//		use at your own risk!

GRAPI grVFile * GRCC grVFile_OpenNewSystem(
	grVFile *					FS,
	grVFile_TypeIdentifier	FileSystemType,  // { DOS, MEMORY, ETC ... },
	const char *			Name, 
	void *					Context, 
	unsigned int 			OpenModeFlags);
	// Opens a file / file system.

GRAPI grBoolean GRCC grVFile_UpdateContext(grVFile *FS, void *Context, int ContextSize);

GRAPI grVFile * GRCC grVFile_GetContext(const grVFile *File);
	// Returns the outer context in which File was opened.

GRAPI grVFile * GRCC grVFile_Open( 
	grVFile *			FS,
	const char *	Name, 
	unsigned int 	OpenModeFlags);

#if 0
// Experimenting:
GRAPI grVFile * GRCC grVFile_GetSystem(grVFile_TypeIdentifier SystemType);
GRAPI grVFile * GRCC grVFile_Create(
	grVFile *		FS,
	const char *	Name, 
	unsigned int 	OpenModeFlags);
GRAPI grVFile * GRCC grVFile_SetParameters(grVFile *File, void *Parameters, int ParameterSize);
GRAPI grVFile * GRCC grVFile_GetParameters(grVFile *File, void *Parameters, int ParameterSize);
#endif

GRAPI grBoolean GRCC grVFile_Destroy(grVFile **FS);

GRAPI void		 GRCC grVFile_CreateRef(grVFile *File);

/*
typedef	enum
{
	grVFile_AppendPath,
	grVFile_PrependPath,
}	grVFile_SearchOrder;
*/

GRAPI grBoolean GRCC grVFile_AddPath(grVFile *FS1, const grVFile *FS2, grBoolean Append);
	// Appends (or prepends) the path associated with FS2 into FS1.
	//    Append==GR_TRUE   causes the FS2 to be searched AFTER FS1
	//    Append==GR_FALSE  causes the FS2 to be searched BEFORE FS1

/*  perhaps grVFile_AppendPath and grVFile_PrependPath */


GRAPI grBoolean GRCC grVFile_DeleteFile(grVFile *FS, const char *FileName);
	// Deletes a file within a file system.  Returns GR_TRUE on success, GR_FALSE
	// on failure.

GRAPI grBoolean GRCC grVFile_RenameFile(grVFile *FS, const char *FileName, const char *NewName);
	// Renames a file within a file system.  Returns GR_TRUE on success, GR_FALSE
	// on failure.

GRAPI grBoolean GRCC grVFile_FileExists(grVFile *FS, const char *FileName);
	// Returns GR_TRUE if the file FileName exists in FS, GR_FALSE otherwise.
	// Does not do any searching (?)

//grVFile_VFileType grVFile_Register( all kinds of stuff );

GRAPI grBoolean GRCC grVFile_Disperse(grVFile *FS, const char *Directory);
// Always disperses recursively (falls out from implementation).

GRAPI grBoolean GRCC grVFile_Close (grVFile *File);
	// closes and destroys the File

//---------- File Specific Operations -----------

typedef	enum
{
	GR_VFILE_SEEKCUR	= 0,
	GR_VFILE_SEEKEND	= 1,
	GR_VFILE_SEEKSET	= 2
}	grVFile_Whence;

GRAPI grBoolean GRCC grVFile_GetS  		 (		grVFile *File, void *Buff, int MaxLen);
												// GetS eats \n\r\0 at the end of a string
GRAPI grBoolean GRCC grVFile_BytesAvailable(		grVFile *File, long *Count);
												// returns number of bytes which a Read can perform immediately
GRAPI grBoolean GRCC grVFile_Read  		 (		grVFile *File, void *Buff, uint32 Count); // cjp modified from int to uint32..
GRAPI grBoolean GRCC grVFile_Write 		 (		grVFile *File, const void *Buff, int Count);
GRAPI grBoolean GRCC grVFile_Seek  		 (		grVFile *File, int where, grVFile_Whence Whence);
												// do NOT seek forward on Internet files!
GRAPI grBoolean GRCC grVFile_Printf		 (		grVFile *File, const char *Format, ...);
GRAPI grBoolean GRCC grVFile_EOF   		 (const grVFile *File);
GRAPI grBoolean GRCC grVFile_Tell  		 (const grVFile *File, long *Position);
GRAPI grBoolean GRCC grVFile_GetProperties (const grVFile *File, grVFile_Properties *Properties);
GRAPI grBoolean GRCC grVFile_GetName		 (const grVFile *File, char *Buff, int MaxBuffLen);

GRAPI grBoolean GRCC grVFile_Size  		 (const grVFile *File, long *Size);
GRAPI grBoolean GRCC grVFile_SetSize		 (		grVFile *File, long Size);
GRAPI grBoolean GRCC grVFile_SetAttributes (		grVFile *File, grVFile_Attributes Attributes);
GRAPI grBoolean GRCC grVFile_SetTime		 (		grVFile *File, const grVFile_Time *Time);

GRAPI grBoolean GRCC grVFile_Rewind		 (		grVFile *File);

GRAPI grVFile * GRCC grVFile_GetHintsFile	 (		grVFile *File);
	// GetHintsFile is Deprecated; use CreateHF instead

GRAPI grVFile * GRCC grVFile_CreateHintsFile(		grVFile *File);
							// if there are no hints :
							//	on a file open for read : returns NULL
							//  on a file open for write: creates a hints subfile
							//		warning : as of 2-8-99 , if you GetHintsFile for write, and
							//		then write nothing, you will get a VFHH0000 at the head of your file!
							//	destroy it when you're done


//---------------------------------------

#ifdef _DEBUG
GRAPI grBoolean GRCC grVFile_IsValid(const grVFile * File);
#else
#define grVFile_IsValid(F) (GR_TRUE)
#endif

#ifdef _DEBUG
// to avoid including windows:
#ifdef WIN32
_declspec(dllimport) void __stdcall OutputDebugStringA(const char * lpOutputString);
#define OutputDebugString OutputDebugStringA
#endif

#ifdef BUILD_BE
#define OutputDebugString// DEBUGGER
#endif

#define DebugShowOpenCount() do { char str[1024]; sprintf(str,"vfiles : %d\n",grVFile_OpenCount()); OutputDebugString(str); } while(0)

GRAPI uint32 GRCC grVFile_OpenCount(void);

#else

#define grVFile_OpenCount() (0)
#define DebugShowOpenCount()

#endif

//---------------------------------------

#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grVFile_Properties grVFile_Properties;
typedef struct grVFile_Time grVFile_Time;
typedef grVFile_Attributes grVFile_Attributes;
typedef grVFile_TypeIdentifier grVFile_TypeIdentifier;
typedef grVFile_Whence grVFile_Whence;



#endif
