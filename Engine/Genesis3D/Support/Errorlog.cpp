/****************************************************************************************/
/*  ERRORLOG.C                                                                          */
/*                                                                                      */
/*  Author: Mike Sandige                                                                */
/*  Description: Generic error logging system implementation                            */
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
#include <windows.h>

#include <stdio.h>
#include <assert.h>		// assert()	
#include <stdlib.h>
#include <string.h>		// memmove(), strncpy() strncat()

#include "Errorlog.h"   

#define USE_STDIO_LOG	//	Outputs to file

#define MAX_ERRORS 230  //  updated to support the insane amount of errors you can
						//	get from the shader scripts before they quit out(CyRiuS)

#define MAX_USER_NAME_LEN	200		// Needed just 10 more chars! (was 100) JP
#define MAX_CONTEXT_LEN		128		// How big should this be?  Must be big enough to allow full paths to files, etc...

#ifdef USE_STDIO_LOG
FILE						*errorlog = NULL;
#define FILENAME			"Jet3D.log"
#endif

typedef struct
{
	grErrorLog_ErrorIDEnumType ErrorID;
	char String[MAX_USER_NAME_LEN+1];
	char Context[MAX_CONTEXT_LEN+1];
} grErrorType;

typedef struct
{
	int ErrorCount;
	int MaxErrors;
	grErrorType ErrorList[MAX_ERRORS];
} grErrorLogType;

grErrorLogType grErrorLog_Locals = {0,MAX_ERRORS};

GRAPI void GRCC grErrorLog_Clear(void)
	// clears error history
{
	grErrorLog_Locals.ErrorCount = 0;
}
	
GRAPI int  GRCC grErrorLog_Count(void)
	// reports size of current error log
{
	return 	grErrorLog_Locals.ErrorCount;
}


GRAPI void GRCC grErrorLog_AddExplicit(grErrorLog_ErrorClassType Error, 
	const char *ErrorIDString,
	const char *ErrorFileString,
	int LineNumber,
	const char *UserString,
	const char *Context)
{
	char	*SDst;
	char	*CDst;
	
	assert( grErrorLog_Locals.ErrorCount >= 0 );

	grErrorLog_Locals.ErrorList[grErrorLog_Locals.ErrorCount].ErrorID = (grErrorLog_ErrorIDEnumType)Error;
	if (grErrorLog_Locals.ErrorCount>=MAX_ERRORS)
	{	// scoot list down by one (loose oldest error)
		memmove(
			(char *)(&( grErrorLog_Locals.ErrorList[0] )),
			(char *)(&( grErrorLog_Locals.ErrorList[1] )),
			sizeof(grErrorType) * (grErrorLog_Locals.MaxErrors-1) );
		grErrorLog_Locals.ErrorCount = grErrorLog_Locals.MaxErrors-1;
	}

	assert( grErrorLog_Locals.ErrorCount < grErrorLog_Locals.MaxErrors );

	SDst = grErrorLog_Locals.ErrorList[grErrorLog_Locals.ErrorCount].String;

	// Copy new error info
	if (ErrorIDString != NULL)
		{
			strncpy(SDst,ErrorIDString,MAX_USER_NAME_LEN);
		}

	strncat(SDst," ",MAX_USER_NAME_LEN);

	if (ErrorFileString!=NULL)
		{
			const char* pModule = strrchr(ErrorFileString, '\\');
			if(!pModule)
				pModule = ErrorFileString;
			else
				pModule++; // skip that backslash
			strncat(SDst,pModule,MAX_USER_NAME_LEN);
			strncat(SDst," ",MAX_USER_NAME_LEN);
		}
	
	{
		char Number[20];
		itoa(LineNumber,Number,10);
		strncat(SDst,Number,MAX_USER_NAME_LEN);
	}
	
	if (UserString != NULL)
		{
			if (UserString[0]!=0)
				{
					strncat(SDst," ",MAX_USER_NAME_LEN);
					strncat(SDst,UserString,MAX_USER_NAME_LEN);
				}
		}

	CDst = grErrorLog_Locals.ErrorList[grErrorLog_Locals.ErrorCount].Context;

	// Clear the context string in the errorlog to prepare for a new one
	memset(CDst, 0, sizeof(char)*MAX_CONTEXT_LEN);

	if (Context != NULL)
	{
		if (Context[0]!=0)
		{
			//strncat(SDst," ",MAX_USER_NAME_LEN);
			strncat(CDst,Context,MAX_USER_NAME_LEN);
		}
	}	

	if (Error == GR_ERR_WINDOWS_API_FAILURE) 
	{
		int     LastError;
		char	*Buff;
		LastError = GetLastError();
		#ifdef ERRORLOG_FULL_REPORTING
		if (FormatMessage(	  FORMAT_MESSAGE_ALLOCATE_BUFFER 
							| FORMAT_MESSAGE_FROM_SYSTEM
							| FORMAT_MESSAGE_IGNORE_INSERTS,
						0,
						LastError,
						0,	
						(LPTSTR)&Buff,
						0,
						NULL)!=0)
			{
				strncat(SDst,Buff,MAX_USER_NAME_LEN);
				LocalFree( Buff );
			}
		else
			{
				char Number[50];
				itoa(LastError,Number,10);
				strncat(SDst," LastError=",MAX_USER_NAME_LEN);
				strncat(SDst,Number,MAX_USER_NAME_LEN);
			}
		#else
			strncat(SDst," ",MAX_USER_NAME_LEN);
		#endif
	}

	grErrorLog_Locals.ErrorCount++;

//	#ifndef NDEBUG
	{
		char	buff[100];
		sprintf(buff, "ErrorLog: %d -", Error);
		OutputDebugString(buff);
		OutputDebugString(SDst);
		OutputDebugString("\r\n");
#ifdef USE_STDIO_LOG
		errorlog = fopen(FILENAME, "at");
		if (!errorlog)
			errorlog = fopen(FILENAME, "wt");

		fprintf(errorlog, "%s\n", SDst);
		fflush(errorlog);
		fclose(errorlog);
//#endif
	}
#endif
}



GRAPI grBoolean GRCC grErrorLog_AppendStringToLastError(const char *String)
{
	char *SDst;
	if (String == NULL)
		{
			return GR_FALSE;
		}

	if (grErrorLog_Locals.ErrorCount>0)
		{
			SDst = grErrorLog_Locals.ErrorList[grErrorLog_Locals.ErrorCount-1].String;

			strncat(SDst,String,MAX_USER_NAME_LEN);
			return GR_TRUE;
		}
	else
		{
			return GR_FALSE;
		}
}

GRAPI grBoolean GRCC grErrorLog_Report(int history, grErrorLog_ErrorClassType *error, const char **UserString, const char **Context)
{
	assert( error != NULL );

	if ( (history > grErrorLog_Locals.ErrorCount) || (history < 0))
		{
			return GR_FALSE;
		}
	
	
	*error = (grErrorLog_ErrorClassType)grErrorLog_Locals.ErrorList[history].ErrorID;
	*UserString = grErrorLog_Locals.ErrorList[history].String;
	*Context = grErrorLog_Locals.ErrorList[history].Context;
	return GR_TRUE;
}


GRAPI const char * GRCC grErrorLog_IntToString(int Number)
{
	static char String[50];
	itoa(Number,String,10);
	return String;
}
