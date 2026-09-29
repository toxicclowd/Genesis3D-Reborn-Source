/****************************************************************************************/
/*  JEPOLYMGR.H                                                                         */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
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

#ifndef GR_POLYMGR_H
#define GR_POLYMGR_H

#include "Dcommon.h"
#include "Engine.h"
#include "BaseType.h"
#include "grTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef struct grPolyMgr				grPolyMgr;

//========================================================================================
//	Structure defs
//========================================================================================

//========================================================================================
//	Function prototypes
//========================================================================================

grPolyMgr *grPolyMgr_Create(void);
grBoolean grPolyMgr_IsValid(const grPolyMgr *Mgr);
grBoolean grPolyMgr_CreateRef(grPolyMgr *Mgr);
void grPolyMgr_Destroy(grPolyMgr **Mgr);
void grPolyMgr_SetDriver(grPolyMgr *Mgr, DRV_Driver *Driver);
void grPolyMgr_RenderGouraudPoly(	grPolyMgr			*Mgr, 
									const grTLVertex	*Verts, 
									int32				NumVerts, 
									uint32				Flags);
void grPolyMgr_RenderMiscPoly(	grPolyMgr				*Mgr, 
								const grTLVertex		*Verts, 
								int32					NumVerts, 
								grRDriver_Layer			*Layers,
								int32					NumLayers,
								uint32					Flags);

void grPolyMgr_RenderWorldPoly(	grPolyMgr				*Mgr, 
								const grTLVertex		*Verts, 
								int32					NumVerts, 
								grRDriver_Layer			*Layers,
								int32					NumLayers,
								void					*LMapCBContext,
								uint32					Flags);
void grPolyMgr_FlushBatch(grPolyMgr *Mgr);

#ifdef __cplusplus
}
#endif

#endif
