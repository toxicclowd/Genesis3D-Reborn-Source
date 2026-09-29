/****************************************************************************************/
/*  UVMAP.C                                                                             */
/*                                                                                      */
/*  Author:  Jason Wood and Charles Bloom                                               */
/*  Description:  View-dependent mapping functions for Jet3D 2.0                        */
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

#include <math.h>
#include <assert.h>

#include "grTypes.h"
#include "Vec3d.h"
#include "Xform3d.h"
#include "UVMap.h"


#pragma message("uvmap : still todos in the mappers")
//maybe should use some kinda scaling factor for u's and v's in spherical formulations
//since both lie in range ~[0.15 .. 0.85]?

static void DoNadaTestFunc(void)
{
	static grUVMapper MyMapper;

	MyMapper = grUVMap_Reflection;
}

GRAPI grBoolean GRCC grUVMap_Reflection(const grXForm3d* pXForm, GR_LVertex* pVerts, const grVec3d* pNormals, int nverts)
{
	int i;
	grVec3d vv, vr;
	grVec3d surfNormal;
	float dot, m;
	grVec3d worldPt;

	assert(pXForm);
	assert(pVerts);
	assert(pNormals);
	assert(nverts >= 0);

	// xform world coords to camera coordinates	
	for (i = 0; i < nverts; i ++)
	{
		worldPt = *((grVec3d*)&pVerts[i]);

		grXForm3d_Transform(pXForm, &worldPt, &vv);
		grXForm3d_Rotate(pXForm, &pNormals[i], &surfNormal);
		
		grVec3d_Normalize(&vv);
		dot = 2.f * grVec3d_DotProduct(&vv, &surfNormal);

		vr.X = dot * surfNormal.X - vv.X;
		vr.Y = dot * surfNormal.Y - vv.Y;
		vr.Z = dot * surfNormal.Z - vv.Z;

		m = 2.f * (float)sqrt(vr.Z * vr.Z + 2 * vr.Z + 2.f);

		pVerts[i].u = vr.X / m + 0.5f;
		pVerts[i].v = vr.Y / m + 0.5f;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grUVMap_Refraction(const grXForm3d* pXForm, GR_LVertex* pVerts, const grVec3d* pNormals, int nverts)
{
	int i;
	grVec3d vv, vr;
	grVec3d surfNormal;
	float dot, m;
	grVec3d worldPt;

	assert(pXForm);
	assert(pVerts);
	assert(pNormals);
	assert(nverts >= 0);

	// xform world coords to camera coordinates	
	for (i = 0; i < nverts; i ++)
	{
		worldPt = *((grVec3d*)&pVerts[i]);

		grXForm3d_Transform(pXForm, &worldPt, &vv);
		grXForm3d_Rotate(pXForm, &pNormals[i], &surfNormal);
		
		grVec3d_Normalize(&vv);
		dot = 2.f * grVec3d_DotProduct(&vv, &surfNormal);
		vr.X = vv.X + dot * surfNormal.X;
		vr.Y = vv.Y + dot * surfNormal.Y;
		vr.Z = vv.Z + dot * surfNormal.Z;

		m = 2.f * (float)sqrt(vr.Z * vr.Z + 2 * vr.Z + 2.f);

		pVerts[i].u = vr.X / m + 0.5f;
		pVerts[i].v = vr.Y / m + 0.5f;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grUVMap_Projection(const grXForm3d* pXForm, GR_LVertex* pVerts, const grVec3d* pNormals, int nverts)
{
	int i;
	grVec3d vv;
	grVec3d worldPt;

	assert(pXForm);
	assert(pVerts);
	assert(nverts >= 0);

	for (i = 0; i < nverts; i ++)
	{
		worldPt = *((grVec3d*)&pVerts[i]);

		grXForm3d_Transform(pXForm, &worldPt, &vv);

		pVerts[i].u = vv.X;
		pVerts[i].v = vv.Y;
	}

	return GR_TRUE;
}
