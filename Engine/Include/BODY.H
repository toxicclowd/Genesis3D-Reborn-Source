/****************************************************************************************/
/*  BODY.H                                                                              */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Actor body interface.		                                            */
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
#ifndef GR_BODY_H
#define GR_BODY_H 

/* This object is for managing the data associated with a skeletal-based mesh, 
   a 'body'.
   This object holds the geometry for the body, the list of materials needed, 
*/

#include "BaseType.h"
#include "Vec3d.h"
#include "Xform3d.h"
#include "VFile.h"
#include "Bitmap.h"
#include "UVMap.h"

#include "grMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GR_BODY_NUMBER_OF_LOD			(16)		// 0 is highest detail (this gives 16 levels of LOD)
#define GR_BODY_NO_PARENT_BONE         (-1)   
#define GR_BODY_HIGHEST_LOD            (0)

#define GR_BODY_ROOT					(-1)	// for specifying 'root' bounding box.

enum
{
	GR_BODY_OPTIMIZE_FLAGS_NONE				= 1 << 0,
	GR_BODY_OPTIMIZE_FLAGS_VERTS			= 1 << 1,
	GR_BODY_OPTIMIZE_FLAGS_NORMALS			= 1 << 2,
	GR_BODY_OPTIMIZE_FLAGS_SORT_VERTS		= 1 << 3,		
	GR_BODY_OPTIMIZE_FLAGS_SORT_FACES		= 1 << 4
};

typedef struct grBody grBody;


GRAPI grBody *GRCC grBody_Create(void);

GRAPI void GRCC grBody_Destroy(grBody **B);

GRAPI grBoolean GRCC grBody_IsValid(const grBody *B);

GRAPI grBoolean GRCC grBody_SetOptimizeFlags(grBody* pBody, uint32 flags);
GRAPI grBoolean GRCC grBody_Optimize(grBody *pBody);
GRAPI grBoolean GRCC grBody_GetGeometryStats(const grBody *B, int lod, int *Vertices, int *Faces, int *Normals);

GRAPI int16 GRCC grBody_GetBlendDataCount(const grBody* pBody);

GRAPI grBoolean GRCC grBody_AddFace(	grBody *B,
								const grVec3d *Vertex1, const grVec3d *Normal1, 
								grFloat U1, grFloat V1, int BoneIndex1,
								const grVec3d *Vertex2, const grVec3d *Normal2, 
									grFloat U2, grFloat V2, int BoneIndex2,
								const grVec3d *Vertex3, const grVec3d *Normal3, 
									grFloat U3, grFloat V3, int BoneIndex3,
								int MaterialIndex);

// If each set of verts was only a single vertex (100% weight), grBody_AddFaceWeightedVerts
// acts like grBody_AddFace in a much less efficient manner.
GRAPI grBoolean GRCC grBody_AddFaceWeightedVerts(	grBody* pBody,
								const grVec3d* pVerts1, const grVec3d* pNormal1, 
									grFloat u1, grFloat v1, const int* pBoneIndexes1, 
									const grFloat* pVertWeights1, int NumVerts1,
								const grVec3d* pVerts2, const grVec3d* pNormal2, 
									grFloat u2, grFloat v2, const int* pBoneIndexes2, 
									const grFloat* pVertWeights2, int NumVerts2,
								const grVec3d* pVerts3, const grVec3d* pNormal3, 
									grFloat u3, grFloat v3, const int* pBoneIndexes3, 
									const grFloat* pVertWeights3, int NumVerts3,
								int materialIndex);

			// Bitmap is added to body.  It's reference count is increased.  Caller still owns a pointer
			// to the bitmap, and is responsible for destroying it.
GRAPI grBoolean GRCC grBody_AddMaterial( grBody *B, 
									const char *MaterialName, 
									grMaterialSpec *Material,
									grFloat Red, 
									grFloat Green, 
									grFloat Blue,
									grUVMapper pMapper,
									int *MaterialIndex);

			// returned bitmap is a pointer to the bitmap in the body's list.  It may not be destroyed.
			// if caller would like to 'own' a copy of that bitmap pointer, it should call grBitmap_CreateRef()
GRAPI grBoolean GRCC grBody_GetMaterial(const grBody *Body, int MaterialIndex,
										const char **MaterialName,
										grMaterialSpec **Material, grFloat *Red, grFloat *Green, grFloat *Blue,
										grUVMapper * pMapper);

			// Bitmap is set into the body.  It's reference count is increased.  Caller still owns a pointer
			// to the bitmap, and is responsible for destroying it.
GRAPI grBoolean GRCC grBody_SetMaterial(grBody *Body, int MaterialIndex,
										grMaterialSpec *Material,  grFloat Red,  grFloat Green,  grFloat Blue, grUVMapper Mapper);

GRAPI int GRCC grBody_GetMaterialCount(const grBody *B);

GRAPI grBoolean GRCC grBody_AddBone( grBody *B, 
							int ParentBoneIndex,
							const char *BoneName, 
							const grXForm3d *AttachmentMatrix,
							int *BoneIndex);

GRAPI grBoolean GRCC grBody_ComputeLevelsOfDetail( grBody *B ,int Levels);

GRAPI int GRCC grBody_GetBoneCount(const grBody *B);

GRAPI void GRCC grBody_GetBone(	const grBody *B, 
						int BoneIndex, 
						const char **BoneName,
						grXForm3d *Attachment, 
						int *ParentBoneIndex);

GRAPI int32 GRCC grBody_GetBoneNameChecksum(const grBody *B);

GRAPI void GRCC grBody_SetBoundingBox( grBody *B,
							int BoneIndex,		// GR_BODY_ROOT for specifing 'root' bounding box.
							const grVec3d *MinimumBoxCorner,
							const grVec3d *MaximumBoxCorner);
 

GRAPI grBoolean GRCC grBody_GetBoundingBox( const grBody *B, 
							int BoneIndex,		// GR_BODY_ROOT for specifing 'root' bounding box.
							grVec3d *MinimumBoxCorner,
							grVec3d *MaximumBoxCorner);

GRAPI grBoolean GRCC grBody_GetBoneByName(const grBody* B,
	const char* BoneName,
	int* pBoneIndex,
	grXForm3d* Attachment,
	int* pParentBoneIndex);

GRAPI grBoolean GRCC grBody_WriteToFile(const grBody *B, grVFile *pFile);
GRAPI grBody  *GRCC  grBody_CreateFromFile(grVFile *pFile);

GRAPI int GRCC grBody_GetIndexedBoneVertexCount(const grBody* pBody, int boneIndex);
GRAPI int GRCC grBody_GetNamedBoneVertexCount(const grBody* pBody, const char* pBoneName);

// use GetxxBoneVertexCount() functions to obtain aSize, the size of the array of vertices prior to
// calling these functions
GRAPI grBoolean GRCC grBody_GetIndexedBoneVertexLocations(const grBody* pBody, int boneIndex, int aSize,
	grVec3d* pVerts);
GRAPI grBoolean GRCC grBody_GetNamedBoneVertexLocations(const grBody* pBody, const char* pBoneName, int aSize,
	grVec3d* pVerts);

#ifdef __cplusplus
}
#endif

#endif
