/****************************************************************************************/
/*  TERRAIN.C                                                                           */
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
/**********---------------------

(later) todos :

	1. good file IO
	2. vis
		with brushes for in-terrain vis !

@@ for urgent
<> for todos
{} for notes/peformance concerns

---------------------

todo :

EDITOR STUFF :

<> grLight UI in editor

<> set heightmap from materials in editor

<> vec3d Size in property list isn't editable !?
	using separate floats is working

<> need a deselect message 

@@ take materials instead of bitmaps for textures;
	write out materials via the NameMgr
	support the UV mapper or whatever is in the material

CODE STUFF :

<> my dynamic lights are different than the world variety; must think
	-> do lighting in light maps?
	the original reasons why not to: 
		1. fill rate; hurts huge on voodoo 1's and whatnot; not an issue in the future
		2. memory usage; we can easily have 16 256x256 textures (2 megs) of terrain textures
			if we have per-pixel lightmaps, that doubles
		3. textures mip, lightmaps don't ; this is a big issue
	if we have mipping grBitmap multi-texturing, then we could do one lightmap pel per 4 texels (or so)

<> make a grWorld_CalculateLighting function

@@ take a two-sided flag

<> Collision is vs. "thick rays" (extruded spheres) not extruded extboxes right now
	should be easy to fix up right

<> file IO options :
	1. save lit textures
	2. save quadtree

<> Icestorm: Port the BoxCollision to ChangeBoxCollision and set it in the ObjectDef
				(currently set to NULL!!) Don't forget: Plane&Impact CAN be NULL!

---------------------***********/

////////////////////////////////////////////////////////////////////////////////
// 
////////// Jet3D Note: Important !!  ///////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
// CJP(chrisjp@eudoramail.com) : 2.18.00
//
// The loading / saving code has been modified to save the heightmap and bitmaps directly to the file, not through the embedded VFS
// This change will break all levels which currently contain one or more terrain objects, but 
//		will allow multiple terrain objects to be saved
// To allow loading of old terrain objects, #define CJP_LOAD_OLDTERRAINOBJECTS
// To allow saving of old terrain objects ( can't think of a reason why..) #define CJP_SAVE_OLDTERRAINOBJECTS
//
// #define CJP_LOAD_OLDTERRAINOBJECTS 1
// #define CJP_SAVE_OLDTERRAINOBJECTS 1

// End of note.

#include "Object.h"
#include "Terrain.h"
#include "Terrain._h"
#include "Engine.h" 
#include "grFrustum.h" 
#include "Ram.h"
#include "grProperty.h"
#include "Util.h"
#include "grChain.h"
#include "grWorld.h"
#include "Errorlog.h"
#include "Camera._h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h> // for sprintf

#ifndef max
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#ifdef BUILD_BE
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

/*}{******************************************************/
//	Protos & Macros

#define allocate(ptr)	ptr = grRam_AllocateClear(sizeof(*ptr))
#define clear(ptr)		memset(ptr,0,sizeof(*ptr))
#define destroy(ptr)	if ( ptr ) { grRam_Free(ptr); (ptr) = NULL; } else
#define max3(a,b,c) max(max(a,b),c)
#define min3(a,b,c) min(min(a,b),c)

static grBoolean grTerrain_IsValid(const grTerrain *Terrain);
static grBoolean GRCC grTerrain_AttachEngine(void *T,grEngine *Engine);
static grBoolean GRCC grTerrain_DetachEngine(void *T,grEngine *Engine);
static grBoolean grTerrain_RefreshQT(grTerrain * T);

static void grTerrain_DeSelect(grTerrain * T);
static void grTerrain_Select(grTerrain * T,grVec3d *pWorldVec);

/*}{******************************************************/
//	Creators, Destroys, File IO

static void FillPalBmp(grBitmap *Bmp,int r,int g,int b)
{
grBitmap * Lock;
grBitmap_Palette * Pal;
int y,s,w,h;
grBitmap_Info Info;
uint8 * bptr;

	Pal = grBitmap_Palette_Create(GR_PIXELFORMAT_24BIT_RGB,256);
	assert(Pal);
	grBitmap_Palette_SetEntryColor(Pal,0,r,g,b,255);
	grBitmap_SetPalette(Bmp,Pal);
	grBitmap_Palette_Destroy(&Pal);

	grBitmap_LockForWriteFormat(Bmp,&Lock,0,0,GR_PIXELFORMAT_8BIT_PAL);

	grBitmap_GetInfo(Lock,&Info,NULL);

	s = Info.Stride;
	w = Info.Width;
	h = Info.Height;
	
	bptr = (uint8 *)grBitmap_GetBits(Lock);
	assert(bptr);

	for(y=0;y<h;y++)
	{
		memset(bptr,0,w);
		bptr += s;
	}

	grBitmap_UnLock(Lock);
}

GRAPI void * GRCC grTerrain_Create(void)
{
grTerrain * T = NULL;
grBoolean ret;

	T = (grTerrain *)grRam_AllocateClear(sizeof(grTerrain));
	if ( ! T )
		return NULL;

	T->Untouchable = GR_TRUE;

	T->SelfCheck = T;

	T->RefCount = 1;

	// Krouer: enable rendering
	T->RenderNextFlag = GR_TRUE;

	T->MaxQuads = 500;
	T->MinError = 0.015f;

	grXForm3d_SetIdentity(&(T->XFTerrainToWorld));
	T->XFWorldToTerrain = T->XFTerrainToWorld;
		
	T->LastTesselatedCameraPos.X = 9999999999.9f;
	T->Changed = GR_FALSE;

	T->Size.X = T->Size.Y = T->Size.Z = 100.0f;

	T->NullTexture = grBitmap_Create(8,8,1,GR_PIXELFORMAT_8BIT_PAL);
	FillPalBmp(T->NullTexture,0,255,0);

	T->HiliteTexture = grBitmap_Create(8,8,1,GR_PIXELFORMAT_8BIT_PAL);
	FillPalBmp(T->HiliteTexture,0,0,255);

	T->TexDim = 1;
	ret = grTerrain_SetATexture(T,T->NullTexture,0,0);
	assert(ret);

	{
	grBitmap * Bmp = NULL;

		Bmp = grBitmap_Create(8,8,1,GR_PIXELFORMAT_8BIT_PAL);
		if (Bmp)
		{
			FillPalBmp(Bmp,0,0,0);

			ret = grTerrain_SetHeightmap(T,Bmp);
			assert(ret);

			grBitmap_Destroy(&Bmp);
		}

		strcpy(T->HeightmapName,"null");
	}

	T->Untouchable = GR_FALSE;

return T;
}

GRAPI grBoolean GRCC  grTerrain_SetSize(grTerrain * T,grVec3d * pSize)
{
	assert( grTerrain_IsValid(T) );
	assert( pSize );

	grTerrain_DeSelect(T);

	if ( T->HM )
	{
	grVec3d Scale;
		
		T->Untouchable = GR_TRUE;

		Scale.X = pSize->X / T->Size.X;
		Scale.Y = pSize->Y / T->Size.Y;
		Scale.Z = pSize->Z / T->Size.Z;

		if ( Scale.Z != 1.0f )
		{
		float * HMptr;
		int cnt;
			cnt = T->HMWidth * T->HMHeight;
			HMptr = T->HM;
			while(cnt--)
			{
				*HMptr++ *= Scale.Z;
			}
		}

		T->Size = *pSize;

		T->InvCubeSize.X = (float)(T->HMWidth  - 1) / T->Size.X;
		T->InvCubeSize.Y = (float)(T->HMHeight - 1) / T->Size.Y;
		T->InvCubeSize.Z = 255.0f   / T->Size.Z;

		T->CubeSize.X = 1.0f / T->InvCubeSize.X;
		T->CubeSize.Y = 1.0f / T->InvCubeSize.Y;
		T->CubeSize.Z = 1.0f / T->InvCubeSize.Z;

		if ( T->QT )
		{
			/* <> Madre de dios, this is slow, but it's actually quite tricky to scale,
			*	because you have things like Sin2Normal and ErrIsotropic and whatnot
			*	which are affected in some funny way by the scaling of coordinate systems
			*
			*	<> notez : we could do an isotropic scaling much more easily
			**/
			QuadTree_Destroy(&(T->QT));
			T->QT = QuadTree_Create(T);

			if ( ! T->QT )
				return GR_FALSE;

			if ( ! grTerrain_RefreshQT(T) )
				return GR_FALSE;
		}
		
		T->Untouchable = GR_FALSE;
	}
	else
	{
		T->Size = *pSize;
	}

return GR_TRUE;
}

GRAPI grBoolean	GRCC grTerrain_GetHeightmap(grTerrain * T,grBitmap ** pBmp)
{
	assert( grTerrain_IsValid(T) );
	*pBmp = T->Heightmap;
return GR_TRUE;
}

#define ispow2(X) ( ( (X) & ~(-(X)) ) == 0 )

GRAPI grBoolean GRCC  grTerrain_SetHeightmap(grTerrain * T,grBitmap * Bmp)
{
grBitmap * Lock = NULL;
grBitmap_Info Info;
int w,h;

	assert( grTerrain_IsValid(T) );
	grTerrain_DeSelect(T);

	if ( Bmp == T->Heightmap ) // do nothing {} Bmp could've changed
	{
		return GR_TRUE;
	}
	else
	{
	grVFile *F1,*F2;
	char *N1,*N2;
		if ( grBitmap_GetPersistableName(Bmp,&F1,&N1) &&
	 		grBitmap_GetPersistableName(T->Heightmap,&F2,&N2) )
		{
			// both bitmaps persist as the same thing!
			if ( F1 == F2 && strcmp(N1,N2) == 0 )
			{
				return GR_TRUE;
			}
		}
	}

	T->Untouchable = GR_TRUE;

	if ( ! grBitmap_GetInfo(Bmp,&Info,NULL) )
		goto fail;

	w = Info.Width;
	h = Info.Height;

	if ( ! ispow2(w) )
	{
		grErrorLog_AddString(-1,"SetHeightmap : width not a power of 2", NULL);
		return GR_FALSE;
	}
	if ( ! ispow2(h) )
	{
		grErrorLog_AddString(-1,"SetHeightmap : height not a power of 2", NULL);
		return GR_FALSE;
	}		
	if ( w < 8 || h < 8 )
	{
		grErrorLog_AddString(-1,"SetHeightmap : width & height must be >= 8", NULL);
		return GR_FALSE;
	}	

	if ( T->QT )
		QuadTree_Destroy(&(T->QT));

	if ( T->HM )
		grRam_Free(T->HM);

	if ( T->Heightmap )
		grBitmap_Destroy(&(T->Heightmap));

	T->Heightmap = Bmp;
	grBitmap_CreateRef(Bmp);

	T->Changed = GR_TRUE;

	{
	int x,y,w,h,s;
	uint8 * bptr;
	float * HMptr,ScaleZ;

		if ( ! grBitmap_LockForRead(Bmp,&Lock,0,0,GR_PIXELFORMAT_8BIT_GRAY,GR_FALSE,0) )
			goto fail;

		if ( ! grBitmap_GetInfo(Lock,&Info,NULL) )
			goto fail;

		w = Info.Width;
		h = Info.Height;
		s = Info.Stride;
		T->HMWidth  = w + 1;		
		T->HMHeight = h + 1;
			
		T->InvCubeSize.X = (float)(T->HMWidth  - 1) / T->Size.X;
		T->InvCubeSize.Y = (float)(T->HMHeight - 1) / T->Size.Y;
		T->InvCubeSize.Z = 255.0f   / T->Size.Z;

		T->CubeSize.X = 1.0f / T->InvCubeSize.X;
		T->CubeSize.Y = 1.0f / T->InvCubeSize.Y;
		T->CubeSize.Z = 1.0f / T->InvCubeSize.Z;

		if ( (T->HM = (float *)grRam_Allocate(T->HMWidth*T->HMHeight*sizeof(float))) == NULL )
			goto fail;
		
		bptr = (uint8 *)grBitmap_GetBits(Lock);
		assert(bptr);
		HMptr = T->HM;
		ScaleZ = T->CubeSize.Z;

		for(y=h;y--;)
		{
			for(x=w;x--;)
			{
			float f;
				f = *bptr++;
				*HMptr++ = f * ScaleZ;
			}
			*HMptr++ = HMptr[-1];
			bptr += s-w;
		}
		for(x=w+1;x--;)
		{
			*HMptr++ = HMptr[- T->HMWidth];
		}

		grBitmap_UnLock(Lock); Lock = NULL;
	}

	T->QT = QuadTree_Create(T);
	if ( ! T->QT )
		goto fail;

	if ( ! grTerrain_RefreshQT(T) )
		goto fail;

	{
	grVFile *BmpF;
	char *BmpN;
		if ( grBitmap_GetPersistableName(Bmp,&BmpF,&BmpN) )
		{
			strcpy(T->HeightmapName,BmpN);
		}
		else
		{
			strcpy(T->HeightmapName,"unknown");
		}
	}

	T->Untouchable = GR_FALSE;

return GR_TRUE;

	fail:

	if ( Lock )
		grBitmap_UnLock(Lock);

return GR_FALSE;
}

static grBoolean grTerrain_RefreshQT(grTerrain * T)
{
	if ( ! T->QT )
		return GR_FALSE;
	grTerrain_SetParameters(T,T->MaxQuads,T->MinError);
	QuadTree_SetTexDim(T->QT,T->TexDim);
return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_Destroy(void ** pT)
{
grTerrain * T;
int i;

	assert(pT);
	T = (grTerrain *)*pT;
	if ( ! T )
		return( GR_TRUE );

	T->RefCount --;
	if ( T->RefCount > 0 )
		return( GR_FALSE );

	if ( T->Engine )
		grTerrain_DetachEngine(T,T->Engine);

	if ( T->NullTexture )
		grBitmap_Destroy(&(T->NullTexture));
	if ( T->HiliteTexture )
		grBitmap_Destroy(&(T->HiliteTexture));

	for(i=0;i<MAX_TEXTURES;i++)
	{
	grBitmap * Bmp;

		if ( T->Textures[i] && T->RegisteredTexture[i] && T->Engine )
			grEngine_RemoveBitmap(T->Engine,T->Textures[i]);

		Bmp = T->Textures[i];
		if ( Bmp )
			grBitmap_Destroy(&Bmp);
		Bmp = T->PreLightTextures[i];
		if ( Bmp )
			grBitmap_Destroy(&Bmp);
	}

	if ( T->QT )
	{
		QuadTree_ShowStats(T->QT);
		QuadTree_Destroy(&(T->QT));
	}

	if ( T->HM )
		grRam_Free(T->HM);

	if ( T->Heightmap )
		grBitmap_Destroy(&(T->Heightmap));

	if ( T->PropertyList )
		grProperty_ListDestroy(&(T->PropertyList));

	destroy(T);

	*pT = NULL;
	return( GR_TRUE );
}

GRAPI void GRCC grTerrain_CreateRef(void * T)
{
	assert( grTerrain_IsValid((grTerrain*)T) );
	((grTerrain*)T)->RefCount ++;
	return;
}

/*}{****************** File IO ************************************/

/*****

<>

this CreateFromFile is awfully slow
it'd be better to write the quadtree & recreate it;
of course then we have to recreate the heightmap plane from the QT
 which is a hassle

*****/

#define grVFile_ReadEntity(VF,ptr)	grVFile_Read( VF,(ptr),sizeof(*(ptr)))
#define grVFile_WriteEntity(VF,ptr)	grVFile_Write(VF,(ptr),sizeof(*(ptr)))

#define DISABLE_PTRMGR

static const uint32 grTerrain_Tag = 0x6E725447; // GTrn

GRAPI grBoolean GRCC grTerrain_WriteToFile(const void *Terrain, grVFile * File, grPtrMgr *PtrMgr)
{
	grTerrain* T = (grTerrain *)Terrain;
#if 0 //{

	{
	grXForm3d XF;
	grVec3d Size;
	uint32 MaxQuads,TexDim;
	float MinError;

		grTerrain_GetXForm(T,&XF);
		Size = T->Size;
		MaxQuads = T->MaxQuads;
		MinError = T->MinError;
		TexDim = T->TexDim;
		
		grVFile_WriteEntity(File, &XF );
		grVFile_WriteEntity(File, &Size );
		grVFile_WriteEntity(File, &MaxQuads );
		grVFile_WriteEntity(File, &MinError );
		grVFile_WriteEntity(File, &TexDim );
	}

return GR_TRUE;

#else //}{

// Added by cjp
#ifdef CJP_SAVE_OLDTERRAINOBJECTS

grVFile *VFS,*SubFile;
grBoolean suc;
int texN;

	assert( grTerrain_IsValid(T) );

	#ifdef DISABLE_PTRMGR
	PtrMgr = NULL;
	#endif

	VFS = grVFile_OpenNewSystem( File, GR_VFILE_TYPE_VIRTUAL, NULL, NULL, 
									GR_VFILE_OPEN_CREATE | GR_VFILE_OPEN_DIRECTORY );
	if ( ! VFS )
		return GR_FALSE;

	SubFile = grVFile_Open( VFS, "Terrain_Info", GR_VFILE_OPEN_CREATE );
	if ( ! SubFile )
	{
		grVFile_Close(VFS);
		return GR_FALSE;
	}

	{
	grXForm3d XF;
	grVec3d Size;
	uint32 MaxQuads,TexDim;
	float MinError;

		grTerrain_GetXForm(T,&XF);
		Size = T->Size;
		MaxQuads = T->MaxQuads;
		MinError = T->MinError;
		TexDim = T->TexDim;
		
		grVFile_WriteEntity(SubFile, &grTerrain_Tag );
		grVFile_WriteEntity(SubFile, &XF );
		grVFile_WriteEntity(SubFile, &Size );
		grVFile_WriteEntity(SubFile, &MaxQuads );
		grVFile_WriteEntity(SubFile, &MinError );
		grVFile_WriteEntity(SubFile, &TexDim );
	}

	suc = grVFile_Close(SubFile); SubFile = NULL;
	assert(suc);

//	if ( ! grBitmap_WriteToFileName2(T->Heightmap, VFS, "Terrain_Heightmap", PtrMgr) ) 
	if ( ! grBitmap_WriteToFileName(T->Heightmap, VFS, "Terrain_Heightmap") ) 
	{
		grVFile_Close(SubFile);
		grVFile_Close(VFS);
		return GR_FALSE;
	}

	for(texN=0;texN<(T->TexDim * T->TexDim);texN++)
	{
	char Name[1024];
	grBitmap * Bmp;

		sprintf(Name,"Terrain_Texture%d",texN);

		if ( T->PreLightTextures[texN] ) 
			Bmp = T->PreLightTextures[texN];
		else
			Bmp = T->Textures[texN];

	//	if ( ! grBitmap_WriteToFileName2(Bmp,VFS,Name,PtrMgr) )
		if ( ! grBitmap_WriteToFileName(Bmp,VFS,Name) )
		{
			grVFile_Close(VFS);
			return GR_FALSE;
		}
	}

	suc = grVFile_Close(VFS);
	assert(suc);

return GR_TRUE;

#else

	// The new code to save terrain objects..
	int texN;
	uint8 Version = 1;
	uint32 Tag = FILE_UNIQUE_ID;

	assert( grTerrain_IsValid(T) );

	{
		grXForm3d XF;
		grVec3d Size;
		uint32 MaxQuads,TexDim;
		float MinError;

		grTerrain_GetXForm(T,&XF);
		Size = T->Size;
		MaxQuads = T->MaxQuads;
		MinError = T->MinError;
		TexDim = T->TexDim;
		
		grVFile_WriteEntity(File, &grTerrain_Tag );
		grVFile_WriteEntity(File, &XF );
		grVFile_WriteEntity(File, &Size );
		grVFile_WriteEntity(File, &MaxQuads );
		grVFile_WriteEntity(File, &MinError );
		grVFile_WriteEntity(File, &TexDim );
	}

//	if ( ! grBitmap_WriteToFileName2(T->Heightmap, VFS, "Terrain_Heightmap", PtrMgr) ) 
	if ( ! grBitmap_WriteToFile(T->Heightmap, File) )
	{
		return GR_FALSE;
	}

	grVFile_Write(File, &Tag, sizeof(uint32));
	grVFile_Write(File, &Version, sizeof(uint8));

	for(texN=0;texN<(T->TexDim * T->TexDim);texN++)
	{
		char* BmpName;
		grVFile* fs;
		char Name[256];
		grBitmap * Bmp;

		if ( T->PreLightTextures[texN] ) 
			Bmp = T->PreLightTextures[texN];
		else
			Bmp = T->Textures[texN];

		grBitmap_GetPersistableName(Bmp, &fs, &BmpName);
		if (BmpName == NULL) {
			strcpy(Name, "Jet3D");
		} else {
			char* Global = strstr(BmpName, "GlobalMaterials");
			if (Global == NULL) {
				strcpy(Name, BmpName);
			} else {
				sprintf(Name,"%s",Global+strlen("GlobalMaterials")+1);
			}
			BmpName = strrchr(Name, '.');
			BmpName[0] = 0;
		}

		grVFile_Write(File, Name, 256);
	}

return GR_TRUE;


#endif // End of new save code. (CJP)

#endif // }

}

GRAPI grTerrain * GRCC grTerrain_CreateFromFileExt(grVFile * File,grVFile *ResourceBaseFS,grPtrMgr *PtrMgr)
{

#if 0 //{
grTerrain *T;

	T = grTerrain_Create();
	if ( ! T )
	{
		grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
		return NULL;
	}

	{
	grXForm3d XF;
	grVec3d Size;
	uint32 MaxQuads,TexDim;
	float MinError;

		grVFile_ReadEntity(File, &XF );
		grVFile_ReadEntity(File, &Size );
		grVFile_ReadEntity(File, &MaxQuads );
		grVFile_ReadEntity(File, &MinError );
		grVFile_ReadEntity(File, &TexDim );

		if ( ! grTerrain_SetXForm(T,&XF) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetSize(T,&Size) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetParameters(T,MaxQuads,MinError) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetTexDim(T,TexDim) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
	}

	return T;

#else //}{

// Added by cjp 2.18.00
// Code to load files with the old VFS style terrain objects.

#ifdef CJP_LOAD_OLDTERRAINOBJECTS

grTerrain *T;
grVFile *VFS,*SubFile;
grBitmap * Heightmap;
int tx,ty;

	#ifdef DISABLE_PTRMGR
	PtrMgr = NULL;
	#endif

	T = grTerrain_Create();
	if ( ! T )
		return NULL;

	VFS = grVFile_OpenNewSystem( File, GR_VFILE_TYPE_VIRTUAL, NULL, NULL, 
									GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY );
	if ( ! VFS )
		return NULL;

	SubFile = grVFile_Open( VFS, "Terrain_Info", GR_VFILE_OPEN_READONLY );
	if ( ! SubFile )
	{
		grVFile_Close(VFS);
		return NULL;
	}

	/*****

	XForm
	Size
	MaxQuads & MinError
	TexDim

	******/

	{
	grXForm3d XF;
	grVec3d Size;
	uint32 MaxQuads,TexDim;
	float MinError;
	uint32 Tag;

		grVFile_ReadEntity(SubFile, &Tag);

		if ( Tag != grTerrain_Tag )
		{
			grErrorLog_AddString(-1,"CreateFromFile : didn't get terrain tag!", NULL);
			return NULL;
		}

		grVFile_ReadEntity(SubFile, &XF );
		grVFile_ReadEntity(SubFile, &Size );
		grVFile_ReadEntity(SubFile, &MaxQuads );
		grVFile_ReadEntity(SubFile, &MinError );
		grVFile_ReadEntity(SubFile, &TexDim );

		if ( ! grTerrain_SetXForm(T,&XF) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetSize(T,&Size) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetParameters(T,MaxQuads,MinError) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetTexDim(T,TexDim) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
	}

	grVFile_Close(SubFile); SubFile = NULL;

//	if ( ! (Heightmap = grBitmap_CreateFromFile2(SubFile,ResourceBaseFS,PtrMgr)) )
	if ( ! (Heightmap = grBitmap_CreateFromFileName(VFS,"Terrain_Heightmap")) )
	{
		grVFile_Close(SubFile);
		grVFile_Close(VFS);
		destroy(T);
		return NULL;
	}

	if ( ! grTerrain_SetHeightmap(T,Heightmap) )
	{
		grTerrain_Destroy(&T);
		grVFile_Close(VFS);
		return NULL;
	}

	for(tx=0;tx<(T->TexDim);tx++)
	{
		for(ty=0;ty<(T->TexDim);ty++)
		{
		char Name[1024];
		grBitmap * Tex;

			sprintf(Name,"Terrain_Texture%d",tx + ty*(T->TexDim));

		//	if ( ! (Tex = grBitmap_CreateFromFileName2(VFS,Name,PtrMgr)) )
			if ( ! (Tex = grBitmap_CreateFromFileName(VFS,Name)) )
			{
				grVFile_Close(VFS);
				destroy(T);
				return NULL;
			}

			grTerrain_SetATexture(T,Tex,tx,ty);
		}
	}

	grVFile_Close(VFS);

return T;

#else
// Load the new direct style of terrain objects.

	grTerrain *T;

	grBitmap * Heightmap;
	int tx,ty;
	uint8 Version;
	uint32 Tag;

	T = (grTerrain *)grTerrain_Create();
	if ( ! T )
		return NULL;

	/*****

	XForm
	Size
	MaxQuads & MinError
	TexDim

	******/

	{
		grXForm3d XF;
		grVec3d Size;
		uint32 MaxQuads,TexDim;
		float MinError;

		grVFile_ReadEntity(File, &Tag);

		if ( Tag != grTerrain_Tag )
		{
			grErrorLog_AddString(-1,"CreateFromFile : didn't get terrain tag!", NULL);
			return NULL;
		}

		grVFile_ReadEntity(File, &XF );
		grVFile_ReadEntity(File, &Size );
		grVFile_ReadEntity(File, &MaxQuads );
		grVFile_ReadEntity(File, &MinError );
		grVFile_ReadEntity(File, &TexDim );

		if ( ! grTerrain_SetXForm(T,&XF) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetSize(T,&Size) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetParameters(T,MaxQuads,MinError) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
		if ( ! grTerrain_SetTexDim(T,TexDim) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : failure", NULL);
			return NULL;
		}
	}

//	if ( ! (Heightmap = grBitmap_CreateFromFile2(SubFile,ResourceBaseFS,PtrMgr)) )
	if ( ! (Heightmap = grBitmap_CreateFromFile(File)) )
	{
		destroy(T);
		return NULL;
	}

	if ( ! grTerrain_SetHeightmap(T,Heightmap) )
	{
		grTerrain_Destroy((void **)&T);
		return NULL;
	}

	if(!grVFile_Read(File, &Tag, sizeof(Tag)))
	{
		grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
		grTerrain_Destroy((void **)&T);
		return NULL;
	}

	if (Tag == FILE_UNIQUE_ID)
	{
		if (!grVFile_Read(File, &Version, sizeof(Version)))
		{
    		grErrorLog_Add( GR_ERR_SYSTEM_RESOURCE, NULL );
			grTerrain_Destroy((void **)&T);
		    return NULL;
		}
	}
	else
	{
		Version = 0;
		grVFile_Seek(File,-((int)sizeof(Tag)),GR_VFILE_SEEKCUR);
	}
	
	// I think it should be for(y values) then for(x values) 
	for(ty=0;ty<(T->TexDim);ty++)
	{
		for(tx=0;tx<(T->TexDim);tx++)
		{
			grBitmap * Tex;

			if (Version) {
				char Name[256];
				grVFile_Read(File, Name, 256);
				Tex = (grBitmap*) grResource_GetResource(grPtrMgr_GetResourceMgr(PtrMgr), GR_RESOURCE_BITMAP, Name);
			} else {
				Tex = grBitmap_CreateFromFile(File);
			}

//			sprintf(Name,"Terrain_Texture%d",tx + ty*(T->TexDim));
//			if ( ! (Tex = grBitmap_CreateFromFileName2(VFS,Name,PtrMgr)) )

			if (Tex==NULL)
			{
				destroy(T);
				return NULL;
			}

			grTerrain_SetATexture(T,Tex,tx,ty);
		}
	}

return T;

// End of new loader code
#endif // OLD_TERRAINOBJECTS

#endif //}

}

GRAPI void * GRCC grTerrain_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
return grTerrain_CreateFromFileExt(File,NULL,PtrMgr);
}

/*}{******************************************************/

static void grTerrain_XFormCameraToTerrainSpace(grTerrain * T,grCamera *pC)
{
grXForm3d CXF;

	/**
		Camera in takes World space to Screen space
		we want to make a camera that takes terrain space to screen space

		the old camera xform is WtoS
		we want to append TtoW
			C = (WtoS) (TtoW)

		notez : the "Transpose" XForm in the camera is the one that
			actually does the WtoS projection
	***/

	grCamera_GetTransposeXForm(pC,&CXF);  // CXF = W ^ -1

	grXForm3d_Multiply(&CXF,&(T->XFTerrainToWorld),&CXF); // CXF = W^-1 T

	grCamera_SetTransposeXForm(pC,&CXF); // C = CXF^-1 = (W^-1 T)^-1 = T^-1 W
}

static void grTerrain_XFormFrustumToTerrainSpace(grTerrain * T,const grFrustum *worldF,grFrustum *pF)
{
	grFrustum_Transform(worldF,&(T->XFWorldToTerrain),pF);
}

extern const grXForm3d *	GRCF grCamera_XForm( const grCamera *Camera);
extern const grXForm3d *	GRCF grCamera_WorldXForm( const grCamera *Camera);
//extern const grVec3d *		GRCF grCamera_GetPov(const grCamera *Camera);

static void grTerrain_MakeTerrainSpaceFrustum(grTerrain * T,const grCamera * Camera,grFrustum *pF)
{
	grCamera_PushXForm((grCamera *)Camera);
	grTerrain_XFormCameraToTerrainSpace(T,(grCamera *)Camera);
	
	grFrustum_SetFromCamera(pF,Camera); // pF in camera space

	grFrustum_TransformAnchored(pF, grCamera_WorldXForm(Camera), grCamera_GetPov(Camera) );

	grCamera_PopXForm((grCamera *)Camera);
}

/*}{****************** Z at XY stuff ************************************/
// !!! ALL OF THIS _Get stuff is in terrain space !!

static void grTerrain_GetZBox(const grTerrain *T,grFloat X,grFloat Y,grFloat * CornerZs, grFloat * pfx,grFloat * pfy)
{
float baseX,baseY;
int sx,sy;
float * HMptr;

	sx = (int)(X * T->InvCubeSize.X);
	sy = (int)(Y * T->InvCubeSize.Y);
	
	sx = GR_CLAMP(sx,0,T->HMWidth -2);
	sy = GR_CLAMP(sy,0,T->HMHeight-2);

	baseX = sx * T->CubeSize.X;
	baseY = sy * T->CubeSize.Y;

	*pfx = (X - baseX) * T->InvCubeSize.X;
	*pfy = (Y - baseY) * T->InvCubeSize.Y;

	HMptr = T->HM + sx + sy * T->HMWidth;
	CornerZs[0] = HMptr[0];
	CornerZs[1] = HMptr[1];
	HMptr += T->HMWidth;
	CornerZs[2] = HMptr[1];
	CornerZs[3] = HMptr[0];

return;
}

GRAPI grFloat GRCC grTerrain_GetHeightAtWorldSpaceVec(const grTerrain *Terrain,const grVec3d *pV)
{
grVec3d V;
	grXForm3d_Transform(&(Terrain->XFWorldToTerrain),pV,&V);
	V.Z = grTerrain_GetHeightAtXY(Terrain,V.X,V.Y);
	grXForm3d_Transform(&(Terrain->XFTerrainToWorld),&V,&V);
return V.Z;
}

GRAPI void GRCC grTerrain_GetNormalAtWorldSpaceVec(const grTerrain *Terrain,const grVec3d *pV,grVec3d *pN)
{
grVec3d V;
	grXForm3d_Transform(&(Terrain->XFWorldToTerrain),pV,&V);
	grTerrain_GetNormalAtXY(Terrain,V.X,V.Y,pN);
	grXForm3d_Rotate(&(Terrain->XFTerrainToWorld),pN,pN);
}

GRAPI grFloat GRCC grTerrain_GetHeightAtXY(const grTerrain *Terrain,grFloat X,grFloat Y)
{
grFloat fx,fy;
grFloat CornerZs[4]; //SW,SE,NE,NW
grFloat z;

	assert( grTerrain_IsValid(Terrain) );

	grTerrain_GetZBox(Terrain,X,Y, CornerZs, &fx,&fy);
	
	z =			fy  * (CornerZs[2] * fx + CornerZs[3] * (1.0f - fx)) + 
		(1.0f - fy) * (CornerZs[1] * fx + CornerZs[0] * (1.0f - fx));

return z;
}

static grBoolean 	grTerrain_GetNormalAtXY_Raw(const grTerrain *Terrain,grFloat X,grFloat Y,
										grVec3d *pNormal,grFloat *pfx,grFloat *pfy)
{
grFloat CornerZs[4]; //SW,SE,NE,NW
grVec3d Seg1,Seg2;

	grTerrain_GetZBox(Terrain,X,Y, CornerZs, pfx,pfy);
	
	// {} could write a custom cross product that takes advantage of our known zeros

	Seg1.X = Terrain->CubeSize.X;
	Seg1.Y = 0.0f;
	Seg1.Z = CornerZs[1] - CornerZs[0]; //SE - SW

	Seg2.X = 0.0f;
	Seg2.Y = Terrain->CubeSize.Y;
	Seg2.Z = CornerZs[3] - CornerZs[0]; //NW - SW

	grVec3d_CrossProduct(&Seg1,&Seg2,pNormal); // pNormal = Seg1 x Seg2 , points up
	assert( pNormal->Z >= 0.0f );
	grVec3d_Normalize(pNormal);

	return GR_TRUE;
}

GRAPI void GRCC grTerrain_GetNormalAtXY_Rough(const grTerrain *Terrain,grFloat X,grFloat Y,grVec3d *pNormal)
{
grFloat fx,fy;
	grTerrain_GetNormalAtXY_Raw(Terrain,X,Y,pNormal,&fx,&fy);
}

GRAPI void GRCC grTerrain_GetNormalAtXY(const grTerrain *Terrain,grFloat X,grFloat Y,grVec3d *pNormal)
{
grFloat fx,fy,mulx,muly,stepx,stepy;
grVec3d NormalX,NormalY;

	assert( grTerrain_IsValid(Terrain) );

	grTerrain_GetNormalAtXY_Raw(Terrain,X,Y,pNormal,&fx,&fy);
	if ( fx < 0.5f )
	{
		mulx = 1.0f - 2.0f * fx;
		stepx = - Terrain->CubeSize.X;
	}
	else
	{
		mulx = 2.0f * fx - 1.0f;
		stepx = + Terrain->CubeSize.X;
	}

	if ( fy < 0.5f )
	{
		muly = 1.0f - 2.0f * fy;
		stepy = - Terrain->CubeSize.Y;
	}
	else
	{
		muly = 2.0f * fy - 1.0f;
		stepy = + Terrain->CubeSize.Y;
	}

	grTerrain_GetNormalAtXY_Raw(Terrain,X+stepx,Y,&NormalX,&fx,&fy);
	grTerrain_GetNormalAtXY_Raw(Terrain,X,Y+stepy,&NormalY,&fx,&fy);

	grVec3d_AddScaled(pNormal,&NormalX,mulx,pNormal);
	grVec3d_AddScaled(pNormal,&NormalY,muly,pNormal);

	grVec3d_Normalize(pNormal);
}

/*}{******************************************************/

grBoolean grExtBox_SphereCollision(grExtBox *pBox,grVec3d *pPos,grFloat Radius)
{
	if(( pPos->X >= (pBox->Min.X - Radius) && pPos->X <= (pBox->Max.X + Radius) )
	&& ( pPos->Y >= (pBox->Min.Y - Radius) && pPos->Y <= (pBox->Max.Y + Radius) )
	&& ( pPos->Z >= (pBox->Min.Z - Radius) && pPos->Z <= (pBox->Max.Z + Radius) ))
		return GR_TRUE;
return GR_FALSE;
}

static grBoolean grTerrain_SetDynamicLightsFromWorld(grTerrain *T,grWorld *World)
{
grChain * LightChain;
grChain_Link * Link;
grExtBox TerrainBox;

	assert( grTerrain_IsValid(T) );

	LightChain = grWorld_GetDLightChain(World);
	if ( ! LightChain )
		return GR_FALSE;

	T->NumDynamicLights = 0;

	grTerrain_GetExtBox(T,&TerrainBox);

	for( Link = grChain_GetFirstLink(LightChain); Link; Link = grChain_LinkGetNext(Link) )
	{
	grLight * Light;
	grVec3d Pos,Color;
	grFloat Radius,Brightness;
	uint32 Flags;
	grTerrain_Light * TLight;

		if ( T->NumDynamicLights >= TERRAIN_MAX_NUM_LIGHTS )
			break; 

		Light = (grLight *)grChain_LinkGetLinkData(Link);

		if ( ! grLight_GetAttributes(Light,&Pos,&Color,&Radius,&Brightness,&Flags) )
			return GR_FALSE;

		// if the light is way out of the extbox of the terrain, don't even add it to the list!
		if ( ! (Flags & GR_LIGHT_FLAG_PARALLEL) )
			if ( ! grExtBox_SphereCollision(&TerrainBox,&Pos,Radius) )
				continue; 

		TLight = T->DynamicLights + T->NumDynamicLights;
		T->NumDynamicLights ++;

		TLight->Type = TERRAIN_LIGHT_SPHERE;

		// must store the position in terrain space!		
		grXForm3d_Transform(&(T->XFWorldToTerrain),&Pos,&(TLight->Vector));

		grVec3d_Scale(&Color, Brightness, &Color); // <> or something

		TLight->Color.r = Color.X;
		TLight->Color.g = Color.Y;
		TLight->Color.b = Color.Z;

		TLight->MaxColor = max3(GR_ABS(TLight->Color.r),GR_ABS(TLight->Color.g),GR_ABS(TLight->Color.b));
	}

return GR_TRUE;
}

static grBoolean grTerrain_RestoreUnLitTextures(grTerrain *T)
{
int i;
grBitmap *OldBmp;

	if ( ! T->TexturesAreLit )
		return GR_TRUE;

	for(i=0;i<(T->TexDim * T->TexDim);i++)
	{
		OldBmp = T->PreLightTextures[i];
		if ( ! OldBmp )
		{
			T->PreLightTextures[i] = T->Textures[i];
			T->Textures[i] = grBitmap_CreateCopy(T->PreLightTextures[i]);

			if ( T->Engine )
			{
				if ( T->RegisteredTexture[i] )
				{
					grEngine_RemoveBitmap(T->Engine,T->PreLightTextures[i]);
				}
				grEngine_AddBitmap(T->Engine,T->Textures[i],GR_ENGINE_BITMAP_TYPE_3D);
				T->RegisteredTexture[i] = GR_TRUE;
			}
		}
		else
		{
			grBitmap_BlitBitmap( OldBmp, T->Textures[i] );
		}
	}

	T->TexturesAreLit = GR_FALSE;
return GR_TRUE;
}

static grBoolean grTerrain_SaveUnLitTextures(grTerrain *T)
{
int i;

	for(i=0;i<(T->TexDim * T->TexDim);i++)
	{
		if ( ! T->PreLightTextures[i] )
		{
			T->PreLightTextures[i] = T->Textures[i];
			T->Textures[i] = grBitmap_CreateCopy(T->PreLightTextures[i]);
			
			if ( T->Engine )
			{
				if ( T->RegisteredTexture[i] )
				{
					grEngine_RemoveBitmap(T->Engine,T->PreLightTextures[i]);
				}
				grEngine_AddBitmap(T->Engine,T->Textures[i],GR_ENGINE_BITMAP_TYPE_3D);
				T->RegisteredTexture[i] = GR_TRUE;
			}
		}
	}

return GR_TRUE;
}

static grBoolean grTerrain_SetDefaultLighting(grTerrain *T)
{
	assert( grTerrain_IsValid(T) );

	if ( T->TexturesAreLit )
	{
		if ( ! grTerrain_RestoreUnLitTextures(T) )
			return GR_FALSE;
	}
	
	if ( T->QT )
	{
		QuadTree_ResetAllVertexLighting(T->QT);
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_SetLightsInTextureFromWorld(grTerrain *T,grWorld *World,grBoolean SelfShadow,grBoolean GetWorldShadows)
{
grLight * Lights[TERRAIN_MAX_NUM_LIGHTS];
int l,NumLights;
grChain * LightChain;
grChain_Link * Link;
grExtBox TerrainBox;

	assert( grTerrain_IsValid(T) );
	if ( ! T->QT )
		return GR_FALSE;

	if ( ! World )
		return GR_FALSE;
				
	if ( T->TexturesAreLit )
	{
		if ( ! grTerrain_RestoreUnLitTextures(T) )
			return GR_FALSE;
	}
	else
	{
		grTerrain_SaveUnLitTextures(T);
	}

	LightChain = grWorld_GetLightChain(World);
	if ( ! LightChain )
		return GR_FALSE;

	grTerrain_GetExtBox(T,&TerrainBox);

	for(NumLights = 0,Link = grChain_GetFirstLink(LightChain); Link; Link = grChain_LinkGetNext(Link) )
	{
	grLight * Light;
	grVec3d Pos,Color;
	grFloat Radius,Brightness;
	uint32 Flags;

		if ( NumLights >= TERRAIN_MAX_NUM_LIGHTS )
			break; 

		Light = (grLight *)grChain_LinkGetLinkData(Link);

		if ( ! grLight_GetAttributes(Light,&Pos,&Color,&Radius,&Brightness,&Flags) )
			return GR_FALSE;

		// if the light is way out of the extbox of the terrain, don't even add it to the list!	
		if ( ! (Flags & GR_LIGHT_FLAG_PARALLEL) )
			if ( ! grExtBox_SphereCollision(&TerrainBox,&Pos,Radius) )
				continue; 

		Lights[NumLights] = grLight_CreateFromLight(Light);

		// must store the position in terrain space!		
		grXForm3d_Transform(&(T->XFWorldToTerrain),&Pos,&Pos);

		if ( ! grLight_SetAttributes(Lights[NumLights],&Pos,&Color,Radius,Brightness,Flags) )
			return GR_FALSE;

		NumLights++;
	}


	QuadTree_ResetAllVertexLighting(T->QT);

	QuadTree_LightTexture(T->QT,Lights,NumLights,SelfShadow,GetWorldShadows);

	for(l=0;l<NumLights;l++)
	{
		grLight_Destroy(&(Lights[l]));
	}

	T->TexturesAreLit = GR_TRUE;

return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_SetLightsOnVertsFromWorld(grTerrain *T,grWorld *World)
{
grLight * Lights[TERRAIN_MAX_NUM_LIGHTS];
int l,NumLights;
grChain * LightChain;
grChain_Link * Link;
grExtBox TerrainBox;

	assert( grTerrain_IsValid(T) );
	if ( ! T->QT )
		return GR_FALSE;

	if ( ! World )
		return GR_FALSE;

	LightChain = grWorld_GetLightChain(World);
	if ( ! LightChain )
		return GR_FALSE;

	grTerrain_GetExtBox(T,&TerrainBox);

	for(NumLights = 0,Link = grChain_GetFirstLink(LightChain); Link; Link = grChain_LinkGetNext(Link) )
	{
	grLight * Light;
	grVec3d Pos,Color;
	grFloat Radius,Brightness;
	uint32 Flags;

		if ( NumLights >= TERRAIN_MAX_NUM_LIGHTS )
			break; 

		Light = (grLight *)grChain_LinkGetLinkData(Link);

		if ( ! grLight_GetAttributes(Light,&Pos,&Color,&Radius,&Brightness,&Flags) )
			return GR_FALSE;

		// if the light is way out of the extbox of the terrain, don't even add it to the list!
		if ( ! (Flags & GR_LIGHT_FLAG_PARALLEL) )
			if ( ! grExtBox_SphereCollision(&TerrainBox,&Pos,Radius) )
				continue; 

		Lights[NumLights] = grLight_CreateFromLight(Light);

		// must store the position in terrain space!		
		grXForm3d_Transform(&(T->XFWorldToTerrain),&Pos,&Pos);

		if ( ! grLight_SetAttributes(Lights[NumLights],&Pos,&Color,Radius,Brightness,Flags) )
			return GR_FALSE;

		NumLights++;
	}

	grTerrain_RestoreUnLitTextures(T);

	QuadTree_LightAllPoints(T->QT,Lights,NumLights);

	for(l=0;l<NumLights;l++)
	{
		grLight_Destroy(&(Lights[l]));
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_SetLightsOnVerts(grTerrain *T, grLight **SrcLights, int Count)
{
	grLight * Lights[TERRAIN_MAX_NUM_LIGHTS];
	int l,NumLights;
	int	i;
	//grChain * LightChain;
	//grChain_Link * Link;
	grExtBox TerrainBox;

	if	(Count > TERRAIN_MAX_NUM_LIGHTS)
		return GR_FALSE;

	assert( grTerrain_IsValid(T) );
	if ( ! T->QT )
		return GR_FALSE;

	grTerrain_GetExtBox(T,&TerrainBox);

	for(NumLights = 0, i = 0; i < Count; i++ )
	{
		grLight * Light;
		grVec3d Pos,Color;
		grFloat Radius,Brightness;
		uint32 Flags;

		Light = SrcLights[i];

		if ( ! grLight_GetAttributes(Light,&Pos,&Color,&Radius,&Brightness,&Flags) )
			return GR_FALSE;

		// if the light is way out of the extbox of the terrain, don't even add it to the list!
		if ( ! (Flags & GR_LIGHT_FLAG_PARALLEL) )
			if ( ! grExtBox_SphereCollision(&TerrainBox,&Pos,Radius) )
				continue; 

		Lights[NumLights] = grLight_CreateFromLight(Light);

		// must store the position in terrain space!		
		grXForm3d_Transform(&(T->XFWorldToTerrain),&Pos,&Pos);

		if ( ! grLight_SetAttributes(Lights[NumLights],&Pos,&Color,Radius,Brightness,Flags) )
			return GR_FALSE;

		NumLights++;
	}

	grTerrain_RestoreUnLitTextures(T);

	QuadTree_LightAllPoints(T->QT,Lights,NumLights);

	for(l=0;l<NumLights;l++)
	{
		grLight_Destroy(&(Lights[l]));
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_GetTextureAtXY(const grTerrain *T,grFloat X,grFloat Y,const grBitmap ** pBmp,int *pTX,int *pTY)
{
	int tx,ty;

	assert( grTerrain_IsValid(T) );
	
	tx = (int)(X * T->TexDim / T->Size.X);
	ty = (int)(Y * T->TexDim / T->Size.Y);

	if ( tx < 0 || tx >= T->TexDim || ty < 0 || ty >= T->TexDim )
		return GR_FALSE;

	if ( pBmp )	*pBmp = T->Textures[ tx + ty * T->TexDim ];
	if ( pTX  ) *pTX = tx;
	if ( pTY  ) *pTY = ty;

	return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_GetTextureAtWorldVec(const grTerrain *T,const grVec3d *pVec,const grBitmap ** pBmp,int *pTX,int *pTY)
{
	grVec3d V;
	grXForm3d_Transform(&(T->XFWorldToTerrain),pVec,&V);
	return grTerrain_GetTextureAtXY(T,V.X,V.Y,pBmp,pTX,pTY);
}

GRAPI grBoolean GRCC grTerrain_SetATexture(grTerrain *T,const grBitmap * Bmp,int x,int y)
{
	int i;

	assert( grTerrain_IsValid(T) );

	grTerrain_DeSelect(T);

	if ( ! Bmp ) Bmp = T->NullTexture;

	if ( x >= T->TexDim || y >= T->TexDim )
		return GR_FALSE;

	i = x + y * T->TexDim;

	grBitmap_CreateRef((grBitmap *)Bmp);

	if ( T->Textures[i] )
	{
		if ( T->RegisteredTexture[i] && T->Engine )
		{
			grEngine_RemoveBitmap(T->Engine,T->Textures[i]);
			T->RegisteredTexture[i] = GR_FALSE;
		}
		grBitmap_Destroy( & (T->Textures[i]) );
	}

	if ( T->PreLightTextures[i] )
		grBitmap_Destroy( & (T->PreLightTextures[i]) );

	T->Textures[i] = (grBitmap *)Bmp;
	T->PreLightTextures[i] = NULL;
	T->RegisteredTexture[i] = GR_FALSE;

	if ( T->Engine )
	{
		grEngine_AddBitmap(T->Engine,T->Textures[i],GR_ENGINE_BITMAP_TYPE_3D);
		T->RegisteredTexture[i] = GR_TRUE;
	}

	return GR_TRUE;
}

static int intlog2(int x)
{
	float xf = (float)x;
	return ((*(int*)&xf) >> 23) - 127;
}

GRAPI grBoolean GRCC grTerrain_SetTexDim(grTerrain *T,int TexDim)
{
	int OldTexDim;
	grBitmap * OldTextures[MAX_TEXTURES];
	grBitmap * OldPLTextures[MAX_TEXTURES];
	int x,y,i;

	assert( grTerrain_IsValid(T) );
	
	grTerrain_DeSelect(T);

	if ( TexDim < 1 || TexDim > MAX_TEXDIM )
	{
		grErrorLog_AddString(-1,"SetTexDim : out of bounds!", NULL);
		return GR_FALSE;
	}

	x = intlog2(TexDim);
	if ( (1<<x) != TexDim ) // !! TexDim must be a power of 2
	{
		grErrorLog_AddString(-1,"SetTexDim : not a power of 2", NULL);
		return GR_FALSE;
	}

	OldTexDim = T->TexDim;
	for(i=0;i<OldTexDim * OldTexDim;i++)
	{
		if ( T->Textures[i] && T->PreLightTextures[i] && T->RegisteredTexture[i] && T->Engine )
		{
			grEngine_RemoveBitmap(T->Engine,T->Textures[i]);
			T->RegisteredTexture[i] = GR_FALSE;
		}
	}

	memcpy(OldTextures,T->Textures,sizeof(grBitmap *)*MAX_TEXTURES);
	memcpy(OldPLTextures,T->PreLightTextures,sizeof(grBitmap *)*MAX_TEXTURES);

	T->TexDim = TexDim;
	memset(T->Textures,0,sizeof(grBitmap *)*MAX_TEXTURES);

	for(x=0;x<TexDim;x++)
	{
		for(y=0;y<TexDim;y++)
		{
			if ( x < OldTexDim && y < OldTexDim )
			{
			int i;
				i = x + y * OldTexDim;
				if ( OldPLTextures[i] )
					grTerrain_SetATexture(T,OldPLTextures[i],x,y);
				else
					grTerrain_SetATexture(T,OldTextures[i],x,y);
			}
			else
				grTerrain_SetATexture(T,NULL,x,y);
		}
	}

	for(x=0;x<OldTexDim * OldTexDim;x++)
	{
		assert( OldTextures[x] );
		if ( OldPLTextures[x] )
		{
			grBitmap_Destroy( OldPLTextures + x );
		}
		grBitmap_Destroy( OldTextures + x );
	}

	if ( T->QT )
	{
		QuadTree_SetTexDim(T->QT,TexDim);
	}

	T->Changed = GR_TRUE;

return GR_TRUE;
}

/*}{******************************************************/

GRAPI grBoolean GRCC grTerrain_RenderPrep(grTerrain * T,grEngine *Engine,grCamera *worldC)
{
grVec3d Pos,Vec;

	assert( grTerrain_IsValid(T) );
	assert( worldC);

	if ( T->Untouchable )
		return GR_TRUE;

	if ( ! T->QT )
		return GR_FALSE;

	grTerrain_AttachEngine(T,Engine);

	{
	grXForm3d XF;
		grCamera_GetXForm(worldC,&XF);

		Pos = XF.Translation;
		grXForm3d_GetIn(&XF,&Vec);
		grXForm3d_Rotate(&(T->XFWorldToTerrain),&Vec,&Vec);

		grXForm3d_Transform(&(T->XFWorldToTerrain),&Pos,&Pos);

		 // camera Pos & Vec now in terrain space
	}

	if ( ! T->Changed )
	{
	float dPos,dVec;
	grVec3d Temp;

		grVec3d_Subtract(&Pos,&(T->LastTesselatedCameraPos),&Temp);
		dPos = grVec3d_Length(&Temp) * T->InvCubeSize.X;

			// difference in position *in heightmap pixels*

		dVec = grVec3d_DotProduct(&Vec,&(T->LastTesselatedCameraVec));
		dVec = 1.0f - GR_ABS(dVec);

		// these numbers are pretty good
		// {} if you put the dPos tolerance too high, you'll get black slivers 
		//		when you make small moves (because of the backfacing)
		if ( dPos >= 0.05f || dVec >= 0.0001f )
		{
			T->Changed = GR_TRUE;
		}
	}

	if ( T->Changed )
	{
	grFrustum F;

		grTerrain_MakeTerrainSpaceFrustum(T,worldC,&F);

		if ( ! QuadTree_Tesselate(T->QT,&Pos,&F) )
		{
			return GR_FALSE;
		}

		if ( T->World )
		{
			grTerrain_SetDynamicLightsFromWorld(T,T->World);
		}

		if ( T->NumDynamicLights > 0 )
		{
			if ( ! QuadTree_LightTesselatedPoints(T->QT,T->DynamicLights,T->NumDynamicLights) )
			{
				return GR_FALSE;
			}
		}

		T->LastTesselatedCameraPos = Pos;
		T->LastTesselatedCameraVec = Vec;

		T->Changed = GR_FALSE;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_RenderThroughCamera(grTerrain *T,const grWorld *World, const grEngine *E,
																	grCamera *Camera)
{
grFrustum F;
grBoolean ret;

	assert( grTerrain_IsValid(T) );	

	if ( ! T->World  ) ((grTerrain *)T)->World = (grWorld *)World;

	if (!grTerrain_RenderPrep((grTerrain*)T,(grEngine *)E,Camera))
		return GR_FALSE;

	grTerrain_MakeTerrainSpaceFrustum(T,Camera,&F);

	grCamera_PushXForm(Camera);
	grTerrain_XFormCameraToTerrainSpace(T,Camera);
	
	ret = QuadTree_Render(T->QT,(grEngine *)E,Camera,&F);
	
	grCamera_PopXForm(Camera);

return ret;
}

GRAPI grBoolean GRCC grTerrain_RenderThroughFrustum(grTerrain *T,const grWorld *World, const grEngine *E,
																grCamera *Camera, const grFrustum *worldF)
{
grFrustum F;
grBoolean ret;

	assert( grTerrain_IsValid(T) );	

	if ( ! T->World  ) ((grTerrain *)T)->World = (grWorld *)World;

	if (!grTerrain_RenderPrep((grTerrain*)T,(grEngine *)E,Camera))
		return GR_FALSE;

	grTerrain_XFormFrustumToTerrainSpace(T,worldF,&F);

	grCamera_PushXForm(Camera);
	grTerrain_XFormCameraToTerrainSpace(T,Camera);

	ret = QuadTree_Render(T->QT,(grEngine *)E,Camera,&F);
	
	grCamera_PopXForm(Camera);

return ret;
}

GRAPI grBoolean GRCC grTerrain_ObjectRender(const void *Terrain,const grWorld *World, const grEngine *E,
											const grCamera *Camera, const grFrustum *F,grObject_RenderFlags RenderFlags)
{
	grTerrain* T = (grTerrain *)Terrain;
	assert( grTerrain_IsValid(T) );	
	assert( World && E && Camera );

	if ( T->Untouchable )
		return GR_TRUE;

	if ( !T->RenderNextFlag )
		return GR_TRUE;

	if ( RenderFlags & GR_OBJECT_RENDER_FLAG_CAMERA_FRUSTUM )
	{
		return grTerrain_RenderThroughCamera((grTerrain*)T,World,E,(grCamera *)Camera);
	}
	else
	{
	grFrustum WF;
		grFrustum_TransformToWorldSpace(F,Camera,&WF);
		return grTerrain_RenderThroughFrustum((grTerrain*)T,World,E,(grCamera *)Camera,&WF);
	}
}

/*}{******************************************************/

GRAPI grBoolean GRCC grTerrain_SetParameters(grTerrain *T,uint32 MaxQuads,float MinError)
{
	assert( grTerrain_IsValid(T) );
	T->MaxQuads = MaxQuads;
	T->MinError = MinError;

	if ( T->QT )
	{
		// if QT is made later, we'll set the parameters then
		QuadTree_SetParameters(T->QT,2,MaxQuads,MinError);
	}

	// force a re-tesselate
	T->Changed = GR_TRUE;

return GR_TRUE;
}

GRAPI void GRCC grExtBox_Transform(const grExtBox *pIn,const grXForm3d * pXF,grExtBox *pOut)
{
grVec3d Corners[8];
int i;

	for(i=0;i<8;i++)
	{
	grVec3d *pV;
		pV = Corners + i;
		if ( i & 1 ) pV->X = pIn->Min.X; else pV->X = pIn->Max.X;
		if ( i & 2 ) pV->Y = pIn->Min.Y; else pV->Y = pIn->Max.Y;
		if ( i & 4 ) pV->Z = pIn->Min.Z; else pV->Z = pIn->Max.Z;
		
		grXForm3d_Transform(pXF,pV,pV);
	}

	pOut->Min = pOut->Max = Corners[0];
	for(i=1;i<8;i++)
	{
	grVec3d *pV;
		pV = Corners + i;
		pOut->Min.X = min(pOut->Min.X,pV->X);
		pOut->Min.Y = min(pOut->Min.Y,pV->Y);
		pOut->Min.Z = min(pOut->Min.Z,pV->Z);
		pOut->Max.X = max(pOut->Max.X,pV->X);
		pOut->Max.Y = max(pOut->Max.Y,pV->Y);
		pOut->Max.Z = max(pOut->Max.Z,pV->Z);
	}
}

GRAPI grBoolean GRCC grTerrain_GetExtBox(const void *Terrain,grExtBox * pBox)
{
	const grTerrain* T = (grTerrain *)Terrain;
	assert( grTerrain_IsValid(T) );
	
	if ( T->QT && ! T->Untouchable )
	{
		QuadTree_GetExtBox(T->QT,pBox);

		// pbox is in terrain space; must xform it back :

		grExtBox_Transform(pBox,&(T->XFTerrainToWorld),pBox);
	}
	else
	{
		// make a little fake box
		pBox->Min = T->XFTerrainToWorld.Translation;
		pBox->Max = pBox->Min;
		pBox->Max.X += T->Size.X;
		pBox->Max.Y += T->Size.Y;
		pBox->Max.Z += T->Size.Z;
	}
 
   return GR_TRUE;
}

static grBoolean grTerrain_IsValid(const grTerrain *T)
{
	assert( T );	
	assert( ! T->QT || QuadTree_IsValid(T->QT) );
	assert( T->SelfCheck == T );
	return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_IntersectsRay(const grTerrain *T,grVec3d *pStart,grVec3d *pDirection)
{
   grExtBox Box;
   grVec3d Start,Direction;

	assert( grTerrain_IsValid(T) );
	assert(pStart && pDirection );

	if ( ! T->QT )
		return GR_FALSE;

	// xform the ray into terrain space :
	grXForm3d_Transform(&(T->XFWorldToTerrain),pStart,&Start);
	grXForm3d_Rotate(&(T->XFWorldToTerrain),pDirection,&Direction);

	QuadTree_GetExtBox(T->QT,&Box);

	// this isn't reall right, if start is outside & direction points towards the QT
	pStart->X = GR_CLAMP(pStart->X,Box.Min.X,Box.Max.X);
	pStart->Y = GR_CLAMP(pStart->Y,Box.Min.Y,Box.Max.Y);
	pStart->Z = GR_CLAMP(pStart->Z,Box.Min.Z,Box.Max.Z);

	#if 1
	/*****
	This isn't actually right, cuz pStart->Z could be above the terrain, but 
	we need to avoid the asserts in QuadTree_ for now
	*****/
	{
	grVec3d Normal;
		grTerrain_GetNormalAtXY(T,Start.X,Start.Y,&Normal);
		if ( grVec3d_DotProduct(&Normal,&Direction) <= 0.0f )
			return GR_TRUE;
	}
	#endif

	return QuadTree_IntersectRay(T->QT,&Start,&Direction);
}

GRAPI grBoolean GRCC grTerrain_SphereCollision(const grTerrain *T,
													const grVec3d *pFrom, const grVec3d *pTo, grFloat Radius,
													grVec3d *Impact, grPlane *Plane)
{
   grVec3d From,To;
   grFloat FromZ,ToZ,MinRadius;
   grExtBox QTBox;

	assert( grTerrain_IsValid(T) );
	if ( ! T->QT )
		return GR_FALSE;

	// xform the ray into terrain space :
	grXForm3d_Transform(&(T->XFWorldToTerrain),pFrom,&From);
	grXForm3d_Transform(&(T->XFWorldToTerrain),pTo  ,&To  );

	MinRadius = min3(T->CubeSize.X,T->CubeSize.Y,T->CubeSize.Z);
	MinRadius *= 0.5f;
	if ( Radius < MinRadius ) Radius = MinRadius;

	QuadTree_GetExtBox(T->QT,&QTBox);

	// early out:
	if ( ! grExtBox_RayCollision(&QTBox,&From,&To,NULL,NULL) &&
		! grExtBox_ContainsPoint(&QTBox,&From) && ! grExtBox_ContainsPoint(&QTBox,&To)  )
		return GR_FALSE;

	FromZ = grTerrain_GetHeightAtXY(T,From.X,From.Y);
	ToZ   = grTerrain_GetHeightAtXY(T,To.X,To.Y);

	if ( From.Z <= (FromZ + Radius) )
	{
		// collided at start!
		*Impact = From;
		Impact->Z = FromZ;
		goto GotImpact;
	}
	
	if ( ! QuadTree_IntersectThickRay(T->QT,&From,&To,Radius,Impact) )
		return GR_FALSE; // no collision

GotImpact :

	// set up the plane from the Impact
	grTerrain_GetNormalAtXY(T,Impact->X,Impact->Y,&(Plane->Normal));

	// fix Impact and Plane back to world space
	grXForm3d_Transform(&(T->XFTerrainToWorld),Impact,Impact);
	grXForm3d_Rotate(&(T->XFTerrainToWorld),&(Plane->Normal),&(Plane->Normal));

	Plane->Dist = grVec3d_DotProduct( Impact, &(Plane->Normal) );
	Plane->Type = Type_Any;

   return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_BoxCollision(const void *Terrain, const grExtBox *Box, 
													const grVec3d *Front, const grVec3d *Back, 
													grVec3d *Impact, grPlane *Plane)
{
   grFloat Radius;
   grVec3d LocalImpact;	// Added by Icestorm: Impact&Plane CAN be NULL
   grPlane LocalPlane;
   const grTerrain* T = (grTerrain *)Terrain;

	if ( ! Impact )
		Impact=&LocalImpact;
	if ( ! Plane )
		Plane=&LocalPlane;
	if ( ! Box )
		Radius = 0.0f;
	else
		Radius = grVec3d_DistanceBetween( &(Box->Max), &(Box->Min) ) * 0.5f;

   return grTerrain_SphereCollision(T,Front,Back,Radius,Impact,Plane);
}

/*}{******************************************************/
/**********
#if 0 //{

static void MakeExtBoxSilhouette(grVec3d * Silhouette,const grVec3d *POV,const grExtBox *pBox)
{
grVec3d Close,Far;

	// the silhoutte is all 6 points which are neither the nearest nor the farthest

	if ( GR_ABS(POV->X - pBox->Min.X) < GR_ABS(POV->X - pBox->Max.X) )
		{	Close.X = pBox->Min.X; Far.X = pBox->Max.X;	} 	
	else
		{	Close.X = pBox->Max.X; Far.X = pBox->Min.X;	} 	
		
	if ( GR_ABS(POV->Y - pBox->Min.Y) < GR_ABS(POV->Y - pBox->Max.Y) )
		{	Close.Y = pBox->Min.Y; Far.Y = pBox->Max.Y;	} 	
	else
		{	Close.Y = pBox->Max.Y; Far.Y = pBox->Min.Y;	} 	

	if ( GR_ABS(POV->Z - pBox->Min.Z) < GR_ABS(POV->Z - pBox->Max.Z) )
		{	Close.Z = pBox->Min.Z; Far.Z = pBox->Max.Z;	} 	
	else
		{	Close.Z = pBox->Max.Z; Far.Z = pBox->Min.Z;	}

	*Silhouette = Close;
	Silhouette->X = Far.X;
	Silhouette++;

	*Silhouette = Close;
	Silhouette->X = Far.X;
	Silhouette->Y = Far.Y;
	Silhouette++;

	*Silhouette = Close;
	Silhouette->Y = Far.Y;
	Silhouette++;
	
	*Silhouette = Close;
	Silhouette->Y = Far.Y;
	Silhouette->Z = Far.Z;
	Silhouette++;

	*Silhouette = Close;
	Silhouette->Z = Far.Z;
	Silhouette++;

	*Silhouette = Close;
	Silhouette->X = Far.X;
	Silhouette->Z = Far.Z;
	Silhouette++;
}

GRAPI grBoolean GRCC grTerrain_ExtBoxIsVis(const grTerrain *T,const grExtBox *pBox,const grCamera *pCamera)
{
const grVec3d * POV;
grVec3d Silhouette[6];
grFrustum F;
int i;

	assert( grTerrain_IsValid(T) );
	assert( pBox && pCamera );

	POV = grCamera_GetPov(pCamera);

	if ( grExtBox_ContainsPoint(pBox,POV) )
		return GR_TRUE;

	MakeExtBoxSilhouette(Silhouette,POV,pBox);

	grFrustum_SetWorldSpaceFromCamera(&F,pCamera);

	// quick test the bbox vs. frustum

	if ( ! grFrustum_SetClipFlagsFromExtBox(&F,pBox,(1UL<<F.NumPlanes)-1,NULL) )
		return GR_FALSE;

	// could just shoot IntersectRays to the 6 sillhouette verts ?
	//	NO ! in fact the frustum has the same problem :
	//		you'll hit quads *behind* the desired object !

#if 0
	{
	grFrustum_ClipInfo ClipInfo;
	grVec3d Work1[64],Work2[64]

		ClipInfo.ClipFlags = (1UL<<F.NumPlanes)-1;
		ClipInfo.NumSrcVerts = 6;
		ClipInfo.SrcVerts = Silhouette;
		ClipInfo.Work1 = Work1;
		ClipInfo.Work2 = Work2;

		if ( ! grFrustum_ClipVerts(&F,&ClipInfo) )
			return GR_FALSE;
		if ( ClipInfo.NumDstVerts < 3 )
			return GR_FALSE;

		if ( ! grFrustum_SetFromVerts(&F,POV,ClipInfo.DstVerts,ClipInfo.NumDstVerts) )
			return GR_TRUE;

	// this is all very nice, but the QuadTree_InterectFrustum function is a nightmare!
	//	what we really want to know is : is the frustum totally covered ?
	//	but to know that we must build a beamtree or something to add up the partial occlusions !!
	//return QuadTree_IntersectFrustum(T->QT,&F);
	}
#endif

	return GR_TRUE;
}

#endif //}
***************/

/*}{******************************************************/

static grProperty * grProperty_GetOrCreate(grProperty_List * pList,int FieldID)
{
   grProperty * pP;

	pP = grProperty_ListFindByDataId(pList,FieldID);

	if ( ! pP )
	{
	grProperty Dummy;
		Dummy.FieldName = NULL;

		if ( ! grProperty_Append(pList,&Dummy) )
			return NULL;

		pP = pList->pgrProperty + (pList->grPropertyN - 1);
	}

   return pP;
}

grBoolean	GRCC grTerrain_GetPropertyListPtr(grTerrain * T, grProperty_List **ppList)
{
   grProperty * P;

	assert( grTerrain_IsValid(T) );
	assert( ppList );

	if ( ! T->PropertyList )
	{
		T->PropertyList = grProperty_ListCreateEmpty();
		if ( ! T->PropertyList )
		{
			*ppList = NULL;
			return GR_FALSE;
		}
	}

	// set up the properties

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_HEIGHTMAP);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillString(P,"Heightmap",T->HeightmapName,TERRAIN_PROPERTY_HEIGHTMAP);
	}
	P->Data.Ptr = T->HeightmapName;

/*
	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_VERTEXLIGHTING);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillButton(P,"LightVerts",TERRAIN_PROPERTY_VERTEXLIGHTING);
	}

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_TEXTURELIGHTING);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillButton(P,"LightTex",TERRAIN_PROPERTY_TEXTURELIGHTING);
	}

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_NOLIGHTING);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillButton(P,"FullBright",TERRAIN_PROPERTY_NOLIGHTING);
	}
*/

	{
	static const char * LightListStrings[] = { "FullBright", "Vertex", "Texture", "WorldShadowed", NULL };
		P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_LIGHTING_LIST);
		if ( ! P )
			return GR_FALSE;
		if ( ! P->FieldName )
		{
			grProperty_FillCombo(P,"Lighting",(char *)LightListStrings[T->LightListSel],TERRAIN_PROPERTY_LIGHTING_LIST,4,(char **)LightListStrings);
		}
		P->Data.String = (char *)LightListStrings[T->LightListSel];
	}

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_MAXQUADS);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillInt(P, "MaxQuads", T->MaxQuads, TERRAIN_PROPERTY_MAXQUADS, 50, 4000, 25);
	}
	P->Data.Int = T->MaxQuads;
		
	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_MINERROR);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillFloat(P, "MinError", T->MinError, TERRAIN_PROPERTY_MINERROR, 0.001f, 0.01f, 0.001f);
	}
	P->Data.Float = T->MinError;

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_TEXDIMLOG2);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillInt(P, "Texlog2",intlog2(T->TexDim), TERRAIN_PROPERTY_TEXDIMLOG2, 0, MAX_TEXDIM_LOG2, 1);
	}
	P->Data.Int = intlog2(T->TexDim);

#if 0 //{ @@ busted

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_SIZE);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		grProperty_FillVec3dGroup(P,"Size",&(T->Size),TERRAIN_PROPERTY_SIZE);
	}
	P->Data.Vector = T->Size;

#else //}{

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_SIZE_X);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		//grProperty_FillFloat(P, "SizeX", T->Size.X, TERRAIN_PROPERTY_SIZE_X, 50.0f, 4000.0f, 10.0f);
		//CyRiuS
		grProperty_FillFloat(P, "SizeX", T->Size.X, TERRAIN_PROPERTY_SIZE_X, 50.0f, 16384.0f, 10.0f);
	}
	P->Data.Float = T->Size.X;

	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_SIZE_Y);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		//CyRiuS
		grProperty_FillFloat(P, "SizeY", T->Size.Y, TERRAIN_PROPERTY_SIZE_Y, 50.0f, 16384.0f, 10.0f);
	}
	P->Data.Float = T->Size.Y;
	
	P = grProperty_GetOrCreate(T->PropertyList,TERRAIN_PROPERTY_SIZE_Z);
	if ( ! P )
		return GR_FALSE;
	if ( ! P->FieldName )
	{
		//CyRiuS
		grProperty_FillFloat(P, "SizeZ", T->Size.Z, TERRAIN_PROPERTY_SIZE_Z, 50.0f, 16384.0f, 1.0f);
	}
	P->Data.Float = T->Size.Z;
#endif //}

	*ppList = T->PropertyList;

   return GR_TRUE;
}

grBoolean	GRCC grTerrain_GetPropertyListCopy(void * T, grProperty_List **ppList)
{
	grTerrain						*Ter = (grTerrain*)T;

	if ( ! grTerrain_GetPropertyListPtr(Ter,ppList) )
		return GR_FALSE;

	*ppList = grProperty_ListCopy(*ppList);
	if ( ! *ppList )
		return GR_FALSE;

   return GR_TRUE;
}

grBoolean	GRCC grTerrain_GetProperty(void * T, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
   grProperty_List *pList;
   grProperty * pP;
	
	if ( ! grTerrain_GetPropertyListPtr((grTerrain*)T,&pList) )
		return GR_FALSE;

	pP = grProperty_ListFindByDataId(pList,FieldID);

	if ( ! pP )
		return GR_FALSE;
	
	*pData = pP->Data;

   return GR_TRUE;
}

grBoolean	GRCC grTerrain_SetProperty(void * T, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	grTerrain							*Ter = (grTerrain*)T;

	assert( grTerrain_IsValid(Ter) );

	grTerrain_DeSelect(Ter);

	switch( FieldID )
	{
		case TERRAIN_PROPERTY_HEIGHTMAP:
		{
			
		grBitmap * Bmp = NULL;
		grBoolean Ret;
			Bmp = grBitmap_CreateFromFileName(NULL,(char *)pData->Ptr);
			if ( ! Bmp )
				return GR_FALSE;
			Ret = grTerrain_SetHeightmap(Ter,Bmp);
			grBitmap_Destroy(&Bmp);
			strcpy(Ter->HeightmapName,(char *)pData->Ptr);
		return Ret;
		}

/*
		case TERRAIN_PROPERTY_VERTEXLIGHTING:
			return grTerrain_SetLightsOnVertsFromWorld(T,T->World);

		case TERRAIN_PROPERTY_TEXTURELIGHTING:
			return grTerrain_SetLightsInTextureFromWorld(T,T->World);

		case TERRAIN_PROPERTY_NOLIGHTING:
			return grTerrain_SetDefaultLighting(T);
*/

		case TERRAIN_PROPERTY_LIGHTING_LIST:
			
			#define strmatch(str,vs)	( strnicmp(str,vs,strlen(vs)) == 0 )

			if ( strmatch(pData->String,"Full") )
			{
				Ter->LightListSel = 0;

				return grTerrain_SetDefaultLighting(Ter);
			}
			else if ( strmatch(pData->String,"vert") )
			{
				Ter->LightListSel = 1;
				return grTerrain_SetLightsOnVertsFromWorld(Ter,Ter->World);
			}
			else if ( strmatch(pData->String,"tex") )
			{
				Ter->LightListSel = 2;
				return grTerrain_SetLightsInTextureFromWorld(Ter,Ter->World,GR_TRUE,GR_FALSE);
			}
			else if ( strmatch(pData->String,"world") )
			{
				Ter->LightListSel = 3;
				return grTerrain_SetLightsInTextureFromWorld(Ter,Ter->World,GR_TRUE,GR_TRUE);
			}
			else
				return GR_FALSE;

		case TERRAIN_PROPERTY_MAXQUADS:
			if ( DataType != PROPERTY_INT_TYPE )
				return GR_FALSE;
			return grTerrain_SetParameters(Ter, pData->Int, Ter->MinError);

		case TERRAIN_PROPERTY_MINERROR:
			if ( DataType != PROPERTY_FLOAT_TYPE )
				return GR_FALSE;
			return grTerrain_SetParameters(Ter, Ter->MaxQuads, pData->Float);

		case TERRAIN_PROPERTY_TEXDIMLOG2:
			if ( DataType != PROPERTY_INT_TYPE )
				return GR_FALSE;
			if ( pData->Int < 0 || pData->Int > MAX_TEXDIM_LOG2 )
				return GR_FALSE;

			return grTerrain_SetTexDim(Ter, 1 << (pData->Int) );

		case TERRAIN_PROPERTY_SIZE:
			return grTerrain_SetSize(Ter,&(pData->Vector));
		
		case TERRAIN_PROPERTY_SIZE_X:
		{
		grVec3d NewSize;
			NewSize = Ter->Size;
			NewSize.X = pData->Float;
			return grTerrain_SetSize(Ter,&NewSize);
		}
		case TERRAIN_PROPERTY_SIZE_Y:
		{
		grVec3d NewSize;
			NewSize = Ter->Size;
			NewSize.Y = pData->Float;
			return grTerrain_SetSize(Ter,&NewSize);
		}

		case TERRAIN_PROPERTY_SIZE_Z:
		{
		grVec3d NewSize;
			NewSize = Ter->Size;
			NewSize.Z = pData->Float;
			return grTerrain_SetSize(Ter,&NewSize);
		}



		default:
			return GR_FALSE;
	}

   return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_SetXForm(void * Terrain,const grXForm3d *pXF)
{
	grTerrain* T = (grTerrain *)Terrain;
	
	assert( grTerrain_IsValid(T) );

	if ( ! grXForm3d_IsOrthonormal(pXF) )
		return GR_FALSE;

	T->XFTerrainToWorld = *pXF;
	T->Changed = GR_TRUE;

	grXForm3d_GetTranspose(pXF,&(T->XFWorldToTerrain));

   return GR_TRUE;
}

GRAPI grBoolean GRCC grTerrain_GetXForm(const void * Terrain,grXForm3d *pXF)
{
	grTerrain *T = (grTerrain *)Terrain;
	assert( grTerrain_IsValid(T) );
	*pXF = T->XFTerrainToWorld;
   return GR_TRUE;
}

static int GRCC grTerrain_GetXFormModFlags ( const void * Instace )
{
   return	GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE;
}

#pragma message ("Cross link between Terrain and editor sources")
#include "EditMsg.h"
//#include "..\..\tools\Editor\editorsdk\include\EditMsg.h"

static void grTerrain_DeSelect(grTerrain * T)
{
	if ( T->HasSelection )
	{
		T->HasSelection = GR_FALSE;
	}
}

static void grTerrain_Select(grTerrain * T,grVec3d *pWorldVec)
{
   grVec3d V;
	assert( grTerrain_IsValid(T) );
	
	grTerrain_DeSelect(T);

	grXForm3d_Transform(&(T->XFWorldToTerrain),pWorldVec,&V);

	if ( V.X < 0.0f || V.Y < 0.0f || V.X > T->Size.X || V.Y > T->Size.Y )
		return;

	T->SelectionX = V.X;
	T->SelectionY = V.Y;

	grTerrain_GetTextureAtXY(T,V.X,V.Y,NULL,&(T->SelectionTexX),&(T->SelectionTexY));

	T->HasSelection = GR_TRUE;
}

static grBoolean	GRCC grTerrain_SendMessage	(void * T, int32 Msg, void * Data)
{
	grTerrain							*Ter = (grTerrain*)T;

	switch(Msg)
	{

	case JETEDITOR_SELECT3D:
	{
	Select3dContextDef *pContext;
	int OldSelX,OldSelY;

			pContext = (Select3dContextDef *)Data;

			if ( Ter->HasSelection )
			{
				OldSelX = Ter->SelectionTexX;
				OldSelY = Ter->SelectionTexY;
			}

			else
				OldSelX = OldSelY = -1;
			
			grTerrain_Select(Ter,&(pContext->Impact));

			if ( OldSelX == Ter->SelectionTexX && OldSelY == Ter->SelectionTexY )
				grTerrain_DeSelect(Ter);	

		return GR_TRUE;
	}

	case JETEDITOR_APPLYMATERIAL:
	{
		if ( Ter->HasSelection )
		{
			grTerrain_SetATexture(Ter,(grBitmap *)Data,Ter->SelectionTexX,Ter->SelectionTexY);
			return GR_TRUE;
		}

		return GR_FALSE;
	}
	case JETEDITOR_APPLYMATERIALSPEC:
	{
		if ( Ter->HasSelection )
		{
			grBitmap* pBitmap;

			pBitmap = grMaterialSpec_GetLayerBitmap((grMaterialSpec *)Data, 0);
			grTerrain_SetATexture(Ter,pBitmap,Ter->SelectionTexX,Ter->SelectionTexY);
			return GR_TRUE;
		}

		return GR_FALSE;
	}

	default:
		return GR_FALSE; // false means did not do anything with the message!!!!
	}
}

GRAPI grBoolean GRCC grTerrain_AttachEngine(void *T,grEngine *Engine)
{
	grTerrain					*Ter = (grTerrain*)T;

	assert( Engine );
	assert( grTerrain_IsValid((grTerrain*)T) );

	// already the one attached, just ignore the attach request
	if ( Ter->Engine == Engine )
		return GR_TRUE;

	grTerrain_DetachEngine(Ter,Ter->Engine);

	Ter->Engine = Engine;

	if ( Ter->Engine )
	{
	int i;

		grEngine_CreateRef(Ter->Engine);

		grEngine_AddBitmap(Ter->Engine,Ter->NullTexture,GR_ENGINE_BITMAP_TYPE_3D);
		grEngine_AddBitmap(Ter->Engine,Ter->HiliteTexture,GR_ENGINE_BITMAP_TYPE_3D);

		for(i=0;i<(Ter->TexDim * Ter->TexDim);i++)
		{
			if ( Ter->Textures[i] && ! Ter->RegisteredTexture[i] )
			{
				grEngine_AddBitmap(Ter->Engine,Ter->Textures[i],GR_ENGINE_BITMAP_TYPE_3D);
				Ter->RegisteredTexture[i] = GR_TRUE;
			}
		}
	}

   return GR_TRUE;
} 

GRAPI grBoolean GRCC grTerrain_DetachEngine(void *T,grEngine *Engine)
{
	grTerrain						*Ter = (grTerrain*)T;

	assert( grTerrain_IsValid(Ter) );
	//assert( T->Engine == Engine );
	
	if ( Ter->Engine )
	{
		int i;
		grEngine_RemoveBitmap(Ter->Engine,Ter->NullTexture);
		grEngine_RemoveBitmap(Ter->Engine,Ter->HiliteTexture);
		
		for(i=0;i<(Ter->TexDim * Ter->TexDim);i++)
		{
			if ( Ter->RegisteredTexture[i] )
			{
				grEngine_RemoveBitmap(Ter->Engine,Ter->Textures[i]);
				Ter->RegisteredTexture[i] = GR_FALSE;
			}
		}
		
		grEngine_Destroy(&(Ter->Engine));
	}
	
	Ter->Engine = NULL;

   return GR_TRUE;
} 

static grBoolean GRCC grTerrain_AttachWorld(void *T,grWorld *World)
{
	assert( World );
	assert( grTerrain_IsValid((grTerrain*)T) );

	((grTerrain*)T)->World = World;

   return GR_TRUE;
} 

static grBoolean GRCC grTerrain_DetachWorld(void *T,grWorld *World)
{
	assert( World );
	assert( grTerrain_IsValid((grTerrain*)T) );

	((grTerrain*)T)->World = NULL;

   return GR_TRUE;
} 

/*}{******************************************************/

#pragma warning(disable : 4028 4090)
grObjectDef grTerrain_ObjectDef =
{
	GR_OBJECT_TYPE_TERRAIN,
	"Terrain",
	GR_OBJECT_VISRENDER, //flags

	grTerrain_Create,
	grTerrain_CreateRef,
	grTerrain_Destroy,

	grTerrain_AttachWorld,
	grTerrain_DetachWorld,
	grTerrain_AttachEngine,
	grTerrain_DetachEngine,
	NULL, //Attach SoundSystem
	NULL, //Detach SoundSystem

	grTerrain_ObjectRender,
	grTerrain_BoxCollision,
	grTerrain_GetExtBox,

	grTerrain_CreateFromFile,
	grTerrain_WriteToFile,

	grTerrain_GetPropertyListCopy,
	grTerrain_SetProperty,
	grTerrain_GetProperty,

	grTerrain_SetXForm,
	grTerrain_GetXForm,
	grTerrain_GetXFormModFlags,

	NULL,NULL,NULL, // children stuff

	NULL, // editdialog
	grTerrain_SendMessage,
	NULL, // updatetime
	NULL, // duplicate
	NULL,	// ChangeBoxCollision
	NULL,	// GetGlobalPropertyList
	NULL,	// SetGlobalProperty
	grTerrain_SetRenderNextTime,
};
#pragma warning(default : 4028 4090)

GRAPI void		GRCC grTerrain_InitObject(const grTerrain *T,grObject *O)
{
	assert( grTerrain_IsValid(T) );	
	assert( O );
	O->Name = NULL;
	O->Methods = &grTerrain_ObjectDef;
	O->Instance = (void *)T;
	O->RefCnt = 0;
}

GRAPI grBoolean GRCC grTerrain_RegisterObjectDef(void)
{
   return grObject_RegisterGlobalObjectDef( &grTerrain_ObjectDef );
}

/*******************************************************/


// Krouer: interact from the BSP
GRAPI void	GRCC grTerrain_SetRenderNextTime(void* T, grBoolean RenderNext)
{
	((grTerrain*)T)->RenderNextFlag = RenderNext;
}
