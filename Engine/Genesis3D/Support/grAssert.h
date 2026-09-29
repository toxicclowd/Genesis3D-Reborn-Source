/****************************************************************************************/
/*  GEASSERT.H                                                                          */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: Replacement for assert interface                                       */
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
#ifndef GR_ASSERT_H
#define GR_ASSERT_H

#include <assert.h>
#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif

// You should use grAssert() anywhere in the Jet3D engine that
// you would normally use assert().
//
// If you wish to be called back when asserts happen, use the
// routine grAssertSetCallback().  It returns the address of
// the callback routine that you're replacing.


#ifdef NDEBUG

	#define grAssert(exp)

#else

	extern void grAssertEntryPoint( void *, void *, unsigned );

	#define grAssert(exp) (void)( (exp) || (grAssertEntryPoint(#exp, __FILE__, __LINE__), 0) )

#endif

void grAssertDefault( void *exp, void *file, unsigned long line );

/************************************************************/

typedef void grAssertCallbackFn( void *exp, void *file, unsigned long line );

grAssertCallbackFn *grAssertSetCallback( grAssertCallbackFn *newAssertCallback );

typedef void (*grAssert_CriticalShutdownCallback) (uint32 Context);

extern void grAssert_SetCriticalShutdownCallback( grAssert_CriticalShutdownCallback CB , uint32 Context,
												grAssert_CriticalShutdownCallback * pOldCB , uint32 * pOldContext);

/************************************************************/

#ifdef __cplusplus
}
#endif

#endif
