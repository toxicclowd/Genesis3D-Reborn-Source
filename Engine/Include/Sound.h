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
#ifndef	GR_SOUND_H
#define	GR_SOUND_H

#include "Sound.h"
#include "VFile.h"
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif


// GR_PUBLIC_APIS

typedef struct grSound_System	grSound_System;
typedef struct grSound_Def		grSound_Def;
typedef struct grSound			grSound;



#ifdef _INC_WINDOWS
	// Windows.h must be previously included for this api to be exposed.
GRAPI	grSound_System * GRCC grSound_CreateSoundSystem(HWND hWnd);
GRAPI	grBoolean GRCC grSound_SetHwnd(HWND hWnd);

#endif

GRAPI	void			GRCC grSound_DestroySoundSystem(grSound_System *Sound);


GRAPI	grSound_Def	   * GRCC grSound_LoadSoundDef(grSound_System *SoundS, grVFile *File);
GRAPI	grBoolean		GRCC grSound_FreeSoundDef(grSound_System *SoundS, grSound_Def *SoundDef);

GRAPI	grSound		   * GRCC grSound_PlaySoundDef(grSound_System *SoundS, 
									grSound_Def *SoundDef, 
									grFloat Volume, 
									grFloat Pan, 
									grFloat Frequency, 
									grBoolean Loop);
GRAPI	grBoolean		GRCC grSound_StopSound(grSound_System *SoundS, grSound *Sound);
GRAPI	grBoolean		GRCC grSound_ModifySound(grSound_System *SoundS, 
									grSound *Sound, 
									grFloat Volume, 
									grFloat Pan, 
									grFloat Frequency);
GRAPI	grBoolean		GRCC grSound_SoundIsPlaying(grSound_System *SoundS, grSound *Sound);
GRAPI	grBoolean		GRCC grSound_SetMasterVolume( grSound_System *SoundS, grFloat Volume );

//	added by tom morris May 2005
GRAPI	int				GRCC grSound_GetStatus(grSound_System *pSoundSys, grSound *pSound);
//	end add

//CyRiuS Begin

GRAPI int GRCC grMp3_LoadSound(grSound_System *SoundS, char * filename, int ref); //load an MP3 into mp3mgr
GRAPI	int GRCC grMp3_PlaySound(grSound_System *SoundS, int song_number, long Volume, grBoolean Loop);

//CyRiuS End


// GR_PRIVATE_APIS

#ifdef	__cplusplus
}
#endif

// Genesis3D: Reborn Aliases

#endif


