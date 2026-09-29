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

typedef struct jeEngine grEngine;
typedef struct jeCamera grCamera;

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef uint32		grBrush_Contents;
typedef grBrush_Contents jeBrush_Contents;


typedef struct jeBrush grBrush;
typedef struct jeBrush jeBrush;
typedef struct jeBrush_Face grBrush_Face;
typedef struct jeBrush_Face jeBrush_Face;

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
typedef struct jeBrushRayInfo grBrushRayInfo;
typedef struct jeBrushRayInfo jeBrushRayInfo;

struct jeBrushRayInfo
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
#ifndef GENESIS_NO_JET_COMPAT

#define JE_BSP_CONTENTS_AIR                      GR_BSP_CONTENTS_AIR
#define JE_BSP_CONTENTS_EMPTY                    GR_BSP_CONTENTS_EMPTY
#define JE_BSP_CONTENTS_EXCLUSIVE                GR_BSP_CONTENTS_EXCLUSIVE
#define JE_BSP_CONTENTS_FLOCK                    GR_BSP_CONTENTS_FLOCK
#define JE_BSP_CONTENTS_SHEET                    GR_BSP_CONTENTS_SHEET
#define JE_BSP_CONTENTS_SOLID                    GR_BSP_CONTENTS_SOLID
#define JE_BSP_SEP_CONTENTS                      GR_BSP_SEP_CONTENTS
#define JE_BSP_VISIBLE_CONTENTS                  GR_BSP_VISIBLE_CONTENTS
#define jeBrush_Create                           grBrush_Create
#define jeBrush_CreateFace                       grBrush_CreateFace
#define jeBrush_CreateFaceFromFile               grBrush_CreateFaceFromFile
#define jeBrush_CreateFromFile                   grBrush_CreateFromFile
#define jeBrush_CreateRef                        grBrush_CreateRef
#define jeBrush_Destroy                          grBrush_Destroy
#define jeBrush_DestroyFace                      grBrush_DestroyFace
#define jeBrush_FaceCalcPlane                    grBrush_FaceCalcPlane
#define jeBrush_FaceGetBrush                     grBrush_FaceGetBrush
#define jeBrush_FaceGetFaceInfo                  grBrush_FaceGetFaceInfo
#define jeBrush_FaceGetFaceInfoIndex             grBrush_FaceGetFaceInfoIndex
#define jeBrush_FaceGetNormal                    grBrush_FaceGetNormal
#define jeBrush_FaceGetVertByIndex               grBrush_FaceGetVertByIndex
#define jeBrush_FaceGetVertCount                 grBrush_FaceGetVertCount
#define jeBrush_FaceGetWorldSpaceVertByIndex     grBrush_FaceGetWorldSpaceVertByIndex
#define jeBrush_FaceMoveVertByIndex              grBrush_FaceMoveVertByIndex
#define jeBrush_FaceSetFaceInfo                  grBrush_FaceSetFaceInfo
#define jeBrush_FaceSetVertByIndex               grBrush_FaceSetVertByIndex
#define jeBrush_GetContents                      grBrush_GetContents
#define jeBrush_GetFaceByIndex                   grBrush_GetFaceByIndex
#define jeBrush_GetFaceCount                     grBrush_GetFaceCount
#define jeBrush_GetLockedToWorldXForm            grBrush_GetLockedToWorldXForm
#define jeBrush_GetNextFace                      grBrush_GetNextFace
#define jeBrush_GetPrevFace                      grBrush_GetPrevFace
#define jeBrush_GetVertArray                     grBrush_GetVertArray
#define jeBrush_GetWorldToLockedXForm            grBrush_GetWorldToLockedXForm
#define jeBrush_GetXForm                         grBrush_GetXForm
#define jeBrush_IsConvex                         grBrush_IsConvex
#define jeBrush_IsValid                          grBrush_IsValid
#define jeBrush_Render                           grBrush_Render
#define jeBrush_SetContents                      grBrush_SetContents
#define jeBrush_SetFaceInfoArray                 grBrush_SetFaceInfoArray
#define jeBrush_SetXForm                         grBrush_SetXForm
#define jeBrush_WriteFaceToFile                  grBrush_WriteFaceToFile
#define jeBrush_WriteToFile                      grBrush_WriteToFile

#endif // GENESIS_NO_JET_COMPAT

#endif
