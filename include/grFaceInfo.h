/****************************************************************************************/
/*  JEFACEINFO.H                                                                        */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D: Reborn Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D: Reborn, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/

#ifndef GEFACEINFO_H
#define GEFACEINFO_H

#include "Vec3d.h"
#include "grMaterial.h"
#include "VFile.h"
#include "grGArray.h"
#include "grPortal.h"
#include "Object.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================
typedef struct jeFaceInfo_Array grFaceInfo_Array;
typedef struct jeFaceInfo_Array jeFaceInfo_Array;
					
typedef grGArray_Index					grFaceInfo_ArrayIndex;
typedef grFaceInfo_ArrayIndex			jeFaceInfo_ArrayIndex;

typedef struct jeFaceInfo				grFaceInfo;
typedef struct jeFaceInfo				jeFaceInfo;

#define GR_FACEINFO_ARRAY_MAX_INDEX		GR_GARRAY_MAX_ELEMENTS
#define GR_FACEINFO_ARRAY_NULL_INDEX	GR_GARRAY_NULL_INDEX

#define FACEINFO_GOURAUD				(1<<0)
#define FACEINFO_FLAT					(1<<1)
#define FACEINFO_TRANSPARENT			(1<<2)
#define FACEINFO_FULLBRIGHT				(1<<3)
#define FACEINFO_VIS_PORTAL				(1<<4)
#define FACEINFO_RENDER_PORTAL_ONLY		(1<<5)	

// These flags are for testing convenience, and should NOT be used for assignment
#define FACEINFO_NO_DRAWFACE			(FACEINFO_VIS_PORTAL)
#define FACEINFO_NO_LIGHTMAP			(FACEINFO_FULLBRIGHT | FACEINFO_VIS_PORTAL | FACEINFO_RENDER_PORTAL_ONLY | FACEINFO_FLAT | FACEINFO_GOURAUD)

//========================================================================================
//	Structure defs
//========================================================================================
struct jeFaceInfo
{
	uint32					Flags;			// See flag definitions above
	grFloat					Alpha;			// Alpha value (0...255)
	grFloat					Rotate;			// (0...PI2)
	grFloat					ShiftU;			// Texture shift U
	grFloat					ShiftV;			// Texture shift V
	grFloat					DrawScaleU;		// Texture scale
	grFloat					DrawScaleV;
	grFloat					LMapScaleU;		// Lightmap scale
	grFloat					LMapScaleV;
	grMaterial_ArrayIndex	MaterialIndex;	// Material for this face

	grObject				*PortalCamera;
};

//========================================================================================
//	Function prototypes
//========================================================================================
GRAPI void			GRCC grFaceInfo_SetDefaults(grFaceInfo *FaceInfo);		// Handy little function
GRAPI grBoolean	GRCC grFaceInfo_Compare(const grFaceInfo *Face1, const grFaceInfo *Face2);
GRAPI grBoolean	GRCC grFaceInfo_NeedsLightmap(const grFaceInfo *pFaceInfo);


GRAPI grFaceInfo_Array * GRCC grFaceInfo_ArrayCreate(int32 StartFaces);


GRAPI grBoolean	GRCC grFaceInfo_ArrayWriteToFile(const grFaceInfo_Array *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
GRAPI grFaceInfo_Array * GRCC grFaceInfo_ArrayCreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);

GRAPI grBoolean	GRCC grFaceInfo_ArrayCreateRef(grFaceInfo_Array *Array);
GRAPI void			GRCC grFaceInfo_ArrayDestroy(grFaceInfo_Array **Array);
GRAPI grBoolean	GRCC grFaceInfo_ArrayIsValid(const grFaceInfo_Array *Array);
GRAPI grBoolean	GRCC grFaceInfo_ArrayIndexIsValid(grFaceInfo_ArrayIndex Index);
GRAPI grFaceInfo_ArrayIndex GRCC grFaceInfo_ArrayAddFaceInfo(grFaceInfo_Array *Array, const grFaceInfo *FaceInfo);
GRAPI grFaceInfo_ArrayIndex GRCC grFaceInfo_ArrayShareFaceInfo(grFaceInfo_Array *Array, const grFaceInfo *FaceInfo);
GRAPI grBoolean	GRCC grFaceInfo_ArrayRefFaceInfoIndex(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index);
GRAPI void			GRCC grFaceInfo_ArrayRemoveFaceInfo(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex *Index);
GRAPI void			GRCC grFaceInfo_ArraySetFaceInfoByIndex(grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index, const grFaceInfo *FaceInfo);
GRAPI const		grFaceInfo * GRCC grFaceInfo_ArrayGetFaceInfoByIndex(const grFaceInfo_Array *Array, grFaceInfo_ArrayIndex Index);

#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#define JE_FACEINFO_ARRAY_MAX_INDEX              GR_FACEINFO_ARRAY_MAX_INDEX
#define JE_FACEINFO_ARRAY_NULL_INDEX             GR_FACEINFO_ARRAY_NULL_INDEX
#define jeFaceInfo_ArrayAddFaceInfo              grFaceInfo_ArrayAddFaceInfo
#define jeFaceInfo_ArrayCreate                   grFaceInfo_ArrayCreate
#define jeFaceInfo_ArrayCreateFromFile           grFaceInfo_ArrayCreateFromFile
#define jeFaceInfo_ArrayCreateRef                grFaceInfo_ArrayCreateRef
#define jeFaceInfo_ArrayDestroy                  grFaceInfo_ArrayDestroy
#define jeFaceInfo_ArrayGetFaceInfoByIndex       grFaceInfo_ArrayGetFaceInfoByIndex
#define jeFaceInfo_ArrayIndexIsValid             grFaceInfo_ArrayIndexIsValid
#define jeFaceInfo_ArrayIsValid                  grFaceInfo_ArrayIsValid
#define jeFaceInfo_ArrayRefFaceInfoIndex         grFaceInfo_ArrayRefFaceInfoIndex
#define jeFaceInfo_ArrayRemoveFaceInfo           grFaceInfo_ArrayRemoveFaceInfo
#define jeFaceInfo_ArraySetFaceInfoByIndex       grFaceInfo_ArraySetFaceInfoByIndex
#define jeFaceInfo_ArrayShareFaceInfo            grFaceInfo_ArrayShareFaceInfo
#define jeFaceInfo_ArrayWriteToFile              grFaceInfo_ArrayWriteToFile
#define jeFaceInfo_Compare                       grFaceInfo_Compare
#define jeFaceInfo_NeedsLightmap                 grFaceInfo_NeedsLightmap
#define jeFaceInfo_SetDefaults                   grFaceInfo_SetDefaults

#endif // GENESIS_NO_JET_COMPAT

#endif
