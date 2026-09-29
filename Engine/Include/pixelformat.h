/****************************************************************************************/
/*  PixelFormat.h                                                                       */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description:  The abstract Pixel primitives                                         */
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
#ifndef	PIXELFORMAT_H
#define	PIXELFORMAT_H

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum		// all supported formats (including shifts)
{
	GR_PIXELFORMAT_NO_DATA = 0,
	GR_PIXELFORMAT_8BIT,				// PAL
	GR_PIXELFORMAT_8BIT_GRAY,		// no palette (intensity from bit value)
	GR_PIXELFORMAT_16BIT_555_RGB,
	GR_PIXELFORMAT_16BIT_555_BGR,
	GR_PIXELFORMAT_16BIT_565_RGB,	// #5
	GR_PIXELFORMAT_16BIT_565_BGR, 
	GR_PIXELFORMAT_16BIT_4444_ARGB, // #7
	GR_PIXELFORMAT_16BIT_1555_ARGB, 
	GR_PIXELFORMAT_24BIT_RGB,		// #9
	GR_PIXELFORMAT_24BIT_BGR,
	GR_PIXELFORMAT_24BIT_YUV,		// * see note below
	GR_PIXELFORMAT_32BIT_RGBX, 
	GR_PIXELFORMAT_32BIT_XRGB, 
	GR_PIXELFORMAT_32BIT_BGRX, 
	GR_PIXELFORMAT_32BIT_XBGR,
	GR_PIXELFORMAT_32BIT_RGBA, 
	GR_PIXELFORMAT_32BIT_ARGB,		// #17
	GR_PIXELFORMAT_32BIT_BGRA, 
	GR_PIXELFORMAT_32BIT_ABGR,
	
	GR_PIXELFORMAT_WAVELET,			// #20 , Wavelet Compression

	GR_PIXELFORMAT_COUNT
} grPixelFormat;
	
/******

there's something wacked out about these format names :

	for 16 bit & 32 bit , the _RGB or _BGR refers to their order
		*in the word or dword* ; since we're on intel, this means
		the bytes in the data file have the *opposite* order !!
		(for example the 32 bit _ARGB is actually B,G,R,A in raw bytes)
	for 24 bit , the _RGB or _BGR refers to their order in the
		actual bytes, so that windows bitmaps actually have
		_RGB order in a dword !!

* YUV : the pixelformat ops here are identical to those of 24bit_RGB ;
		this is just a place-keeper to notify you that you should to a YUV_to_RGB conversion

*********/

#define GR_PIXELFORMAT_8BIT_PAL GR_PIXELFORMAT_8BIT

typedef uint32	(*grPixelFormat_Composer   )(int R,int G,int B,int A);
typedef void	(*grPixelFormat_Decomposer )(uint32 Pixel,int *R,int *G,int *B,int *A);

typedef void	(*grPixelFormat_ColorGetter)(uint8 **ppData,int *R,int *G,int *B,int *A);
typedef void	(*grPixelFormat_ColorPutter)(uint8 **ppData,int  R,int  G,int  B,int  A);

typedef uint32	(*grPixelFormat_PixelGetter)(uint8 **ppData);
typedef void	(*grPixelFormat_PixelPutter)(uint8 **ppData,uint32 Pixel);

typedef struct grPixelFormat_Operations
{
	uint32	RMask;
	uint32	GMask;
	uint32	BMask;
	uint32	AMask;

	int		RShift;
	int		GShift;
	int		BShift;
	int		AShift;

	int		RAdd;
	int		GAdd;
	int		BAdd;
	int		AAdd;

	int			BytesPerPel;
	grBoolean	HasPalette;
	char *		Description;
	
	grPixelFormat_Composer		ComposePixel;
	grPixelFormat_Decomposer	DecomposePixel;

	grPixelFormat_ColorGetter	GetColor;
	grPixelFormat_ColorPutter	PutColor;

	grPixelFormat_PixelGetter	GetPixel;
	grPixelFormat_PixelPutter	PutPixel;
} grPixelFormat_Operations;

	// the Masks double as boolean "HaveAlpha" .. etc..

GRAPI const grPixelFormat_Operations * GRCC grPixelFormat_GetOperations( grPixelFormat Format );

	// quick accessors to _GetOps
GRAPI grBoolean	GRCC grPixelFormat_IsValid(		grPixelFormat Format);
GRAPI unsigned int GRCC grPixelFormat_BytesPerPel(	grPixelFormat Format );
GRAPI grBoolean	GRCC grPixelFormat_HasPalette(		grPixelFormat Format );
GRAPI grBoolean	GRCC grPixelFormat_HasAlpha(		grPixelFormat Format );
GRAPI grBoolean	GRCC grPixelFormat_HasGoodAlpha(	grPixelFormat Format ); // more than 1 bit of alpha
GRAPI const char * GRCC grPixelFormat_Description(	grPixelFormat Format );
GRAPI grBoolean	GRCC grPixelFormat_IsRaw(			grPixelFormat Format );
									// 'Raw' means pixels can be made with the Compose operations

GRAPI uint32		GRCC grPixelFormat_ComposePixel(	grPixelFormat Format,int R,int G,int B,int A);
GRAPI void			GRCC grPixelFormat_DecomposePixel(	grPixelFormat Format,uint32 Pixel,int *R,int *G,int *B,int *A);
			
															// these four functions move ppData to the next pixel

GRAPI void			GRCC grPixelFormat_GetColor(grPixelFormat Format,uint8 **ppData,int *R,int *G,int *B,int *A);
GRAPI void			GRCC grPixelFormat_PutColor(grPixelFormat Format,uint8 **ppData,int R,int G,int B,int A);

GRAPI uint32		GRCC grPixelFormat_GetPixel(grPixelFormat Format,uint8 **ppData);
GRAPI void			GRCC grPixelFormat_PutPixel(grPixelFormat Format,uint8 **ppData,uint32 Pixel);
	
GRAPI uint32		GRCC grPixelFormat_ConvertPixel(grPixelFormat Format,uint32 Pixel,grPixelFormat ToFormat);


#ifdef __cplusplus
}
#endif


// Genesis3D: Reborn gr* Aliases
typedef grPixelFormat grPixelFormat;

#endif
