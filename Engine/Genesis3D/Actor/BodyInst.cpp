/****************************************************************************************/
/*  BODYINST.C                                                                          */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Actor body instance implementation.                                    */
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

#include "BODY._H"
#include "BodyInst.h"
#include "Ram.h"
#include "Errorlog.h"
#include "StrBlock.h"

#include "Camera._h"


typedef struct grBodyInst
{
	const grBody			*BodyTemplate;
	grBodyInst_Geometry		 ExportGeometry;
	int						 LastLevelOfDetail;
	grBodyInst_Index		 FaceCount;
} grBodyInst;




void GRCF grBodyInst_PostScale(const grXForm3d *M,const grVec3d *S,grXForm3d *Scaled)
{
	Scaled->AX = M->AX * S->X;
	Scaled->BX = M->BX * S->X;
	Scaled->CX = M->CX * S->X;

	Scaled->AY = M->AY * S->Y;
	Scaled->BY = M->BY * S->Y;
	Scaled->CY = M->CY * S->Y;

	Scaled->AZ = M->AZ * S->Z;
	Scaled->BZ = M->BZ * S->Z;
	Scaled->CZ = M->CZ * S->Z;
	Scaled->Translation = M->Translation;
}


grBodyInst *GRCF grBodyInst_Create(const grBody *B)
{
	grBodyInst *BI;
	assert( B != NULL );
	assert( grBody_IsValid(B) != GR_FALSE );
	
	BI = GR_RAM_ALLOCATE_STRUCT_CLEAR(grBodyInst);
	if (BI == NULL)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBodyInst_Create.");
			return NULL;
		}
	BI->BodyTemplate = B;
	{
		grBodyInst_Geometry *G = &(BI->ExportGeometry);
		G->SkinVertexCount =0;
		G->SkinVertexArray = NULL;
		
		G->NormalCount = 0;
		G->NormalArray = NULL;
		
		G->FaceCount = (grBody_Index) 0;
		G->FaceListSize = 0; 
		G->FaceList = NULL;
	}

	BI->LastLevelOfDetail   = -1;
	BI->FaceCount =  0;

	return BI;
}
			

void GRCF grBodyInst_Destroy( grBodyInst **BI)
{
	grBodyInst_Geometry *G;
	assert( BI != NULL );
	assert( *BI != NULL );
	G = &( (*BI)->ExportGeometry );
	if (G->SkinVertexArray != NULL )
		{
			grRam_Free( G->SkinVertexArray );
			G->SkinVertexArray = NULL;
		}
	if (G->NormalArray != NULL )
		{
			grRam_Free( G->NormalArray );
			G->NormalArray = NULL;
		}
	if (G->FaceList != NULL )
		{
			grRam_Free( G->FaceList );
			G->FaceList = NULL;
		}
	grRam_Free( *BI );
	*BI = NULL;
}



#define GR_BODYINST_FACELIST_SIZE_FOR_TRIANGLE (8)

static grBodyInst_Geometry * GRCF grBodyInst_GetGeometryPrep(	
	grBodyInst *BI, 
	int LevelOfDetail)
{
	const grBody *B;
	grBodyInst_Geometry *G;
	LevelOfDetail;		// unused param
	
	assert( BI != NULL );
	assert( grBody_IsValid(BI->BodyTemplate) != GR_FALSE );
	B = BI->BodyTemplate;

	G = &(BI->ExportGeometry);
	assert( G  != NULL );

	if (G->SkinVertexCount != B->XSkinVertexCount)
		{
			if (G->SkinVertexArray!=NULL)
				{
					grRam_Free(G->SkinVertexArray);
				}
			G->SkinVertexArray = GR_RAM_ALLOCATE_ARRAY_CLEAR(grBodyInst_SkinVertex,B->XSkinVertexCount);
			if ( G->SkinVertexArray == NULL )
				{
					grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBodyInst_GetGeometryPrep.");
					G->SkinVertexCount = 0;
					return NULL;
				}
			G->SkinVertexCount  = B->XSkinVertexCount;
		}

	if (G->NormalCount != B->SkinNormalCount)
		{
			if (G->NormalArray!=NULL)
				{
					grRam_Free(G->NormalArray);
				}
			G->NormalArray = GR_RAM_ALLOCATE_ARRAY_CLEAR( grVec3d,B->SkinNormalCount);
			if ( G->NormalArray == NULL )
				{
					grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBodyInst_GetGeometryPrep.");
					G->NormalCount = 0;
					return NULL;
				}
			G->NormalCount  = B->SkinNormalCount;
		}

	if (BI->FaceCount != B->SkinFaces[GR_BODY_HIGHEST_LOD].FaceCount)
		{
			if (G->FaceList!=NULL)
				{
					grRam_Free(G->FaceList);
				}
			G->FaceListSize = sizeof(grBody_Index) * 
					B->SkinFaces[GR_BODY_HIGHEST_LOD].FaceCount * 
					GR_BODYINST_FACELIST_SIZE_FOR_TRIANGLE;
			G->FaceList = GR_RAM_ALLOCATE_ARRAY_CLEAR(grBody_Index,
							B->SkinFaces[GR_BODY_HIGHEST_LOD].FaceCount * 
							GR_BODYINST_FACELIST_SIZE_FOR_TRIANGLE);
			if ( G->FaceList == NULL )
				{
					grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grBodyInst_GetGeometryPrep.");
					BI->FaceCount = 0;
					return NULL;
				}
			BI->FaceCount = B->SkinFaces[GR_BODY_HIGHEST_LOD].FaceCount;
		}
	return G;
}

const grBodyInst_Geometry * GRCF grBodyInst_GetGeometry(
	const grBodyInst *BI, 
	const grVec3d *ScaleVector,
	const grXFArray *BoneTransformArray,
	int LevelOfDetail,
	const grCamera *Camera)
{
	grBodyInst_Geometry *G;
	const grBody *B;
	grXForm3d *BoneXFArray;
	int      BoneXFCount;
	grBody_Index BoneIndex;

	assert( BI != NULL );
	assert( BoneTransformArray != NULL );
	assert( grBody_IsValid(BI->BodyTemplate) != GR_FALSE );
	
	G = grBodyInst_GetGeometryPrep((grBodyInst *)BI,LevelOfDetail);
	if (G == NULL)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grBodyInst_GetGeometry.");
			return NULL;
		}
		

	B = BI->BodyTemplate;

	BoneXFArray = grXFArray_GetElements(BoneTransformArray,&BoneXFCount);
	if ( BoneXFArray == NULL)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grBodyInst_GetGeometry.");
			return NULL;
		}
	if (BoneXFCount != B->BoneCount)
		{	
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grBodyInst_GetGeometry.");
			return NULL;
		}


	{	
		int i,LevelOfDetailBit;
	
		if (Camera != NULL)
		{
			// transform and project all appropriate points
			grBody_XSkinVertex *S;
			grBodyInst_SkinVertex  *D;
			LevelOfDetailBit = 1 << LevelOfDetail;
			BoneIndex = -1;  // S->BoneIndex won't ever be this.
			grVec3d_Set(&(G->Maxs), -GR_BODY_REALLY_BIG_NUMBER, -GR_BODY_REALLY_BIG_NUMBER, -GR_BODY_REALLY_BIG_NUMBER );
			grVec3d_Set(&(G->Mins), GR_BODY_REALLY_BIG_NUMBER, GR_BODY_REALLY_BIG_NUMBER, GR_BODY_REALLY_BIG_NUMBER );
			for (i=B->XSkinVertexCount,S=B->XSkinVertexArray,D=G->SkinVertexArray; 
				 i>0; 
				 i--,S++,D++)
				{
					grXForm3d ObjectToCamera;
					if (S->BoneIndex!=BoneIndex)
						{ //Keep XSkinVertexArray sorted by BoneIndex for best performance
							BoneIndex = S->BoneIndex;
							grXForm3d_Multiply(		grCamera_XForm(Camera), 
													&(BoneXFArray[BoneIndex]),
													&ObjectToCamera);
							grBodyInst_PostScale(&ObjectToCamera,ScaleVector,&ObjectToCamera);
						}
					if ( S->LevelOfDetailMask && LevelOfDetailBit )
						{
							grVec3d *VecDestPtr = &(D->SVPoint);
// @@@
							if (S->nBlends == 0)
							{
								grXForm3d_Transform(  &(ObjectToCamera), &(S->XPoint),VecDestPtr);
							}

							else // we need to do some blending
							{
								int iblend;
								grVec3d worldLoc, blendLoc;

								grVec3d_Clear(&blendLoc);

								for (iblend = S->bdaOffset; 
									iblend < (S->nBlends + S->bdaOffset); iblend ++)
								{
									grVec3d ScaledPoint;
									grBody_BlendData* pBD = &B->blendDataArray[iblend];

									assert(pBD != NULL);

									ScaledPoint.X = pBD->XPoint.X * ScaleVector->X;
									ScaledPoint.Y = pBD->XPoint.Y * ScaleVector->Y;
									ScaledPoint.Z = pBD->XPoint.Z * ScaleVector->Z;

									// get world space loc of individual blend vert
									grXForm3d_Transform(&BoneXFArray[pBD->boneIndex],
										//&pBD->XPoint, &worldLoc);
										&ScaledPoint,&worldLoc);

									// add weighted worldLoc to blendLoc

									blendLoc.X += pBD->weight * worldLoc.X;
									blendLoc.Y += pBD->weight * worldLoc.Y;
									blendLoc.Z += pBD->weight * worldLoc.Z;
								}

								// now do the world to camera space xform

								grXForm3d_Transform(grCamera_XForm(Camera), &blendLoc, VecDestPtr);
							}


							#ifdef ONE_OVER_Z_PIPELINE
							grCamera_ProjectZ( Camera, VecDestPtr, VecDestPtr);
							#else
							grCamera_Project( Camera, VecDestPtr, VecDestPtr);
							#endif
							D->SVU = S->XU;
							D->SVV = S->XV;

							D->SVW = S->XPoint; // -JFW

							if (VecDestPtr->X > G->Maxs.X ) G->Maxs.X = VecDestPtr->X;
							if (VecDestPtr->X < G->Mins.X ) G->Mins.X = VecDestPtr->X;
							if (VecDestPtr->Y > G->Maxs.Y ) G->Maxs.Y = VecDestPtr->Y;
							if (VecDestPtr->Y < G->Mins.Y ) G->Mins.Y = VecDestPtr->Y;
							if (VecDestPtr->Z > G->Maxs.Z ) G->Maxs.Z = VecDestPtr->Z;
							if (VecDestPtr->Z < G->Mins.Z ) G->Mins.Z = VecDestPtr->Z;
							D->ReferenceBoneIndex = BoneIndex;
						}
				}
		} // camera != NULL

// @@ NEED TO ADD BLEND CODE HERE
#pragma message("blend doesn't work for a NULL camera")
		else  // camera is NULL
		{
			// transform all appropriate points
			grBody_XSkinVertex *S;
			grBodyInst_SkinVertex  *D;
			LevelOfDetailBit = 1 << LevelOfDetail;
			BoneIndex = -1;  // S->BoneIndex won't ever be this.
			grVec3d_Set(&(G->Maxs), -GR_BODY_REALLY_BIG_NUMBER, -GR_BODY_REALLY_BIG_NUMBER, -GR_BODY_REALLY_BIG_NUMBER );
			grVec3d_Set(&(G->Mins), GR_BODY_REALLY_BIG_NUMBER, GR_BODY_REALLY_BIG_NUMBER, GR_BODY_REALLY_BIG_NUMBER );
			
			for (i=B->XSkinVertexCount,S=B->XSkinVertexArray,D=G->SkinVertexArray; 
				 i>0; 
				 i--,S++,D++)
				{
					grXForm3d ObjectToWorld;
					if (S->BoneIndex!=BoneIndex)
						{ //Keep XSkinVertexArray sorted by BoneIndex for best performance
							BoneIndex = S->BoneIndex;
							grBodyInst_PostScale(&BoneXFArray[BoneIndex],ScaleVector,&ObjectToWorld);

						}
					if ( S->LevelOfDetailMask && LevelOfDetailBit )
						{
							grVec3d *VecDestPtr = &(D->SVPoint);
							grXForm3d_Transform(  &(ObjectToWorld),
												&(S->XPoint),VecDestPtr);
							D->SVU = S->XU;
							D->SVV = S->XV;

							D->SVW = S->XPoint; // -JFW

							if (VecDestPtr->X > G->Maxs.X ) G->Maxs.X = VecDestPtr->X;
							if (VecDestPtr->X < G->Mins.X ) G->Mins.X = VecDestPtr->X;
							if (VecDestPtr->Y > G->Maxs.Y ) G->Maxs.Y = VecDestPtr->Y;
							if (VecDestPtr->Y < G->Mins.Y ) G->Mins.Y = VecDestPtr->Y;
							if (VecDestPtr->Z > G->Maxs.Z ) G->Maxs.Z = VecDestPtr->Z;
							if (VecDestPtr->Z < G->Mins.Z ) G->Mins.Z = VecDestPtr->Z;
							D->ReferenceBoneIndex = BoneIndex;
						}
				}
		} // camera is NULL

			{
				grBody_Normal *S;
				grVec3d *D;
				// rotate all appropriate normals
				for (i=B->SkinNormalCount,S=B->SkinNormalArray,D=G->NormalArray;
					 i>0; 
					 i--,S++,D++)
				{
					if ( S->LevelOfDetailMask && LevelOfDetailBit )
					{
						if (S->nBlends == 0)
						{
							grXForm3d_Rotate(&(BoneXFArray[S->BoneIndex]),
								&(S->Normal),D);
						}
						
						else
						{
							int iblend;
							grVec3d xNormal;

							grVec3d_Clear(D);

							for (iblend = S->bdaOffset; 
								iblend < (S->nBlends + S->bdaOffset); iblend ++)
							{
								grBody_BlendData* pBD = &B->blendDataArray[iblend];

								assert(pBD != NULL);

								// transform normal into world space
								grXForm3d_Rotate(&BoneXFArray[pBD->boneIndex],
									&pBD->Normal, &xNormal);

								// add weighted normal to blendNormal

								D->X += pBD->weight * xNormal.X;
								D->Y += pBD->weight * xNormal.Y;
								D->Z += pBD->weight * xNormal.Z;
							}

							grVec3d_Normalize(D);							
						}
						
					}
				}
			}

	}


	if (LevelOfDetail != BI->LastLevelOfDetail)
	{
		// build face list to export
		int i,j;
		grBody_Index Count;
		const grBody_Triangle *T;
		grBody_Index *D;
		Count = B->SkinFaces[LevelOfDetail].FaceCount;

		for (i=0,T=B->SkinFaces[LevelOfDetail].FaceArray,D=G->FaceList;
				i<Count; 
				i++,T++,B++)
			{
				*D = GR_BODYINST_FACE_TRIANGLE;
				D++;
				*D = T->MaterialIndex;
				D++;
				for (j=0; j<3; j++)
					{
						*D = T->VtxIndex[j];
						D++;
						*D = T->NormalIndex[j];
						D++;
					}
			}
		assert( ((uint32)D) - ((uint32)G->FaceList) == (uint32)(G->FaceListSize) );
		G->FaceCount = Count;
		((grBodyInst *)BI)->LastLevelOfDetail = LevelOfDetail;
	}



	return G;
}	
