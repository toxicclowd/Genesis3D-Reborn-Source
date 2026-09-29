/****************************************************************************************/
/*  SOUND.H                                                                             */
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
#ifndef	JE_SOUND_H
#define	JE_SOUND_H

#include "Sound.h"
#include "VFile.h"
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif


// JET_PUBLIC_APIS

typedef struct jeSound_System	jeSound_System;
typedef struct jeSound_Def		jeSound_Def;
typedef struct jeSound			jeSound;

typedef struct jeSound_System	grSound_System;
typedef struct jeSound_Def		grSound_Def;
typedef struct jeSound			grSound;


#ifdef _INC_WINDOWS
	// Windows.h must be previously included for this api to be exposed.
JETAPI	jeSound_System * JETCC jeSound_CreateSoundSystem(HWND hWnd);
JETAPI	jeBoolean JETCC jeSound_SetHwnd(HWND hWnd);

#endif

JETAPI	void			JETCC jeSound_DestroySoundSystem(jeSound_System *Sound);


JETAPI	jeSound_Def	   * JETCC jeSound_LoadSoundDef(jeSound_System *SoundS, jeVFile *File);
JETAPI	jeBoolean		JETCC jeSound_FreeSoundDef(jeSound_System *SoundS, jeSound_Def *SoundDef);

JETAPI	jeSound		   * JETCC jeSound_PlaySoundDef(jeSound_System *SoundS, 
									jeSound_Def *SoundDef, 
									jeFloat Volume, 
									jeFloat Pan, 
									jeFloat Frequency, 
									jeBoolean Loop);
JETAPI	jeBoolean		JETCC jeSound_StopSound(jeSound_System *SoundS, jeSound *Sound);
JETAPI	jeBoolean		JETCC jeSound_ModifySound(jeSound_System *SoundS, 
									jeSound *Sound, 
									jeFloat Volume, 
									jeFloat Pan, 
									jeFloat Frequency);
JETAPI	jeBoolean		JETCC jeSound_SoundIsPlaying(jeSound_System *SoundS, jeSound *Sound);
JETAPI	jeBoolean		JETCC jeSound_SetMasterVolume( jeSound_System *SoundS, jeFloat Volume );

//	added by tom morris May 2005
JETAPI	int				JETCC jeSound_GetStatus(jeSound_System *pSoundSys, jeSound *pSound);
//	end add

//CyRiuS Begin

JETAPI int JETCC jeMp3_LoadSound(jeSound_System *SoundS, char * filename, int ref); //load an MP3 into mp3mgr
JETAPI	int JETCC jeMp3_PlaySound(jeSound_System *SoundS, int song_number, long Volume, jeBoolean Loop);

//CyRiuS End


// JET_PRIVATE_APIS

#ifdef	__cplusplus
}
#endif

// Genesis3D: Reborn Aliases
#define grSound_CreateSoundSystem     jeSound_CreateSoundSystem
#define grSound_SetHwnd               jeSound_SetHwnd
#define grSound_DestroySoundSystem    jeSound_DestroySoundSystem
#define grSound_LoadSoundDef          jeSound_LoadSoundDef
#define grSound_FreeSoundDef          jeSound_FreeSoundDef
#define grSound_PlaySoundDef          jeSound_PlaySoundDef
#define grSound_StopSound             jeSound_StopSound
#define grSound_ModifySound           jeSound_ModifySound
#define grSound_SoundIsPlaying        jeSound_SoundIsPlaying
#define grSound_SetMasterVolume       jeSound_SetMasterVolume
#define grSound_GetStatus             jeSound_GetStatus
#define grMp3_LoadSound               jeMp3_LoadSound
#define grMp3_PlaySound               jeMp3_PlaySound

#endif


