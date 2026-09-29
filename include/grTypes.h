/****************************************************************************************/
/*  JETYPES.H                                                                           */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D: Reborn Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D: Reborn, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#ifndef GR_TYPES_H
#define GR_TYPES_H

#include "BaseType.h"
#include "Vec3d.h"

#ifdef __cplusplus
extern "C" {
#endif

//
//	Render flags for poly operations
//
#define GR_RENDER_FLAG_ALPHA				0x00000001	// Alpha in the vertices are valid
#define GR_RENDER_FLAG_SPECULAR				0x00000002	// Specular in the vertices are valid
#define GR_RENDER_FLAG_COLORKEY				0x00000004	// Texture format has color key on the poly being rendered
#define GR_RENDER_FLAG_CLAMP_UV				0x00000008	// Clamp U and V in BOTH directions
#define GR_RENDER_FLAG_COUNTER_CLOCKWISE	0x00000010	// Winding of poly will be counter-clockwise
#define GR_RENDER_FLAG_NO_ZTEST				0x00000020	// No ZTest should be performed
#define GR_RENDER_FLAG_NO_ZWRITE			0x00000040	// No ZWrites should be performed
#define GR_RENDER_FLAG_STEST				0x00000080	// Span test should be performed (if set, polys should be front to back)
#define GR_RENDER_FLAG_SWRITE				0x00000100	// Spans should be written to the sbuffer
#define GR_RENDER_FLAG_FLUSHBATCH			0x00000200	// Flushes the current batch of polys (if any), and the current poly
#define GR_RENDER_FLAG_BILINEAR_FILTER		0x00000400	// Enable bilinear filtering
#define GR_RENDER_FLAG_WIREFRAME			0x00000800	// Toggle wireframe rendering
#define GR_RENDER_FLAG_HWTRANSFORM			0x00001000	// Renderer support Hardware Transform
#define GR_RENDER_FLAG_VERTEXBUFFER         0x10000000  // Renderer use VertexBuffer

// Device Caps for the current driver
typedef struct
{
	uint32			SuggestedDefaultRenderFlags;	// What the driver suggest the DefaultRenderFlags should be
	uint32			CanChangeRenderFlags;			// RenderFlags that you can change
	// Other Device related stuff should go here (
} grDeviceCaps;

typedef struct
{
	grFloat	u, v;
	grFloat	r, g, b, a;
} grUVRGBA;

typedef struct
{
	float r, g, b, a;
} grRGBA;

typedef struct
{
	float r, g, b;
} grRGB;

typedef struct
{
	int32	Left;
	int32	Right;
	int32	Top;
	int32	Bottom;
} grRect;

typedef struct
{
	grFloat MinX,MaxX;
	grFloat MinY,MaxY;
} grFloatRect;

// Lit vertex
typedef struct
{
/*
	grVec3d	Position;
	grRGBA	Color,SpecularColor;
	float	u,v,pad1,pad2;
*/
	// FIXME:  Convert 3d X,Y,Z to grVec3d
	float X, Y, Z, pad;								// 3d vertex
	// FIXME:  Convert r,g,b,a to GR_RGBA
	float r, g, b, a;								// color
	float u, v,pad1,pad2;							// Uv's
	float sr, sg, sb, pad3;							// specular color
} grLVertex;	// 64 bytes

// Transformed Lit vertex
typedef struct
{
	float x, y, z, pad;								// screen points
	float r, g, b, a;								// color
	float u, v, pad1,pad2;							// Uv's
	float sr, sg, sb, pad3;							// specular color
} grTLVertex;	// 64 bytes

typedef struct
{
	grVec3d World;
	grVec3d Normal;
	grRGBA Color;
	float u, v;
	float pad1,pad2;
} grVertex;	// 64 bytes !

// HW Transformation Vertex Struct - paradoxnj
typedef struct grHWVertex
{
	grVec3d					Pos;
	grVec3d					Normal;
	uint32					Diffuse;
	float					u, v;
	float					lu, lv;
} grHWVertex;

// temporary: !	Get rid of the GR_ types!

#define GR_RGBA				grRGBA
#define GR_Rect				grRect	
#define GR_LVertex			grLVertex
#define GR_TLVertex			grTLVertex


#ifdef __cplusplus
}
#endif



//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

typedef grDeviceCaps jeDeviceCaps;
typedef grUVRGBA jeUVRGBA;
typedef grRGBA jeRGBA;
typedef grRGB jeRGB;
typedef grRect jeRect;
typedef grFloatRect jeFloatRect;
typedef grLVertex jeLVertex;
typedef grTLVertex jeTLVertex;
typedef grVertex jeVertex;
typedef grHWVertex jeHWVertex;
#define JE_LVertex                               GR_LVertex
#define JE_RENDER_FLAG_ALPHA                     GR_RENDER_FLAG_ALPHA
#define JE_RENDER_FLAG_BILINEAR_FILTER           GR_RENDER_FLAG_BILINEAR_FILTER
#define JE_RENDER_FLAG_CLAMP_UV                  GR_RENDER_FLAG_CLAMP_UV
#define JE_RENDER_FLAG_COLORKEY                  GR_RENDER_FLAG_COLORKEY
#define JE_RENDER_FLAG_COUNTER_CLOCKWISE         GR_RENDER_FLAG_COUNTER_CLOCKWISE
#define JE_RENDER_FLAG_FLUSHBATCH                GR_RENDER_FLAG_FLUSHBATCH
#define JE_RENDER_FLAG_HWTRANSFORM               GR_RENDER_FLAG_HWTRANSFORM
#define JE_RENDER_FLAG_NO_ZTEST                  GR_RENDER_FLAG_NO_ZTEST
#define JE_RENDER_FLAG_NO_ZWRITE                 GR_RENDER_FLAG_NO_ZWRITE
#define JE_RENDER_FLAG_SPECULAR                  GR_RENDER_FLAG_SPECULAR
#define JE_RENDER_FLAG_STEST                     GR_RENDER_FLAG_STEST
#define JE_RENDER_FLAG_SWRITE                    GR_RENDER_FLAG_SWRITE
#define JE_RENDER_FLAG_VERTEXBUFFER              GR_RENDER_FLAG_VERTEXBUFFER
#define JE_RENDER_FLAG_WIREFRAME                 GR_RENDER_FLAG_WIREFRAME
#define JE_RGBA                                  GR_RGBA
#define JE_Rect                                  GR_Rect
#define JE_TLVertex                              GR_TLVertex

#endif // GENESIS_NO_JET_COMPAT

#endif
