/****************************************************************************************/
/*  SPRING.C                                                                            */
/*                                                                                      */
/*  Author:  Jason Wood                                                                 */
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

#include "BaseType.h"
#include "Ram.h"
#include "Vec3d.h"

#include "Part.h"
#include "Spring.h"

typedef struct grSpring
{
	grParticle* p1, *p2;
	float Ks, Kd; // spring and damping constants
	float r0; // initial length

	grSpring_ForceFunc forceFunc;

}grSpring;

static void grSpring_ComputeR0(grSpring* pSpring)
{
	grVec3d pos1, pos2, diff;

	grParticle_GetPos(pSpring->p1, &pos1);
	grParticle_GetPos(pSpring->p2, &pos2);

	grVec3d_Subtract(&pos2, &pos1, &diff);
	pSpring->r0 = grVec3d_Length(&diff);

	assert(pSpring->r0 > GR_EPSILON);
}

/////////////////////////////////////////////////////////////////////////////////
// ctor / dtor

grSpring* grSpring_Create(float Ks, float Kd, grParticle* p1, grParticle* p2,
	grSpring_ForceFunc forceFunc)
{
	grSpring* pSpring;

	assert(p1);
	assert(p2);

	assert(Ks > GR_EPSILON);
	assert(Kd > GR_EPSILON && Kd <= 1.f);
	assert(forceFunc);

	pSpring = (grSpring*)grRam_Allocate(sizeof(grSpring));

	pSpring->Ks = Ks;
	pSpring->Kd = Kd;
	pSpring->p1 = p1;
	pSpring->p2 = p2;
	pSpring->forceFunc = forceFunc;

	grSpring_ComputeR0(pSpring);

	return pSpring;
}

void grSpring_Destroy(grSpring** ppSpring)
{
	assert(ppSpring);
	assert(*ppSpring);

	grRam_Free(*ppSpring);
	*ppSpring = NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// accessors

grParticle* grSpring_GetPart1(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->p1;
}

grParticle* grSpring_GetPart2(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->p2;
}

float grSpring_GetKs(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->Ks;
}

float grSpring_GetKd(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->Kd;
}

float grSpring_GetR0(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->r0;
}

grSpring_ForceFunc grSpring_GetForceFunc(const grSpring* pSpring)
{
	assert(pSpring);

	return pSpring->forceFunc;
}

/////////////////////////////////////////////////////////////////////////////////

grBoolean grSpring_SetPart1(grSpring* pSpring, const grParticle* part1)
{
	assert(pSpring);
	assert(part1);

	pSpring->p1 = (grParticle*)part1;

	grSpring_ComputeR0(pSpring);

	return GR_TRUE;
}

grBoolean grSpring_SetPart2(grSpring* pSpring, const grParticle* part2)
{
	assert(pSpring);
	assert(part2);

	pSpring->p2 = (grParticle*)part2;

	grSpring_ComputeR0(pSpring);

	return GR_TRUE;
}

grBoolean grSpring_SetKs(grSpring* pSpring, float Ks)
{
	assert(pSpring);
	assert(Ks > GR_EPSILON);

	pSpring->Ks = Ks;

	return GR_TRUE;
}

grBoolean grSpring_SetKd(grSpring* pSpring, float Kd)
{
	assert(pSpring);
	assert(Kd > GR_EPSILON && Kd <= 1.f);

	pSpring->Kd = Kd;

	return GR_TRUE;
}

grBoolean grSpring_SetForceFunc(grSpring* pSpring, grSpring_ForceFunc forceFunc)
{
	assert(pSpring);
	assert(forceFunc);

	pSpring->forceFunc = forceFunc;

	return GR_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////
// functions

// built-in force functions

// compute forces for particles attached to damped spring.
// our constraint function reads C = |x1 - x2| - r0 = 0.
grBoolean grSpring_ForceFunc_ComputeDamped(grSpring* pSpring)
{
	grVec3d pos1, pos2, dpos;
	grVec3d v1, v2, dv;
	grVec3d force;

	float len, C, Cdot, F;

	assert(pSpring);

	grParticle_GetPos(pSpring->p1, &pos1);
	grParticle_GetPos(pSpring->p2, &pos2);
	grVec3d_Subtract(&pos1, &pos2, &dpos);
	len = grVec3d_Normalize(&dpos);

	C = len - pSpring->r0;

	grParticle_GetVel(pSpring->p1, &v1);
	grParticle_GetVel(pSpring->p2, &v2);
	grVec3d_Subtract(&v1, &v2, &dv);

	// Cdot = dC / dt = (dC / dx) * (dx / dt) =
	// ((x1 - x2) / |l|, (dx1 / dt - dx2 / dt))
	Cdot = grVec3d_DotProduct(&dpos, &dv);

	// F = -(Ks * C + Kd * dC / Dt)
	F = -(pSpring->Ks * C + pSpring->Kd * Cdot);

	force.X = F * dpos.X;
	force.Y = F * dpos.Y;
	force.Z = F * dpos.Z;

	grParticle_AddForce(pSpring->p1, &force);

	grVec3d_Inverse(&force);
	grParticle_AddForce(pSpring->p2, &force);

	return GR_TRUE;
}

grBoolean grSpring_ForceFunc_ComputeCriticallyDamped(grSpring* pSpring)
{
	assert(pSpring);

	return GR_TRUE;
}
