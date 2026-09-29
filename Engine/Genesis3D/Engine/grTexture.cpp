/*!
	@file grTexture.cpp
	
	@author Anthony Rufrano (paradoxnj)
	@brief Hardware textures

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
#include "Engine.h"
#include "grTexture.h"
#include "Engine._h"

GRAPI grTexture * GRCC grTexture_Create(grEngine *Engine, int32 Width, int32 Height, int32 MipLevels, grRDriver_PixelFormat *Format)
{
	return Engine->DriverInfo.RDriver->THandle_Create(Width, Height, MipLevels, Format);
}

GRAPI grTexture * GRCC grTexture_CreateFromFile(grEngine *Engine, grVFile *File)
{
	return Engine->DriverInfo.RDriver->THandle_CreateFromFile(File);
}

GRAPI grBoolean GRCC grTexture_Destroy(grEngine *Engine, grTexture *Texture)
{
	return Engine->DriverInfo.RDriver->THandle_Destroy(Texture);
}

GRAPI grBoolean GRCC grTexture_Lock(grEngine *Engine, grTexture *Texture, int32 MipLevel, void **data)
{
	return Engine->DriverInfo.RDriver->THandle_Lock(Texture, MipLevel, data);
}

GRAPI grBoolean GRCC grTexture_Unlock(grEngine *Engine, grTexture *Texture, int32 MipLevel)
{
	return Engine->DriverInfo.RDriver->THandle_UnLock(Texture, MipLevel);
}

GRAPI grBoolean GRCC grTexture_GetInfo(grEngine *Engine, grTexture *Texture, int32 MipLevel, grTexture_Info *TextureInfo)
{
	return Engine->DriverInfo.RDriver->THandle_GetInfo(Texture, MipLevel, TextureInfo);
}
