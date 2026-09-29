/****************************************************************************************/
/*  JEPARTICLE.H                                                                        */
/*                                                                                      */
/*  Author:                                                                             */
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
#ifndef	GR_PARTICLE_H
#define	GR_PARTICLE_H

#include "GRWORLD.H"
#include "grTypes.h"
#include "Bitmap.h"
#include "Vec3d.h"

#ifdef	__cplusplus
extern "C" {
#endif


////////////////////////////////////////////////////////////////////////////////////////
//	Particle system structs
////////////////////////////////////////////////////////////////////////////////////////
typedef struct jeParticle grParticle;
typedef struct jeParticle jeParticle;
typedef struct jeParticle_System grParticle_System;
typedef struct jeParticle_System jeParticle_System;


typedef struct jeMaterialSpec		grMaterialSpec;


////////////////////////////////////////////////////////////////////////////////////////
//	Prototypes
////////////////////////////////////////////////////////////////////////////////////////

//	Add a new paticle to the particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grParticle_SystemAddParticle(
	grParticle_System	*ps,			// particle system to add it to
	const grWorld		*World,			// world to add it to
	grMaterialSpec		*Texture,		// texture to use
	const grLVertex		*Vert,			// vert info
	const grVec3d		*AnchorPoint,	// anchor point
	float				Time,			// how many seconds it will last
	const grVec3d		*Velocity,		// velocity
	float				Scale,			// art scale
	const grVec3d		*Gravity );		// pull of gravity

//	Removes all references to an anchor point.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grParticle_SystemRemoveAnchorPoint(
	grParticle_System	*ps,				// particle system from which this anchor point will be removed
	grVec3d				*AnchorPoint );		// the anchor point to remove

//	Process a frame of the particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemFrame(
	grParticle_System	*ps,			// particle system to process
	float				DeltaTime );	// amount of elaped seconds

//	Create a particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grParticle_System * GRCC grParticle_SystemCreate(
	float	MajorVersion,	// major version number of calling app
	float	MinorVersion );	// minor version number of calling app

//	Destroy a particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemDestroy(
	grParticle_System	*ps );	// particle system to destroy

//	Return the current number of active particles.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grParticle_GetCount(
	grParticle_System	*ps );	// particle system whose particle count we want

//	Destroy all particles.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemRemoveAll(
	grParticle_System	*ps );	// particle system whose particles will all be destroyed


#ifdef	__cplusplus
}
#endif

//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define jeParticle_GetCount                      grParticle_GetCount
#define jeParticle_SystemAddParticle             grParticle_SystemAddParticle
#define jeParticle_SystemCreate                  grParticle_SystemCreate
#define jeParticle_SystemDestroy                 grParticle_SystemDestroy
#define jeParticle_SystemFrame                   grParticle_SystemFrame
#define jeParticle_SystemRemoveAll               grParticle_SystemRemoveAll
#define jeParticle_SystemRemoveAnchorPoint       grParticle_SystemRemoveAnchorPoint

#endif // GENESIS_NO_JET_COMPAT

#endif
