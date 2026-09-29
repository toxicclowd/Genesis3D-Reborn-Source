/****************************************************************************************/
/*  UTIL.H                                                                              */
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
#pragma once

#ifndef UTIL_H
#define UTIL_H

#include <Math.h>

#include "../include/ExtBox.h"
#include "Point.h"
#include "../include/VFile.h"
#include "../include/XForm3d.h"

#pragma warning( disable: 4514 )// Do NOT report "Inline Function Removed" as a warning

#ifdef __cplusplus
extern "C" {
#endif

#define UTIL_MAX_RESOURCE_LENGTH	(128)
#define FLOAT_NEAR_ZERO				(0.00001f)

void		Util_Init( unsigned long hStringResources ) ;

char *		Util_GetRcString( char * psz, const unsigned int idResource ) ;
char *		Util_LoadLocalRcString(unsigned int resid);  //allocates memory and fills string
char *		Util_LoadText( unsigned int resid );
grBoolean	Util_IsKeyDown( int vKey ) ;
char *		Util_StrDup( const char * const psz ) ;
int			Util_GetAppPath( char *Buf, int BufSize );
float		Util_GetTime();

// GDI THIN WRAPS
grBoolean	Util_Polyline( int32 hDC, Point * pPoints, int32 nPoints ) ;

// MATH AND SUCH
void		Util_ExtBox_Union( const grExtBox *B1, const grExtBox *B2, grExtBox *Result ) ;
void		Util_geExtBox_ExtendToEnclose( grExtBox *B, const grVec3d *Point ) ;
grFloat		Util_geExtBox_GetExtent( const grExtBox *B, int32 nElement ) ;
void		Util_geExtBox_InitFromTwoPoints( grExtBox * B, const grVec3d * p1, const grVec3d * p2 ) ;
grBoolean	Util_geExtBox_Intersection( const grExtBox *B1, const grExtBox *B2, grExtBox *Result	);
void		Util_ExtBox_SetInvalid( grExtBox *B ) ;
void		Util_ExtBox_Transform( const grExtBox *B, const grXForm3d *XForm, grExtBox *Result ) ;
void		Util_ExtBox_TransformJ( const grExtBox *B, const grXForm3d *XForm, grExtBox *Result ) ;
grFloat		Util_log2( grFloat f ) ;
grFloat		Util_NearestLowerPowerOf2( const grFloat fVal ) ;

int32		Util_Time( void ) ;

grFloat		Util_PointToLineDistanceSquared( const Point * pL1, const Point * pL2, const Point * pPoint ) ;
grFloat		Util_PointDistanceSquared( const Point * pL1, const Point * pL2 ) ;

// FILE STUFF
void		Util_DriveAndPathOnly( char * pszPath ) ;
void		Util_NameOnly( char * pszPath ) ;
void		Util_NewExtension(char * pszString, const char * pszNewExt) ;
void		Util_StripTrailingBackslash(char * pszString ) ;
grBoolean	Util_geVFile_ReadString( grVFile * pFile, char * pszBuffer, const int32 nMaxChars ) ;

__inline grBoolean IsFloatZero( grFloat f )
{
	if( fabs( f ) < FLOAT_NEAR_ZERO )
		return GR_TRUE ;

	return GR_FALSE ;	 
}
__inline grFloat Util_Round( grFloat f )
{
	return (grFloat)floor((f)+0.5f) ;
}

#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: Util.h */