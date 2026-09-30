/*!
	@file grMaterial.h 
	
	@author John Pollard
	@brief Material accessor and Material array usage

	@par Licence
	The contents of this file are subject to the Genesis3D: Reborn Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.genesis3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Genesis3D: Reborn, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/

#ifndef GR_MATERIAL_H
#define GR_MATERIAL_H

#include "grTypes.h"
#include "Array.h"
#include "Bitmap.h"
#include "VFile.h"
#include "Engine.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

//=======================================================================================
//	
//=======================================================================================
/*! @def GR_MATERIAL_ARRAY_NULL_INDEX
	@brief Define the NULL index indicator
*/
#define GR_MATERIAL_ARRAY_NULL_INDEX	GR_ARRAY_NULL_INDEX

/*! @def GR_MATERIAL_MAX_NAME_SIZE
	@brief Define the maximal lenght of a material name
*/
#define GR_MATERIAL_MAX_NAME_SIZE		256

//=======================================================================================
//	Function prototypes
//=======================================================================================
/*! @name grMaterialSpec related functions and data
	@{
*/
/*! @def GR_MATERIALSPEC_DIFFUSE_INDEX
	@brief Define the diffuse color index for function grMaterialSpec_SetColor
*/
#define GR_MATERIALSPEC_DIFFUSE_INDEX		0
/*! @def GR_MATERIALSPEC_SPECULAR_INDEX
	@brief Define the specular color index for function grMaterialSpec_SetColor
*/
#define GR_MATERIALSPEC_SPECULAR_INDEX		1
/*! @def GR_MATERIALSPEC_AMBIENT_INDEX
	@brief Define the ambient color index for function grMaterialSpec_SetColor
*/
#define GR_MATERIALSPEC_AMBIENT_INDEX		2
/*! @def GR_MATERIALSPEC_EMISSIVE_INDEX
	@brief Define the emissive color index for function grMaterialSpec_SetColor
*/
#define GR_MATERIALSPEC_EMISSIVE_INDEX		3

typedef struct grTexture grTexture;
typedef struct grShader grShader;
typedef struct grXForm3d grXForm3d;

typedef enum grMaterialSpec_LayerType
{
    GR_MATERIAL_LAYER_BASE=0,
    GR_MATERIAL_LAYER_ALPHA,
    // PBR layers (.jmat version 2, same values as LAYER_TYPE_* in DCommon.h)
    GR_MATERIAL_LAYER_NORMAL=3,		// tangent-space normal map, linear
    GR_MATERIAL_LAYER_ORM=4,		// R = occlusion, G = roughness, B = metalness, linear
    GR_MATERIAL_LAYER_EMISSIVE=5,	// sRGB
    GR_MATERIAL_LAYER_HEIGHT=6		// parallax height, linear (optional)
} grMaterialSpec_LayerType;

/*! @name PBR material parameters (roadmap Phase 2)
	A material without them, and without PBR layers, renders exactly as before.
	@{
*/
#define GR_MATERIAL_ALPHA_OPAQUE		0
#define GR_MATERIAL_ALPHA_CUTOUT		1	//!< clip below AlphaCutoff
#define GR_MATERIAL_ALPHA_BLEND			2	//!< blended like a transparent face

#define GR_MATERIAL_PBR_TWO_SIDED		0x0001
#define GR_MATERIAL_PBR_RETRO			0x0002	//!< point sampling and texel-snapped shading

/*! @typedef grMaterialSpec_PBR
	@brief Scalar PBR parameters; this is also their layout in a .jmat file (48 bytes).
*/
typedef struct grMaterialSpec_PBR
{
	float	BaseColor[4];		//!< linear tint of the base layer (default 1,1,1,1)
	float	Roughness;			//!< roughness, times the ORM map's G when there is one (default 1)
	float	Metal;				//!< metalness, times the ORM map's B when there is one (default 1)
	float	Emissive[3];		//!< linear emissive color, times the emissive map when there is one (default 0)
	float	EmissiveIntensity;	//!< multiplies Emissive (default 1)
	float	AlphaCutoff;		//!< for GR_MATERIAL_ALPHA_CUTOUT (default 0.5)
	uint8	AlphaMode;			//!< GR_MATERIAL_ALPHA_*
	uint8	Reserved;
	uint16	Flags;				//!< GR_MATERIAL_PBR_*
} grMaterialSpec_PBR;
/*!@}*/
typedef grMaterialSpec_LayerType grMaterialSpec_LayerType;



typedef struct grMaterialSpec_Thumbnail
{
	uint8  width;
	uint8  height;
	uint8* contents;
} grMaterialSpec_Thumbnail;

/*! @typedef grMaterialSpec
*   @brief A JMAT file content description
*   @see grMaterial
*	@see grTexture
*/
typedef struct grMaterialSpec grMaterialSpec;

/*! @fn grMaterialSpec* grMaterialSpec_Create()
*   @brief Create an empty grMaterialSpec instance
	@param pEngine The engine associate with this material
	@param pResourceMgr The resource manager for accessing dependancies
	@return The grMaterialSpec instance created if succeed, NULL otherwise
*   @see grMaterial
*	@see grTexture
*/
GRAPI grMaterialSpec* GRCC grMaterialSpec_Create(grEngine* pEngine, grResourceMgr* pResourceMgr);

GRAPI grBoolean GRCC grMaterialSpec_CreateRef(grMaterialSpec* MaterialSpec);

/*! @fn grMaterialSpec* grMaterialSpec_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
*   @brief Create a grMaterialSpec instance from the JMAT file
	@param VFile The file already opened for read operations
	@param pEngine The engine associate with this material
	@param ResMgr The resource manager instance
	@return The grMaterialSpec instance created if succeed, NULL otherwise
*   @see grMaterial
*	@see grTexture
*/
GRAPI grMaterialSpec* GRCC grMaterialSpec_CreateFromFile(grVFile *VFile, grEngine* pEngine, grResourceMgr *ResMgr);

/*! @fn void grMaterialSpec_Destroy(grMaterialSpec **ppMaterialSpec);
*   @brief Destroy the grMaterialSpec instance
	@param ppMaterialSpec The grMaterialSpec instance pointer address
	@note The *ppMaterialSpec is set to NULL before returning
*/
GRAPI void GRCC grMaterialSpec_Destroy(grMaterialSpec **ppMaterialSpec);

/*! @fn grBoolean grMaterialSpec_WriteToFile(grMaterialSpec* MatSpec, grVFile *VFile)
*   @brief Write the grMaterialSpec instance into a JMAT file
	@param MatSpec The grMaterialSpec instance to write
	@param VFile The JMAT file already opened for write operations
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean GRCC grMaterialSpec_WriteToFile(grMaterialSpec* MatSpec, grVFile *VFile);

/*! @fn grBoolean grMaterialSpec_AddLayer(grMaterialSpec* MatSpec, int32 layerIndex, int32 Kind, grMaterialSpec_LayerType layerType, int32 layerMapper, const char* LayerName)
*   @brief Add a layer from a resource identifier to the current grMaterialSpec instance
	@param MatSpec The grMaterialSpec instance to modify
	@param layerIndex The layer index 0 based of the grMaterialSpec to modify
    @param Kind The resource type identifier
    @param layerType The layer behavior identifier
    @param layerMapper The UV mapper identifier
    @param LayerNam The layer resource identifier
	@return GR_TRUE if succeed, GR_FALSE otherwise

    @todo check if layerType is not a duplicate of layerIndex
    @see grResource.h for the Resource type list of possible values
*/
GRAPI grBoolean GRCC grMaterialSpec_AddLayer(grMaterialSpec* MatSpec, int32 layerIndex, int32 Kind, grMaterialSpec_LayerType layerType, int32 layerMapper, const char* LayerName);

/*! @fn grBoolean grMaterialSpec_AddLayerFromFile(grMaterialSpec* MatSpec, int32 layerIndex, grVFile *File, grBoolean UseColorKey, uint32 ColorKey)
*   @brief Add a layer from a file to the current grMaterialSpec instance
	@param MatSpec The grMaterialSpec instance to modify
	@param layerIndex The layer index 0 based of the grMaterialSpec to modify
    @param File The file that contains the layer definition (grTexture or grBitmap)
    @param UseColorKey Does the new material must use a colorkey, valid only if the File contains a grBitmap
    @param ColorKey The Color key palette index value
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean GRCC grMaterialSpec_AddLayerFromFile(grMaterialSpec* MatSpec, int32 layerIndex, grVFile *File, grBoolean UseColorKey, uint32 ColorKey);

/*! @fn grBoolean grMaterialSpec_AddLayerFromBitmap(grMaterialSpec* MatSpec, int32 layerIndex, grBitmap* Bitmap)
*   @brief Add a layer from a file to the current grMaterialSpec instance
	@param MatSpec The grMaterialSpec instance to modify
	@param layerIndex The layer index 0 based of the grMaterialSpec to modify
    @param pBitmap The layer content provided by a grBitmap instance already initialised
    @param ResName The resource name identifer for the grBitmap
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean GRCC grMaterialSpec_AddLayerFromBitmap(grMaterialSpec* MatSpec, int32 layerIndex, grBitmap* Bitmap, const char* ResName);

/*! @fn grBoolean grMaterialSpec_RemoveLayer(grMaterialSpec* MatSpec, int32 layerIndex)
    @brief Remove the layer identified by its index from the current grMaterialSpec
	@param MatSpec The grMaterialSpec instance to modify
	@param layerIndex The layer index 0 based of the grMaterialSpec to remove
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean GRCC grMaterialSpec_RemoveLayer(grMaterialSpec* MatSpec, int32 layerIndex);

/*! @fn grBoolean grMaterialSpec_SetLayerTransform(grMaterialSpec* MatSpec, int32 layerIndex, grXForm3d* LayerXFrom)
    @brief Change the transform matrix of the layer identified by its index from the current grMaterialSpec
	@param MatSpec The grMaterialSpec instance to modify
	@param layerIndex The layer index 0 based of the grMaterialSpec to modify
    @param LayerXFrom The transform matrix to set
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean GRCC grMaterialSpec_SetLayerTransform(grMaterialSpec* MatSpec, int32 layerIndex, grXForm3d* LayerXFrom);

GRAPI grBoolean GRCC grMaterialSpec_SetShader(grMaterialSpec* MatSpec, grShader* Shader);

GRAPI grBoolean GRCC grMaterialSpec_SetColor(grMaterialSpec* MatSpec, int32 ColorIndex, grRGBA* Color);

GRAPI grBoolean GRCC grMaterialSpec_SetThumbnail(grMaterialSpec* MatSpec, grMaterialSpec_Thumbnail* pThumb);

GRAPI uint32 GRCC grMaterialSpec_GetLayerCount(const grMaterialSpec* MatSpec);

GRAPI grTexture* GRCC grMaterialSpec_GetLayerTexture(const grMaterialSpec* MatSpec, int32 layerIndex);

GRAPI grBitmap* GRCC grMaterialSpec_GetLayerBitmap(const grMaterialSpec* MatSpec, int32 layerIndex);

GRAPI grXForm3d* GRCC grMaterialSpec_GetLayerTransform(const grMaterialSpec* MatSpec, int32 layerIndex);

GRAPI grShader* GRCC grMaterialSpec_GetShader(const grMaterialSpec* MatSpec);

GRAPI grRGBA* GRCC grMaterialSpec_GetColor(const grMaterialSpec* MatSpec, int32 colorIndex);

GRAPI uint32 GRCC grMaterialSpec_GetColors(const grMaterialSpec* MatSpec, grRGBA* Diffuse, grRGBA* Specular, grRGBA* Ambient, grRGBA* Emissive);

GRAPI grMaterialSpec_Thumbnail* GRCC grMaterialSpec_GetThumbnail(const grMaterialSpec* MatSpec);

GRAPI uint32 GRCC grMaterialSpec_Height(const grMaterialSpec* MatSpec);

GRAPI uint32 GRCC grMaterialSpec_Width(const grMaterialSpec* MatSpec);

/*! @fn int32 grMaterialSpec_FindLayer(const grMaterialSpec* MatSpec, grMaterialSpec_LayerType layerType)
	@brief Index of the first layer of that type, or -1
*/
GRAPI int32 GRCC grMaterialSpec_FindLayer(const grMaterialSpec* MatSpec, grMaterialSpec_LayerType layerType);

/*! @fn grBoolean grMaterialSpec_IsPBR(const grMaterialSpec* MatSpec)
	@brief True when the material has PBR parameters or a normal, ORM or emissive layer
*/
GRAPI grBoolean GRCC grMaterialSpec_IsPBR(const grMaterialSpec* MatSpec);

/*! @fn grBoolean grMaterialSpec_GetPBR(const grMaterialSpec* MatSpec, grMaterialSpec_PBR* PBR)
	@brief Fills PBR with the material's parameters and returns GR_TRUE, or returns GR_FALSE with
	the defaults adjusted to its maps (Metal 0 without an ORM map, Emissive 1 with an emissive map)
*/
GRAPI grBoolean GRCC grMaterialSpec_GetPBR(const grMaterialSpec* MatSpec, grMaterialSpec_PBR* PBR);

/*! @fn grBoolean grMaterialSpec_SetPBR(grMaterialSpec* MatSpec, const grMaterialSpec_PBR* PBR)
	@brief Sets the PBR parameters; NULL removes them (the file is then written as version 1)
*/
GRAPI grBoolean GRCC grMaterialSpec_SetPBR(grMaterialSpec* MatSpec, const grMaterialSpec_PBR* PBR);

/*! @fn void grMaterialSpec_DefaultPBR(grMaterialSpec_PBR* PBR)
	@brief The parameters a material without its own gets
*/
GRAPI void GRCC grMaterialSpec_DefaultPBR(grMaterialSpec_PBR* PBR);

/*! @fn int32 grMaterialSpec_GetLayerType(const grMaterialSpec* MatSpec, int32 layerIndex)
	@brief The layer's grMaterialSpec_LayerType, or -1
*/
GRAPI int32 GRCC grMaterialSpec_GetLayerType(const grMaterialSpec* MatSpec, int32 layerIndex);

/*!@}*/

/*! @name grMaterial related functions and data
	@{
*/
/*! @typedef grMaterial
*   @brief A reference to a Material used by the Engine
*/
typedef struct grMaterial grMaterial;
/* Must stay grArray_Index (uint32): face infos store it on disk and
   GR_MATERIAL_ARRAY_NULL_INDEX does not fit in 16 bits. */
typedef grArray_Index grMaterial_ArrayIndex;

/*! @fn void grMaterial_Destroy(grMaterial **ppMaterial)
*   @brief Destroy the current grMaterial
*   @param[in] Material The grMaterial struct to destroy
	@note The *ppMaterial is set to NULL before returning
*/
GRAPI void					GRCC grMaterial_Destroy(grMaterial **ppMaterial);

/*! @fn grBoolean grMaterial_CreateRef(grMaterial *Material)
*   @brief Reference the current Material
*   @param[in] Material The Material to increment its reference counter
*   @return GR_TRUE if success, GR_FALSE otherwise
*/
GRAPI grBoolean			GRCC grMaterial_CreateRef(grMaterial *Material);

/*! @fn grMaterial* grMaterial_Create(const char *MatName)
*   @brief Create a new Material.
*   @param[in] MatName The name of the new Material.
*   @return The grMaterial created. NULL if failed.
*/
GRAPI grMaterial			* GRCC grMaterial_Create(const char *MatName);
GRAPI grBoolean			GRCC grMaterial_SetBitmap(grMaterial *Mat, grBitmap *Bitmap, const char *BitmapName);
GRAPI const grBitmap	* GRCC grMaterial_GetBitmap(const grMaterial *Mat);

/*! @fn const char* grMaterial_GetName( const grMaterial *Mat)
*   @brief Read the name of the current Material.
*   @param[in] Mat The grMaterial to read its name.
*   @return The name if succeed, NULL otherwise.
*/
GRAPI const char			* GRCC grMaterial_GetName( const grMaterial *Mat);

/*! @fn const char* grMaterial_GetBitmapName( const grMaterial *Mat)
*   @brief Read the name of the Bitmap linked with the current Material
*   @param[in] Mat The grMaterial to read its bitmap name
*   @return The bitmap name if succeed, NULL otherwise
*	@author Bruno Pettorelli (krouer@genesis3d.com)
*/
GRAPI const char			* GRCC grMaterial_GetBitmapName( const grMaterial *Mat);

/*! @fn const grMaterialSpec* grMaterial_GetMaterialSpec( const grMaterial *Mat)
*   @brief Grant access to the grMaterialSpec of the current Material
*   @param[in] Mat The grMaterial to access its grMaterialSpec
*   @return The grMaterialSpec member if succeed, NULL otherwise
*	@author Bruno Pettorelli (krouer@genesis3d.com)
*/
GRAPI const grMaterialSpec	* GRCC grMaterial_GetMaterialSpec(const grMaterial *Mat);
/*!@}*/

/*! @name grMaterial_Array related functions and data
	@{
*/
/*! @typedef grMaterial_Array
*   @brief An Array of grMaterial struct
*   @see grArray
*/
typedef struct grMaterial_Array grMaterial_Array;

/*! @typedef grMaterial_Array
*   @brief The index type
*   @see grArray_Index
*/

/*! @fn grMaterial_Array* grMaterial_ArrayCreate(int32 StartMaterials)
*   @brief Create a material array
*   @param[in] StartMaterials The count of grMaterial the array will hold
*   @return A grMaterial_Array instance if succeed otherwise NULL
*/
GRAPI grMaterial_Array		* GRCC grMaterial_ArrayCreate(int32 StartMaterials);

/*! @fn grMaterial_Array* grMaterial_ArrayCreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
*   @brief Create a material array from a file
*   @param[in] VFile The file where to read array data
*	@param[in] PtrMgr The pointer manager to use
*   @return A grMaterial_Array instance if succeed otherwise NULL
*/
GRAPI grMaterial_Array		* GRCC grMaterial_ArrayCreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);

/*! @fn grBoolean grMaterial_ArrayWriteToFile(grMaterial_Array *MatArray, grVFile *VFile, grPtrMgr *PtrMgr)
*   @brief Write a material array to a file
*	@param[in] MatArray The grMaterial_Array to write
*   @param[in] VFile The file where to read array data
*	@param[in] PtrMgr The pointer manager to use
*   @return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean			GRCC grMaterial_ArrayWriteToFile(grMaterial_Array *MatArray, grVFile *VFile, grPtrMgr *PtrMgr);

/*! @fn grBoolean grMaterial_ArrayCreateRef(grMaterial_Array *MatArray)
*   @brief Increment the grMaterial_Array reference counter
*	@param[in] MatArray The grMaterial_Array to modify
*   @return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean			GRCC grMaterial_ArrayCreateRef(grMaterial_Array *MatArray);

/*! @fn void grMaterial_ArrayDestroy(grMaterial_Array **ppArray)
*   @brief Decrement the reference counter, destroy the instance if reach 0 and reset the address to NULL in all cases
*	@param[in,out] ppArray The address of grMaterial_Array to destroy when its reference counter reach 0
*   @return GR_TRUE if succeed, GR_FALSE otherwise
	@note The *ppArray is set to NULL before returning
*/
GRAPI void					GRCC grMaterial_ArrayDestroy(grMaterial_Array **ppArray);
GRAPI grMaterial_ArrayIndex GRCC grMaterial_ArrayCreateMaterial(grMaterial_Array *MatArray, const char *MatName);
GRAPI void					GRCC grMaterial_ArrayDestroyMaterial(grMaterial_Array *MatArray, grMaterial_ArrayIndex *Index);
GRAPI const grMaterial * GRCC grMaterial_ArrayGetMaterialByIndex(const grMaterial_Array *Array, grMaterial_ArrayIndex Index);
GRAPI grMaterial_ArrayIndex GRCC grMaterial_ArrayGetMaterialIndex(const grMaterial_Array *Array, const grMaterial *Material);
GRAPI grBoolean			GRCC grMaterial_ArraySetMaterialBitmap(grMaterial_Array *Array, grMaterial_ArrayIndex Index, grBitmap *Bitmap, const char *BitmapName);
GRAPI grMaterial			* GRCC grMaterial_ArrayGetNextMaterial(grMaterial_Array *Array, const grMaterial *Start);
GRAPI grBoolean			GRCC grMaterial_ArraySetMaterialSpec(grMaterial_Array *Array, grMaterial_ArrayIndex Index, grMaterialSpec *MatSpec, const char *MatName);
/*!@}*/


#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
