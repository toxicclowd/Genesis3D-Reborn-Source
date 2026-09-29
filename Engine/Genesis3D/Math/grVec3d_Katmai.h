/****************************************************************************************/
/*  grVec3d_Katmai.h                                                                    */
/*                                                                                      */
/*  Author: Anthony Rufrano                                                             */
/*  Description: Katmai (SSE) optimized vector math                                     */
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

#ifndef GR_VEC3D_KATMAI_H
#define GR_VEC3D_KATMAI_H

#include "BaseType.h"

typedef struct grVec3d					grVec3d;

grFloat									grVec3d_DotProduct_SSE(const grVec3d *v1, const grVec3d *v2);
void									grVec3d_CrossProduct_SSE(const grVec3d *v1, const grVec3d *v2, grVec3d *result);
void									grVec3d_Normalize_SSE(grVec3d *v1);
void									grVec3d_Scale_SSE(const grVec3d *v1, float scale, grVec3d *result);
grFloat									grVec3d_Length_SSE(const grVec3d *v1);
void									grVec3d_Subtract_SSE(const grVec3d *v1, const grVec3d *v2, grVec3d *result);
void									grVec3d_Add_SSE(const grVec3d *v1, const grVec3d *v2, grVec3d *result);
void									grVec3d_AddScaled_SSE(const grVec3d *v1, const grVec3d *v2, float scale, grVec3d *result);
grFloat									grVec3d_DistanceBetween_SSE(const grVec3d *v1, const grVec3d *v2);

#endif