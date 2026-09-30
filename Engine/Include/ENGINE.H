/*!
	@file Engine.h 
	
	@author Charles Bloom/John Pollard
	@brief Maintains the driver interface, as well as the bitmaps attached to the driver.

	@par Licence
	The contents of this file are subject to the Jet3D Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.jet3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Jet3D, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/

#ifndef GR_ENGINE_H
#define GR_ENGINE_H

#include "BaseType.h"
#include "grTypes.h"
#include "Bitmap.h"
#include "Camera.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------------------------------

/*! @typedef grEngine
	@brief A reference to the Engine
*/
typedef struct grEngine			grEngine;

/*! @typedef grDriver
	@brief A reference to a Renderer device
*/
typedef struct grDriver			grDriver;

/*! @typedef grDriverMode
	@brief A reference to a Renderer mode
*/
typedef struct grDriver_Mode	grDriver_Mode;

/*! @typedef grDriverSystem
	@brief A reference to a Renderer enumerator
*/
typedef struct grDriver_System	grDriver_System;


/*! @typedef grTexture
	@brief A reference to a material texture
*/
typedef struct grTexture		grTexture;

/*!	@typedef grShader
	@brief A reference to a shader
*/
typedef struct grShader			grShader;

/*! @typedef grVertexBuffer
	@brief A reference to a vertex buffer
*/
typedef struct grVertexBuffer	grVertexBuffer;

/*! @typedef grIndexBuffer
	@brief A reference to an index buffer
*/
typedef struct grIndexBuffer	grIndexBuffer;

/*!	@typedef grFont
	@brief A reference to a true type font
*/
typedef struct grFont			grFont;

#define GR_FONT_NORMAL			0x00000001
#define GR_FONT_BOLD			0x00000002

typedef enum
{
	GR_ENGINE_BITMAP_TYPE_2D,
	GR_ENGINE_BITMAP_TYPE_3D,
} grEngine_BitmapType;


typedef enum
{
	FrameState_None = 0,
	FrameState_Begin,
} grEngine_FrameState;


//-------------------------------------------------
// Engine Functions
//-------------------------------------------------

// ------- Create/Destroy
#ifdef WINVER
		// include windows to get this API 
GRAPI grEngine* GRCC grEngine_Create(HWND hWnd, const char *AppName, const char *DriverDirectory);
#endif
	
GRAPI grBoolean	GRCC grEngine_CreateRef(grEngine *Engine);

GRAPI void			GRCC grEngine_Free(grEngine *Engine);			// Don't use this :
GRAPI void			GRCC grEngine_Destroy(grEngine **pEngine);

GRAPI grBoolean	GRCC grEngine_IsValid(const grEngine *E);

// ------- Misc functions
GRAPI void			GRCC grEngine_EnableFrameRateCounter(grEngine *Engine, grBoolean Enabled);

GRAPI grBoolean	GRCC grEngine_Activate(grEngine *Engine, grBoolean bActive);
GRAPI grBoolean	GRCC grEngine_UpdateWindow(grEngine *Engine);

GRAPI grBoolean	GRCC grEngine_GetFrameState(const grEngine *Engine, grEngine_FrameState *FrameState);

//-------- The main Frame functions:

GRAPI grBoolean	GRCC grEngine_BeginFrame(grEngine *Engine, grCamera *Camera, grBoolean ClearScreen);
GRAPI grBoolean	GRCC grEngine_EndFrame(grEngine *Engine);

GRAPI grBoolean	GRCC grEngine_FlushScene(grEngine *Engine);

// call updategamma when drivers change
GRAPI grBoolean	GRCC grEngine_SetGamma(grEngine *Engine, float Gamma);
GRAPI grBoolean	GRCC grEngine_GetGamma(grEngine *Engine, float *pGamma);
GRAPI void			GRCC grEngine_UpdateGamma(grEngine *Engine);

GRAPI grBoolean	GRCC grEngine_AddBitmap(		grEngine *Engine, grBitmap *Bitmap, grEngine_BitmapType Type);
GRAPI grBoolean	GRCC grEngine_RemoveBitmap(	grEngine *Engine, grBitmap *Bitmap);

GRAPI grBoolean	GRCC grEngine_ScreenShot(grEngine *Engine, const char *FileName);

grBoolean				grEngine_BitmapListInit(grEngine *Engine);
grBoolean				grEngine_BitmapListShutdown(grEngine *Engine);
grBoolean				grEngine_AttachAll(grEngine *Engine);
grBoolean				grEngine_DetachAll(grEngine *Engine);

/*	// CB disabled in 2.0
//-------- the splash screen
grBoolean				grEngine_DoSplashScreen(grEngine *Engine, grDriver_Mode *DriverMode);
*/

//-------- engine fonts
grBoolean				grEngine_InitFonts(grEngine *Engine);
grBoolean				grEngine_ShutdownFonts(grEngine *Engine);
// BEGIN - Hardware True Type Fonts - paradoxnj 8/3/2005
GRAPI grFont *		GRCC grEngine_CreateFont(grEngine *Engine, int32 Height, int32 Width, uint32 Weight, grBoolean Italic, const char *facename);
GRAPI grBoolean	GRCC grEngine_DestroyFont(grEngine *Engine, grFont **Font);

GRAPI grBoolean	GRCC grEngine_Printf(grEngine *Engine, grFont *Font, int32 x, int32 y, uint32 Color, const char *String, ...);
grBoolean			grEngine_DebugPrintf(grEngine *Engine, uint32 Color, const char *String, ...);
// END - Hardware True Type Fonts - paradoxnj 8/3/2005

// ------- Built-in drivers (parameters to grEngine_RegisterDriver)
// BEGIN - Software Driver no longer exists - paradoxnj 8/3/2005
//GRAPI	void*    GRCC grEngine_SoftwareDriver(void);
// END - Software Driver no longer exists - paradoxnj 8/3/2005

GRAPI	void*    GRCC grEngine_D3DDriver(void);

//-------- Engine Driver system stuff (for getting a driver to set the mode)
GRAPI grDriver_System * GRCC grEngine_GetDriverSystem(grEngine *Engine);

GRAPI grDriver		* GRCC grDriver_SystemGetNextDriver(grDriver_System *DriverSystem, grDriver *Start);
GRAPI grDriver_Mode * GRCC grDriver_GetNextMode(grDriver *Driver, grDriver_Mode *Start);

GRAPI grBoolean	GRCC grDriver_GetName(const grDriver *Driver, const char **Name);
GRAPI grBoolean	GRCC grDriver_ModeGetName(const grDriver_Mode *Mode, const char **Name);
GRAPI grBoolean	GRCC grDriver_ModeGetWidthHeight(const grDriver_Mode *Mode, int32 *pWidth, int32 *pHeight);
GRAPI grBoolean	GRCC grDriver_ModeGetAttributes(const grDriver_Mode *Mode, int32 *pWidth, int32 *pHeight, int32 *pBpp);
							// CB : GetWidthHeight is deprecated ; use GetAttributes instead!

GRAPI grBoolean 	GRCC grEngine_RegisterDriver(grEngine *Engine, void* HookProc);

GRAPI grBoolean	GRCC grEngine_GetDeviceCaps(grEngine *pEngine, grDeviceCaps *DeviceCaps);
GRAPI grBoolean	GRCC grEngine_GetDefaultRenderFlags(grEngine *pEngine, uint32 *RenderFlags);
GRAPI grBoolean	GRCC grEngine_SetDefaultRenderFlags(grEngine *pEngine, uint32 RenderFlags);

#ifdef WINVER
GRAPI grBoolean	GRCC grEngine_SetDriverAndMode(	grEngine		*Engine, 
													HWND			hWnd,
													grDriver		*Driver, 
													grDriver_Mode	*DriverMode);
#endif
													
GRAPI grBoolean	GRCC grEngine_GetDriverAndMode(	const grEngine *Engine, 
													grDriver **Driver, 
													grDriver_Mode **DriverMode);

GRAPI grBoolean	GRCC grEngine_ShutdownDriver(grEngine *Engine);

grBoolean				grEngine_ResetDriver(grEngine *Engine);	// Resets the current driver


//--------- Render driver stuff (this is the main hard-core driver.  the engine just wraps this up with grDriver)
//	NOTE - You MUST include dcommon.h BEFORE engine.h to use the hard-core driver functions!
#ifdef DCOMMON_H
	typedef struct grEngine_ChangeDriverCB		grEngine_ChangeDriverCB;

	typedef grBoolean GRCC grEngine_ShutdownDriverCB(DRV_Driver *Driver, void *Conext);
	typedef grBoolean GRCC grEngine_StartupDriverCB(DRV_Driver *Driver, void *Conext);

	// Driver will make more sense in the future (instead of RDriver)
	DRV_Driver* GRCF grEngine_GetDriver(const grEngine * Engine);
	
	GRAPI grEngine_ChangeDriverCB * GRCC grEngine_CreateChangeDriverCB(	grEngine					*Engine, 
																		grEngine_ShutdownDriverCB	*ShutdownDriverCB, 
																		grEngine_StartupDriverCB	*StartupDriverCB,
																		void						*Context);
	GRAPI void GRCC grEngine_DestroyChangeDriverCB(grEngine *Engine, grEngine_ChangeDriverCB **ChangeDriverCB);
#endif

#ifdef WINVER
HINSTANCE				grEngine_LoadLibrary( const char * lpLibFileName, const char *DriverDirectory);
#endif

//-------- drawing with the engine (Decals & Misc Polys)

GRAPI grBoolean GRCC grEngine_DrawBitmap(const grEngine *Engine,
	const grBitmap *Bitmap,	// should have been a TYPE_2D
	const grRect * Source, uint32 x, uint32 y);

GRAPI grBoolean GRCC grEngine_DrawBitmap3D(const grEngine *Engine,
	const grBitmap *Bitmap, // should have been a TYPE_3D
	const grRect * pRect, uint32 x, uint32 y);

// BEGIN - grTexture implementation - paradoxnj 5/12/2005
GRAPI grTexture *GRCC grEngine_CreateTextureFromFile(const grEngine *Engine, grVFile *File);
GRAPI void GRCC grEngine_DestroyTexture(const grEngine *Engine, grTexture *Texture);

GRAPI grBoolean GRCC grEngine_DrawTexture(const grEngine *Engine, const grTexture *Texture, int32 x, int32 y);
// END - grTexture implementation - paradoxnj 5/12/2005

// foward definition of grMaterialSpec
typedef struct grMaterialSpec grMaterialSpec;

GRAPI void GRCC grEngine_RenderPoly(const grEngine *Engine, const GR_TLVertex *Points, 
						int NumPoints, const grMaterialSpec *Texture, uint32 Flags);
						// Texture should have been a TYPE_3D

GRAPI void GRCC grEngine_RenderPolyArray(const grEngine *Engine, const GR_TLVertex ** pPoints, int * pNumPoints, int NumPolys, 
								const grMaterialSpec *Texture, uint32 Flags);

GRAPI void	GRCC grEngine_FillRect(grEngine *Engine, const grRect *Rect, const grRGBA *Color);

//--------------------------------------------------------------------------

#ifdef WINVER
// Registers an Object given a handle to a DLL - Incarnadine
GRAPI grBoolean GRCC grEngine_RegisterObject(HINSTANCE DllHandle);

// Registers all Objects in a particular path. - Incarnadine
GRAPI grBoolean GRCC grEngine_RegisterObjects(char * DllPath);
#endif

// Krouer
// debug BSP info
GRAPI grBoolean GRCC grEngine_GetBSPDebugInfo(grEngine *Engine, int32 *NumMakeFaces, int32 *NumMergedFaces, int32 *NumSubdividedFaces, int32 *NumDrawFaces);
// toggle wireframe rendering for gameshell/BSP object
GRAPI grBoolean GRCC grEngine_SetRenderMode(grEngine *Engine, int32 RenderMode);

// Cool function from Darkrift
GRAPI grFloat GRCC grEngine_GetFPS(grEngine *Engine);
GRAPI grBoolean GRCC grEngine_DrawText(grEngine *Engine, char *text, int x,int y,uint32 color);
GRAPI grBoolean GRCC grEngine_SetFog(grEngine *Engine, float r, float g, float b, float start, float endi, grBoolean enable);

//--------- Look Profiles (roadmap Phase 3)
// Classic draws like the original engine. Enhanced adds HDR lighting with per-pixel dynamic
// lights, shadows and post-processing (tonemapping, exposure, bloom, fog, SSAO). Stylized is
// Enhanced plus retro options (low render resolution, palette, dither, CRT, vertex snapping).
// The defaults come from the driver's ini ([Look] in Direct3D12Driver.ini).
#define GR_LOOK_CLASSIC			0
#define GR_LOOK_ENHANCED		1
#define GR_LOOK_STYLIZED		2

#define GR_TONEMAP_NONE			0
#define GR_TONEMAP_ACES			1
#define GR_TONEMAP_AGX			2

// Same layout as DRV_LookSettings (Dcommon.h).
typedef struct grEngine_LookSettings
{
	int32		Profile;			// GR_LOOK_*
	int32		Tonemapper;			// GR_TONEMAP_*
	int32		AutoExposure;		// adapt to the scene's brightness
	float		ExposureEV;			// manual exposure, or the compensation added to auto exposure
	int32		Bloom;
	float		BloomIntensity;		// 0..1
	int32		SSAO;
	float		SSAORadius;			// world units
	float		SSAOIntensity;		// 0..1
	int32		Shadows;			// real-time shadows for dynamic lights with GR_LIGHT_FLAG_CAST_SHADOWS
	float		HeightFogDensity;	// 0 = off
	float		HeightFogBase;		// world height where the fog is densest
	float		HeightFogFalloff;	// per world unit above HeightFogBase
	float		HeightFogColor[3];	// 0..1
	// Stylized
	float		RenderScale;		// 0.125..1, upscaled with nearest filtering
	int32		PaletteBits;		// bits per channel, 0 = full color
	int32		Dither;				// ordered dither before quantizing
	int32		CRT;				// scanlines and a slight vignette
	int32		VertexSnap;			// screen grid width in pixels that vertices snap to, 0 = off
	int32		SnapLighting;		// dynamic lighting per lightmap texel
} grEngine_LookSettings;

// The profile alone; the other settings keep their values. Returns GR_FALSE if the driver
// has no Look Profiles (it then draws Classic).
GRAPI grBoolean GRCC grEngine_SetLookProfile(grEngine *Engine, int32 Profile);
GRAPI int32 GRCC grEngine_GetLookProfile(const grEngine *Engine);
GRAPI grBoolean GRCC grEngine_SetLookSettings(grEngine *Engine, const grEngine_LookSettings *Look);
GRAPI grBoolean GRCC grEngine_GetLookSettings(const grEngine *Engine, grEngine_LookSettings *Look);
// GR_TRUE when dynamic lights that cast shadows are lit and shadowed by the GPU.
GRAPI grBoolean GRCC grEngine_GpuShadowsEnabled(const grEngine *Engine);

// BEGIN - Hardware T&L - paradoxnj 6/8/2005
GRAPI grBoolean GRCC grEngine_SetMatrix(grEngine *Engine, uint32 Type, grXForm3d *Matrix);
GRAPI grBoolean GRCC grEngine_GetMatrix(grEngine *Engine, uint32 Type, grXForm3d *Matrix);
GRAPI grBoolean GRCC grEngine_SetCamera(grEngine *Engine, grCamera *Camera);
// END - Hardware T&L - paradoxnj 6/8/2005

#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases

typedef grEngine_BitmapType grEngine_BitmapType;
typedef grEngine_FrameState grEngine_FrameState;


#endif // GR_ENGINE_H
