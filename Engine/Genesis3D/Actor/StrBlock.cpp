/****************************************************************************************/
/*  STRBLOCK.C																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: String block implementation.											*/
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

//   a list of strings implemented as a single block of memory for fast
//   loading.  The 'Data' Field is interpreted as an array of integer 
//   offsets relative to the beginning of the data field.  After the int list
//   is the packed string data.  Since no additional allocations are needed 
//   this object can be file loaded as one block. 


#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "StrBlock.h"
#include "Ram.h"
#include "Errorlog.h"

#define STRBLOCK_MAX_STRINGLEN 255


typedef struct grStrBlock
{
	int Count;
	grStrBlock *SanityCheck;
	union 
		{
			int IntArray[1];		// char offset into CharArray for string[n]
			char CharArray[1];
		} Data;
		
} grStrBlock;


GRAPI int GRCC grStrBlock_GetChecksum(const grStrBlock *SB)
{
	int Count;
	int Len;
	int i,j;
	const char *Str;
	int Checksum=0;
	assert( SB != NULL );

	Count = grStrBlock_GetCount(SB);
	for (i=0; i<Count; i++)
		{
			Str = grStrBlock_GetString(SB,i);
			assert(Str!=NULL);
			Len = strlen(Str);
			for (j=0; j<Len; j++)
				 {
					Checksum += (int)Str[j];
				}
			Checksum = Checksum*3;
		}
	return Checksum;
}

GRAPI grStrBlock *GRCC grStrBlock_Create(void)
{
	grStrBlock *SB;
	
	SB = GR_RAM_ALLOCATE_STRUCT_CLEAR(grStrBlock);

	if ( SB == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grStrBlock_Create.");
			return NULL;
		}
	SB->Count=0;
	SB->SanityCheck = SB;
	return SB;
}


GRAPI void GRCC grStrBlock_Destroy(grStrBlock **SB)
{
	assert( (*SB)->SanityCheck == (*SB) );
	assert(  SB != NULL );
	assert( *SB != NULL );	
	grRam_Free( *SB );
	*SB = NULL;
}


static int GRCC grStrBlock_BlockSize(const grStrBlock *B)
{
	int Offset;
	const char *LastStr;
	assert( B != NULL );
	assert( B->SanityCheck == B );

	if ( B->Count == 0 )
		return 0;
	Offset = B->Data.IntArray[B->Count-1];
	LastStr = &(B->Data.CharArray[Offset]);

	return strlen(LastStr) + 1 + Offset;
}


GRAPI void GRCC grStrBlock_Delete(grStrBlock **ppSB,int Nth)
{
	int BlockSize;
	int StringLen;
	int CloseSize;
	const char *String;
	assert(  ppSB  != NULL );
	assert( *ppSB  != NULL );
	assert( Nth >=0 );
	assert( Nth < (*ppSB)->Count );
	assert( (*ppSB)->SanityCheck == (*ppSB) );

	String = grStrBlock_GetString(*ppSB,Nth);
	assert( String != NULL );
	StringLen = strlen(String) + 1;
		
	BlockSize = grStrBlock_BlockSize(*ppSB);

	{
		grStrBlock *B = *ppSB;
		char *ToBeReplaced;
		char *Replacement=NULL;
		int i;
		ToBeReplaced = &((*ppSB)->Data.CharArray[(*ppSB)->Data.IntArray[Nth]]);
		if (Nth< (*ppSB)->Count-1)
			Replacement  = &((*ppSB)->Data.CharArray[(*ppSB)->Data.IntArray[Nth+1]]);
		for (i=Nth+1,CloseSize = 0; i<(*ppSB)->Count ; i++)
			{
				CloseSize += strlen(&((*ppSB)->Data.CharArray[(*ppSB)->Data.IntArray[i]])) +1;
				B->Data.IntArray[i] -= StringLen;
			}
		for (i=0; i<(*ppSB)->Count ; i++)
			{
				B->Data.IntArray[i] -= sizeof(int);
			}
		// crunch out Nth string
		if (Nth< (*ppSB)->Count-1)
			memmove(ToBeReplaced,Replacement,CloseSize);
		// crunch out Nth index
		memmove(&(B->Data.IntArray[Nth]),
				&(B->Data.IntArray[Nth+1]),
				BlockSize - ( sizeof(int) *  (Nth+1) ) );

	}
	
	{
		grStrBlock * NewgrStrBlock;

		NewgrStrBlock = (grStrBlock *)grRam_Realloc( *ppSB, 
			BlockSize				// size of data block
			+ sizeof(grStrBlock)		// size of strblock structure
			- StringLen				// size of dying string
			- sizeof(int) );		// size of new index to string
		if ( NewgrStrBlock != NULL )
			{
				*ppSB = NewgrStrBlock;
				(*ppSB)->SanityCheck = NewgrStrBlock;
			}
	}

	(*ppSB)->Count--;
}



GRAPI grBoolean GRCC grStrBlock_FindString(const grStrBlock* pSB, const char* String, int* pIndex)
{
	int i;
	int Count;
	const char *Str;

	assert(pSB != NULL);
	assert(String != NULL);
	assert(pIndex != NULL);
	assert( pSB->SanityCheck == pSB );

	Count = grStrBlock_GetCount(pSB);
	for (i=0; i<Count; i++)
	{
		Str = grStrBlock_GetString(pSB,i);
		if(strcmp(String, Str) == 0)
		{
			*pIndex = i;
			return GR_TRUE;
		}
	}
	return GR_FALSE;
}


GRAPI grBoolean GRCC grStrBlock_Append(grStrBlock **ppSB,const char *String)
{
	int BlockSize;
	assert(  ppSB  != NULL );
	assert( *ppSB  != NULL );
	assert( String != NULL );
	assert( (*ppSB)->SanityCheck == (*ppSB) );

	if (strlen(String)>=STRBLOCK_MAX_STRINGLEN)
		{
			grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grStrBlock_Append: string too long.");
			return GR_FALSE;
		}

	BlockSize = grStrBlock_BlockSize(*ppSB);

	{
		grStrBlock * NewgrStrBlock;

		NewgrStrBlock = (grStrBlock*)grRam_Realloc( *ppSB, 
			BlockSize				// size of data block
			+ sizeof(grStrBlock)		// size of strblock structure
			+ strlen(String) + 1		// size of new string
			+ sizeof(int) );		// size of new index to string
		if ( NewgrStrBlock == NULL )
			{
				grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grStrBlock_Append: failed to allocate space for new string.");
				return GR_FALSE;
			}
		*ppSB = NewgrStrBlock;
		(*ppSB)->SanityCheck = NewgrStrBlock;
	}

	{
		grStrBlock *B = *ppSB;
		int i;
		for (i=0; i<B->Count; i++)
			{
				B->Data.IntArray[i] += sizeof(int);
			}
		if (B->Count > 0)
			{
				memmove(&(B->Data.IntArray[B->Count+1]),
						&(B->Data.IntArray[B->Count]),
						BlockSize - sizeof(int) * B->Count);
			}
		B->Data.IntArray[B->Count] = BlockSize + sizeof(int);
		strcpy(&(B->Data.CharArray[B->Data.IntArray[B->Count]]),String);
	}
	(*ppSB)->Count++;
	return GR_TRUE;
}

GRAPI const char *GRCC grStrBlock_GetString(const grStrBlock *SB, int Index)
{
	assert( SB != NULL );
	assert( Index >= 0 );
	assert( Index < SB->Count );
	assert( SB->SanityCheck == SB );
	return &(SB->Data.CharArray[SB->Data.IntArray[Index]]);
}

GRAPI int GRCC grStrBlock_GetCount(const grStrBlock *SB)
{
	assert( SB != NULL);
	assert( SB->SanityCheck == SB );
	return SB->Count;
}


#define STRBLOCK_BIN_FILE_TYPE 0x424B4253	// 'SBKB'


typedef struct
{
	int Count;
	uint32 Size;
} grStrBlock_FileHeader;

GRAPI grStrBlock* GRCC grStrBlock_CreateFromFile(grVFile* pFile)
{
	int32 u;
	grStrBlock *SB;
	grStrBlock_FileHeader Header;

	assert( pFile != NULL );

	if(grVFile_Read(pFile, &u, sizeof(u)) == GR_FALSE)
	{
		grErrorLog_Add( GR_ERR_FILEIO_READ , "grStrBlock_CreateFromFile: Failed to read header.");
		return NULL;
	}

	if (u!=STRBLOCK_BIN_FILE_TYPE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_FORMAT , "grStrBlock_CreateFromFile: Bad or wrong header.");
			return NULL;
		}

	if (grVFile_Read(pFile, &Header,sizeof(grStrBlock_FileHeader)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grStrBlock_CreateFromFile: Failed to read header block.");
			return NULL;
		}
	
	SB = (grStrBlock *)grRam_AllocateClear( sizeof(grStrBlock) + Header.Size );
	if( SB == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grStrBlock_CreateFromFile.");
			return NULL;	
		}
	SB->SanityCheck = SB;
	SB->Count = Header.Count; 

	if (grVFile_Read(pFile, &(SB->Data),Header.Size) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grStrBlock_CreateFromFile.");
			return NULL;
		}
	return SB;
}
			

GRAPI grBoolean GRCC grStrBlock_WriteToFile(const grStrBlock *SB,grVFile *pFile)
{
	uint32 u;
	grStrBlock_FileHeader Header;

	assert( SB != NULL );
	assert( pFile != NULL );
	assert( SB->SanityCheck == SB );

	// Write the format flag
	u = STRBLOCK_BIN_FILE_TYPE;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grStrBlock_WriteToFile: Failed to write header.");
			return GR_FALSE;
		}

	Header.Size = grStrBlock_BlockSize(SB);
	Header.Count = SB->Count;

	if(grVFile_Write(pFile, &Header, sizeof(grStrBlock_FileHeader)) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grStrBlock_WriteToFile: Failed to write header block.");
			return GR_FALSE;
		}
	
	if (grVFile_Write(pFile, &(SB->Data),Header.Size) == GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grStrBlock_WriteToFile: Failed to write string data.");
			return GR_FALSE;
		}
		
	return GR_TRUE;
}
