/****************************************************************************************/
/*  JEPORTAL.H                                                                          */
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

#ifndef JEPORTAL_H
#define JEPORTAL_H

#include "grPoly.h"
#include "grPlane.h"
#include "grFrustum.h"
#include "Camera.h"
#include "grProperty.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grPortal grPortal;

typedef grBoolean GRCC grPortal_RenderFunc(grPortal *Portal, const grPlane *Plane, const grXForm3d *FaceXForm, void *Context, grCamera *Camera, grFrustum *Frustum);

typedef struct grPortal
{
	int32				RefCount;

	grXForm3d			XForm;
	int32				Recursion;
	grPortal_RenderFunc	*RenderFunc;
} grPortal;

GRAPI grPortal	* GRCC grPortal_Create();
GRAPI grBoolean	GRCC grPortal_CreateRef(grPortal *Portal);
GRAPI void			GRCC grPortal_Destroy(grPortal **Portal);
GRAPI grBoolean	GRCC grPortal_IsValid(const grPortal *Portal);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
