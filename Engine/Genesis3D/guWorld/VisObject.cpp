/****************************************************************************************/
/*  VISOBJECT.C                                                                         */
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
/******************

The render pipe here can be a little confusing, so here it is.

1. Every object is added to all the areas contained in its BBox
2. If an object is only in one area, this is flagged as a special case.
3. If an object is visible through only one portal, we render the object
	(either immediately or at the end of the scene) through that portal.
4. If an object is visible through several portals, then :
	A. if it is only in one area, we render through all the portals that see it
	B. if in more than one area, we just render with the camera

The result is that we do not currently have to special case the
	"camera is inside the BBox of the object ? -> just render once"
Because that happens automagically.

********************/

#define DO_DEBUG_INFO

#define DO_RENDER_IMMEDIATE // else delay till all vis is done

#include "Engine.h"
#include "grFrustum.h"
#include "Object.h"
#include "List.h"
#include "VisObject.h"
#include "Ram.h"
#include "Errorlog.h"
#include "MemPool.h"
#include <assert.h>

#ifndef NDEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

#ifdef DO_DEBUG_INFO
int NumObjects = 0;
int NumObjectsVisible = 0;
int NumObjectsPortalled = 0;
#define DEBUG_INFO(x) x
#else
#define DEBUG_INFO(x)
#endif

struct grVisObject
{
	uint32		VisFrame;	// you set these up
	grFrustum	VisFrustum;
	grBoolean	VisHasFrustum;	// if ! HasFrustum, use the whole camera frustum
	grBoolean	InOneArea;
	uint32		AreaUID;

	grVisObjectList * MyOwner;
	uint32		PrepFrame,FrustumCount;
	grObject	Object;	
	grVisObject * VisList;	
	HashNode	* MyHashNode;
};

struct grVisObjectList
{
	grVisObjectList * MySelf1;

	Hash * ObjectHash;
	int NumObjects;
	MemPool * VisObjectPool;

	uint32		VisFrame;	// this stuff was set on this frame :
	grEngine *	Engine;
	grCamera *	Camera;
	grFrustum	CamFrustum;

	// grVisObject * VisList;
	grVisObjectList * MySelf2;
};

grVisObjectList *	grVisObjectList_Create(void)
{
grVisObjectList * List;
	List = (grVisObjectList *)grRam_AllocateClear(sizeof(*List));
	if ( ! List )	
		return NULL;

	List->MySelf1 = List->MySelf2 = List;

	List->ObjectHash = Hash_Create();
	if ( ! List->ObjectHash )
	{
		grVisObjectList_Destroy(&List);
		return NULL;
	}

	List->VisObjectPool = MemPool_Create(sizeof(grVisObject),32,64);
	if ( ! List->VisObjectPool )
	{
		grVisObjectList_Destroy(&List);
		return NULL;
	}

	List->NumObjects = 0;

	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );

return List;
}

void grVisObjectList_Destroy(grVisObjectList ** pList)
{
grVisObjectList * List;
grVisObject * VO;

	assert(pList);
	List = *pList;
	*pList = NULL;
	if ( ! List )
		return;

	VO = grVisObjectList_GetNext(List,NULL);
	while( VO )
	{
	grVisObject * VONext;
		VONext = grVisObjectList_GetNext(List,VO);
		grVisObjectList_DestroyObject(List,VO);
		VO = VONext;
	}

	assert(List->NumObjects == 0);

	if ( List->ObjectHash )
		Hash_Destroy(List->ObjectHash);

	if ( List->VisObjectPool )
		MemPool_Destroy(&(List->VisObjectPool));

	grRam_Free(List);
}

grVisObject * grVisObjectList_CreateObject(	grVisObjectList * List,grObject *Obj)
{
grVisObject * VO;

	assert( Obj);
	assert( grVisObjectList_IsValid(List) );

	if ( Hash_Get(List->ObjectHash,(uint32)Obj->Instance,NULL) )
	{
		grErrorLog_AddString(-1,"VisObjectList_CreateObject : Object already in list !",NULL);
		return NULL;
	}

	VO = (grVisObject *)MemPool_GetHunk(List->VisObjectPool);
	if ( ! VO )
		return NULL;

	VO->Object = *Obj;
	grObject_CreateRef(Obj);

	VO->VisFrame = -1;

	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );

	VO->MyHashNode = Hash_Add(List->ObjectHash,(uint32)Obj->Instance,(uint32)VO);

	assert( HashNode_Data(VO->MyHashNode) == (uint32)VO );

	List->NumObjects ++;

	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );

	DEBUG_INFO(NumObjects++);

	VO->MyOwner = List;

return VO;
}

grVisObject * grVisObjectList_FindObject(	const grVisObjectList * List,const grObject *Obj)
{
grVisObject * VO;

	assert( grVisObjectList_IsValid(List) );

	if ( ! Hash_Get(List->ObjectHash,(uint32)Obj->Instance,(uint32 *)&VO) )
		return NULL;

return VO;
}

void grVisObjectList_DestroyObject(grVisObjectList * List,grVisObject *VO)
{
	assert( VO );
	assert( grVisObjectList_IsValid(List) );

	assert( HashNode_Data(VO->MyHashNode) == (uint32)VO );
	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );
	assert( List->NumObjects > 0 );

#ifdef _DEBUG
	{
	grVisObject * VO2;
	HashNode * hn;

		if ( ! (hn = Hash_Get(List->ObjectHash,(uint32)(VO->Object.Instance),(uint32 *)&VO2)) )
			assert("object not in hash!" == NULL);

		assert(hn == VO->MyHashNode);
		assert(VO == VO2);
		assert(VO->MyOwner == List);
	}
#endif

	Hash_DeleteNode(List->ObjectHash,VO->MyHashNode);

	grObject_Free(&(VO->Object));

	List->NumObjects --;

	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );
	
	DEBUG_INFO(NumObjects--);
}

void grVisObjectList_RenderStart(grVisObjectList * List, const grEngine *Engine, 
							const grCamera *Camera, uint32 VisFrame)
{
	assert( grVisObjectList_IsValid(List) );

	DEBUG_INFO(NumObjectsVisible = NumObjectsPortalled = 0);

	List->Engine = (grEngine *)Engine;
	List->Camera = (grCamera *)Camera;
	grFrustum_SetWorldSpaceFromCamera(&(List->CamFrustum),Camera);
	List->VisFrame = VisFrame;
}

void grVisObject_RenderInternal(const grVisObjectList * List,const grVisObject * VO,uint32 VisFrame)
{
	assert(List->VisFrame == VisFrame);
	if ( VisFrame == VO->VisFrame )
	{
		if ( VO->PrepFrame != VisFrame )
		{
			DEBUG_INFO(NumObjectsVisible++);

			((grVisObject *)VO)->PrepFrame = VisFrame;

			//grObject_RenderPrep(&(VO->Object),List->Camera);
		}
	
		if ( VO->VisHasFrustum )
		{
			//grObject_RenderThroughFrustum(&(VO->Object),List->Engine,&(VO->VisFrustum),VisFrame);
			DEBUG_INFO(NumObjectsPortalled++);
		}
		else
		{
			//grObject_RenderThroughFrustum(&(VO->Object),List->Engine,&(List->CamFrustum),VisFrame);
		}
		
		DEBUG( ((grVisObject *)VO)->VisFrame = -1 );
	}
}

void grVisObject_Render(grVisObject *VO,const grFrustum *Frustum,uint32 VisFrame)
{

#ifdef DO_RENDER_IMMEDIATE
	if ( VO->InOneArea )
	{
		assert(VO->MyOwner->VisFrame == VisFrame);

		DEBUG_INFO(NumObjectsPortalled++);

		if ( VO->PrepFrame != VisFrame )
		{
			DEBUG_INFO(NumObjectsVisible++);
			VO->PrepFrame = VisFrame;
			VO->FrustumCount = 0;

			//grObject_RenderPrep(&(VO->Object),VO->MyOwner->Camera);
		}

		//grObject_RenderThroughFrustum(&(VO->Object),VO->MyOwner->Engine,Frustum,VisFrame ^ VO->FrustumCount);

		VO->FrustumCount += 256;
		VO->VisFrame = -1; // won't be rendered again

		return;
	}

	// else in more that one area
#endif

	if ( VO->VisFrame == VisFrame )
	{
		// already seen this object this frame from a different frustum
		VO->VisHasFrustum = GR_FALSE;
	}
	else
	{
		VO->VisFrame = VisFrame;
		VO->VisHasFrustum = GR_TRUE;
		VO->VisFrustum = *Frustum;
	}
}

void grVisObject_MarkVis(grVisObject *VO,uint32 VisFrame)
{
	VO->VisFrame = VisFrame;
	VO->VisHasFrustum = GR_FALSE;
}

void grVisObjectList_RenderAll(const grVisObjectList * List,uint32 VisFrame)
{
HashNode * hn;
DEBUG(int ObjsWalked=0);

	assert(List->VisFrame == VisFrame);
	assert( grVisObjectList_IsValid(List) );

	hn = NULL;

	assert( Hash_NumMembers(List->ObjectHash) == List->NumObjects );

	while( (hn = Hash_WalkNext(List->ObjectHash,hn)) )
	{
	grVisObject * VO;
		VO = (grVisObject *)HashNode_Data(hn);

		assert( hn == VO->MyHashNode );
		assert(VO->MyOwner == List);

		grVisObject_RenderInternal(List,VO,VisFrame);
		
		DEBUG(ObjsWalked++);
		assert(ObjsWalked <= List->NumObjects );
	}
	assert(ObjsWalked == List->NumObjects );
}

grVisObject * grVisObjectList_GetNext(const grVisObjectList * List,grVisObject *VO)
{
HashNode * hn;
	assert( grVisObjectList_IsValid(List) );

	if ( VO )
		hn = VO->MyHashNode;
	else
		hn = NULL;

	hn = Hash_WalkNext(List->ObjectHash,hn);
	
	if ( ! hn )
		return NULL;
			
	VO = (grVisObject *)HashNode_Data(hn);

	assert( ! VO || VO->MyOwner == List);

return VO;
}

grBoolean grVisObjectList_IsValid(const grVisObjectList * List)
{
	if ( ! List) return GR_FALSE;
	if ( ! (List->MySelf1 == List) ) return GR_FALSE;
	if ( ! (List->MySelf2 == List) ) return GR_FALSE;

	if ( List->NumObjects < 0 )
		return GR_FALSE;

	if ( ! List->ObjectHash )
		return GR_FALSE;

	if ( ! MemPool_IsValid(List->VisObjectPool) )
		return GR_FALSE;

	if ( Hash_NumMembers(List->ObjectHash) != List->NumObjects )
		return GR_FALSE;

return GR_TRUE;
}

const grObject *	grVisObject_Object(const grVisObject *VO)
{
	assert(VO);
	return &(VO->Object);
}

void				grVisObject_AddArea(grVisObject *VO,uint32 AreaUID)
{
	if ( AreaUID == 0 )
	{
		VO->InOneArea = GR_TRUE;
	}
	else if ( VO->AreaUID == 0 )
	{
		VO->InOneArea = GR_TRUE;
	}
	else if ( VO->AreaUID != AreaUID )
	{
		VO->InOneArea = GR_FALSE;
	}
	VO->AreaUID = AreaUID;
}
