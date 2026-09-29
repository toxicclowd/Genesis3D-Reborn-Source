/****************************************************************************************/
/*  BOX.H                                                                               */
/*                                                                                      */
/*  Author: Jason Wood                                                                  */
/*  Description: Box is a 3D Oriented Bounding Box                                      */
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
//
// This implementation may have a inaccuracy which allows
// the test to return that boxes overlap, when they are actually
// separated by a small distance.
//

#if !defined (GR_BOX_H)
#define GR_BOX_H

#include "Vec3d.h"
#include "Xform3d.h"

typedef struct grBox
{
	// all member variables are **PRIVATE**
	// the Box's scales along the Box's local frame axes

	float xScale, yScale, zScale;

	// the Box's local frame origin lies at (0, 0, 0) in local space
	//
	// these are the scaled Box axes in the global frame
	 
	grVec3d GlobalFrameAxes[3];

	// the transformation that takes the Box's axes from local space
	// to global space, and its inverse

	grXForm3d Transform, TransformInv;

}grBox;

/////////////////////////////////////////////////////////////////////////////
// call this to set up an Box for the first time or when the Box's
// local frame axes scale(s) change
void grBox_Set(grBox* Box, float xScale, float yScale, float zScale, const grXForm3d* Transform);


// call this to set the Box's transformation matrix (does not change the
// scales of the Box's local frame axes)
void grBox_SetXForm(grBox* Box, const grXForm3d* Transform);


// returns GR_TRUE if the boxes overlap, GR_FALSE otherwise
grBoolean grBox_DetectCollisionBetween(const grBox* Box1, const grBox* Box2);

#endif
