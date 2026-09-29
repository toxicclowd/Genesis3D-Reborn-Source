/****************************************************************************************/
/*  QUAD.H                                                                              */
/*                                                                                      */
/*  Author:  Charles Bloom                                                              */
/*  Description:                                                                        */
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
#ifndef QUAD_H
#define QUAD_H

#include "BaseType.h"
#include "grLight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QuadTree			QuadTree;

typedef struct grTerrain		grTerrain;
typedef struct grTerrain_Light	grTerrain_Light;

QuadTree *	QuadTree_Create(const grTerrain *T);

void		QuadTree_Destroy(QuadTree **pQT);

grBoolean	QuadTree_SetTexDim(QuadTree *QT,int Dim);

grBoolean	QuadTree_Tesselate(QuadTree *QT,grVec3d * pPos,grFrustum *pFrustum);

grBoolean	QuadTree_Render(const QuadTree *QT,grEngine *E,grCamera *Cam,grFrustum *F);

grBoolean	QuadTree_LightTesselatedPoints(QuadTree *QT,grTerrain_Light * Lights,int NumLights);

void		QuadTree_LightAllPoints(QuadTree *QT,grLight ** Lights,int NumLights);

void		QuadTree_LightTexture(  QuadTree *QT,grLight ** Lights,int NumLights,grBoolean SelfShadow,grBoolean WorldShadow);

void		QuadTree_ShowStats(const QuadTree *QT);

void		QuadTree_SetParameters(QuadTree * QT,uint32 BaseDepth,uint32 MaxQuads,float MinError);

grBoolean	QuadTree_IsValid(const QuadTree *QT);

void		QuadTree_GetExtBox(const QuadTree * QT,grExtBox * Box);

grBoolean	QuadTree_IntersectRay(QuadTree *QT,grVec3d *pStart,grVec3d *pDirection);

grBoolean	QuadTree_IntersectThickRay(const QuadTree * QT,const grVec3d * From,const grVec3d * To,grFloat Radius,grVec3d * pImpact);

void		QuadTree_ResetAllVertexLighting(QuadTree * QT);

#ifdef __cplusplus
}
#endif

//---------------------------------------------------------------------------
#endif // QUAD_H
