/****************************************************************************************/
/*  JELIGHT.C                                                                           */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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
#include <assert.h>
#include <memory.h>

#include "grLight.h"

#include "Errorlog.h"
#include "Ram.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

typedef struct grLight
{
	int32		RefCount;

	grVec3d		Pos;
	grVec3d		Color;
	grFloat		Radius;
	grFloat		Brightness;

	uint32		Flags;
} grLight;

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
		((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define GR_LIGHT_TAG			MAKEFOURCC('G', 'E', 'L', 'F')		// 'GE' 'L'ight 'F'ile
#define GR_LIGHT_VERSION		0x0000

//========================================================================================
//	grLight_Create
//========================================================================================
GRAPI grLight * GRCC grLight_Create(void)
{
	grLight		*Light;

	Light = GR_RAM_ALLOCATE_STRUCT(grLight);

	if (!Light)
		return NULL;

	ZeroMem(Light);

	grVec3d_Set(&Light->Color, 1.0f, 1.0f, 1.0f);

	Light->Brightness = 1.0f;
	Light->Radius = 200.0f;

	Light->RefCount = 1;

	return Light;
}

//========================================================================================
//	grLight_CreateFromFile
//========================================================================================
GRAPI grLight * GRCC grLight_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
{
	grLight		*Light;
	uint32		Tag;
	uint16		Version;

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, (void **)&Light))
			return NULL;

		if (Light)
		{
			if (!grLight_CreateRef(Light))
				return NULL;

			return Light;		// Ptr found in stack, return it
		}
	}

	// Read header info
	if (!grVFile_Read(VFile, &Tag, sizeof(Tag)))
		return NULL;

	if (Tag != GR_LIGHT_TAG)
		return NULL;

	if (!grVFile_Read(VFile, &Version, sizeof(Version)))
		return NULL;

	if (Version != GR_LIGHT_VERSION)
		return NULL;

	// Create and Read the light
	Light = GR_RAM_ALLOCATE_STRUCT(grLight);

	if (!Light)
		return NULL;

	ZeroMem(Light);
	
	if (!grVFile_Read(VFile, Light, sizeof(grLight)))
		goto ExitWithError;

	Light->RefCount = 1;	// Icestorm: Moved it AFTER reading Light, so saved refs are ignored

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Light))
			goto ExitWithError;
	}

	return Light;

	ExitWithError:
	{
		if (Light)
			grRam_Free(Light);

		return NULL;
	}
}

//========================================================================================
//	grLight_CreateFromLight
//========================================================================================
GRAPI grLight * GRCC grLight_CreateFromLight(const grLight *SrcLight)
{
	grLight		*Light;

	Light = GR_RAM_ALLOCATE_STRUCT(grLight);

	if (!Light)
		return NULL;

	ZeroMem(Light);

	*Light = *SrcLight;
	Light->RefCount = 1;

	return Light;
}

//========================================================================================
//	grLight_WriteToFile
//========================================================================================
GRAPI grBoolean GRCC grLight_WriteToFile(const grLight *Light, grVFile *VFile, grPtrMgr *PtrMgr)
{
	uint32				Tag;
	uint16				Version;

	if (PtrMgr)
	{
		uint32		Count;

		if (!grPtrMgr_WritePtr(PtrMgr, VFile, (void*)Light, &Count))
			return GR_FALSE;

		if (Count)
			return GR_TRUE;		// Ptr was on stack, so return
	}

	assert(grLight_IsValid(Light) == GR_TRUE);

	// Write TAG
	Tag = GR_LIGHT_TAG;

	if (!grVFile_Write(VFile, &Tag, sizeof(Tag)))
		return GR_FALSE;

	// Write version
	Version = GR_LIGHT_VERSION;

	if (!grVFile_Write(VFile, &Version, sizeof(Version)))
		return GR_FALSE;
	
	// Write the light
	if (!grVFile_Write(VFile, Light, sizeof(grLight)))
		return GR_FALSE;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, (void*)Light))
			return GR_FALSE;
	}

	return GR_TRUE;
}

//========================================================================================
//	grLight_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grLight_CreateRef(grLight *Light)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	assert(Light->RefCount < (0xFFFFFFFF>>1)-1);
	
	Light->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grLight_Destroy
//========================================================================================
GRAPI void GRCC grLight_Destroy(grLight **Light)
{
	assert(Light);
	assert(grLight_IsValid(*Light) == GR_TRUE);

	(*Light)->RefCount --;

	if ((*Light)->RefCount == 0)
		grRam_Free(*Light);

	*Light = NULL;
}

//========================================================================================
//	grLight_IsValid
//========================================================================================
GRAPI grBoolean GRCC grLight_IsValid(const grLight *Light)
{
	if (!Light)
		return GR_FALSE;

	if (Light->RefCount <= 0)
		return GR_FALSE;

	return GR_TRUE;
}

//========================================================================================
// grLight_SetAttributes
//========================================================================================
GRAPI grBoolean GRCC grLight_SetAttributes(grLight *Light, 
								const grVec3d *Pos, 
								const grVec3d *Color, 
								grFloat Radius, 
								grFloat Brightness, 
								uint32 Flags)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	Light->Pos = *Pos;
	Light->Color = *Color;
	Light->Radius = Radius;
	Light->Brightness = Brightness;
	Light->Flags = Flags;

	Light->Radius = grLight_GetRadius(Light);

	return GR_TRUE;
}

//========================================================================================
// grLight_GetAttributes
//========================================================================================
GRAPI grBoolean GRCC grLight_GetAttributes(const grLight *Light, 
								grVec3d *Pos, 
								grVec3d *Color, 
								grFloat *Radius, 
								grFloat *Brightness, 
								uint32 *Flags)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	if (Pos)
		*Pos = Light->Pos;
	if (Color)
		*Color = Light->Color;
	if (Radius)
		*Radius = Light->Radius;
	if (Brightness)
		*Brightness = Light->Brightness;
	if (Flags)
		*Flags = Light->Flags;

	return GR_TRUE;
}

//========================================================================================
//	grLight_GetFlags
//========================================================================================
GRAPI uint32 GRCC grLight_GetFlags(const grLight *Light)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	return Light->Flags;
}

//========================================================================================
//	grLight_GetRadius
//========================================================================================
GRAPI grFloat GRCC grLight_GetRadius(const grLight *Light)
{
	switch( Light->Flags & GR_LIGHT_FLAG_TYPEMASK )
	{
		case GR_LIGHT_FLAG_SUN:
			return 999999999999999.0f;

		case 0: // <> for backward compatibility
		case GR_LIGHT_FLAG_LINEAR_FALLOFF:
			return Light->Radius;

		case GR_LIGHT_FLAG_INVERSE_FALLOFF:
			return Light->Brightness;

		case GR_LIGHT_FLAG_INVERSE_SQUARE_FALLOFF:
			return grFloat_Sqrt(Light->Brightness);

		default:
			grErrorLog_AddString(-1,"grLight : unknown type!",NULL);
			return 0.0f;
	}
}

//========================================================================================
//	grLight_SetSunLight
//========================================================================================
GRAPI grBoolean GRCC grLight_SetSunLight(grLight *Light, 
								const grVec3d *DirectionToSun, 
								const grVec3d *Color, 
								grFloat Brightness)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	Light->Pos = *DirectionToSun;
	Light->Color = *Color;
	Light->Brightness = Brightness;
	Light->Flags = GR_LIGHT_FLAG_SUN;

	Light->Radius = grLight_GetRadius(Light);

return GR_TRUE;
}

GRAPI grBoolean GRCC grLight_SetInverseLight(grLight *Light, 
								const grVec3d *Pos, 
								const grVec3d *Color, 
								grFloat Brightness)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	Light->Pos = *Pos;
	Light->Color = *Color;
	Light->Brightness = Brightness;
	Light->Flags = GR_LIGHT_FLAG_INVERSE_FALLOFF;

	Light->Radius = grLight_GetRadius(Light);

return GR_TRUE;
}

GRAPI grBoolean GRCC grLight_SetInverseSquaredLight(grLight *Light, 
								const grVec3d *Pos, 
								const grVec3d *Color, 
								grFloat Brightness)
{
	assert(grLight_IsValid(Light) == GR_TRUE);

	Light->Pos = *Pos;
	Light->Color = *Color;
	Light->Brightness = Brightness;
	Light->Flags = GR_LIGHT_FLAG_INVERSE_SQUARE_FALLOFF;

	Light->Radius = grLight_GetRadius(Light);

return GR_TRUE;
}

//========================================================================================
//	grLight_CalculateLighting
//========================================================================================
GRAPI grBoolean	GRCC grLight_CalculateLighting(const grLight * Light,const grVec3d *pPos,const grVec3d *pNormal,
													grRGBA * pColor)
{
grFloat scale;

	assert(grLight_IsValid(Light));

	pColor->a = 255.0f; 

	if ( (Light->Flags & GR_LIGHT_FLAG_TYPEMASK) == GR_LIGHT_FLAG_SUN )
	{
		scale = grVec3d_DotProduct(&(Light->Pos),pNormal);

		if ( scale < 0.0f )
		{
			pColor->r = pColor->g = pColor->b = 0.0f;
			return GR_TRUE;
		}
	}
	else
	{
	grVec3d v;
	grFloat len;

		grVec3d_Subtract(&(Light->Pos),pPos,&v);

		len = grVec3d_Normalize(&v);

		scale = grVec3d_DotProduct(&v,pNormal);

		if ( scale < 0.0f )
		{
			pColor->r = pColor->g = pColor->b = 0.0f;
			return GR_TRUE;
		}

		switch( Light->Flags & GR_LIGHT_FLAG_TYPEMASK )
		{
			case 0: // <> for backward compatibility
			case GR_LIGHT_FLAG_LINEAR_FALLOFF:
				
				if ( len >= Light->Radius )
					scale = 0.0f;
				else
					scale *= ( 1.0f - (len / Light->Radius) );

				break;

			case GR_LIGHT_FLAG_INVERSE_FALLOFF:

				scale /= len;

				break;

			case GR_LIGHT_FLAG_INVERSE_SQUARE_FALLOFF:
			
				scale /= (len * len);

				break;

			default:
				grErrorLog_AddString(-1,"grLight : unknown type!",NULL);
				return GR_FALSE;			
		}
	}

	scale *= Light->Brightness;
	pColor->r = Light->Color.X * scale;
	pColor->g = Light->Color.Y * scale;
	pColor->b = Light->Color.Z * scale;

return GR_TRUE;
}
