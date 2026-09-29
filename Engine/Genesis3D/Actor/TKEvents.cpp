/****************************************************************************************/
/*  TKARRAY.C																			*/
/*                                                                                      */
/*  Author: Stephen Balkum	                                                            */
/*  Description: Time-keyed events implementation.										*/
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
/* grTKEvents
	(Time-Keyed-Events)

	grTKEvents is a sorted array of times with an identifying descriptor.
	The descriptors are stored as strings in a separate, packed buffer.

*/

#include <assert.h>
#include <string.h>

#include "TKEvents.h"
#include "TKArray.h"
#include "Errorlog.h"
#include "Ram.h"

typedef struct
{
	grTKEvents_TimeType EventTime;
	uint32 DataOffset;
}	EventType;

typedef struct grTKEventsIterator 
{
	grTKEvents_TimeType EndTime;
	int CurrentIndex;
}	grTKEventsIterator;

typedef struct grTKEvents
{
	grTKArray* pTimeKeys;
	uint32 DataSize;
	char* pEventData;

	grTKEventsIterator Iterator;
}	grTKEvents;



// General validity test.
// Use TKE_ASSERT_VALID to test array for reasonable data.
#ifdef _DEBUG

#define TKE_ASSERT_VALID(E) grTKEvents_Asserts(E)

// Do not call this function directly.  Use TKE_ASSERT_VALID
static void GRCC grTKEvents_Asserts(const grTKEvents* E)
{
	assert( (E) != NULL );
	assert( (E)->pTimeKeys != NULL );
	if(grTKArray_NumElements((E)->pTimeKeys) == 0)
	{
		assert( (E)->pEventData == NULL );
	}
	else
	{
		assert( (E)->pEventData != NULL );
	}
}

#else // !_DEBUG

#define TKE_ASSERT_VALID(E) ((void)0)

#endif // _DEBUG

grTKEvents* GRCC grTKEvents_Create(void)
	// Creates a new event array.
{
	grTKEvents* pEvents;

	pEvents = GR_RAM_ALLOCATE_STRUCT_CLEAR(grTKEvents);
	if(!pEvents)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKEvents_Create.");
		return NULL;
	}

	pEvents->pTimeKeys = grTKArray_Create(sizeof(EventType));
	if(!pEvents->pTimeKeys)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKEvents_Create.");
		grRam_Free(pEvents);
		return NULL;
	}

	pEvents->DataSize = 0;
	pEvents->pEventData = NULL;

	pEvents->Iterator.CurrentIndex = 0;
	pEvents->Iterator.EndTime = -99e33f;	// you could sample here I suppose...
	
	return pEvents;
}


void GRCC grTKEvents_Destroy(grTKEvents** ppEvents)
	// Destroys array.
{
	grTKEvents* pE;

	assert(ppEvents);
	pE = *ppEvents;
	assert(pE);

	if( pE->pEventData != NULL )
		{
			grRam_Free(pE->pEventData);
		}
	
	if (pE->pTimeKeys != NULL)
		{
			grTKArray_Destroy(&pE->pTimeKeys);
		}
	grRam_Free(*ppEvents);
	*ppEvents = NULL;
}


grBoolean GRCC grTKEvents_Insert(grTKEvents* pEvents, grTKEvents_TimeType tKey, const char* pEventData)
{
	int nIndex;
	uint32 DataLength;
	uint32 InitialOffset;
	int nNumElements;
	EventType* pKeyInfo;
	char* pNewData;

	TKE_ASSERT_VALID(pEvents);

	if( grTKArray_Insert(&pEvents->pTimeKeys, tKey, &nIndex) != GR_TRUE )
	{
		grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grTKEvents_Insert: failed to insert.");
		return GR_FALSE;
	}
	pKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex);
	assert( pKeyInfo != NULL ); // I just successfully added it; it better be there.

	DataLength = strlen(pEventData) + 1;

	// Resize data to add new stuff
	pNewData = (char *)grRam_Realloc(pEvents->pEventData, pEvents->DataSize + DataLength);
	if(!pNewData)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKEvents_Insert.");
		if( grTKArray_DeleteElement(&pEvents->pTimeKeys, nIndex) == GR_FALSE)
		{
			// This object is now in an unstable state.
			assert(0);
		}
		// invalidate the iterator
		pEvents->Iterator.EndTime = -99e33f;	// you could sample here I suppose...
		return GR_FALSE;
	}
	pEvents->pEventData = pNewData;

	// Find where new data will go
	nNumElements = grTKArray_NumElements(pEvents->pTimeKeys);
	assert(nIndex < nNumElements); // sanity check
	if(nIndex == nNumElements - 1)
	{
		// We were added to the end
		InitialOffset = pEvents->DataSize;
	}
	else
	{
		EventType* pNextKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex + 1);
		assert( pNextKeyInfo != NULL );

		InitialOffset = pNextKeyInfo->DataOffset;
	}
	pKeyInfo->DataOffset = InitialOffset;

	// Add new data, moving only if necessary
	if(InitialOffset < pEvents->DataSize)
	{
		memmove(pEvents->pEventData + InitialOffset + DataLength,	// dest
				pEvents->pEventData + InitialOffset,				// src
				pEvents->DataSize - InitialOffset);					// count
	}
	memcpy(	pEvents->pEventData + InitialOffset,	// dest
			pEventData,								// src
			DataLength);							// count

	pEvents->DataSize += DataLength;

	// Bump all remaining offsets up
	nIndex++;
	while(nIndex < nNumElements)
	{
		pKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex);
		assert( pKeyInfo != NULL );
		pKeyInfo->DataOffset += DataLength;

		nIndex++;
	}

	// invalidate the iterator
	pEvents->Iterator.EndTime = -99e33f;	// you could sample here I suppose...

	return GR_TRUE;
}


grBoolean GRCC grTKEvents_Delete(grTKEvents* pEvents, grTKEvents_TimeType tKey)
{
	int nIndex, Count;
	grTKEvents_TimeType tFound;
	EventType* pKeyInfo;
	int DataOffset, DataSize;
	char *pNewData;

	TKE_ASSERT_VALID(pEvents);

	nIndex = grTKArray_BSearch(pEvents->pTimeKeys, tKey);

	if( nIndex < 0 )
	{	// key wasn't found
		grErrorLog_Add(GR_ERR_SEARCH_FAILURE, "grTKEvents_Delete: key not found for delete.");
		return GR_FALSE;
	}

	tFound = grTKArray_ElementTime(pEvents->pTimeKeys, nIndex);
	if(tFound < tKey - GR_TKA_TIME_TOLERANCE)
	{
		// key not found
		grErrorLog_Add(GR_ERR_SEARCH_FAILURE, "grTKEvents_Delete: key not found for delete.");
		return GR_FALSE;
	}

	pKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex);
	DataOffset = pKeyInfo->DataOffset;
	if(nIndex < grTKArray_NumElements(pEvents->pTimeKeys) - 1)
	{
		// not the last element
		pKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex + 1);
		DataSize = pKeyInfo->DataOffset - DataOffset;

		memmove(pEvents->pEventData + DataOffset,				// dest
				pEvents->pEventData + DataOffset + DataSize,	// src
				pEvents->DataSize - DataOffset - DataSize);		// count
	}
	else
	{
		// It's the last element and no memory needs to be moved
		DataSize = pEvents->DataSize - DataOffset;
	}

	// Adjust data
	pEvents->DataSize -= DataSize;
	if (pEvents->DataSize == 0)
	{
		grRam_Free (pEvents->pEventData);
		pEvents->pEventData = NULL;
	}
	else
	{
		pNewData = (char *)grRam_Realloc(pEvents->pEventData, pEvents->DataSize);
		// If the reallocation failed, it doesn't really hurt.  However, it is a 
		// sign of problems ahead.
		if(pNewData)
		{
			pEvents->pEventData = pNewData;
		}
	}

	// Finally, remove this element
	grTKArray_DeleteElement(&pEvents->pTimeKeys, nIndex);

	// Adjust the offsets
	Count = grTKArray_NumElements(pEvents->pTimeKeys);
	while(nIndex < Count)
	{
		pKeyInfo = (EventType *)grTKArray_Element(pEvents->pTimeKeys, nIndex);
		assert( pKeyInfo != NULL );
		pKeyInfo->DataOffset -= DataSize;
		nIndex++;
	}

	// invalidate the iterator
	pEvents->Iterator.EndTime = -99e33f;	// you could sample here I suppose...

	return GR_TRUE;
}


#define TKEVENTS_FILE_VERSION 0x00F0		// Restrict to 16 bits
#define TKEVENTS_BIN_FILE_TYPE   0x42454B54 // 'TKEB'


grTKEvents* GRCC grTKEvents_CreateFromFile(
	grVFile* pFile)					// stream positioned at array data
	// Creates a new array from the given stream.
{
	uint32 u;
	grTKEvents* pEvents;

	assert( pFile != NULL );

	// Read the format/version flag
	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
	{
		grErrorLog_Add(GR_ERR_FILEIO_READ, "grTKEvents_CreateFromFile.");
		return NULL;
	}

	pEvents = GR_RAM_ALLOCATE_STRUCT_CLEAR(grTKEvents);
	if(!pEvents)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKEvents_CreateFromFile.");
		return NULL;
	}
	pEvents->pEventData = NULL;
	pEvents->pTimeKeys  = NULL;

		if(u == TKEVENTS_BIN_FILE_TYPE)
			{
				if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_FILEIO_READ, "grTKEvents_CreateFromFile.");
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}
				if (u != TKEVENTS_FILE_VERSION)
					{
						grErrorLog_AddString(GR_ERR_FILEIO_VERSION,"grTKEvents_CreateFromFile: Failure to recognize file version", NULL);
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}

				if(grVFile_Read(pFile, &(pEvents->DataSize), sizeof(pEvents->DataSize)) == GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_FILEIO_READ, "grTKEvents_CreateFromFile.");
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}

				pEvents->pEventData = (char *)grRam_AllocateClear(pEvents->DataSize);
				if(!pEvents->pEventData)
					{
						grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grTKEvents_CreateFromFile.");
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}

				if(grVFile_Read(pFile, pEvents->pEventData, pEvents->DataSize) == GR_FALSE)
					{
						grErrorLog_Add(GR_ERR_FILEIO_READ, "grTKEvents_CreateFromFile.");
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}
				pEvents->pTimeKeys = grTKArray_CreateFromFile(pFile);
				if(!pEvents->pTimeKeys)
					{
						grErrorLog_Add(GR_ERR_FILEIO_READ, "grTKEvents_CreateFromFile.");
						grTKEvents_Destroy(&pEvents);
						return NULL;
					}
			}

	return pEvents;
}

grBoolean GRCC grTKEvents_WriteToFile(
	const grTKEvents* pEvents,		// sorted array to write
	grVFile* pFile)					// stream positioned for writing
	// Writes the array to the given stream.
{
	uint32 u;
	assert( pEvents != NULL );
	assert( pFile != NULL );

	u = TKEVENTS_BIN_FILE_TYPE;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_WRITE, "grTKEvents_WriteToFile.");
			return GR_FALSE;
		}
	u = TKEVENTS_FILE_VERSION;
	// Write the version
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_WRITE, "grTKEvents_WriteToFile.");
			return GR_FALSE;
		}

	if(grVFile_Write(pFile, &pEvents->DataSize, sizeof(pEvents->DataSize)) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_WRITE, "grTKEvents_WriteToFile.");
			return GR_FALSE;
		}

	if(grVFile_Write(pFile, pEvents->pEventData, pEvents->DataSize) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_WRITE, "grTKEvents_WriteToFile.");
			return GR_FALSE;
		}

	if (grTKArray_WriteToFile(pEvents->pTimeKeys, pFile)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_FILEIO_WRITE, "grTKEvents_WriteToFile.");
			return GR_FALSE;
		}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grTKEvents_GetExtents(grTKEvents *Events,
		grTKEvents_TimeType *FirstEventTime,
		grTKEvents_TimeType *LastEventTime)
{
	int Count;
	assert( Events != NULL );
	
	Count = grTKArray_NumElements(Events->pTimeKeys);
	if (Count<0)
		{
			return GR_FALSE;
		}

	*FirstEventTime = grTKArray_ElementTime(Events->pTimeKeys, 0);
	*LastEventTime  = grTKArray_ElementTime(Events->pTimeKeys, Count-1);
	return GR_TRUE;
}

void GRCC grTKEvents_SetupIterator(
	grTKEvents* pEvents,				// Event list to iterate
	grTKEvents_TimeType StartTime,				// Inclusive search start
	grTKEvents_TimeType EndTime)				// Non-inclusive search stop
	// For searching or querying the array for events between two times
	// times are compaired [StartTime,EndTime), '[' is inclusive, ')' is 
	// non-inclusive.  This prepares the PathGetNextEvent() function.  
{
	grTKEventsIterator* pTKEI;

	assert( pEvents != NULL );

	pTKEI = &pEvents->Iterator;

	pTKEI->EndTime = EndTime;

	// Initialize search with first index before StartTime
	pTKEI->CurrentIndex = grTKArray_BSearch(pEvents->pTimeKeys, StartTime - GR_TKA_TIME_TOLERANCE);
	while( (pTKEI->CurrentIndex > -1) && 
		(grTKArray_ElementTime(pEvents->pTimeKeys, pTKEI->CurrentIndex) >= StartTime - GR_TKA_TIME_TOLERANCE) )
	{
		pTKEI->CurrentIndex--;
	}
}


grBoolean GRCC grTKEvents_GetNextEvent(
	grTKEvents* pEvents,				// Event list to iterate
	grTKEvents_TimeType *pTime,				// Return time, if found
	const char **ppEventString)		// Return data, if found
	// Iterates from StartTime to EndTime as setup in grTKEvents_CreateIterator()
	// and for each event between these times [StartTime,EndTime)
	// this function will return Time and EventString returned for that event
	// and the iterator will be positioned for the next search.  When there 
	// are no more events in the range, this function will return NULL (Time
	// will be 0 and ppEventString will be empty).
{
	grTKEventsIterator* pTKEI;
	grTKArray* pTimeKeys;
	EventType* pKeyInfo;
	int Index;

	assert(pEvents);
	assert(pTime);
	assert(ppEventString);

	pTKEI = &pEvents->Iterator;

	pTimeKeys = pEvents->pTimeKeys;

	pTKEI->CurrentIndex++;
	Index = pTKEI->CurrentIndex;
	if(Index < grTKArray_NumElements(pTimeKeys))
	{
		*pTime = grTKArray_ElementTime(pTimeKeys, Index);
		if(*pTime + GR_TKA_TIME_TOLERANCE < pTKEI->EndTime)
		{
			// Looks good.  Get the string and return.
			pKeyInfo = (EventType *)grTKArray_Element(pTimeKeys, Index);
			*ppEventString = pEvents->pEventData + pKeyInfo->DataOffset;
			return GR_TRUE;
		}
	}

	// None found, clean up
	*pTime = 0.0f;
	*ppEventString = NULL;
	return GR_FALSE;
}
