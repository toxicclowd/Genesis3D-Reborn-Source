/****************************************************************************************/
/*  ERRORLOG.H                                                                          */
/*                                                                                      */
/*  Author: Mike Sandige                                                                */
/*  Description: Generic error logging system interface                                 */
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
/*  
	Simple error id logger.
	errors are logged to this object using ErrorLog_Add()
	errors are retrieved using ErrorLog_Report()

	created:  Mike Sandige 1/2/98
*/


#ifndef GR_ERRORLOG_H
#define GR_ERRORLOG_H

#include "basetype.h"

//#ifndef NDEBUG 
	#define ERRORLOG_FULL_REPORTING
//#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
	Temporary structure forward, to hold place for when grThreadQueue is fully
	implemented.
*/
typedef	struct	grErrorLog	grErrorLog;

typedef enum
{
//do not use these errors - use the next list
	GR_ERR_DRIVER_INIT_FAILED=500,				// Could not init Driver
//do not use these errors - use the next list
	GR_ERR_DRIVER_NOT_FOUND,				// File open error for driver
//do not use these errors - use the next list
	GR_ERR_DRIVER_NOT_INITIALIZED,			// Driver shutdown failure
	GR_ERR_INVALID_DRIVER,					// Wrong driver version, or bad driver
	GR_ERR_DRIVER_BEGIN_SCENE_FAILED,
	GR_ERR_DRIVER_END_SCENE_FAILED,
//do not use these errors - use the next list
	GR_ERR_NO_PERF_FREQ,
	GR_ERR_FILE_OPEN_ERROR,
//do not use these errors - use the next list
	GR_ERR_INVALID_PARMS,
	GR_ERR_OUT_OF_MEMORY,
} grErrorLog_ErrorIDEnumType;

typedef enum 
{
	GR_ERR_MEMORY_RESOURCE,
	GR_ERR_DISPLAY_RESOURCE,
	GR_ERR_SOUND_RESOURCE,
	GR_ERR_SYSTEM_RESOURCE,
	GR_ERR_INTERNAL_RESOURCE,
	
	GR_ERR_FILEIO_OPEN,
	GR_ERR_FILEIO_CLOSE,
	GR_ERR_FILEIO_READ,
	GR_ERR_FILEIO_WRITE,
	GR_ERR_FILEIO_FORMAT,
	GR_ERR_FILEIO_VERSION,
	
	GR_ERR_LIST_FULL,
	GR_ERR_DATA_FORMAT,
	GR_ERR_BAD_PARAMETER,
	GR_ERR_SEARCH_FAILURE,

	GR_ERR_WINDOWS_API_FAILURE,
	GR_ERR_SUBSYSTEM_FAILURE,
	GR_ERR_SHADER_SCRIPT, //added (cyrius)
	GR_ERR_PARSE_ERROR, //added (cyrius)
	GR_ERR_PARSE_FAILURE, //added (cyrius)

} grErrorLog_ErrorClassType;

GRAPI void GRCC grErrorLog_Clear(void);
	// clears error history

GRAPI int  GRCC grErrorLog_Count(void);
	// reports size of current error log

GRAPI void GRCC grErrorLog_AddExplicit(grErrorLog_ErrorClassType,
	const char *ErrorIDString,
	const char *ErrorFileString,
	int LineNumber,
	const char *UserString,
	const char *Context);
	// not intended to be used directly: use ErrorLog_Add or ErrorLog_AddString


#ifdef ERRORLOG_FULL_REPORTING
	// 'Debug' version includes a textual error id, and the user string

	#define grErrorLog_Add(Error, Context) grErrorLog_AddExplicit((grErrorLog_ErrorClassType)(Error), #Error, __FILE__, __LINE__,"", Context)
		// logs an error.  

	#define grErrorLog_AddString(Error,String, Context) grErrorLog_AddExplicit((grErrorLog_ErrorClassType)(Error), #Error, __FILE__,__LINE__, String, Context)
		// logs an error with additional identifing string.  
	
GRAPI	grBoolean GRCC grErrorLog_AppendStringToLastError(const char *String);// use grErrorLog_AppendString

	#define grErrorLog_AppendString(XXX) grErrorLog_AppendStringToLastError(XXX)
		// adds text to the previous logged error

#else
	// 'Release' version does not include the textual error id, or the user string

	#define grErrorLog_Add(Error, Context) grErrorLog_AddExplicit((grErrorLog_ErrorClassType)(Error), "", __FILE__, __LINE__,"", Context)
		// logs an error.  

	#define grErrorLog_AddString(Error,String, Context) grErrorLog_AddExplicit((grErrorLog_ErrorClassType)(Error), "", __FILE__,__LINE__, "", Context)
		// logs an error with additional identifing string.  
	
	#define grErrorLog_AppendString(XXX)
		// adds text to the previous logged error

#endif

GRAPI const char * GRCC grErrorLog_IntToString(int Number);
	// turns Number into a string.  uses a fixed static character string.  
	// for use with the context parameter of grErrorLog_AddString()

GRAPI grBoolean GRCC grErrorLog_Report(int History, grErrorLog_ErrorClassType *Error, const char **UserString, const char **Context);
	// reports from the error log.  
	// history is 0 for most recent,  1.. for second most recent etc.
	// returns GR_TRUE if report succeeded.  GR_FALSE if it failed.

#ifdef __cplusplus
}
#endif

#endif
