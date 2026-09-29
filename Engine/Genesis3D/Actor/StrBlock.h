/****************************************************************************************/
/*  STRBLOCK.H																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: String block interface.												*/
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
#ifndef GR_STRBLOCK_H
#define GR_STRBLOCK_H

#include "BaseType.h"	// grBoolean
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grStrBlock grStrBlock;

GRAPI grStrBlock *GRCC grStrBlock_Create(void);
GRAPI void GRCC grStrBlock_Destroy(grStrBlock **SB);

GRAPI grBoolean GRCC grStrBlock_Append(grStrBlock **ppSB,const char *String);

GRAPI void GRCC grStrBlock_Delete(grStrBlock **ppSB,int Nth);

GRAPI const char *GRCC grStrBlock_GetString(const grStrBlock *SB, int Index);

GRAPI grBoolean GRCC grStrBlock_FindString(const grStrBlock* pSB, const char* String, int* pIndex);

GRAPI int GRCC grStrBlock_GetCount(const grStrBlock *SB);
GRAPI int GRCC grStrBlock_GetChecksum(const grStrBlock *SB);

GRAPI grStrBlock* GRCC grStrBlock_CreateFromFile(grVFile* pFile);
GRAPI grBoolean GRCC grStrBlock_WriteToFile(const grStrBlock *SB,grVFile *pFile);

#ifdef __cplusplus
}
#endif

#endif
