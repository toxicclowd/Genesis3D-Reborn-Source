/****************************************************************************************/
/*  AMBOBJECT.C                                                                         */
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
#include <windows.h>

#include <string.h>
#include <float.h>
#include "AmbObject.h"
#include "grTypes.h"
#include "grProperty.h"
#include "grUserPoly.h"
#include "errorlog.h"
#include "Genesis3D.h"
#include "ram.h"
#include "memory.h"
#include "assert.h"
#include "dsound.h"
#include "resource.h"
#include "grResource.h"

#include "snd.h"

////////////////////// IMPORTANT
// If you change the structure formats, data ids, then bump the version number
//////////////////////

#define AMBOBJ_VERSION 1


#define PROP1_NAME "FileName:"
#define PROP2_NAME "Radius:"
//Royce-2
#define PROP3_NAME "Display"
//---

//	Tom
#define PROP4_NAME "Mute"

// BEGIN - Add loop checkbox to editor - paradoxnj
#define PROP5_NAME "Loop"
// END - Add loop checkbox to editor - paradoxnj

//Royce
#define OBJ_PERSIST_SIZE 5000
//---

enum 
{ 
	AMBOBJ_NAMELIST = PROPERTY_LOCAL_DATATYPE_START,
	AMBOBJ_SIZE,
	//Royce-2
	AMBOBJ_DISPLAYTOGGLE,
	//---
	//	Tom
	AMBOBJ_PROPERTY_MUTE_BOX,

	// BEGIN - Add loop checkbox to editor - paradoxnj
	AMBOBJ_PROPERTY_LOOP_BOX
	// END - Add loop checkbox to editor - paradoxnj
};

enum {
	AMB_NAMELIST_INDEX,
	AMB_SIZE_INDEX,
	//Royce-2
	AMB_DISPLAYTOGGLE_INDEX,
	//---
	AMB_INDEX_MUTE_BOX,
	// BEGIN - Add loop checkbox to editor - paradoxnj
	AMB_INDEX_LOOP_BOX,
	// END - Add loop checkbox to editor - paradoxnj

	AMB_LAST_INDEX
};

#define DEFAULT_RADIUS 1000.0f

static 	grBitmap	*pBitmap = NULL;
static grMaterialSpec *MatSpec;
typedef struct AmbObj {
	EffectResource  Resource;		// Resources: Camera, Engine, World, SoundSystem
	Snd				SndData;		// All info for playing the sound
	char			Name[256];
	int				RefCnt;
	grUserPoly		*Poly;
	grLVertex		Vertex;
	//Royce-2
	int				DisplayToggle;
	//---
	//	Tom
	grBoolean		bMute;
	// BEGIN - Add loop checkbox to editor - paradoxnj
	grBoolean		bLoop;
	// END - Add loop checkbox to editor - paradoxnj
} AmbObj;

grProperty AmbProperties[AMB_LAST_INDEX];
grProperty_List AmbPropertyList = { AMB_LAST_INDEX, &AmbProperties[0] };

#define MAX_NAMES 1024
char *NameList[MAX_NAMES];
int NameListCount = 0;

#define UTIL_MAX_RESOURCE_LENGTH	(128)
static char stringbuffer[UTIL_MAX_RESOURCE_LENGTH + 1];
static char	*NoSelection = "< none >";

//////////////////////////////////////////////////////////////////////////////
//
//  LOCAL UTILITY
//
//////////////////////////////////////////////////////////////////////////////

static grBoolean Util_StrDupManagePtr(char **dest, char *src, int min_size)
	{
	int len;

	assert(dest);
	assert(src);

	len = strlen(src)+1;

	if (*dest)
		{
		if ( len < min_size )
			{
			strcpy(*dest, src);
			return GR_TRUE;
			}

		grRam_Free(*dest);
		*dest = NULL;
		}

	*dest = grRam_Allocate(__max(min_size, len));
	if (*dest == NULL)
		{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, NULL );
		return GR_FALSE;
		}

	strcpy(*dest, src);
	return GR_TRUE;
	}

static int Util_GetAppPath(
	char	*Buf,		// where to store path name
	int		BufSize )	// size of buf
{

	// locals
	int	Count;

	// get exe full path name
	Count = GetModuleFileName( NULL, Buf, BufSize );
	if ( Count == 0 )
	{
		return 0;
	}

	// eliminate the exe from the path name
	while ( Count >= 0 )
	{
		if ( Buf[Count] == '\\' )
		{
			break;
		}
		Buf[Count] = '\0';
		Count--;
	}

	// all done
	return Count;

} 

//////////////////////////////////////////////////////////////////////////////
//
//  LOCAL BITMAP RELATED
//
//////////////////////////////////////////////////////////////////////////////

static grBoolean AmbObject_LoadBmp()
{
	// Jeff:  Ambient bitmap from resources - 8/18/2005
	
	grVFile	* BmpFile;
	HRSRC hFRes; 
    HGLOBAL hRes; 
    grVFile_MemoryContext Context; 
    HINSTANCE hInst;
    
	#ifdef _DEBUG 
	    hInst = LoadLibrary("AmbientObj.ddl");
	#else
        hInst = LoadLibrary("AmbientObj.dll");
    #endif
    hFRes = FindResource(hInst, MAKEINTRESOURCE(IDR_AMBIENT) ,"grBitmap"); 
    hRes = LoadResource(hInst, hFRes) ;  
    
    Context.Data  = LockResource(hRes); 
    Context.DataLength = SizeofResource(hInst,hFRes); 

	BmpFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_MEMORY,	NULL,
		                            &Context,GR_VFILE_OPEN_READONLY  );
	if( BmpFile == NULL )
		return( GR_FALSE );
	pBitmap = grBitmap_CreateFromFile( BmpFile );
	grVFile_Close( BmpFile );
	if( pBitmap == NULL )
		return( GR_FALSE );
	grBitmap_SetColorKey( pBitmap, GR_TRUE, 255, GR_TRUE );

	
	return( GR_TRUE );
	
}

static grBoolean AmbObject_InitIcon( AmbObj * pAmbObj )
{
	assert(pAmbObj != NULL);

	//Royce-2
	assert(pBitmap);
	
	if( pBitmap == NULL )
		if( !AmbObject_LoadBmp() )
			return( GR_FALSE );
			
	//---

	pAmbObj->Vertex.r = 255.0f;
	pAmbObj->Vertex.g = 255.0f;
	pAmbObj->Vertex.b = 255.0f;
	pAmbObj->Vertex.a = 255.0f;
	pAmbObj->Vertex.u = 0.0f;
	pAmbObj->Vertex.v = 0.0f;
	pAmbObj->Vertex.sr = 255.0f;
	pAmbObj->Vertex.sg = 255.0f;
	pAmbObj->Vertex.sb = 255.0f;

	pAmbObj->Vertex.X = 0.0f;
	pAmbObj->Vertex.Y = 0.0f;
	pAmbObj->Vertex.Z = 0.0f;

	if (!MatSpec)
	{
	    MatSpec = grMaterialSpec_Create(grResourceMgr_GetEngine(grResourceMgr_GetSingleton()), grResourceMgr_GetSingleton());
#pragma message ("Krouer: change NULL to something better next time")
	    grMaterialSpec_AddLayerFromBitmap(MatSpec, 0, pBitmap, NULL);
	}
    
	pAmbObj->Poly = grUserPoly_CreateSprite(	&pAmbObj->Vertex,
									MatSpec,
									1.0f,
									GR_RENDER_FLAG_ALPHA | GR_RENDER_FLAG_NO_ZWRITE );

	return GR_TRUE;
}				

static grBoolean AmbObject_UpdateIcon( AmbObj * pAmbObj )
{
	assert(pAmbObj != NULL);

	pAmbObj->Vertex.X = pAmbObj->SndData.Pos.X;
	pAmbObj->Vertex.Y = pAmbObj->SndData.Pos.Y;
	pAmbObj->Vertex.Z = pAmbObj->SndData.Pos.Z;

	//Royce-2
	if (pAmbObj->DisplayToggle)
	{
		if (!MatSpec)
	    {
	        MatSpec = grMaterialSpec_Create(grResourceMgr_GetEngine(grResourceMgr_GetSingleton()), grResourceMgr_GetSingleton());
#pragma message ("Krouer: change NULL to something better next time")
	        grMaterialSpec_AddLayerFromBitmap(MatSpec, 0, pBitmap, NULL);
	    }
		grUserPoly_UpdateSprite(pAmbObj->Poly, &pAmbObj->Vertex, MatSpec, 1.0f);
	}
	//---

	return GR_TRUE;
}				

//////////////////////////////////////////////////////////////////////////////
//
//  LOCAL SOUND RELATED
//
//////////////////////////////////////////////////////////////////////////////

static grBoolean AmbObject_LoadSound(AmbObj * pAmbObj, char *Name)
{
	grResourceMgr *ResourceMgr;
	grVFile *SoundDir,*SndFile = NULL;
	grSound_Def *NewSoundDef;

	if (!pAmbObj->Resource.Sound || !pAmbObj->Resource.World)
		return GR_TRUE;

	//Royce-2
	if (!Name || !Name[0] || !strcmp(NoSelection, Name))
	//---
	{
		return GR_TRUE;
	}

	// clear any old sound
	if (pAmbObj->SndData.SoundDef != NULL)
	{
		Snd_Remove(&pAmbObj->Resource, &pAmbObj->SndData);
		grSound_FreeSoundDef(pAmbObj->Resource.Sound, pAmbObj->SndData.SoundDef);
		pAmbObj->SndData.SoundDef = NULL;
	}

	assert(pAmbObj->Resource.World);
	ResourceMgr = grWorld_GetResourceMgr(pAmbObj->Resource.World);

	if (ResourceMgr == NULL)
		{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"AmbObject_LoadSound: grWorld_GetResourceMgr() failed", Name);
		return GR_FALSE;
		}

	SoundDir = grResource_GetVFile(ResourceMgr, "Sounds");

	//SndFile = grVFile_OpenNewSystem( SoundDir, GR_VFILE_TYPE_DOS, Name, NULL, GR_VFILE_OPEN_READONLY );
	SndFile = grVFile_Open( SoundDir, Name, GR_VFILE_OPEN_READONLY);

	if (SndFile == NULL)
	{
		grErrorLog_AddString(GR_ERR_FILEIO_OPEN,"AmbObject_LoadSound: grVFile_Open() failed", Name);
		goto LOAD_CLEAN;
	}

	// create the new sound def
	NewSoundDef = grSound_LoadSoundDef( pAmbObj->Resource.Sound, SndFile );
	if (NewSoundDef == NULL)
	{
		grErrorLog_AddString(GR_ERR_FILEIO_READ,"AmbObject_LoadSound: grSound_LoadSoundDef() failed", Name);
		goto LOAD_CLEAN;
	}

	pAmbObj->SndData.SoundDef = NewSoundDef;
	
	grVFile_Close( SndFile );

	// [MLB-ICE]
	grResource_MgrDestroy(&ResourceMgr);	// Icestorm: We should clear this Instance up, it was referenced!
	// [MLB-ICE] EOB

	return( GR_TRUE );

LOAD_CLEAN:

	if (SndFile != NULL)
		grVFile_Close( SndFile );

	strcpy(pAmbObj->Name, NoSelection);

	// [MLB-ICE]
	grResource_MgrDestroy(&ResourceMgr);	// Icestorm: We should clear this Instance up, it was referenced!
	// [MLB-ICE] EOB

	return GR_TRUE;
}

static grBoolean AmbObj_ReadSoundNames(grVFile *FileBase, int *FileCount)
	{
	grVFile_Finder * Finder;

	assert(FileBase);

	Finder = grVFile_CreateFinder(FileBase,"*.wav");
	if ( ! Finder )
		{
		grVFile_Close(FileBase);
		return GR_FALSE;
		}

	while( grVFile_FinderGetNextFile(Finder) )
		{
		grVFile_Properties Properties;
		grVFile_FinderGetProperties(Finder,&Properties);

		strlwr(Properties.Name);

		if (Util_StrDupManagePtr(&NameList[(*FileCount)++], Properties.Name, 32) == GR_FALSE)
			{
			grVFile_DestroyFinder(Finder);
			return GR_FALSE;
			}
		}

	grVFile_DestroyFinder(Finder);

	return GR_TRUE;
	}

static grBoolean GRCC AmbObj_GetSoundNames( void * Instance, grWorld * pWorld )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;
	grResourceMgr *ResourceMgr;
	grVFile *SoundDir;

	assert( Instance );
	assert( pWorld );

	Util_StrDupManagePtr(&NameList[0], NoSelection, 32);
	NameListCount = 1;

	ResourceMgr = grWorld_GetResourceMgr(pWorld);

	if (ResourceMgr == NULL)
		return GR_FALSE;

	SoundDir = grResource_GetVFile(ResourceMgr, "Sounds");

	if (!SoundDir)
		return GR_FALSE;

	if (!AmbObj_ReadSoundNames(SoundDir, &NameListCount))
		return GR_FALSE;

	// [MLB-ICE]
	grResource_MgrDestroy(&ResourceMgr);	// Icestorm: We should clear this Instance up, it was referenced!
	// [MLB-ICE] EOB

	return( GR_TRUE );
}

//////////////////////////////////////////////////////////////////////////////
//
//  PROCESS ATTACH/DETACH
//
//////////////////////////////////////////////////////////////////////////////

void Init_Class( HINSTANCE hInstance )
{
	//Royce-2
	AmbObject_LoadBmp(); //failure is not fatal, don't bother checking
	//pBitmap is now our flag to tell us whether "ambient.bmp" is in the
	//host exe's dir. If it is new instances will default DisplayToggle ON
	//(i.e. we assume we're in the editor instead of some game)
	//---
}

void Destroy_Class( void )
{
	int i;

	for (i = 0; i < MAX_NAMES; i++)
		{
		if (NameList[i])
			{
			grRam_Free(NameList[i]);
			NameList[i] = NULL;
			}
		}

	NameListCount = 0;
	//Royce-2
	grBitmap_Destroy(&pBitmap);
	//---
}

//////////////////////////////////////////////////////////////////////////////
//
//  DLL INTERFACE
//
//////////////////////////////////////////////////////////////////////////////

void * GRCC CreateInstance( void )
{
	AmbObj *pAmbObj;

	pAmbObj = GR_RAM_ALLOCATE_STRUCT( AmbObj );
	if( pAmbObj == NULL )
		return( NULL );
	memset(pAmbObj, 0, sizeof(*pAmbObj));
	pAmbObj->SndData.Min = DEFAULT_RADIUS;
	//	tom morris feb 2005 -- changed default loop to TRUE
	//	set default mute to FALSE;
	pAmbObj->bLoop= /*GR_FALSE*/GR_TRUE; // 
	pAmbObj->SndData.Loop = GR_TRUE;
	pAmbObj->bMute = GR_FALSE;
	//	end tom morris feb 2005
	//Royce-2
	pAmbObj->DisplayToggle = pBitmap ? GR_TRUE : GR_FALSE;
	//---
	strcpy(pAmbObj->Name, NoSelection);
	pAmbObj->RefCnt = 1;
	//Royce-2

	return( pAmbObj );
	/*
	if( !AmbObject_InitIcon( pAmbObj ) )
		goto CI_ERROR;

	

CI_ERROR:
	grRam_Free( pAmbObj );
	return( NULL );
	*/
	//---
}


void GRCC CreateRef(void * Instance)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );

	pAmbObj->RefCnt++;
}

grBoolean GRCC Destroy(void **pInstance)
{
	AmbObj **hAmbObj = (AmbObj**)pInstance;
	AmbObj *pAmbObj = *hAmbObj;

	assert( pInstance );
	assert( pAmbObj->RefCnt > 0 );

	pAmbObj->RefCnt--;
	if( pAmbObj->RefCnt == 0 )
	{
		if (pAmbObj->SndData.SoundDef != NULL)
		{
			if (pAmbObj->SndData.Sound)
			{
			//	tom morris June 2005
			int iResult = 0;
			iResult = grSound_GetStatus(pAmbObj->Resource.Sound, pAmbObj->SndData.Sound);
			if (iResult & DSBSTATUS_PLAYING)
			//	commented out by tom 
			//	if (grSound_SoundIsPlaying(pAmbObj->Resource.Sound, pAmbObj->SndData.Sound))
			//
			grSound_StopSound(pAmbObj->Resource.Sound, pAmbObj->SndData.Sound);

				pAmbObj->SndData.Sound = NULL;
			}

			Snd_Remove(&pAmbObj->Resource, &pAmbObj->SndData);
			grSound_FreeSoundDef(pAmbObj->Resource.Sound, pAmbObj->SndData.SoundDef);
			pAmbObj->SndData.SoundDef = NULL;
		}

		if (pAmbObj->Poly)
			{
			grUserPoly_Destroy(&pAmbObj->Poly);
			}

		grRam_Free( pAmbObj );
	}

	return GR_TRUE;
}


grBoolean GRCC Render(const void * Instance, const grWorld * pWorld, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );
	assert( pWorld );
	assert( Engine );
	assert( Camera );

	if( grWorld_GetRenderRecursion( pWorld ) > 1 )
		return GR_TRUE;

	if (pAmbObj->Resource.Sound != NULL && pAmbObj->SndData.SoundDef != NULL)
	{
		pAmbObj->SndData.Loop = pAmbObj->bLoop;

		pAmbObj->Resource.Camera = (grCamera *)Camera;
		Snd_Process( &pAmbObj->Resource, 0.0f, pAmbObj->bMute, &pAmbObj->SndData );
	}
	
	return( GR_TRUE );
}

grBoolean GRCC AttachWorld( void * Instance, grWorld * pWorld )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );
	assert( pWorld );

	pAmbObj->Resource.World = pWorld;

	if (!AmbObj_GetSoundNames(Instance, pAmbObj->Resource.World))
		return GR_FALSE;

	grProperty_FillCombo( &AmbPropertyList.pgrProperty[AMB_NAMELIST_INDEX], 
		PROP1_NAME, pAmbObj->Name, AMBOBJ_NAMELIST, NameListCount, NameList );

	if (pAmbObj->Name[0] && !pAmbObj->SndData.SoundDef)
		{
		AmbObject_LoadSound(pAmbObj, pAmbObj->Name);
		}

	//Royce-2
	
	// add the pAmbObj->Poly to the world
	if (pAmbObj->DisplayToggle && !pAmbObj->Poly ) {
		if (AmbObject_InitIcon(pAmbObj)) {
			if ( grWorld_AddUserPoly( pWorld, pAmbObj->Poly, GR_FALSE ) == GR_FALSE )
			{
				grUserPoly_Destroy( &( pAmbObj->Poly ) );
				return GR_FALSE;
			}
		}
		else return GR_FALSE;
		AmbObject_UpdateIcon(pAmbObj);
	}
	//---
	


	return( GR_TRUE );
}

grBoolean	GRCC DettachWorld( void * Instance, grWorld * pWorld )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );

	//Royce-2
	if (pAmbObj->Poly) {
		if (pAmbObj->DisplayToggle) 
			if ( grWorld_RemoveUserPoly( pWorld, pAmbObj->Poly) == GR_FALSE ) 
				return GR_FALSE;
		
		
		grUserPoly_Destroy(&(pAmbObj->Poly));
		//---
	}

	// clear any old sound
	if (pAmbObj->SndData.SoundDef != NULL)
	{
		Snd_Remove(&pAmbObj->Resource, &pAmbObj->SndData);
	}

	pAmbObj->Resource.World = NULL;

	return( GR_TRUE );
}
				
grBoolean	GRCC AttachEngine ( void * Instance, grEngine *Engine )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;


	assert( Instance );
	assert( Engine );

	//Royce-2
	if( pBitmap )
		return( grEngine_AddBitmap( (grEngine*)Engine, pBitmap, GR_ENGINE_BITMAP_TYPE_3D ) );	
	//---
	return GR_TRUE;
	Instance;
}

grBoolean	GRCC DettachEngine( void * Instance, grEngine *Engine )
{
	assert( Instance );

	//Royce-2
	
	if( pBitmap )
		grEngine_RemoveBitmap(	Engine, pBitmap );
	//---
	return( GR_TRUE );
	Instance;
}

grBoolean	GRCC AttachSoundSystem( void * Instance, grSound_System *SoundSystem )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;


	assert( Instance );

	pAmbObj->Resource.Sound = SoundSystem;

	if (pAmbObj->Name[0] && !pAmbObj->SndData.SoundDef)
		{
		AmbObject_LoadSound(pAmbObj, pAmbObj->Name);
		}

	return( GR_TRUE );
}

grBoolean	GRCC DettachSoundSystem( void * Instance, grSound_System *SoundSystem )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );

	pAmbObj->Resource.Sound = NULL;

	return( GR_TRUE );
	SoundSystem;
}

grBoolean	GRCC Collision(const grObject *Object, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{
	return( GR_FALSE );
}

grBoolean GRCC SetMaterial(void * Instance,const grBitmap *Bmp,const grRGBA * Color)
{
	return( GR_TRUE );
}

grBoolean GRCC GetMaterial(const void * Instance,grBitmap **pBmp,grRGBA * Color)
{
	return( GR_TRUE );
}

grBoolean GRCC GetExtBox(const void * Instance,grExtBox *BBox)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;
	grVec3d Point;

	assert( Instance );
	assert( BBox );

	Point = pAmbObj->SndData.Pos;

	grExtBox_Set (  BBox, 
					Point.X-5.0f, Point.Y-5.0f, Point.Z-5.0f,
					Point.X+5.0f, Point.Y+5.0f, Point.Z+5.0f);

	return( GR_TRUE );
}


void *	GRCC CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	AmbObj * pAmbObj;
	BYTE Version;
	uint32 Tag;

	pAmbObj = GR_RAM_ALLOCATE_STRUCT( AmbObj );
	memset(pAmbObj, 0, sizeof(*pAmbObj));
	
	if( pAmbObj == NULL )
		return( NULL );

 	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:Tag" );
		goto CFF_ERROR;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:Version" );
	       	goto CFF_ERROR;
		}
	}
	else
	{
		//for backwards compatibility with old object format
		Version = 1;
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
	}
	
	if (Version >= 1)
	{
	
	    if( !grVFile_Read(	File, pAmbObj->Name, sizeof( pAmbObj->Name) ) )
		{
    	    grErrorLog_Add(GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:Name");
		    goto CFF_ERROR;
		}

	    if( !grVFile_Read(	File, &pAmbObj->SndData.Pos, sizeof( pAmbObj->SndData.Pos) ) )
		{
    	    grErrorLog_Add(GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:SndData.Pos");
		    goto CFF_ERROR;
		}
	
	    if( !grVFile_Read(	File, &pAmbObj->SndData.Min, sizeof( pAmbObj->SndData.Min) ) )
		{
    	    grErrorLog_Add(GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:SndData.Min");
		    goto CFF_ERROR;
		}

	    if( !grVFile_Read(	File, &pAmbObj->SndData.Loop, sizeof( pAmbObj->SndData.Loop) ) )
		{
    	    grErrorLog_Add(GR_ERR_FILEIO_READ, "AmbObject_CreateFromFile:SndData.Loop");
		    goto CFF_ERROR;
		}
	}
	

	//Royce-2
	//this property is detected based on the existance of ambient.bmp
	pAmbObj->DisplayToggle = pBitmap ? GR_TRUE : GR_FALSE; //not a fatal error
	//---
	pAmbObj->RefCnt = 1;

	//	tom morris	feb 2005 -- setting loop checkbox to match saved state
	pAmbObj->bLoop = pAmbObj->SndData.Loop;
	//	end tom morris feb 2005

	AmbObject_LoadSound(pAmbObj, pAmbObj->Name);

	return( pAmbObj );

CFF_ERROR:

	grRam_Free( pAmbObj );
	return( NULL );
	PtrMgr;
}



grBoolean	GRCC WriteToFile(const void * Instance,grVFile * File, grPtrMgr *PtrMgr)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;
	BYTE Version = AMBOBJ_VERSION;
	uint32 Tag = FILE_UNIQUE_ID;

	assert( Instance );


	if( !grVFile_Write(	File, &Tag, sizeof(Tag)))
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:Tag");
	    return( GR_FALSE );
	}
	
	if( !grVFile_Write(	File, &Version, sizeof(Version) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:Version");
	    return( GR_FALSE );
	}

	if( !grVFile_Write(	File, pAmbObj->Name, sizeof( pAmbObj->Name) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:Name");
	    return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pAmbObj->SndData.Pos, sizeof( pAmbObj->SndData.Pos) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:SndData.Pos");
		return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pAmbObj->SndData.Min, sizeof( pAmbObj->SndData.Min) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:SndData.Min");
		return( GR_FALSE );
	}

	if( !grVFile_Write(	File, &pAmbObj->SndData.Loop, sizeof( pAmbObj->SndData.Loop) ) )
	{
    	grErrorLog_Add(GR_ERR_FILEIO_WRITE, "AmbObject_WriteToFile:SndData.Loop");
		return( GR_FALSE );
	}

	return( GR_TRUE );
	PtrMgr;
}


grBoolean	GRCC GetPropertyList(void * Instance, grProperty_List **List)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );

	//assert(pAmbObj->Resource.World);
	if (pAmbObj->Resource.World)
		{
		if (!AmbObj_GetSoundNames(Instance, pAmbObj->Resource.World))
			return GR_FALSE;

		grProperty_FillCombo( &AmbPropertyList.pgrProperty[AMB_NAMELIST_INDEX], 
			PROP1_NAME, pAmbObj->Name, AMBOBJ_NAMELIST, NameListCount, NameList );
		}
	else
		{
		grProperty_FillCombo( &AmbPropertyList.pgrProperty[AMB_NAMELIST_INDEX], 
			PROP1_NAME, NoSelection, AMBOBJ_NAMELIST, 1, &NoSelection );
		}


	grProperty_FillFloat( &AmbPropertyList.pgrProperty[AMB_SIZE_INDEX], 
		PROP2_NAME, pAmbObj->SndData.Min, AMBOBJ_SIZE, 0, FLT_MAX, 10.0f );

	//Royce-2
	grProperty_FillCheck( &AmbPropertyList.pgrProperty[AMB_DISPLAYTOGGLE_INDEX],
		PROP3_NAME, pAmbObj->DisplayToggle, AMBOBJ_DISPLAYTOGGLE);
	//---

	//	Tom
	grProperty_FillCheck( &AmbPropertyList.pgrProperty[AMB_INDEX_MUTE_BOX],
		PROP4_NAME, pAmbObj->bMute, AMBOBJ_PROPERTY_MUTE_BOX);

	// BEGIN - Add loop checkbox to editor - paradoxnj
	grProperty_FillCheck( &AmbPropertyList.pgrProperty[AMB_INDEX_LOOP_BOX],
		PROP5_NAME, pAmbObj->bLoop, AMBOBJ_PROPERTY_LOOP_BOX);

	*List = grProperty_ListCopy( &AmbPropertyList);
	AmbPropertyList.bDirty = GR_FALSE;

	if( *List == NULL )
		return( GR_FALSE );

	return( GR_TRUE );
}


grBoolean	GRCC SetProperty( void * Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );
	assert( pData );

	//Royce-2
	
	switch (FieldID) 
	{ 
	case AMBOBJ_SIZE :
	
		if (pData->Float < 0.0f)
			pData->Float = 0.0f;

		pAmbObj->SndData.Min = pData->Float;
		break;
	case AMBOBJ_NAMELIST:
		
		if (strcmp(pData->String, NoSelection) == 0)
			return GR_TRUE;

		strcpy(pAmbObj->Name, pData->String);
		AmbObject_LoadSound(pAmbObj, pData->String);
		AmbPropertyList.bDirty = GR_TRUE;
		break;

	//	Tom
	case AMBOBJ_PROPERTY_MUTE_BOX:
	{
		pAmbObj->bMute = (grBoolean)pData->Bool;
		break;
	}

	case AMBOBJ_PROPERTY_LOOP_BOX:
		{
			pAmbObj->bLoop = (grBoolean)pData->Bool;
			break;
		}
	case AMBOBJ_DISPLAYTOGGLE:
		pAmbObj->DisplayToggle = pData->Bool;
		if (pAmbObj->DisplayToggle ) 
		{
			if ( pAmbObj->Resource.World) 
			{
				if (pBitmap) {
					if (!pAmbObj->Poly) 
						AmbObject_InitIcon(pAmbObj);

					//turn on the sprite
					if ( grWorld_AddUserPoly( pAmbObj->Resource.World, pAmbObj->Poly, GR_FALSE ) == GR_FALSE ) {
						//if this busts we should probably hear about it
						//but it is not fatal
						grUserPoly_Destroy( &( pAmbObj->Poly ) );
						grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to add the Ambient UserPoly to the World.", NULL);
						//note that the bitmap may still be outstanding
					}
					AmbObject_UpdateIcon(pAmbObj);
				}
			}
			
		}
		else {
			//turn off the sprite
			if (pAmbObj->Poly) {
				if ( grWorld_RemoveUserPoly( pAmbObj->Resource.World, pAmbObj->Poly) == GR_FALSE ) {
					//something bad has probably gone wrong here
					//but I still don't think it should be fatal
					grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to remove the Ambient UserPoly from the World.", NULL);
					break; //don't orphan the poly 
				}
			}
			
			
		}
		break;
	}
	//---

	


	return( GR_TRUE );
}

grBoolean	GRCC SetXForm(void * Instance,const grXForm3d *XF)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );
	assert( XF );

	pAmbObj->SndData.Pos = XF->Translation;
	AmbObject_UpdateIcon(pAmbObj);

	return( GR_TRUE );
}

grBoolean GRCC GetXForm(const void * Instance,grXForm3d *XF)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );
	assert( XF );

	grXForm3d_SetIdentity(XF);
	XF->Translation = pAmbObj->SndData.Pos;
	return( GR_TRUE );
}

int	GRCC GetXFormModFlags( const void * Instance )
{
	Instance;
	return( GR_OBJECT_XFORM_TRANSLATE);
}

grBoolean GRCC GetChildren(const void * Instance,grObject * Children,int MaxNumChildren)
{
	return( GR_TRUE );
}

grBoolean GRCC AddChild(void * Instance,const grObject * Child)
{
	return( GR_TRUE );
}

grBoolean GRCC RemoveChild(void * Instance,const grObject * Child)
{
	return( GR_TRUE );
}

grBoolean GRCC EditDialog (void * Instance,HWND Parent)
{
	return( GR_TRUE );
}


grBoolean GRCC MessageFunction (void * Instance, int32 Msg, void * Data)
{
	AmbObj *pAmbObj = (AmbObj*)Instance;

	assert( Instance );

	switch (Msg)
		{
		default:
			return GR_FALSE;
			break;
		}// switch

	return( GR_TRUE );
}


grBoolean	GRCC UpdateTimeDelta(void * Instance, float TimeDelta )
{
	// locals
	//AmbObj	*pAmbObj;

	// ensure valid data
	assert( Instance != NULL );

	if ( TimeDelta == 0.0f )
	{
		return GR_TRUE;
	}

	return GR_TRUE;
}


//Royce
////////////////////////////////////////////////////////////////////////////////////////
//
//	DuplicateInstance()
//
///////////////////////////////////////////////////////////////////////////////////////
void * GRCC DuplicateInstance(void * Instance)
{
	grVFile *ramdisk, *ramfile;
	grVFile_MemoryContext vfsmemctx;
	grObject* newAmbObj = NULL;
	grPtrMgr *ptrMgr = NULL;


	vfsmemctx.Data = grRam_Allocate(OBJ_PERSIST_SIZE); //"I dunno, 100K sounds good."
	vfsmemctx.DataLength = OBJ_PERSIST_SIZE;

	if (!vfsmemctx.Data) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to allocate enough RAM to duplicate this object", NULL);
		return NULL;
	}

	ramdisk = grVFile_OpenNewSystem
	(
		NULL, 
		GR_VFILE_TYPE_MEMORY|GR_VFILE_TYPE_VIRTUAL,
		"Memory",
		NULL,
		GR_VFILE_OPEN_CREATE|GR_VFILE_OPEN_DIRECTORY
	);

	if (!ramdisk) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a VFile Memory Directory", NULL);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	ramfile = grVFile_Open(ramdisk, "tempObject", GR_VFILE_OPEN_CREATE);

	if (!ramfile) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a VFile Memory File", NULL);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}
	ptrMgr = grPtrMgr_Create();

	if (!ptrMgr) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to create a Pointer Manager", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	if (!WriteToFile(Instance, ramfile, grPtrMgr_Create())) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to write the object to a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	if (!grVFile_Rewind(ramfile)) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to rewind the temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	newAmbObj = CreateFromFile(ramfile, ptrMgr);
	if (!newAmbObj) {
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "Unable to reade the object back from a temp VFile Memory File", NULL);
		grVFile_Close(ramfile);
		grVFile_Close(ramdisk);
		grRam_Free(vfsmemctx.Data);
		return NULL;
	}

	grVFile_Close(ramfile);
	grVFile_Close(ramdisk);

	grRam_Free(vfsmemctx.Data);

	return( newAmbObj );
}
//---

// Icestorm
grBoolean	GRCC ChangeBoxCollision(const void *Instance,const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	return( GR_FALSE );
}