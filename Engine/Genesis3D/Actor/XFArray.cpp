/****************************************************************************************/
/*  XFARRAY.C																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Array of transforms implementation.									*/
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
#include "XFArray.h"
#include "Ram.h"
#include "Errorlog.h"


typedef struct grXFArray
{
	int		 TransformCount;
	grXForm3d *TransformArray;
} grXFArray;

grXFArray *GRCC grXFArray_Create(int Size)
{
	grXFArray *XFA;

	assert( Size > 0 );

	XFA = GR_RAM_ALLOCATE_STRUCT_CLEAR( grXFArray );
	if (XFA == NULL)
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grXFArray_Create.");
			return NULL;
		}
	XFA->TransformArray = GR_RAM_ALLOCATE_ARRAY_CLEAR(grXForm3d,Size);
	if (XFA->TransformArray == NULL)
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grXFArray_Create.");
			grRam_Free( XFA );
			return NULL;
		}
	XFA->TransformCount = Size;
	{
		grXForm3d X;
		grXForm3d_SetIdentity(&X);
		grXFArray_SetAll(XFA,&X);
	}
	return XFA;
}

void GRCC grXFArray_Destroy( grXFArray **XFA )
{
	assert( XFA != NULL );
	assert( *XFA != NULL );
	assert( (*XFA)->TransformCount > 0 );
	assert( (*XFA)->TransformArray != NULL );
	
	(*XFA)->TransformCount = -1;
	grRam_Free( (*XFA)->TransformArray);
	(*XFA)->TransformArray = NULL;
	grRam_Free( (*XFA) );
	(*XFA) = NULL;
}

grXForm3d *GRCC grXFArray_GetElements(const grXFArray *XFA, int *Size)
{
	assert( XFA != NULL );
	assert( Size != NULL );
	assert( XFA->TransformCount > 0 );
	assert( XFA->TransformArray != NULL );

	*Size = XFA->TransformCount;
	return XFA->TransformArray;
}

void GRCC grXFArray_SetAll(grXFArray *XFA, const grXForm3d *Matrix)
{
	assert( XFA != NULL );
	assert( Matrix != NULL );
	assert( XFA->TransformCount > 0 );
	assert( XFA->TransformArray != NULL );
	{
		int i;
		grXForm3d *X;
		for (i=0,X=XFA->TransformArray; i<XFA->TransformCount; i++,X++)
			{
				*X = *Matrix;
			}
	}
}
