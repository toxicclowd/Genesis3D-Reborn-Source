/****************************************************************************************/
/*  JECHAIN.H                                                                           */
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

#ifndef GR_CHAIN_H
#define GR_CHAIN_H

#include "BaseType.h"
#include "VFile.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================

//========================================================================================
//	Structure defs
//========================================================================================
typedef struct jeChain grChain;
typedef struct jeChain jeChain;
typedef struct jeChain_Link grChain_Link;
typedef struct jeChain_Link jeChain_Link;

typedef grBoolean grChain_IOFunc(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr); // Write
typedef grBoolean grChain_ReadIOFunc(grVFile *VFile, void **LinkData, void *Context, grPtrMgr *PtrMgr); //Read

//========================================================================================
//	Function prototypes
//========================================================================================
grChain		*grChain_Create(void);
grBoolean	grChain_CreateRef(grChain *Chain);

grBoolean	grChain_WriteToFile(const grChain *Chain, grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);
grChain		*grChain_CreateFromFile(grVFile *VFile, grChain_ReadIOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);

void		grChain_Destroy(grChain **Chain);
grBoolean	grChain_IsValid(const grChain *Chain);
grChain_Link *grChain_FindLink(const grChain *Chain, void *LinkData);
grBoolean	grChain_AddLink(grChain *Chain, grChain_Link *Link);
grBoolean	grChain_InsertLinkAfter(grChain *Chain, grChain_Link *InsertAfter, grChain_Link *Link);
grBoolean	grChain_InsertLinkBefore(grChain *Chain, grChain_Link *InsertBefore, grChain_Link *Link);
grBoolean	grChain_AddLinkData(grChain *Chain, void *LinkData);
grBoolean	grChain_InsertLinkData(grChain *Chain, grChain_Link *InsertAfter, void *LinkData);
grBoolean	grChain_RemoveLink(grChain *Chain, grChain_Link *Link);
grBoolean	grChain_RemoveLinkData(grChain *Chain, void *LinkData);
uint32		grChain_GetLinkCount(const grChain *Chain);
grChain_Link *grChain_GetFirstLink(const grChain *Chain);
grChain_Link *grChain_GetLinkByIndex(const grChain *Chain, uint32 Index);
void		*grChain_GetLinkDataByIndex(const grChain *Chain, uint32 Index);
void		*grChain_GetNextLinkData(grChain *Chain, void *Start);
grChain_Link *grChain_LinkCreate(void *LinkData);
void		grChain_LinkDestroy(grChain_Link **Link);
grBoolean	grChain_LinkIsValid(const grChain_Link *Link);
void		*grChain_LinkGetLinkData(const grChain_Link *Link);
grChain_Link *grChain_LinkGetNext(const grChain_Link *Link);
grChain_Link *grChain_LinkGetPrev(const grChain_Link *Link);

uint32		grChain_LinkDataGetIndex(const grChain *Chain, void *LinkData);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define jeChain_AddLink                          grChain_AddLink
#define jeChain_AddLinkData                      grChain_AddLinkData
#define jeChain_Create                           grChain_Create
#define jeChain_CreateFromFile                   grChain_CreateFromFile
#define jeChain_CreateRef                        grChain_CreateRef
#define jeChain_Destroy                          grChain_Destroy
#define jeChain_FindLink                         grChain_FindLink
#define jeChain_GetFirstLink                     grChain_GetFirstLink
#define jeChain_GetLinkByIndex                   grChain_GetLinkByIndex
#define jeChain_GetLinkCount                     grChain_GetLinkCount
#define jeChain_GetLinkDataByIndex               grChain_GetLinkDataByIndex
#define jeChain_GetNextLinkData                  grChain_GetNextLinkData
#define jeChain_IOFunc                           grChain_IOFunc
#define jeChain_InsertLinkAfter                  grChain_InsertLinkAfter
#define jeChain_InsertLinkBefore                 grChain_InsertLinkBefore
#define jeChain_InsertLinkData                   grChain_InsertLinkData
#define jeChain_IsValid                          grChain_IsValid
#define jeChain_LinkCreate                       grChain_LinkCreate
#define jeChain_LinkDataGetIndex                 grChain_LinkDataGetIndex
#define jeChain_LinkDestroy                      grChain_LinkDestroy
#define jeChain_LinkGetLinkData                  grChain_LinkGetLinkData
#define jeChain_LinkGetNext                      grChain_LinkGetNext
#define jeChain_LinkGetPrev                      grChain_LinkGetPrev
#define jeChain_LinkIsValid                      grChain_LinkIsValid
#define jeChain_ReadIOFunc                       grChain_ReadIOFunc
#define jeChain_RemoveLink                       grChain_RemoveLink
#define jeChain_RemoveLinkData                   grChain_RemoveLinkData
#define jeChain_WriteToFile                      grChain_WriteToFile

#endif // GENESIS_NO_JET_COMPAT

#endif

