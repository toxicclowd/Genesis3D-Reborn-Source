/****************************************************************************************/
/*  JELIGHT.H                                                                           */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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
#ifndef GR_LIGHT2_H
#define GR_LIGHT2_H

#include "BaseType.h"
#include "Vec3d.h"
#include "VFile.h"
#include "grPtrMgr.h"
#include "grTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================

typedef struct grLight grLight;

#define	GR_LIGHT_FLAG_SUN						(1<<0)
#define	GR_LIGHT_FLAG_PARALLEL					(1<<0) // synonym for sun
#define	GR_LIGHT_FLAG_LINEAR_FALLOFF			(1<<1)
#define	GR_LIGHT_FLAG_INVERSE_FALLOFF			(1<<2)
#define	GR_LIGHT_FLAG_INVERSE_SQUARE_FALLOFF	(1<<3)
#define GR_LIGHT_FLAG_TYPEMASK					((1<<8) - 1)

#define	GR_LIGHT_FLAG_FAST_LIGHTING_MODEL		(1<<8)

//========================================================================================
// standard utilities :

GRAPI grLight		* GRCC grLight_Create(void);
GRAPI grLight		* GRCC grLight_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
GRAPI grLight		* GRCC grLight_CreateFromLight(const grLight *SrcLight);
GRAPI grBoolean	GRCC grLight_WriteToFile(const grLight *Light, grVFile *VFile, grPtrMgr *PtrMgr);
GRAPI grBoolean	GRCC grLight_CreateRef(grLight *Light);
GRAPI void			GRCC grLight_Destroy(grLight **Light);
GRAPI grBoolean	GRCC grLight_IsValid(const grLight *Light);

//========================================================================================
// a handy function to calculate the illumination for you:

GRAPI grBoolean	GRCC grLight_CalculateLighting(const grLight * Light,const grVec3d *pPos,const grVec3d *pNormal,
													grRGBA * pColor);

//========================================================================================

GRAPI grBoolean	GRCC grLight_SetAttributes(	grLight *Light, 
									const grVec3d *Pos, 
									const grVec3d *Color, 
									grFloat Radius, 
									grFloat Brightness, 
									uint32 Flags);

GRAPI grBoolean	GRCC grLight_GetAttributes(const grLight *Light, 
									grVec3d *Pos, 
									grVec3d *Color, 
									grFloat *Radius, 
									grFloat *Brightness, 
									uint32 *Flags);

GRAPI uint32		GRCC grLight_GetFlags(const grLight *Light);

GRAPI grFloat    GRCC grLight_GetRadius(const grLight *Light); 
									// calculates the radius for non LINEAR lights
									// returns a large value for sun lights

// shortcuts to SetAttributes to set up various types of lights:

GRAPI grBoolean	GRCC grLight_SetSunLight(grLight *Light, 
								const grVec3d *DirectionToSun, 
								const grVec3d *Color, 
								grFloat Brightness);

GRAPI grBoolean	GRCC grLight_SetInverseLight(grLight *Light, 
								const grVec3d *Pos, 
								const grVec3d *Color, 
								grFloat Brightness);

GRAPI grBoolean	GRCC grLight_SetInverseSquaredLight(grLight *Light, 
								const grVec3d *Pos, 
								const grVec3d *Color, 
								grFloat Brightness);

//========================================================================================

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
