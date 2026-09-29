/****************************************************************************************/
/*  ASMXFORM3D.H                                                                        */
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
#ifndef GR_ASMXFORM_H
#define GR_ASMXFORM_H

#include "Xform3d.h"

#ifdef __cplusplus
extern "C" {
#endif

void GRCC grXForm3d_TransformVecArrayKatmai(const grXForm3d *XForm, const grVec3d *Source, grVec3d *Dest, int32 Count);

void GRCC grXForm3d_TransformArrayKatmai(const grXForm3d *XForm,
										   const grVec3d *Source,
										   grVec3d *Dest,
										   int32 SourceStride,
										   int32 DestStride,
										   int32 Count);

void GRCC grXForm3d_TransformArrayX86(const grXForm3d *XForm,
										const grVec3d *Source,
										grVec3d *Dest,
										int32 SourceStride,
										int32 DestStride,
										int32 Count);

void GRCC grXForm3d_TransformVecArrayX86(const grXForm3d *XForm, const grVec3d *Source, grVec3d *Dest, int32 Count);

void GRCC grXForm3d_TransformVecArray3DNow(const grXForm3d *XForm, const grVec3d *Source, grVec3d *Dest, int32 Count);

void GRCC grXForm3d_TransformArray3DNow(const grXForm3d *XForm,
										  const grVec3d *Source,
										  grVec3d *Dest,
										  int32 SourceStride,
										  int32 DestStride,
										  int32 Count);


#ifdef __cplusplus
}
#endif

#endif // GR_ASMXFORM_H
