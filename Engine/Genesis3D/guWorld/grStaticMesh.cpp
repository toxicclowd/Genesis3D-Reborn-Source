/**
   @file grStaticMesh.cpp
                                                                                      
   @author Anthony Rufrano	                                                          
   @brief Static mesh code     		                                          
                                                                                      
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
#include <assert.h>
#include <vector>
#include <string>
#include <windows.h>

#include "grStaticMesh.h"
#include "Ram.h"
#include "Engine.h"
#include "Camera.h"
#include "grFrustum.h"
#include "grMaterial.h"
#include "Bitmap.h"
#include "Actor.h"
#include "Body.h"
#include "Body._H"
#include "ExtBox.h"
#include "grTypes.h"

typedef struct grStaticMesh
{
	std::string							Name;
	uint32								RefCount;

	std::vector<grTLVertex>				Vertices;
	std::vector<grBody_Material>		Materials;

	grXForm3d							XForm;
	grExtBox							BBox;

	grBody_TriangleList					Faces;
} grStaticMesh;

GRAPI grStaticMesh * GRCC grStaticMesh_Create(const char *MeshName, grResourceMgr *ResMgr, grXForm3d *XForm)
{
	grActor_Def							*ActorDef = NULL;
	grBody								*Body = NULL;
	int									NumVerts, NumFaces, NumNormals;
	grStaticMesh						*Mesh = NULL;
	
	ActorDef = (grActor_Def*)grResource_Get(ResMgr, (char*)MeshName);
	if (!ActorDef)
	{
		grVFile							*Dir = NULL, *File = NULL;
		std::string						meshpath, filename, fullpath;
		std::string						name = MeshName;

		uint32 pos = name.find_first_of('.');
		meshpath = name.substr(0, pos - 1);
		filename = name.substr(pos + 1, name.size() - 1);

		fullpath = "StaticMesh\\";
		fullpath += meshpath;

		Dir = grResource_GetVFile(ResMgr, (char*)meshpath.c_str());
		if (!Dir)
		{
			std::string				temppath;

			temppath = meshpath;
			temppath += ".jetpak";

			Dir = grResource_GetVFile(ResMgr, (char*)temppath.c_str());
			if (!Dir)
			{
				Dir = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS, fullpath.c_str(), NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
				if (!Dir)
				{
					fullpath += ".jetpak";
					Dir = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_VIRTUAL, fullpath.c_str(), NULL, GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY);
					if (!Dir)
						return NULL;

					meshpath += ".jetpak";
				}

				grResource_AddVFile(ResMgr, (char*)meshpath.c_str(), Dir);
			}
		}

		File = grVFile_Open(Dir, (char*)filename.c_str(), GR_VFILE_OPEN_READONLY);
		if (!File)
			return NULL;

		ActorDef = grActor_DefCreateFromFile(File);
		if (!ActorDef)
		{
			grVFile_Close(File);
			return NULL;
		}

		grVFile_Close(File);

		grResource_Add(ResMgr, (char*)MeshName, GR_RESOURCE_ACTOR, ActorDef);
	}
    
	Body = grActor_GetBody(ActorDef);
	if (!Body)
	{
		grActor_DefDestroy(&ActorDef);
		return NULL;
	}

	grBody_GetGeometryStats(Body, 0, &NumVerts, &NumFaces, &NumNormals);

	Mesh = (grStaticMesh*)grRam_AllocateClear(sizeof(grStaticMesh));
	if (!Mesh)
	{
		grActor_DefDestroy(&ActorDef);
		return NULL;
	}

	Mesh->Name = MeshName;
	Mesh->RefCount = 1;
	
	grXForm3d_Copy(XForm, &Mesh->XForm);
	Mesh->Vertices.resize(NumVerts);
	Mesh->Materials.resize(grBody_GetMaterialCount(Body));

	memcpy(&Mesh->Faces, &Body->SkinFaces[0], sizeof(grBody_TriangleList));

	for (int32 i = 0; i < grBody_GetMaterialCount(Body); i++)
	{
		const char					*matname = NULL;

		grBody_GetMaterial(Body, i, &matname, &Mesh->Materials[i].MatSpec, &Mesh->Materials[i].Red, &Mesh->Materials[i].Green, &Mesh->Materials[i].Blue, &Mesh->Materials[i].Mapper);
	}

	//	by trilobite	Jan. 2011
	//for (i = 0; i < NumVerts; i++)
	for (int32 i = 0; i < NumVerts; i++)
	//
	{
		grVec3d						temp;

		grVec3d_Copy(&Body->XSkinVertexArray[i].XPoint, &temp);
		grXForm3d_Transform(XForm, &temp, &temp);

		Mesh->Vertices[i].x = temp.X;
		Mesh->Vertices[i].y = temp.Y;
		Mesh->Vertices[i].z = temp.Z;
		Mesh->Vertices[i].pad = 0.0f;

		Mesh->Vertices[i].u = Body->XSkinVertexArray[i].XU;
		Mesh->Vertices[i].v = Body->XSkinVertexArray[i].XV;
		Mesh->Vertices[i].pad1 = 0.0f;
		Mesh->Vertices[i].pad2 = 0.0f;

		Mesh->Vertices[i].r = Mesh->Vertices[i].g = Mesh->Vertices[i].b = Mesh->Vertices[i].a = 0.0f;
		Mesh->Vertices[i].sr = Mesh->Vertices[i].sg = Mesh->Vertices[i].sb = Mesh->Vertices[i].pad3 = 0.0f;
	}

	//	by trilobite	Jan. 2011
	//for (i = 0; i < NumFaces; i++)
	for (int32 i = 0; i < NumFaces; i++)
	//
	{
		for (int x = 0; x < 3; x++)
		{
			Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[x]].sr = Mesh->Materials[Mesh->Faces.FaceArray[i].MaterialIndex].Red;
			Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[x]].sg = Mesh->Materials[Mesh->Faces.FaceArray[i].MaterialIndex].Green;
			Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[x]].sb = Mesh->Materials[Mesh->Faces.FaceArray[i].MaterialIndex].Blue;
		}
	}

	return Mesh;
}

GRAPI uint32 GRCC grStaticMesh_CreateRef(grStaticMesh *Mesh)
{
	assert(Mesh != NULL);

	Mesh->RefCount++;
	return Mesh->RefCount;
}

GRAPI uint32 GRCC grStaticMesh_Destroy(grStaticMesh **Mesh)
{
	assert(*Mesh != NULL);

	(*Mesh)->RefCount--;
	if ((*Mesh)->RefCount == 0)
	{
		(*Mesh)->Vertices.clear();
		(*Mesh)->Materials.clear();

		grRam_Free((*Mesh));
		(*Mesh) = NULL;

		return 0;
	}

	return (*Mesh)->RefCount;
}

GRAPI grBoolean GRCC grStaticMesh_Render(grStaticMesh *Mesh, grEngine *Engine, grCamera *Camera, grFrustum *Frustum, grXForm3d *XForm)
{
	grFrustum				worldfrustum;

	assert(Mesh != NULL);
	assert(Engine != NULL);
	assert(Camera != NULL);
	assert(XForm != NULL);

	if (Frustum == NULL)
		grFrustum_SetFromCamera(&worldfrustum, Camera);
	else
		worldfrustum = *Frustum;

	for (grBody_Index i = 0; i < Mesh->Faces.FaceCount; i++)
	{
		grTLVertex					v[3];

		for (int x = 0; x < 3; x++)
		{
			for (int j = 0; j < 3; j++)
			{
				v[x].x = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].x;
				v[x].y = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].y;
				v[x].z = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].z;

				v[x].r = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].r;
				v[x].g = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].g;
				v[x].b = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].b;
				v[x].a = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].a;

				v[x].sr = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].sr;
				v[x].sg = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].sg;
				v[x].sb = Mesh->Vertices[Mesh->Faces.FaceArray[i].VtxIndex[j]].sb;
			}
		}

		//grEngine_RenderPoly(Engine, v, 3, Mesh->Materials[Mesh->Faces.FaceArray[i].MaterialIndex].Bitmap, GR_RENDER_FLAG_SPECULAR | GR_RENDER_FLAG_COUNTER_CLOCKWISE);
		grEngine_RenderPoly(Engine, v, 3, Mesh->Materials[Mesh->Faces.FaceArray[i].MaterialIndex].MatSpec, GR_RENDER_FLAG_SPECULAR | GR_RENDER_FLAG_COUNTER_CLOCKWISE);
	}

/*
// Krouer : slight moveto materialspec
#ifdef _USE_BITMAPS
		grEngine_RenderPoly(Engine, (grTLVertex*)v, 3, grMaterial_GetBitmap(Mesh->Materials[0]), GR_RENDER_FLAG_COUNTER_CLOCKWISE);
#else
*/

/*#pragma message("Krouer: think to add the grTexture support here")
#endif
*/	
	return GR_TRUE;
}

GRAPI grBoolean GRCC grStaticMesh_GetExtBox(grStaticMesh *Mesh, grExtBox *BBox)
{
	assert(Mesh != NULL);
	assert(BBox != NULL);

	BBox->Min.X = -20.0f;
	BBox->Min.Y = -20.0f;
	BBox->Min.Z = -20.0f;

	BBox->Max.X = 20.0f;
	BBox->Max.Y = 20.0f;
	BBox->Max.Z = 20.0f;

	return GR_TRUE;
}

GRAPI grBoolean GRCC grStaticMesh_SetExtBox(grStaticMesh *Mesh, grExtBox *BBox)
{
	assert(Mesh != NULL);
	assert(BBox != NULL);

	grVec3d_Copy(&Mesh->BBox.Min, &BBox->Min);
	grVec3d_Copy(&Mesh->BBox.Max, &BBox->Max);

	return GR_TRUE;
}

GRAPI grBoolean GRCC grStaticMesh_Collision(grStaticMesh *Mesh, grExtBox *BBox, grVec3d *Front, grVec3d *Back, grVec3d *Impact, grPlane *Plane)
{
	return GR_TRUE;
}
