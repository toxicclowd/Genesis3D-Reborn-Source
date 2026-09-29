/****************************************************************************************/
/*  JEUSERPOLY.H                                                                        */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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

#ifndef GR_USERPOLY_H
#define GR_USERPOLY_H

#include "Engine.h"
#include "BaseType.h"
#include "grTypes.h"
#include "Bitmap.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef struct grUserPoly grUserPoly;
typedef struct grMaterialSpec grMaterialSpec;


typedef enum
{
	Type_Line,
	Type_Tri,
	Type_Quad, 
	Type_Sprite
} grUserPoly_Type;
typedef grUserPoly_Type grUserPoly_Type;


//========================================================================================
//	Structure defs
//========================================================================================
GRAPI grUserPoly	* GRCC grUserPoly_CreateTri(	const grLVertex		*v1, 
												const grLVertex		*v2, 
												const grLVertex		*v3, 
												const grMaterialSpec *Material,
												uint32				Flags);
GRAPI grUserPoly	* GRCC grUserPoly_CreateQuad(	const grLVertex		*v1, 
												const grLVertex		*v2, 
												const grLVertex		*v3, 
												const grLVertex		*v4, 
												const grMaterialSpec *Material,
												uint32				Flags);
GRAPI grUserPoly	* GRCC grUserPoly_CreateSprite(	const grLVertex		*v1, 
												const grMaterialSpec *Material,
													grFloat				Scale,
													uint32				Flags);
GRAPI grUserPoly	* GRCC grUserPoly_CreateLine(const grLVertex *v1, const grLVertex *v2, grFloat Scale, uint32 Flags);

GRAPI grBoolean	GRCC grUserPoly_IsValid(const grUserPoly *Poly);
GRAPI grBoolean	GRCC grUserPoly_CreateRef(grUserPoly *Poly);
GRAPI void			GRCC grUserPoly_Destroy(grUserPoly **Poly);
GRAPI grBoolean	GRCC grUserPoly_UpdateTri(	grUserPoly *Poly, 
												const grLVertex *v1, 
												const grLVertex *v2, 
												const grLVertex *v3, 
												const grMaterialSpec *Material);

GRAPI grBoolean	GRCC grUserPoly_UpdateQuad(	grUserPoly *Poly, 
												const grLVertex *v1, 
												const grLVertex *v2, 
												const grLVertex *v3, 
												const grLVertex *v4, 
												const grMaterialSpec *Material);

GRAPI grBoolean	GRCC grUserPoly_UpdateSprite(grUserPoly *Poly, const grLVertex *v1, const grMaterialSpec *Material, grFloat Scale);

GRAPI grBoolean	GRCC grUserPoly_UpdateLine(grUserPoly *Poly, const grLVertex *v1, const grLVertex *v2, grFloat Scale);

GRAPI grBoolean	GRCC grUserPoly_Render(const grUserPoly *Poly, const grEngine *Engine, const grCamera *Camera, const grFrustum *Frustum);

//========================================================================================
//	Function prototypes
//========================================================================================
#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
