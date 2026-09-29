/****************************************************************************************/
/*  SND.C                                                                               */
/*                                                                                      */
/*  Author: Peter Siamidis                                                              */
/*  Description:    Plays a sound														*/
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
#include <memory.h>
#include <assert.h>
#include "Genesis3D.h"
#include "Camera.h"
#include "Ram.h"
#include "Errorlog.h"
#include "Snd.h"
#include "Sound3d.h"
#include "dsound.h"


////////////////////////////////////////////////////////////////////////////////////////
//	Hardwired stuff
////////////////////////////////////////////////////////////////////////////////////////
#define SND_MINAUDIBLEVOLUME	0.1f

//!!!!!!!!!!!!Get rid of this
typedef struct
{
	EffectResource	*ExternalResource;	// external resource list
} SndResource;

////////////////////////////////////////////////////////////////////////////////////////
//	Interface setup
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean	Snd_Modify( SndResource *Resource, Snd *Data, Snd *NewData, uint32 Flags );
static void			Snd_Pause( SndResource *Resource, Snd *Data, grBoolean Pause );



////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_GetName()
//
////////////////////////////////////////////////////////////////////////////////////////
static char * Snd_GetName(
	void )	// no parameters
{
	return "Sound";

} // Snd_GetName()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_Get3dSoundValues()
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean Snd_Get3dSoundValues(
	EffectResource	*ExternalResource,	// assorted required external resources
	Snd				*Data,				// data of effect
	grFloat			*Volume,			// where to store the volume
	grFloat			*Pan,				// where to store the pan
	grFloat			*Frequency )		// where to store the frequency
{

	// locals
	grXForm3d	SoundXf;
	grFloat			VolDelta, PanDelta;

	// ensure valid data
	assert( ExternalResource != NULL );
	assert( Data != NULL );
	assert( Volume != NULL );
	assert( Pan != NULL );
	assert( Frequency != NULL );

	// get the camera xform
	assert( ExternalResource->Camera != NULL );
	grCamera_GetXForm( ExternalResource->Camera, &SoundXf );

	// get 3d sound values
	grSound3D_GetConfig(
		ExternalResource->World,
		&SoundXf, 
		&( Data->Pos ), 
		Data->Min, 
		0,
		Volume,
		Pan,
		Frequency );

	// return true or false depending on whether or not its worth modifying the sound
	VolDelta = Data->LastVolume - *Volume;
	if ( VolDelta < 0.0f )
	{
		VolDelta = -VolDelta;
	}
	PanDelta = Data->LastPan - *Pan;
	if ( PanDelta < 0.0f )
	{
		PanDelta = -PanDelta;
	}
	if ( ( VolDelta > 0.03f ) || ( PanDelta > 0.02f ) )
	{
		return GR_TRUE;
	}
	return GR_FALSE;

} // Snd_Get3dSoundValues()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_Remove()
//
////////////////////////////////////////////////////////////////////////////////////////
void Snd_Remove(
	EffectResource *Resource,	// available resources
	Snd			*Data )		// effect data
{

	// ensure valid data
	assert( Resource != NULL );
	assert( Data != NULL );

	// stop the sound
	if ( Data->Sound != NULL )
	{
		grSound_StopSound( Resource->Sound, Data->Sound );
	}

	Data->Sound = NULL;
} // Snd_Remove()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_Process()
//
//	Perform processing on an indivual effect. A return of GR_FALSE means that the
//	effect needs to be removed.
//	modified by tom morris May 2005
////////////////////////////////////////////////////////////////////////////////////////
grBoolean Snd_Process(
					  EffectResource	*Resource,		//	available resources
					  float				TimeDelta,		//	elapsed time
					  grBoolean			bMute,			//	mute the sound
					  Snd				*Data )			//	effect data
{
	// locals
	grBoolean	Result = GR_FALSE;
	grFloat		Volume = 0.0f;
	grFloat		Pan = 0.0f;
	grFloat		Frequency = 1.0f;

	// ensure valid data
	assert( Resource != NULL );
	assert( Data != NULL );

	//	Handle MUTE status separately
	//	if the mute is CHECKED, find out if the sound is playing,
	//	then stop it and get outta here.
	if (bMute)
	{
		if ( Data->Sound != NULL)
		{
			int iResult;
			iResult = grSound_GetStatus( Resource->Sound, Data->Sound);
			if (iResult & DSBSTATUS_PLAYING)
			{
				grSound_StopSound( Resource->Sound, Data->Sound );
				Data->Sound = NULL;
				return GR_TRUE;
			}
			return GR_TRUE;
		}
		return GR_TRUE;
	}	//	if (bMute)...

	if ( Data->Sound != NULL)
	{
		// if the user wants to change looping status
		if (Data->Loop == !Data->LastLoop)
		{
			grSound_StopSound( Resource->Sound, Data->Sound );
			Data->Sound = NULL;

			Data->Sound = grSound_PlaySoundDef(	Resource->Sound,
				Data->SoundDef,
				Data->LastVolume, Data->LastPan, Frequency,
				Data->Loop );

			Data->LastLoop = Data->Loop;

			if( Data->Sound == NULL )
			{
				grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Process: sound def play failed.", NULL );
				return GR_TRUE;
			}

			return GR_TRUE;
		}	//	if (Data->Loop == !Data->LastLoop)...

		// adjust the sound if required
		if ( Snd_Get3dSoundValues( Resource, Data, &Volume, &Pan, &Frequency ) == GR_TRUE )
		{
			Result = grSound_ModifySound(Resource->Sound, Data->Sound, Volume, Pan, Frequency );
			if ( Result == GR_FALSE )
			{
				grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Process: sound modify failed.", NULL );
				return GR_TRUE;
			}

			Data->LastVolume = Volume;
			Data->LastPan = Pan;
		}	//	if ( Snd_Get3dSoundValues(...

		// display debug info
#ifdef SND_DEBUGINFO
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 100, "Last:  Vol:%.2f Pan:%.2f", Data->LastVolume, Data->LastPan );
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 120, "Cur:   Vol:%.2f Pan:%.2f", Volume, Pan );
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 140, "Delta: Vol:%.2f Pan:%.2f", fabs( Data->LastVolume - Volume ), fabs( Data->LastPan - Pan ) );
#endif

		// stop the sound if its volume is out of hearing range
		if ( Data->Loop == GR_TRUE)
		{
			if ( (Data->LastVolume < SND_MINAUDIBLEVOLUME))
			{
				grSound_StopSound( Resource->Sound, Data->Sound );
				Data->Sound = NULL;
				Data->LastVolume = 0;
				Data->LastPan = 0;
			}	//	if ( (Data->LastVolume ...
		}	//	if ( Data->Loop == GR_TRUE)...
	}	//	if ( Data->Sound != NULL)...
	else	//	sound hasn't started to play yet...
	{
		Snd_Get3dSoundValues( Resource, Data, &( Data->LastVolume ), &( Data->LastPan ), &Frequency );
		if ( Data->LastVolume >= SND_MINAUDIBLEVOLUME )
		{
			Data->Sound = grSound_PlaySoundDef(	Resource->Sound,
				Data->SoundDef,
				Data->LastVolume, Data->LastPan, Frequency,
				Data->Loop );

			Data->LastLoop = Data->Loop;
		}	//	if ( Data->LastVolume ...
	}	//	else

	// all done
	return GR_TRUE;

	// get rid of warnings
	TimeDelta;

} // Snd_Process()



/* //	previous version pre-May 2005 -- inconsistent loop non-loop performance	
grBoolean Snd_Process(
	EffectResource	*Resource,		// available resources
	float		TimeDelta,		// elapsed time
	grBoolean	bMute,
	Snd			*Data )			// effect data
{

	// locals
	grBoolean	Result;
	grFloat		Volume;
	grFloat		Pan = 0.0f;
	grFloat		Frequency = 1.0f;

	// ensure valid data
	assert( Resource != NULL );
	assert( Data != NULL );

	//	if the mute is CHECKED, crank it down, down, down...
	//	then get outta here.
	if ( bMute)
	{
		if (Data->Sound != NULL)
		{
			grFloat	fMute = 0.0f;
			Result = grSound_ModifySound(	Resource->Sound,
				Data->Sound,
				fMute, Pan, Frequency );
			if ( Result == GR_FALSE )
			{
				grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Process: sound modify failed.", NULL );
				return GR_TRUE;
			}

			Data->LastVolume = fMute;
			Data->LastPan = Pan;

		}
		return GR_TRUE;
	}

	// stop the sound if required...
	if ( Data->Sound != NULL )
	{
		// if the sound is done then zap this effect
		if (	( Data->Loop == GR_FALSE ) &&
			( grSound_SoundIsPlaying( Resource->Sound, Data->Sound ) == GR_FALSE ) )
		{
			return GR_FALSE;
		}

		// adjust the sound if required
		if ( Snd_Get3dSoundValues( Resource, Data, &Volume, &Pan, &Frequency ) == GR_TRUE )
		{

			Result = grSound_ModifySound(	Resource->Sound,
				Data->Sound,
				Volume, Pan, Frequency );
			if ( Result == GR_FALSE )
			{
				grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Process: sound modify failed.", NULL );
				return GR_TRUE;
			}

			Data->LastVolume = Volume;
			Data->LastPan = Pan;
		}

		// display debug info
#ifdef SND_DEBUGINFO
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 100, "Last:  Vol:%.2f Pan:%.2f", Data->LastVolume, Data->LastPan );
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 120, "Cur:   Vol:%.2f Pan:%.2f", Volume, Pan );
		grEngine_Printf( Resource->ExternalResource->Engine, 100, 140, "Delta: Vol:%.2f Pan:%.2f", fabs( Data->LastVolume - Volume ), fabs( Data->LastPan - Pan ) );
#endif

		// stop the sound if its volume is out of hearing range
		if ( Data->Loop == GR_TRUE)
		{
			if ( (Data->LastVolume < SND_MINAUDIBLEVOLUME))
			{
				grSound_StopSound( Resource->Sound, Data->Sound );
				Data->Sound = NULL;
				Data->LastVolume = 0;
				Data->LastPan = 0;
			}
		}
	}
	// ...or restart it
	else
	{
		// only restart looping non paused sounds
		if (	( Data->Loop == GR_TRUE ) &&
			( Data->Paused == GR_FALSE ) )
		{

			// restart it if its volume is now in hearing range
			Snd_Get3dSoundValues( Resource, Data, &( Data->LastVolume ), &( Data->LastPan ), &Frequency );
			if ( Data->LastVolume >= SND_MINAUDIBLEVOLUME )
			{
				Data->Sound = grSound_PlaySoundDef(	Resource->Sound,
					Data->SoundDef,
					Data->LastVolume, Data->LastPan, Frequency,
					Data->Loop );
				if( Data->Sound == NULL )
				{
					grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Process: sound def play failed.", NULL );
					return GR_TRUE;
				}

			}
			else
			{
				Data->LastVolume = 0;
				Data->LastPan = 0;
			}
		}
	}

	// all done
	return GR_TRUE;

	// get rid of warnings
	TimeDelta;

} // Snd_Process()
*/

////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_Modify()
//
//	Adjust the effect.
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean Snd_Modify(
	SndResource	*Resource,	// available resources
	Snd			*Data,		// effect data
	Snd			*NewData,	// new data
	uint32		Flags )		// user flags
{

	// ensure valid data
	assert( Resource != NULL );
	assert( Data != NULL );
	assert( NewData != NULL );

	// adjust the position
	if ( Flags & SND_POS )
	{

		// save new position
		grVec3d_Copy( &( NewData->Pos ), &( Data->Pos ) );

		// adjust the sound
		if ( Data->Sound != NULL )
		{

			// locals
			grBoolean	Result;
			grFloat		Volume;
			grFloat		Pan;
			grFloat		Frequency;

			// adjust the sound
			if ( Snd_Get3dSoundValues( Resource->ExternalResource, Data, &Volume, &Pan, &Frequency ) == GR_TRUE )
			{
				Result = grSound_ModifySound(	Resource->ExternalResource->Sound,
												Data->Sound,
												Volume, Pan, Frequency );
				if( Result == GR_FALSE )
				{
					grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Modify: sound modify failed.", NULL );
					return GR_FALSE;
				}

				Data->LastVolume = Volume;
				Data->LastPan = Pan;
			}
		}
	}

	// all done
	return GR_TRUE;

} // Snd_Modify()



////////////////////////////////////////////////////////////////////////////////////////
//
//	Snd_Pause()
//
//	Pause/unpause effect.
//
////////////////////////////////////////////////////////////////////////////////////////
void Snd_Pause(
	SndResource	*Resource,	// available resources
	Snd			*Data,		// effect data
	grBoolean	Pause )		// new pause state
{

	// ensure valid data
	assert( Resource != NULL );
	assert( Data != NULL );

	// pause the sound...
	if (	( Pause == GR_TRUE ) &&
			( Data->Paused == GR_FALSE ) )
	{
		if ( Data->Sound != NULL )
		{
			grSound_StopSound( Resource->ExternalResource->Sound, Data->Sound );
			Data->Sound = NULL;
			Data->LastVolume = 0;
			Data->LastPan = 0;
		}
		Data->Paused = GR_TRUE;
	}
	// ...or start it up again
	else if (	( Pause == GR_FALSE ) &&
				( Data->Paused == GR_TRUE ) )
	{

		// locals
		grFloat	Frequency;

		// play the sound
		if (	( Data->Sound == NULL ) ||
				( grSound_SoundIsPlaying( Resource->ExternalResource->Sound, Data->Sound ) == GR_FALSE ) )
		{
			Snd_Get3dSoundValues( Resource->ExternalResource, Data, &( Data->LastVolume ), &( Data->LastPan ), &Frequency );
			Data->Sound = grSound_PlaySoundDef(	Resource->ExternalResource->Sound,
												Data->SoundDef,
												Data->LastVolume, Data->LastPan, Frequency,
												Data->Loop );
			if( Data->Sound == NULL )
			{
				grErrorLog_AddString( GR_ERR_SOUND_RESOURCE, "Snd_Pause: sound def play failed.", NULL );
				return;
			}
			Data->Paused = GR_FALSE;
		}
	}

} // Snd_Pause()
