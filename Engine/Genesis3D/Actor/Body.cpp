/****************************************************************************************/
/*  BODY.C                                                                              */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Actor body implementation.                                             */
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

#include <assert.h>						//assert()
#include <math.h> 						//fabs()
#include <stdlib.h>						//qsort()

#include "Body.h"
#include "BODY._H"
#include "Ram.h"
#include "Errorlog.h"
#include "Log.h"

#include "grResource.h"

#define MAX(aa,bb)   ( (aa)>(bb)?(aa):(bb) )
#define MIN(aa,bb)   ( (aa)<(bb)?(aa):(bb) )

// disable "Unreferenced local function has been removed" warning.
#pragma warning (disable:4505)


#if defined(DEBUG) || !defined(NDEBUG)
GRAPI grBoolean GRCC grBody_SanityCheck(const grBody *B)
{
	int i,j,k;
	int Lod,FaceCount,VertexCount,NormalCount,BoneCount;
	grBody_XSkinVertex *SV;
	grBody_Bone *Bone;
	grBody_Normal *N;

	Lod = B->LevelsOfDetail;
	VertexCount = B->XSkinVertexCount;
	NormalCount = B->SkinNormalCount;
	BoneCount   = B->BoneCount;

	if (B->MaterialNames == NULL )
		return GR_FALSE;
	if (B->MaterialCount != grStrBlock_GetCount(B->MaterialNames))
		return GR_FALSE;

	if (B->BoneNames == NULL)
		return GR_FALSE;
	if (B->BoneCount != grStrBlock_GetCount(B->BoneNames))
		return GR_FALSE;

	if ((B->XSkinVertexArray == NULL) && (B->XSkinVertexCount>0))
		return GR_FALSE;
	if ((B->SkinNormalArray == NULL) && (B->SkinNormalCount>0))
		return GR_FALSE;
	if ((B->BoneArray == NULL) && (B->BoneCount>0))
		return GR_FALSE;
	if ((B->MaterialArray == NULL) && (B->MaterialCount>0))
		return GR_FALSE;


	for (i=0; i<Lod; i++)
		{
			grBody_Triangle *F;
			FaceCount = B->SkinFaces[i].FaceCount;
			for (j=0,F=B->SkinFaces[i].FaceArray; j<FaceCount; j++,F++)
				{
					for (k=0; k<3; k++)
						{
							if ((F->VtxIndex[k]    < 0) || (F->VtxIndex[k]    >= VertexCount  ))
								return GR_FALSE;
							if ((F->NormalIndex[k] < 0) || (F->NormalIndex[k] >= NormalCount  ))
								return GR_FALSE;
							if ((F->MaterialIndex  < 0) || (F->MaterialIndex  >= B->MaterialCount))
								return GR_FALSE;
						}
				}
		}
	for (i=0,SV = B->XSkinVertexArray; i<VertexCount; i++,SV++)
		{
			if ((SV->BoneIndex < 0) || (SV->BoneIndex >= BoneCount))
				return GR_FALSE;
		}

	for (i=0,N = B->SkinNormalArray; i<NormalCount; i++,N++)
		{
			if ((N->BoneIndex < 0) || (N->BoneIndex >= BoneCount))
				return GR_FALSE;
		}

	for (i=0,Bone = B->BoneArray; i<BoneCount; i++,Bone++)
		{
			if (Bone->ParentBoneIndex != GR_BODY_NO_PARENT_BONE)
				{
					if ((Bone->ParentBoneIndex < 0) || (Bone->ParentBoneIndex > i))
						return GR_FALSE;
				}
		}

	return GR_TRUE;
				
}
#endif


GRAPI grBoolean GRCC grBody_IsValid(const grBody *B)
{
	if ( B == NULL )
		return GR_FALSE;
	if ( B -> IsValid != B )
		return GR_FALSE;
	assert( grBody_SanityCheck(B) != GR_FALSE) ;
	return GR_TRUE;
}
	

static grBody *GRCF grBody_CreateNull(void)
{
	grBody *B;
	int i;

	B = GR_RAM_ALLOCATE_STRUCT_CLEAR(grBody);
	if ( B == NULL)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grBody_CreateNull:  Failed to allocate space for grBody.");
			return NULL;
		}
	B->IsValid          = NULL;
	B->XSkinVertexCount	= 0;
	B->XSkinVertexArray	= NULL;
	
	B->SkinNormalCount	= 0;
	B->SkinNormalArray	= NULL;

	B->BoneCount		= 0;
	B->BoneArray		= NULL;
	B->BoneNames		= NULL;
			
	B->MaterialCount	= 0;
	B->MaterialArray	= NULL;
	B->MaterialNames	= NULL;
	for (i=0; i<GR_BODY_NUMBER_OF_LOD; i++)
		{
			B->SkinFaces[i].FaceCount = 0;
			B->SkinFaces[i].FaceArray = NULL;
		}
	B->LevelsOfDetail = 1;

	B->optFlags = (   GR_BODY_OPTIMIZE_FLAGS_VERTS 
					| GR_BODY_OPTIMIZE_FLAGS_NORMALS 
					| GR_BODY_OPTIMIZE_FLAGS_SORT_VERTS 
					| GR_BODY_OPTIMIZE_FLAGS_SORT_FACES);

	B->IsValid = B;

	grVec3d_Set(&(B->BoundingBoxMin),0.0f,0.0f,0.0f);
	grVec3d_Set(&(B->BoundingBoxMax),0.0f,0.0f,0.0f);

	B->blendDataCount = 0;
	B->blendDataArray = NULL;

	return B;
}

static void GRCF grBody_DestroyPossiblyIncompleteBody( grBody **PB ) 
{
	grBody *B;
	int i;

	B = *PB;
	if ( ! B )
		return;
	B->IsValid = NULL;
	if (B->XSkinVertexArray != NULL)
		{
			grRam_Free( B->XSkinVertexArray );
			B->XSkinVertexArray = NULL;
		}
	if (B->SkinNormalArray != NULL)
		{
			grRam_Free( B->SkinNormalArray );
			B->SkinNormalArray = NULL;
		}
	if (B->BoneNames != NULL)
		{
			grStrBlock_Destroy(&(B->BoneNames));
			B->BoneNames = NULL;
		}
	if (B->BoneArray != NULL)
		{	
			grRam_Free(B->BoneArray);
			B->BoneArray = NULL;
		}
	if (B->MaterialArray != NULL)
		{
			for (i=0; i<B->MaterialCount; i++)
				{
					// <> CB ; see note above
					// this doesn't seem to prevent us from crashing here
					//	when an actor has an error during _Create
					#if 1
					if ( (uint32)(B->MaterialArray[i].MatSpec) > 1 )
					#endif
						grMaterialSpec_Destroy(&(B->MaterialArray[i].MatSpec));
					B->MaterialArray[i].MatSpec = NULL;
				}
			grRam_Free( B->MaterialArray );
			B->MaterialArray = NULL;
		}
	if (B->MaterialNames != NULL)
		{
			grStrBlock_Destroy(&(B->MaterialNames));
			B->MaterialNames = NULL;
		}
	
	for (i=0; i<GR_BODY_NUMBER_OF_LOD; i++)
		{
			if (B->SkinFaces[i].FaceArray != NULL)
				{
					grRam_Free(B->SkinFaces[i].FaceArray);
					B->SkinFaces[i].FaceArray = NULL;
				}
		}

	if (B->blendDataArray != NULL)
	{
		grRam_Free(B->blendDataArray);
		B->blendDataArray = NULL;
	}

	grRam_Free(*PB);
	*PB = NULL;
}

GRAPI grBody *GRCC grBody_Create(void)
{
	grBody *B;

	B = grBody_CreateNull();
	if ( B == NULL)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_Create.");
			return NULL;
		}

	B->BoneNames = grStrBlock_Create();
	if (B->BoneNames == NULL)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grBody_Create.");
			grBody_DestroyPossiblyIncompleteBody(&B);
			return NULL;
		}
	B->MaterialNames	= grStrBlock_Create();

	if (B->MaterialNames == NULL)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grBody_Create.");
			grBody_DestroyPossiblyIncompleteBody(&B);
			return NULL;
		}

	assert( grBody_SanityCheck(B) != GR_FALSE );
	return B;
}

GRAPI void GRCC grBody_Destroy(grBody **PB)
{
	assert(  PB != NULL );
	assert( *PB != NULL );
	assert( grBody_IsValid(*PB) != GR_FALSE );
	grBody_DestroyPossiblyIncompleteBody( PB );
}


GRAPI grBoolean GRCC grBody_GetGeometryStats(const grBody *B, int lod, int *Vertices, int *Faces, int *Normals)
{
	assert( grBody_IsValid(B) == GR_TRUE );
	assert( ( lod >=0 ) && ( lod < GR_BODY_NUMBER_OF_LOD ) );
	*Vertices = B->XSkinVertexCount;
	*Faces    = B->SkinFaces[lod].FaceCount;
	*Normals  = B->SkinNormalCount;
	return GR_TRUE;
}



GRAPI int GRCC grBody_GetBoneCount(const grBody *B)
{
	assert( B != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	return B->BoneCount;
}
	
GRAPI void GRCC grBody_GetBone(const grBody *B, 
	int BoneIndex, 
	const char **BoneName,
	grXForm3d *Attachment,
	int *ParentBoneIndex)
{
	assert( B != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	assert( Attachment != NULL );
	assert( ParentBoneIndex != NULL );
	assert(  BoneName != NULL );

	assert( BoneIndex >=0 );
	assert( BoneIndex < B->BoneCount );
	*Attachment = B->BoneArray[BoneIndex].AttachmentMatrix;
	*ParentBoneIndex = B->BoneArray[BoneIndex].ParentBoneIndex;
	*BoneName = grStrBlock_GetString(B->BoneNames,BoneIndex);
}

GRAPI int32 GRCC grBody_GetBoneNameChecksum(const grBody *B)
{
	assert( grBody_IsValid(B) != GR_FALSE );
	
	if (B->BoneNames != NULL)
		{
			return grStrBlock_GetChecksum( B->BoneNames );
		}
	else
		return 0;
}


GRAPI grBoolean GRCC grBody_GetBoundingBox( const grBody *B, 
							int BoneIndex, 
							grVec3d *MinimumBoxCorner,
							grVec3d *MaximumBoxCorner)
{
	assert( B != NULL);
	assert( MinimumBoxCorner != NULL );
	assert( MaximumBoxCorner != NULL );
	assert( (BoneIndex >=0)            || (BoneIndex == GR_BODY_ROOT));
	assert( (BoneIndex < B->BoneCount) || (BoneIndex == GR_BODY_ROOT));
	if (BoneIndex == GR_BODY_ROOT)
		{
		#pragma message ("discontinue this?")
			*MinimumBoxCorner = B->BoundingBoxMin;
			*MaximumBoxCorner = B->BoundingBoxMax;
		}
	else
		{			
			grBody_Bone *Bone = &(B->BoneArray[BoneIndex]);

			if (Bone->BoundingBoxMin.X > Bone->BoundingBoxMax.X)
				{
					// bone has no bounding box (hopefully because it has no geometry)  
					// This is a valid condition - not really an error.
					// it's possible that this could be an error condition.  But if it is
					// it is ignored.
					return GR_FALSE;
				}
			*MinimumBoxCorner = Bone->BoundingBoxMin;
			*MaximumBoxCorner = Bone->BoundingBoxMax;
		}
	return GR_TRUE;
}

GRAPI void GRCC grBody_SetBoundingBox( grBody *B, 
							int BoneIndex,
							const grVec3d *MinimumBoxCorner,
							const grVec3d *MaximumBoxCorner)
{
	assert( B != NULL);
	assert( MinimumBoxCorner != NULL );
	assert( MaximumBoxCorner != NULL );
	assert( (BoneIndex >=0)            || (BoneIndex == GR_BODY_ROOT));
	assert( (BoneIndex < B->BoneCount) || (BoneIndex == GR_BODY_ROOT));
	if (BoneIndex == GR_BODY_ROOT)
		{
			B->BoundingBoxMin = *MinimumBoxCorner;
			B->BoundingBoxMax = *MaximumBoxCorner;
		}
	else
		{			
			B->BoneArray[BoneIndex].BoundingBoxMin = *MinimumBoxCorner;
			B->BoneArray[BoneIndex].BoundingBoxMax = *MaximumBoxCorner;
		}
}
							



GRAPI grBoolean GRCC grBody_GetBoneByName(const grBody* B,
	const char* BoneName,
	int* pBoneIndex,
	grXForm3d* Attachment,
	int* pParentBoneIndex)
{
	assert( B != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	assert( Attachment != NULL );
	assert( pParentBoneIndex != NULL );
	assert( pBoneIndex != NULL );
	assert(  BoneName != NULL );

	if(grStrBlock_FindString(B->BoneNames, BoneName, pBoneIndex) == GR_TRUE)
	{
		*Attachment = B->BoneArray[*pBoneIndex].AttachmentMatrix;
		*pParentBoneIndex = B->BoneArray[*pBoneIndex].ParentBoneIndex;

		return GR_TRUE;
	}

	return GR_FALSE;
}

GRAPI int GRCC grBody_GetMaterialCount(const grBody *B)
{
	assert( B != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	return B->MaterialCount;
}

#define GR_BODY_TOLERANCE (0.001f)

static grBoolean GRCF grBody_XSkinVertexCompare(
	const grBody_XSkinVertex *SV1,
	const grBody_XSkinVertex *SV2)
{
	assert( SV1 != NULL );
	assert( SV2 != NULL );
	if (grVec3d_Compare( &(SV1->XPoint), &(SV2->XPoint), 
						GR_BODY_TOLERANCE) == GR_FALSE)
		{
			return GR_FALSE;
		}
	if (fabs(SV1->XU - SV2->XU) > GR_BODY_TOLERANCE)
		{
			return GR_FALSE;
		}
	if (fabs(SV1->XV - SV2->XV) > GR_BODY_TOLERANCE)
		{
			return GR_FALSE;
		}
	return GR_TRUE;
}	


static void GRCF grBody_SwapVertexIndices( grBody *B, grBody_Index Index1, grBody_Index Index2)
	// zips through all triangles, and swaps index1 and index2.
{
	int i,j,lod;
	grBody_Index Count;
	grBody_Triangle *T;

	assert( B!=NULL );	
	for (lod = 0; lod< GR_BODY_NUMBER_OF_LOD; lod++)
		{
			Count = B->SkinFaces[lod].FaceCount;
			for (i=0,T=B->SkinFaces[lod].FaceArray;
					i<Count; 
					i++,T++)
				{
					for (j=0; j<3; j++)	
						{	
							if (T->VtxIndex[j] == Index1)
								{
									T->VtxIndex[j] = Index2;
								}
							else
								{
									if (T->VtxIndex[j] == Index2)
										{
											T->VtxIndex[j] = Index1;
										}
								}	
						}
				}
		}
}


typedef struct 
		{
			int BoneIndex;
			int OriginalIndex;
			int ReMapIndex;
		} grBody_SkinSortVMap;

static int grBody_QSortSkinVertexCompare( const void *arg1, const void *arg2 )
{
	grBody_SkinSortVMap *V1,*V2;
	assert( arg1 );
	assert( arg2 );

	V1 = (grBody_SkinSortVMap *)arg1; 
	V2 = (grBody_SkinSortVMap *)arg2;

	if (V1->BoneIndex < V2->BoneIndex)
		return -1;
	if (V1->BoneIndex > V2->BoneIndex)
		return 1;
	return 0;
}


static grBoolean GRCF grBody_SortSkinVertices( grBody *B )
{
	grBody_Triangle *T;
	int i,j,lod;
	int Count;
	grBoolean AnyChanges = GR_FALSE;
	grBody_SkinSortVMap *VertexMap;
	grBody_XSkinVertex  *VertexCopy;

	assert( B != NULL );
	
	Count = B->XSkinVertexCount;
	VertexMap = GR_RAM_ALLOCATE_ARRAY(grBody_SkinSortVMap,Count);
	if (VertexMap == NULL)
		{
			grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE,"grBody_SortSkinVertices: failed to allocate array",NULL);
			return GR_FALSE;
		}
	VertexCopy = GR_RAM_ALLOCATE_ARRAY(grBody_XSkinVertex,Count);
	if (VertexMap == NULL)
		{
			grErrorLog_AddString(GR_ERR_MEMORY_RESOURCE,"grBody_SortSkinVertices: failed to allocate copy of vertex array",NULL);
			grRam_Free(VertexMap);
			return GR_FALSE;
		}
	for (i=0; i<Count; i++)
		{
			VertexMap[i].BoneIndex = B->XSkinVertexArray[i].BoneIndex;
			VertexMap[i].OriginalIndex = i;
			VertexCopy[i] = B->XSkinVertexArray[i];
		}

	qsort( VertexMap, Count, sizeof(VertexMap[0]), grBody_QSortSkinVertexCompare);

	for (i=0; i<Count; i++)
		{
			B->XSkinVertexArray[i] = VertexCopy[VertexMap[i].OriginalIndex];
			VertexMap[VertexMap[i].OriginalIndex].ReMapIndex = i;
		}

	for (lod = 0; lod< GR_BODY_NUMBER_OF_LOD; lod++)
		{
			Count = B->SkinFaces[lod].FaceCount;
			for (i=0,T=B->SkinFaces[lod].FaceArray;
					i<Count; 
					i++,T++)
				{
					for (j=0; j<3; j++)	
						{	
							T->VtxIndex[j] = VertexMap[T->VtxIndex[j]].ReMapIndex;
						}
				}
		}

	grRam_Free(VertexMap);
	grRam_Free(VertexCopy);
	return GR_TRUE;

#if 0

	for (i=0; i<Count; i++)
		{
			for (j=0; j<Count-1; j++)
				{
					if (B->XSkinVertexArray[j].BoneIndex > B->XSkinVertexArray[j+1].BoneIndex)
						{
							grBody_XSkinVertex Swap;

							Swap= B->XSkinVertexArray[j];
							B->XSkinVertexArray[j] = B->XSkinVertexArray[j+1];
							B->XSkinVertexArray[j+1] = Swap;
							grBody_SwapVertexIndices(B,(grBody_Index)j,(grBody_Index)(j+1));
							AnyChanges = GR_TRUE;
						}
				}
			if (AnyChanges != GR_TRUE)
				{
					break;
				}
			AnyChanges = GR_FALSE;
		}
#endif
}

// @@	
static grBoolean GRCF grBody_AddSkinVertex(	grBody *B,
	const grVec3d *Vertex, 
	grFloat U, grFloat V,
	grBody_Index BoneIndex, 
	grBody_Index *Index,
	int16 nBlends,
	grBody_Index bdaOffset)
{
	grBody_Bone *Bone;
	grBody_XSkinVertex *SV;
	grBody_XSkinVertex NewSV;
	int i;
	assert( B != NULL );
	assert( Vertex != NULL );
	assert( Index != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
		
	assert( B->XSkinVertexCount+1 > 0 );
	
	NewSV.XPoint = *Vertex;
	NewSV.XU     =  U;
	NewSV.XV     =  V;
	NewSV.LevelOfDetailMask = GR_BODY_HIGHEST_LOD_MASK;
	NewSV.BoneIndex = BoneIndex;
	NewSV.nBlends = nBlends;
	NewSV.bdaOffset = bdaOffset;

	
	assert( B->BoneCount > BoneIndex );
	Bone = &(B->BoneArray[BoneIndex]);
	

	if ((B->optFlags & GR_BODY_OPTIMIZE_FLAGS_VERTS))
	{
		// see if new Vertex is already in XSkinVertexArray
		for (i=0; i<B->XSkinVertexCount; i++)
			{
				SV = &(B->XSkinVertexArray[i]);
				if (SV->BoneIndex == BoneIndex && 
					SV->nBlends == nBlends && SV->bdaOffset == bdaOffset)
					{
						if (grBody_XSkinVertexCompare(SV,&NewSV) == GR_TRUE )
							{
								*Index = (grBody_Index)i;
								return GR_TRUE;
							}
					}
			}
	}
	// new Vertex needs to be added to XSkinVertexArray
	SV = GR_RAM_REALLOC_ARRAY( B->XSkinVertexArray ,
					grBody_XSkinVertex, (B->XSkinVertexCount + 1) );
	if ( SV == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddSkinVertex.");
			return GR_FALSE;
		}
	B->XSkinVertexArray = SV;

	B->XSkinVertexArray[B->XSkinVertexCount] = NewSV;
	*Index = B->XSkinVertexCount;

	Bone->BoundingBoxMin.X = MIN(Bone->BoundingBoxMin.X,NewSV.XPoint.X);
	Bone->BoundingBoxMin.Y = MIN(Bone->BoundingBoxMin.Y,NewSV.XPoint.Y);
	Bone->BoundingBoxMin.Z = MIN(Bone->BoundingBoxMin.Z,NewSV.XPoint.Z);
	Bone->BoundingBoxMax.X = MAX(Bone->BoundingBoxMax.X,NewSV.XPoint.X);
	Bone->BoundingBoxMax.Y = MAX(Bone->BoundingBoxMax.Y,NewSV.XPoint.Y);
	Bone->BoundingBoxMax.Z = MAX(Bone->BoundingBoxMax.Z,NewSV.XPoint.Z);

	B->XSkinVertexCount ++ ;
	return GR_TRUE;
}



// @@
static grBoolean GRCF grBody_AddNormal( grBody *B, 
		const grVec3d *Normal, 
		grBody_Index BoneIndex, 
		grBody_Index *Index,
		int16 nBlends,
		grBody_Index bdaOffset )
{
	grBody_Normal *NewNormalArray;
	grBody_Normal *N;
	grVec3d NNorm;
	int i;

	assert(      B != NULL );
	assert( Normal != NULL );
	assert(  Index != NULL );	
	assert( grBody_IsValid(B) != GR_FALSE );
	
	assert( B->SkinNormalCount+1 > 0 );
	NNorm = *Normal;
	grVec3d_Normalize(&NNorm);		

	if ((B->optFlags & GR_BODY_OPTIMIZE_FLAGS_NORMALS))
	{
		// see if new normal is already in SkinNormalArray
		for (i=0, N = B->SkinNormalArray; i<B->SkinNormalCount; i++,N++)
			{
				if (N->BoneIndex == BoneIndex && 
					N->nBlends == nBlends && N->bdaOffset == bdaOffset)
					{
						if ( grVec3d_Compare( &(N->Normal),&NNorm,GR_BODY_TOLERANCE ) == GR_TRUE )
							{
								*Index = (grBody_Index)i;
								return GR_TRUE;
							}
					}
			}
	}

	//  new normal needs to be added to SkinNormalArray
	NewNormalArray = GR_RAM_REALLOC_ARRAY( B->SkinNormalArray,		
						grBody_Normal,(B->SkinNormalCount+1));
	if (NewNormalArray == NULL)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddNormal");
			return GR_FALSE;
		}
	B->SkinNormalArray = NewNormalArray;
	B->SkinNormalArray[ B->SkinNormalCount ].Normal    = NNorm;
	B->SkinNormalArray[ B->SkinNormalCount ].BoneIndex = BoneIndex;
	B->SkinNormalArray[ B->SkinNormalCount ].LevelOfDetailMask = GR_BODY_HIGHEST_LOD_MASK;
	B->SkinNormalArray[ B->SkinNormalCount ].nBlends = nBlends;
	B->SkinNormalArray[ B->SkinNormalCount ].bdaOffset = bdaOffset;

	*Index = B->SkinNormalCount;
	B->SkinNormalCount ++ ;
	return GR_TRUE;
}



static grBoolean GRCF grBody_AddToFaces( grBody *B, grBody_Triangle *F, int DetailLevel )
{
	grBody_Triangle *NewFaceArray;
	grBody_TriangleList *FL;
	
	assert( B != NULL );
	assert( F != NULL );
	assert( DetailLevel >= 0);
	assert( DetailLevel < GR_BODY_NUMBER_OF_LOD );
	assert( grBody_IsValid(B) != GR_FALSE );

	FL = &( B->SkinFaces[DetailLevel] );
	
	assert( F->MaterialIndex >= 0 );
	assert( F->MaterialIndex < B->MaterialCount );
	
	NewFaceArray = GR_RAM_REALLOC_ARRAY( FL->FaceArray, 
						grBody_Triangle,(FL->FaceCount+1) );
	if ( NewFaceArray == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddToFaces");
			return GR_FALSE;
		}

	FL->FaceArray = NewFaceArray;
	
	{
		int i;
		// insertion sort new face into FaceArray keqyed on MaterialIndex
		if ((B->optFlags & GR_BODY_OPTIMIZE_FLAGS_SORT_FACES))
		{
			grBody_Index MaterialIndex = F->MaterialIndex;
			for (i=FL->FaceCount; i>=1; i--)
				{
					if (FL->FaceArray[i-1].MaterialIndex <= MaterialIndex)
						break;
					FL->FaceArray[i] = FL->FaceArray[i-1];
				}
			FL->FaceArray[i] = *F;
		}
		else
		{
			FL->FaceArray[FL->FaceCount] = *F;
		}			
	}
	FL->FaceCount ++;

	return GR_TRUE;
}
			
GRAPI grBoolean GRCC grBody_SetOptimizeFlags(grBody* pBody, uint32 flags)
{	
	assert(pBody);

	pBody->optFlags = flags;

	return GR_TRUE;
}


static int grBody_QSortFaceCompare( const void *arg1, const void *arg2 )
{
	grBody_Triangle  *T1,*T2;
	assert( arg1 );
	assert( arg2 );

	T1 = (grBody_Triangle  *)arg1; 
	T2 = (grBody_Triangle  *)arg2;

	if (T1->MaterialIndex < T2->MaterialIndex)
		return -1;
	if (T1->MaterialIndex > T2->MaterialIndex)
		return 1;
	return 0;
}


GRAPI grBoolean GRCC grBody_Optimize(grBody *pBody)
{
	int lod;

	assert( pBody );
	
	// sort the verts by bone
	if (grBody_SortSkinVertices(pBody)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grBody_Optimize");
			return GR_FALSE;
		}
	
	//sort the polys
	for (lod = 0; lod< GR_BODY_NUMBER_OF_LOD; lod++)
		{
			grBody_TriangleList *FL;
			FL = &( pBody->SkinFaces[lod] );
			if (FL->FaceCount>0)
				qsort( &(FL->FaceArray[0]), FL->FaceCount, sizeof(FL->FaceArray[0]), grBody_QSortFaceCompare);
		}
	
	return GR_TRUE;
}

// ---------------------------------------------------------------------------------------
// grBody_AddBlendData - add blend elements to a grBody
// ---------------------------------------------------------------------------------------
// params							use
// ---------------------------------------------------------------------------------------
// pBody							pointer to grBody structure
// weight							weighting used for blend
// pLoc								pointer to position vector of blend in bone B's local frame
// pNormal							pointer to normal vector of blend in bone B's local frame
// boneIndex					index of bone B in grBody structure
// ---------------------------------------------------------------------------------------
// @@

grBoolean grBody_AddBlendData(grBody* pBody, grFloat weight, const grVec3d* pLoc, const grVec3d* pNormal, int boneIndex)
{
	grBody_BlendData* pBD;

	assert(pBody != NULL);
	assert(pLoc != NULL);
	assert(pNormal != NULL);
	assert(boneIndex >= 0 && boneIndex < pBody->BoneCount);

	pBD = (grBody_BlendData*)grRam_Realloc(pBody->blendDataArray, (pBody->blendDataCount + 1) * sizeof(grBody_BlendData));
	if(pBD == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddBlendData");
		return GR_FALSE;
	}
	pBody->blendDataArray = pBD;

	// reusing pBD here - ouchie ...
	pBD = &pBody->blendDataArray[pBody->blendDataCount];

	pBD->weight = weight;
	pBD->XPoint = *pLoc;
	pBD->Normal = *pNormal;
	pBD->boneIndex = (grBody_Index)boneIndex;

	pBody->blendDataCount ++;

	return GR_TRUE;
}

GRAPI int16 GRCC grBody_GetBlendDataCount(const grBody* pBody)
{
	assert(pBody != NULL);

	return pBody->blendDataCount;
}

// @@
grBoolean grBody_AddBlendFace(grBody* pBody,
	const grVec3d* pVert1, const grVec3d* pNormal1, 
	grFloat u1, grFloat v1, int boneIndex1, int nBlends1, int bdaOffset1,

	const grVec3d* pVert2, const grVec3d* pNormal2, 
	grFloat u2, grFloat v2, int boneIndex2, int nBlends2, int bdaOffset2,

	const grVec3d* pVert3, const grVec3d* pNormal3, 
	grFloat u3, grFloat v3, int boneIndex3, int nBlends3, int bdaOffset3,

	int materialIndex)
{
	grBody_Triangle F;

	assert(pBody != NULL);
	assert(grBody_IsValid(pBody) != GR_FALSE);

	assert(pVert1 != NULL);
	assert(pNormal1 != NULL);
	assert(boneIndex1 >= 0);
	assert(boneIndex1 < pBody->BoneCount);
	assert((nBlends1 + bdaOffset1) >= 0);
	assert((nBlends1 + bdaOffset1) <= pBody->blendDataCount);
	assert(bdaOffset1 >= 0);
	assert(bdaOffset1 <= pBody->blendDataCount);

	assert(pVert2 != NULL);
	assert(pNormal2 != NULL);
	assert(boneIndex2 >= 0);
	assert(boneIndex2 < pBody->BoneCount);
	assert((nBlends2 + bdaOffset2) >= 0);
	assert((nBlends2 + bdaOffset2) <= pBody->blendDataCount);
	assert(bdaOffset2 >= 0);
	assert(bdaOffset2 <= pBody->blendDataCount);

	assert(pVert3 != NULL);
	assert(pNormal3 != NULL);
	assert(boneIndex3 >= 0);
	assert(boneIndex3 < pBody->BoneCount);
	assert((nBlends3 + bdaOffset3) >= 0);
	assert((nBlends3 + bdaOffset3) <= pBody->blendDataCount);
	assert(bdaOffset3 >= 0);
	assert(bdaOffset3 <= pBody->blendDataCount);

	assert(materialIndex >= 0);
	assert(materialIndex < pBody->MaterialCount);

	// add verts

	if (grBody_AddSkinVertex(pBody, pVert1, u1, v1, (grBody_Index)boneIndex1, &(F.VtxIndex[0]),
		(int16)nBlends1, (grBody_Index)bdaOffset1) == GR_FALSE)
	{
		// error already recorded
		return GR_FALSE;
	}
	if (grBody_AddSkinVertex(pBody, pVert2, u2, v2, (grBody_Index)boneIndex2, &(F.VtxIndex[1]),
		(int16)nBlends2, (grBody_Index)bdaOffset2) == GR_FALSE)
	{
		// error already recorded
		return GR_FALSE;
	}
	if (grBody_AddSkinVertex(pBody, pVert3, u3, v3, (grBody_Index)boneIndex3, &(F.VtxIndex[2]),
		(int16)nBlends3, (grBody_Index)bdaOffset3) == GR_FALSE)
	{
		// error already recorded
		return GR_FALSE;
	}

	// add normals

	if (grBody_AddNormal(pBody, pNormal1, (grBody_Index)boneIndex1, &(F.NormalIndex[0]), 
		(int16)nBlends1, (grBody_Index)bdaOffset1) == GR_FALSE)
	{	
		// error already recorded
		return GR_FALSE;
	}
	if (grBody_AddNormal(pBody, pNormal2, (grBody_Index)boneIndex2, &(F.NormalIndex[1]), 
		(int16)nBlends2, (grBody_Index)bdaOffset2) == GR_FALSE)
	{	
		// error already recorded
		return GR_FALSE;
	}
	if (grBody_AddNormal(pBody, pNormal3, (grBody_Index)boneIndex3, &(F.NormalIndex[2]), 
		(int16)nBlends3, (grBody_Index)bdaOffset3) == GR_FALSE)
	{	
		// error already recorded
		return GR_FALSE;
	}

	// add face

	F.MaterialIndex = (grBody_Index)materialIndex;
	if (grBody_AddToFaces(pBody, &F, GR_BODY_HIGHEST_LOD ) == GR_FALSE)
	{	
		// error already recorded
		return GR_FALSE;
	}

	if ((pBody->optFlags & GR_BODY_OPTIMIZE_FLAGS_SORT_VERTS))
		if (grBody_SortSkinVertices(pBody)==GR_FALSE)
			{
				//ignore.
			}

	return GR_TRUE;
}

grBoolean grBody_CompareBlendData(const grBody_BlendData* pBD, const grVec3d* pV, const grVec3d* pN, grFloat weight, grBody_Index boneIndex)
{
	assert( pBD != NULL );
	assert( pV != NULL );

	if(grVec3d_Compare(&pBD->XPoint, pV, GR_BODY_TOLERANCE) == GR_FALSE)
		return(GR_FALSE);

	if(grVec3d_Compare(&pBD->Normal, pN, GR_BODY_TOLERANCE) == GR_FALSE)
		return(GR_FALSE);

	if(fabs(pBD->weight - weight) > GR_BODY_TOLERANCE)
		return(GR_FALSE);

	if(pBD->boneIndex != boneIndex)
		return(GR_FALSE);

#pragma message ("(steve)FIX ME:")
#pragma message ("Steve:  can you also make sure that the optimization flags are handled right")
#pragma message ("for the blended poly additions?  Thanks")

	return(GR_TRUE);
}

grBoolean grBody_FindBlendData(const grBody* pBody, 
							   const grVec3d* pVerts, 
							   const grVec3d* pNormals,
							   const grFloat* pWeights, 
							   const int* pBoneIndexes, 
							   int NumVerts, 
							   int* pBlendDataOffset) // return here if search successful
{
	int i, j;

	assert(pBody != NULL);
	assert(pVerts != NULL);
	assert(pNormals != NULL);
	assert(pWeights != NULL);
	assert(pBoneIndexes != NULL);
	assert(NumVerts > 1); // make this search useful
	assert(pBlendDataOffset != NULL);

	for(i=0;i<(pBody->blendDataCount - NumVerts);i++)
	{
		if(grBody_CompareBlendData(pBody->blendDataArray + i, pVerts + 0, pNormals + 0, pWeights[0], (grBody_Index)pBoneIndexes[0]) != GR_FALSE)
		{
			for(j=1;j<NumVerts;j++)
			{
				if(grBody_CompareBlendData(pBody->blendDataArray + i + j, pVerts + j, pNormals + j, pWeights[j], (grBody_Index)pBoneIndexes[j]) == GR_FALSE)
				{
					break;
				}
			}
			if(j == NumVerts)
			{
				// found them
				*pBlendDataOffset = i;
				return(GR_TRUE);
			}
		}
	}

	return(GR_FALSE);
}

// ---------------------------------------------------------------------------------------
// grBody_AddBlendDatArrayWithRedundancyCheck - add some blend elements to a grBody,
// checking to see if the same bone(s) are referenced more than once.
// ---------------------------------------------------------------------------------------
// params							use
// ---------------------------------------------------------------------------------------
// pBody								pointer to grBody structure
// pWeights							array of weightings
// pVerts								array of vertices
// pNormals							array of normals
// pBoneIndices					array of bone indices
// num									number of blendings
// ---------------------------------------------------------------------------------------
// @@

grBoolean grBody_AddBlendDataArrayWithRedundancyCheck(grBody* pBody,
	const grFloat* pWeights, const grVec3d* pVerts, const grVec3d* pNormals,
	const int* pBoneIndices, int num, int* pNumActualBlends)
{
	int i, currBoneIndex, j;
	float totalWeight;
	grBoolean* pVisitedIndices;
	grBoolean found;

	assert(pBody != NULL);
	assert(pWeights != NULL);
	assert(pVerts != NULL);
	assert(pNormals != NULL);
	assert(pBoneIndices != NULL);
	assert(num > 0);
	assert(pNumActualBlends != NULL);

	pVisitedIndices = (grBoolean*)grRam_Allocate(num * sizeof(grBoolean));
	assert(pVisitedIndices != NULL);

	for (i = 0; i < num; i ++)
		pVisitedIndices[i] = GR_FALSE;

	*pNumActualBlends = 0;

	for (i = 0; i < num; i ++)
	{
		if (pVisitedIndices[i] == GR_TRUE)
			continue;

		currBoneIndex = pBoneIndices[i];
		totalWeight = 0.0f;

		for (j = i; j < num; j ++)
		{
			if (pVisitedIndices[j] == GR_FALSE && pBoneIndices[j] == currBoneIndex)
			{
				pVisitedIndices[j] = GR_TRUE;
				totalWeight += pWeights[j];
			}
		}

		// search for an already existing blend data with these characteristics

		found = GR_FALSE;

		for (j = 0; j < pBody->blendDataCount; j ++)
		{
			if (grBody_CompareBlendData(&pBody->blendDataArray[j], 
				&pVerts[i], &pNormals[i], totalWeight, (grBody_Index)currBoneIndex) == GR_TRUE)
			{
				found = GR_TRUE;
				break;
			}
		}

		if (found == GR_FALSE)
		{
			if (GR_FALSE == grBody_AddBlendData(pBody, totalWeight, 
				&pVerts[i], &pNormals[i], currBoneIndex))
			{
				return(GR_FALSE);
			}

			*pNumActualBlends ++;
		}
	}

	grRam_Free(pVisitedIndices);

	return GR_TRUE;
}

//#define USE_STEVE

#ifndef USE_STEVE

GRAPI grBoolean GRCC grBody_AddFaceWeightedVerts(	grBody* pBody,
	const grVec3d* pVerts1, const grVec3d* pNormals1, 
		grFloat u1, grFloat v1, const int* pBoneIndexes1, 
		const grFloat* pVertWeights1, int NumVerts1,
	const grVec3d* pVerts2, const grVec3d* pNormals2, 
		grFloat u2, grFloat v2, const int* pBoneIndexes2, 
		const grFloat* pVertWeights2, int NumVerts2,
	const grVec3d* pVerts3, const grVec3d* pNormals3, 
		grFloat u3, grFloat v3, const int* pBoneIndexes3, 
		const grFloat* pVertWeights3, int NumVerts3,
	int materialIndex)
{
	int bdaOffset1, bdaOffset2, bdaOffset3;
	grBoolean bResult;
	int nBlends1, nBlends2, nBlends3;
#ifdef _DEBUG
	int i;
#endif

	assert(pBody != NULL);
	assert(grBody_IsValid(pBody) != GR_FALSE);

	assert(pVerts1 != NULL);
	assert(pNormals1 != NULL);
	assert(pVertWeights1 != NULL);
	assert(pBoneIndexes1 != NULL);
	assert(NumVerts1 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts1;i++)
	{
		assert(pBoneIndexes1[i] >= 0);
		assert(pBoneIndexes1[i] < pBody->BoneCount);
	}
#endif

	assert(pVerts2 != NULL);
	assert(pNormals2 != NULL);
	assert(pVertWeights2 != NULL);
	assert(pBoneIndexes2 != NULL);
	assert(NumVerts2 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts2;i++)
	{
		assert(pBoneIndexes2[i] >= 0);
		assert(pBoneIndexes2[i] < pBody->BoneCount);
	}
#endif

	assert(pVerts3 != NULL);
	assert(pNormals3 != NULL);
	assert(pVertWeights3 != NULL);
	assert(pBoneIndexes3 != NULL);
	assert(NumVerts3 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts3;i++)
	{
		assert(pBoneIndexes3[i] >= 0);
		assert(pBoneIndexes3[i] < pBody->BoneCount);
	}
#endif

	assert(materialIndex >= 0);
	assert(materialIndex < pBody->MaterialCount);

	if (NumVerts1 > 1)
	{
		bdaOffset1 = pBody->blendDataCount;

		grBody_AddBlendDataArrayWithRedundancyCheck(pBody,
			pVertWeights1, pVerts1, pNormals1, pBoneIndexes1, NumVerts1, &nBlends1);
	}
	else
	{
		nBlends1 = 0;
		bdaOffset1 = 0;
	}

	if (NumVerts2 > 1)
	{
		bdaOffset2 = pBody->blendDataCount;

		grBody_AddBlendDataArrayWithRedundancyCheck(pBody,
			pVertWeights2, pVerts2, pNormals2, pBoneIndexes2, NumVerts2, &nBlends2);
	}
	else
	{
		nBlends2 = 0;
		bdaOffset2 = 0;
	}

	if (NumVerts3 > 1)
	{
		bdaOffset3 = pBody->blendDataCount;

		grBody_AddBlendDataArrayWithRedundancyCheck(pBody,
			pVertWeights3, pVerts3, pNormals3, pBoneIndexes3, NumVerts3, &nBlends3);
	}
	else
	{
		nBlends3 = 0;
		bdaOffset3 = 0;
	}

	bResult = grBody_AddBlendFace(pBody, 
		pVerts1, pNormals1, u1, v1, *pBoneIndexes1, nBlends1, bdaOffset1,
		pVerts2, pNormals2, u2, v2, *pBoneIndexes2, nBlends2, bdaOffset2,
		pVerts3, pNormals3, u3, v3, *pBoneIndexes3, nBlends3, bdaOffset3,
		materialIndex);

	return(bResult);
}

/////////////////////////////////////////////////////////////////////////////////
////STEVE'S ORIGINAL CODE FOLLOWS ---------------->>>>>>>>>>>>>  ////////////////
/////////////////////////////////////////////////////////////////////////////////

#else // USE_STEVE

GRAPI grBoolean GRCC grBody_AddFaceWeightedVerts(	grBody* pBody,
	const grVec3d* pVerts1, const grVec3d* pNormals1, 
		grFloat u1, grFloat v1, const int* pBoneIndexes1, 
		const grFloat* pVertWeights1, int NumVerts1,
	const grVec3d* pVerts2, const grVec3d* pNormals2, 
		grFloat u2, grFloat v2, const int* pBoneIndexes2, 
		const grFloat* pVertWeights2, int NumVerts2,
	const grVec3d* pVerts3, const grVec3d* pNormals3, 
		grFloat u3, grFloat v3, const int* pBoneIndexes3, 
		const grFloat* pVertWeights3, int NumVerts3,
	int materialIndex)
{
	int bdaOffset1, bdaOffset2, bdaOffset3;
	grBoolean bResult;
	int i;
	int nBlends1, nBlends2, nBlends3;

	assert(pBody != NULL);
	assert(grBody_IsValid(pBody) != GR_FALSE);

	assert(pVerts1 != NULL);
	assert(pNormals1 != NULL);
	assert(pVertWeights1 != NULL);
	assert(pBoneIndexes1 != NULL);
	assert(NumVerts1 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts1;i++)
	{
		assert(pBoneIndexes1[i] >= 0);
		assert(pBoneIndexes1[i] < pBody->BoneCount);
	}
#endif

	assert(pVerts2 != NULL);
	assert(pNormals2 != NULL);
	assert(pVertWeights2 != NULL);
	assert(pBoneIndexes2 != NULL);
	assert(NumVerts2 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts2;i++)
	{
		assert(pBoneIndexes2[i] >= 0);
		assert(pBoneIndexes2[i] < pBody->BoneCount);
	}
#endif

	assert(pVerts3 != NULL);
	assert(pNormals3 != NULL);
	assert(pVertWeights3 != NULL);
	assert(pBoneIndexes3 != NULL);
	assert(NumVerts3 > 0);
#ifdef _DEBUG
	for(i=0;i<NumVerts3;i++)
	{
		assert(pBoneIndexes3[i] >= 0);
		assert(pBoneIndexes3[i] < pBody->BoneCount);
	}
#endif

	assert(materialIndex >= 0);
	assert(materialIndex < pBody->MaterialCount);

	// Search for already existing data in blendDataArray

	if(NumVerts1 > 1)
	{
		nBlends1 = NumVerts1;

		if(GR_FALSE == grBody_FindBlendData(pBody, 
											pVerts1, 
											pNormals1,
											pVertWeights1, 
											pBoneIndexes1, 
											NumVerts1, 
											&bdaOffset1) )
		{
			// didn't find them
			bdaOffset1 = pBody->blendDataCount;

			for(i=0;i<NumVerts1;i++)
			{
				if(GR_FALSE == grBody_AddBlendData(pBody, pVertWeights1[i], pVerts1 + i, pNormals1 + i, pBoneIndexes1[i]))
					return(GR_FALSE);
			}
		}
	}
	else
	{
		nBlends1 = 0;
		bdaOffset1 = 0;
	}

	if(NumVerts2 > 1)
	{
		nBlends2 = NumVerts2;

		if(GR_FALSE == grBody_FindBlendData(pBody, 
											pVerts2, 
											pNormals2,
											pVertWeights2, 
											pBoneIndexes2, 
											NumVerts2, 
											&bdaOffset2) )
		{
			// didn't find them
			bdaOffset2 = pBody->blendDataCount;

			for(i=0;i<NumVerts2;i++)
			{
				if(GR_FALSE == grBody_AddBlendData(pBody, pVertWeights2[i], pVerts2 + i, pNormals2 + i, pBoneIndexes2[i]))
					return(GR_FALSE);
			}
		}
	}
	else
	{
		nBlends2 = 0;
		bdaOffset2 = 0;
	}

	if(NumVerts3 > 1)
	{
		nBlends3 = NumVerts3;

		if(GR_FALSE == grBody_FindBlendData(pBody, 
											pVerts3, 
											pNormals3,
											pVertWeights3, 
											pBoneIndexes3, 
											NumVerts3, 
											&bdaOffset3) )
		{
			// didn't find them
			bdaOffset3 = pBody->blendDataCount;

			for(i=0;i<NumVerts3;i++)
			{
				if(GR_FALSE == grBody_AddBlendData(pBody, pVertWeights3[i], pVerts3 + i, pNormals3 + i, pBoneIndexes3[i]))
					return(GR_FALSE);
			}
		}
	}
	else
	{
		nBlends3 = 0;
		bdaOffset3 = 0;
	}

	bResult = grBody_AddBlendFace(pBody, 
		pVerts1, pNormals1, u1, v1, *pBoneIndexes1, nBlends1, bdaOffset1,
		pVerts2, pNormals2, u2, v2, *pBoneIndexes2, nBlends2, bdaOffset2,
		pVerts3, pNormals3, u3, v3, *pBoneIndexes3, nBlends3, bdaOffset3,
		materialIndex);

	return(bResult);
}

#endif // USE_STEVE

GRAPI grBoolean GRCC grBody_AddFace(	grBody *B,
	const grVec3d *Vertex1, const grVec3d *Normal1, 
		grFloat U1, grFloat V1, int BoneIndex1,
	const grVec3d *Vertex2, const grVec3d *Normal2, 
		grFloat U2, grFloat V2, int BoneIndex2,
	const grVec3d *Vertex3, const grVec3d *Normal3, 
		grFloat U3, grFloat V3, int BoneIndex3,
	int MaterialIndex)
{
	grBody_Triangle F;
	
	assert( B != NULL );
	assert( Vertex1 != NULL );
	assert( Normal1 != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );

	assert( BoneIndex1 >= 0 );
	assert( BoneIndex1 < B->BoneCount );

	assert( Vertex2 != NULL );
	assert( Normal2 != NULL );
	assert( BoneIndex2 >= 0 );
	assert( BoneIndex2 < B->BoneCount );

	assert( Vertex3 != NULL );
	assert( Normal3 != NULL );
	assert( BoneIndex3 >= 0 );
	assert( BoneIndex3 < B->BoneCount );

	assert( MaterialIndex >= 0 );
	assert(	MaterialIndex < B->MaterialCount );

	if (grBody_AddSkinVertex(B,Vertex1,U1,V1,(grBody_Index)BoneIndex1,&(F.VtxIndex[0]), 0, 0)==GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}
	if (grBody_AddSkinVertex(B,Vertex2,U2,V2,(grBody_Index)BoneIndex2,&(F.VtxIndex[1]), 0, 0)==GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}
	if (grBody_AddSkinVertex(B,Vertex3,U3,V3,(grBody_Index)BoneIndex3,&(F.VtxIndex[2]), 0, 0)==GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}

	if (grBody_AddNormal( B, Normal1, (grBody_Index)BoneIndex1, &(F.NormalIndex[0]), 0, 0) == GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}
	if (grBody_AddNormal( B, Normal2, (grBody_Index)BoneIndex2, &(F.NormalIndex[1]), 0, 0) == GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}
	if (grBody_AddNormal( B, Normal3, (grBody_Index)BoneIndex3, &(F.NormalIndex[2]), 0, 0) == GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}

	F.MaterialIndex = (grBody_Index)MaterialIndex;
	if (grBody_AddToFaces( B, &F, GR_BODY_HIGHEST_LOD ) == GR_FALSE)
		{	// error already recorded
			return GR_FALSE;
		}

	if ((B->optFlags & GR_BODY_OPTIMIZE_FLAGS_SORT_VERTS))
		if (grBody_SortSkinVertices(B)==GR_FALSE)
			{
				//ignore
			}
		
	return GR_TRUE;
			
}


GRAPI grBoolean GRCC grBody_AddMaterial( grBody *B, 
	const char *MaterialName, 
	grMaterialSpec *Bitmap,
	grFloat Red, grFloat Green, grFloat Blue,
	grUVMapper pMapper,
	int *MaterialIndex)
{
	int FoundIndex;
	grBody_Material *NewMaterial;
	assert( B != NULL );
	assert( MaterialIndex != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	assert( B->MaterialCount >= 0 );

	if (MaterialName == NULL)
		{
			grErrorLog_Add(-1,"grBody_AddMaterial: name can not be NULL.");
			return GR_FALSE;
		}
	if (MaterialName[0] == 0)
		{
			grErrorLog_Add(-1,"grBody_AddMaterial: name must have > 0 length.");
			return GR_FALSE;
		}
	if (grStrBlock_FindString(B->MaterialNames, MaterialName, &FoundIndex) == GR_TRUE)
		{
			grErrorLog_AddString(-1,"grBody_AddMaterial: name already used-", MaterialName);
			return GR_FALSE;
		}
	
	
	NewMaterial = GR_RAM_REALLOC_ARRAY( B->MaterialArray, grBody_Material,(B->MaterialCount+1) );
	if ( NewMaterial == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddMaterial.");
			return GR_FALSE;
		}
	
	
	B->MaterialArray = NewMaterial;
	if (grStrBlock_Append(&(B->MaterialNames),MaterialName) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddMaterial.");
			return GR_FALSE;
		}

	{
		grBody_Material *M = &(B->MaterialArray[B->MaterialCount]);
		M->MatSpec = Bitmap;
		if (Bitmap != NULL)
			grMaterialSpec_CreateRef(Bitmap);
		M->Red    = Red;
		M->Green  = Green;
		M->Blue   = Blue;
		M->Mapper = pMapper;

	}
	*MaterialIndex = B->MaterialCount; 
	B->MaterialCount ++;
	return GR_TRUE;
}
			
GRAPI grBoolean GRCC grBody_GetMaterial(const grBody *B, int MaterialIndex,
										const char **MaterialName,
										grMaterialSpec **Bitmap, grFloat *Red, grFloat *Green, grFloat *Blue,
										grUVMapper * pMapper)
{
	assert( B      != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	assert( Red    != NULL );
	assert( Green  != NULL );
	assert( Blue   != NULL );
	assert( Bitmap != NULL );
	assert(pMapper != NULL);
	assert( MaterialIndex >= 0 );
	assert( MaterialIndex < B->MaterialCount );
	assert( MaterialName != NULL );
	*MaterialName      = grStrBlock_GetString(B->MaterialNames,MaterialIndex);

	{
		grBody_Material *M = &(B->MaterialArray[MaterialIndex]);
		*Bitmap = M->MatSpec;
		*Red    = M->Red;
		*Green  = M->Green;
		*Blue   = M->Blue;
		*pMapper = M->Mapper;
	}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grBody_SetMaterial(grBody *B, int MaterialIndex,
										grMaterialSpec *Material,  grFloat Red,  grFloat Green,  grFloat Blue,
										grUVMapper Mapper)
{
	assert( grBody_IsValid(B) != GR_FALSE );
	assert( MaterialIndex >= 0 );
	assert( MaterialIndex < B->MaterialCount );
	{
		grBody_Material *M = &(B->MaterialArray[MaterialIndex]);
		M->MatSpec= Material;

		M->Red    = Red;
		M->Green  = Green;
		M->Blue   = Blue;
		M->Mapper = Mapper;
	}
	return GR_TRUE;
}




GRAPI grBoolean GRCC grBody_AddBone( grBody *B, 
	int ParentBoneIndex,
	const char *BoneName, 
	const grXForm3d *AttachmentMatrix,
	int *BoneIndex)
{
	grBody_Bone *NewBones;
	assert( B != NULL );
	assert( BoneName != NULL );
	assert( BoneIndex != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );

	assert( ParentBoneIndex < B->BoneCount );
	assert( ( ParentBoneIndex >= 0)  || (ParentBoneIndex == GR_BODY_NO_PARENT_BONE));
	assert( B->BoneCount >= 0 );
	
	NewBones = GR_RAM_REALLOC_ARRAY( B->BoneArray, 
						grBody_Bone, (B->BoneCount+1) );
	if ( NewBones == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBody_AddBone.");
			return GR_FALSE;
		}
	
	B->BoneArray = NewBones;
	if (grStrBlock_Append(&(B->BoneNames),BoneName) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grBody_AddBone.");
			return GR_FALSE;
		}
	
	{
		grBody_Bone *Bone = &(B->BoneArray[B->BoneCount]);
		grVec3d_Set(&(Bone->BoundingBoxMin),
			GR_BODY_REALLY_BIG_NUMBER,GR_BODY_REALLY_BIG_NUMBER,GR_BODY_REALLY_BIG_NUMBER);
		grVec3d_Set(&(Bone->BoundingBoxMax),
			-GR_BODY_REALLY_BIG_NUMBER,-GR_BODY_REALLY_BIG_NUMBER,-GR_BODY_REALLY_BIG_NUMBER);
		Bone->AttachmentMatrix = *AttachmentMatrix;
		Bone->ParentBoneIndex = (grBody_Index)ParentBoneIndex;
	}
	*BoneIndex = B->BoneCount;
	B->BoneCount++;
	return GR_TRUE;
}



GRAPI grBoolean GRCC grBody_ComputeLevelsOfDetail( grBody *B ,int Levels)
{
	assert( B != NULL);
	assert( Levels >= 0 );
	assert( Levels < GR_BODY_NUMBER_OF_LOD );
	assert( grBody_IsValid(B) != GR_FALSE );
	#pragma message ("LOD code goes here:")
	B->LevelsOfDetail = GR_BODY_HIGHEST_LOD_MASK; // Levels
	Levels;
	return GR_TRUE;
}	



#define GR_BODY_GEOMETRY_NAME "Geometry"
#define GR_BODY_BITMAP_DIRECTORY_NAME "Bitmaps"

#define GR_BODY_FILE_TYPE 0x5E444F42     // 'BODY'
#define GR_BODY_FILE_VERSION 0x00F2		// Restrict version to 16 bits




static grBoolean GRCF grBody_ReadGeometry(grBody *B, grVFile *pFile)
{
	uint32 u;
	int i;

	assert( B != NULL );
	assert( pFile != NULL );
	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry: Failed to read header.");	return GR_FALSE; }
	if (u!=GR_BODY_FILE_TYPE)
		{	grErrorLog_Add( GR_ERR_FILEIO_FORMAT , "grBody_ReadGeometry: bad or wrong header");  return GR_FALSE; }


	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry: Failed to version.");	return GR_FALSE; }
	if (u!=GR_BODY_FILE_VERSION)
		{	grErrorLog_Add( GR_ERR_FILEIO_VERSION , "grBody_ReadGeometry: old or wrong version");   return GR_FALSE; }
	

	if(grVFile_Read(pFile, &(B->BoundingBoxMin), sizeof(B->BoundingBoxMin)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if(grVFile_Read(pFile, &(B->BoundingBoxMax), sizeof(B->BoundingBoxMax)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if(grVFile_Read(pFile, &(B->XSkinVertexCount), sizeof(B->XSkinVertexCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if (B->XSkinVertexCount>0)
		{
			u = sizeof(grBody_XSkinVertex) * B->XSkinVertexCount;
			B->XSkinVertexArray = (grBody_XSkinVertex *)grRam_Allocate(u);
			if (B->XSkinVertexArray == NULL)
				{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate vertex array.");   return GR_FALSE;  }
			if(grVFile_Read(pFile, B->XSkinVertexArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry: skin vertex array");	 return GR_FALSE; }
		}

	if(grVFile_Read(pFile, &(B->SkinNormalCount), sizeof(B->SkinNormalCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if (B->SkinNormalCount>0)
		{
			u = sizeof(grBody_Normal) * B->SkinNormalCount;
			B->SkinNormalArray = (grBody_Normal *)grRam_Allocate(u);
			if (B->SkinNormalArray == NULL)
				{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate normal array.");   return GR_FALSE;  }
			if(grVFile_Read(pFile, B->SkinNormalArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry: skin normal array.");	return GR_FALSE; }
		}

	if(grVFile_Read(pFile, &(B->blendDataCount), sizeof(B->blendDataCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if (B->blendDataCount>0)
		{
			u = sizeof(grBody_BlendData) * B->blendDataCount;
			B->blendDataArray = (grBody_BlendData *)grRam_Allocate(u);
			if (B->blendDataArray == NULL)
				{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate blend array.");   return GR_FALSE;  }
			if(grVFile_Read(pFile, B->blendDataArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	return GR_FALSE; }
		}

	if(grVFile_Read(pFile, &(B->BoneCount), sizeof(B->BoneCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");  return GR_FALSE; }

	if (B->BoneCount>0)
		{
			u = sizeof(grBody_Bone) * B->BoneCount;
			B->BoneArray = (grBody_Bone *)grRam_Allocate(u);
			if (B->BoneArray == NULL)
				{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate bone array.");   return GR_FALSE;  }
			if(grVFile_Read(pFile, B->BoneArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");  return GR_FALSE; }
		}

	B->BoneNames = grStrBlock_CreateFromFile(pFile);
	if (B->BoneNames==NULL)
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_ReadGeometry."); 	 return GR_FALSE; }
	
	if(grVFile_Read(pFile, &(B->MaterialCount), sizeof(B->MaterialCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if (B->MaterialCount > 0)
	{
		// reserve mem for B->MaterialArray as per normal
		u = sizeof(grBody_Material) * B->MaterialCount;
		B->MaterialArray = (grBody_Material *)grRam_Allocate(u);
		if (B->MaterialArray == NULL)
			{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate material array");   return GR_FALSE;  }

		if(grVFile_Read(pFile, B->MaterialArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry: material array");	 return GR_FALSE; }
	}

	#if 1	// <>
	// CB added this because it seems the Bitmap pointer is
	//	read in with the Material array, and is later used as a boolean
	//	for "should this material have a texture"
	for(u=0;u<(uint32)B->MaterialCount;u++)
	{
		if ( B->MaterialArray[u].MatSpec )
			B->MaterialArray[u].MatSpec = (grMaterialSpec *)1;
	}
	#endif
			
	B->MaterialNames = grStrBlock_CreateFromFile(pFile);
	if ( B->MaterialNames == NULL )
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grBody_ReadGeometry."); 	 return GR_FALSE; }

	if(grVFile_Read(pFile, &(B->LevelsOfDetail), sizeof(B->LevelsOfDetail)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }

	if (B->LevelsOfDetail > GR_BODY_NUMBER_OF_LOD)
		{	grErrorLog_Add( GR_ERR_FILEIO_FORMAT , "grBody_ReadGeometry.");	 return GR_FALSE; }

	for (i=0; i<B->LevelsOfDetail; i++)
		{
			if(grVFile_Read(pFile, &(u), sizeof(u)) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }
			B->SkinFaces[i].FaceCount = (grBody_Index)u;
			
			if (u>0)
				{
					u = sizeof(grBody_Triangle) * u;
					B->SkinFaces[i].FaceArray = (grBody_Triangle *)grRam_Allocate(u);
					if (B->SkinFaces[i].FaceArray == NULL)
						{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grBody_ReadGeometry: Failed to allocate face array.");   return GR_FALSE;  }
					if(grVFile_Read(pFile, B->SkinFaces[i].FaceArray, u) == GR_FALSE)
						{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_ReadGeometry.");	 return GR_FALSE; }
				}
		}

	assert( grBody_IsValid(B) != GR_FALSE );
	return GR_TRUE;
}

GRAPI grBody *GRCC grBody_CreateFromFile(grVFile *pFile)
{
	grBody  *B = NULL;
	int i;

	grVFile *VFile = NULL;
	grVFile *SubFile = NULL;
	grVFile *BitmapDirectory = NULL;
	
	assert( pFile != NULL );

	SubFile = NULL;
	BitmapDirectory = NULL;

	VFile = grVFile_OpenNewSystem(pFile,GR_VFILE_TYPE_VIRTUAL, NULL, 
									NULL, GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_READONLY);
	if (VFile == NULL)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_CreateFromFile: Failed to open subsystem.");
		goto CreateError;
	}
	
	SubFile = grVFile_Open(VFile,GR_BODY_GEOMETRY_NAME,GR_VFILE_OPEN_READONLY);
	if (SubFile == NULL)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_CreateFromFile: Failed to open geometry subfile.");
		goto CreateError;
	}

	B = grBody_CreateNull();
	if (B==NULL)
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_CreateFromFile: Failed to create empty body.");
		goto CreateError;
	}

	{
		grVFile * LZFS;

		LZFS =  grVFile_OpenNewSystem(SubFile,GR_VFILE_TYPE_LZ, NULL, NULL,GR_VFILE_OPEN_READONLY);
		if ( ! LZFS )
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_CreateFromFile: Failed to open compressed subfile.");
			goto CreateError;
		}

		if ( ! grBody_ReadGeometry(B,LZFS) )
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_CreateFromFile: Failed to read body geometry.");
			goto CreateError;
		}

		if ( ! grVFile_Close(LZFS) )
		{
			grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_CreateFromFile: Failed to close compressed subfile.");
			goto CreateError;
		}
	}

	if (!grVFile_Close(SubFile))
	{
		grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_CreateFromFile: Failed to close geometry subfile.");
		goto CreateError;
	}

	BitmapDirectory = grVFile_Open(VFile,GR_BODY_BITMAP_DIRECTORY_NAME, 
									GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_READONLY);
	if (BitmapDirectory == NULL)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grBody_CreateFromFile: Failed to open bitmap subdirectory.");
		goto CreateError;
	}
	
	for (i=0; i<B->MaterialCount; i++)
	{
		grBody_Material *M;
		M = &(B->MaterialArray[i]);

		if (M->MatSpec != NULL)
		{
			char FName[1000];
			sprintf(FName,"%d",i);
			
			M->MatSpec = NULL;

			SubFile = grVFile_Open(BitmapDirectory,FName,GR_VFILE_OPEN_READONLY);
			if (SubFile == NULL)
			{
				grErrorLog_AddString( GR_ERR_FILEIO_READ , "grBody_CreateFromFile: Failed to open bitmap subfile:",FName);
				goto CreateError;
			}

			M->MatSpec = grMaterialSpec_Create(grResourceMgr_GetEngine(grResourceMgr_GetSingleton()), grResourceMgr_GetSingleton());
			if (M->MatSpec == NULL)
			{
				grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE , "grBody_CreateFromFile: Failed to read bitmap:",FName);
				goto CreateError;
			}
			grMaterialSpec_AddLayerFromFile(M->MatSpec, 0, SubFile, GR_TRUE, 255);
			if (!grVFile_Close(SubFile))
			{
				grErrorLog_AddString( GR_ERR_FILEIO_CLOSE , "grBody_CreateFromFile: Failed to close bitmap subfile:",FName);
				goto CreateError;
			}
		}
	}
	if (!grVFile_Close(BitmapDirectory))
	{
		grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_CreateFromFile: Failed to close bitmap directory.");
		goto CreateError;
	}
	if (!grVFile_Close(VFile))
	{
		grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_CreateFromFile: Failed to close body subsystem.");
		goto CreateError;
	}
	return B;

CreateError:
	grBody_DestroyPossiblyIncompleteBody(&B);
	if (SubFile != NULL)
		grVFile_Close(SubFile);
	if (BitmapDirectory != NULL)
		grVFile_Close(BitmapDirectory);
	if (VFile != NULL)
		grVFile_Close(VFile);
	return NULL;
}



GRAPI grBoolean GRCC grBody_WriteGeometry(const grBody *B,grVFile *pFile)
{
	uint32 u;
	int i;

	assert( B != NULL );
	assert( pFile != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );

	// Write the format flag
	u = GR_BODY_FILE_TYPE;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	// Write the version
	u = GR_BODY_FILE_VERSION;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
	
	if(grVFile_Write(pFile, &(B->BoundingBoxMin), sizeof(B->BoundingBoxMin)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if(grVFile_Write(pFile, &(B->BoundingBoxMax), sizeof(B->BoundingBoxMax)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if(grVFile_Write(pFile, &(B->XSkinVertexCount), sizeof(B->XSkinVertexCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	assert( (B->XSkinVertexCount==0) || (B->XSkinVertexArray!=NULL));
	
	if (B->XSkinVertexCount>0)
		{
			u = sizeof(grBody_XSkinVertex) * B->XSkinVertexCount;
			if(grVFile_Write(pFile, B->XSkinVertexArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
		}

	if(grVFile_Write(pFile, &(B->SkinNormalCount), sizeof(B->SkinNormalCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if (B->SkinNormalCount>0)
		{
			u = sizeof(grBody_Normal) * B->SkinNormalCount;
			if(grVFile_Write(pFile, B->SkinNormalArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
		}

	if(grVFile_Write(pFile, &(B->blendDataCount), sizeof(B->blendDataCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if (B->blendDataCount>0)
		{
			u = sizeof(grBody_BlendData) * B->blendDataCount;
			if(grVFile_Write(pFile, B->blendDataArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
		}

	if(grVFile_Write(pFile, &(B->BoneCount), sizeof(B->BoneCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if (B->BoneCount>0)
		{
			u = sizeof(grBody_Bone) * B->BoneCount;
			if(grVFile_Write(pFile, B->BoneArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
		}

	if (grStrBlock_WriteToFile(B->BoneNames,pFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_WriteGeometry."); 	return GR_FALSE; }
	
	if(grVFile_Write(pFile, &(B->MaterialCount), sizeof(B->MaterialCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	if (B->MaterialCount>0)
		{
			u = sizeof(grBody_Material) * B->MaterialCount;
			if(grVFile_Write(pFile, B->MaterialArray, u) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
		}
	
	if (grStrBlock_WriteToFile(B->MaterialNames,pFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_WriteGeometry."); 	return GR_FALSE; }
	
	if(grVFile_Write(pFile, &(B->LevelsOfDetail), sizeof(B->LevelsOfDetail)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }

	for (i=0; i<B->LevelsOfDetail; i++)
		{
			u = B->SkinFaces[i].FaceCount;
			if(grVFile_Write(pFile, &(u), sizeof(u)) == GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
			if (u>0)
				{
					u = sizeof(grBody_Triangle) * u;
					if(grVFile_Write(pFile, B->SkinFaces[i].FaceArray, u) == GR_FALSE)
						{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grBody_WriteGeometry.");	return GR_FALSE; }
				}
		}
	return GR_TRUE;
}


GRAPI grBoolean GRCC grBody_WriteToFile(const grBody *B, grVFile *pFile)
{
	int i;
	grVFile *VFile;
	grVFile *SubFile;
	grVFile *BitmapDirectory;

	assert( grBody_IsValid(B) != GR_FALSE );
	assert( pFile != NULL );

	VFile = grVFile_OpenNewSystem(pFile,GR_VFILE_TYPE_VIRTUAL, NULL, 
									NULL, GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_CREATE);
	if (VFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grBody_WriteToFile: Failed to open body subsystem.");	goto WriteError;}
	
	SubFile = grVFile_Open(VFile,GR_BODY_GEOMETRY_NAME,GR_VFILE_OPEN_CREATE);
	if (SubFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grBody_WriteToFile: Failed to open subfile.");	goto WriteError;}

	{
	grVFile * LZFS;

	LZFS = grVFile_OpenNewSystem(SubFile,GR_VFILE_TYPE_LZ, NULL, NULL, GR_VFILE_OPEN_CREATE);
	if ( ! LZFS )
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grBody_WriteToFile: Failed to open compressed file.");	goto WriteError;}

	if ( ! grBody_WriteGeometry(B,LZFS) )
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grBody_WriteToFile: Failed to write body geometry.");	goto WriteError;}

	Log_Printf("Actor : Body : Geometry : ");
	if ( ! grVFile_Close(LZFS) )
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_WriteToFile: Failed to close compressed file.");	goto WriteError;}

	}

	if (grVFile_Close(SubFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_WriteToFile: Failed to close subfile.");	goto WriteError;}
		
	BitmapDirectory = grVFile_Open(VFile,GR_BODY_BITMAP_DIRECTORY_NAME, 
									GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_CREATE);
	if (BitmapDirectory == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grBody_WriteToFile: Failed to open bitmap subdir.");	goto WriteError;}
	
	for (i=0; i<B->MaterialCount; i++)
	{
		grBody_Material *M;
		M = &(B->MaterialArray[i]);

		if (M->MatSpec != NULL)
		{
			char FName[1000];
			sprintf(FName,"%d",i);

			SubFile = grVFile_Open(BitmapDirectory,FName,GR_VFILE_OPEN_CREATE);
			if (SubFile == NULL)
			{
				grErrorLog_AddString( GR_ERR_FILEIO_OPEN , "grBody_WriteToFile: Failed to open bitmap file:",FName);
				goto WriteError;
			}

			if (grMaterialSpec_WriteToFile(M->MatSpec, SubFile)==GR_FALSE)
			{
				grErrorLog_AddString( GR_ERR_FILEIO_WRITE , "grBody_WriteToFile: Failed to write bitmap:",FName);
				goto WriteError;
			}
					
			if (grVFile_Close(SubFile)==GR_FALSE)
			{
				grErrorLog_AddString( GR_ERR_FILEIO_CLOSE , "grBody_WriteToFile: Failed to close bitmap:",FName);
				goto WriteError;
			}
		}
	}
	if (grVFile_Close(BitmapDirectory)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_WriteToFile: Failed to close bitmap subdir.");	goto WriteError;}
	if (grVFile_Close(VFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grBody_WriteToFile: Failed to close body subsystem.");	goto WriteError;}
	
	return GR_TRUE;
	WriteError:
		return GR_FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////
// exposed geometry APIs

GRAPI int GRCC grBody_GetIndexedBoneVertexCount(const grBody* pBody, int boneIndex)
{
	int i, n;

	assert(pBody);

	if (boneIndex < 0 || boneIndex >= pBody->BoneCount)
		return 0;

	for (n = 0, i = 0; i < pBody->XSkinVertexCount; i ++)
	{
		if (pBody->XSkinVertexArray[i].BoneIndex == (grBody_Index)boneIndex)
		{
			n ++;
		}
	}

	return n;
}

GRAPI int GRCC grBody_GetNamedBoneVertexCount(const grBody* pBody, const char* pBoneName)
{
	int i, n;
	int boneIndex;

	assert(pBody);
	assert(pBoneName);

	if (! grStrBlock_FindString(pBody->MaterialNames, pBoneName, &boneIndex))
		return GR_FALSE;

	for (n = 0, i = 0; i < pBody->XSkinVertexCount; i ++)
	{
		if (pBody->XSkinVertexArray[i].BoneIndex == (grBody_Index)boneIndex)
		{
			n ++;
		}
	}

	return n;
}

// local space functions

GRAPI grBoolean GRCC grBody_GetIndexedBoneVertexLocations(const grBody* pBody, int boneIndex, int aSize,
	grVec3d* pVerts)
{
	int n;
	grBody_Index i;

	assert(pBody);
	assert(pVerts);

	if (boneIndex < 0 || boneIndex >= pBody->BoneCount)
		return GR_FALSE;

	for (n = 0, i = 0; i < pBody->XSkinVertexCount; i ++)
	{
		if (pBody->XSkinVertexArray[i].BoneIndex == (grBody_Index)boneIndex)
		{
			if (n == aSize)
				return GR_FALSE;

			pVerts[n].X = pBody->XSkinVertexArray[i].XPoint.X;
			pVerts[n].Y = pBody->XSkinVertexArray[i].XPoint.Y;
			pVerts[n].Z = pBody->XSkinVertexArray[i].XPoint.Z;

			n ++;
		}
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grBody_GetNamedBoneVertexLocations(const grBody* pBody, const char* pBoneName, int aSize,
	grVec3d* pVerts)
{
	int n, boneIndex;
	grBody_Index i;

	assert(pBody);
	assert(pBoneName);
	assert(pVerts);

	if (! grStrBlock_FindString(pBody->MaterialNames, pBoneName, &boneIndex))
		return GR_FALSE;

	for (n = 0, i = 0; i < pBody->XSkinVertexCount; i ++)
	{
		if (pBody->XSkinVertexArray[i].BoneIndex == (grBody_Index)boneIndex)
		{
			if (n == aSize)
				return GR_FALSE;

			pVerts[n].X = pBody->XSkinVertexArray[i].XPoint.X;
			pVerts[n].Y = pBody->XSkinVertexArray[i].XPoint.Y;
			pVerts[n].Z = pBody->XSkinVertexArray[i].XPoint.Z;

			n ++;
		}
	}

	return GR_TRUE;
}
