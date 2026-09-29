/*!
	@file grTexture.h 
	
	@author Anthony Rufrano (paradoxnj)
	@brief Hardware textures

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
#ifndef GR_TEXTURE_H
#define GR_TEXTURE_H

#include "BaseType.h"
#include "pixelformat.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grEngine grEngine;

typedef struct grTexture grTexture;

#ifndef RDRIVER_PIXELFORMAT_DEFINED
#define RDRIVER_PIXELFORMAT_DEFINED
typedef struct grRDriver_PixelFormat
{
	grPixelFormat	PixelFormat;
	uint32			Flags;				
} grRDriver_PixelFormat;

#define RDRIVER_THANDLE_HAS_COLORKEY	(1<<0)

typedef enum
{
	Rop_None,
	Rop_Multiply,
	Rop_MultiplyX2,
	Rop_MultiplyX4,
	Rop_Add,
} grRDriver_Rop;

typedef struct grTexture_Info
{
	int32					Width;
	int32					Height;
	int32					Stride;
	uint32					ColorKey;
	uint32					Flags;
	uint8					Log;
	grRDriver_PixelFormat	PixelFormat;
    void*                   Direct;
} grTexture_Info;
#endif

typedef grRDriver_PixelFormat grRDriver_PixelFormat;
typedef grRDriver_Rop grRDriver_Rop;
typedef grTexture_Info grTexture_Info;

/*!
	@fn grTexture *grTexture_Create(grEngine *Engine, int32 Width, int32 Height, int32 MipLevels, grRDriver_PixelFormat *Format)
	@brief Creates a hardware texture
	@param[in] Engine The engine to create from
	@param[in] Width The width of the texture
	@param[in] Height The height of the texture
	@param[in] MipLevels The number of mip levels to generate
	@param[in] Format The pixel format to use
	@return The new texture
*/
GRAPI grTexture * GRCC grTexture_Create(grEngine *Engine, int32 Width, int32 Height, int32 MipLevels, grRDriver_PixelFormat *Format);

/*!
	@fn grTexture *grTexture_CreateFromFile(grEngine *Engine, grVFile *File)
	@brief Creates a hardware texture from file
	@param[in] Engine The engine to create from
	@param[in] File The file that contains the texture data
	@return The new texture
	@note This function can support the following file formats:  BMP, TGA, DDS, JPG, and PNG)
*/
GRAPI grTexture * GRCC grTexture_CreateFromFile(grEngine *Engine, grVFile *File);

/*!
	@fn grBoolean grTexture_Destroy(grEngine *Engine, grTexture *Texture)
	@brief Destroys a hardware texture
	@param[in] Engine The engine that holds the driver
	@param[in] Texture The texture to destroy
	@return GR_TRUE on success, GR_FALSE on failure
	@note Failure will result in a memory leak!!!
*/
GRAPI grBoolean GRCC grTexture_Destroy(grEngine *Engine, grTexture *Texture);

/*!
	@fn grBoolean grTexture_Lock(grEngine *Engine, grTexture *Texture, int32 MipLevel, void **data)
	@brief Locks a texture for writing
	@param[in] Engine The engine that holds the driver
	@param[in] Texture The texture to lock
	@param[in] MipLevel The mip level to lock
	@param[out] data The bitmap data
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grTexture_Lock(grEngine *Engine, grTexture *Texture, int32 MipLevel, void **data);

/*!
	@fn grBoolean grTexture_Unlock(grEngine *Engine, grTexture *Texture, int32 MipLevel)
	@brief Unlocks a texture
	@param[in] Engine The engine that holds the driver
	@param[in] Texture The texture to unlock
	@param[in] MipLevel The mip level to unlock
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grTexture_Unlock(grEngine *Engine, grTexture *Texture, int32 MipLevel);

/*!
	@fn grBoolean grTexture_GetInfo(grEngine *Engine, grTexture *Texture, int32 MipLevel, grTexture_Info *TextureInfo)
	@brief Gets a texture's information
	@param[in] Engine The engine that holds the driver
	@param[in] Texture The texture to query
	@param[in] MipLevel The mip level to query
	@param[out] TextureInfo The texture's information
	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grTexture_GetInfo(grEngine *Engine, grTexture *Texture, int32 MipLevel, grTexture_Info *TextureInfo);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif // GR_TEXTURE_H
