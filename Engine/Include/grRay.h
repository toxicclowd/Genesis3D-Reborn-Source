/*!
  @file grRay.h

  @author Anthony Rufrano (paradoxnj)
  @brief Ray casting code
                                                                   
  The contents of this file are subject to the Genesis3D: Reborn Public License
  Version 1.02 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.genesis3d.com
                                                                     
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
  the License for the specific language governing rights and limitations
  under the License.
                                 
  The Original Code is Genesis3D: Reborn, released December 12, 1999.
  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved
*/

#ifndef GR_RAY_H
#define GR_RAY_H

#include "BaseType.h"
#include "Vec3d.h"
#include "ExtBox.h"
#include "grPlane.h"

/*!
	@struct grRay
	@brief Represents a ray in 3D space
*/
typedef struct grRay
{
	grVec3d						Origin;			///< Where the ray begins
	grVec3d						Direction;		///< The direction the ray is pointing
} grRay;

/*!
	@fn grRay_Set(grRay *Ray, grVec3d *Origin, grVec3d *Dir)
	@brief Sets the ray's data
	@param[in] Ray The ray to modify
	@param[in] Origin The point at which the ray begins
	@param[in] Dir The direction the ray is pointing
*/
GRAPI void GRCC grRay_Set(grRay *Ray, grVec3d *Origin, grVec3d *Dir);

/*!
	@fn void grRay_Get(const grRay *Ray, grVec3d *Origin, grVec3d *Dir)
	@brief Gets the ray's data
	@param[in] Ray The ray to query
	@param[out] Origin The point at which the ray begins
	@param[out] Dir The direction the ray is pointing
*/
GRAPI void GRCC grRay_Get(const grRay *Ray, grVec3d *Origin, grVec3d *Dir);

/*!
	@fn grBoolean grRay_IntersectsWithTriangle(const grRay *Ray, const grVec3d *V1, const grVec3d *V2, const grVec3d *V3, grBoolean Cull, float *T, grVec3d *Impact)
	@brief Checks if a ray is intersecting with a triangle and returns the impact point
	@param[in] Ray The ray to query
	@param[in] V1 A triangle vertex
	@param[in] V2 A triangle vertex
	@param[in] V3 A triangle vertex
	@param[in] Cull Flag to tell the function to cull the ray at the impact point
	@param[out] T The distance from the ray's origin to the point of intersection
	@param[out] Impact The point of impact
	@return GR_TRUE if a collision occurred, GR_FALSE if not
*/
GRAPI grBoolean GRCC grRay_IntersectsWithTriangle(const grRay *Ray, const grVec3d *V1, const grVec3d *V2, const grVec3d *V3, grBoolean Cull, float *T, grVec3d *Impact);

/*!
	@fn grBoolean grRay_IntersectsWithPlane(const grRay *Ray, const grPlane *Plane, grBoolean Cull, float *T, grVec3d *Impact)
	@brief Checks if a ray intersects with a plane
	@param[in] Ray The ray to check with
	@param[in] Plane The plane to check against
	@param[in] Cull Flag to tell the function to cull the ray at the point of impact
	@param[out] T The distance from the ray's origin to the impact point
	@param[out] Impact The impact point
	@return GR_TRUE if a collision occurred, GR_FALSE if not
*/
GRAPI grBoolean GRCC grRay_IntersectsWithPlane(const grRay *Ray, const grPlane *Plane, grBoolean Cull, float *T, grVec3d *Impact);

/*!
	@fn grBoolean grRay_IntersectsWithExtBox(const grRay *Ray, const grExtBox *ExtBox, grBoolean Cull, float *T, grVec3d *Impact)
	@brief Checks if a ray intersects with an axis-aligned bounding box
	@param[in] Ray The ray to check with
	@param[in] ExtBox The bounding box to check against
	@param[in] Cull Flag to tell the function to cull the ray at the point of impact
	@param[out] T The distance from the ray's origin to the point of impact
	@param[out] Impact The impact point
	@return GR_TRUE if a collision occurred, GR_FALSE if not
*/
GRAPI grBoolean GRCC grRay_IntersectsWithExtBox(const grRay *Ray, const grExtBox *ExtBox, grBoolean Cull, float *T, grVec3d *Impact);


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
