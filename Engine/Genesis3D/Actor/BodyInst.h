/****************************************************************************************/
/*  BODYINST.H                                                                          */
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Actor body instance interface.		                                    */
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
#ifndef GR_BODYINST_H
#define GR_BODYINST_H 

/* This object is for accessing and retrieving an 'instance' of the geometry
   for a body.  
   
   The retrieval is a list of drawing commands in world space or 
   in camera space.  

   An array of transforms that corresponds to the bones in the body is needed.
*/


#include "BaseType.h"
#include "Xform3d.h"
#include "Body.h"
#include "XFArray.h"
#include "Camera.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct grBodyInst grBodyInst;

typedef int16 grBodyInst_Index;

typedef enum 
{
	GR_BODYINST_FACE_TRIANGLE,
	GR_BODYINST_FACE_TRISTRIP,
	GR_BODYINST_FACE_TRIFAN
} grBodyInst_FaceType;


typedef struct grBodyInst_SkinVertex
{
	grVec3d SVPoint;
	// added unxformed body skin vert member to structure for uv mapping
	grVec3d SVW; // world-space (unxformed, unprojected) point
	grFloat SVU,SVV;
	int	ReferenceBoneIndex;
} grBodyInst_SkinVertex;

typedef struct grBodyInst_Geometry 
{
	grBodyInst_Index		 SkinVertexCount;
	grBodyInst_SkinVertex	*SkinVertexArray;

	grBodyInst_Index		 NormalCount;
	grVec3d					*NormalArray;

	grBodyInst_Index		 FaceCount;
	int32					 FaceListSize;
	grBodyInst_Index		*FaceList;

	grVec3d					 Maxs, Mins;
}	grBodyInst_Geometry;

/* format for grBodyInst_Geometry.FaceList:
	primitive type (GR_BODY_FACE_TRIANGLE,	  GR_BODY_FACE_TRISTRIP,  GR_BODY_FACE_TRIFAN )
	followed by material index
	followed by...
	case primitive 
		GR_BODY_FACE_TRIANGLE:
		  vertex index 1, normal index 1
		  vertex index 2, normal index 2
		  vertex index 3, normal index 3
		  (next primitive)
		GR_BODY_FACE_TRISTRIP:
		  triangle count
		  vertex index 1, normal index 1
		  vertex index 2, normal index 2
		  vertex index 3, normal index 3
		  vertex index 4, normal index 4
		  ...  # vertices is triangle count+2
		  (next primitive)
		GR_BODY_FACE_TRIFAN:
		  triangle count
		  vertex index 1, normal index 1
		  vertex index 2, normal index 2
		  vertex index 3, normal index 3
		  vertex index 4, normal index 4
		  ...  # vertices is triangle count+2
		  (next primitive)
*/




grBodyInst *GRCF grBodyInst_Create( const grBody *B );
void GRCF grBodyInst_Destroy(grBodyInst **BI);

const grBodyInst_Geometry * GRCF grBodyInst_GetGeometry( 
								const grBodyInst *BI,
								const grVec3d *Scale,
								const grXFArray *BoneXformArray,
								int LevelOfDetail,
								const grCamera *Camera);


#ifdef __cplusplus
}
#endif

#endif
