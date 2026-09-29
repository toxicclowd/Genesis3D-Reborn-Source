/****************************************************************************************/
/*  PART.C                                                                              */
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

typedef struct grParticle
{
	grVec3d p, v, a; // pos, vel, acc
	float mass, oneOverMass;

	grParticle_Flags flags;

	grParticle_IntegratorFunc integratorFunc;

	float t; // time particle has been alive

}grParticle;


/////////////////////////////////////////////////////////////////////////////////
// ctor / dtor

grParticle* grParticle_Create(float mass, grVec3d* p, grVec3d* v, grParticle_Flags flags,
	grParticle_IntegratorFunc integratorFunc)
{
	grParticle* part;

	assert(p);
	assert(v);

	part = (grParticle*)grRam_Allocate(sizeof(grParticle));
	if (! part) return NULL;

	part->mass = mass;
	if (part->mass < GR_EPSILON)
		part->oneOverMass = 0.f;
	else
		part->oneOverMass = 1 / mass;

	part->p = *p;
	part->v = *v;

	part->flags = flags;

	part->integratorFunc = integratorFunc;

	return part;
}

void grParticle_Destroy(grParticle** part)
{
	assert(part);
	assert(*part);

	grRam_Free(*part);
	*part = NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// accessors

float grParticle_GetMass(const grParticle* part)
{
	assert(part);

	return part->mass;
}

float grParticle_GetOneOverMass(const grParticle* part)
{
	assert(part);

	return part->oneOverMass;
}

grBoolean grParticle_GetPos(const grParticle* part, grVec3d* pos)
{
	assert(part);
	assert(pos);

	*pos = part->p;

	return GR_TRUE;
}

grBoolean grParticle_GetVel(const grParticle* part, grVec3d* vel)
{
	assert(part);
	assert(vel);

	*vel = part->v;

	return GR_TRUE;
}

grBoolean grParticle_GetAcc(const grParticle* part, grVec3d* acc)
{
	assert(part);
	assert(acc);

	*acc = part->a;

	return GR_TRUE;
}

grParticle_IntegratorFunc grParticle_GetIntegratorFunc(const grParticle* part)
{
	assert(part);

	return part->integratorFunc;
}

float grParticle_GetTime(const grParticle* part)
{
	assert(part);

	return part->t;
}

grParticle_Flags grParticle_GetFlags(const grParticle* part)
{
	assert(part);

	return part->flags;
}

/////////////////////////////////////////////////////////////////////////////////

grBoolean grParticle_SetMass(grParticle* part, float mass)
{
	assert(part);

	part->mass = mass;
	if (part->mass < GR_EPSILON)
		part->oneOverMass = 0.f;
	else
		part->oneOverMass = 1 / mass;
	
	return GR_TRUE;
}

grBoolean grParticle_SetPos(grParticle* part, const grVec3d* pos)
{
	assert(part);
	assert(pos);

	part->p = *pos;

	return GR_TRUE;
}

grBoolean grParticle_SetVel(grParticle* part, const grVec3d* vel)
{
	assert(part);
	assert(vel);

	part->v = *vel;

	return GR_TRUE;
}

grBoolean grParticle_SetIntegratorFunc(grParticle* part, grParticle_IntegratorFunc func)
{
	assert(part);

	part->integratorFunc = func;

	return GR_TRUE;
}

grBoolean grParticle_SetFlags(grParticle* part, grParticle_Flags flags)
{
	assert(part);

	part->flags = flags;

	return GR_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////
// fns

grBoolean grParticle_ClearAcc(grParticle* part)
{
	assert(part);

	grVec3d_Clear(&part->a);

	return GR_TRUE;
}

grBoolean grParticle_AddForce(grParticle* part, const grVec3d* pForce)
{
	assert(part);
	assert(pForce);

	part->a.X += part->oneOverMass * pForce->X;
	part->a.Y += part->oneOverMass * pForce->Y;
	part->a.Z += part->oneOverMass * pForce->Z;

	return GR_TRUE;
}

grBoolean grParticle_AddAcc(grParticle* part, const grVec3d* pAcc)
{
	assert(part);
	assert(pAcc);

	part->a.X += pAcc->X;
	part->a.Y += pAcc->Y;
	part->a.Z += pAcc->Z;

	return GR_TRUE;
}

grBoolean grParticle_UpdateTime(grParticle* part, float dt)
{
	assert(part);
	assert(dt > GR_EPSILON);

	part->t += dt;

	return GR_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////
// integrator functions

grBoolean grParticle_IntegratorFunc_EulerStep(grParticle* part, float dt)
{
	grVec3d dp, dv;

	assert(part);
	assert(dt > GR_EPSILON);

	dv.X = dt * part->a.X;
	dv.Y = dt * part->a.Y;
	dv.Z = dt * part->a.Z;

	dp.X = dt * part->v.X + dv.X;
	dp.Y = dt * part->v.Y + dv.Y;
	dp.Z = dt * part->v.Z + dv.Z;

	part->p.X += dp.X;
	part->p.Y += dp.Y;
	part->p.Z += dp.Z;

	part->v.X += dv.X;
	part->v.Y += dv.Y;
	part->v.Z += dv.Z;

	return GR_TRUE;
}

// takes 2 Euler steps, but is more accurate
grBoolean grParticle_IntegratorFunc_EulerMidPoint1(grParticle* part, float dt)
{
	grParticle midPart, tmpPart;

	assert(part);
	assert(dt > GR_EPSILON);

	tmpPart.a = part->a;
	tmpPart.v = part->v;
	tmpPart.p = part->v;

	grParticle_IntegratorFunc_EulerStep(&tmpPart, dt);
	
	midPart.a = tmpPart.a;

	midPart.v.X = 0.5f * (part->v.X + tmpPart.v.X);
	midPart.v.Y = 0.5f * (part->v.Y + tmpPart.v.Y);
	midPart.v.Z = 0.5f * (part->v.Z + tmpPart.v.Z);

	midPart.p.X = 0.5f * (part->p.X + tmpPart.p.X);
	midPart.p.Y = 0.5f * (part->p.Y + tmpPart.p.Y);
	midPart.p.Z = 0.5f * (part->p.Z + tmpPart.p.Z);

	grParticle_IntegratorFunc_EulerStep(&midPart, 0.5f * dt);

	part->v = midPart.v;
	part->p = midPart.p;

	return GR_TRUE;
}
