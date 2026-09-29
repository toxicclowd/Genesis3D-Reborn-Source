/*!
	@file grResource.h
	
	@author
	@brief The Resource Manager definition

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

	@par Documentation
	@ref resourceMgr "The Resource Manager page"
*/

/*! @page resourceMgr The Resource Manager

	This object is designed to help resource to be loaded from a PAK/VFS file or
	a folder in direct access.

	@par Resource
	A resource can be a folder or a file. In the file, we have subcategorized actors,
	bitmaps, shaders, sounds and musics.

    @par Resource Manager
	A #grResourceMgr maintains a list of all resources that was requested by the user
	application. The resource is strongly linked to the application.

	@par The Default Directory Identifiers are:
	@anchor defaultDirs 
	<ul>
	<li><b>Sounds</b> to store all sounds files</li>
	<li><b>GlobalMaterials</b> to store all bitmap files</li>
	<li><b>Actors</b> to store all actors files</li>
	<li><b>Shaders</b> to store all shaders files</li>
	</ul>
	The identifiers have been choose identical to directories because it simplifies the link
	between the identifier and the directory. These identifers by default are linked with their
	respective directory. When PAK file will be introduce, developpers will have the ability to
	release their content in a single file.
	@par Setting up the GlobalMaterials from a PAK file
	@code
    //create resource manager 
    grResourceMgr *ResourceMgr = grResource_MgrCreate(); 
    
    //open virtual file systems 
    VFile* FS = grVFile_OpenNewSystem(NULL,GR_VFILE_TYPE_DOS,"Bitmaps.JetPak", 
                                      NULL,GR_VFILE_OPEN_READONLY); 

    VFile* VFS = grVFile_OpenNewSystem(FS,GR_VFILE_TYPE_VIRTUAL,NULL,NULL, 
                                       GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY); 

    //add file system to resource manager 
    grResource_AddVFile( ResourceMgr, "GlobalMaterials", VFS); 
	@endcode
	@par Setting up the GlobalMaterials from a directory
	@code
    //create resource manager 
    grResourceMgr *ResourceMgr = grResource_MgrCreate(); 

    //Add the GlobaMaterials directory to resource
    grResource_OpenDirectory(ResourceMgr, "./GlobalMaterials", GlobalMaterials);
	@endcode

	@par The 4 default Directories are:
	<ul>
	<li><b>Sounds</b> to store all sounds files</li>
	<li><b>GlobalMaterials</b> to store all bitmap files</li>
	<li><b>Actors</b> to store all actors files</li>
	<li><b>Shaders</b> to store all shaders files</li>
	</ul>
*/

#ifndef	GR_RESOURCE_H
#define GR_RESOURCE_H

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif


/*! @name Resource kinds
	@brief Possible value for Type parameter of grResource_GetResource() */
/*@{*/
/*! @def GR_RESOURCE_ANY
	@brief The resource is anything
*/
#define GR_RESOURCE_ANY		0x0000
/*! @def GR_RESOURCE_VFS
	@brief The resource is a #grVFile pointer
*/
#define GR_RESOURCE_VFS		0x0001
/*! @def GR_RESOURCE_BITMAP
	@brief The resource is a #grBitmap pointer
*/
#define GR_RESOURCE_BITMAP	0x0002
/*! @def GR_RESOURCE_SHADER
	@brief The resource is a #grShader pointer
*/
#define GR_RESOURCE_SHADER	0x0004
/*! @def GR_RESOURCE_SOUND
	@brief The resource is a #grSound pointer
*/
#define GR_RESOURCE_SOUND	0x0005
/*! @def GR_RESOURCE_ACTOR
	@brief The resource is a #grActor pointer
*/
#define GR_RESOURCE_ACTOR	0x0006
/*! @def GR_RESOURCE_MATERIAL
	@brief The resource is a #grMaterialSpec Material pointer build from JMAT file
*/
#define GR_RESOURCE_MATERIAL	0x0010
/*! @def GR_RESOURCE_TEXTURE
	@brief The resource is a #grTexture Texture pointer build from image file by driver
*/
#define GR_RESOURCE_TEXTURE		0x0011
/*@}*/

/*! @typedef grResourceMgr
	@brief The grResourceMgr struct
*/
typedef struct jeResourceMgr grResourceMgr;
typedef struct jeResourceMgr jeResourceMgr;

typedef struct jeEngine grEngine;

////////////////////////////////////////////////////////////////////////////////////////
//	Resource manager functions
////////////////////////////////////////////////////////////////////////////////////////

/*! @fn grResourceMgr* grResource_MgrCreate(grEngine* pEngine)
	@brief Create a resource manager.
	@param pEngine The current grEngine instance
	@return The grResourceMgr instance if succeed

	The @p pEngine parameter is need to built texture resource.
*/
GRAPI grResourceMgr* GRCC grResource_MgrCreate(grEngine* pEngine);

/*!	@fn int grResource_MgrIncRefcount(grResourceMgr* ResourceMgr)
	@brief Increment a resource managers reference/usage count.
	@param ResourceMgr Manager whose ref count will be incremented
	@return The new reference counter value
*/
GRAPI int32 GRCC grResource_MgrIncRefcount(grResourceMgr* ResourceMgr );

/*! @fn void GRCC grResource_MgrDestroy(grResourceMgr** DeadResourceMgr)
	@brief Destroy a resource manager.
	@param DeadResourceMgr Manager to zap

	The Resource Manager is zapped oly if its reference counter is zero.<br>
	The function first decrement the ref count.
*/
GRAPI void GRCC grResource_MgrDestroy(grResourceMgr** DeadResourceMgr);
   
/*! @fn grBoolean grResourceMgr_SetEngine(grEngine* pEngine)
	@brief Assign the current grEngine instance to the grResourceMgr
	@param pEngine The current grEngine instance
	@return GR_TRUE when succeed
*/
GRAPI grBoolean GRCC grResourceMgr_SetEngine(grResourceMgr *ResourceMgr, grEngine* pEngine);

GRAPI grEngine* GRCC grResourceMgr_GetEngine(const grResourceMgr *ResourceMgr);

GRAPI grResourceMgr* GRCC grResourceMgr_GetSingleton();

////////////////////////////////////////////////////////////////////////////////////////
//	Generic resource functions
////////////////////////////////////////////////////////////////////////////////////////

//	Add a new resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grResource_Add(
	grResourceMgr	*ResourceMgr,	// resource manager to add it to
	char			*Name,			// name
    uint32          Type,           // type
	void			*Data );		// data

/*! @brief Get an existing resource.
	@param ResourceMgr The resource manager to delete it from
	@param Name The resource name
    @deprecated Replace by grResource_GetResource
*/
GRAPI void * GRCC grResource_Get(
	grResourceMgr	*ResourceMgr,	// resource manager to get it from
	char			*Name );		// resource name

/*! @brief Delete an existing resource.
	
	Decrement the @p Name identified resource reference counter and remove it when reach zero.

	@param ResourceMgr The resource manager to delete it from
	@param Name The resource name
	@return 0 if the resource is fully deleted else return the reference counter value

    @deprecated Replace by grResource_ReleaseResource
*/
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grResource_Delete(grResourceMgr *ResourceMgr, char *Name );




////////////////////////////////////////////////////////////////////////////////////////
//	VFile specific resource functions
////////////////////////////////////////////////////////////////////////////////////////

//	Add a new VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grBoolean GRCC grResource_AddVFile(
	grResourceMgr	*ResourceMgr,	// resource list to add it to
	char			*Name,			// name
	grVFile			*Data );		// data

//	Get an existing VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI grVFile * GRCC grResource_GetVFile(
	grResourceMgr	*ResourceMgr,	// resource list to get it from
	char			*Name );		// name

//	Delete an existing VFile resource.
//
////////////////////////////////////////////////////////////////////////////////////////
GRAPI int GRCC grResource_DeleteVFile(
	grResourceMgr	*ResourceMgr,	// resource list to delete it from
	char			*Name );		// name

/*! @fn grBoolean grResource_OpenDirectory(grResourceMgr* pResourceMgr, char* DirName, char* ResName)
	@brief Open a vfile(directory) for Resource Manager (WITH AutoRemove on ResMgrDestroy)
	@param pResourceMgr The Resource Manager instance
	@param DirName Path of directory
	@param ResName Alias of this resource
	@return GR_TRUE when succeed
	@author Icestorm
*/
GRAPI grBoolean GRCC grResource_OpenDirectory(grResourceMgr* pResourceMgr, char* DirName, char* ResName);

/*! @fn grResourceMgr* grResource_MgrCreateDefault(grEngine* pEngine)
	@brief Create a Resource Manager using the default paths.
	@param pEngine The current grEngine
	@return The grResourceMgr instance if succeed
	@author Incarnadine modified by Icestorm [Added autoremove], modified by Krouer for pEngine parameter
	@see grResource_OpenDirectory

    It makes usage of grResource_OpenDirectory to create its @ref defaultDirs "4 defaults directories".
*/
GRAPI grResourceMgr* GRCC grResource_MgrCreateDefault(grEngine* pEngine);

/*! @fn void* grResource_GetResource(grResourceMgr *ResourceMgr, int32 Type, char *Name);
	@brief Create a resource from memory or disk
	
	Read resource from disk and create them from the founded file

	@note The name can contains in first the directory of the resource.
	@see @ref defaultDirs "Default directories"
    @param[in] ResourceMgr The resource list to get it from
	@param[in] Type The awaited resource pointer (material, shader, actor, bitmap, ...) type
	@param[in] Name The resource name
	@return The resource pointer to be casted following the flag of @a Type or NULL if failed
*/
GRAPI void* GRCC grResource_GetResource(grResourceMgr *ResourceMgr, int32 Type, char *Name);

/*! @fn void grResource_ExportResource(grResourceMgr *ResourceMgr, int32 Type, char *Name);
	@brief Save a resource to its format on the disk

	@remark Use when older level are load in the engine.

    @param[in] ResourceMgr The resource list to get it from
	@param[in] Type The awaited resource pointer (shader, actor, bitmap, ...) type
	@param[in] Name The resource name

	@note Only bitmaps export is currently implemented and bmp generated file aren't BMP format files.
*/
GRAPI void GRCC grResource_ExportResource(grResourceMgr *ResourceMgr, int32 Type, char *Name, void* Data);

/*! @fn void* grResource_Release(grResourceMgr *ResourceMgr, int32 Type, char *Name);
	@brief Release the identified resource
	
	Decrement the resource usage and if reach zero, remove the resource from the managed pool.
	Try to call the Destroy of the resource depending of the Type param.<br>Superseed the grResource_Delete
	function.

	@note The name can contains in first the directory of the resource.
	@see @ref defaultDirs "Default directories"
    @param[in] ResourceMgr The resource list to get it from
	@param[in] Type The resource (material, shader, actor, bitmap, ...) type
	@param[in] Name The resource name identifier
	@return GR_TRUE when succeed
*/
GRAPI grBoolean GRCC grResource_ReleaseResource(grResourceMgr *ResourceMgr, int32 Type, char *Name);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define JE_RESOURCE_ACTOR                        GR_RESOURCE_ACTOR
#define JE_RESOURCE_ANY                          GR_RESOURCE_ANY
#define JE_RESOURCE_BITMAP                       GR_RESOURCE_BITMAP
#define JE_RESOURCE_MATERIAL                     GR_RESOURCE_MATERIAL
#define JE_RESOURCE_SHADER                       GR_RESOURCE_SHADER
#define JE_RESOURCE_SOUND                        GR_RESOURCE_SOUND
#define JE_RESOURCE_TEXTURE                      GR_RESOURCE_TEXTURE
#define JE_RESOURCE_VFS                          GR_RESOURCE_VFS
#define jeResourceMgr_GetEngine                  grResourceMgr_GetEngine
#define jeResourceMgr_GetSingleton               grResourceMgr_GetSingleton
#define jeResourceMgr_SetEngine                  grResourceMgr_SetEngine
#define jeResource_Add                           grResource_Add
#define jeResource_AddVFile                      grResource_AddVFile
#define jeResource_Delete                        grResource_Delete
#define jeResource_DeleteVFile                   grResource_DeleteVFile
#define jeResource_ExportResource                grResource_ExportResource
#define jeResource_Get                           grResource_Get
#define jeResource_GetResource                   grResource_GetResource
#define jeResource_GetVFile                      grResource_GetVFile
#define jeResource_MgrCreate                     grResource_MgrCreate
#define jeResource_MgrCreateDefault              grResource_MgrCreateDefault
#define jeResource_MgrDestroy                    grResource_MgrDestroy
#define jeResource_MgrIncRefcount                grResource_MgrIncRefcount
#define jeResource_OpenDirectory                 grResource_OpenDirectory
#define jeResource_Release                       grResource_Release
#define jeResource_ReleaseResource               grResource_ReleaseResource
#define jeVFile_OpenNewSystem                    grVFile_OpenNewSystem

#endif // GENESIS_NO_JET_COMPAT

#endif
