/****************************************************************************************/
/*  PATH.C																				*/
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
#include <assert.h>
#include <math.h>   //fmod()

#include "Path.h"
#include "Quatern.h"
#include "Errorlog.h"
#include "Ram.h"
#include "TKArray.h"
#include "VKFrame.h"
#include "QKFrame.h"
#include "Vec3d.h"

#define min(aa,bb)  (( (aa)>(bb) ) ? (bb) : (aa) )
#define max(aa,bb)  (( (aa)>(bb) ) ? (aa) : (bb) )


#define grPath_TimeType grFloat

typedef int8 Bool8;

typedef void (GRCC *InterpolationFunction)(
	const void *KF1,
	const void *KF2, 
	grPath_TimeType T,
	void *Result);


#define FLAG_DIRTY   (0x01)
#define FLAG_LOOPED  (0x01)
#define FLAG_OTHER	 (0x696C6345)
#define FLAG_EMPTY	 (0x21657370)

typedef enum
{
	GR_PATH_VK_LINEAR,
	GR_PATH_VK_HERMITE,
	GR_PATH_VK_HERMITE_ZERO_DERIV,
	GR_PATH_QK_LINEAR,
	GR_PATH_QK_SLERP,
	GR_PATH_QK_SQUAD,
	GR_PATH_MANY_INTERPOLATORS
} grPath_InterpolationType;

typedef struct
{
	grTKArray *KeyList;
	// was int.. int InterpolationType;				// type of interpolation for channel
	grPath_InterpolationType InterpolationType;
	
	grPath_TimeType StartTime;			// First time in channel's path
	grPath_TimeType EndTime;			// Last time in channel's path

	// --remember keys used for last sample--
	int32 LastKey1;						// smaller key
	int32 LastKey2;						// larger key (keys may be equal)
	grPath_TimeType LastKey1Time;		// Time at LastKey1
	grPath_TimeType LastKey2Time;		// Time at LastKey2
									// if last key is not valid: LastKey1Time > LastKey2Time
} grPath_Channel;


typedef struct _grPath
{
	grPath_Channel Rotation;
	grPath_Channel Translation;
	unsigned int Dirty    : 1;						
	unsigned int Looped   : 1;
	unsigned int AllowCuts: 1;
	unsigned int RefCount :29;
} grPath;


typedef struct 
{
	InterpolationFunction InterpolationTable[GR_PATH_MANY_INTERPOLATORS];
	int32 Flags[2];
} grPath_StaticType;

grPath_StaticType grPath_Statics = 
{
	{ 	grVKFrame_LinearInterpolation,
		grVKFrame_HermiteInterpolation,
		grVKFrame_HermiteInterpolation,
		grQKFrame_LinearInterpolation,
		grQKFrame_SlerpInterpolation,
		grQKFrame_SquadInterpolation
	},
	{FLAG_OTHER,FLAG_EMPTY},
};



static grVKFrame_InterpolationType GRCF grPath_PathToVKInterpolation(grPath_InterpolationType I)
{
	switch (I)
		{
			case (GR_PATH_VK_LINEAR):			  return VKFRAME_LINEAR;
			case (GR_PATH_VK_HERMITE):			  return VKFRAME_HERMITE;
			case (GR_PATH_VK_HERMITE_ZERO_DERIV): return VKFRAME_HERMITE_ZERO_DERIV;
			default: assert(0);
		}
	return VKFRAME_LINEAR;  // this is just for warning removal
}
			
static grPath_InterpolationType GRCF grPath_VKToPathInterpolation(grVKFrame_InterpolationType I)
{
	switch (I)
		{
			case (VKFRAME_LINEAR):				return GR_PATH_VK_LINEAR;
			case (VKFRAME_HERMITE):				return GR_PATH_VK_HERMITE;
			case (VKFRAME_HERMITE_ZERO_DERIV):  return GR_PATH_VK_HERMITE_ZERO_DERIV;
			default: assert(0);
		}
	return GR_PATH_VK_LINEAR; // this is just for warning removal
}

static grQKFrame_InterpolationType GRCF grPath_PathToQKInterpolation(grPath_InterpolationType I)
{
	switch (I)
		{
			case (GR_PATH_QK_LINEAR):	return QKFRAME_LINEAR;
			case (GR_PATH_QK_SLERP):	return QKFRAME_SLERP;
			case (GR_PATH_QK_SQUAD):	return QKFRAME_SQUAD;
			default: assert(0);
		}
	return QKFRAME_LINEAR;  // this is just for warning removal
}
			
static grPath_InterpolationType GRCF grPath_QKToPathInterpolation(grQKFrame_InterpolationType I)
{
	switch (I)
		{
			case (QKFRAME_LINEAR):	return GR_PATH_QK_LINEAR;
			case (QKFRAME_SLERP):	return GR_PATH_QK_SLERP;
			case (QKFRAME_SQUAD):	return GR_PATH_QK_SQUAD;
			default: assert(0);
		}
	return GR_PATH_QK_LINEAR; // this is just for warning removal
}


GRAPI void GRCC grPath_CreateRef( grPath *P )
{
	assert( P != NULL );
	P->RefCount++;
}

GRAPI void GRCC grPath_SetCutMode(grPath *P, grBoolean Enable)
{
	assert( P != NULL );
	P->AllowCuts = Enable;
	P->Dirty = FLAG_DIRTY;
}

GRAPI grBoolean GRCC grPath_GetCutMode(grPath *P)
{
	assert( P != NULL );
	return (P->AllowCuts);
}
	

GRAPI grPath *GRCC grPath_Create(
	grPath_Interpolator TranslationInterpolation,	// type of interpolation for translation channel
	grPath_Interpolator RotationInterpolation,	// type of interpolation for rotation channel
	grBoolean Looped)				// GR_TRUE if end of path is connected to head
	
{
	grPath *P;

	P = (grPath *)grRam_AllocateClear(sizeof(grPath));

	if ( P == NULL )
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grPath_Create.");
		return NULL;
	}

	P->Rotation.KeyList    = NULL;
	P->Translation.KeyList = NULL;
	
	P->RefCount  = 0;
	P->Dirty     = FLAG_DIRTY;

	if (Looped==GR_TRUE)
		P->Looped = FLAG_LOOPED;
	else
		P->Looped = 0;


	switch (RotationInterpolation)
		{
			case (GR_PATH_INTERPOLATE_LINEAR):
				P->Rotation.InterpolationType = GR_PATH_QK_LINEAR;
				break;
			case (GR_PATH_INTERPOLATE_SLERP):
				P->Rotation.InterpolationType = GR_PATH_QK_SLERP; 
				break;
			case (GR_PATH_INTERPOLATE_SQUAD):
				P->Rotation.InterpolationType = GR_PATH_QK_SQUAD;
				break;
			default:
				assert(0);
		}
	
	P->Rotation.KeyList = NULL;

	switch (TranslationInterpolation)
		{
			case (GR_PATH_INTERPOLATE_LINEAR):
				P->Translation.InterpolationType = GR_PATH_VK_LINEAR;
				break;
			case (GR_PATH_INTERPOLATE_HERMITE):
				P->Translation.InterpolationType = GR_PATH_VK_HERMITE;
				break;
			case (GR_PATH_INTERPOLATE_HERMITE_ZERO_DERIV):
				P->Translation.InterpolationType = GR_PATH_VK_HERMITE_ZERO_DERIV;
				break;
			default:
				assert(0);
		}

	P->Translation.KeyList = NULL;

	return P;
}

static grBoolean GRCF grPath_SetupRotationKeyList(grPath *P)
{
	assert( P != NULL );
	switch (P->Rotation.InterpolationType)
		{
			case (GR_PATH_QK_LINEAR):
				P->Rotation.KeyList = grQKFrame_LinearCreate();
				break;
			case (GR_PATH_QK_SLERP):
				P->Rotation.KeyList = grQKFrame_SlerpCreate();
				break;
			case (GR_PATH_QK_SQUAD):
				P->Rotation.KeyList = grQKFrame_SquadCreate();
				break;
			default:
				assert(0);
		}
	if (P->Rotation.KeyList == NULL)
		{
			return GR_FALSE;
		}
	return GR_TRUE;	
}

static grBoolean GRCF grPath_SetupTranslationKeyList(grPath *P)
{
	assert( P != NULL );
	switch (P->Translation.InterpolationType)
		{
			case (GR_PATH_VK_LINEAR):
				P->Translation.KeyList = grVKFrame_LinearCreate();
				break;
			case (GR_PATH_VK_HERMITE):
				P->Translation.KeyList = grVKFrame_HermiteCreate();
				break;
			case (GR_PATH_VK_HERMITE_ZERO_DERIV):
				P->Translation.KeyList = grVKFrame_HermiteCreate();
				break;
			default:
				assert(0);
		}
	if (P->Translation.KeyList == NULL)
		{
			return GR_FALSE;
		}
	return GR_TRUE;
}

GRAPI grPath *GRCC grPath_CreateCopy(const grPath *Src)
{
	grPath *P;
	grPath_TimeType Time;
	grBoolean Looped;

	int i,Count;
	grPath_Interpolator RInterp;
	grPath_Interpolator TInterp;

	assert ( Src != NULL );

	switch (Src->Rotation.InterpolationType)
		{
			case (GR_PATH_QK_LINEAR):
				RInterp = GR_PATH_INTERPOLATE_LINEAR;
				break;
			case (GR_PATH_QK_SLERP):
				RInterp = GR_PATH_INTERPOLATE_SLERP;
				break;
			case (GR_PATH_QK_SQUAD):
				RInterp = GR_PATH_INTERPOLATE_SQUAD;
				break;
			default:
				assert(0);
		}
	
	switch (Src->Translation.InterpolationType)
		{
			case (GR_PATH_VK_LINEAR):
				TInterp = GR_PATH_INTERPOLATE_LINEAR;
				break;
			case (GR_PATH_VK_HERMITE):
				TInterp = GR_PATH_INTERPOLATE_HERMITE;
				break;
			case (GR_PATH_VK_HERMITE_ZERO_DERIV):
				TInterp = GR_PATH_INTERPOLATE_HERMITE_ZERO_DERIV;
				break;
			default:
				assert(0);
		}
	
	if (Src->Looped)
		Looped = GR_TRUE;
	else
		Looped = GR_FALSE;

	P = grPath_Create(TInterp, RInterp, Looped);	
	if (P == NULL)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateCopy.");
			return NULL;
		}

	{
		grVec3d V;
		Count = 0;
		if (Src->Translation.KeyList != NULL)
			{
				Count = grTKArray_NumElements(Src->Translation.KeyList);
			}
		if (Count>0)
			{
				if (grPath_SetupTranslationKeyList(P)==GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateCopy.");
						grPath_Destroy(&P);
						return NULL;
					}

				for (i=0; i<Count; i++)
					{
						int Index;
						grVKFrame_Query(Src->Translation.KeyList, i, &Time, &V);
						if (grVKFrame_Insert(&(P->Translation.KeyList), Time, &V,&Index) == GR_FALSE)
							{
								grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateCopy.");
								grPath_Destroy(&P);
								return NULL;
							}
					}
			}
	}

	{
		grQuaternion Q;
		Count = 0;
		if (Src->Rotation.KeyList != NULL)
			{
				Count = grTKArray_NumElements(Src->Rotation.KeyList);
			}
		if (Count>0)
			{
				if (grPath_SetupRotationKeyList(P)==GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateCopy.");
						grPath_Destroy(&P);
						return NULL;
					}

				for (i=0; i<Count; i++)
					{
						int Index;
						grQKFrame_Query(Src->Rotation.KeyList, i, &Time, &Q);
						if (grQKFrame_Insert(&(P->Rotation.KeyList), Time, &Q, &Index) == GR_FALSE)
							{
								grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateCopy.");
								grPath_Destroy(&P);
								return NULL;
							}
					}
			}
	}
	return P;
}
	


GRAPI void GRCC grPath_Destroy(grPath **PP)
{
	grPath *P;
	
	assert( PP  != NULL );
	assert( *PP != NULL );
	
	P = *PP;

	if ( P->RefCount > 0)
		{
			P->RefCount -- ;
			return;
		}
	if ( P->Rotation.KeyList != NULL)
	{
		grTKArray_Destroy(&(P->Rotation.KeyList));
		P->Rotation.KeyList = NULL;
	}

	if ( P->Translation.KeyList != NULL)
	{
		grTKArray_Destroy(&(P->Translation.KeyList));
		P->Translation.KeyList = NULL;
	}

	grRam_Free(*PP);

	*PP = NULL;
}


static void GRCF grPath_Recompute(grPath *P)
	// Recompute any pre-computed constants for the current path.
{
	grBoolean Looped;
	assert(P);

	P->Dirty = 0;

	P->Translation.LastKey1Time = 0.0f;
	P->Translation.LastKey2Time = -1.0f;
	if (P->Looped)
		Looped = GR_TRUE;
	else
		Looped = GR_FALSE;

	if (P->Translation.KeyList != NULL)
	{
		if (grTKArray_NumElements(P->Translation.KeyList) > 0 )
		{
			P->Translation.StartTime =	grTKArray_ElementTime(P->Translation.KeyList,0);
			P->Translation.EndTime   =	grTKArray_ElementTime(P->Translation.KeyList,
										grTKArray_NumElements(P->Translation.KeyList) - 1);
		}
		if(P->Translation.InterpolationType == GR_PATH_VK_HERMITE)
			grVKFrame_HermiteRecompute(Looped, GR_FALSE, P->Translation.KeyList,GR_PATH_MAXIMUM_CUT_TIME);
		else if (P->Translation.InterpolationType == GR_PATH_VK_HERMITE_ZERO_DERIV)
			grVKFrame_HermiteRecompute(Looped, GR_TRUE, P->Translation.KeyList,GR_PATH_MAXIMUM_CUT_TIME);
	}
	
	P->Rotation.LastKey1Time = 0.0f;
	P->Rotation.LastKey2Time = -1.0f;

	if (P->Rotation.KeyList != NULL)
	{
		if (grTKArray_NumElements(P->Rotation.KeyList) > 0 )
		{
			P->Rotation.StartTime = grTKArray_ElementTime(P->Rotation.KeyList,0);
			P->Rotation.EndTime   = grTKArray_ElementTime(P->Rotation.KeyList,
									grTKArray_NumElements(P->Rotation.KeyList) - 1);
		}
		if (P->Rotation.InterpolationType == GR_PATH_QK_SQUAD)
			grQKFrame_SquadRecompute(Looped, P->Rotation.KeyList,GR_PATH_MAXIMUM_CUT_TIME);
		else if (P->Rotation.InterpolationType == GR_PATH_QK_SLERP)
			grQKFrame_SlerpRecompute(P->Rotation.KeyList);

	}
}	

//------------------ time based keyframe operations
GRAPI grBoolean GRCC grPath_InsertKeyframe(
	grPath *P, 
	int ChannelMask, 
	grPath_TimeType Time, 
	const grXForm3d *Matrix)
{
	int VIndex;
	int QIndex = 0;
	assert( P != NULL );
	assert( Matrix != NULL );
	assert( ( ChannelMask & GR_PATH_ROTATION_CHANNEL    ) ||
			( ChannelMask & GR_PATH_TRANSLATION_CHANNEL ) );
	
	if (ChannelMask & GR_PATH_ROTATION_CHANNEL)
	{	
		grQuaternion Q;
		grQuaternion_FromMatrix(Matrix, &Q);
		grQuaternion_Normalize(&Q);
		if (P->Rotation.KeyList==NULL)
		{
			if (grPath_SetupRotationKeyList(P)==GR_FALSE)
				{
					grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_InsertKeyframe.");
					return GR_FALSE;
				}
		}
		if (grQKFrame_Insert(&(P->Rotation.KeyList), Time, &Q, &QIndex) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_InsertKeyframe.");
			return GR_FALSE;
		}
	}

	
	if (ChannelMask & GR_PATH_TRANSLATION_CHANNEL)
	{
		grBoolean ErrorOccured = GR_FALSE;
		if (P->Translation.KeyList == NULL)
			{
				if (grPath_SetupTranslationKeyList(P)==GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_InsertKeyframe.");
						ErrorOccured = GR_TRUE;
					}
			}
		if (ErrorOccured == GR_FALSE)
			{
				if (grVKFrame_Insert( &(P->Translation.KeyList), Time, &(Matrix->Translation), &VIndex) == GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_InsertKeyframe.");
						ErrorOccured = GR_TRUE;
					}
			}
		if (ErrorOccured != GR_FALSE)
			{
				if (ChannelMask & GR_PATH_ROTATION_CHANNEL)
					{	// clean up previously inserted rotation
						if (grTKArray_DeleteElement(&(P->Rotation.KeyList),QIndex)==GR_FALSE)
							{
								grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_InsertKeyframe.");
							}
					}
				P->Dirty = FLAG_DIRTY;
				return GR_FALSE;
			}
	}

	P->Dirty = FLAG_DIRTY;

	return GR_TRUE;
}
	
GRAPI grBoolean GRCC grPath_DeleteKeyframe(
	grPath *P,
	int Index,
	int ChannelMask)
{
	int ErrorOccured= 0;

	assert( P != NULL );
	assert( ( ChannelMask & GR_PATH_ROTATION_CHANNEL    ) ||
			( ChannelMask & GR_PATH_TRANSLATION_CHANNEL ) );

	if (ChannelMask & GR_PATH_ROTATION_CHANNEL)
	{
		if (grTKArray_DeleteElement( &(P->Rotation.KeyList), Index) == GR_FALSE)
		{
			ErrorOccured = 1;
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_DeleteKeyframe.");
		}
	}
			
	if (ChannelMask & GR_PATH_TRANSLATION_CHANNEL)
	{
		if (grTKArray_DeleteElement( &(P->Translation.KeyList), Index) == GR_FALSE)
		{
			ErrorOccured = 1;
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPath_DeleteKeyframe.");
		}
	}

	P->Dirty = FLAG_DIRTY;


	if (ErrorOccured)
	{
		return GR_FALSE;
	}

	return GR_TRUE;
}


GRAPI void GRCC grPath_GetKeyframe(
	const grPath *P, 
	int Index,				// gets keyframe[index]
	int Channel,			// for this channel
	grPath_TimeType *Time,	// returns the time of the keyframe
	grXForm3d *Matrix) 		// returns the matrix of the keyframe
{
	assert( P != NULL );
	assert( Index >= 0 );
	assert( Time != NULL );
	assert( Matrix != NULL );

	grXForm3d_SetIdentity(Matrix);

	switch (Channel)
	{
	case (GR_PATH_ROTATION_CHANNEL):
		{
			grQuaternion Q;
			assert( Index < grTKArray_NumElements(P->Rotation.KeyList) );
			grQKFrame_Query(P->Rotation.KeyList, Index, Time, &Q);
			grQuaternion_ToMatrix(&Q, Matrix);
		}
		break;

	case (GR_PATH_TRANSLATION_CHANNEL):
		{
			assert( Index < grTKArray_NumElements(P->Translation.KeyList) );
			grVKFrame_Query(P->Translation.KeyList, Index, Time, &(Matrix->Translation));
		}
		break;

	default:
		assert(0);
	}
}

GRAPI grBoolean GRCC grPath_ModifyKeyframe(
	grPath *P, 
	int Index,						// keyframe[index]
	int ChannelMask,				// for this channel
	const grXForm3d *Matrix) 		// new matrix for the keyframe
{
	assert( P != NULL );
	assert( Index >= 0 );
	assert( Matrix != NULL );
	assert( ( ChannelMask & GR_PATH_ROTATION_CHANNEL    ) ||
			( ChannelMask & GR_PATH_TRANSLATION_CHANNEL ) );


	if (ChannelMask & GR_PATH_ROTATION_CHANNEL)
		{
			grQuaternion Q;
			assert( Index < grTKArray_NumElements(P->Rotation.KeyList) );
			grQuaternion_FromMatrix(Matrix, &Q);
			grQuaternion_Normalize(&Q);
			grQKFrame_Modify(P->Rotation.KeyList, Index, &Q);
		}

	if (ChannelMask & GR_PATH_TRANSLATION_CHANNEL)
		{
			assert( Index < grTKArray_NumElements(P->Translation.KeyList) );
			grVKFrame_Modify(P->Translation.KeyList, Index, &(Matrix->Translation));
		}

	P->Dirty = FLAG_DIRTY;
	return GR_TRUE;
}


GRAPI int GRCC grPath_GetKeyframeCount(const grPath *P, int Channel)
{
	assert( P != NULL );

	switch (Channel)
	{
		case (GR_PATH_ROTATION_CHANNEL):
			if (P->Rotation.KeyList!=NULL)
				{
					return grTKArray_NumElements(P->Rotation.KeyList);
				}
			else
				{
					return 0;
				}
			break;

		case (GR_PATH_TRANSLATION_CHANNEL):
			if (P->Translation.KeyList!=NULL)
				{
					return grTKArray_NumElements(P->Translation.KeyList);
				}
			else
				{
					return 0;
				}
			break;

		default:
			assert(0);
	}
	return 0; // this is just for warning removal
}

GRAPI int GRCC grPath_GetKeyframeIndex(const grPath *P, int Channel, grFloat Time)
	// retrieves the index of the keyframe at a specific time for a specific channel
{
	int KeyIndex;
	grTKArray *Array = NULL;

	assert ((Channel == GR_PATH_TRANSLATION_CHANNEL) ||
			(Channel == GR_PATH_ROTATION_CHANNEL));

	switch (Channel)
	{
		case GR_PATH_ROTATION_CHANNEL :
			Array = P->Rotation.KeyList;
			break;

		case GR_PATH_TRANSLATION_CHANNEL :
			Array = P->Translation.KeyList;
			break;
	}

	// find the time in the channel's array
	KeyIndex = grTKArray_BSearch (Array, Time);
	if (KeyIndex != -1)
	{
		// since grTKArray_BSearch will return the "closest" key,
		// I need to make sure that it's exact...
		if (fabs (Time - grTKArray_ElementTime (Array, KeyIndex)) > GR_TKA_TIME_TOLERANCE)
		{
			KeyIndex = -1;
		}
	}

	return KeyIndex;
}


static grPath_TimeType GRCF grPath_AdjustTimeForLooping(
	grBoolean Looped,
	grPath_TimeType Time, 
	grPath_TimeType TStart, 
	grPath_TimeType TEnd)
{
	if (Looped!=GR_FALSE)
	{
		if (Time < TStart)
		{
			return (grPath_TimeType)fmod(Time - TStart, TEnd - TStart) + TStart + TEnd;
		}
		else
		{
			if (Time >= TEnd)
			{
				if(TStart + GR_TKA_TIME_TOLERANCE > TEnd)
					return TStart;

				return (grPath_TimeType)fmod(Time - TStart, TEnd - TStart) + TStart;
			}
			else
			{
				return Time;
			}
		}
	}
	else
	{
		return Time;
	}
}


static grBoolean GRCF grPath_SampleChannel(
	const grPath_Channel *Channel,			// channel to sample
	grBoolean Looped,
	grBoolean AllowCuts,
	grPath_TimeType Time, 
	void *Result)
				// return GR_TRUE if sample was made,
				// return GR_FALSE if no sample was made (no keyframes)
{
	int Index1,Index2;				// index of keyframe just before and after Time
	grPath_TimeType Time1, Time2;	// Times in those keyframes	
	grPath_TimeType T;				// 0..1 blending factor
	grPath_TimeType AdjTime;		// parameter Time adjusted for looping.
	int Length;
	
	assert( Channel != NULL );
	assert( Result != NULL );

	if (Channel->KeyList == NULL)	
		return GR_FALSE;
	
	Length = grTKArray_NumElements( Channel->KeyList );
			
	if ( Length == 0 )
	{
		//Interpolate(Channel,NULL,NULL,Time,Result);
		return GR_FALSE;
	}

	AdjTime = grPath_AdjustTimeForLooping(Looped,Time,
			Channel->StartTime,Channel->EndTime);

	if (	( Channel->LastKey1Time <= AdjTime ) && 
			( AdjTime < Channel->LastKey2Time  ) )
	{  
		Index1 = Channel->LastKey1;
		Index2 = Channel->LastKey2;
		Time1  = Channel->LastKey1Time;
		Time2  = Channel->LastKey2Time;
	}
	else
	{
		Index1 = grTKArray_BSearch( Channel->KeyList,
								AdjTime);
		Index2 = Index1 + 1;

		// edje conditions: if Time is off end of path's time, use end point twice
		if ( Index1 < 0 )	
		{
			if (Looped!=GR_FALSE) 
			{
				Index1 = Length -1;
			}
			else
			{
				Index1 = 0;
			}
		}
		if ( Index2 >= Length )
		{
			if (Looped!=GR_FALSE)
			{
				Index2 = 0;
			}
			else
			{
				Index2 = Length - 1;
			}
		}
		((grPath_Channel *)Channel)->LastKey1 = Index1;
		((grPath_Channel *)Channel)->LastKey2 = Index2;
		Time1 = ((grPath_Channel *)Channel)->LastKey1Time = grTKArray_ElementTime(Channel->KeyList, Index1);
		Time2 = ((grPath_Channel *)Channel)->LastKey2Time = grTKArray_ElementTime(Channel->KeyList, Index2);
	}
	
	if (Index1 == Index2)
		T=0.0f;			// Time2 == Time1 !
	else
		{
			if (AllowCuts && ((Time2-Time1)<GR_PATH_MAXIMUM_CUT_TIME))
				{
					T=0.0f;
				}
			else	
				{
					T = (AdjTime-Time1) / (Time2 - Time1);
				}
		}
	
	grPath_Statics.InterpolationTable[Channel->InterpolationType](
				grTKArray_Element(Channel->KeyList,Index1),
				grTKArray_Element(Channel->KeyList,Index2),
				T,Result);

	return GR_TRUE;
}


GRAPI void GRCC grPath_Sample(const grPath *P, grPath_TimeType Time, grXForm3d *Matrix)
{
	grQuaternion	Rotation;
	grVec3d		Translation;

	assert( P != NULL );
	assert( Matrix != NULL );


	if (P->Dirty)
		{
			grPath_Recompute((grPath *)P);
		}

	if(grPath_SampleChannel(&(P->Rotation), P->Looped, P->AllowCuts, Time, (void*)&Rotation) == GR_TRUE)
	{
		grQuaternion_ToMatrix(&Rotation, Matrix);
	}
	else
	{
		grXForm3d_SetIdentity(Matrix);
	}

	if(grPath_SampleChannel(&(P->Translation), P->Looped, P->AllowCuts, Time, (void*)&Translation) == GR_TRUE)
	{
		Matrix->Translation = Translation;
	}
	else
	{
		Matrix->Translation.X = Matrix->Translation.Y = Matrix->Translation.Z = 0.0f;
	}

}

GRAPI void GRCC grPath_SampleChannels(const grPath *P, grPath_TimeType Time, grQuaternion *Rotation, grVec3d *Translation)
{
	grBoolean Looped;
	assert( P != NULL );
	assert( Rotation != NULL );
	assert( Translation != NULL );

	if (P->Dirty)
		{
			grPath_Recompute((grPath *)P);
		}

	if (P->Looped)
		Looped = GR_TRUE;
	else
		Looped = GR_FALSE;
	
	if(grPath_SampleChannel(&(P->Rotation), Looped, P->AllowCuts, Time, (void*)Rotation) == GR_FALSE)
	{
		grQuaternion_SetNoRotation(Rotation);
	}

	if(grPath_SampleChannel(&(P->Translation), Looped, P->AllowCuts, Time, (void*)Translation) == GR_FALSE)
	{
		Translation->X  = Translation->Y = Translation->Z = 0.0f;
	}
}


GRAPI grBoolean GRCC grPath_GetTimeExtents(const grPath *P, grPath_TimeType *StartTime, grPath_TimeType *EndTime)
	// returns false and times are unchanged if there is no extent (no keys)
{
	grPath_TimeType TransStart,TransEnd,RotStart,RotEnd;

	int RCount,TCount;
	assert( P != NULL );
	assert( StartTime != NULL );
	assert( EndTime != NULL );
	// this is a pain because each channel may have 0,1, or more keys
	
	if (P->Rotation.KeyList!=NULL)
		RCount = grTKArray_NumElements( P->Rotation.KeyList );
	else
		RCount = 0;

	if (P->Translation.KeyList!=NULL)
		TCount = grTKArray_NumElements( P->Translation.KeyList );
	else
		TCount = 0;
	
	if (RCount>0)
		{	
			RotStart = grTKArray_ElementTime(P->Rotation.KeyList, 0);
			if (RCount>1)
				{
					RotEnd = grTKArray_ElementTime(P->Rotation.KeyList, RCount-1);
				}
			else
				{
					RotEnd = RotStart;
				}
			if (TCount>0)
				{	// Rotation and Translation keys
					TransStart = grTKArray_ElementTime(P->Translation.KeyList, 0);
					if (TCount>1)
						{
							TransEnd = grTKArray_ElementTime(P->Translation.KeyList,TCount-1);
						}
					else
						{
							TransEnd = TransStart;
						}

					*StartTime = min(TransStart,RotStart);
					*EndTime   = max(TransEnd,RotEnd);
				}
			else
				{	// No Translation Keys
					*StartTime = RotStart;
					*EndTime   = RotEnd;
				}
		}
	else
		{  // No Rotation Keys
			if (TCount>0)
				{
					*StartTime = grTKArray_ElementTime(P->Translation.KeyList, 0);
					if (TCount>1)
						{
							*EndTime = grTKArray_ElementTime(P->Translation.KeyList,TCount-1);
						}
					else
						{
							*EndTime = *StartTime;
						}
				}
			else
				{	// No Rotation or Translation keys
					return GR_FALSE;
				}
		}
	return GR_TRUE;	
}


#define GR_PATH_FILE_VERSION 0x1002		//15 bits!

/*
	file header:
	 15 bit version id, 
	 7 bit Rotation InterpolationType,
	 7 bit Translation InterpolationType, 
	 1 bit for translation keys exist, 
	 1 bit for rotation keys exist
	 1 bit for allow cuts
*/
#define GR_PATH_MAX_INT_TYPE_COUNT      (127)		// 7 bits 
#define GR_PATH_TRANS_SHIFT_INTO_HEADER (10)		// 7 bits shifted into bits 10..
#define GR_PATH_ROT_SHIFT_INTO_HEADER   (3)			// 7 bits shifted into bits 3..

GRAPI grBoolean GRCC grPath_WriteToFile(const grPath *P, grVFile *F)
{
	uint32 Header;
	int C,R,T,Looped;

	assert( F != NULL );
	assert( P != NULL );
	assert( GR_PATH_FILE_VERSION < 0xFFFF );

	C=R=T=0;

	if (P->Rotation.KeyList != NULL)
		{
			if (grTKArray_NumElements(P->Rotation.KeyList)>0)
				{
					R = GR_TRUE;
				}
		}
				
	if (P->Translation.KeyList != NULL)
		{
			if (grTKArray_NumElements(P->Translation.KeyList)>0)
				{
					T = GR_TRUE;
				}
		}

	if (P->AllowCuts)
		C=1;

	if (P->Looped)
		Looped = 1;
	else
		Looped = 0;
	assert( P->Translation.InterpolationType <= GR_PATH_MAX_INT_TYPE_COUNT);	
	assert( P->Rotation.InterpolationType <= GR_PATH_MAX_INT_TYPE_COUNT);		

	Header = 
		(GR_PATH_FILE_VERSION << 17) |
		(C)     | 
		(T<<1)  | 
		(R<<2) 	| 
		(P->Translation.InterpolationType << GR_PATH_TRANS_SHIFT_INTO_HEADER) | 
		(P->Rotation.InterpolationType    << GR_PATH_ROT_SHIFT_INTO_HEADER  );

	if	(grVFile_Write(F, &Header,sizeof(uint32)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE ,"grPath_WriteToFile: Failure to write Path File Header.");
			return GR_FALSE;
		}

	if (T==1)
		{
			if (grVKFrame_WriteToFile( F, P->Translation.KeyList, 
										grPath_PathToVKInterpolation(P->Translation.InterpolationType),
										Looped)==GR_FALSE)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE ,"grPath_WriteToFile.");
					return GR_FALSE;
				}
		}
	if (R==1)
		{
			if (grQKFrame_WriteToFile( F, P->Rotation.KeyList, 
										grPath_PathToQKInterpolation(P->Rotation.InterpolationType),
										Looped)==GR_FALSE)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE ,"grPath_WriteToFile.");
					return GR_FALSE;
				}
		}
	
	return GR_TRUE;
}


GRAPI grPath* GRCC grPath_CreateFromFile(grVFile* F)
{
	grPath *P;
	int Looping;//int Interp,Looping;
	grVKFrame_InterpolationType Interp;
	
	uint32 Header;

	assert( F != NULL );
	
	if(grVFile_Read(F, &Header, sizeof(Header)) == GR_FALSE)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grPath_CreateFromFile.");
		return NULL;
	}

	if ((Header>>17) != GR_PATH_FILE_VERSION)
		{
			grErrorLog_Add( GR_ERR_FILEIO_VERSION, "grPath_CreateFromFile: Bad path file version.");
			return NULL;
		}

	P = (grPath *)grRam_AllocateClear(sizeof(grPath));
	if (P == NULL)
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "grPath_CreateFromFile.");
			return NULL;
		}
	P->Translation.KeyList = NULL;
	P->Rotation.KeyList = NULL;
	
	P->Translation.InterpolationType = (grPath_InterpolationType) ((int)(Header >> GR_PATH_TRANS_SHIFT_INTO_HEADER) & GR_PATH_MAX_INT_TYPE_COUNT);
	P->Rotation.InterpolationType    = (grPath_InterpolationType) ((int)(Header >> GR_PATH_ROT_SHIFT_INTO_HEADER) & GR_PATH_MAX_INT_TYPE_COUNT);
	// this will be replaced by the path reader (if the path has keys)

	P->Translation.LastKey1Time = 0.0f;
	P->Translation.LastKey2Time = -1.0f;

	P->Rotation.LastKey1Time = 0.0f;
	P->Rotation.LastKey2Time = -1.0f;
	P-> Dirty    = 0;
	P-> Looped   = 0;
	P-> RefCount = 0;

	if ((Header >> 1) & 0x1)
		{
			P->Translation.KeyList = grVKFrame_CreateFromFile(F,&Interp,&Looping,GR_PATH_MAXIMUM_CUT_TIME);
			if (P->Translation.KeyList == NULL)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateFromFile.");
					grRam_Free(P);
					return NULL;
				}
			P->Translation.InterpolationType = grPath_VKToPathInterpolation(Interp);
			if( Looping != 0 )
				P->Looped = FLAG_LOOPED;
		}

	if ((Header >> 2) & 0x1)
		{
			P->Rotation.KeyList = grQKFrame_CreateFromFile(F,(grQKFrame_InterpolationType *)&Interp,&Looping,GR_PATH_MAXIMUM_CUT_TIME);
			if (P->Rotation.KeyList == NULL)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grPath_CreateFromFile.");
					if (P->Translation.KeyList != NULL)
						{
							grTKArray_Destroy(&P->Translation.KeyList);
						}
					grRam_Free(P);
					return NULL;
				}
			P->Rotation.InterpolationType = grPath_QKToPathInterpolation((grQKFrame_InterpolationType)Interp);
			if( Looping != 0 )
				P->Looped = FLAG_LOOPED;

		}
	P->AllowCuts = (Header & 0x1);
	P->Dirty = FLAG_DIRTY;
	return P;
}
