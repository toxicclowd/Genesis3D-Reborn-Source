/****************************************************************************************/
/*  POP.C																				*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Path optimizer.														*/
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
#include <math.h>

#include "pop.h"
#include "path.h"

#include "MkUtil.h"   // ONLY for Interrupt!


#define xxSUPERSAMPLE

grBoolean Pop_RotationCompare( const grPath *PLong, const grPath *PShort, grFloat Tolerance)
{
	grXForm3d M1;
	grFloat T;
	#ifdef SUPERSAMPLE
		grFloat StartTime,EndTime;
		grFloat LastT=0.0f;
		grFloat DT=0.0f;
	#endif
	grQuaternion Q1,Q2;
	grVec3d V1,V2;

	int Count;
	int i;

	assert( PLong != NULL );
	assert( PShort != NULL );
	
	Count = grPath_GetKeyframeCount(PLong,GR_PATH_ROTATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	assert( grPath_GetKeyframeCount(PShort,GR_PATH_ROTATION_CHANNEL) <= Count );

	for (i=0; i<Count; i++)
		{
			grPath_GetKeyframe(PLong,i,GR_PATH_ROTATION_CHANNEL, &T, &M1);
			grPath_SampleChannels(PShort, T,&Q1,&V1);
			grPath_SampleChannels(PLong , T,&Q2,&V2);
			if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
				return GR_FALSE; 
			#ifdef SUPERSAMPLE
				if (i>0)
					{
						if (i>1)
							{
								if (T-LastT < DT)
									{
										DT = T-LastT;
									}
							}
						else
							{
								DT = T-LastT;
							}
					}
				LastT = T;
			#endif
		}

	#ifdef SUPERSAMPLE
		DT = DT / 3.0f;   // 'oversample' at 3 times

		if (Count>0)
			{
				grPath_GetKeyframe(PLong,0,GR_PATH_ROTATION_CHANNEL, &StartTime, &M1);
			}
		if (Count>1)
			{
				grPath_GetKeyframe(PLong,Count-1,GR_PATH_ROTATION_CHANNEL, &EndTime, &M1);
			}
		else
			{
				EndTime = StartTime;
			}

		for (T=StartTime; T<=EndTime; T+=DT)
			{
				grPath_SampleChannels(PLong,  T, &Q1, &V1);
				grPath_SampleChannels(PShort, T, &Q2, &V2);
				if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
					{
						return GR_FALSE; 
					}
			}
	#endif

	return GR_TRUE;
}


grBoolean Pop_ZapRotationsIfAllKeysEqual( grPath *P, grFloat Tolerance, grBoolean *AllEqual )
{
	grXForm3d M1;
	grFloat T;
	grQuaternion Q1,Q2;
	grVec3d V1,V2;

	int Count;
	int i;

	assert( AllEqual != NULL );
	assert( P != NULL );

	*AllEqual = GR_FALSE;

	Count = grPath_GetKeyframeCount(P,GR_PATH_ROTATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	grPath_GetKeyframe(P,0,GR_PATH_ROTATION_CHANNEL, &T, &M1);
	grPath_SampleChannels(P, T,&Q1,&V1);

	for (i=1; i<Count; i++)
		{
			grPath_GetKeyframe(P,i,GR_PATH_ROTATION_CHANNEL, &T, &M1);
			grPath_SampleChannels(P , T,&Q2,&V2);
			if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
				return GR_TRUE; 
		}

	// if we get to here, all keys are equal.  (delete all but first and last)
	for (i=Count-2; i>0; i--)
		{
			//grPath_GetKeyframe(P,i,GR_PATH_ROTATION_CHANNEL, &T, &M1);
			if (grPath_DeleteKeyframe(P,i,GR_PATH_ROTATION_CHANNEL) == GR_FALSE)
				{
					return GR_FALSE;
				}
		}

	*AllEqual = GR_TRUE;
	return GR_TRUE;
}


#if 0
grBoolean Pop_RotationComparePortion( grPath *PLong, grPath *PShort, grFloat Tolerance,int StartIndex,int EndIndex)
{
	grXForm3d M1;
	grFloat T;
	#ifdef SUPERSAMPLE
		grFloat StartTime,EndTime;
		grFloat LastT=-9e29f;
		grFloat DT=9e29f;
	#endif
	grQuaternion Q1,Q2;
	grVec3d V1,V2;

	int Count;
	int i,n;

	assert( PLong != NULL );
	assert( PShort != NULL );
	
	Count = grPath_GetKeyframeCount(PLong,GR_PATH_ROTATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	assert( grPath_GetKeyframeCount(PShort,GR_PATH_ROTATION_CHANNEL) <= Count );


	for (i=StartIndex; i<EndIndex; i++)
		{
			n = i;
			if (n < 0)
				{
					n = Count + n;
				}
			if (n >= Count)
				{
					n = n-Count;
				}

			grPath_GetKeyframe(PLong,n,GR_PATH_ROTATION_CHANNEL, &T, &M1);
			//grQuaternion_FromMatrix(&M1,&Q1);
			grPath_SampleChannels(PShort, T,&Q1,&V1);
			grPath_SampleChannels(PLong , T,&Q2,&V2);
			if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
				return GR_FALSE; 
			#ifdef SUPERSAMPLE
				if (fabs(T-LastT) < DT)
					{
						DT = (grFloat)fabs(T-LastT);
					}
				LastT = T;
			#endif
		}

	#ifdef SUPERSAMPLE
		DT = DT / 3.0f;   // 'oversample' at 3 times


		if (StartIndex < 0)
			{
				StartIndex = Count + StartIndex;
			}
		if (StartIndex >= Count)
			{
				StartIndex = StartIndex-Count;
			}
		if (EndIndex < 0)
			{
				EndIndex = Count + EndIndex;
			}
		if (EndIndex >= Count)
			{
				EndIndex = EndIndex-Count;
			}

		if (Count==1)
			{		// if count is 0, it didn't get here
					// if count is 1, there is only one possible return value, and that is checked in above loop.
				return GR_TRUE;
			}

		grPath_GetKeyframe(PLong,StartIndex,GR_PATH_ROTATION_CHANNEL, &StartTime, &M1);
		grPath_GetKeyframe(PLong,EndIndex,GR_PATH_ROTATION_CHANNEL, &EndTime, &M1);

		if (EndTime>StartTime)
			{
				for (T=StartTime; T<=EndTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
			}
		else
			{
				grFloat LastTime;
				grPath_GetKeyframe(PLong,Count-1,GR_PATH_ROTATION_CHANNEL, &LastTime, &M1);
				for (T=0.0f; T<=EndTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
				for (T=StartTime; T<=LastTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
			}
	#endif

	return GR_TRUE;
}
#endif


grBoolean Pop_TranslationCompare( const grPath *PLong, const grPath *PShort, grFloat Tolerance)
{
	grXForm3d M1;
	grFloat T;
	#ifdef SUPERSAMPLE
		grFloat StartTime,EndTime;
		grFloat LastT=0.0f;
		grFloat DT=0.0f;
	#endif
	grQuaternion Q1;
	//grQuaternion Q2;
	grVec3d V1;
	//veVec3d V2;

	int Count;
	int i;

	assert( PLong != NULL );
	assert( PShort != NULL );
	
	Count = grPath_GetKeyframeCount(PLong,GR_PATH_TRANSLATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	assert( grPath_GetKeyframeCount(PShort,GR_PATH_TRANSLATION_CHANNEL) <= Count );

	for (i=0; i<Count; i++)
		{
			grPath_GetKeyframe(PLong,i,GR_PATH_TRANSLATION_CHANNEL, &T, &M1);
			grPath_SampleChannels(PShort, T,&Q1,&V1);
			//grPath_SampleChannels(PLong , T,&Q2,&V2);
			if (grVec3d_Compare(&V1, &(M1.Translation),Tolerance) == GR_FALSE)
				{
					return GR_FALSE; 
				}
			#ifdef SUPERSAMPLE
				if (i>0)
					{
						if (i>1)
							{
								if (T-LastT < DT)
									{
										DT = T-LastT;
									}
							}
						else
							{
								DT = T-LastT;
							}
					}
				LastT = T;
			#endif
		}

	#ifdef SUPERSAMPLE

		DT = DT / 3.0f;   // 'oversample' at 3 times

		if (Count>0)
			{
				grPath_GetKeyframe(PLong,0,GR_PATH_TRANSLATION_CHANNEL, &StartTime, &M1);
			}
		if (Count>1)
			{
				grPath_GetKeyframe(PLong,Count-1,GR_PATH_TRANSLATION_CHANNEL, &EndTime, &M1);
			}
		else
			{
				EndTime = StartTime;
			}

		for (T=StartTime; T<=EndTime; T+=DT)
			{
				grPath_SampleChannels(PLong,  T, &Q1, &V1);
				grPath_SampleChannels(PShort, T, &Q2, &V2);
				if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
					{
						return GR_FALSE; 
					}
			}

	#endif
	return GR_TRUE;
}

grBoolean Pop_ZapTranslationsIfAllKeysEqual( grPath *P, grFloat Tolerance, grBoolean *AllEqual )
{
	grXForm3d M1;
	grFloat T;
	grQuaternion Q1,Q2;
	grVec3d V1,V2;

	int Count;
	int i;

	assert( P != NULL );
	assert( AllEqual != NULL );

	*AllEqual = GR_FALSE;
	
	Count = grPath_GetKeyframeCount(P,GR_PATH_TRANSLATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	grPath_GetKeyframe(P,0,GR_PATH_TRANSLATION_CHANNEL, &T, &M1);
	grPath_SampleChannels(P, T,&Q1,&V1);

	for (i=1; i<Count; i++)
		{
			grPath_GetKeyframe(P,i,GR_PATH_TRANSLATION_CHANNEL, &T, &M1);
			grPath_SampleChannels(P , T,&Q2,&V2);
			if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
				return GR_TRUE; 
		}

	// if we get to here, all keys are equal.  (delete all but first and last)
	for (i=Count-2; i>0; i--)
		{
			//grPath_GetKeyframe(P,i,GR_PATH_TRANSLATION_CHANNEL, &T, &M1);
			if (grPath_DeleteKeyframe(P,i,GR_PATH_TRANSLATION_CHANNEL) == GR_FALSE)
				{
					return GR_FALSE;
				}
		}

	*AllEqual = GR_TRUE;
	return GR_TRUE;
}

#if 0
grBoolean Pop_TranslationComparePortion( grPath *PLong, grPath *PShort, grFloat Tolerance,int StartIndex,int EndIndex)
{
	grXForm3d M1;
	grFloat T;
	#ifdef SUPERSAMPLE
		grFloat StartTime,EndTime;
		grFloat LastT=-9e29f;
		grFloat DT=9e29f;
	#endif
	grQuaternion Q1,Q2;
	grVec3d V1,V2;

	int Count;
	int i,n;

	assert( PLong != NULL );
	assert( PShort != NULL );
	
	Count = grPath_GetKeyframeCount(PLong,GR_PATH_TRANSLATION_CHANNEL);
	if (Count == 0)
		{
			return GR_TRUE;
		}
	assert( grPath_GetKeyframeCount(PShort,GR_PATH_TRANSLATION_CHANNEL) <= Count );


	for (i=StartIndex; i<EndIndex; i++)
		{
			n = i;
			if (n < 0)
				{
					n = Count + n;
				}
			if (n >= Count)
				{
					n = n-Count;
				}

			grPath_GetKeyframe(PLong,n,GR_PATH_TRANSLATION_CHANNEL, &T, &M1);
			//grQuaternion_FromMatrix(&M1,&Q1);
			grPath_SampleChannels(PShort, T,&Q1,&V1);
			grPath_SampleChannels(PLong , T,&Q2,&V2);
			if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
				return GR_FALSE; 
			#ifdef SUPERSAMPLE
				if (fabs(T-LastT) < DT)
					{
						DT = (grFloat)fabs(T-LastT);
					}
				LastT = T;
			#endif
		}

	#ifdef SUPERSAMPLE

		DT = DT / 3.0f;   // 'oversample' at 3 times


		if (StartIndex < 0)
			{
				StartIndex = Count + StartIndex;
			}
		if (StartIndex >= Count)
			{
				StartIndex = StartIndex-Count;
			}
		if (EndIndex < 0)
			{
				EndIndex = Count + EndIndex;
			}
		if (EndIndex >= Count)
			{
				EndIndex = EndIndex-Count;
			}

		if (Count==1)
			{		// if count is 0, it didn't get here
					// if count is 1, there is only one possible return value, and that is checked in above loop.
				return GR_TRUE;
			}


		grPath_GetKeyframe(PLong,StartIndex,GR_PATH_TRANSLATION_CHANNEL, &StartTime, &M1);
		grPath_GetKeyframe(PLong,EndIndex,GR_PATH_TRANSLATION_CHANNEL, &EndTime, &M1);

		if (EndTime>StartTime)
			{
				for (T=StartTime; T<=EndTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
			}
		else
			{
				grFloat LastTime;
				grPath_GetKeyframe(PLong,Count-1,GR_PATH_TRANSLATION_CHANNEL, &LastTime, &M1);
				for (T=0.0f; T<=EndTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
				for (T=StartTime; T<=LastTime; T+=DT)
					{
						grPath_SampleChannels(PLong,  T, &Q1, &V1);
						grPath_SampleChannels(PShort, T, &Q2, &V2);
						if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
							{
								return GR_FALSE; 
							}
					}
			}
	#endif

	return GR_TRUE;
}
#endif

grBoolean pop_ZapLastKey(const grPath *P, grPath *Popt, int Channel1, int Channel2, grFloat Tolerance)
{
	assert( Popt != NULL );
	assert( Channel1 != Channel2 );
	assert( Channel1 == GR_PATH_ROTATION_CHANNEL || Channel1 == GR_PATH_TRANSLATION_CHANNEL );
	assert( Channel2 == GR_PATH_ROTATION_CHANNEL || Channel2 == GR_PATH_TRANSLATION_CHANNEL );
	
	if (grPath_GetKeyframeCount(Popt, Channel1)==2)
		{
			if (grPath_GetKeyframeCount(Popt, Channel2)>=2)
				{
					grFloat T1,T2,StartTime1,EndTime1,StartTime2,EndTime2;
					grXForm3d M;
					grQuaternion Q1,Q2;
					grVec3d V1,V2;

					if (grPath_GetTimeExtents(Popt, &StartTime1, &EndTime1)==GR_FALSE)
						{
							return GR_FALSE;
						}
			
					grPath_GetKeyframe(Popt,0,Channel1, &T1, &M);
					grPath_GetKeyframe(Popt,1,Channel1, &T2, &M);
					
					grPath_SampleChannels(Popt, T1, &Q1, &V1);
					grPath_SampleChannels(Popt, T2, &Q2, &V2);
					
					// first and last key have to be equal!
					if (Channel1 == GR_PATH_ROTATION_CHANNEL)
						{
							if (grQuaternion_Compare(&Q1,&Q2,Tolerance) == GR_FALSE)
								{
									return GR_TRUE;
								}
						}
					else
						{
							if (grVec3d_Compare(&V1, &V2,Tolerance) == GR_FALSE)
								{
									return GR_TRUE; 
								}
						}

					if (grPath_DeleteKeyframe(Popt,1,Channel1) == GR_FALSE)
						{
							return GR_FALSE;
						}
					if (grPath_GetTimeExtents(Popt,&StartTime2,&EndTime2)==GR_FALSE)
						{	// cant get extents: try to reverse change and bail out
							if (grPath_InsertKeyframe(Popt,Channel1,T2,&M) == GR_FALSE)
								{
									return GR_FALSE;
								}
							return GR_FALSE;
						}
					if (      (fabs(StartTime1-StartTime2) > Tolerance)
						  ||  (fabs(EndTime1-EndTime2) > Tolerance)     )
						{	// new extents are bad: reverse change and bail out
							if (grPath_InsertKeyframe(Popt,Channel1,T2,&M) == GR_FALSE)
								{
									return GR_FALSE;
								}
							return GR_FALSE;
						}

						
					if (Channel1 == GR_PATH_ROTATION_CHANNEL)
						{
							if ( Pop_RotationCompare   (P,Popt,Tolerance)==GR_FALSE )
								{	// new path has too much error: reverse change and bail out
									if (grPath_InsertKeyframe(Popt,Channel1,T2,&M) == GR_FALSE)
										{
											return GR_FALSE;
										}
									return GR_FALSE;
								}
						}
					else
						{
							if (Pop_TranslationCompare(P,Popt,Tolerance)==GR_FALSE )
								{
									if (grPath_InsertKeyframe(Popt,Channel1,T2,&M) == GR_FALSE)
										{
											return GR_FALSE;
										}
									return GR_FALSE;
								}
						}
					return GR_TRUE;
				}	
		}
	return GR_TRUE;
}

grBoolean pop_ZapIdentityKey(grPath *P, grPath *Popt,int Channel,grFloat Tolerance)
{
	grFloat T;
	grXForm3d M;
	grQuaternion Q;
	grVec3d V;

	assert( Popt != NULL );
	assert( Channel == GR_PATH_ROTATION_CHANNEL || Channel == GR_PATH_TRANSLATION_CHANNEL );
	
	if (grPath_GetKeyframeCount(Popt, Channel)==1)
		{
			grPath_GetKeyframe(Popt,0,Channel, &T, &M);
			grPath_SampleChannels(Popt, T,&Q,&V);

			switch (Channel)
				{
					case (GR_PATH_ROTATION_CHANNEL):
						{
							grQuaternion QI;
							grQuaternion_SetNoRotation(&QI);
							if (grQuaternion_Compare(&Q,&QI, Tolerance) == GR_TRUE)
								{
									if (grPath_DeleteKeyframe(Popt,0,Channel) == GR_FALSE)
										{
											return GR_FALSE;
										}
									if ( Pop_RotationCompare   (P,Popt,Tolerance)==GR_FALSE )
										{
											if (grPath_InsertKeyframe(Popt,GR_PATH_ROTATION_CHANNEL,T,&M) == GR_FALSE)
												{
													return GR_FALSE;
												}
										}
									assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
								}
						}
						break;
					case (GR_PATH_TRANSLATION_CHANNEL):
						{
							grVec3d VI;
							grVec3d_Clear(&VI);
							if (grVec3d_Compare(&V, &VI ,Tolerance) == GR_TRUE)
								{
									if (grPath_DeleteKeyframe(Popt,0,Channel) == GR_FALSE)
										{
											return GR_FALSE;
										}
									if ( Pop_TranslationCompare(P,Popt,Tolerance)==GR_FALSE )
										{
											if (grPath_InsertKeyframe(Popt,GR_PATH_TRANSLATION_CHANNEL,T,&M) == GR_FALSE)
												{
													return GR_FALSE;
												}
										}
									assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
								}
						}
						break;
					default:
						assert(0);
				}
		}

	return GR_TRUE;
}

#if 0
grPath *pop_CreateCopy(grPath *Src)
{
	grPath *P;
	
	int i,Count;
	int RInterp=0;
	int TInterp=0;

	assert ( Src != NULL );

	P = grPath_Create(GR_PATH_INTERPOLATE_HERMITE, GR_PATH_INTERPOLATE_SQUAD,GR_FALSE);	
	if (P == NULL)
		{
			return NULL;
		}

	{
		Count = grPath_GetKeyframeCount(Src,GR_PATH_TRANSLATION_CHANNEL);
		if (Count>0)
			{
				grFloat Time;
				grXForm3d M;

				for (i=0; i<Count; i++)
					{
						grPath_GetKeyframe(Src,i,GR_PATH_TRANSLATION_CHANNEL,&Time,&M);
						if (grPath_InsertKeyframe(P,GR_PATH_TRANSLATION_CHANNEL,Time,&M)==GR_FALSE)
							{
								grPath_Destroy(&P);
								return NULL;
							}
					}
			}
	}

	{
		Count = grPath_GetKeyframeCount(Src,GR_PATH_ROTATION_CHANNEL);
		if (Count>0)
			{
				grFloat Time;
				grXForm3d M;

				for (i=0; i<Count; i++)
					{
						grPath_GetKeyframe(Src,i,GR_PATH_ROTATION_CHANNEL,&Time,&M);
						if (grPath_InsertKeyframe(P,GR_PATH_ROTATION_CHANNEL,Time,&M)==GR_FALSE)
							{
								grPath_Destroy(&P);
								return NULL;
							}
					}
			}
	}

		return P;
}
#endif
					

grPath *Pop_PathOptimize( grPath *P, grFloat Tolerance)
{
	int Count;
	grPath *Popt;
	int i,pass;
	grBoolean AnyRemoved;
	grFloat T;
	grXForm3d M;
	grBoolean AllEqualRotations,AllEqualTranslations;
	

	assert( P != NULL );

	Popt = grPath_CreateCopy(P);
	if (Popt==NULL)
		{
			return NULL;
		}


	AnyRemoved = GR_TRUE;
	pass = 0;

	if (Pop_ZapRotationsIfAllKeysEqual(Popt,Tolerance,&AllEqualRotations) == GR_FALSE)
		{
			grPath_Destroy(&Popt);
			return NULL;
		}
	//assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
	
	if (Pop_ZapTranslationsIfAllKeysEqual(Popt,Tolerance,&AllEqualTranslations) == GR_FALSE)
		{
			grPath_Destroy(&Popt);
			return NULL;
		}
	//assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );

		
	if (AllEqualRotations)
		{
			// Try to kill last 2 keys, if the extent of the path is defined by the other channel...
			if (pop_ZapLastKey(P,Popt,GR_PATH_ROTATION_CHANNEL,GR_PATH_TRANSLATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
		}
	else
		{
			Count = grPath_GetKeyframeCount(Popt,GR_PATH_ROTATION_CHANNEL);
			//for (i=1; i< Count-1; i++)
			for (i=Count-2; i>=1; i--)
				{
					grPath_GetKeyframe(P,i,GR_PATH_ROTATION_CHANNEL, &T, &M);
					if (MkUtil_Interrupt())
						{
							grPath_Destroy(&Popt);
							return NULL;
						}
				
					if (grPath_DeleteKeyframe(Popt,i,GR_PATH_ROTATION_CHANNEL) == GR_FALSE)
						{
							grPath_Destroy(&Popt);
							return NULL;
						}
					//if ( Pop_RotationComparePortion(P,Popt,Tolerance,i-2,i+2)!=GR_FALSE )
					if ( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE )
						{
							AnyRemoved = GR_TRUE;
						}
					else
						{
							//grPath_GetKeyframe(P,i,GR_PATH_ROTATION_CHANNEL, &T, &M);
							if (grPath_InsertKeyframe(Popt,GR_PATH_ROTATION_CHANNEL,T,&M) == GR_FALSE)
								{
									grPath_Destroy(&Popt);
									return NULL;
								}
						}
					/*
					if (Pop_RotationCompare   (P,Popt,Tolerance)==GR_FALSE )
						{
							grPath_GetKeyframe(P,i,GR_PATH_ROTATION_CHANNEL, &T, &M);
							Pop_RotationCompare   (P,Popt,Tolerance);
						}
					*/
					assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
				}

			assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
			assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
			// Try to kill last 2 keys, if the extent of the path is defined by the other channel...
			if (pop_ZapLastKey(P,Popt,GR_PATH_ROTATION_CHANNEL,GR_PATH_TRANSLATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
			assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
			if (pop_ZapIdentityKey(P,Popt,GR_PATH_ROTATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
			assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
		}
	
	if (AllEqualTranslations)
		{
			// Try to kill last 2 keys, if the extent of the path is defined by the other channel...
			if (pop_ZapLastKey(P,Popt,GR_PATH_TRANSLATION_CHANNEL,GR_PATH_ROTATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
		}
	else
		{
			Count = grPath_GetKeyframeCount(Popt,GR_PATH_TRANSLATION_CHANNEL);
			//for (i=1; i< Count-1; i++)
			for (i=Count-2; i>=1; i--)
				{
					grPath_GetKeyframe(P,i,GR_PATH_TRANSLATION_CHANNEL, &T, &M);
					if (MkUtil_Interrupt())
						{
							grPath_Destroy(&Popt);
							return NULL;
						}
					if (grPath_DeleteKeyframe(Popt,i,GR_PATH_TRANSLATION_CHANNEL) == GR_FALSE)
						{
							grPath_Destroy(&Popt);
							return NULL;
						}
					//if ( Pop_TranslationComparePortion(P,Popt,Tolerance,i-2,i+2)!=GR_FALSE )
					if ( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE )
						{
							AnyRemoved = GR_TRUE;
						}
					else
						{
							//grPath_GetKeyframe(P,i,GR_PATH_TRANSLATION_CHANNEL, &T, &M);
							if (grPath_InsertKeyframe(Popt,GR_PATH_TRANSLATION_CHANNEL,T,&M) == GR_FALSE)
								{
									grPath_Destroy(&Popt);
									return NULL;
								}
						}
					assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
				}

			assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
			assert( Pop_RotationCompare   (P,Popt,Tolerance)!=GR_FALSE );
			// Try to kill last 2 keys, if the extent of the path is defined by the other channel...
			if (pop_ZapLastKey(P,Popt,GR_PATH_TRANSLATION_CHANNEL,GR_PATH_ROTATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
			assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
			if (pop_ZapIdentityKey(P,Popt,GR_PATH_TRANSLATION_CHANNEL,Tolerance)==GR_FALSE)
				{
					grPath_Destroy(&Popt);
					return NULL;
				}
			assert( Pop_TranslationCompare(P,Popt,Tolerance)!=GR_FALSE );
		}

			
	return Popt;
}





