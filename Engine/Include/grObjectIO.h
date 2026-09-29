/****************************************************************************************/
/*  JEOBJECTIO.H                                                                        */
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
#ifndef _GEOBJECTIO_H_

#define _GEOBJECTIO_H_

#include "GRWORLD.H"
#include "object.h"
#include "vfile.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grObjectIO grObjectIO;

grObjectIO *grObjectIO_Create(grWorld *pWorld);
void grObjectIO_Destroy(grObjectIO **grIO);
grBoolean grObjectIO_WriteObject(grObjectIO *grIO, grVFile *File, grPtrMgr *PtrMgr, grObject *Obj);
grBoolean grObjectIO_ReadObject(grObjectIO *grIO, grVFile *File, grPtrMgr *PtrMgr, grObject **Obj);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif