/****************************************************************************************/
/*  OBJUTIL.C                                                                           */
/*                                                                                      */
/*  Author:  Peter Siamidis                                                             */
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vfile.h"
#include "bitmap.h"
#include "ram.h"
#include "errorlog.h"
#include "grResource.h"
#include "ObjUtil.h"


////////////////////////////////////////////////////////////////////////////////////////
//	Globals
////////////////////////////////////////////////////////////////////////////////////////
static char		*NoSelection = "< none >";



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_StrDup()
//
////////////////////////////////////////////////////////////////////////////////////////
char * ObjUtil_StrDup(
	const char	*const psz )	// string to copy
{

	// copy string
	char * p = (char *)grRam_Allocate( strlen( psz ) + 1 );
	if ( p ) 
	{
		strcpy( p, psz );
	}
	else
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
	}

	// return string
	return p;

} // ObjUtil_StrDup()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_LoadLibraryString()
//
////////////////////////////////////////////////////////////////////////////////////////
char * ObjUtil_LoadLibraryString(
	HINSTANCE		hInstance,	// instance handle
	unsigned int	ID )		// message id
{

	// locals
	#define		MAX_STRING_SIZE	256
	static char	StringBuf[MAX_STRING_SIZE];
	char		*NewString;
	int			Size;

	// ensure valid data
	assert( hInstance != NULL );
	assert( ID >= 0 );

	// get resource string
	Size = LoadString( hInstance, ID, StringBuf, MAX_STRING_SIZE );
	if ( Size <= 0 )
	{
		grErrorLog_Add( GR_ERR_WINDOWS_API_FAILURE, "Failed to get resource string" );
		return NULL;
	}

	// copy resource string
	NewString = grRam_Allocate( Size + 1 );
	if ( NewString == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Failed to allocate string memory" );
		return NULL;
	}
	strcpy( NewString, StringBuf );

	// all done
	return NewString;

} // ObjUtil_LoadLibraryString()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_LogError()
//
////////////////////////////////////////////////////////////////////////////////////////
void ObjUtil_LogError(
	HINSTANCE	hInstance,	// instance to get strings from
	int			Type,		// error type
	int			Message )	// error message
{

	// locals
	char	StringMessage[256];

	// ensure valid data
	assert( hInstance != NULL );
	assert( Type >= 0 );
	assert( Message >= 0 );
	
	// log generic error message if we have no hInstance
	if ( hInstance == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "No class handle" );
		return;
	}
		
	// get error message
	if ( LoadString( hInstance, Message, StringMessage, 256 ) <= 0 )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Could not get error message string" );
		return;
	}

	// log error
	grErrorLog_Add( Type, StringMessage );

} // ObjUtil_LogError()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_WriteString()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean ObjUtil_WriteString(
	grVFile	*File,		// file to write to
	char	*String )	// string to write out
{

	// locals
	int			Size;
	grBoolean	Result = GR_TRUE;

	// ensure valid data
	assert( File != NULL );
	assert( String != NULL );

	// write out complete
	Size = strlen( String ) + 1;
	assert( Size > 0 );
	Result &= grVFile_Write( File, &Size, sizeof( Size ) );
	Result &= grVFile_Write( File, String, Size );

	// all done
	return Result;

} // ObjUtil_WriteString()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_ReadString()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean ObjUtil_ReadString(
	grVFile	*File,		// file to read from
	char	**String )	// where to save string pointer
{

	// locals
	int			Size;
	grBoolean	Result = GR_TRUE;

	// ensure valid data
	assert( File != NULL );
	assert( String != NULL );

	// read string
	Result &= grVFile_Read( File, &( Size ), sizeof( Size ) );
	if ( ( Size > 0 ) && ( Result == GR_TRUE ) )
	{
		*String = grRam_Allocate( Size );
		if ( *String == NULL )
		{
			return GR_FALSE;
		}
		Result &= grVFile_Read( File, *String, Size );
	}

	// all done
	return Result;

} // ObjUtil_ReadString()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_CreateBitmapFromFileName()
//
//	Create a bitmap from a file.
//
////////////////////////////////////////////////////////////////////////////////////////
grBitmap * ObjUtil_CreateBitmapFromFileName(
	grVFile		*File,			// file system to use
	const char	*Name,			// name of the file
	const char	*AlphaName )	// name of the alpha file
{

	// locals
	grVFile		*BmpFile;
	grBitmap	*Bmp;
	grBoolean	Result;

	// ensure valid data
	assert( Name != NULL );

	// open the bitmap
	if ( File == NULL )
	{
		BmpFile = grVFile_OpenNewSystem( NULL, GR_VFILE_TYPE_DOS, Name, NULL, GR_VFILE_OPEN_READONLY );
	}
	else
	{
		BmpFile = grVFile_Open( File, Name, GR_VFILE_OPEN_READONLY );
	}
	if ( BmpFile == NULL )
	{
		grErrorLog_Add( GR_ERR_FILEIO_OPEN, NULL );
		return NULL;
	}

	// create the bitmap
	Bmp = grBitmap_CreateFromFile( BmpFile );
	grVFile_Close( BmpFile );
	if ( Bmp == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		return NULL;
	}

	// add alpha if required...
	if ( AlphaName != NULL )
	{

		// locals
		grBitmap	*AlphaBmp;
		grVFile		*AlphaFile;

		// open alpha file
		if ( File == NULL )
		{
			AlphaFile = grVFile_OpenNewSystem( NULL, GR_VFILE_TYPE_DOS, AlphaName, NULL, GR_VFILE_OPEN_READONLY );
		}
		else
		{
			AlphaFile = grVFile_Open( File, AlphaName, GR_VFILE_OPEN_READONLY );
		}
		if( AlphaFile == NULL )
		{
			grErrorLog_Add( GR_ERR_FILEIO_OPEN, NULL );
			grBitmap_Destroy( &Bmp );
			return NULL;
		}

		// create alpha bitmap
		AlphaBmp = grBitmap_CreateFromFile( AlphaFile );
		grVFile_Close( AlphaFile );
		if ( AlphaBmp == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			grBitmap_Destroy( &Bmp );
			return NULL;
		}

		// fail if alpha isn't same size as main bitmap
		if (	( grBitmap_Width( Bmp ) != grBitmap_Width( AlphaBmp ) ) ||
				( grBitmap_Height( Bmp ) != grBitmap_Height( AlphaBmp ) ) )
		{
			grErrorLog_Add( GR_ERR_BAD_PARAMETER, NULL );
			grBitmap_Destroy( &AlphaBmp );
			grBitmap_Destroy( &Bmp );
			return NULL;
		}

		// set its alpha
		Result = grBitmap_SetAlpha( Bmp, AlphaBmp );
		if ( Result == GR_FALSE )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			grBitmap_Destroy( &AlphaBmp );
			grBitmap_Destroy( &Bmp );
			return NULL;
		}

		// don't need the alpha anymore
		grBitmap_Destroy( &AlphaBmp );
	}
	// ...or just set the color key
	else
	{
		Result = grBitmap_SetColorKey( Bmp, GR_TRUE, 255, GR_FALSE );
		assert( Result );
	}

	// all done
	return Bmp;

} // ObjUtil_CreateBitmapFromFileName()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_DestroyBitmapList()
//
////////////////////////////////////////////////////////////////////////////////////////
void ObjUtil_DestroyBitmapList(
	BitmapList	**DeadList )	// list to destroy
{

	// locals
	BitmapList	*List;
	int			i;

	// ensure valid data
	assert( DeadList != NULL );
	assert( *DeadList != NULL );

	// get list pointer
	List = *DeadList;

	// destroy file list
	if ( List->Name != NULL )
	{
		assert( List->Total > 0 );
		for ( i = 0; i < List->Total; i++ )
		{
			if ( List->Name[i] != NULL )
			{
				grRam_Free( List->Name[i] );
			}
		}
		grRam_Free( List->Name );
	}

	// destroy width and height lists
	if ( List->Width != NULL )
	{
		grRam_Free( List->Width );
	}
	if ( List->Height != NULL )
	{
		grRam_Free( List->Height );
	}

	// destroy numeric sizes list
	if ( List->NumericSizes != NULL )
	{
		grRam_Free( List->NumericSizes );
	}

	// destroy string sizes list
	if ( List->StringSizes != NULL )
	{
		for ( i = 0; i < List->SizesListSize; i++ )
		{
			assert( List->StringSizes[i] != NULL );
			grRam_Free( List->StringSizes[i] );
		}
	}

	// destroy active list
	if ( List->ActiveList != NULL )
	{
		grRam_Free( List->ActiveList );
	}

	// free bitmaplist struct
	grRam_Free( List );

	// zap pointer
	*DeadList = NULL;

} // ObjUtil_DestroyBitmapList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_TextureGroupSetActiveList()
//
////////////////////////////////////////////////////////////////////////////////////////
static void ObjUtil_TextureGroupSetActiveList(
	BitmapList	*AvailableArt,		// list of all available art
	int			NewActiveCurSize )	// new active size slot
{

	// locals
	int	BmpNum;

	// ensure valid data
	assert( AvailableArt != NULL );
	assert( NewActiveCurSize >= 0 );
	assert( NewActiveCurSize < AvailableArt->SizesListSize );

	// setup current list
	AvailableArt->ActiveCurSize = NewActiveCurSize;
	AvailableArt->ActiveList[0] = AvailableArt->Name[0];
	AvailableArt->ActiveCount = 1;
	for ( BmpNum = 1; BmpNum < AvailableArt->Total; BmpNum++ )
	{

		// zap old entry
		AvailableArt->ActiveList[BmpNum] = NULL;

		// add new entry if required
		if ( AvailableArt->Width[BmpNum] == AvailableArt->NumericSizes[AvailableArt->ActiveCurSize] )
		{
			AvailableArt->ActiveList[AvailableArt->ActiveCount] = AvailableArt->Name[BmpNum];
			AvailableArt->ActiveCount++;
		}
	}

} // ObjUtil_TextureGroupSetActiveList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_CreateBitmapList()
//
////////////////////////////////////////////////////////////////////////////////////////
BitmapList * ObjUtil_CreateBitmapList(
	grResourceMgr	*ResourceMgr,	// resource manager to use
	char			*ResourceName,	// name of resource
	char			*FileFilter )	// file filter
{

	// locals
	BitmapList		*Bmps;
	grVFile			*FileDir = NULL;
	grVFile_Finder	*Finder = NULL;
	int				CurFile;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceName != NULL );
	assert( FileFilter != NULL );

	// allocate bitmaplist struct
	Bmps = grRam_AllocateClear( sizeof( *Bmps ) );
	if ( Bmps == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return NULL;
	}

	// get vfile dir
	FileDir = grResource_GetVFile( ResourceMgr, ResourceName );
	if ( FileDir == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// determine how many files there are
	Bmps->Total = 1;
	while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
	{
		Bmps->Total++;
	}

	// destroy finder
	grVFile_DestroyFinder( Finder );
	Finder = NULL;

	// allocate name list
	Bmps->Name = grRam_AllocateClear( sizeof( char * ) * Bmps->Total );
	if ( Bmps->Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// allocate width list
	Bmps->Width = grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->Width == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// allocate height list
	Bmps->Height = grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->Height == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// allocate numeric sizes list
	Bmps->NumericSizes = grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->NumericSizes == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// allocate string sizes list
	Bmps->StringSizes = grRam_AllocateClear( sizeof( char * ) * Bmps->Total );
	if ( Bmps->StringSizes == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_ObjUtil_BuildBitmapList;
	}

	// first entry is always the "no selection" slot
	CurFile = 0;
	Bmps->Name[CurFile++] = ObjUtil_StrDup( NoSelection );

	// build file list
	while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
	{

		// locals
		grVFile_Properties	Properties;
		grBitmap			*Bitmap;

		// get properties of current file
		if( grVFile_FinderGetProperties( Finder, &Properties ) == GR_FALSE )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_ObjUtil_BuildBitmapList;
		}

		// save file name
		assert( CurFile < Bmps->Total );
		Bmps->Name[CurFile] = ObjUtil_StrDup( Properties.Name );
		if ( Bmps->Name[CurFile] == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_ObjUtil_BuildBitmapList;
		}

		// save width and height
		Bitmap = ObjUtil_CreateBitmapFromFileName( FileDir, Bmps->Name[CurFile], NULL );
		if ( Bitmap == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_ObjUtil_BuildBitmapList;
		}
		Bmps->Width[CurFile] = grBitmap_Width( Bitmap );
		Bmps->Height[CurFile] = grBitmap_Height( Bitmap );
		grBitmap_Destroy( &Bitmap );

		// add size to numeric sizes list
		{

			// locals
			grBoolean	AddIt;
			int			i;

			// check if it needs to be added to numeric sizes list
			AddIt = GR_TRUE;
			for ( i = 0; i < Bmps->Total; i++ )
			{
				if ( Bmps->Width[CurFile] == Bmps->NumericSizes[i] )
				{
					AddIt = GR_FALSE;
					break;
				}
			}

			// add it if required
			if ( AddIt == GR_TRUE )
			{
				for ( i = 0; i < Bmps->Total; i++ )
				{
					if (	( Bmps->Width[CurFile] < Bmps->NumericSizes[i] ) ||
							( Bmps->NumericSizes[i] == 0 ) )
					{
						int	Hold1, Hold2;
						Hold1 = Bmps->Width[CurFile];
						for ( ; i < Bmps->Total; i++ )
						{
							Hold2 = Bmps->NumericSizes[i];
							Bmps->NumericSizes[i] = Hold1;
							Hold1 = Hold2;
						}
						AddIt = GR_FALSE;
						break;
					}
				}
			}
			assert( AddIt == GR_FALSE );
		}

		// adjust file counter
		CurFile++;
	}

	// destroy finder
	grVFile_DestroyFinder( Finder );

	// close vfile dir
	if ( grResource_DeleteVFile( ResourceMgr, ResourceName ) == 0 )
	{
		grVFile_Close( FileDir );
	}

	// create string sizes list
	{

		// locals
		int		i;
		char	Buf[256];

		// build list
		Bmps->SizesListSize = Bmps->Total;
		for ( i = 0; i < Bmps->Total; i++ )
		{
			if ( Bmps->NumericSizes[i] == 0 )
			{
				Bmps->SizesListSize = i;
				break;
			}
			itoa( Bmps->NumericSizes[i], Buf, 10 );
			Bmps->StringSizes[i] = ObjUtil_StrDup( Buf );
		}
	}

	// create active list
	{

		// allocate list
		Bmps->ActiveList = grRam_AllocateClear( sizeof( char * ) * Bmps->Total );
		if ( Bmps->ActiveList == NULL )
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
			goto ERROR_ObjUtil_BuildBitmapList;
		}

		// setup current list
		ObjUtil_TextureGroupSetActiveList( Bmps, 0 );
	}

	// return bitmaplist struct
	return Bmps;


	//
	//	error handling
	//
	ERROR_ObjUtil_BuildBitmapList:

	// destroy bitmap list
	assert( Bmps != NULL );
	ObjUtil_DestroyBitmapList( &Bmps );

	// destroy finder
	if ( Finder != NULL )
	{
		grVFile_DestroyFinder( Finder );
	}

	// close vfile dir
	if ( FileDir != NULL )
	{
		if ( grResource_DeleteVFile( ResourceMgr, ResourceName ) == 0 )
		{
			grVFile_Close( FileDir );
		}
	}

	// return failure
	return NULL;

} // ObjUtil_CreateBitmapList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_TextureGroupGetStringFromList()
//
////////////////////////////////////////////////////////////////////////////////////////
static char * ObjUtil_TextureGroupGetStringFromList(
	BitmapList	*AvailableArt,		// list of all available art
	char		*CompareString )	// string we are looking for
{

	// locals
	int	i;

	// ensure valid data
	assert( AvailableArt != NULL );
	assert( CompareString != NULL );

	// locate string in available bitmaps list
	for ( i = 0; i < AvailableArt->Total; i++ )
	{
		assert( AvailableArt->Name[i] != NULL );
		if ( stricmp( AvailableArt->Name[i], CompareString ) == 0 )
		{
			return AvailableArt->Name[i];
		}
	}

	// if we got to here then no string was found
	return NULL;

} // ObjUtil_TextureGroupGetStringFromList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_TextureGroupSetSize()
//
////////////////////////////////////////////////////////////////////////////////////////
void ObjUtil_TextureGroupSetSize(
	grEngine		*Engine,			// engine to use
	grResourceMgr	*ResourceMgr,		// resource manager to use
	BitmapList		*AvailableArt,		// list of all available art
	char			*ChosenSizeName,	// name of size that was chosen
	char			**SaveBitmapName,	// current bitmap name and where to save new name
	char			**SaveAlphaName,	// current alpha name and where to save new name
	grBitmap		**SaveArt,			// current art and where to save new art
	char			**SaveArtName )		// current art name and where to save new name
{

	// locals
	grBoolean	SizeChangeFailed = GR_TRUE;
	int			i;

	// ensure valid data
	assert( Engine != NULL );
	assert( ResourceMgr != NULL );
	assert( AvailableArt != NULL );
	assert( ChosenSizeName != NULL );
	assert( SaveBitmapName != NULL );
	assert( SaveAlphaName != NULL );
	assert( SaveArt != NULL );
	assert( SaveArtName != NULL );

	// locate new active size
	for ( i = 0; i < AvailableArt->SizesListSize; i++ )
	{
		if ( stricmp( AvailableArt->StringSizes[i], ChosenSizeName ) == 0 )
		{

			// do nothing if same size was chosen
			if ( i == AvailableArt->ActiveCurSize )
			{
				return;
			}

			// make adjustments
			ObjUtil_TextureGroupSetActiveList( AvailableArt, i );
			SizeChangeFailed = GR_FALSE;
			i = AvailableArt->SizesListSize;
		}
	}
	assert( SizeChangeFailed == GR_FALSE );

	// reset bitmap pointers
	*SaveBitmapName = AvailableArt->ActiveList[0];
	*SaveAlphaName = AvailableArt->ActiveList[0];

	// destroy existing art
	if ( *SaveArt != NULL )
	{

		// free old art
		grEngine_RemoveBitmap( Engine, *SaveArt );
		assert( *SaveArtName != NULL );
		if ( grResource_Delete( ResourceMgr, *SaveArtName ) == 0 )
		{
			grBitmap_Destroy( &( *SaveArt ) );
		}
		*SaveArt = NULL;

		// free old art name
		grRam_Free( *SaveArtName );
		*SaveArtName = NULL;
	}

} // ObjUtil_TextureGroupSetSize()



////////////////////////////////////////////////////////////////////////////////////////
//
//	ObjUtil_TextureGroupSetArt()
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean ObjUtil_TextureGroupSetArt(
	grEngine		*Engine,			// engine to use
	grResourceMgr	*ResourceMgr,		// resource manager to use
	BitmapList		*AvailableArt,		// list of all available art
	char			*ChosenBitmapName,	// name of bitmap that was chosen
	char			*ChosenAlphaName,	// name of alpha that was chosen
	char			**SaveBitmapName,	// current bitmap name and where to save new name
	char			**SaveAlphaName,	// current alpha name and where to save new name
	grBitmap		**SaveArt,			// current art and where to save new art
	char			**SaveArtName )		// current art name and where to save new name
{

	// ensure valid data
	assert( Engine != NULL );
	assert( ResourceMgr != NULL );
	assert( AvailableArt != NULL );
	assert( ( ( ChosenBitmapName == NULL ) && ( ChosenAlphaName == NULL ) ) == GR_FALSE );
	assert( SaveBitmapName != NULL );
	assert( SaveAlphaName != NULL );
	assert( SaveArt != NULL );
	assert( SaveArtName != NULL );

	// destroy existing art
	if ( *SaveArt != NULL )
	{

		// free old art
		grEngine_RemoveBitmap( Engine, *SaveArt );
		assert( *SaveArtName != NULL );
		if ( grResource_Delete( ResourceMgr, *SaveArtName ) == 0 )
		{
			grBitmap_Destroy( &( *SaveArt ) );
		}
		*SaveArt = NULL;

		// free old art name
		grRam_Free( *SaveArtName );
		*SaveArtName = NULL;
	}

	// get chosen string from available art list
	if ( ChosenBitmapName != NULL )
	{
		*SaveBitmapName = ObjUtil_TextureGroupGetStringFromList( AvailableArt, ChosenBitmapName );
	}
	else
	{
		*SaveAlphaName = ObjUtil_TextureGroupGetStringFromList( AvailableArt, ChosenAlphaName );
	}
	assert( *SaveBitmapName != NULL );
	assert( *SaveAlphaName != NULL );

	// do nothing further if there is no main bitmap
	if ( stricmp( *SaveBitmapName, NoSelection ) == 0 )
	{
		return GR_TRUE;
	}

	// build art name
	{

		// locals
		int	Size;

		// determine length of full art name
		Size = strlen( *SaveBitmapName ) + 1;
		if ( stricmp( *SaveAlphaName, NoSelection ) != 0 )
		{
			Size += strlen( *SaveAlphaName );
		}

		// create full artname
		*SaveArtName = grRam_Allocate( Size );
		if ( *SaveArtName == NULL )
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Failed to allocate full art name" );
			goto ERROR_ObjUtil_TextureGroupSetArt;
		}
		strcpy( *SaveArtName, *SaveBitmapName );
		if ( stricmp( *SaveAlphaName, NoSelection ) != 0 )
		{
			strcat( *SaveArtName, *SaveAlphaName );
		}
	}

	// get new art
	*SaveArt = grResource_Get( ResourceMgr, *SaveArtName );

	// if it doesn't exist then create it
	if ( *SaveArt == NULL )
	{

		// locals
		grVFile	*FileDir;

		// get vfile dir
		FileDir = grResource_GetVFile( ResourceMgr, "GlobalMaterials" );
		if ( FileDir == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Failed to get a vfile resource" );
			goto ERROR_ObjUtil_TextureGroupSetArt;
		}

		// create new art
		if ( stricmp( *SaveAlphaName, NoSelection ) != 0 )
		{
			*SaveArt = ObjUtil_CreateBitmapFromFileName( FileDir, *SaveBitmapName, *SaveAlphaName );
		}
		else
		{
			*SaveArt = ObjUtil_CreateBitmapFromFileName( FileDir, *SaveBitmapName, NULL );
		}

		// close vfile dir
		if ( grResource_DeleteVFile( ResourceMgr, "GlobalMaterials" ) == 0 )
		{
			grVFile_Close( FileDir );
		}

		// fail if art wasnt created
		if ( *SaveArt == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Failed to create new art" );
			goto ERROR_ObjUtil_TextureGroupSetArt;
		}

		// add it to the resource manager
		grResource_Add( ResourceMgr, *SaveArtName, GR_RESOURCE_BITMAP, *SaveArt );
	}

	// add it to the engine
	grEngine_AddBitmap( Engine, *SaveArt, GR_ENGINE_BITMAP_TYPE_3D );

	// all done
	return GR_TRUE;


	//
	//	error handling
	//
	ERROR_ObjUtil_TextureGroupSetArt:

	// free full art name
	if ( *SaveArtName != NULL )
	{
		grRam_Free( *SaveArtName );
		*SaveArtName = NULL;
	}

	// return failure
	return GR_FALSE;

} // ObjUtil_TextureGroupSetArt()
