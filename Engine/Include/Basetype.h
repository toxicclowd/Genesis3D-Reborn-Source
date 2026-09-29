/****************************************************************************************/
/*  BASETYPE.H                                                                          */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: Basic type definitions and calling convention defines                  */
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
#ifndef GR_BASETYPE_H
#define GR_BASETYPE_H
 
/*
	Some basic types defined with clear names for
	more specific data definitions
*/
 
#ifdef __cplusplus
extern "C" {
#endif


//------------------------------ 
// function types

// Krouer - change calling convention to __stdcall
// keep __fastcall for internal call perhaps engine can increase the gain by using __inline instead
// but __inline will increase the size
#define	GRCF	__fastcall
#define	GRCC	__stdcall

// paradoxnj - We don't care about static libs.  Changed to conventional DLL export
#ifdef GENESIS3D_EXPORTS
#define GRAPI					_declspec(dllexport)
#else
#define GRAPI					_declspec(dllimport)
#endif

#define G3DLINE __inline //added (cyrius)

//------------------------------

typedef int			grBoolean;
typedef grBoolean	grBoolean;
#define GR_FALSE	((grBoolean)0)
#define GR_TRUE		((grBoolean)1)

//------------------------------

typedef float grFloat;
typedef grFloat grFloat;

typedef signed long     int32;
typedef signed short    int16;
typedef signed char     int8 ;
typedef unsigned long	uint32;
typedef unsigned short	uint16;
typedef unsigned char	uint8 ;

//------------------------------

#ifndef NULL
#define NULL													(0)
#endif

#define	GR_PI													((grFloat)3.14159265358979323846)
#define	GR_TWOPI											((grFloat)6.28318530717958647692)
#define	GR_HALFPI											((grFloat)1.57079632679489661923)

#define GR_DEGS_PER_RAD								((grFloat)0.01745329251994329576)
#define GR_RADS_PER_DEG								((grFloat)57.2957795130823208767)

// BEGIN - 32-bit color values - paradoxnj 8/3/2005
// maps unsigned 8 bits/channel to uint32
#define GR_COLOR_ARGB(a,r,g,b)						((uint32)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define GR_COLOR_RGBA(r,g,b,a)						GR_COLOR_ARGB(a,r,g,b)
#define GR_COLOR_XRGB(r,g,b)						GR_COLOR_ARGB(0xff,r,g,b)

#define GR_COLOR_XYUV(y,u,v)						GR_COLOR_ARGB(0xff,y,u,v)
#define GR_COLOR_AYUV(a,y,u,v)						GR_COLOR_ARGB(a,y,u,v)

// maps floating point channels (0.f to 1.f range) to uint32
#define GR_COLOR_COLORVALUE(r,g,b,a)				GR_COLOR_RGBA((uint32)((r)*255.f),(uint32)((g)*255.f),(uint32)((b)*255.f),(uint32)((a)*255.f))

#define GR_COLOR_GETARGB(argb,a,r,g,b)				{a=((argb>>24)&0xff); r=((argb>>16)&0xff); g=((argb>>8)&0xff); b=((argb)&0xff); }

// END - 32-bit color values - paradoxnj 8/3/2005

// should probably be moved to trig module
__inline grFloat grFloat_DegToRad(grFloat d)
{
	return d * GR_DEGS_PER_RAD;
}

__inline grFloat grFloat_RadToDeg(grFloat r)
{
	return r * GR_RADS_PER_DEG;
}


//------------------------------
// macros on basic jet types

#define GR_ABS(x)										( (x) < 0 ? (-(x)) : (x) )
#define GR_CLAMP(x,lo,hi)								( (x) < (lo) ? (lo) : ( (x) > (hi) ? (hi) : (x) ) )
#define GR_CLAMP8(x)									GR_CLAMP(x,0,255)
#define GR_CLAMP16(x)									GR_CLAMP(x,0,65536)
#define GR_BOOLSAME(x,y)								( ( (x) && (y) ) || ( !(x) && !(y) ) )

#define GR_EPSILON										((grFloat)0.000797f)
#define GR_FLOATS_EQUAL(x,y)							( GR_ABS((x) - (y)) < GR_EPSILON )
#define GR_FLOAT_ISZERO(x)								GR_FLOATS_EQUAL(x,0.0f)

// you're right... inline funcs are more useful :^)
static __inline grFloat grFloat_Sqr(grFloat a)
{
	return a * a;
}

static __inline grFloat grFloat_Cube(grFloat a)
{
	return a * a * a;
}

//------------------------------

// CB : what does the optimizer do with inline assembly in inline functions ?
//		will it turn off all optimizations?

static grFloat __inline grFloat_RoundToInt(grFloat val) // rounds depending on how you set grCPU_FloatControl
{
	__asm
	{
		FLD  val
		FRNDINT
		FSTP val
	}
return val;
}

static grFloat __inline grFloat_Sqrt(grFloat val)
{
	__asm 
	{
		FLD  val		// 1 clock
		FSQRT			// 30-70 clocks
		FSTP val		// 2 clocks
	}
return val;
}

static grFloat __inline grFloat_Sin(grFloat val)
{
	__asm 
	{
		FLD  val		// 1 clock
		FSIN			// ~ 200 clocks
		FSTP val		// 2 clocks
	}
return val;
}

static grFloat __inline grFloat_Cos(grFloat val)
{
	__asm 
	{
		FLD  val		// 1 clock
		FCOS			// ~ 200 clocks
		FSTP val		// 2 clocks
	}
return val;
}

static int32 __inline grFloat_ToInt(grFloat f)
{
int32 i;
	__asm
	{
		FLD   f
		FISTP i
	}
return i;
}

#pragma warning (disable:4514)	// unreferenced inline function

//------------------------------

#ifdef __cplusplus
class grUnknown
{
protected:
	virtual ~grUnknown()						{}

public:
	virtual uint32					AddRef() = 0;
	virtual uint32					Release() = 0;
};
typedef grUnknown grUnknown;
#endif

// Genesis3D: Reborn gr* Aliases



#ifdef __cplusplus
}
#endif

#endif
