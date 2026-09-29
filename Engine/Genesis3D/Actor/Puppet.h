/****************************************************************************************/
/*  PUPPET.H																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Puppet interface.										.				*/
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
#ifndef GR_PUPPET_H
#define GR_PUPPET_H

#include "Motion.h"
#include "Camera.h"
#include "Body.h"
#include "Pose.h"
#include "ExtBox.h"			// grExtBox for grPuppet_RenderThroughFrustum

#include "VFile.h"
#include "UVMap.h"

#include "grFrustum.h"
#include "Engine.h"
#include "grWorld.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct grPuppet grPuppet;

//	[MacroArt::Begin]
//	Thanks Dee(cryscan@home.net)	
float	GRCF grPuppet_GetAlpha(const grPuppet *P);
void	GRCF grPuppet_SetAlpha(grPuppet *P,float Alpha);
//	[MacroArt::End]

grPuppet* GRCF grPuppet_Create(grVFile *TextureFS, const grBody *B, grEngine *pEngine);

void GRCF grPuppet_Destroy(grPuppet **P);

grBoolean grPuppet_RenderThroughFrustum(const grPuppet *P, 
					const grPose		*Joints, 
					const grExtBox		*Box, 
					grEngine			*Engine, 
					const grWorld		*World,
					const grCamera		*Camera, 
					const grFrustum		*Frustum,
					grBoolean			updateStaticLighting);
	
grBoolean grPuppet_Render(const grPuppet *P,
					const grPose		*Joints,
					grEngine			*Engine, 
					const grWorld		*World,
					const grCamera		*Camera, 
					grExtBox			*Box,
					grBoolean			updateStaticLighting);

int GRCF grPuppet_GetMaterialCount( grPuppet *P );
grBoolean grPuppet_GetMaterial( grPuppet *P, int MaterialIndex,
									grMaterialSpec **Bitmap, 
									grFloat *Red, grFloat *Green, grFloat *Blue,
									grUVMapper * pMapper);
grBoolean grPuppet_SetMaterial(grPuppet *P, int MaterialIndex, grMaterialSpec *Bitmap, 
										grFloat Red, grFloat Green, grFloat Blue, grUVMapper Mapper);

void	  grPuppet_SetShadow(grPuppet *P, grBoolean DoShadow, grFloat Scale, 
						const grMaterialSpec *ShadowMap,int BoneIndex);

void	  grPuppet_GetLightingOptions(const grPuppet *P,
	grBoolean *UseFillLight,
	grVec3d *FillLightNormal,
	grFloat *FillLightRed,				
	grFloat *FillLightGreen,				
	grFloat *FillLightBlue,				
	grFloat *AmbientLightRed,			
	grFloat *AmbientLightGreen,			
	grFloat *AmbientLightBlue,			
	grBoolean *UseAmbientLightFromFloor,
	int32 *MaximumDynamicLightsToUse,
	int32 *MaximumStaticLightsToUse,	
	int32 *LightReferenceBoneIndex,
	grBoolean *PerBoneLighting
	);

void	  grPuppet_SetLightingOptions(grPuppet *P,
	grBoolean UseFillLight,
	const grVec3d *FillLightNormal,
	grFloat FillLightRed,				// 0 .. 255
	grFloat FillLightGreen,				// 0 .. 255
	grFloat FillLightBlue,				// 0 .. 255
	grFloat AmbientLightRed,			// 0 .. 255
	grFloat AmbientLightGreen,			// 0 .. 255
	grFloat AmbientLightBlue,			// 0 .. 255
	grBoolean AmbientLightFromFloor,
	int MaximumDynamicLightsToUse,		// 0 for none
	int MaximumStaticLightsToUse, // 0 for none
	int LightReferenceBoneIndex,
	int PerBoneLighting);

grEngine* GRCF grPuppet_GetEngine(grPuppet *P);

#ifdef __cplusplus
}
#endif


#endif
