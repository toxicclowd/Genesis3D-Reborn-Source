/****************************************************************************************/
/*  BitmapList.h                                                                        */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description: Maintains a pool of bitmap pointers.                                   */
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
#ifndef BITMAPLIST_H
#define BITMAPLIST_H

#include "grTypes.h"
#include "Dcommon.h"
#include "Bitmap.h"

typedef struct BitmapList		BitmapList;

#ifdef __cplusplus
extern "C" {
#endif

BitmapList *BitmapList_Create(void);
grBoolean BitmapList_Destroy(BitmapList *pList);

grBoolean BitmapList_SetGamma(BitmapList *pList, grFloat Gamma);

grBoolean BitmapList_AttachAll(BitmapList *pList, DRV_Driver *Drivera, grFloat Gamma);
grBoolean BitmapList_DetachAll(BitmapList *pList);

	// _Add & _Remove do NOT return Ok/NOk	
grBoolean BitmapList_Add(BitmapList *pList, grBitmap *Bitmap);	// returns Was It New ?
grBoolean BitmapList_Remove(BitmapList *pList,grBitmap *Bitmap);// returns Was It Removed ?
	// _Add & _Remove also do not do any Attach or Detach

grBoolean BitmapList_Has(BitmapList *pList, grBitmap *Bitmap);

#ifndef NDEBUG
int			BitmapList_CountMembers(BitmapList *pList);
int			BitmapList_CountMembersAttached(BitmapList *pList);
#endif

#ifdef __cplusplus
}
#endif
#endif
