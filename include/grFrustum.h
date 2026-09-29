/****************************************************************************************/
/*  JEFRUSTUM.H                                                                         */
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
#ifndef GR_FRUSTUM2_H		// For now, also defined in Frustum.h (that file will be gone soon)
#define GR_FRUSTUM2_H

#include "BaseType.h"
#include "grTypes.h"
#include "Camera.h"
#include "grPlane.h"

#ifdef __cplusplus
extern "C" {
#endif

//================================================================================
//================================================================================
#define GR_FRUSTUM_MAX_PLANES		32	// 32 bits in uint32, sigh...
#define CLIP_PLANE_EPSILON			0.001f	// CB note : just use GR_EPSILON instead

//================================================================================
//================================================================================
typedef struct jeFrustum
{
	int32			NumPlanes;
	grPlane		Planes[GR_FRUSTUM_MAX_PLANES];

	grPlane		*FrontPlane;		// Pointer into the above list of planes if there is a front clip plane, NULL otherwise

	// Quick LUTs For BBox testing against frustum
	//	CB note : need to document how this is used : !!
	int32			FrustumBBoxIndexes[GR_FRUSTUM_MAX_PLANES*6];
	int32			*pFrustumBBoxIndexes[GR_FRUSTUM_MAX_PLANES];
} grFrustum;
typedef struct jeFrustum jeFrustum;

// NOTE - SrcVerts can be the same array as Work1, but SrcVerts cannot be same as Work2!!!
typedef struct
{
	uint32		ClipFlags;			// Bit for each frustum plane

	int32			NumSrcVerts;
	const grLVertex	*SrcVerts;			// Verts to be clipped

	grLVertex	*Work1;				// Working temps (should be at least as big as NumSrcVerts+1)
	grLVertex	*Work2;

	// This is to be filled in by clip function
	// DstVerts could be a pointer to SrcVerts, Work1, or Work2
	int32			NumDstVerts;		// Num DstVerts
	grLVertex	*DstVerts;			// Dest array
} grFrustum_LClipInfo;
typedef grFrustum_LClipInfo jeFrustum_LClipInfo;

typedef struct
{
	uint32		ClipFlags;			// Bit for each frustum plane

	int32			NumSrcVerts;
	const grVec3d	*SrcVerts;			// Verts to be clipped

	grVec3d		*Work1;				// Working temps (should be at least as big as NumSrcVerts+1)
	grVec3d		*Work2;

	// This is to be filled in by clip function
	// DstVerts could be a pointer to SrcVerts, Work1, or Work2
	int32			NumDstVerts;		// Num DstVerts
	grVec3d		*DstVerts;			// Dest array
} grFrustum_ClipInfo;
typedef grFrustum_ClipInfo jeFrustum_ClipInfo;


//================================================================================
// frustum setup functions

GRAPI void		GRCC grFrustum_SetFromCamera(grFrustum *Frustum, const grCamera *Camera);
GRAPI void		GRCC grFrustum_SetWorldSpaceFromCamera(grFrustum *Frustum, const grCamera *Camera);
GRAPI grBoolean	GRCC grFrustum_SetFromVerts(grFrustum *Frustum, const grVec3d *POV, const grVec3d *Verts, int32 NumVerts);
GRAPI grBoolean	GRCC grFrustum_SetFromVerts2(grFrustum *Frustum, const grVec3d *Verts, int32 NumVerts);
GRAPI grBoolean	GRCC grFrustum_SetFromLVerts(grFrustum *Frustum, const grVec3d *POV, const grLVertex *Verts, int32 NumVerts);
GRAPI grBoolean  GRCC grFrustum_SetFromLVerts2(grFrustum *Frustum, const grLVertex *Verts, int32 NumVerts, grBoolean Flip);

GRAPI grBoolean	GRCC grFrustum_AddPlane(grFrustum *Frustum, const grPlane *SrcPlane, grBoolean FrontPlane);
	// Returns GR_TRUE on success, GR_FALSE if plane could not be added (Out of space)

GRAPI void		GRCC grFrustum_RotateToWorldSpace(	const grFrustum *In, const grCamera *Camera, grFrustum *Out);
GRAPI void		GRCC grFrustum_TransformToWorldSpace(const grFrustum *In, const grCamera *Camera, grFrustum *Out);
GRAPI void		GRCC grFrustum_Rotate(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out);
GRAPI void		GRCC grFrustum_Transform(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out);
GRAPI void		GRCC grFrustum_TransformRenorm(const grFrustum *In, const grXForm3d *XForm, grFrustum *Out);
GRAPI void		GRCC grFrustum_TransformAnchored(grFrustum *F, const grXForm3d *XForm, const grVec3d * Anchor);

//================================================================================
// miscellaneous

GRAPI grBoolean	GRCC grFrustum_SetClipFlagsFromExtBox(const grFrustum *Frustum,const grExtBox *BBox,uint32 InClipFlags,uint32 *pClipFlags);
					// returns GR_FALSE if the bbox is totally outside the frustum
					//	pClipFlags is optional

//================================================================================
// Clip functions :
// CB note : these should probably take a MaxOutVerts parameter too, to assert on !!

GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUV(	const grPlane *pPlane, 
											const grLVertex *pIn, grLVertex *pOut,
											int32 NumVerts, int32 *OutVerts);
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGB(	const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts);
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGBA(	const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts);
GRAPI grBoolean GRCC grFrustum_ClipLVertsToPlaneXYZUVRGBAS(const grPlane *pPlane, 
												const grLVertex *pIn, grLVertex *pOut,
												int32 NumVerts, int32 *OutVerts);
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUV(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo);
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGB(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo);
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGBA(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo);
GRAPI grBoolean GRCC grFrustum_ClipLVertsXYZUVRGBAS(const grFrustum *Frustum, grFrustum_LClipInfo *ClipInfo);

GRAPI grBoolean GRCC grFrustum_ClipVertsToPlane(	const grPlane *pPlane, 
										const grVec3d *pIn, grVec3d *pOut,
										int32 NumVerts, int32 *OutVerts);

GRAPI grBoolean GRCC grFrustum_ClipVerts(const grFrustum *Frustum, grFrustum_ClipInfo *ClipInfo);

GRAPI grBoolean GRCC grFrustum_PointCollision(const grFrustum *Frustum, const grVec3d *Point, grFloat Radius);
//================================================================================

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define JE_FRUSTUM_MAX_PLANES                    GR_FRUSTUM_MAX_PLANES
#define jeFrustum_AddPlane                       grFrustum_AddPlane
#define jeFrustum_ClipLVertsToPlaneXYZUV         grFrustum_ClipLVertsToPlaneXYZUV
#define jeFrustum_ClipLVertsToPlaneXYZUVRGB      grFrustum_ClipLVertsToPlaneXYZUVRGB
#define jeFrustum_ClipLVertsToPlaneXYZUVRGBA     grFrustum_ClipLVertsToPlaneXYZUVRGBA
#define jeFrustum_ClipLVertsToPlaneXYZUVRGBAS    grFrustum_ClipLVertsToPlaneXYZUVRGBAS
#define jeFrustum_ClipLVertsXYZUV                grFrustum_ClipLVertsXYZUV
#define jeFrustum_ClipLVertsXYZUVRGB             grFrustum_ClipLVertsXYZUVRGB
#define jeFrustum_ClipLVertsXYZUVRGBA            grFrustum_ClipLVertsXYZUVRGBA
#define jeFrustum_ClipLVertsXYZUVRGBAS           grFrustum_ClipLVertsXYZUVRGBAS
#define jeFrustum_ClipVerts                      grFrustum_ClipVerts
#define jeFrustum_ClipVertsToPlane               grFrustum_ClipVertsToPlane
#define jeFrustum_PointCollision                 grFrustum_PointCollision
#define jeFrustum_Rotate                         grFrustum_Rotate
#define jeFrustum_RotateToWorldSpace             grFrustum_RotateToWorldSpace
#define jeFrustum_SetClipFlagsFromExtBox         grFrustum_SetClipFlagsFromExtBox
#define jeFrustum_SetFromCamera                  grFrustum_SetFromCamera
#define jeFrustum_SetFromLVerts                  grFrustum_SetFromLVerts
#define jeFrustum_SetFromLVerts2                 grFrustum_SetFromLVerts2
#define jeFrustum_SetFromVerts                   grFrustum_SetFromVerts
#define jeFrustum_SetFromVerts2                  grFrustum_SetFromVerts2
#define jeFrustum_SetWorldSpaceFromCamera        grFrustum_SetWorldSpaceFromCamera
#define jeFrustum_Transform                      grFrustum_Transform
#define jeFrustum_TransformAnchored              grFrustum_TransformAnchored
#define jeFrustum_TransformRenorm                grFrustum_TransformRenorm
#define jeFrustum_TransformToWorldSpace          grFrustum_TransformToWorldSpace

#endif // GENESIS_NO_JET_COMPAT

#endif
