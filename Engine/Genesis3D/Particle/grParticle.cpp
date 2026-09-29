/****************************************************************************************/
/*  JEPARTICLE.C                                                                        */
/*                                                                                      */
/*  Author:  Eli Boling & Peter Siamidis                                                */
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
#include <memory.h>
#include <assert.h>
#include "Ram.h"
#include "grUserPoly.h"
#include "grParticle.h"
#include "grVersion.h"


////////////////////////////////////////////////////////////////////////////////////////
//	Particle attricute flags
////////////////////////////////////////////////////////////////////////////////////////
#define	PARTICLE_HASVELOCITY	( 1 << 1 )
#define PARTICLE_HASGRAVITY		( 1 << 2 )


////////////////////////////////////////////////////////////////////////////////////////
//	grParticle struct
////////////////////////////////////////////////////////////////////////////////////////
typedef struct grParticle
{
	grLVertex		ptclVertex;
	grMaterialSpec	*ptclMaterial;
	grUserPoly		*ptclPoly;
	unsigned		ptclFlags;
	grParticle		*ptclNext;
	grParticle		*ptclPrev;
	float			Scale;
	grVec3d			Gravity;
	float			Alpha;
	grVec3d			CurrentAnchorPoint;
	const grVec3d	*AnchorPoint;
	float			ptclTime;
	float			ptclTotalTime;
	grVec3d			ptclVelocity;
	grWorld			*World;

} grParticle;


////////////////////////////////////////////////////////////////////////////////////////
//	Particle system struct
////////////////////////////////////////////////////////////////////////////////////////
typedef	struct grParticle_System
{
	grParticle	*psParticles;

} grParticle_System;



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemCreate()
//
//	Create a particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grParticle_System * GRCC grParticle_SystemCreate(
	float	MajorVersion,	// major version number of calling app
	float	MinorVersion )	// minor version number of calling app
{

	// locals
	grParticle_System	*ps;

	// fail if version numbers don't match
	if ( ( MajorVersion != GRT_MAJOR_VERSION ) || ( MinorVersion != GRT_MINOR_VERSION ) )
	{
		return NULL;
	}

	// allocate struct
	ps = (grParticle_System *)grRam_AllocateClear( sizeof( *ps ) );
	if ( ps == NULL )
	{
		return ps;
	}

	// all done
	return ps;

} // grParticle_SystemCreate()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_Destroy()
//
//	Destroy a particle.
//
////////////////////////////////////////////////////////////////////////////////////////
static void grParticle_Destroy(
	grParticle	*p )	// particle to destroy
{

	// destroy the poly
	if ( p->ptclPoly != NULL )
	{
		grWorld_RemoveUserPoly( p->World, p->ptclPoly );
		grUserPoly_Destroy( &( p->ptclPoly ) );
	}

	// free the struct
	grRam_Free( p );

} // grParticle_Destroy()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_Unlink()
//
//	Remove a particle from the list of particles.
//
////////////////////////////////////////////////////////////////////////////////////////
static void grParticle_Unlink(
	grParticle_System	*ps,		// particle system in which particle exists
	grParticle			*ptcl )		// particle to unlink
{

	// make list adjustments
	if ( ptcl->ptclPrev )
	{
		ptcl->ptclPrev->ptclNext = ptcl->ptclNext;
	}
	if ( ptcl->ptclNext )
	{
		ptcl->ptclNext->ptclPrev = ptcl->ptclPrev;
	}
	if ( ps->psParticles == ptcl )
	{
		ps->psParticles = ptcl->ptclNext;
	}

} // grParticle_Unlink()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_Create()
//
//	Create a new particle.
//
////////////////////////////////////////////////////////////////////////////////////////
static grParticle * grParticle_Create(
	grParticle_System	*ps,		// particle system to create it in
	grMaterialSpec		*Texture,	// texture to use
	const grLVertex		*Vert )		// vert data
{

	// locals
	grParticle	*ptcl;

	// allocate struct
	ptcl = (grParticle *)grRam_AllocateClear( sizeof( *ptcl ) );
	if ( ptcl == NULL )
	{
		return ptcl;
	}

	// init struct
	ptcl->ptclNext = ps->psParticles;
	ps->psParticles = ptcl;
	if ( ptcl->ptclNext )
	{
		ptcl->ptclNext->ptclPrev = ptcl;
	}
	ptcl->ptclMaterial = Texture;
	ptcl->ptclVertex = *Vert;

	// all done
	return ptcl;

} // grParticle_Create()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemDestroy()
//
//	Destroy a particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemDestroy(
	grParticle_System	*ps )	// particle system to destroy
{

	// locals
	grParticle	*ptcl;

	// zap all particles
	ptcl = ps->psParticles;
	while ( ptcl != NULL )
	{

		// locals
		grParticle	*temp;

		// destroy this particle
		temp = ptcl->ptclNext;
		grParticle_Destroy( ptcl );
		ptcl = temp;
	}

	// free particle system
	grRam_Free( ps );

} // grParticle_SystemDestroy()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemRemoveAll()
//
//	Destroy all particles.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemRemoveAll(
	grParticle_System	*ps )	// particle system whose particles will all be destroyed
{

	// locals
	grParticle	*ptcl;

	// zap all particles
	ptcl = ps->psParticles;
	while ( ptcl != NULL )
	{

		// locals
		grParticle	*temp;

		// kill this particle and go to next one
		temp = ptcl->ptclNext;
		grParticle_Unlink( ps, ptcl );
		grParticle_Destroy( ptcl );
		ptcl = temp;
	}

} // grParticle_SystemRemoveAll()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_GetCount()
//
//	Return the current number of active particles.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grParticle_GetCount(
	grParticle_System	*ps )	// particle system whose particle count we want
{

	// locals
	grParticle	*ptcl;
	int			TotalParticleCount = 0;

	// ensure valid data
	assert( ps != NULL );

	// count up how many particles are active in this particle system
	ptcl = ps->psParticles;
	while ( ptcl )
	{
		ptcl = ptcl->ptclNext;
		TotalParticleCount++;
	}

	// return the active count
	return TotalParticleCount;

} // grParticle_GetCount()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemFrame()
//
//	Process a frame of the particle system.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI void GRCC grParticle_SystemFrame(
	grParticle_System	*ps,			// particle system to process
	float				DeltaTime )		// amount of elaped seconds
{

	// locals
	grParticle	*ptcl;

	// process all particles
	ptcl = ps->psParticles;
	while ( ptcl != NULL )
	{

		// adjust particles life remaining
		ptcl->ptclTime -= DeltaTime;

		// destroy the particle..
		if ( ptcl->ptclTime <= 0.0f )
		{

			// locals
			grParticle	*temp;

			// destroy this particle
			temp = ptcl->ptclNext;
			grParticle_Unlink( ps, ptcl );
			grParticle_Destroy( ptcl );
			ptcl = temp;
			continue;
		}
		// ...or process it
		else
		{

			// locals
			grVec3d	DeltaPos = { 0.0f, 0.0f, 0.0f };

			// apply velocity
			if ( ptcl->ptclFlags & PARTICLE_HASVELOCITY )
			{
				grVec3d_Scale( &( ptcl->ptclVelocity ), DeltaTime, &DeltaPos );
			}

			// apply gravity
			if ( ptcl->ptclFlags & PARTICLE_HASGRAVITY )
			{

				// locals
				grVec3d	Gravity;

				// make gravity vector
				grVec3d_Scale( &( ptcl->Gravity ), DeltaTime, &Gravity );

				// apply gravity to built in velocity and DeltaPos
				grVec3d_Add( &( ptcl->ptclVelocity ), &Gravity, &( ptcl->ptclVelocity ) );
				grVec3d_Add( &DeltaPos, &Gravity, &DeltaPos );
			}
			
			// apply DeltaPos to particle position
			if (	( ptcl->ptclFlags & PARTICLE_HASVELOCITY ) ||
					( ptcl->ptclFlags & PARTICLE_HASGRAVITY ) )
			{
				grVec3d_Add( (grVec3d *)&( ptcl->ptclVertex.X ), &DeltaPos, (grVec3d *)&( ptcl->ptclVertex.X ) );
			}

			// make the particle follow its anchor point if it has one
			if ( ptcl->AnchorPoint != NULL )
			{

				// locals
				grVec3d	AnchorDelta;

				grVec3d_Subtract( ptcl->AnchorPoint, &( ptcl->CurrentAnchorPoint ), &AnchorDelta );
				grVec3d_Add( (grVec3d *)&( ptcl->ptclVertex.X ), &AnchorDelta, (grVec3d *)&( ptcl->ptclVertex.X ) );
				grVec3d_Copy( ptcl->AnchorPoint, &( ptcl->CurrentAnchorPoint ) );
			}

			// destroy the particle if it is in solid space
			//undone
		}

		// adjust particle alpha
		assert( ptcl->ptclTotalTime > 0.0f );
		assert( ptcl->ptclPoly != NULL );
		ptcl->ptclVertex.a = ptcl->Alpha * ( ptcl->ptclTime / ptcl->ptclTotalTime );
		assert( ptcl->ptclVertex.a >= 0.0f );
		assert( ptcl->ptclVertex.a <= 255.0f );
		grUserPoly_UpdateSprite( ptcl->ptclPoly, &ptcl->ptclVertex, ptcl->ptclMaterial, ptcl->Scale );

		// get next particle
		ptcl = ptcl->ptclNext;
	}

} // grParticle_SystemFrame()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemRemoveAnchorPoint()
//
//	Removes all references to an anchor point.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grParticle_SystemRemoveAnchorPoint(
	grParticle_System	*ps,				// particle system from which this anchor point will be removed
	grVec3d				*AnchorPoint )		// the anchor point to remove
{

	// locals	
	grParticle	*ptcl;
	grBoolean	AtLeastOneFound = GR_FALSE;

	// ensure valid data
	assert( ps != NULL );
	assert( AnchorPoint != NULL );

	// eliminate achnor point from all particles in this particle system
	ptcl = ps->psParticles;
	while ( ptcl != NULL )
	{
		if ( ptcl->AnchorPoint == AnchorPoint )
		{
			ptcl->AnchorPoint = NULL;
			AtLeastOneFound = GR_TRUE;
		}
		ptcl = ptcl->ptclNext;
	}

	// all done
	return AtLeastOneFound;

} // grParticle_SystemRemoveAnchorPoint()



////////////////////////////////////////////////////////////////////////////////////////
//
//	grParticle_SystemAddParticle()
//
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
	const grVec3d		*Gravity )		// pull of gravity
{

	// locals
	grParticle	*ptcl;

	// create a new particle
	ptcl = grParticle_Create( ps, Texture, Vert );
	if ( ptcl == NULL )
	{
		return GR_FALSE;
	}

	// setup gravity
	if ( Gravity != NULL )
	{
		grVec3d_Copy( Gravity, &( ptcl->Gravity ) );
		ptcl->ptclFlags |= PARTICLE_HASGRAVITY;
	}

	// setup velocity
	if ( Velocity != NULL )
	{
		grVec3d_Copy( Velocity, &( ptcl->ptclVelocity ) );
		ptcl->ptclFlags |= PARTICLE_HASVELOCITY;
	}

	// setup the anchor point
	if ( AnchorPoint != NULL )
	{
		grVec3d_Copy( AnchorPoint, &( ptcl->CurrentAnchorPoint ) );
		ptcl->AnchorPoint = AnchorPoint;
	}

	// setup remaining data
	ptcl->Scale = Scale;
	ptcl->ptclTime = Time;
	ptcl->ptclTotalTime = Time;
	ptcl->Alpha = Vert->a;
	ptcl->World = (grWorld *)World;

	// add the poly to the world
	ptcl->ptclPoly = grUserPoly_CreateSprite(	&ptcl->ptclVertex,
												ptcl->ptclMaterial,
												ptcl->Scale,
												GR_RENDER_FLAG_ALPHA | GR_RENDER_FLAG_NO_ZWRITE );
	if ( grWorld_AddUserPoly( ptcl->World, ptcl->ptclPoly, GR_FALSE ) == GR_FALSE )
	{
		grUserPoly_Destroy( &( ptcl->ptclPoly ) );
		return GR_FALSE;
	}

	// all done
	return GR_TRUE;

} // grParticle_SystemAddParticle()
