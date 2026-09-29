/****************************************************************************************/
/*  UVMAP.H                                                                             */
/*                                                                                      */
/*  Author:                                                                             */
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

#ifndef UVMAP_H
#define UVMAP_H

#include "grTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**

	built-in function pointers:

	grUVMap_Reflection
	grUVMap_Refraction
	grUVMap_Projection

**/

///////////////////////////////////////////////////////////////////////////////////////
// CB for an array mapper

typedef grBoolean (GRCC *grUVMapper) (const grXForm3d* pXForm,
	grLVertex* pVerts,const grVec3d* pNormals, int nVerts);

typedef grBoolean (*grUVMapVertex) (const grXForm3d* pXForm,
	grVertex* pVerts,int nVerts);

///////////////////////////////////////////////////////////////////////////////////////
// functions

// these functions take an array of GR_LVertices and normals
GRAPI grBoolean GRCC grUVMap_Reflection(const grXForm3d* pXForm, grLVertex* pVerts, const grVec3d* pNormals, int nverts);
									// xform is (transpose of) camera xform
GRAPI grBoolean GRCC grUVMap_Refraction(const grXForm3d* pXForm, grLVertex* pVerts, const grVec3d* pNormals, int nverts);
GRAPI grBoolean GRCC grUVMap_Projection(const grXForm3d* pXForm, grLVertex* pVerts, const grVec3d* pNormals, int nverts);

#ifdef __cplusplus
}
#endif

#endif

