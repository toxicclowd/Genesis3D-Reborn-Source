/****************************************************************************************/
/*  SPRING.H                                                                            */
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

#ifndef SPRING_H
#define SPRING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grSpring grSpring;

typedef grBoolean (*grSpring_ForceFunc)(grSpring* pSpring, float dt);

/////////////////////////////////////////////////////////////////////////////////
// ctor / dtor

grSpring* grSpring_Create(float Ks, float Kd, grParticle* p1, grParticle* p2,
	grSpring_ForceFunc forceFunc);
void grSpring_Destroy(grSpring** ppSpring);

/////////////////////////////////////////////////////////////////////////////////
// accessors

grParticle* grSpring_GetPart1(const grSpring* pSpring);
grParticle* grSpring_GetPart2(const grSpring* pSpring);
float grSpring_GetKs(const grSpring* pSpring);
float grSpring_GetKd(const grSpring* pSpring);
float grSpring_GetR0(const grSpring* pSpring);
grSpring_ForceFunc grSpring_GetForceFunc(const grSpring* pSpring);

grBoolean grSpring_SetPart1(grSpring* pSpring, const grParticle* part1);
grBoolean grSpring_SetPart2(grSpring* pSpring, const grParticle* part2);
grBoolean grSpring_SetKs(grSpring* pSpring, float Ks);
grBoolean grSpring_SetKd(grSpring* pSpring, float Kd);
grBoolean grSpring_SetForceFunc(grSpring* pSpring, grSpring_ForceFunc forceFunc);

/////////////////////////////////////////////////////////////////////////////////
// fns

grBoolean grSpring_ForceFunc_ComputeDamped(grSpring* pSpring);
grBoolean grSpring_ForceFunc_ComputeCriticallyDamped(grSpring* pSpring);

#ifdef __cplusplus
}
#endif

#endif
