/*!
	@file grSoundSystem.h 
	
	@author Anthony Rufrano
	@brief New and improved sound system

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

/*!  @note  SoundSystem
*	This object represents the engine's sound core. This is the basic entity of the rendering engine.
*/
#ifndef GR_SOUNDSYSTEM_H
#define GR_SOUNDSYSTEM_H

#include "BaseType.h"
#include "VFile.h"




/*!
*	@typedef grSoundSystem
*	@brief The sound system structure
*/
typedef struct jeSoundSystem grSoundSystem;
typedef struct jeSoundSystem jeSoundSystem;

/*!
*	@typedef grSound
*	@brief A reference to a sound buffer
*/
typedef struct jeSound							grSound;

/*!
*	@typedef grSound3d
*	@brief A 3D sound
*/
typedef struct jeSound3d						grSound3d;

/*!
*	@typedef grSoundListener
*	@brief Represents the position of the player in the world relative to all 3D sounds.
*/
typedef struct jeSoundListener					grSoundListener;

/*!
*	@fn grSoundSystem *grSoundSystem_Create(HWND hWnd, uint32 Flags)
*	@brief Creates the sound system
*	@param[in] hWnd The window handle to attach the sound system to.
*   @param[in] Flags The sound system creation flags.
*	@return The created sound system.  NULL if unsuccessful.
*/
#if defined(_INC_WINDOWS) || defined(_WINDEF_)
GRAPI grSoundSystem *GRCC grSoundSystem_Create(HWND hWnd, uint32 Flags);
#endif


/*!
*	@fn uint32 grSoundSystem_CreateRef(grSoundSystem *SoundSys)
*	@brief Increments the reference counter for the sound system.
*	@param[in] SoundSys The sound system to reference.
*	@return The number of current references
*/
GRAPI uint32 GRCC grSoundSystem_CreateRef(grSoundSystem *SoundSys);

/*!
*	@fn uint32 grSoundSystem_Destroy(grSoundSystem **SoundSys)
*	@brief Dereferences the sound system.  Destroys it if there are no more references.
*	@param[in] SoundSys The sound system to dereference.
*	@return The number of current references
*/
GRAPI uint32 GRCC grSoundSystem_Destroy(grSoundSystem **SoundSys);

/*!
*	@fn grSound *grSoundSystem_LoadSound(grSoundSystem *SoundSys, grVFile *File, uint32 Flags)
*	@brief Loads a sound from a file.
*	@param[in] SoundSys The active sound system.
*	@param[in] File The file to load it from.
*   @param[in] Flags The flags of the sound.
*	@return The loaded sound.  NULL on failure.
*/
GRAPI grSound *GRCC grSoundSystem_LoadSound(grSoundSystem *SoundSys, grVFile *File, uint32 Flags);

/*!
*	@fn grBoolean grSoundSystem_DestroySound(grSoundSystem *SoundSys, grSound **Snd)
*	@brief Destroys a loaded sound.
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to destroy.
*	@return GR_TRUE on success, GR_FALSE on failure
*	@note Failure will result in a memory leak!!!
*/
GRAPI grBoolean GRCC grSoundSystem_DestroySound(grSoundSystem *SoundSys, grSound **Snd);

/*!
*	@fn grSound *grSoundSystem_PlaySound(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Plays a sound
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to play
*	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grSoundSystem_PlaySound(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn grBoolean grSoundSystem_StopSound(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Stops a sound
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to stop
*	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grSoundSystem_StopSound(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn grBoolean grSoundSystem_PauseSound(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Pauses a sound
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to pause
*	@return GR_TRUE on success, GR_FALSE on failure
*/
GRAPI grBoolean GRCC grSoundSystem_PauseSound(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn grBoolean grSoundSystem_IsPlaying(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Checks if a sound is playing
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to query
*	@return GR_TRUE if the sound is playing, GR_FALSE if not
*/
GRAPI grBoolean GRCC grSoundSystem_IsPlaying(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn grBoolean grSoundSystem_IsPaused(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Checks if a sound is paused
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to query
*	@return GR_TRUE if the sound is paused, GR_FALSE if not
*/
GRAPI grBoolean GRCC grSoundSystem_IsPaused(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn grBoolean grSoundSystem_IsLooping(grSoundSystem *SoundSys, grSound *Snd)
*	@brief Checks if a sound is looping
*	@param[in] SoundSys The active sound system
*	@param[in] Snd The sound to query
*	@return GR_TRUE if the sound is looping, GR_FALSE if not
*/
GRAPI grBoolean GRCC grSoundSystem_IsLooping(grSoundSystem *SoundSys, grSound *Snd);

/*!
*	@fn float grSound_GetVolume(grSound *Snd)
*	@brief Gets the volume of a sound
*	@param[in] Snd The sound to query
*	@return The volume of the sound
*	@note The value will be between 0.0f and 1.0f
*/
GRAPI float GRCC grSound_GetVolume(grSound *Snd);

/*!
*	@fn grBoolean grSound_SetVolume(grSound *Snd, float vol)
*	@brief Sets the volume of a sound
*	@param[in] Snd The sound to modify
*	@param[in] vol The new volume
*	@return GR_TRUE on success, GR_FALSE on failure
*	@note The value should be between 0.0f and 1.0f.  It can be modified while the sound is playing.
*/
GRAPI grBoolean GRCC grSound_SetVolume(grSound *Snd, float vol);

/*!
*	@fn float grSound_GetPan(grSound *Snd)
*	@brief Gets the panning property of the sound
*	@param[in] Snd The sound to query
*	@return The panning value
*	@note This value will be between -1.0f and 1.0f
*/
GRAPI float GRCC grSound_GetPan(grSound *Snd);

/*!
*	@fn grBoolean grSound_SetPan(grSound *Snd, float pan)
*	@brief Sets the panning property of the sound
*	@param[in] Snd The sound to modify
*	@param[in] pan The new panning value
*	@return GR_TRUE on success, GR_FALSE on failure
*	@note The value should be between -1.0f and 1.0f.  It can be modified while the sound is playing.
*/
GRAPI grBoolean GRCC grSound_SetPan(grSound *Snd, float pan);


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define jeSoundSystem_Create                     grSoundSystem_Create
#define jeSoundSystem_CreateRef                  grSoundSystem_CreateRef
#define jeSoundSystem_Destroy                    grSoundSystem_Destroy
#define jeSoundSystem_DestroySound               grSoundSystem_DestroySound
#define jeSoundSystem_IsLooping                  grSoundSystem_IsLooping
#define jeSoundSystem_IsPaused                   grSoundSystem_IsPaused
#define jeSoundSystem_IsPlaying                  grSoundSystem_IsPlaying
#define jeSoundSystem_LoadSound                  grSoundSystem_LoadSound
#define jeSoundSystem_PauseSound                 grSoundSystem_PauseSound
#define jeSoundSystem_PlaySound                  grSoundSystem_PlaySound
#define jeSoundSystem_StopSound                  grSoundSystem_StopSound
#define jeSound_GetPan                           grSound_GetPan
#define jeSound_GetVolume                        grSound_GetVolume
#define jeSound_SetPan                           grSound_SetPan
#define jeSound_SetVolume                        grSound_SetVolume

#endif // GENESIS_NO_JET_COMPAT

#endif
