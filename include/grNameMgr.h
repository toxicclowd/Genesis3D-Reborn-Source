/****************************************************************************************/
/*  JENAMEMGR.H                                                                         */
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
#ifndef __GR_NAMEMGR_H__
#define __GR_NAMEMGR_H__

#include "BaseType.h"
#include "VFile.h"

#ifdef NEWSAVE

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jeChain			grChain;
typedef struct jeChain_Link		grChain_Link;

typedef struct jeNameMgr grNameMgr;
typedef struct jeNameMgr jeNameMgr;
typedef void *  (GRCC *grNameMgr_CreateFromFileCallback)(grVFile *VFile, grNameMgr *NM);
typedef grBoolean (GRCC *grNameMgr_WriteToFileCallback)(void *DataPtr, grVFile *VFile, grNameMgr *NM);

#define GR_NAME_MGR_CREATE_FOR_READ (1<<0)
#define GR_NAME_MGR_CREATE_FOR_WRITE (1<<1)

GRAPI grNameMgr * GRCC grNameMgr_Create(grVFile *System, int32 CreateFlags);
GRAPI grBoolean GRCC grNameMgr_CreateRef(grNameMgr *NameMgr);
GRAPI void GRCC grNameMgr_Destroy(grNameMgr **NameMgr);
GRAPI grBoolean GRCC grNameMgr_Write(grNameMgr *NM, grVFile *VFile, void *PtrToData, grNameMgr_WriteToFileCallback CB_Write);
GRAPI grBoolean GRCC grNameMgr_Read(grNameMgr *NM, grVFile *VFile, grNameMgr_CreateFromFileCallback CB_Read, void **ReturnPointer);
GRAPI grBoolean GRCC grNameMgr_WriteFlush(grNameMgr *NM);

#ifdef __cplusplus
}
#endif

#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define JE_NAME_MGR_CREATE_FOR_READ              GR_NAME_MGR_CREATE_FOR_READ
#define JE_NAME_MGR_CREATE_FOR_WRITE             GR_NAME_MGR_CREATE_FOR_WRITE
#define jeNameMgr_Create                         grNameMgr_Create
#define jeNameMgr_CreateRef                      grNameMgr_CreateRef
#define jeNameMgr_Destroy                        grNameMgr_Destroy
#define jeNameMgr_Read                           grNameMgr_Read
#define jeNameMgr_Write                          grNameMgr_Write
#define jeNameMgr_WriteFlush                     grNameMgr_WriteFlush

#endif // GENESIS_NO_JET_COMPAT

#endif
