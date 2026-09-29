/*!
	@file Array.h 
	
	@author 
	@brief Default array management set of functions

	@par Licence
	The contents of this file are subject to the Jet3D Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.jet3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Jet3D, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/

#ifndef GR_ARRAY_H
#define GR_ARRAY_H

#include "BaseType.h"
#include "VFile.h"
#include "grPtrMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------
// types

/*! @typedef grArray
    @brief A reference to an Array
*/
typedef struct grArray grArray;

/*! @typedef grArray_Index
    @brief The grArray Index type
*/
typedef uint32 grArray_Index;

/*! @typedef grArray_Defragmenter
    @brief A reference to an Array defragmenter

	@ref jearraydefrag
*/
typedef struct grArray_Defragmenter grArray_Defragmenter;

/*! @def grArray_MaxIndex
    @brief The last grArray Index value
*/
#define grArray_MaxIndex        ((grArray_Index)((~ ((grArray_Index)0) )-1))

/*! @def GR_ARRAY_NULL_INDEX
    @brief The NULL grArray Index value
*/
#define GR_ARRAY_NULL_INDEX     (grArray_MaxIndex+1)


/*! @typedef grArray_IOFunc
*   @brief A reference to the Read/Write function to load/save array elements.
*/
typedef grBoolean (* grArray_IOFunc) (grVFile * File,void * Element,void *Context);

//-----------------------------------------------------------------------
// protos

/*! @fn grArray* grArray_Create(int32 HunkLength, int32 NumHunks, int32 AutoExtendNumItems)
    @brief Create a grArray instance for NumHunks elements of HunkLength bytes.
	@param[in] HunkLength The size of the element
	@param[in] NumHunks	The count of elements
	@param[in] AutoExtendNumItems The number to use when need to increase Array size
	@return The grArray instance or NULL if failed
*/
GRAPI grArray*			GRCC grArray_Create(int32 HunkLength, int32 NumHunks, int32 AutoExtendNumItems);

GRAPI void 			GRCC grArray_CreateRef(		grArray * Array);
GRAPI void 			GRCC grArray_Destroy(		grArray ** pArray);
GRAPI grBoolean		GRCC grArray_Extend(			grArray * Array, int32 NumHunks);
GRAPI void				GRCC grArray_Reset(			grArray * Array);

GRAPI void *			GRCC grArray_GetNewElement(	grArray * Array);
GRAPI grBoolean		GRCC grArray_FreeElement(	grArray * Array,void * Element);
	
GRAPI grArray_Index	GRCC grArray_GetElementIndex(void * Element);
							// == (ArrayPtr - Array)/sizeof(Item)
GRAPI void *			GRCC grArray_GetElement(grArray * Array,grArray_Index Index);
							// == Array[Index]

GRAPI void *			GRCC grArray_GetNextElement(grArray * Array,void * H);
							// use NULL to get it started

GRAPI grArray_Index GRCC grArray_GetNextIndex(grArray * Array, grArray_Index Index);
							// use GR_ARRAY_NULL_INDEX to get it started
/*! @fn grArray_Defragmenter* grArray_DefragmentStart(grArray * Array)
	@brief Create  drefragmenter for the array Array
	@param[in] Array The array to defragment
	@return The defragmenter instance or NULL if failed
	@todo Implement this function in order to return other thing than an empty structure.
*/
GRAPI grArray_Defragmenter* GRCC grArray_DefragmentStart(grArray * Array);

/*! @fn grArray_Index grArray_Defragment(grArray_Defragmenter * D, grArray_Index I)
	@brief Return the defragmented index of the Ith element
	@param[in] D The array defragmenter to use
	@param[in] I The array index to defragment
	@return The new index of the element
	@todo Implement body of this function, currently only return I value.
*/
GRAPI grArray_Index		GRCC grArray_Defragment(grArray_Defragmenter * D, grArray_Index I);

/*! @fn void grArray_DefragmentEnd(grArray_Defragmenter * D)
	@brief End the defragmenter use and destroy its resources
	@param[in] D The array defragmenter to use
	@todo Implement body of this function, currently do nothing
*/
GRAPI void 				GRCC grArray_DefragmentEnd(grArray_Defragmenter * D);

/*! @fn grArray* grArray_CreateFromFile(grVFile * File, uint16 lVersionSizeOffset, grArray_IOFunc ElementReader,void *ReaderContext)
	@brief Create an array from data read from a file


	@param[in] File The file from where to read initialisating data.
	@param[in] lVersionSizeOffset The size offset to add in the element size allocation.
	@param[in] ElementReader The element reader callback function.
	@param[in] ReaderContext The data to pass to the callback.
	@return An array instance or NULL if failed.
*/
GRAPI grArray *		GRCC grArray_CreateFromFile(grVFile * File, uint16 lVersionSizeOffset, grArray_IOFunc ElementReader,void *ReaderContext);

/*! @fn grBoolean grArray_WriteToFile(const grArray * Array, grVFile * File, grArray_IOFunc ElementWriter,void *WriterContext)
	@brief Write array data to the file.


	@param[in] Array The array to write.
	@param[in] File The file where to write data.
	@param[in] ElementWriter The element writer callback function.
	@param[in] WriterContext The data to pass to the callback.
	@return GR_TRUE if succeed, GR_FALSE otherwise.
*/
GRAPI grBoolean		GRCC grArray_WriteToFile(const grArray * Array, grVFile * File, grArray_IOFunc ElementWriter,void *WriterContext);
							// you should _Defragment before you WriteToFile if you can !!!!

//-----------------------------------------------------------------------
// docs

/*! @page jearraydefrag grArray Defrag Documentation
Degfrag sample code:<br>
<b>The defragmenter code is not implemented yet. No need to use it!</b>
@code
void DoMyDefrag(grArray * A)
{
grArray_Defragmenter * D;

	D = grArray_DefragmentStart(A);
	assert(D);

	while( I have stuff )
	{
		// remap an Index
		MyIndex = grArray_GetElementIndex(MyData->Pointer);
		grArray_Defragment(D, & MyIndex );
		MyData->Index = MyIndex;
	}

	grArray_DefragmentDone(D);

	while ( I have stuff )
	{
		MyData->Pointer = grArray_GetElement(A,MyData->Index);
	}

}
@endcode

*/

#ifdef __cplusplus
}
#endif

#endif /* ARRAY_H */

