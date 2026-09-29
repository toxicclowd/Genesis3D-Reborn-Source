/****************************************************************************************/
/*  WAVELET.H                                                                           */
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
#ifndef WAVELET_H
#define WAVELET_H

#include "BaseType.h"
#include "Bitmap.h"
#include "ThreadQueue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grWavelet grWavelet;
typedef struct grWavelet_Options grWavelet_Options;

struct grWavelet_Options
{
	int transformN;
	int coderN;
	grBoolean transposeLHs;
	float ratio;
	grBoolean tblock;
};

grWavelet * grWavelet_Create(const grBitmap_Info * Info,const void * Bits,const grBitmap * Bmp,
									const grWavelet_Options * opts);
void		grWavelet_CreateRef(grWavelet * w);
grWavelet * grWavelet_CreateEmpty(int width,int height);
grWavelet * grWavelet_CreateFromFile(grBitmap * Bmp,grVFile * File);
grWavelet * grWavelet_CreateFromBitmap(const grBitmap * Bmp,const grWavelet_Options * opts);
void 		grWavelet_Destroy(grWavelet ** pW);

void		grWavelet_GetInfo( const grWavelet *w, grBitmap_Info * Info);
grBoolean	grWavelet_HasAlpha(const grWavelet *w);

grBoolean	grWavelet_Compress(grWavelet * w,const grBitmap_Info * Info,const void * Bits,
									const grBitmap * Bmp,const grWavelet_Options * opts);
grBoolean	grWavelet_WriteToFile(const grWavelet * W,grVFile * File);

grBoolean	grWavelet_CanDecompressMips(const grWavelet *w,const grBitmap_Info * ToInfo );
grBoolean	grWavelet_Decompress(const grWavelet * w,const grBitmap_Info * Info,void * Bits);
grBoolean	grWavelet_DecompressMips(const grWavelet * W,const grBitmap_Info ** InfoArray,const void ** BitsArray,uint32 MipLow,uint32 MipHigh);

grBoolean	grWavelet_SetOptions(grWavelet_Options *opts,int clevel,grBoolean NeedMips,grFloat ratio);

grBoolean	grWavelet_SetExpertOptions(grWavelet_Options *opts,grFloat Ratio,int TransformN,int CoderN,grBoolean TransposeLHs,grBoolean Block);

const char *grWavelet_GetOptionsDescription(void);

grBoolean	grWavelet_ShouldDecompressStreaming(const grWavelet * W);

void		grWavelet_CheckStreaming(const grWavelet * W);
void		grWavelet_WaitStreaming( const grWavelet * W);

grThreadQueue_Job * grWavelet_StreamingJob(const grWavelet *W);

#ifdef __cplusplus
}
#endif


#endif


