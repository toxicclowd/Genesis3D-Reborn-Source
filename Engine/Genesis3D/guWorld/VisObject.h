/****************************************************************************************/
/*  VISOBJECT.H                                                                         */
/*                                                                                      */
/*  Author:  Charles Bloom                                                              */
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
#ifndef GR_VISOBJECT_H
#define GR_VISOBJECT_H

#include "Engine.h"
#include "grFrustum.h"
#include "Object.h"
#include "List.h"

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------

typedef struct grVisObject		grVisObject;
typedef struct grVisObjectList	grVisObjectList;

//-------------------------

grVisObjectList *	grVisObjectList_Create(void);
void				grVisObjectList_Destroy(grVisObjectList ** pList);
grBoolean			grVisObjectList_IsValid(const grVisObjectList * List);

//-------------------------

grVisObject *		grVisObjectList_CreateObject(	grVisObjectList * List,grObject *Obj);
grVisObject *		grVisObjectList_FindObject(	const grVisObjectList * List,const grObject *Obj);
void				grVisObjectList_DestroyObject(grVisObjectList * List,grVisObject *VO);

grVisObject *		grVisObjectList_GetNext(const grVisObjectList * List,grVisObject *VO); // use NULL to start the walk

//-------------------------

void				grVisObjectList_RenderStart(grVisObjectList * List, const grEngine *Engine, 
							const grCamera *Camera, uint32 VisFrame);

void				grVisObjectList_RenderAll(const grVisObjectList * List,uint32 VisFrame);


//-------------------------

void				grVisObject_MarkVis(grVisObject *VO,uint32 VisFrame);

void				grVisObject_Render(grVisObject *VO,const grFrustum *Frustum,uint32 VisFrame);
								// not const, cuz it marks visframe

const grObject *	grVisObject_Object(const grVisObject *VO);

void				grVisObject_AddArea(grVisObject *VO,uint32 AreaUID); // use 0 to clear the list

//-------------------------

#ifdef __cplusplus
}
#endif

#endif

