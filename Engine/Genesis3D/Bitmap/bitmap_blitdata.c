/****************************************************************************************/
/*  Bitmap_BlitData.c                                                                   */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description:  The Bitmap_BlitData function                                          */
/*					Does all format conversions											*/
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
#include	<math.h>
#include	<time.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdlib.h>
#include	<string.h>

#include	"BaseType.h"
#include	"grTypes.h"
#include	"Ram.h"

#include	"Bitmap.h"
#include	"Bitmap._h"
#include	"Bitmap.__h"
#include	"bitmap_blitdata.h"

#include	"VFile.h"
#include	"Errorlog.h"

#include	"Wavelet.h"
#include	"palcreate.h"
#include	"palettize.h"

#include	"Tsc.h"

#ifdef DO_TIMER
#include	"Timer.h"
#endif

#ifdef BUILD_BE
#define DONT_USE_ASM 1
#endif

#define DONT_USE_ASM

/*}{*********************************************************************/

// parameters to the main BlitData call are set up in here & shared
//  with all the children functions

// this may actually be better than being thread-friendly
// because we prevent cache-thrashing

#pragma message("Bitmap BlitData : holding a semaphore on static variables")
#pragma warning (disable : 4731)

extern grThreadQueue_Semaphore * Bitmap_BlitData_Lock;

static int SrcXtra,DstXtra;
static int SrcPelBytes,DstPelBytes;
static int SrcRowBytes,DstRowBytes;
static int SrcXtraBytes,DstXtraBytes;
static const grPixelFormat_Operations *SrcOps,*DstOps;
static grPixelFormat SrcFormat,DstFormat;

static const grBitmap_Info * SrcInfo;
static		 grBitmap_Info * DstInfo;
static const void *SrcData;
static		 void *DstData;
static const grBitmap *SrcBmp;
static		 grBitmap *DstBmp;
static const grBitmap_Palette *SrcPal;
static		 grBitmap_Palette *DstPal;
static int SizeX,SizeY;

static grPixelFormat_Decomposer		SrcDecomposePixel;
static grPixelFormat_Composer		DstComposePixel;
static grPixelFormat_ColorGetter	SrcGetColor;
static grPixelFormat_ColorPutter	DstPutColor;
static grPixelFormat_PixelGetter	SrcGetPixel;
static grPixelFormat_PixelPutter	DstPutPixel;

/*}{*********************************************************************/

grBoolean BlitData_Raw(void);
grBoolean BlitData_SameFormat(void);
grBoolean BlitData_Palettize(void);
grBoolean BlitData_DePalettize(void);
grBoolean BlitData_FromSeparateAlpha(void);
grBoolean BlitData_ToSeparateAlpha(void);
grBoolean BlitData_Wavelet_Compress(void);
grBoolean BlitData_Wavelet_DeCompress(void);

grBoolean grBitmap_BlitData_Sub(	const grBitmap_Info * iSrcInfo,const void *iSrcData, const grBitmap *iSrcBmp,
								grBitmap_Info * iDstInfo,void *iDstData,	const grBitmap *iDstBmp,
								int iSizeX,int iSizeY)
{

	// warming up...

	SrcInfo = iSrcInfo;
	DstInfo = iDstInfo;
	SrcData = iSrcData;
	DstData = iDstData;
	SrcBmp  = iSrcBmp;
	DstBmp  = (grBitmap *)iDstBmp;
	SizeX	= iSizeX;
	SizeY	= iSizeY;

	assert(SrcInfo && SrcData && DstInfo && DstData);

	// SrcData & DstData may be the same!
	// SrcBmp  & DstBmp  may be NULL !

	if ( SizeX > SrcInfo->Width || SizeX > DstInfo->Width ||
		 SizeY > SrcInfo->Height|| SizeY > DstInfo->Height)
	{
		grErrorLog_AddString(-1,"Bitmap_BlitData : size mismatch", NULL);	
		return GR_FALSE;
	}

	SrcFormat = SrcInfo->Format;
	DstFormat = DstInfo->Format;

	SrcOps = grPixelFormat_GetOperations(SrcFormat);
	DstOps = grPixelFormat_GetOperations(DstFormat);

	if ( ! SrcOps || ! DstOps )
		return GR_FALSE;
		
	SrcPelBytes = SrcOps->BytesPerPel;
	DstPelBytes = DstOps->BytesPerPel;

	SrcRowBytes = SrcPelBytes * SrcInfo->Stride;
	DstRowBytes = DstPelBytes * DstInfo->Stride;

	SrcXtra = SrcInfo->Stride - SizeX;
	DstXtra = DstInfo->Stride - SizeX;

	SrcXtraBytes = SrcXtra * SrcPelBytes;
	DstXtraBytes = DstXtra * DstPelBytes;

	DstComposePixel		= DstOps->ComposePixel;
	DstPutColor			= DstOps->PutColor;
	DstPutPixel			= DstOps->PutPixel;
	SrcDecomposePixel	= SrcOps->DecomposePixel;
	SrcGetColor			= SrcOps->GetColor;
	SrcGetPixel			= SrcOps->GetPixel;

	SrcPal = SrcInfo->Palette;
	if ( ! SrcPal && SrcBmp )
		SrcPal = grBitmap_GetPalette(SrcBmp);
	DstPal = DstInfo->Palette;
	if ( ! DstPal && DstBmp )
		DstPal = grBitmap_GetPalette(DstBmp);

	// all systems go!

	/** copy the palette **/

	if ( grPixelFormat_HasPalette(SrcFormat) && ! SrcPal )
	{
		grErrorLog_AddString(-1, "grBitmap_BlitData:  Palettized format, with no palette.", NULL);
		return GR_FALSE;
	}

	if ( SrcPal && grPixelFormat_HasPalette(DstFormat) )
	{
		if ( ! DstInfo->Palette )
		{
		grPixelFormat Format;
			Format = SrcPal->Format;
			if ( ! grPixelFormat_HasAlpha(Format) )
			{
				if ( SrcInfo->HasColorKey && ! DstInfo->HasColorKey )
					Format = GR_PIXELFORMAT_32BIT_ARGB;
			}
			grBitmap_AllocPalette(DstBmp,Format,DstBmp->Driver);
			if ( ! DstInfo->Palette )
				DstInfo->Palette = grBitmap_GetPalette(DstBmp);
		}
		DstPal = DstInfo->Palette;
		if ( ! DstPal )
		{
			grErrorLog_AddString(-1, "grBitmap_BlitData:  couldn't alloc new dest palette.", NULL);
			return GR_FALSE;
		}

		if ( ! grBitmap_Palette_Copy(SrcPal,DstPal) )
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : Palette_Copy failed", NULL);
			return GR_FALSE;
		}
		
		if ( SrcInfo->HasColorKey )
		{
			if ( ! grBitmap_Palette_SetEntryColor(DstInfo->Palette,SrcInfo->ColorKey,0,0,0,0) )
			{
				return GR_FALSE;
			}
		}
	}

	/****
	**
	*
		modes:
			0. types are same
			1. Wavelet <-> raw
			2. Pal -> raw (easy)	(now raw means "not wavelet or pal")
			3. raw -> Pal (hard)
			4. raw <-> raw (just bit-ops)

		each of these also exists for separate alpha types
	*
	**
	 ****/

	/****/
	
	if (	SrcBmp && SrcBmp->Alpha && SrcBmp->Alpha->LockOwner && 
		DstBmp && DstBmp->Alpha && DstBmp->Alpha->LockOwner )
	{
		if ( ! grBitmap_BlitBitmap(SrcBmp->Alpha,DstBmp->Alpha) )
			return GR_FALSE;
		// now continue through to blit the main bitmap
	}
	else if ( SrcBmp && SrcBmp->Alpha && SrcBmp->Alpha->LockOwner && 
			( grPixelFormat_HasAlpha(DstFormat) || DstInfo->HasColorKey ) )
	{
		// there is no separate alpha -> color key conversion
		// note that we cannot add separate alpha -> CK because the 
		// separate-alpha *target* type has colorkey too !
		return BlitData_FromSeparateAlpha();
	}
	else if ( DstBmp && DstBmp->Alpha && DstBmp->Alpha->LockOwner && 
			grPixelFormat_HasAlpha(SrcFormat) )
	{
		// there is no separate alpha -> color key conversion
		// note that we cannot add separate alpha -> CK because the 
		// separate-alpha *target* type has colorkey too !
		return BlitData_ToSeparateAlpha();
	}

	/****/

	if (	SrcFormat == GR_PIXELFORMAT_WAVELET ||
			DstFormat == GR_PIXELFORMAT_WAVELET )
	{
		if (	SrcFormat == GR_PIXELFORMAT_WAVELET &&
				DstFormat == GR_PIXELFORMAT_WAVELET )
		{
			// or just memcpy
			grErrorLog_AddString(-1,"Bitmap_BlitData : no wavelet->wavelet blits yet", NULL);	
			return GR_FALSE;
		}

		if ( SizeX != SrcInfo->Width || SizeX != DstInfo->Width ||
			 SizeY != SrcInfo->Height || SizeY != DstInfo->Height )
		{
			// no partial blits right now for wavelets
			grErrorLog_AddString(-1,"Bitmap_BlitData : no partial wavelet blits yet", NULL);
			return GR_FALSE;
		}

		if ( SrcFormat == GR_PIXELFORMAT_WAVELET )
		{
			return BlitData_Wavelet_DeCompress();	
		}
		else
		{
			return BlitData_Wavelet_Compress();	
		}
	}
	else if ( SrcFormat == DstFormat )
	{
		return BlitData_SameFormat();
	}
	else if (	grPixelFormat_HasPalette(SrcFormat) ||
				grPixelFormat_HasPalette(DstFormat) )
	{
		assert(SrcFormat != DstFormat);
		if (	grPixelFormat_HasPalette(SrcFormat) &&
				grPixelFormat_HasPalette(DstFormat) )
			return GR_FALSE;	// already picked up by SameFormat , or two different palettized = abort!

		if ( grPixelFormat_HasPalette(SrcFormat) )
		{
			return BlitData_DePalettize();
		}
		else
		{
			if ( ! DstInfo->Palette )
			{
				// make it
				if ( DstBmp && DstBmp->Driver )
				{
				grPixelFormat Format;
					Format = SrcInfo->Palette ? SrcInfo->Palette->Format : GR_PIXELFORMAT_32BIT_XRGB;
					if ( ! grPixelFormat_HasAlpha(Format) )
					{
						if ( SrcInfo->HasColorKey && ! DstInfo->HasColorKey )
							Format = GR_PIXELFORMAT_32BIT_ARGB;
					}
					grBitmap_AllocPalette(DstBmp,Format,DstBmp->Driver);
					if ( ! DstInfo->Palette )
						DstInfo->Palette = grBitmap_GetPalette(DstBmp);
				}
				else
				{
					DstInfo->Palette = grBitmap_Palette_Create(PALETTE_FORMAT_DEFAULT,256);
				}
				if ( ! DstInfo->Palette )
				{
					grErrorLog_AddString(-1,"Bitmap_BlitData : Pal create failed", NULL);	
					return GR_FALSE;
				}

				if ( SrcPal )
				{
					grBitmap_Palette_Copy(SrcPal,DstInfo->Palette);
				}
				else if ( DstPal )
				{
					grBitmap_Palette_Copy(DstPal,DstInfo->Palette);
				}
				else // Nobody had a palette !
				{
				grBitmap_Palette * NewPal;
				grBitmap_Info Info;
					Info = *SrcInfo;
					Info.Width = SizeX;
					Info.Height = SizeY;
					NewPal = createPalette(&Info,SrcData);
					if ( ! NewPal )
					{
						grErrorLog_AddString(-1,"Bitmap_BlitData : createPalette failed", NULL);	
						return GR_FALSE;
					}
					grBitmap_Palette_Copy(NewPal,DstInfo->Palette);
					if ( SrcBmp && ((SizeX*SizeY) > ((SrcInfo->Width * SrcInfo->Height)>>2)) )
					{
						grBitmap_SetPalette((grBitmap *)SrcBmp,NewPal);
					}
					grBitmap_Palette_Destroy(&NewPal);
				}

				DstPal = DstInfo->Palette;

				SrcPal = SrcInfo->Palette;
				if ( ! SrcPal && SrcBmp )
					SrcPal = grBitmap_GetPalette(SrcBmp);
			}

			return BlitData_Palettize();
		}
	}
	else
	{
		return BlitData_Raw();
	}

	assert(0);
	// must have returned before here
	//return GR_FALSE;
}

grBoolean grBitmap_BlitData(	const grBitmap_Info * iSrcInfo,const void *iSrcData, const grBitmap *iSrcBmp,
								grBitmap_Info * iDstInfo,void *iDstData,	const grBitmap *iDstBmp,
								int iSizeX,int iSizeY)
{
grBoolean Ret;
	assert(Bitmap_BlitData_Lock);
	grThreadQueue_Semaphore_Lock(Bitmap_BlitData_Lock);
	Ret = grBitmap_BlitData_Sub(	
							iSrcInfo,iSrcData,iSrcBmp,
							iDstInfo,iDstData,iDstBmp,	
							iSizeX,iSizeY);
	grThreadQueue_Semaphore_UnLock(Bitmap_BlitData_Lock);
return Ret;
}

/*}{*********************************************************************/

grBoolean BlitData_Raw(void)
{
int x,y;
char *SrcPtr,*DstPtr;
int R,G,B,A;
uint32 ColorKey,Pixel;

	if ( ! SrcOps || ! DstOps )
		return GR_FALSE;

	SrcPtr = (char *)SrcData;
	DstPtr = (char *)DstData;

	// this generic converter is pretty damned slow.
	// fortunately Jet3D uses mostly the (Pal -> UnPal) conversion
	// or the (Wavelet -> UnPal) conversion, so screw this

	if ( SrcPelBytes == 0 || DstPelBytes == 0 ) 
	{
		grErrorLog_AddString(-1,"Bitmap_BlitData : invalid format", NULL);
		return GR_FALSE;
	}
	else if ( SrcOps->AMask && ! (DstOps->AMask) && DstInfo->HasColorKey )
	{
		ColorKey = DstInfo->ColorKey;

		// special case for "Src has alpha & Dst doesn't, but has ColorKey"

		for(y=SizeY;y--;)
		{
			for(x=SizeX;x--;)
			{
				SrcGetColor((uint8 **)&SrcPtr,&R,&G,&B,&A);
				if ( A < ALPHA_TO_TRANSPARENCY_THRESHOLD )
				{
					Pixel = ColorKey;
				}
				else
				{
					Pixel = DstComposePixel(R,G,B,A);
					if ( Pixel == ColorKey )
					{
						Pixel ^= 1;
					}
				}
				DstPutPixel((uint8**)&DstPtr,Pixel);
			}
			SrcPtr += SrcXtraBytes;
			DstPtr += DstXtraBytes;
		}

	return GR_TRUE;
	}
	else if ( SrcInfo->HasColorKey && DstInfo->HasColorKey )
	{
	uint32 DstColorKey;

		ColorKey = SrcInfo->ColorKey;
		DstColorKey = DstInfo->ColorKey;

		for(y=SizeY;y--;)
		{
			for(x=SizeX;x--;)
			{
				Pixel = SrcGetPixel((uint8**)&SrcPtr);
				if ( Pixel == ColorKey )
				{
					DstPutPixel((uint8**)&DstPtr,DstColorKey);
				}
				else
				{
					SrcDecomposePixel(Pixel,&R,&G,&B,&A);
					Pixel = DstComposePixel(R,G,B,A);
					if ( Pixel == DstColorKey )
						Pixel ^= 1;
					DstPutPixel((uint8**)&DstPtr,Pixel);
				}
			}
			SrcPtr += SrcXtraBytes;
			DstPtr += DstXtraBytes;
		}

	return GR_TRUE;
	}
	else if ( DstInfo->HasColorKey )
	{
		ColorKey = DstInfo->ColorKey;

		for(y=SizeY;y--;)
		{
			for(x=SizeX;x--;)
			{
				SrcGetColor((uint8**)&SrcPtr,&R,&G,&B,&A);
				Pixel = DstComposePixel(R,G,B,A);
				if ( Pixel == ColorKey )
				{
					Pixel ^= 1;
				}
				DstPutPixel((uint8**)&DstPtr,Pixel);
			}
			SrcPtr += SrcXtraBytes;
			DstPtr += DstXtraBytes;
		}

	return GR_TRUE;
	}
	else if ( SrcInfo->HasColorKey )
	{
		// generic converter does the cases we don't understand

		ColorKey = SrcInfo->ColorKey;

		for(y=SizeY;y--;)
		{
			for(x=SizeX;x--;)
			{
				Pixel = SrcGetPixel((uint8**)&SrcPtr);
				if ( Pixel == ColorKey )
				{
					DstPutColor((uint8**)&DstPtr,0,0,0,0);
				}
				else
				{
					SrcDecomposePixel(Pixel,&R,&G,&B,&A);
					DstPutColor((uint8**)&DstPtr,R,G,B,A);
				}
			}
			SrcPtr += SrcXtraBytes;
			DstPtr += DstXtraBytes;
		}

	return GR_TRUE;
	}
	else
	{
		for(y=SizeY;y--;)
		{
			for(x=SizeX;x--;)
			{
				SrcGetColor((uint8**)&SrcPtr,&R,&G,&B,&A);
				DstPutColor((uint8**)&DstPtr,R,G,B,A);
			}
			SrcPtr += SrcXtraBytes;
			DstPtr += DstXtraBytes;
		}

	return GR_TRUE;
	}
}

/*}{*********************************************************************/

grBoolean BlitData_FromSeparateAlpha(void)
{
grBitmap_Info AlphaInfo;
void * AlphaData;
uint8 *SrcPtr,*DstPtr,*AlphaPtr;
int x,y,R,G,B,A;
uint32 ColorKey,Pixel;
int AlphaXtra;

	/*******
	**
		support the extra Alpha Bmp
		the common case is 8bit + 8bit -> 4444

		we're pretty lazy about this; it's not optimized for speed

	**
	 ******/

	#pragma message("Bitmap_BlitData: inconsistent handling of separates with color keys!")

	SrcPtr = (uint8 *)SrcData;
	DstPtr = (uint8 *)DstData;

	if ( ! grBitmap_GetInfo(SrcBmp->Alpha,&AlphaInfo,NULL) )
		return GR_FALSE;
	if ( AlphaInfo.Format != GR_PIXELFORMAT_8BIT_GRAY )
	{
		grErrorLog_AddString(-1,"Bitmap_BlitData : Alpha must be grayscale", NULL);
		return GR_FALSE;
	}

	AlphaData = grBitmap_GetBits(SrcBmp->Alpha);
	if ( ! AlphaData )
		return GR_FALSE;

	AlphaPtr = (uint8 *)AlphaData;
	AlphaXtra = AlphaInfo.Stride - SizeX;

	if ( grPixelFormat_HasPalette(SrcFormat) )
	{
		if ( SrcFormat == DstFormat )
		{
		uint8 Pixel,DstColorKey;

			assert(DstInfo->HasColorKey);
			
			DstColorKey = (uint8)DstInfo->ColorKey;
			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					Pixel = *SrcPtr++;
					A = *AlphaPtr++;
					if ( A < ALPHA_TO_TRANSPARENCY_THRESHOLD )
						*DstPtr++ = DstColorKey;
					else
						*DstPtr++ = Pixel;
				}
				SrcPtr += SrcXtra;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}
			return GR_TRUE;
		}
		else if ( SrcFormat == GR_PIXELFORMAT_8BIT )
		{
		uint8 *PalPtr,PalData[768];
		int pal;

			if ( ! grBitmap_Palette_GetData(SrcPal,PalData,GR_PIXELFORMAT_24BIT_RGB,256) )
				return GR_FALSE;

			// with seperate alpha

			if ( ! grPixelFormat_HasAlpha(DstFormat) && DstInfo->HasColorKey )
			{
			uint32 Pixel,DstColorKey;
				// source is palettized
				// dest has color key and no alpha
				DstColorKey = DstInfo->ColorKey;
				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						pal = *SrcPtr++;
						PalPtr = &PalData[3*pal];
						R = *PalPtr++;
						G = *PalPtr++;
						B = *PalPtr;
						A = *AlphaPtr++;
						if ( A < 128 )
						{
							DstPutPixel(&DstPtr,DstColorKey);
						}
						else
						{
							Pixel = DstComposePixel(R,G,B,255);
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							DstPutPixel(&DstPtr,Pixel);
						}
					}
					SrcPtr += SrcXtra;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}
			}
			else if ( DstInfo->HasColorKey )
			{
			uint32 Pixel,DstColorKey;
				// source is palettized
				// dest has alpha and color key
				DstColorKey = DstInfo->ColorKey;
				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						pal = *SrcPtr++;
						PalPtr = &PalData[3*pal];
						R = *PalPtr++;
						G = *PalPtr++;
						B = *PalPtr;
						A = *AlphaPtr++;
						Pixel = DstComposePixel(R,G,B,A);
						if ( Pixel == DstColorKey )
							Pixel ^= 1;
						DstPutPixel(&DstPtr,Pixel);
					}
					SrcPtr += SrcXtra;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}
			}
			else
			{
				// source is palettized
				// dest has alpha and no color key
				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						pal = *SrcPtr++;
						PalPtr = &PalData[3*pal];
						R = *PalPtr++;
						G = *PalPtr++;
						B = *PalPtr;
						A = *AlphaPtr++;
						DstPutColor(&DstPtr,R,G,B,A);
					}
					SrcPtr += SrcXtra;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}
			}

			return GR_TRUE;
		}
		else
		{
			return GR_FALSE;
		}
		assert("should not get here" == NULL);
	}
	else
	{
		// this generic converter is pretty damned slow.
		// fortunately Jet3D uses mostly the (Pal -> UnPal) conversion
		// or the (Wavelet -> UnPal) conversion, so screw this

		// Src is not palettized

		assert( ! SrcOps->AMask );
		assert( DstOps->AMask || DstInfo->HasColorKey );

		// <> doesn't do -> palettize
		//	should never get a (separates)->(palettized) with current driver, but bad to assume...
		// perhaps the best thing is to do separates -> 32bitRGBA then do 32bitRGBA -> Dest with the normal converters

		if ( grPixelFormat_HasPalette(DstFormat) )
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : FromSeparateAlpha : doesn't do Palettize!", NULL);
			return GR_FALSE;
		}

		if ( SrcPelBytes == 0 ) 
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : FromSeparateAlpha : bad Src format", NULL);
			return GR_FALSE;
		}
		else if ( DstPelBytes == 0 || ! DstPutColor || ! DstComposePixel ) 
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : FromSeparateAlpha : bad Dst format", NULL);
			return GR_FALSE;
		}


		if ( DstOps->AMask )
		{

			//separates -> alpha

			if ( SrcInfo->HasColorKey && DstInfo->HasColorKey )
			{
			uint32 DstColorKey;

				ColorKey	= SrcInfo->ColorKey;
				DstColorKey	= DstInfo->ColorKey;

				// with seperate alpha

				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						Pixel = SrcGetPixel(&SrcPtr);
						if ( Pixel == ColorKey )
						{
							AlphaPtr++;
							DstPutPixel(&DstPtr,DstColorKey);
						}
						else
						{
							SrcDecomposePixel(Pixel,&R,&G,&B,&A);
							A = *AlphaPtr++;
							Pixel = DstComposePixel(R,G,B,A);
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							DstPutPixel(&DstPtr,Pixel);
						}
					}
					SrcPtr += SrcXtraBytes;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}

			return GR_TRUE;
			}
			else if ( DstInfo->HasColorKey )
			{
				ColorKey = DstInfo->ColorKey;

				// with seperate alpha

				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						SrcGetColor(&SrcPtr,&R,&G,&B,&A);
						A = *AlphaPtr++;
						Pixel = DstComposePixel(R,G,B,A);
						if ( Pixel == ColorKey )
						{
							Pixel ^= 1;
						}
						DstPutPixel(&DstPtr,Pixel);
					}
					SrcPtr += SrcXtraBytes;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}

			return GR_TRUE;
			}
			else if ( SrcInfo->HasColorKey )
			{
				// with seperate alpha

				ColorKey = SrcInfo->ColorKey;

				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						Pixel = SrcGetPixel(&SrcPtr);
						if ( Pixel == ColorKey )
						{
							AlphaPtr++;
							DstPutColor(&DstPtr,0,0,0,0);
						}
						else
						{
							SrcDecomposePixel(Pixel,&R,&G,&B,&A);
							A = *AlphaPtr++;
							DstPutColor(&DstPtr,R,G,B,A);
						}
					}
					SrcPtr += SrcXtraBytes;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}

			return GR_TRUE;
			}
			else
			{
				// with seperate alpha
				for(y=SizeY;y--;)
				{
					for(x=SizeX;x--;)
					{
						SrcGetColor(&SrcPtr,&R,&G,&B,&A);
						A = *AlphaPtr++;
						DstPutColor(&DstPtr,R,G,B,A);
					}
					SrcPtr += SrcXtraBytes;
					DstPtr += DstXtraBytes;
					AlphaPtr += AlphaXtra;
				}

			return GR_TRUE;
			}

		}
		else
		{
		uint32 DstColorKey;

			ColorKey	= SrcInfo->ColorKey;
			DstColorKey	= DstInfo->ColorKey;

			assert(DstInfo->HasColorKey);

			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					SrcGetColor(&SrcPtr,&R,&G,&B,&A);
					A = *AlphaPtr++;
					if ( A < 128 )
					{
						DstPutPixel(&DstPtr,DstColorKey);
					}
					else
					{
						Pixel = DstComposePixel(R,G,B,255);
						if ( Pixel == ColorKey )
							Pixel ^= 1;
						DstPutPixel(&DstPtr,Pixel);
					}
				}
				SrcPtr += SrcXtraBytes;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}

			return GR_TRUE;
		}
	}

	assert("should not get here" == NULL);
return GR_FALSE;
// end Seperate Alpha conversions
}

/*}{*********************************************************************/

grBoolean BlitData_ToSeparateAlpha(void)
{
grBitmap_Info AlphaInfo;
void * AlphaData;
uint8 *SrcPtr,*DstPtr,*AlphaPtr;
int x,y,R,G,B,A;
uint32 ColorKey,Pixel;
int AlphaXtra;

	/*******
	**
		support the extra Alpha Bmp
		the common case is (4444) -> (8bit + 8bit)

		we're pretty lazy about this; it's not optimized for speed

	**
	 ******/

	SrcPtr = (uint8 *)SrcData;
	DstPtr = (uint8 *)DstData;

	if ( ! grBitmap_GetInfo(DstBmp->Alpha,&AlphaInfo,NULL) )
		return GR_FALSE;
	if ( AlphaInfo.Format != GR_PIXELFORMAT_8BIT_GRAY )
	{
		grErrorLog_AddString(-1,"Bitmap_BlitData : Alpha must be grayscale", NULL);
		return GR_FALSE;
	}

	AlphaData = grBitmap_GetBits(DstBmp->Alpha);
	if ( ! AlphaData )
		return GR_FALSE;

	AlphaPtr = (uint8 *)AlphaData;
	AlphaXtra = AlphaInfo.Stride - SizeX;

	if ( grPixelFormat_HasPalette(DstFormat) )
	{
		// <>
		grErrorLog_AddString(-1,"BlitData : doesn't support blit to palettized separates now", NULL);
		// (alpha) -> pal + separate
		//	requires palettization !!
		return GR_FALSE;
	}
	else
	{
		// this generic converter is pretty damned slow.
		// fortunately Jet3D uses mostly the (Pal -> UnPal) conversion
		// or the (Wavelet -> UnPal) conversion, so screw this

		assert( SrcOps->AMask && !(DstOps->AMask) );

		if ( SrcPelBytes == 0 || DstPelBytes == 0 ) 
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : bad formats", NULL);
			return GR_FALSE;
		}
		else if ( SrcInfo->HasColorKey && DstInfo->HasColorKey )
		{
		uint32 DstColorKey;

			ColorKey = SrcInfo->ColorKey;
			DstColorKey = DstInfo->ColorKey;

			// with seperate alpha

			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					Pixel = SrcGetPixel(&SrcPtr);
					if ( Pixel == ColorKey )
					{
						*AlphaPtr++ = 0;
						DstPutPixel(&DstPtr,DstColorKey);
					}
					else
					{
						SrcDecomposePixel(Pixel,&R,&G,&B,&A);
						Pixel = DstComposePixel(R,G,B,255);
						if ( Pixel == DstColorKey )	Pixel ^= 1;
						DstPutPixel(&DstPtr,Pixel);
						*AlphaPtr++ = A;
					}
				}
				SrcPtr += SrcXtraBytes;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}
		}
		else if ( DstInfo->HasColorKey )
		{
			ColorKey = DstInfo->ColorKey;

			// with seperate alpha

			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					SrcGetColor(&SrcPtr,&R,&G,&B,&A);
					Pixel = DstComposePixel(R,G,B,255);
					if ( Pixel == ColorKey ) Pixel ^= 1;
					*AlphaPtr++ = A;
					DstPutPixel(&DstPtr,Pixel);
				}
				SrcPtr += SrcXtraBytes;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}
		}
		else if ( SrcInfo->HasColorKey )
		{
			// with seperate alpha

			ColorKey = SrcInfo->ColorKey;

			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					Pixel = SrcGetPixel(&SrcPtr);
					if ( Pixel == ColorKey )
					{
						*AlphaPtr++ = 0;
						DstPutColor(&DstPtr,0,0,0,0);
					}
					else
					{
						SrcDecomposePixel(Pixel,&R,&G,&B,&A);
						DstPutColor(&DstPtr,R,G,B,255);
						*AlphaPtr++ = A;
					}
				}
				SrcPtr += SrcXtraBytes;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}
		}
		else
		{
			// with seperate alpha
			for(y=SizeY;y--;)
			{
				for(x=SizeX;x--;)
				{
					SrcGetColor(&SrcPtr,&R,&G,&B,&A);
					DstPutColor(&DstPtr,R,G,B,255);
					*AlphaPtr++ = A;
				}
				SrcPtr += SrcXtraBytes;
				DstPtr += DstXtraBytes;
				AlphaPtr += AlphaXtra;
			}
		}

		assert( AlphaPtr	== (((uint8 *)AlphaData) + AlphaInfo.Stride * SizeY) );
		assert( SrcPtr		== (((uint8 *)SrcData) + SrcRowBytes * SizeY ) );
		assert( DstPtr		== (((uint8 *)DstData) + DstRowBytes * SizeY ) );

	return GR_TRUE;
	}

	// end Seperate Alpha conversions

return GR_FALSE;
}

/*}{*********************************************************************/

grBoolean BlitData_SameFormat(void)
{
char *SrcPtr,*DstPtr;
grPixelFormat Format;

	Format = SrcFormat;
	SrcPtr = (char *)SrcData;
	DstPtr = (char *)DstData;

	if ( (!DstInfo->HasColorKey) || 
			( SrcInfo->HasColorKey && DstInfo->HasColorKey && SrcInfo->ColorKey == DstInfo->ColorKey ) )
	{
	int RowBytes,SrcStepBytes,DstStepBytes,y;
		// just a mem-copy, with strides
		
		RowBytes = SizeX * SrcPelBytes;
		SrcStepBytes = SrcXtraBytes + RowBytes;
		DstStepBytes = DstXtraBytes + RowBytes;
		for(y=SizeY;y--;)
		{
			memcpy( DstPtr, SrcPtr, RowBytes );
			SrcPtr += SrcStepBytes;
			DstPtr += DstStepBytes;
		}

		return GR_TRUE;
	}
	else // same format, different color key
	{
	int x,y;
	uint32 Pixel,DstColorKey;

		//this is common

		assert(DstInfo->HasColorKey);
		DstColorKey = DstInfo->ColorKey;
		
		if ( SrcInfo->HasColorKey )
		{
		uint32 SrcColorKey ;

			SrcColorKey = SrcInfo->ColorKey;

			assert(SrcColorKey != DstColorKey);
			
			// start : formats same, source & dest have different color key
			
			switch(SrcPelBytes)
			{
				default:
					return GR_FALSE;
				case 1:
				{
				uint8 *pSrc,*pDst;
					pSrc = (uint8 *)SrcPtr;
					pDst = (uint8 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == SrcColorKey )
								Pixel = DstColorKey;
							else if ( Pixel == DstColorKey )
								Pixel = SrcColorKey;
							*pDst++ = (uint8)Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
				case 2:
				{
				uint16 *pSrc,*pDst;
					pSrc = (uint16 *)SrcPtr;
					pDst = (uint16 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == SrcColorKey )
								Pixel = DstColorKey;
							else if ( Pixel == DstColorKey )
								Pixel = SrcColorKey;
							*pDst++ = (uint16)Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
				case 3:
				{
				uint8 *pSrc,*pDst;
					pSrc = (uint8 *)SrcPtr;
					pDst = (uint8 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = (pSrc[0]<<16) + (pSrc[1]<<8) + pSrc[2];
							if ( Pixel == SrcColorKey )
								Pixel = DstColorKey;
							else if ( Pixel == DstColorKey )
								Pixel = SrcColorKey;
							pDst[0] = (uint8)(Pixel>>16);
							pDst[1] = (uint8)((Pixel>>8)&0xFF);
							pDst[2] = (uint8)(Pixel&0xFF);
							pSrc += 3;
							pDst += 3;
						}
						pSrc += SrcXtraBytes;
						pDst += DstXtraBytes;
					}
					return GR_TRUE;
				}
				case 4:
				{
				uint32 *pSrc,*pDst;
					pSrc = (uint32 *)SrcPtr;
					pDst = (uint32 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == SrcColorKey )
								Pixel = DstColorKey;
							else if ( Pixel == DstColorKey )
								Pixel = SrcColorKey;
							*pDst++ = Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
			}

			// end : formats same, source & dest have different color key
		}
		else
		{
		
			// start : formats same, dest had color key, source doesn't

			switch(SrcPelBytes)
			{
				default:
					return GR_FALSE;
				case 1:
				{
				uint8 *pSrc,*pDst;
					pSrc = (uint8 *)SrcPtr;
					pDst = (uint8 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							*pDst++ = (uint8)Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
				case 2:
				{
				uint16 *pSrc,*pDst;
					pSrc = (uint16 *)SrcPtr;
					pDst = (uint16 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							*pDst++ = (uint16)Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
				case 3:
				{
				uint8 *pSrc,*pDst;
					pSrc = (uint8 *)SrcPtr;
					pDst = (uint8 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = (pSrc[0]<<16) + (pSrc[1]<<8) + pSrc[2];
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							pDst[0] = (uint8)(Pixel>>16);
							pDst[1] = (uint8)((Pixel>>8)&0xFF);
							pDst[2] = (uint8)(Pixel&0xFF);
							pSrc += 3;
							pDst += 3;
						}
						pSrc += SrcXtraBytes;
						pDst += DstXtraBytes;
					}
					return GR_TRUE;
				}
				case 4:
				{
				uint32 *pSrc,*pDst;
					pSrc = (uint32 *)SrcPtr;
					pDst = (uint32 *)DstPtr;

					for(y=SizeY;y--;)
					{
						for(x=SizeX;x--;)
						{
							Pixel = *pSrc++;
							if ( Pixel == DstColorKey )
								Pixel ^= 1;
							*pDst++ = Pixel;
						}
						pSrc += SrcXtra;
						pDst += DstXtra;
					}
					return GR_TRUE;
				}
			}
			
			// end : formats same, dest had color key, source doesn't
		}

		return GR_TRUE;
	}
// must have returned by now
}
/*}{*********************************************************************/

grBoolean BlitData_DePalettize(void)
{
	// pal -> unpal : easy
	if ( SrcFormat == GR_PIXELFORMAT_8BIT )
	{
	uint8 * SrcPtr;
	grBitmap_Palette * DstPal;
	int x,y,pal;
	const grPixelFormat_Operations *SrcOps,*DstOps;

		x = y = pal = 0; //touch 'em

		SrcOps = grPixelFormat_GetOperations(SrcPal->Format);
		DstOps = grPixelFormat_GetOperations(DstFormat);
		if ( ! SrcOps || ! DstOps )
		{
			return GR_FALSE;
		}

		// NO special cases
		// just convert the Palette to the desired format, then do raw writes!

		DstPal = grBitmap_Palette_Create(DstFormat,256);
		if ( ! DstPal )
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : Palette_Create failed", NULL);	
			return GR_FALSE;
		}

		// we do all alpha & colorkey by manipulating the DstPal lookup table !

		//{} all these _grBitmap_Palette functions need failure checking

		if ( ! grBitmap_Palette_Copy(SrcPal,DstPal) )
		{
			grErrorLog_AddString(-1,"Bitmap_BlitData : Palette_Copy failed", NULL);
			grBitmap_Palette_Destroy(&DstPal);
			return GR_FALSE;
		}

		if ( SrcInfo->HasColorKey )
		{
			if ( ! grBitmap_Palette_SetEntryColor(DstPal,SrcInfo->ColorKey,0,0,0,0) )
			{
				grBitmap_Palette_Destroy(&DstPal);
				return GR_FALSE;
			}
		}

		if ( DstInfo->HasColorKey ) // everything in Jet3D has colorkey!
		{
		int pal;
		uint32 Pixel;
			for(pal=0;pal<DstPal->Size;pal++)
			{
				//{} all this GetEntry/SetEntry is awfully slow
				grBitmap_Palette_GetEntry(DstPal,pal,&Pixel);
				if ( Pixel == DstInfo->ColorKey )
				{
					grBitmap_Palette_SetEntry(DstPal,pal,Pixel^1);
				}
			}
			
		}

		if ( SrcInfo->HasColorKey && DstInfo->HasColorKey )
		{
			if ( ! grBitmap_Palette_SetEntry(DstPal,SrcInfo->ColorKey,DstInfo->ColorKey) )
			{
				grBitmap_Palette_Destroy(&DstPal);
				return GR_FALSE;
			}
		}

		if ( SrcOps->AMask && ! DstOps->AMask && DstInfo->HasColorKey )
		{
		int pal,R,G,B,A;
		uint32 Pixel;

			// if Src format has alpha & Dst format doesn't, turn it into color key

			for(pal=0;pal<DstPal->Size;pal++)
			{
				grBitmap_Palette_GetEntry(SrcPal,pal,&Pixel);
				if ( SrcInfo->HasColorKey && Pixel == SrcInfo->ColorKey )
				{
					A = 0;
				}
				else
				{
					grPixelFormat_DecomposePixel(SrcPal->Format,Pixel,&R,&G,&B,&A);
				}
				if ( A < ALPHA_TO_TRANSPARENCY_THRESHOLD )
					grBitmap_Palette_SetEntry(DstPal,pal,DstInfo->ColorKey);
			}
		}

		SrcPtr = (uint8 *)SrcData;

		// Pal -> UnPal loops : very common & very fast

		switch( grPixelFormat_BytesPerPel(DstFormat) )
		{
			default:
			{
				grBitmap_Palette_Destroy(&DstPal);
				return GR_FALSE;
			}
			case 1:
			{
			uint8 *DstPtr,*PalData;
				PalData = (uint8 *)DstPal->Data;
				DstPtr  = (uint8 *)DstData;
				for(y=SizeY;y--;)
				{

					#ifdef DONT_USE_ASM

					for(x=SizeX;x--;)
					{
						pal = *SrcPtr++;
						*DstPtr++ = PalData[pal];
					}

					#else

					//#pragma message("Bitmap_Blitdata :using assembly DePalettize code")
					// {} is this minimal push safe in _fastcall ? aparently so!

					__asm
					{
						push ebp

						mov ecx,SizeX
						mov esi,SrcPtr
						mov edi,DstPtr
						mov ebp,PalData

						xor eax,eax
						xor edx,edx
						
					moredata1:

						mov al, BYTE PTR [esi]
						mov dl, BYTE PTR [ebp + eax]
						mov BYTE PTR [edi], dl

						inc esi
						inc edi
						dec ecx

						jnz moredata1

						pop ebp
					}

					SrcPtr += SizeX;
					DstPtr += SizeX;

					#endif

					SrcPtr += SrcXtra;
					DstPtr += DstXtra;
				}
				break;
			}
			case 2:
			{
			uint16 *DstPtr,*PalData;

				PalData = (uint16 *)DstPal->Data;
				DstPtr  = (uint16 *)DstData;

			#ifdef DO_TIMER
			{
			#pragma message("Blitdata : doing timer")
				TIMER_VARS(WordCopy);

				timerFP = fopen("q:\\timer.log","at+");
				Timer_Start();
				TIMER_P(WordCopy);
			#endif // DO_TIMER

				for(y=SizeY;y--;)
				{
					#ifdef DONT_USE_ASM

					for(x=SizeX;x--;)
					{
						pal = *SrcPtr++;
						*DstPtr++ = PalData[pal];
					}

					#else

					#if 1 // {
					if ( (SizeX&1) == 0 )
					{
						assert( (((uint32)PalData)&3) == 0 );
						assert( (((uint32)DstPtr )&3) == 0 );

						// pair two pixels so we can output in dwords
						
						__asm
						{
							//pusha
							push ebp

							mov ecx,SizeX
							mov esi,SrcPtr
							mov edi,DstPtr
							mov ebp,PalData

							xor eax,eax
							
						moredata2_z:

							//WordCopy : 0.000664 secs
							//	about 12 clocks per pixel (!?)

							// this is godly fast

							movzx eax, BYTE PTR [esi+0]
							movzx eax, WORD PTR [ebp + eax*2]

							movzx edx, BYTE PTR [esi+1]
							movzx edx, WORD PTR [ebp + edx*2]
							shl edx,16

							xor eax,edx

							mov DWORD PTR [edi], eax

							add esi,2
							add edi,4

							sub ecx,2
							jnz moredata2_z

#if 0 //{ 
						// the old bad way:
						// 0.000710 secs
						moredata2_z:

							//xor edx,edx	//xor edx,0 ; sneaky trick?

							//mov al, BYTE PTR [esi]
							movzx eax, BYTE PTR [esi]
							inc esi

							movzx edx, WORD PTR [ebp + eax*2]
							//mov dx, WORD PTR [ebp + eax*2]

							movzx eax, BYTE PTR [esi]
							inc esi

							// make room fo a new dx
							//shl edx,16
							//mov dx, WORD PTR [ebp + eax*2]	// !! STALL !! ; movzx eax, [] instead?
							// byte order is wrong; fix with rol; 1 clock
							//rol edx,16

							movzx eax, WORD PTR [ebp + eax*2]	// can I do this?
							shl eax,16
							xor edx,eax

							mov DWORD PTR [edi], edx
							add edi,4

							sub ecx,2
							jnz moredata2_z
#endif //}

							pop ebp
							//popa
						}

					}
					else
					#endif //}
					{

						__asm
						{
							//pusha
							push ebp

							mov ecx,SizeX
							mov esi,SrcPtr
							mov edi,DstPtr
							mov ebp,PalData

							xor eax,eax
							xor edx,edx
							
						moredata2:

							// about 14 clocks (!)

							//mov al, BYTE PTR [esi]
							movzx eax, BYTE PTR [esi]
							//movzx edx, WORD PTR [ebp + eax*2]
							mov dx, WORD PTR [ebp + eax*2]
							mov WORD PTR [edi], dx

							inc esi
							add edi,2

							dec ecx
							jnz moredata2

							pop ebp
							//popa
						}
					}

					SrcPtr += SizeX;
					DstPtr += SizeX;

					#endif

					SrcPtr += SrcXtra;
					DstPtr += DstXtra;
				}

				#ifdef DO_TIMER
				TIMER_Q(WordCopy);
				TIMER_COUNT();
				Timer_Stop();
				if ( timerFP )
				{
					TIMER_REPORT(WordCopy);
				}
			}
			#endif

				// C , Debug :
				//WordCopy             : 0.001243 : 99.4 %
				// asm paired : mov al,
				//WordCopy             : 0.000858 : 99.1 %
				// asm paired : movzx eax,
				//WordCopy             : 0.000798 : 98.9 %
				// asm paired : with xor edx,0 & movzx edx,
				//WordCopy             : 0.000903 : 99.2 %
				// asm paired : with xor edx,0 & mov dx,
				//WordCopy             : 0.000941 : 99.4 %
				// asm : not paired 
				//WordCopy             : 0.000765 : 98.8 %
				// asm : paired, using xor edx,eax !
				//WordCopy             : 0.000710 : 98.9 %

				break;
			}
			case 3:
			{
			uint8 *DstPtr,*PalData;
				PalData = (uint8 *)DstPal->Data;
				DstPtr  = (uint8 *)DstData;

//				pushTSC();

				for(y=SizeY;y--;)
				{

					#ifdef DONT_USE_ASM
					{
					uint8 *PalPtr;

						for(x=SizeX;x--;)
						{
							pal = *SrcPtr++;
							PalPtr = PalData + (3*pal);
							*DstPtr++ = *PalPtr++;
							*DstPtr++ = *PalPtr++;
							*DstPtr++ = *PalPtr;
						}

					}
					#else

					__asm
					{
						push ebp

						mov ecx,SizeX
						mov esi,SrcPtr
						mov edi,DstPtr
						mov ebp,PalData

						xor eax,eax
						xor edx,edx
						
					moredata3:

						movzx eax, BYTE PTR [esi]
						inc esi

						imul eax,3
						add eax,ebp
						mov dl, BYTE PTR [eax]
						mov BYTE PTR [edi], dl
						inc edi
						mov dl, BYTE PTR [eax + 1]
						mov BYTE PTR [edi], dl
						inc edi
						mov dl, BYTE PTR [eax + 2]
						mov BYTE PTR [edi], dl
						inc edi

						dec ecx
						jnz moredata3

						pop ebp
					}

					SrcPtr += SizeX;
					DstPtr += SizeX*3;

					#endif

					SrcPtr += SrcXtra;
					DstPtr += DstXtra;
				}
				
//				showPopTSCper("depal 24bit",SizeX*SizeY,"pixel");

				break;
			}
			case 4:
			{
			uint32 *DstPtr,*PalData;
				PalData = (uint32 *)DstPal->Data;
				DstPtr  = (uint32 *)DstData;
				for(y=SizeY;y--;)
				{
					#ifdef DONT_USE_ASM

					for(x=SizeX;x != 0; x--)
					{
						pal = *SrcPtr++;
						*DstPtr++ = PalData[pal];
					}

					#else

					assert( (((uint32)PalData)&3) == 0);
					assert( (((uint32)DstPtr)&3) == 0);

					__asm
					{
						push ebp

						mov ecx,SizeX
						mov esi,SrcPtr
						mov edi,DstPtr
						mov ebp,PalData

						xor eax,eax
						
					moredata4:

						mov al, BYTE PTR [esi]
						mov edx, DWORD PTR [ebp + eax*4]
						mov DWORD PTR [edi], edx

						inc esi
						add edi,4

						dec ecx
						jnz moredata4

						pop ebp
					}

					SrcPtr += SizeX;
					DstPtr += SizeX;

					#endif

					SrcPtr += SrcXtra;
					DstPtr += DstXtra;
				}
				break;
			}
		}

		grBitmap_Palette_Destroy(&DstPal);

		return GR_TRUE;
	}
return GR_FALSE;
}

/*}{*********************************************************************/

grBoolean BlitData_Palettize(void)
{
	// unpal -> pal : hard
return palettizePlane(	SrcInfo,SrcData,
						DstInfo,DstData,
						SizeX,SizeY);
}

/*}{*********************************************************************/

grBoolean BlitData_Wavelet_Compress(void)
{
const grWavelet_Options * opts;
	if ( DstBmp && DstBmp->HasWaveletOptions )
		opts = &(DstBmp->WaveletOptions);
	else if ( SrcBmp && SrcBmp->HasWaveletOptions )
		opts = &(SrcBmp->WaveletOptions);
	else
		opts = NULL;
return grWavelet_Compress((grWavelet *)DstData,SrcInfo,SrcData,SrcBmp,opts);
}

/*}{*********************************************************************/

grBoolean BlitData_Wavelet_DeCompress(void)
{
	if ( SrcBmp && SrcBmp->WaveletMipLock > 0 )
	{
		if ( ! grWavelet_DecompressMips((grWavelet *)SrcData,(const grBitmap_Info **)&DstInfo,(const void **)&DstData,SrcBmp->WaveletMipLock,SrcBmp->WaveletMipLock) )
		{
			grErrorLog_AddString(-1,"Wavelet_DecompressMips failed!",NULL);
			assert(0);
			return GR_FALSE;
		}
	}
	else
	{
		if ( ! grWavelet_Decompress((grWavelet *)SrcData,DstInfo,DstData) )
		{
			grErrorLog_AddString(-1,"Wavelet_Decompress failed!",NULL);
			assert(0);
			return GR_FALSE;
		}
	}

return GR_TRUE;
}

#pragma warning (default : 4731)