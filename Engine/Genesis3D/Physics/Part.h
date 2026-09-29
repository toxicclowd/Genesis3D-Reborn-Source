/****************************************************************************************/
/*  PART.H                                                                              */
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

#ifndef PART_H
#define PART_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	GR_PARTICLE_FLAGS_NONE											= 1 << 0,
	GR_PARTICLE_FLAGS_COLLIDE_RIGIDBODY					= 1 << 1,
	GR_PARTICLE_FLAGS_COLLIDE_BARRIER						= 1 << 2

}grParticle_Flags;

typedef struct grParticle grParticle;

typedef grBoolean (*grParticle_IntegratorFunc)(grParticle* part, float dt);

// built-in integrator functions
grBoolean grParticle_IntegratorFunc_EulerStep(grParticle* part, float dt);
grBoolean grParticle_IntegratorFunc_EulerMidPoint1(grParticle* part, float dt);

/////////////////////////////////////////////////////////////////////////////////
// ctor / dtor
grParticle* grParticle_Create(float mass, grVec3d* p, grVec3d* v, grParticle_Flags flags,
	grParticle_IntegratorFunc integratorFunc);
void grParticle_Destroy(grParticle** part);

/////////////////////////////////////////////////////////////////////////////////
// accessors

float grParticle_GetMass(const grParticle* part);
float grParticle_GetOneOverMass(const grParticle* part);
grBoolean grParticle_GetPos(const grParticle* part, grVec3d* pos);
grBoolean grParticle_GetVel(const grParticle* part, grVec3d* vel);
grBoolean grParticle_GetAcc(const grParticle* part, grVec3d* acc);
grParticle_IntegratorFunc grParticle_GetIntegratorFunc(const grParticle* part);
float grParticle_GetTime(const grParticle* part);
grParticle_Flags grParticle_GetFlags(const grParticle* part);

grBoolean grParticle_SetPos(grParticle* part, const grVec3d* pos);
grBoolean grParticle_SetMass(grParticle* part, float mass);
grBoolean grParticle_SetVel(grParticle* part, const grVec3d* vel);
grBoolean grParticle_SetIntegratorFunc(grParticle* part, grParticle_IntegratorFunc func);
grBoolean grParticle_SetFlags(grParticle* part, grParticle_Flags flags);

/////////////////////////////////////////////////////////////////////////////////
// fns

grBoolean grParticle_ClearAcc(grParticle* part);
grBoolean grParticle_AddForce(grParticle* part, const grVec3d* pForce);
grBoolean grParticle_AddAcc(grParticle* part, const grVec3d* pAcc);
grBoolean grParticle_UpdateTime(grParticle* part, float dt);

#ifdef __cplusplus
}
#endif

#endif
