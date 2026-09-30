/****************************************************************************************/
/*  DCOMMON.H                                                                           */
/*                                                                                      */
/*  Author: John Pollard                                                                */
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
#ifndef DCOMMON_H
#define DCOMMON_H

//#include <Windows.h>	// {} CB commented out windows
// If you include Windows it MUST be before dcommon!

// FIXME:  What should we do with these?
#include "Basetype.h"
#include "XForm3d.h"
#include "Vec3d.h"
#include "PixelFormat.h"
#include "grTypes.h"
#include "VFile.h"
#include "Camera.h"
#include "grStaticMesh.h"
#include "grChain.h"
#include "grLight.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push)
#pragma pack(8)

#ifndef WINVER
#ifdef STRICT
typedef struct HWND__ * HWND;
typedef struct HBITMAP__ * HBITMAP;
#else // STRICT
typedef void * HWND;
typedef void * HBITMAP;
#endif // STRICT

#ifndef VOID
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif

#ifndef BASETYPES
#define BASETYPES
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef char *PSZ;
#endif  /* !BASETYPES */

typedef unsigned long       DWORD;
typedef int                 grBoolean;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;

typedef struct tagRECT
{
    LONG    left;
    LONG    top;
    LONG    right;
    LONG    bottom;
} RECT;

#endif // WINVER

#define	DRIVERCC _fastcall

#ifndef __cplusplus
	#define DllImport	__declspec( dllimport )
	#define DllExport	__declspec( dllexport )
#else
	#define DllImport	extern "C" __declspec( dllimport )
	#define DllExport	extern "C" __declspec( dllexport )
#endif

#define DRV_VERSION_MAJOR		200			// Jet 2.0
#define DRV_VERSION_MINOR		13			// version 3 has specular rgb in the verts ; 4 has bigger debug info ; 5 adds GPUTimings ; 6 adds WorldGeometry ; 7 adds DRV_WorldView clip planes and draw counts ; 8 adds WorldMesh_Render ; 9 adds PBR world faces and lights ; 10 adds THandle_CreateFromDDS ; 11 adds WorldMesh_RenderPBR and parallax ; 12 adds static world lights, 32 lights ; 13 adds frame lighting (World_SetFrame/EndPass), DRV_WorldView.ModelToWorld and Look Profiles
#define DRV_VMAJS				"200"
#define DRV_VMINS				"4" 
#define DRV_VMAJS_PLUS_DRV_VMINS	"200.4"


#ifndef US_TYPEDEFS
#define US_TYPEDEFS

	typedef uint8	U8;
	typedef uint16	U16;
	typedef uint32	U32;
	typedef char	C8;
	typedef int8	S8;
	typedef int16	S16;
	typedef int32	S32;
#endif

//===
// BEGIN - grTexture implementation - paradoxnj 5/12/2005
//typedef struct grTexture	grTexture;
typedef struct grTexture			grTexture;
// END - grTexture implementation - paradoxnj 5/12/2005

// BEGIN - Shaders - paradoxnj 6/8/2005
typedef struct grShader				grShader;
// END - Shaders - paradoxnj 6/8/2005

// BEGIN - Hardware True Type Fonts - paradoxnj 8/3/2005
typedef struct grFont				grFont;
// END - Hardware True Type Fonts - paradoxnj 8/3/2005

// BEGIN - Rendering Data sections - krouer 6/21/2005
typedef struct grMaterialSpec		grMaterialSpec;
typedef struct
{
	grMaterialSpec	*Material;		// The MaterialSpec of the Render

	uint32			StartIndex;		// The index from where to start
	uint32			StartVertex;	// The vertex from where to start
	uint32			IndexCount;		// The number of index to use
	uint32			VertexCount;	// The number of vertex to use
} grRenderSectionData;
// END - Rendering Data sections - krouer 6/21/2005

// BEGIN - Material layer type enumerations - krouer 8/16/2005
#define LAYER_TYPE_BASE 0 
#define LAYER_TYPE_LIGHTMAP 1 
#define LAYER_TYPE_BUMPMAP 2 
// PBR layers (roadmap Phase 2, .jmat version 2). A missing layer falls back to a constant.
#define LAYER_TYPE_NORMAL 3			// tangent-space normal map (RG used, Z rebuilt)
#define LAYER_TYPE_ORM 4			// R = occlusion, G = roughness, B = metalness (linear)
#define LAYER_TYPE_EMISSIVE 5		// sRGB
#define LAYER_TYPE_HEIGHT 6			// parallax height (optional, not used by the renderer yet)
// END - Material layer type enumerations - krouer 8/16/2005


// DriverFormat flag bits (Exclusive)
#define RDRIVER_PF_2D_SHIFT					(0)			// Supports being used as a 2d decal surface
#define RDRIVER_PF_3D_SHIFT					(1)			// Supports being used as a 3d poly surface
#define RDRIVER_PF_LIGHTMAP_SHIFT			(2)			// Surface is a lightmap surface
#define RDRIVER_PF_PALETTE_SHIFT			(3)			// Surface is a palette
#define RDRIVER_PF_ALPHA_SURFACE_SHIFT		(4)			// Surface is an alpha map
// DriverFormat flag bits (Non-Exclusive)
#define RDRIVER_PF_OPTIONAL_SHIFT			(16)
#define RDRIVER_PF_HAS_ALPHA_SURFACE_SHIFT	(RDRIVER_PF_OPTIONAL_SHIFT + 0)		// Surface can take an alpha map
#define RDRIVER_PF_ALPHA_SHIFT				(RDRIVER_PF_OPTIONAL_SHIFT + 1)		// PixelFormat has alpha
#define RDRIVER_PF_CAN_DO_COLORKEY_SHIFT	(RDRIVER_PF_OPTIONAL_SHIFT + 2)		// Surface supports colorkeying
#define RDRIVER_PF_COMBINE_LIGHTMAP_SHIFT	(RDRIVER_PF_OPTIONAL_SHIFT + 3)		// Supports being rendered with a lightmap (3d will be set as well)

// DriverFormat flags (Exclusive)
#define RDRIVER_PF_2D						(1<<RDRIVER_PF_2D_SHIFT)				
#define RDRIVER_PF_3D						(1<<RDRIVER_PF_3D_SHIFT)				
#define RDRIVER_PF_LIGHTMAP					(1<<RDRIVER_PF_LIGHTMAP_SHIFT)			
#define RDRIVER_PF_PALETTE					(1<<RDRIVER_PF_PALETTE_SHIFT)			
#define RDRIVER_PF_ALPHA_SURFACE			(1<<RDRIVER_PF_ALPHA_SURFACE_SHIFT)			
// DriverFormat flags (Exclusive)
#define RDRIVER_PF_HAS_ALPHA_SURFACE		(1<<RDRIVER_PF_HAS_ALPHA_SURFACE_SHIFT)		
#define RDRIVER_PF_ALPHA					(1<<RDRIVER_PF_ALPHA_SHIFT)			
#define RDRIVER_PF_CAN_DO_COLORKEY			(1<<RDRIVER_PF_CAN_DO_COLORKEY_SHIFT)
#define RDRIVER_PF_COMBINE_LIGHTMAP			(1<<RDRIVER_PF_COMBINE_LIGHTMAP_SHIFT)	

#define RDRIVER_PF_MAJOR_MASK				((1<<RDRIVER_PF_OPTIONAL_SHIFT)-1)

#ifndef RDRIVER_PIXELFORMAT_DEFINED
#define RDRIVER_PIXELFORMAT_DEFINED
typedef struct grRDriver_PixelFormat
{
	grPixelFormat	PixelFormat;
	uint32			Flags;				
} grRDriver_PixelFormat;

#define RDRIVER_THANDLE_HAS_COLORKEY	(1<<0)		// The thandle is using color keying

typedef enum
{
	Rop_None,
	Rop_Multiply,			// P' = P1*P2	
	Rop_MultiplyX2,			// P' = P1*P2*2+Clamp (To allow for overbright lightmaps, looks more vibrant)
	Rop_MultiplyX4,			// P' = P1*P2*4+Clamp (To allow for overbright lightmaps, looks more vibrant)
	Rop_Add,				// P' = P1+P2+Clamp
} grRDriver_Rop;

// BEGIN - grTexture implementation - paradoxnj 5/12/2005
typedef struct grTexture_Info
{
	int32					Width;
	int32					Height;
	int32					Stride;
	uint32					ColorKey;
	uint32					Flags;
	uint8					Log;
	grRDriver_PixelFormat	PixelFormat;
    void*                   Direct;
} grTexture_Info;
#endif

typedef struct
{
	// BEGIN - grTexture implementation - paradoxnj 5/12/2005
	//grTexture	*THandle;		// THandle for this layer
	grTexture			*THandle;
	// END - grTexture implementation - paradoxnj 5/12/2005

	grRDriver_Rop		Rop;			// Blend mode to next THandle in the layer cascade
	
	// Shift and Scale values for this layer (based off the base UV set for the poly)
	grFloat				ShiftU;
	grFloat				ShiftV;
	grFloat				ScaleU;
	grFloat				ScaleV;
} grRDriver_Layer;

typedef struct 
{
	void				*RGBLight[2];
	grBoolean			Dynamic;
} grRDriver_LMapCBInfo;

typedef grRDriver_PixelFormat grRDriver_PixelFormat;
typedef grRDriver_Rop grRDriver_Rop;
typedef grTexture_Info grTexture_Info;
typedef grRDriver_Layer grRDriver_Layer;
typedef grRDriver_LMapCBInfo grRDriver_LMapCBInfo;

//===

typedef struct
{
	S32	LMapCount[16][4];				// LMap size / MipLevel
} DRV_Debug;

typedef struct
{
	int32		CacheFull;
	int32		CacheRemoved;
	int32		CacheFlushes;
	int32		TexMisses;
	int32		TexMissesFresh;
	int32		LMapMisses;
	int32		TexMissBytes;
	int32		LMapMissBytes;
	int32		TexBlitBytes;
	int32		LMapBlitBytes;

	int32		CacheTypes;
	int32		CacheMisses[32];
	int32		CacheFreshMisses[32];
	int32		CacheUses[32];
	int32		CacheSlots[32];

	int32		CardMem,SlotMem,UsedMem;
	float		MipBias;
	int32		Balances,BalancesFailed;
} DRV_CacheInfo;

// GPU time spent on the last completed frame, measured with timestamp queries.
// Valid is 0 until the driver has read back its first frame.
typedef struct
{
	float		SceneMs;		// BeginScene to the present pass
	float		PresentMs;		// present pass (HDR target to back buffer)
	float		TotalMs;
	int32		Valid;

	// Version 7: draw counts of the last frame recorded (independent of Valid).
	int32		DrawCalls;		// scene draws, world path included
	int32		WorldDraws;		// indexed draws of the GPU world path
	int32		WorldFaces;		// faces those draws covered
} DRV_GPUTimings;

//
//	World geometry (version 6): BSP draw faces uploaded once and drawn by the GPU in
//	world space, instead of being clipped, transformed and projected on the CPU.
//
typedef struct
{
	float		Pos[3];			// model space
	float		Normal[3];		// face plane normal (facing side)
	float		Tangent[4];		// texture U direction, w = bitangent sign
	float		u, v;			// face texture coordinates (texvec projection, FixShift applied)
	uint32		Face;			// index into the geometry's face table
} DRV_WorldVertex;

typedef struct
{
	uint32		FirstIndex;		// triangle list range in the index array
	uint32		NumIndices;
} DRV_WorldFace;

// Model-to-camera transform and the camera's screen projection (see grCamera_Project).
typedef struct
{
	grXForm3d	ModelToCamera;
	float		Scale;
	float		XCenter;
	float		YCenter;
	float		ZScale;
	float		ZFar;			// camera-space far clip distance, 0 = none
	float		HalfWidth;		// the frustum spans XCenter +/- HalfWidth, YCenter +/- HalfHeight
	float		HalfHeight;
	// Nested views (portals, mirrors) clip to their camera-space frustum instead of the
	// camera rect: a point is inside when Plane.xyz . p + Plane.w >= 0 for every plane.
	// 0 = use HalfWidth/HalfHeight. At most DRV_WORLD_MAX_CLIP_PLANES, including ZFar.
	int32		NumClipPlanes;
	float		ClipPlanes[8][4];
	// Version 13: model to world space, for world-space lighting and shadows (identity for
	// world-space geometry such as actors).
	grXForm3d	ModelToWorld;
} DRV_WorldView;

#define DRV_WORLD_MAX_CLIP_PLANES	8

// Results of WORLD_GEOMETRY_RENDER_FACE.
#define DRV_WORLD_FACE_DRAWN		1	// queued on the GPU path
#define DRV_WORLD_FACE_FALLBACK		0	// not drawn; use the transformed-poly path for this face
#define DRV_WORLD_FACE_STALE		2	// not drawn; the geometry handle is gone, rebuild it

// Returns 0 when the driver cannot (or is configured not to) use world geometry.
typedef uint32 DRIVERCC WORLD_GEOMETRY_CREATE(const DRV_WorldVertex *Verts, int32 NumVerts,
											  const uint32 *Indices, int32 NumIndices,
											  const DRV_WorldFace *Faces, int32 NumFaces);
typedef grBoolean DRIVERCC WORLD_GEOMETRY_DESTROY(uint32 Geometry);
// Queues one face, in order with the other render calls. Layers, LMapCBContext and
// Flags mean the same as for RENDER_W_POLY; Alpha is 0..255.
typedef int32 DRIVERCC WORLD_GEOMETRY_RENDER_FACE(uint32 Geometry, uint32 Face, const DRV_WorldView *View,
												  grRDriver_Layer *Layers, int32 NumLayers,
												  void *LMapCBContext, uint32 Flags, float Alpha);

//
//	World meshes (version 8): per-frame triangles in model space with vertex colors (actors,
//	skinned and lit on the CPU), projected by the GPU with a DRV_WorldView like world faces.
//
typedef struct
{
	float		Pos[3];			// model space
	float		u, v;			// texture coordinates, as grTLVertex
	float		r, g, b, a;		// 0..255, as grTLVertex
} DRV_MeshVertex;

// Queues a triangle list (NumVerts / 3 triangles) textured with Layer, in order with the other
// render calls. Flags mean the same as for RENDER_MISC_TEXTURE_POLY. Returns
// DRV_WORLD_FACE_DRAWN, or DRV_WORLD_FACE_FALLBACK when nothing was drawn. Verts == NULL
// only asks whether the path is available.
typedef int32 DRIVERCC WORLD_MESH_RENDER(const DRV_MeshVertex *Verts, int32 NumVerts, const DRV_WorldView *View,
										 grRDriver_Layer *Layer, uint32 Flags);

//
//	PBR world faces (version 9): metallic/roughness GGX shading for materials with PBR layers
//	or parameters. The lightmap is the diffuse irradiance; the lights set with
//	WORLD_GEOMETRY_SET_LIGHTS add specular and normal-map detail.
//
#define DRV_MATERIAL_ALPHA_OPAQUE	0
#define DRV_MATERIAL_ALPHA_CUTOUT	1	// clip below AlphaCutoff
#define DRV_MATERIAL_ALPHA_BLEND	2	// blended; the engine also sets GR_RENDER_FLAG_ALPHA

#define DRV_MATERIAL_TWO_SIDED		0x0001
#define DRV_MATERIAL_RETRO			0x0002	// point sampling and texel-snapped shading
#define DRV_MATERIAL_LEGACY			0x0004	// version 13: no PBR data (Enhanced look): a fullbright face stays unlit

typedef struct
{
	grTexture	*NormalMap;		// NULL = flat normal
	grTexture	*ORMMap;		// NULL = white: the scalars below are the values
	grTexture	*EmissiveMap;	// NULL = white
	float		BaseColor[4];	// linear tint, multiplies layer 0
	float		Roughness;		// times ORMMap.g
	float		Metal;			// times ORMMap.b
	float		Emissive[3];	// linear color * intensity, times EmissiveMap
	float		AlphaCutoff;
	uint32		AlphaMode;		// DRV_MATERIAL_ALPHA_*
	uint32		Flags;			// DRV_MATERIAL_*
	// Version 11: parallax occlusion mapping
	grTexture	*HeightMap;		// NULL = none; R = height, 1 = the surface
	float		HeightScale;	// depth of height 0, in texture widths
} DRV_WorldMaterial;

// A point light in the model space of the geometry drawn next (see grBSPNode_Light).
typedef struct
{
	float		Pos[3];
	float		Radius;
	float		Color[3];		// 0..1 * brightness
	uint32		Flags;			// DRV_WORLD_LIGHT_*
} DRV_WorldLight;

// Version 12: a static light, already baked into the lightmap (or vertex lighting) with
// shadows. It adds only the normal map's detail and specular, faded where the baked
// lighting shows the light is blocked. Dynamic lights come first in the list.
// Version 13 drivers light PBR surfaces from WORLD_SET_FRAME's lights instead.
#define DRV_WORLD_LIGHT_STATIC		(1<<0)

#define DRV_WORLD_MAX_LIGHTS		32

//
//	Frame lighting (version 13, roadmap Phase 3): every light of the frame in world space,
//	binned into clusters by the driver (clustered forward). Point and spot lights fall off
//	linearly to 0 at Radius like the engine's lightmaps: Color * (Radius - distance) * N.L,
//	in 0..255 lightmap units. A directional light gives Color * N.L.
//
#define DRV_LIGHT_POINT				0
#define DRV_LIGHT_SPOT				1
#define DRV_LIGHT_DIRECTIONAL		2
#define DRV_LIGHT_TYPE_MASK			0x0003
#define DRV_LIGHT_STATIC			0x0004	// baked into the lightmaps (and actor lighting) with shadows
#define DRV_LIGHT_CAST_SHADOWS		0x0008	// a dynamic light that casts real-time shadows

typedef struct
{
	float		Pos[3];			// world space (not used by directional lights)
	float		Radius;
	float		Color[3];		// 0..1 * brightness
	uint32		Flags;			// DRV_LIGHT_*
	float		Dir[3];			// spot and directional: the direction the light travels (unit length)
	float		CosOuter;		// spot: cosine of the cone's half angle, where it reaches 0
	float		CosInner;		// spot: cosine of the half angle where it starts to fade
	float		Padding[3];
} DRV_Light;

#define DRV_MAX_FRAME_LIGHTS		1024

// Sets the frame's lights and its main camera (ModelToCamera = world to camera). The engine
// calls it before it draws the world; the driver clusters the lights for this camera, and
// views of other cameras (portals, mirrors) test every light.
typedef void DRIVERCC WORLD_SET_FRAME(const DRV_WorldView *MainView, const DRV_Light *Lights, int32 NumLights);
// Marks the end of the 3D scene: post-processing (exposure, tonemapping, bloom, fog...)
// applies to what was drawn before it; what follows (the 2D overlay) is drawn on top.
typedef void DRIVERCC WORLD_END_PASS(void);

//
//	Look Profiles (version 13, roadmap Phase 3). Classic draws exactly like earlier
//	versions: gamma-space lighting, no post-processing. Enhanced adds HDR lighting with
//	per-pixel dynamic lights, shadows and post-processing; Stylized is Enhanced with the
//	retro options below. Settings that do not apply to the profile are ignored.
//
#define DRV_LOOK_CLASSIC			0
#define DRV_LOOK_ENHANCED			1
#define DRV_LOOK_STYLIZED			2

#define DRV_TONEMAP_NONE			0
#define DRV_TONEMAP_ACES			1
#define DRV_TONEMAP_AGX				2

typedef struct
{
	int32		Profile;			// DRV_LOOK_*
	int32		Tonemapper;			// DRV_TONEMAP_*
	int32		AutoExposure;		// adapt to the scene's brightness
	float		ExposureEV;			// manual exposure, or the compensation added to auto exposure
	int32		Bloom;
	float		BloomIntensity;		// 0..1
	int32		SSAO;
	float		SSAORadius;			// world units
	float		SSAOIntensity;		// 0..1
	int32		Shadows;			// real-time shadows for dynamic lights that cast them
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
} DRV_LookSettings;

// Sets the look; GET_LOOK returns the current one (the driver's ini defaults at first).
typedef grBoolean DRIVERCC SET_LOOK(const DRV_LookSettings *Look);
typedef grBoolean DRIVERCC GET_LOOK(DRV_LookSettings *Look);

// Version 10: a texture from a DDS file image (BC1-7 or uncompressed, with its mip chain), as
// G3DTexImport writes them. sRGB formats load as their UNORM equivalents because the shaders
// do the gamma conversion. Returns NULL for a format or layout the driver cannot use. Such
// textures cannot be locked.
typedef grTexture *DRIVERCC THANDLE_CREATE_FROM_DDS(const void *Data, uint32 Size);

// Version 11: PBR world meshes (actors). The vertex color is the diffuse irradiance (the
// engine's CPU lighting, dynamic lights included), as the lightmap is for world faces; the
// lights set with WORLD_GEOMETRY_SET_LIGHTS (same space as Pos) add specular and the normal
// map's detail. Returns as WORLD_MESH_RENDER.
typedef struct
{
	float		Pos[3];			// model space
	float		u, v;			// texture coordinates, as grTLVertex
	float		r, g, b, a;		// 0..255, as grTLVertex
	float		Normal[3];		// unit length
	float		Tangent[4];		// texture U direction, w = bitangent sign
} DRV_MeshVertexPBR;

typedef int32 DRIVERCC WORLD_MESH_RENDER_PBR(const DRV_MeshVertexPBR *Verts, int32 NumVerts, const DRV_WorldView *View,
											 grRDriver_Layer *Layer, uint32 Flags, const DRV_WorldMaterial *Material);

// Sets the lights for the PBR faces queued after it (at most DRV_WORLD_MAX_LIGHTS are used).
typedef void DRIVERCC WORLD_GEOMETRY_SET_LIGHTS(const DRV_WorldLight *Lights, int32 NumLights);
// As WORLD_GEOMETRY_RENDER_FACE, shaded with Material.
typedef int32 DRIVERCC WORLD_GEOMETRY_RENDER_FACE_PBR(uint32 Geometry, uint32 Face, const DRV_WorldView *View,
													  grRDriver_Layer *Layers, int32 NumLayers,
													  void *LMapCBContext, uint32 Flags, float Alpha,
													  const DRV_WorldMaterial *Material);

typedef struct
{
	HWND		hWnd;
	
	U8			*Buffer;

	S32			Width;
	S32			Height;

	S32			PixelPitch;
	S32			BytesPerPixel;

	S32			R_shift;
	S32			G_shift;
	S32			B_shift;

	U32			R_mask;
	U32			G_mask;
	U32			B_mask;

	S32			R_width;
	S32			G_width;
	S32			B_width;
} DRV_Window;

#pragma pack(push)
#pragma pack(8)
typedef struct 
{
    U8 r, g, b;								// RGB components for RGB lightmaps
} DRV_RGB;
#pragma pack(pop)

//===========================================================================================
// FIXME:  Get palette stuff, and bitmap out of dcommon
#define	DRV_PALETTE_ENTRIES	256
typedef	DRV_RGB	DRV_Palette[DRV_PALETTE_ENTRIES];

// Bitmap hook into the drivers (engine uses these explicitly as is)
typedef struct
{
	char	Name[32];						// Duh, name of bitmap...
	U32		Flags;							// Flags
	S32		Width;							// Width of bitmap
	S32		Height;							// Height of bitmap
	U8		MipLevels;
	U8		*BitPtr[4];						// Pointer to location of bits (up to 4 miplevels)
	DRV_RGB *Palette;

	// Driver sets these in register functions
	//S32		Id;								// Bitmap handle for hardware...
	// BEGIN - grTexture implementation - paradoxnj 5/12/2005
	//grTexture	*THandle;
	grTexture			*THandle;
	// END - grTexture implementation - paradoxnj 5/12/2005

} DRV_Bitmap;
//===========================================================================================

#define LMAP_TYPE_LIGHT			0
#define LMAP_TYPE_FOG			1

typedef struct
{
	char				AppName[512];
	S32					Driver;
	char				DriverName[512];
	S32					Mode;
	char				ModeName[512];
	S32					Width;
	S32					Height;
	HWND				hWnd;
} DRV_DriverHook;

typedef struct
{
	// Texture info
	grVec3d		VecU;
	grVec3d		VecV;
	int32		TexMinsX;
	int32		TexMinsY;
	int32		TexWidth;
	int32		TexHeight;
	float		TexShiftX;
	float		TexShiftY;

	// Camera info
	grXForm3d	CXForm;
	grVec3d		CPov;

	float		XCenter;
	float		YCenter;

	float		XScale;
	float		YScale;
	float		XScaleInv;			// 1 / XScale
	float		YScaleInv;			// 1 / YScale;


	grVec3d		PlaneNormal;		// Face normal
	float		PlaneDist;
	grVec3d		RPlaneNormal;		// Rotated Face normal
	grVec3d		Pov;
} GInfo;

// FIXME:  Move this into the GetDeviceCaps stuff
// What the driver can support as far as texture mapping is concerned
#define DRV_SUPPORT_ALPHA					(1<<0)		// Driver can do alpha blending
#define DRV_SUPPORT_COLORKEY				(1<<1)		// Driver can do pixel masking
#define DRV_SUPPORT_GAMMA					(1<<2)		// Gamma function works with the driver

// A hint to the engine as far as what to turn on and off...
#define DRV_PREFERENCE_NO_MIRRORS			(1<<0)		// Engine should NOT render mirrors
#define DRV_PREFERENCE_SORT_WORLD_FB		(1<<1)		// Sort world Front to Back
#define DRV_PREFERENCE_SORT_WORLD_BF		(1<<2)		// Sort world Back to Front
#define DRV_PREFERENCE_DRAW_WALPHA_IN_BSP	(1<<3)		// Draw world alphas in BSP sort

typedef struct DRV_EngineSettings
{
	U32			CanSupportFlags;
	U32			PreferenceFlags;
	U32			Reserved1;
	U32			Reserved2;
} DRV_EngineSettings;

// BEGIN - Hardware T&L - paradoxnj 4/5/2005
enum grXFormType
{
	GR_XFORM_TYPE_VIEW = 0,
	GR_XFORM_TYPE_WORLD,
	GR_XFORM_TYPE_PROJECTION
};

// END - Hardware T&L - paradoxnj 4/5/2005

// Enumeration defines
typedef grBoolean DRV_ENUM_MODES_CB( S32 Mode, char *ModeName, S32 Width, S32 Height, S32 BPP, void *Context);
typedef grBoolean DRV_ENUM_DRV_CB( S32 Driver, char *DriverName, void *Context);

typedef grBoolean DRIVERCC DRV_ENUM_DRIVER(DRV_ENUM_DRV_CB *Cb, void *Context); 
typedef grBoolean DRIVERCC DRV_ENUM_MODES(S32 Driver, char *DriverName, DRV_ENUM_MODES_CB *Cb, void *Context); 

typedef grBoolean DRV_ENUM_PFORMAT_CB(grRDriver_PixelFormat *Format, void *Context);
typedef grBoolean DRIVERCC DRV_ENUM_PFORMAT(DRV_ENUM_PFORMAT_CB *Cb, void *Context); 

typedef grBoolean DRIVERCC DRV_GET_DEVICE_CAPS(grDeviceCaps *DeviceCaps);

// Create/Destroy/Etc Driver functions
typedef grBoolean DRIVERCC DRV_INIT(DRV_DriverHook *Hook);
typedef grBoolean DRIVERCC DRV_SHUTDOWN(void);
typedef grBoolean DRIVERCC DRV_RESET(void);
typedef grBoolean DRIVERCC DRV_UPDATE_WINDOW(void);
typedef grBoolean DRIVERCC DRV_SET_ACTIVE(grBoolean Active);

// BEGIN - grTexture implementation - paradoxnj 5/12/2005
// Texture surface functions
typedef grTexture *DRIVERCC CREATE_TEXTURE(int32 Width, int32 Height, int32 NumMipLevels, const grRDriver_PixelFormat *PixelFormat);
typedef grTexture *DRIVERCC CREATE_TEXTURE_FROM_FILE(grVFile *File);

typedef grBoolean DRIVERCC DESTROY_TEXTURE(grTexture *THandle);

typedef grBoolean DRIVERCC LOCK_THANDLE(grTexture *THandle, int32 MipLevel, void **Data);
typedef grBoolean DRIVERCC UNLOCK_THANDLE(grTexture *THandle, int32 MipLevel);

typedef grBoolean DRIVERCC SET_PALETTE(grTexture *THandle, grTexture *PalHandle);
typedef grTexture *DRIVERCC GET_PALETTE(grTexture *THandle);

typedef grBoolean DRIVERCC SET_ALPHA(grTexture *THandle, grTexture *PalHandle);
typedef grTexture *DRIVERCC GET_ALPHA(grTexture *THandle);

typedef grBoolean DRIVERCC THANDLE_GET_INFO(grTexture *THandle, int32 MipLevel, grTexture_Info *Info);
// END - grTexture implementation - paradoxnj 5/12/2005

// Scene management functions
typedef grBoolean DRIVERCC BEGIN_SCENE(grBoolean Clear, grBoolean ClearZ, RECT *WorldRect, grBoolean Wireframe);
typedef grBoolean DRIVERCC END_SCENE(void);
typedef grBoolean DRIVERCC BEGIN_BATCH(void);
typedef grBoolean DRIVERCC END_BATCH(void);

// Render functions
typedef grBoolean DRIVERCC RENDER_G_POLY(grTLVertex *Pnts, int32 NumPoints, uint32 Flags);
typedef grBoolean DRIVERCC RENDER_W_POLY(grTLVertex *Pnts, int32 NumPoints, grRDriver_Layer *Layers, int32 NumLayers, void *LMapCBContext, uint32 Flags);
typedef grBoolean DRIVERCC RENDER_MT_POLY(grTLVertex *Pnts, int32 NumPoints, grRDriver_Layer *Layers, int32 NumLayers, uint32 Flags);

typedef grBoolean DRIVERCC DRAW_DECAL(grTexture *THandle, RECT *SRect, int32 x, int32 y);

typedef grBoolean DRIVERCC SCREEN_SHOT(const char *Name);
typedef grBoolean DRIVERCC DRAW_TEXT(char *text, int x, int y, uint32 color);

typedef grBoolean DRIVERCC SET_FOG(float r, float g, float b, float start, float endi, grBoolean enable);

typedef grBoolean DRIVERCC SET_GAMMA(float Gamma);
typedef grBoolean DRIVERCC GET_GAMMA(float *Gamma);

// BEGIN - Hardware T&L - paradoxnj 4/5/2005
typedef grBoolean DRIVERCC SET_MATRIX(uint32 type, grXForm3d *XForm);
typedef grBoolean DRIVERCC GET_MATRIX(uint32 type, grXForm3d *XForm);
typedef grBoolean DRIVERCC SET_CAMERA(grCamera *Camera);
// END - Hardware T&L - paradoxnj 4/5/2005

// Static Meshes - paradoxnj 8/1/2005
typedef uint32 DRIVERCC ADD_STATIC_MESH(grHWVertex *Points, int32 NumPoints, grRDriver_Layer *Layers, int32 NumLayers, uint32 Flags);
typedef grBoolean DRIVERCC REMOVE_STATIC_MESH(uint32 id);
typedef grBoolean DRIVERCC RENDER_STATIC_MESH(uint32 id, int32 StartVertex, int32 NumPolys, grXForm3d *XForm);
// Static Meshes - paradoxnj 8/1/2005

// BEGIN - Hardware True Type Fonts - paradoxnj 8/3/2005
typedef grFont * DRIVERCC CREATE_FONT(int32 Height, int32 Width, uint32 Weight, grBoolean Italic, const char *facename);
typedef grBoolean DRIVERCC DRAW_FONT(grFont *Font, int32 x, int32 y, uint32 Color, const char *text);
typedef grBoolean DRIVERCC DESTROY_FONT(grFont **Font);
// END - Hardware True Type Fonts - paradoxnj 8/3/2005

// BEGIN - Render state access - paradoxnj 12/25/2005
typedef grBoolean DRIVERCC SET_RENDER_STATE(uint32 state, uint32 value);
// END - Render state access - paradoxnj 12/25/2005

typedef void GRCC SETUP_LIGHTMAP_CB(grRDriver_LMapCBInfo *LMapCBInfo, void *Context);

typedef struct
{
	char				*Name;
	S32					VersionMajor;
	S32					VersionMinor;

	// Error handling hooks set by driver
	S32					LastError;							// Last error driver made
	char				*LastErrorStr;						// NULL terminated error string
	
	// Enum Modes/Drivers
	DRV_ENUM_DRIVER		*EnumSubDrivers;
	DRV_ENUM_MODES		*EnumModes;
	
	DRV_ENUM_PFORMAT	*EnumPixelFormats;

	// Device Caps
	DRV_GET_DEVICE_CAPS	*GetDeviceCaps;

	// Init/DeInit functions
	DRV_INIT			*Init;
	DRV_SHUTDOWN		*Shutdown;
	DRV_RESET			*Reset;
	DRV_UPDATE_WINDOW	*UpdateWindow;
	DRV_SET_ACTIVE		*SetActive;
	
	// Create/Destroy texture functions
	CREATE_TEXTURE		*THandle_Create;
	
	// BEGIN - grTexture implementation - paradoxnj 5/12/2005
	CREATE_TEXTURE_FROM_FILE	*THandle_CreateFromFile;
	// END - grTexture implementation - paradoxnj 5/12/2005

	DESTROY_TEXTURE		*THandle_Destroy;

	// Texture manipulation functions
	LOCK_THANDLE		*THandle_Lock;
	UNLOCK_THANDLE		*THandle_UnLock;

	// Palette access functions
	SET_PALETTE			*THandle_SetPalette;
	GET_PALETTE			*THandle_GetPalette;

	// Palette access functions
	SET_ALPHA			*THandle_SetAlpha;
	GET_ALPHA  			*THandle_GetAlpha;

	THANDLE_GET_INFO	*THandle_GetInfo;

	// Scene management functions
	BEGIN_SCENE			*BeginScene;
	END_SCENE			*EndScene;

	BEGIN_BATCH			*BeginBatch;
	END_BATCH			*EndBatch;
	
	// Render functions
	RENDER_G_POLY		*RenderGouraudPoly;
	RENDER_W_POLY		*RenderWorldPoly;
	RENDER_MT_POLY		*RenderMiscTexturePoly;

	//Decal functions
	DRAW_DECAL			*DrawDecal;

	S32					NumWorldPixels;
	S32					NumWorldSpans;
	S32					NumRenderedPolys;
	DRV_CacheInfo		*CacheInfo;

	SCREEN_SHOT			*ScreenShot;

	SET_GAMMA			*SetGamma;
	GET_GAMMA			*GetGamma;
	
	// BEGIN - Hardware T&L - paradoxnj 4/5/2005
	SET_MATRIX			*SetMatrix;
	GET_MATRIX			*GetMatrix;
	SET_CAMERA			*SetCamera;
	// END - Hardware T&L - paradoxnj 4/5/2005

	// Driver preferences
	DRV_EngineSettings	*EngineSettings;

	// The engine supplies these for the drivers misc use
	SETUP_LIGHTMAP_CB	*SetupLightmap;

	// KROUER: move this 2 new function to the end of the structure, keep backward compatibility
	// Introduced by Gerald
	DRAW_TEXT		   *DrawText;
	SET_FOG				*SetFog;

	// BEGIN - Static Meshes - paradoxnj 8/2/2005
	ADD_STATIC_MESH		*StaticMesh_Add;
	REMOVE_STATIC_MESH	*StaticMesh_Remove;
	RENDER_STATIC_MESH	*StaticMesh_Render;
	// END - Static Meshes - paradoxnj 8/2/2005

	// BEGIN - Hardware True Type Fonts - paradoxnj 8/13/2005
	CREATE_FONT			*Font_Create;
	DRAW_FONT			*Font_Draw;
	DESTROY_FONT		*Font_Destroy;
	// END - Hardware Truw Type Fonts - paradoxnj 8/13/2005

	// BEGIN - Render state access - paradoxnj 12/25/2005
	SET_RENDER_STATE	*SetRenderState;
	// END - Render state access - paradoxnj 12/25/2005

	// Version 5: GPU timings for the debug overlay (NULL if the driver has none).
	DRV_GPUTimings		*GPUTimings;

	// Version 6: world geometry (NULL if the driver has none).
	WORLD_GEOMETRY_CREATE		*WorldGeometry_Create;
	WORLD_GEOMETRY_DESTROY		*WorldGeometry_Destroy;
	WORLD_GEOMETRY_RENDER_FACE	*WorldGeometry_RenderFace;

	// Version 8: world meshes (NULL if the driver has none).
	WORLD_MESH_RENDER			*WorldMesh_Render;

	// Version 9: PBR world faces (NULL if the driver has none).
	WORLD_GEOMETRY_SET_LIGHTS		*WorldGeometry_SetLights;
	WORLD_GEOMETRY_RENDER_FACE_PBR	*WorldGeometry_RenderFacePBR;

	// Version 10: DDS textures (NULL if the driver has none).
	THANDLE_CREATE_FROM_DDS			*THandle_CreateFromDDS;

	// Version 11: PBR world meshes (NULL if the driver has none).
	WORLD_MESH_RENDER_PBR			*WorldMesh_RenderPBR;

	// Version 13: frame lighting and Look Profiles (NULL if the driver has none).
	WORLD_SET_FRAME					*World_SetFrame;
	WORLD_END_PASS					*World_EndPass;
	SET_LOOK						*SetLook;
	GET_LOOK						*GetLook;
} DRV_Driver;

enum grRenderState
{
	GR_RENDERSTATE_ENABLE_ZBUFFER = 0,
	GR_RENDERSTATE_ENABLE_ZWRITES,
	GR_RENDERSTATE_ENABLE_ALPHABLENDING,
	GR_RENDERSTATE_ALPHAREF,
	GR_RENDERSTATE_ALPHAFUNC,
	GR_RENDERSTATE_ENABLE_ALPHATESTING,
	GR_RENDERSTATE_DEPTHFUNC,
	GR_RENDERSTATE_FILLMODE,
	GR_RENDERSTATE_SHADEMODE,
	GR_RENDERSTATE_CULLMODE,
	GR_RENDERSTATE_ENABLE_FOG,
	GR_RENDERSTATE_FOGCOLOR,
	GR_RENDERSTATE_FOGSTART,
	GR_RENDERSTATE_FOGEND,
	GR_RENDERSTATE_HWLIGHTINGENABLE,
	GR_RENDERSTATE_AMBIENTLIGHT,
	GR_RENDERSTATE_ENABLE_STENCIL,
	GR_RENDERSTATE_STENCILREF,
	GR_RENDERSTATE_STENCILMASK,
	GR_RENDERSTATE_STENCILWRITEMASK,
	GR_RENDERSTATE_STENCILFUNC,
	GR_RENDERSTATE_STENCILFAIL,
	GR_RENDERSTATE_STENCILZFAIL,
	GR_RENDERSTATE_STENCILPASS
};

enum grFill
{
	GR_FILL_POINT = 0,
	GR_FILL_WIREFRAME,
	GR_FILL_SOLID
};

enum grCullMode
{
	GR_CULL_NONE = 0,
	GR_CULL_CW,
	GR_CULL_CCW
};

enum grShadeMode
{
	GR_SHADE_FLAT = 0,
	GR_SHADE_GOURAUD,
	GR_SHADE_PHONG
};

enum grRenderCmpFunc
{
	GR_CMP_NEVER = 0,
	GR_CMP_LESS,
	GR_CMP_EQUAL,
	GR_CMP_LEQUAL,
	GR_CMP_GREATER,
	GR_CMP_GEQUAL,
	GR_CMP_NEQUAL,
	GR_CMP_ALWAYS
};

enum grStencilOp
{
	GR_STENCILOP_KEEP = 0,
	GR_STENCILOP_ZERO,
	GR_STENCILOP_REPLACE,
	GR_STENCILOP_INCRWRAP,
	GR_STENCILOP_DECRWRAP,
	GR_STENCILOP_INVERT,
	GR_STENCILOP_INCR,
	GR_STENCILOP_DECR
};

enum grBlendOp
{
	GR_BLEND_ZERO = 0,
	GR_BLEND_ONE,
	GR_BLEND_SOURCE,
	GR_BLEND_INVERSESOURCE,
	GR_BLEND_SOURCEALPHA,
	GR_BLEND_INVERSESOURCEALPHA,
	GR_BLEND_DEST,
	GR_BLEND_INVERSEDEST,
	GR_BLEND_DESTALPHA,
	GR_BLEND_INVERSEDESTALPHA,
	GR_BLEND_SOURCEALPHASAT
};

typedef struct grDriverStats
{
	int32								NumPolysRendered;
	int32								NumVertexBuffersRendered;
	int32								NumDecalsRendered;
} grDriverStats;

#ifdef __cplusplus
class CDRV_Driver
{
protected:
	virtual ~CDRV_Driver()						{}

public:
	virtual grBoolean					GetName(char *Name) = 0;
	virtual grBoolean					GetDriverStats(grDriverStats *Stats) = 0;

	virtual grBoolean					EnumSubDrivers(DRV_ENUM_DRV_CB *Cb, void *Context) = 0;
	virtual grBoolean					EnumModes(int32 Driver, char *DriverName, DRV_ENUM_MODES_CB *Cb, void *Context) = 0;

	virtual grBoolean					GetDeviceCaps(grDeviceCaps *Caps) = 0;

	virtual grBoolean					Initialize(DRV_DriverHook *Hook) = 0;
	virtual grBoolean					Shutdown() = 0;
	
	virtual grTexture					*THandle_Create(int32 Width, int32 Height, int32 NumMipLevels, const grRDriver_PixelFormat *Format) = 0;
	virtual grTexture					*THandle_CreateFromFile(grVFile *File) = 0;

	virtual grBoolean					SetRenderState(uint32 state, uint32 value) = 0;

	virtual grBoolean					BeginScene(uint32 ClearFlags) = 0;
	virtual grBoolean					EndScene() = 0;
	
	virtual grBoolean					SetOrtho(int32 Left, int32 Right, int32 Width, int32 Height) = 0;
	virtual grBoolean					SetPerspective(float fov, float aspect, float znear, float zfar) = 0;

	virtual grBoolean					EnableLight(int32 id, grLight *Light) = 0;

//	virtual grBoolean					RenderVertexBuffer(grVertexBuffer *VB, int16 StartVertex, grVec3d *Position, grVec3d *Rotation, uint32 Flags) = 0;
	virtual grBoolean					DrawBitmap(grTexture *THandle, RECT *SRect, int32 x, int32 y) = 0;

	virtual grBoolean					Screenshot(const char *filename) = 0;

	virtual grBoolean					SetGamma(float Gamma) = 0;
	virtual grBoolean					GetGamma(float *Gamma) = 0;

	virtual grBoolean					DrawText(char *Text, int x, int y, uint32 Color) = 0;
};
#endif

typedef grBoolean DRV_Hook(DRV_Driver **Hook);

//
//	Error defines set by the driver.  These will be in the LastError member of AFX_DRIVER
//	structure.  LastErrorStr will contain a NULL terminated detail error string set by the driver
//
#define DRV_ERROR_NONE					0	// No error has occured
#define DRV_ERROR_INVALID_PARMS			1	// invalid parameters passed
#define DRV_ERROR_NULL_WINDOW			2	// Null window supplied
#define DRV_ERROR_INIT_ERROR			3	// Error intitializing
#define DRV_ERROR_INVALID_REGISTER_MODE	4	// Invalid register mode
#define DRV_ERROR_NO_MEMORY				5	// Not enough ram
#define DRV_ERROR_MAX_TEXTURES			6	// Max texture capacity has been exceeded...
#define DRV_ERROR_GENERIC				7	// Generic error	 
#define DRV_ERROR_UNDEFINED				8	// An undefined error has occured
#define DRV_ERROR_INVALID_WINDOW_MODE	9	// Requested window/full not supported

typedef enum
{
	RENDER_NONE,
	RENDER_WORLD,
	RENDER_MESHES,
	RENDER_MODELS
} DRV_RENDER_MODE;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
