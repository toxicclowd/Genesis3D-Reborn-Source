/****************************************************************************************/
/*  MOTION.H	                                                                        */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Motion interface.					                                    */
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
#ifndef GR_MOTION_H
#define GR_MOTION_H

/*	motion

	This object is a list of named Path objects

*/

#include <stdio.h>
#include "BaseType.h"
#include "Path.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

// GR_PUBLIC_APIS
typedef struct grMotion grMotion;

GRAPI grMotion *GRCC grMotion_Create(grBoolean ManageNames);

GRAPI void GRCC grMotion_Destroy(grMotion **PM);

// GR_PRIVATE_APIS

GRAPI grBoolean GRCC grMotion_IsValid(const grMotion *M);

	// AddPath adds a reference of P to the motion M.  Ownership is shared - The caller must destroy P.
GRAPI grBoolean GRCC grMotion_AddPath(grMotion *M, grPath *P,const char *Name,int *Index);

GRAPI grBoolean GRCC grMotion_HasNames(const grMotion *M);
GRAPI int32 GRCC grMotion_GetNameChecksum(const grMotion *M);

GRAPI grBoolean GRCC grMotion_RemoveNames(grMotion *M);

GRAPI void GRCC grMotion_SampleChannels(const grMotion *M, int PathIndex, grFloat Time, grQuaternion *Rotation, grVec3d *Translation);
GRAPI grBoolean GRCC grMotion_SampleChannelsNamed(const grMotion *M, const char *PathName, grFloat Time, grQuaternion *Rotation, grVec3d *Translation);

GRAPI void GRCC grMotion_Sample(const grMotion *M, int PathIndex, grFloat Time, grXForm3d *Transform);
GRAPI grBoolean GRCC grMotion_SampleNamed(const grMotion *M, const char *PathName, grFloat Time, grXForm3d *Transform);

	// the returned Paths from _Get functions should not be destroyed.  
	// if ownership is desired, call grPath_CreateRef() to create another owner. 
	// an 'owner' has access to the object regardless of the number of other owners, and 
	// an owner must call the object's destroy method to relinquish ownership
GRAPI grPath *GRCC grMotion_GetPathNamed(const grMotion *M,const char *Name);
GRAPI const char *GRCC grMotion_GetNameOfPath(const grMotion *M, int Index);

// GR_PUBLIC_APIS
GRAPI grPath *GRCC grMotion_GetPath(const grMotion *M,int Index);
GRAPI int GRCC grMotion_GetPathCount(const grMotion *M);


GRAPI grBoolean GRCC grMotion_SetName(grMotion *M, const char * Name);
GRAPI const char *GRCC grMotion_GetName(const grMotion *M);

// GR_PRIVATE_APIS

	// support for compound motions.  A motion can either have sub-motions, or be single motion.
	// these functions support motions that have sub-motions.
GRAPI int GRCC grMotion_GetSubMotionCount(const grMotion*M);

	// the returned motions from these _Get functions should not be destroyed.  
	// if ownership is desired, call grMotion_CreateRef() to create another owner. 
	// an 'owner' has access to the object regardless of the number of other owners, and 
	// an owner must call the object's destroy method to relinquish ownership
GRAPI grMotion *GRCC grMotion_GetSubMotion(const grMotion *M,int Index);
GRAPI grMotion *GRCC grMotion_GetSubMotionNamed(const grMotion *M,const char *Name);
GRAPI grBoolean GRCC grMotion_AddSubMotion(
								grMotion *ParentMotion,
								grFloat TimeScale,			// Scale factor for this submotion
								grFloat TimeOffset,			// Time in parent motion when submotion should start
								grMotion *SubMotion,
								grFloat StartTime,			// Blend start time (relative to submotion)
								grFloat StartMagnitude,		// Blend start magnitude (0..1)
								grFloat EndTime,			// Blend ending time (relative to submotion)
								grFloat EndMagnitude,		// Blend ending magnitude (0..1)
								const grXForm3d *Transform,	// Base transform to apply to this submotion
								int *Index);				// returned motion index

GRAPI grMotion *GRCC  grMotion_RemoveSubMotion(grMotion *ParentMotion, int SubMotionIndex);

// Get/Set submotion time offset.  The time offset is the offset into the 
// compound (parent) motion at which the submotion should start.
GRAPI grFloat   GRCC  grMotion_GetTimeOffset( const grMotion *M,int SubMotionIndex );
GRAPI grBoolean  GRCC grMotion_SetTimeOffset( grMotion *M,int SubMotionIndex,grFloat TimeOffset );

// Get/Set submotion time scale.  Time scaling is applied to the submotion after the TimeOffset
// is applied.  The formula is:  (CurrentTime - TimeOffset) * TimeScale
GRAPI grFloat   GRCC  grMotion_GetTimeScale( const grMotion *M,int SubMotionIndex );
GRAPI grBoolean  GRCC grMotion_SetTimeScale( grMotion *M,int SubMotionIndex,grFloat TimeScale );

// Get blending amount for a particular submotion.  The Time parameter is parent-relative.
GRAPI grFloat    GRCC grMotion_GetBlendAmount( const grMotion *M, int SubMotionIndex, grFloat Time);

// Get/Set blending path.  The keyframe times in the blend path are relative to the submotion.
GRAPI grPath    *GRCC grMotion_GetBlendPath( const grMotion *M,int SubMotionIndex );
GRAPI grBoolean  GRCC grMotion_SetBlendPath( grMotion *M,int SubMotionIndex, grPath *Blend );

GRAPI const grXForm3d *GRCC grMotion_GetBaseTransform( const grMotion *M,int SubMotionIndex );
GRAPI grBoolean  GRCC grMotion_SetBaseTransform( grMotion *M,int SubMotionIndex, grXForm3d *BaseTransform );
GRAPI grBoolean  GRCC grMotion_GetTransform(const grMotion *M, grFloat Time, grXForm3d *Transform);
// GR_PUBLIC_APIS

	// gets time of first key and time of last key (as if motion did not loop)
	// if there are no paths in the motion: returns GR_FALSE and times are not set
	// otherwise returns GR_TRUE
	//
	// For a compound motion, GetTimeExtents will return the extents of the scaled submotions.
	// For a single motion, no scaling is applied.
GRAPI grBoolean GRCC grMotion_GetTimeExtents(const grMotion *M,grFloat *StartTime,grFloat *EndTime);

// Only one event is allowed per time key.

GRAPI grBoolean GRCC grMotion_InsertEvent(grMotion *M, grFloat tKey, const char* String);
	// Inserts the new event and corresponding string.

GRAPI grBoolean GRCC grMotion_DeleteEvent(grMotion *M, grFloat tKey);
	// Deletes the event

GRAPI void GRCC grMotion_SetupEventIterator(
	grMotion *M,
	grFloat StartTime,				// Inclusive search start
	grFloat EndTime);				// Non-inclusive search stop
	// For searching or querying the array for events between two times
	// times are compaired [StartTime,EndTime), '[' is inclusive, ')' is 
	// non-inclusive.  This prepares the grMotion_GetNextEvent() function.

GRAPI grBoolean GRCC grMotion_GetNextEvent(
	grMotion *M,						// Event list to iterate
	grFloat *pTime,				// Return time, if found
	const char **ppEventString);	// Return data, if found
	// Iterates from StartTime to EndTime as setup in grMotion_SetupEventIterator()
	// and for each event between these times [StartTime,EndTime)
	// this function will return Time and EventString returned for that event
	// and the iterator will be positioned for the next search.  When there 
	// are no more events in the range, this function will return GR_FALSE (Time
	// will be 0 and ppEventString will be empty).

GRAPI grBoolean GRCC grMotion_GetEventExtents(const grMotion *M,
			grFloat *FirstEventTime,
			grFloat *LastEventTime);
	// returns the time associated with the first and last events 
	// returns GR_FALSE if there are no events (and Times are not set)


// GR_PRIVATE_APIS
GRAPI grMotion *GRCC grMotion_CreateFromFile(grVFile *f);
GRAPI grBoolean GRCC grMotion_WriteToFile(const grMotion *M,grVFile *pFile);

#ifdef __cplusplus
}
#endif


#endif
