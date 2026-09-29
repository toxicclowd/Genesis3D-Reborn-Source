/****************************************************************************************/
/*  QKFRAME.H																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Quaternion keyframe interface.											*/
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
/* grQKFrame   (grQuaternion - Keyframe)
	This module handles interpolation for keyframes that contain a quaternion
	This is intended to support Path.c
	grTKArray supplies general support for a time-keyed array, and this supplements
	that support to include the specific time-keyed arrays:
	  An array of grQuaternion interpolated linearly
	  An array of grQuaternion with spherical linear interpolation (SLERP)
	  An array of grQuaternion with spherical quadrangle 
		interpolation (SQUAD) as defined by:
	    Advanced Animation and Rendering Techniques by Alan Watt and Mark Watt

	These are phycially separated and have different base structures because
	the different interpolation techniques requre different additional data.
	
	The two lists are created with different creation calls,
	interpolated with different calls, but insertion and queries share a call.
	
	Quadrangle interpolation requires additional computation after changes are
	made to the keyframe list.  Call grQKFrame_SquadRecompute() to update the
	calculations.
*/
#ifndef GR_QKFRAME_H
#define GR_QKFRAME_H


#include "TKArray.h"
#include "Quatern.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	QKFRAME_LINEAR,
	QKFRAME_SLERP,
	QKFRAME_SQUAD
} grQKFrame_InterpolationType;


grTKArray *GRCC grQKFrame_LinearCreate(void);
	// creates a frame list for linear interpolation

grTKArray *GRCC grQKFrame_SlerpCreate();
	// creates a frame list for spherical linear interpolation	

grTKArray *GRCC grQKFrame_SquadCreate();
	// creates a frame list for spherical linear interpolation	


grBoolean GRCC grQKFrame_Insert(
	grTKArray **KeyList,			// keyframe list to insert into
	grTKArray_TimeType Time,		// time of new keyframe
	const grQuaternion *Q,			// quaternion at new keyframe
	int *Index);					// index of new frame
	// inserts a new keyframe with the given time and vector into the list.

void GRCC grQKFrame_Query(
	const grTKArray *KeyList,		// keyframe list
	int Index,						// index of frame to return
	grTKArray_TimeType *Time,		// time of the frame is returned
	grQuaternion *V);					// vector from the frame is returned
	// returns the vector and the time at keyframe[index] 

void GRCC grQKFrame_Modify(
	grTKArray *KeyList,				// keyframe list
	int Index,						// index of frame to change
	const grQuaternion *Q);			// vector for the new key
	// modifies a vector at keyframe[index]

void GRCC grQKFrame_LinearInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result);			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates linearly
	
void GRCC grQKFrame_SlerpInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result);			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates using spherical linear blending

void GRCC grQKFrame_SquadInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result);			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates using spherical quadratic blending

void GRCC grQKFrame_SquadRecompute(
	int       Looped,			// if keylist has the first key connected to last key
	grTKArray *KeyList,			// list of keys to recompute hermite values for
	grFloat CutInterval);		// intervals <= CutInterval are to be treated as discontinuous
	// rebuild precomputed data for keyframe list.

void GRCC grQKFrame_SlerpRecompute(
	grTKArray *KeyList);		// list of keys to recompute hermite values for
	// rebuild precomputed data for keyframe list.

grBoolean GRCC grQKFrame_WriteToFile(grVFile *pFile, grTKArray *grQKFrame, 
								grQKFrame_InterpolationType InterpolationType, int Looping);
grTKArray *GRCC grQKFrame_CreateFromFile(	
	grVFile						*pFile, 
	grQKFrame_InterpolationType *InterpolationType, 
	int							*Looping,
	grFloat						CutInterval);	// intervals <= CutInterval are to be treated as discontinuous



#ifdef __cplusplus
}
#endif


#endif 
