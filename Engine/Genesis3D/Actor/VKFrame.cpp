/****************************************************************************************/
/*  VKFRAME.C																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Vector keyframe implementation.										*/
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
/* grVKFrame  (Vector-Keyframe)
	This module handles interpolation for keyframes that contain a vector (a grVec3d)
	This is intended to support Path.c
	grTKArray supplies general support for a time-keyed array, and this supplements
	that support to include the two specific time-keyed arrays:
	  An array of grVec3d interpolated linearly
	  An array of grVec3d interpolated with hermite blending
	These are phycially separated and have different base structures because:
		linear blending requires less data.
		future blending might require more data.
	The two types of lists are created with different creation calls,
	interpolated with different calls, but insertion and queries share a call.
	
	Hermite interpolation requires additional computation after changes are
	made to the keyframe list.  Call grVKFrame_HermiteRecompute() to update the
	calculations.
*/
#include <assert.h>

#include "Vec3d.h"
#include "VKFrame.h"
#include "Errorlog.h"
#include "Ram.h"

#define LINEAR_BLEND(a,b,t)  ( (t)*((b)-(a)) + (a) )	
			// linear blend of a and b  0<t<1 where  t=0 ->a and t=1 ->b

typedef struct
{
	grTKArray_TimeType	Time;		// Time for this keyframe
	grVec3d		V;					// vector for this keyframe
}  grVKFrame;		
	// This is the root structure that grVKFrame supports
	// all keyframe types must begin with this structure.  Time is first, so
	// that this structure can be manipulated by grTKArray

typedef struct
{
	grVKFrame Key;					// key values for this keyframe
	grVec3d		SDerivative;		// Hermite Derivative (Incoming) 
	grVec3d		DDerivative;		// Hermite Derivative (Outgoing) 
}	grVKFrame_Hermite;
	// keyframe data for hermite blending
	// The structure includes computed derivative information.  

typedef struct
{
	grVKFrame Key;				// key values for this keyframe
}	grVKFrame_Linear;
	// keyframe data for linear interpolation
	// The structure includes no additional information.

grTKArray *GRCC grVKFrame_LinearCreate(void)
	// creates a frame list for linear interpolation
{
	return grTKArray_Create(sizeof(grVKFrame_Linear) );
}


grTKArray *GRCC grVKFrame_HermiteCreate(void)
	// creates a frame list for hermite interpolation	
{
	return grTKArray_Create(sizeof(grVKFrame_Hermite) );
}


grBoolean GRCC grVKFrame_Insert(
	grTKArray **KeyList,			// keyframe list to insert into
	grTKArray_TimeType Time,		// time of new keyframe
	const grVec3d *V,				// vector at new keyframe
	int *Index)					// index of new key
	// inserts a new keyframe with the given time and vector into the list.
{
	assert( KeyList != NULL );
	assert( *KeyList != NULL );
	assert( V != NULL );
	assert(   sizeof(grVKFrame_Hermite) == grTKArray_ElementSize(*KeyList) 
	       || sizeof(grVKFrame_Linear) == grTKArray_ElementSize(*KeyList) );

	if (grTKArray_Insert(KeyList, Time, Index) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grVKFrame_Insert: grTKArray_Insert failed.");
			return GR_FALSE;
		}
	else
		{
			grVKFrame *KF;
			KF = (grVKFrame *)grTKArray_Element(*KeyList,*Index);
			KF->V = *V;
			return GR_TRUE;
		}
}

void GRCC grVKFrame_Query(
	const grTKArray *KeyList,		// keyframe list
	int Index,						// index of frame to return
	grTKArray_TimeType *Time,		// time of the frame is returned
	grVec3d *V)						// vector from the frame is returned
	// returns the vector and the time at keyframe[index] 
{
	grVKFrame *KF;
	assert( KeyList != NULL );
	assert( Time != NULL );
	assert( V != NULL );
	assert( Index < grTKArray_NumElements(KeyList) );
	assert( Index >= 0 );
	assert(   sizeof(grVKFrame_Hermite) == grTKArray_ElementSize(KeyList) 
	       || sizeof(grVKFrame_Linear) == grTKArray_ElementSize(KeyList) );
		
	KF = (grVKFrame *)grTKArray_Element(KeyList,Index);
	*Time = KF->Time;
	*V    = KF->V;
}


void GRCC grVKFrame_Modify(
	grTKArray *KeyList,				// keyframe list
	int Index,						// index of frame to change
	const grVec3d *V)				// vector for the key
	// chganes the vector at keyframe[index] 
{
	grVKFrame *KF;
	assert( KeyList != NULL );
	assert( V != NULL );
	assert( Index < grTKArray_NumElements(KeyList) );
	assert( Index >= 0 );
	assert(   sizeof(grVKFrame_Hermite) == grTKArray_ElementSize(KeyList) 
	       || sizeof(grVKFrame_Linear) == grTKArray_ElementSize(KeyList) );
		
	KF = (grVKFrame *)grTKArray_Element(KeyList,Index);
	KF->V = *V;
}


void GRCC grVKFrame_LinearInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result)			// put the result in here (grVec3d)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates linearly
{
	grVec3d *Vec1,*Vec2;
	grVec3d *VNew = (grVec3d *)Result;
	
	assert( Result != NULL );
	assert( KF1 != NULL );
	assert( KF2 != NULL );
	
	assert( T >= (grFloat)0.0f );
	assert( T <= (grFloat)1.0f );
	
	if ( KF1 == KF2 )
		{
			*VNew = ((grVKFrame_Linear *)KF1)->Key.V;
			return;
		}

	Vec1 = &( ((grVKFrame_Linear *)KF1)->Key.V);
	Vec2 = &( ((grVKFrame_Linear *)KF2)->Key.V);
	
	VNew->X = LINEAR_BLEND(Vec1->X,Vec2->X,T);
	VNew->Y = LINEAR_BLEND(Vec1->Y,Vec2->Y,T);
	VNew->Z = LINEAR_BLEND(Vec1->Z,Vec2->Z,T);
}



void GRCC grVKFrame_HermiteInterpolation(
	const void *KF1,		// pointer to first keyframe
	const void *KF2,		// pointer to second keyframe
	grFloat T,				// 0 <= T <= 1   blending parameter
	void *Result)			// put the result in here (grVec3d)
		// interpolates to get a vector between the two vectors at the two
		// keyframes where T==0 returns the vector for KF1 
		// and T==1 returns the vector for KF2
		// interpolates using 'hermite' blending
{
	grVec3d *Vec1,*Vec2;
	grVec3d *VNew = (grVec3d *)Result;
	
	assert( Result != NULL );
	assert( KF1 != NULL );
	assert( KF2 != NULL );
	
	assert( T >= (grFloat)0.0f );
	assert( T <= (grFloat)1.0f );
	
	if ( KF1 == KF2 )
		{
			*VNew = ((grVKFrame_Hermite *)KF1)->Key.V;
			return;
		}

	Vec1 = &( ((grVKFrame_Hermite *)KF1)->Key.V);
	Vec2 = &( ((grVKFrame_Hermite *)KF2)->Key.V);

	{
		grFloat	t2;			// T sqaured
		grFloat	t3;			// T cubed
		grFloat   H1,H2,H3,H4;	// hermite basis function coefficients

		t2 = T * T;
		t3 = t2 * T;
	
		H2 = -(t3 + t3) + t2*3.0f;
		H1 = 1.0f - H2;
		H4 = t3 - t2;
		H3 = H4 - t2 + T;   //t3 - 2.0f * t2 + t;
		
		grVec3d_Scale(Vec1,H1,VNew);
		grVec3d_AddScaled(VNew,Vec2,H2,VNew);
		grVec3d_AddScaled(VNew,&( ((grVKFrame_Hermite *)KF1)->DDerivative),H3,VNew);
		grVec3d_AddScaled(VNew,&( ((grVKFrame_Hermite *)KF2)->SDerivative),H4,VNew);
	}
}


void GRCC grVKFrame_HermiteRecompute(
	int Looped,				 // if keylist has the first key connected to last key
	grBoolean ZeroDerivative,// if each key should have a zero derivatives (good for 2 point S curves)
	grTKArray *KeyList,		 // list of keys to recompute hermite values for
	grFloat CutInterval)	 // intervals <= CutInterval are to be treated as discontinuous
	// rebuild precomputed data for keyframe list.
{
	// compute the incoming and outgoing derivatives at each keyframe
	int i;
	grVec3d V0,V1,V2;
	grFloat Time0, Time1, Time2, N0, N1, N0N1;
	grVKFrame_Hermite *TK;
	grVKFrame_Hermite *Vector= NULL;
	int count;
	int Index0,Index1,Index2;

	assert( KeyList != NULL );
	assert( sizeof(grVKFrame_Hermite) == grTKArray_ElementSize(KeyList) );
	
			
	// Compute derivatives at the keyframe points:
	// The derivative is the average of the source chord p[i]-p[i-1]
	// and the destination chord p[i+1]-p[i]
	//     (where i is Index1 in this function)
	//  D = 1/2 * ( p[i+1]-p[i-1] ) = 1/2 *( (p[i+1]-p[i]) + (p[i]-p[i-1]) )
	//  The very first and last chords are simply the 
	// destination and source derivative.
	//   These 'averaged' D's are adjusted for variences in the time scale
	// between the Keyframes.  To do this, the derivative at each keyframe
	// is split into two parts, an incoming ('source' DS) 
	// and an outgoing ('destination' DD) derivative.
	// DD[i] = DD[i] * 2 * N[i]  / ( N[i-1] + N[i] )   
	// DS[i] = DS[i] * 2 * N[i-1]/ ( N[i-1] + N[i] )
	//    where N[i] is time between keyframes i and i+1
	// Since the chord dealt with on a given chord between key[i] and key[i+1], only
	// one of the derivates are needed for each keyframe.  For key[i] the outgoing
	// derivative at is needed (DD[i]).  For key[i+1], the incoming derivative
	// is needed (DS[i+1])   ( note that  (1/2) * 2 = 1 )

	count = grTKArray_NumElements(KeyList);
	if (count > 0)
		{
			Vector = (grVKFrame_Hermite *)grTKArray_Element(KeyList,0);
		}

	if (ZeroDerivative!=GR_FALSE)
		{	// in this case, just bang all derivatives to zero.
			for (i =0; i< count; i++)
				{
					TK = &(Vector[i]);
					grVec3d_Clear(&(TK->DDerivative));
					grVec3d_Clear(&(TK->SDerivative));
				}
			return;
		}

	if (count < 3)			
		{
			Looped = GR_FALSE;	
			// cant compute slopes without a closed loop: 
			// so compute slopes as if it is not closed.
		}
	for (i =0; i< count; i++)
		{
			TK = &(Vector[i]);
			Index0 = i-1;
			Index1 = i;
			Index2 = i+1;

			Time1 = Vector[Index1].Key.Time;
			if (Index1 == 0)
				{
					if (Looped != GR_TRUE)
						{
							Index0 = 0;			
							Time0 = Vector[Index0].Key.Time;
						}
					else
						{
							Index0 = count-2;
							Time0 = Time1 - (Vector[count-1].Key.Time - Vector[count-2].Key.Time);
						}
				}
			else
				{
					Time0 = Vector[Index0].Key.Time;
				}


			if (Index2 == count)
				{
					if (Looped != GR_TRUE)
						{
							Index2 = count-1;
							Time2 = Vector[Index2].Key.Time;
						}
					else
						{
							Index2 = 1;
							Time2 = Time1 + (Vector[1].Key.Time - Vector[0].Key.Time);
						}
				}
			else
				{
					Time2 = Vector[Index2].Key.Time;
				}

			V0 = Vector[Index0].Key.V;
			V1 = Vector[Index1].Key.V;
			V2 = Vector[Index2].Key.V;

			N0    = (Time1 - Time0);
			N1    = (Time2 - Time1);
			N0N1  = N0 + N1;

			if ( ( (Looped != GR_TRUE) && (Index1 == 0)) || (N0<=CutInterval) )
				{
					grVec3d_Subtract(&V2,&V1,&(TK->SDerivative));
					grVec3d_Copy( &(TK->SDerivative), &(TK->DDerivative));
				}
			else if ( ( (Looped != GR_TRUE) && (Index1 == count-1) ) || (N1<=CutInterval) )
				{
					grVec3d_Subtract(&V1,&V0,&(TK->SDerivative));
					grVec3d_Copy( &(TK->SDerivative), &(TK->DDerivative));
				}
			else
			{
				grVec3d Slope;
				grVec3d_Subtract(&V2,&V0,&Slope);
				grVec3d_Scale(&Slope, (N1 / N0N1), &(TK->DDerivative));
				grVec3d_Scale(&Slope, (N0 / N0N1), &(TK->SDerivative));
			}
		}	
}		


#define LINEARTIME_TOLERANCE (0.0001f)
#define VKFRAME_LINEARTIME_COMPRESSION 0x2

uint32 GRCC grVKFrame_ComputeBlockSize(grTKArray *KeyList, int Compression)
{
	uint32 Size=0;
	int Count;

	assert( KeyList != NULL );
	assert( Compression < 0xFF);
	
	Count = grTKArray_NumElements(KeyList);

	Size += sizeof(uint32);		// flags
	Size += sizeof(uint32);		// count

	if (Compression & VKFRAME_LINEARTIME_COMPRESSION)
		{
			Size += sizeof(grFloat) * 2;
		}
	else
		{
			Size += sizeof(grFloat) * Count;
		}

	Size += sizeof(grFloat) * 3 * Count;
	return Size;
}


grTKArray *GRCC grVKFrame_CreateFromFile(	grVFile *pFile, 
												grVKFrame_InterpolationType		*InterpolationType, 
												int		*Looping, 
												grFloat CutInterval)
{
	uint32 u;
	int BlockSize;
	int Compression;
	int Count,i;
	int FieldSize;
	char *Block;
	grFloat *Data;
	grTKArray *KeyList;
	grVKFrame_Linear* pLinear0;
	grVKFrame_Linear* pLinear;
	
	assert( pFile != NULL );
	assert( InterpolationType != NULL );
	assert( Looping != NULL );
	
	if (grVFile_Read(pFile, &BlockSize, sizeof(int)) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_READ,"grVKFrame_CreateFromFile: Failed to read header.");
			return NULL;
		}
	if (BlockSize<0)
		{
			grErrorLog_Add(GR_ERR_FILEIO_FORMAT,"grVKFrame_CreateFromFile: Bad Blocksize.");
			return NULL;
		}
			
	Block = (char *)grRam_AllocateClear(BlockSize);
	if(grVFile_Read(pFile, Block, BlockSize) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_READ,"grVKFrame_CreateFromFile: Failed to read header block.");
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
			grErrorLog_Add(GR_ERR_FILEIO_VERSION,"grVKFrame_CreateFromFile: Bad Compression Flag");
			return NULL;
		}
	switch (*InterpolationType)
		{
			case (VKFRAME_LINEAR):
					FieldSize = sizeof(grVKFrame_Linear);
					break;
			case (VKFRAME_HERMITE):
			case (VKFRAME_HERMITE_ZERO_DERIV):
					FieldSize = sizeof(grVKFrame_Hermite);
					break;
			default:
					grRam_Free(Block);	
					grErrorLog_Add(GR_ERR_FILEIO_VERSION,"grVKFrame_CreateFromFile: Bad InterpolationType");
					return NULL;
		}

	KeyList = grTKArray_CreateEmpty(FieldSize,Count);
	if (KeyList == NULL)
		{
			grRam_Free(Block);	
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grVKFrame_CreateFromFile.");
			return NULL;
		}

	Data = (grFloat *)(Block + sizeof(uint32)*2);
			
	pLinear0 = (grVKFrame_Linear*)grTKArray_Element(KeyList, 0);

	pLinear = pLinear0;

	if (Compression & VKFRAME_LINEARTIME_COMPRESSION)
		{
			grFloat fi;
			grFloat fCount = (grFloat)Count;
			grFloat Time,DeltaTime;
			Time = *(Data++);
			DeltaTime = *(Data++);
			for(fi=0.0f;fi<fCount;fi+=1.0f)
				{
					pLinear->Key.Time = Time + fi*DeltaTime;
					pLinear = (grVKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}
	else
		{
			for(i=0;i<Count;i++)
				{
					pLinear->Key.Time = *(Data++);
					pLinear = (grVKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
				}
		}

	pLinear = pLinear0;
	for(i=0;i<Count;i++)
		{
			pLinear->Key.V.X =*(Data++);
			pLinear->Key.V.Y =*(Data++);
			pLinear->Key.V.Z =*(Data++);
			pLinear = (grVKFrame_Linear *)  ( ((char *)pLinear) + FieldSize );
		}

	switch (*InterpolationType)
		{
			case (VKFRAME_LINEAR):
					break;
			case (VKFRAME_HERMITE):
				grVKFrame_HermiteRecompute(	*Looping, GR_FALSE, KeyList, CutInterval);
					break;
			case (VKFRAME_HERMITE_ZERO_DERIV):
				grVKFrame_HermiteRecompute(	*Looping, GR_TRUE, KeyList, CutInterval);
					break;
			default:
				assert(0);
		}
	grRam_Free(Block);
	return KeyList;	

}

grBoolean GRCC grVKFrame_WriteToFile(grVFile *pFile, grTKArray *KeyList, 
		grVKFrame_InterpolationType InterpolationType, int Looping)
{
	#define WBERREXIT  {grErrorLog_Add( GR_ERR_FILEIO_WRITE,"grVKFrame_WriteToFile.");return GR_FALSE;}
	uint32 u,BlockSize;
	int Compression=0;
	int Count,i;
	grFloat Time,DeltaTime;

	assert( pFile != NULL );
	assert( InterpolationType < 0xFF);
	assert( (Looping == 0) || (Looping == 1) );

	if (grTKArray_NumElements(KeyList)>2)
		{
			if ( grTKArray_SamplesAreTimeLinear(KeyList,LINEARTIME_TOLERANCE) != GR_FALSE )
				{
					Compression |= VKFRAME_LINEARTIME_COMPRESSION;
				}
		}

	u = (InterpolationType << 16) |  (Compression << 8) |  Looping;
	
	BlockSize = grVKFrame_ComputeBlockSize(KeyList,Compression);

	if (grVFile_Write(pFile, &BlockSize,sizeof(uint32)) == GR_FALSE)
		WBERREXIT;
	
	if (grVFile_Write(pFile, &u, sizeof(uint32)) == GR_FALSE)
		WBERREXIT;
	
	Count = grTKArray_NumElements(KeyList);
	if (grVFile_Write(pFile, &Count, sizeof(uint32)) == GR_FALSE)
		WBERREXIT;

	if (Compression & VKFRAME_LINEARTIME_COMPRESSION)
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

	for(i=0;i<Count;i++)
		{
			grVKFrame_Linear* pLinear = (grVKFrame_Linear*)grTKArray_Element(KeyList, i);
			if (grVFile_Write(pFile, &(pLinear->Key.V.X),sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
			if (grVFile_Write(pFile, &(pLinear->Key.V.Y),sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
			if (grVFile_Write(pFile, &(pLinear->Key.V.Z),sizeof(grFloat)) == GR_FALSE)
				WBERREXIT;
		}

	return GR_TRUE;
}
