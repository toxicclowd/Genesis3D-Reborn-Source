/****************************************************************************************/
/*  PATH.H																				*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Time-indexed keyframe creation, maintenance, and sampling.				*/
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
#ifndef GR_PATH_H
#define GR_PATH_H

#include "BaseType.h"
#include "Xform3d.h"
#include "Quatern.h"
#include "VFile.h"

#ifdef __cplusplus
	extern "C" {
#endif


typedef struct _grPath grPath;

#define GR_PATH_ROTATION_CHANNEL    1
#define GR_PATH_TRANSLATION_CHANNEL 2

#define GR_PATH_ALL_CHANNELS (GR_PATH_ROTATION_CHANNEL | GR_PATH_TRANSLATION_CHANNEL)

#ifndef GR_PATH_ENUMS
	#define GR_PATH_ENUMS
	typedef enum 
	{
		GR_PATH_INTERPOLATE_LINEAR  = 0,	// linear blend for translation or rotation channel
		GR_PATH_INTERPOLATE_HERMITE,		// hermite cubic spline for translation channel
		GR_PATH_INTERPOLATE_SLERP,			// spherical-linear blend for rotation channel
		GR_PATH_INTERPOLATE_SQUAD,			// higher order blend for rotation channel 'G1' continuity
		//GR_PATH_INTEROPLATE_TRIPOD,		 // not supported yet.
		GR_PATH_INTERPOLATE_HERMITE_ZERO_DERIV = 7	// hermite cubic with zero derivative at keyframes ('easing' curve)
	}grPath_Interpolator;
#endif


#define GR_PATH_MAXIMUM_CUT_TIME (0.001f)

GRAPI void GRCC grPath_CreateRef( grPath *P );

GRAPI grPath *GRCC grPath_Create(
	grPath_Interpolator TranslationInterpolation,	// type of interpolation for translation channel
	grPath_Interpolator RotationInterpolation,	// type of interpolation for rotation channel
	grBoolean Looped);				// True if end of path is connected to head
	// creates new grPath
	//  A looping path should have the same first & last point.  The path
	//  generator will choose arbitrarily between these points for a 
	//  sample exactly at the end of the loop.

GRAPI grPath *GRCC grPath_CreateCopy( const grPath *P );
	
GRAPI void GRCC grPath_Destroy(grPath **PP);		
	// destroys path *PP

GRAPI void GRCC grPath_SetCutMode(grPath *P, grBoolean Enable);
	// If 'Cut' mode is enabled, keyframes with times that differ by <= GR_PATH_MAXIMUM_CUT_TIME
	// are treated as discontinuous.  This can be used to create 'cuts' or 'jumps' in the path.
	// The mode is set per path.

GRAPI grBoolean GRCC grPath_GetCutMode(grPath *P);
	// returns the status of the 'cut' mode in the given path P

//------------------ time based keyframe operations
GRAPI grBoolean GRCC grPath_InsertKeyframe(
	grPath *P, 
	int ChannelMask, 
	grFloat Time, 
	const grXForm3d *Matrix); 
	// inserts a keyframe at a specific time.
	
GRAPI grBoolean GRCC grPath_DeleteKeyframe(
	grPath *P,
	int Index,
	int ChannelMask); 
	// deletes the nth keyframe

GRAPI grBoolean GRCC grPath_GetTimeExtents(
	const grPath *P,
	grFloat *StartTime, 
	grFloat *EndTime);
	// gets the time for the first and last keys in the path (ignoring looping)
	// if there are no keys, return GR_FALSE and times are not set.
	// returns GR_TRUE if there are keys.

//----------------- index based keyframe operations
GRAPI void GRCC grPath_GetKeyframe(
	const grPath *P, 
	int Index,				// gets keyframe[index]
	int Channel,			// for this channel
	grFloat *Time,			// returns the time of the keyframe
	grXForm3d *Matrix);		// returns the matrix of the keyframe
	// retrieves keyframe[index], and it's time

GRAPI int GRCC grPath_GetKeyframeCount(const grPath *P,int Channel);
	// retrieves count of keyframes for a specific channel

GRAPI int GRCC grPath_GetKeyframeIndex(const grPath *P, int Channel, grFloat Time);
	// retrieves the index of the keyframe at a specific time for a specific channel

//----------------- sampling a path  (time based)
GRAPI void GRCC grPath_Sample(const grPath *P, grFloat Time,grXForm3d *Matrix);
	// returns a transform matrix sampled at 'Time'.
	// p is not const because information is cached in p for next sample

GRAPI void GRCC grPath_SampleChannels(
	const grPath *P, 
	grFloat Time, 
	grQuaternion *Rotation, 
	grVec3d *Translation);
	// returns a rotation and a translation for the path at 'Time'
	// p is not const because information is cached in p for next sample

GRAPI grBoolean GRCC grPath_OffsetTimes(grPath *P, 
	int StartingIndex, int ChannelMask, grFloat TimeOffset );
		// slides all samples in path starting with StartingIndex down by TimeOffset

GRAPI grBoolean GRCC grPath_ModifyKeyframe(
	grPath *P,
	int Index,
	int ChannelMask,
	const grXForm3d *Matrix);
	

//------------------ saving/loading a path
GRAPI grPath* GRCC grPath_CreateFromFile(grVFile *F);
	// loads a file 

GRAPI grBoolean GRCC grPath_WriteToFile(const grPath *P, grVFile *F);
	// dumps a path to a file

#ifdef __cplusplus
	}
#endif


#endif
