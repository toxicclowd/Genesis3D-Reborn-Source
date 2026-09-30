/****************************************************************************************/
/*  Engine.c                                                                            */
/*                                                                                      */
/*  Author: Charles Bloom/John Pollard                                                  */
/*  Description: Maintains the driver interface, as well as the bitmaps attached		*/
/*					to the driver.														*/
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

#ifdef WIN32
#include <windows.h>
#pragma warning (disable:4201)
#include <mmsystem.h> //timeGetTime
#pragma warning (default:4201)
#endif // WIN32

#ifdef BUILD_BE
#include <OS.h>
#include <image.h>
#include <Debug.h>
#include <stdio.h>
#define OutputDebugString(a) printf(a) //DEBUGGER
#define FreeLibrary unload_add_on
#define HINSTANCE image_id

#define Sleep(a) snooze(a * 1000)
#endif

#include <string.h>
#include <stdlib.h> // _MAX_PATH

#include "Dcommon.h"
#include "Engine.h"
#include "Engine._h"

#include "Errorlog.h"
#include "Dcommon.h"
#include "BitmapList.h"
#include "Bitmap.h"
#include "Bitmap._h"
#include "Log.h"
#include "List.h"
#include "grAssert.h"
#include "Cpu.h"
#include "VFile.h"
#include "grChain.h"
#include "Ram.h"
#include "grVersion.h" // Incarnadine

#include "grBSP.h"

#ifdef _DEBUG
	#define DEBUG_OUTPUT_LEVEL		0
	//#define DEBUG_OUTPUT_LEVEL		2
#else
	#define DEBUG_OUTPUT_LEVEL		0
#endif

//=====================================================================================
//=====================================================================================
typedef grBoolean GRCC grEngine_ShutdownDriverCB(DRV_Driver *Driver, void *Conext);
typedef grBoolean GRCC grEngine_StartupDriverCB(DRV_Driver *Driver, void *Conext);

typedef struct grEngine_ChangeDriverCB
{
	grEngine_ShutdownDriverCB		*ShutdownDriverCB;
	grEngine_StartupDriverCB		*StartupDriverCB;
	void							*Context;
} grEngine_ChangeDriverCB;

//=====================================================================================
//=====================================================================================
static grBoolean Engine_EnumSubDrivers(Engine_DriverInfo *DriverInfo, const char *DriverDirectory);
static grBoolean Engine_EnumSubDriversCB(int32 DriverId, char *Name, void *Context);
static grBoolean Engine_EnumModesCB(int32 ModeId, char *Name, int32 Width, int32 Height, int32 Bpp, void *Context);

static grBoolean Engine_InitDriver(	grEngine		*Engine, 
									HWND			hWnd,
									grDriver		*Driver,
									grDriver_Mode	*DriverMode);

static void Engine_DrawFontBuffer(grEngine *Engine);
static void Engine_Tick(grEngine *Engine);

static void SubLarge(LARGE_INTEGER *start, LARGE_INTEGER *end, LARGE_INTEGER *delta);

#define ABS(xx)	( (xx) < 0 ? (-(xx)) : (xx) )


// extern data for stats purpose - Krouer
extern int32 NumMakeFaces;
extern int32 NumMergedFaces;
extern int32 NumSubdividedFaces;

//=====================================================================================
// ------- Create/Destroy
//=====================================================================================

//=====================================================================================
//	grEngine_Create
//=====================================================================================
GRAPI grEngine * GRCC grEngine_Create(HWND hWnd, const char *AppName, const char *DriverDirectory)
{
	grEngine* Engine{};
	int32		i{}, Length{};

	assert(AppName);
	assert(hWnd);

	// Attempt to create a new engine object
	Engine = (grEngine *)grRam_AllocateClear(sizeof(grEngine));

	if (!Engine)
	{
		grErrorLog_Add(GR_ERR_OUT_OF_MEMORY, NULL);
		goto ExitWithError;
	}

	Engine->RefCount = 1;

	Engine->MySelf1 = Engine;
	Engine->MySelf2 = Engine;
	
	if (!List_Start())
	{
		grErrorLog_Add(GR_ERR_OUT_OF_MEMORY, NULL);
		goto ExitWithError;
	}	

	if	(DriverDirectory)
	{
		Length = strlen(DriverDirectory) + 1;
		Engine->DriverDirectory = (char *)grRam_Allocate(Length);

		if (!Engine->DriverDirectory)
			goto ExitWithError;

		memcpy(Engine->DriverDirectory, DriverDirectory, Length);
	}
	
	Engine->hWnd = hWnd;
	strcpy(Engine->AppName, AppName);
	
	// Build the wavetable
	for (i = 0; i < 20; i++)
		Engine->WaveTable[i] = (int16)(((i * 65)%200) + 50);

	// Be flexible if they didn't want us to load any driver DLLs
	if	(DriverDirectory)
	{
		if (! Engine_EnumSubDrivers(&Engine->DriverInfo, DriverDirectory))
			goto ExitWithError;
	}

	if (!grEngine_BitmapListInit(Engine))
		goto ExitWithError;

	if (!grEngine_InitFonts(Engine))				// Must be after BitmapList
		goto ExitWithError;

	Engine->DisplayFrameRateCounter = GR_TRUE;	// Default to showing the FPS counter

#if 0
	// @@ impolite !!
	grAssert_SetCriticalShutdownCallback( (grAssert_CriticalShutdownCallback)grEngine_ShutdownDriver , (uint32)Engine,
											NULL, NULL);
#endif	

	Engine->CurrentGamma = 3.0f;

	if (!grCPU_GetInfo() )
		goto ExitWithError;

	Engine->ChangeDriverCBChain = grChain_Create();

	if (!Engine->ChangeDriverCBChain)
		goto ExitWithError;

	return Engine;
	
	// Error cleanup:
	ExitWithError:
	{
		// BEGIN - FIX - Engine not cleaning up everything on error - paradoxnj
		//#pragma message ("Engine is not destroying everything on **error** here!")
		// END - FIX - Engine not cleaning up everything on error - paradoxnj
		if (Engine)
		{
			if (Engine->DriverDirectory)
				grRam_Free(Engine->DriverDirectory);

			// BEGIN - FIX - Engine not cleaning up everything on error - paradoxnj
			if (Engine->ChangeDriverCBChain != nullptr)
				grChain_Destroy(&Engine->ChangeDriverCBChain);

//			grEngine_ShutdownFonts(Engine);
			grEngine_BitmapListShutdown(Engine);
			List_Stop();
			// END - FIX - Engine not cleaning up everything on error - paradoxnj

			grRam_Free(Engine);
		}

		return nullptr;
	}
}

//=====================================================================================
//	grEngine_CreateRef
//=====================================================================================
GRAPI grBoolean GRCC grEngine_CreateRef(grEngine *Engine)
{
	assert(grEngine_IsValid(Engine));

	Engine->RefCount++;

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_Free
//=====================================================================================

GRAPI void	GRCC grEngine_Destroy(grEngine **pEngine)
{
	assert( pEngine );
	grEngine_Free(*pEngine);
	*pEngine = nullptr;
}

GRAPI void GRCC grEngine_Free(grEngine *Engine)
{
	grBoolean		Ret{};

	assert(grEngine_IsValid(Engine));
	assert( Engine->RefCount > 0);

	Engine->RefCount--;

	if (Engine->RefCount > 0)
		return;

	Ret = grEngine_ShutdownFonts(Engine);
	assert(Ret == GR_TRUE);

	Ret = grEngine_ShutdownDriver(Engine);
	assert(Ret == GR_TRUE);

	Ret = grEngine_BitmapListShutdown(Engine);
	assert(Ret == GR_TRUE);

	if (Engine->DriverDirectory)
		grRam_Free(Engine->DriverDirectory);

	if (Engine->ChangeDriverCBChain)
		grChain_Destroy(&Engine->ChangeDriverCBChain);

	List_Stop();

	grRam_Free(Engine);
}

//=====================================================================================
//	grEngine_IsValid
//=====================================================================================
GRAPI grBoolean GRCC grEngine_IsValid(const grEngine *E)
{
	if (!E) 
		return GR_FALSE;
	if (E->MySelf1 != E) 
		return GR_FALSE;
	if (E->MySelf2 != E) 
		return GR_FALSE;
	if (E->RefCount < 0)
		return GR_FALSE;
	//if (!IsWindowHandleValid(E->hWnd)) 
	if (!E->hWnd) 
		return GR_FALSE;

	return GR_TRUE;
}

//=====================================================================================
// ------- Misc functions
//=====================================================================================

//=====================================================================================
//	grEngine_EnabledFrameRateCounter
//=====================================================================================
GRAPI void	GRCC grEngine_EnableFrameRateCounter(grEngine *Engine, grBoolean Enabled)
{
	assert( grEngine_IsValid(Engine) );
	Engine->DisplayFrameRateCounter = Enabled;
}

//=====================================================================================
//	grEngine_Activate
//		this hits the drivers activation code to manage
//		surfaces and exclusive modes for devices (WM_ACTIVATEAPP)
//=====================================================================================
GRAPI grBoolean GRCC grEngine_Activate(grEngine *Engine, grBoolean bActive)
{
	DRV_Driver	*RDriver{};
	
	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);

	RDriver	=Engine->DriverInfo.RDriver;

	if( RDriver)
	{
		if ( RDriver->SetActive )
			return	RDriver->SetActive(bActive);
	}

	return	GR_TRUE;
}

static int32 UpdateWindowRecursion = 0;
//====================================================================================
//	grEngine_UpdateWindow
//		this call updates the drivers with a new rect to blit to
//		(usually the result of a window move or resize)
//====================================================================================
GRAPI grBoolean GRCC grEngine_UpdateWindow(grEngine *Engine)
{
	DRV_Driver	*RDriver{};
		
	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);

	assert(UpdateWindowRecursion == 0);

	UpdateWindowRecursion++;
	
	// Driver->UpdateWindow ONLY supports re-positioning, and NOT resizing... (As of 2/24/99)
	RDriver	= Engine->DriverInfo.RDriver;

	if( RDriver)
	{
	#if 0
		if (RDriver->UpdateWindow )
			return RDriver->UpdateWindow();
	#else
		grDriver* Driver{};
		grDriver_Mode* DriverMode{};

		if (!grEngine_GetDriverAndMode(Engine, &Driver, &DriverMode))
			return GR_FALSE;

		if (!grEngine_SetDriverAndMode(Engine, Engine->hWnd, Driver, DriverMode))
			return GR_FALSE;		
	#endif
	}

	UpdateWindowRecursion--;

	assert(UpdateWindowRecursion == 0);

	return GR_TRUE;
}

//===================================================================================
//	grEngine_GetFrameState
//===================================================================================
GRAPI grBoolean GRCC grEngine_GetFrameState(const grEngine *Engine, grEngine_FrameState *FrameState)
{
	*FrameState = Engine->FrameState;

	return GR_TRUE;
}

//===================================================================================
//-------- The main Frame functions:
//===================================================================================

//=====================================================================================
//	grEngine_Prep
//=====================================================================================
static	grBoolean grEngine_Prep(grEngine *Engine)
{
	assert( grEngine_IsValid(Engine) );

	return grEngine_AttachAll(Engine);
}

//===================================================================================
//	grEngine_BeginFrame
//===================================================================================
GRAPI grBoolean GRCC grEngine_BeginFrame(grEngine *Engine, grCamera *Camera, grBoolean ClearScreen)
{
	RECT	DrvRect{}, *pDrvRect{};

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("BEGIN grEngine_BeginFrame\n");
#endif

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);
	
	// Make sure the driver is avtive
	if (!Engine->DriverInfo.RDriver)
	{
		grErrorLog_Add(GR_ERR_DRIVER_NOT_INITIALIZED, NULL);
		return GR_FALSE;
	}
	
	assert(Engine->DriverInfo.RDriver != NULL);

	if (!grEngine_Prep(Engine))
		return GR_FALSE;

	// Do some timing stuff
#ifdef WIN32
	QueryPerformanceCounter(&Engine->CurrentTic);
#endif

	// Clear some debug info
	memset(&Engine->DebugInfo, 0, sizeof(Engine->DebugInfo));

//	Engine->FontInfo.NumDebugStrings = 0;

	if(Camera)
	{
		grRect			gDrvRect{};
		grDriver_Mode* CurMode{};

		grCamera_GetClippingRect(Camera, &gDrvRect);
	
		CurMode = Engine->DriverInfo.CurMode;

		if (CurMode->Width != -1 && CurMode->Height != -1)
		{
			if (gDrvRect.Left < 0)
			{
				grErrorLog_AddString(-1, "Invalid Camera for FULLSCREEN", NULL);
				return GR_FALSE;
			}

			if (gDrvRect.Right >= CurMode->Width)
			{
				grErrorLog_AddString(-1, "Invalid Camera for FULLSCREEN", NULL);
				return GR_FALSE;
			}

			if (gDrvRect.Top < 0)
			{
				grErrorLog_AddString(-1, "Invalid Camera for FULLSCREEN", NULL);
				return GR_FALSE;
			}

			if (gDrvRect.Bottom >= CurMode->Height)
			{
				grErrorLog_AddString(-1, "Invalid Camera for FULLSCREEN", NULL);
				return GR_FALSE;
			}
		}

		DrvRect.left	=gDrvRect.Left;
		DrvRect.top		=gDrvRect.Top;
		DrvRect.right	=gDrvRect.Right;
		DrvRect.bottom	=gDrvRect.Bottom;

		pDrvRect = &DrvRect;
	}
	else
		pDrvRect = nullptr;

	if (!Engine->DriverInfo.RDriver->BeginScene(ClearScreen, GR_TRUE, pDrvRect, (Engine->RenderMode==RenderMode_Lines)))
	{
		grErrorLog_Add(GR_ERR_DRIVER_BEGIN_SCENE_FAILED, NULL);
		return GR_FALSE;
	}

	Engine->FrameState = FrameState_Begin;

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("END grEngine_BeginFrame\n");
#endif

	return GR_TRUE;
}

#ifdef WIN32
//=====================================================================================
//	IsKeyDown
//=====================================================================================
static grBoolean IsKeyDown(int KeyCode, HWND hWnd)
{
	//if (GetFocus() == hWnd)
		if (GetAsyncKeyState(KeyCode) & 0x8000)
			return GR_TRUE;

	return GR_FALSE;
}
#endif

//===================================================================================
//	grEngine_GetFPS
//===================================================================================
GRAPI grFloat GRCC grEngine_GetFPS(grEngine *Engine)
{
	return Engine->Fps;
}

//===================================================================================
//	grEngine_EndFrame
//===================================================================================
GRAPI grBoolean GRCC grEngine_EndFrame(grEngine *Engine)
{
	LARGE_INTEGER		NowTic{}, DeltaTic{};
	float				Fps{};
	//DRV_Debug			*Debug;

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("BEGIN grEngine_EndFrame\n");
#endif

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_Begin);

	if (!Engine->DriverInfo.RDriver)
	{
		grErrorLog_Add(GR_ERR_DRIVER_NOT_INITIALIZED, NULL);
		return GR_FALSE;
	}
	
	assert(Engine->DriverInfo.RDriver != NULL);

	// Flush the scene before the text is drawn...
	grEngine_FlushScene(Engine);

	// Draw the text
	Engine_DrawFontBuffer(Engine);

	Engine->FrameState = FrameState_None;

	if (!Engine->DriverInfo.RDriver->EndScene())
	{
		grErrorLog_Add(GR_ERR_DRIVER_END_SCENE_FAILED, NULL);
		return GR_FALSE;
	}

	// Do some timing stuff
	QueryPerformanceCounter(&NowTic);	
	SubLarge(&Engine->CurrentTic, &NowTic, &DeltaTic);	
	
	if (DeltaTic.LowPart > 0)
		Fps =  (float)grCPU_PerformanceFreq / (float)DeltaTic.LowPart;
	else 
		Fps = 100.0f;

	Engine->Fps = Fps;

	#define AVERAGE_FPS_HISTORY (30)	// about one second

	if (Engine->DisplayFrameRateCounter == GR_TRUE)			// Dieplay debug info
	{
	float AverageFps{};
	DRV_CacheInfo	*pCacheInfo{};
	static float		FpsArray[AVERAGE_FPS_HISTORY];
	static int32		NumFps = 0, i;

		// Changed Average Fps to go accross last n frames, JP...
		FpsArray[(NumFps++) % AVERAGE_FPS_HISTORY] = Fps;
		
		for (AverageFps = 0.0f, i=0; i<AVERAGE_FPS_HISTORY; i++)
			AverageFps += FpsArray[i];

		AverageFps *= (1.0f/(float)AVERAGE_FPS_HISTORY);

		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Fps    : %2.2f / %2.2f", Fps, AverageFps);

		if (Engine->DriverInfo.RDriver->GPUTimings && Engine->DriverInfo.RDriver->GPUTimings->Valid)
		{
			const DRV_GPUTimings *Gpu = Engine->DriverInfo.RDriver->GPUTimings;
			grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "GPU    : %2.2f ms (scene %2.2f, present %2.2f)",
									Gpu->TotalMs, Gpu->SceneMs, Gpu->PresentMs);
		}
		if (Engine->DriverInfo.RDriver->GPUTimings)
		{
			const DRV_GPUTimings *Gpu = Engine->DriverInfo.RDriver->GPUTimings;
			grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Draws  : %4i (world %4i for %5i faces)",
									Gpu->DrawCalls, Gpu->WorldDraws, Gpu->WorldFaces);
		}
		
		
		Engine->DebugInfo.RenderedPolys = Engine->DriverInfo.RDriver->NumRenderedPolys;

		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Polys  : %4i/%4i/%4i", Engine->DebugInfo.TraversedPolys, Engine->DebugInfo.SentPolys, Engine->DebugInfo.RenderedPolys);

		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Mirrors: %3i, DLights: %3i, Fog    : %3i", 
								Engine->DebugInfo.NumMirrors,Engine->DebugInfo.NumDLights,Engine->DebugInfo.NumFog);

		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Actors : %3i, Models: %3i", Engine->DebugInfo.NumActors, Engine->DebugInfo.NumModels);
		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "LMap1  : %3i, LMap2  : %3i", Engine->DebugInfo.LMap1, Engine->DebugInfo.LMap2);
		
		grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "BSP    : TF %d", NumMakeFaces);

		pCacheInfo = Engine->DriverInfo.RDriver->CacheInfo;

		if (pCacheInfo)
		{
			grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Cache : tex: %3i (%3ik), lmap: %3i (%3ik)",
										pCacheInfo->TexMisses , pCacheInfo->TexMissBytes >>10,
										pCacheInfo->LMapMisses,pCacheInfo->LMapMissBytes >>10);
										
			grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), "Cache : mem: %3ik/%3ik/%3ik bal=%d bias=%f",
										pCacheInfo->CardMem >>10,pCacheInfo->SlotMem >>10,pCacheInfo->UsedMem >>10,
										pCacheInfo->Balances,pCacheInfo->MipBias);

			{
			char str[1024];
			int i;
				strcpy(str,"Cache : [");

				for(i=0;i<7;i++)
				{
				char work[1024];
					sprintf(work,"%d,%d,%d|",
						pCacheInfo->CacheSlots[i],
						pCacheInfo->CacheUses[i],
						pCacheInfo->CacheMisses[i]);
					strcat(str,work);
				}

				grEngine_DebugPrintf(Engine, GR_COLOR_XRGB(255, 255, 255), str);
			}
		}
	}

	// Do an engine frame
	Engine_Tick(Engine);

#if 0
	if (IsKeyDown(VK_F12, Engine->hWnd))
	{
		int32		i{};
		FILE* f{};
		char		Name[256];

		for (i=0 ;i<999; i++)		// Only 999 bmps, oh well...
		{
			sprintf(Name, "J3D%i.Bmp", i);

			f = fopen(Name, "rb");

			if (f)
			{
				fclose(f);
				continue;
			}
							
			grEngine_ScreenShot(Engine, Name);
		}
	}
#endif

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("END grEngine_EndFrame\n");
#endif

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_ScreenShot
//=====================================================================================
GRAPI grBoolean GRCC grEngine_ScreenShot(grEngine *Engine, const char *FileName)
{
	assert( grEngine_IsValid(Engine) );

	return Engine->DriverInfo.RDriver->ScreenShot(FileName);
}

//=====================================================================================
//	grEngine_DrawDDText
//=====================================================================================
GRAPI grBoolean GRCC grEngine_DrawText(grEngine *Engine, char *text, int x,int y,uint32 color)
{
	assert( grEngine_IsValid(Engine) );
   
   if (Engine->DriverInfo.RDriver->DrawText) {
	   return Engine->DriverInfo.RDriver->DrawText(text,x,y,color);
   } else {
      return grEngine_Printf(Engine, Engine->FontInfo.Font, x, y, color, text);
   }
}

//=====================================================================================
//	grEngine_SetFog
//=====================================================================================
GRAPI grBoolean GRCC grEngine_SetFog(grEngine *Engine, float r, float g, float b, float start, float endi, grBoolean enable)
{
	//assert( grEngine_IsValid(Engine) ); 
	//for some reason this prevents the code from working...
#pragma message ("Krouer: do not tested at 17th january 2005")

	//MessageBox(Engine->hWnd,"poks","poks",MB_OK);
	return Engine->DriverInfo.RDriver->SetFog(r,g,b,start,endi,enable);
}

//=====================================================================================
//	grEngine_GetDriver
//=====================================================================================
DRV_Driver * GRCF grEngine_GetDriver(const grEngine * Engine)
{
	assert( grEngine_IsValid(Engine) );
	
	return Engine->DriverInfo.RDriver;
}

//===================================================================================
//	grEngine_CreateChangeDriverCB
//===================================================================================
GRAPI grEngine_ChangeDriverCB * GRCC grEngine_CreateChangeDriverCB(	grEngine					*Engine, 
																	grEngine_ShutdownDriverCB	*ShutdownDriverCB, 
																	grEngine_StartupDriverCB	*StartupDriverCB,
																	void						*Context)
{
	grEngine_ChangeDriverCB		*ChangeDriverCB{};

	assert(grEngine_IsValid(Engine));
	assert(Engine->ChangeDriverCBChain);
	assert(ShutdownDriverCB);
	assert(StartupDriverCB);

	ChangeDriverCB = GR_RAM_ALLOCATE_STRUCT(grEngine_ChangeDriverCB);

	if (!ChangeDriverCB)
		return nullptr;

	ChangeDriverCB->ShutdownDriverCB = ShutdownDriverCB;
	ChangeDriverCB->StartupDriverCB = StartupDriverCB;
	ChangeDriverCB->Context = Context;

	// If there already is a driver, start it up now
	if (Engine->DriverInfo.RDriver)
	{
		if (!StartupDriverCB(Engine->DriverInfo.RDriver, Context))
		{
			grRam_Free(ChangeDriverCB);
			return nullptr;
		}
	}

	

	if (!grChain_AddLinkData(Engine->ChangeDriverCBChain, ChangeDriverCB))
	{
		if (!ShutdownDriverCB(Engine->DriverInfo.RDriver, Context))
		{
			assert(0);
		}
		grRam_Free(ChangeDriverCB);
		return nullptr;
	}

	return ChangeDriverCB;
}

//===================================================================================
//	grEngine_DestroyChangeDriverCB
//===================================================================================

GRAPI void GRCC grEngine_DestroyChangeDriverCB(grEngine *Engine, grEngine_ChangeDriverCB **ChangeDriverCB)
{
	grBoolean		Ret{};

	assert(grEngine_IsValid(Engine));
	assert(ChangeDriverCB);
	assert(*ChangeDriverCB);
	assert((*ChangeDriverCB)->ShutdownDriverCB);
	assert((*ChangeDriverCB)->StartupDriverCB);
	assert(grChain_FindLink(Engine->ChangeDriverCBChain, *ChangeDriverCB));

	// Actual calls to shutdown driver start here.
	// If there is a driver, shut it down now
	if (Engine->DriverInfo.RDriver)
	{
		if (!(*ChangeDriverCB)->ShutdownDriverCB(Engine->DriverInfo.RDriver, (*ChangeDriverCB)->Context))
		{
			assert(0);
		}
	}

	Ret = grChain_RemoveLinkData(Engine->ChangeDriverCBChain, *ChangeDriverCB);
	assert(Ret == GR_TRUE);

	grRam_Free(*ChangeDriverCB);
	*ChangeDriverCB = nullptr;
}

/*}{**** SECTION : Bitmap Lists  *********************/

//=====================================================================================
//	grEngine_SetGamma
//=====================================================================================
GRAPI grBoolean GRCC grEngine_SetGamma(grEngine *Engine, float Gamma)
{
	assert( grEngine_IsValid(Engine) );

	if ( Gamma < 0.01f )
		Gamma  = 0.01f;

	if ( ABS( Engine->CurrentGamma - Gamma) < 0.01f )
		return GR_TRUE;

	Engine->CurrentGamma = Gamma;

	grEngine_UpdateGamma(Engine);

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_GetGamma
//=====================================================================================
GRAPI grBoolean GRCC grEngine_GetGamma(grEngine *Engine, float *Gamma)
{
	assert( grEngine_IsValid(Engine) );
	assert(Gamma);

	*Gamma = Engine->CurrentGamma;

	return GR_TRUE;//Engine->DriverInfo.RDriver->GetGamma(Gamma);
}

GRAPI void GRCC grEngine_UpdateGamma(grEngine *Engine)
{
	DRV_Driver * RDriver{};
	grFloat LastBitmapGamma{};

	assert( grEngine_IsValid(Engine) );

	RDriver = Engine->DriverInfo.RDriver;

	LastBitmapGamma = Engine->BitmapGamma;

	if ( RDriver && (RDriver->EngineSettings->CanSupportFlags & DRV_SUPPORT_GAMMA) )
	{
		if ( RDriver->SetGamma(Engine->CurrentGamma) )
			Engine->BitmapGamma = 1.0f;
		else
			Engine->BitmapGamma = Engine->CurrentGamma;
	}
	else
	{
		Engine->BitmapGamma = Engine->CurrentGamma;
	}

	if ( ABS(Engine->BitmapGamma - LastBitmapGamma) < 0.1f )
	{
		Engine->BitmapGamma = LastBitmapGamma;
	}
	else
	{
		// Attach all the bitmaps for the engine
		if (!BitmapList_SetGamma(Engine->AttachedBitmaps, Engine->BitmapGamma))
		{
			grErrorLog_AddString(-1, "grEngine_UpdateGamma:  BitmapList_SetGamma for Engine failed", NULL);
		}
	}

}

//================================================================================
//	grEngine_BitmapListInit
//	Initializes the engine bitmaplist
//================================================================================
grBoolean grEngine_BitmapListInit(grEngine *Engine)
{
	assert( grEngine_IsValid(Engine) );
	assert(Engine->AttachedBitmaps == NULL);

	if ( Engine->AttachedBitmaps == NULL )
	{
		Engine->AttachedBitmaps = BitmapList_Create();
		if ( ! Engine->AttachedBitmaps )
		{
			grErrorLog_AddString(-1, "grEngine_BitmapListInit:  BitmapList_Create failed...", NULL);
			return GR_FALSE;
		}
	}
	return GR_TRUE;
}

//================================================================================
//	grEngine_BitmapListShutdown
//================================================================================
grBoolean grEngine_BitmapListShutdown(grEngine *Engine)
{
	assert( grEngine_IsValid(Engine) );

	if ( Engine->AttachedBitmaps )
	{
		assert(	Engine->DriverInfo.RDriver || BitmapList_CountMembersAttached(Engine->AttachedBitmaps) == 0 );

		//BitmapList_DetachAll(Engine->AttachedBitmaps);
		// Destroy detaches for you!
		BitmapList_Destroy(Engine->AttachedBitmaps);
		Engine->AttachedBitmaps = nullptr;
	}

	return GR_TRUE;
}

//================================================================================
//	grEngine_AddBitmap
//================================================================================
GRAPI grBoolean GRCC grEngine_AddBitmap(grEngine *Engine, grBitmap *Bitmap, grEngine_BitmapType Type)
{
	assert( grEngine_IsValid(Engine) );
	assert(Bitmap);
	assert(Engine->AttachedBitmaps);
	//assert(Engine->FrameState == FrameState_None);

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("grEngine_AddBitmap...\n");
#endif

	if ( Type == GR_ENGINE_BITMAP_TYPE_2D )
	{
		grBitmap_SetDriverFlags(Bitmap,RDRIVER_PF_2D);
	}
	else if ( Type == GR_ENGINE_BITMAP_TYPE_3D )
	{
		grBitmap_SetDriverFlags(Bitmap,RDRIVER_PF_3D); // <> combine lightmap is irrelevant ?
	}
	else
	{
		grErrorLog_AddString(-1, "grEngine_AddBitmap:  Invalid Type!", NULL);
		return GR_FALSE;
	}

	// Add bitmap to the list of bitmaps attached to the engine
	if ( BitmapList_Add(Engine->AttachedBitmaps, (grBitmap *)Bitmap) )
	{
		if ( Engine->DriverInfo.RDriver )
		{
			if ( ! grBitmap_AttachToDriver(Bitmap,Engine->DriverInfo.RDriver,0) )
			{
				grErrorLog_AddString(-1, "grEngine_AddBitmap:  AttachToDriver failed!", NULL);
				return GR_FALSE;
			}
		}
	}

	return GR_TRUE;
}

//================================================================================
//	grEngine_RemoveBitmap
//================================================================================
GRAPI grBoolean GRCC grEngine_RemoveBitmap(grEngine *Engine, grBitmap *Bitmap)
{
	assert( grEngine_IsValid(Engine) );
	assert(Bitmap);
	assert(Engine->AttachedBitmaps);
//	assert(Engine->FrameState == FrameState_None);

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("grEngine_RemoveBitmap...\n");
#endif

	if ( BitmapList_Remove(Engine->AttachedBitmaps, Bitmap) )
	{
		if (!grBitmap_DetachDriver(Bitmap, GR_TRUE))
		{
			grErrorLog_AddString(-1, "grEngine_RemoveBitmap:  grBitmap_DetachDriver failed...", NULL);
			return GR_FALSE;
		}
	}
	
	return GR_TRUE;
}


/*}{**** SECTION : Render/Draw  *********************/

//================================================================================
//	grEngine_RenderPoly
//================================================================================
GRAPI void GRCC grEngine_RenderPoly(const grEngine *Engine,
	const grTLVertex *Points, int NumPoints, const grMaterialSpec *Texture, uint32 Flags)
{
	grBoolean	Ret{};

	assert(grEngine_IsValid(Engine));
	assert(Engine->FrameState == FrameState_Begin);
	assert(Points );

	if ( Texture )
	{
		grTexture* TH{};
		grRDriver_Layer		Layer{};
		//assert(grEngine_HasBitmap(Engine, Texture) == GR_TRUE);		// This check is slow, but safe

		TH = grMaterialSpec_GetLayerTexture(Texture, 0);
		if (TH==nullptr) {
			grBitmap* bmp = grMaterialSpec_GetLayerBitmap(Texture, 0);
			if (bmp == nullptr) return;
			TH = grBitmap_GetTHandle(bmp);
		}
		assert(TH);

		Layer.THandle = TH;

		Flags |= Engine->DefaultRenderFlags;

		Ret = Engine->DriverInfo.RDriver->RenderMiscTexturePoly((grTLVertex *)Points, NumPoints, &Layer, 1, Flags);
	}
	else
	{
		Ret = Engine->DriverInfo.RDriver->RenderGouraudPoly((grTLVertex *)Points, NumPoints, Flags);
	}

	assert(Ret == GR_TRUE);
}

GRAPI void GRCC grEngine_RenderPolyArray(const grEngine *Engine, const grTLVertex ** pPoints, int * pNumPoints, int NumPolys, 
								const grMaterialSpec *Texture, uint32 Flags)
{
	grBoolean		Ret{};
	int				pn{};
	DRV_Driver* Driver{};
	grRDriver_Layer Layer{};


	assert(grEngine_IsValid(Engine));
	assert(Engine->FrameState == FrameState_Begin);
	assert(pPoints && pNumPoints );

	Driver = Engine->DriverInfo.RDriver;
	assert(Driver);

	if ( Texture )
	{
		grTexture* TH{};
	
		TH = grMaterialSpec_GetLayerTexture(Texture, 0);
		assert(TH);

		Layer.THandle = TH;

		Flags |= Engine->DefaultRenderFlags;

		for(pn=0;pn<NumPolys;pn++)
		{
			assert(pPoints[pn]);
			Ret = Driver->RenderMiscTexturePoly((grTLVertex *)pPoints[pn],pNumPoints[pn],&Layer, 1, Flags);
			assert(Ret);
		}
	}
	else
	{
		for(pn=0;pn<NumPolys;pn++)
		{
			assert(pPoints[pn]);
			Ret = Driver->RenderGouraudPoly((grTLVertex *)pPoints[pn],pNumPoints[pn],Flags);
			assert(Ret);
		}
	}

}

//================================================================================
//	grEngine_DrawBitmap
//================================================================================
GRAPI grBoolean GRCC grEngine_DrawBitmap(const grEngine *Engine,
	const grBitmap *Bitmap,
	const grRect * Source, uint32 x, uint32 y)
{
	grTexture* TH{};
	grBoolean			Ret{};
	
	//#pragma message("make grRect the same as RECT, or don't use RECT!?")
	// The drivers once did not include Jet3D .h's
	// (D3D uses RECT so thats why the drivers adopted RECT's...)
	#pragma message("Engine : Make the drivers use grRect, JP")

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_Begin);
	assert(Bitmap);
	
	assert(Engine->AttachedBitmaps);
	assert(BitmapList_Has(Engine->AttachedBitmaps, (grBitmap *)Bitmap) == GR_TRUE);

	TH = grBitmap_GetTHandle(Bitmap);
	assert(TH);

	//Ret = Engine->DriverInfo.RDriver->Drawdecal(TH,(RECT *)Source,x,y);

	if (Source)		// Source CAN be NULL!!!
	{
		RECT rect{};

		rect.left = Source->Left;
		rect.top = Source->Top;
		rect.right = Source->Right;
		rect.bottom = Source->Bottom;

		Ret = Engine->DriverInfo.RDriver->DrawDecal(TH, &rect, x,y);
	}
	else
		Ret = Engine->DriverInfo.RDriver->DrawDecal(TH, NULL, x,y);

	if ( ! Ret )
	{
		grErrorLog_AddString(-1,"grEngine_DrawBitmap : DrawDecal failed", NULL);	
	}

	return Ret;
}

GRAPI grTexture *GRCC grEngine_CreateTextureFromFile(const grEngine *Engine, grVFile *File)
{
	grBitmap* pBmp{};

	// DDS files (G3DTexImport) go to the driver as a memory image.
	if (Engine->DriverInfo.RDriver->THandle_CreateFromDDS) {
		long		Size = 0;
		uint32		Magic = 0;

		if (grVFile_Size(File, &Size) && Size > 128 &&
			grVFile_Read(File, &Magic, sizeof(Magic)) && Magic == 0x20534444) {	// "DDS "
			uint8		*Data = (uint8*)grRam_Allocate(Size);
			grTexture	*Texture = NULL;

			if (Data) {
				memcpy(Data, &Magic, sizeof(Magic));
				if (grVFile_Read(File, Data + sizeof(Magic), Size - sizeof(Magic)))
					Texture = Engine->DriverInfo.RDriver->THandle_CreateFromDDS(Data, (uint32)Size);
				grRam_Free(Data);
			}
			return Texture;
		}
		grVFile_Seek(File, 0, GR_VFILE_SEEKSET);
	}

	if (Engine->DriverInfo.RDriver->THandle_CreateFromFile) {
		return Engine->DriverInfo.RDriver->THandle_CreateFromFile(File);
	}
	
	// code to replace the splash screen - This is a memory leak - paradoxnj
	//pBmp = grBitmap_CreateFromFile(File);
	//grEngine_AddBitmap((grEngine*)Engine, pBmp, GR_ENGINE_BITMAP_TYPE_3D);
	//return grBitmap_GetTHandle(pBmp);
	return NULL;
}

GRAPI void GRCC grEngine_DestroyTexture(const grEngine *Engine, grTexture *Texture)
{
	Engine->DriverInfo.RDriver->THandle_Destroy(Texture);
}

GRAPI grBoolean GRCC grEngine_DrawTexture(const grEngine *Engine, const grTexture *Texture, int32 x, int32 y)
{
	return Engine->DriverInfo.RDriver->DrawDecal((grTexture*)Texture, NULL, x, y);
}

//================================================================================
//	grEngine_DrawBitmap3D
//================================================================================
GRAPI grBoolean GRCC grEngine_DrawBitmap3D(const grEngine *Engine,
	const grBitmap *Bitmap, const grRect * pRect, uint32 x, uint32 y)
{
	grTexture* TH{};
	grBoolean			Ret{};
	float				w{}, h{};
	float				u1{}, v1{}, u2{}, v2{};
	grTLVertex			Points[4];
	grRect				Rect{};
	grRDriver_Layer		Layer{};

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_Begin);
	assert(Bitmap);
	
	assert(Engine->AttachedBitmaps);
	assert(BitmapList_Has(Engine->AttachedBitmaps, (grBitmap *)Bitmap) == GR_TRUE);

	w = (float)grBitmap_Width( Bitmap);
	h = (float)grBitmap_Height(Bitmap);

	assert( w <= 256.0f );
	assert( h <= 256.0f );

	w = 1.0f/w; h = 1.0f/h;

	TH = grBitmap_GetTHandle(Bitmap);
	assert(TH);

	if ( pRect )
	{
		Rect = *pRect;
		Rect.Right ++;
		Rect.Bottom ++;
	}
	else
	{
		Rect.Left = Rect.Top = 0;
		Rect.Right = grBitmap_Width( Bitmap);
		Rect.Bottom = grBitmap_Height(Bitmap);
	}

	u1 = Rect.Left * w;
	u2 = Rect.Right* w;
	v1 = Rect.Top  * h;
	v2 = Rect.Bottom*h;

	assert( u1 >= 0.0f && u1 <= 1.0f );
	assert( u2 >= 0.0f && u2 <= 1.0f );
	assert( v1 >= 0.0f && v1 <= 1.0f );
	assert( v2 >= 0.0f && v2 <= 1.0f );
	assert( u2 >= u1 && v2 >= v1 );

	w = (float)(Rect.Right - Rect.Left);
	h = (float)(Rect.Bottom - Rect.Top);

	Points[0].x = (float)x;
	Points[0].y = (float)y;
	Points[0].z = 1.0f;
	Points[0].u = u1;
	Points[0].v = v1;
	Points[0].r = Points[0].g = Points[0].b = Points[0].a = 255.0f;

	Points[3] = Points[2] = Points[1] = Points[0];

	Points[2].u = Points[1].u = u2;
	Points[1].x += w;
	Points[2].x += w;

	Points[3].v = Points[2].v = v2;
	Points[2].y += h;
	Points[3].y += h;

	Layer.THandle = TH;

	Ret = Engine->DriverInfo.RDriver->RenderMiscTexturePoly((grTLVertex *)Points, 4, &Layer, 1, GR_RENDER_FLAG_CLAMP_UV);

	if ( ! Ret )
	{
		grErrorLog_AddString(-1,"grEngine_DrawBitmap3D : Render failed", NULL);	
	}

	return Ret;
}

//=====================================================================================
//	grEngine_AttachAll
//=====================================================================================
grBoolean grEngine_AttachAll(grEngine *Engine)
{
	DRV_Driver* RDriver{};

	assert( Engine );
	// called in beginframe	

	RDriver = Engine->DriverInfo.RDriver;
	assert( RDriver );

    // If current driver is not active, then split
	if (! RDriver)
        return GR_TRUE;

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("BEGIN BitmapList_AttachAll\n");
#endif

	// Attach all the bitmaps for the engine
	if (!BitmapList_AttachAll(Engine->AttachedBitmaps, RDriver, Engine->BitmapGamma))
	{
		grErrorLog_AddString(-1, "grEngine_AttachAll:  BitmapList_AttachAll for Engine failed...", NULL);
		return GR_FALSE;
	}

#if (DEBUG_OUTPUT_LEVEL >= 2)
	OutputDebugString("END BitmapList_AttachAll\n");
#endif

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_DetachAll
//=====================================================================================
grBoolean grEngine_DetachAll(grEngine *Engine)
{
	assert( grEngine_IsValid(Engine) );

	// Shutdown all the grBitmaps
	if (!BitmapList_DetachAll(Engine->AttachedBitmaps))
	{
		grErrorLog_AddString(-1, "grEngine_DetachAll:  BitmapList_DetachAll failed for engine.", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

/*}{**** SECTION : Init/Reset/Shutdown  *********************/

extern unsigned char splash_bmp[];
extern int splash_bmp_Length;
static	grBoolean	grEngine_DoSplashScreen(grEngine *Engine, grDriver_Mode *DriverMode)
{
	int32					Width{}, Height{};
	grRect 					Rect{};
	grBitmap* Bitmap{};
	grTexture* Texture{};
	grTexture_Info			Info{};
	grVFile* MemFile{};
	grVFile_MemoryContext	Context{};
	int32					ImageWidth{}, ImageHeight{};
	int32					X{}, Y{};
	grBoolean				UseJeBitmap = GR_FALSE;

	grDriver_ModeGetWidthHeight(DriverMode, &Width, &Height);
	if (Width == -1)
	{
	
		RECT	R{};
		GetClientRect(Engine->hWnd, &R);

		Rect.Left = R.left;
		Rect.Right = R.right;
		Rect.Top = R.top;
		Rect.Bottom = R.bottom;
	}
	else
	{
		Rect.Left = 0;
		Rect.Right = Width-1;
		Rect.Top = 0;
		Rect.Bottom = Height-1;
	}

	Context.Data = splash_bmp;
	Context.DataLength = splash_bmp_Length;
	MemFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_MEMORY, NULL, &Context, GR_VFILE_OPEN_READONLY);
	if	(!MemFile)
		return GR_FALSE;
	
	Texture = grEngine_CreateTextureFromFile(Engine, MemFile);
	if (!Texture)
	{
		UseJeBitmap = GR_TRUE;

		Bitmap = grBitmap_CreateFromFile(MemFile);
		if (!Bitmap)
		{
			OutputDebugString("grEngine_DoSplashScreen:  Could not create bitmap!!");
			return GR_FALSE;
		}

		if (grEngine_AddBitmap(Engine, Bitmap, GR_ENGINE_BITMAP_TYPE_2D) == GR_FALSE)
		{
			OutputDebugString("grEngine_DoSplashScreen:  Could not add bitmap to engine!!");
			return GR_FALSE;
		}

		ImageWidth = grBitmap_Width(Bitmap);
		ImageHeight = grBitmap_Height(Bitmap);
	}
	else
	{
		Engine->DriverInfo.RDriver->THandle_GetInfo(Texture, 0, &Info);

		ImageWidth = Info.Width;
		ImageHeight = Info.Height;
	}

	grVFile_Close(MemFile);

	X = (Rect.Right - ImageWidth) / 2;
	Y = (Rect.Bottom - ImageHeight) / 2;
	
	
	grEngine_BeginFrame(Engine, NULL, GR_TRUE);

	if (!UseJeBitmap)
		grEngine_DrawTexture(Engine, Texture, X, Y);
	else
		grEngine_DrawBitmap(Engine, Bitmap, NULL, X, Y);

	grEngine_EndFrame(Engine);
	
	if (UseJeBitmap)
	{
		grEngine_RemoveBitmap(Engine, Bitmap);
		grBitmap_Destroy(&Bitmap);
	}
	else
	{
		grEngine_DestroyTexture(Engine, Texture);
		Texture = nullptr;
	}

	Sleep(2000);
	return GR_TRUE;
}

//=====================================================================================
//	grEngine_SetDriverAndMode
//=====================================================================================
GRAPI grBoolean GRCC grEngine_SetDriverAndMode(	grEngine		*Engine, 
												HWND			hWnd,
												grDriver		*Driver, 
												grDriver_Mode	*DriverMode)
{
	grDeviceCaps		DeviceCaps{};

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);		// They can't change modes in between begin/end frame calls
	assert(Driver);
	assert(DriverMode);

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("BEGIN grEngine_SetDriverAndMode\n");
#endif

	//	Set up the Render Driver
	if (!Engine_InitDriver(Engine, hWnd, Driver, DriverMode))
		return GR_FALSE;

	// Get the default suggested render flags
	grEngine_GetDeviceCaps(Engine, &DeviceCaps);
	// Set them
	grEngine_SetDefaultRenderFlags(Engine, DeviceCaps.SuggestedDefaultRenderFlags);

	grEngine_UpdateGamma(Engine);

	//if (!Engine->FontInfo.Font) {
	//	Engine->FontInfo.Font = grEngine_CreateFont(Engine, 18, 0, GR_FONT_BOLD, GR_FALSE, "Arial");
	//}
/*
	if (!Engine->FontInfo.Font)
		return GR_FALSE;
*/
#if 1
	// Do the splash screen
	if	(Engine->SplashDisplayed == GR_FALSE)
	{
		if	(grEngine_DoSplashScreen(Engine, DriverMode) == GR_FALSE)
			return GR_FALSE;
		Engine->SplashDisplayed = GR_TRUE;
	}
#endif

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("END grEngine_SetDriverAndMode\n");
#endif

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_GetDriverAndMode
//=====================================================================================
GRAPI grBoolean GRCC grEngine_GetDriverAndMode(	const grEngine *Engine, 
												grDriver **Driver, 
												grDriver_Mode **DriverMode)
{
	assert( grEngine_IsValid(Engine) );
	assert(Driver);
	assert(DriverMode);

	*Driver = Engine->DriverInfo.CurDriver;
	*DriverMode = Engine->DriverInfo.CurMode;

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_GetDriverSystem
//=====================================================================================
GRAPI grDriver_System * GRCC grEngine_GetDriverSystem(grEngine *Engine)
{
	assert( grEngine_IsValid(Engine) );

	return (grDriver_System*)&Engine->DriverInfo;
}

//=====================================================================================
//	grEngine_ShutdownDriver
//=====================================================================================
GRAPI grBoolean GRCC grEngine_ShutdownDriver(grEngine *Engine)
{
	//	by trilobite jan. 2011
	//Engine_DriverInfo *DrvInfo;
	Engine_DriverInfo* DrvInfo{};
	//

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);

	DrvInfo = &(Engine->DriverInfo);

	assert(DrvInfo);

	if (!DrvInfo->RDriver)
		return GR_TRUE;			// Just return true, and don't do nothing

	// Destroy the font
	//if (Engine->FontInfo.Font)
	//	grEngine_DestroyFont(Engine, &Engine->FontInfo.Font);

	// First, reset the driver
	if (!grEngine_ResetDriver(Engine))
	{
		grErrorLog_AddString(-1, "grEngine_ShutdownDriver:  grEngine_ResetDriver failed.", NULL);
		return GR_FALSE;
	}

	// Shutdown the driver
	DrvInfo->RDriver->Shutdown();

	if	(DrvInfo->DriverHandle)
	{
		if (!FreeLibrary((HINSTANCE)(DrvInfo->DriverHandle)) )
			return GR_FALSE;
	}

	DrvInfo->RDriver = nullptr;
	DrvInfo->DriverHandle = (int32)NULL;

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_ResetDriver
//=====================================================================================
grBoolean grEngine_ResetDriver(grEngine *Engine)
{
	grChain_Link		*Link{};

	assert(Engine != NULL);
	assert(Engine->DriverInfo.RDriver);

	// To be safe, detach all things from the current driver
	if (!grEngine_DetachAll(Engine))
	{
		grErrorLog_AddString(-1, "grEngine_ResetDriver:  grEngine_DetachAll failed.", NULL);
		return GR_FALSE;
	}

	for (Link = grChain_GetFirstLink(Engine->ChangeDriverCBChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grEngine_ChangeDriverCB* ChangeDriverCB{};

		ChangeDriverCB = (grEngine_ChangeDriverCB*)grChain_LinkGetLinkData(Link);
		assert(ChangeDriverCB);

		if (!ChangeDriverCB->ShutdownDriverCB(Engine->DriverInfo.RDriver, ChangeDriverCB->Context))
			return GR_FALSE;
	}

	// Reset the driver
	if (!Engine->DriverInfo.RDriver->Reset())
	{
		grErrorLog_AddString(-1, "grEngine_ResetDriver:  Engine->DriverInfo.RDriver->Reset() failed.", NULL);
		return GR_FALSE;
	}

	return GR_TRUE;
}

//===================================================================================
//	grEngine_InitFonts
//===================================================================================
grBoolean grEngine_InitFonts(grEngine *Engine)
{
	Engine_FontInfo* Fi{};

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);

	Fi = &Engine->FontInfo;

	assert(Fi->FontBitmap == NULL);

	// Load the bitmap
	{
		grVFile* MemFile{};
		grVFile_MemoryContext	Context{};

		{
			extern unsigned char font_bmp[];
			extern int font_bmp_length;

			Context.Data = font_bmp;
			Context.DataLength = font_bmp_length;

			MemFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_MEMORY, NULL, &Context, GR_VFILE_OPEN_READONLY);
		}

		if	(!MemFile)
		{
			grErrorLog_AddString(-1,"InitFonts : grVFile_OpenNewSystem Memory fontbmp failed.", NULL);
			return GR_FALSE;
		}

		if ( (Fi->FontBitmap = grBitmap_CreateFromFile(MemFile)) == NULL)
		{
			grErrorLog_AddString(-1,"InitFonts : grBitmap_CreateFromFile failed.", NULL);
			goto fail;
		}

		#if 0
		#pragma message("Engine : fonts will have alpha once Decals do : CB");
		// <> CB : give fonts alpha so they look purty
		//			pointless right now cuz we don't get enum'ed a _2D_ type with alpha
		{
		grBitmap * FontAlpha;
			FontAlpha = grBitmap_Create( grBitmap_Width(Fi->FontBitmap), grBitmap_Height(Fi->FontBitmap), 1, GR_PIXELFORMAT_8BIT_GRAY );
			if ( FontAlpha )
			{
				if ( grBitmap_BlitBitmap(Fi->FontBitmap,FontAlpha) )
				{
					if ( ! grBitmap_SetAlpha( Fi->FontBitmap, FontAlpha ) )
					{
						grErrorLog_AddString(-1,"InitFonts : SetAlpha failed : non-fatal", NULL);
					}
				}
				else
				{
					grErrorLog_AddString(-1,"InitFonts : BlitBitmap failed : non-fatal", NULL);
				}
				grBitmap_Destroy(&FontAlpha);
			}
		}
		#endif

		if (!grBitmap_SetColorKey(Fi->FontBitmap, GR_TRUE, 0, GR_FALSE))
		{
			grErrorLog_AddString(-1,"InitFonts : grBitmap_SetColorKey failed.", NULL);
			goto fail;
		}

		if ( ! grEngine_AddBitmap(Engine,Fi->FontBitmap,GR_ENGINE_BITMAP_TYPE_2D) )
		{
			grErrorLog_AddString(-1,"InitFonts : grEngine_AddBitmap failed.", NULL);
			goto fail;
		}

		goto success;

		fail:

		grVFile_Close(MemFile);
		return GR_FALSE;

		success:
		
		grVFile_Close(MemFile);
	}

	//
	//	Setup font lookups
	//
	{
		int PosX{}, PosY{}, Width{}, i{};

		PosX = 0;
		PosY = 0;
		Width = 128*8;

		for (i=0; i< 128; i++)
		{
			Fi->FontLUT1[i] = (PosX<<16) | PosY;
			PosX+=8;

			if (PosX >= Width)
			{
				PosY += 14;
				PosX = 0;
			}
		}
	}

	return GR_TRUE;
}

//===================================================================================
//	grEngine_ShutdownFonts
//===================================================================================
grBoolean grEngine_ShutdownFonts(grEngine *Engine)
{
	Engine_FontInfo* Fi{};

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_None);

	Fi = &Engine->FontInfo;

	if (Fi->FontBitmap)
	{
		if (!grEngine_RemoveBitmap(Engine, Fi->FontBitmap))
		{
			grErrorLog_AddString(-1, "grEngine_ShutdownFonts:  grEngine_RemoveBitmap failed.", NULL);
			return GR_FALSE;
		}

		grBitmap_Destroy(&Fi->FontBitmap);
	}

	return GR_TRUE;
}

//=====================================================================================
//	grEngine_LoadLibrary
//=====================================================================================

HINSTANCE grEngine_LoadLibrary( const char * lpLibFileName, const char *DriverDirectory)
{
	char	Buff[_MAX_PATH];
	char* StrEnd{};
	HINSTANCE	Library{};

	//-------------------------
	strcpy(Buff, DriverDirectory);
	StrEnd = Buff + strlen(Buff) - 1;
	if ( *StrEnd != '\\' && *StrEnd != '/' && *StrEnd != ':' )
	{
		strcat(Buff,"\\");
	}
	strcat(Buff, lpLibFileName);
	Library = LoadLibrary(Buff);
	if ( Library )
		return Library;

#pragma message("Engine : LoadLibrary : need grConfig_GetDriverDir")
#ifdef LOADLIBRARY_HARDCODES
	#pragma message("Engine : using LoadLibrary HardCodes : curdir, q:\\jet, c:\\jet")

	//-------------------------

	strcpy(Buff, "q:\\jet");
	StrEnd = Buff + strlen(Buff) - 1;
	if ( *StrEnd != '\\' && *StrEnd != '/' && *StrEnd != ':' )
	{
		strcat(Buff,"\\");
	}
	strcat(Buff, lpLibFileName);
	Library = LoadLibrary(Buff);
	if ( Library )
		return Library;

	//-------------------------

	strcpy(Buff, "c:\\jet3d");
	StrEnd = Buff + strlen(Buff) - 1;
	if ( *StrEnd != '\\' && *StrEnd != '/' && *StrEnd != ':' )
	{
		strcat(Buff,"\\");
	}
	strcat(Buff, lpLibFileName);
	Library = LoadLibrary(Buff);
	if ( Library )
		return Library;
#endif

return NULL;
}
 
extern GInfo GlobalInfo;		// AHH!!!  Get rid of this!!!

//=====================================================================================
//	EngineInitDriver
//=====================================================================================

static grBoolean Engine_InitDriver(	grEngine		*Engine, 
									HWND			hWnd,
									grDriver		*Driver,
									grDriver_Mode	*DriverMode)
{
	Engine_DriverInfo* DrvInfo{};
	DRV_Hook* Hook{};
	DRV_DriverHook		DLLDriverHook{};
	DRV_Driver* RDriver{};
	grChain_Link* Link{};

	assert(grEngine_IsValid(Engine));
	assert(Engine->FrameState == FrameState_None);

	assert(Driver != NULL);
	assert(DriverMode != NULL);

	DrvInfo = &Engine->DriverInfo;

	// grEngine_ShutdownDriver calls _Reset which detaches all

	if (! grEngine_ShutdownDriver(Engine))
	{
		grErrorLog_AddString(-1, "Engine_InitDriver:  grEngine_ShutdownDriver failed.", NULL);
		goto Failure;
	}
	assert(!DrvInfo->RDriver);
	
	DrvInfo->CurDriver = Driver;
	DrvInfo->CurMode = DriverMode;

	if (!Driver->HookProc)
	{
		assert(Engine->DriverDirectory);
		DrvInfo->DriverHandle = (int32)grEngine_LoadLibrary(Driver->FileName, Engine->DriverDirectory);
	
		if (!DrvInfo->DriverHandle)
		{
			grErrorLog_Add(GR_ERR_DRIVER_NOT_FOUND, NULL);
			goto Failure;
		}
	
		Hook = (DRV_Hook*)GetProcAddress((HINSTANCE)(DrvInfo->DriverHandle), "DriverHook");		
		if (!Hook)
		{
			grErrorLog_Add(GR_ERR_INVALID_DRIVER, NULL);
			goto Failure;
		}
	}
	else
	{
		Hook = Driver->HookProc;
	}

	if (!Hook(&DrvInfo->RDriver))
	{
		DrvInfo->RDriver = NULL;
		grErrorLog_Add(GR_ERR_INVALID_DRIVER, NULL);
		goto Failure;
	}

	assert(DrvInfo->RDriver);

	// Get a handy pointer to the driver
	RDriver = DrvInfo->RDriver;

	if (RDriver->VersionMajor != DRV_VERSION_MAJOR || RDriver->VersionMinor != DRV_VERSION_MINOR)
	{
		grErrorLog_Add(GR_ERR_INVALID_DRIVER, NULL);
		goto Failure;
	}

	strcpy(DLLDriverHook.AppName, Engine->AppName);

	//
	//	Setup what driver they want
	//

	DLLDriverHook.Driver = Driver->Id;
	strcpy(DLLDriverHook.DriverName, Driver->Name);
	DLLDriverHook.Mode = DriverMode->Id;
	DLLDriverHook.Width = DriverMode->Width;
	DLLDriverHook.Height = DriverMode->Height;
	DLLDriverHook.hWnd = hWnd;
	strcpy(DLLDriverHook.ModeName, DriverMode->Name);
	
	if (!RDriver->Init(&DLLDriverHook))
	{
		grErrorLog_Add(GR_ERR_DRIVER_INIT_FAILED, NULL);
		grErrorLog_AddString(-1, RDriver->LastErrorStr , NULL);
		goto Failure;
	}

	Engine->hWnd = hWnd;		// Store the new hWnd

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("BEGIN StartupDriverCB\n");
#endif

	// Call all the changedriver CB's to notify them of the new driver
	for (Link = grChain_GetFirstLink(Engine->ChangeDriverCBChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grEngine_ChangeDriverCB* ChangeDriverCB{};

		ChangeDriverCB = (grEngine_ChangeDriverCB*)grChain_LinkGetLinkData(Link);
		assert(ChangeDriverCB);

		if (!ChangeDriverCB->StartupDriverCB(RDriver, ChangeDriverCB->Context))
			{
				grErrorLog_Add(GR_ERR_DRIVER_INIT_FAILED, NULL);
				goto Failure;
			}
	}

#if (DEBUG_OUTPUT_LEVEL >= 1)
	OutputDebugString("END StartupDriverCB\n");
#endif

	return GR_TRUE;

	Failure:
	#pragma message("need better clean up on failure (restore previous mode)")
	DrvInfo->RDriver = nullptr;
	return GR_FALSE;
}

#pragma warning (default:4100)

GRAPI grBoolean GRCC grEngine_FlushScene(grEngine *Engine)
{
	DRV_Driver* RDriver{};

	assert( grEngine_IsValid(Engine) );
	assert(Engine->FrameState == FrameState_Begin);

	RDriver = Engine->DriverInfo.RDriver;
	assert(RDriver);

	RDriver->EndBatch();
	RDriver->BeginBatch();

	return GR_TRUE;
}

//===================================================================================
//	Engine_Tick
//===================================================================================
static void Engine_Tick(grEngine *Engine)
{
	int32		i{};

	for (i=0; i< 20; i++)
	{
		if (Engine->WaveDir[i] == 1)
			Engine->WaveTable[i] += 14;
		else
			Engine->WaveTable[i] -= 14;
		if (Engine->WaveTable[i] < 50)
		{
			Engine->WaveTable[i] += 14;
			Engine->WaveDir[i] = 1;
		}
		if (Engine->WaveTable[i] > 255)
		{
			Engine->WaveTable[i] -= 14;
			Engine->WaveDir[i] = 0;
		}
	}
}

/*}{**** SECTION : Text  *********************/

//===================================================================================
//	Engine_DrawFontBuffer
//===================================================================================
static void Engine_DrawFontBuffer(grEngine *Engine)
{
	grRect			Rect{};
	int32			i{}, x{}, y{}, size{}, StrLength{};
	int32			w{};
	Engine_FontInfo* Fi{};
	char* Str{};
	int32			FontWidth{}, FontHeight{};

	assert(grEngine_IsValid(Engine));
	assert(Engine->FrameState == FrameState_Begin);
		
	Fi = &Engine->FontInfo;

	if ( Fi->NumStrings == 0) 
		return;

	assert( Fi->FontBitmap );
	assert( grBitmap_GetTHandle(Fi->FontBitmap) );

	FontWidth	= 8;
	FontHeight	= 15;
		
	for (i=0; i< Fi->NumStrings; i++)
	{
		uint32					color;

		x = Fi->ClientStrings[i].x;
		y = Fi->ClientStrings[i].y;
		color = Fi->ClientStrings[i].Color;
		//g = Fi->ClientStrings[i].g;
		//b = Fi->ClientStrings[i].b;

		size = Fi->ClientStrings[i].size;
		Str = Fi->ClientStrings[i].String;
		StrLength = strlen(Str);

		//grEngine_DrawText(Engine,Str,x,y,r,g,b);
		//if (Engine->DriverInfo.RDriver->Font_Draw) 
		//{
		//	Engine->DriverInfo.RDriver->Font_Draw(Fi->ClientStrings[i].Font, x, y, color, Str);
		//}
		if (Engine->DriverInfo.RDriver->DrawText)
		{
			Engine->DriverInfo.RDriver->DrawText(Str, x, y, color);
		}
		else
		{
		   // No driver text: draw the built-in 8x15 bitmap font as decals.
		   for (w=0; w< StrLength; w++)
		   {
			   Rect.Left = (Fi->FontLUT1[*Str & 0x7f]>>16);
			   Rect.Right = Rect.Left + FontWidth - 1;
			   Rect.Top = (Fi->FontLUT1[*Str & 0x7f]&0xffff);
			   Rect.Bottom = Rect.Top + FontHeight - 1;

			   if ( ! grEngine_DrawBitmap(Engine, Fi->FontBitmap, &Rect, x, y) )
			   {
				   // this is circular : printf failed, so use printf to write an error !?
				   //grEngine_Printf(Engine, 10, 50, "Could not draw font...\n");
				   grErrorLog_AddString(-1,"DrawFontBuffer : Could not draw font...\n", NULL);
			   }
			   //x+= 16;
			   x += FontWidth;
			   Str++;
		   }
		}
	}

	Fi->NumStrings = 0;
	Fi->NumDebugStrings = 0;
}

static void SubLarge(LARGE_INTEGER *start, LARGE_INTEGER *end, LARGE_INTEGER *delta)
{
	_asm {
		mov ebx,dword ptr [start]
		mov esi,dword ptr [end]

		mov eax,dword ptr [esi+0]
		sub eax,dword ptr [ebx+0]

		mov edx,dword ptr [esi+4]
		sbb edx,dword ptr [ebx+4]

		mov ebx,dword ptr [delta]
		mov dword ptr [ebx+0],eax
		mov dword ptr [ebx+4],edx
	}
}

//===================================================================================
// grEngine_Puts
//===================================================================================
grBoolean grEngine_Puts(grEngine *Engine, grFont *Font, int32 x, int32 y, uint32 Color, const char *String)
{
	Engine_FontInfo* Fi{};

	Fi = &Engine->FontInfo;

	if (strlen(String) >= MAX_CLIENT_STRING_LEN)
		return GR_FALSE;
					 
	if (Fi->NumStrings >= MAX_CLIENT_STRINGS)
		return GR_FALSE;

	strcpy(Fi->ClientStrings[Fi->NumStrings].String, String);

	Fi->ClientStrings[Fi->NumStrings].x = x;	
	Fi->ClientStrings[Fi->NumStrings].y = y;

	Fi->ClientStrings[Fi->NumStrings].Color = Color;
	Fi->ClientStrings[Fi->NumStrings].size = 0;
	Fi->ClientStrings[Fi->NumStrings].Font = Font;

	Fi->NumStrings++;

	return GR_TRUE;
}

//========================================================================================
//	grEngine_Printf
//========================================================================================
GRAPI grBoolean GRCC grEngine_Printf(grEngine *Engine, grFont *Font, int32 x, int32 y, uint32 Color, const char *String, ...)
{
	va_list			ArgPtr{};
    char			TempStr[1024];

	assert(grEngine_IsValid(Engine));
//	assert(Engine->FrameState == FrameState_Begin); // can do this anywhere

	va_start(ArgPtr, String);
    vsprintf(TempStr, String, ArgPtr);
	va_end(ArgPtr);

	return grEngine_Puts(Engine, Font, x, y, Color, TempStr);
}


grBoolean grEngine_DebugPrintf(grEngine *Engine, uint32 Color, const char *String, ...)
{
	grBoolean ret{};
	va_list			ArgPtr{};
   	char			TempStr[1024];

	assert(grEngine_IsValid(Engine));
//	assert(Engine->FrameState == FrameState_Begin); // can do this anywhere

	va_start(ArgPtr, String);
    vsprintf(TempStr, String, ArgPtr);
	va_end(ArgPtr);

	ret = grEngine_Puts(Engine, Engine->FontInfo.Font, 2, 2 + 15 * Engine->FontInfo.NumDebugStrings, Color, TempStr);

	Engine->FontInfo.NumDebugStrings++;

	return ret;
}


/*}{**** SECTION : THandles  *********************/

/*}{**** SECTION : grDriver stuff *********************/

#pragma message ("Engine : grDriver_* : do these go here?  (grDriver name space) :")  

//=====================================================================================
//	grDriver_SystemGetNextDriver
//=====================================================================================
GRAPI grDriver * GRCC grDriver_SystemGetNextDriver(grDriver_System *DriverSystem, grDriver *Start)
{
	Engine_DriverInfo* DriverInfo{};
	grDriver* Last{};

	assert(DriverSystem != NULL);
	
	DriverInfo = (Engine_DriverInfo*)DriverSystem;

	if (!DriverInfo->NumSubDrivers)
		return NULL;

	Last = &DriverInfo->SubDrivers[DriverInfo->NumSubDrivers-1];

	if (Start)							// If they have a driver, return the next one
		Start++;
	else
		Start = DriverInfo->SubDrivers;	// Else, return the first one...

	if (Start > Last)					// No more drivers left
		return NULL;

	// This must be true!!!
	assert(Start >= DriverInfo->SubDrivers && Start <= Last);

	return Start;	 // This is it...
}

//=====================================================================================
//	grDriver_GetNextMode
//=====================================================================================
GRAPI grDriver_Mode * GRCC grDriver_GetNextMode(grDriver *Driver, grDriver_Mode *Start)
{
	grDriver_Mode* Last{};

	Last = &Driver->Modes[Driver->NumModes-1];

	if (Start)						// If there is a start, return the next one
		Start++;
	else
		Start = Driver->Modes;		// Else, return the first

	if (Start > Last)				// No more Modes left
		return NULL;

	// This must be true...
	assert(Start >= Driver->Modes && Start <= Last);

	return Start;
}

//=====================================================================================
//	grDriver_GetName
//=====================================================================================
GRAPI grBoolean GRCC grDriver_GetName(const grDriver *Driver, const char **Name)
{
	assert(Driver);
	assert(Name);

	*Name = Driver->Name;

	return GR_TRUE;
}

//=====================================================================================
//	grDriver_ModeGetName
//=====================================================================================
GRAPI grBoolean GRCC grDriver_ModeGetName(const grDriver_Mode *Mode, const char **Name)
{
	assert(Mode);
	assert(Name);

	*Name = Mode->Name;

	return GR_TRUE;
}

//=====================================================================================
//	grDriver_ModeGetWidthHeight
//=====================================================================================
GRAPI grBoolean GRCC grDriver_ModeGetWidthHeight(const grDriver_Mode *Mode, int32 *pWidth, int32 *pHeight)
{
	assert(Mode);
	assert(pWidth);
	assert(pHeight);

	*pWidth = Mode->Width;
	*pHeight = Mode->Height;

	return GR_TRUE;
}

GRAPI grBoolean	GRCC grDriver_ModeGetAttributes(const grDriver_Mode *Mode, int32 *pWidth, int32 *pHeight, int32 *pBpp)
{
	assert(Mode);
	assert(pWidth);
	assert(pHeight);

	*pWidth = Mode->Width;
	*pHeight = Mode->Height;
	*pBpp = Mode->Bpp;

	return GR_TRUE;
}

//===================================================================================
//	EnumSubDriversCB
//===================================================================================
static grBoolean Engine_EnumSubDriversCB(int32 DriverId, char *Name, void *Context)
{
	Engine_DriverInfo* DriverInfo{};
	DriverInfo = (Engine_DriverInfo*)Context;
	DRV_Driver* RDriver{};
	grDriver* Driver{};

	if (DriverInfo->NumSubDrivers+1 >= MAX_SUB_DRIVERS)
		return GR_FALSE;		// Stop when no more driver slots available

	Driver = &DriverInfo->SubDrivers[DriverInfo->NumSubDrivers];
	
	Driver->Id = DriverId;
	strcpy(Driver->Name, Name);
	if	(DriverInfo->CurHookProc)
		Driver->HookProc = DriverInfo->CurHookProc;
	else
		strcpy(Driver->FileName, DriverInfo->CurFileName);

	RDriver = DriverInfo->RDriver;

	// Store this, so enum modes know what driver we are working on...
	DriverInfo->CurDriver = Driver;
	
	if (!RDriver->EnumModes(Driver->Id, Driver->Name, Engine_EnumModesCB, (void*)DriverInfo))
		return FALSE;

	DriverInfo->NumSubDrivers++;

	return GR_TRUE;
}

//===================================================================================
//	EnumModesCB
//===================================================================================
static grBoolean Engine_EnumModesCB(int32 ModeId, char *Name, int32 Width, int32 Height, int32 Bpp, void *Context)
{
	Engine_DriverInfo* DriverInfo{};
	grDriver* Driver{};
	grDriver_Mode* Mode{};

	DriverInfo = (Engine_DriverInfo*)Context;

	Driver = DriverInfo->CurDriver;
	
	if (Driver->NumModes+1 >= MAX_DRIVER_MODES)
		return GR_FALSE;

	Mode = &Driver->Modes[Driver->NumModes];

	Mode->Id = ModeId;
	strcpy(Mode->Name, Name);
	Mode->Width = Width;
	Mode->Height = Height;
	Mode->Bpp = Bpp;

	Driver->NumModes++;

	return GR_TRUE;
}


//===================================================================================
//	EnumSubDrivers
//===================================================================================
static grBoolean Engine_EnumSubDrivers(Engine_DriverInfo *DriverInfo, const char *DriverDirectory)
{
	DRV_Hook* DriverHook{};
#ifdef WIN32
	HINSTANCE			Handle{};
#endif
#ifdef BUILD_BE
	image_id			Handle;
#endif

	DRV_Driver* RDriver{};
	grVFile* DosDir{};
	grVFile_Finder* Finder{};

	assert(DriverDirectory);

	DosDir = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS, DriverDirectory, NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
	if	(!DosDir)
		return GR_TRUE;
	Finder = grVFile_CreateFinder(DosDir, "*.dll");
	if	(!Finder)
	{
		grVFile_Close(DosDir);
		return GR_FALSE;
	}

	DriverInfo->NumSubDrivers = 0;
	DriverInfo->CurHookProc = nullptr;

	while	(grVFile_FinderGetNextFile(Finder) == GR_TRUE)
	{
		grVFile_Properties	Properties;

		grVFile_FinderGetProperties(Finder, &Properties);

		Handle = grEngine_LoadLibrary(Properties.Name, DriverDirectory);

		if (!Handle)
			continue;

		strcpy(DriverInfo->CurFileName, Properties.Name);

		DriverHook = (DRV_Hook*)GetProcAddress((HINSTANCE)(Handle), "DriverHook");
		if (!DriverHook)
		{
			FreeLibrary(Handle);
			continue;
		}

		if (!DriverHook(&RDriver))
		{
			FreeLibrary(Handle);
			continue;
		}

		if (RDriver->VersionMajor != DRV_VERSION_MAJOR || RDriver->VersionMinor != DRV_VERSION_MINOR)
		{
			grErrorLog_AddString(-1,"Engine_EnumSubDrivers : found driver of wrong vesion (non-fatal)",Properties.Name);
			FreeLibrary(Handle);
			continue;
		}

		DriverInfo->RDriver = RDriver;	// temporary storage of the RDriver pointer
		
		if (!RDriver->EnumSubDrivers(Engine_EnumSubDriversCB, (void*)DriverInfo))
		{
			grErrorLog_AddString(-1,"Engine_EnumSubDrivers : RDriver->EnumSub failed!)",Properties.Name);
			FreeLibrary(Handle);
			continue;		// Should we return FALSE, or just continue?
							// if you change your mind and decide to return, be sure to destroy the finder.
		}

		DriverInfo->RDriver = nullptr;		// clear out the RDriver pointer!

		FreeLibrary(Handle);
	}

	grVFile_DestroyFinder (Finder);
	grVFile_Close(DosDir);
	return GR_TRUE;
}

//===================================================================================
//	grEngine_RegisterDriver
//===================================================================================
GRAPI void* GRCC grEngine_D3DDriver(void)
{
#ifdef WIN32
	// Preserve ABI compatibility with checked-in applications while routing the
	// old entry point to the sole supported external renderer.
	static HMODULE Direct3D12Module = nullptr;
	if (!Direct3D12Module)
		Direct3D12Module = LoadLibraryA("Direct3D12Driver.dll");

	if (!Direct3D12Module)
		return nullptr;

	return reinterpret_cast<void*>(GetProcAddress(Direct3D12Module, "DriverHook"));
#else
	return nullptr;
#endif
}

GRAPI grBoolean GRCC grEngine_RegisterDriver(grEngine *Engine, void* HookProc)
{
	DRV_Hook* DriverHook{};
	DRV_Driver* RDriver{};

	assert( grEngine_IsValid(Engine) );
	assert(HookProc);

	DriverHook = (DRV_Hook *)HookProc;

	if (!DriverHook(&RDriver))
	{
		grErrorLog_AddString(-1,"grEngine_RegisterDriver : Hook proc failed", NULL);
		return GR_FALSE;
	}

	if (RDriver->VersionMajor != DRV_VERSION_MAJOR || RDriver->VersionMinor != DRV_VERSION_MINOR)
	{
		grErrorLog_AddString(-1,"grEngine_RegisterDriver : driver wrong vesion", NULL);
		return GR_FALSE;
	}

	Engine->DriverInfo.RDriver = RDriver;	// temporary storage of the RDriver pointer
	Engine->DriverInfo.CurHookProc = DriverHook;
	
	if (!RDriver->EnumSubDrivers(Engine_EnumSubDriversCB, (void*)&Engine->DriverInfo))
	{
		grErrorLog_AddString(-1,"Engine_EnumSubDrivers : RDriver->EnumSub failed!)", NULL);
		Engine->DriverInfo.RDriver = NULL;		// clear out the RDriver pointer!
		return GR_FALSE;
	}

	Engine->DriverInfo.RDriver = nullptr;		// clear out the RDriver pointer!
	return GR_TRUE;
}

//===================================================================================
//	grEngine_GetDeviceCaps
//===================================================================================
GRAPI grBoolean GRCC grEngine_GetDeviceCaps(grEngine *pEngine, grDeviceCaps *DeviceCaps)
{
	memset(DeviceCaps, 0, sizeof(*DeviceCaps));

	if (!pEngine->DriverInfo.RDriver)
		return GR_FALSE;

#if 0
	DeviceCaps->SuggestedDefaultRenderFlags = GR_RENDER_FLAG_BILINEAR_FILTER;
	DeviceCaps->CanChangeRenderFlags = 0xFFFFFFFF;
#else
	pEngine->DriverInfo.RDriver->GetDeviceCaps(DeviceCaps);
#endif

	return GR_TRUE;
	pEngine;
}

//===================================================================================
//	grEngine_GetDefaultRenderFlags
//===================================================================================
GRAPI grBoolean GRCC grEngine_GetDefaultRenderFlags(grEngine *pEngine, uint32 *RenderFlags)
{
	*RenderFlags = pEngine->DefaultRenderFlags;

	return GR_TRUE;
	pEngine;
}

//===================================================================================
//	grEngine_SetDefaultRenderFlags
//===================================================================================
GRAPI grBoolean GRCC grEngine_SetDefaultRenderFlags(grEngine *pEngine, uint32 RenderFlags )
{
	pEngine->DefaultRenderFlags = RenderFlags;

	return GR_TRUE;
	pEngine;
}

/*}**** SECTION : EOF *********************/

// Registers an Object given a handle to a DLL - Incarnadine
GRAPI grBoolean GRCC grEngine_RegisterObject(HINSTANCE DllHandle)
{
	grBoolean (*RegisterDef)(float MajorVersion, float MinorVersion);

	assert( DllHandle );

#ifdef WIN32	
	RegisterDef = (grBoolean (*)(float MajorVersion, float MinorVersion))GetProcAddress( DllHandle, "Object_RegisterDef" );
#endif

#ifdef BUILD_BE
		get_image_symbol(DllHandle, "Object_RegisterDef", B_SYMBOL_TYPE_TEXT, (void **)&RegisterDef);			
#endif

	return( (*RegisterDef)( GRT_MAJOR_VERSION, GRT_MINOR_VERSION) );
}

// Registers all Objects in a particular path. - Incarnadine
GRAPI grBoolean GRCC grEngine_RegisterObjects(char * DllPath)
{
	// locals
	grVFile* DllDir{};
	grVFile_Finder* Finder{};
	grVFile_Properties	Properties{};
	HINSTANCE			DllHandle{};
	char* FullName{};

	// open dll directory
	DllDir = grVFile_OpenNewSystem(
		NULL,
		GR_VFILE_TYPE_DOS,
		DllPath,
		NULL,
		GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY );
	if ( DllDir != nullptr )
	{

		// create our directory finder
		#ifndef NDEBUG
		Finder = grVFile_CreateFinder( DllDir, "*.ddl" );
		#else
		Finder = grVFile_CreateFinder( DllDir, "*.dll" );
		#endif
		
		if( Finder != nullptr )
		{

			// start processing files
			while ( grVFile_FinderGetNextFile( Finder ) == GR_TRUE )
			{

				// get properties of current file
				if( grVFile_FinderGetProperties( Finder, &Properties ) == GR_FALSE )
				{
					grErrorLog_AddString( GR_ERR_FILEIO_READ, "InitObjects: Unable to get dll file properties.", NULL );
					goto ERROR_INITOBJECTS;
				}


				// save dll full name
				FullName = (char*)grRam_Allocate( strlen( DllPath ) + strlen( Properties.Name ) + 2 );
				if ( FullName == nullptr )
				{
					grErrorLog_AddString( GR_ERR_MEMORY_RESOURCE, "InitObjects: Unable to allocate dll full name.", NULL );
					goto ERROR_INITOBJECTS;
				}
				strcpy( FullName, DllPath );
				strcat( FullName, "\\" );
				strcat( FullName, Properties.Name );

				// load up the dll
#ifdef WIN32
				DllHandle = LoadLibrary( FullName );
#endif
#ifdef BUILD_BE
				DllHandle = load_add_on( FullName );
#endif

				if ( DllHandle == nullptr )
				{
					grErrorLog_AddString( GR_ERR_FILEIO_READ, "InitObjects: Unable to load object dll.", Properties.Name );
					grRam_Free( FullName );
					continue;
				}

				// setup the object functions
				if ( grEngine_RegisterObject( DllHandle ) == GR_FALSE )
				{
					grErrorLog_AddString( GR_ERR_INTERNAL_RESOURCE, "InitObjects: failed to find get functions for object dll.", Properties.Name  );
					grRam_Free( FullName );
					continue;
				}
				grRam_Free( FullName );

			}

			// destroy finder
			grVFile_DestroyFinder( Finder );
		}

		// close file system
		grVFile_Close( DllDir );
	}
	return( GR_TRUE );

ERROR_INITOBJECTS:
	grVFile_DestroyFinder( Finder );
	grVFile_Close( DllDir );
	return( GR_FALSE );
}

// BSP stats accessor
GRAPI grBoolean GRCC grEngine_GetBSPDebugInfo(grEngine *Engine, int32 *pNumMakeFaces, int32 *pNumMergedFaces, int32 *pNumSubdividedFaces, int32 *pNumDrawFaces)
{
   if (pNumMakeFaces)
	   *pNumMakeFaces = NumMakeFaces;
   if (pNumMergedFaces)
	   *pNumMergedFaces = NumMergedFaces;
   if (pNumSubdividedFaces)
	   *pNumSubdividedFaces = NumSubdividedFaces;
   if (pNumDrawFaces)
	   *pNumDrawFaces = NumMakeFaces+NumSubdividedFaces-NumMergedFaces;

	return GR_TRUE;
}

// Engine render mode
GRAPI grBoolean GRCC grEngine_SetRenderMode(grEngine *Engine, int32 RenderMode)
{
   if (Engine) {
      Engine->RenderMode = RenderMode;
      return GR_TRUE;
   }
   return GR_FALSE;
}

// BEGIN - Bug Fix - grEngine_FillRect() not implemented - paradoxnj
GRAPI void GRCC grEngine_FillRect(grEngine *Engine, const grRect *Rect, const grRGBA *Color)
{
	grTLVertex		DrvVertex[4];
	DRV_Driver* RDriver{};

	RDriver = Engine->DriverInfo.RDriver;

	assert(RDriver != NULL);

#define NEARZ								0.5f

	DrvVertex[0].x = (float)Rect->Left;
	DrvVertex[0].y = (float)Rect->Top;
	DrvVertex[0].z = NEARZ;
	DrvVertex[0].u = 0.0f;
	DrvVertex[0].v = 0.0f;
	DrvVertex[0].r = Color->r;
	DrvVertex[0].g = Color->g;
	DrvVertex[0].b = Color->b;
	DrvVertex[0].a = Color->a;
	
	DrvVertex[1].x = (float)Rect->Right;
	DrvVertex[1].y = (float)Rect->Top;
	DrvVertex[1].z = NEARZ;
	DrvVertex[1].u = 0.0f;
	DrvVertex[1].v = 0.0f;
	DrvVertex[1].r = Color->r;
	DrvVertex[1].g = Color->g;
	DrvVertex[1].b = Color->b;
	DrvVertex[1].a = Color->a;

	DrvVertex[2].x = (float)Rect->Right;
	DrvVertex[2].y = (float)Rect->Bottom;
	DrvVertex[2].z = NEARZ;
	DrvVertex[2].u = 0.0f;
	DrvVertex[2].v = 0.0f;
	DrvVertex[2].r = Color->r;
	DrvVertex[2].g = Color->g;
	DrvVertex[2].b = Color->b;
	DrvVertex[2].a = Color->a;

	DrvVertex[3].x = (float)Rect->Left;
	DrvVertex[3].y = (float)Rect->Bottom;
	DrvVertex[3].z = NEARZ;
	DrvVertex[3].u = 0.0f;
	DrvVertex[3].v = 0.0f;
	DrvVertex[3].r = Color->r;
	DrvVertex[3].g = Color->g;
	DrvVertex[3].b = Color->b;
	DrvVertex[3].a = Color->a;

	if (Color->a != 255.0f)
		RDriver->RenderGouraudPoly(DrvVertex, 4, GR_RENDER_FLAG_FLUSHBATCH);
	else
		RDriver->RenderGouraudPoly(DrvVertex, 4, GR_RENDER_FLAG_ALPHA | GR_RENDER_FLAG_FLUSHBATCH);
}
// END - Bug Fix - grEngine_FillRect() not implemented - paradoxnj

// BEGIN - Hardware T&L - paradoxnj 6/8/2005
GRAPI grBoolean GRCC grEngine_SetMatrix(grEngine *Engine, uint32 Type, grXForm3d *Matrix)
{
	if (Engine->DriverInfo.RDriver->SetMatrix)
		return Engine->DriverInfo.RDriver->SetMatrix(Type, Matrix);

	return GR_FALSE;
}

GRAPI grBoolean GRCC grEngine_GetMatrix(grEngine *Engine, uint32 Type, grXForm3d *Matrix)
{
	if (Engine->DriverInfo.RDriver->GetMatrix)
		return Engine->DriverInfo.RDriver->GetMatrix(Type, Matrix);

	return GR_FALSE;
}

GRAPI grBoolean GRCC grEngine_SetCamera(grEngine *Engine, grCamera *Camera)
{
	if (Engine->DriverInfo.RDriver->SetCamera)
		return Engine->DriverInfo.RDriver->SetCamera(Camera);

	return GR_FALSE;
}
// END - Hardware T&L - paradoxnj 6/8/2005

GRAPI grFont * GRCC grEngine_CreateFont(grEngine *Engine, int32 Height, int32 Width, uint32 Weight, grBoolean Italic, const char *facename)
{
	assert(Engine != NULL);
	
	if (Engine->DriverInfo.RDriver->Font_Create)
		return Engine->DriverInfo.RDriver->Font_Create(Height, Width, Weight, Italic, facename);

	return NULL;
}

GRAPI grBoolean GRCC grEngine_DestroyFont(grEngine *Engine, grFont **Font)
{
	assert(Engine != NULL);
	assert(Font != NULL);

	if (Engine->DriverInfo.RDriver->Font_Destroy)
		return Engine->DriverInfo.RDriver->Font_Destroy(Font);

	return GR_FALSE;
}
