/****************************************************************************************/
/*  Bitmap.c                                                                            */
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
#define DONT_DEC_STREAMING	// <> doesn't decompress a streaming file
							// when it is attached to a driver

/**********
***
*
-----------------------------------------------
NOTEZ BIEN !

	If your name is not Charles, and you make a change to this file,
	please comment it!  write //Initials around changes!

-----------------------------------------------


see @@ for urgent todos !
see <> for todos
see {} for notes/long-term-todos

-------

{} when palettizing lots of mips, blit them together, so we only make
	one closestPal palInfo for all the blits (big speedup).
	(for UpdateMips too)
	perhaps the only way is to keep a cache of the last palInfo made and
	check the palette to see if its reusable.  Check on memory cost of the palInfo.

{} _Lock and _UnLock should percolate up the ->DataOwner chain , so you can't make
	a DataOwner Xerox and then lock the original for read & the copy for write
	(that's a no-no cuz you might get the same bits )

{} when we blit from one set of mips to another, we could copy the palette (or build the palette!)
	many times.  We must do this in general, because the mips could all have different palettes.
	The answer is to keep track of "palette was just copied from X"
	(actually, setting different palettes on two locked mips will cause bad data!!)

{} make a _BlitOnto which merges with alpha?

{} BTW FYI :
		1. glide 4444 surfaces do alpha and colorkey ; colorkey overrides alpha
		2. we're preferring 1555 over colorkey now

*
***
 ********/

#include	<math.h>
#include	<time.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdlib.h>
#include	<string.h>

#include	"BaseType.h"
#include	"grTypes.h"
#include	"Ram.h"

#include	"VFile.h"
#include	"Errorlog.h"
#include	"Log.h"
#include	"ThreadLog.h"
#include	"MemPool.h"

#include	"Bitmap.h"
#include	"Bitmap._h"
#include	"Bitmap.__h"
#include	"bitmap_blitdata.h"
#include	"bitmap_gamma.h"

#include	"Wavelet.h"
#include	"palcreate.h"
#include	"palettize.h"
#include	"CodePal.h"
#include	"SortPal.h"
#include	"SortPalx.h"

#include	"ThreadQueue.h"

#ifdef BUILD_BE
#define strnicmp strncasecmp
#endif

#ifdef DO_TIMER
#include	"Timer.h"
#endif

#define allocate(ptr)	ptr = grRam_Allocate(sizeof(*ptr))
#define clear(ptr)		memset(ptr,0,sizeof(*ptr))

#define SHIFT_R_ROUNDUP(val,shift)	(((val)+(1<<(shift)) - 1)>>(shift))

/*}{ ************* statics *****************/

//#define DO_TIMER

//#define DONT_USE_ASM

#ifdef _DEBUG
#define Debug(x)	x
static int _Bitmap_Debug_ActiveCount = 0;
static int _Bitmap_Debug_ActiveRefs = 0;
#else
#define Debug(x)
#endif

static int32 BitmapInit_RefCount = 0;
static MemPool * BitmapPool = NULL;
grThreadQueue_Semaphore * Bitmap_Gamma_Lock = NULL;
grThreadQueue_Semaphore * Bitmap_BlitData_Lock = NULL;

void grBitmap_Start(void)
{
	if ( BitmapInit_RefCount == 0 )
	{
		BitmapPool = MemPool_Create(sizeof(grBitmap),100,100);
		assert(BitmapPool);
		Palettize_Start();
		PalCreate_Start();
		Bitmap_Gamma_Lock = grThreadQueue_Semaphore_Create();
		assert(Bitmap_Gamma_Lock);
		Bitmap_BlitData_Lock = grThreadQueue_Semaphore_Create();
		assert(Bitmap_BlitData_Lock);
	}
	BitmapInit_RefCount ++;
}

void grBitmap_Stop(void)
{
	assert(BitmapInit_RefCount > 0 );
	BitmapInit_RefCount --;
	if ( BitmapInit_RefCount == 0 )
	{
		assert(BitmapPool);
		MemPool_Destroy(&BitmapPool);
		Palettize_Stop();
		PalCreate_Stop();
		assert(Bitmap_Gamma_Lock);
		grThreadQueue_Semaphore_Destroy(&Bitmap_Gamma_Lock);
		assert(Bitmap_BlitData_Lock);
		grThreadQueue_Semaphore_Destroy(&Bitmap_BlitData_Lock);
	}
}

/*}{ ******** Creator Functions **********************/

grBitmap * grBitmap_Create_Base(void)
{
grBitmap * Bmp;

	grBitmap_Start();

	Bmp = (grBitmap *)MemPool_GetHunk(BitmapPool);

	Bmp->RefCount = 1;

	Bmp->DriverGamma = Bmp->DriverGammaLast = 1.0f;

	Debug(_Bitmap_Debug_ActiveRefs ++);
	Debug(_Bitmap_Debug_ActiveCount ++);

return Bmp;
}

void grBitmap_Destroy_Base(grBitmap *Bmp)
{
	assert(Bmp);
	assert(Bmp->RefCount == 0);
	Debug(_Bitmap_Debug_ActiveCount --);

	MemPool_FreeHunk(BitmapPool,Bmp);

	grBitmap_Stop();
}

GRAPI grBitmap *	GRCC	grBitmap_CreateCopy(const grBitmap * Src)
{
grBitmap_Info Info;
grBitmap * Ret;

	assert( Src );
	if ( ! grBitmap_GetInfo(Src,&Info,NULL) )
		return NULL;

	Info.MaximumMip = Info.MinimumMip = 0;

	Ret = grBitmap_CreateFromInfo(&Info);
	if ( ! Ret )
		return NULL;

	if ( ! grBitmap_BlitBitmap(Src,Ret) )
	{
		grBitmap_Destroy(&Ret);
		return NULL;
	}

	grBitmap_SetMipCount(Ret,Src->SeekMipCount);

return Ret;
}

GRAPI void GRCC	grBitmap_CreateRef(grBitmap *Bmp)
{
	assert(Bmp);
	Bmp->RefCount ++;
	Debug(_Bitmap_Debug_ActiveRefs ++);
}

GRAPI grBitmap *	GRCC	grBitmap_Create(
	int32					 Width,
	int32					 Height,
	int32					 MipCount,
	grPixelFormat Format)
{
grBitmap * Bmp;

	Bmp = grBitmap_Create_Base();
	if ( ! Bmp )
		return NULL;

	assert( Width > 0 );
	assert( Height > 0 );
	if ( MipCount == 0 )
		MipCount = 1;
	assert( MipCount > 0 );

	Bmp->Info.Width = Width;
	Bmp->Info.Stride = Width;
	Bmp->Info.Height = Height;
	Bmp->Info.Format = Format;

	Bmp->Info.MinimumMip = 0;
	Bmp->Info.MaximumMip = 0;
	Bmp->Info.HasColorKey = GR_FALSE;

	Bmp->SeekMipCount = MipCount;

	if ( Format == GR_PIXELFORMAT_WAVELET )
	{
		Bmp->Wavelet = grWavelet_CreateEmpty(Width,Height);
	}

return Bmp;
}

GRAPI grBitmap *	GRCC	grBitmap_CreateFromInfo(const grBitmap_Info * pInfo)
{
grBitmap * Bmp;

	assert(pInfo);
	assert(grBitmap_Info_IsValid(pInfo));

	Bmp = grBitmap_Create_Base();
	if ( ! Bmp )
		return NULL;

	Bmp->Info = *pInfo;

	if ( Bmp->Info.Stride < Bmp->Info.Width )
		Bmp->Info.Stride = Bmp->Info.Width;

	if ( Bmp->Info.Palette )
		grBitmap_Palette_CreateRef(Bmp->Info.Palette);

	if ( Bmp->Info.Format == GR_PIXELFORMAT_WAVELET )
	{
		Bmp->Wavelet = grWavelet_CreateEmpty(Bmp->Info.Width,Bmp->Info.Height);
	}

return Bmp;
}

GRAPI grBoolean	GRCC	 grBitmap_Destroy(grBitmap **Bmp)
{
int			i;
grBitmap *	Bitmap;

	assert(Bmp);

	Bitmap = *Bmp;

	if ( Bitmap )
	{
		if ( Bitmap->LockOwner )
		{
			return grBitmap_UnLock(Bitmap);
		}

		if ( Bitmap->RefCount <= 1 )
		{
			if ( Bitmap->DataOwner )
			{
				grBitmap_Destroy(&(Bitmap->DataOwner));
				Bitmap->DataOwner = NULL;
			}
			else
			{
				if ( Bitmap->Driver )
				{
					grBitmap_DetachDriver(Bitmap,GR_FALSE);
				}

				for	(i = Bitmap->Info.MinimumMip; i <= Bitmap->Info.MaximumMip; i++)
				{
					if	(Bitmap->Data[i])
						grRam_Free(Bitmap->Data[i]);
				}

				if ( Bitmap->Wavelet )
				{
					grWavelet_Destroy(&(Bitmap->Wavelet));
				}
			}
		}

		Debug(assert(_Bitmap_Debug_ActiveRefs > 0));
		Debug(_Bitmap_Debug_ActiveRefs --);

		Bitmap->RefCount --;

		if ( Bitmap->RefCount <= 0 )
		{
			if	(Bitmap->Alpha)
			{
				grBitmap_Destroy(&Bitmap->Alpha);
			}

			if	(Bitmap->Info.Palette)
			{
				grBitmap_Palette_Destroy(&(Bitmap->Info.Palette));
			}

			if	(Bitmap->DriverInfo.Palette)
			{
				grBitmap_Palette_Destroy(&(Bitmap->DriverInfo.Palette));
			}

			grBitmap_Destroy_Base(Bitmap);

			*Bmp = NULL;

			return GR_TRUE;
		}
	}

return GR_FALSE;
}

#if 0 // {} off limits until _Lock & _UnLock percolates up DataOwner
grBitmap * grBitmap_CreateXerox(grBitmap *BmpSrc)
{
grBitmap * Bmp;

	assert( grBitmap_IsValid(BmpSrc) );
	if ( BmpSrc->LockOwner )
		return NULL;	//{} return grBitmap_CreateXeroxFromLock()

	Bmp = grBitmap_Create_Base();
	if ( ! Bmp )
		return NULL;
			
	memcpy(Bmp,BmpSrc);

	Bmp->LockCount = 0;
	Bmp->RefCount = 1;

	Bmp->DataOwner = BmpSrc;
	grBitmap_CreateRef(BmpSrc);
return Bmp;
}
#endif

grBoolean grBitmap_AllocSystemMip(grBitmap *Bmp,int32 mip)
{
	if ( ! Bmp )
	{
		return GR_FALSE;
	}

	if ( Bmp->LockOwner && mip != 0 ) return GR_FALSE;

	if ( ! Bmp->Data[mip] )
	{
	int32 bytes;
		bytes = grBitmap_MipBytes(Bmp,mip);
		if ( bytes == 0 )
		{
			Bmp->Data[mip] = NULL;
			return GR_TRUE;
		}
		Bmp->Data[mip] = grRam_Allocate( bytes );
	}

return (Bmp->Data[mip]) ? GR_TRUE : GR_FALSE;
}

grBoolean grBitmap_AllocPalette(grBitmap *Bmp,grPixelFormat Format,DRV_Driver * Driver)
{
grBitmap_Info * BmpInfo;
	assert(Bmp);

	if ( Driver )
		BmpInfo = &(Bmp->DriverInfo);
	else
		BmpInfo = &(Bmp->Info);

	if ( ! grPixelFormat_IsRaw(Format) )
		Format = GR_PIXELFORMAT_32BIT_XRGB;

	if ( ! BmpInfo->Palette )
	{
		assert( BmpInfo->Format == GR_PIXELFORMAT_8BIT_PAL );

		if ( Driver )
		{
		grBoolean BmpHasAlpha;
			
			BmpHasAlpha = GR_FALSE;
			if ( grPixelFormat_HasGoodAlpha(Bmp->Info.Format) )
				BmpHasAlpha = GR_TRUE;
			else if ( Bmp->Info.Palette && grPixelFormat_HasGoodAlpha(Bmp->Info.Palette->Format) )
				BmpHasAlpha = GR_TRUE;

			if ( BmpHasAlpha || (Bmp->Info.HasColorKey && ! Bmp->DriverInfo.HasColorKey ) )
				Format = GR_PIXELFORMAT_32BIT_ARGB;

			BmpInfo->Palette = grBitmap_Palette_CreateFromDriver(Driver,Format,256);
		}
		else
		{			
			BmpInfo->Palette = grBitmap_Palette_Create(Format,256);
		}
	}

	if ( ! BmpInfo->Palette )
		return GR_FALSE;

	if ( BmpInfo->HasColorKey )
	{
		if ( ! BmpInfo->Palette->HasColorKey )
		{
			BmpInfo->Palette->HasColorKey = GR_TRUE;
			BmpInfo->Palette->ColorKey = 1; // <>
		}
		BmpInfo->Palette->ColorKeyIndex = BmpInfo->ColorKey;
	}

	if ( Driver )
	{
		assert( Bmp->DriverHandle );
		assert( BmpInfo->Palette->DriverHandle );
		if ( ! Driver->THandle_SetPalette(Bmp->DriverHandle,BmpInfo->Palette->DriverHandle) )
		{
			grErrorLog_AddString(-1,"AllocPal : THandle_SetPalette", NULL);
			return GR_FALSE;
		}
	}

	if ( ! Bmp->Info.Palette )
	{
		Bmp->Info.Palette = grBitmap_Palette_CreateCopy(BmpInfo->Palette);
	}

return GR_TRUE;
}

/*}{ *************** Thread & Streaming stuff *******************/

#if 0 	//<> expose this?
GRAPI grBoolean GRCC grBitmap_WaitForUnLock(const grBitmap *Bmp)
{
	assert( grBitmap_IsValid(Bmp) );
	
	if ( Bmp->LockOwner || Bmp->DataOwner )
		return GR_FALSE;

//	if ( grThreadQueue_ActiveJobCount() <= 1 )
//		return GR_FALSE;

	while ( Bmp->LockCount )
	{
		grThreadQueue_Sleep(1);
	}

	if ( Bmp->LockOwner || Bmp->DataOwner )
		return GR_FALSE;

return GR_TRUE;
}
#endif

/****

	just about every function should call either _WaitReady or _PeekReady

	the former waits for any streaming to finish
	the latter updates the bitmap based on the current status of the steaming.

****/

grBoolean grBitmap_WaitReady(const grBitmap *Bmp)
{
	assert( grBitmap_IsValid(Bmp) );
	
	if ( Bmp->StreamingStatus >= GR_BITMAP_STREAMING_STARTED )
	{
		assert(Bmp->Wavelet);
		ThreadLog_Printf("Wavelet : Waiting %08X Streaming\n",(uint32)(Bmp->Wavelet));
		grWavelet_WaitStreaming(Bmp->Wavelet);
		((grBitmap *)Bmp)->StreamingStatus = GR_BITMAP_STREAMING_DATADONE;
		((grBitmap *)Bmp)->StreamingTHandle = GR_FALSE;
	}

return GR_TRUE;
}

grBoolean grBitmap_PeekReady(const grBitmap *Bmp)
{
	assert( grBitmap_IsValid(Bmp) );

	if ( Bmp->DataOwner )
		Bmp = Bmp->DataOwner;
	if ( Bmp->LockOwner || Bmp->LockCount < 0 )
		return GR_TRUE;
			
	//get streaming progress & time since last streaming update;
	// then call _Update_SystemToDriver

	// Warning : grBitmap_Update_SystemToDriver calls PeekReady
	//	and PeekReady calls grBitmap_Update_SystemToDriver !
	//	be carefull!

	if ( Bmp->StreamingStatus >= GR_BITMAP_STREAMING_STARTED )
	{
		assert(Bmp->Wavelet);
		if ( Bmp->DriverHandle )
		{
			if ( ! grWavelet_StreamingJob(Bmp->Wavelet) )
			{
				((grBitmap *)Bmp)->StreamingStatus = GR_BITMAP_STREAMING_DATADONE;
				((grBitmap *)Bmp)->StreamingTHandle = GR_FALSE;
			}
			else
			{
				if ( grWavelet_ShouldDecompressStreaming(Bmp->Wavelet) )
				{
					ThreadLog_Printf("Wavelet : Decompressing %08X Streaming\n",(uint32)(Bmp->Wavelet));
					grBitmap_Update_SystemToDriver((grBitmap *)Bmp);
					((grBitmap *)Bmp)->StreamingStatus = GR_BITMAP_STREAMING_CHANGED;
					
					if ( ! grWavelet_StreamingJob(Bmp->Wavelet) )
					{
						((grBitmap *)Bmp)->StreamingStatus = GR_BITMAP_STREAMING_DATADONE;
						((grBitmap *)Bmp)->StreamingTHandle = GR_FALSE;
					}
				}
				else
				{
					((grBitmap *)Bmp)->StreamingStatus = GR_BITMAP_STREAMING_IDLE;
				}
			}
		}
	}

return GR_TRUE;
}

GRAPI grBitmap_StreamingStatus GRCC grBitmap_GetStreamingStatus(const grBitmap *Bmp)
{
grThreadQueue_JobStatus Status;
grThreadQueue_Job * Job;
grBitmap_StreamingStatus Ret;

	assert( grBitmap_IsValid(Bmp) );

	if ( Bmp->DataOwner )
		Bmp = Bmp->DataOwner;
	if ( Bmp->LockOwner || Bmp->LockCount < 0 )
		return GR_BITMAP_STREAMING_ERROR;
			
	if ( Bmp->StreamingStatus < GR_BITMAP_STREAMING_STARTED )
		return GR_BITMAP_STREAMING_NOT;

	assert(Bmp->Wavelet);

	Job = grWavelet_StreamingJob(Bmp->Wavelet);
	if ( ! Job )
	{
		//StreamingStatus could be DATADONE ; if so, return a real _DONE
		if ( Bmp->StreamingStatus == GR_BITMAP_STREAMING_DONE )
			Ret = GR_BITMAP_STREAMING_NOT;
		else
			Ret = GR_BITMAP_STREAMING_DONE;
	}
	else
	{
		if ( ! grThreadQueue_WaitOnJob(Job,GR_THREADQUEUE_STATUS_RUNNING) )
		{
			grErrorLog_AddString(-1,"Bitmap_GetStreamingStatus : WaitOnJob failed! Continuing anyway!",NULL);
		}

		Ret = GR_BITMAP_STREAMING_IDLE;
		if ( Bmp->Wavelet && grWavelet_ShouldDecompressStreaming(Bmp->Wavelet) )
			Ret = GR_BITMAP_STREAMING_CHANGED;

		Job = grWavelet_StreamingJob(Bmp->Wavelet);;
		if ( ! Job )
		{
			Ret = GR_BITMAP_STREAMING_DONE;
		}
		else
		{
			Status = grThreadQueue_JobGetStatus(Job);
			if ( Status == GR_THREADQUEUE_STATUS_COMPLETED )
				Ret = GR_BITMAP_STREAMING_DONE;
		}
	}

	((grBitmap *)Bmp)->StreamingStatus = Ret;

	if (Ret == GR_BITMAP_STREAMING_DONE ||
		Ret == GR_BITMAP_STREAMING_NOT )
		((grBitmap *)Bmp)->StreamingTHandle = GR_FALSE;

return Ret;
}

/*}{ *************** Locks *******************/

GRAPI grBoolean	GRCC	 grBitmap_LockForWrite(
	grBitmap *			Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip)
{
int32 mip;

	assert( grBitmap_IsValid(Bmp) );
	assert( Target);
	assert(MaximumMip >= MinimumMip);
	assert( &Bmp != Target );

	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockCount || Bmp->LockOwner )
	{
		grErrorLog_AddString(-1,"LockForWrite : already locked", NULL);
		return GR_FALSE;
	}

	if ( Bmp->DriverHandle )
	{
		if ( (MinimumMip < Bmp->DriverInfo.MinimumMip) ||
			 (MaximumMip > Bmp->DriverInfo.MaximumMip) )
		{
			grErrorLog_AddString(-1,"LockForWrite : Driver : invalid mip", NULL);
			return GR_FALSE;
		}
	}
	else
	{
		if ( (MinimumMip < Bmp->Info.MinimumMip) ||
			 (MaximumMip >= MAXMIPLEVELS) )
		{
			grErrorLog_AddString(-1,"LockForWrite : System : invalid mip", NULL);
			return GR_FALSE;
		}

		if ( MaximumMip > Bmp->Info.MaximumMip )
		{
			if ( ! grBitmap_MakeSystemMips(Bmp,Bmp->Info.MaximumMip,MaximumMip) )
				return GR_FALSE;
			Bmp->Info.MaximumMip = MaximumMip;
		}
	}
	
	Bmp->Persistable = GR_FALSE;

	for(mip=MinimumMip;mip <= MaximumMip;mip ++)
	{
		if ( Bmp->DriverHandle )
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipOnDriver(Bmp,mip,-1);
		}
		else
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipSystem(Bmp,mip,-1);
		}
		if ( ! Target[ mip - MinimumMip ] )
		{
			grErrorLog_AddString(-1,"LockForWrite : CreateLockFromMip failed", NULL);
			mip--;
			while(mip >= MinimumMip )
			{
				grBitmap_Destroy( & Target[ mip - MinimumMip ] );
				mip--;
			}
			return GR_FALSE;
		}
	}

	assert( Bmp->LockCount == - (MaximumMip - MinimumMip + 1) );

	return GR_TRUE;
}

GRAPI grBoolean	GRCC grBitmap_LockForWriteFormat(
	grBitmap *			Bmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip,
	grPixelFormat 		Format)
{
int32 mip;

	assert( grBitmap_IsValid(Bmp) );
	assert( Target);
	assert(MaximumMip >= MinimumMip);
	assert( &Bmp != Target );
	
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockCount || Bmp->LockOwner )
	{
		grErrorLog_AddString(-1,"LockForWrite : already locked", NULL);
		return GR_FALSE;
	}

	if ( Format != Bmp->Info.Format && Format != Bmp->DriverInfo.Format )
	{
		grErrorLog_AddString(-1,"LockForWriteFormat : must be System or Driver Format !", NULL);
		return GR_FALSE;
	}

	if ( Format == Bmp->DriverInfo.Format )
	{
		if ( MinimumMip < Bmp->DriverInfo.MinimumMip || MaximumMip > Bmp->DriverInfo.MaximumMip )
		{
			grErrorLog_AddString(-1,"LockForWrite : invalid Driver mip", NULL);
			return GR_FALSE;
		}
	}
	else
	{
		assert( Format == Bmp->Info.Format );

		if ( Bmp->DriverHandle )
		{
			if ( ! grBitmap_Update_DriverToSystem(Bmp) )
			{
				grErrorLog_AddString(-1,"LockForWrite : Update_DriverToSystem", NULL);
				return GR_FALSE;
			}
		}

		// create mips?

		if ( MinimumMip < Bmp->Info.MinimumMip || MaximumMip >= MAXMIPLEVELS )
		{
			grErrorLog_AddString(-1,"LockForWrite : invalid System mip", NULL);
			return GR_FALSE;
		}
		
		if ( ! grBitmap_MakeSystemMips(Bmp,Bmp->Info.MaximumMip,MaximumMip) )
			return GR_FALSE;
		Bmp->Info.MaximumMip = MaximumMip;
	}

	Bmp->Persistable = GR_FALSE;

	for(mip=MinimumMip;mip <= MaximumMip;mip ++)
	{
		if ( Bmp->DriverHandle && Format == Bmp->DriverInfo.Format )
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipOnDriver(Bmp,mip,-1);
		}
		else
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipSystem(Bmp,mip,-1);
		}

		if ( ! Target[ mip - MinimumMip ] )
		{
			grErrorLog_AddString(-1,"LockForWrite : CreateLockFromMip failed", NULL);
			mip--;
			while(mip >= MinimumMip )
			{
				grBitmap_Destroy( & Target[ mip - MinimumMip ] );
				mip--;
			}
			return GR_FALSE;
		}
	}

	assert( Bmp->LockCount == - (MaximumMip - MinimumMip + 1) );

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_LockForReadNative(
	const grBitmap *	iBmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip)
{
int32 mip;
grBitmap * Bmp = (grBitmap *)iBmp;

	assert( grBitmap_IsValid(Bmp) );
	assert( Target);
	assert(MaximumMip >= MinimumMip);
	assert( &Bmp != Target );

// <> lock-for-read : don't do peekready ? if it's a wavelet, 
//	grBitmap_PeekReady(Bmp);

	if ( (MinimumMip < Bmp->Info.MinimumMip && MinimumMip < Bmp->DriverInfo.MinimumMip) ||
		 (MaximumMip >= MAXMIPLEVELS) )
	{
		grErrorLog_AddString(-1,"LockForRead : invalid mip", NULL);
		return GR_FALSE;
	}

	if ( Bmp->LockCount < 0 || Bmp->LockOwner )
	{
		grErrorLog_AddString(-1,"LockForRead : already locked", NULL);
		return GR_FALSE;
	}

	for(mip=MinimumMip;mip <= MaximumMip;mip ++)
	{
		// err on the side of *not* choosing the driver data to read from !
		if ( Bmp->DriverHandle && Bmp->DriverDataChanged
			&& mip <= Bmp->DriverInfo.MaximumMip && mip >= Bmp->DriverInfo.MinimumMip)
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipOnDriver(Bmp,mip,1);
		}
		else
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMipSystem(Bmp,mip,1);
		}
		if ( ! Target[ mip - MinimumMip ] )
		{
			grErrorLog_AddString(-1,"LockForRead : CreateLockFromMip failed", NULL);
			mip--;
			while(mip >= MinimumMip )
			{
				grBitmap_Destroy( & Target[ mip - MinimumMip ] );
				mip--;
			}
			return GR_FALSE;
		}
	}

return GR_TRUE;
}

GRAPI grBoolean	GRCC grBitmap_LockForRead(
	const grBitmap *	iBmp,
	grBitmap **			Target,
	int32				MinimumMip,
	int32				MaximumMip,
	grPixelFormat		Format,
	grBoolean			HasColorKey,
	uint32				ColorKey)
{
int32 mip;
grBitmap * Bmp = (grBitmap *)iBmp;

	assert( grBitmap_IsValid(Bmp) );
	assert( Target);
	assert(MaximumMip >= MinimumMip);
	assert( &Bmp != Target );

// <> lock-for-read : don't do peekready ?
//	grBitmap_PeekReady(Bmp);

	if ( MinimumMip < Bmp->Info.MinimumMip ||
//		 MaximumMip > Bmp->Info.MaximumMip
		MaximumMip >= MAXMIPLEVELS )
	{
		grErrorLog_AddString(-1,"LockForRead : invalid mip", NULL);
		return GR_FALSE;
	}

	if ( Bmp->LockCount < 0 || Bmp->LockOwner )
	{
		grErrorLog_AddString(-1,"LockForRead : already locked", NULL);
		return GR_FALSE;
	}
	
	//LockForRead must special case wavelet for making many mips at once
	if ( Bmp->Info.Format == GR_PIXELFORMAT_WAVELET )
	{
	grBitmap * Lock;
	grBitmap_Info * Infos[MAXMIPLEVELS];
	void * Bits[MAXMIPLEVELS];
	int32 i,MipCount;;

		assert(Bmp->Wavelet);
		assert(grPixelFormat_BytesPerPel(Format) > 0 );

		MipCount = MaximumMip - MinimumMip + 1;

		for(mip=MinimumMip;mip <= MaximumMip;mip ++)
		{
			i = mip - MinimumMip;
			Lock = grBitmap_CreateLock_CopyInfo(Bmp,1,mip);
			if ( ! Lock )
			{
				grBitmap_UnLockArray(Target,mip - MinimumMip);
				return GR_FALSE;
			}

			Lock->Info.Format = Format;
			Lock->Info.ColorKey = ColorKey;
			Lock->Info.HasColorKey = HasColorKey;
			if ( ! grBitmap_AllocSystemMip(Lock,0) )
			{
				grBitmap_UnLockArray(Target,mip - MinimumMip + 1);
				return GR_FALSE;
			}
			Target[i] = Lock;
			Infos[i] = &(Lock->Info);
			Bits[i] = Lock->Data[0];
		}

		if ( grWavelet_CanDecompressMips(Bmp->Wavelet,Infos[0]) )
		{
			if ( ! grWavelet_DecompressMips(Bmp->Wavelet,(const grBitmap_Info **)Infos,(const void **)Bits,MinimumMip,MaximumMip) )
			{
				grErrorLog_AddString(-1,"LockForRead : Wavelet_DecompressMips failed!", NULL);
				grBitmap_UnLockArray(Target,MipCount);
				return GR_FALSE;
			}
		}
		else
		{
			if ( MinimumMip != 0 )
			{
				grErrorLog_AddString(-1,"sorry, can't lock wavelet with minMip != 0", NULL);
				grBitmap_UnLockArray(Target,MipCount);
				return GR_FALSE;
			}

			if ( ! grWavelet_Decompress(Bmp->Wavelet,Infos[0],Bits[0]) )
			{
				grErrorLog_AddString(-1,"LockForRead : Wavelet_Decompress failed!", NULL);
				grBitmap_UnLockArray(Target,MipCount);
				return GR_FALSE;
			}

			// now make the mips

			for(i=1;i<MipCount;i++)
			{
				if ( ! grBitmap_UpdateMips_Data(Infos[i-1],Bits[i-1],Infos[i],Bits[i]) )
				{
					grErrorLog_AddString(-1,"LockForRead : UpdateMips_Data failed!", NULL);
					grBitmap_UnLockArray(Target,MipCount);
					return GR_FALSE;
				}
			}
		}
	}
	else
	{
		for(mip=MinimumMip;mip <= MaximumMip;mip ++)
		{
			Target[ mip - MinimumMip ] = grBitmap_CreateLockFromMip(Bmp,mip, Format,HasColorKey,ColorKey,1);
			if ( ! Target[ mip - MinimumMip ] )
			{
				grErrorLog_AddString(-1,"LockForRead : CreateLockFromMip failed", NULL);
				mip--;
				while(mip >= MinimumMip )
				{
					grBitmap_Destroy( & Target[ mip - MinimumMip ] );
					mip--;
				}
				return GR_FALSE;
			}
		}
	}

return GR_TRUE;
}

grBoolean grBitmap_UnLockArray_NoChange(grBitmap **Locks,int32 Size)
{
int i;
grBoolean Ret = GR_TRUE;
	assert(Locks);
	for(i=0;i<Size;i++)
	{
		if ( ! grBitmap_UnLock_NoChange(Locks[i]) )
			Ret = GR_FALSE;
	}
return Ret;
}

GRAPI grBoolean	GRCC grBitmap_UnLockArray(grBitmap **Locks,int32 Size)
{
int i;
grBoolean Ret = GR_TRUE;
	assert(Locks);
	for(i=0;i<Size;i++)
	{
		if ( ! grBitmap_UnLock(Locks[i]) )
			Ret = GR_FALSE;
	}
return Ret;
}

grBoolean grBitmap_UnLock_Internal(grBitmap *Bmp,grBoolean Apply)
{
grBoolean Ret = GR_TRUE;

	if ( ! Bmp )
	{
		grErrorLog_AddString(-1,"UnLock : bad bmp", NULL);
		return GR_FALSE;
	}

	assert( Bmp->LockCount == 0 );

	if ( Bmp->LockOwner )
	{
	int DoUpdate = 0;

		assert(Bmp->LockOwner->LockCount != 0);
		if ( Bmp->LockOwner->LockCount > 0 )
		{
			Bmp->LockOwner->LockCount --;
		}
		else if ( Bmp->LockOwner->LockCount < 0 )
		{
			Bmp->LockOwner->LockCount ++;

			if ( Apply )
			{
			grBitmap_Palette * Pal;

				Bmp->LockOwner->Modified[Bmp->Info.MinimumMip] = GR_TRUE;
			
				Pal = Bmp->DriverInfo.Palette ? Bmp->DriverInfo.Palette : Bmp->Info.Palette;
				if ( Pal )
					grBitmap_SetPalette(Bmp->LockOwner,Pal);

				// this palette will be destroyed later on

				Bmp->HasAverageColor = GR_FALSE;
			}

			if ( Bmp->LockOwner->LockCount == 0 && Apply )
			{
				// last unlock for write
				// if Bmp is on hardware, flag the Data[] as needing update
				if ( Bmp->DriverBitsLocked )
				{
					assert( Bmp->DriverHandle );
					Bmp->LockOwner->DriverDataChanged = GR_TRUE;
					DoUpdate = 1;
				}
				else
				{
					DoUpdate = -1;
				}
			}
		}
		
		if ( Bmp->DriverHandle && Bmp->DriverBitsLocked )
		{
			assert(Bmp->Driver);
			if ( ! Bmp->Driver->THandle_UnLock(Bmp->DriverHandle, Bmp->DriverMipLock - Bmp->DriverMipBase) )
			{
				grErrorLog_AddString(-1,"UnLock : thandle_unlock", NULL);
				Ret = GR_FALSE;
			}
			Bmp->DriverBitsLocked = GR_FALSE;
			Bmp->DriverMipLock = 0;
		}

		if ( Bmp->Alpha )
		{
			if ( ! grBitmap_UnLock(Bmp->Alpha) )
				Ret = GR_FALSE;

			Bmp->Alpha = NULL;
		}

		if ( DoUpdate )
		{
			assert(Bmp->LockOwner->LockCount == 0 );
			// we just finished unlocking a lock-for-write
			if ( DoUpdate > 0 )
			{
			//	don't update from driver -> system, leaved the changed data on the driver
			//	we've got DriverDataChanged
			//	if ( ! grBitmap_Update_DriverToSystem(Bmp) )
			//		Ret = GR_FALSE;
			}
			else
			{
				if ( Bmp->LockOwner->DriverHandle )
					if ( ! grBitmap_Update_SystemToDriver(Bmp->LockOwner) )
						Ret = GR_FALSE;
			}
		}

		// we did a CreateRef on the lockowner
		grBitmap_Destroy(&(Bmp->LockOwner));
		Bmp->LockOwner = NULL;

	}
	// else fail ?

	assert(Bmp->RefCount == 1);

	grBitmap_Destroy(&Bmp);

	assert(Bmp == NULL);

return Ret;
}

GRAPI grBoolean	GRCC grBitmap_UnLock(grBitmap *Bmp)
{
return grBitmap_UnLock_Internal(Bmp,GR_TRUE);
}

grBoolean grBitmap_UnLock_NoChange(grBitmap *Bmp)
{
return grBitmap_UnLock_Internal(Bmp,GR_FALSE);
}

GRAPI void *	GRCC grBitmap_GetBits(grBitmap *Bmp)
{
void * bits;

	assert( grBitmap_IsValid(Bmp) );

	if ( ! Bmp )
	{
		grErrorLog_AddString(-1,"GetBits : bad bmp", NULL);
		return NULL;
	}

	if ( ! Bmp->LockOwner )	// must be a lock!
	{
		grErrorLog_AddString(-1,"GetBits : not a lock", NULL);
		return NULL;
	}

	if ( Bmp->DriverHandle )
	{
		assert(Bmp->Driver);
		if ( ! Bmp->Driver->THandle_Lock(Bmp->DriverHandle,Bmp->DriverMipLock - Bmp->DriverMipBase,&bits) )
		{
			grErrorLog_AddString(-1,"GetBits : THandle_Lock", NULL);
			return NULL;
		}

		Bmp->DriverBitsLocked = GR_TRUE;
	}
	else if ( Bmp->Wavelet )
	{
		//{} with WaveletMipLock
		bits = Bmp->Wavelet;
	}
	else
	{
		bits = Bmp->Data[0];
	}

return bits;
}

/*}{ ************* _CreateLock_#? *********************/

grBitmap * grBitmap_CreateLock_CopyInfo(grBitmap *BmpSrc,int32 LockCnt,int32 mip)
{
grBitmap * Bmp;

	assert( grBitmap_IsValid(BmpSrc) );

	// all _CreateLocks go through here

	Bmp = grBitmap_Create_Base();
	if ( ! Bmp )
		return NULL;

	grBitmap_MakeMipInfo(&(BmpSrc->Info),mip,&(Bmp->Info));
	grBitmap_MakeMipInfo(&(BmpSrc->DriverInfo),mip,&(Bmp->DriverInfo));
	Bmp->DriverFlags = BmpSrc->DriverFlags;
	Bmp->DriverGamma = BmpSrc->DriverGamma;
	Bmp->DriverGammaLast = BmpSrc->DriverGammaLast;
		
	Bmp->Info.Palette = NULL;
	Bmp->DriverInfo.Palette = NULL;

	Bmp->Driver	= BmpSrc->Driver;
	Bmp->PreferredFormat= BmpSrc->PreferredFormat;

	Bmp->LockOwner = BmpSrc;
	grBitmap_CreateRef(BmpSrc); // we do a _Destroy() in UnLock()

	Bmp->HasWaveletOptions = BmpSrc->HasWaveletOptions;
	Bmp->WaveletOptions = BmpSrc->WaveletOptions;

	BmpSrc->LockCount += LockCnt;

return Bmp;
}

void grBitmap_MakeMipInfo(grBitmap_Info *Src,int32 mip,grBitmap_Info * Target)
{
	assert( Src && Target );
	assert( mip >= 0 && mip < MAXMIPLEVELS );
	*Target = *Src;

	Target->Width  = SHIFT_R_ROUNDUP(Target->Width,mip);
	Target->Height = SHIFT_R_ROUNDUP(Target->Height,mip);
	Target->Stride = SHIFT_R_ROUNDUP(Target->Stride,mip);
	Target->MinimumMip = Target->MaximumMip = mip;
}

grBitmap * grBitmap_CreateLockFromMip(grBitmap *Src,int32 mip,
	grPixelFormat Format,
	grBoolean	HasColorKey,
	uint32		ColorKey,
	int32			LockCnt)
{
grBitmap * Ret;

	assert( grBitmap_IsValid(Src) );
	if ( mip < 0 || mip >= MAXMIPLEVELS )
		return NULL;

	// LockForRead always goes through here

	// you can never lock Wavelet data (or can you?)
//	if ( grPixelFormat_BytesPerPel(Format) < 1 )
//		return NULL;

	if ( Src->DriverInfo.Format == Format &&
		 GR_BOOLSAME(Src->DriverInfo.HasColorKey,HasColorKey) &&
		 (!HasColorKey || Src->DriverInfo.ColorKey == ColorKey) &&
		 mip >= Src->DriverInfo.MinimumMip && mip <= Src->DriverInfo.MaximumMip )
	{
		return grBitmap_CreateLockFromMipOnDriver(Src,mip,LockCnt);
	}

	if ( Src->DriverHandle )
	{
		if ( ! grBitmap_Update_DriverToSystem(Src) )
		{
			return NULL;
		}
	}
		
	if ( ! Src->Data[mip] )
	{
		if ( ! grBitmap_MakeSystemMips(Src,mip,mip) )
			return NULL;
	}

	if ( Src->Info.Format == Format &&
		 GR_BOOLSAME(Src->Info.HasColorKey,HasColorKey) &&
		 (!HasColorKey || Src->Info.ColorKey == ColorKey) )
	{
		return grBitmap_CreateLockFromMipSystem(Src,mip,LockCnt);
	}

	Ret = grBitmap_CreateLock_CopyInfo(Src,LockCnt,mip);

	if ( ! Ret )
		return NULL;

	Ret->Info.Stride = Ret->Info.Width;	// {} ?

	Ret->Info.Format = Format;
	Ret->Info.ColorKey = ColorKey;
	Ret->Info.HasColorKey = HasColorKey;

	if ( grPixelFormat_HasPalette(Format) && Src->Info.Palette )
	{
		Ret->Info.Palette = Src->Info.Palette;
		grBitmap_Palette_CreateRef(Ret->Info.Palette);
	}

	assert( Ret->Alpha == NULL );
	if ( ! grPixelFormat_HasGoodAlpha(Format) && Src->Alpha )
	{
		if ( ! grBitmap_LockForRead(Src->Alpha,&(Ret->Alpha),mip,mip,GR_PIXELFORMAT_8BIT_GRAY,0,0) )
		{
			grErrorLog_AddString(-1,"CreateLockFromMip : LockForRead failed", NULL);
			grBitmap_Destroy(&Ret);
			return NULL;
		}
		assert( Ret->Alpha );
	}

	assert( grBitmap_IsValid(Ret) );

	if (	Src->Info.Format == GR_PIXELFORMAT_WAVELET &&
			Ret->Info.Format == GR_PIXELFORMAT_WAVELET )
	{
		assert(Src->Wavelet);
		Ret->Wavelet = Src->Wavelet;
		grWavelet_CreateRef(Ret->Wavelet);

		Ret->WaveletMipLock = mip;
	}
	else
	{
		assert( Ret->Info.Format != GR_PIXELFORMAT_WAVELET);

		if ( ! grBitmap_AllocSystemMip(Ret,0) )
		{
			grBitmap_Destroy(&Ret);
			return NULL;
		}

		if ( ! grBitmap_BlitMip( Src, mip, Ret, 0 ) )
		{
			grErrorLog_AddString(-1,"CreateLockFromMip : BlitMip failed", NULL);
			grBitmap_Destroy(&Ret);
			return NULL;
		}
	}

return Ret;
}

grBitmap * grBitmap_CreateLockFromMipSystem(grBitmap *Src,int32 mip,int32 LockCnt)
{
grBitmap * Ret;

	assert( grBitmap_IsValid(Src) );
	if ( mip < Src->Info.MinimumMip || mip >= MAXMIPLEVELS )
		return NULL;

	// you can never lock Wavelet data {}
	//if ( grPixelFormat_BytesPerPel(Src->Info.Format) < 1 )
	//	return NULL;

	if ( ! Src->Data[mip] )
	{
		if ( ! grBitmap_MakeSystemMips(Src,mip,mip) )
			return NULL;
	}

	Ret = grBitmap_CreateLock_CopyInfo(Src,LockCnt,mip);

	if ( ! Ret ) return NULL;

	Ret->Data[0] = Src->Data[mip];

	if ( Src->Info.Format == GR_PIXELFORMAT_WAVELET )
	{
		assert(Src->Wavelet);
		Ret->Wavelet = Src->Wavelet;
		Ret->WaveletMipLock = mip;
	}

	Ret->Info.Palette = Src->Info.Palette;
	if ( Ret->Info.Palette )
		grBitmap_Palette_CreateRef(Ret->Info.Palette);

	Ret->DataOwner = Src;
	grBitmap_CreateRef(Src);

	assert( Ret->Alpha == NULL );
	if ( ! grPixelFormat_HasGoodAlpha(Src->Info.Format) && Src->Alpha )
	{
		if ( ! grBitmap_LockForRead(Src->Alpha,&(Ret->Alpha),mip,mip,GR_PIXELFORMAT_8BIT_GRAY,0,0) )
		{
			grErrorLog_AddString(-1,"CreateLockFromMipSystem : LockForRead failed", NULL);
			grBitmap_Destroy(&Ret);
			return NULL;
		}
		assert( Ret->Alpha );
	}

	assert( grBitmap_IsValid(Ret) );

return Ret;
}

grBitmap * grBitmap_CreateLockFromMipOnDriver(grBitmap *Src,int32 mip,int32 LockCnt)
{
grBitmap * Ret;

	assert( grBitmap_IsValid(Src) );
	if ( ! Src->DriverHandle || ! Src->Driver || Src->DriverMipLock || mip < Src->DriverInfo.MinimumMip || mip > Src->DriverInfo.MaximumMip )
		return NULL;

	// the driver can never have Wavelet data
	// {} it could have S3TC data, though..
	assert( grPixelFormat_BytesPerPel(Src->DriverInfo.Format) > 0);

	Ret = grBitmap_CreateLock_CopyInfo(Src,LockCnt,mip);

	if ( ! Ret ) return NULL;

	Ret->DriverMipLock = mip;
	Ret->DriverHandle = Src->DriverHandle;
	Ret->DriverMipBase = Src->DriverMipBase;

	Ret->DataOwner = Src;
	grBitmap_CreateRef(Src);

	Ret->DriverInfo.Palette = Src->DriverInfo.Palette;

	if ( ! grBitmap_MakeDriverLockInfo(Ret,mip,&(Ret->DriverInfo)) )
	{
		grErrorLog_AddString(-1,"CreateLockFromMipOnDriver : UpdateInfo failed", NULL);
		grBitmap_Destroy(&Ret);
		return NULL;
	}

	assert(Ret->DriverInfo.Palette == Src->DriverInfo.Palette);

	Ret->Info = Ret->DriverInfo;	//{} shouldn't be necessary
	
	if ( Ret->DriverInfo.Palette )
		grBitmap_Palette_CreateRef(Ret->DriverInfo.Palette);
	if ( Ret->Info.Palette )
		grBitmap_Palette_CreateRef(Ret->Info.Palette);

	assert( grBitmap_IsValid(Ret) );

return Ret;
}

/*}{ ************* Driver Attachment *********************/

#define MAX_DRIVER_FORMATS (100)

static grBoolean EnumPFCB(grRDriver_PixelFormat *pFormat,void *Context)
{
grRDriver_PixelFormat **pDriverFormatsPtr;
	pDriverFormatsPtr = (grRDriver_PixelFormat **)Context;
	**pDriverFormatsPtr = *pFormat;
	(*pDriverFormatsPtr) += 1;
return GR_TRUE;
}

static int32 NumBitsOn(uint32 val)
{
uint32 count = 0;
	while(val)
	{
		count += val&1;
		val >>= 1;
	}
return count;
}

static grBoolean IsInArray(uint32 Val,uint32 *Array,int32 Len)
{
	while(Len--)
	{
		if ( Val == *Array )
			return GR_TRUE;
		Array--;
	}
return GR_FALSE;
}

grBoolean grBitmap_ChooseDriverFormat(
	grPixelFormat	SeekFormat1,
	grPixelFormat	SeekFormat2,
	grBoolean		SeekCK,
	grBoolean		SeekAlpha,
	grBoolean		SeekSeparates,
	uint32			SeekFlags,
	grRDriver_PixelFormat *DriverFormatsArray,int32 ArrayLen,
	grRDriver_PixelFormat *pTarget)
{
int32 i,rating;
int32 FormatRating[MAX_DRIVER_FORMATS];
grRDriver_PixelFormat * DriverPalFormat;
grRDriver_PixelFormat * pf;
grBoolean FoundAlpha;
uint32 SeekMajor,SeekMinor;
const grPixelFormat_Operations *seekops,*pfops;

	assert(pTarget && DriverFormatsArray && ArrayLen > 0);

	if ( SeekAlpha )
		SeekCK = GR_FALSE;	// you can't have both

	if ( SeekFlags & RDRIVER_PF_ALPHA_SURFACE )
		SeekFlags = RDRIVER_PF_ALPHA_SURFACE;
	else if ( SeekFlags & RDRIVER_PF_PALETTE )
		SeekFlags = RDRIVER_PF_PALETTE;

	SeekMajor = SeekFlags & RDRIVER_PF_MAJOR_MASK;
	SeekMinor = SeekFlags - SeekMajor;

	pf = DriverFormatsArray;
	DriverPalFormat = NULL;
	for(i=0;i<ArrayLen;i++)
	{
		if ( pf->Flags & RDRIVER_PF_PALETTE )
		{
			if ( ! DriverPalFormat || grPixelFormat_HasGoodAlpha(pf->PixelFormat) )
				DriverPalFormat = pf;
		}
		pf++;
	}

	seekops = grPixelFormat_GetOperations(SeekFormat1);

	for(i=0;i<ArrayLen;i++)
	{
		pf = DriverFormatsArray + i;
		rating = 0;

		/***
		{}

		this is the code to try to pick the closest format the driver has to offer
		the choosing precedence is :

			1. if want alpha, get alpha
			2. match the _3D_ type flags exactly (PF_MAJOR)
			3. match PreferredFormat
			4. match the bitmap's system format
			5. match up the pixel formats masks

		we use fuzzy logic : we apply rating to all the matches and then choose
		the one with the best rating.

		possible todos :
			1. be aware of memory use and format-conversion times, and penalize
				or favor formats accordingly

		note : we currently try to use 1555 for colorkey.

		***/

		if ( (pf->Flags & RDRIVER_PF_MAJOR_MASK) == SeekMajor )
		{
			rating += 1<<23;
		}
		else if ( (pf->Flags & SeekMajor) != SeekMajor )
		{
		    FormatRating[i] = 0;
			continue;
		}
		else
		{
			// advantage to as few different major flags as possible
			// higher priority than similarity of pixelformats
			rating += (32 - NumBitsOn( (pf->Flags ^ SeekFlags) & RDRIVER_PF_MAJOR_MASK ))<<6;
		}

		pfops = grPixelFormat_GetOperations(pf->PixelFormat);

		if ( pf->PixelFormat == SeekFormat1 )
		{
			rating += 1<<16;
		}
		else if ( pf->PixelFormat == SeekFormat2 )
		{
			rating += 1<<15;
		}
		else if ( grPixelFormat_IsRaw(SeekFormat1) && grPixelFormat_IsRaw(pf->PixelFormat) )
		{
		int32 R,G,B,A;
			// measure similarity
			R = (seekops->RMask >> seekops->RShift) ^ (pfops->RMask >> pfops->RShift);
			G = (seekops->GMask >> seekops->GShift) ^ (pfops->GMask >> pfops->GShift);
			B = (seekops->BMask >> seekops->BShift) ^ (pfops->BMask >> pfops->BShift);
			A = (seekops->AMask >> seekops->AShift) ^ (pfops->AMask >> pfops->AShift);
			R = 8 - NumBitsOn(R);
			G = 8 - NumBitsOn(G);
			B = 8 - NumBitsOn(B);
			A = 4*(8 - NumBitsOn(A)); // right number of A bits is molto importante
			rating += (R + G + B + A);
		}
		else
		{
			// one of the formats is not raw
			// palettized ? compressed ?
			rating += 16;
		}

		FoundAlpha = GR_FALSE;

		if ( NumBitsOn(pfops->AMask) > 2 )
			FoundAlpha = GR_TRUE;

		if ( grPixelFormat_HasPalette(pf->PixelFormat) )
		{
            // if Pixelformat is 8BIT_PAL , look at the palette's format to see if it
			//		had alpha!
			assert(DriverPalFormat);
			if ( grPixelFormat_HasGoodAlpha(DriverPalFormat->PixelFormat) )
				FoundAlpha = GR_TRUE;
		}

		if ( SeekAlpha && FoundAlpha )
			rating += 1<<24; // HIGHEST ! (except separates)
		else if ( (!SeekAlpha) && (!FoundAlpha) )
			rating += 1<<10; // LOWEST
		else if ( SeekAlpha )
		{
			if ( NumBitsOn(pfops->AMask) || pf->Flags & RDRIVER_PF_CAN_DO_COLORKEY )
			{
				// sought Alpha, found CK
				rating += 1<<19;
			}
		}

		if ( (pf->Flags & RDRIVER_PF_HAS_ALPHA_SURFACE) && SeekSeparates )
		{
			// separates doesn't count as alpha unless we asked for it !
			rating += 1<<25; // VERY HIGHEST !
		}
	
		if ( SeekCK ) 
		{
			if ( pf->PixelFormat == GR_PIXELFORMAT_16BIT_1555_ARGB )
			{
				rating += 1<<23; // just lower than alpha
			}
			else if ( FoundAlpha ) //better than nothing!
			{
				rating += 1<<18;	// higher than !FoundAlpha
			}
		}

		if ( GR_BOOLSAME((pf->Flags & RDRIVER_PF_CAN_DO_COLORKEY),SeekCK) )
		{
			rating += 1<<17;
		}

		FormatRating[i] = rating;
	}

	rating = 0;

	for(i=0;i<ArrayLen;i++)
	{
		if ( FormatRating[i] > rating )
		{
			rating = FormatRating[i];
			*pTarget = DriverFormatsArray[i];
		}
	}

	if ( rating == 0)
	{
		grErrorLog_AddString(-1,"ChooseDriverFormat : no valid formats found!", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}
#if 0
grBoolean grBitmap_ChooseDriverFormat(
	grPixelFormat	SeekFormat1,
	grPixelFormat	SeekFormat2,
	grBoolean		SeekCK,
	grBoolean		SeekAlpha,
	grBoolean		SeekSeparates,
	uint32			SeekFlags,
	grRDriver_PixelFormat *DriverFormatsArray,int32 ArrayLen,
	grRDriver_PixelFormat *pTarget)
{
int32 i,rating;
int32 FormatRating[MAX_DRIVER_FORMATS];
grRDriver_PixelFormat * DriverPalFormat;
grRDriver_PixelFormat * pf;
grBoolean FoundAlpha;
uint32 SeekMajor,SeekMinor;
const grPixelFormat_Operations *seekops,*pfops;

	assert(pTarget && DriverFormatsArray && ArrayLen > 0);

#if 0 // @@
	if ( SeekAlpha )
		SeekCK = GR_FALSE;	// you can't have both, you bastard!
#endif

	if ( SeekFlags & RDRIVER_PF_ALPHA_SURFACE )
		SeekFlags = RDRIVER_PF_ALPHA_SURFACE;
	else if ( SeekFlags & RDRIVER_PF_PALETTE )
		SeekFlags = RDRIVER_PF_PALETTE;

	if ( ! SeekFormat1 )
		SeekFormat1 = SeekFormat2;

	SeekMajor = SeekFlags & RDRIVER_PF_MAJOR_MASK;
	SeekMinor = SeekFlags - SeekMajor;

	pf = DriverFormatsArray;
	DriverPalFormat = NULL;
	for(i=0;i<ArrayLen;i++)
	{
		if ( pf->Flags & RDRIVER_PF_PALETTE )
		{
			assert( grPixelFormat_IsRaw(pf->PixelFormat) );
			if ( ! DriverPalFormat || grPixelFormat_HasGoodAlpha(pf->PixelFormat) )
				DriverPalFormat = pf;
		}
		pf++;
	}

	seekops = grPixelFormat_GetOperations(SeekFormat1);

	for(i=0;i<ArrayLen;i++)
	{
		pf = DriverFormatsArray + i;
		rating = 0;

		/***
		{}

		this is the code to try to pick the closest format the driver has to offer
		the choosing precedence is :

			1. if want alpha, get alpha
			2. match the _3D_ type flags exactly
			3. match PreferredFormat
			4. match the bitmap's system format
			5. match up the formats masks

		***/

		if ( (pf->Flags & RDRIVER_PF_MAJOR_MASK) == SeekMajor )
		{
			rating += 1<<23;
		}
		else if ( (pf->Flags & SeekMajor) != SeekMajor )
		{
		    FormatRating[i] = 0;
			continue;
		}
		else
		{
			// advantage to as few different major flags as possible
			// higher priority than similarity of pixelformats
			rating += (32 - NumBitsOn( (pf->Flags ^ SeekFlags) & RDRIVER_PF_MAJOR_MASK )) <<6;
		}

		pfops = grPixelFormat_GetOperations(pf->PixelFormat);

		if ( pf->PixelFormat == SeekFormat1 )
		{
			rating += 1<<22;
		}
		else if ( pf->PixelFormat == SeekFormat2 )
		{
			rating += 1<<21;
		}
		else if ( grPixelFormat_IsRaw(SeekFormat1) && grPixelFormat_IsRaw(pf->PixelFormat) )
		{
		int32 R,G,B,A;
			// measure similarity
			R = (seekops->RMask >> seekops->RShift) ^ (pfops->RMask >> pfops->RShift);
			G = (seekops->GMask >> seekops->GShift) ^ (pfops->GMask >> pfops->GShift);
			B = (seekops->BMask >> seekops->BShift) ^ (pfops->BMask >> pfops->BShift);
			A = (seekops->AMask >> seekops->AShift) ^ (pfops->AMask >> pfops->AShift);
			R = 8 - NumBitsOn(R);
			G = 8 - NumBitsOn(G);
			B = 8 - NumBitsOn(B);
			A = 4*(8 - NumBitsOn(A)); // right number of A bits is molto importante
			rating += R + G + B + A;
		}

		FoundAlpha = GR_FALSE;

		if ( NumBitsOn(pfops->AMask) > 2 )
			FoundAlpha = GR_TRUE;

		if ( grPixelFormat_HasPalette(pf->PixelFormat) )
		{
                        // if Pixelformat is 8BIT_PAL , look at the palette's format to see if it
			//		had alpha!
			assert(DriverPalFormat);
			if ( grPixelFormat_HasGoodAlpha(DriverPalFormat->PixelFormat) )
				FoundAlpha = GR_TRUE;
		}

		if ( SeekAlpha && FoundAlpha )
			rating += 1<<24; // HIGHEST ! (except separates)
		else if ( (!SeekAlpha) && (!FoundAlpha) )
			rating += 1<<16; // LOWEST

		if ( (pf->Flags & RDRIVER_PF_HAS_ALPHA_SURFACE) && SeekSeparates )
		{
			// separates doesn't count as alpha unless we asked for it !
			rating += 1<<25; // VERY HIGHEST !
		}
	
		if ( (pf->PixelFormat == GR_PIXELFORMAT_16BIT_1555_ARGB) && SeekCK )
		{
			rating += 1<<23; // just lower than alpha
		}
		else if ( GR_BOOLSAME((pf->Flags & RDRIVER_PF_CAN_DO_COLORKEY),SeekCK) )
		{
			rating += 1<<20;
		}

		if ( SeekCK && FoundAlpha ) //better than nothing!
		{
			rating += 1<<17;	// higher than !FoundAlpha
		}

		FormatRating[i] = rating;
	}

	rating = 0;

	for(i=0;i<ArrayLen;i++)
	{
		if ( FormatRating[i] > rating )
		{
			rating = FormatRating[i];
			*pTarget = DriverFormatsArray[i];
		}
	}

	if ( rating == 0)
	{
		grErrorLog_AddString(-1,"ChooseDriverFormat : no valid formats found!", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}
#endif

grTexture * grBitmap_CreateTHandle(DRV_Driver *Driver,int32 Width,int32 Height,int32 NumMipLevels,
			grPixelFormat SeekFormat1,grPixelFormat SeekFormat2,grBoolean SeekCK,grBoolean SeekAlpha,grBoolean SeekSeparates,uint32 DriverFlags)
{
grRDriver_PixelFormat DriverFormats[MAX_DRIVER_FORMATS];
grRDriver_PixelFormat *DriverFormatsPtr;
int32 DriverFormatsCount;
grRDriver_PixelFormat DriverFormat;
grTexture * Ret;

	DriverFormatsPtr = DriverFormats;
	Driver->EnumPixelFormats(EnumPFCB,&DriverFormatsPtr);
	DriverFormatsCount = ((uint32)DriverFormatsPtr - (uint32)DriverFormats)/sizeof(*DriverFormatsPtr);
	assert(DriverFormatsCount < MAX_DRIVER_FORMATS && DriverFormatsCount >= 0);

	if ( DriverFormatsCount == 0 )
	{
		grErrorLog_AddString(-1,"Bitmap_CreateTHandle : no formats found!", NULL);
		return NULL;
	}

	if ( ! SeekFormat1 )
		SeekFormat1 = SeekFormat2;
	else if ( ! SeekFormat2 )
		SeekFormat2 = SeekFormat1;

	assert( grPixelFormat_IsValid(SeekFormat1) );
	assert( grPixelFormat_IsValid(SeekFormat2) );

	// now choose DriverFormat
	if ( ! grBitmap_ChooseDriverFormat(SeekFormat1,SeekFormat2,SeekCK,SeekAlpha,SeekSeparates,DriverFlags,
										DriverFormats,DriverFormatsCount,&DriverFormat) )
		return NULL;

	assert( grPixelFormat_IsValid(DriverFormat.PixelFormat) );

#if 1 //{
	Log_Printf("Bitmap : Chose %s for %s",
		grPixelFormat_Description(DriverFormat.PixelFormat),
		grPixelFormat_Description(SeekFormat1));

	if ( SeekFormat1 != SeekFormat2 )
		Log_Printf(" (%s)",grPixelFormat_Description(SeekFormat2));
	if ( SeekCK )
		Log_Printf(" (sought CK)");
	if ( SeekAlpha )
		Log_Printf(" (sought Alpha)");
	if ( DriverFormat.Flags & RDRIVER_PF_2D )
		Log_Printf(" (2D)");
	if ( DriverFormat.Flags & RDRIVER_PF_3D )
		Log_Printf(" (3D)");
	if ( DriverFormat.Flags & RDRIVER_PF_CAN_DO_COLORKEY )
		Log_Printf(" (can CK)");
	if ( DriverFormat.Flags & RDRIVER_PF_PALETTE )
		Log_Printf(" (Palette)");
	if ( DriverFormat.Flags & RDRIVER_PF_ALPHA_SURFACE )
		Log_Printf(" (Alpha)");
	if ( DriverFormat.Flags & RDRIVER_PF_LIGHTMAP )
		Log_Printf(" (Lightmap)");

	Log_Printf("\n");
#endif //}

	Ret = Driver->THandle_Create(
		Width,Height,
		NumMipLevels,
		&DriverFormat);

	if ( ! Ret )
	{
		grErrorLog_AddString(-1, Driver->LastErrorStr, NULL);
		grErrorLog_AddString(-1,"Bitmap_CreateTHandle : Driver->THandle_Create failed", NULL);
	}

return Ret;
}

GRAPI	grBoolean	GRCC	grBitmap_HasAlpha(const grBitmap * Bmp)
{
	assert( grBitmap_IsValid(Bmp) );
	
	if ( Bmp->Alpha )
		return GR_TRUE;

	if ( Bmp->Wavelet )
		return grWavelet_HasAlpha(Bmp->Wavelet);

	if ( grPixelFormat_HasGoodAlpha(Bmp->Info.Format) )
		return GR_TRUE;

	if ( grPixelFormat_HasPalette(Bmp->Info.Format) && Bmp->Info.Palette )
	{
		if ( grPixelFormat_HasGoodAlpha(Bmp->Info.Palette->Format) )
			return GR_TRUE;
	}	

return GR_FALSE;
}

grBoolean	BITMAP_GR_INTERNAL grBitmap_AttachToDriver(grBitmap *Bmp, 
	DRV_Driver * Driver, uint32 DriverFlags)
{

	/**************
	* When you want to change the Driver,
	* I still need a copy of the old one to get the bits out
	* of the old THandles.  That is:
	* 
	* 	AttachDriver(Bmp,Driver)
	* 	<do stuff>
	* 	Change Driver Entries
	* 	AttachDriver(Bmp,Driver)
	* 
	* is forbidden!  The two different options are :
	* 
	* 	1.
	* 
	* 	AttachDriver(Bmp,Driver)
	* 	<do stuff>
	* 	DetachDriver(Bmp)
	* 	Change Driver Entries
	* 	AttachDriver(Bmp,Driver)
	* 
	* 	2.
	* 
	* 	AttachDriver(Bmp,Driver1)
	* 	<do stuff>
	* 	Driver2 = copy of Driver1
	* 	Change Driver2 Entries
	* 	AttachDriver(Bmp,Driver2)
	* 	Free Driver1
	* 
	* This isn't so critical when just changing modes,
	* but is critical when changing drivers.
	* 
	****************/

	assert( grBitmap_IsValid(Bmp) );

	if ( Bmp->LockOwner || Bmp->DataOwner || Bmp->LockCount )
	{
		grErrorLog_AddString(-1,"AttachToDriver : not an isolated bitmap", NULL);
		return GR_FALSE;
	}

	if ( Bmp->DriverHandle && Bmp->Driver == Driver )
	{
		assert( DriverFlags == 0 || DriverFlags == Bmp->DriverFlags );
		return GR_TRUE;
	}

	if ( ! grBitmap_DetachDriver(Bmp,GR_TRUE) )
	{
		grErrorLog_AddString(-1,"AttachToDriver : detach failed", NULL);
		return GR_FALSE;
	}

	if ( DriverFlags == 0 )
	{
		DriverFlags = Bmp->DriverFlags;
		if ( ! DriverFlags )
		{
			//	return GR_FALSE;
			// ? {}
			DriverFlags = RDRIVER_PF_3D;
		}
	}

	if ( Driver )
	{
	int32 NumMipLevels;
	int32 Width,Height;
	grBoolean WantAlpha;
	grTexture * DriverHandle;

//		grBitmap_PeekReady(Bmp); // this is about to be done in UpdateSystem anyway..

//		if ( Bmp->DriverFlags & RDRIVER_PF_COMBINE_LIGHTMAP )
//			Bmp->SeekMipCount = max(Bmp->SeekMipCount,4);

		NumMipLevels = max(Bmp->SeekMipCount,(Bmp->Info.MaximumMip + 1));
		if ( NumMipLevels > 4 ) NumMipLevels = 4; // {} kind of a hack, our drivers ignore mips > 4

		// make sizes power-of-two and square
		// {} note : must let drivers do this to correctly scale UV's 
		Width	= Bmp->Info.Width;
		Height	= Bmp->Info.Height;

		WantAlpha = grBitmap_HasAlpha(Bmp);
		if ( grPixelFormat_HasGoodAlpha(Bmp->PreferredFormat) )
			WantAlpha = GR_TRUE;

		assert( grBitmap_IsValid(Bmp) );

		DriverHandle = grBitmap_CreateTHandle(Driver,Width,Height,NumMipLevels,
			Bmp->PreferredFormat,Bmp->Info.Format,Bmp->Info.HasColorKey,
			WantAlpha, (Bmp->Alpha) ? GR_TRUE : GR_FALSE,
			DriverFlags);

		assert( grBitmap_IsValid(Bmp) );

		if ( ! DriverHandle )
			return GR_FALSE;

		Bmp->DriverHandle = DriverHandle;
		Bmp->Driver = Driver;
		Bmp->DriverFlags = DriverFlags;

#ifdef _DEBUG
		Bmp->DriverInfo = Bmp->Info;
		assert( grBitmap_IsValid(Bmp) );
#endif
		clear(&(Bmp->DriverInfo));

		Bmp->DriverMipBase = 0;
		if ( ! grBitmap_MakeDriverLockInfo(Bmp,0,&(Bmp->DriverInfo)) )
		{
			grErrorLog_AddString(-1,"AttachToDriver : updateinfo", NULL);
			return GR_FALSE;
		}

		Bmp->DriverInfo.MinimumMip = 0;

		Width = Bmp->Info.Width / Bmp->DriverInfo.Width;
		while( Width > 1 )
		{
			Bmp->DriverInfo.MinimumMip ++;
			Width >>= 1;
		}

		Bmp->DriverMipBase = Bmp->DriverInfo.MinimumMip;

		Bmp->DriverInfo.MaximumMip = Bmp->DriverInfo.MinimumMip + NumMipLevels - 1;
		
		assert( grBitmap_IsValid(Bmp) );

/*******
		if ( grPixelFormat_HasPalette(Bmp->DriverInfo.Format) )
		{
			if ( ! Bmp->Info.Palette )
			{
				Bmp->Info.Palette = createPaletteFromBitmapNoLock(Bmp, GR_FALSE);
				if ( ! Bmp->Info.Palette )
				{
					grErrorLog_AddString(-1,"AttachToDriver : createPalette failed!", NULL);
					grBitmap_Palette_Destroy(&(Bmp->DriverInfo.Palette));
					Driver->THandle_Destroy(Bmp->DriverHandle);
					Bmp->DriverHandle = NULL;
					return GR_FALSE;
				}
			}

			if ( ! grBitmap_AllocPalette(&(Bmp->DriverInfo), Bmp->Info.Palette->Format,Bmp->Driver) )
			{
				grErrorLog_AddString(-1,"AttachToDriver : Palette_Create", NULL);
				Driver->THandle_Destroy(Bmp->DriverHandle);
				Bmp->DriverHandle = NULL;
				return GR_FALSE;
			}
			assert( Bmp->DriverInfo.Palette->DriverHandle );

			if ( ! Bmp->DriverInfo.HasColorKey )
			{
				Bmp->DriverInfo.HasColorKey = Bmp->DriverInfo.Palette->HasColorKey;
				Bmp->DriverInfo.ColorKey = Bmp->DriverInfo.Palette->ColorKey;
			}

			grBitmap_Palette_Copy(Bmp->Info.Palette,Bmp->DriverInfo.Palette);

			if ( ! Driver->THandle_SetPalette(Bmp->DriverHandle,Bmp->DriverInfo.Palette->DriverHandle) )
			{
				grErrorLog_AddString(-1,"AttachToDriver : THandle_SetPalette", NULL);
				grBitmap_Palette_Destroy(&(Bmp->DriverInfo.Palette));
				Driver->THandle_Destroy(Bmp->DriverHandle);
				Bmp->DriverHandle = NULL;
				return GR_FALSE;
			}
		}
*******/

		assert( grBitmap_IsValid(Bmp) );

#ifdef DONT_DEC_STREAMING
		if (	Bmp->StreamingStatus >= GR_BITMAP_STREAMING_STARTED && 
				Bmp->StreamingStatus < GR_BITMAP_STREAMING_DATADONE )
		{
			assert( Bmp->Wavelet );
			assert( grWavelet_StreamingJob(Bmp->Wavelet) );
			Bmp->StreamingTHandle = GR_TRUE;
			return GR_TRUE;
		}
#endif

		if ( ! grBitmap_Update_SystemToDriver(Bmp) )
		{
			grErrorLog_AddString(-1,"AttachToDriver : Update_SystemToDriver", NULL);
			Driver->THandle_Destroy(Bmp->DriverHandle);
			Bmp->DriverHandle = NULL;
			return GR_FALSE;
		}

		// {} Palette : Update_System calls Blit_Data, which should build it for us 
		//		if Driver is pal & System isn't
	}

return GR_TRUE;
}

grBoolean grBitmap_FixDriverFlags(uint32 *pFlags)
{
uint32 DriverFlags;
	assert(pFlags);
	DriverFlags = *pFlags;
	
	if ( DriverFlags & RDRIVER_PF_COMBINE_LIGHTMAP )
		DriverFlags |= RDRIVER_PF_3D;
	if ( DriverFlags & RDRIVER_PF_CAN_DO_COLORKEY )
	{
		// <> someone is doing this!
		// bad!
		DriverFlags ^= RDRIVER_PF_CAN_DO_COLORKEY;
		//	return GR_FALSE;
	}
	if ( (DriverFlags & RDRIVER_PF_COMBINE_LIGHTMAP) &&
		(DriverFlags & (RDRIVER_PF_LIGHTMAP | RDRIVER_PF_PALETTE) ) )
		return GR_FALSE;
	if ( NumBitsOn(DriverFlags & RDRIVER_PF_MAJOR_MASK) == 0 )
		return GR_FALSE;
	*pFlags = DriverFlags;
return GR_TRUE;
}

grBoolean BITMAP_GR_INTERNAL grBitmap_SetDriverFlags(grBitmap *Bmp,uint32 Flags)
{
	assert( grBitmap_IsValid(Bmp) );
	assert(Flags);
	if ( ! grBitmap_FixDriverFlags(&Flags) )
	{
		Bmp->DriverFlags = 0;
		return GR_FALSE;
	}
	Bmp->DriverFlags = Flags;
return GR_TRUE;
}

grBoolean BITMAP_GR_INTERNAL grBitmap_DetachDriver(grBitmap *Bmp,grBoolean DoUpdate)
{
grBoolean Ret = GR_TRUE;

	assert(grBitmap_IsValid(Bmp) );

	if ( Bmp->LockOwner || Bmp->DataOwner || Bmp->LockCount )
	{
		grErrorLog_AddString(-1,"DetachDriver : not an isolated bitmap!", NULL);
		return GR_FALSE;
	}

	if ( Bmp->RefCount > 1 )
		DoUpdate = GR_TRUE;

	if ( Bmp->Driver && Bmp->DriverHandle )
	{
		if ( DoUpdate )
		{
			if ( ! grBitmap_Update_DriverToSystem(Bmp) )
			{
				grErrorLog_AddString(-1,"DetachDriver : Update_DriverToSystem", NULL);
				Ret = GR_FALSE;
			}
			assert(Bmp->DriverDataChanged == GR_FALSE);
		}
			Bmp->Driver->THandle_Destroy(Bmp->DriverHandle);
		Bmp->DriverHandle = NULL;
	}

	if ( Bmp->DriverInfo.Palette )
	{
		// save it for later in case we re-attach
		if ( ! Bmp->Info.Palette )
		{
		grBitmap_Palette * NewPal;
		grPixelFormat Format;
			Format = Bmp->DriverInfo.Palette->Format;
			NewPal = grBitmap_Palette_Create(Format,256);
			if ( NewPal )
			{
				if ( grBitmap_Palette_Copy(Bmp->DriverInfo.Palette,NewPal) )
				{
					Bmp->Info.Palette = NewPal;
				}
				else
				{
					grBitmap_Palette_Destroy(&NewPal);
				}
			}
		}

		grBitmap_Palette_Destroy(&(Bmp->DriverInfo.Palette));
	}

	if ( Bmp->Alpha )
	{
		if ( ! grBitmap_DetachDriver(Bmp->Alpha,DoUpdate) )
		{
			grErrorLog_AddString(-1,"DetachDriver : detach alpha", NULL);
			Ret = GR_FALSE;
		}
	}

	Bmp->DriverInfo.Width = Bmp->DriverInfo.Height = Bmp->DriverInfo.Stride = 0;
	Bmp->DriverInfo.MinimumMip = Bmp->DriverInfo.MaximumMip = 0;
	Bmp->DriverInfo.ColorKey = Bmp->DriverInfo.HasColorKey = 0;
	Bmp->DriverInfo.Format = GR_PIXELFORMAT_NO_DATA;
	Bmp->DriverInfo.Palette = NULL;
	Bmp->DriverMipLock = 0;
	Bmp->DriverBitsLocked = GR_FALSE;
	Bmp->DriverDataChanged = GR_FALSE;
	Bmp->DriverHandle = NULL;
	Bmp->Driver = NULL;

	//Bmp->DriverFlags left intentionally !

return Ret;
}

grBoolean GRCC grBitmap_SetGammaCorrection_DontChange(grBitmap *Bmp,grFloat Gamma)
{
	assert(grBitmap_IsValid(Bmp));
	assert( Gamma > 0.0f );

	if ( ! Bmp->DriverGammaSet )
	{
		Bmp->DriverGammaLast = Bmp->DriverGamma;
		Bmp->DriverGamma = Gamma;
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_SetGammaCorrection(grBitmap *Bmp,grFloat Gamma,grBoolean Apply)
{
	assert(grBitmap_IsValid(Bmp));
	assert( Gamma > 0.0f );

	/***

	there are actually some anomalies involved in exposing this to the user:
		if the driver's gamma correction is turned on, then this Gamma will be applied *in addition* to the driver gamma.
	There's no easy way to avoid that problem.

	we like to expose this to the user so that they can disable software gamma correction on some bitmaps
		(eg. procedurals)
	perhaps provide a Bitmap_DisableGamma(Bmp) instead of SetGamma ?

	**/

	if ( Apply && Bmp->DriverHandle )
	{
		grBitmap_PeekReady(Bmp);
		if ( fabs(Bmp->DriverGamma - Gamma) > 0.1f )
		{
			if ( grPixelFormat_BytesPerPel(Bmp->Info.Format) == 0 && Bmp->DriverHandle )
			{
				// system format is compressed, and Bmp is on the card

				if ( (Bmp->DriverGammaLast >= Bmp->DriverGamma && Bmp->DriverGamma >= Gamma) ||
					 (Bmp->DriverGammaLast <= Bmp->DriverGamma && Bmp->DriverGamma <= Gamma) )
				{
					// moving in the same direction

					// invert the old
					if ( ! grBitmap_Gamma_Apply(Bmp,GR_TRUE) )
						return GR_FALSE;

					Bmp->DriverGammaLast = Bmp->DriverGamma;
					Bmp->DriverGamma = Gamma;

					// apply the new
					if ( ! grBitmap_Gamma_Apply(Bmp,GR_FALSE) )
						return GR_FALSE;
				}
				else
				{
					// changed direction so must do an update

					if ( ! grBitmap_Update_DriverToSystem(Bmp) )
						return GR_FALSE;

					Bmp->DriverGammaLast = Bmp->DriverGamma = Gamma;
					
					if ( ! grBitmap_Update_SystemToDriver(Bmp) )
						return GR_FALSE;
				}
			}
			else
			{
				if ( ! grBitmap_Update_DriverToSystem(Bmp) )
					return GR_FALSE;

				Bmp->DriverGammaLast = Bmp->DriverGamma = Gamma;
				
				if ( ! grBitmap_Update_SystemToDriver(Bmp) )
					return GR_FALSE;
			}
		}
	}
	else
	{
		Bmp->DriverGamma = Gamma;
	}
	Bmp->DriverGammaSet = GR_TRUE;

return GR_TRUE;
}

grTexture * BITMAP_GR_INTERNAL grBitmap_GetTHandle(const grBitmap *Bmp)
{
//	assert( grBitmap_IsValid(Bmp) );

	// <> make this an assert?
	//if ( ! Bmp->DriverHandle )
	//	return NULL;

	if ( Bmp->StreamingTHandle )
		grBitmap_PeekReady(Bmp);

	return Bmp->DriverHandle;
}

grBoolean grBitmap_Update_SystemToDriver(grBitmap *Bmp)
{
grBitmap * SrcLocks[MAXMIPLEVELS];
grBoolean Ret,MipsChanged;
int32 mip,mipMin,mipMax;
grTexture * SaveDriverHandle;
grBitmap * SaveAlpha;
int32 SaveMaxMip;
	
	assert( grBitmap_IsValid(Bmp) );

	/**

		this function is totally hacked out, because what we really need to do
		is lock Bmp for Read (system) & Write (driver) , but that's illegal!

	**/

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;

	if ( Bmp->LockCount > 0 || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"Update_SystemToDriver : not an original bitmap", NULL);
		return GR_FALSE;
	}

	if ( ! Bmp->DriverHandle )
	{
		grErrorLog_AddString(-1,"Update_SystemToDriver : no driver data", NULL);
		return GR_FALSE;
	}

#if 0 // <> NO! YOU CANNOT CALL PEEKREADY!	PeekReady calls us !
	grBitmap_PeekReady(Bmp);
#endif

	//if Bmp->Format == Wavelet && Wavelet_CanDoMips , 
	//	then just do a LockForWrite & direct decompress!

	// <> thread the wavelet decompressor

	if ( Bmp->Info.Format == GR_PIXELFORMAT_WAVELET &&
		grWavelet_CanDecompressMips(Bmp->Wavelet,&(Bmp->DriverInfo)) )
	{
	grBitmap * DstLocks[MAXMIPLEVELS];
	grBitmap_Info Infos[MAXMIPLEVELS];
	grBitmap_Info * InfoPtrs[MAXMIPLEVELS];
	void * Bits[MAXMIPLEVELS];
	int32 i;

		mipMax = Bmp->DriverInfo.MaximumMip;
		if ( ! grBitmap_LockForWrite(Bmp,DstLocks,0,mipMax) )
			return GR_FALSE;

		for(i=0;i<=mipMax;i++)
		{
			InfoPtrs[i] = &Infos[i];
			grBitmap_GetInfo(DstLocks[i],InfoPtrs[i],NULL);
			Bits[i] = grBitmap_GetBits(DstLocks[i]);
			assert(Bits[i]);
		}

		if ( ! grWavelet_DecompressMips(Bmp->Wavelet,(const grBitmap_Info **)InfoPtrs,(const void **)Bits,0,mipMax) )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : Wavelet_DecompressMips failed!", NULL);
			grBitmap_UnLockArray_NoChange(DstLocks,mipMax+1);
			return GR_FALSE;
		}

		grBitmap_UnLockArray_NoChange(DstLocks,mipMax+1);
		
		if ( ! grBitmap_Gamma_Apply(Bmp,GR_FALSE) )
		{
			grErrorLog_AddString(-1,"AttachToDriver : Gamma_Apply failed!", NULL);
			return GR_FALSE;
		}

	return GR_TRUE;
	}

	MipsChanged = GR_FALSE;
	for(mip=Bmp->DriverInfo.MinimumMip;mip<=Bmp->DriverInfo.MaximumMip;mip++)
	{
		if ( Bmp->Modified[mip] && mip != Bmp->Info.MinimumMip )
		{
			assert(Bmp->Data[mip]);
			MipsChanged = GR_TRUE;
		}
	}

	//make mips after driver blit
	
	mipMin = Bmp->DriverInfo.MinimumMip;

	if ( MipsChanged )
		mipMax = Bmp->DriverInfo.MaximumMip;
	else
		mipMax = mipMin;


	SaveDriverHandle = Bmp->DriverHandle;
	Bmp->DriverHandle = NULL;	// so Lock() won't use the driver data

	SaveAlpha = Bmp->Alpha;

	if ( Bmp->Alpha && ! grPixelFormat_HasGoodAlpha(Bmp->DriverInfo.Format) && 
			(Bmp->DriverFlags & RDRIVER_PF_HAS_ALPHA_SURFACE) )
	{
		// hide the alpha so that it won't be used to make a colorkey in the target
		// we'll blit it independently later
		Bmp->Alpha = NULL;
	}

	// note : LockForReadNative calls PeekReady, but DriverHandle has been
	//	set to NULL so we don't get called again!

	if ( ! grBitmap_LockForReadNative(Bmp,SrcLocks,mipMin,mipMax) )
	{
		grErrorLog_AddString(-1,"Update_SystemToDriver : LockForReadNative", NULL);
		return GR_FALSE;
	}

	Ret = GR_TRUE;
	Bmp->DriverHandle = SaveDriverHandle;
	Bmp->Alpha = NULL;

	// we should have always updated the driver to system before fiddling the system
	assert( ! Bmp->DriverDataChanged );

	/**

		Bmp is a driver BMP

		the SrcLocks are locks of the system bits.

	**/

	for(mip=mipMin;mip <=mipMax;mip++)
	{
	grBitmap *SrcMip;
	void * SrcBits,*DstBits;
	grBitmap_Info DstInfo;

		SrcMip = SrcLocks[mip - mipMin];
		SrcBits = grBitmap_GetBits(SrcMip);

		DstInfo = Bmp->DriverInfo;

		if ( ! grBitmap_MakeDriverLockInfo(Bmp,mip,&DstInfo) )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : MakeInfo", NULL);
			Ret = GR_FALSE;
			continue;
		}

		// Zooma! THandle_Lock might lock the Win16 Lock !
		//	this is really bad when _BlitData is a wavelet decompress !
		// {} try this : decompress to a buffer in memory (on a thread)
		//	then THandle_Lock and just do a (prefetching) memcpy
//<>		#pragma message("Bitmap : minimize time spent in a THandle_Lock!")

		if ( ! Bmp->Driver->THandle_Lock(SaveDriverHandle,mip - Bmp->DriverMipBase,&DstBits) )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : THandle_Lock", NULL);
			Ret = GR_FALSE;
			continue;
		}

		if ( ! SrcBits || ! DstBits )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : No Bits", NULL);
			Ret = GR_FALSE;
			continue;
		}

		assert( DstInfo.Palette == Bmp->DriverInfo.Palette );

		if ( ! grBitmap_BlitData(	&(SrcMip->Info),SrcBits,SrcMip,
									&DstInfo,		DstBits,Bmp,
									SrcMip->Info.Width,SrcMip->Info.Height) )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : BlitData", NULL);
			assert(0);
			Ret = GR_FALSE;
			continue;
		}

		if ( ! Bmp->Driver->THandle_UnLock(SaveDriverHandle,mip - Bmp->DriverMipBase) )
		{
			grErrorLog_AddString(-1,"Update_SystemToDriver : THandle_UnLock", NULL);
			Ret = GR_FALSE;
			continue;
		}

		// normally this would be done by the Bitmap_UnLock ,
		//  but since we don't lock ..
		if ( DstInfo.Palette != Bmp->DriverInfo.Palette )
		{
			//assert( OldDstPal == NULL );
			grBitmap_SetPalette(Bmp,DstInfo.Palette);
			grBitmap_Palette_Destroy(&(DstInfo.Palette));
			// must destroy here, since DstInfo is on the stack!
		}
	}

	Bmp->Alpha = SaveAlpha;
	Bmp->DriverBitsLocked = GR_FALSE;
	Bmp->DriverMipLock = 0;
	Bmp->DriverDataChanged = GR_FALSE;

	grBitmap_UnLockArray(SrcLocks, mipMax - mipMin + 1 );

	if ( ! Ret )
	{
		grErrorLog_AddString(-1,"Update_SystemToDriver : Locking and Blitting error", NULL);
	}

	if ( Bmp->Alpha && ! grPixelFormat_HasGoodAlpha(Bmp->DriverInfo.Format) && 
			(Bmp->DriverFlags & RDRIVER_PF_HAS_ALPHA_SURFACE) )
	{
	grTexture * AlphaTH;

		// blit the alpha surface to the separate alpha

		AlphaTH = Bmp->Driver->THandle_GetAlpha(Bmp->DriverHandle);
		if ( !AlphaTH || AlphaTH != Bmp->Alpha->DriverHandle)
		{
			if ( ! grBitmap_AttachToDriver(Bmp->Alpha,Bmp->Driver,Bmp->Alpha->DriverFlags | RDRIVER_PF_ALPHA_SURFACE) )
			{
				grErrorLog_AddString(-1,"AttachToDriver : attach Alpha", NULL);
				return GR_FALSE;
			}

			assert(Bmp->Alpha->DriverHandle);
			if ( ! Bmp->Driver->THandle_SetAlpha(Bmp->DriverHandle,Bmp->Alpha->DriverHandle) )
			{
				grErrorLog_AddString(-1,"AttachToDriver : THandle_SetAlpha", NULL);
				grBitmap_DetachDriver(Bmp->Alpha,GR_FALSE);
				grBitmap_DetachDriver(Bmp,GR_FALSE);
				return GR_FALSE;
			}
			
			AlphaTH = Bmp->Driver->THandle_GetAlpha(Bmp->DriverHandle);
			assert(AlphaTH == Bmp->Alpha->DriverHandle);
		}
	}

	// now bits are on driver , gamma correct them

	//for gamma : just Gamma up to mipMax then make mips from it
	//	seems to work

	SaveMaxMip = Bmp->DriverInfo.MaximumMip;
	Bmp->DriverInfo.MaximumMip = mipMax;
	if ( ! grBitmap_Gamma_Apply(Bmp,GR_FALSE) )
	{
		grErrorLog_AddString(-1,"AttachToDriver : Gamma_Apply failed!", NULL);
		Ret = GR_FALSE;
	}
	Bmp->DriverInfo.MaximumMip = SaveMaxMip;

	if ( ! MipsChanged && mipMax < Bmp->DriverInfo.MaximumMip )
	{
		for(mip=mipMax+1;mip<= Bmp->DriverInfo.MaximumMip; mip++)
		{
			if ( ! grBitmap_UpdateMips(Bmp,mip-1,mip) )
			{
				grErrorLog_AddString(-1,"AttachToDriver : UpdateMips on driver failed!", NULL);
				return GR_FALSE;
			}
		}
	}

	Bmp->DriverDataChanged = GR_FALSE; // in case _SetPal freaks us out

return Ret;
}

grBoolean grBitmap_Update_DriverToSystem(grBitmap *Bmp)
{
grBitmap *DriverLocks[MAXMIPLEVELS];
grBoolean Ret;
int32 mip;
	
	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;

	if ( Bmp->LockCount > 0 || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"Update_DriverToSystem : not an original bitmap", NULL);
		return GR_FALSE;
	}

	if ( ! Bmp->DriverHandle )
	{
		grErrorLog_AddString(-1,"Update_DriverToSystem : no driver data", NULL);
		return GR_FALSE;
	}

	if ( ! Bmp->DriverDataChanged )
		return GR_TRUE;

	// bits are on driver; undo the gamma to copy them home

	Log_Puts("Bitmap : Doing Update_DriverToSystem");

	if ( ! grBitmap_Gamma_Apply(Bmp,GR_TRUE) ) // undo the gamma!
		return GR_FALSE;

	if ( Bmp->Info.Palette && Bmp->DriverInfo.Palette )
	{
		if ( ! grBitmap_Palette_Copy(Bmp->DriverInfo.Palette,Bmp->Info.Palette) )
		{
			grErrorLog_AddString(-1,"Update_DriverToSystem : Palette_Copy", NULL);
		}
	}

	if ( grBitmap_LockForReadNative(Bmp,DriverLocks,
			Bmp->DriverInfo.MinimumMip,Bmp->DriverInfo.MaximumMip) )
	{
		Ret = GR_TRUE;

		for(mip=Bmp->DriverInfo.MinimumMip;mip <=Bmp->DriverInfo.MaximumMip;mip++)
		{	
		grBitmap *MipBmp;
		grBitmap_Info SystemInfo;

			MipBmp = DriverLocks[mip];

			if ( Bmp->Modified[mip] )
			{
			void *DriverBits,*SystemBits;
				DriverBits = grBitmap_GetBits(MipBmp);
				assert( MipBmp->DriverBitsLocked );

				if ( ! grBitmap_AllocSystemMip(Bmp,mip) )
					Ret = GR_FALSE;

				SystemBits = Bmp->Data[mip];

				grBitmap_MakeMipInfo(&(Bmp->Info),mip,&SystemInfo);

				if ( DriverBits && SystemBits )
				{
					// _Update_DriverToSystem
					// {} palette (not) made in AttachToDriver; must be made in here->
					if ( ! grBitmap_BlitData(	&(MipBmp->Info), DriverBits, MipBmp,
												&SystemInfo,	SystemBits, Bmp,
												SystemInfo.Width,SystemInfo.Height) )
						Ret = GR_FALSE;
				}
				else
				{
					Ret = GR_FALSE;
				}
			}
			
			grBitmap_UnLock(DriverLocks[mip]);
		}

		Bmp->DriverDataChanged = GR_FALSE;
	}
	else
	{
		Ret = GR_FALSE;
	}

	if ( ! Ret )
	{
		grErrorLog_AddString(-1,"Update_DriverToSystem : Locking and Blitting error", NULL);
	}

	if ( ! grBitmap_Gamma_Apply(Bmp,GR_FALSE) ) // redo the gamma!
		return GR_FALSE;

return Ret;
}

/*}{ ************* Mip Control *****************/

// Note : all the Mip control 

GRAPI grBoolean GRCC grBitmap_RefreshMips(grBitmap *Bmp)
{
int32 mip;

	assert( grBitmap_IsValid(Bmp) );

	if ( Bmp->LockOwner || Bmp->LockCount || Bmp->DataOwner )
		return GR_FALSE;

	for(mip = (Bmp->Info.MinimumMip + 1);mip <= Bmp->Info.MaximumMip;mip++)
	{
		if ( Bmp->Data[mip] && !(Bmp->Modified[mip]) )
		{
		int32 src;
			src = mip-1;
			while( ! Bmp->Data[src] )
			{
				src--;
				if ( src < Bmp->Info.MinimumMip )
					return GR_FALSE;
			}
			if ( ! grBitmap_UpdateMips(Bmp,src,mip) )
				return GR_FALSE;
		}
	}

#if 0	// never turn off a modified flag
	for(mip=0;mip<MAXMIPLEVELS;mip++)
		Bmp->Modified[mip] = GR_FALSE;
#endif

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_UpdateMips(grBitmap *Bmp,int32 fm,int32 to)
{
grBitmap * Locks[MAXMIPLEVELS];
void *FmBits,*ToBits;
grBitmap_Info FmInfo,ToInfo;
grBoolean Ret = GR_FALSE;

	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner || Bmp->LockCount > 0 || Bmp->DataOwner )
		return GR_FALSE;

	if ( fm >= to )
		return GR_FALSE;

	if ( Bmp->DriverHandle ) 
	{
		//{} this version does *NOT* make new mips if to > Bmp->DriverInfo.MaximumMip

		if ( ! grBitmap_LockForWrite(Bmp,Locks,fm,to) )
			return GR_FALSE;

		if ( grBitmap_GetInfo(Locks[0],&FmInfo,NULL) && grBitmap_GetInfo(Locks[to - fm],&ToInfo,NULL) )
		{
			FmBits = grBitmap_GetBits(Locks[0]);
			ToBits = grBitmap_GetBits(Locks[to - fm]);
		
			if ( FmBits && ToBits )
			{
				Ret = grBitmap_UpdateMips_Data(	&FmInfo, FmBits, 
												&ToInfo, ToBits );
			}
		}

		grBitmap_UnLockArray_NoChange(Locks,to - fm + 1);
	}
	else
	{
		Ret = grBitmap_UpdateMips_System(Bmp,fm,to);
	}

return Ret;
}

grBoolean grBitmap_UpdateMips_System(grBitmap *Bmp,int32 fm,int32 to)
{
grBitmap_Info FmInfo,ToInfo;
grBoolean Ret;

	assert( grBitmap_IsValid(Bmp) );

	// this is called to create new mips in LockFor* -> CreateLockFrom* (through MakeSystemMips)

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;
//	if ( Bmp->LockCount > 0 )
//		return GR_FALSE;
	if ( Bmp->DataOwner )
		return GR_FALSE;

	// {} for compressed data, just don't make mips and say we did!
	if ( grPixelFormat_BytesPerPel(Bmp->Info.Format) < 1 )
		return GR_TRUE;

	while(Bmp->Data[fm] == NULL || fm == to )
	{
		fm--;
		if ( fm < 0 )
			return GR_FALSE;
	}

	if ( fm < Bmp->Info.MinimumMip || fm > Bmp->Info.MaximumMip ||
	     to < fm || to >= MAXMIPLEVELS )
		return GR_FALSE;

	if ( ! Bmp->Data[to] )
	{
		if ( ! grBitmap_AllocSystemMip(Bmp,to) )
			return GR_FALSE;
	}

	assert( to > fm && fm >= 0 );

	FmInfo = ToInfo = Bmp->Info;

	FmInfo.Width = SHIFT_R_ROUNDUP(Bmp->Info.Width ,fm);
	FmInfo.Height= SHIFT_R_ROUNDUP(Bmp->Info.Height,fm);
	FmInfo.Stride= SHIFT_R_ROUNDUP(Bmp->Info.Stride,fm);
	ToInfo.Width = SHIFT_R_ROUNDUP(Bmp->Info.Width ,to);
	ToInfo.Height= SHIFT_R_ROUNDUP(Bmp->Info.Height,to);
	ToInfo.Stride= SHIFT_R_ROUNDUP(Bmp->Info.Stride,to);

	Ret = grBitmap_UpdateMips_Data(	&FmInfo, Bmp->Data[fm],
									&ToInfo, Bmp->Data[to]);

	Bmp->Info.MaximumMip = max(Bmp->Info.MaximumMip,to);

return Ret;
}

grBoolean grBitmap_UpdateMips_Data(	grBitmap_Info * FmInfo,void * FmBits,
									grBitmap_Info * ToInfo,void * ToBits)
{
int32 fmxtra,tow,toh,toxtra,fmw,fmh,fmstep,x,y,bpp;

	assert( FmInfo && ToInfo && FmBits && ToBits );
	assert( FmInfo->Format == ToInfo->Format && FmInfo->HasColorKey == ToInfo->HasColorKey );

	tow = ToInfo->Width;
	toh = ToInfo->Height;
	toxtra = ToInfo->Stride - ToInfo->Width;
	
	x = ToInfo->Width;
	fmstep = 1;
	while( x < FmInfo->Width )
	{
		fmstep += fmstep;
		x += x;
	}

	fmw = FmInfo->Width;
	fmh = FmInfo->Height;
	fmxtra = (FmInfo->Stride - tow) * fmstep; // amazingly simple and correct! think about it!

	// fmh == 15
	// toh == 8
	// fmstep == 2
	// 7*2 <= 14 -> Ok
	if ( (toh-1)*fmstep > (fmh - 1) )
	{
		grErrorLog_AddString(-1,"UpdateMips_Data : Vertical mip scaling doesn't match horizontal!", NULL);
		return GR_FALSE;
	}

	// {} todo : average for some special cases (16rgb,24rgb,32rgb)

	bpp = grPixelFormat_BytesPerPel(FmInfo->Format);

	if ( fmstep == 2 && bpp > 1 )
	{
	int32 R1,G1,B1,A1,R2,G2,B2,A2,R3,G3,B3,A3,R4,G4,B4,A4;
	grPixelFormat_ColorGetter GetColor;
	grPixelFormat_ColorPutter PutColor;
	const grPixelFormat_Operations *ops;
	uint8 *fmp,*fmp2,*top;

		fmp = (uint8*)FmBits;
		top = (uint8*)ToBits;

		ops = grPixelFormat_GetOperations(FmInfo->Format);
		GetColor = ops->GetColor;
		PutColor = ops->PutColor;

		fmxtra *= bpp;
		toxtra *= bpp;

		if ( FmInfo->HasColorKey )
		{
		uint32 ck,p1,p2,p3,p4;
		grPixelFormat_PixelGetter GetPixel;
		grPixelFormat_PixelPutter PutPixel;
		grPixelFormat_Decomposer DecomposePixel;

			assert( FmInfo->ColorKey == ToInfo->ColorKey );
			ck = FmInfo->ColorKey;
			GetPixel = ops->GetPixel;
			PutPixel = ops->PutPixel;
			DecomposePixel = ops->DecomposePixel;
		
			// {} the colorkey mip-subsampler
			// slow as hell; yet another reason to not use CK !
			
			for(y=toh;y--;)
			{
				//y = 7, fmh = 15; y*2+1 == fmh : last line is not a double line
				if ( (y+y + 1) == fmh )	fmp2 = fmp;
				else					fmp2 = fmp + (FmInfo->Stride*bpp);
				for(x=tow;x--;)
				{
					p1 = GetPixel(&fmp);
					p2 = GetPixel(&fmp);
					p3 = GetPixel(&fmp2);
					p4 = GetPixel(&fmp2);
					if ( p1 == ck || p4 == ck )
					{
						PutPixel(&top,ck);
					}
					else
					{
						// p1 and p4 are not ck;
						if ( p2 == ck ) p2 = p1;
						if ( p3 == ck ) p3 = p4;
						DecomposePixel(p1,&R1,&G1,&B1,&A1);
						DecomposePixel(p2,&R2,&G2,&B2,&A2);
						DecomposePixel(p3,&R3,&G3,&B3,&A3);
						DecomposePixel(p4,&R4,&G4,&B4,&A4);
						PutColor(&top,(R1+R2+R3+R4+2)>>2,(G1+G2+G3+G4+2)>>2,(B1+B2+B3+B4+2)>>2,(A1+A2+A3+A4+2)>>2);
					}
				}
				fmp += fmxtra;
				top += toxtra;
			}
		}
		else
		{
			for(y=toh;y--;)
			{
				//y = 7, fmh = 15; y*2+1 == fmh : last line is not a double line
				if ( (y+y + 1) == fmh )	fmp2 = fmp;
				else					fmp2 = fmp + (FmInfo->Stride*bpp);
				for(x=tow;x--;)
				{
					GetColor(&fmp ,&R1,&G1,&B1,&A1);
					GetColor(&fmp ,&R2,&G2,&B2,&A2);
					GetColor(&fmp2,&R3,&G3,&B3,&A3);
					GetColor(&fmp2,&R4,&G4,&B4,&A4);
					PutColor(&top,(R1+R2+R3+R4+2)>>2,(G1+G2+G3+G4+2)>>2,(B1+B2+B3+B4+2)>>2,(A1+A2+A3+A4+2)>>2);
				}
				fmp += fmxtra;
				top += toxtra;
			}
		}

		assert( top == (((uint8 *)ToBits) + ToInfo->Stride * ToInfo->Height * bpp ) );
		assert( fmp == (((uint8 *)FmBits) + FmInfo->Stride * ToInfo->Height * 2 * bpp ) );
	}
	else if ( fmstep == 2 && grPixelFormat_HasPalette(FmInfo->Format) )
	{
	int32 R,G,B;
	uint8 *fmp,*fmp2,*top;
	uint8 paldata[768],*palptr;
	int32 p;
	palInfo * PalInfo;

		assert(bpp == 1);
		assert(FmInfo->Palette);

		if ( ! grBitmap_Palette_GetData(FmInfo->Palette,paldata,GR_PIXELFORMAT_24BIT_RGB,256) )
			return GR_FALSE;

		if ( ! (PalInfo = closestPalInit(paldata)) )
			return GR_FALSE;

		fmp = (uint8*)FmBits;
		top = (uint8*)ToBits;

		// @@ colorkey?

		for(y=toh;y--;)
		{
			//y = 7, fmh = 15; y*2+1 == fmh : last line is not a double line
			if ( (y*2 + 1) == fmh )	fmp2 = fmp;
			else					fmp2 = fmp + (FmInfo->Stride*bpp);

			for(x=tow;x--;)
			{
				p = *fmp++;
				palptr = paldata + p*3;
				R  = palptr[0]; G  = palptr[1]; B  = palptr[2]; 
				p = *fmp++;
				palptr = paldata + p*3;
				R += palptr[0]; G += palptr[1]; B += palptr[2]; 
				p = *fmp2++;
				palptr = paldata + p*3;
				R += palptr[0]; G += palptr[1]; B += palptr[2]; 
				p = *fmp2++;
				palptr = paldata + p*3;
				R += palptr[0]; G += palptr[1]; B += palptr[2]; 

				R = (R+2)>>2;
				G = (G+2)>>2;
				B = (B+2)>>2;

				p = closestPal(R,G,B,PalInfo);
				*top++ = (uint8)p;
			}
			fmp += fmxtra;
			top += toxtra;
		}

		closestPalFree(PalInfo);

		assert( top == (((uint8 *)ToBits) + ToInfo->Stride * ToInfo->Height * bpp ) );
		assert( fmp == (((uint8 *)FmBits) + FmInfo->Stride * ToInfo->Height * 2 * bpp ) );
	}
	else
	{
		// we just sub-sample to make mips, so we don't have to
		//	know anything about pixelformat.
		// (btw this spoils the whole point of mips, so we might as well kill the mip!)

		//{} Blend correctly !?

		switch( bpp )
		{
			default:
			{
				return GR_FALSE;
			}
			case 1:
			{
				uint8 *fmp,*top;
				fmp = (uint8*)FmBits;
				top = (uint8*)ToBits;
				for(y=toh;y--;)
				{
					for(x=tow;x--;)
					{
						*top++ = *fmp;
						fmp += fmstep;
					}
					fmp += fmxtra;
					top += toxtra;
				}
				break;
			}
			case 2:
			{
				uint16 *fmp,*top;
				fmp = (uint16*)FmBits;
				top = (uint16*)ToBits;
				for(y=toh;y--;)
				{
					for(x=tow;x--;)
					{
						*top++ = *fmp;
						fmp += fmstep;
					}
					fmp += fmxtra;
					top += toxtra;
				}
				break;
			}
			case 4:
			{
				uint32 *fmp,*top;
				fmp = (uint32*)FmBits;
				top = (uint32*)ToBits;
				for(y=toh;y--;)
				{
					for(x=tow;x--;)
					{
						*top++ = *fmp;
						fmp += fmstep;
					}
					fmp += fmxtra;
					top += toxtra;
				}
				break;
			}
			case 3:
			{
				uint8 *fmp,*top;
				fmp = (uint8*)FmBits;
				top = (uint8*)ToBits;
				fmstep = (fmstep - 1) * 3;
				fmxtra *= 3;
				toxtra *= 3;
				for(y=toh;y--;)
				{
					for(x=tow;x--;)
					{
						*top++ = *fmp++;
						*top++ = *fmp++;
						*top++ = *fmp++;
						fmp += fmstep;
					}
					fmp += fmxtra;
					top += toxtra;
				}
				break;
			}
		}
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_ClearMips(grBitmap *Bmp)
{
int32 mip;
DRV_Driver * Driver;

	// WARNING ! This destroys any mips!

	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner || Bmp->LockCount || Bmp->DataOwner )
		return GR_FALSE;

	if ( Bmp->SeekMipCount == 0 && Bmp->Info.MaximumMip == 0 )
		return GR_TRUE;

	Driver = Bmp->Driver;
	if ( Driver )
	{
		if ( ! grBitmap_DetachDriver(Bmp,GR_TRUE) )
			return GR_FALSE;
	}
	assert(Bmp->Driver == NULL);

	mip = Bmp->Info.MinimumMip;
	if ( mip == 0 ) 
		mip++;

	Bmp->SeekMipCount = mip;

	for( ; mip <= Bmp->Info.MaximumMip ; mip++)
	{
		if ( Bmp->Data[mip] )
		{
			grRam_Free( Bmp->Data[mip] );
			Bmp->Data[mip] = NULL;
		}
	}

	Bmp->Info.MaximumMip = Bmp->Info.MinimumMip;

	if ( Driver )
	{
		if ( ! grBitmap_AttachToDriver(Bmp,Driver,0) )
			return GR_FALSE;
	}

return GR_TRUE;
}

GRAPI grBoolean 	GRCC	grBitmap_SetMipCount(grBitmap *Bmp,int32 Count)
{
DRV_Driver * Driver;

	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner || Bmp->LockCount || Bmp->DataOwner )
		return GR_FALSE;

// @@ don't do this ?
//	if ( Bmp->Info.MaximumMip < (Count-1) )
//		grBitmap_MakeSystemMips(Bmp,0,Count-1);

	if ( Bmp->SeekMipCount == Count )
		return GR_TRUE;

	Driver = Bmp->Driver;
	if ( Driver )
	{
		if ( ! grBitmap_DetachDriver(Bmp,GR_TRUE) )
			return GR_FALSE;
	}
	assert(Bmp->Driver == NULL);

	Bmp->SeekMipCount = Count;

	if ( Driver )
	{
		if ( ! grBitmap_AttachToDriver(Bmp,Driver,0) )
			return GR_FALSE;
	}

return GR_TRUE;
}

grBoolean grBitmap_MakeSystemMips(grBitmap *Bmp,int32 low,int32 high)
{
int32 mip;

	assert( grBitmap_IsValid(Bmp) );

	// this is that CreateLockFromMip uses to make its new data

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;
//	if ( Bmp->LockCount > 0 )
//		return GR_FALSE;
	if ( Bmp->DataOwner )
		return GR_FALSE;

	// {} for compressed data, just don't make mips and say we did!
	if ( grPixelFormat_BytesPerPel(Bmp->Info.Format) < 1 )
		return GR_TRUE;

	if ( low < 0 || high >= MAXMIPLEVELS || low > high )
		return GR_FALSE;

	for( mip = low; mip <= high; mip++)
	{
		if ( ! Bmp->Data[mip] )
		{
			if ( ! grBitmap_AllocSystemMip(Bmp,mip) )
				return GR_FALSE;
	
			if ( mip != 0 )
			{
				if ( ! grBitmap_UpdateMips_System(Bmp,mip-1,mip) )
					return GR_FALSE;
			}
		}
	}

	Bmp->Info.MinimumMip = min(Bmp->Info.MinimumMip,low);
	Bmp->Info.MaximumMip = max(Bmp->Info.MaximumMip,high);

return GR_TRUE;
}

/*}{ ******* Miscellany ***********/

GRAPI uint32 GRCC grBitmap_MipBytes(const grBitmap *Bmp,int32 mip)
{
uint32 bytes;
	if ( ! Bmp )
		return 0;
	bytes = grPixelFormat_BytesPerPel(Bmp->Info.Format) * 
						SHIFT_R_ROUNDUP(Bmp->Info.Stride,mip) *
						SHIFT_R_ROUNDUP(Bmp->Info.Height,mip);
return bytes;
}

GRAPI grBoolean GRCC grBitmap_GetInfo(const grBitmap *Bmp, grBitmap_Info *Info, grBitmap_Info *SecondaryInfo)
{
	assert( grBitmap_IsValid(Bmp) );

	assert(Info);

	if ( Bmp->DriverHandle )
	{
		*Info = Bmp->DriverInfo;
	}
	else
	{
		*Info = Bmp->Info;
	}

	if ( SecondaryInfo )
		*SecondaryInfo = Bmp->Info;

	return GR_TRUE;
}

grBoolean grBitmap_MakeDriverLockInfo(grBitmap *Bmp,int32 mip,grBitmap_Info *Into)
{
grTexture_Info TInfo;

	// MakeDriverLockInfo also doesn't full out the full info, so it must be a valid info first!
	// Bmp also gets some crap written into him.

	assert(Bmp && Into); // not necessarily valid

	if ( ! Bmp->DriverHandle || ! Bmp->Driver || mip < Bmp->DriverInfo.MinimumMip || mip > Bmp->DriverInfo.MaximumMip )
		return GR_FALSE;

	if ( ! Bmp->Driver->THandle_GetInfo(Bmp->DriverHandle,mip - Bmp->DriverMipBase,&TInfo) )
	{
		grErrorLog_AddString(-1,"MakeDriverLockInfo : THandle_GetInfo", NULL);
		return GR_FALSE;
	}

	Bmp->DriverMipLock	= mip;
	Bmp->DriverFlags	= TInfo.PixelFormat.Flags;

	Into->Width			= TInfo.Width;
	Into->Height		= TInfo.Height;
	Into->Stride		= TInfo.Stride;
	Into->Format		= TInfo.PixelFormat.PixelFormat;
	Into->ColorKey		= TInfo.ColorKey;

	if ( TInfo.Flags & RDRIVER_THANDLE_HAS_COLORKEY )
		Into->HasColorKey = GR_TRUE;
	else
		Into->HasColorKey = GR_FALSE;

	Into->MinimumMip = Into->MaximumMip = mip;

	if ( grPixelFormat_HasPalette(Into->Format) && Into->Palette && Into->Palette->HasColorKey )
	{
		Into->HasColorKey = GR_TRUE;
		Into->ColorKey = Into->Palette->ColorKeyIndex;
	}

return GR_TRUE;
}

GRAPI int32 GRCC	grBitmap_Width(const grBitmap *Bmp)
{
	assert(Bmp);
return(Bmp->Info.Width);
}

GRAPI int32 GRCC	grBitmap_Height(const grBitmap *Bmp)
{
	assert(Bmp);
return(Bmp->Info.Height);
}

GRAPI grBoolean GRCC grBitmap_Blit(const grBitmap *Src, int32 SrcPositionX, int32 SrcPositionY,
						grBitmap *Dst, int32 DstPositionX, int32 DstPositionY,
						int32 SizeX, int32 SizeY )
{
	assert( grBitmap_IsValid(Src) );
	assert( grBitmap_IsValid(Dst) );
	return grBitmap_BlitMipRect(Src,0,SrcPositionX,SrcPositionY,
								Dst,0,DstPositionX,DstPositionY,
								SizeX,SizeY);
}

GRAPI grBoolean GRCC grBitmap_BlitBitmap(const grBitmap * Src, grBitmap * Dst )
{
	assert( grBitmap_IsValid(Src) );
	assert( grBitmap_IsValid(Dst) );
	assert( Src != Dst );
	return grBitmap_BlitMipRect(Src,0,0,0,Dst,0,0,0,-1,-1);
}

GRAPI grBoolean GRCC grBitmap_BlitBestMip(const grBitmap * Src, grBitmap * Dst )
{
int32 Width,Mip;
	assert( grBitmap_IsValid(Src) );
	assert( grBitmap_IsValid(Dst) );
	assert( Src != Dst );
	for(Mip=0;	(Width = SHIFT_R_ROUNDUP(Src->Info.Width,Mip)) > Dst->Info.Width ; Mip++) ;
	return grBitmap_BlitMipRect(Src,Mip,0,0,Dst,0,0,0,-1,-1);
}

GRAPI grBoolean GRCC grBitmap_BlitMip(const grBitmap * Src, int32 SrcMip, grBitmap * Dst, int32 DstMip )
{
	assert( grBitmap_IsValid(Src) );
	assert( grBitmap_IsValid(Dst) );
	return grBitmap_BlitMipRect(Src,SrcMip,0,0,Dst,DstMip,0,0,-1,-1);
}

grBoolean grBitmap_BlitMipRect(const grBitmap * Src, int32 SrcMip, int32 SrcX,int32 SrcY,
									 grBitmap * Dst, int32 DstMip, int32 DstX,int32 DstY,
							int32 SizeX,int32 SizeY)
{
grBitmap * SrcLock,* DstLock;
grBoolean SrcUnLock,DstUnLock;
grBitmap_Info *SrcLockInfo,*DstLockInfo;
uint8 *SrcBits,*DstBits;
	
	assert(Src && Dst);
	grBitmap_PeekReady(Src);
	grBitmap_WaitReady(Dst);

	assert( Src != Dst );
	// <> if Src == Dst we could still do this, but we assert SrcMip != DstMip & be smart

	SrcUnLock = DstUnLock = GR_FALSE;

	if ( Src->LockOwner )
	{
		assert( Src->LockOwner->LockCount );
		if ( SrcMip != 0 )
		{
			grErrorLog_AddString(-1,"BlitMipRect : Src is a lock and mip != 0", NULL);
			goto fail;
		}

		SrcLock = (grBitmap *)Src;
	}
	else
	{
		if ( ! grBitmap_LockForReadNative((grBitmap *)Src,&SrcLock,SrcMip,SrcMip) )
		{
			grErrorLog_AddString(-1,"BlitMipRect : LockForReadNative", NULL);
			goto fail;
		}
		SrcUnLock = GR_TRUE;
	}

	if ( Dst->LockOwner )
	{
		if ( DstMip != 0 )
			goto fail;
//		if ( Dst->LockOwner->LockCount >= 0 )
//			goto fail;
//		{} can't check this, cuz we use _BlitMip to create locks for read
		DstLock = Dst;
	}
	else
	{
		if ( ! grBitmap_LockForWrite(Dst,&DstLock,DstMip,DstMip) )
		{
			grErrorLog_AddString(-1,"BlitMipRect : LockForWrite", NULL);
			goto fail;
		}
		DstUnLock = GR_TRUE;
	}

	Src = Dst = NULL;

	if ( SrcLock->DriverHandle ) 
		SrcLockInfo = &(SrcLock->DriverInfo);
	else
		SrcLockInfo = &(SrcLock->Info);

	if ( DstLock->DriverHandle ) 
		DstLockInfo = &(DstLock->DriverInfo);
	else
		DstLockInfo = &(DstLock->Info);

	if ( ! (SrcBits = (uint8*)grBitmap_GetBits(SrcLock)) || 
		 ! (DstBits = (uint8*)grBitmap_GetBits(DstLock)) )
	{
		grErrorLog_AddString(-1,"BlitMipRect : GetBits", NULL);
		goto fail;
	}

	if ( SizeX < 0 )
		SizeX = min(SrcLockInfo->Width,DstLockInfo->Width);
	if ( SizeY < 0 )
		SizeY = min(SrcLockInfo->Height,DstLockInfo->Height);

	if (( (SrcX + SizeX) > SrcLockInfo->Width ) ||
		( (SrcY + SizeY) > SrcLockInfo->Height) ||
		( (DstX + SizeX) > DstLockInfo->Width ) ||
		( (DstY + SizeY) > DstLockInfo->Height))
	{
		grErrorLog_AddString(-1,"BlitMipRect : dimensions bad", NULL);
		goto fail;
	}

	SrcBits += grPixelFormat_BytesPerPel(SrcLockInfo->Format) * ( SrcY * SrcLockInfo->Stride + SrcX );
	DstBits += grPixelFormat_BytesPerPel(DstLockInfo->Format) * ( DstY * DstLockInfo->Stride + DstX );

	// _BlitMipRect : made palette
	if ( ! grBitmap_BlitData(	SrcLockInfo,SrcBits,SrcLock,
								DstLockInfo,DstBits,DstLock,
								SizeX,SizeY) )
	{
		goto fail;
	}

	if ( SrcUnLock ) grBitmap_UnLock(SrcLock);
	if ( DstUnLock ) grBitmap_UnLock(DstLock);

	return GR_TRUE;

	fail:

	if ( SrcUnLock ) grBitmap_UnLock(SrcLock);
	if ( DstUnLock ) grBitmap_UnLock(DstLock);

	return GR_FALSE;
}

GRAPI grBoolean 	GRCC	grBitmap_SetFormatMin(grBitmap *Bmp,grPixelFormat NewFormat)
{
grBitmap_Palette * Pal;

	assert(grBitmap_IsValid(Bmp));

	Pal = grBitmap_GetPalette(Bmp);
	if ( Bmp->Info.HasColorKey )
	{
	uint32 CK;
		if ( grPixelFormat_IsRaw(NewFormat) )
		{
			if ( grPixelFormat_IsRaw(Bmp->Info.Format) )
			{
				CK = grPixelFormat_ConvertPixel(Bmp->Info.Format,Bmp->Info.ColorKey,NewFormat);
			}
			else if ( grPixelFormat_HasPalette(Bmp->Info.Format) )
			{
				assert(Pal);
				grBitmap_Palette_GetEntry(Pal,Bmp->Info.ColorKey,&CK);
				CK = grPixelFormat_ConvertPixel(Pal->Format,CK,NewFormat);
				if ( ! CK ) CK = 1;
			}
		}
		else
		{
			if ( grPixelFormat_HasPalette(NewFormat) )
			{
				CK = 255;
			}
			else
			{
				CK = 1;
			}
		}
		
		return grBitmap_SetFormat(Bmp,NewFormat,GR_TRUE,CK,Pal);
	}
	else
	{
		return grBitmap_SetFormat(Bmp,NewFormat,GR_FALSE,0,Pal);
	}
}

GRAPI grBoolean GRCC grBitmap_SetCompressionOptions(grBitmap * Bmp,int32 clevel,grBoolean NeedMips,grFloat ratio)
{
	assert(grBitmap_IsValid(Bmp));

	Bmp->HasWaveletOptions = grWavelet_SetOptions(&(Bmp->WaveletOptions),clevel,NeedMips,ratio);

return Bmp->HasWaveletOptions;
}

GRAPI grBoolean GRCC grBitmap_SetCompressionOptionsExpert(grBitmap * Bmp,grFloat Ratio,int32 TransformN,int32 CoderN,grBoolean TransposeLHs,grBoolean Block)
{
	assert(grBitmap_IsValid(Bmp));

	Bmp->HasWaveletOptions = grWavelet_SetExpertOptions(&(Bmp->WaveletOptions),Ratio,TransformN,CoderN,TransposeLHs,Block);

return Bmp->HasWaveletOptions;
}

GRAPI grBoolean GRCC grBitmap_SetFormat(grBitmap *Bmp, 
							grPixelFormat NewFormat, 
							grBoolean HasColorKey, uint32 ColorKey,
							const grBitmap_Palette *Palette )
{
	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner || Bmp->LockCount || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"SetFormat : not an original bitmap", NULL);
		return GR_FALSE;	
	}
	// can't do _SetFormat on a locked mip, cuz it would change the size of all the locked mips = no good

	// always affects the non-Driver copy

	if ( NewFormat == GR_PIXELFORMAT_WAVELET )
	{
		grBitmap_ClearMips(Bmp);

		if ( Bmp->Wavelet )
		{
			assert(Bmp->Info.Format == GR_PIXELFORMAT_WAVELET);
			return GR_TRUE;
		}
			
		if ( Bmp->Info.HasColorKey )
			Bmp->Info.HasColorKey = grBitmap_UsesColorKey(Bmp);

		if ( Bmp->HasWaveletOptions )
			Bmp->Wavelet = grWavelet_CreateFromBitmap(Bmp,&(Bmp->WaveletOptions));
		else
			Bmp->Wavelet = grWavelet_CreateFromBitmap(Bmp,NULL);
			
		if ( ! Bmp->Wavelet )
			return GR_FALSE;

		Bmp->Info.Format = GR_PIXELFORMAT_WAVELET;

		if ( Bmp->Data[0] )
		{
			grRam_Free(Bmp->Data[0]);
			Bmp->Data[0] = NULL;
		}
		
		if ( Bmp->Alpha )
		{
			grBitmap_Destroy(&(Bmp->Alpha));
			Bmp->Alpha = NULL;
		}

		return GR_TRUE;
	}

	if ( NewFormat == Bmp->Info.Format )
	{
		// but not wavelet

		if ( grPixelFormat_HasPalette(NewFormat) && Palette )
		{
			if ( ! grBitmap_SetPalette(Bmp,(grBitmap_Palette *)Palette) )
				return GR_FALSE;
		}

		if ( (! HasColorKey )
			|| ( HasColorKey && Bmp->Info.HasColorKey && ColorKey == Bmp->Info.ColorKey ) )
		{
			Bmp->Info.HasColorKey = HasColorKey;
			Bmp->Info.ColorKey = ColorKey;
			return GR_TRUE;
		}
		else
		{
		grBitmap_Info OldInfo;

			OldInfo = Bmp->Info;

			assert(HasColorKey);

			// just change the colorkey

			Bmp->Info.HasColorKey = HasColorKey;
			Bmp->Info.ColorKey = ColorKey;

			if ( Bmp->Data[Bmp->Info.MinimumMip] == NULL )
				return GR_TRUE;
		
			assert(Bmp->Info.MinimumMip == 0); //{} this is just out of laziness

			// _SetFormat : same format
			if ( ! grBitmap_BlitData(	&OldInfo,		Bmp->Data[Bmp->Info.MinimumMip], NULL,
										&(Bmp->Info),	Bmp->Data[Bmp->Info.MinimumMip], NULL,
										Bmp->Info.Width, Bmp->Info.Height) )
			{
				return GR_FALSE;
			}

			return GR_TRUE;
		}
	}
	else
	{
	grBitmap_Info OldInfo;
	int OldBPP,NewBPP;
	int OldMaxMips;
	DRV_Driver * Driver;

		if ( grPixelFormat_HasPalette(NewFormat) )
		{
			if ( Palette )
			{
				if ( ! grBitmap_SetPalette(Bmp,(grBitmap_Palette *)Palette) )
					return GR_FALSE;
			}
			else
			{
				if ( ! grBitmap_GetPalette(Bmp) && ! grPixelFormat_HasPalette(Bmp->Info.Format) )
				{
				grBitmap_Palette *NewPal;
					NewPal = grBitmap_Palette_CreateFromBitmap(Bmp,GR_FALSE);
					if ( ! NewPal )
					{
						grErrorLog_AddString(-1,"_SetFormat : createPaletteFromBitmap failed", NULL);
						return GR_FALSE;
					}
					if ( ! grBitmap_SetPalette(Bmp,NewPal) )
						return GR_FALSE;
					grBitmap_Palette_Destroy(&NewPal);
				}
			}
		}

		Driver = Bmp->Driver;
		if ( Driver )
			if ( ! grBitmap_DetachDriver(Bmp,GR_TRUE) )
				return GR_FALSE;

		OldBPP = grPixelFormat_BytesPerPel(Bmp->Info.Format);
		NewBPP = grPixelFormat_BytesPerPel(NewFormat);

		OldInfo = Bmp->Info;
		Bmp->Info.Format = NewFormat;
		Bmp->Info.HasColorKey = HasColorKey;
		Bmp->Info.ColorKey = ColorKey;

		// {} this is not very polite; we do restore them later, though...
		OldMaxMips = max(Bmp->Info.MaximumMip,Bmp->DriverInfo.MaximumMip);
		grBitmap_ClearMips(Bmp);		

		if ( ! Bmp->Wavelet && Bmp->Data[Bmp->Info.MinimumMip] == NULL && 
				Bmp->DriverHandle == NULL )
			return GR_TRUE;

		if ( OldBPP == NewBPP )
		{
		grBitmap * Lock;
		void * Bits;
			// can work in place
			if ( ! grBitmap_LockForWrite(Bmp,&Lock,0,0) )
				return GR_FALSE;

			if ( ! (Bits = grBitmap_GetBits(Lock)) )
			{
				grBitmap_UnLock(Lock);
				return GR_FALSE;
			}

			// _SetFormat : new format
			if ( ! grBitmap_BlitData(	&OldInfo,		Bits, Lock,
										&(Lock->Info),	Bits, Lock,
										Lock->Info.Width, Lock->Info.Height) )
			{
				grBitmap_UnLock(Lock);
				return GR_FALSE;
			}

			grBitmap_UnLock(Lock);
		}
		else // NewFormat is raw && != OldFormat
		{
		grBitmap OldBmp;
		grBitmap *Lock,*SrcLock;
		void *Bits,*OldBits;

			OldBmp = *Bmp;
			OldBmp.Info = OldInfo;

			// clear out the Bmp for putting the new format in
			Bmp->Info.Stride = Bmp->Info.Width;
			Bmp->Data[0] = NULL;
			Bmp->Alpha = NULL;
			Bmp->Wavelet = NULL;
			Bmp->WaveletMipLock = 0;

			if ( ! grBitmap_AllocSystemMip(Bmp,0) )
				return GR_FALSE;

			if ( ! grBitmap_LockForReadNative(&OldBmp,&SrcLock,0,0) )
				return GR_FALSE;

			if ( ! grBitmap_LockForWrite(Bmp,&Lock,0,0) )
				return GR_FALSE;

			if ( ! (Bits = grBitmap_GetBits(Lock)) )
			{
				grBitmap_UnLock(Lock);
				return GR_FALSE;
			}
			if ( ! (OldBits = grBitmap_GetBits(SrcLock)) )
			{
				grBitmap_UnLock(Lock);
				return GR_FALSE;
			}

			// _SetFormat : new format
			if ( ! grBitmap_BlitData(	&OldInfo,		OldBits,		SrcLock,
										&(Lock->Info),	Bits,			Lock,
										Lock->Info.Width, Lock->Info.Height) )
			{
				// try to undo as well as possible
				return GR_FALSE;
			}
		
			grBitmap_UnLock(Lock);
			grBitmap_UnLock(SrcLock);

			if ( OldBmp.Data[0] )
			{
				grRam_Free(OldBmp.Data[0]);
				OldBmp.Data[0] = NULL;
			}
			// ok, now delete wavelet
			if ( OldBmp.Wavelet )
			{
				grWavelet_Destroy(&(OldBmp.Wavelet));
			}

			if ( grPixelFormat_HasGoodAlpha(NewFormat) )
			{
				grBitmap_Destroy(&(OldBmp.Alpha));
			}
			else
			{
				Bmp->Alpha = OldBmp.Alpha;
			}
		}

		{
		int32 mip;
			mip = Bmp->Info.MinimumMip;
			while( mip < OldMaxMips )
			{
				grBitmap_UpdateMips(Bmp,mip,mip+1);
				mip++;
			}
		}

		if ( Driver )
		{		
			if ( ! grBitmap_AttachToDriver(Bmp,Driver,0) )
				return GR_FALSE;
		}
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_SetColorKey(grBitmap *Bmp, grBoolean HasColorKey, uint32 ColorKey , grBoolean Smart)
{
	assert( grBitmap_IsValid(Bmp) );

	if ( Bmp->LockOwner || Bmp->LockCount || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"SetColorKey : not an original bitmap", NULL);
		return GR_FALSE;	
	}

	// see comments in SetFormat

	if ( Bmp->DriverHandle )
		grBitmap_Update_DriverToSystem(Bmp);

	if ( HasColorKey && 
			((uint32)ColorKey>>1) >= ((uint32)1<<(grPixelFormat_BytesPerPel(Bmp->Info.Format)*8 - 1)) )
	{
		grErrorLog_AddString(-1,"grBitmap_SetColorKey : invalid ColorKey pixel!", NULL);
		return GR_FALSE;
	}
	if ( HasColorKey && grPixelFormat_HasAlpha(Bmp->Info.Format) )
	{
		grErrorLog_AddString(-1,"grBitmap_SetColorKey : non-fatal : Alpha and ColorKey together won't work right", NULL);
	}

	if ( HasColorKey && Smart && Bmp->Data[0] )
	{
		Bmp->Info.HasColorKey = GR_TRUE;
		Bmp->Info.ColorKey = ColorKey;
		if ( ! grBitmap_UsesColorKey(Bmp) )
		{
			Bmp->Info.HasColorKey = GR_FALSE;
			Bmp->Info.ColorKey = 1;
		}
	}
	else
	{
		Bmp->Info.HasColorKey = HasColorKey;
		Bmp->Info.ColorKey = ColorKey;
	}

	if ( Bmp->DriverHandle )
		grBitmap_Update_SystemToDriver(Bmp);

return GR_TRUE;
}

grBoolean grBitmap_UsesColorKey(const grBitmap * Bmp)
{
void * Bits;
const grPixelFormat_Operations * ops;
int32 x,y,w,h,s;
uint32 pel,ColorKey;

	grBitmap_WaitReady(Bmp);

	if ( ! Bmp->Info.HasColorKey )
		return GR_FALSE;

	if ( ! Bmp->Data[0] )
	{
		grErrorLog_AddString(-1,"UsesColorKey : no data!", NULL);
		return GR_TRUE;
	}

	assert( Bmp->Info.MinimumMip == 0 );

	Bits = Bmp->Data[0];
	ops = grPixelFormat_GetOperations(Bmp->Info.Format);
	assert(ops);

	w = Bmp->Info.Width;
	h = Bmp->Info.Height;
	s = Bmp->Info.Stride;

	ColorKey = Bmp->Info.ColorKey;

	switch(ops->BytesPerPel)
	{
		case 0:
			grErrorLog_AddString(-1,"UsesColorKey : invalid format", NULL);
			return GR_TRUE;
		case 3:
			#pragma message("Bitmap : UsesColorKey : no 24bit Smart ColorKey")
			grErrorLog_AddString(-1,"UsesColorKey : no 24bit Smart ColorKey", NULL);
			return GR_TRUE;	
		case 1:
		{
		uint8 * ptr;
			ptr = (uint8*)Bits;
			for(y=h;y--;)
			{
				for(x=w;x--;)
				{
					pel = *ptr++;
					if ( pel == ColorKey )
					{
						Log_Printf("UsesColorKey : Yes\n");
						return GR_TRUE;	
					}
				}
				ptr += (s-w);
			}
			break;
		}
		case 2:
		{
		uint16 * ptr;
			ptr = (uint16*)Bits;
			for(y=h;y--;)
			{
				for(x=w;x--;)
				{
					pel = *ptr++;
					if ( pel == ColorKey )
					{
						Log_Printf("UsesColorKey : Yes\n");
						return GR_TRUE;	
					}
				}
				ptr += (s-w);
			}
			break;
		}
		case 4:
		{
		uint32 * ptr;
			ptr = (uint32*)Bits;
			for(y=h;y--;)
			{
				for(x=w;x--;)
				{
					pel = *ptr++;
					if ( pel == ColorKey )
					{
						Log_Printf("UsesColorKey : Yes\n");
						return GR_TRUE;	
					}
				}
				ptr += (s-w);
			}
			break;
		}
	}
return GR_FALSE;
}


GRAPI grBoolean GRCC grBitmap_SetPalette(grBitmap *Bmp, const grBitmap_Palette *Palette)
{
	assert(Bmp); // not nec. valid
	assert( grBitmap_Palette_IsValid(Palette) );
	grBitmap_WaitReady(Bmp);

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;

/* //{} breaks PalCreate
	if ( Bmp->LockCount > 0 || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"SetPalette : not an original bitmap", NULL);
		return GR_FALSE;
	}
*/

	// warning : Bitmap_Blitdata calls us when it auto-creates a palette!

	// note that when we _SetPalette on a bitmap, all its write-locked children
	//	also get new palettes

	if ( Bmp->Info.Palette != Palette )
	{
		// save the palette even if we're not palettized, for later use
		if ( Palette->Driver )
		{
			if ( ! grBitmap_AllocPalette(Bmp,Palette->Format,NULL) )
				return GR_FALSE;
			
			if ( ! grBitmap_Palette_Copy(Palette,Bmp->Info.Palette) )
				return GR_FALSE;
		}
		else
		{
			if ( Bmp->Info.Palette )
				grBitmap_Palette_Destroy(&(Bmp->Info.Palette));

			Bmp->Info.Palette = (grBitmap_Palette *)Palette;
			grBitmap_Palette_CreateRef(Bmp->Info.Palette);
		}
	}

	if ( grPixelFormat_HasPalette(Bmp->DriverInfo.Format) &&
		Bmp->DriverInfo.Palette != Palette )
	{
		if ( Palette->Driver == Bmp->Driver && 
			( ! Palette->HasColorKey || ! Bmp->DriverInfo.ColorKey ||
				(uint32)Palette->ColorKeyIndex == Bmp->DriverInfo.ColorKey ) )
		{
			if ( Bmp->DriverInfo.Palette )
				grBitmap_Palette_Destroy(&(Bmp->DriverInfo.Palette));
			Bmp->DriverInfo.Palette = (grBitmap_Palette *)Palette;
			grBitmap_Palette_CreateRef(Bmp->DriverInfo.Palette);
		}
		else if ( Bmp->DriverInfo.Palette )
		{
			if ( ! grBitmap_Palette_Copy(Palette,Bmp->DriverInfo.Palette) )
				return GR_FALSE;
		}
		else
		{
			// IS GR_PIXELFORMAT_NO_DATA a safe replacement for 0 here?
			if ( ! grBitmap_AllocPalette(Bmp,GR_PIXELFORMAT_NO_DATA,Bmp->Driver) )
				return GR_FALSE;

			if ( ! grBitmap_Palette_Copy(Palette,Bmp->DriverInfo.Palette) )
				return GR_FALSE;
		}
	}

	if ( Bmp->DriverHandle )
	{
		// if one has pal and other doesn't this is real change!
		if (	grPixelFormat_HasPalette(Bmp->Info.Format) &&
			  ! grPixelFormat_HasPalette(Bmp->DriverInfo.Format) )
		{
			// this over-rides any driver changes!
			Bmp->DriverDataChanged = GR_FALSE;
			if ( ! grBitmap_Update_SystemToDriver(Bmp) )
				return GR_FALSE;
		}
		else if ( ! grPixelFormat_HasPalette(Bmp->Info.Format) &&
				grPixelFormat_HasPalette(Bmp->DriverInfo.Format) )
		{
			Bmp->DriverDataChanged = GR_TRUE;
		}
	}

	assert( grBitmap_IsValid(Bmp) );

return GR_TRUE;
}

GRAPI grBitmap_Palette * GRCC grBitmap_GetPalette(const grBitmap *Bmp)
{
	if ( ! Bmp ) return NULL;

	if ( Bmp->Driver && Bmp->DriverInfo.Palette )
	{
		assert(Bmp->Info.Palette);
		return Bmp->DriverInfo.Palette;
	}

	return Bmp->Info.Palette;
}


GRAPI grBitmap * GRCC grBitmap_GetAlpha(const grBitmap *Bmp)
{
	if ( ! Bmp ) return NULL;
	return Bmp->Alpha;
}

GRAPI grBoolean GRCC grBitmap_SetAlpha(grBitmap *Bmp, const grBitmap *AlphaBmp)
{
	assert( grBitmap_IsValid(Bmp) );
	
	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;
	if ( Bmp->LockCount > 0 || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"SetAlpha : not an original bitmap", NULL);
		return GR_FALSE;
	}

	if ( AlphaBmp == Bmp->Alpha )
		return GR_TRUE;

	if ( Bmp->DriverHandle )
	{
		grBitmap_Update_DriverToSystem(Bmp);
	}

	if ( Bmp->Alpha )
	{
		grBitmap_Destroy(&(Bmp->Alpha));
	}

	Bmp->Alpha = (grBitmap *)AlphaBmp;
	if ( AlphaBmp )
	{
		assert( grBitmap_IsValid(AlphaBmp) );
		grBitmap_CreateRef(Bmp->Alpha);
	}

	if ( Bmp->DriverHandle )
	{
		// upload the new alpha to the driver bitmap
		grBitmap_Update_SystemToDriver(Bmp);
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_SetPreferredFormat(grBitmap *Bmp,grPixelFormat Format)
{

	if ( Bmp->LockOwner )
		Bmp = Bmp->LockOwner;
	if ( Bmp->LockCount > 0 || Bmp->DataOwner )
	{
		grErrorLog_AddString(-1,"SetPrefferedFormat : not an original bitmap", NULL);
		return GR_FALSE;
	}

	if ( Bmp->PreferredFormat != Format )
	{
	DRV_Driver * Driver;
		Bmp->PreferredFormat = Format;
		Driver = Bmp->Driver;
		if ( Driver )
		{
			if ( ! grBitmap_DetachDriver(Bmp,GR_TRUE) )
				return GR_FALSE;
			if ( ! grBitmap_AttachToDriver(Bmp,Driver,0) )
				return GR_FALSE;
		}
	}

return GR_TRUE;
}

GRAPI grPixelFormat GRCC grBitmap_GetPreferredFormat(const grBitmap *Bmp)
{
	if ( ! Bmp ) return GR_PIXELFORMAT_NO_DATA;
return Bmp->PreferredFormat;
}

/*}{ ************** FILE I/O ************************/


GRAPI grBoolean  GRCC grBitmap_GetPersistableName(const grBitmap *Bmp, grVFile ** pBaseFS, char ** pName)
{
	if ( Bmp->Persistable )
	{
		*pBaseFS = (grVFile *)Bmp->PersistBaseFS;
		*pName = (char *)Bmp->PersistName;
		return GR_TRUE;
	}
	else
	{
		*pBaseFS = NULL;
		*pName = (char *)Bmp->PersistName;
		return GR_FALSE;
	}
}

GRAPI grBitmap * GRCC grBitmap_CreateFromFileName(const grVFile *BaseFS,const char *Name)
{
	grVFile * File;
	grBitmap * Bitmap;

	if ( BaseFS )
	{
		File = grVFile_Open((grVFile *)BaseFS, Name, GR_VFILE_OPEN_READONLY);
	}
	else
	{
		if ( strnicmp(Name,"http:",5) == 0 || strnicmp(Name,"ftp:",4) == 0 || strnicmp(Name,"www.",4) == 0 )
		{
		grVFile * inet;
			inet = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_INTERNET,NULL,NULL,GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);
			assert(inet);
			File = grVFile_Open(inet,Name,GR_VFILE_OPEN_READONLY);
			grVFile_Close(inet);
		}
		else
		{
			File = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS,Name,NULL,GR_VFILE_OPEN_READONLY);
		}
	}
	if ( ! File )
		return NULL;

	Bitmap = grBitmap_CreateFromFile(File);
	grVFile_Close(File);

	if ( ! Bitmap->Persistable )
	{
		Bitmap->Persistable = GR_TRUE;
		Bitmap->PersistBaseFS = BaseFS;
		strcpy(Bitmap->PersistName,Name);
	}

	return Bitmap;
}

GRAPI grBoolean GRCC grBitmap_WriteToFileName(const grBitmap * Bmp,const grVFile *BaseFS,const char *Name)
{
	grVFile * File;
	grBoolean Ret;

	if ( BaseFS )
	{
		File = grVFile_Open((grVFile *)BaseFS, Name, GR_VFILE_OPEN_CREATE);
	}
	else
	{
		File = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS,Name,NULL,GR_VFILE_OPEN_CREATE);
	}

	if ( ! File )
		return GR_FALSE;

	Ret = grBitmap_WriteToFile(Bmp,File);

	grVFile_Close(File);

	if ( ! Bmp->Persistable )
	{
		((grBitmap *)Bmp)->Persistable = GR_TRUE;
		((grBitmap *)Bmp)->PersistBaseFS = BaseFS;
		strcpy(((grBitmap *)Bmp)->PersistName,Name);
	}

	return Ret;
}

GRAPI grBitmap * GRCC grBitmap_CreateFromFileName2(const grVFile *BaseFS,const char *Name,grPtrMgr *PtrMgr)
{
	grVFile * File;
	grBitmap * Bitmap;

	if ( BaseFS )
	{
		File = grVFile_Open((grVFile *)BaseFS, Name, GR_VFILE_OPEN_READONLY);
	}
	else
	{
		if ( strnicmp(Name,"http:",5) == 0 || strnicmp(Name,"ftp:",4) == 0 || strnicmp(Name,"www.",4) == 0 )
		{
		grVFile * inet;
			inet = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_INTERNET,NULL,NULL,GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);
			assert(inet);
			File = grVFile_Open(inet,Name,GR_VFILE_OPEN_READONLY);
			grVFile_Close(inet);
		}
		else
		{
			File = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS,Name,NULL,GR_VFILE_OPEN_READONLY);
		}
	}
	if ( ! File )
		return NULL;
	Bitmap = grBitmap_CreateFromFile2(File,(grVFile *)BaseFS,PtrMgr);
	grVFile_Close(File);

	if ( ! Bitmap->Persistable )
	{
		Bitmap->Persistable = GR_TRUE;
		Bitmap->PersistBaseFS = BaseFS;
		strcpy(Bitmap->PersistName,Name);
	}

	return Bitmap;
}

GRAPI grBoolean GRCC grBitmap_WriteToFileName2(const grBitmap * Bmp,const grVFile *BaseFS,const char *Name,grPtrMgr *PtrMgr)
{
	grVFile * File;
	grBoolean Ret;

	if ( BaseFS )
	{
		File = grVFile_Open((grVFile *)BaseFS, Name, GR_VFILE_OPEN_CREATE);
	}
	else
	{
		File = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS,Name,NULL,GR_VFILE_OPEN_CREATE);
	}

	if ( ! File )
		return GR_FALSE;

	Ret = grBitmap_WriteToFile2(Bmp,File,PtrMgr);

	grVFile_Close(File);

	if ( ! Bmp->Persistable )
	{
		((grBitmap *)Bmp)->Persistable = GR_TRUE;
		((grBitmap *)Bmp)->PersistBaseFS = BaseFS;
		strcpy(((grBitmap *)Bmp)->PersistName,Name);
	}

	return Ret;
}

GRAPI grBitmap * GRCC grBitmap_CreateFromFile2(grVFile *VFile,grVFile *ResourceBaseFS,grPtrMgr *PtrMgr)
{
	grBitmap * Bmp;
	uint8 NameStrLen;

	if ( PtrMgr )
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Bmp))
			return NULL;

		if ( Bmp )
		{
			grBitmap_CreateRef(Bmp);
			return Bmp;
		}
	}
	
	grVFile_Read(VFile,&NameStrLen,1);

	if ( NameStrLen > 0 )
	{
	char Name[1024];
		grVFile_Read(VFile,Name,NameStrLen);
		Name[NameStrLen] = 0;

		Bmp = grBitmap_CreateFromFileName(ResourceBaseFS,Name);
	}
	else
	{
		Bmp = grBitmap_CreateFromFile(VFile);
	}

	if ( ! Bmp )
		return NULL;

	if ( PtrMgr )
		grPtrMgr_PushPtr(PtrMgr,Bmp);

return Bmp;
}

GRAPI grBoolean GRCC grBitmap_WriteToFile2(const grBitmap *Bmp,grVFile *VFile,grPtrMgr *PtrMgr)
{
uint8 NameStrLen;

	if ( PtrMgr )
	{
	uint32 Count;

		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void *)Bmp, &Count))
			return GR_FALSE;

		if (Count)		// Already loaded
			return GR_TRUE;
	}
	
	if ( ! Bmp->Persistable )	NameStrLen = 0;
	else						NameStrLen = strlen(Bmp->PersistName);

	grVFile_Write(VFile,&NameStrLen,1);

	if ( NameStrLen > 0 )
	{
		grVFile_Write(VFile,Bmp->PersistName,NameStrLen);
	}
	else
	{
		grBitmap_WriteToFile(Bmp,VFile);
	}

	if ( PtrMgr )
		grPtrMgr_PushPtr(PtrMgr,(void *)Bmp);

return GR_TRUE;
}

// GeBm Tag in 4 bytes {}
typedef uint32			grBmTag_t;
#define JEBM_TAG		((grBmTag_t)0x6D426547)	// "GeBm"

// version in a byte
#define JEBM_VERSION			(((uint32)JEBM_VERSION_MAJOR<<4) + (uint32)JEBM_VERSION_MINOR)
#define VERSION_MAJOR(Version)	(((Version)>>4)&0x0F)
#define VERSION_MINOR(Version)	((Version)&0x0F)

#define MIP_MASK				(0xF)
#define MIP_FLAG_COMPRESSED		(1<<4)
#define MIP_FLAG_PAETH_FILTERED	(1<<5)

static grBoolean grBitmap_ReadFromBMP(grBitmap * Bmp,grVFile * F);

GRAPI grBitmap * GRCC grBitmap_CreateFromFile(grVFile *F)
{
grBitmap *	Bmp;
grBmTag_t Tag;
grVFile * HF;

	assert(F);

	//{} we don't thread these reads, because we need to block on them!
	// the bitmap is not valid until these reads finish, and when we
	// return, we gaurantee a valid bitmap.

	Bmp = grBitmap_Create_Base();
	if ( ! Bmp )
		return NULL;

	Tag = 0;
	if ( (HF = grVFile_GetHintsFile(F)) != NULL )
	{
		if ( grVFile_Read(HF, &Tag, sizeof(Tag)) )
		{
			if ( Tag != JEBM_TAG )
			{
				grVFile_Seek(HF, - (int)sizeof(Tag), GR_VFILE_SEEKSET);
			}
		}
	}

	Bmp->StreamingStatus = GR_BITMAP_STREAMING_NOT;
		// we'll set it to CHANGED later

	if ( Tag == JEBM_TAG )
	{
	uint8 flags;
	uint8 Version;
	int32 mip;
	
		// see WriteToFile for comments on the file format
		assert( HF );

		if ( ! grVFile_Read(HF, &Version, sizeof(Version)) )
			goto fail;

		if ( VERSION_MAJOR(Version) != VERSION_MAJOR(JEBM_VERSION) )
		{
			grErrorLog_AddString(-1,"CreateFromFile : incompatible GeBm version", NULL);	
			goto fail;
		}

		if ( ! grBitmap_ReadInfo(Bmp,HF) )
			goto fail;

		if ( Bmp->Info.Palette )
		{
			Bmp->Info.Palette = NULL;
			if ( Version <= (4<<4) )
			{
				if ( ! ( Bmp->Info.Palette = grBitmap_Palette_CreateFromFile(F)) )
					goto fail;
			}
			else
			{
				if ( ! ( Bmp->Info.Palette = grBitmap_Palette_CreateFromFile(HF)) )
					goto fail;
			}
		}

		if ( Bmp->Info.Format == GR_PIXELFORMAT_WAVELET )
		{

			Bmp->Wavelet = grWavelet_CreateFromFile(Bmp,F);

			if ( ! Bmp->Wavelet )
			{
				grErrorLog_AddString(-1,"grWavelet_CreateFromFile failed!",NULL);
				goto fail;
			}

			if ( grWavelet_StreamingJob(Bmp->Wavelet) )
				Bmp->StreamingStatus = GR_BITMAP_STREAMING_STARTED;
			// else already set to STREAMING_NOT
		}
		else
		{
			for(;;)
			{
				if ( ! grVFile_Read(HF, &flags, sizeof(flags)) )
					goto fail;

				mip = flags & MIP_MASK;

				if ( mip > Bmp->Info.MaximumMip )
					break;

				assert(mip >= Bmp->Info.MinimumMip );
				assert( Bmp->Info.Stride == Bmp->Info.Width );

				if ( ! grBitmap_AllocSystemMip(Bmp,mip) )
					goto fail;

				if ( flags & MIP_FLAG_COMPRESSED )
				{
				grVFile * LzF;

					LzF = grVFile_OpenNewSystem(F,GR_VFILE_TYPE_LZ,NULL,NULL,GR_VFILE_OPEN_READONLY);
					if ( ! LzF )
					{
						grErrorLog_AddString(-1,"Bitmap_CreateFromFile : LZ File Open failed",NULL);
						return NULL;
					}

					if ( ! grVFile_Read(LzF, Bmp->Data[mip], grBitmap_MipBytes(Bmp,mip) ) )
					{
						grVFile_Close(LzF);
						grErrorLog_AddString(-1,"Bitmap_CreateFromFile : LZ File Read failed",NULL);
						return NULL;
					}

					if ( ! grVFile_Close(LzF) )
					{
						grErrorLog_AddString(-1,"Bitmap_CreateFromFile : LZ File Close failed",NULL);
						return NULL;
					}
				}
				else
				{
					if ( ! grVFile_Read(F, Bmp->Data[mip], grBitmap_MipBytes(Bmp,mip) ) )
						goto fail;
				}

				if ( flags & MIP_FLAG_PAETH_FILTERED )
				{
					grErrorLog_AddString(-1,"Bitmap_CreateFromFile : Paeth Filter not supported in this version!",NULL);
					return NULL;
				}

				Bmp->Modified[mip] = GR_TRUE;
			}
		}

		if( Bmp->Alpha )
		{
			if ( ! (Bmp->Alpha = grBitmap_CreateFromFile(F)) )
				goto fail;
		}
	}	// end grBitmap reader
	else 
	{
		if ( ! grVFile_Read(F, &Tag, sizeof(Tag)) )
			goto fail;

		if ( ! grVFile_Seek(F, - (int)sizeof(Tag), GR_VFILE_SEEKCUR) )
			goto fail;

		if ( (Tag&0xFFFF) == 0x4D42 )	// 'BM'
		{
		
			if ( ! grBitmap_ReadFromBMP(Bmp,F) )
				goto fail;
		}
		else
		{
			// grErrorLog_AddString(-1,"CreateFromFile : unknown format", NULL);
			goto fail;
		}
	}

	if ( ! Bmp->Persistable )
	{
		Bmp->Persistable = GR_TRUE;
		Bmp->PersistBaseFS = NULL;
		grVFile_GetName(F,Bmp->PersistName,sizeof(Bmp->PersistName));
	}

	return Bmp;

fail:
	assert(Bmp);

	grBitmap_Destroy(&Bmp);
	return NULL;
}

GRAPI grBoolean GRCC grBitmap_WriteToFile(const grBitmap *Bmp, grVFile *F)
{
grBmTag_t grBM_Tag;
uint8  grBM_Version;
uint8 flags;
int32 mip;
grVFile * HF;
	
	assert(Bmp && F);
	assert( grBitmap_IsValid(Bmp) );
	grBitmap_WaitReady(Bmp);

	grBM_Tag = JEBM_TAG;
	grBM_Version = JEBM_VERSION;

	if ( Bmp->DriverHandle )
	{
		if ( ! grBitmap_Update_DriverToSystem((grBitmap *)Bmp) )
		{
			grErrorLog_AddString(-1,"WriteToFile : Update_DriverToSystem", NULL);	
			return GR_FALSE;
		}
	}

	HF = grVFile_GetHintsFile(F);
	if ( ! HF )
		return GR_FALSE;

	if ( ! grVFile_Write(HF, &grBM_Tag, sizeof(grBM_Tag)) )
		return GR_FALSE;

	if ( ! grVFile_Write(HF, &grBM_Version, sizeof(grBM_Version)) )
		return GR_FALSE;

	if ( ! grBitmap_WriteInfo(Bmp,HF) )
		return GR_FALSE;

	#ifdef COUNT_HEADER_SIZES
		Header_Sizes += 15;
	#endif

	// the pointer Bmp->Info.Palette serves as boolean : HasPalette
	if ( Bmp->Info.Palette )
	{
		if ( ! grBitmap_Palette_WriteToFile(Bmp->Info.Palette,HF) )
			return GR_FALSE;
	}

	if ( Bmp->Info.Format == GR_PIXELFORMAT_WAVELET )
	{
		if ( ! grWavelet_WriteToFile(Bmp->Wavelet,F) )
			return GR_FALSE;
	}
	else
	{
		for( mip = Bmp->Info.MinimumMip; mip <= Bmp->Info.MaximumMip; mip++ )
		{

			// write out all the interesting mips :
			//	the first one, and then mips which are not just
			//	sub-samples of the first (eg. that have been user-set)

			if ( (mip == Bmp->Info.MinimumMip || Bmp->Modified[mip]) && Bmp->Data[mip] )
			{
			uint8 * MipData;
			grBoolean MipDataAlloced;
			uint32 MipDataLen;
			grVFile * LzF;

				MipDataLen = SHIFT_R_ROUNDUP(Bmp->Info.Width,mip) * SHIFT_R_ROUNDUP(Bmp->Info.Height,mip) *
								grPixelFormat_BytesPerPel(Bmp->Info.Format);

				if ( Bmp->Info.Stride == Bmp->Info.Width )
				{
					MipData = (uint8*)Bmp->Data[mip];
					MipDataAlloced = GR_FALSE;
				}
				else
				{
				int32 w,h,s,y;
				uint8 * fptr,*tptr;
				
					if ( ! (MipData = (uint8*)grRam_Allocate(MipDataLen) ) )
					{
						grErrorLog_AddString(-1,"Bitmap_WriteToFile : Ram_Alloc failed!",NULL);
						return GR_FALSE;
					}

					MipDataAlloced = GR_TRUE;

					s = SHIFT_R_ROUNDUP(Bmp->Info.Stride,mip)* grPixelFormat_BytesPerPel(Bmp->Info.Format);
					w = SHIFT_R_ROUNDUP(Bmp->Info.Width,mip) * grPixelFormat_BytesPerPel(Bmp->Info.Format);
					h = SHIFT_R_ROUNDUP(Bmp->Info.Height,mip);

					fptr = (uint8*)Bmp->Data[mip];
					tptr = MipData;
					for(y=h;y--;)
					{
						memcpy(tptr,fptr,w);
						fptr += s;
						tptr += w;
					}
				}

				assert( mip <= MIP_MASK );
				flags = (uint8)mip;
				//flags |= MIP_FLAG_COMPRESSED;

				if ( ! grVFile_Write(HF, &flags, sizeof(flags)) )
					return GR_FALSE;

				if ( flags & MIP_FLAG_COMPRESSED )
					LzF = grVFile_OpenNewSystem(F,GR_VFILE_TYPE_LZ,NULL,NULL,GR_VFILE_OPEN_CREATE);
				else
					LzF = F;
				
				if ( ! LzF )
				{
					if ( MipDataAlloced )
						grRam_Free(MipData);
					grErrorLog_AddString(-1,"Bitmap_WriteToFile : LZ File Open failed",NULL);
					return GR_FALSE;
				}

				if ( ! grVFile_Write(LzF, MipData, MipDataLen ) )
					return GR_FALSE;

				if ( flags & MIP_FLAG_COMPRESSED )
				{
					if ( ! grVFile_Close(LzF) )
					{
						if ( MipDataAlloced )
							grRam_Free(MipData);
						grErrorLog_AddString(-1,"Bitmap_WriteToFile : LZ File Close failed",NULL);
						return GR_FALSE;
					}
				}

				if ( MipDataAlloced )
					grRam_Free(MipData);
			}
		}
		
		// mip > MaximumMip signals End-Of-Mips

		flags = MIP_MASK;
		if ( ! grVFile_Write(HF, &flags, sizeof(flags)) )
			return GR_FALSE;
	}

	// the pointer Bmp->Alpha serves as boolean : HasAlpha

	if( Bmp->Alpha )
	{
		if ( ! grBitmap_WriteToFile(Bmp->Alpha,F) )
			return GR_FALSE;
	}

	if ( ! Bmp->Persistable )
	{
		((grBitmap *)Bmp)->Persistable = GR_TRUE;
		((grBitmap *)Bmp)->PersistBaseFS = NULL;
		grVFile_GetName(F,((grBitmap *)Bmp)->PersistName,sizeof(((grBitmap *)Bmp)->PersistName));
	}

return GR_TRUE;
}

/*}{********** Windows BMP Crap *******/

#pragma pack(1)
typedef struct 
{
	uint32      biSize;
	long		biWidth;
	long		biHeight;
	uint16      biPlanes;
	uint16      biBitCount;
	uint32      biCompression;
	uint32      biSizeImage;
	long		biXPelsPerMeter;
	long		biYPelsPerMeter;
	uint32      biClrUsed;
	uint32      biClrImportant;
} BITMAPINFOHEADER;

typedef struct 
{
	uint16   bfType;
	uint32   bfSize;
	uint16   bfReserved1;
	uint16   bfReserved2;
	uint32   bfOffBits;
} BITMAPFILEHEADER;

typedef struct 
{
    uint8    B;
    uint8    G;
    uint8    R;
    uint8    rgbReserved;
} RGBQUAD;
#pragma pack()

static grBoolean grBitmap_ReadFromBMP(grBitmap * Bmp,grVFile * F)
{
BITMAPFILEHEADER 	bmfh;
BITMAPINFOHEADER	bmih;
int32 bPad,myRowWidth,bmpRowWidth,pelBytes;

	// Windows Bitmap

	if ( ! grVFile_Read(F, &bmfh, sizeof(bmfh)) )
		return GR_FALSE;

	assert(bmfh.bfType == 0x4D42);

	bPad = bmfh.bfOffBits;

	if ( ! grVFile_Read(F, &bmih, sizeof(bmih)) )
		return GR_FALSE;

	if ( bmih.biSize > sizeof(bmih) )
	{
		grVFile_Seek(F, bmih.biSize - sizeof(bmih), GR_VFILE_SEEKCUR);
	}
	else if ( bmih.biSize < sizeof(bmih) )
	{
		grErrorLog_AddString(-1,"CreateFromFile : bmih size bad", NULL);	
		return GR_FALSE;
	}

	if ( bmih.biCompression )
	{
		grErrorLog_AddString(-1,"CreateFromFile : only BI_RGB BMP compression supported", NULL);
		return GR_FALSE;
	}

	bPad -= sizeof(bmih) + sizeof(bmfh);

	switch (bmih.biBitCount) 
	{
		case 8:			/* colormapped image */
			if ( bmih.biClrUsed == 0 ) bmih.biClrUsed = 256;

			if ( ! (Bmp->Info.Palette = grBitmap_Palette_Create(GR_PIXELFORMAT_32BIT_XRGB,bmih.biClrUsed)) )
				return GR_FALSE;

			if ( ! grVFile_Read(F, Bmp->Info.Palette->Data, bmih.biClrUsed * 4) )
				return GR_FALSE;

			bPad -= bmih.biClrUsed * 4;

			Bmp->Info.Format = GR_PIXELFORMAT_8BIT_PAL;
			pelBytes = 1;
			break;
		case 16:			
			Bmp->Info.Format = GR_PIXELFORMAT_16BIT_555_RGB;
			// tried 555,565_BGR & RGB, seems to have too much green
			pelBytes = 2;
			break;
		case 24:			
			Bmp->Info.Format = GR_PIXELFORMAT_24BIT_BGR;
			pelBytes = 3;
			break;
		case 32:			
			Bmp->Info.Format = GR_PIXELFORMAT_32BIT_XRGB; // surprisingly sane !?
			pelBytes = 4;
			break;
		default:
			return GR_FALSE;
	}

	if ( bPad < 0 )
	{
		grErrorLog_AddString(-1,"CreateFromFile : bPad bad", NULL);
		return GR_FALSE;
	}

	grVFile_Seek(F, bPad, GR_VFILE_SEEKCUR);
	
	Bmp->Info.Width = bmih.biWidth;
	Bmp->Info.Height = abs(bmih.biHeight);
	Bmp->Info.Stride = ((bmih.biWidth+3)&(~3));

	Bmp->Info.HasColorKey = GR_FALSE;

	myRowWidth	= Bmp->Info.Stride * pelBytes;
	bmpRowWidth = (((bmih.biWidth * pelBytes) + 3)&(~3));

	assert( bmpRowWidth <= myRowWidth );

	if ( ! grBitmap_AllocSystemMip(Bmp,0) )
		return GR_FALSE;

	if ( bmih.biHeight > 0 )
	{
	int32 y;
	char * row;
		row = (char *)Bmp->Data[0];
		row += (Bmp->Info.Height - 1) * myRowWidth;
		for(y= Bmp->Info.Height;y--;)
		{
			if ( ! grVFile_Read(F, row, bmpRowWidth) )
				return GR_FALSE;				
			row -= myRowWidth;
		}
	}
	else
	{
	int32 y;
	char * row;
		row = (char *)Bmp->Data[0];
		for(y= Bmp->Info.Height;y--;)
		{
			if ( ! grVFile_Read(F, row, bmpRowWidth) )
				return GR_FALSE;				
			row += myRowWidth;
		}
	}

return GR_TRUE;
}	// end BMP reader

/*
KROUER:
First try to save grBitmap as windows bmps
Function cancelled for the moment
static grBoolean grBitmap_WriteToBMP(grBitmap * Bmp,grVFile * F)
{
	BITMAPFILEHEADER bfh = 
	{
		((unsigned short)'B' | ((unsigned short)'M' << 8)),
		sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + Bmp->Info.Width * Bmp->Info.Height * 2,
		0,
		0,
		sizeof(BITMAPINFOHEADER)
	};

	BITMAPINFOHEADER bi =
	{
		sizeof(BITMAPINFOHEADER),
		Bmp->Info.Width,
		Bmp->Info.Height,
		1,
		24,
		0,
		0,
		0,
		0,
		0,
		0
	};
	return GR_TRUE;
}
*/

/*}{ *** Packed Info IO ***/

#define INFO_FLAG_WH_ARE_LOG2	(1<<0)
#define INFO_FLAG_HAS_CK     	(1<<1)
#define INFO_FLAG_HAS_ALPHA  	(1<<2)
#define INFO_FLAG_HAS_PAL    	(1<<3)
#define INFO_FLAG_HAS_AVERAGE  	(1<<4)
#define INFO_FLAG_IF_NOT_LOG2_ARE_BYTE	(1<<5)

grBoolean grBitmap_ReadInfo(grBitmap *Bmp,grVFile * F)
{
uint8 data[4];
uint8 flags;
uint8 b;
uint16 w;
grBitmap_Info * pi;

	pi = &(Bmp->Info);

	if ( ! grVFile_Read(F,data,3) )
		return GR_FALSE;

	flags = data[0];

	pi->Format = (grPixelFormat)data[1]; // could go in 5 bits
	if ( ! grPixelFormat_IsValid(pi->Format) )
		return GR_FALSE;

	b = data[2];

	pi->MaximumMip = (b>>4)&0xF;
	Bmp->SeekMipCount = (b)&0xF;

	if ( flags & INFO_FLAG_HAS_PAL )
		pi->Palette  = (grBitmap_Palette *)1;
	if ( flags & INFO_FLAG_HAS_ALPHA )
		Bmp->Alpha = (grBitmap *)1;

	if ( flags & INFO_FLAG_WH_ARE_LOG2 )
	{
	int logw,logh;

		if ( ! grVFile_Read(F,&b,1) )
			return GR_FALSE;

		logw = (b>>4)&0xF;
		logh = (b   )&0xF;

		pi->Width = 1<<logw;
		pi->Height= 1<<logh;
	}
	else if ( flags & INFO_FLAG_IF_NOT_LOG2_ARE_BYTE )
	{
		if ( ! grVFile_Read(F,&b,1) )
			return GR_FALSE;
		pi->Width = b;
		if ( ! grVFile_Read(F,&b,1) )
			return GR_FALSE;
		pi->Height = b;
	}
	else
	{
		if ( ! grVFile_Read(F,&w,2) )
			return GR_FALSE;
		pi->Width = w;
		if ( ! grVFile_Read(F,&w,2) )
			return GR_FALSE;
		pi->Height = w;
	}

	if ( (flags & INFO_FLAG_HAS_CK) && grPixelFormat_BytesPerPel(pi->Format) > 0 )
	{
	uint8 * ptr;
		pi->HasColorKey = GR_TRUE;

		if ( ! grVFile_Read(F,data,grPixelFormat_BytesPerPel(pi->Format)) )
			return GR_FALSE;
		
		ptr = data;
		pi->ColorKey = grPixelFormat_GetPixel(pi->Format,&ptr);
	}

	if ( flags & INFO_FLAG_HAS_AVERAGE )
	{
		if ( ! grVFile_Read(F,data,3) )
			return GR_FALSE;

		Bmp->HasAverageColor = GR_TRUE;
		Bmp->AverageR = data[0];
		Bmp->AverageG = data[1];
		Bmp->AverageB = data[2];
	}

	pi->Stride = pi->Width;

	return GR_TRUE;
}

grBoolean grBitmap_WriteInfo(const grBitmap *Bmp,grVFile * F)
{
uint8 data[64];
uint8 * ptr;
uint8 flags;
uint8 b;
int len,logw,logh;
const grBitmap_Info * pi;
int R,G,B;

/*
	bit flags :
		W&H are log2
		HasCK
		HasAlpha
		HasPal

		W&H logs in 1 byte, or W & H each in 2 bytes

		Format in 5 bits
		MaxMip in 3 bits
		Bmp->SeekMipCount in 3 bits

		CK in bpp bytes
*/

	pi = &(Bmp->Info);
	ptr = data;

	assert( pi->Width < 65536 && pi->Height < 65536 );
	assert( pi->MinimumMip == 0 );
	assert( grPixelFormat_IsValid(pi->Format) );

	flags = 0;
	*ptr++ = 0; // flags will go there

	*ptr++ = pi->Format; // could go in 5 bits

	b = (uint8)((pi->MaximumMip << 4) + Bmp->SeekMipCount); // could go in 6 bits
	*ptr++ = b;

	if ( pi->Palette )
		flags |= INFO_FLAG_HAS_PAL;
	if ( Bmp->Alpha )
		flags |= INFO_FLAG_HAS_ALPHA;

	for(logw=0;(1<<logw) < pi->Width;logw++);
	for(logh=0;(1<<logh) < pi->Height;logh++);

	if ( (1<<logw) == pi->Width && (1<<logh) == pi->Height )
	{
		flags |= INFO_FLAG_WH_ARE_LOG2;
		assert( logw <= 0xF && logh <= 0xF );
		b = (logw<<4) + logh;
		*ptr++ = b;
	}
	else
	{
		if ( pi->Width < 256 && pi->Height < 256 )
		{
			flags |= INFO_FLAG_IF_NOT_LOG2_ARE_BYTE;
			*ptr++ = (uint8)pi->Width;
			*ptr++ = (uint8)pi->Height;
		}
		else
		{
			*((uint16 *)ptr) = (uint16)pi->Width;  ptr += 2;
			*((uint16 *)ptr) = (uint16)pi->Height; ptr += 2;
		}
	}

	if ( pi->HasColorKey && grPixelFormat_BytesPerPel(pi->Format) > 0 )
	{
		flags |= INFO_FLAG_HAS_CK;

		grPixelFormat_PutPixel(pi->Format,&ptr,pi->ColorKey);
	}

	if ( grBitmap_GetAverageColor(Bmp,&R,&G,&B) )
	{
		flags |= INFO_FLAG_HAS_AVERAGE;
		*ptr++ = GR_CLAMP8(R);
		*ptr++ = GR_CLAMP8(G);
		*ptr++ = GR_CLAMP8(B);
	}

	*data = flags;
	len = (int)(ptr - data);

	if ( ! grVFile_Write(F,data,len) )
		return GR_FALSE;

	return GR_TRUE;
}

/*}{ ***************** Palette Functions *******************/

grBoolean grBitmap_Palette_BlitData(grPixelFormat SrcFormat,const void *SrcData,const grBitmap_Palette * SrcPal,
									grPixelFormat DstFormat,	  void *DstData,const grBitmap_Palette * DstPal,
									int32 Pixels)
{
char *SrcPtr,*DstPtr;
grBoolean SrcHasCK,DstHasCK;
uint32 SrcCK,DstCK;
int SrcCKi,DstCKi;

	assert( SrcData && DstData );

	assert( grPixelFormat_IsRaw(SrcFormat) );
	assert( grPixelFormat_IsRaw(DstFormat) );

	SrcPtr = (char *)SrcData;
	DstPtr = (char *)DstData;

	if ( SrcPal && SrcPal->HasColorKey )
	{
		SrcHasCK = GR_TRUE;
		SrcCK = SrcPal->ColorKey;
		SrcCKi = SrcPal->ColorKeyIndex;
	}
	else
	{
		SrcHasCK = GR_FALSE;
	}

	if ( DstPal && DstPal->HasColorKey )
	{
		DstHasCK = GR_TRUE;
		DstCK = DstPal->ColorKey;
		DstCKi = DstPal->ColorKeyIndex;
	}
	else
	{
		DstHasCK = GR_FALSE;
	}

#if 0 // {} ?
	if ( SrcHasCK && DstHasCK )
	{
		if ( DstCKi == -1 )
			DstCKi = SrcCKi;
	}
#endif

	// no, can't do this, and if SrcCKi < 0 then it's just ignored, which is correct
	//assert( SrcCKi >= 0 );
	//assert( DstCKi >= 0 );

	// CK -> no CK : do nothing
	// no CK -> CK : avoid CK
	// CK -> CK    : assert the CKI's are the same; change color at CKI

	{
	uint32 Pixel;
	int p,R,G,B,A;
	const grPixelFormat_Operations *SrcOps,*DstOps;
	grPixelFormat_Composer		ComposePixel;
	grPixelFormat_Decomposer	DecomposePixel;
	grPixelFormat_PixelPutter	PutPixel;
	grPixelFormat_PixelGetter	GetPixel;

		SrcOps = grPixelFormat_GetOperations(SrcFormat);
		DstOps = grPixelFormat_GetOperations(DstFormat);
		assert(SrcOps && DstOps);

		GetPixel = SrcOps->GetPixel;
		DecomposePixel = SrcOps->DecomposePixel;
		ComposePixel = DstOps->ComposePixel;
		PutPixel = DstOps->PutPixel;

		if ( SrcOps->AMask && ! DstOps->AMask )
		{
			// alpha -> CK in the palette
			for(p=0;p<Pixels;p++)
			{
				Pixel = GetPixel((uint8**)&SrcPtr);
				DecomposePixel(Pixel,&R,&G,&B,&A);
				if ( SrcHasCK && ( p == SrcCKi || Pixel == SrcCK ) ) 
					A = 0;
				Pixel = ComposePixel(R,G,B,A);

				if ( DstHasCK )
				{
					if ( p == DstCKi || A < 128 )
						Pixel = DstCK;
					else if ( Pixel == DstCK )
						Pixel ^= 1;

					// BTW this makes dark blue into dark purple on glide
				}
				PutPixel((uint8**)&DstPtr,Pixel);
			}
		}
		else if ( ! SrcOps->AMask && DstOps->AMask )
		{
			// CK -> alpha in the palette
			for(p=0;p<Pixels;p++)
			{
				Pixel = GetPixel((uint8**)&SrcPtr);
				DecomposePixel(Pixel,&R,&G,&B,&A);
				if ( SrcHasCK && ( p == SrcCKi || Pixel == SrcCK ) ) 
					A = 0;

				Pixel = ComposePixel(R,G,B,A);
				if ( DstHasCK )
				{
					if ( p == DstCKi )
						Pixel = DstCK;
					else if ( Pixel == DstCK )
						Pixel ^= 1;
				}
				PutPixel((uint8**)&DstPtr,Pixel);
			}
		}
		else
		{
			// both have alpha or both don't
			for(p=0;p<Pixels;p++)
			{
				Pixel = GetPixel((uint8**)&SrcPtr);
				DecomposePixel(Pixel,&R,&G,&B,&A);
				if ( (SrcHasCK && ( p == SrcCKi || Pixel == SrcCK )) ||
					 DstHasCK && p == DstCKi ) 
				{
					Pixel = DstCK;
				}
				else
				{
					Pixel = ComposePixel(R,G,B,A);
					if ( DstHasCK && Pixel == DstCK )
						Pixel ^= 1;
				}
				PutPixel((uint8**)&DstPtr,Pixel);
			}
		}
	}

return GR_TRUE;
}

GRAPI grBitmap_Palette * GRCC grBitmap_Palette_Create(grPixelFormat Format,int32 Size)
{
grBitmap_Palette * P;
int DataBytes;
const grPixelFormat_Operations * ops;

	ops = grPixelFormat_GetOperations(Format);
	if ( ! ops->RMask )
	{
		grErrorLog_AddString(-1,"grBitmap_Palette_Create : Invalid format for a palette!", NULL);
		return NULL;
	}

	DataBytes = grPixelFormat_BytesPerPel(Format) * Size;
	if ( DataBytes == 0 )
	{
		grErrorLog_AddString(-1,"grBitmap_Palette_Create : Invalid format for a palette!", NULL);
		return NULL;
	}

	P = (grBitmap_Palette *)grRam_Allocate(sizeof(grBitmap_Palette));
	if ( ! P ) return NULL;
	clear(P);

	P->Size = Size;
	P->Format = Format;
	if ( ! (P->Data = grRam_Allocate(DataBytes)) )
	{
		grRam_Free(P);
		return NULL;
	}

	P->RefCount = 1;
	P->LockCount = 0;

	P->HasColorKey = GR_FALSE;

return P;
}

GRAPI grBoolean GRCC grBitmap_Palette_CreateRef(grBitmap_Palette *P)
{
	if ( ! P || P->RefCount < 1 )
		return GR_FALSE;
	P->RefCount ++;
return GR_TRUE;
}

GRAPI grBitmap_Palette * GRCC grBitmap_Palette_CreateFromBitmap(grBitmap * Bmp,grBoolean Slow)
{
grBitmap_Palette * Pal;
	Pal = grBitmap_GetPalette(Bmp);
	if ( Pal )
	{
		grBitmap_Palette_CreateRef(Pal);
		return Pal;
	}
	else
	{
		return createPaletteFromBitmap(Bmp, Slow);
	}
}

grBitmap_Palette * BITMAP_GR_INTERNAL grBitmap_Palette_CreateFromDriver(DRV_Driver * Driver,grPixelFormat Format,int32 Size)
{
grBitmap_Palette * P;
grTexture_Info TInfo;

	assert(Driver);

	P = (grBitmap_Palette *)grRam_Allocate(sizeof(grBitmap_Palette));

	if ( ! P ) return NULL;
	clear(P);

	P->Size = Size;
	P->Driver = Driver;	

	// {} the pixelformat passed in here has non-trivial implications when the
	//		driver provides more than one possible palette type

	assert( grPixelFormat_IsRaw(Format) );

	P->DriverHandle = grBitmap_CreateTHandle(Driver,Size,1,1,
			Format,GR_PIXELFORMAT_NO_DATA,0,grPixelFormat_HasAlpha(Format),0,RDRIVER_PF_PALETTE);
	if ( ! P->DriverHandle )
	{
		grErrorLog_AddString(-1,"Palette_CreateFromDriver : CreateTHandle", NULL);	
		grRam_Free(P);
		return NULL;
	}

	Driver->THandle_GetInfo(P->DriverHandle,0,&TInfo);
	P->Format = TInfo.PixelFormat.PixelFormat;

	P->HasColorKey = (TInfo.Flags & RDRIVER_THANDLE_HAS_COLORKEY) ? GR_TRUE : GR_FALSE;
	P->ColorKey = TInfo.ColorKey;
	P->ColorKeyIndex = -1;

	P->RefCount = 1;

return P;
}

GRAPI grBitmap_Palette * GRCC grBitmap_Palette_CreateCopy(const grBitmap_Palette *Palette)
{
grBitmap_Palette * P;

	if ( ! Palette )
		return NULL;

	if ( Palette->Driver )
	{
		P = grBitmap_Palette_CreateFromDriver(Palette->Driver,Palette->Format,Palette->Size);
	}
	else
	{
		P = grBitmap_Palette_Create(Palette->Format,Palette->Size);
	}

	if ( ! P ) return NULL;

	if ( ! grBitmap_Palette_Copy(Palette,P) )
	{
		grBitmap_Palette_Destroy(&P);
		return NULL;
	}

return P;
}

GRAPI grBoolean GRCC grBitmap_Palette_Destroy(grBitmap_Palette ** ppPalette)
{
grBitmap_Palette * Palette;
	assert(ppPalette);
	if ( Palette = *ppPalette )
	{
		if ( Palette->LockCount )
			return GR_FALSE;
		Palette->RefCount --;
		if ( Palette->RefCount <= 0 )
		{
			if ( Palette->Data )
				grRam_Free(Palette->Data);
			if ( Palette->DriverHandle )
			{
				Palette->Driver->THandle_Destroy(Palette->DriverHandle);
				Palette->DriverHandle = NULL;
			}
			grRam_Free(Palette);
		}
	}
	*ppPalette = NULL;
return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_Lock(grBitmap_Palette *P, void **pBits, grPixelFormat *pFormat,int32 *pSize)
{
	assert(P);
	assert(pBits);

	if ( P->LockCount )
		return GR_FALSE;
	P->LockCount++;

	*pBits = NULL;

	if ( P->Data )
	{
		*pBits		= P->Data;
		if ( pFormat )
			*pFormat= P->Format;
		if ( pSize )
			*pSize	= P->Size;
	}
	else if ( P->DriverHandle )
	{
	grTexture_Info TInfo;

		if ( ! P->Driver->THandle_GetInfo(P->DriverHandle,0,&TInfo) )
			return GR_FALSE;

		if ( TInfo.Height != 1 )
			return GR_FALSE;

		if ( ! (P->Driver->THandle_Lock(P->DriverHandle,0,pBits)) )
			*pBits = NULL;

		P->DriverBits = *pBits;

		if ( pFormat )
			*pFormat = TInfo.PixelFormat.PixelFormat;
		if ( pSize )
			*pSize = TInfo.Width;
	}

	return (*pBits) ? GR_TRUE : GR_FALSE;
}

GRAPI grBoolean GRCC grBitmap_Palette_UnLock(grBitmap_Palette *P)
{
	assert(P);
	if ( P->LockCount <= 0 )
		return GR_FALSE;
	P->LockCount--;
	if ( P->LockCount == 0 )
	{
		if ( P->HasColorKey )
		{
			if ( P->ColorKeyIndex >= 0 && P->ColorKeyIndex < P->Size )
			{
			uint8 *Bits,*pBits;
			uint32 Pixel;
			int p;
			const grPixelFormat_Operations *ops;
			grPixelFormat_PixelPutter	PutPixel;
			grPixelFormat_PixelGetter	GetPixel;

				if ( P->Data )
				{
					Bits = (uint8*)P->Data;
				}
				else if ( P->DriverBits )
				{
					Bits = (uint8*)P->DriverBits;
				}

				ops = grPixelFormat_GetOperations(P->Format);
				assert(ops);

				GetPixel = ops->GetPixel;
				PutPixel = ops->PutPixel;

				for(p=0;p<P->Size;p++)
				{
					pBits = Bits;
					Pixel = GetPixel(&Bits);
					if ( p == P->ColorKeyIndex )
					{
						PutPixel(&pBits,P->ColorKey);
					}
					else if ( Pixel == P->ColorKey )
					{
						Pixel ^= 1;
						PutPixel(&pBits,Pixel);
					}
				}
			}
		}
		if ( P->DriverHandle )
		{
			if ( ! P->Driver->THandle_UnLock(P->DriverHandle,0) )
				return GR_FALSE;
			P->DriverBits = NULL;
		}
	}
return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_SetFormat(grBitmap_Palette * P,grPixelFormat Format)
{
void * NewData;
	
	assert(P);

	if ( P->DriverHandle ) // can't change format on card!
		return GR_FALSE;

	assert( ! P->HasColorKey ); // can't have colorkey accept on crappy Glide

	if ( Format == P->Format )
		return GR_TRUE;

	NewData = grRam_Allocate( grPixelFormat_BytesPerPel(Format) * P->Size );
	if ( ! NewData )
		return GR_FALSE;

	if ( ! grBitmap_Palette_BlitData(P->Format,P->Data,NULL,Format,NewData,NULL,P->Size) )
	{
		grRam_Free(NewData);
		return GR_FALSE;
	}

	grRam_Free(P->Data);
	P->Data = NewData;
	P->Format = Format;

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_GetData(const grBitmap_Palette *P,void *Into,grPixelFormat Format,int32 Size)
{
grPixelFormat FmFormat;
const void *FmData;
int32 FmSize;
grBoolean Ret;

	assert(P);
	assert(Into);

	if ( ! grBitmap_Palette_Lock((grBitmap_Palette *)P,(void **)&FmData,&FmFormat,&FmSize) )
		return GR_FALSE;

	if ( FmSize < Size )
		Size = FmSize;

	Ret = grBitmap_Palette_BlitData(FmFormat,FmData,P,Format,Into,NULL,Size);
	
	grBitmap_Palette_UnLock((grBitmap_Palette *)P);

return Ret;
}

GRAPI grBoolean GRCC grBitmap_Palette_GetInfo(const grBitmap_Palette *P,grBitmap_Info *pInfo)
{
	assert(P && pInfo);

	pInfo->Width = pInfo->Stride = P->Size;
	pInfo->Height = 1;

	pInfo->Format = P->Format;
	pInfo->HasColorKey = P->HasColorKey;
	pInfo->ColorKey = P->ColorKey;
	pInfo->MaximumMip = pInfo->MinimumMip = 0;
	pInfo->Palette = NULL;

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_SetData(grBitmap_Palette *P,const void *From,grPixelFormat Format,int32 Colors)
{
grPixelFormat PalFormat;
void *PalData;
int32 PalSize;
grBoolean Ret;

	assert(P);
	assert(From);

	if ( ! grBitmap_Palette_Lock(P,&PalData,&PalFormat,&PalSize) )
		return GR_FALSE;

	if ( PalSize < Colors )
		Colors = PalSize;

	Ret = grBitmap_Palette_BlitData(Format,From,NULL,PalFormat,PalData,P,Colors);
	
	if ( ! grBitmap_Palette_UnLock(P) )
		return GR_FALSE;

return Ret;
}

GRAPI grBoolean GRCC grBitmap_Palette_Copy(const grBitmap_Palette * Fm,grBitmap_Palette * To)
{
grPixelFormat FmFormat,ToFormat;
void *FmData,*ToData;
int32 FmSize,ToSize;
grBoolean Ret;

	assert(Fm);
	assert(To);
	if ( Fm == To )
		return GR_TRUE;

	if ( ! grBitmap_Palette_Lock((grBitmap_Palette *)Fm,&FmData,&FmFormat,&FmSize) )
		return GR_FALSE;

	if ( ! grBitmap_Palette_Lock(To,&ToData,&ToFormat,&ToSize) )
	{
		grBitmap_Palette_UnLock((grBitmap_Palette *)Fm);
		return GR_FALSE;
	}

	if ( FmSize > ToSize )
	{
		Ret = GR_FALSE;
	}
	else
	{
		Ret = grBitmap_Palette_BlitData(FmFormat,FmData,Fm,ToFormat,ToData,To,FmSize);
	}
	
	grBitmap_Palette_UnLock((grBitmap_Palette *)Fm);
	grBitmap_Palette_UnLock(To);

return Ret;
}

GRAPI grBoolean GRCC grBitmap_Palette_SetEntryColor(grBitmap_Palette *P,int32 Color,int32 R,int32 G,int32 B,int32 A)
{
	assert(P);
	
	if ( A < 80 && ! grPixelFormat_HasAlpha(P->Format) && P->HasColorKey )
	{
		return grBitmap_Palette_SetEntry(P,Color,P->ColorKey);
	}
	else if ( P->HasColorKey )
	{
	uint32 Pixel;

		// might have alpha AND colorkey !

		if ( Color == P->ColorKeyIndex ) // and A > 80 because of the above
			return GR_FALSE;

		Pixel = grPixelFormat_ComposePixel(P->Format,R,G,B,A);
		if ( Pixel == P->ColorKey )
			Pixel ^= 1;
			
		return grBitmap_Palette_SetEntry(P,Color,Pixel);
	}
	else
	{
		return grBitmap_Palette_SetEntry(P,Color,grPixelFormat_ComposePixel(P->Format,R,G,B,A));
	}
}

GRAPI grBoolean GRCC grBitmap_Palette_GetEntryColor(const grBitmap_Palette *P,int32 Color,int32 *R,int32 *G,int32 *B,int32 *A)
{
uint32 Pixel;
	assert(P);
	if ( P->HasColorKey )
	{
		if ( Color == P->ColorKeyIndex )
		{
			*R = *G = *B = *A = 0;
			return GR_TRUE;
		}
		else
		{
			if ( ! grBitmap_Palette_GetEntry(P,Color,&Pixel) )
				return GR_FALSE;
			if ( Pixel == P->ColorKey )
			{
				*R = *G = *B = *A = 0;
			}
			else
			{
				grPixelFormat_DecomposePixel(P->Format,Pixel,R,G,B,A);
			}
		}
	}
	else
	{
		if ( ! grBitmap_Palette_GetEntry(P,Color,&Pixel) )
			return GR_FALSE;
		grPixelFormat_DecomposePixel(P->Format,Pixel,R,G,B,A);
	}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_SetEntry(grBitmap_Palette *P,int32 Color,uint32 Pixel)
{
	assert(P);

	if ( P->HasColorKey )
	{
		if ( Color == P->ColorKeyIndex )
			return GR_FALSE;
	}

	if ( P->Data )
	{
	char *Data;

		if ( Color >= P->Size )
			return GR_FALSE;

		Data = (char *)(P->Data) + Color * grPixelFormat_BytesPerPel(P->Format);
		grPixelFormat_PutPixel(P->Format,(uint8 **)&Data,Pixel);
	}
	else
	{
	char *Data;
	grPixelFormat Format;
	int Size;

		if ( ! grBitmap_Palette_Lock(P,(void **)&Data,&Format,&Size) )
			return GR_FALSE;

		if ( Color >= Size )
		{
			grBitmap_Palette_UnLock(P);
			return GR_FALSE;
		}

		Data += Color * grPixelFormat_BytesPerPel(Format);
		grPixelFormat_PutPixel(Format,(uint8**)&Data,Pixel);

		grBitmap_Palette_UnLock(P);
	}
return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_GetEntry(const grBitmap_Palette *P,int32 Color,uint32 *Pixel)
{

	assert(P);

	if ( P->Data )
	{
	char *Data;

		if ( Color >= P->Size )
			return GR_FALSE;

		Data = (char *)(P->Data) + Color * grPixelFormat_BytesPerPel(P->Format);
		*Pixel = grPixelFormat_GetPixel(P->Format,(uint8 **)&Data);
	}
	else
	{
	char *Data;
	grPixelFormat Format;
	int Size;

		// must cast away const cuz we don't have a lockforread/write on palettes

		if ( ! grBitmap_Palette_Lock((grBitmap_Palette *)P,(void **)&Data,&Format,&Size) )
			return GR_FALSE;

		if ( Color >= Size )
		{
			grBitmap_Palette_UnLock((grBitmap_Palette *)P);
			return GR_FALSE;
		}

		Data += Color * grPixelFormat_BytesPerPel(Format);
		*Pixel = grPixelFormat_GetPixel(Format,(uint8 **)&Data);

		grBitmap_Palette_UnLock((grBitmap_Palette *)P);
	}
return GR_TRUE;
}

#define PALETTE_INFO_FORMAT_MASK	(0x1F)
#define PALETTE_INFO_FLAG_SIZE256	(1<<5)	// 5 is the low
#define PALETTE_INFO_FLAG_COMPRESS	(1<<6)

GRAPI grBitmap_Palette * GRCC grBitmap_Palette_CreateFromFile(grVFile *F)
{
grBitmap_Palette * P;
int Size;
grPixelFormat Format;
uint8 flags,b;
grVFile * HF;

	// for old version compatibility :
	// in new versions, F is already a hints file
	if ( ( HF = grVFile_GetHintsFile(F) ) == NULL )
		HF = F;

	if ( ! grVFile_Read(HF, &flags, sizeof(flags)) )
		return NULL;

	Format = (grPixelFormat)(flags & PALETTE_INFO_FORMAT_MASK);

	if ( flags & PALETTE_INFO_FLAG_SIZE256 )
	{
		Size = 256;
	}
	else
	{
		if ( ! grVFile_Read(HF, &b, sizeof(b)) )
			return NULL;
		Size = b;
	}

	P = grBitmap_Palette_Create(Format,Size);
	if ( ! P )
		return NULL;

	if ( flags & PALETTE_INFO_FLAG_COMPRESS )
	{
		if ( ! codePal_Read(P, F) )
		{
			grErrorLog_AddString(-1,"Bitmap_Palette_CreateFromFile : codePal failed!",NULL);
			return NULL;
		}
	}
	else
	{
		if ( ! grVFile_Read(F, P->Data, grPixelFormat_BytesPerPel(P->Format) * P->Size) )
		{
			grRam_Free(P);
			return NULL;
		}
	}

return P;
}

	// we usually write the palette's header in one byte :^)

grBoolean GRCC grBitmap_Palette_WriteHeaderToFile(int32 Size,grPixelFormat Format,grBoolean Compressed,grVFile *F)
{
uint8 b;

	b = Format;
	assert( b < 32 );
	if ( Size == 256 )
		b |= PALETTE_INFO_FLAG_SIZE256;

	// to compress :

	if ( Compressed )
		b |= PALETTE_INFO_FLAG_COMPRESS;

	if ( ! grVFile_Write(F, &b, sizeof(b)) )
		return GR_FALSE;

	if ( Size != 256 )
	{
		assert(Size < 256);
		b = (uint8)Size;
		
		if ( ! grVFile_Write(F, &b, sizeof(b)) )
			return GR_FALSE;
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_WriteToFile(const grBitmap_Palette *P,grVFile *F)
{
grPixelFormat Format;
void *Data;
int Size,codedLen,StartPos;

	assert(P);

	// assert(F->IsHintsFile); // F is a hints file!

	assert( P->HasColorKey == GR_FALSE ); // system palettes can't have color key!

	if ( ! grBitmap_Palette_Lock((grBitmap_Palette *)P,&Data,&Format,&Size) )
		return GR_FALSE;

	grBitmap_Palette_UnLock((grBitmap_Palette *)P);

	grVFile_Tell(F,(int32 *)&StartPos);

	grBitmap_Palette_WriteHeaderToFile(Size,Format,GR_TRUE,F);

	if ( ! codePal_Write(P, F, &codedLen) )
	{
		grErrorLog_AddString(-1,"Bitmap_Palette_WriteToFile : codePal failed!",NULL);
		return GR_FALSE;
	}
	
	if ( (uint32)codedLen > (grPixelFormat_BytesPerPel(Format) * Size) )
	{
		grVFile_Seek(F, StartPos, GR_VFILE_SEEKSET);

		grBitmap_Palette_WriteHeaderToFile(Size,Format,GR_FALSE,F);

		if ( ! grBitmap_Palette_Lock((grBitmap_Palette *)P,&Data,&Format,&Size) )
			return GR_FALSE;

		if ( ! grVFile_Write(F, Data, grPixelFormat_BytesPerPel(Format) * Size) )
		{
			grBitmap_Palette_UnLock((grBitmap_Palette *)P);
			return GR_FALSE;
		}
		
		grBitmap_Palette_UnLock((grBitmap_Palette *)P);
	}

return GR_TRUE;
}

GRAPI grBoolean GRCC grBitmap_Palette_SortColors(grBitmap_Palette * P,grBoolean Slower)
{
int permutation[256],usage[256];
uint8 paldata[768];
int flags;
int i;

	assert(P);
	
	for(i=0;i<256;i++)
	{
		usage[i] = 1;
		permutation[i] = i;
	}

	// <> usePal / reducePal

	if ( Slower )
		flags = SORTPAL_OPTIMIZE;
	else
		flags = SORTPAL_FAST;

	if ( grPixelFormat_HasAlpha(P->Format) )
	{
	uint8 rgba_in[1024];
	uint8 rgba_out[1024];

		// if the palette has alpha, use the permutation to shuffle  
		//	the alphas and thereby retain them!

		if ( ! grBitmap_Palette_GetData(P,rgba_in,GR_PIXELFORMAT_32BIT_RGBA,P->Size) )
			return GR_FALSE;
		if ( ! grBitmap_Palette_GetData(P,paldata,GR_PIXELFORMAT_24BIT_RGB,P->Size) )
			return GR_FALSE;

		if ( ! sortPal(P->Size,paldata,permutation,usage,flags) )
			return GR_FALSE;

		for(i=0;i<256;i++)
		{
		int j,R,G,B,A;
			j = permutation[i];
			R = paldata[3*i + 0];
			G = paldata[3*i + 1];
			B = paldata[3*i + 2];
			A = rgba_in[4*j];
			rgba_out[4*i + 0] = A;
			rgba_out[4*i + 1] = B;
			rgba_out[4*i + 2] = G;
			rgba_out[4*i + 3] = R;
		}

		if ( ! grBitmap_Palette_SetData(P,rgba_out,GR_PIXELFORMAT_32BIT_RGBA,P->Size) )
			return GR_FALSE;
	}
	else
	{
		if ( ! grBitmap_Palette_GetData(P,paldata,GR_PIXELFORMAT_24BIT_RGB,P->Size) )
			return GR_FALSE;

		if ( ! sortPal(P->Size,paldata,permutation,usage,flags) )
			return GR_FALSE;
			
		if ( ! grBitmap_Palette_SetData(P,paldata,GR_PIXELFORMAT_24BIT_RGB,P->Size) )
			return GR_FALSE;
	}

return GR_TRUE;
}

/*}{ ******************** EOF **************************/

// {} put ErrorLogs indicating where we failed in _IsValid

grBoolean grBitmap_IsValid(const grBitmap *Bmp)
{
	if ( ! Bmp ) return GR_FALSE;

	assert( Bmp->RefCount >= 1 );

	assert( ! (Bmp->LockCount && Bmp->LockOwner) );

	assert( !( (Bmp->DriverDataChanged || Bmp->DriverBitsLocked) &&
			! Bmp->DriverHandle ) );
	assert( ! (Bmp->DriverHandle && ! Bmp->Driver) );

	if ( ! grBitmap_Info_IsValid(&(Bmp->Info)) )
		return GR_FALSE;

	if ( Bmp->DriverHandle && ! grBitmap_Info_IsValid(&(Bmp->DriverInfo)) )
		return GR_FALSE;

	if ( Bmp->LockOwner && Bmp->Alpha )
		assert( Bmp->Alpha->LockOwner );

	if ( Bmp->LockOwner )
	{
		assert(Bmp->LockOwner != Bmp);
		assert( Bmp->LockOwner->LockCount );
	}

	if ( Bmp->DataOwner )
	{
		assert(Bmp->DataOwner != Bmp);
		assert( Bmp->DataOwner->RefCount >= 2 );
	}

	if ( Bmp->Alpha )
	{
		assert(Bmp->Alpha != Bmp);
		if ( ! grBitmap_IsValid(Bmp->Alpha) )
			return GR_FALSE;
	}

return GR_TRUE;
}

grBoolean grBitmap_Info_IsValid(const grBitmap_Info *Info)
{
	if ( ! Info ) return GR_FALSE;

	assert( Info->Width > 0 && Info->Height > 0 && Info->Stride >= Info->Width );

	assert( Info->MinimumMip >= 0 && Info->MaximumMip < MAXMIPLEVELS && Info->MinimumMip <= Info->MaximumMip );

	assert( Info->Format > GR_PIXELFORMAT_NO_DATA && Info->Format < GR_PIXELFORMAT_COUNT );

//	ok to have palette on non-palettized
//	if ( ! grPixelFormat_HasPalette(Info->Format) && Info->Palette )
//		return GR_FALSE;

	if ( Info->Palette )
		if ( ! grBitmap_Palette_IsValid(Info->Palette) )
			return GR_FALSE;

return GR_TRUE;
}

grBoolean grBitmap_Palette_IsValid(const grBitmap_Palette *Pal)
{
	if ( ! Pal ) return GR_FALSE;

	assert(  Pal->Data ||  Pal->DriverHandle );
	assert( !Pal->Data || !Pal->DriverHandle );

	assert( (Pal->Driver && Pal->DriverHandle) ||
		(! Pal->Driver && ! Pal->DriverHandle) );

	assert( Pal->RefCount >= 1 && Pal->Size >= 1 );
	assert( Pal->Format > GR_PIXELFORMAT_NO_DATA && Pal->Format < GR_PIXELFORMAT_COUNT );

return GR_TRUE;
}

#ifdef _DEBUG
GRAPI uint32 GRCC grBitmap_Debug_GetCount(void)
{
	assert(  _Bitmap_Debug_ActiveRefs >=  _Bitmap_Debug_ActiveCount );

//	Log_Printf("grBitmap_Debug_GetCount : Refs = %d\n",_Bitmap_Debug_ActiveRefs);

//	grBitmap_Gamma_Debug_Report();

	if (  _Bitmap_Debug_ActiveCount == 0 )
		assert(_Bitmap_Debug_ActiveRefs == 0 );

	return _Bitmap_Debug_ActiveCount;
}
#endif

/*}{ ******************** EOF **************************/

GRAPI grBoolean GRCC grBitmap_GetAverageColor(const grBitmap *Bmp,int32 *pR,int32 *pG,int32 *pB)
{

	if ( ! Bmp->HasAverageColor )
	{
	int32 bpp,x,y,w,h,xtra,dock;
	grPixelFormat Format;
	uint8 * ptr;
	uint32 R,G,B,A,Rt,Gt,Bt,cnt,ck;

		//{} Rt == Rtotal , probably won't overflow; we can handle a 4096x4095 solid-white image

		if ( Bmp->DriverHandle && Bmp->DriverDataChanged )
		{
			// must use the driver bits
			if ( ! grBitmap_Update_DriverToSystem((grBitmap *)Bmp) )
			{
				grErrorLog_AddString(-1,"Bitmap_AverageColor : DriverToSystem failed!",NULL);
				return GR_FALSE;
			}
		}

		Format = Bmp->Info.Format;
		bpp = grPixelFormat_BytesPerPel(Format);
		ptr = (uint8*)Bmp->Data[0];	

		if ( ! ptr || bpp < 1 )
		{
			grErrorLog_AddString(-1,"Bitmap_AverageColor : no data!",NULL);
			return GR_FALSE;
		}

		w = Bmp->Info.Width;
		h = Bmp->Info.Height;
		xtra = (Bmp->Info.Stride - w)*bpp;
		ck = Bmp->Info.ColorKey;
		dock = Bmp->Info.HasColorKey;

		Rt = Gt = Bt = cnt = 0;

		if ( grPixelFormat_HasPalette(Format) )
		{
			// <> Blech!
			grErrorLog_AddString(-1,"Bitmap_AverageColor : doesn't support palettized yet!",NULL);
			#pragma message("Bitmap_AverageColor : doesn't support palettized yet!")
			return GR_FALSE;
		}
		else
		{
		const grPixelFormat_Operations * ops;
		grPixelFormat_ColorGetter GetColor;
		grPixelFormat_PixelGetter GetPixel;
		grPixelFormat_Decomposer Decomposer;

			assert( grPixelFormat_IsRaw(Format) );

			ops = grPixelFormat_GetOperations(Format);
			GetColor = ops->GetColor;
			GetPixel = ops->GetPixel;
			Decomposer = ops->DecomposePixel;

			if ( dock )
			{
				for(y=h;y--;)
				{
					for(x=w;x--;)
					{
					uint32 Pixel;
						Pixel = GetPixel(&ptr);
						if ( Pixel != ck )
						{
							Decomposer(Pixel,(int32 *)&R,(int32 *)&G,(int32 *)&B,(int32 *)&A);
							Rt += R; Gt += G; Bt += B;
							cnt ++;
						}
					}
					ptr += xtra;
				}
			}
			else
			{
				for(y=h;y--;)
				{
					for(x=w;x--;)
					{
						GetColor(&ptr,(int32 *)&R,(int32 *)&G,(int32 *)&B,(int32 *)&A);
						if ( A > 80 )
						{
							Rt += R; Gt += G; Bt += B;
							cnt ++;
						}
					}
					ptr += xtra;
				}
			}
		}

		((grBitmap *)Bmp)->AverageR = (Rt + (cnt>>1)) / cnt;
		((grBitmap *)Bmp)->AverageG = (Gt + (cnt>>1)) / cnt;
		((grBitmap *)Bmp)->AverageB = (Bt + (cnt>>1)) / cnt;
		((grBitmap *)Bmp)->HasAverageColor = GR_TRUE;
	}

	if ( pR ) *pR = Bmp->AverageR;
	if ( pG ) *pG = Bmp->AverageG;
	if ( pB ) *pB = Bmp->AverageB;

return GR_TRUE;
}
