/****************************************************************************************/
/*  APROJECT.C                                                                          */
/*                                                                                      */
/*  Author:                                                                             */
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
/*
  AProject.cpp -- Actor Studio Project file API

  Copyright © 1998, Eclipse Entertainment
*/
#include "AProject.h"
#include "array.h"
#include "ram.h"
#include <assert.h>
#include "util.h"
#include "ErrorLog.h"
#include "FilePath.h"
#include <stdio.h>

#pragma warning(disable : 4201 4214 4115 4514)
#include <windows.h>
#pragma warning(default : 4201 4214 4115)

#define APJ_VERSION_MAJOR 0
#define APJ_VERSION_MINOR 90

// project file version string.
static const char AProject_VersionString[] = "APJ Version %d.%d";


typedef struct tag_ApjOutput
{
	char *Filename;
	ApjOutputFormat Fmt;
} ApjOutput;

typedef struct tag_ApjPaths
{
	grBoolean ForceRelative;
	char *Materials;
	char *TempFiles;
} ApjSearchPaths;

typedef struct tag_ApjBody
{
	char *Filename;
	ApjBodyFormat Fmt;
} ApjBody;

// Entry in Materials section
typedef struct
{
	char *Name;			// Material name
	ApjMaterialFormat Fmt;  // type
	char *Filename;		// texture filename (may be NULL)
	GR_RGBA Color;		//
} ApjMaterialEntry;

// materials section
typedef struct tag_ApjMaterials
{
	int Count;
	Array *Items;	// array of ApjMaterialEntry structures
} ApjMaterials;


// Entry in motions section
typedef struct
{
	char *Name;				// motion name
	ApjMotionFormat Fmt;	// motion file format
	char *Filename;			// file that contains the motion
	grBoolean OptFlag;		// optimization flag
	int OptLevel;			// motion optimization level
	char *Bone;				// name of root bone to grab
} ApjMotionEntry;

// motions section
typedef struct tag_ApjMotions
{
	int Count;
	Array *Items;	// Array of ApjMotionEntry structures
} ApjMotions;



struct tag_AProject
{
	ApjOutput		Output;
	ApjSearchPaths	Paths;
	ApjBody			Body;
	ApjMaterials	Materials;
	ApjMotions		Motions;
};


// extensions for body types.
// these must match the ApjBodyFormat enumeration
static const char *BodyExtensions[] = {".max", ".nfo", ".bdy", ".act"};



// Determine body format from file extension.
// Returns ApjBody_Invalid if unknown extension
ApjBodyFormat AProject_GetBodyFormatFromFilename (const char *Name)
{
	char Ext[MAX_PATH];
	int x;

	if (FilePath_GetExt (Name, Ext) != GR_FALSE)
	{
		for (x = 0; x <= ApjBody_Act; ++x)
		{
			if (stricmp (BodyExtensions[x], Ext) == 0)
			{
				return x+1;
			}
		}
	}
	return ApjBody_Invalid;
}

static const char *MotionExtensions[] = {".max", ".key", ".mot", ".act"};

ApjMotionFormat AProject_GetMotionFormatFromFilename (const char *Filename)
{
	char Ext[MAX_PATH];
	int x;

	if (FilePath_GetExt (Filename, Ext) != GR_FALSE)
	{
		for (x = 0; x < ApjMotion_TypeCount; ++x)
		{
			if (stricmp (MotionExtensions[x], Ext) == 0)
			{
				return x+1;
			}
		}
	}
	return ApjMotion_Invalid;
}


// Create empty project
AProject *AProject_Create (const char *OutputName)
{
	AProject *pProject;
	grBoolean NoErrors;
	char OutputNameAndExt[MAX_PATH];

	assert (OutputName != NULL);

	pProject = GR_RAM_ALLOCATE_STRUCT (AProject);
	if (pProject == NULL)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Allocating project structure",NULL);
		return NULL;
	}

	// Initialize defaults
	// Output
	pProject->Output.Filename = NULL;
	pProject->Output.Fmt = ApjOutput_Binary;

	// Paths
	pProject->Paths.ForceRelative = GR_TRUE;
	pProject->Paths.Materials = NULL;

	// Body
	pProject->Body.Filename = NULL;
	pProject->Body.Fmt = ApjBody_Max;

	// Materials
	pProject->Materials.Count = 0;
	pProject->Materials.Items = NULL;

	// Motions
	pProject->Motions.Count = 0;
	pProject->Motions.Items = NULL;

	FilePath_SetExt (OutputName, ".act", OutputNameAndExt);

	// Allocate required memory
	NoErrors = 
		((pProject->Output.Filename = Util_Strdup (OutputNameAndExt)) != NULL) &&
		((pProject->Paths.Materials = Util_Strdup ("")) != NULL) &&
		((pProject->Paths.TempFiles = Util_Strdup (".\\BldTemp")) != NULL) &&
		((pProject->Body.Filename	= Util_Strdup ("")) != NULL);
		
	// Build motion and material arrays.  Initially empty.
	NoErrors = NoErrors &&
		((pProject->Materials.Items = Array_Create (1, sizeof (ApjMaterialEntry))) != NULL);

	NoErrors = NoErrors &&
		((pProject->Motions.Items = Array_Create (1, sizeof (ApjMotionEntry))) != NULL);

	// if unsuccessful, destroy any allocated data
	if (!NoErrors)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Initializing project structure",NULL);
		if (pProject != NULL)
		{
			AProject_Destroy (&pProject);
		}
	}

	return pProject;
}

// Free all memory allcated by project structure.
void AProject_Destroy (AProject **ppProject)
{
	AProject *pProject;

	assert (ppProject != NULL);
	pProject = *ppProject;
	assert (pProject != NULL);

	while (pProject->Materials.Count > 0)
	{
		AProject_RemoveMaterial (pProject, pProject->Materials.Count-1);
	}

	while (pProject->Motions.Count > 0)
	{
		AProject_RemoveMotion (pProject, pProject->Motions.Count-1);
	}

	if (pProject->Output.Filename != NULL)	grRam_Free (pProject->Output.Filename);
	if (pProject->Paths.Materials != NULL)	grRam_Free (pProject->Paths.Materials);
	if (pProject->Paths.TempFiles != NULL)	grRam_Free (pProject->Paths.TempFiles);
	if (pProject->Body.Filename != NULL)	grRam_Free (pProject->Body.Filename);

	if (pProject->Materials.Items != NULL)	Array_Destroy (&pProject->Materials.Items);
	if (pProject->Motions.Items != NULL)	Array_Destroy (&pProject->Motions.Items);

	grRam_Free (*ppProject);
}

typedef enum
{
	READ_SUCCESS,
	READ_ERROR,
	READ_EOF
} ApjReadResult;

static ApjReadResult AProject_GetNonBlankLine (grVFile *FS, char *Buffer, int BufferSize)
{
	while (!grVFile_EOF (FS))
	{
		if (grVFile_GetS (FS, Buffer, BufferSize) == GR_FALSE)
		{
			// some kind of error...
			return READ_ERROR;
		}

		// search for and remove any newlines
		{
			char *c = strchr (Buffer, '\n');
			if (c != NULL)
			{
				*c = '\0';
			}
		}
					
		// if the line is not blank, then return it...
		{
			char *c = Buffer;

			while ((*c != '\0') && (c < (Buffer + BufferSize)))
			{
				if (!isspace (*c))
				{
					return READ_SUCCESS;
				}
				++c;
			}
		}
		// line's blank, go get the next one...
	}
	// end of file
	return READ_EOF;
}

static grBoolean AProject_CheckFileVersion (grVFile *FS)
{
	char VersionString[1024];
	int VersionMajor, VersionMinor;
	int rslt;

	// read string
	if (AProject_GetNonBlankLine (FS, VersionString, sizeof (VersionString)) != READ_SUCCESS)
	{
		// error...
		grErrorLog_AddString (GR_ERR_FILEIO_READ, "Reading project version string",NULL);
		return GR_FALSE;
	}

	// format must match project version string
	rslt = sscanf (VersionString, AProject_VersionString, &VersionMajor, &VersionMinor);
	if (rslt != 2)
	{
		grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Incompatible file type",NULL);
		return GR_FALSE;
	}

	// make sure we know how to read this version
	if ((VersionMajor < APJ_VERSION_MAJOR) ||
		((VersionMajor == APJ_VERSION_MAJOR) && (VersionMinor <= APJ_VERSION_MINOR)))
	{
		return GR_TRUE;
	}

	grErrorLog_AddString (GR_ERR_FILEIO_VERSION, "Incompatible project file version",NULL);
	return GR_FALSE;
}

// Section keys
static const char Paths_Key[]			= "[Paths]";
static const char ForceRelative_Key[]	= "ForceRelative";
static const char MaterialsPath_Key[]	= "MaterialsPath";
static const char TempFilesPath_Key[]	= "TempFiles";
static const char EndPaths_Key[]		= "[EndPaths]";

static const char Output_Key[]			= "[Output]";
static const char OutputFilename_Key[]	= "Filename";
static const char OutputFormat_Key[]	= "Format";
static const char EndOutput_Key[]		= "[EndOutput]";

static const char Body_Key[]			= "[Body]";
static const char BodyFilename_Key[]	= "Filename";
static const char BodyFormat_Key[]		= "Format";
static const char EndBody_Key[]			= "[EndBody]";

static const char Materials_Key[]		= "[Materials]";
static const char MaterialsCount_Key[]	= "Count";
static const char EndMaterials_Key[]	= "[EndMaterials]";

static const char Motions_Key[]			= "[Motions]";
static const char MotionsCount_Key[]	= "Count";
static const char EndMotions_Key[]		= "[EndMotions]";

// strip leading spaces from string before copying it
static grBoolean AProject_SetString (char **pString, const char *NewValue)
{
	const char *c = NewValue;

	while ((c != '\0') && isspace (*c))
	{
		++c;
	}

	return Util_SetString (pString, c);
}

// Load [Paths] section
static grBoolean AProject_LoadPathsInfo (AProject *pProject, grVFile *FS)
{

	for (;;)	// infinite loop
	{
		char Buffer[1024];
		char *c;

		if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
		{
			grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading paths info",NULL);
			return GR_FALSE;
		}

		if (_strnicmp (Buffer, ForceRelative_Key, strlen (ForceRelative_Key)) == 0)
		{
			c = &Buffer[strlen (ForceRelative_Key)];
			// Set force relative flag if not explicitly turned off
			pProject->Paths.ForceRelative = ((*c == '\0') || (*(c+1) != '0')) ? GR_TRUE : GR_FALSE;
		}
		else if (_strnicmp (Buffer, MaterialsPath_Key, strlen (MaterialsPath_Key)) == 0)
		{
			c = &Buffer[strlen (MaterialsPath_Key)];
			AProject_SetString (&pProject->Paths.Materials, c);
		}
		else if (_strnicmp (Buffer, TempFilesPath_Key, strlen (TempFilesPath_Key)) == 0)
		{
			c = &Buffer[strlen (TempFilesPath_Key)];
			AProject_SetString (&pProject->Paths.TempFiles, c);
		}
		else if (_strnicmp (Buffer, EndPaths_Key, strlen (EndPaths_Key)) == 0)
		{
			return GR_TRUE;
		}
		else
		{
			// bad entry...
			grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad Paths section entry",NULL);
			return GR_FALSE;
		}
	}
}

static grBoolean AProject_WritePathsInfo (const AProject *pProject, grVFile *FS)
{
	if ((grVFile_Printf (FS, "%s\r\n", Paths_Key) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %c\r\n", ForceRelative_Key, (pProject->Paths.ForceRelative == GR_TRUE) ? '1' : '0') == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %s\r\n", MaterialsPath_Key, pProject->Paths.Materials) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %s\r\n", TempFilesPath_Key, pProject->Paths.TempFiles) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s\r\n", EndPaths_Key) == GR_FALSE))
	{
		grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing Paths section",NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

static grBoolean AProject_LoadOutputInfo (AProject *pProject, grVFile *FS)
{
	for (;;)
	{
		char Buffer[1024];
		char *c;

		if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
		{
			grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading output file info",NULL);
			return GR_FALSE;
		}

		if (_strnicmp (Buffer, OutputFilename_Key, strlen (OutputFilename_Key)) == 0)
		{
			c = &Buffer[strlen (OutputFilename_Key)];
			AProject_SetString (&pProject->Output.Filename, c);
		}
		else if (_strnicmp (Buffer, OutputFormat_Key, strlen (OutputFormat_Key)) == 0)
		{
			c = &Buffer[strlen (OutputFormat_Key)];
			// format assumed binary unless text specified
			pProject->Output.Fmt = ((*c == '\0') || (*(c+1) != '0')) ? ApjOutput_Binary : ApjOutput_Text;
		}
		else if (_strnicmp (Buffer, EndOutput_Key, strlen (EndOutput_Key)) == 0)
		{
			return GR_TRUE;
		}
		else
		{
			// bad entry
			grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad Output section entry",NULL);
			return GR_FALSE;
		}
	}
}

static grBoolean AProject_WriteOutputInfo (const AProject *pProject, grVFile *FS)
{
	if ((grVFile_Printf (FS, "%s\r\n", Output_Key) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %s\r\n", OutputFilename_Key, pProject->Output.Filename) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %c\r\n", OutputFormat_Key, (pProject->Output.Fmt == ApjOutput_Binary) ? '1' : '0') == GR_FALSE) ||
		(grVFile_Printf (FS, "%s\r\n", EndOutput_Key) == GR_FALSE))
	{
		grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing Output section",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}


static grBoolean AProject_LoadBodyInfo (AProject *pProject, grVFile *FS)
{
	for (;;)
	{
		char Buffer[1024];
		char *c;

		if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
		{
			grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading Body info",NULL);
			return GR_FALSE;
		}

		if (_strnicmp (Buffer, BodyFilename_Key, strlen (BodyFilename_Key)) == 0)
		{
			c = &Buffer[strlen (BodyFilename_Key)];
			AProject_SetString (&pProject->Body.Filename, c);
			// if we haven't loaded a format yet, try to get it from the filename
			// this will be overridden by a format if it's there...
			if (pProject->Body.Fmt == ApjBody_Invalid)
			{
				pProject->Body.Fmt = AProject_GetBodyFormatFromFilename (pProject->Body.Filename);
			}
		}
		else if (_strnicmp (Buffer, BodyFormat_Key, strlen (BodyFormat_Key)) == 0)
		{
			c = &Buffer[strlen (BodyFormat_Key)];
			// Determine body file format
			if (*c != '\0')
			{
				switch (*(c+1))
				{
					case '1' : pProject->Body.Fmt = ApjBody_Max; break;
					case '2' : pProject->Body.Fmt = ApjBody_Nfo; break;
					case '3' : pProject->Body.Fmt = ApjBody_Bdy; break;
					case '4' : pProject->Body.Fmt = ApjBody_Act; break;
					default  : pProject->Body.Fmt = ApjBody_Invalid; break;
				}
			}
			else
			{
				pProject->Body.Fmt = ApjBody_Invalid;
			}

			if (pProject->Body.Fmt == ApjBody_Invalid)
			{
				grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Unknown body file format",NULL);
				return GR_FALSE;
			}
		}
		else if (_strnicmp (Buffer, EndBody_Key, strlen (EndBody_Key)) == 0)
		{
			if (pProject->Body.Fmt == ApjBody_Invalid)
			{
				grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Unknown body file format",NULL);
				return GR_FALSE;
			}
			return GR_TRUE;
		}
		else
		{
			// bad entry
			grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad Body section entry",NULL);
			return GR_FALSE;
		}
	}
}

// ugly, but it works...
static char AProject_BodyFormatToChar (ApjBodyFormat Fmt)
{
	return (char)(((int)Fmt) + '0');
}

static grBoolean AProject_WriteBodyInfo (const AProject *pProject, grVFile *FS)
{
	if ((grVFile_Printf (FS, "%s\r\n", Body_Key) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %s\r\n", BodyFilename_Key, pProject->Body.Filename) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %c\r\n", BodyFormat_Key, AProject_BodyFormatToChar (pProject->Body.Fmt)) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s\r\n", EndBody_Key) == GR_FALSE))
	{
		grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing Body section",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}

static grBoolean AProject_UnquoteString (char *TheString)
{
	char *c = &TheString[strlen (TheString)-1];
	if (*c != '"')
	{
		return GR_FALSE;	// no ending quote
	}
	*c = '\0';	// rip quote from the end

	if (*TheString != '"')
	{
		return GR_FALSE;	// no beginning quote
	}
	strcpy (TheString, (TheString+1));
	return GR_TRUE;
}

static grBoolean AProject_ParseMaterial 
	(
	  char *Buffer,
	  char *Name,
	  ApjMaterialFormat *Fmt,
	  char *Filename,
	  GR_RGBA *Color
	)
{
	char *NameStr, *FmtStr, *FilenameStr;
	char *rStr, *gStr, *bStr, *aStr;

	// parse the items from the line
	if ((NameStr	= strtok (Buffer, ",")) == NULL)return GR_FALSE;
	if ((FmtStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((FilenameStr= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((rStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((gStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((bStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((aStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;

	// set the items
	*Fmt = (*FmtStr == '1') ? ApjMaterial_Texture : ApjMaterial_Color;

	if (AProject_UnquoteString (NameStr)	 == GR_FALSE) return GR_FALSE;
	if (AProject_UnquoteString (FilenameStr) == GR_FALSE) return GR_FALSE;

	strcpy (Name, NameStr);
	strcpy (Filename, FilenameStr);

	Color->r = (float)atof (rStr);
	Color->g = (float)atof (gStr);
	Color->b = (float)atof (bStr);
	Color->a = (float)atof (aStr);

	return GR_TRUE;
}

static grBoolean AProject_LoadMaterialsInfo (AProject *pProject, grVFile *FS)
{
	for (;;)
	{
		char Buffer[1024];
		char *c;

		if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
		{
			grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading Materials info",NULL);
			return GR_FALSE;
		}

		if (_strnicmp (Buffer, MaterialsCount_Key, strlen (MaterialsCount_Key)) == 0)
		{
			int Count = 0;

			c = &Buffer[strlen (MaterialsCount_Key)];
			if (*c != '\0')
			{
				Count = atoi (c+1);
				if (Count < 0)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Negative materials count",NULL);
					return GR_FALSE;
				}
			}
			// load and add each material
			for (; Count > 0; --Count)
			{
				char Name[MAX_PATH];
				ApjMaterialFormat Fmt;
				char Filename[MAX_PATH];
				GR_RGBA Color;
				int Index;

				if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading Materials info",NULL);
					return GR_FALSE;
				}

				// parse the material's parts
				if (AProject_ParseMaterial (Buffer, Name, &Fmt, Filename, &Color) == GR_FALSE)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad material",NULL);
					return GR_FALSE;
				}
				// and then add the material.
				if (AProject_AddMaterial (pProject, Name, Fmt, Filename, Color.r, Color.g, Color.b, Color.a, &Index) == GR_FALSE)
				{
					return GR_FALSE;
				}
			}
		}
		else if (_strnicmp (Buffer, EndMaterials_Key, strlen (EndMaterials_Key)) == 0)
		{
			return GR_TRUE;
		}
		else
		{
			// bad entry
			grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad Materials section entry",NULL);
			return GR_FALSE;
		}
	}
}

static grBoolean AProject_WriteMaterialsInfo (const AProject *pProject, grVFile *FS)
{
	int i;

	if ((grVFile_Printf (FS, "%s\r\n", Materials_Key) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %d\r\n", MaterialsCount_Key, pProject->Materials.Count) == GR_FALSE))
	{
		goto Error;
	}

	for (i = 0; i < pProject->Materials.Count; ++i)
	{
		// format each material's information and write it
		ApjMaterialEntry *pEntry;
		char Buffer[1024];

		pEntry = Array_ItemPtr (pProject->Materials.Items, i);
		assert (pEntry->Filename != NULL);

		sprintf (Buffer, "\"%s\",%d,\"%s\",%f,%f,%f,%f", 
			pEntry->Name, pEntry->Fmt, pEntry->Filename,
			pEntry->Color.r, pEntry->Color.g, pEntry->Color.b, pEntry->Color.a);
		if (grVFile_Printf (FS, "%s\r\n", Buffer) == GR_FALSE)
		{
			goto Error;

		}
	}

	if (grVFile_Printf (FS, "%s\r\n", EndMaterials_Key) == GR_FALSE)
	{
		goto Error;
	}

	return GR_TRUE;
Error:
	grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing Materials section",NULL);
	return GR_FALSE;
}

static grBoolean AProject_ParseMotion 
	(
	  char *Buffer,
	  char *Name,
	  ApjMotionFormat *Fmt,
	  char *Filename,
	  grBoolean *OptFlag,
	  int *OptLevel,
	  char *BoneName
	)
{
	char *NameStr, *FilenameStr, *FmtStr, *OptFlagStr, *OptLevelStr, *BoneNameStr;

	// parse the items from the line
	if ((NameStr		= strtok (Buffer, ",")) == NULL)return GR_FALSE;
	if ((FmtStr			= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((FilenameStr	= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((OptFlagStr		= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((OptLevelStr	= strtok (NULL, ",")) == NULL)	return GR_FALSE;
	if ((BoneNameStr	= strtok (NULL, ",")) == NULL)	return GR_FALSE;

	// set the items
	strcpy (Name, NameStr);
	if ((*FmtStr < '1') || (*FmtStr > '3'))
	{
		return GR_FALSE;
	}

	*Fmt = (ApjMotionFormat)(*FmtStr - '0');

	if (AProject_UnquoteString (NameStr)	== GR_FALSE) return GR_FALSE;
	if (AProject_UnquoteString (FilenameStr)== GR_FALSE) return GR_FALSE;
	if (AProject_UnquoteString (BoneNameStr)== GR_FALSE) return GR_FALSE;

	*OptFlag = (*OptFlagStr == '0') ? GR_FALSE : GR_TRUE;

	if (isdigit (*OptLevelStr))
	{
		*OptLevel = *OptLevelStr - '0';
	}
	else
	{
		*OptLevel = 0;
	}
	strcpy (Name, NameStr);
	strcpy (Filename, FilenameStr);
	strcpy (BoneName, BoneNameStr);

	return GR_TRUE;
}

static grBoolean AProject_LoadMotionsInfo (AProject *pProject, grVFile *FS)
{
	for (;;)
	{
		char Buffer[1024];
		char *c;

		if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
		{
			grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading Motions info",NULL);
			return GR_FALSE;
		}

		if (_strnicmp (Buffer, MotionsCount_Key, strlen (MotionsCount_Key)) == 0)
		{
			int Count = 0;

			c = &Buffer[strlen (MotionsCount_Key)];
			if (*c != '\0')
			{
				Count = atoi (c+1);
				if (Count < 0)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Negative Motions count",NULL);
					return GR_FALSE;
				}
			}
			// load and add each motion
			for (; Count > 0; --Count)
			{
				char Name[MAX_PATH];
				char Filename[MAX_PATH];
				char BoneName[MAX_PATH];
				int OptLevel;
				grBoolean OptFlag;
				int Index;
				ApjMotionFormat Fmt;

				if (AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer)) != READ_SUCCESS)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading Motions info",NULL);
					return GR_FALSE;
				}

				// parse the motion's parts
				if (AProject_ParseMotion (Buffer, Name, &Fmt, Filename, &OptFlag, &OptLevel, BoneName) == GR_FALSE)
				{
					grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad motion",NULL);
					return GR_FALSE;
				}
				// and then add the motion
				if (AProject_AddMotion (pProject, Name, Filename, Fmt, OptFlag, OptLevel, BoneName, &Index) == GR_FALSE)
				{
					return GR_FALSE;
				}
			}
		}
		else if (_strnicmp (Buffer, EndMotions_Key, strlen (EndMotions_Key)) == 0)
		{
			return GR_TRUE;
		}
		else
		{
			// bad entry
			grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Bad Motions section entry",NULL);
			return GR_FALSE;
		}
	}
}

// ugly, but it works...
static char AProject_MotionFormatToChar (ApjMotionFormat Fmt)
{
	return (char)(((int)Fmt) + '0');
}

static grBoolean AProject_WriteMotionsInfo (const AProject *pProject, grVFile *FS)
{
	int i;

	if ((grVFile_Printf (FS, "%s\r\n", Motions_Key) == GR_FALSE) ||
		(grVFile_Printf (FS, "%s %d\r\n", MotionsCount_Key, pProject->Motions.Count) == GR_FALSE))
	{
		goto Error;
	}

	for (i = 0; i < pProject->Motions.Count; ++i)
	{
		// format each motion's information and write it
		ApjMotionEntry *pEntry;
		char Buffer[1024];

		pEntry = Array_ItemPtr (pProject->Motions.Items, i);
		sprintf (Buffer, "\"%s\",%c,\"%s\",%c,%d,\"%s\"", 
			pEntry->Name, AProject_MotionFormatToChar (pEntry->Fmt), 
			pEntry->Filename, (pEntry->OptFlag ? '1' : '0'), pEntry->OptLevel, pEntry->Bone);
		if (grVFile_Printf (FS, "%s\r\n", Buffer) == GR_FALSE)
		{
			goto Error;

		}
	}

	if (grVFile_Printf (FS, "%s\r\n", EndMotions_Key) == GR_FALSE)
	{
		goto Error;
	}

	return GR_TRUE;
Error:
	grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing Motions section",NULL);
	return GR_FALSE;
}


// Project file section loader function type
typedef grBoolean (* ApjSectionLoader) (AProject *pProject, grVFile *FS);

typedef struct
{
	const char *SectionName;	// section name to find
	ApjSectionLoader Load;		// function that loads this section
} ApjSectionDispatchEntry;


// Table of section names and loader functions.
// Used to scan for and load project file sections.
static const ApjSectionDispatchEntry ApjSectionDispatchTable[] =
{
	{Paths_Key,		AProject_LoadPathsInfo},
	{Output_Key,	AProject_LoadOutputInfo},
	{Body_Key,		AProject_LoadBodyInfo},
	{Materials_Key,	AProject_LoadMaterialsInfo},
	{Motions_Key,	AProject_LoadMotionsInfo}
};

static int ApjNumSections = sizeof (ApjSectionDispatchTable)/sizeof (ApjSectionDispatchEntry);

// Read a project from a file.
AProject *AProject_CreateFromFile (grVFile *FS)
{
	AProject *pProject = NULL;
	char Buffer[1024];		// any line longer than this is an error
	grBoolean NoErrors;

	assert (FS != NULL);

	// create empty project
	NoErrors = ((pProject = AProject_Create ("")) != NULL);

	// check file version information
	NoErrors = NoErrors && (AProject_CheckFileVersion (FS) != GR_FALSE);

	// Sections can be in any order
	while (NoErrors && (grVFile_EOF (FS) == GR_FALSE))
	{
		int Section;

		// read a line
		ApjReadResult rslt = AProject_GetNonBlankLine (FS, Buffer, sizeof (Buffer));
		switch (rslt)
		{
			case READ_ERROR :
				grErrorLog_AddString (GR_ERR_FILEIO_READ, "Loading project",NULL);
				NoErrors = GR_FALSE;
				break;

			case READ_EOF :
				break;

			case READ_SUCCESS :
			{
				// get the section name and process that section
				grBoolean FoundIt;
				const ApjSectionDispatchEntry *pEntry = NULL;

				// determine which section, and go read that.
				for (FoundIt = GR_FALSE, Section = 0; (FoundIt == GR_FALSE) && (Section < ApjNumSections); ++Section)
				{
					pEntry = &ApjSectionDispatchTable[Section];

					FoundIt = (_strnicmp (Buffer, pEntry->SectionName, strlen (pEntry->SectionName)) == 0);
				}

				if (FoundIt)
				{
					NoErrors = pEntry->Load (pProject, FS);
				}
				else
				{
					// didn't find a good section name
					NoErrors = GR_FALSE;
					grErrorLog_AddString (GR_ERR_FILEIO_FORMAT, "Expected section name",NULL);
				}
				break;
			}
		}
	}

	if (!NoErrors)
	{
		// some kind of error occurred.
		// Clean up and exit
		if (pProject != NULL)
		{
			AProject_Destroy (&pProject);
		}
	}

	return pProject;
}

AProject *AProject_CreateFromFilename (const char *Filename)
{
	grVFile *FS;
	AProject *Project;

	FS = grVFile_OpenNewSystem (NULL, GR_VFILE_TYPE_DOS, Filename, NULL, GR_VFILE_OPEN_READONLY);
	if (FS == NULL)
	{
		// unable to open file for reading
		return NULL;
	}

	Project = AProject_CreateFromFile (FS);
	grVFile_Close (FS);

	return Project;
}


grBoolean AProject_WriteToFile (const AProject *pProject, grVFile *FS)
{
	if ((grVFile_Printf (FS, AProject_VersionString, APJ_VERSION_MAJOR, APJ_VERSION_MINOR) == GR_FALSE) ||
		(grVFile_Printf (FS, "\r\n\r\n") == GR_FALSE))
	{
		grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Writing version string",NULL);
		return GR_FALSE;
	}

	if ((AProject_WritePathsInfo (pProject, FS) != GR_FALSE) &&
		(grVFile_Printf (FS, "\r\n") != GR_FALSE) &&
		(AProject_WriteOutputInfo (pProject, FS) != GR_FALSE) &&
		(grVFile_Printf (FS, "\r\n") != GR_FALSE) &&
	    (AProject_WriteBodyInfo (pProject, FS) != GR_FALSE) &&
		(grVFile_Printf (FS, "\r\n") != GR_FALSE) &&
		(AProject_WriteMaterialsInfo (pProject, FS) != GR_FALSE) &&
		(grVFile_Printf (FS, "\r\n") != GR_FALSE) &&
		(AProject_WriteMotionsInfo (pProject, FS) != GR_FALSE))
	{
		return GR_TRUE;
	}
	return GR_FALSE;
}

grBoolean AProject_WriteToFilename (const AProject *pProject, const char *Filename)
{
	grVFile *FS;
	grBoolean rslt;

	FS = grVFile_OpenNewSystem (NULL, GR_VFILE_TYPE_DOS, Filename, NULL, GR_VFILE_OPEN_CREATE);
	if (FS == NULL)
	{
		// unable to open file for writing
		grErrorLog_AddString (GR_ERR_FILEIO_WRITE, "Opening file",NULL);
		return GR_FALSE;
	}

	rslt = AProject_WriteToFile (pProject, FS);

	grVFile_Close (FS);

	return rslt;
}

// Paths section
grBoolean AProject_GetForceRelativePaths (const AProject *pProject)
{
	return pProject->Paths.ForceRelative;
}

grBoolean AProject_SetForceRelativePaths (AProject *pProject, const grBoolean Flag)
{
	pProject->Paths.ForceRelative = Flag;
	return GR_TRUE;
}


const char *AProject_GetMaterialsPath (const AProject *pProject)
{
	return pProject->Paths.Materials;
}

grBoolean AProject_SetMaterialsPath (AProject *pProject, const char *Path)
{
	if (AProject_SetString (&pProject->Paths.Materials, Path) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting materials path",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}

const char *AProject_GetObjPath (const AProject *pProject)
{
	return pProject->Paths.TempFiles;
}

grBoolean AProject_SetObjPath (AProject *pProject, const char *Path)
{
	if (AProject_SetString (&pProject->Paths.TempFiles, Path) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting temp files path",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}


const char *AProject_GetOutputFilename (const AProject *pProject)
{
	return pProject->Output.Filename;
}

grBoolean AProject_SetOutputFilename (AProject *pProject, const char *Filename)
{
	if (AProject_SetString (&pProject->Output.Filename, Filename) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting output filename",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}

ApjOutputFormat AProject_GetOutputFormat (const AProject *pProject)
{
	return pProject->Output.Fmt;
}

grBoolean AProject_SetOutputFormat (AProject *pProject, const ApjOutputFormat Fmt)
{
	assert ((Fmt == ApjOutput_Text) || (Fmt == ApjOutput_Binary));

	pProject->Output.Fmt = Fmt;
	return GR_TRUE;
}

const char *AProject_GetBodyFilename (const AProject *pProject)
{
	return pProject->Body.Filename;
}

grBoolean AProject_SetBodyFilename (AProject *pProject, const char *Filename)
{
	if (AProject_SetString (&pProject->Body.Filename, Filename) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting body filename",NULL);
		return GR_FALSE;
	}
	return GR_TRUE;
}


ApjBodyFormat AProject_GetBodyFormat (const AProject *pProject)
{
	return pProject->Body.Fmt;
}

grBoolean AProject_SetBodyFormat (AProject *pProject, ApjBodyFormat Fmt)
{
	assert ((Fmt >= ApjBody_Invalid) && (Fmt <= ApjBody_Act));

	pProject->Body.Fmt = Fmt;
	return GR_TRUE;
}

int AProject_GetMaterialsCount (const AProject *pProject)
{
	return pProject->Materials.Count;
}

static void AProject_FreeMaterialInfo (ApjMaterialEntry *pEntry)
{
	if (pEntry->Name != NULL) grRam_Free (pEntry->Name);
	if (pEntry->Filename != NULL) grRam_Free (pEntry->Filename);
}

grBoolean AProject_AddMaterial
	(
	  AProject *pProject,
	  const char *MaterialName,
	  const ApjMaterialFormat Fmt,
	  const char *TextureFilename,
	  const float Red, const float Green, const float Blue, const float Alpha,
	  int *pIndex		// returned index
	)
{
	ApjMaterialEntry *pEntry;
	int ArraySize;

	assert ((Fmt == ApjMaterial_Color) || (Fmt == ApjMaterial_Texture));

	ArraySize = Array_GetSize (pProject->Materials.Items);
	if (pProject->Materials.Count == ArraySize)
	{
		// array is full, have to extend it
		int NewSize;
		
		NewSize = Array_Resize (pProject->Materials.Items, 2*ArraySize);
		if (NewSize <= ArraySize)
		{
			// couldn't resize
			grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Adding material",NULL);
			return GR_FALSE;
		}
	}
	pEntry = Array_ItemPtr (pProject->Materials.Items, pProject->Materials.Count);
	pEntry->Name = NULL;
	pEntry->Filename = NULL;

	if (((pEntry->Name = Util_Strdup (MaterialName)) == NULL) ||
		((pEntry->Filename = Util_Strdup (TextureFilename)) == NULL))
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Adding material",NULL);
		AProject_FreeMaterialInfo (pEntry);
		return GR_FALSE;
	}
	pEntry->Fmt = Fmt;
	pEntry->Color.r = Red;
	pEntry->Color.g = Green;
	pEntry->Color.b = Blue;
	pEntry->Color.a = Alpha;

	*pIndex = (pProject->Materials.Count)++;
	return GR_TRUE;
}

grBoolean AProject_RemoveMaterial (AProject *pProject, const int Index)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	AProject_FreeMaterialInfo (pEntry);

	Array_DeleteAt (pProject->Materials.Items, Index);
	--(pProject->Materials.Count);

	return GR_TRUE;
}

// returns -1 if not found
int AProject_GetMaterialIndex (const AProject *pProject, const char *MaterialName)
{
	int Item;

	for (Item = 0; Item < pProject->Materials.Count; ++Item)
	{
		ApjMaterialEntry *pEntry = Array_ItemPtr (pProject->Materials.Items, Item);
		if (stricmp (pEntry->Name, MaterialName) == 0)
		{
			return Item;
		}
	}

	return -1;
}


ApjMaterialFormat AProject_GetMaterialFormat (const AProject *pProject, const int Index)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	return pEntry->Fmt;
}

grBoolean AProject_SetMaterialFormat (AProject *pProject, const int Index, const ApjMaterialFormat Fmt)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);
	assert ((Fmt == ApjMaterial_Color) || (Fmt == ApjMaterial_Texture));

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	pEntry->Fmt = Fmt;

	return GR_TRUE;
}

const char *AProject_GetMaterialName (const AProject *pProject, const int Index)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	return pEntry->Name;
}

grBoolean AProject_SetMaterialName (AProject *pProject, const int Index, const char *MaterialName)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	if (AProject_SetString (&pEntry->Name, MaterialName) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting material name",NULL);
		return GR_FALSE;
	}
	assert (pEntry->Name != NULL);
	return GR_TRUE;
}

const char *AProject_GetMaterialTextureFilename (const AProject *pProject, const int Index)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	return pEntry->Filename;
}

grBoolean AProject_SetMaterialTextureFilename (AProject *pProject, const int Index, const char *TextureFilename)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);
	assert (TextureFilename != NULL);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	if (AProject_SetString (&pEntry->Filename, TextureFilename) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting material filename",NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}


GR_RGBA AProject_GetMaterialTextureColor (const AProject *pProject, const int Index)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	return pEntry->Color;
}

grBoolean AProject_SetMaterialTextureColor (AProject *pProject, const int Index, 
	const float Red, const float Green, const float Blue, const float Alpha)
{
	ApjMaterialEntry *pEntry;

	assert (Index < pProject->Materials.Count);

	pEntry = Array_ItemPtr (pProject->Materials.Items, Index);
	pEntry->Color.r = Red;
	pEntry->Color.g = Green;
	pEntry->Color.b = Blue;
	pEntry->Color.a = Alpha;

	return GR_TRUE;
}


// Motions section
int AProject_GetMotionsCount (const AProject *pProject)
{
	return pProject->Motions.Count;
}

static void AProject_FreeMotionInfo (ApjMotionEntry *pEntry)
{
	if (pEntry->Name != NULL) grRam_Free (pEntry->Name);
	if (pEntry->Filename != NULL) grRam_Free (pEntry->Filename);
	if (pEntry->Bone != NULL) grRam_Free (pEntry->Bone);
}

grBoolean AProject_AddMotion
	(
	  AProject *pProject,
	  const char *MotionName,
	  const char *Filename,
	  const ApjMotionFormat Fmt,
	  const grBoolean OptFlag,
	  const int OptLevel,
	  const char *BoneName,
	  int *pIndex	// returned index
	)
{
	ApjMotionEntry *pEntry;
	int ArraySize;

	assert ((OptLevel >= 0) && (OptLevel <= 9));
	assert ((Fmt > ApjMotion_Invalid) && (Fmt < ApjMotion_TypeCount));

	ArraySize = Array_GetSize (pProject->Motions.Items);
	if (pProject->Motions.Count == ArraySize)
	{
		// array is full, have to extend it
		int NewSize;
		
		NewSize = Array_Resize (pProject->Motions.Items, 2*ArraySize);
		if (NewSize <= ArraySize)
		{
			// couldn't resize
			grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Adding Motion",NULL);
			return GR_FALSE;
		}
	}
	pEntry = Array_ItemPtr (pProject->Motions.Items, pProject->Motions.Count);
	pEntry->Name = NULL;
	pEntry->Filename = NULL;
	pEntry->Bone = NULL;
	pEntry->Fmt = Fmt;

	if (((pEntry->Name = Util_Strdup (MotionName)) == NULL) ||
		((pEntry->Filename = Util_Strdup (Filename)) == NULL) ||
		((pEntry->Bone = Util_Strdup (BoneName)) == NULL))
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Adding Motion",NULL);
		AProject_FreeMotionInfo (pEntry);
		return GR_FALSE;
	}
	pEntry->OptFlag = OptFlag;
	pEntry->OptLevel = OptLevel;

	*pIndex = (pProject->Motions.Count)++;
	return GR_TRUE;
}

grBoolean AProject_RemoveMotion (AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	AProject_FreeMotionInfo (pEntry);

	Array_DeleteAt (pProject->Motions.Items, Index);
	--(pProject->Motions.Count);

	return GR_TRUE;
}

int AProject_GetMotionIndex (const AProject *pProject, const char *MotionName)
{
	int Item;

	for (Item = 0; Item < pProject->Motions.Count; ++Item)
	{
		ApjMotionEntry *pEntry = Array_ItemPtr (pProject->Motions.Items, Item);
		if (strcmp (pEntry->Name, MotionName) == 0)
		{
			return Item;
		}
	}

	return -1;
}


ApjMotionFormat AProject_GetMotionFormat (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->Fmt;
}

grBoolean AProject_SetMotionFormat (AProject *pProject, const int Index, const ApjMotionFormat Fmt)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);
	assert ((Fmt > ApjMotion_Invalid) && (Fmt < ApjMotion_TypeCount));

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	pEntry->Fmt = Fmt;

	return GR_TRUE;
}

const char *AProject_GetMotionName (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->Name;
}

grBoolean AProject_SetMotionName (AProject *pProject, const int Index, const char *MotionName)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	if (AProject_SetString (&pEntry->Name, MotionName) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting Motion name",NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

const char *AProject_GetMotionFilename (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->Filename;
}

grBoolean AProject_SetMotionFilename (AProject *pProject, const int Index, const char *Filename)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	if (AProject_SetString (&pEntry->Filename, Filename) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting Motion filename",NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

grBoolean AProject_GetMotionOptimizationFlag (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->OptFlag;
}

grBoolean AProject_SetMotionOptimizationFlag (AProject *pProject, const int Index, const grBoolean Flag)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);

	pEntry->OptFlag = Flag;
	return GR_TRUE;
}

int AProject_GetMotionOptimizationLevel (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->OptLevel;
}

grBoolean AProject_SetMotionOptimizationLevel (AProject *pProject, const int Index, const int OptLevel)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);
	assert ((OptLevel >= 0) && (OptLevel <= 9));

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	pEntry->OptLevel = OptLevel;

	return GR_TRUE;
}

const char *AProject_GetMotionBone (const AProject *pProject, const int Index)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	return pEntry->Bone;
}

grBoolean AProject_SetMotionBone (AProject *pProject, const int Index, const char *BoneName)
{
	ApjMotionEntry *pEntry;

	assert (Index < pProject->Motions.Count);

	pEntry = Array_ItemPtr (pProject->Motions.Items, Index);
	if (AProject_SetString (&pEntry->Bone, BoneName) == GR_FALSE)
	{
		grErrorLog_AddString (GR_ERR_MEMORY_RESOURCE, "Setting Motion bone",NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}
