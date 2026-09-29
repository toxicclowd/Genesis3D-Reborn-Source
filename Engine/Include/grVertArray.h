/*!
	@file grVertArray.h 
	
	@author John Pollard
	@brief Vertex array definition and management functions

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

#ifndef GR_VERTARRAY_H
#define GR_VERTARRAY_H

#include "Vec3d.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================
//	Typedefs/#defines
//========================================================================================

/*! @typedef grVertArray
    @brief A reference to an Array of grVertex
*/
typedef struct grVertArray grVertArray;

/*! @typedef grVertArray_Optimizer
    @brief A reference to an Optimizer of Array of grVertex
*/
typedef struct grVertArray_Optimizer grVertArray_Optimizer;

/*! @typedef grVertArray_Index
    @brief The grVertrray Index type
*/
typedef	uint16							grVertArray_Index;
typedef grVertArray_Index				grVertArray_Index;


/*! @def GR_VERTARRAY_MAX_VERTS
	@brief The Max number of grVertex in a grVertArray
*/
#define GR_VERTARRAY_MAX_VERTS			(0xffff-1)

/*! @def GR_VERTARRAY_NULL_INDEX
	@brief The index indicating no value present
*/
#define	GR_VERTARRAY_NULL_INDEX			(GR_VERTARRAY_MAX_VERTS+1)

//========================================================================================
//	Structure defs
//========================================================================================

//========================================================================================
//	Function prototypes
//========================================================================================
GRAPI grVertArray		* GRCC grVertArray_Create(int32 StartVerts);
GRAPI grVertArray		* GRCC grVertArray_CreateFromFile(grVFile *VFile);
GRAPI grBoolean		GRCC grVertArray_WriteToFile(const grVertArray *Array, grVFile *VFile);
GRAPI void				GRCC grVertArray_Destroy(grVertArray **VArray);
GRAPI grBoolean		GRCC grVertArray_IsValid(const grVertArray *VArray);
GRAPI grVertArray_Index GRCC grVertArray_AddVert(grVertArray *Array, const grVec3d *Vert);
GRAPI grVertArray_Index GRCC grVertArray_ShareVert(grVertArray *Array, const grVec3d *Vert);
GRAPI void				GRCC grVertArray_RemoveVert(grVertArray *Array, grVertArray_Index *Index);
GRAPI grBoolean		GRCC grVertArray_RefVertByIndex(grVertArray *Array, grVertArray_Index Index);
GRAPI void				GRCC grVertArray_SetVertByIndex(grVertArray *VArray, grVertArray_Index Index, const grVec3d *Vert);
GRAPI const grVec3d	* GRCC grVertArray_GetVertByIndex(const grVertArray *VArray, grVertArray_Index Index);
GRAPI int16			GRCC grVertArray_GetMaxIndex( const grVertArray *VArray );
GRAPI grVertArray_Optimizer * GRCC grVertArray_CreateOptimizer(grVertArray *Array);
GRAPI void				GRCC grVertArray_DestroyOptimizer(grVertArray *Array, grVertArray_Optimizer **Optimizer);
GRAPI grVertArray_Index GRCC grVertArray_GetOptimizedIndex(grVertArray *Array, grVertArray_Optimizer *Optimizer, grVertArray_Index Index);
GRAPI grBoolean		GRCC grVertArray_GetEdgeVerts(grVertArray_Optimizer *Optimizer, const grVec3d *v1, const grVec3d *v2, grVertArray_Index *EdgeVerts, int32 *NumEdgeVerts, int32 MaxEdgeVerts);


#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif
