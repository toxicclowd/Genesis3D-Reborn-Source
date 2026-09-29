/**
   @file StaticMeshObj.c
                                                                                      
   @author Anthony Rufrano	                                                          
   @brief Static mesh object code     		                                          
                                                                                      
	@par Licence
	The contents of this file are subject to the Jet3D Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.jet3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Jet3D, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <float.h>
#include "VFile.h"
#include "grProperty.h"
#include "Ram.h"
#include "grResource.h"
#include "grWorld.h"
#include "StaticMeshObj.h"
//#include "Resource.h"
#include "grStaticMesh.h"
#include "grVersion.h"
#include "Errorlog.h"

//NOTE BY TRILOBITE
//This file is not complete. Missing many functions declared in its associated ObjectDef.c file. It will compile, but will not build due to many link errors

#define STATICMESH_OBJECT_VERSION						1

enum
{
	SMESH_FILENAME_ID = PROPERTY_LOCAL_DATATYPE_START,
	SMESH_LAST_ID
};

enum
{
	SMESH_FILENAME_INDEX = 0,
	SMESH_LAST_INDEX
};

static HINSTANCE						hObjInstance = NULL;
static grProperty						StaticMeshProperties[SMESH_LAST_INDEX];
static grProperty_List					StaticMeshPropertyList = { SMESH_LAST_INDEX, &(StaticMeshProperties[0]) };
static char								**FileList = NULL;
static int								TotalFiles = 0;

typedef struct StaticMeshObject
{
	grStaticMesh						*sm;
	
	grWorld								*World;
	grResourceMgr						*ResMgr;
	grEngine							*Engine;

	grXForm3d							XForm;

	int									RefCount;
} StaticMeshObject;

static grBoolean BuildFileList(grResourceMgr *ResMgr)
{
	grVFile								*Dir = NULL, *VFS = NULL;
	grVFile_Finder						*Finder = NULL;
	int									CurrFile = 0;

	Dir = grResource_GetVFile(ResMgr, "StaticMesh");
	if (!Dir)
	{
		if (!grResource_OpenDirectory(ResMgr, "StaticMesh", "StaticMesh"))
			return GR_FALSE;

		Dir = grResource_GetVFile(ResMgr, "StaticMesh");
		if (!Dir)
			return GR_FALSE;
	}

	Finder = grVFile_CreateFinder(Dir, "*");
	if (!Finder)
		return GR_FALSE;

	while (grVFile_FinderGetNextFile(Finder) == GR_TRUE)
	{
		grVFile_Properties				Props;

		grVFile_FinderGetProperties(Finder, &Props);
		if (Props.AttributeFlags & GR_VFILE_ATTRIB_DIRECTORY)
		{
			grVFile_Finder				*DirFinder = NULL;
			grVFile						*SubDir = NULL;

			SubDir = grVFile_OpenNewSystem(Dir, GR_VFILE_TYPE_DOS, Props.Name, NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
			if (!SubDir)
			{
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			DirFinder = grVFile_CreateFinder(SubDir, "*.jsm");
			if (!DirFinder)
			{
				grVFile_Close(SubDir);
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			while (grVFile_FinderGetNextFile(DirFinder) == GR_TRUE)
			{
				TotalFiles++;
			}

			grVFile_DestroyFinder(DirFinder);
			grVFile_Close(SubDir);
		}
		else if (!strcmp(Props.Name, ".jetpak"))
		{
			grVFile						*PakFile = NULL;
			grVFile_Finder				*PakFinder = NULL;

			PakFile = grVFile_OpenNewSystem(Dir, GR_VFILE_TYPE_VIRTUAL, Props.Name, NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
			if (!PakFile)
			{
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			PakFinder = grVFile_CreateFinder(PakFile, "*.jsm");
			if (!PakFinder)
			{
				grVFile_Close(PakFile);
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			while (grVFile_FinderGetNextFile(PakFinder) == GR_TRUE)
			{
				TotalFiles++;
			}

			grVFile_DestroyFinder(PakFinder);
			grVFile_Close(PakFile);
		}
	}

	grVFile_DestroyFinder(Finder);

	FileList = grRam_AllocateClear(sizeof(char*) * TotalFiles);
	if (!FileList)
		return GR_FALSE;

	Finder = grVFile_CreateFinder(Dir, "*");
	if (!Finder)
		return GR_FALSE;

	while (grVFile_FinderGetNextFile(Finder) == GR_TRUE)
	{
		grVFile_Properties				Props;

		grVFile_FinderGetProperties(Finder, &Props);
		if (Props.AttributeFlags & GR_VFILE_ATTRIB_DIRECTORY)
		{
			grVFile_Finder				*DirFinder = NULL;
			grVFile						*SubDir = NULL;

			SubDir = grVFile_OpenNewSystem(Dir, GR_VFILE_TYPE_DOS, Props.Name, NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
			if (!SubDir)
			{
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			DirFinder = grVFile_CreateFinder(SubDir, "*.jsm");
			if (!DirFinder)
			{
				grVFile_Close(SubDir);
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			while (grVFile_FinderGetNextFile(DirFinder) == GR_TRUE)
			{
				grVFile_Properties				FileProps;

				grVFile_FinderGetProperties(DirFinder, &FileProps);
				FileList[CurrFile++] = FileProps.Name;
			}

			grVFile_DestroyFinder(DirFinder);
			grVFile_Close(SubDir);
		}
		else if (!strcmp(Props.Name, ".jetpak"))
		{
			grVFile						*PakFile = NULL;
			grVFile_Finder				*PakFinder = NULL;

			PakFile = grVFile_OpenNewSystem(Dir, GR_VFILE_TYPE_VIRTUAL, Props.Name, NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
			if (!PakFile)
			{
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			PakFinder = grVFile_CreateFinder(PakFile, "*.jsm");
			if (!PakFinder)
			{
				grVFile_Close(PakFile);
				grVFile_DestroyFinder(Finder);
				return GR_FALSE;
			}

			while (grVFile_FinderGetNextFile(PakFinder) == GR_TRUE)
			{
				grVFile_Properties				FileProps;

				grVFile_FinderGetProperties(PakFinder, &FileProps);
				FileList[CurrFile++] = FileProps.Name;
			}

			grVFile_DestroyFinder(PakFinder);
			grVFile_Close(PakFile);
		}
	}

	grVFile_DestroyFinder(Finder);
	return GR_TRUE;
}

void Init_Class(HINSTANCE hInstance)
{
	assert(hInstance != NULL);

	hObjInstance = hInstance;
}

void Deinit_Class()
{
	if (FileList)
	{
		int							i;

		for (i = 0; i < TotalFiles; i++)
			grRam_Free(FileList[i]);

		grRam_Free(FileList);
	}
}

void * GRCC CreateInstance()
{
	StaticMeshObject				*Mesh;

	Mesh = (StaticMeshObject*)grRam_AllocateClear(sizeof(StaticMeshObject));
	if (!Mesh)
		return NULL;

	Mesh->RefCount = 1;
	Mesh->sm = NULL;
	Mesh->World = NULL;
	Mesh->Engine = NULL;
	Mesh->ResMgr = NULL;
	
	grXForm3d_SetIdentity(&Mesh->XForm);

	return (void*)Mesh;
}

void GRCC CreateRef(void *Instance)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;

	assert(Mesh != NULL);

	Mesh->RefCount++;
}

grBoolean GRCC Destroy(void **Instance)
{
	StaticMeshObject			**pSM = (StaticMeshObject**)Instance;

	grStaticMesh_Destroy(&(*pSM)->sm);
	grRam_Free((*pSM));

	(*pSM) = NULL;
	return GR_TRUE;
}

grBoolean GRCC Render(const void *Instance, const grWorld *World, const grEngine *Engine, const grCamera *Camera, const grFrustum *CameraSpaceFrustum, grObject_RenderFlags RenderFlags)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;


	//ORIG commented out by trilobite
	//  return grStaticMesh_Render(Mesh->sm, (const grEngine*)Engine, (const grCamera*)Camera, (const grFrustum*)CameraSpaceFrustum, &Mesh->XForm);
	//
	// EXPERIMENTAL REPLACEMENT BELOW BY trilobite
	return grStaticMesh_Render(Mesh->sm, (grEngine*)Engine, (grCamera*)Camera, (grFrustum*)CameraSpaceFrustum, &Mesh->XForm);
	
}

grBoolean GRCC AttachWorld(void *Instance, grWorld *World)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;

	if (Mesh->World != NULL)
		DetachWorld((void*)Mesh, Mesh->World);

	Mesh->World = World;
	Mesh->ResMgr = grWorld_GetResourceMgr(World);

	return GR_TRUE;
}

grBoolean GRCC DetachWorld(void *Instance, grWorld *World)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;

	if (Mesh->World != World)
		return GR_FALSE;

	Mesh->World = NULL;
	Mesh->ResMgr = NULL;

	return GR_TRUE;
}

grBoolean GRCC AttachEngine(void *Instance, grEngine *Engine)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;

	if (Mesh->Engine)
		DetachEngine((void*)Mesh, Mesh->Engine);

	Mesh->Engine = Engine;
	return GR_TRUE;
}

grBoolean GRCC DetachEngine(void *Instance, grEngine *Engine)
{
	StaticMeshObject			*Mesh = (StaticMeshObject*)Instance;

	if (Mesh->Engine != Engine)
		return GR_FALSE;

	Mesh->Engine = NULL;

	return GR_TRUE;
}

grBoolean GRCC AttachSoundSystem(void *Instance, grSound_System *SoundSys)
{
	Instance;
	SoundSys;

	return GR_TRUE;
}

grBoolean GRCC DetachSoundSystem(void *Instance, grSound_System *SoundSys)
{
	Instance;
	SoundSys;

	return GR_TRUE;
}

grBoolean GRCC Collision(const void *Instance, const grExtBox *Box, const grVec3d *Front, const grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{
	Impact = NULL;
	Plane = NULL;
	return GR_FALSE;
}

grBoolean GRCC GetExtBox(const void *Instance, grExtBox *BBox)
{
	StaticMeshObject				*Mesh = (StaticMeshObject*)Instance;

	return grStaticMesh_GetExtBox(Mesh, BBox);
}

void * GRCC CreateFromFile(grVFile *File, grPtrMgr *PtrMgr)
{
	return NULL;
}

grBoolean GRCC WriteToFile(const void *Instance, grVFile *File, grPtrMgr *PtrMgr)
{
	// Stub implementation
	return GR_TRUE;
}

grBoolean GRCC GetPropertyList(void *Instance, grProperty_List **List)
{
	// Stub implementation
	*List = &StaticMeshPropertyList;
	return GR_TRUE;
}

grBoolean GRCC SetProperty(void *Instance, int32 FieldID, PROPERTY_FIELD_TYPE DataType, grProperty_Data *pData)
{
	// Stub implementation
	return GR_TRUE;
}

grBoolean GRCC SetXForm(void *Instance, const grXForm3d *XForm)
{
	StaticMeshObject *Mesh = (StaticMeshObject*)Instance;
	
	if (!Mesh || !XForm)
		return GR_FALSE;
		
	Mesh->XForm = *XForm;
	return GR_TRUE;
}

grBoolean GRCC GetXForm(const void *Instance, grXForm3d *XForm)
{
	const StaticMeshObject *Mesh = (const StaticMeshObject*)Instance;
	
	if (!Mesh || !XForm)
		return GR_FALSE;
		
	*XForm = Mesh->XForm;
	return GR_TRUE;
}

int GRCC GetXFormModFlags(const void *Instance)
{
	// Return that we support translation and rotation
	return (GR_OBJECT_XFORM_TRANSLATE | GR_OBJECT_XFORM_ROTATE);
}

grBoolean GRCC GetChildren(const void *Instance, grObject *Children, int MaxNumChildren)
{
	// No children support
	return GR_TRUE;
}

grBoolean GRCC AddChild(void *Instance, const grObject *Child)
{
	// No children support
	return GR_TRUE;
}

grBoolean GRCC RemoveChild(void *Instance, const grObject *Child)
{
	// No children support
	return GR_TRUE;
}

grBoolean GRCC EditDialog(void *Instance, HWND Parent)
{
	// No edit dialog yet
	return GR_TRUE;
}

grBoolean GRCC Frame(void *Instance, float TimeDelta)
{
	// Nothing to update per frame yet
	return GR_TRUE;
}

grBoolean GRCC SendAMessage(void *Instance, int32 Msg, void *Data)
{
	// No message handling yet
	return GR_FALSE;
}

grBoolean GRCC ChangeBoxCollision(const void *Instance, const grVec3d *Pos, const grExtBox *FrontBox, const grExtBox *BackBox, grExtBox *ImpactBox, grPlane *Plane)
{
	// No collision support yet
	return GR_FALSE;
}
