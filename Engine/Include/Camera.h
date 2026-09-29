/****************************************************************************************/
/*  CAMERA.H                                                                            */
/*                                                                                      */
/*  Author:                                                                             */
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
#ifndef grCAMERA_H
#define grCAMERA_H

#include "BaseType.h"
#include "Vec3d.h"
#include "Xform3d.h"
#include "grTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//================================================================================
//	Structure defines
//================================================================================
typedef struct grCamera	grCamera;


//================================================================================
//	Function ProtoTypes
//================================================================================

//-----------------------------------------------------
//	Create/Destroy
//

GRAPI grCamera *	GRCC grCamera_Create(grFloat FovRadians, const grRect *Rect);
GRAPI void			GRCC grCamera_Destroy(grCamera **pCamera);

//-----------------------------------------------------
//	Camera XForm's
//

GRAPI grBoolean	GRCC grCamera_SetXForm(grCamera *Camera, const grXForm3d *XForm);
GRAPI void			GRCC grCamera_GetXForm( const grCamera *Camera,grXForm3d *pXForm);
GRAPI grBoolean	GRCC grCamera_SetTransposeXForm(grCamera *Camera, const grXForm3d *XForm);
GRAPI void			GRCC grCamera_GetTransposeXForm( const grCamera *Camera,grXForm3d *pXForm);

GRAPI grBoolean	GRCC grCamera_PushXForm(grCamera *Camera);
GRAPI grBoolean	GRCC grCamera_PopXForm( grCamera *Camera);

//-----------------------------------------------------
//	Misc Get/Set
//

GRAPI void		GRCC grCamera_GetClippingRect(const grCamera *Camera, grRect *Rect);

// Added by Jeff 02/09/05:  Returns Camera's FOV and Rect
GRAPI void     GRCC grCamera_GetAttributes(grCamera *Camera, grFloat *FovRadians, grRect *Rect);

GRAPI void		GRCC grCamera_SetAttributes(grCamera *Camera, grFloat FovRadians, const grRect *Rect);
GRAPI void		GRCC grCamera_SetZScale(grCamera *Camera, grFloat ZScale);
GRAPI grFloat	GRCC grCamera_GetZScale(const grCamera *Camera);

// BEGIN - Far clip plane - paradoxnj 2/9/2005
GRAPI void		GRCC grCamera_SetFarClipPlane(grCamera *Camera, grBoolean Enable, grFloat ZFar);
GRAPI void		GRCC grCamera_GetFarClipPlane(const grCamera *Camera, grBoolean *Enable, grFloat *ZFar);
// END - Far clip plane - paradoxnj 2/9/2005

//-----------------------------------------------------
//	Transform/Project :
//
GRAPI void GRCC grCamera_ScreenPointToWorld(	const grCamera	*Camera,
														int32			 ScreenX,
														int32			 ScreenY,
														grVec3d			*Vector);
GRAPI void GRCC grCamera_Project(	const grCamera	*Camera, 
											const grVec3d	*PointInCameraSpace, 
											grVec3d			*ProjectedPoint);
GRAPI void GRCC grCamera_ProjectArray(const grCamera	*Camera, 
												const grVec3d	*FmPoints, 
												int32			FmStride,
												grVec3d			*ToPoints, 
												int32			ToStride, 
												int32			Count);
GRAPI void GRCC grCamera_ProjectAndClampArray(const grCamera	*Camera, 
												const grVec3d	*FmPoints, 
												int32			FmStride,
												grVec3d			*ToPoints, 
												int32			ToStride, 
												int32			Count);
GRAPI void GRCC grCamera_ProjectZ(const grCamera	*Camera, 
											const grVec3d	*PointInCameraSpace, 
											grVec3d			*ProjectedPoint);
											
GRAPI void GRCC grCamera_ProjectAndClamp(const grCamera	*Camera, 
										const grVec3d	*PointInCameraSpace, 
										grVec3d			*ProjectedPoint);

GRAPI void GRCC grCamera_Transform(	const grCamera	*Camera, 
												const grVec3d	*WorldSpacePoint, 
												grVec3d			*CameraSpacePoint);
GRAPI void GRCC grCamera_TransformVecArray(	const grCamera	*Camera, 
														const grVec3d	*WorldSpacePointPtr, 
														grVec3d			*CameraSpacePointPtr,
														int32			Count);

GRAPI void GRCC grCamera_TransformAndProjectVecArray(	const grCamera *Camera, 
																const grVec3d *WorldSpacePointPtr, 
																grVec3d *ProjectedSpacePointPtr,
																int32 Count);
GRAPI void GRCC grCamera_TransformAndProjectArray(const grCamera	*Camera, 
															const grVec3d	*WorldSpacePointPtr, 
															int32			WorldStride,
															grVec3d			*ProjectedSpacePointPtr, 
															int32			ProjectedStride,
															int32			Count);
GRAPI void GRCC grCamera_TransformAndProjectLArray(	const grCamera		*Camera, 
																const grLVertex	*WorldSpacePointPtr, 
																grTLVertex			*ProjectedSpacePointPtr,
																int32				Count);
GRAPI void GRCC grCamera_TransformAndProject(	const	grCamera *Camera,
														const	grVec3d *Point, 
														grVec3d	*ProjectedPoint);
GRAPI void GRCC grCamera_TransformAndProjectL(const grCamera *Camera,
														const grLVertex *Point, 
														grTLVertex *ProjectedPoint);

GRAPI void GRCC grCamera_TransformAndProjectAndClampArray(const grCamera	*Camera, 
															const grVec3d	*WorldSpacePointPtr, 
															int32			WorldStride,
															grVec3d			*ProjectedSpacePointPtr, 
															int32			ProjectedStride,
															int32			Count);
GRAPI void GRCC grCamera_TransformLArray(	const grCamera	*Camera, 
																	const grLVertex		*WorldSpacePointPtr, 
																	grLVertex			*CameraSpacePointPtr,
																	int32				Count);
GRAPI void GRCC grCamera_ProjectAndClampLArray(	const grCamera		*Camera, 
															const grLVertex		*CameraSpacePointPtr, 
															grTLVertex			*ProjectedSpacePointPtr,
															int32				Count);
GRAPI void GRCC grCamera_TransformAndProjectAndClampLArray(	const grCamera		*Camera, 
																const grLVertex	*WorldSpacePointPtr, 
																grTLVertex			*ProjectedSpacePointPtr,
																int32				Count);
GRAPI void GRCC grCamera_TransformAndProjectAndClamp(	const	grCamera *Camera,
														const	grVec3d *Point, 
														grVec3d	*ProjectedPoint);
GRAPI void GRCC grCamera_TransformL(	const grCamera	*Camera,
												const grLVertex *Point, 
												grLVertex		*TransformedPoint);
GRAPI void GRCC grCamera_ProjectAndClampL(const grCamera	*Camera,
													const grLVertex *Point, 
													grTLVertex		*ProjectedPoint);
GRAPI void GRCC grCamera_TransformAndProjectAndClampL(const grCamera *Camera,
														const grLVertex *Point, 
														grTLVertex *ProjectedPoint);

GRAPI grVec3d *GRCC grCamera_GetPov2(grCamera *Camera);

//-----------------------------------------------------

#ifdef __cplusplus
}
#endif

// Genesis3D: Reborn gr* Aliases


#endif
