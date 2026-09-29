/****************************************************************************************/
/*  FILEPATH.H																			*/
/*                                                                                      */
/*  Author: Jim Mischel		                                                            */
/*  Description: Useful file and pathname functions.									*/
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
#ifndef FILEPATH_H

#define FILEPATH_H

#include "basetype.h"

#ifdef __cplusplus
	extern "C" {
#endif

// Extract drive (d:\) from pPath and place in pDrive
grBoolean FilePath_GetDrive (char const *pPath, char *pDrive);

// Extract directory from pPath and place in pDir
grBoolean FilePath_GetDir (char const *pPath, char *pDir);

// Extract Name from pPath and place in pName
grBoolean FilePath_GetName (char const *pPath, char *pName);

// Extract Extension from pPath and place in pExt
grBoolean FilePath_GetExt (char const *pPath, char *pExt);

// Extract drive and directory from pPath and place in pDriveDir
// pDriveDir may be the same as pPath
grBoolean FilePath_GetDriveAndDir (char const *pPath, char *pDriveDir);

// Extract Name and extension from pPath and place in pName
// pName may be the same as pPath
grBoolean FilePath_GetNameAndExt (char const *pPath, char *pName);

// set extension of pSourceFile to pExt and place result in pDestFile
// pDestFile may be the same as pSourceFile
grBoolean FilePath_SetExt (char const *pSourceFile, char const *pExt, char *pDestFile);

// Terminate pPath with a slash (by appending if necessary), and return result in pDest.
// pPath and pDest may be the same.
grBoolean FilePath_SlashTerminate (const char *pPath, char *pDest);

// Append pName to pPath and return result in pDest.
// pDest may be the same as pPath or pName.
grBoolean FilePath_AppendName (char const *pPath, char const *pName, char *pDest);

// Search for a Filename in the semicolon-separated paths specified in SearchPath.
// If found, returns GR_TRUE and the full path name of the file in FoundPath.
// Returns GR_FALSE if unsuccessful.
grBoolean FilePath_SearchForFile (const char *Filename, const char *SearchPath, char *FoundPath);


grBoolean FilePath_AppendSearchDir (char *SearchList, const char *NewDir);
grBoolean FilePath_ResolveRelativePath (const char *Relative, char *Resolved);
grBoolean FilePath_ResolveRelativePathList (const char *RelativeList, char *ResolvedList);

#ifdef __cplusplus
	}
#endif


#endif
