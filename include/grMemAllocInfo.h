/****************************************************************************************/
/*  grMemAllocInfo.h                                                                    */
/*                                                                                      */
/*  Author: David Eisele                                                                */
/*  Description: Extended memoryleak debugging module                                   */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D: Reborn Public License       */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                            */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  This file was not part of the original Genesis3D: Reborn, released December 12, 1999.           */
/*                                                                                      */
/****************************************************************************************/

#ifndef GR_MEMALLOCINFO_H
#define GR_MEMALLOCINFO_H

#define GR_DEACTIVATE_JMAI
#define JE_DEACTIVATE_JMAI

#ifdef NDEBUG
	#ifndef GR_DEACTIVATE_JMAI
		#define GR_DEACTIVATE_JMAI
	#endif
	#ifndef JE_DEACTIVATE_JMAI
		#define JE_DEACTIVATE_JMAI
	#endif
#endif

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

#define jMAI_CREATED				(1<<0)		// module created/destroyed flag
#define jMAI_ACTIVE					(1<<1)		// turn on/off flag
#define	jMAI_SAVE_ON_FREE			(1<<2)		// save/delete infos on free flag
#define jMAI_EXCLUDE_ALL_FILES		(1<<3)		// exclude/include all files flag

#ifndef GR_DEACTIVATE_JMAI

GRAPI void GRCC grMemAllocInfo_Create(const char *FName);
GRAPI void GRCC grMemAllocInfo_Destroy();
GRAPI void GRCC grMemAllocInfo_Activate();
GRAPI void GRCC grMemAllocInfo_DeActivate(grBoolean DojMAIReport);
GRAPI void GRCC grMemAllocInfo_FileReport(const char *FName, const char *DumpFile, grBoolean FreedMemoryReport);
GRAPI uint32 GRCC grMemAllocInfo_GetFlags();

#else
	#define grMemAllocInfo_Create(FName)
	#define grMemAllocInfo_Activate()
	#define grMemAllocInfo_DeActivate(DojMAIReport)
	#define grMemAllocInfo_Destroy()
	#define grMemAllocInfo_FileReport(FName,DumpFile,FreedMemoryReport)
	#define grMemAllocInfo_GetFlags(Flags)								0
#endif

#ifdef __cplusplus
}
#endif

//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================
#ifndef GENESIS_NO_JET_COMPAT

#ifndef JE_DEACTIVATE_JMAI
#define JE_DEACTIVATE_JMAI                       GR_DEACTIVATE_JMAI
#endif

#define jeMemAllocInfo_Activate                  grMemAllocInfo_Activate
#define jeMemAllocInfo_Create                    grMemAllocInfo_Create
#define jeMemAllocInfo_DeActivate                grMemAllocInfo_DeActivate
#define jeMemAllocInfo_Destroy                   grMemAllocInfo_Destroy
#define jeMemAllocInfo_FileReport                grMemAllocInfo_FileReport
#define jeMemAllocInfo_GetFlags                  grMemAllocInfo_GetFlags

#endif // GENESIS_NO_JET_COMPAT

#endif // GR_MEMALLOCINFO_H