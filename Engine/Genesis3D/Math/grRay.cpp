/*!
  @file grRay.cpp

  @author Anthony Rufrano (paradoxnj)
  @brief Ray casting code
                                                                   
  The contents of this file are subject to the Jet3D Public License
  Version 1.02 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.jet3d.com
                                                                     
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
  the License for the specific language governing rights and limitations
  under the License.
                                 
  The Original Code is Jet3D, released December 12, 1999.
  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved
*/

#include <assert.h>
#include <math.h>
#include "grRay.h"

GRAPI void GRCC grRay_Set(grRay *Ray, grVec3d *Origin, grVec3d *Dir)
{
	assert(Ray != NULL);
	assert(Origin != NULL);
	assert(Dir != NULL);

	grVec3d_Copy(Origin, &Ray->Origin);
	grVec3d_Copy(Dir, &Ray->Direction);
}

GRAPI void GRCC grRay_Get(const grRay *Ray, grVec3d *Origin, grVec3d *Dir)
{
	assert(Ray != NULL);
	assert(Origin != NULL);
	assert(Dir != NULL);

	grVec3d_Copy(&Ray->Origin, Origin);
	grVec3d_Copy(&Ray->Direction, Dir);
}

GRAPI grBoolean GRCC grRay_IntersectsWithTriangle(const grRay *Ray, const grVec3d *V1, const grVec3d *V2, const grVec3d *V3, grBoolean Cull, float *T, grVec3d *Impact)
{
	grVec3d					pvec, tvec, qvec;
	grVec3d					edge1, edge2;
	float					det, u, v;

	grVec3d_Subtract(V2, V1, &edge1);
	grVec3d_Subtract(V3, V1, &edge2);

	grVec3d_CrossProduct(&Ray->Direction, &edge2, &pvec);

	det = grVec3d_DotProduct(&edge1, &pvec);
	if (Cull && (det < 0.0001f))
		return GR_FALSE;
	else if ((det < 0.0001f) && (det > -0.0001f))
		return GR_FALSE;

	//Calc distance to plane.  Less than 0 means the ray is behind the plane.
	grVec3d_Subtract(&Ray->Origin, V1, &tvec);
	u = grVec3d_DotProduct(&tvec, &pvec);
	if (u < 0.0f || u > det)
		return GR_FALSE;

	grVec3d_CrossProduct(&tvec, &edge1, &qvec);
	v = grVec3d_DotProduct(&Ray->Origin, &qvec);
	if (v < 0.0f || (u + v) > det)
		return GR_FALSE;

	if (T != NULL)
	{
		float				invDet;

		*T = grVec3d_DotProduct(&edge2, &qvec);
		invDet = 1.0f / det;
		*T *= invDet;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grRay_IntersectsWithPlane(const grRay *Ray, const grPlane *Plane, grBoolean Cull, float *T, grVec3d *Impact)
{
	float						Vd, Vo, _t;

	Vd = grVec3d_DotProduct(&Plane->Normal, &Ray->Direction);
	if (fabs(Vd) < 0.00001f)
		return GR_FALSE;

	if (Cull && (Vd > 0.0f))
		return GR_FALSE;

	Vo = -((grVec3d_DotProduct(&Plane->Normal, &Ray->Origin) + Plane->Dist));
	_t = Vo / Vd;
	if (_t < 0.0f)
		return GR_FALSE;

	if (Impact)
		grVec3d_AddScaled(&Ray->Origin, &Ray->Direction, _t, Impact);

	if (T)
		*T = _t;

	return GR_TRUE;
}

GRAPI grBoolean GRCC grRay_InterestsWithExtBox(const grRay *Ray, const grExtBox *ExtBox, grBoolean Cull, float *T, grVec3d *Impact)
{
	grVec3d					MaxT;
	grBoolean				Inside = GR_TRUE;
	float					plane;

	grVec3d_Set(&MaxT, -1.0f, -1.0f, -1.0f);

	if (Ray->Origin.X < ExtBox->Min.X)
	{
		Impact->X = ExtBox->Min.X;
		Inside = GR_FALSE;
		if (Ray->Direction.X != 0.0f)
			MaxT.X = (ExtBox->Min.X - Ray->Origin.X) / Ray->Direction.X;
	}
	else if (Ray->Origin.X > ExtBox->Max.X)
	{
		Impact->X = ExtBox->Max.X;
		Inside = GR_FALSE;
		if (Ray->Direction.X != 0.0f)
			MaxT.X = (ExtBox->Max.X - Ray->Origin.X) / Ray->Direction.X;
	}
	
	if (Ray->Origin.Y < ExtBox->Min.Y)
	{
		Impact->Y = ExtBox->Min.Y;
		Inside = GR_FALSE;
		if (Ray->Direction.Y != 0.0f)
			MaxT.Y = (ExtBox->Min.Y - Ray->Origin.Y) / Ray->Direction.Y;
	}
	else if (Ray->Origin.Y > ExtBox->Max.Y)
	{
		Impact->Y = ExtBox->Max.Y;
		Inside = GR_FALSE;
		if (Ray->Direction.Y != 0.0f)
			MaxT.Y = (ExtBox->Max.Y - Ray->Origin.Y) / Ray->Direction.Y;
	}

	if (Ray->Origin.Z < ExtBox->Min.Z)
	{
		Impact->Z = ExtBox->Min.Z;
		Inside = GR_FALSE;
		if (Ray->Direction.Z != 0.0f)
			MaxT.Z = (ExtBox->Min.Z - Ray->Origin.Z) / Ray->Direction.Z;
	}
	else if (Ray->Origin.Z > ExtBox->Max.Z)
	{
		Impact->Z = ExtBox->Max.Z;
		Inside = GR_FALSE;
		if (Ray->Direction.Z != 0.0f)
			MaxT.Z = (ExtBox->Max.Z - Ray->Origin.Z) / Ray->Direction.Z;
	}

	if (Inside)
	{
		grVec3d_Copy(&Ray->Origin, Impact);
		return GR_TRUE;
	}

	plane = MaxT.X;

	if (MaxT.Y > MaxT.X)
		plane = MaxT.Y;
	
	if (MaxT.Z > MaxT.X)
		plane = MaxT.Z;

	if (plane < 0.0f)
		return GR_FALSE;

	if (plane == MaxT.X)
	{
		Impact->X = Ray->Origin.X + MaxT.X * Ray->Direction.X;
		if ((Impact->X < ExtBox->Min.X - 0.00001f) || (Impact->X < ExtBox->Max.X + 0.00001f))
			return GR_FALSE;
	}
	else if (plane == MaxT.Y)
	{
		Impact->Y = Ray->Origin.Y + MaxT.Y * Ray->Direction.Y;
		if ((Impact->Y < ExtBox->Min.Y - 0.00001f) || (Impact->Y < ExtBox->Max.Y + 0.00001f))
			return GR_FALSE;
	}
	else if (plane == MaxT.Z)
	{
		Impact->Z = Ray->Origin.Z + MaxT.Z * Ray->Direction.Z;
		if ((Impact->Z < ExtBox->Min.Z - 0.00001f) || (Impact->Z < ExtBox->Max.Z + 0.00001f))
			return GR_FALSE;
	}

	return GR_TRUE;
}
