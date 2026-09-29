/****************************************************************************************/
/*  JEPLANE.C                                                                           */
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
#include <math.h>

// Public Dependents
#include "grPlane.h"

// Private Dependents
#include "Errorlog.h"

//====================================================================================
//	grPlane_SetFromVerts
//====================================================================================
GRAPI void	GRCC grPlane_SetFromVerts(grPlane *Plane, const grVec3d *V1, const grVec3d *V2, const grVec3d *V3)
{
	grVec3d		Vect1, Vect2;
	
	// Get the 2 vectors to derive the normal
	grVec3d_Subtract(V1, V2, &Vect1);
	grVec3d_Subtract(V3, V2, &Vect2);
	
	// The normal is the cross between these 2 vectors
	grVec3d_CrossProduct(&Vect1, &Vect2, &Plane->Normal);
	grVec3d_Normalize(&Plane->Normal);

	// Get the planes distance from the origin, by projecting a vert on the plane
	// along the plane normal, to the origin...
	Plane->Dist = grVec3d_DotProduct(V1, &Plane->Normal);

	// Finally, get the plane type
	Plane->Type = grPlane_TypeFromUnitVector(&Plane->Normal);
}

//=====================================================================================
//	grPlane_Inverse
//=====================================================================================
GRAPI void	GRCC grPlane_Inverse(grPlane *Plane)
{
	assert(Plane);

	grVec3d_Inverse(&Plane->Normal);
	Plane->Dist = -Plane->Dist;
}

//====================================================================================
//	grPlane_Rotate
//====================================================================================
GRAPI void	GRCC grPlane_Rotate(const grPlane *In, const grXForm3d *XForm, grPlane *Out)
{
	assert(In);
	assert(XForm);
	assert(Out);

	grXForm3d_Rotate(XForm, &In->Normal, &Out->Normal);

	Out->Dist = In->Dist;
	Out->Type = Type_Any;
}

//====================================================================================
//	grPlane_Transform
//====================================================================================
GRAPI  void GRCC grPlane_Transform(const grPlane *In, const grXForm3d *XForm, grPlane *Out)
{
	grVec3d		PointOnPlane;

	assert(In);
	assert(XForm);
	assert(Out);

	// Put a point on the plane
	grVec3d_Scale(&In->Normal, In->Dist, &PointOnPlane);
	// Transform the point
	grXForm3d_Transform(XForm, &PointOnPlane, &PointOnPlane);
	// Rotate the plane
	grXForm3d_Rotate(XForm, &In->Normal, &Out->Normal);
	// Find the Dist of the new plane by projecting the transformed point on the new plane
	Out->Dist = grVec3d_DotProduct(&Out->Normal, &PointOnPlane);

	Out->Type = Type_Any;
}

//====================================================================================
//	grPlane_TransformRenorm
//====================================================================================
GRAPI  void GRCC grPlane_TransformRenorm(const grPlane *In, const grXForm3d *XForm, grPlane *Out)
{
	grVec3d		PointOnPlane;

	assert(In);
	assert(XForm);
	assert(Out);

	// Put a point on the plane
	grVec3d_Scale(&In->Normal, In->Dist, &PointOnPlane);
	// Transform the point
	grXForm3d_Transform(XForm, &PointOnPlane, &PointOnPlane);
	// Rotate the plane
	grXForm3d_Rotate(XForm, &In->Normal, &Out->Normal);
	grVec3d_Normalize(&Out->Normal); // if XForm isn't normalized, need to renorm here
	// Find the Dist of the new plane by projecting the transformed point on the new plane
	Out->Dist = grVec3d_DotProduct(&Out->Normal, &PointOnPlane);

	Out->Type = Type_Any;
}

//=====================================================================================
//	grPlane_TypeFromUnitVector
//=====================================================================================
grPlane_Type grPlane_TypeFromUnitVector(const grVec3d *V1)
{
	float	X, Y, Z;

	assert(V1);

	X = (float)fabs(V1->X);
	Y = (float)fabs(V1->Y);
	Z = (float)fabs(V1->Z);

	if (X == 1.0f)			
		return Type_X;			// Axial aligned on X
	else if (Y == 1.0f)			
		return Type_Y;			// Axial aligned on Y
	else if (Z == 1.0f)
		return Type_Z;			// Axial aligned on Z
	else if (X >= Y && X >= Z)	
		return Type_AnyX;		// Non Axial, X dominent axis
	else if (Y >= X && Y >= Z)
		return Type_AnyY;		// Non Axial, Y dominent axis
	else
		return Type_AnyZ;		// Non Axial, Z dominent axis
}

//=====================================================================================
//	grPlane_GetAAVectors	(Axial aligned vectors)
//=====================================================================================
GRAPI grBoolean GRCC grPlane_GetAAVectors(const grPlane *Plane, grVec3d *Xv, grVec3d *Yv)
{
	int32	BestAxis;
	float	Dot, Best;
	int32	i;

	assert(Plane);
	assert(Xv);
	assert(Yv);
	
	Best = 0.0f;
	BestAxis = -1;
	
	for (i=0 ; i<3 ; i++)
	{
		Dot = (float)fabs(grVec3d_GetElement((grVec3d*)&Plane->Normal, i));

		if (Dot > Best)
		{
			Best = Dot;
			BestAxis = i;
		}
	}

	switch(BestAxis)
	{
		case 0:						// X
			Xv->X = 0.0f;
			Xv->Y = 0.0f;
			Xv->Z = 1.0f;

			Yv->X = 0.0f;
			Yv->Y =-1.0f;
			Yv->Z = 0.0f;
			break;
		case 1:						// Y
			Xv->X = 1.0f;
			Xv->Y = 0.0f;
			Xv->Z = 0.0f;

			Yv->X = 0.0f;
			Yv->Y = 0.0f;
			Yv->Z = 1.0f;
			break;
		case 2:						// Z
			Xv->X = 1.0f;
			Xv->Y = 0.0f;
			Xv->Z = 0.0f;

			Yv->X = 0.0f;
			Yv->Y =-1.0f;
			Yv->Z = 0.0f;
			break;

		default:
			grErrorLog_AddString(-1,"grPlane_GetAAVectors: No Axis found.\n", "");
			return GR_FALSE;
	}

	return GR_TRUE;
}

//=====================================================================================
//	grPlane_PointDistance
//=====================================================================================
float PLANE_CC grPlane_PointDistance(const grPlane *Plane, const grVec3d *Point)
{
   assert(Plane);
   assert(Point);

	return grVec3d_DotProduct(Point, &Plane->Normal) - Plane->Dist;
}

//=====================================================================================
//	grPlane_PointDistanceFast
//	NOTE - This assumes the plane is facing positive!  If not, you MUST reverse the 
//		returned distance!!!
//=====================================================================================
float PLANE_CC grPlane_PointDistanceFast(const grPlane *Plane, const grVec3d *Point)
{
   float	Dist;

   assert(Plane);
   assert(Point);

   switch (Plane->Type)
   {
	   case Type_X:
           Dist = (Point->X - Plane->Dist);
           break;
	   case Type_Y:
           Dist = (Point->Y - Plane->Dist);
           break;
	   case Type_Z:
           Dist = (Point->Z - Plane->Dist);
           break;
	      
       default:
           Dist = grVec3d_DotProduct(Point, &Plane->Normal) - Plane->Dist;
           break;
    }

    return Dist;
}


//=======================================================================================
//	grPlane_BoxSide
//=======================================================================================
grPlane_Side grPlane_BoxSide(const grPlane *Plane, const grExtBox *Box, grFloat Epsilon)
{
	grPlane_Side	Side;
	int32			i;
	grVec3d			Corners[2];
	grFloat			Dist1, Dist2;
	grVec3d			*Mins, *Maxs;

	assert(Plane);
	assert(Box);
		
	Mins = &((grExtBox*)Box)->Min;
	Maxs = &((grExtBox*)Box)->Max;

	// Axial planes are easy
	if (Plane->Type < 3)
	{
		Side = 0;

		if (grVec3d_GetElement(Maxs, Plane->Type) > Plane->Dist+Epsilon)
			Side |= PSIDE_FRONT;
		if (grVec3d_GetElement(Mins, Plane->Type) < Plane->Dist-Epsilon)
			Side |= PSIDE_BACK;

		return Side;
	}
	
	for (i=0 ; i<3 ; i++)
	{
		if (grVec3d_GetElement(&((grPlane*)Plane)->Normal, i) < 0)
		{
			grVec3d_SetElement(&Corners[0], i, grVec3d_GetElement(Mins, i));
			grVec3d_SetElement(&Corners[1], i, grVec3d_GetElement(Maxs, i));
		}
		else
		{
			grVec3d_SetElement(&Corners[1], i, grVec3d_GetElement(Mins, i));
			grVec3d_SetElement(&Corners[0], i, grVec3d_GetElement(Maxs, i));
		}
	}

	Dist1 = grVec3d_DotProduct(&Plane->Normal, &Corners[0]) - Plane->Dist;
	Dist2 = grVec3d_DotProduct(&Plane->Normal, &Corners[1]) - Plane->Dist;

	Side = 0;

	if (Dist1 >= Epsilon)
		Side = PSIDE_FRONT;
	if (Dist2 < Epsilon)
		Side |= PSIDE_BACK;

	return Side;
}

//====================================================================================
//	grPlane_Compare
//====================================================================================
grBoolean grPlane_Compare(const grPlane *Plane1, const grPlane *Plane2, float NEpsilon, float DEpsilon)
{
	assert(Plane1);
	assert(Plane2);

	if (fabs(Plane1->Normal.X - Plane2->Normal.X) < NEpsilon && 
		fabs(Plane1->Normal.Y - Plane2->Normal.Y) < NEpsilon && 
		fabs(Plane1->Normal.Z - Plane2->Normal.Z) < NEpsilon && 
		fabs(Plane1->Dist - Plane2->Dist) < DEpsilon)
			return GR_TRUE;

	return GR_FALSE;
}

