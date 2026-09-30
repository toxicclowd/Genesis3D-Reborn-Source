/****************************************************************************************/
/*  JEMATERIAL.C                                                                        */
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
#include <assert.h>
#include <string.h>

// Public dependents
#include "grMaterial.h"

#include "DCommon.h"
// Private dependents
#include "grMaterial._h"
#include "Errorlog.h"
#include "Array.h"
#include "Ram.h"
#include "Engine.h"

#include "grGArray.h"

#include "grResource.h"

#include "log.h"
#include "grTexture.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)


//=======================================================================================
//	** grMaterialSpec **
//=======================================================================================
#define MATSPEC_DIFFUSE_FLAG	0x0001
#define MATSPEC_SPECULAR_FLAG	0x0002
#define MATSPEC_AMBIENT_FLAG	0x0004
#define MATSPEC_EMISSIVE_FLAG	0x0008
#define MATSPEC_SHADER_FLAG		0x0010
#define MATSPEC_THUMBS_FLAG		0x0020
#define MATSPEC_SIZE_FLAG		0x0040
#define MATSPEC_PBR_FLAG		0x0080		// version 2: a grMaterialSpec_PBR block follows the size

//=======================================================================================
//	grMaterialSpec_Create
//=======================================================================================
GRAPI grMaterialSpec * GRCC grMaterialSpec_Create(grEngine* pEngine, grResourceMgr* pResourceMgr)
{
	grMaterialSpec	*MaterialSpec;

	MaterialSpec = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec);

	if (!MaterialSpec)
		return NULL;

	ZeroMem(MaterialSpec);
	MaterialSpec->pEngine = pEngine;

	MaterialSpec->RefCnt = 1;

	return MaterialSpec;
}

GRAPI grBoolean GRCC grMaterialSpec_CreateRef(grMaterialSpec* MaterialSpec)
{
	if (MaterialSpec) {
		MaterialSpec->RefCnt++;
		return GR_TRUE;
	}
	return GR_FALSE;
}

//=======================================================================================
//	grMaterialSpec_Destroy
//=======================================================================================
GRAPI void GRCC grMaterialSpec_Destroy(grMaterialSpec **MaterialSpec)
{
	int idx;
	grMaterialSpec* pMatSpec = *MaterialSpec;

	if (!(--pMatSpec->RefCnt)) {
		// empty the layers
		for (idx=0; idx<pMatSpec->LayerCounts; idx++) {
			if (pMatSpec->pLayers[idx]) {
				if (pMatSpec->pLayers[idx]->Kind == GR_RESOURCE_BITMAP) {
					grBitmap_Destroy(&pMatSpec->pLayers[idx]->pBitmap);
					grResource_Delete(grResourceMgr_GetSingleton(), pMatSpec->pLayers[idx]->Name);
				} else {
					grResource_ReleaseResource(grResourceMgr_GetSingleton(), pMatSpec->pLayers[idx]->Kind, pMatSpec->pLayers[idx]->Name);
				}

				grRam_Free(pMatSpec->pLayers[idx]);
			}
		}

		// destroy the shader

		// free the resource
		grRam_Free(*MaterialSpec);

		*MaterialSpec = NULL;
	}
}

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
		((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define GR_MATSPEC_TAG			MAKEFOURCC('J', 'M', 'A', 'T')		// 'J' 'MAT'erial definition
#define GR_MATSPEC_VERSION		0x0002		// 2 adds the PBR block; files without one are still written as 1

//========================================================================================
//	grMaterialSpec_CreateFromFile
//========================================================================================
GRAPI grMaterialSpec* GRCC grMaterialSpec_CreateFromFile(grVFile *VFile, grEngine* pEngine, grResourceMgr *ResMgr)
{
	uint8 Version;
	uint8 FileVersion;
	uint16 Flags;
	uint32 Tag;
	grRGBA Color;
	char ShaderName[GR_MATERIAL_MAX_NAME_SIZE];
	grMaterialSpec	*MaterialSpec;

	MaterialSpec = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec);

	if (!MaterialSpec) {
		goto ExitInError;
	}
	ZeroMem(MaterialSpec);

	MaterialSpec->pEngine = pEngine;
	MaterialSpec->RefCnt = 1;
	MaterialSpec->Height = MaterialSpec->Width = 0;

	// Read the JMAT tag
	if (!grVFile_Read(VFile, &Tag, sizeof(Tag))) {
		goto ExitInError;
	}
	if (Tag != GR_MATSPEC_TAG) {
		goto ExitInError;
	}

	// Read the JMAT version
	if (!grVFile_Read(VFile, &Version, sizeof(Version))) {
		goto ExitInError;
	}
	// Error if the version is not supported
	if (Version > GR_MATSPEC_VERSION) {
		goto ExitInError;
	}
	FileVersion = Version;

	// Read the material spec flags
	if (!grVFile_Read(VFile, &Flags, sizeof(Flags))) {
		goto ExitInError;
	}
	MaterialSpec->Flags = Flags;

	// Read the layer count
	if (!grVFile_Read(VFile, &Version, sizeof(Version))) {
		goto ExitInError;
	}
	MaterialSpec->LayerCounts = Version;

	// Read the color and assign them
	// 1st Read the Diffuse color if specified in the flags
	if (MaterialSpec->Flags&MATSPEC_DIFFUSE_FLAG) {
		if (!grVFile_Read(VFile, &Color, sizeof(Color))) {
			goto ExitInError;
		}
		MaterialSpec->Diffuse = Color;
	}

	// 2nd Read the Specular color if specified in the flags
	if (MaterialSpec->Flags&MATSPEC_SPECULAR_FLAG) {
		if (!grVFile_Read(VFile, &Color, sizeof(Color))) {
			goto ExitInError;
		}
		MaterialSpec->Specular = Color;
	}

	// 3rd Read the Ambient color if specified in the flags
	if (MaterialSpec->Flags&MATSPEC_AMBIENT_FLAG) {
		if (!grVFile_Read(VFile, &Color, sizeof(Color))) {
			goto ExitInError;
		}
		MaterialSpec->Ambient = Color;
	}

	// 4th Read the Emissive color if specified in the flags
	if (MaterialSpec->Flags&MATSPEC_EMISSIVE_FLAG) {
		if (!grVFile_Read(VFile, &Color, sizeof(Color))) {
			goto ExitInError;
		}
		MaterialSpec->Emissive = Color;
	}

	// Read the Shader name if specified in the flags
	if (MaterialSpec->Flags&MATSPEC_SHADER_FLAG) {
		if (!grVFile_Read(VFile, ShaderName, GR_MATERIAL_MAX_NAME_SIZE)) {
			goto ExitInError;
		}
		MaterialSpec->pShader = (grShader*) grResource_GetResource(grResourceMgr_GetSingleton(), GR_RESOURCE_SHADER, ShaderName);
	}

	if (MaterialSpec->Flags&MATSPEC_SIZE_FLAG) {
		grVFile_Read(VFile, &MaterialSpec->Width, sizeof(MaterialSpec->Width));
		grVFile_Read(VFile, &MaterialSpec->Height, sizeof(MaterialSpec->Height));
	}

	if (FileVersion >= 2 && (MaterialSpec->Flags&MATSPEC_PBR_FLAG)) {
		if (!grVFile_Read(VFile, &MaterialSpec->PBR, sizeof(MaterialSpec->PBR))) {
			goto ExitInError;
		}
	} else {
		MaterialSpec->Flags &= ~MATSPEC_PBR_FLAG;
	}

	// Read the layer descriptions
	for (Version=0; Version<MaterialSpec->LayerCounts; Version++) {
		grMaterialSpec_Layer* pLayer = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Layer);
		grXForm3d_SetIdentity(&pLayer->XForm);

		// Read the resource type
		if (!grVFile_Read(VFile, &pLayer->Kind, sizeof(pLayer->Kind))) {
			goto ExitInError;
		}

		// Read the layer type
		if (!grVFile_Read(VFile, &pLayer->Type, sizeof(pLayer->Type))) {
			goto ExitInError;
		}

		// Read the layer UV mapper
		if (!grVFile_Read(VFile, &pLayer->UVMapID, sizeof(pLayer->UVMapID))) {
			goto ExitInError;
		}

		// Read the Layer resource name
		if (!grVFile_Read(VFile, pLayer->Name, GR_MATERIAL_MAX_NAME_SIZE)) {
			goto ExitInError;
		}

		// Create the texture from the resource manager
		pLayer->pTexture = (grTexture*) grResource_GetResource(grResourceMgr_GetSingleton(), pLayer->Kind, pLayer->Name);

		if (!grVFile_Read(VFile, &pLayer->XForm, sizeof(pLayer->XForm))) {
			goto ExitInError;
		}

		if (pLayer->Kind == GR_RESOURCE_BITMAP) {
			grBitmap_CreateRef((grBitmap*)pLayer->pTexture);
		}

		MaterialSpec->pLayers[Version] = pLayer;
	}

	// Write the thumbnail image
	if (MaterialSpec->Flags&MATSPEC_THUMBS_FLAG) {
		MaterialSpec->pThumbnail = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Thumbnail);
		if (MaterialSpec->pThumbnail == NULL) {
			goto ExitInError;
		}

		if (!grVFile_Read(VFile, &MaterialSpec->pThumbnail->width, sizeof(MaterialSpec->pThumbnail->width))) {
			goto ExitInError;
		}
		if (!grVFile_Read(VFile, &MaterialSpec->pThumbnail->height, sizeof(MaterialSpec->pThumbnail->height))) {
			goto ExitInError;
		}
		Tag = MaterialSpec->pThumbnail->width*MaterialSpec->pThumbnail->height*3;
		MaterialSpec->pThumbnail->contents = (uint8 *)grRam_Allocate(Tag);
		if (!grVFile_Read(VFile, MaterialSpec->pThumbnail->contents, Tag)) {
			goto ExitInError;
		}
	}

	if (MaterialSpec->Height == 0 || MaterialSpec->Width == 0) {
		grMaterialSpec_Layer* pLayer;

		// calc the with and height from layers
		MaterialSpec->Flags |= MATSPEC_SIZE_FLAG;

		pLayer = MaterialSpec->pLayers[0];
		if (pLayer->Kind == GR_RESOURCE_BITMAP) {
			MaterialSpec->Width = (uint16) grBitmap_Width((grBitmap*)pLayer->pTexture);
			MaterialSpec->Height = (uint16) grBitmap_Height((grBitmap*)pLayer->pTexture);
		} else {
			grTexture_Info texInfo;
			grTexture_GetInfo(MaterialSpec->pEngine, pLayer->pTexture, 0, &texInfo);
			MaterialSpec->Width = (uint16) texInfo.Width;
			MaterialSpec->Height = (uint16) texInfo.Height;
		}
	}

	return MaterialSpec;


ExitInError:
	if (MaterialSpec) {
		grMaterialSpec_Destroy(&MaterialSpec);
	}
	return NULL;
}

GRAPI grBoolean GRCC grMaterialSpec_WriteToFile(grMaterialSpec* MatSpec, grVFile *VFile)
{
	uint8 Version;
	uint32 Tag;
	assert(MatSpec);

	// Write the JMAT tag
	Tag = GR_MATSPEC_TAG;
	if (!grVFile_Write(VFile, &Tag, sizeof(Tag))) {
		return GR_FALSE;
	}

	// Write the JMAT version
	// Only materials with PBR parameters need version 2, so older engines still read the rest
	Version = (MatSpec->Flags&MATSPEC_PBR_FLAG) ? GR_MATSPEC_VERSION : 1;
	if (!grVFile_Write(VFile, &Version, sizeof(Version))) {
		return GR_FALSE;
	}

#pragma message ("Krouer: Remove write shader until they are implemented. Remove the line below when done")
	MatSpec->Flags &= ~MATSPEC_SHADER_FLAG;
	// Write the material spec flags
	if (!grVFile_Write(VFile, &MatSpec->Flags, sizeof(MatSpec->Flags))) {
		return GR_FALSE;
	}

	// Write the layer count
	Version = (uint8) MatSpec->LayerCounts;
	if (!grVFile_Write(VFile, &Version, sizeof(Version))) {
		return GR_FALSE;
	}

	// Write the colors
	// 1st Write the Diffuse color if specified in the flags
	if (MatSpec->Flags&MATSPEC_DIFFUSE_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->Diffuse, sizeof(MatSpec->Diffuse))) {
			return GR_FALSE;
		}
	}

	// 2nd Write the Specular color if specified in the flags
	if (MatSpec->Flags&MATSPEC_SPECULAR_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->Specular, sizeof(MatSpec->Specular))) {
			return GR_FALSE;
		}
	}

	// 3rd Write the Ambient color if specified in the flags
	if (MatSpec->Flags&MATSPEC_AMBIENT_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->Ambient, sizeof(MatSpec->Ambient))) {
			return GR_FALSE;
		}
	}

	// 4th Write the Emissive color if specified in the flags
	if (MatSpec->Flags&MATSPEC_EMISSIVE_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->Emissive, sizeof(MatSpec->Emissive))) {
			return GR_FALSE;
		}
	}

	// Write the Shader name if specified in the flags
	if (MatSpec->Flags&MATSPEC_SHADER_FLAG) {
		//char ShaderName[GR_MATERIAL_MAX_NAME_SIZE];
#pragma message ("Krouer: grShader must have access on their physical name: grShader_GetName")
		//grShader_GetName(MatSpec->pShader, ShaderName, GR_MATERIAL_MAX_NAME_SIZE);

		/*
		if (!grVFile_Write(VFile, ShaderName, GR_MATERIAL_MAX_NAME_SIZE)) {
			return GR_FALSE;
		}
		*/
	}

	if (MatSpec->Flags&MATSPEC_SIZE_FLAG) {
		grVFile_Write(VFile, &MatSpec->Width, sizeof(MatSpec->Width));
		grVFile_Write(VFile, &MatSpec->Height, sizeof(MatSpec->Height));
	}

	if (MatSpec->Flags&MATSPEC_PBR_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->PBR, sizeof(MatSpec->PBR))) {
			return GR_FALSE;
		}
	}

	// Write the layers descriptions
	for (Version=0; Version<MatSpec->LayerCounts; Version++) {
		grMaterialSpec_Layer* pLayer = MatSpec->pLayers[Version];

		// Write the resource type
		if (!grVFile_Write(VFile, &pLayer->Kind, sizeof(pLayer->Kind))) {
			return GR_FALSE;
		}

		// Write the layer type
		if (!grVFile_Write(VFile, &pLayer->Type, sizeof(pLayer->Type))) {
			return GR_FALSE;
		}

		// Write the layer UV mapper
		if (!grVFile_Write(VFile, &pLayer->UVMapID, sizeof(pLayer->UVMapID))) {
			return GR_FALSE;
		}

		// Write the Layer resource name
		if (!grVFile_Write(VFile, pLayer->Name, GR_MATERIAL_MAX_NAME_SIZE)) {
			return GR_FALSE;
		}

		// Write the XForm matrix of the layer
		if (!grVFile_Write(VFile, &pLayer->XForm, sizeof(pLayer->XForm))) {
			return GR_FALSE;
		}
	}

	// Write the thumbnail image
	if (MatSpec->Flags&MATSPEC_THUMBS_FLAG) {
		if (!grVFile_Write(VFile, &MatSpec->pThumbnail->width, sizeof(MatSpec->pThumbnail->width))) {
			return GR_FALSE;
		}
		if (!grVFile_Write(VFile, &MatSpec->pThumbnail->height, sizeof(MatSpec->pThumbnail->height))) {
			return GR_FALSE;
		}
		if (!grVFile_Write(VFile, MatSpec->pThumbnail->contents, (MatSpec->pThumbnail->width*MatSpec->pThumbnail->height*3))) {
			return GR_FALSE;
		}
	}

	return GR_TRUE;
}

GRAPI uint32 GRCC grMaterialSpec_GetLayerCount(const grMaterialSpec* MatSpec)
{
	assert(MatSpec);
	return MatSpec->LayerCounts;
}

GRAPI grBoolean GRCC grMaterialSpec_AddLayer(grMaterialSpec* MatSpec, int32 layerIndex, int32 Kind, grMaterialSpec_LayerType layerType, int32 layerMapper, const char* LayerName)
{
	grMaterialSpec_Layer* pLayer;
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return GR_FALSE;
	}

	pLayer = MatSpec->pLayers[layerIndex];
	
	if (pLayer) {
#pragma message ("Krouer: grTexture_Destroy must have access on their physical name: grShader_GetName")
		//grTexture_Destroy(MatSpec->pEngine, pLayer->pTexture);
	} else {
		pLayer = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Layer);
		ZeroMem(pLayer);
		MatSpec->pLayers[layerIndex] = pLayer;
		MatSpec->LayerCounts++;
	}
	strcpy(pLayer->Name, LayerName);
	pLayer->Kind = (uint16) Kind;
	grXForm3d_SetIdentity(&pLayer->XForm);

	pLayer->pTexture = (grTexture*) grResource_GetResource(grResourceMgr_GetSingleton(), pLayer->Kind, pLayer->Name);

	if (pLayer->Kind == GR_RESOURCE_BITMAP) {
		grBitmap_CreateRef((grBitmap*)pLayer->pTexture);
	}

	pLayer->Type = (uint8) layerType;
	pLayer->UVMapID = (uint8) layerMapper;

	if (pLayer->pTexture) {
		return GR_TRUE;
	}
	return GR_FALSE;
}

GRAPI grBoolean GRCC grMaterialSpec_RemoveLayer(grMaterialSpec* MatSpec, int32 layerIndex)
{
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return GR_FALSE;
	}

	if (MatSpec->pLayers[layerIndex]) {
#pragma message ("Krouer: grTexture_Destroy must have access on their physical name: grShader_GetName")
		//grTexture_Destroy(MatSpec->pEngine, pLayer->pTexture);
		if (MatSpec->pLayers[layerIndex]->Kind == GR_RESOURCE_BITMAP) {
			grBitmap_Destroy((grBitmap**)&MatSpec->pLayers[layerIndex]->pTexture);
		}

		// Free the slot
		grRam_Free(MatSpec->pLayers[layerIndex]);
		MatSpec->pLayers[layerIndex] = NULL;

		// Modify the counter
		MatSpec->LayerCounts--;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grMaterialSpec_SetLayerTransform(grMaterialSpec* MatSpec, int32 layerIndex, grXForm3d* LayerXFrom)
{
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return GR_FALSE;
	}

	if (MatSpec->pLayers[layerIndex]) {
		// assign new matrix
		MatSpec->pLayers[layerIndex]->XForm=*LayerXFrom;
		return GR_TRUE;
	}
	return GR_FALSE;
}

GRAPI grBoolean GRCC grMaterialSpec_SetShader(grMaterialSpec* MatSpec, grShader* Shader)
{
	assert(MatSpec);
	if (MatSpec->pShader == Shader) return GR_TRUE;
	if (MatSpec->pShader) {
#pragma message ("Krouer: grShader must have access a destroy function: grShader_Destroy")
		/* Krouer: wait for new grShader implementation
		grShader_Destroy(&MatSpec->pShader); */
	}
	MatSpec->pShader = Shader;
	if (MatSpec->pShader) {
#pragma message ("Krouer: grShader must have access to a create ref: grShader_CreateRef")
		/* Krouer: wait for new grShader implementation
		grShader_CreateRef(Shader); */
		MatSpec->Flags |= MATSPEC_SHADER_FLAG;
	} else {
		MatSpec->Flags &= ~MATSPEC_SHADER_FLAG;
	}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grMaterialSpec_SetColor(grMaterialSpec* MatSpec, int32 ColorIndex, grRGBA* Color)
{
	assert(MatSpec);
	switch (ColorIndex) {
	case GR_MATERIALSPEC_DIFFUSE_INDEX:
		if (Color) {
			MatSpec->Flags |= MATSPEC_DIFFUSE_FLAG;
			MatSpec->Diffuse = *Color;
		} else {
			MatSpec->Flags &= ~MATSPEC_DIFFUSE_FLAG;
		}
		break;
	case GR_MATERIALSPEC_SPECULAR_INDEX:
		if (Color) {
			MatSpec->Flags |= MATSPEC_SPECULAR_FLAG;
			MatSpec->Specular = *Color;
		} else {
			MatSpec->Flags &= ~MATSPEC_SPECULAR_FLAG;
		}
		break;
	case GR_MATERIALSPEC_AMBIENT_INDEX:
		if (Color) {
			MatSpec->Flags |= MATSPEC_AMBIENT_FLAG;
			MatSpec->Ambient = *Color;
		} else {
			MatSpec->Flags &= ~MATSPEC_AMBIENT_FLAG;
		}
		break;
	case GR_MATERIALSPEC_EMISSIVE_INDEX:
		if (Color) {
			MatSpec->Flags |= MATSPEC_EMISSIVE_FLAG;
			MatSpec->Emissive = *Color;
		} else {
			MatSpec->Flags &= ~MATSPEC_EMISSIVE_FLAG;
		}
		break;
	default:
		return GR_FALSE;
	}
	return GR_TRUE;
}

GRAPI grShader* GRCC grMaterialSpec_GetShader(const grMaterialSpec* MatSpec)
{
	assert(MatSpec);
	if (MATSPEC_SHADER_FLAG&MatSpec->Flags) return MatSpec->pShader;
	return NULL;
}

GRAPI grTexture* GRCC grMaterialSpec_GetLayerTexture(const grMaterialSpec* MatSpec, int32 layerIndex)
{
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return NULL;
	}

	if (MatSpec->pLayers[layerIndex] && MatSpec->pLayers[layerIndex]->Kind != GR_RESOURCE_BITMAP) {
		// return the texture
		return MatSpec->pLayers[layerIndex]->pTexture;
	}
	return NULL;
}

GRAPI grBitmap* GRCC grMaterialSpec_GetLayerBitmap(const grMaterialSpec* MatSpec, int32 layerIndex)
{
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return NULL;
	}

	if (MatSpec->pLayers[layerIndex] && MatSpec->pLayers[layerIndex]->Kind == GR_RESOURCE_BITMAP) {
		// return the texture
		return MatSpec->pLayers[layerIndex]->pBitmap;
	}
	return NULL;
}

GRAPI int32 GRCC grMaterialSpec_GetLayerType(const grMaterialSpec* MatSpec, int32 layerIndex)
{
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0 || !MatSpec->pLayers[layerIndex]) {
		return -1;
	}
	return MatSpec->pLayers[layerIndex]->Type;
}

GRAPI int32 GRCC grMaterialSpec_FindLayer(const grMaterialSpec* MatSpec, grMaterialSpec_LayerType layerType)
{
	int32 idx;

	assert(MatSpec);

	for (idx=0; idx<GR_MATERIAL_MAX_LAYER; idx++) {
		if (MatSpec->pLayers[idx] && MatSpec->pLayers[idx]->Type == (uint8)layerType) {
			return idx;
		}
	}
	return -1;
}

GRAPI grBoolean GRCC grMaterialSpec_IsPBR(const grMaterialSpec* MatSpec)
{
	assert(MatSpec);

	return (MatSpec->Flags&MATSPEC_PBR_FLAG)
		|| grMaterialSpec_FindLayer(MatSpec, GR_MATERIAL_LAYER_NORMAL) >= 0
		|| grMaterialSpec_FindLayer(MatSpec, GR_MATERIAL_LAYER_ORM) >= 0
		|| grMaterialSpec_FindLayer(MatSpec, GR_MATERIAL_LAYER_EMISSIVE) >= 0;
}

GRAPI void GRCC grMaterialSpec_DefaultPBR(grMaterialSpec_PBR* PBR)
{
	assert(PBR);

	ZeroMem(PBR);
	PBR->BaseColor[0] = PBR->BaseColor[1] = PBR->BaseColor[2] = PBR->BaseColor[3] = 1.0f;
	PBR->Roughness = 1.0f;
	PBR->Metal = 1.0f;
	PBR->EmissiveIntensity = 1.0f;
	PBR->AlphaCutoff = 0.5f;
	PBR->AlphaMode = GR_MATERIAL_ALPHA_OPAQUE;
}

GRAPI grBoolean GRCC grMaterialSpec_GetPBR(const grMaterialSpec* MatSpec, grMaterialSpec_PBR* PBR)
{
	assert(MatSpec);
	assert(PBR);

	if (MatSpec->Flags&MATSPEC_PBR_FLAG) {
		*PBR = MatSpec->PBR;
		return GR_TRUE;
	}
	grMaterialSpec_DefaultPBR(PBR);
	// Without parameters the maps alone decide: no ORM map means a dielectric, and an
	// emissive map shows as it is.
	if (grMaterialSpec_FindLayer(MatSpec, GR_MATERIAL_LAYER_ORM) < 0) {
		PBR->Metal = 0.0f;
	}
	if (grMaterialSpec_FindLayer(MatSpec, GR_MATERIAL_LAYER_EMISSIVE) >= 0) {
		PBR->Emissive[0] = PBR->Emissive[1] = PBR->Emissive[2] = 1.0f;
	}
	return GR_FALSE;
}

GRAPI grBoolean GRCC grMaterialSpec_SetPBR(grMaterialSpec* MatSpec, const grMaterialSpec_PBR* PBR)
{
	assert(MatSpec);

	if (PBR) {
		MatSpec->PBR = *PBR;
		MatSpec->Flags |= MATSPEC_PBR_FLAG;
	} else {
		ZeroMem(&MatSpec->PBR);
		MatSpec->Flags &= ~MATSPEC_PBR_FLAG;
	}
	return GR_TRUE;
}

GRAPI grXForm3d* GRCC grMaterialSpec_GetLayerTransform(const grMaterialSpec* MatSpec, int32 layerIndex)
{
	assert(MatSpec);
	assert(MatSpec);

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return NULL;
	}

	if (MatSpec->pLayers[layerIndex]) {
		// return the texture
		return &MatSpec->pLayers[layerIndex]->XForm;
	}
	return NULL;
}

GRAPI grRGBA* GRCC grMaterialSpec_GetColor(const grMaterialSpec* MatSpec, int32 colorIndex)
{
	grMaterialSpec* MS;
	assert(MatSpec);
	
	MS = (grMaterialSpec*) MatSpec;
	if (colorIndex==GR_MATERIALSPEC_DIFFUSE_INDEX && (MatSpec->Flags&MATSPEC_DIFFUSE_FLAG)) return &MS->Diffuse;
	if (colorIndex==GR_MATERIALSPEC_SPECULAR_INDEX && (MatSpec->Flags&MATSPEC_SPECULAR_FLAG)) return &MS->Specular;
	if (colorIndex==GR_MATERIALSPEC_AMBIENT_INDEX && (MatSpec->Flags&MATSPEC_AMBIENT_FLAG)) return &MS->Ambient;
	if (colorIndex==GR_MATERIALSPEC_EMISSIVE_INDEX && (MatSpec->Flags&MATSPEC_EMISSIVE_FLAG)) return &MS->Emissive;
	return NULL;
}

GRAPI uint32 GRCC grMaterialSpec_GetColors(const grMaterialSpec* MatSpec, grRGBA* Diffuse, grRGBA* Specular, grRGBA* Ambient, grRGBA* Emissive)
{
	uint32 colormask;
	assert(MatSpec);

	colormask = 0;

	if (MatSpec->Flags&MATSPEC_DIFFUSE_FLAG) {
		*Diffuse = MatSpec->Diffuse;
		colormask |= MATSPEC_DIFFUSE_FLAG;
	}
	if (MatSpec->Flags&MATSPEC_SPECULAR_FLAG) {
		*Specular = MatSpec->Specular;
		colormask |= MATSPEC_SPECULAR_FLAG;
	}
	if (MatSpec->Flags&MATSPEC_AMBIENT_FLAG) {
		*Ambient = MatSpec->Ambient;
		colormask |= MATSPEC_AMBIENT_FLAG;
	}
	if (MatSpec->Flags&MATSPEC_EMISSIVE_FLAG) {
		*Emissive = MatSpec->Emissive;
		colormask |= MATSPEC_EMISSIVE_FLAG;
	}
	return colormask;
}

GRAPI grMaterialSpec_Thumbnail* GRCC grMaterialSpec_GetThumbnail(const grMaterialSpec* MatSpec)
{
	assert(MatSpec);
	return MatSpec->pThumbnail;
}

GRAPI grBoolean GRCC grMaterialSpec_SetThumbnail(grMaterialSpec* MatSpec, grMaterialSpec_Thumbnail* pThumb)
{
	int32 ThumbSize;
	assert(MatSpec);
	
	MatSpec->pThumbnail = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Thumbnail);
	if (MatSpec->pThumbnail == NULL) {
		return GR_FALSE;
	}

	ThumbSize = pThumb->width*pThumb->height*3;
	MatSpec->pThumbnail->contents = (uint8 *) grRam_Allocate(ThumbSize);
	if (MatSpec->pThumbnail->contents == NULL) {
		return GR_FALSE;
	}
	MatSpec->pThumbnail->width = pThumb->width;
	MatSpec->pThumbnail->height = pThumb->height;
	memcpy(MatSpec->pThumbnail->contents, pThumb->contents, ThumbSize);

	MatSpec->Flags |= MATSPEC_THUMBS_FLAG;
	return GR_TRUE;
}

GRAPI uint32 GRCC grMaterialSpec_Height(const grMaterialSpec* MatSpec)
{
	if (MatSpec->Height == 0 || MatSpec->Width == 0) {
		grMaterialSpec_Layer* pLayer;

		// calc the with and height from layers
		((grMaterialSpec*)MatSpec)->Flags |= MATSPEC_SIZE_FLAG;

		pLayer = MatSpec->pLayers[0];
		if (pLayer->Kind == GR_RESOURCE_BITMAP) {
			((grMaterialSpec*)MatSpec)->Width = (uint16) grBitmap_Width((grBitmap*)pLayer->pTexture);
			((grMaterialSpec*)MatSpec)->Height = (uint16) grBitmap_Height((grBitmap*)pLayer->pTexture);
		} else {
			grTexture_Info texInfo;
			grTexture_GetInfo(MatSpec->pEngine, pLayer->pTexture, 0, &texInfo);
			((grMaterialSpec*)MatSpec)->Width = (uint16) texInfo.Width;
			((grMaterialSpec*)MatSpec)->Height = (uint16) texInfo.Height;
		}
	}
	return MatSpec->Height;
}

GRAPI uint32 GRCC grMaterialSpec_Width(const grMaterialSpec* MatSpec)
{
	if (MatSpec->Height == 0 || MatSpec->Width == 0) {
		grMaterialSpec_Layer* pLayer;

		// calc the with and height from layers
		((grMaterialSpec*)MatSpec)->Flags |= MATSPEC_SIZE_FLAG;

		pLayer = MatSpec->pLayers[0];
		if (pLayer->Kind == GR_RESOURCE_BITMAP) {
			((grMaterialSpec*)MatSpec)->Width = (uint16) grBitmap_Width((grBitmap*)pLayer->pTexture);
			((grMaterialSpec*)MatSpec)->Height = (uint16) grBitmap_Height((grBitmap*)pLayer->pTexture);
		} else {
			grTexture_Info texInfo;
			grTexture_GetInfo(MatSpec->pEngine, pLayer->pTexture, 0, &texInfo);
			((grMaterialSpec*)MatSpec)->Width = (uint16) texInfo.Width;
			((grMaterialSpec*)MatSpec)->Height = (uint16) texInfo.Height;
		}
	}
	return MatSpec->Width;
}

GRAPI grBoolean GRCC grMaterialSpec_AddLayerFromFile(grMaterialSpec* MatSpec, int32 layerIndex, grVFile *File, grBoolean UseColorKey, uint32 ColorKey)
{
	grBitmap* bmp;

	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return GR_FALSE;
	}

	MatSpec->pLayers[layerIndex] = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Layer);
	if ((bmp = grBitmap_CreateFromFile(File)) == NULL) {
		MatSpec->pLayers[layerIndex]->pTexture = grTexture_CreateFromFile(MatSpec->pEngine, File);
		MatSpec->pLayers[layerIndex]->Kind = GR_RESOURCE_TEXTURE;
	} else {
		MatSpec->pLayers[layerIndex]->pTexture = (grTexture*) bmp;
        grBitmap_SetColorKey(bmp, UseColorKey, ColorKey, GR_TRUE);

        MatSpec->pLayers[layerIndex]->Kind = GR_RESOURCE_BITMAP;
		grEngine_AddBitmap(MatSpec->pEngine, bmp, GR_ENGINE_BITMAP_TYPE_3D);
	}
	if (MatSpec->pLayers[layerIndex]->pTexture==NULL) {
		grRam_Free(MatSpec->pLayers[layerIndex]);
		return GR_FALSE;
	}

	grXForm3d_SetIdentity(&MatSpec->pLayers[layerIndex]->XForm);
	MatSpec->pLayers[layerIndex]->Type = 0;
	MatSpec->pLayers[layerIndex]->UVMapID = 0;
	MatSpec->LayerCounts++;

	return GR_TRUE;
}

GRAPI grBoolean GRCC grMaterialSpec_AddLayerFromBitmap(grMaterialSpec* MatSpec, int32 layerIndex, grBitmap* pBitmap, const char* ResName)
{
	if (layerIndex>=GR_MATERIAL_MAX_LAYER || layerIndex<0) {
		return GR_FALSE;
	}

    // Store the bitmap in the Texture slot
	MatSpec->pLayers[layerIndex] = GR_RAM_ALLOCATE_STRUCT(grMaterialSpec_Layer);
	MatSpec->pLayers[layerIndex]->pTexture = (grTexture*) pBitmap;
	MatSpec->pLayers[layerIndex]->Kind = GR_RESOURCE_BITMAP;
    // if trying to create a Material with a NULL bitmap
	if (MatSpec->pLayers[layerIndex]->pTexture==NULL) {
		grRam_Free(MatSpec->pLayers[layerIndex]);
		return GR_FALSE;
	}

    memset(MatSpec->pLayers[layerIndex]->Name, 0, GR_MATERIAL_MAX_NAME_SIZE);
    if (ResName) {
        strcpy(MatSpec->pLayers[layerIndex]->Name, ResName);
    }

    // To avoid problem, add a ref on the bitmap
    grBitmap_CreateRef(pBitmap);
    if (layerIndex == 0) {
        MatSpec->Width = grBitmap_Width(pBitmap);
        MatSpec->Height = grBitmap_Height(pBitmap);
    }

	grXForm3d_SetIdentity(&MatSpec->pLayers[layerIndex]->XForm);
	MatSpec->pLayers[layerIndex]->Type = 0;
	MatSpec->pLayers[layerIndex]->UVMapID = 0;
	MatSpec->LayerCounts++;

	return GR_TRUE;
}