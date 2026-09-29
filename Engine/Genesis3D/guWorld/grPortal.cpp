/****************************************************************************************/
/*  JEPORTAL.C                                                                          */
/*                                                                                      */
/*  Author:  John Pollard                                                               */
/*  Description:                                                                        */
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
#include <assert.h>
#include <string.h>

#include "grPortal.h"

#include "Ram.h"

//========================================================================================
//========================================================================================
#define ZeroMem(a) memset(a, 0, sizeof(*a))
#define ZeroMemArray(a, s) memset(a, 0, sizeof(*a)*s)

//========================================================================================
//	grPortal_Create
//========================================================================================
GRAPI grPortal * GRCC grPortal_Create(void)
{
	grPortal *Portal;

	Portal = GR_RAM_ALLOCATE_STRUCT(grPortal);

	if (!Portal)
		return NULL;

	ZeroMem(Portal);

	Portal->RefCount = 1;

	grXForm3d_SetIdentity(&Portal->XForm);

	return Portal;
}

//========================================================================================
//	grPortal_CreateRef
//========================================================================================
GRAPI grBoolean GRCC grPortal_CreateRef(grPortal *Portal)
{
	assert (grPortal_IsValid(Portal));

	Portal->RefCount++;

	return GR_TRUE;
}

//========================================================================================
//	grPortal_Destroy
//========================================================================================
GRAPI void GRCC grPortal_Destroy(grPortal **Portal)
{
	assert(Portal);
	assert(grPortal_IsValid(*Portal));

	(*Portal)->RefCount--;

	if ((*Portal)->RefCount > 0)
		return;

	grRam_Free(*Portal);
	*Portal = NULL;
}

//========================================================================================
//	grPortal_IsValid
//========================================================================================
GRAPI grBoolean GRCC grPortal_IsValid(const grPortal *Portal)
{
	if (!Portal)
		return GR_FALSE;

	if (Portal->RefCount <= 0)
		return GR_FALSE;

	return GR_TRUE;
}
