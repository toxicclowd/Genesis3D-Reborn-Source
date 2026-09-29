/****************************************************************************************/
/*  MOTION.C	                                                                        */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Motion implementation.				                                    */
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

/*
	This object is a list of (named) grPath objects, 
	and an associated event list 

*/
 
#include <assert.h>
#include <string.h>		// strcmp, strnicmp

#include "BaseType.h"
#include "Ram.h"
#include "Errorlog.h"
#include "Motion.h"
#include "TKEvents.h"
#include "StrBlock.h"

#pragma warning(disable : 4201)		// we're using nameless structures

#define grPath_TimeType grFloat

#define MIN(aa,bb)  (( (aa)>(bb) ) ? (bb) : (aa) )
#define MAX(aa,bb)  (( (aa)>(bb) ) ? (aa) : (bb) )

typedef enum { MOTION_NODE_UNDECIDED, MOTION_NODE_BRANCH, MOTION_NODE_LEAF } grMotion_NodeType;

#define MOTION_BLEND_PART_OF_TRANSFORM(TForm)  ((TForm).Translation.X)						
#define MOTION_BLEND_PART_OF_VECTOR(Vec)  ((Vec).X)						


typedef struct grMotion_Leaf
{
	int			PathCount;		
	int32		NameChecksum;	// checksum based on names and list order
	grTKEvents *Events;
	grStrBlock *NameArray;
	grPath	  **PathArray;
} grMotion_Leaf;


typedef struct grMotion_Mixer
{
	grFloat   TimeScale;		// multipler for time
	grFloat   TimeOffset;		// already scaled.
	grPath   *Blend;			// path used to interpolate blending amounts. 
	grXForm3d Transform;		// base transform for this motion (if TransformUsed==GR_TRUE)
	grBoolean TransformUsed;	// GR_FALSE if there is no base transform.
	grMotion *Motion;			
} grMotion_Mixer;

typedef struct grMotion_Branch
{
	int				MixerCount;
	int				CurrentEventIterator;
	grMotion_Mixer *MixerArray;
} grMotion_Branch;


typedef struct grMotion
{
	char			 *Name;
	int				  CloneCount;
	grBoolean		  MaintainNames;		
	grMotion_NodeType NodeType;
	union 
		{
			grMotion_Leaf   Leaf;
			grMotion_Branch Branch;
		};
	grMotion *SanityCheck;
} grMotion;


GRAPI grBoolean GRCC grMotion_IsValid(const grMotion *M)
{
	if (M == NULL)
		return GR_FALSE;
	if (M->SanityCheck!=M)
		return GR_FALSE;
	return GR_TRUE;
}

GRAPI grBoolean GRCC grMotion_SetName(grMotion *M, const char *Name)
{
	char *NewName;

	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	NewName = (char *)grRam_Allocate( strlen(Name)+1 );
	if (NewName == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_SetName.");
			return GR_FALSE;
		}
	if (M->Name!=NULL)
		{
			grRam_Free(M->Name);
		}
	M->Name = NewName;
	strcpy(M->Name, Name);
	return GR_TRUE;
}

GRAPI const char * GRCC grMotion_GetName(const grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	return M->Name;
}
		
static grBoolean GRCF grMotion_InitNodeAsLeaf(grMotion *M,grBoolean SetupStringBlock)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( M->NodeType == MOTION_NODE_UNDECIDED );

	M->NodeType = MOTION_NODE_LEAF;
	
	M->Leaf.PathCount     = 0;
	M->Leaf.Events        = NULL;
	M->Leaf.PathArray     = NULL;
	M->Leaf.NameChecksum  = 0;
	if ((M->MaintainNames != GR_FALSE) && (SetupStringBlock!=GR_FALSE))
		{
			M->Leaf.NameArray = grStrBlock_Create();
			if (M->Leaf.NameArray == NULL)	
				{
					grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_InitNodeAsLeaf");
					return GR_FALSE;
				}
		}
	else
		{
			M->Leaf.NameArray  = NULL;
		}
	return GR_TRUE;
}

static grBoolean GRCF grMotion_InitNodeAsBranch(grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( M->NodeType == MOTION_NODE_UNDECIDED );

	M->NodeType = MOTION_NODE_BRANCH;
	
	M->Branch.MixerCount           = 0;
	M->Branch.CurrentEventIterator = 0;
	M->Branch.MixerArray           = NULL;
	return GR_TRUE;
}


GRAPI grMotion * GRCC grMotion_Create(grBoolean WithNames)
{
	grMotion *M;
	assert( (WithNames==GR_TRUE) || (WithNames==GR_FALSE) );

	M = GR_RAM_ALLOCATE_STRUCT_CLEAR(grMotion);

	if ( M == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_Create.");
			return NULL;
		}

	M->Name          = NULL;
	M->CloneCount	 = 0;
	M->MaintainNames = WithNames;
	M->NodeType      = MOTION_NODE_UNDECIDED;
	M->SanityCheck   = M;
	return M;
}


GRAPI grBoolean GRCC grMotion_RemoveNames(grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->CloneCount > 0)
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER,"grMotion_RemoveNames: Can't remove names from a cloned motion.");
			return GR_FALSE;
		}

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_BRANCH):
				grErrorLog_Add(GR_ERR_BAD_PARAMETER,"grMotion_RemoveNames: Can't remove names from a compound motion.");
				return GR_FALSE;
				break;
			case (MOTION_NODE_LEAF):
				assert( M->Leaf.PathCount >= 0 );
				
				if ( M->Leaf.NameArray != NULL )
					{	
						grStrBlock_Destroy(&(M->Leaf.NameArray));
					}
				M->Leaf.NameArray = NULL;
				break;
			default:
				assert(0);
		}

	M->MaintainNames = GR_FALSE;
	return GR_TRUE;
}



GRAPI void GRCC grMotion_Destroy(grMotion **PM)
{
	int i;
	grMotion *M;
	
	assert(PM   != NULL );
	assert(*PM  != NULL );
	M = *PM;
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->CloneCount > 0 )
		{
			M->CloneCount--;
			return;
		}

	if (M->Name != NULL)
		{
			grRam_Free(M->Name);
			M->Name = NULL;
		}

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_BRANCH):
				for (i=0; i<M->Branch.MixerCount; i++)
					{
						assert( M->Branch.MixerArray[i].Motion != NULL );
						grMotion_Destroy( &(M->Branch.MixerArray[i].Motion));
						M->Branch.MixerArray[i].Motion = NULL;

						if (M->Branch.MixerArray[i].Blend != NULL )
							{
								grPath_Destroy( &(M->Branch.MixerArray[i].Blend));
								M->Branch.MixerArray[i].Blend = NULL;
							}
						
					}
				if (M->Branch.MixerArray != NULL)
					{
						grRam_Free(M->Branch.MixerArray);
						M->Branch.MixerArray = NULL;
					}
				M->Branch.MixerCount = 0;
				M->Branch.CurrentEventIterator = 0;
				break;
			case (MOTION_NODE_LEAF):
				if (M->MaintainNames == GR_TRUE)
					{	
						grBoolean Test=	grMotion_RemoveNames(M);
						assert( Test != GR_FALSE );
						Test;
					}
				for (i=0; i< M->Leaf.PathCount; i++)
					{
						assert( M->Leaf.PathArray[i] );
						grPath_Destroy( &( M->Leaf.PathArray[i] ) );
						M->Leaf.PathArray[i] = NULL;
					}
				if (M->Leaf.PathArray!=NULL)
					{
						grRam_Free(M->Leaf.PathArray);
						M->Leaf.PathArray = NULL;
					}
				M->Leaf.PathCount = 0;
				if ( M->Leaf.Events != NULL )
					{
						grTKEvents_Destroy( &(M->Leaf.Events) );
					}
				break;
			default:
				assert(0);
		}
	M->NodeType = MOTION_NODE_UNDECIDED;
	grRam_Free( *PM );
	*PM = NULL;
}

GRAPI grBoolean GRCC grMotion_AddPath(grMotion *M,
	grPath *P,const char *Name,int *PathIndex)
{
	int PathCount;
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				if (grMotion_InitNodeAsLeaf(M,GR_TRUE)==GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddPath.");
						return GR_FALSE;
					}
				break;
			case (MOTION_NODE_BRANCH):
				grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_AddPath.");
				return GR_FALSE;
			case (MOTION_NODE_LEAF):
				break;
			default:
				assert(0);
		}

	assert( M->Leaf.PathCount >= 0 );

	if (Name!=NULL)
		{
			if (grMotion_GetPathNamed( M, Name) != NULL )
				{
					grErrorLog_AddString(GR_ERR_BAD_PARAMETER,"grMotion_AddPath: Path already exists with same name.", Name);
					return GR_FALSE;
				}
		}

	PathCount = M->Leaf.PathCount;

	{
		grPath **NewPathArray;

		NewPathArray = (grPath**)grRam_Realloc(M->Leaf.PathArray, (1+PathCount) * sizeof(grPath*) );

		if ( NewPathArray == NULL )
			{	
				grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_AddPath.");
				return GR_FALSE;
			}
		M->Leaf.PathArray = NewPathArray;
	}

	M->Leaf.PathArray[PathCount] = P;

	if ( M->MaintainNames == GR_TRUE )
		{
			
			assert (M->Leaf.NameArray != NULL);
			if (grStrBlock_Append(&(M->Leaf.NameArray),Name)==GR_FALSE)
				{
					grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddPath.");
					assert(M->Leaf.PathArray[PathCount]);
					grPath_Destroy(&(M->Leaf.PathArray[PathCount]));
					return GR_FALSE;
				}
			M->Leaf.NameChecksum = grStrBlock_GetChecksum(M->Leaf.NameArray);
		}						
															
	M->Leaf.PathCount = PathCount+1;
	*PathIndex = PathCount;
	grPath_CreateRef(P);
	return GR_TRUE;
}


// returns 0 if there is no name information... or if children don't all share the same checksum.
GRAPI int32 GRCC grMotion_GetNameChecksum(const grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				return 0;
			case (MOTION_NODE_BRANCH):
				{
					int i;
					int32 Checksum,FirstChecksum;
					if (M->Branch.MixerCount<1)
						return 0;
					assert( M->Branch.MixerArray[0].Motion );
					FirstChecksum = grMotion_GetNameChecksum( M->Branch.MixerArray[0].Motion );
					
					for (i=1; i<M->Branch.MixerCount; i++)
						{
							assert( M->Branch.MixerArray[i].Motion );
							Checksum = grMotion_GetNameChecksum( M->Branch.MixerArray[i].Motion );
							if (Checksum != FirstChecksum)
								return 0;
						}
					return FirstChecksum;
				}
			case (MOTION_NODE_LEAF):
				return M->Leaf.NameChecksum;
			default:
				assert(0);
		}
	return 0;
}	

GRAPI grBoolean GRCC grMotion_HasNames(const grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( (M->MaintainNames == GR_TRUE) || (M->MaintainNames == GR_FALSE) );
	// if M has names, all children of M have names. 
	return M->MaintainNames;
}

GRAPI grPath * GRCC grMotion_GetPathNamed(const grMotion *M,const char *Name)
{
	int i;

	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	
	if (M->NodeType != MOTION_NODE_LEAF)
		{	// not an error condition.
			return NULL;
		}
			
	assert( M->Leaf.PathCount >=0 );
	
	if (Name != NULL)	
		{
			if ( M->MaintainNames == GR_TRUE )
				{
					for (i=0; i<M->Leaf.PathCount; i++)
						{
							if ( strcmp(Name,grStrBlock_GetString(M->Leaf.NameArray,i))==0 )
								{
									return M->Leaf.PathArray[i];
								}
						}
				}
		}
	return NULL;
}
			

#define LINEAR_BLEND(a,b,t)  ( (t)*((b)-(a)) + (a) )	
			// linear blend of a and b  0<t<1 where  t=0 ->a and t=1 ->b



GRAPI void GRCC grMotion_Sample(const grMotion *M, int PathIndex, grPath_TimeType Time, grXForm3d *Transform)
{
	grQuaternion Rotation;
	grVec3d		 Translation;
	assert( M           != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( Transform   != NULL );

	grMotion_SampleChannels(M,PathIndex,Time,&Rotation,&Translation);
	grQuaternion_ToMatrix(&Rotation,Transform);
	Transform->Translation = Translation;
}


GRAPI void GRCC grMotion_SampleChannels(const grMotion *M, int PathIndex, grPath_TimeType Time, grQuaternion *Rotation, grVec3d *Translation)
{
	assert( M           != NULL);
	assert( Rotation    != NULL );
	assert( Translation != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				assert(0);
				break;
			case (MOTION_NODE_BRANCH):
				{
					grQuaternion R;
					grVec3d      T;
					grMotion_Mixer *Mixer;
					int i;

					if ( M->Branch.MixerCount == 0 )
						{
							grVec3d_Clear(Translation);
							grQuaternion_SetNoRotation(Rotation);
							return;
						}

					assert( M->Branch.MixerCount > 0);
					Mixer = &(M->Branch.MixerArray[0]);
					
					assert(Mixer->Motion != NULL );
					grMotion_SampleChannels(Mixer->Motion,PathIndex,
											(Time - Mixer->TimeOffset) * Mixer->TimeScale,
											Rotation,Translation);
				
					for (i=1; i<M->Branch.MixerCount; i++)
						{
							grFloat BlendAmount;
							grFloat MixTime;

							Mixer = &(M->Branch.MixerArray[i]);

							assert( Mixer->Motion != NULL );
							assert( Mixer->Blend  != NULL );

							MixTime = (Time - Mixer->TimeOffset) * Mixer->TimeScale;

							grMotion_SampleChannels(Mixer->Motion,PathIndex,MixTime,&R,&T);
							{
								grVec3d BlendVector;
								grQuaternion Dummy;
								grPath_SampleChannels(Mixer->Blend,MixTime,&Dummy,&BlendVector);
								BlendAmount = MOTION_BLEND_PART_OF_VECTOR(BlendVector);
							}
							grQuaternion_Slerp(Rotation,&R,BlendAmount,Rotation);
							Translation->X = LINEAR_BLEND(Translation->X,T.X,BlendAmount);
							Translation->Y = LINEAR_BLEND(Translation->Y,T.Y,BlendAmount);
							Translation->Z = LINEAR_BLEND(Translation->Z,T.Z,BlendAmount);
						}
				}
				break;
			case (MOTION_NODE_LEAF):
				{
					grPath *P;
					assert( ( PathIndex >=0 ) && ( PathIndex < M->Leaf.PathCount ) );
					P= M->Leaf.PathArray[PathIndex];
					assert( P != NULL );
					grPath_SampleChannels(P,Time,Rotation,Translation);
				}
				break;
			default:
				assert(0);
		}
}		

GRAPI grBoolean GRCC grMotion_SampleNamed(const grMotion *M, const char *PathName, grPath_TimeType Time, grXForm3d *Transform)
{
	grQuaternion Rotation;
	grVec3d		 Translation;
	assert( M           != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( Transform   != NULL );

	if (grMotion_SampleChannelsNamed(M,PathName,Time,&Rotation,&Translation)==GR_FALSE)
		{
			return GR_FALSE;
		}

	grQuaternion_ToMatrix(&Rotation,Transform);
	Transform->Translation = Translation;
	return GR_TRUE;
}



GRAPI grBoolean GRCC grMotion_SampleChannelsNamed(const grMotion *M, const char *PathName, grPath_TimeType Time, grQuaternion *Rotation, grVec3d *Translation)
{
	grBoolean AnyChannels=GR_FALSE;
	assert( M           != NULL);
	assert( Rotation    != NULL );
	assert( Translation != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				return GR_FALSE;
				break;
			case (MOTION_NODE_BRANCH):
				{
					int i;
					grQuaternion R;
					grVec3d T;
					grMotion_Mixer *Mixer;

					if ( M->Branch.MixerCount == 0 )
						{
							grVec3d_Clear(Translation);
							grQuaternion_SetNoRotation(Rotation);
							return GR_TRUE;
						}

					assert( M->Branch.MixerCount > 0 );

					for (i=0; i<M->Branch.MixerCount; i++)
						{
							grFloat BlendAmount;
							grFloat MixTime;

							Mixer = &(M->Branch.MixerArray[i]);

							assert( Mixer->Motion != NULL );
							assert( Mixer->Blend  != NULL );

							MixTime = (Time - Mixer->TimeOffset) * Mixer->TimeScale;

							// hmm. is BlendAmount still good if there is no path?
							if ( grMotion_SampleChannelsNamed(Mixer->Motion,PathName,MixTime,&R,&T)
								 != GR_FALSE )
								{
									if (AnyChannels != GR_FALSE)
										{
											{
												grVec3d BlendVector;
												grQuaternion Dummy;
												grPath_SampleChannels(Mixer->Blend,MixTime,&Dummy,&BlendVector);
												BlendAmount = MOTION_BLEND_PART_OF_VECTOR(BlendVector);
											}
											grQuaternion_Slerp(Rotation,&R,BlendAmount,Rotation);
											Translation->X = LINEAR_BLEND(Translation->X,T.X,BlendAmount);
											Translation->Y = LINEAR_BLEND(Translation->Y,T.Y,BlendAmount);
											Translation->Z = LINEAR_BLEND(Translation->Z,T.Z,BlendAmount);
										}
									else
										{
											*Rotation = R;
											*Translation = T;
											AnyChannels = GR_TRUE;
										}
								}
						}
				}
				break;
			case (MOTION_NODE_LEAF):
				{
					grPath *P;
					P = grMotion_GetPathNamed(M, PathName);
					if (P == NULL)
						{
							return GR_FALSE;
						}
					grPath_SampleChannels(P,Time,Rotation,Translation);
					AnyChannels = GR_TRUE;
				}
				break;
			default:
				assert(0);
		}
	return AnyChannels;
}		


GRAPI grPath * GRCC grMotion_GetPath(const grMotion *M,int Index)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
 	
	if (M->NodeType != MOTION_NODE_LEAF)
		{	// not an error condition.
			return NULL;
		}
			
	assert( M->Leaf.PathCount >=0 );
	assert( Index <= M->Leaf.PathCount );
	assert( Index >= 0 );

	return M->Leaf.PathArray[Index];
}

GRAPI const char * GRCC grMotion_GetNameOfPath(const grMotion *M, int Index)
{
	grPath *P;
	assert( M != NULL );

	if (M->NodeType!=MOTION_NODE_LEAF)
		{
			return NULL;
		}
	if (grMotion_HasNames(M)==GR_FALSE)
		{
			return NULL;
		}

	P = grMotion_GetPath(M,Index);
	if (P==NULL)
		{
			return NULL;
		}
	assert( M->Leaf.NameArray!=NULL );

	return grStrBlock_GetString(M->Leaf.NameArray,Index);

}
			
	

GRAPI int GRCC grMotion_GetPathCount(const grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->NodeType != MOTION_NODE_LEAF)
		{	// not an error condition.
			return 0;
		}
	assert( M->Leaf.PathCount >=0 );
	return M->Leaf.PathCount;
}


GRAPI grBoolean GRCC grMotion_GetTimeExtents(const grMotion *M,grPath_TimeType *StartTime,grPath_TimeType *EndTime)
{
	int i,found;
	grPath_TimeType Start,End;
	assert( M != NULL );
	assert( StartTime != NULL );
	assert( EndTime != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	found = 0;

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_BRANCH):
				for (i=0; i<M->Branch.MixerCount; i++)
					{
						if (grMotion_GetTimeExtents(M->Branch.MixerArray[i].Motion,&Start,&End)!=GR_FALSE)
							{
								found++;

								// Assertions in AddSubMotion and SetTimeScale prevent TimeScale from being 0.
								End = M->Branch.MixerArray[i].TimeOffset + ((End - Start) / M->Branch.MixerArray[i].TimeScale);
								Start += M->Branch.MixerArray[i].TimeOffset;

								// If time scale is negative, then End will be < Start, which violates
								// the entire idea of extents.  So we'll swap them.
								if (End < Start)
								{
									grFloat Temp = Start;
									Start = End;
									End = Temp;
								}
								if (found==1)
									{
										*StartTime = Start;
										*EndTime   = End;
									}
								else
									{	//found>1
										*StartTime = MIN(*StartTime,Start);
										*EndTime   = MAX(*EndTime,End);
									}
							}
					}										
				break;			
			case (MOTION_NODE_LEAF):
				found = 0;
				for (i=0; i<M->Leaf.PathCount; i++)
					{
						if (grPath_GetTimeExtents(M->Leaf.PathArray[i],&Start,&End)!=GR_FALSE)
							{
								found++;
								if (found==1)
									{
										*StartTime = Start;
										*EndTime   = End;
									}
								else
									{	//found>1
										*StartTime = MIN(*StartTime,Start);
										*EndTime   = MAX(*EndTime,End);
									}
							}
					}
				break;
			default:
				assert(0);
		}
	if (found>0)
		{
			return GR_TRUE;
		}
	return GR_FALSE;
}			


GRAPI int GRCC grMotion_GetSubMotionCount(const grMotion *M)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			return M->Branch.MixerCount;
		}
	return 0;
}


#pragma message ("do we want to copy these before returning them?")
GRAPI grMotion * GRCC grMotion_GetSubMotion(const grMotion *M,int SubMotionIndex)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	if (M->NodeType != MOTION_NODE_BRANCH )
		{
			return NULL;
		}
	assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
	assert( M->Branch.MixerArray != NULL );

	return M->Branch.MixerArray[SubMotionIndex].Motion;
}

GRAPI grMotion * GRCC grMotion_GetSubMotionNamed(const grMotion *M,const char *Name)
{
	int i;
	assert( M != NULL);	
	assert( Name != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->NodeType != MOTION_NODE_BRANCH)
		{
			return NULL;
		}
	assert( M->Branch.MixerArray != NULL );
	for (i=0; i<M->Branch.MixerCount; i++)
		{
			grMotion *MI = M->Branch.MixerArray[i].Motion;
			assert( MI != NULL );
			if (MI->Name!=NULL)
				{
					if (strcmp(MI->Name,Name)==0)
						{
							return MI;
						}
				}
		}
	return NULL;
}

static grBoolean GRCF grMotion_SearchForSubMotion(const grMotion *Parent, const grMotion*Child)
{
	int i;
	assert( Parent != NULL );
	assert( Child  != NULL );
	assert( grMotion_IsValid(Parent) != GR_FALSE );
	assert( grMotion_IsValid(Child) != GR_FALSE );

	if (Parent == Child)
		return GR_TRUE;

	if (Parent->NodeType != MOTION_NODE_BRANCH)
		return GR_FALSE;

	assert( Parent->Branch.MixerArray != NULL );

	for (i=0; i<Parent->Branch.MixerCount; i++)
		{
			assert( Parent->Branch.MixerArray[i].Motion != NULL );
			if (grMotion_SearchForSubMotion(Parent->Branch.MixerArray[i].Motion,Child)==GR_TRUE)
				return GR_TRUE;
		}
	return GR_FALSE;
}

GRAPI grBoolean GRCC grMotion_AddSubMotion(grMotion *ParentMotion, 
								grFloat TimeScale, 
								grFloat TimeOffset,
								grMotion *SubMotion, 
								grFloat StartTime, grFloat StartMagnitude,
								grFloat EndTime,   grFloat EndMagnitude,
								const grXForm3d *Transform,
								int *Index)

{

	int Count;
	grMotion_Mixer *NewMixerArray;
	assert( ParentMotion != NULL );
	assert( TimeScale	 != 0.0f );
	assert( SubMotion    != NULL );
	assert( Index        != NULL );
	//assert( Transform    != NULL );
	assert( ( StartMagnitude >= 0.0f) && ( StartMagnitude <=1.0f ));
	assert( ( EndMagnitude   >= 0.0f) && ( EndMagnitude   <=1.0f ));
	assert( grMotion_IsValid(ParentMotion) != GR_FALSE );
	assert( grMotion_IsValid(SubMotion) != GR_FALSE );

	switch (ParentMotion->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				if (grMotion_InitNodeAsBranch(ParentMotion)==GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddSubMotion.");
						return GR_FALSE;
					}
				break;
			case (MOTION_NODE_LEAF):
				{
					grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grMotion_AddSubMotion: Can't add a submotion to a Leaf motion.");
					return GR_FALSE;
				}
			case (MOTION_NODE_BRANCH):
				break;
			default:
				assert(0);
		}

	if (ParentMotion->MaintainNames != SubMotion->MaintainNames)
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grMotion_AddSubMotion: Can't add a submotion with different MaintainNames convention.");  
			return GR_FALSE;
		}
		
	if (grMotion_SearchForSubMotion(SubMotion,ParentMotion)!=GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grMotion_AddSubMotion: Can't add - would create a circular loop of submotions.");
			return GR_FALSE;
		}
			
	Count = ParentMotion->Branch.MixerCount;
	NewMixerArray = (grMotion_Mixer *)grRam_Realloc(ParentMotion->Branch.MixerArray, (1+Count) * sizeof(grMotion_Mixer) );
	if ( NewMixerArray == NULL )
		{	
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_AddSubMotion.");
			return GR_FALSE;
		}
		
	ParentMotion->Branch.MixerArray = NewMixerArray;
	{
		grMotion_Mixer *Mixer;
		grXForm3d BlendKeyTransform;
		Mixer = &(ParentMotion->Branch.MixerArray[Count]);
	
		Mixer->Motion     = SubMotion;
		Mixer->TimeScale  = TimeScale;
		Mixer->TimeOffset = TimeOffset;
		
		Mixer->Blend = grPath_Create(GR_PATH_INTERPOLATE_HERMITE_ZERO_DERIV,GR_PATH_INTERPOLATE_SLERP,GR_FALSE);
		if (Mixer->Blend==NULL)
			{	
				grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddSubMotion.");
				return GR_FALSE;
			}
		MOTION_BLEND_PART_OF_TRANSFORM(BlendKeyTransform) = StartMagnitude;
		if (grPath_InsertKeyframe(Mixer->Blend,
						GR_PATH_TRANSLATION_CHANNEL,StartTime,&BlendKeyTransform)==GR_FALSE)
			{
				grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddSubMotion.");
				grPath_Destroy(&(Mixer->Blend));
				return GR_FALSE;
			}

		MOTION_BLEND_PART_OF_TRANSFORM(BlendKeyTransform) = EndMagnitude;
		if (grPath_InsertKeyframe(Mixer->Blend,
						GR_PATH_TRANSLATION_CHANNEL,EndTime,&BlendKeyTransform)==GR_FALSE)
			{
				grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_AddSubMotion.");
				grPath_Destroy(&(Mixer->Blend));
				return GR_FALSE;
			}
		if (Transform == NULL)
			{
				Mixer->TransformUsed = GR_FALSE;
			}
		else
			{
				Mixer->TransformUsed = GR_TRUE;
				Mixer->Transform = *Transform;
			}
	}
	
	*Index = Count;
	SubMotion->CloneCount++;
	ParentMotion->Branch.MixerCount++;

	return GR_TRUE;
}

GRAPI grMotion * GRCC grMotion_RemoveSubMotion(grMotion *ParentMotion, int SubMotionIndex)
{
	int Count;
	grMotion *M;
	assert( ParentMotion != NULL );
	assert( grMotion_IsValid(ParentMotion) != GR_FALSE );

	if (ParentMotion->NodeType != MOTION_NODE_BRANCH)
		{
			return NULL;
		}
	
	Count = ParentMotion->Branch.MixerCount;
	assert( (SubMotionIndex>=0) && (SubMotionIndex<Count));
	
	M = ParentMotion->Branch.MixerArray[SubMotionIndex].Motion;
	assert( ParentMotion->Branch.MixerArray[SubMotionIndex].Blend != NULL );
	grPath_Destroy( &(ParentMotion->Branch.MixerArray[SubMotionIndex].Blend) );
	
	if (Count>1)
		{
			memcpy( &(ParentMotion->Branch.MixerArray[SubMotionIndex]),
					&(ParentMotion->Branch.MixerArray[SubMotionIndex+1]),
					sizeof(grMotion_Mixer) * (Count-(SubMotionIndex+1)));
		}
	ParentMotion->Branch.MixerCount--;

	{
		grMotion_Mixer *NewMixerArray;
		if (ParentMotion->Branch.MixerCount == 0)
			{
				grRam_Free(ParentMotion->Branch.MixerArray);
				ParentMotion->Branch.MixerArray = NULL;
			}
		else
			{
				NewMixerArray = (grMotion_Mixer *)grRam_Realloc(ParentMotion->Branch.MixerArray, 
									(ParentMotion->Branch.MixerCount) * sizeof(grMotion_Mixer) );
				if ( NewMixerArray != NULL )
					{	
						ParentMotion->Branch.MixerArray = NewMixerArray;
					}
			}
	}
	grMotion_Destroy( &M );
	return M;
}
	


GRAPI grFloat   GRCC grMotion_GetTimeOffset( const grMotion *M,int SubMotionIndex )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	// wrong node type is neither error nor invalid.  return value is just 0

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			return M->Branch.MixerArray[SubMotionIndex].TimeOffset;
		}
	return 0.0f;
}

GRAPI grBoolean  GRCC grMotion_SetTimeOffset( grMotion *M,int SubMotionIndex,grFloat TimeOffset )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			M->Branch.MixerArray[SubMotionIndex].TimeOffset = TimeOffset;
			return GR_TRUE;
		}
	return GR_FALSE;
}

GRAPI grFloat   GRCC grMotion_GetTimeScale( const grMotion *M,int SubMotionIndex )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	// wrong node type is neither error nor invalid.  return value is just 1

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			return M->Branch.MixerArray[SubMotionIndex].TimeScale;
		}
	return 1.0f;
}

GRAPI grBoolean  GRCC grMotion_SetTimeScale( grMotion *M,int SubMotionIndex,grFloat TimeScale )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( TimeScale != 0.0f);

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			M->Branch.MixerArray[SubMotionIndex].TimeScale = TimeScale;
			return GR_TRUE;
		}
	return GR_FALSE;
}

GRAPI grFloat    GRCC grMotion_GetBlendAmount( const grMotion *M, int SubMotionIndex, grFloat Time)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	// wrong node type is neither error nor invalid.  return value is just 0

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			grQuaternion Dummy;
			grVec3d BlendVector;
			grFloat BlendAmount;

			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			assert( M->Branch.MixerArray[SubMotionIndex].Blend != NULL );
			grPath_SampleChannels(M->Branch.MixerArray[SubMotionIndex].Blend,
								  ( Time - M->Branch.MixerArray[SubMotionIndex].TimeOffset )
								    * M->Branch.MixerArray[SubMotionIndex].TimeScale,
								   &Dummy,&BlendVector);
			BlendAmount = MOTION_BLEND_PART_OF_VECTOR(BlendVector);
			return BlendAmount;
		}
	return 0.0f;
}

GRAPI grPath    * GRCC grMotion_GetBlendPath( const grMotion *M,int SubMotionIndex )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	// wrong node type is neither error nor invalid.  return value is just NULL

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			return M->Branch.MixerArray[SubMotionIndex].Blend;
		}
	return NULL;
}

GRAPI grBoolean  GRCC grMotion_SetBlendPath( grMotion *M,int SubMotionIndex, grPath *Blend )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			grPath *P;
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			assert( Blend != NULL );
			P = M->Branch.MixerArray[SubMotionIndex].Blend;
			grPath_Destroy(&P);
			P = grPath_CreateCopy(Blend);
			if ( P == NULL )
				{
					grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_SetBlendPath.");
					return GR_FALSE;
				}
			M->Branch.MixerArray[SubMotionIndex].Blend = P;
			return GR_TRUE;
		}
	return GR_FALSE;
}


GRAPI const grXForm3d * GRCC grMotion_GetBaseTransform( const grMotion *M,int SubMotionIndex )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	// wrong node type is neither error nor invalid.  return value is just NULL

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			if (M->Branch.MixerArray[SubMotionIndex].TransformUsed != GR_FALSE)
				{
					return &(M->Branch.MixerArray[SubMotionIndex].Transform);
				}
			else
				{
					return NULL;
				}
		}
	return NULL;
}

GRAPI grBoolean  GRCC grMotion_SetBaseTransform( grMotion *M,int SubMotionIndex, grXForm3d *BaseTransform )
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->NodeType == MOTION_NODE_BRANCH)
		{
			assert( (SubMotionIndex>=0) && (SubMotionIndex<M->Branch.MixerCount));
			assert( BaseTransform != NULL );
			if (BaseTransform!=NULL)
				{
					M->Branch.MixerArray[SubMotionIndex].Transform     = *BaseTransform;
					M->Branch.MixerArray[SubMotionIndex].TransformUsed = GR_TRUE;
				}
			else
				{
					M->Branch.MixerArray[SubMotionIndex].TransformUsed = GR_FALSE;
				}
					
			return GR_TRUE;
		}
	return GR_FALSE;
}


#pragma warning( disable : 4701)	// don't want to set Translation until we are ready
GRAPI grBoolean GRCC grMotion_GetTransform( const grMotion *M, grFloat Time, grXForm3d *Transform)
{
	assert( M         != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				{
					return GR_FALSE;
				}
				break;
			case (MOTION_NODE_BRANCH):
				{
					grQuaternion R,Rotation;
					grVec3d      T,Translation;
					grMotion_Mixer *Mixer;
					grFloat MixTime;
					int i;
					int MixCount=0;

					if ( M->Branch.MixerCount == 0 )
						{
							return GR_FALSE;
						}
					assert( M->Branch.MixerCount > 0 );

					for (i=0; i<M->Branch.MixerCount; i++)
						{
							grFloat BlendAmount;
							grBoolean DoMix=GR_FALSE;

							Mixer = &(M->Branch.MixerArray[i]);

							assert( Mixer->Motion != NULL );
							assert( Mixer->Blend  != NULL );
							
							MixTime = (Time - Mixer->TimeOffset) * Mixer->TimeScale;
							if (grMotion_GetTransform(Mixer->Motion,MixTime,Transform)!=GR_FALSE)
								{
									DoMix=GR_TRUE;
									if (Mixer->TransformUsed!=GR_FALSE)
										{
											grXForm3d_Multiply(&(Mixer->Transform),Transform,Transform);
										}
								}
							else
								{
									if (Mixer->TransformUsed!=GR_FALSE)
										{
											DoMix = GR_TRUE;
											*Transform = Mixer->Transform;
										}
								}
							if (DoMix!=GR_FALSE)
								{
									if (MixCount==0)
										{
											grQuaternion_FromMatrix(Transform,&Rotation);
											Translation = Transform->Translation;
										}
									else
										{
											grQuaternion_FromMatrix(Transform,&R);
											T = Transform->Translation;
											{
												grVec3d BlendVector;
												grQuaternion Dummy;
												grPath_SampleChannels(Mixer->Blend,MixTime,&Dummy,&BlendVector);
												BlendAmount = MOTION_BLEND_PART_OF_VECTOR(BlendVector);
											}
											grQuaternion_Slerp(&Rotation,&R,BlendAmount,&Rotation);
											Translation.X = LINEAR_BLEND(Translation.X,T.X,BlendAmount);
											Translation.Y = LINEAR_BLEND(Translation.Y,T.Y,BlendAmount);
											Translation.Z = LINEAR_BLEND(Translation.Z,T.Z,BlendAmount);
										}
									
									MixCount++;
								}
						}
					if (MixCount>0)
						{
							grQuaternion_ToMatrix(&Rotation,Transform);
							Transform->Translation = Translation;
							return GR_TRUE;
						}
					return GR_FALSE;
				}
				break;
			case (MOTION_NODE_LEAF):
				{
					return GR_FALSE;
				}
				break;
			default:
				assert(0);
		}
	return GR_FALSE;
}
#pragma warning( default : 4701)	


//--------------------------------------------------------------------------------------------
//   Event Support

GRAPI grBoolean GRCC grMotion_GetEventExtents(const grMotion *M,grFloat *FirstEventTime,grFloat *LastEventTime)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( FirstEventTime != NULL );
	assert( LastEventTime != NULL );

	return grTKEvents_GetExtents(M->Leaf.Events,FirstEventTime,LastEventTime);
}	


	// Inserts the new event and corresponding string.
GRAPI grBoolean GRCC grMotion_InsertEvent(grMotion *M, grPath_TimeType tKey, const char* String)
{
	assert( M != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );
	assert( String != NULL );

	if (M->NodeType != MOTION_NODE_LEAF )
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grMotion_InsertEvent: Motion not a leaf - Can only insert events in a leaf.");
			return GR_FALSE;
		}

	if (M->Leaf.Events == NULL)
		{
			M->Leaf.Events = grTKEvents_Create();
			if ( M->Leaf.Events == NULL )
				{
					grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_InsertEvent.");
					return GR_FALSE;
				}
		}
	if (grTKEvents_Insert(M->Leaf.Events, tKey,String)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_InsertEvent.");
			return GR_FALSE;
		};
	return GR_TRUE;
}
	

			
	// Deletes the event
GRAPI grBoolean GRCC grMotion_DeleteEvent(grMotion *M, grPath_TimeType tKey)
{
	assert( M != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );

	if (M->NodeType != MOTION_NODE_LEAF )
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grMotion_DeleteEvent: Motion not a leaf - Can only delete events in a leaf.");
			return GR_FALSE;
		}
	if ( M->Leaf.Events == NULL )
		{
			grErrorLog_Add(GR_ERR_SEARCH_FAILURE, "grMotion_DeleteEvent: no events in motion.");
			return GR_FALSE;
		}
	if (grTKEvents_Delete(M->Leaf.Events,tKey)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_DeleteEvent.");
			return GR_FALSE;
		}
	return GR_TRUE;
}

GRAPI void GRCC grMotion_SetupEventIterator(
	grMotion *M,
	grPath_TimeType StartTime,				// Inclusive search start
	grPath_TimeType EndTime)				// Non-inclusive search stop
	// For searching or querying the array for events between two times
	// times are compaired [StartTime,EndTime), '[' is inclusive, ')' is 
	// non-inclusive.  This prepares the grMotion_GetNextEvent() function.
{
	int i;
	assert( M != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_LEAF):
				if ( M->Leaf.Events != NULL )
					{
						grTKEvents_SetupIterator(M->Leaf.Events,StartTime,EndTime);
					}	
				break;
			case (MOTION_NODE_BRANCH):
				for (i=0; i<M->Branch.MixerCount; i++)
					{
						grMotion_Mixer *Mixer;
				
						Mixer = &(M->Branch.MixerArray[i]);

						grMotion_SetupEventIterator(Mixer->Motion,
							(StartTime - Mixer->TimeOffset) * Mixer->TimeScale,
							(EndTime - Mixer->TimeOffset) * Mixer->TimeScale);
					}
				M->Branch.CurrentEventIterator =0;
				break;
			default:
				assert(0);
		}
}		


GRAPI grBoolean GRCC grMotion_GetNextEvent(
	grMotion *M,						// Event list to iterate
	grPath_TimeType *pTime,				// Return time, if found
	const char **ppEventString)		// Return data, if found
	// Iterates from StartTime to EndTime as setup in grMotion_SetupEventIterator()
	// and for each event between these times [StartTime,EndTime)
	// this function will return Time and EventString returned for that event
	// and the iterator will be positioned for the next search.  When there 
	// are no more events in the range, this function will return GR_FALSE (Time
	// will be 0 and ppEventString will be empty).
{
	assert( M != NULL);
	assert( grMotion_IsValid(M) != GR_FALSE );

	assert( pTime != NULL );
	assert( ppEventString != NULL );

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				{
					return GR_FALSE;
				}
				break;
			case (MOTION_NODE_LEAF):
				if ( M->Leaf.Events != NULL )
					{
						return grTKEvents_GetNextEvent(M->Leaf.Events,pTime,ppEventString);
					}	
				break;
			case (MOTION_NODE_BRANCH):
				while (M->Branch.CurrentEventIterator < M->Branch.MixerCount)
					{
						if (grMotion_GetNextEvent(
									M->Branch.MixerArray[M->Branch.CurrentEventIterator].Motion,
									pTime,ppEventString) !=GR_FALSE)
							return GR_TRUE;
						M->Branch.CurrentEventIterator++;
					}
				break;
			default:
				assert(0);
		}

	return GR_FALSE;
}
	
//------------------------------------------------------------------------------------------------------
//    Read/Write support

#define MOTION_BIN_FILE_TYPE 0x424E544D 	// 'MTNB'
#define MOTION_FILE_VERSION 0x00F0			// Restrict version to 16 bits

typedef struct
{
	int PathCount;
	int32 NameChecksum;
	uint32 Flags;
} grMotion_FileLeafHeader;

static grBoolean GRCF grMotion_ReadBranch(grMotion *M, grVFile *pFile)
{
	assert( M != NULL );
	assert( pFile != NULL );
	if (grMotion_InitNodeAsBranch(M)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grMotion_ReadBranch.");
			return GR_FALSE;
		}

	#pragma message("finish this")
	grErrorLog_Add( GR_ERR_INTERNAL_RESOURCE, "grMotion_ReadBranch: not implemented.");
	return GR_FALSE;
}

static grBoolean GRCF grMotion_ReadLeaf(grMotion *M, grVFile *pFile)
{
	int i;
	grMotion_FileLeafHeader Header;
	assert( M != NULL );
	assert( pFile != NULL );
	if (grMotion_InitNodeAsLeaf(M,GR_FALSE)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE,"grMotion_ReadLeaf.");
			return GR_FALSE;
		}

	if (grVFile_Read(pFile, &Header, sizeof(grMotion_FileLeafHeader)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ,"grMotion_ReadLeaf: failed to read leaf header."); 
			return GR_FALSE; 
		}
	M->Leaf.NameChecksum = Header.NameChecksum;
	
	if (Header.Flags & 0x1)
		{
			M->Leaf.Events = grTKEvents_CreateFromFile(pFile);
			if (M->Leaf.Events == NULL )
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE,"grMotion_ReadLeaf.");
					return GR_FALSE; 
				}
		}
	else
		{
			M->Leaf.Events = NULL;
		}

	if (Header.Flags & 0x2)
		{
			M->Leaf.NameArray = grStrBlock_CreateFromFile(pFile);
			if (M->Leaf.NameArray == NULL)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE,"grMotion_ReadLeaf.");
					return GR_FALSE; 
				}
		}
	else
		{
			M->Leaf.NameArray = NULL;
		}

	M->Leaf.PathCount = 0;
	M->Leaf.PathArray = (grPath **)grRam_Allocate( Header.PathCount * sizeof(grPath*) );

	if ( M->Leaf.PathArray == NULL )
		{	
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE,"grMotion_ReadLeaf.");
			return GR_TRUE;
		}

	for (i=0; i<Header.PathCount; i++)
		{
			M->Leaf.PathArray[i] = grPath_CreateFromFile(pFile);
			if (M->Leaf.PathArray[i] == NULL )
				{
					grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE,"grMotion_ReadLeaf: failed to read path.",grErrorLog_IntToString(i));
					return GR_FALSE; 
				}
			M->Leaf.PathCount++;
		}
	return GR_TRUE;
}

GRAPI grMotion* GRCC grMotion_CreateFromFile(grVFile* pFile)
{
	uint32 u;	
	grBoolean MaintainNames;
	int NodeType;
	int NameLength;
	grMotion *M;

	assert( pFile != NULL );

	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grMotion_CreateFromFile: failed to read motion header.");
		return NULL;
	}

	if(u != MOTION_BIN_FILE_TYPE)
	{
		grErrorLog_Add( GR_ERR_FILEIO_FORMAT , "grMotion_CreateFromFile: wrong file type - not a motion.");
		return NULL;
	}
	
	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grMotion_CreateFromFile: failed to read version number.");
			return NULL;
		}
	if (u!=MOTION_FILE_VERSION)
		{
			grErrorLog_Add( GR_ERR_FILEIO_VERSION , "grMotion_CreateFromFile: bad or old version number.");
			return NULL;
		}
	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grMotion_CreateFromFile: failed to read motion flags.");
			return NULL;
		}

	if (u & (1<<16)) 
		{
			MaintainNames = GR_TRUE;
		}
	else
		{
			MaintainNames = GR_FALSE;
		}

	NameLength = (u & 0xFFFF);
	NodeType   = (u >> 24);
	M = grMotion_Create(MaintainNames);
	if ( M == NULL )
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grMotion_CreateFromFile.");
			return NULL;
		}
	if (NameLength>0)
		{
			M->Name = (char *)grRam_Allocate(NameLength);
			if ( M->Name == NULL )
				{
					grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grMotion_CreateFromFile.");
					grMotion_Destroy(&M);
					return NULL;
				}
 			if ( grVFile_Read (pFile, M->Name, NameLength ) == GR_FALSE )
				{
					grErrorLog_Add( GR_ERR_FILEIO_READ , "grMotion_CreateFromFile: failed to read motion name.");
					grMotion_Destroy(&M);
					return NULL;
				}
		}
	else
		{
			M->Name = NULL;
		}
	switch (NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_BRANCH):
				if (grMotion_ReadBranch(M,pFile)==GR_FALSE)
					{
						grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_CreateFromFile.");
						grMotion_Destroy(&M);
						return NULL;
					}
				break;
			case (MOTION_NODE_LEAF):
				if (grMotion_ReadLeaf(M,pFile)==GR_FALSE)
					{
						grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_CreateFromFile.");
						grMotion_Destroy(&M);
						return NULL;
					}
				break;
			default:
				assert(0);
				break;
		}
	return M;
}


static grBoolean GRCF grMotion_WriteLeaf(const grMotion *M, grVFile *pFile)
{
	int i;
	grMotion_FileLeafHeader Header;

	#define MOTION_LEAF_EVENTS_FLAG    (1)
	#define MOTION_LEAF_NAMEARRAY_FLAG (2)

	assert( M != NULL );
	assert( pFile != NULL );
	assert( M->NodeType == MOTION_NODE_LEAF);
	assert( grMotion_IsValid(M) != GR_FALSE );

	Header.PathCount = M->Leaf.PathCount;
	Header.NameChecksum = M->Leaf.NameChecksum;
	Header.Flags = 0;

	if (M->Leaf.Events != NULL)
		{
			Header.Flags |= MOTION_LEAF_EVENTS_FLAG;
		}

	if (M->Leaf.NameArray != NULL)
		{
			Header.Flags |= MOTION_LEAF_NAMEARRAY_FLAG;
		}
		
		
	if (grVFile_Write(pFile, &Header, sizeof(grMotion_FileLeafHeader)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE, "grMotion_WriteLeaf: failed to write leaf header."); 
			return GR_FALSE; 
		}
			

	if (Header.Flags & MOTION_LEAF_EVENTS_FLAG)
		{
			if (grTKEvents_WriteToFile(M->Leaf.Events,pFile)==GR_FALSE)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_WriteLeaf."); 
					return GR_FALSE; 
				}
		}

	
	if (Header.Flags & MOTION_LEAF_NAMEARRAY_FLAG)
		{
			if (grStrBlock_WriteToFile(M->Leaf.NameArray,pFile)==GR_FALSE)
				{
					grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_WriteLeaf."); 
					return GR_FALSE; 
				}
		}

	for (i=0; i<M->Leaf.PathCount; i++)
		{
			if (grPath_WriteToFile(M->Leaf.PathArray[i],pFile) == GR_FALSE)
				{
					grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_WriteLeaf: failed to write path", grErrorLog_IntToString(i)); 
					return GR_FALSE; 
				}
		}
	return GR_TRUE;
}

static grBoolean GRCF grMotion_WriteBranch(const grMotion *M, grVFile *pFile)
{
	assert( M != NULL );
	assert( pFile != NULL );
	assert( M->NodeType == MOTION_NODE_BRANCH);
	assert( grMotion_IsValid(M) != GR_FALSE );
	#pragma message("finish this")
	grErrorLog_Add(GR_ERR_INTERNAL_RESOURCE,"grMotion_WriteBranch: saving of compound motions not implemented.");
	return GR_FALSE;
}


GRAPI grBoolean GRCC grMotion_WriteToFile(const grMotion *M,grVFile *pFile)
{
	uint32 u;

	assert( M != NULL );
	assert( pFile != NULL );
	assert( grMotion_IsValid(M) != GR_FALSE );


	// Write the format flag
	u = MOTION_BIN_FILE_TYPE;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE, "grMotion_WriteToFile: failed to write motion header.");
			return GR_FALSE;
		}

	u = MOTION_FILE_VERSION;
	if (grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE, "grMotion_WriteToFile: failed to write motion version number.");
			return GR_FALSE;
		}
	if ( M->Name != NULL )
		{
			u = strlen(M->Name)+1;
		}
	else
		{
			u = 0;
		}
	assert( u < 0xFFFF );
	
	if (M->MaintainNames != GR_FALSE)
		{
			u |= (1<<16);
		}
	assert( M->NodeType < 0xFF );
	u |= (M->NodeType << 24);
	if (grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE, "grMotion_WriteToFile: failed to write motion flags.");
			return GR_FALSE;
		}
	if ((u&0xFFFF) > 0)
		{
			if (grVFile_Write(pFile, M->Name, (u&0xFFFF)) == GR_FALSE)
				{
					grErrorLog_AddString( GR_ERR_FILEIO_WRITE, "grMotion_WriteToFile: failed to write motion name", M->Name);
					return GR_FALSE;
				}
		}

	switch (M->NodeType)
		{
			case (MOTION_NODE_UNDECIDED):
				break;
			case (MOTION_NODE_BRANCH):
				if (grMotion_WriteBranch(M,pFile)==GR_FALSE)
					{
						grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_WriteToFile.");
						return GR_FALSE;
					}
				break;
			case (MOTION_NODE_LEAF):
				if (grMotion_WriteLeaf(M,pFile)==GR_FALSE)
					{
						grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grMotion_WriteToFile.");
						return GR_FALSE;
					}
				break;
			default:
				assert(0);
				break;
		}
	return GR_TRUE;
}
