/****************************************************************************************/
/*  Bitmap.h                                                                            */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description:  Abstract Bitmap system                                                */
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
#ifndef BITMAP_H
#define BITMAP_H

#include "BaseType.h"
#include "PixelFormat.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

// from vfile.h
typedef	struct grVFile grVFile;

/***********************************************************************************/

typedef struct grBitmap			grBitmap;
typedef struct grBitmap_Palette	grBitmap_Palette;

typedef struct grBitmap_Info
{
	int32					Width;
	int32					Height;
	int32					Stride;		// stride is in *pixels* ; it is the step to the next line : Stride >= Width
	grPixelFormat			Format;
	int32					MinimumMip;	//*including* minimumMip == 0 often
	int32					MaximumMip;	//*including* maximumMip == nummips-1
	grBoolean				HasColorKey;
	uint32					ColorKey;	// meaningless unless HasColorKey ; the ColorKey is a Pixel in Format
	grBitmap_Palette *		Palette;
} grBitmap_Info;

/***********************************************************************************/		
// Bitmap methods

// see a big comment at the end of this file

/************************************************************************/

GRAPI grBitmap *	GRCC	grBitmap_Create(int32 Width, int32 Height, int32 MipCount, grPixelFormat Format ); 
GRAPI void			GRCC	grBitmap_CreateRef(grBitmap *Bmp);
GRAPI grBitmap *	GRCC	grBitmap_CreateCopy(const grBitmap * Src);
GRAPI grBitmap *	GRCC	grBitmap_CreateFromInfo(const grBitmap_Info * pInfo);

GRAPI grBitmap *	GRCC	grBitmap_CreateFromFile( grVFile *F );
GRAPI grBitmap *	GRCC	grBitmap_CreateFromFileName(const grVFile *BaseFS,const char *Name);
GRAPI grBoolean 	GRCC	grBitmap_WriteToFile( const grBitmap *Bmp, grVFile *F );
GRAPI grBoolean	GRCC	grBitmap_WriteToFileName(const grBitmap * Bmp,const grVFile *BaseFS,const char *Name);
										// BaseFS is not really const if it is a virtual file;
										//  it *is* const if it is a dos directory
GRAPI grBitmap *	GRCC	grBitmap_CreateFromFile2(grVFile *VFile,grVFile *ResourceBaseFS,grPtrMgr *PtrMgr);
GRAPI grBoolean	GRCC	grBitmap_WriteToFile2(const grBitmap *Bmp,grVFile *VFile,grPtrMgr *PtrMgr);
GRAPI grBitmap *	GRCC	grBitmap_CreateFromFileName2(const grVFile *BaseFS,const char *Name,grPtrMgr *PtrMgr);
GRAPI grBoolean	GRCC	grBitmap_WriteToFileName2(const grBitmap * Bmp,const grVFile *BaseFS,const char *Name,grPtrMgr *PtrMgr);
										// the '2' functions try to write a minimal bitmap, either using the PtrMgr
										//	or writing a file name; they are NOT compatible with the original File IO funcs

GRAPI grBoolean 	GRCC	grBitmap_Destroy(grBitmap **Bmp);
	// returns whether Bmp was actually destroyed : not success/failure

GRAPI grBoolean	GRCC	grBitmap_GetPersistableName(const grBitmap *Bmp, grVFile ** pBaseFS, char ** pName);

GRAPI grBoolean 	GRCC	grBitmap_GetInfo(const grBitmap *Bmp, grBitmap_Info *Info, grBitmap_Info *SecondaryInfo);
	//LockForWrite returns data in Info's format

GRAPI grBoolean 	GRCC	grBitmap_Blit(const	grBitmap *Src, int32 SrcPositionX, int32 SrcPositionY,
										grBitmap *Dst, int32 DstPositionX, int32 DstPositionY,
										int32 SizeX, int32 SizeY );

GRAPI grBoolean 	GRCC	grBitmap_BlitMip(const grBitmap * Src, int32 SrcMip, grBitmap * Dst, int32 DstMip );
										// don't use this with Src == Dst, use UpdateMips instead !

GRAPI grBoolean 	GRCC	grBitmap_BlitBitmap(const grBitmap * Src, grBitmap * Dst);

GRAPI grBoolean 	GRCC	grBitmap_BlitBestMip(const grBitmap * Src, grBitmap * Dst);
										// blits the largest mip from Src that fits in Dst

GRAPI grBoolean 	GRCC	grBitmap_LockForRead(		// a non-exclusive lock
	const grBitmap *	Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip,
	grPixelFormat 		Format,
	grBoolean			RespectColorKey,
	uint32				ColorKey);
									// not really const, stores lock-count, but *data* is const
									// will do a format conversion!

GRAPI grBoolean	GRCC	grBitmap_LockForReadNative(
	const grBitmap *	Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip);
									// lock for read in a format that gaurantee no conversions
									// then do GetInfo on the locks to see what you have!

GRAPI grBoolean 	GRCC	grBitmap_LockForWrite(	// an exclusive lock
	grBitmap *			Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip);

GRAPI grBoolean 	GRCC	grBitmap_LockForWriteFormat(
	grBitmap *			Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip,
	grPixelFormat 		Format);
									// Format must be one of the two returned in GetInfo !!

GRAPI grBoolean 	GRCC	grBitmap_UnLock(grBitmap *Bmp);	// must be done on All locked mips
GRAPI grBoolean 	GRCC	grBitmap_UnLockArray(grBitmap **Locks, int32 Size);

GRAPI grBoolean 	GRCC	grBitmap_SetFormat(grBitmap *Bmp, 
							grPixelFormat NewFormat, 
							grBoolean RespectColorKey, uint32 ColorKey,
							const grBitmap_Palette * Palette);
	// _SetFormat may cause you to lose color information!
	// SetFormat does a conversion!
	// if NewFormat is palettized and Palette is NULL, we create a palette for the bitmap!

GRAPI grBoolean 	GRCC	grBitmap_SetFormatMin(grBitmap *Bmp,grPixelFormat NewFormat);
								// the Min version keeps colorkey & palette from the old format

GRAPI grBoolean	GRCC	grBitmap_SetColorKey(grBitmap *Bmp, grBoolean HasColorKey, uint32 ColorKey, grBoolean Smart);
	// SetColorKey discards old colorkey information!
	//	does not do a conversion (changes the colorkey in the current data
	// if 'Smart' is on, we don't set HasColorKey to true unless it is actually used!

GRAPI grBoolean	GRCC	grBitmap_GetAverageColor(const grBitmap *Bmp,int32 *pR, int32 *pG, int32 *pB);
	// tells you the average color; computes it and caches it out

GRAPI grBoolean	GRCC	grBitmap_SetCompressionOptions(grBitmap * Bmp, int32 CompressionLevel, grBoolean NeedMips, grFloat ratio);
	// CompressionOptions to be used in any later compression (not written to file)
	// level is 0-10	for 0 = fastest , 10 = most compression
	// NeedMips tells the coder whether to provide for fast mip creation in decompression
	// ratio is the *minimum* compression ratio (In_Len/Out_Len); higher ratios create loss
GRAPI grBoolean	GRCC	grBitmap_SetCompressionOptionsExpert(grBitmap * Bmp,grFloat Ratio,int32 TransformN,int32 CoderN,grBoolean TransposeLHs,grBoolean Block);

GRAPI grBitmap_Palette * 	GRCC	grBitmap_GetPalette(const grBitmap *Bmp);
GRAPI grBoolean			GRCC	grBitmap_SetPalette(grBitmap *Bmp, const grBitmap_Palette *Palette);
	// _SetPal tries to _CreateRef your Palette, so no copy occurs & palettes may be shared
	// you may _Destroy() palette after using it to set (though its bits may not be freed)
	//	(hence Palette is *not* const)
	// Warning : SetPalette on any mip changes the palette of ALL mips !
	// see Palette note at _UnLock
	// _SetPal destroys the bitmap's original palette and refs the new one, 
	//		so if you setpal with the bitmap's palette, there is no net change in ref counts (good!)

GRAPI grBoolean	GRCC	grBitmap_HasAlpha(const grBitmap * Bmp);
	// returns true if bitmap has *any* type of alpha

GRAPI grBitmap *	GRCC	grBitmap_GetAlpha(const grBitmap *Bmp);
GRAPI grBoolean 	GRCC	grBitmap_SetAlpha(grBitmap *Bmp, const grBitmap *AlphaBmp);
	// we Ref the AlphaBmp, so you may destroy it after calling Set()
	// it may be NULL
	// there's only one Alpha per bitmap (for the top Mip) right now

GRAPI grBoolean	GRCC	grBitmap_SetGammaCorrection(grBitmap *Bmp,grFloat Gamma,grBoolean Apply);
	// this Gamma does not change the *original* (system/secondary) bits
	//	it only affects the appearance when drawn
	// note : if you write to the gamma corrected bits, you must gamma correct manually if you
	//	wish to fit in smoothly with the previous data
	// warning : if you use this function with many different gammas, performance will suffer!
	//	use one global gamma for all bitmaps!  try to let the engine manage gamma for you,
	//	via grEngine_SetGamma !

GRAPI grBoolean		GRCC	grBitmap_SetPreferredFormat(grBitmap *Bmp,grPixelFormat Format);
GRAPI grPixelFormat	GRCC	grBitmap_GetPreferredFormat(const grBitmap *Bmp);

GRAPI void *		GRCC	grBitmap_GetBits(grBitmap *Bmp);	// works only on a Lock()

GRAPI grBoolean 	GRCC	grBitmap_RefreshMips(grBitmap *Bmp);	// rebuilds mips; *tries* to be smart & not overwrite manually-fixed mips
												// RefreshMips does *not* build mips that don't exist
GRAPI grBoolean 	GRCC	grBitmap_UpdateMips(grBitmap *Bmp,int32 SourceMip,int32 TargetMip);	
												// will create the target if it doesn't exist;
												// will overwrite manually-fixed mips!
GRAPI grBoolean 	GRCC	grBitmap_SetMipCount(grBitmap *Bmp,int32 Count);
												// creates or destroys to match the new count

GRAPI grBoolean 	GRCC	grBitmap_ClearMips(grBitmap *Bmp);	// Destroy all mips (except the first) !
												// use with care! this is not polite!

// Shortcuts
GRAPI int32		GRCC	grBitmap_Width(const grBitmap *Bitmap);
GRAPI int32		GRCC	grBitmap_Height(const grBitmap *Bitmap);
GRAPI uint32		GRCC	grBitmap_MipBytes(const grBitmap * Bitmap, int32 mip);

/**
*
* if Bitmap is a lock for read, functions that modify it return failure
* if Bitmap is a lock for write, functions that modify it attempt to
*	modify the owner of the lock
*
* warning : if you lock multiple mips for write, and then modify one of the mips
*		(such as via SetPalette) it may affect the owner and all sibling mips!
*		doing different SetPalettes with different palettes on different locked mips 
*		has undefined behavior!
*
**/

#ifdef _DEBUG
GRAPI uint32		GRCC	grBitmap_Debug_GetCount(void);

	// assert this is zero before you shutdown !

#endif

/***********************************************************************************/

typedef enum
{
	GR_BITMAP_STREAMING_ERROR=0,
	GR_BITMAP_STREAMING_NOT,
	GR_BITMAP_STREAMING_STARTED,
	GR_BITMAP_STREAMING_IDLE,
	GR_BITMAP_STREAMING_CHANGED,
	GR_BITMAP_STREAMING_DATADONE,
	GR_BITMAP_STREAMING_DONE,
} grBitmap_StreamingStatus;

GRAPI grBitmap_StreamingStatus GRCC grBitmap_GetStreamingStatus(const grBitmap *Bmp);

		/** on a file which is streaming, the sequence of returns looks like :

			GR_BITMAP_STREAMING_IDLE
			GR_BITMAP_STREAMING_CHANGED
			GR_BITMAP_STREAMING_IDLE
			GR_BITMAP_STREAMING_IDLE
			GR_BITMAP_STREAMING_CHANGED
			...
			GR_BITMAP_STREAMING_DONE
			GR_BITMAP_STREAMING_NOT
			GR_BITMAP_STREAMING_NOT
			GR_BITMAP_STREAMING_NOT
			...

		Status >= GR_BITMAP_STREAMING_STARTED means streaming has started & is in progress

		the user should never see _STARTED or _DATADONE

		***/

/***********************************************************************************/

// palette methods :

GRAPI grBitmap_Palette * 	GRCC	grBitmap_Palette_Create(grPixelFormat Format, int32 Size);

GRAPI grBitmap_Palette * 	GRCC	grBitmap_Palette_CreateCopy(const grBitmap_Palette *Palette);

GRAPI grBitmap_Palette *	GRCC	grBitmap_Palette_CreateFromFile(grVFile *F);

GRAPI grBitmap_Palette *	GRCC	grBitmap_Palette_CreateFromBitmap(grBitmap * Bmp,grBoolean Slow);
												// does GetPalette, and if NULL, then
												// it create an optimal palette for a
												//	non-palettized bitmap
												//	(this is a create, you must destroy later!)
												// put Slow == TRUE for higher quality & slower

GRAPI grBoolean		GRCC	grBitmap_Palette_SortColors(grBitmap_Palette * P,grBoolean Slower);

GRAPI grBoolean		GRCC	grBitmap_Palette_CreateRef(grBitmap_Palette *Palette);

GRAPI grBoolean		GRCC	grBitmap_Palette_Destroy(grBitmap_Palette ** ppPalette);

GRAPI grBoolean		GRCC	grBitmap_Palette_WriteToFile(const grBitmap_Palette *Palette,grVFile *F);

GRAPI grBoolean		GRCC	grBitmap_Palette_SetFormat(grBitmap_Palette * Palette,grPixelFormat Format);

GRAPI grBoolean		GRCC	grBitmap_Palette_Copy(const grBitmap_Palette * Src,grBitmap_Palette * Target);

GRAPI grBoolean		GRCC	grBitmap_Palette_GetInfo(const	grBitmap_Palette *P,grBitmap_Info *Into);
												// get the info as if it were a bitmap; Into->Height == 1

GRAPI grBoolean		GRCC	grBitmap_Palette_Lock(grBitmap_Palette *Palette, void **pBits, grPixelFormat *pFormat, int32 *pSize);
												// pFormat & pSize are optional

GRAPI grBoolean		GRCC	grBitmap_Palette_UnLock(grBitmap_Palette *Palette);
											// palette unlock does NOT notify the bitmap that the palette has changed.
											// call Bitmap_SetPalette() with the same palette pointer 
											// 	to tell the bitmap that it must to some processing
											// (don't worry, it won't duplicate it or copy it onto itself)

GRAPI grBoolean		GRCC	grBitmap_Palette_GetData(const grBitmap_Palette *P,      void *Into,grPixelFormat Format, int32 Colors);
GRAPI grBoolean		GRCC	grBitmap_Palette_SetData(      grBitmap_Palette *P,const void *From,grPixelFormat Format, int32 Colors);
											// does Lock/UnLock for you
											// From and Into are arrays of Colors*grPixelFormat_BytesPerPel bytes

GRAPI grBoolean		GRCC	grBitmap_Palette_SetEntryColor(      grBitmap_Palette *P, int32 Color, int32 R, int32 G, int32 B, int32 A);
GRAPI grBoolean		GRCC	grBitmap_Palette_GetEntryColor(const grBitmap_Palette *P, int32 Color, int32 *R, int32 *G, int32 *B, int32 *A);
											// Set/Get does Lock/Unlock for you ; these are slow! do not use these to work on all the colors!

GRAPI grBoolean		GRCC	grBitmap_Palette_SetEntry(      grBitmap_Palette *P, int32 Color, uint32 Pixel);
GRAPI grBoolean		GRCC	grBitmap_Palette_GetEntry(const grBitmap_Palette *P, int32 Color, uint32 *Pixel);

/***********************************************************************************/

/************************************************************************

A brief tutorial on the Bitmap system, by Charles Bloom, cbloom@eclipsegames.com

The Bitmap is a smart wrapper for complex functionality.  You give it hints to
the opaque Bitmap object, and it tries its best to follow those hints, but it
may not always do so.  The Bitmap is the owner of its bits; you must Lock the
bitmap to get permission to touch those bits, and UnLock to tell the bitmap
you are done.  The format may change between two Locks.  Bitmaps can also be
multiply owned, so you should account for the fact that others may touch your
bitmap between your uses.

The Bitmap contains one or two pixel-sets representing an image.  The "primary" is
a fast-blitting version of the image, and the "secondary" is a storage version
(eventually wavelet compressed) which can be used to rebuild the primary if it is
freed or damaged.  Both cary a generalized format.

Let's do an example.  I want to load a bitmap, set it up for drawing with the
Jet3D Engine, and then blit some interactive stuff into it.

************************************************************************/

#if 0
// {
//-----------------------------------------------------------------------------

void Init(grEngine * Engine);
void Shutdown(void);
void Draw(void);
void DrawPolite(void);

static grBitmap * myBM = NULL;
static grEngine * myEngine = NULL;

void Init(grEngine * Engine)
{
grBoolean success;
grBitmap_Info Info;

	myEngine = Engine;	// this is not looked well upon; for ease of demonstration only!
	assert(Engine);

	myBM = grBitmap_CreateFromFileName(NULL,"mybitmap.bmp");

	// CreateFromFile can load windows BMP files, or custom GeBm files.

	assert(myBM);

	// get the main info; I don't care about the secondary, so leave it NULL

	success = grBitmap_GetInfo(myBM,&Info,NULL);
	assert(success);

	// make sure I loaded a bitmap in the format I understand !

	if ( Info.Format == GR_PIXELFORMAT_8BIT_PAL )
	{
		// I want palette index 255 to act as transparency, so I must use SetColorKey

		success = grBitmap_SetColorKey(myBM,GR_TRUE,255);
		assert(success);

		// just for fun, let's modify the palette:
		if (1)
		{
		grBitmap_Palette * Pal;

			// get the palette ; I don't care if its primary or secondary, so
			/// I don't use the Info.Palette field

			Pal = grBitmap_GetPalette(myBM);
			assert(Pal);

			// I'm only fiddling one entry, so don't bother with a full Lock() UnLock()
			//  sequence on the palette

			// make palette index zero bright red; we use alpha = 255 for opaque

			success = grBitmap_Palette_SetEntryColor(Pal,0,255,0,0,255);
			assert(success);

			// tell the bitmap system you've changed the palette; this function
			//  is smart enough to not do unecessary copies or whatever.

			success = grBitmap_SetPalette(myBM,Pal);
			assert(success);
		}

	}
	else
	{
		// otherwise, treat black as transparent, in whatever format I have

		success = grBitmap_SetColorKey(myBM,GR_TRUE,grPixelFormat_ComposePixel(Info.Format,0,0,0,0));
		assert(success);
	}	

	// note that I did NOT use SetFormat.  SetFormat may do a conversion, and since the original
	//	bitmap was created without colorkey, it would have been converted to a new format but
	//	kept its property of having no colorkey!
	// (SetFormat will fiddle the bits and whatever way necessary to keep bitmaps as visually similar
	//		as possible)

	// I want to fiddle the fast format in 565 later, so cue the bitmap to try to give me that format.

	success = grBitmap_SetPreferredFormat(myBM,GR_PIXELFORMAT_16BIT_565_RGB);
	assert(success);

	// Add it to the engine so it can be used for drawing.

	success = grEngine_AddBitmap(myEngine,myBM);
	assert(success);
}

void Shutdown(void)
{
grBoolean WasDestroyed;

	assert(myBM);
	
	// clean up

	grEngine_RemoveBitmap(myEngine,myBM);

	WasDestroyed = grBitmap_Destroy(&myBM);

	// someone else might have done _CreateRef on our bitmap,
	//  so we can't be sure it's actually destroyed.
	// this code is still ready to be run again with a new call to Init()

	//assert(WasDestroyed);

	myBM = NULL;
	myEngine = NULL;
}

void Draw(void)
{
grBitmap * Lock;
grBoolean success;
grBitmap_Info Info;
uint16 *bits,*bptr;
int x,y;

	// lets fiddle the bits.
	// we need to lock the bitmap for write.
	//	LockForWrite is an exclusive lock, unlike LockForRead which is non-blocking
	// request our favorite format, and only lock Mip 0 (the full size bitmap)

	success = grBitmap_LockForWriteFormat(myBM,&Lock,0,0,GR_PIXELFORMAT_16BIT_565_RGB);
	if ( ! success )
	{
		// well, we tried to be nice; if we were very polite, we would do a LockForWrite
		// here, and try to fiddle the bits in whatever format we got; However, we aren't
		// that polite, so we just do a _SetFormat
		//
		// note that we are destroying the original bitmap by changing its format
		// we should only do this if we are going to draw into the bitmap

		success = grBitmap_SetFormat(myBM,GR_PIXELFORMAT_16BIT_565_RGB,GR_TRUE,0,NULL);
		assert(success);

		// now we should be able to get the bits we want, *but* they may not be the
		// primary (fast) format; oh well, it's the best we can do...
		// (if you must have the fastest bits, then use only _LockForWrite, never LockForWriteFormat,
		// which might have to do a conversion)

		success = grBitmap_LockForWriteFormat(myBM,&Lock,0,0,GR_PIXELFORMAT_16BIT_565_RGB);
		assert(success);
	}

	// now Lock is our bitmap in 565
	// we do a GetInfo because the Lock's info could be different than
	//	the original bitmap's (particularly the Palette & the Stride)

	success = grBitmap_GetInfo(Lock,&Info,NULL);
	assert(success);

	// you can only call _GetBits on a locked bitmap

	bits = grBitmap_GetBits(Lock);
	assert( bits );

	bptr = bits;
	for(y=0; y < Info.Height; y++)
	{
		for(x=0; x < Info.Width; x++)
		{
		uint16 R,G,B;
			// make a silly 565 gradient
			R = x & 0x1F;
			G = x & 0x3F;
			B = y & 0x1F;

			*bptr++ = (R<<11) + (G<<5) + B;
		}

		// note that bptr is a word pointer, and Stride is in pixels :

		bptr += Info.Stride -  Info.Width;
	}
	bits = bptr = NULL;

	// you call Unlock on all the mips you locked - not on the original bitmap!

	success = grBitmap_UnLock(Lock);
	assert(success);

	// now, we only fiddled the full-size Mip, and there might be more,
	//  so lets percolate the changes into the smaller mips:

	success = grBitmap_RefreshMips(myBM);
	assert(success);

	// a null rect means use the whole bitmap;
	// Engine_DrawBitmap blits a 2d decal to the framebuffer (fast)

	success = grEngine_DrawBitmap(myEngine,myBM,NULL,0,0);
	assert(success);

}

void DrawPolite(void)
{
grBitmap * Lock;
grBoolean success;
grBitmap_Info Info;
void *bits;
int x,y;

	// this function does the same thing as Draw() , but is more polite
	// lock in the fastest format (whatever it is)
	// because we did SetPreferred, this should be 565_RGB, but might not be

	success = grBitmap_LockForWrite(myBM,&Lock,0,0);
	assert(success);

	success = grBitmap_GetInfo(Lock,&Info,NULL);
	assert(success);

	bits = grBitmap_GetBits(Lock);
	assert( bits );

	if ( Info.Format == GR_PIXELFORMAT_16BIT_565_RGB )
	{
	uint16 *wptr;

		// our favorite format

		wptr = bits;
		for(y=0; y < Info.Height; y++)
		{
			for(x=0; x < Info.Width; x++)
			{
			uint16 R,G,B;
				// make a silly 565 gradient
				R = x & 0x1F;
				G = x & 0x3F;
				B = y & 0x1F;

				*wptr++ = (R<<11) + (G<<5) + B;
			}
			wptr += Info.Stride -  Info.Width;
		}
	}
	else
	{
	uint8 * bptr;

		// oh well, do our best
		// bitmaps must have had a good reason to not give us the format we preferred,

		bptr = bits;
		for(y=0; y < Info.Height; y++)
		{
			for(x=0; x < Info.Width; x++)
			{
			uint32 R,G,B;

				// put a color in any format

				R = (x & 0x1F)<<3;
				G = (x & 0x3F)<<2;
				B = (y & 0x1F)<<3;

				// we use alpha of 255 for opaque

				grPixelFormat_PutColor(Info.Format,&bptr,R,G,B,255);
			}

			bptr += (Info.Stride -  Info.Width) * grPixelFormat_BytesPerPel(Info.Format);
		}
	}
	bits = NULL;

	// same as before:

	success = grBitmap_UnLock(Lock);
	assert(success);

	success = grBitmap_RefreshMips(myBM);
	assert(success);

	success = grEngine_DrawBitmap(myEngine,myBM,NULL,0,0);
	assert(success);

}

// end tutorial on the Bitmap system
//-----------------------------------------------------------------------------
// }

/***********************************************************************************/

#endif
#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases
typedef struct grBitmap_Info grBitmap_Info;


#endif
