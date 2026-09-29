/****************************************************************************************/
/*  OBJECT.C                                                                            */
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Object.h"
#include "Errorlog.h"
#include "Ram.h"
#include "crc32.h"
#include "grFrustum.h"
#include "Util.h"
#include "Log.h"

#ifndef WINVER
#define HWND void *
#endif

#define MAX_DEFS	(100)
#define INVALID_INDEX	(-1)

//#define ObjectError(str,Obz)	grErrorLog_AddString(-1,"grObject " (((Obz) != nullptr) ? (((grObject *)(Obz))->Name) : "Unknown") " Error: " str, nullptr))
__inline void ObjectError(const char* str, const grObject* Obz)
{
	char msg[1024];
	sprintf_s(msg, "grObject %s Error: %s", (((Obz) != nullptr) ? (((grObject *)(Obz))->Name) : "Unknown"), str);
	grErrorLog_AddString(-1, msg, (((Obz) != nullptr) ? (((grObject *)(Obz))->Name) : "Unknown"));
}


/*}{********************** Manager Functions ******************/

static const uint32 grObject_Tag = 0x424F4547; //GEOB

static grObjectDef	RegisteredDefs[MAX_DEFS];
static uint32		RegisteredTag[MAX_DEFS];
static unsigned int			NumRegisteredDefs = 0;

uint32 __inline grObject_DefTag(const grObjectDef * Methods)
{
return CRC32_Array((const uint8 *)Methods->Name,strlen(Methods->Name));
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_RegisterGlobalObjectDef(const grObjectDef * Methods)
{
	uint32 Tag;
	int i;


	assert(Methods);

	if ( strlen(Methods->Name) == 0 )
	{
		grErrorLog_AddString(-1,"Object Error: no Name!",Methods->Name);
		return GR_FALSE;
	}

	Tag = grObject_DefTag(Methods);

	// <> this is not thread-safe

	for(i=0;i<NumRegisteredDefs;i++)
	{
		if ( RegisteredTag[i] == Tag )
		{
			if ( memcmp(&(RegisteredDefs[i]),Methods,sizeof(*Methods)) == 0 )
				return GR_TRUE;
			else
			{
				grErrorLog_AddString(-1,"Object Error: Tag collision!",Methods->Name);
				return GR_FALSE;
			}
		}
	}

	if ( NumRegisteredDefs == MAX_DEFS )
	{
		grErrorLog_AddString(-1,"Object Error: too many defs!",Methods->Name);
		return GR_FALSE;
	}

	RegisteredTag[ NumRegisteredDefs] = Tag;
	RegisteredDefs[NumRegisteredDefs] = *Methods;
	NumRegisteredDefs++;
	
	return GR_TRUE;
}
//====================================================================================================
//====================================================================================================
int32 grObject_FindObjectDef( const char * TypeName  )
{
	int i;

	for( i = 0; i < NumRegisteredDefs; i++ )
	{
		if( strcmp( TypeName, RegisteredDefs[i].Name) == 0 )
			return( i );
	}
	return( INVALID_INDEX );
}
//====================================================================================================
//====================================================================================================
GRAPI int32		GRCC grObject_GetRegisteredN()
{
	return( NumRegisteredDefs );
}

//====================================================================================================
//====================================================================================================

GRAPI const char*	GRCC grObject_GetRegisteredDefName( int Index )
{
	assert( Index < NumRegisteredDefs );

	return( RegisteredDefs[Index].Name );
}

GRAPI uint32	GRCC grObject_GetRegisteredFlags( int Index )
{
	assert( Index < NumRegisteredDefs );

	return( RegisteredDefs[Index].Flags);
}

GRAPI grBoolean	GRCC grObject_GetRegisteredPropertyList(const char * TypeName, grProperty_List **List)
{
	int Index;

	Index =  grObject_FindObjectDef( TypeName  );
	if( Index == INVALID_INDEX )
		return( GR_FALSE );
	if( RegisteredDefs[Index].GetGlobalPropertyList == nullptr )
	{
		*List = nullptr;
		return( GR_TRUE );
	}
	return( (*RegisteredDefs[Index].GetGlobalPropertyList)(List) );
}

GRAPI grBoolean	GRCC grObject_SetRegisteredProperty( const char * TypeName, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	int Index;

	Index =  grObject_FindObjectDef( TypeName  );
	if( Index == INVALID_INDEX )
		return( GR_FALSE );
	if( RegisteredDefs[Index].SetGlobalProperty == nullptr )
	{
		return( GR_TRUE );
	}
	return( (*RegisteredDefs[Index].SetGlobalProperty)(FieldID, DataType, pData) );
}


//====================================================================================================
//====================================================================================================
GRAPI grObject *	GRCC grObject_Create( const char * TypeName )
{
	grObject* Object{};
	int32 Index = 0;

	Index = grObject_FindObjectDef( TypeName );

	if( Index == INVALID_INDEX )
		return nullptr;

	Object = (grObject *)grRam_AllocateClear(sizeof(grObject)); // <> MemPool

	if( Object == nullptr )
		return( nullptr );

	Object->Name = nullptr;
	Object->Methods = &RegisteredDefs[Index];
	Object->RefCnt = 1;
	Object->pWorld = nullptr;
	Object->pEngine = nullptr;
	Object->pSoundSystem = nullptr;
	Object->Contents = CONTENTS_SOLID;
	Object->Parent = nullptr;
	Object->Self = Object;
	Object->Children = grChain_Create();

	if (!Object->Children)
		goto ExitWithError;

	Object->Instance = Object->Methods->CreateInstance();

	if( Object->Instance == nullptr )
		goto ExitWithError;

	return Object;

	ExitWithError:
	{
		if (Object)
		{
			if (Object->Instance)
			{
				assert(Object->Methods);
				assert(Object->Children);

				Object->Methods->Destroy(&Object->Instance);
			}

			if (Object->Children)
				grChain_Destroy(&Object->Children);

			grRam_Free(Object);
		}
		return nullptr;
	}
}
//====================================================================================================
//====================================================================================================
GRAPI grObject *	GRCC grObject_Duplicate( grObject *pObject )
{
	grObject * pObjectCopy{};
	int32 Index{};

	if( pObject->Methods->DuplicateInstance == nullptr )
		return( nullptr );

	Index = grObject_FindObjectDef( grObject_GetTypeName(pObject) );

	if( Index == INVALID_INDEX )
		return nullptr;

	pObjectCopy = (grObject *)grRam_AllocateClear(sizeof(grObject)); // <> MemPool
	if( pObjectCopy == nullptr )
		return( nullptr );

	pObjectCopy->Name = nullptr;
	pObjectCopy->Methods = &RegisteredDefs[Index];
	pObjectCopy->RefCnt = 1;
	//Royce
	pObjectCopy->Children = grChain_Create();
	//---

	pObjectCopy->Instance = pObject->Methods->DuplicateInstance(pObject->Instance);
	if( pObjectCopy->Instance == nullptr )
	{
		grRam_Free( pObjectCopy );
		return( nullptr );
	}
	return pObjectCopy;
}

//====================================================================================================
//====================================================================================================
GRAPI void		GRCC grObject_Destroy(grObject ** pObject)
{
	grObject * Object;
	assert(pObject);
	Object = *pObject;
	if ( ! Object )
		return;
	if (Object != Object->Self) {
		return;
	}
	assert( Object->RefCnt > 0 );
#ifdef _DEBUG
	Log_Printf("grObject_Destroy %s\n", Object->Name);
#endif

	Object->RefCnt--;

	if( Object->RefCnt == 0 )
	{
		if (Object->Children)
			grChain_Destroy(&Object->Children);

		if( Object->Name != nullptr )
			grRam_Free(Object->Name); // <> MemPool

		// Free the instance
		if( Object->Instance != nullptr ) // Krouer: do not crash if object has failed to load
			grObject_Free(Object); 

		grRam_Free(Object); // <> MemPool
	}

	*pObject = nullptr;
}

//====================================================================================================
//====================================================================================================
GRAPI void			GRCC grObject_SetName( grObject * pObject, const char * Name )
{
	assert( pObject );
	assert( Name );

	if( pObject->Name != nullptr )
		grRam_Free( pObject->Name );
	pObject->Name = Util_StrDup( Name );
}

GRAPI const char  *GRCC grObject_GetName( const grObject * pObject )
{
	assert( pObject );
	return( pObject->Name );
}

/*}{********************** Object Functions ******************/


GRAPI grObject *	GRCC grObject_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr)
{
	grObject * Object = nullptr;
	grVFile * HintsFile = nullptr;
	uint32 Tag;
	long StartPos = -1;
	uint32	NameLng{};

	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, File, (void **)&Object))
			return nullptr;

		if (Object)
		{
			grObject_CreateRef(Object);

			return Object;		// Ptr found in stack, return it
		}
	}

	HintsFile = grVFile_CreateHintsFile(File);
	if ( ! HintsFile )
		return nullptr;

	if ( ! grVFile_Tell(HintsFile,&StartPos))
		goto fail;

	Object = (grObject *)grRam_AllocateClear(sizeof(grObject)); // <> MemPool
	if ( ! Object )
		goto fail;

	Object->Contents = CONTENTS_SOLID;
	Object->Children = grChain_Create();
	Object->RefCnt = 1;
	Object->Self = Object;

	if (!Object->Children)
		goto fail;

	if (PtrMgr)
	{
#pragma message( "Should we recover the pushed pointer on failure?")
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Object))
			goto fail;
	}

	if ( ! grVFile_Read(HintsFile,&Tag,sizeof(Tag)) )
		goto fail;

	if ( Tag != grObject_Tag )
	{
		ObjectError("didn't get GEOB tag!",nullptr);
		goto fail;
	}
	
	if ( ! grVFile_Read(HintsFile,&Tag,sizeof(Tag)) )
		goto fail;

	{
	int i;
		for(i=0;i<NumRegisteredDefs;i++)
		{
			if ( RegisteredTag[i] == Tag )
			{
				Object->Methods = &RegisteredDefs[i];
				break;
			}
		}
	}
	
	if ( ! grVFile_Read(File, &NameLng,sizeof(NameLng)) )
		goto fail;

	if( NameLng )
	{
		Object->Name = (char *)grRam_Allocate( NameLng );
		if ( ! grVFile_Read(File, Object->Name, NameLng) )
			goto fail;
	}

	if ( ! Object->Methods )
	{
		ObjectError("Couldn't find registered def to create!",Object);
		goto fail;
	}
	else if ( ! Object->Methods->CreateFromFile )
	{
		ObjectError("Found registered with no create!",Object);
		goto fail;
	}

	grVFile_Close(HintsFile);
	HintsFile = nullptr;

	Object->Instance = Object->Methods->CreateFromFile(File, PtrMgr);
	
	if ( ! 	Object->Instance )
		goto fail;

	return Object;

fail:

	ObjectError("CreateFromFile failed",Object);
	grObject_Destroy(&Object);
	if ( HintsFile )
	{
		if ( StartPos != -1 )
			grVFile_Seek(HintsFile,StartPos,GR_VFILE_SEEKSET);
		grVFile_Close(HintsFile);
	}

	return nullptr;
}


//====================================================================================================
//====================================================================================================

GRAPI grBoolean	GRCC grObject_WriteToFile(const grObject * Object,grVFile * File, grPtrMgr *PtrMgr)

{
	grVFile * HintsFile = nullptr;
	uint32 Tag;
	long StartPos = -1;
	uint32	NameLng{};

	assert(Object && Object->Instance && Object->Methods);

	if (PtrMgr)
	{
		uint32		Count;

		// writes the pointer header
		if (!grPtrMgr_WritePtr(PtrMgr, File, (void*)Object, &Count))
			return GR_FALSE;

		if (Count)
			return GR_TRUE;		// Ptr was on stack, so return

		assert ( Object->Methods->WriteToFile );

		// For object reentrance this need to be done here - consulted John
		// if an error occurs after this then a pop need to be done
		if (PtrMgr)
		{
			// Push the ptr on the stack
			if (!grPtrMgr_PushPtr(PtrMgr, (void*)Object))
				return GR_FALSE;
		}

	}

	if ( ! Object->Methods->WriteToFile )
		return GR_FALSE;

	HintsFile = grVFile_CreateHintsFile(File);
	if ( ! HintsFile )
		goto fail;

	if ( ! grVFile_Tell(HintsFile,&StartPos))
		goto fail;

	if ( ! grVFile_Write(HintsFile,&grObject_Tag,sizeof(grObject_Tag)) )
		goto fail;

	Tag = grObject_DefTag(Object->Methods);

	if ( ! grVFile_Write(HintsFile,&Tag,sizeof(Tag)) )
		goto fail;

	if( Object->Name  )
	{
		NameLng = strlen( Object->Name ) + 1;
	}
	else
		NameLng = 0;
	if ( ! grVFile_Write(File, &NameLng,sizeof(NameLng)) )
		goto fail;

	if( NameLng )
		if ( ! grVFile_Write(File, Object->Name, NameLng) )
			goto fail;

	grVFile_Close(HintsFile);
	HintsFile = nullptr;

	if ( ! Object->Methods->WriteToFile(Object->Instance,File, PtrMgr) )
		goto fail;

	return GR_TRUE;

fail:

	if (PtrMgr)
		{
		grPtrMgr_PopPtr(PtrMgr, (void*)Object);
		}

	ObjectError("WriteToFile failed",Object);
	if (HintsFile)
	{
		if ( StartPos != -1 )
			grVFile_Seek(HintsFile,StartPos,GR_VFILE_SEEKSET);
		grVFile_Close(HintsFile);
	}
	return GR_FALSE;
}



//====================================================================================================
//====================================================================================================
GRAPI grObject_Type	GRCC grObject_GetType(const grObject * Object)
{
	assert(Object && Object->Instance && Object->Methods);
	return Object->Methods->Type;
}

//====================================================================================================
//====================================================================================================
GRAPI const char *GRCC grObject_GetTypeName	(const grObject * Object)
{
	assert(Object && Object->Instance && Object->Methods);
	return Object->Methods->Name;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_GetPropertyList(const grObject *Object, grProperty_List **List)
{
	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);

	if (!Object->Methods->GetPropertyList)
		return GR_FALSE;

	return Object->Methods->GetPropertyList(Object->Instance, List);
}

//====================================================================================================
//====================================================================================================

GRAPI grBoolean	GRCC grObject_SetProperty(grObject *Object, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);

	if (!Object->Methods->SetProperty)
		return GR_FALSE;

	return Object->Methods->SetProperty(Object->Instance, FieldID, DataType, pData );
}

//====================================================================================================
//====================================================================================================

GRAPI grBoolean	GRCC grObject_GetProperty(const grObject *Object, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data * pData )
{
	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);

	if (!Object->Methods->GetProperty)
		return GR_FALSE;

	return Object->Methods->GetProperty(Object->Instance, FieldID, DataType, pData );
}

//====================================================================================================
//====================================================================================================
GRAPI void   *		GRCC grObject_GetInstance( const grObject *Object )
{
	assert(Object);

	return( Object->Instance );
}

//====================================================================================================
//====================================================================================================
GRAPI void			GRCC grObject_CreateInstanceRef(grObject * Object)
{
	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);
	assert(Object->Methods->CreateRef);
	Object->Methods->CreateRef(Object->Instance);
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_Free		(grObject * Object)
{
	assert(Object && Object->Instance && Object->Methods);
	assert(Object->Methods->Destroy);
	return( Object->Methods->Destroy(&(Object->Instance)));
}

//====================================================================================================
//====================================================================================================
GRAPI void		GRCC grObject_CreateRef	(grObject * Object)
{
	assert(Object );
	 Object->RefCnt++;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_Render(	const grObject			*Object,
												const grWorld			*World, 
												const grEngine			*Engine, 
												const grCamera			*Camera, 
												const grFrustum			*CameraSpaceFrustum, 
												grObject_RenderFlags	RenderFlags)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->Render )
		return GR_FALSE;

	return Object->Methods->Render(Object->Instance, World, Engine, Camera, CameraSpaceFrustum, RenderFlags);
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_AttachWorld( grObject *Object, grWorld * pWorld )
{
	//grChain_Link	*Link;
	grBoolean		Ret{};
	grObject	*Object2{};

	assert(Object && Object->Instance && Object->Methods);

	Ret = GR_TRUE;
	
	//Royce
	if (Object->Children) {
	//---
		for (Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
		{
			assert(Object2);

			if (!Object2->Methods->AttachWorld )
				continue;

			Ret &= Object2->Methods->AttachWorld( Object2->Instance, pWorld);
		}
	//Royce
	}
	//---

	if (!Object->Methods->AttachWorld )
		return Ret;

	Ret &= Object->Methods->AttachWorld(Object->Instance, pWorld );
	Object->pWorld = pWorld;

	return Ret;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_DettachWorld( grObject *Object, grWorld * pWorld )
{
	//grChain_Link	*Link;
	grBoolean		Ret{};
	grObject	*Object2{};

	assert(Object && Object->Instance && Object->Methods);
	if (Object != Object->Self) {
		return GR_FALSE;
	}

	Ret = GR_TRUE;

	for (Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
	{
		assert(Object2);

		if (Object2 != Object2->Self) {
			continue;
		}

		if (!Object2->Methods->DettachWorld )
			continue;

		Ret &= Object2->Methods->DettachWorld( Object2->Instance, pWorld);
	}

	if (!Object->Methods->DettachWorld)
		return Ret;

	Ret &= Object->Methods->DettachWorld(Object->Instance, pWorld );
	Object->pWorld = nullptr;

	return Ret;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_AttachEngine( grObject *Object, grEngine *Engine )
{
	//grChain_Link	*Link;
	grBoolean		Ret{};
	grObject	*Object2{};

	assert(Object && Object->Instance && Object->Methods);

	Ret = GR_TRUE;

	//Royce
	if (Object->Children) {
	//---
		for (Object2 = (grObject*)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
		{
		
			assert(Object2);

			if (!Object2->Methods->AttachEngine )
				continue;

			Ret &= Object2->Methods->AttachEngine( Object2->Instance, Engine);
		}
	//Royce
	}
	//----

	if (!Object->Methods->AttachEngine )
		return Ret;
	
	Ret &= Object->Methods->AttachEngine(Object->Instance, Engine );
	Object->pEngine = Engine;

	return Ret;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_DettachEngine( grObject *Object, grEngine *Engine )
{
	//grChain_Link	*Link;
	grBoolean		Ret{};
	grObject	*Object2{};

	assert(Object && Object->Instance && Object->Methods);
	if (Object != Object->Self) {
		return GR_TRUE;
	}

	Ret = GR_TRUE;

	for (Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
	{
		assert(Object2);

		if (Object2 != Object2->Self) {
			continue;
		}

		if (!Object2->Methods->DettachEngine )
			continue;

		Ret &= Object2->Methods->DettachEngine( Object2->Instance, Engine);
	}

	if (!Object->Methods->DettachEngine )
		return Ret;

	Ret &= Object->Methods->DettachEngine(Object->Instance, Engine );
	Object->pEngine = nullptr;

	return Ret;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_AttachSoundSystem( grObject *Object, grSound_System *SoundSystem )
{
	//grChain_Link	*Link;
	grBoolean		Ret{};
	grObject* Object2{};

	assert(Object && Object->Instance && Object->Methods);
	Object->pSoundSystem = SoundSystem;

	Ret = GR_TRUE;

	//Royce
	if (Object->Children) {
	//---
		for (Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
		{
		
			assert(Object2);

			if (!Object2->Methods->AttachSoundSystem )
				continue;

			Ret &= Object2->Methods->AttachSoundSystem( Object2->Instance, SoundSystem);
		}
	//Royce
	}
	//---

	if (!Object->Methods->AttachSoundSystem )
		return Ret;


	Ret &= Object->Methods->AttachSoundSystem(Object->Instance, SoundSystem );

	return Ret;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_DettachSoundSystem( grObject *Object, grSound_System *SoundSystem )
{
	grChain_Link	*Link{};
	grBoolean		Ret{};

	assert(Object && Object->Instance && Object->Methods);
	if (Object != Object->Self) {
		return GR_FALSE;
	}

	Object->pSoundSystem = nullptr;

	Ret = GR_TRUE;

	for (Link = grChain_GetFirstLink(Object->Children); Link; Link = grChain_LinkGetNext(Link))
	{
		grObject	*Object2 = (grObject*)Link;
		
		assert(Object2);
		if (Object2 != Object2->Self) {
			continue;
		}
	
		if (!Object2->Methods || !Object2->Methods->DettachSoundSystem )
			continue;

		Ret &= Object2->Methods->DettachSoundSystem(Object2->Instance, SoundSystem);
	}

	if (!Object->Methods->DettachSoundSystem )
		return Ret;

	Ret &= Object->Methods->DettachSoundSystem(Object->Instance, SoundSystem);


	return Ret;
}


//====================================================================================================
//	grObject_Collision
//====================================================================================================
GRAPI grBoolean GRCC grObject_Collision(const grObject *Object, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane, grObject ** pSubObject)
{
grChain_Link * Link{};
grFloat Distance{},ClosestDistance{};
grVec3d ClosestImpact{};
grPlane ClosestPlane{};
const grObject * ClosestObject{};
grBoolean GotHit{};

	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);

	GotHit = GR_FALSE;
	if(grObject_GetContents(Object) != CONTENTS_SOLID) return GotHit; // Incarnadine

	if ( pSubObject )
	{
		assert( Object->Children );
		for( Link = grChain_GetFirstLink(Object->Children); Link; Link = grChain_LinkGetNext(Link) )
		{
		grObject *Child,*ChildSubO;
			Child = (grObject *)grChain_LinkGetLinkData(Link);
			assert(Child);

			if (Impact && Plane)
			{
				if ( grObject_Collision(Child, Box, Front, Back, Impact, Plane, &ChildSubO) )
				{
					Distance = grVec3d_DistanceBetweenSquared(Front,Impact);
					if ( ! GotHit || Distance < ClosestDistance )
					{
						GotHit = GR_TRUE;
						ClosestDistance = Distance;
						ClosestImpact = *Impact;
						ClosestPlane = *Plane;
						ClosestObject = ChildSubO;
					}
				}
			} else
				if ( grObject_Collision(Child, Box, Front, Back, nullptr, nullptr, &ChildSubO) )
					return GR_TRUE;
		}
	}

	if ( Object->Methods->Collision)
	{
		if (Impact && Plane)
		{
			if ( Object->Methods->Collision(Object->Instance, Box, Front, Back, Impact, Plane) )
			{
				Distance = grVec3d_DistanceBetweenSquared(Front,Impact);
				if ( ! GotHit || Distance < ClosestDistance )
				{
					GotHit = GR_TRUE;
					ClosestDistance = Distance;
					ClosestImpact = *Impact;
					ClosestPlane = *Plane;
					ClosestObject = Object;
				}
			}
		} else
			if ( Object->Methods->Collision(Object->Instance, Box, Front, Back, nullptr, nullptr) )
				return GR_TRUE;
	}

	if ( GotHit )
	{
		*Impact = ClosestImpact;
		*Plane = ClosestPlane;
		if ( pSubObject ) *pSubObject = (grObject *)ClosestObject;
		return GR_TRUE;
	}

return GR_FALSE;
}

// Added by Icestorm
//====================================================================================================
//	grObject_ChangeBoxCollision
//====================================================================================================
GRAPI grBoolean GRCC grObject_ChangeBoxCollision(const grObject *Object, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane, grObject ** SubObject)
{
	grChain_Link* Link{};
	grFloat Distance{}, ClosestDistance{};
	grExtBox ClosestImpactBox{};
	grPlane ClosestPlane{};
	const grObject* ClosestObject{};
	grBoolean GotHit{};

	assert(Object);
	assert(Object->Instance);
	assert(Object->Methods);

	GotHit = GR_FALSE;

	if ( SubObject )
	{
		assert( Object->Children );
		for( Link = grChain_GetFirstLink(Object->Children); Link; Link = grChain_LinkGetNext(Link) )
		{
			grObject *Child,*ChildSubO{};
			Child = (grObject *)grChain_LinkGetLinkData(Link);
			assert(Child);

			if (ImpactBox && Plane)
			{
				if ( grObject_ChangeBoxCollision(Child, Pos, FrontBox, BackBox, ImpactBox, Plane, &ChildSubO) )
				{
					Distance = grVec3d_DistanceBetweenSquared(&FrontBox->Min, &ImpactBox->Min);
					if ( ! GotHit || Distance < ClosestDistance )
					{
						GotHit = GR_TRUE;
						ClosestDistance = Distance;
						ClosestImpactBox = *ImpactBox;
						ClosestPlane = *Plane;
						ClosestObject = ChildSubO;
					}
				}
			} else
				if ( grObject_ChangeBoxCollision(Child, Pos, FrontBox, BackBox, nullptr, nullptr, &ChildSubO) )
					return GR_TRUE;
		}
	}

	if ( Object->Methods->ChangeBoxCollision)
	{
		if (ImpactBox && Plane)
		{
			if ( Object->Methods->ChangeBoxCollision(Object->Instance, Pos, FrontBox, BackBox, ImpactBox, Plane) )
			{
				Distance = grVec3d_DistanceBetweenSquared(&FrontBox->Min, &ImpactBox->Min);
				if ( ! GotHit || Distance < ClosestDistance )
				{
					GotHit = GR_TRUE;
					ClosestDistance = Distance;
					ClosestImpactBox = *ImpactBox;
					ClosestPlane = *Plane;
					ClosestObject = Object;
				}
			}
		} else
			if ( Object->Methods->ChangeBoxCollision(Object->Instance, Pos, FrontBox, BackBox, nullptr, nullptr) )
				return GR_TRUE;
	}

	if ( GotHit )
	{
		*ImpactBox = ClosestImpactBox;
		*Plane = ClosestPlane;
		if ( SubObject ) *SubObject = (grObject *)ClosestObject;
		return GR_TRUE;
	}

return GR_FALSE;
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_GetExtBox	(const grObject * Object,grExtBox *BBox)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->GetExtBox )
		return GR_FALSE;

	return Object->Methods->GetExtBox(Object->Instance,BBox);
}


//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_SetXForm	(grObject * Object,const grXForm3d *XF)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->SetXForm)
		return GR_FALSE;

	return Object->Methods->SetXForm(Object->Instance, XF);
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean GRCC grObject_GetXForm	(const grObject * Object,grXForm3d *XF)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->GetXForm)
		return GR_FALSE;

	return Object->Methods->GetXForm(Object->Instance,XF);
}

//====================================================================================================
//====================================================================================================
GRAPI int GRCC grObject_GetXFormModFlags( const grObject * Object )
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->GetXFormModFlags)
		return GR_FALSE;

	return Object->Methods->GetXFormModFlags(Object->Instance);
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_GetChildren(const grObject * Object,grObject * Children,int MaxNumChildren)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->GetChildren )
		return GR_FALSE;

	return Object->Methods->GetChildren(Object->Instance,Children,MaxNumChildren);
}

//====================================================================================================
//	grObject_GetNextChild
//====================================================================================================
GRAPI grObject *GRCC grObject_GetNextChild(const grObject *Object, grObject *Start)
{
	assert(Object && Object->Instance && Object->Methods);

	return (grObject *)grChain_GetNextLinkData(Object->Children, Start);
}

//====================================================================================================
//====================================================================================================
GRAPI grObject *GRCC grObject_GetParent( const grObject *Object )
{
	assert(Object && Object->Instance && Object->Methods);

	return( Object->Parent );
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_AddChild	(grObject * Object, grObject * Child)
{
/*	if (Object != nullptr)
	{
		if (Object->Instance != nullptr)
		{
			if (Object->Methods != nullptr)
			{
				if (!grChain_AddLinkData(Object->Children, (void*)Child))
					return GR_FALSE;

				if (!Object->Methods->AddChild)
					return GR_TRUE;

				//if (Object->Methods->Type != GR_OBJECT_TYPE_MODEL || Child->Methods->Type != GR_OBJECT_TYPE_ACTOR)
				//{
				if (Object != nullptr)
				{ 
				if (Object->pWorld != nullptr && Child != nullptr)
					grObject_AttachWorld(Child, Object->pWorld);
				if (Object->pEngine != nullptr && Child != nullptr)
					grObject_AttachEngine(Child, Object->pEngine);
				if (Object->pSoundSystem != nullptr && Child != nullptr)
					grObject_AttachSoundSystem(Child, Object->pSoundSystem);
				//}
				Child->Parent = Object;
				//grObject_CreateRef(Child);
				return Object->Methods->AddChild(Object->Instance, Child);
				}
			}
			return GR_FALSE;
		}
		return GR_FALSE;
	}
	return GR_FALSE;
	*/

//by trilobite

	assert(Object && Object->Instance && Object->Methods);

	assert(!grChain_FindLink(Object->Children, (void*)Child));

	if (!grChain_AddLinkData(Object->Children, (void*)Child))
		return GR_FALSE;
	
	if (!Object->Methods->AddChild)
		return GR_TRUE;

	//if (Object->Methods->Type != GR_OBJECT_TYPE_MODEL || Child->Methods->Type != GR_OBJECT_TYPE_ACTOR)
	//{
		if( Object->pWorld )
			grObject_AttachWorld( Child, Object->pWorld );
		if( Object->pEngine )
			grObject_AttachEngine( Child, Object->pEngine );
		if( Object->pSoundSystem )
			grObject_AttachSoundSystem( Child, Object->pSoundSystem );
	//}
	Child->Parent = Object;
	//grObject_CreateRef(Child);
	return Object->Methods->AddChild(Object->Instance,Child);

	
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_RemoveChild(grObject* Object, grObject* Child)
{

/*
	if (Object != nullptr)
	{
		if (!grChain_FindLink(Object->Children, (void*)Child))
			return GR_FALSE;

		if (!grChain_RemoveLinkData(Object->Children, (void*)Child))
			return GR_FALSE;
	
		if (Object != nullptr)
		{
			if (Object->Methods != nullptr)
				if (!Object->Methods->RemoveChild)
					return GR_TRUE;
		}
	}

	if (Object != nullptr && Child != nullptr)
	{
		if (Object->pWorld != nullptr)
			grObject_DettachWorld(Child, Object->pWorld);
		if (Object->pEngine != nullptr)
			grObject_DettachEngine(Child, Object->pEngine);
		if (Object->pSoundSystem != nullptr)
			grObject_DettachSoundSystem(Child, Object->pSoundSystem);
		Child->Parent = nullptr;

		return Object->Methods->RemoveChild(Object->Instance, Child);
	}

	return GR_TRUE;

	*/

	
	assert(Object && Object->Instance && Object->Methods);

	if (!grChain_FindLink(Object->Children, (void*)Child))
		return GR_FALSE;

	if (!grChain_RemoveLinkData(Object->Children, (void*)Child))
		return GR_FALSE;

	if (!Object->Methods->RemoveChild )
		return GR_TRUE;
	if( Object->pWorld )
		grObject_DettachWorld( Child, Object->pWorld );
	if( Object->pEngine )
		grObject_DettachEngine( Child, Object->pEngine );
	if( Object->pSoundSystem )
		grObject_DettachSoundSystem( Child, Object->pSoundSystem );
	Child->Parent = nullptr;

	return Object->Methods->RemoveChild(Object->Instance,Child);

	
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_EditDialog (grObject * Object,HWND Parent)
{
	assert(Object && Object->Instance && Object->Methods);

	if (!Object->Methods->EditDialog )
		return GR_FALSE;
		
	return Object->Methods->EditDialog(Object->Instance,Parent);
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_SendMessage (grObject * Object,int32 Msg, void * Data)
{
	assert(Object && Object->Instance && Object->Methods);
	
	if (!Object->Methods->SendMessage )
		return GR_FALSE;

	return Object->Methods->SendMessage(Object->Instance, Msg, Data );
}

//====================================================================================================
//====================================================================================================
GRAPI grBoolean	GRCC grObject_Frame (grObject * Object,float TimeDelta )
{
	grBoolean		Ret = GR_TRUE;
	grObject	*Object2{};
	assert(Object && Object->Instance && Object->Methods);
	

	for (Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, nullptr); Object2; Object2 = (grObject *)grChain_GetNextLinkData(Object->Children, Object2))
	{
		assert(Object2);

		if (!Object2->Methods->Frame )
			continue;

		Ret &= Object2->Methods->Frame( Object2->Instance, TimeDelta);
	}

	if (!Object->Methods->Frame )
		return Ret;
	Ret &= Object->Methods->Frame(Object->Instance, TimeDelta );
	return Ret;
}

/*}{********************** Crap ******************/

static grObject TestO = { nullptr, nullptr };

//====================================================================================================
//====================================================================================================
static void TestFunc(grObject * O)
{
	grObject_Free(O);
}

GRAPI int32 GRCC grObject_GetContents(const grObject * Object)
{
	assert(Object != nullptr);
	return Object->Contents;
}

GRAPI void GRCC grObject_SetContents(grObject * Object, int32 Contents)
{
	assert(Object != nullptr);
	Object->Contents = Contents;
}

GRAPI void GRCC grObject_SetRenderNextPass(const grObject* Object, grBoolean RenderNext)
{
	assert(Object != nullptr);

	if (Object->Methods->SetRenderNextPass)
		Object->Methods->SetRenderNextPass(Object->Instance, RenderNext);
}

GRAPI uint32 GRCC grObject_GetFlags(const grObject * Object)
{
	assert(Object != nullptr);

	return Object->Methods->Flags;
}
