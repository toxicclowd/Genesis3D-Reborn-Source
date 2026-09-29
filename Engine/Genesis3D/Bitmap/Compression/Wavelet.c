/****************************************************************************************/
/*  WAVELET.C                                                                           */
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
/*}{********************************************************************/

/*}{*******

(at level 0)

on the Katmai-500 , a 256x256 image decompresses wav -> 24BGR in 0.045 seconds !

On my K6-350 , it's 0.085 seconds

Zero-ing takes almost as much time as the blit or the decode !!

**}{********/

#define WAVELET_DOZERO

#define USE_GLOBAL_TSC // @@

#ifdef USE_GLOBAL_TSC
#define DECOMPRESS_MINIMUM_DELAY	(300)
#else
#define DECOMPRESS_MINIMUM_DELAY	(1000)
#endif

#define DECOMPRESS_MAXIMUM_DELAY	(10000) // in MHz :

#define WAVELET_VERSION				(3)

#include <stdio.h> // for sprintf

#include "VFile.h"
#include "Ram.h"
#include "Tsc.h"
#include "Log.h"
#include "Cpu.h"
#include "Timer.h"
#include "ThreadLog.h"
#include "ThreadQueue.h"

#include "Utility.h"
#include "Image.h"
#include "codeimage.h"
#include "Coder.h"
#include "transform.h"
#include "YUV.h"
#include "palcreate.h"
#include "palettize.h"
#include "IntMath.h"

#include "Wavelet.h"
#include "Wavelet._h"

#include "Bitmap.h"
#include "Bitmap._h"	// just for grBitmap_UsesColorKey
#include "Bitmap.__h"

grBoolean	grWavelet_AddFromFile(grWavelet *W,grVFile * File,grBoolean Streaming);
static int chooseLevels(int size);
static grBoolean BlitBitmapToImageYUV(const grBitmap_Info * Info,const void * Bits,const grBitmap * Bmp,image * im);
static grBoolean BlitImageYUVToBitmap(const image * im,int imw,int imh, const grBitmap_Info * Info, void *Bits);

static grWavelet_Options defaultOpts =
{
	1,
	0,
	GR_FALSE,
	1.0,
	GR_FALSE
};

static tsc_type GlobalDecompressTSC;

TIMER_VARS(Wavelet);
TIMER_VARS(Wavelet_Ram);
TIMER_VARS(Wavelet_Blit);
TIMER_VARS(Wavelet_DecAndZero);
TIMER_VARS(Wavelet_Zero);
TIMER_VARS(Wavelet_Transform);

/*}{********************************************************************/

grBoolean	grWavelet_SetExpertOptions(grWavelet_Options *opts,grFloat Ratio,int TransformN,int CoderN,grBoolean TransposeLHs,grBoolean Block)
{
	assert(opts);

	if ( CoderN >= num_coders || CoderN < 0 )
	{
		grErrorLog_AddString(-1,"Wavelet : bad options : coderN invalid",NULL);
		return GR_FALSE;
	}
	if ( TransformN >= nTransforms || TransformN < 0 )
	{
		grErrorLog_AddString(-1,"Wavelet : bad options : transformN invalid",NULL);
		return GR_FALSE;
	}
	if ( Ratio < 1.0f )
	{
		grErrorLog_AddString(-1,"Wavelet : bad options : Ratio invalid",NULL);
		return GR_FALSE;
	}

	opts->transformN = TransformN;
	opts->ratio = Ratio;
	opts->coderN = CoderN;
	opts->tblock = Block;
	opts->transposeLHs = TransposeLHs;
	
	Log_Printf("Brando : Options : coder = %s, transform = %s\n",
		coder_list[opts->coderN]->name,transformNames[opts->transformN]);
	if ( opts->transposeLHs || opts->tblock )
	{
		Log_Printf("Brando : Options :");
		if ( opts->transposeLHs )
			Log_Printf(" transposeLHs");
		if ( opts->transposeLHs && opts->tblock )
			Log_Printf(" and");
		if ( opts->tblock )
			Log_Printf(" trans-block");
		Log_Printf("\n");
	}
	if ( opts->ratio > 1.0 )
		Log_Printf("Brando : Options : Truncate to ratio : %f\n",opts->ratio);

return GR_TRUE;
}

grBoolean	grWavelet_SetOptions(grWavelet_Options *opts,int clevel,grBoolean NeedMips,grFloat ratio)
{
	assert(opts);

		 if ( clevel <= 1 )		opts->coderN = 5;	// coderFast3
	else if ( clevel <= 2 )		opts->coderN = 4;	// coderFast2
	else if ( clevel <= 5 )		opts->coderN = 0;	// coderBP
	else if ( clevel <= 8 )		opts->coderN = 1;	// coderBPBF
	else						opts->coderN = 2;	// coderBPB2

	// trans 0 is l97, 1 is cdf22

	opts->transposeLHs = GR_FALSE;

	if ( NeedMips )
	{
		opts->transformN = 1;
		opts->tblock = GR_FALSE;
	}
	else
	{
		opts->tblock = GR_TRUE;
		if ( clevel <= 0 || clevel == 2 || clevel == 3 )
			opts->transformN = 1;
		else
			opts->transformN = 0;
	}

	// @@ we never get transformN == 6 == S+P

	if ( clevel == 5 || clevel > 6 )
		opts->transposeLHs = GR_TRUE;

	if ( ratio < 1.0 ) ratio = 1.0;
	opts->ratio = ratio;

	Log_Printf("Brando : Options : coder = %s, transform = %s\n",
		coder_list[opts->coderN]->name,transformNames[opts->transformN]);
	if ( opts->transposeLHs || opts->tblock )
	{
		Log_Printf("Brando : Options :");
		if ( opts->transposeLHs )
			Log_Printf(" transposeLHs");
		if ( opts->transposeLHs && opts->tblock )
			Log_Printf(" and");
		if ( opts->tblock )
			Log_Printf(" trans-block");
		Log_Printf("\n");
	}
	if ( opts->ratio > 1.0 )
		Log_Printf("Brando : Options : Truncate to ratio : %f\n",opts->ratio);

return GR_TRUE;
}

const char * grWavelet_GetOptionsDescription(void)
{
int i;
static char opts_desc[4096];
#define opts_end ( opts_desc + strlen(opts_desc) )

	opts_desc[0] = 0;

	sprintf(opts_end,"coders:\n");
	for(i=0;i<num_coders;i++)
	{
		sprintf(opts_end,"%2d = %s\n",i,coder_list[i]->name);
	}
	sprintf(opts_end,"transforms:\n");
	for(i=0;i<nTransforms;i++)
	{
		sprintf(opts_end,"%2d = %s\n",i,transformNames[i]);
	}
	
return (const char *)opts_desc;
}

grBoolean	grWavelet_HasAlpha(const grWavelet *w)
{
	assert(w);
	if ( w->planes == 4 || ( w->alphaMask > 0 ) )
		return GR_TRUE;
	else
		return GR_FALSE;
}

void grWavelet_GetInfo(const grWavelet *w, grBitmap_Info * Info)
{
	assert( w && Info );

	memset(Info,0,sizeof(*Info));

	Info->ColorKey = 1;

	if ( w->planes == 4 )
		Info->Format = GR_PIXELFORMAT_32BIT_ARGB;
	else if ( w->planes == 1 )
		Info->Format = GR_PIXELFORMAT_8BIT_GRAY;
	else
		Info->Format = GR_PIXELFORMAT_24BIT_RGB;

	if ( w->alphaMask == -1 )
		Info->HasColorKey = GR_TRUE;

	Info->MaximumMip = w->levels - 1;
	Info->Width  = w->width;
	Info->Height = w->height;
	Info->Stride = w->width;
}

void grWavelet_MakeWH(int width,int height,int *pwidth,int *pheight)
{
int shift,s;
int llw,llh;

	// find the padded width to make it a power of two TIMES an LL size
	// the LL need not be a power of two!

	shift = chooseLevels(min(width,height));
	
	// this makes a width=257 image pad all the way to 320 (instead of 288)
	// we must do it to make the LL even-width (LL of 10, not 9 bytes)
	shift ++;

	llw = width;
	llh = height;
	for(s=shift;s--;)
	{
		llw = (llw+1)>>1;
		llh = (llh+1)>>1;
	}
	*pwidth  = llw<<shift;
	*pheight = llh<<shift;

#ifdef _LOG
	if ( width != *pwidth || height != *pheight )
		Log_Printf("Wavelet : extended image %dx%d -> %dx%d (LL %dx%d)\n",width,height,*pwidth,*pheight,llw+llw,llh+llh);
#endif
}

void grWavelet_Setup(grWavelet * w,int width,int height)
{
int levels;

	assert(w);

	grWavelet_MakeWH(width,height,&width,&height);

	// the original width & height are stored in the bitmap wrapper

	levels = chooseLevels(min(width,height));

	w->transformN = defaultOpts.transformN;
	w->coderN = defaultOpts.coderN;
	w->transposeLHs = defaultOpts.transposeLHs;
	w->tblock = defaultOpts.tblock;
	w->tag = WAVELET_TAG;

	w->width  = width;
	w->height = height;
	w->planes = 3;

	w->levels = levels;
	w->compLen = width*height*4;
}

grWavelet * grWavelet_CreateEmpty(int width,int height)
{
grWavelet * w;

	w = (grWavelet *)new(grWavelet);
	if ( ! w )
		return NULL;

	w->RefCount = 1;

	grWavelet_Setup(w,width,height);

	if ( (w->comp = (uint8*)grRam_Allocate(w->compLen)) == NULL ) 
	{
		destroy(w); return NULL;
	}

return w;
}

void grWavelet_CreateRef(grWavelet * w)
{
	assert(w);

	w->RefCount ++;
}

grWavelet * grWavelet_CreateFromBitmap(const grBitmap * Bmp,const grWavelet_Options * opts)
{
grBitmap * Lock;
grBitmap_Info Info;
void * Bits;
grWavelet * w;

	if ( ! grBitmap_LockForReadNative(Bmp,&Lock,0,0) )
		return NULL;

	grBitmap_GetInfo(Lock,&Info,NULL);
	Bits = grBitmap_GetBits(Lock);

	w = grWavelet_Create(&Info,Bits,Lock,opts);

	grBitmap_UnLock(Lock);

#if 0 // <> if you want to see the Mirrored Image
	((grBitmap *)Bmp)->Info.Width = w->width;
	((grBitmap *)Bmp)->Info.Height = w->height;
	((grBitmap *)Bmp)->Info.Stride = Bmp->Info.Width;
#endif

return w;
}

grWavelet * grWavelet_Create(const grBitmap_Info * Info,const void * Bits,const grBitmap * Bmp,
	const grWavelet_Options * opts)
{
grWavelet * w;

	if ( ! Info || ! Bits || ! Bmp )
		return NULL;

	w = grWavelet_CreateEmpty(Info->Width,Info->Height);
	if ( ! w )
	{
		BrandoError("grWavelet_Create : Alloc failed");
		return NULL;
	}

	if ( ! grWavelet_Compress(w,Info,Bits,Bmp,opts) )
	{
		grWavelet_Destroy(&w);
		return NULL;
	}

return w;
}

grBoolean grWavelet_Compress(grWavelet *w,const grBitmap_Info * Info,const void * Bits,const grBitmap * Bmp,
	const grWavelet_Options * opts)
{
image *im = NULL;

	SetupUtility();

	grWavelet_Setup(w,Info->Width,Info->Height);

	if ( ! opts ) opts = &defaultOpts;
	w->transformN = opts->transformN;
	w->coderN = opts->coderN;
	w->transposeLHs = opts->transposeLHs;
	w->tblock = opts->tblock;

	if ( Info->HasColorKey && Bmp )
	{
		if ( ! grBitmap_UsesColorKey(Bmp) )
			((grBitmap_Info *)Info)->HasColorKey = GR_FALSE;
	}

	if ( Bmp->Info.Format == GR_PIXELFORMAT_8BIT_GRAY )
		w->planes = 1;
	else
		w->planes = 3;

	if ( grBitmap_HasAlpha(Bmp) )
	{
		w->planes = 4;
		im = newImage(w->width,w->height,w->planes);
	}
	else if ( Info->HasColorKey )
	{
		im = newImageAlpha(w->width,w->height,w->planes,GR_TRUE);
		w->alphaMask = -1;
	}
	else
	{
		im = newImage(w->width,w->height,w->planes);
	}
			
	if ( ! im )
		return GR_FALSE;

	// that's a float!
	w->stopLen = (uint32)((Info->Width * Info->Height * w->planes) / (opts->ratio));

	grCPU_EnterMMX();

	w->comp = (uint8*)grRam_Realloc(w->comp,im->tot_size);
	if ( ! w->comp )
	{
		BrandoError("grWavelet_Compress : realloc failed");
		goto fail;
	}
		
	assert(w->comp);

	// copy and YUV
	if ( ! BlitBitmapToImageYUV(Info,Bits,Bmp,im) )
	{
		BrandoError("grWavelet_Compress : BlitBitmapToImage failed");
		goto fail;
	}

	if ( ! extendImage(im,Info->Width,Info->Height) )
	{
		BrandoError("grWavelet_Compress : extendImage failed");
		goto fail;
	}

	transformImageInt(im,w->levels,GR_FALSE,w->transformN,w->transposeLHs,w->tblock);

	// might have transposed :
	w->width  = im->width;
	w->height = im->height;

	// detect a transpose and flip widths & heights accorrdingly
	if ((Info->Width > Info->Height && im->width < im->height) ||
		(Info->Width < Info->Height && im->width > im->height) )
	{
		// transposed !
		if ( ! zeroExtendedImage(im,Info->Height,Info->Width,w->levels) )
		{
			BrandoError("grWavelet_Compress : extendImage failed");
			goto fail;
		}
	}
	else
	{
		// not transposed !
		if ( ! zeroExtendedImage(im,Info->Width,Info->Height,w->levels) )
		{
			BrandoError("grWavelet_Compress : extendImage failed");
			goto fail;
		}
	}

	if ( ! zeroAlphaImage(im,w->levels) )
	{
		BrandoError("grWavelet_Compress : zeroAlphaImage failed");
		goto fail;
	}

	if ( ! encodeWaveletImage(w,im) ) 
	{
		BrandoError("grWavelet_Compress : encodeWavelet failed");
		goto fail;
	}

	freeImage(im); im = NULL;

	grCPU_LeaveMMX();

	w->stopLen = min(w->stopLen,w->compLen);
	w->stopLen = max(w->stopLen,w->compLenLL);
	w->compLen = w->stopLen;
	w->streamGot = w->stopLen;

	w->comp = (uint8*)grRam_Realloc(w->comp,w->compLen + 1024);
	assert(w->comp);

	Log_Printf("Wavelet_Compress : %7d -> %7d = %1.3f bpp\n",	(Info->Width * Info->Height * w->planes),
																w->compLen,
																8.0f * w->compLen / (Info->Width * Info->Height * w->planes) );

	return GR_TRUE;

fail :

	assert(im);
	freeImage(im);
	grCPU_LeaveMMX();
	return GR_FALSE;
}

#define WAVE_FLAG_HASALPHA		(1<<0)
#define WAVE_FLAG_ALPHAISBOOL	(1<<1)
#define WAVE_FLAG_TRANSPOSELHS	(1<<2)
#define WAVE_FLAG_TBLOCK		(1<<3)

	// we're out of space for flags !
#define WAVE_FLAG_PLANES_SHIFT	(4)

#define VERSION_SHIFT	(13)
#define LL_LEN_MASK		((1<<VERSION_SHIFT)-1)
#define WRITE_WAVELET_VERSION	((WAVELET_VERSION << VERSION_SHIFT)&0xFFFF)

grBoolean grWavelet_WriteToFile(const grWavelet * w,grVFile * F)
{
uint8 byte;
uint16 word;
grVFile * HF;

	assert(w && F);
	
	HF = grVFile_GetHintsFile(F);
	assert(HF);

	/*{*

	flags :
		hasalpha
		alphaisbool

	planes in 4 bits, mix with flags
	code w & h as logs, 4 bits each
	levels in 4 bits +	bps in 4 bits (safe ?)
	coderN & transformN , each in 4 bits

	compLenLL in 2 bytes , with the wavelet version

	total : 6 bytes

	*}*/

	byte = (uint8)((w->planes)<<WAVE_FLAG_PLANES_SHIFT);
	assert(w->planes <= ((1<<(8-WAVE_FLAG_PLANES_SHIFT))-1));

	if ( w->alphaMask )
		byte |= WAVE_FLAG_HASALPHA;

	if ( Wavelet_AlphaIsTransparency(w) )
		byte |= WAVE_FLAG_ALPHAISBOOL;

	if ( w->transposeLHs )
		byte |= WAVE_FLAG_TRANSPOSELHS;

	if ( w->tblock )
		byte |= WAVE_FLAG_TBLOCK;

	if ( ! grVFile_Write(HF, &byte, sizeof(byte)) )
		return GR_FALSE;

	assert(w->coderN <= 0xF);
	assert(w->transformN <= 0xF);

	byte = (uint8)(((w->coderN)<<4) + (w->transformN));
	if ( ! grVFile_Write(HF, &byte, sizeof(byte)) )
		return GR_FALSE;

	assert(w->levels <= 0xF);
	assert(w->bps <= 0xF);

	byte = (uint8)(((w->levels)<<4) + (w->bps));
	if ( ! grVFile_Write(HF, &byte, sizeof(byte)) )
		return GR_FALSE;

	assert( w->compLenLL <= LL_LEN_MASK);

	word = (uint16)w->compLenLL;
	word |= WRITE_WAVELET_VERSION;
	if ( ! grVFile_Write(HF, &word, sizeof(word)) )
		return GR_FALSE;

	if ( ! grVFile_Write(HF, &(w->compLen), sizeof(w->compLen)))
		return GR_FALSE;

	if ( ! grVFile_Write(HF, w->comp, w->compLenLL))
		return GR_FALSE;

	if ( ! grVFile_Write(F, w->comp + w->compLenLL, w->compLen - w->compLenLL))
		return GR_FALSE;

	return GR_TRUE;
}

grThreadQueue_Job * grWavelet_StreamingJob(const grWavelet *w)
{
	assert(w);
	return w->StreamingJob;
}

void grWavelet_AddFromFileFunc(grThreadQueue_Job * MyJob,void * Context)
{
grWavelet * w;
	w = (grWavelet *)Context;
	if ( ! w )
		return;
	assert( w->StreamingJob );
	if ( ! grWavelet_AddFromFile(w,w->StreamingFile,GR_TRUE) )
	{
		grErrorLog_AddString(-1,"grWavelet_AddFromFile",NULL); //<> use ErrorLog
	}
}

grWavelet * grWavelet_CreateFromFile(grBitmap * Bmp,grVFile * F)
{
grWavelet * w;
uint8 byte;
uint16 word;
grVFile * HF;

	assert(F);

	w = (grWavelet *)new(grWavelet);
	if ( ! w )
		return NULL;

	w->tag = WAVELET_TAG;
	w->comp = NULL;
	w->RefCount = 1;

	grWavelet_MakeWH(Bmp->Info.Width,Bmp->Info.Height,&(w->width),&(w->height));

	HF = grVFile_GetHintsFile(F);
	assert(HF);

	if ( ! grVFile_Read(HF, &byte, sizeof(byte)) )
		goto fail;

	w->planes = (byte)>>WAVE_FLAG_PLANES_SHIFT;

	if ( byte & WAVE_FLAG_HASALPHA )
	{
		if ( byte & WAVE_FLAG_ALPHAISBOOL )
		{
			Bmp->Info.HasColorKey = GR_TRUE;			
			w->alphaMask = -1;
		}
		else
		{
			w->alphaMask = 1;
		}
	}
	
	if ( byte & WAVE_FLAG_TRANSPOSELHS )
		w->transposeLHs = GR_TRUE;

	if ( byte & WAVE_FLAG_TBLOCK )
	{
		w->tblock = GR_TRUE;
		swapints(w->width,w->height);
	}

	if ( ! grVFile_Read(HF, &byte, sizeof(byte)) )
		goto fail;

	w->coderN = byte>>4;
	w->transformN = byte & 0xF;

	if ( w->coderN >= num_coders )
	{
		grErrorLog_AddString(-1,"Wavelet : bad header : coderN invalid",NULL);
		goto fail;
	}
	if ( w->transformN >= nTransforms )
	{
		grErrorLog_AddString(-1,"Wavelet : bad header : transformN invalid",NULL);
		goto fail;
	}

	if ( ! grVFile_Read(HF, &byte, sizeof(byte)) )
		goto fail;

	w->levels = byte>>4;
	w->bps	= byte & 0xF;

	if ( (1L<<(w->levels)) >= w->width || (1L<<(w->levels)) >= w->height )
	{
		grErrorLog_AddString(-1,"Wavelet : bad header : levels invalid",NULL);
		goto fail;
	}

	if ( ! grVFile_Read(HF, &word, sizeof(word)) )
		goto fail;
		
	w->compLenLL = (word & LL_LEN_MASK);

	word &= ~ LL_LEN_MASK;
	if ( word != WRITE_WAVELET_VERSION )
	{
		grErrorLog_AddString(-1,"Wavelet : bad header : incompatible version",NULL);
		goto fail;
	}	

	if ( ! grVFile_Read(HF, &(w->compLen), sizeof(w->compLen)) )
		goto fail;

	if ( w->compLen > (uint32)(w->width * w->height * 4) )
	{
		grErrorLog_AddString(-1,"Wavelet : bad header : compLen invalid",NULL);
		goto fail;
	}

	if ( ! (w->comp = (uint8*)grRam_AllocateClear(w->compLen)) )
		goto fail;

#if 1 // @@ ?
	readTSC(((grWavelet *)w)->DecompressTSC);
#endif

#if 0 // make it not decompress until an explicit attach triggers a decompress
	// signed/unsigned problem?
	w->DecompressTSC[0] = 0x7FFFFFFF;
	w->DecompressTSC[1] = 0x7FFFFFFF;
#endif

	if ( ! grVFile_Read(HF, w->comp, w->compLenLL))
		goto fail;

	w->streamGot = w->compLenLL;

	{
	grVFile_Properties Prop;

		if ( grVFile_GetProperties(F,&Prop) && (Prop.AttributeFlags & GR_VFILE_ATTRIB_REMOTE) )
		{
			w->StreamingLock = grThreadQueue_Semaphore_Create();
			if ( ! w->StreamingLock )
				goto fail;

			w->StreamingFile = F;
			grVFile_CreateRef(F);

			w->StreamingJob = grThreadQueue_JobCreate(grWavelet_AddFromFileFunc,w,NULL,16384);
			assert(w->StreamingJob);
		}
		else
		{
			grWavelet_AddFromFile(w,F,GR_FALSE);
		}
	}

	return w;

fail :
	destroy(w);
	return NULL;
}

grBoolean LockStreamingFile(grWavelet * w)
{
	if ( w->StreamingFile )
	{
		assert( w->StreamingLock );
		grThreadQueue_Semaphore_Lock(w->StreamingLock);
		
		if ( ! w->StreamingFile )
		{
			grThreadQueue_Semaphore_UnLock(w->StreamingLock);
			return GR_FALSE;
		}
		return GR_TRUE;
	}
return GR_FALSE;
}

void UnLockStreamingFile(grWavelet * w)
{
	if ( w->StreamingFile )
	{
		assert(w->StreamingLock);
		grThreadQueue_Semaphore_UnLock(w->StreamingLock);
	}
}

grBoolean grWavelet_AddFromFile(grWavelet *w,grVFile * F,grBoolean Streaming)
{
int32 qlen;
grBoolean KeepGoing;
uint32 millis;
int32 startpos,fsize;

	millis = 1;
	
	ThreadLog_Printf("AddFromFile : starting %08X \n",(uint32)w);

	if ( ! grVFile_Tell(F,&startpos) )
		startpos = 0;

	while( w->compLen > w->streamGot )
	{
		LockStreamingFile(w);
		if ( grVFile_EOF(F) )
		{
			KeepGoing = GR_FALSE;
		}
		else
		{
			KeepGoing = grVFile_BytesAvailable(F,&qlen);
			if ( KeepGoing )
			{
				if ( grVFile_Size(F,&fsize) )
				{
					fsize -= startpos;
				}
				else
				{
					fsize = 1UL<<29;
				}
			}
		}

		if ( millis != 1024 )		
			ThreadLog_Printf("AddFromFile : %08X : %d available, slept %d\n",(uint32)w,qlen,millis);

		if ( ! KeepGoing )
		{
			UnLockStreamingFile(w);
			break;
		}

		// {} must also do || (would reading this cause EOF?) 


		qlen = min(qlen, (int32)(w->compLen - w->streamGot));
		if ( qlen > 2048 || ( qlen + w->streamGot >= w->compLen) || ( qlen + ((int32)w->streamGot) >= fsize ) )
		{
			KeepGoing = grVFile_Read(F,w->comp + w->streamGot,qlen);
			
			w->streamGot += qlen;
			
			UnLockStreamingFile(w);

			if ( ! KeepGoing )
			{
				assert( grVFile_IsValid(F) );
				grErrorLog_AddString(-1,"grWavelet_AddFromFile : VF_Read failed!",NULL);
				assert(0);
				break;
			}

			millis = max(1,(millis>>2));
		}
		else
		{
			UnLockStreamingFile(w);

			millis = min(1024,millis<<1);
			grThreadQueue_Sleep(millis);
		}
	}

	// can't use LockStreamingFile here
	if ( w->StreamingFile )
	{
		grThreadQueue_Semaphore_Lock(w->StreamingLock);
		grVFile_Close(w->StreamingFile);
		w->StreamingFile = NULL;
		grThreadQueue_Semaphore_UnLock(w->StreamingLock);
		ThreadLog_Printf("AddFromFile %08X done\n",(uint32)w);
	}

return GR_TRUE;
}

void grWavelet_CheckStreaming(const grWavelet * w)
{
grThreadQueue_JobStatus Status;
grThreadQueue_Job * Job;

	Job = w->StreamingJob;
	if ( ! Job )
		return;

	if ( ! grThreadQueue_WaitOnJob(Job,GR_THREADQUEUE_STATUS_RUNNING) )
		grErrorLog_AddString(-1,"Wavelet Streaming : WaitOnJob failed! Continuing anyway!",NULL);

	Status = grThreadQueue_JobGetStatus(Job);
	if ( Status == GR_THREADQUEUE_STATUS_COMPLETED )
	{
		assert( ! w->StreamingFile );
		ThreadLog_Printf("Brando : Job Destroyed by CheckStreaming : %08X\n",(uint32)w);
		grThreadQueue_JobDestroy(&(Job));
		((grWavelet *)w)->StreamingJob = NULL;
		return;
	}
	else if ( w->StreamingFile )
	{
		if ( LockStreamingFile((grWavelet *)w) )
		{
		int32 len;
		
			assert( w->StreamingFile );
			if ( grVFile_BytesAvailable(w->StreamingFile,&len) )
			{
				if ( (len > 0 ) && grVFile_Read(w->StreamingFile,w->comp + w->streamGot,len) )
				{
					((grWavelet *)w)->streamGot += len;
				}
			}
			UnLockStreamingFile((grWavelet *)w);
		}
	}

}

void grWavelet_WaitStreaming(const grWavelet * w)
{
	assert(w);
	
	if ( ! w->StreamingJob )
		return;

	if ( ! grThreadQueue_WaitOnJob(w->StreamingJob,GR_THREADQUEUE_STATUS_COMPLETED) )
		grErrorLog_AddString(-1,"Wavelet Streaming : WaitOnJob failed! Continuing anyway!",NULL);

	assert( ! w->StreamingFile );

	ThreadLog_Printf("Brando : Job Destroyed by WaitStreaming : %08X\n",(uint32)w);
	grThreadQueue_JobDestroy(&(((grWavelet *)w)->StreamingJob));
	((grWavelet *)w)->StreamingJob = NULL;
}

void grWavelet_Destroy(grWavelet ** pW)
{
	assert(pW);
	if ( *pW )
	{
	grWavelet * w;
		w = *pW;
		assert( w->RefCount > 0 );
		w->RefCount --;
		if ( w->RefCount == 0 )
		{
			grWavelet_WaitStreaming(w);
			
			assert( ! w->StreamingJob );

			if ( w->StreamingFile )
			{
				grThreadQueue_Semaphore_Lock(w->StreamingLock);
				grVFile_Close(w->StreamingFile);
				w->StreamingFile = NULL;
				grThreadQueue_Semaphore_UnLock(w->StreamingLock);
			}

			if ( w->StreamingLock )
			{
				grThreadQueue_Semaphore_Destroy(&(w->StreamingLock));
				w->StreamingLock = NULL;
			}

			if ( w->comp )
				destroy(w->comp);
			destroy(w);
			*pW = NULL;
		}
	}
}

typedef struct HookContext
{
	const grBitmap_Info ** InfoArray;
	void ** BitsArray;
	int MipLow,MipHigh;
} HookContext;

void pyramidHook_ImageToBitmap(void *passback, image *im, int level, int width, int height)
{
HookContext * cntx;
int i;

	cntx = (HookContext *)passback;

	assert( level >= cntx->MipLow );
	assert( level <= cntx->MipHigh);

	i = level - cntx->MipLow;
	
	if ( ! BlitImageYUVToBitmap(im,width,height, cntx->InfoArray[i], cntx->BitsArray[i] ) )
	{
		BrandoError("Hook_ImageToBitmap : BlitImageYUVToBitmap failed!");
	}
}

grBoolean grWavelet_CanDecompressMips(const grWavelet *w,const grBitmap_Info * ToInfo )
{
	assert( w );

	if ( w->tblock )
		return GR_FALSE;
			
	if ( ! transformMips[ w->transformN ] )
		return GR_FALSE;

	if ( w->width != w->height ) // really ?
		return GR_FALSE;

	if ( grPixelFormat_IsRaw(ToInfo->Format) )
		return GR_TRUE;

	// if we have the palette, we can decompress mips from the wavelet,
	//	if we must build the palette, we can't!
	#pragma message("Wavelet : DecompressMips to Palettized broken")
	#if 0
	// @@ there's a bug ; presumably in BlitImageYUVtoBitmap
	if ( grPixelFormat_HasPalette(ToInfo->Format) && ToInfo->Palette )
		return GR_TRUE;
	#endif

return GR_FALSE;
}

grBoolean	grWavelet_ShouldDecompressStreaming(const grWavelet * w)
{
int len;
tsc_type tsc;
double hz;

	grWavelet_CheckStreaming(w);

	len = w->streamGot - w->streamDecoded;

	assert( len >= 0 );
	if ( len < 4 )
		return GR_FALSE;

	readTSC(tsc);
	hz = diffTSChz(w->DecompressTSC,tsc);

	if ( w->streamDecoded == 0 && w->streamGot >= w->compLenLL )
	{
		ThreadLog_Printf("ShouldDec : %08X : hz = %f , len = %d : Saw LL\n",(uint32)w,hz,len);
		return GR_TRUE;
	}
	// don't ever decompress too frequently
#ifdef USE_GLOBAL_TSC
	if ( diffTSChz(GlobalDecompressTSC,tsc) < (DECOMPRESS_MINIMUM_DELAY*1000000.0) )
#else
	if ( hz < (DECOMPRESS_MINIMUM_DELAY*1000000.0) )
#endif
	{
//		ThreadLog_Printf("ShouldDec : %08X : too recent\n",(uint32)w);
		return GR_FALSE;
	}

	if ( len >= 4096 || (len*100)/(w->compLen) > 10 || ((len/(w->streamDecoded+1)) > 5 ) )
	{
		// saw another 2048 bytes, let's decompress !
		// (or saw more than 10% of the total)

		ThreadLog_Printf("ShouldDec : %08X : hz = %f , len = %d : Got Data\n",(uint32)w,hz,len);
		return GR_TRUE;
	}

	if ( len >= 1024 && hz > (DECOMPRESS_MAXIMUM_DELAY*1000000.0) )
	{
		// check a timer and return true if it's been a while
		ThreadLog_Printf("ShouldDec : %08X : hz = %f , len = %d : Long Wait\n",(uint32)w,hz,len);
		return GR_TRUE;
	}

	if ( w->streamGot == w->compLen && (len*100/w->compLen) > 5 )
	{
		// we're at the end, so go ahead and finish it off correctly..
		ThreadLog_Printf("ShouldDec : %08X : hz = %f , len = %d : Finish Off \n",(uint32)w,hz,len);
		return GR_TRUE;
	}

//	ThreadLog_Printf("ShouldDec : %08X : not enough data : %d\n",(uint32)w,len);
	return GR_FALSE;
}

grBoolean	grWavelet_DecompressMips(const grWavelet * w,const grBitmap_Info ** InfoArray,const void ** BitsArray,uint32 MipLow,uint32 MipHigh)
{
image * im = NULL;

	assert( w );
	assert( InfoArray && BitsArray );

	if ( MipLow == 0 && MipHigh == 0 )
	{
		return grWavelet_Decompress(w,InfoArray[0],(void *)BitsArray[0]);
	}

	SetupUtility();

	ThreadLog_Printf("Brando : PreDecompress %08X Mips : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

	grWavelet_CheckStreaming(w);

	if ( w->streamGot < w->compLenLL )
		return GR_FALSE; // {} return GR_TRUE ?

	if ( ! grWavelet_CanDecompressMips(w,InfoArray[0]) )
	{
		BrandoError("Wavelet_DecompressMips : This format can't do mips!");
		return GR_FALSE;
	}

	if ( w->alphaMask )
	{
		if ( (im = newImageAlpha(w->width,w->height,w->planes,Wavelet_AlphaIsTransparency(w))) == NULL )
			return GR_FALSE;
	}
	else
	{
		if ( (im = newImage(w->width,w->height,w->planes)) == NULL )
			return GR_FALSE;
	}

	grCPU_EnterMMX();

#ifdef WAVELET_DOZERO
	zeroImage(im);
#endif

	// must fill this in *before* decodeWavelet 
	// could underestimate length; better than overestimating
	((grWavelet *)w)->stopLen = w->streamGot;
	((grWavelet *)w)->streamDecoded = w->stopLen;
	readTSC(((grWavelet *)w)->DecompressTSC);
	readTSC(GlobalDecompressTSC);

	ThreadLog_Printf("Brando : Decompressing %08X Mips : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

	if ( ! decodeWaveletImage((grWavelet *)w,im) )
	{
		BrandoError("Decompress : decodeWavelet failed!");
		goto fail;
	}

	//{} if InfoArray is palettized, make the palette
	// (the standard problem is that we make mips low -> high)

	if ( InfoArray[0]->Format == GR_PIXELFORMAT_8BIT_PAL && ! InfoArray[0]->Palette )
	{
		BrandoError("Decompress : palettized image with no palette!");
		goto fail;
	}

	{
	HookContext cntx;
		cntx.InfoArray = InfoArray;
		cntx.BitsArray = (void **)BitsArray;
		cntx.MipLow = MipLow;
		cntx.MipHigh = MipHigh;

		unTransformImageIntToPyramid(im,w->levels,w->transformN,
			MipLow,MipHigh,
			pyramidHook_ImageToBitmap,(void *)&cntx,w->transposeLHs);
	}

	grCPU_LeaveMMX();

	freeImage(im); im = NULL;

	ThreadLog_Printf("Brando : Decompressed %08X Mips : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

	return GR_TRUE;
	
fail:
	assert( im );
	freeImage(im);
	grCPU_LeaveMMX();

	return GR_FALSE;
}

grBoolean	grWavelet_Decompress(const grWavelet * w,const grBitmap_Info * Info,void * Bits)
{
image * im;

	assert( w );
	assert( Info && Bits );

	SetupUtility();

	ThreadLog_Printf("Brando : PreDecompress %08X : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

	grWavelet_CheckStreaming(w);

	if ( w->streamGot < w->compLenLL )
		return GR_TRUE; // {} return GR_TRUE ?

pushTSC();

	TIMER_P(Wavelet);

	TIMER_P(Wavelet_Ram);

	if ( w->alphaMask )
	{
		if ( (im = newImageAlpha(w->width,w->height,w->planes,Wavelet_AlphaIsTransparency(w))) == NULL )
			return GR_FALSE;
	}
	else
	{
		if ( (im = newImage(w->width,w->height,w->planes)) == NULL )
			return GR_FALSE;
	}

	TIMER_Q(Wavelet_Ram);

	grCPU_EnterMMX();

	TIMER_P(Wavelet_DecAndZero);

	TIMER_P(Wavelet_Zero);

	zeroImage(im);

	TIMER_Q(Wavelet_Zero);

	// must fill this in *before* decodeWavelet 
	// could underestimate length; better than overestimating
	
	((grWavelet *)w)->stopLen = w->streamGot;
	((grWavelet *)w)->streamDecoded = w->stopLen;
	readTSC(((grWavelet *)w)->DecompressTSC);
	readTSC(GlobalDecompressTSC);

	ThreadLog_Printf("Brando : Decompressing %08X : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

#ifdef _LOG
pushTSC();
#endif

	if ( ! decodeWaveletImage((grWavelet *)w,im) )
	{
		grCPU_LeaveMMX();
		freeImage(im);
		BrandoError("Decompress : decodeWavelet failed!");
		return GR_FALSE;
	}

	TIMER_Q(Wavelet_DecAndZero);

#ifdef _LOG
grCPU_LeaveMMX();
showPopTSCper("Decode Image",w->width*w->height,"pixel");
grCPU_EnterMMX();
#endif

	TIMER_P(Wavelet_Transform);

	transformImageInt(im,w->levels,GR_TRUE,w->transformN,w->transposeLHs,w->tblock);

	TIMER_Q(Wavelet_Transform);

#ifdef _LOG
grCPU_LeaveMMX();
showPopTSCper("Decode & Untransform Image",w->width*w->height,"pixel");
pushTSC();
grCPU_EnterMMX();
#endif

	TIMER_P(Wavelet_Blit);

	if ( ! BlitImageYUVToBitmap(im,im->width,im->height, Info, Bits) )
	{
		BrandoError("Decompress : BlitImageYUVToBitmap failed!");
	}

	TIMER_Q(Wavelet_Blit);

#ifdef _LOG
showPopTSCper("Blit",w->width*w->height,"pixel");
#endif

	grCPU_LeaveMMX();

	TIMER_P(Wavelet_Ram);

	freeImage(im);

	TIMER_Q(Wavelet_Ram);

	TIMER_Q(Wavelet);

	ThreadLog_Printf("Brando : Decompressed %08X : %d (now %d) -> %dx%d at %08X %08X\n",(uint32)w,w->streamDecoded,w->streamGot,w->width,w->height,w->DecompressTSC[0],w->DecompressTSC[1]);

	return GR_TRUE;
}

void Wavelet_DoReport(void)
{
	TIMER_REPORT(Wavelet_Zero);
	TIMER_REPORT(Wavelet_Ram);
	TIMER_REPORT(Wavelet_Blit);
	TIMER_REPORT(Wavelet_DecAndZero);
	TIMER_REPORT(Wavelet_Transform);
	TIMER_REPORT(Wavelet);
}

/*}{********************************************************************/

#define MIN_LL_SIZE (8)

static int chooseLevels(int size)
{
int levels;

	if ( size <= MIN_LL_SIZE ) return 0;

	levels = intlog2(size);

	assert( levels > 0 );

	while( (size>>levels) < MIN_LL_SIZE ) levels--;

	assert( levels > 0 );

return levels;
}

/*}{********************************************************************/
/** 'image' to Bitmap Data blitters **/

//{} make sure they respect the fact that the 'im' and 'Info' may not have same width & height

static grBoolean BlitBitmapToImageYUV(const grBitmap_Info * Info,const void * Bits,const grBitmap * Bmp,image * im)
{
grPixelFormat Format;
grBitmap * AlphaBmp;
const uint8 * ptr;
uint32 Pixel,ColorKey;
int stride_bytes;
int x,y,w,h,imw,imh,ims,imxtra;

	assert( Info && Bits && im );
		
	Format = Info->Format;
	ptr = (uint8*)Bits;

	w = Info->Width;
	h = Info->Height;
	imw = im->width;
	imh = im->height;
	assert( w <= imw );
	assert( h <= imh );
	ims = im->stride;
	imxtra = ims - w;

	ColorKey = Info->ColorKey;
	stride_bytes = (Info->Stride - w) * grPixelFormat_BytesPerPel(Format);

	if ( im->planes == 1 )
	{
	int ** imrows,*imptr;
		assert(Format == GR_PIXELFORMAT_8BIT_GRAY );

		imrows = im->data[0];
		imptr = imrows[0];

		assert( ! Info->HasColorKey ); // {} just laziness

		for(y=0;y<h;y++)
		{
			for(x=w;x--;)
			{
				*imptr++ = *ptr++;
			}
			imptr += imxtra;
			ptr += stride_bytes;
		}
		return GR_TRUE;
	}
	else
	{
	int ** Yrows,** Urows, ** Vrows;
	int * Yptr,* Uptr, * Vptr;
	int avgY=0,avgU=0,avgV=0;
		assert( im->planes == 3 || im->planes == 4 );

		Yrows = im->data[0];
		Urows = im->data[1];
		Vrows = im->data[2];
		Yptr = Yrows[0];
		Uptr = Urows[0];
		Vptr = Vrows[0];

		// Bmp may be palettized, have separate alpha, or have color key
		//

		if ( Info->HasColorKey )
		{
			if ( Bmp )
			{
			int R,G,B;
				if ( ! grBitmap_GetAverageColor(Bmp,&R,&G,&B) )
				{
					grErrorLog_AddString(-1,"Wavelet : average color failed!",NULL);
					return GR_FALSE;
				}

				RGBi_to_YUVi(R,G,B,&avgY,&avgU,&avgV);
			}
			else
			{
				// <> find something yourself !
				grErrorLog_AddString(-1,"Wavelet : Compressor needs Bmp for average color!",NULL);
				assert(0);
			}
		}

		if ( Bmp )
			AlphaBmp = grBitmap_GetAlpha(Bmp);
		else
			AlphaBmp = NULL;
		if ( AlphaBmp )
		{
		void * AlphaBits;
			AlphaBits = grBitmap_GetBits(AlphaBmp);
			if ( AlphaBits )
			{
			grBitmap_Info AlphaInfo;
			uint8 *sptr;
			int *aptr;

				grBitmap_GetInfo(AlphaBmp,&AlphaInfo,NULL);

				assert(im->planes == 4);
				assert( AlphaInfo.Format == GR_PIXELFORMAT_8BIT_GRAY );

				// fill out the plane-3 alpha

				sptr = (uint8*)AlphaBits;
				aptr = im->data[3][0];
				for(y=h;y--;)
				{
					for(x=w;x--;)
					{
						*aptr++ = *sptr++;
					}
					sptr += AlphaInfo.Stride - w;
					aptr += imxtra;
				}

				((grBitmap_Info *)Info)->HasColorKey = GR_FALSE;
			}
		}
		
		if ( grPixelFormat_HasPalette(Format) )
		{
		int Ypal[256],Upal[256],Vpal[256],Apal[256];
		uint32 RGBApal[256];
		grPixelFormat PalFormat;
		grBitmap_Info PalInfo;
		int Size,p;
		grBitmap_Palette * Pal;
			
			Pal = Info->Palette;

			// pal -> yuv int image

			grBitmap_Palette_GetInfo(Pal,&PalInfo);

			PalFormat = PalInfo.Format;
			Size = PalInfo.Width;

			if ( ! grBitmap_Palette_GetData(Pal,RGBApal,GR_PIXELFORMAT_32BIT_RGBA,256) )
				return GR_FALSE;

			for(p=0;p<Size;p++)
			{
				Pixel = RGBApal[p];
				RGBi_to_YUVi(	(Pixel>>24)&0xFF,(Pixel>>16)&0xFF,(Pixel>>8)&0xFF,
					Ypal+p,Upal+p,Vpal+p);
				Apal[p] = Pixel&0xFF;
			}

			if ( im->alpha && im->alphaIsBoolean && Info->HasColorKey )
			{
			uint8 * aptr;
				aptr = im->alpha;
				for(y=0;y<h;y++)
				{
					for(x=0;x<w;x++)
					{
						p = *ptr++;
						if ( (uint32)p == ColorKey )
						{
							*aptr++ = 0;

							// meaningless color; stuff it
							
							if ( y == 0 )
							{
								if ( x == 0 )
								{
									*Yptr++ = avgY;
									*Uptr++ = avgU;
									*Vptr++ = avgV;
								}
								else
								{
									*Yptr++ = Yptr[-1];
									*Uptr++ = Uptr[-1];
									*Vptr++ = Vptr[-1];
								}
							}
							else
							{
								*Yptr++ = Yptr[-ims];
								*Uptr++ = Uptr[-ims];
								*Vptr++ = Vptr[-ims];
							}
						}
						else
						{
							*Yptr++ = Ypal[p];
							*Uptr++ = Upal[p];
							*Vptr++ = Vpal[p];
							*aptr++ = 255;
						}
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					aptr += imxtra;
					ptr += stride_bytes;
				}
			}
			else if ( im->planes == 4 && grPixelFormat_HasAlpha(PalFormat) )
			{
			int * aptr;
				assert( ! Info->HasColorKey ); // {} fuck you
				aptr = im->data[3][0];
				for(y=0;y<h;y++)
				{
					for(x=w;x--;)
					{
						p = *ptr++;
						*Yptr++ = Ypal[p];
						*Uptr++ = Upal[p];
						*Vptr++ = Vpal[p];
						*aptr++ = Apal[p];
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					aptr += imxtra;
					ptr += stride_bytes;
				}
			}
			else
			{
				assert( ! Info->HasColorKey ); // {} fuck you
				for(y=0;y<h;y++)
				{
					for(x=w;x--;)
					{
						p = *ptr++;
						*Yptr++ = Ypal[p];
						*Uptr++ = Upal[p];
						*Vptr++ = Vpal[p];
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					ptr += stride_bytes;
				}
				return GR_TRUE;
			}
		}
		else // not palettized source
		{
		const grPixelFormat_Operations * ops;
		grPixelFormat_ColorGetter GetColor;
		grPixelFormat_PixelGetter GetPixel;
		grPixelFormat_Decomposer DecomposePixel;
		int R,G,B,A;

			ops = grPixelFormat_GetOperations(Format);
			GetColor = ops->GetColor;
			GetPixel = ops->GetPixel;
			DecomposePixel = ops->DecomposePixel;

			if ( grPixelFormat_HasAlpha(Format) )
			{
			int * aptr;
				assert(im->planes == 4);
				assert( ! Info->HasColorKey ); // {} damns!
				
				aptr = im->data[3][0];

				for(y=0;y<h;y++)
				{
					for(x=w;x--;)
					{
						GetColor((uint8 **)&ptr,&R,&G,&B,&A);
						RGBi_to_YUVi(R,G,B,Yptr++,Uptr++,Vptr++);
						*aptr++ = A;
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					aptr += imxtra;
					ptr += stride_bytes;
				}
				return GR_TRUE;
			}
			else if ( Info->HasColorKey )
			{
			uint8 * aptr;
				assert(im->alpha);
				assert(im->alphaIsBoolean);
				aptr = im->alpha;
				
				for(y=0;y<h;y++)
				{
					for(x=0;x<w;x++)
					{
						Pixel = GetPixel((uint8 **)&ptr);
						if ( Pixel == ColorKey )
						{
							*aptr++ = 0;

							// meaningless color; stuff it
							
							if ( x == 0 )
							{						
								if ( y == 0 )
								{
									*Yptr++ = avgY;
									*Uptr++ = avgU;
									*Vptr++ = avgV;
								}
								else
								{
									*Yptr++ = Yptr[-ims];
									*Uptr++ = Uptr[-ims];
									*Vptr++ = Vptr[-ims];
								}
							}
							else
							{
								// we mostly do horizontal stretches;
								// <> ideally we would choose the type of stretch which we
								// wavelet first : horizontal if we horizontal first
								*Yptr++ = Yptr[-1];
								*Uptr++ = Uptr[-1];
								*Vptr++ = Vptr[-1];
							}
						}
						else
						{
							DecomposePixel(Pixel,&R,&G,&B,&A);
							RGBi_to_YUVi(R,G,B,Yptr++,Uptr++,Vptr++);
							*aptr++ = 255;
						}
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					aptr += imxtra;
					ptr += stride_bytes;
				}
				return GR_TRUE;
			}
			else
			{
				for(y=0;y<h;y++)
				{
					for(x=w;x--;)
					{
						GetColor((uint8 **)&ptr,&R,&G,&B,&A);
						RGBi_to_YUVi(R,G,B,Yptr++,Uptr++,Vptr++);
					}
					Yptr += imxtra;
					Uptr += imxtra;
					Vptr += imxtra;
					ptr += stride_bytes;
				}
				return GR_TRUE;
			}
		}
	}

return GR_FALSE;
}

static grBoolean BlitImageYUVToBitmap(const image * im,int imw,int imh, const grBitmap_Info * Info, void *Bits)
{
grPixelFormat Format;
uint8 * ptr;
int x,y;
const grPixelFormat_Operations * ops;
grPixelFormat_Composer Composer;
grPixelFormat_ColorPutter PutColor;
grPixelFormat_PixelPutter PutPixel;
int R,G,B,A,Y,U,V;
int stride_bytes,astride,astep;
uint32 Pixel,ColorKey;	
uint8 * aptr;
	
	assert( Info && Bits && im );
		
	Format = Info->Format;

	imw = min(imw, Info->Width );
	imh = min(imh, Info->Height);

	ptr = (uint8*)Bits;

	ColorKey = Info->ColorKey;
	stride_bytes = (Info->Stride - imw) * grPixelFormat_BytesPerPel(Format);

	ops = grPixelFormat_GetOperations(Format);
	assert(ops);
	Composer = ops->ComposePixel;
	PutColor = ops->PutColor;
	PutPixel = ops->PutPixel;

	if ( im->planes == 1 )
	{
	int * imptr,**imrows;
	
		assert( ! Info->HasColorKey ); // {} just laziness

		imrows = im->data[0];
		imptr = imrows[0];
		if ( Format == GR_PIXELFORMAT_8BIT_GRAY )
		{
			for(y=0;y<imh;y++)
			{
				imptr = imrows[y];
				for(x=imw;x--;)
				{
				int v;
					v = *imptr++;
					*ptr++ = minmax(v,0,255);
				}
				ptr += stride_bytes;
			}
		}
		else
		{
			for(y=0;y<imh;y++)
			{
				imptr = imrows[y];
				for(x=imw;x--;)
				{
					R = *imptr++;
					R = minmax(R,0,255);
					PutColor(&ptr,R,R,R,255);
				}
				ptr += stride_bytes;
			}
		}
		return GR_TRUE;
	}
	else
	{
	int ** Yrows,** Urows, ** Vrows;
	int * Yptr,* Uptr, * Vptr;

		assert( im->planes == 3 || im->planes == 4 );

		Yrows = im->data[0];
		Urows = im->data[1];
		Vrows = im->data[2];

		// alpha in 'im' has no mips
		// compute astride & astep to sub-sample the alpha
		astep = im->width / imw;
		astride = (im->height / imh) * (im->stride) - (astep * imw);

		aptr = im->alpha;

		// im is YUV, possibly with alpha or CK
		// Info is either raw or palettized
		// no separates

		if ( grPixelFormat_HasPalette(Format) )
		{
		grBitmap_Palette *Pal;
		palInfo * pi;
		uint8 paldata[768];
		int palval;

			//{} palettize with alpha !?

			if ( Info->Palette )
			{			
				grBitmap_Palette_GetData(Info->Palette,paldata,GR_PIXELFORMAT_24BIT_RGB,256);
				
				RGBb_to_YUVb_line(paldata,paldata,256);

				Pal = NULL;
			}
			else
			{
				// create the palette !

				assert( imw >= (Info->Width>>1) );
				assert( imh >= (Info->Height>>1) );

				Pal = createPaletteFromImage(im);
				assert(Pal);
				
				grBitmap_Palette_GetData(Pal,paldata,GR_PIXELFORMAT_24BIT_RGB,256);
			}

			// palettize!

			pi = closestPalInit(paldata);
			assert(pi);

			if ( Info->HasColorKey )
			{
			int ck;
				ck = Info->ColorKey;

				if ( im->alpha )
				{
				int athresh;

					// use im->alpha to colorkey

					if ( im->alphaIsBoolean )
						athresh = 1;
					else
						athresh = 128;

					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						for(x=imw;x--;)
						{
							A = *aptr++;
							if ( A < athresh )
							{
								Yptr++; Uptr++; Vptr++;
								*ptr++ = ck;
							}
							else
							{
								Y = *Yptr++; putminmax(Y,0,255);
								U = *Uptr++; putminmax(U,0,255);
								V = *Vptr++; putminmax(V,0,255);
								palval = closestPal(Y,U,V,pi);
								if ( palval == ck )
									palval ^= 1; //{} not good to do on palettes!
								*ptr++ = palval;
							}
						}
						ptr += stride_bytes;
						aptr += astride;
					}
				}
				else if ( im->planes == 4 )
				{
				int ** Arows,*Aptr;
					Arows = im->data[3];
					// use alpha plane to colorkey

					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						Aptr = Arows[y];
						for(x=imw;x--;)
						{
							A = *Aptr;
							if ( A < 128 )
							{
								*ptr = ck;
							}
							else
							{
								Y = *Yptr++; putminmax(Y,0,255);
								U = *Uptr++; putminmax(U,0,255);
								V = *Vptr++; putminmax(V,0,255);
								palval = closestPal(Y,U,V,pi);
								if ( palval == ck )	palval ^= 1; //{} not good to do on palettes!
								*ptr = palval;
							}
							ptr++; Yptr++; Uptr++; Vptr++; Aptr++;
						}
						ptr += stride_bytes;
					}
				}
				else
				{
					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						for(x=imw;x--;)
						{
							Y = *Yptr++; putminmax(Y,0,255);
							U = *Uptr++; putminmax(U,0,255);
							V = *Vptr++; putminmax(V,0,255);
							palval = closestPal(Y,U,V,pi);
							if ( palval == ck )
								palval ^= 1; //{} not good to do on palettes!
							*ptr++ = palval;
						}
						ptr += stride_bytes;
					}
				}
			}
			else // no colorkey
			{
				for(y=0;y<imh;y++)
				{
					Yptr = Yrows[y];
					Uptr = Urows[y];
					Vptr = Vrows[y];
					for(x=imw;x--;)
					{
						Y = *Yptr++; putminmax(Y,0,255);
						U = *Uptr++; putminmax(U,0,255);
						V = *Vptr++; putminmax(V,0,255);
						palval = closestPal(Y,U,V,pi);
						*ptr++ = palval;
					}
					ptr += stride_bytes;
				}
			}

			closestPalFree(pi);

			if ( Pal )
			{
				assert( Info->Palette == NULL );
				YUVb_to_RGBb_line(paldata,paldata,256);
				grBitmap_Palette_SetData(Pal,paldata,GR_PIXELFORMAT_24BIT_RGB,256);
				((grBitmap_Info *)Info)->Palette = Pal;
			}

			return GR_TRUE;
		}
		else // no palette ; im -> raw
		{
			if ( im->alpha && (Info->HasColorKey || grPixelFormat_HasAlpha(Format)) )
			{
				if ( im->alphaIsBoolean )
				{	
					if ( grPixelFormat_HasAlpha(Format) )
					{
						// ck -> alpha
						for(y=0;y<imh;y++)
						{
							Yptr = Yrows[y];
							Uptr = Urows[y];
							Vptr = Vrows[y];
							YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
							
							if ( Info->HasColorKey )
							{
								for(x=imw;x--;)
								{
									R = *Yptr++; G = *Uptr++; B = *Vptr++;
									A = *aptr; aptr += astep;
									if ( A )
									{
										Pixel = Composer(R,G,B,255);
										if ( Pixel == ColorKey ) Pixel ^= 1;
										PutPixel(&ptr,Pixel);
									}
									else
										PutColor(&ptr,R,G,B,0);
								}
							}
							else // no CK
							{
								for(x=imw;x--;)
								{
									R = *Yptr++; G = *Uptr++; B = *Vptr++;
									A = *aptr; aptr += astep;
									if ( A )
										PutColor(&ptr,R,G,B,255);
									else
										PutColor(&ptr,R,G,B,0);
								}
							}
							ptr += stride_bytes;
							aptr += astride;
						}
						return GR_TRUE;
					}
					else
					{
						assert( Info->HasColorKey );
						// ck -> ck
						for(y=0;y<imh;y++)
						{
							Yptr = Yrows[y];
							Uptr = Urows[y];
							Vptr = Vrows[y];
							YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
							for(x=imw;x--;)
							{
								R = *Yptr++; G = *Uptr++; B = *Vptr++;
								A = *aptr; aptr += astep;
								if ( A )			
								{
									Pixel = Composer(R,G,B,255);
									if ( Pixel == ColorKey ) Pixel ^= 1;
									PutPixel(&ptr,Pixel);
								}
								else
									PutPixel(&ptr,ColorKey);
							}
							ptr += stride_bytes;
							aptr += astride;
						}
						return GR_TRUE;
					}
				}
				else // im has real alpha
				{
					if ( grPixelFormat_HasAlpha(Format) )
					{
						// alpha -> alpha
						for(y=0;y<imh;y++)
						{
							Yptr = Yrows[y];
							Uptr = Urows[y];
							Vptr = Vrows[y];
							YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
							if ( Info->HasColorKey )
							{
								for(x=imw;x--;)
								{
									R = *Yptr++; G = *Uptr++; B = *Vptr++;
									A = *aptr; aptr += astep;
									Pixel = Composer(R,G,B,A);
									if ( Pixel == ColorKey ) Pixel ^= 1;
									PutPixel(&ptr,Pixel);
								}
							}
							else
							{
								for(x=imw;x--;)
								{
									R = *Yptr++; G = *Uptr++; B = *Vptr++;
									A = *aptr; aptr += astep;
									PutColor(&ptr,R,G,B,A);
								}
							}
							ptr += stride_bytes;
							aptr += astride;
						}
						return GR_TRUE;
					}
					else
					{
						assert( Info->HasColorKey );
						// alpha->ck
						for(y=0;y<imh;y++)
						{
							Yptr = Yrows[y];
							Uptr = Urows[y];
							Vptr = Vrows[y];
							YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
							for(x=imw;x--;)
							{
								R = *Yptr++; G = *Uptr++; B = *Vptr++;
								A = *aptr; aptr += astep;
								if ( A < 128 )
								{
									PutPixel(&ptr,ColorKey);
								}
								else
								{
									Pixel = Composer(R,G,B,255);
									if ( Pixel == ColorKey ) Pixel ^= 1;
									PutPixel(&ptr,Pixel);
								}
							}
							ptr += stride_bytes;
							aptr += astride;
						}
						return GR_TRUE;
					}
				}
			}
			else if ( im->planes == 4 && (Info->HasColorKey || grPixelFormat_HasAlpha(Format)) )
			{
			int ** Arows,*Aptr;
				Arows = im->data[3];
				if ( grPixelFormat_HasAlpha(Format) )
				{
					// alpha -> alpha
					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						Aptr = Arows[y];
						YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
						if ( Info->HasColorKey )
						{
							for(x=imw;x--;)
							{
								R = *Yptr++; G = *Uptr++; B = *Vptr++; A = *Aptr++;
								Pixel = Composer(R,G,B,A);
								if ( Pixel == ColorKey ) Pixel ^= 1;
								PutPixel(&ptr,Pixel);
							}
						}
						else
						{
							for(x=imw;x--;)
							{
								R = *Yptr++; G = *Uptr++; B = *Vptr++; A = *Aptr++;
								PutColor(&ptr,R,G,B,A);
							}
						}
						ptr += stride_bytes;
					}
					return GR_TRUE;
				}
				else
				{
					assert( Info->HasColorKey );
					// alpha->ck
					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						Aptr = Arows[y];
						YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
						for(x=imw;x--;)
						{
							R = *Yptr++; G = *Uptr++; B = *Vptr++; A = *Aptr++;
							if ( A < 128 )
							{
								PutPixel(&ptr,ColorKey);
							}
							else
							{
								Pixel = Composer(R,G,B,255);
								if ( Pixel == ColorKey ) Pixel ^= 1;
								PutPixel(&ptr,Pixel);
							}
						}
						ptr += stride_bytes;
					}
					return GR_TRUE;
				}
			}
			else	// no alpha : raw -> raw (maybe ck)
			{
				if ( Info->HasColorKey )
				{
					for(y=0;y<imh;y++)
					{
						Yptr = Yrows[y];
						Uptr = Urows[y];
						Vptr = Vrows[y];
						YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
						for(x=imw;x--;)
						{
							R = *Yptr++; G = *Uptr++; B = *Vptr++;
							Pixel = Composer(R,G,B,255);
							if ( Pixel == ColorKey ) Pixel ^= 1;
							PutPixel(&ptr,Pixel);
						}
						ptr += stride_bytes;
					}
					return GR_TRUE;
				}
				else
				{
					if ( Info->Format == GR_PIXELFORMAT_24BIT_BGR )
					{
						YUVi_to_BGRb_lines(imw,imh,Yrows,Urows,Vrows,ptr,Info->Stride * 3);
					}
					else
					{
						for(y=0;y<imh;y++)
						{
							Yptr = Yrows[y];
							Uptr = Urows[y];
							Vptr = Vrows[y];

						#if 0
							YUVi_to_RGBi_line(Yptr,Uptr,Vptr,imw);
							for(x=imw;x--;)
							{
								R = *Yptr++; G = *Uptr++; B = *Vptr++;
								PutColor(&ptr,R,G,B,255);
							}
						#else
						{
						uint8 XRGBline[8192],*XRGBptr;
							assert( imw <= 2048 );
							YUVi_to_XRGB_line(Yptr,Uptr,Vptr,XRGBline,imw);
							XRGBptr = XRGBline;
							for(x=imw;x--;)
							{
								PutColor(&ptr,XRGBptr[2],XRGBptr[1],XRGBptr[0],255);
								XRGBptr += 4;
							}
						}
						#endif

							ptr += stride_bytes;
						}
					}
					return GR_TRUE;
				}
			}
		}
	}

	assert("should not get here" == NULL);
}
