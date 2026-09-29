/****************************************************************************************/
/*  QKFRAME.H																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Quaternion keyframe implementation.									*/
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
#include <assert.h>

#include "Vec3d.h"
#include "QKFrame.h"
#include "Errorlog.h"
#include "Ram.h"

#define LINEAR_BLEND(a,b,t)  ( (t)*((b)-(a)) + (a) )	
			// linear blend of a and b  0<t<1 where  t=0 ->a and t=1 ->b

typedef struct
{
	grTKArray_TimeType	Time;				// Time for this keyframe
	grQuaternion	Q;					// quaternion for this keyframe
}  QKeyframe;		
	// This is the root structure that grQKFrame supports
	// all keyframe types must begin with this structure.  Time is first, so
	// that this structure can be manipulated by grTKArray

typedef struct
{
	QKeyframe Key;				// key values for this keyframe
}	grQKFrame_Linear;
	// keyframe data for linear interpolation
	// The structure includes no additional information.

typedef struct
{
	QKeyframe Key;				// key values for this keyframe
}	grQKFrame_Slerp;
	// keyframe data for spherical linear interpolation
	// The structure includes no additional information.

typedef struct
{
	QKeyframe Key;				// key values for this keyframe
	grQuaternion  QuadrangleCorner;	
}	grQKFrame_Squad;
	// keyframe data for spherical quadratic interpolation


grTKArray *GRCC grQKFrame_LinearCreate()
	// creates a frame list for linear interpolation
{
	return grTKArray_Create(sizeof(grQKFrame_Linear) );
}


grTKArray *GRCC grQKFrame_SlerpCreate()
	// creates a frame list for spherical linear interpolation	
{
	return grTKArray_Create(sizeof(grQKFrame_Slerp) );
}

grTKArray *GRCC grQKFrame_SquadCreate()
	// creates a frame list for spherical linear interpolation	
{
	return grTKArray_Create(sizeof(grQKFrame_Squad) );
}


grBoolean GRCC grQKFrame_Insert(
	grTKArray **KeyList,			// keyframe list to insert into
	grTKArray_TimeType Time,		// time of new keyframe
	const grQuaternion *Q,			// quaternion at new keyframe
	int *Index)						// index of new key
	// inserts a new keyframe with the given time and vector into the list.
{
	assert( KeyList != NULL );
	assert( *KeyList != NULL );
	assert( Q != NULL );
	assert(   sizeof(grQKFrame_Squad) == grTKArray_ElementSize(*KeyList) 
	       || sizeof(grQKFrame_Slerp) == grTKArray_ElementSize(*KeyList) 
		   || sizeof(grQKFrame_Linear) == grTKArray_ElementSize(*KeyList) );

	if (grTKArray_Insert(KeyList, Time, Index) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grQKFrame_Insert: grTKArray_Insert failed.");
			return GR_FALSE;
		}
	else
		{
			QKeyframe *KF;
			KF = (QKeyframe *)grTKArray_Element(*KeyList,*Index);
			KF->Q = *Q;
			return GR_TRUE;
		}
}

void GRCC grQKFrame_Query(
	const grTKArray *KeyList,		// keyframe list
	int Index,						// index of frame to return
	grTKArray_TimeType *Time,		// time of the frame is returned
	grQuaternion *Q)					// vector from the frame is returned
	// returns the vector and the time at keyframe[index] 
{
	QKeyframe *KF;
	assert( KeyList != NULL );
	assert( Time != NULL );
	assert( Q != NULL );
	assert( Index < grTKArray_NumElements(KeyList) );
	assert( Index >= 0 );
	assert(   sizeof(grQKFrame_Squad) == grTKArray_ElementSize(KeyList) 
	       || sizeof(grQKFrame_Slerp) == grTKArray_ElementSize(KeyList) 
		   || sizeof(grQKFrame_Linear) == grTKArray_ElementSize(KeyList) );
	
	KF = (QKeyframe *)grTKArray_Element(KeyList,Index);
	*Time = KF->Time;
	*Q    = KF->Q;
}

void GRCC grQKFrame_Modify(
	grTKArray *KeyList,				// keyframe list
	int Index,						// index of frame to change
	const grQuaternion *Q)			// vector for the new key
{
	QKeyframe *KF;
	assert( KeyList != NULL );
	assert( Q != NULL );
	assert( Index < grTKArray_NumElements(KeyList) );
	assert( Index >= 0 );
	assert(   sizeof(grQKFrame_Squad) == grTKArray_ElementSize(KeyList) 
	       || sizeof(grQKFrame_Slerp) == grTKArray_ElementSize(KeyList) 
		   || sizeof(grQKFrame_Linear) == grTKArray_ElementSize(KeyList) );
	
	KF = (QKeyframe *)grTKArray_Element(KeyList,Index);
	KF->Q  = *Q;
}



void GRCC grQKFrame_LinearInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result)			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates linearly
{
	grQuaternion *Q1,*Q2;
	grQuaternion *QNew = (grQuaternion *)Result;
	
	assert( Result != NULL );
	assert( KF1 != NULL );
	assert( KF2 != NULL );
	
	assert( T >= (grFloat)0.0f );
	assert( T <= (grFloat)1.0f );
	
	if ( KF1 == KF2 )
		{
			*QNew = ((grQKFrame_Linear *)KF1)->Key.Q;
			return;
		}

	Q1 = &( ((grQKFrame_Linear *)KF1)->Key.Q);
	Q2 = &( ((grQKFrame_Linear *)KF2)->Key.Q);
	
	QNew->X = LINEAR_BLEND(Q1->X,Q2->X,T);
	QNew->Y = LINEAR_BLEND(Q1->Y,Q2->Y,T);
	QNew->Z = LINEAR_BLEND(Q1->Z,Q2->Z,T);
	QNew->W = LINEAR_BLEND(Q1->W,Q2->W,T);
	if (grQuaternion_Normalize(QNew)==0.0f)
		{
			grQuaternion_SetNoRotation(QNew);
		}

}



void GRCC grQKFrame_SlerpInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result)			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates using spherical linear blending
{
	grQuaternion *Q1,*Q2;
	grQuaternion *QNew = (grQuaternion *)Result;
	
	assert( Result != NULL );
	assert( KF1 != NULL );
	assert( KF2 != NULL );
	
	assert( T >= (grFloat)0.0f );
	assert( T <= (grFloat)1.0f );
	
	if ( KF1 == KF2 )
		{
			*QNew = ((grQKFrame_Slerp *)KF1)->Key.Q;
			return;
		}
 
	Q1 = &( ((grQKFrame_Slerp *)KF1)->Key.Q);
	Q2 = &( ((grQKFrame_Slerp *)KF2)->Key.Q);
	grQuaternion_SlerpNotShortest(Q1,Q2,T,QNew);
}




void GRCC grQKFrame_SquadInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result)			// put the result in here (grQuaternion)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates using spherical quadratic blending
{
	grQuaternion *Q1,*Q2;
	grQuaternion *QNew = (grQuaternion *)Result;
	
	assert( Result != NULL );
	assert( KF1 != NULL );
	assert( KF2 != NULL );
	
	assert( T >= (grFloat)0.0f );
	assert( T <= (grFloat)1.0f );
	
	if ( KF1 == KF2 )
		{
			*QNew = ((grQKFrame_Squad *)KF1)->Key.Q;
			return;
		}

	Q1 = &( ((grQKFrame_Squad *)KF1)->Key.Q);
	Q2 = &( ((grQKFrame_Squad *)KF2)->Key.Q);
	
	{
		grQuaternion *A1,*B2;
		grQuaternion SL1,SL2;
				
		A1 = &( ((grQKFrame_Squad *)KF1)->QuadrangleCorner);
		B2 = &( ((grQKFrame_Squad *)KF2)->QuadrangleCorner);

		grQuaternion_SlerpNotShortest(Q1,   Q2,   T, &SL1);
				assert( grQuaternion_IsUnit(&SL1) == GR_TRUE);
		grQuaternion_SlerpNotShortest(A1,   B2,   T, &SL2);
				assert( grQuaternion_IsUnit(&SL2) == GR_TRUE);
		grQuaternion_SlerpNotShortest(&SL1, &SL2, (2.0f*T*(1.0f-T)), QNew);
				assert( grQuaternion_IsUnit(QNew) == GR_TRUE);
	}
}


static void GRCC grQKFrame_QuadrangleCorner(
	const grQuaternion *Q0,
	const grQuaternion *Q1,
	const grQuaternion *Q2,
	grQuaternion *Corner)
	// compute quadrangle corner for a keyframe containing Q1.
	//  Q0 and Q2 are the quaternions for the previous and next keyframes 
	// corner is the newly computed quaternion
{
	grQuaternion Q1Inv,LnSum;

	assert( Q0 != NULL );
	assert( Q1 != NULL );
	assert( Q2 != NULL );
	assert( Corner != NULL );

	assert( grQuaternion_IsUnit(Q1) == GR_TRUE );

	Q1Inv.W = Q1->W;
	Q1Inv.X = -Q1->X;
	Q1Inv.Y = -Q1->Y;
	Q1Inv.Z = -Q1->Z;
				
	{
		grQuaternion Q1InvQ2, Q1InvQ0;
		grQuaternion Ln1,Ln2;

		grQuaternion_Multiply(&Q1Inv,Q2,&Q1InvQ2);
		grQuaternion_Multiply(&Q1Inv,Q0,&Q1InvQ0);
		grQuaternion_Ln(&Q1InvQ0,&Ln1);
		grQuaternion_Ln(&Q1InvQ2,&Ln2);
		grQuaternion_Add(&Ln1,&Ln2,&LnSum);
		grQuaternion_Scale(&LnSum,-0.25f,&LnSum);
	}

	grQuaternion_Exp(&LnSum,Corner);
	grQuaternion_Multiply(Q1,Corner,Corner);
}

static void GRCC grQKFrame_ChooseBestQuat(const grQuaternion *Q0,grQuaternion *Q1)
	// adjusts the sign of Q1:  to either Q1 or -Q1
	// adjusts Q1 such that Q1 is the 'closest' of the two choices to Q0.
{
	grQuaternion pLessQ,pPlusQ;
	grFloat MagpLessQ,MagpPlusQ;

	assert( Q0 != NULL );
	assert( Q1 != NULL );
	
	grQuaternion_Add(Q0,Q1,&pPlusQ);
	grQuaternion_Subtract(Q0,Q1,&pLessQ);
		
	grQuaternion_Multiply(&pPlusQ,&pPlusQ,&pPlusQ);
	grQuaternion_Multiply(&pLessQ,&pLessQ,&pLessQ);

	MagpLessQ=   (pLessQ.W * pLessQ.W) + (pLessQ.X * pLessQ.X) 
					  + (pLessQ.Y * pLessQ.Y) + (pLessQ.Z * pLessQ.Z);

	MagpPlusQ=   (pPlusQ.W * pPlusQ.W) + (pPlusQ.X * pPlusQ.X) 
					  + (pPlusQ.Y * pPlusQ.Y) + (pPlusQ.Z * pPlusQ.Z);

	if (MagpLessQ >= MagpPlusQ)
		{
			Q1->X = -Q1->X;
			Q1->Y = -Q1->Y;
			Q1->Z = -Q1->Z;
			Q1->W = -Q1->W;
		}
}




void GRCC grQKFrame_SquadRecompute(
	int Looped,				// if keylist has the first key connected to last key
	grTKArray *KeyList,		// list of keys to recompute hermite values for
	grFloat CutInterval)	// intervals <= CutInterval are to be treated as discontinuous
	// rebuild precomputed data for keyframe list.
{

	// compute the extra interpolation points at each keyframe
	// see Advanced Animation and Rendering Techniques 
	//     by Alan Watt and Mark Watt, pg 366
	int i;
	grQKFrame_Squad *QList=NULL;
	int count;
	grFloat T0,T1,T2;
	int Index0,Index1,Index2;
	assert( KeyList != NULL );

	count = grTKArray_NumElements(KeyList);

	if (count > 0)
		{
			QList = (grQKFrame_Squad *)grTKArray_Element(KeyList,0);

			for (i =0; i< count-1; i++)
				{
					grQKFrame_ChooseBestQuat(&(QList[i].Key.Q),&(QList[i+1].Key.Q) );
				}
		}

	if (count<3)
		{
			Looped = 0;
			// cant compute 'slopes' without enough points to loop. 
			// so treat path as non-looped.
		}
	for (i =0; i< count; i++)
		{
			Index0 = i-1;
			Index1 = i;
			Index2 = i+1;

			if (Index1 == 0)
				{
					if (Looped != GR_TRUE)
						{
							Index0 = 0;
						}
					else
						{
							Index0 = count-2;
						}
				}

			if (Index2 == count)
				{
					if (Looped != GR_TRUE)
						{
							Index2 = count-1;
						}
					else
						{
							Index2 = 1;
						}
				}
			
			T0=QList[Index0].Key.Time;
			T1=QList[Index1].Key.Time;
			T2=QList[Index2].Key.Time;

			if (( Looped != GR_TRUE) && (Index1 == 0) || (T1-T0 <= CutInterval) )
				{
					grQuaternion_Copy(
						&(QList[i].Key.Q),
						&(QList[i].QuadrangleCorner) );
				}
			else if ((( Looped != GR_TRUE) && (Index1 == count-1)) || (T2-T1 <= CutInterval) )
				{
					grQuaternion_Copy(
						&(QList[i].Key.Q),
						&(QList[i].QuadrangleCorner) );
				}
			else
			{
				grQKFrame_QuadrangleCorner( 
					&(QList[Index0].Key.Q),
					&(QList[Index1].Key.Q),
					&(QList[Index2].Key.Q),
					&(QList[i].QuadrangleCorner) );
	
			}
		}	
}					



void GRCC grQKFrame_SlerpRecompute(
	grTKArray *KeyList)			// list of keys to recompute hermite values for

	// rebuild precomputed data for keyframe list.
	// also make sure that each successive key is the 'closest' quaternion choice
	// to the previous one.
{

	int i;
	grQKFrame_Slerp *QList;
	int count;
	assert( KeyList != NULL );

	count = grTKArray_NumElements(KeyList);

	if (count > 0)
		{
			QList = (grQKFrame_Slerp  *)grTKArray_Element(KeyList,0);
			for (i =0; i< count-1; i++)
				{
					grQKFrame_ChooseBestQuat(&(QList[i].Key.Q),&(QList[i+1].Key.Q) );
				}
		}
}

//------------------------------------------------------------------------
#define QKFRAME_HINGE_COMPRESSION 0x1
#define QKFRAME_LINEARTIME_COMPRESSION 0x2


#define HINGE_TOLERANCE (0.0001f)
#define LINEARTIME_TOLERANCE (0.0001f)

static grBoolean GRCC grQKFrame_PathIsHinged(grTKArray *KeyList, grFloat Tolerance)
{
	int i,Count;
	grVec3d Axis;
	grVec3d NextAxis;
	grFloat Angle; 
	grQKFrame_Linear* pLinear;

	assert( KeyList != NULL );

	Count = grTKArray_NumElements(KeyList);
	
	if (Count<2)
		return GR_FALSE;
	pLinear = (grQKFrame_Linear*)grTKArray_Element(KeyList, 0);
	if (grQuaternion_GetAxisAngle(&(pLinear->Key.Q),&Axis,&Angle)==GR_FALSE)
		{
			return GR_FALSE;
		}
		
	for (i=1; i<Count; i++)
		{
			pLinear = (grQKFrame_Linear*)grTKArray_Element(KeyList, i);
			if (grQuaternion_GetAxisAngle(&(pLinear->Key.Q),&NextAxis,&Angle)==GR_FALSE)
				{
					return GR_FALSE;
				}
				
			if (grVec3d_Compare(&Axis,&NextAxis,Tolerance) == GR_FALSE)
				{	
					return GR_FALSE;
				}
		}
	return GR_TRUE;
}


static int GRCC grQKFrame_DetermineCompressionType(grTKArray *KeyList)
{
	int Compression=0;
	int NumElements=0;

	assert( KeyList != NULL );

	NumElements = grTKArray_NumElements(KeyList);

	if (NumElements>2)
		{
			if ( grTKArray_SamplesAreTimeLinear(KeyList,LINEARTIME_TOLERANCE) != GR_FALSE )
				{
					Compression |= QKFRAME_LINEARTIME_COMPRESSION;
				}
		}


	if (NumElements>3)
		{
			 if ( grQKFrame_PathIsHinged(KeyList,HINGE_TOLERANCE)!=GR_FALSE )
				{
					Compression |= QKFRAME_HINGE_COMPRESSION;
				}
		}

	return Compression;
}





uint32 GRCC grQKFrame_ComputeBlockSize(grTKArray *KeyList, int Compression)
{
	uint32 Size=0;
	int Count;
	assert( KeyList != NULL );
	assert( Compression < 0xFF);
	
	Count = grTKArray_NumElements(KeyList);

	Size += sizeof(uint32);		// flags
	Size += sizeof(uint32);		// count

	if (Compression & QKFRAME_LINEARTIME_COMPRESSION)
		{
			Size += sizeof(grFloat) * 2;
		}
	else
		{
			Size += sizeof(grFloat) * Count;
		}

	switch (Compression & (~QKFRAME_LINEARTIME_COMPRESSION) )
		{
			case 0:
				Size += sizeof(grQuaternion) * Count;
				break;
			case QKFRAME_HINGE_COMPRESSION:
				Size += (sizeof(grFloat) * 3) + sizeof(grFloat) * Count;
				break;
			default:
				assert(0);
		}	
	return Size;
}

grTKArray *GRCC grQKFrame_CreateFromFile(
			grVFile	*pFile, 
			grQKFrame_InterpolationType		*InterpolationType, 
			int		*Looping,
			grFloat	CutInterval)
{
	uint32 u;
	int BlockSize;
	int Compression;
	int Count,i;
	int FieldSize;
	char *Block;
	grFloat *Data;
	grTKArray *KeyList;
	grQKFrame_Linear* pLinear0;
	grQKFrame_Linear* pLinear;

	assert( pFile != NULL );
	assert( InterpolationType != NULL );
	assert( Looping != NULL );
	
	if (grVFile_Read(pFile, &BlockSize, sizeof(int)) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_READ,"grQKFrame_CreateFromFile: Failed to read header.");
			return NULL;
		}
	if (BlockSize<0)
		{
			grErrorLog_Add(GR_ERR_FILEIO_FORMAT,"grQKFrame_CreateFromFile: Bad Blocksize.");
			return NULL;
		}
			
	Block = (char *)grRam_AllocateClear(BlockSize);
	if(grVFile_Read(pFile, Block, BlockSize) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_READ,"grQKFrame_CreateFromFile.");
			return NULL;
		}
	u = *(uint32 *)Block;
	*((int *)InterpolationType) = (u>>16)& 0xFF;
	Compression = (u>>8) & 0xFF;
	*Looping           = (u & 0x1);		
	Count = *(((uint32 *)Block)+1);
	
	if (Compression > 0xFF)
		{
			grRam_Free(Block);	
			grErrorLog_Add(GR_ERR_FILEIO_VERSION,"grQKFrame_CreateFromFile: Bad Compression Flag.");
			return NULL;
		}
	switch (*InterpolationType)
		{
			case (QKFRAME_LINEAR):
				FieldSize = sizeof(grQKFrame_Linear);
				break;
			case (QKFRAME_SLERP):
				FieldSize = sizeof(grQKFrame_Slerp);
				break;
			case (QKFRAME_SQUAD):
				FieldSize = sizeof(grQKFrame_Squad);
				break;
			default:
				grRam_Free(Block);
				grErrorLog_Add(GR_ERR_FILEIO_VERSION,"grQKFrame_CreateFromFile: Bad InterpolationType");
				return NULL;
		}
	
	KeyList = grTKArray_CreateEmpty(FieldSize,Count);
	if (KeyList == NULL)
		{
			grRam_Free(Block);	
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grQKFrame_CreateFromFile.");
			return NULL;
		}

	Data = (grFloat *)(Block + sizeof(uint32)*2);
			
	pLinear0 = (grQKFrame_Linear*)grTKArray_Element(KeyList, 0);

	pLinear = pLinear0;

	if (Compression & QKFRAME_LINEARTIME_COMPRESSION)
		{
			grFloat fi;
			grFloat fCount = (grFloat)Count;
			grFloat Time,DeltaTime;
			Time = *(Data++);
			DeltaTime = *(Data++);
			for(fi=0.0f;fi<fCount;fi+=1.0f)
				{
					pLinear->Key.Time = Time + fi*DeltaTime;
					pLinear = (grQKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}
	else
		{
			for(i=0;i<Count;i++)
				{
					pLinear->Key.Time = *(Data++);
					pLinear = (grQKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}

	pLinear = pLinear0;

	if (Compression & QKFRAME_HINGE_COMPRESSION)
		{
			grVec3d Hinge;
			Hinge.X = *(Data++);
			Hinge.Y = *(Data++);
			Hinge.Z = *(Data++);

			for(i=0;i<Count;i++)
				{
					grQuaternion_SetFromAxisAngle(&(pLinear->Key.Q),&Hinge,*(Data++));
					pLinear = (grQKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}
	else
		{
			for(i=0;i<Count;i++)
				{
					pLinear->Key.Q = *(grQuaternion *)Data;
					Data += sizeof(grQuaternion)/sizeof(grFloat);
					pLinear = (grQKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}
	
	switch (*InterpolationType)
		{
			case (QKFRAME_LINEAR):
					break;
			case (QKFRAME_SLERP):
				grQKFrame_SlerpRecompute( KeyList);
					break;
			case (QKFRAME_SQUAD):
				grQKFrame_SquadRecompute( *Looping, KeyList, CutInterval);
					break;
			default:
				assert(0);
		}
	grRam_Free(Block);	
	return KeyList;						
}

grBoolean GRCC grQKFrame_WriteToFile(grVFile *pFile, grTKArray *KeyList, 
		grQKFrame_InterpolationType InterpolationType, int Looping)
{
	#define WBERREXIT  {grErrorLog_Add( GR_ERR_FILEIO_WRITE,"grQKFrame_WriteToFile.");return GR_FALSE;}
	uint32 u,BlockSize;
	int Compression;
	int Count,i;
	grFloat Time,DeltaTime;
	assert( pFile != NULL );
	assert( InterpolationType < 0xFF);
	assert( (Looping == 0) || (Looping == 1) );


	Compression = grQKFrame_DetermineCompressionType(KeyList);
	u = (InterpolationType << 16) | (Compression << 8) |  Looping;
	
	BlockSize = grQKFrame_ComputeBlockSize(KeyList,Compression);

	if (grVFile_Write(pFile, &BlockSize,sizeof(uint32)) == GR_FALSE)
		WBERREXIT;
	
	if (grVFile_Write(pFile, &u, sizeof(uint32)) == GR_FALSE)
		WBERREXIT;
	
	Count = grTKArray_NumElements(KeyList);
	if (grVFile_Write(pFile, &Count, sizeof(uint32)) == GR_FALSE)
		WBERREXIT;
	
	if (Compression & QKFRAME_LINEARTIME_COMPRESSION)
		{
			Time = grTKArray_ElementTime(KeyList, 0);
			DeltaTime = grTKArray_ElementTime(KeyList, 1)- Time;
			if (grVFile_Write(pFile, &Time,sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
			if (grVFile_Write(pFile, &DeltaTime,sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
		}
	else
		{
			for(i=0;i<Count;i++)
				{
					Time = grTKArray_ElementTime(KeyList, i);
					if (grVFile_Write(pFile, &Time,sizeof(grFloat)) == GR_FALSE)
						WBERREXIT;
				}
		}

	if (Compression & QKFRAME_HINGE_COMPRESSION)
		{
			grVec3d Hinge;
			grFloat Angle;

			grQKFrame_Linear* pLinear = (grQKFrame_Linear*)grTKArray_Element(KeyList, 0);
			grQuaternion_GetAxisAngle(&(pLinear->Key.Q),&Hinge,&Angle);
			grVec3d_Normalize(&Hinge);
			if (grVFile_Write(pFile, &Hinge.X,sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
			if (grVFile_Write(pFile, &Hinge.Y,sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
			if (grVFile_Write(pFile, &Hinge.Z,sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;

			for(i=0;i<Count;i++)
				{
					grQKFrame_Linear* pLinear = (grQKFrame_Linear*)grTKArray_Element(KeyList, i);
					grQuaternion_GetAxisAngle(&(pLinear->Key.Q),&Hinge,&Angle);
					if (grVFile_Write(pFile, &Angle,sizeof(grFloat)) == GR_FALSE)
						WBERREXIT;
				}
		}
	else
		{
			for(i=0;i<Count;i++)
				{
					grQKFrame_Linear* pLinear = (grQKFrame_Linear*)grTKArray_Element(KeyList, i);
					if (grVFile_Write(pFile, &(pLinear->Key.Q),sizeof(grQuaternion)) == GR_FALSE)
						WBERREXIT;
				}
		}
		
	return GR_TRUE;
}
