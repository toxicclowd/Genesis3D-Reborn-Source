////////////////////////////////////////////////////////////////////////////////////////
//	Bitmaplist struct
////////////////////////////////////////////////////////////////////////////////////////
typedef struct BitmapList
{
	int		Total;
	char	**Name;
	int		*Width;
	int		*Height;
	int		*NumericSizes;
	char	**StringSizes;
	int		SizesListSize;

} BitmapList;
static BitmapList		*Bitmaps;
static char				*NoSelection = "< none >";

// Added by Incarnadine
// This function gets the correct translation for a collision/render box.  The problem was,
// boxes are positioned by their center, while ActorObj may be positioned anywhere.  This
// takes that into account and returns where the box should be positioned.
void GetBoxTranslation(const grExtBox *Box, const grActor *Actor, grVec3d *Translation)
{
	grVec3d BoxCenterToActor;
	grVec3d BoxCenter;	
	grVec3d TranslationMod;
	grVec3d ActorPos, Zero;
	ActorObj *Object;

	assert(Box != NULL);
	assert(Translation != NULL);
	assert(Actor != NULL);
	assert(Actor->Object != NULL);	
	
	Object = Actor->Object;
	grVec3d_Set(&Zero,0,0,0);
	ActorPos = Actor->Xf.Translation;	
	grExtBox_GetTranslation(Box,&BoxCenter);
	grVec3d_Subtract(&BoxCenter, &Zero,&BoxCenterToActor);
	BoxCenterToActor.X = 0.0f;//*= Object->ScaleX;
	BoxCenterToActor.Y *= Object->ScaleX;
	BoxCenterToActor.Z = 0.0f;//*= Object->ScaleZ;	
	grVec3d_Subtract(&BoxCenterToActor,&BoxCenter,&TranslationMod);
	grVec3d_Add(&TranslationMod,&ActorPos,Translation);
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_StrDup()
//
////////////////////////////////////////////////////////////////////////////////////////
static char * Util_StrDup(
	const char	*const String )	// string to copy
{

	// locals
	char	*NewString;

	// ensure valid data
	assert( String != NULL );

	// copy string
	NewString = (char *)grRam_Allocate( strlen( String ) + 1 );
	if ( NewString ) 
	{
		strcpy( NewString, String );
	}
	else
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
	}

	// return string
	return NewString;

} // Util_StrDup()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_ResetActorMaterialToDefault()
//
//	Resets an actors material to its default one.
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean Util_ResetActorMaterialToDefault(
	grActor		*Actor,				// actor to reset
	grActor_Def	*ActorDef,			// actor def from which to get default info from
	int			MaterialIndex )		// material index
{

	// locals
	grBody		*Body;
	grBoolean	Result;
	const char	*MaterialName;
	grMaterialSpec	*Bitmap;
	grFloat		Red, Green, Blue;
	grUVMapper	Mapper;

	// ensure valid data
	assert( Actor != NULL );
	assert( ActorDef != NULL );
	assert( MaterialIndex >= 0 );

	// get actor def body
	Body = grActor_GetBody( ActorDef );
	assert ( Body != NULL );

	// get default material
	Result = grBody_GetMaterial(	Body, MaterialIndex, &MaterialName,
									&Bitmap, &Red, &Green, &Blue,
									&Mapper );
	if ( Result == GR_FALSE )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "Failed to reset actor material to default" );
		return GR_FALSE;
	}

	// reset actor matet
	return grActor_SetMaterial( Actor, MaterialIndex, Bitmap, Red, Green, Blue, Mapper );

} // Util_ResetActorMaterialToDefault()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_CreateBitmapFromFileName()
//
//	Create a bitmap from a file.
//
////////////////////////////////////////////////////////////////////////////////////////
static grMaterialSpec * Util_CreateBitmapFromFileName(
	grVFile		*File,			// file system to use
	const char	*Name,			// name of the file
	const char	*AlphaName,		// name of the alpha file
	const grResourceMgr* ResourceMgr)	
{

	// locals
	grVFile		*BmpFile;
	grBitmap	*Bmp;
	grMaterialSpec *MatSpec;
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
	
	MatSpec = grMaterialSpec_Create(grResourceMgr_GetEngine(ResourceMgr), (grResourceMgr*) ResourceMgr);
	
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

#pragma message ("Krouer: change NULL to something better next time")
	grMaterialSpec_AddLayerFromBitmap(MatSpec, 0, Bmp, NULL);

	// all done
	return MatSpec;

} // Util_CreateBitmapFromFileName()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_DestroyBitmapList()
//
////////////////////////////////////////////////////////////////////////////////////////
static void Util_DestroyBitmapList(
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

	// free bitmaplist struct
	grRam_Free( List );

	// zap pointer
	*DeadList = NULL;

} // Util_DestroyBitmapList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_CreateBitmapList()
//
////////////////////////////////////////////////////////////////////////////////////////
static BitmapList * Util_CreateBitmapList(
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
	Bmps = (BitmapList *)grRam_AllocateClear( sizeof( *Bmps ) );
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
		goto ERROR_Util_BuildBitmapList;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_Util_BuildBitmapList;
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
	Bmps->Name = (char **)grRam_AllocateClear( sizeof( char * ) * Bmps->Total );
	if ( Bmps->Name == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// allocate width list
	Bmps->Width = (int *)grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->Width == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// allocate height list
	Bmps->Height = (int *)grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->Height == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// allocate numeric sizes list
	Bmps->NumericSizes = (int *)grRam_AllocateClear( sizeof( int * ) * Bmps->Total );
	if ( Bmps->NumericSizes == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// allocate string sizes list
	Bmps->StringSizes = (char **)grRam_AllocateClear( sizeof( char * ) * Bmps->Total );
	if ( Bmps->StringSizes == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_Util_BuildBitmapList;
	}

	// first entry is always the "no selection" slot
	CurFile = 0;
	Bmps->Name[CurFile++] = Util_StrDup( NoSelection );

	// build file list
	while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
	{

		// locals
		grVFile_Properties	Properties;
		//grBitmap			*Bitmap;
		grMaterialSpec		*MatSpec;

		// get properties of current file
		if( grVFile_FinderGetProperties( Finder, &Properties ) == GR_FALSE )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_Util_BuildBitmapList;
		}

		// save file name
		assert( CurFile < Bmps->Total );
		Bmps->Name[CurFile] = Util_StrDup( Properties.Name );
		if ( Bmps->Name[CurFile] == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_Util_BuildBitmapList;
		}

		// save width and height
		MatSpec = Util_CreateBitmapFromFileName( FileDir, Bmps->Name[CurFile], NULL, ResourceMgr );
		if ( MatSpec == NULL )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			goto ERROR_Util_BuildBitmapList;
		}
		Bmps->Width[CurFile] = grMaterialSpec_Width( MatSpec );
		Bmps->Height[CurFile] = grMaterialSpec_Height( MatSpec );
		grMaterialSpec_Destroy( &MatSpec );

		// add sise to numeric sizes list
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
			sprintf(Buf, "%i" , Bmps->NumericSizes[i]); //itoa( Bmps->NumericSizes[i], Buf, 10 );
			Bmps->StringSizes[i] = Util_StrDup( Buf );
		}
	}

	// return bitmaplist struct
	return Bmps;

	// error handling
	ERROR_Util_BuildBitmapList:

	// destroy bitmap list
	assert( Bmps != NULL );
	Util_DestroyBitmapList( &Bmps );

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

} // Util_CreateBitmapList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_DrawPoly()
//
///////////////////////////////////////////////////////////////////////////////////////
static void Util_DrawPoly(
	grWorld		*World,	// world in which to draw poly
	grLVertex	*V1,	// top left
	grLVertex	*V2,	// top right
	grLVertex	*V3,	// bottom right
	grLVertex	*V4 )	// bottom left
{

	// locals
	grUserPoly	*Poly;

	// ensure valid data
	assert( World != NULL );
	assert( V1 != NULL );
	assert( V2 != NULL );
	assert( V3 != NULL );
	assert( V4 != NULL );

	// draw poly
	Poly = grUserPoly_CreateQuad( V1, V2, V3, V4, NULL, GR_RENDER_FLAG_ALPHA );
	grWorld_AddUserPoly( World, Poly, GR_TRUE );
	grUserPoly_Destroy( &Poly );

} // Util_DrawPoly()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_DrawExtBox()
//
///////////////////////////////////////////////////////////////////////////////////////
static void Util_DrawExtBox(
	grWorld		*World,		// world to draw it in
	GR_RGBA		*Color,		// color to draw it in
	grExtBox	*ExtBox )	// extent box to draw
{

	// locals
	grLVertex	Vertex[4];
	int			i;

	// ensure valid data
	assert( World != NULL );
	assert( Color != NULL );
	assert( ExtBox != NULL );

	// init vert struct
	for ( i = 0; i < 4; i++ )
	{
		Vertex[i].a = Color->a;
		Vertex[i].r = Color->r;
		Vertex[i].g = Color->g;
		Vertex[i].b = Color->b;
	}
	Vertex[0].u = 0.0f;
	Vertex[0].v = 0.0f;
	Vertex[1].u = 1.0f;
	Vertex[1].v = 0.0f;
	Vertex[2].u = 1.0f;
	Vertex[2].v = 1.0f;
	Vertex[3].u = 0.0f;
	Vertex[3].v = 1.0f;

	// side 1
	Vertex[0].X = ExtBox->Min.X;
	Vertex[0].Y = ExtBox->Max.Y;
	Vertex[0].Z = ExtBox->Min.Z;
	Vertex[1].X = ExtBox->Max.X;
	Vertex[1].Y = ExtBox->Max.Y;
	Vertex[1].Z = ExtBox->Min.Z;
	Vertex[2].X = ExtBox->Max.X;
	Vertex[2].Y = ExtBox->Min.Y;
	Vertex[2].Z = ExtBox->Min.Z;
	Vertex[3].X = ExtBox->Min.X;
	Vertex[3].Y = ExtBox->Min.Y;
	Vertex[3].Z = ExtBox->Min.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

	// side 2
	Vertex[0].X = ExtBox->Min.X;
	Vertex[0].Y = ExtBox->Max.Y;
	Vertex[0].Z = ExtBox->Max.Z;
	Vertex[1].X = ExtBox->Max.X;
	Vertex[1].Y = ExtBox->Max.Y;
	Vertex[1].Z = ExtBox->Max.Z;
	Vertex[2].X = ExtBox->Max.X;
	Vertex[2].Y = ExtBox->Min.Y;
	Vertex[2].Z = ExtBox->Max.Z;
	Vertex[3].X = ExtBox->Min.X;
	Vertex[3].Y = ExtBox->Min.Y;
	Vertex[3].Z = ExtBox->Max.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

	// side 3
	Vertex[0].X = ExtBox->Min.X;
	Vertex[0].Y = ExtBox->Max.Y;
	Vertex[0].Z = ExtBox->Min.Z;
	Vertex[1].X = ExtBox->Min.X;
	Vertex[1].Y = ExtBox->Max.Y;
	Vertex[1].Z = ExtBox->Max.Z;
	Vertex[2].X = ExtBox->Max.X;
	Vertex[2].Y = ExtBox->Max.Y;
	Vertex[2].Z = ExtBox->Max.Z;
	Vertex[3].X = ExtBox->Max.X;
	Vertex[3].Y = ExtBox->Max.Y;
	Vertex[3].Z = ExtBox->Min.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

	// side 4
	Vertex[0].X = ExtBox->Max.X;
	Vertex[0].Y = ExtBox->Max.Y;
	Vertex[0].Z = ExtBox->Min.Z;
	Vertex[1].X = ExtBox->Max.X;
	Vertex[1].Y = ExtBox->Max.Y;
	Vertex[1].Z = ExtBox->Max.Z;
	Vertex[2].X = ExtBox->Max.X;
	Vertex[2].Y = ExtBox->Min.Y;
	Vertex[2].Z = ExtBox->Max.Z;
	Vertex[3].X = ExtBox->Max.X;
	Vertex[3].Y = ExtBox->Min.Y;
	Vertex[3].Z = ExtBox->Min.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

	// side 5
	Vertex[0].X = ExtBox->Max.X;
	Vertex[0].Y = ExtBox->Min.Y;
	Vertex[0].Z = ExtBox->Min.Z;
	Vertex[1].X = ExtBox->Max.X;
	Vertex[1].Y = ExtBox->Min.Y;
	Vertex[1].Z = ExtBox->Max.Z;
	Vertex[2].X = ExtBox->Min.X;
	Vertex[2].Y = ExtBox->Min.Y;
	Vertex[2].Z = ExtBox->Max.Z;
	Vertex[3].X = ExtBox->Min.X;
	Vertex[3].Y = ExtBox->Min.Y;
	Vertex[3].Z = ExtBox->Min.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

	// side 6
	Vertex[0].X = ExtBox->Min.X;
	Vertex[0].Y = ExtBox->Min.Y;
	Vertex[0].Z = ExtBox->Min.Z;
	Vertex[1].X = ExtBox->Min.X;
	Vertex[1].Y = ExtBox->Min.Y;
	Vertex[1].Z = ExtBox->Max.Z;
	Vertex[2].X = ExtBox->Min.X;
	Vertex[2].Y = ExtBox->Max.Y;
	Vertex[2].Z = ExtBox->Max.Z;
	Vertex[3].X = ExtBox->Min.X;
	Vertex[3].Y = ExtBox->Max.Y;
	Vertex[3].Z = ExtBox->Min.Z;
	Util_DrawPoly( World, &( Vertex[0] ), &( Vertex[1] ), &( Vertex[2] ), &( Vertex[3] ) );

} // Util_DrawExtBox()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_CreateEmptyList()
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean Util_CreateEmptyList(
	char	***List,		// where to save list pointer
	int		*ListSize )		// where to save list size
{

	// locals
	char	**NewList;

	// ensure valid data
	assert( List != NULL );
	assert( ListSize != NULL );

	// reset passed in data
	*List = NULL;
	*ListSize = 0;

	// allocate list
	NewList = (char **)grRam_Allocate( sizeof( char * ) );
	if ( NewList == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return GR_FALSE;
	}

	// init data
	NewList[0] = Util_StrDup( NoSelection );
	*ListSize = 1;

	// all done
	*List = NewList;
	return GR_TRUE;

} // Util_CreateEmptyList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_WriteString()
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean Util_WriteString(
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

	// log errors
	if ( Result != GR_TRUE )
	{
		grErrorLog_Add( GR_ERR_FILEIO_WRITE, NULL );
	}

	// all done
	return Result;

} // Util_WriteString()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_DestroyFileList()
//
////////////////////////////////////////////////////////////////////////////////////////
static void Util_DestroyFileList(
	char	***DeadList,		// list to destroy
	int		*DeadListSize )		// size of list
{

	// locals
	char	**List;
	int		ListSize;
	int		i;

	// save pointers
	assert( DeadList != NULL );
	List = *DeadList;
	assert( List != NULL );
	assert( DeadListSize != NULL );
	ListSize = *DeadListSize;
	assert( ListSize > 0 );

	// free all strings
	for ( i = 0; i < ListSize; i++ )
	{
		if ( List[i] != NULL )
		{
			grRam_Free( List[i] );
			List[i] = NULL;
		}
	}

	// free the list itself
	grRam_Free( List );

	// zap final data
	List = NULL;
	ListSize = 0;

} // Util_DestroyFileList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_BuildFileList()
//
////////////////////////////////////////////////////////////////////////////////////////
static char ** Util_BuildFileList(
	grResourceMgr	*ResourceMgr,
	char			*ResourceName,
	char			*FileFilter,
	int				*FileListSize )
{

	// locals
	grVFile			*FileDir = NULL;
	grVFile_Finder	*Finder = NULL;
	int				TotalFiles = 0;
	int				CurFile;
	char			**FileList = NULL;

	// ensure valid data
	assert( ResourceMgr != NULL );
	assert( ResourceName != NULL );
	assert( FileFilter != NULL );
	assert( FileListSize );

	// get vfile dir
	FileDir = grResource_GetVFile( ResourceMgr, ResourceName );
	if ( FileDir == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		return NULL;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		goto ERROR_Util_BuildFileList;
	}

	// determine how many files there are
	TotalFiles = 1;
	while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
	{
		TotalFiles++;
	}

	// destroy finder
	grVFile_DestroyFinder( Finder );
	Finder = NULL;

	// allocate file list
	FileList = (char **)grRam_AllocateClear( sizeof( char * ) * TotalFiles );
	if ( FileList == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		goto ERROR_Util_BuildFileList;
	}

	// create directory finder
	Finder = grVFile_CreateFinder( FileDir, FileFilter );
	if ( Finder == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
		goto ERROR_Util_BuildFileList;
	}

	// first entry is always the "no selection" slot
	CurFile = 0;
	FileList[CurFile++] = Util_StrDup( NoSelection );

	// build file list
	while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
	{

		// locals
		grVFile_Properties	Properties;

		// get properties of current file
		if( grVFile_FinderGetProperties( Finder, &Properties ) == GR_FALSE )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, NULL );
			continue;
		}

		// save file name
		assert( CurFile < TotalFiles );
		FileList[CurFile] = Util_StrDup( Properties.Name );
		if ( FileList[CurFile] != NULL )
		{
			CurFile++;
		}
	}

	// destroy finder
	grVFile_DestroyFinder( Finder );

	// close vfile dir
	if ( grResource_DeleteVFile( ResourceMgr, ResourceName ) == 0 )
	{
		grVFile_Close( FileDir );
	}

	// return file list
	*FileListSize = CurFile;
	return FileList;

	// error handling
	ERROR_Util_BuildFileList:

	// destroy file list
	if ( FileList != NULL )
	{
		CurFile = 0;
		while ( CurFile < TotalFiles )
		{
			if ( FileList[CurFile] != NULL )
			{
				grRam_Free( FileList[CurFile] );
			}
			CurFile++;
		}
		grRam_Free( FileList );
	}

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

	// zap file list size
	*FileListSize = 0;

	// return failure
	return NULL;

} // Util_BuildFileList()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Util_LoadLibraryString()
//
////////////////////////////////////////////////////////////////////////////////////////

#ifdef BUILD_BE

#include <Resources.h>
#include <image.h>

static char *Util_LoadLibraryString(image_id libhinst, int32 resid)
{
	BResources resourcefile;
	int result;
	char *rcbuffer;
 	image_info info;
	size_t outSize;
	
	assert(libhinst > 0);
	assert(resid);

///	hResources = (image_id)hStringResources ;
	
	if(get_image_info(libhinst,&info) != B_OK)
		return NULL;
		
	BFile* resFile = new BFile(info.name , B_READ_ONLY);
	
	resourcefile.SetTo(resFile,false);

	char* loadedString = (char *)resourcefile.FindResource('DATA', resid, &outSize);
	
	//
	//	Note that if we did't allocate space and copy the string, then we
	//	would be limited to having one string loaded at a time. Or we would
	//	setup some kind of revolving buffer.  Either of these options is
	//	risky and could eventually cause a problem elsewhere... 	 LF
	//
 
	// Allocate memory for the string
	rcbuffer = (char *)malloc(strlen(loadedString) + 1); //(char*)grRam_Allocate(strlen(loadedString) + 1);
	strcpy(rcbuffer, loadedString);
 
//#ifndef NDEBUG
//	memset(stringbuffer, 0xFF, UTIL_MAX_RESOURCE_LENGTH + 1);
//#endif
 
	// return the allocated string
	return (rcbuffer);
}//Util_LoadLibraryString

#endif

#ifdef WIN32

static char * Util_LoadLibraryString(
	HINSTANCE		hInstance,
	unsigned int	ID )
{

	// locals
	#define		MAX_STRING_SIZE	255
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
		grErrorLog_Add( GR_ERR_WINDOWS_API_FAILURE, NULL );
		return NULL;
	}

	// copy resource string
	NewString = (char *)grRam_Allocate( Size + 1 );
	if ( NewString == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return NULL;
	}
	strcpy( NewString, StringBuf );

	// all done
	return NewString;

} // Util_LoadLibraryString()

#endif
