/*!
	@file grBrush.h 
	
	@author John Pollard
	@brief Brushes and Faces API definitions

	@par Licence
	The contents of this file are subject to the Genesis3D: Reborn Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.genesis3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Genesis3D: Reborn, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/

#ifndef JEBRUSH_H
#define JEBRUSH_H

#include "BaseType.h"

#include "grFaceInfo.h"
#include "Xform3d.h"
#include "grPlane.h"
#include "grVertArray.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grEngine grEngine;
typedef struct grCamera grCamera;

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef uint32		grBrush_Contents;
typedef grBrush_Contents grBrush_Contents;


typedef struct grBrush grBrush;
typedef struct grBrush_Face grBrush_Face;

#define GR_BSP_CONTENTS_FLOCK		(1<<0)
#define GR_BSP_CONTENTS_SOLID		(1<<1)
#define GR_BSP_CONTENTS_AIR			(1<<2)
#define GR_BSP_CONTENTS_SHEET		(1<<3)
#define GR_BSP_CONTENTS_EMPTY		(1<<4)

#define GR_BSP_VISIBLE_CONTENTS		(GR_BSP_CONTENTS_FLOCK | GR_BSP_CONTENTS_SHEET| GR_BSP_CONTENTS_SOLID | GR_BSP_CONTENTS_EMPTY | GR_BSP_CONTENTS_AIR)
//#define GR_BSP_SEP_CONTENTS		(GR_BSP_CONTENTS_SOLID|GR_BSP_CONTENTS_EMPTY|GR_BSP_CONTENTS_AIR)
#define GR_BSP_SEP_CONTENTS			(~GR_BSP_CONTENTS_SHEET)
#define GR_BSP_CONTENTS_EXCLUSIVE	(GR_BSP_CONTENTS_SOLID | GR_BSP_CONTENTS_AIR);

//========================================================================================
//	Structure defs
//========================================================================================
typedef struct grBrushRayInfo grBrushRayInfo;

struct grBrushRayInfo
{
	grBrush				*Brush;
	grBrush_Face		*BrushFace;
	grVec3d				Impact;
	grPlane				Plane;
	grBrush_Contents	c1, c2;		// For testing.  c1 = Front leaf contents, c2 = Back leaf contents
};

//========================================================================================
//	Function prototypes
//========================================================================================
GRAPI grBrush* GRCC grBrush_Create(int32 EstimatedVerts);


GRAPI grBrush* GRCC grBrush_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMGr);
GRAPI grBoolean	GRCC grBrush_WriteToFile(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMGr);

GRAPI grBoolean	GRCC grBrush_CreateRef(grBrush *Brush);
GRAPI void			GRCC grBrush_Destroy(grBrush **Brush);
GRAPI grBoolean	GRCC grBrush_IsValid(const grBrush *Brush);
GRAPI grBoolean	GRCC grBrush_IsConvex(const grBrush *Brush);
GRAPI void			GRCC grBrush_SetContents(grBrush *Brush, grBrush_Contents Contents);
GRAPI grBrush_Contents GRCC grBrush_GetContents(const grBrush *Brush);
GRAPI void			GRCC grBrush_SetXForm(grBrush *Brush, const grXForm3d *XForm, grBoolean Locked);
GRAPI const grXForm3d* GRCC grBrush_GetXForm(grBrush *Brush);

GRAPI const grXForm3d* GRCC grBrush_GetWorldToLockedXForm(grBrush *Brush);
GRAPI const grXForm3d* GRCC grBrush_GetLockedToWorldXForm(grBrush *Brush);
GRAPI grVertArray* GRCC grBrush_GetVertArray(const grBrush *Brush);

GRAPI grBoolean	GRCC grBrush_SetFaceInfoArray(grBrush *Brush, grFaceInfo_Array *Array);

GRAPI grBrush_Face* GRCC grBrush_CreateFace(grBrush *Brush, int32 NumVerts);
GRAPI grBrush_Face* GRCC grBrush_CreateFaceFromFile(grBrush *Brush, grVFile *VFile);
GRAPI grBoolean	GRCC grBrush_WriteFaceToFile(const grBrush *Brush, const grBrush_Face *Face, grVFile *VFile);
GRAPI void			GRCC grBrush_DestroyFace(grBrush *Brush, grBrush_Face **Face);
GRAPI int32		GRCC grBrush_GetFaceCount(const grBrush *Brush);
GRAPI grBrush_Face* GRCC grBrush_GetNextFace(const grBrush *Brush, const grBrush_Face *Start);
	// If start is NULL, the first face will be returned...
GRAPI grBrush_Face* GRCC grBrush_GetPrevFace(const grBrush *Brush, const grBrush_Face *Start);
	// Start CANNOT be NULL!  It MUST be a valid object...
GRAPI grBrush_Face* GRCC grBrush_GetFaceByIndex(const grBrush *Brush, int32 Index);

GRAPI grBrush* GRCC grBrush_FaceGetBrush(const grBrush_Face *Face);
GRAPI int32	GRCC grBrush_FaceGetVertCount(const grBrush_Face *Face);

/*! @fn void grBrush_FaceSetVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert)
	@brief Add or modify a vertex of a face of a brush
	@param Face The Brush face to reset vertex data
	@param Index The index of the vertex to reset
	@param Vert The vertex data to write at @p Index slot of vertex of the @p Face
*/
GRAPI void		 GRCC grBrush_FaceSetVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert);

/*! @fn void grBrush_FaceMoveVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert)
	@brief Move a vertex of a face of a brush
	@param Face The Brush face to reset vertex data
	@param Index The index of the vertex to reset
	@param Vert The move vector data for vertex of the @p Face
*/
GRAPI void		 GRCC grBrush_FaceMoveVertByIndex(grBrush_Face *Face, int32 Index, const grVec3d *Vert);
GRAPI const grVec3d* GRCC grBrush_FaceGetVertByIndex(const grBrush_Face *Face, int32 Index);
GRAPI grVec3d	 GRCC grBrush_FaceGetWorldSpaceVertByIndex(const grBrush_Face *Face, int32 Index);

GRAPI void		GRCC grBrush_FaceCalcPlane(grBrush_Face *Face);
GRAPI const grVec3d* GRCC grBrush_FaceGetNormal(grBrush_Face *Face);

GRAPI grBoolean	GRCC grBrush_FaceGetFaceInfo(grBrush_Face *Face, grFaceInfo *FaceInfo);
GRAPI grBoolean	GRCC grBrush_FaceSetFaceInfo(grBrush_Face *Face, const grFaceInfo *FaceInfo);

GRAPI grFaceInfo_ArrayIndex GRCC grBrush_FaceGetFaceInfoIndex(grBrush_Face *Face);

GRAPI grBoolean	GRCC grBrush_Render(const grBrush *Brush, const grEngine *Engine, const grCamera *Camera);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
