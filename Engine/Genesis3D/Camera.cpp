/****************************************************************************************/
/*  CAMERA.C                                                                            */
/*                                                                                      */
/*  Author:                                                                             */
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
#include <math.h>
#include <assert.h>

#include "Camera.h"
#include "Ram.h"
#include "Errorlog.h"
#include "List.h"
#include "Camera._h"


#ifndef max
#define max(AA,BB)  (  ((AA)>(BB)) ?(AA):(BB)  )
#endif
#ifndef min
#define min(AA,BB)  (  ((AA)<(BB)) ?(AA):(BB)  )
#endif

#define CAMERA_MINIMUM_PROJECTION_DISTANCE (0.010f)

//=====================================================================================
//	grCamera_Create
//=====================================================================================
GRAPI grCamera *GRCC grCamera_Create(grFloat FovRadians, const grRect *Rect)
{
	grCamera *Camera;

	assert( Rect != NULL );

	Camera = GR_RAM_ALLOCATE_STRUCT_CLEAR(grCamera);

	if (Camera == NULL)
	{
		grErrorLog_Add(-1, "grCamera_Create: CreateCamera failed");
		return NULL;
	}

	Camera->ZScale = 0.5f;

	grCamera_SetAttributes(Camera,FovRadians,Rect);

	// BEGIN - Far clip plane - paradoxnj MODIFIED 3/9/2005
	Camera->ZFarEnable = GR_TRUE;
	Camera->ZFar = 10000.0f;
	// END - Far clip plane - paradoxnj MODIFIED 3/9/2005

	return Camera;
}

//=====================================================================================
//	grCamera_Destroy
//=====================================================================================
GRAPI void GRCC grCamera_Destroy(grCamera **pCamera)
{
	assert( pCamera  != NULL );
	if ( *pCamera )
	{
	grCamera * Camera;
		Camera = *pCamera;

		if ( Camera->XFormStack )
		{
		grXForm3d * pXF;
			while( pXF = (grXForm3d *)Stack_Pop(Camera->XFormStack) )
			{
				grRam_Free(pXF);
			}
			Stack_Destroy(Camera->XFormStack);
		}

		grRam_Free(Camera);
	}
	*pCamera = NULL;
}

//
//	Camera XForm's
//

//=====================================================================================
//	grCamera_PushXForm
//=====================================================================================
GRAPI grBoolean GRCC grCamera_PushXForm(grCamera *Camera)
{
grXForm3d * pXF;
	assert( Camera );

	if ( ! List_Start() )
		return GR_FALSE;

	if ( ! Camera->XFormStack )
		if ( ! (Camera->XFormStack = Stack_Create()) )
			return GR_FALSE;

	pXF = (grXForm3d *)grRam_Allocate(sizeof(grXForm3d)*2);
	if ( ! pXF )
		return GR_FALSE;
	pXF[0] = Camera->TransposeXForm;
	pXF[1] = Camera->XForm;
	Stack_Push(Camera->XFormStack,(void *)pXF);

return GR_TRUE;
}

//=====================================================================================
//	grCamera_PopXForm
//=====================================================================================
GRAPI grBoolean GRCC grCamera_PopXForm( grCamera *Camera)
{
grXForm3d * pXF;
	assert( Camera );

	if ( ! Camera->XFormStack )
		return GR_FALSE;

	pXF = (grXForm3d *)Stack_Pop(Camera->XFormStack);
	if ( ! pXF )
		return GR_FALSE;

	Camera->TransposeXForm = pXF[0];
	Camera->XForm = pXF[1];
	grRam_Free(pXF);

	Camera->Pov = Camera->TransposeXForm.Translation;

	List_Stop();
return GR_TRUE;
}

//========================================================================================
//	grCamera_SetTransposeXForm
//========================================================================================
GRAPI grBoolean GRCC grCamera_SetTransposeXForm(grCamera *Camera, const grXForm3d *XForm)
{
	assert(Camera != NULL);
	assert(XForm != NULL);

	Camera->XForm = *XForm;		// Make a copy of the model XForm

	// Convert the model transform into a camera xform...
	if ( grXForm3d_IsOrthogonal(XForm) )
	{
		grXForm3d_GetTranspose(XForm, &Camera->TransposeXForm);
	}
	else
	{
		grXForm3d_GetInverse(XForm, &Camera->TransposeXForm);
	}

	Camera->Pov = Camera->TransposeXForm.Translation;

	return GR_TRUE;
}

//========================================================================================
//	grCamera_SetXForm
//========================================================================================
GRAPI grBoolean GRCC grCamera_SetXForm(grCamera *Camera, const grXForm3d *XForm)
{
	assert(Camera != NULL);
	assert(XForm != NULL);

	Camera->TransposeXForm = *XForm;		// Make a copy of the model XForm
	
	// Convert the model transform into a camera xform...
	grXForm3d_GetTranspose(XForm, &Camera->XForm);

	Camera->Pov = XForm->Translation;

	return GR_TRUE;
}

//========================================================================================
//	grCamera_GetXForm
// GetXForm returns the same thing passed to _SetXForm
//========================================================================================
GRAPI void GRCC grCamera_GetXForm( const grCamera *Camera,grXForm3d *pXForm)
{
	assert(Camera && pXForm);
	*pXForm = Camera->TransposeXForm;
}

//========================================================================================
//	grCamera_GetTransposeXForm
//========================================================================================
GRAPI void GRCC grCamera_GetTransposeXForm( const grCamera *Camera,grXForm3d *pXForm)
{
	assert(Camera && pXForm);
	*pXForm = Camera->XForm;
}

//========================================================================================
//	grCamera_XForm
//========================================================================================
const grXForm3d * GRCF grCamera_XForm( const grCamera *Camera)
{
	assert(Camera != NULL);
	return &(Camera->XForm);
}

//========================================================================================
//	grCamera_WorldXForm
//========================================================================================
const grXForm3d * GRCF grCamera_WorldXForm( const grCamera *Camera)
{
	assert(Camera != NULL);
	return &(Camera->TransposeXForm);
}

//
//	Misc Get/Set
//

//=====================================================================================
//	grCamera_GetClippingRect
//=====================================================================================
GRAPI void GRCC grCamera_GetClippingRect(const grCamera *Camera, grRect *Rect)
{
	assert( Camera != NULL );
	assert( Rect != NULL );
	Rect->Left   = (int32)Camera->Left;
	Rect->Right  = (int32)Camera->Right;
	Rect->Top    = (int32)Camera->Top;
	Rect->Bottom = (int32)Camera->Bottom;
}

//=====================================================================================
//	grCamera_GetPov
//=====================================================================================
const grVec3d *GRCF grCamera_GetPov(const grCamera *Camera)
{
	assert( Camera != NULL );
	return &(Camera->Pov);
}

GRAPI grVec3d *GRCC grCamera_GetPov2(grCamera *Camera)
{
	assert( Camera != NULL );
	return &(Camera->Pov);
}

//=====================================================================================
//	grCamera_GetWidthHeight
//=====================================================================================
void GRCF grCamera_GetWidthHeight(const grCamera *Camera,grFloat *Width,grFloat *Height)
{
	assert( Width  != NULL );
	assert( Height != NULL );
	assert( Camera != NULL );

	*Width  = Camera->Width;
	*Height = Camera->Height;
}
		
//=====================================================================================
//	grCamera_GetScale
//=====================================================================================
float GRCF grCamera_GetScale(const grCamera *Camera)
{
	assert( Camera != NULL );

	return Camera->Scale;
}


//=====================================================================================
//	grCamera_GetAttributes - Added by Jeff  02/09/05
//  Returns camera's FOV and Rect
//=====================================================================================
GRAPI void GRCC grCamera_GetAttributes(grCamera *Camera, grFloat *FovRadians, grRect *Rect)
{

	assert( Camera != NULL );
	assert( Rect != NULL );
	assert( FovRadians != NULL);

	*FovRadians = Camera->FovRadians;

	Rect->Left   = (int32)Camera->Left;
	Rect->Right  = (int32)Camera->Right;
	Rect->Top    = (int32)Camera->Top;
	Rect->Bottom = (int32)Camera->Bottom;
}

//=====================================================================================
//	grCamera_SetAttributes
//=====================================================================================
GRAPI void GRCC grCamera_SetAttributes(grCamera *Camera, grFloat FovRadians, const grRect *Rect)
{
	grFloat	Width, Height;
	grFloat	XRatio,YRatio;	
	grFloat Fov;

	assert (Camera != NULL);
	assert (Rect != NULL);
	assert ( FovRadians > 0.0f && FovRadians < GR_PI );

	Width  = (grFloat)(Rect->Right - Rect->Left); //+1.0f;
	Height = (grFloat)(Rect->Bottom - Rect->Top); //+1.0f;

	assert( Width > 0.0f  );
	assert( Height > 0.0f );

#define TOO_SMALL (0.0001f)		// width and Fov must be >= TOO_SMALL

	if (Width <=0.0f)
		Width = TOO_SMALL;				// Just in case
	if (Height <=0.0f)
		Height = TOO_SMALL;				// Just in case

	Camera->Width   = Width;
	Camera->Height  = Height;
	
	Camera->FovRadians	= FovRadians;

	Fov = 2.0f / (float)tan(FovRadians*0.5f);

	XRatio  = Width  / Fov;
	YRatio  = Height / Fov;
	
	Camera->Scale   = max(XRatio, YRatio);
	//Camera->YScale = Camera->XScale;


	Camera->Left    = (grFloat)Rect->Left;
	Camera->Right   = (grFloat)Rect->Right; // Jeff: removed -1
	Camera->Top     = (grFloat)Rect->Top;
	Camera->Bottom  = (grFloat)Rect->Bottom;  // Jeff: removed -1

	Camera->XCenter = Camera->Left + ( Width  * 0.5f ) - 0.5f;
	Camera->YCenter = Camera->Top  + ( Height * 0.5f ) - 0.5f;

/******

When we project to screen space, we scale up camera coords by multiplying 
by Scale. That means the maximum camera coord is

	 X = (Width * Z / Scale)
	 Y = (Height* Z / Scale)


*********/

	{
	double AngleX,AngleY;

		/**

		if Width > Height (as usual)

		then AngleX =  FovRadians/2

		and AngleY < AngleX

		**/

		AngleX =  atan(2.0f * Camera->Scale / Width);
		Camera->CosViewAngleX = (grFloat)cos(AngleX);
		Camera->SinViewAngleX = (grFloat)sin(AngleX);

		AngleY =  atan(2.0f * Camera->Scale / Height);
		Camera->CosViewAngleY = (grFloat)cos(AngleY);
		Camera->SinViewAngleY = (grFloat)sin(AngleY);
	}
}

//=====================================================================================
//	grCamera_SetZScale
//=====================================================================================
GRAPI void GRCC grCamera_SetZScale(grCamera *Camera, grFloat ZScale)
{
	assert(Camera);
	Camera->ZScale = ZScale;
}

//=====================================================================================
//	grCamera_GetZScale
//=====================================================================================
GRAPI grFloat GRCC grCamera_GetZScale(const grCamera *Camera)
{
	assert(Camera);
	return Camera->ZScale;
}

//=====================================================================================
//	grCamera_GetScreenProjection
//=====================================================================================
GRAPI void GRCC grCamera_GetScreenProjection(const grCamera *Camera, grFloat *Scale, grFloat *XCenter, grFloat *YCenter)
{
	assert(Camera);
	*Scale = Camera->Scale;
	*XCenter = Camera->XCenter;
	*YCenter = Camera->YCenter;
}

//=====================================================================================
//	grCamera_GetScreenSize
//=====================================================================================
GRAPI void GRCC grCamera_GetScreenSize(const grCamera *Camera, grFloat *Width, grFloat *Height)
{
	assert(Camera);
	*Width = Camera->Width;
	*Height = Camera->Height;
}

// BEGIN - Far clip plane - paradoxnj 2/9/2005
//=====================================================================================
//	grCamera_SetFarClipPlane
//=====================================================================================
GRAPI void GRCC grCamera_SetFarClipPlane(grCamera *Camera, grBoolean Enable, grFloat ZFar)
{
	assert(Camera != NULL);

	Camera->ZFarEnable = Enable;
	Camera->ZFar = ZFar;
}

//=====================================================================================
//	grCamera_GetFarClipPlane
//=====================================================================================
GRAPI void GRCC grCamera_GetFarClipPlane(const grCamera *Camera, grBoolean *Enable, grFloat *ZFar)
{
	assert(Camera != NULL);

	*Enable = Camera->ZFarEnable;
	*ZFar = Camera->ZFar;
}
// END - Far clip plane - paradoxnj 2/9/2005

//
//	Camera Transform/Project
//

//========================================================================================
//	grCamera_ScreenPointToWorld
//========================================================================================
GRAPI void GRCC grCamera_ScreenPointToWorld(	const grCamera	*Camera,
														int32			 ScreenX,
														int32			 ScreenY,
														grVec3d			*Vector)
// Takes a screen X and Y pair, and a camera and generates a vector pointing
// in the direction from the camera position to the screen point.
{
	grVec3d In,Left,Up;
	grVec3d ScaledIn,ScaledLeft,ScaledUp ;
	float	XCenter ;
	float	YCenter ;
	float	Scale ;
	const grXForm3d *pM;

	pM = &(Camera->TransposeXForm);
	XCenter = Camera->XCenter ;
	YCenter = Camera->YCenter ;
	Scale   = Camera->Scale ;

	grXForm3d_GetIn( pM, &In ) ;
	grXForm3d_GetLeft( pM, &Left ) ;
	grXForm3d_GetUp( pM, &Up ) ;
	
	grVec3d_Scale(&In,   Scale, &ScaledIn);
	grVec3d_Scale(&Left, XCenter - ((grFloat)ScreenX), &ScaledLeft );
	grVec3d_Scale(&Up,   YCenter - ((grFloat)ScreenY), &ScaledUp   );

	grVec3d_Copy(&ScaledIn, Vector);
	grVec3d_Add(Vector,		&ScaledLeft,	Vector );
	grVec3d_Add(Vector,		&ScaledUp,		Vector );
	grVec3d_Normalize(Vector);
}


//========================================================================================
//	grCamera_Project
//========================================================================================
GRAPI void GRCC grCamera_Project(	const grCamera	*Camera, 
											const grVec3d	*PointInCameraSpace, 
											grVec3d			*ProjectedPoint)
	// project from camera space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
{
	grFloat Z;

	assert( Camera != NULL );
	assert( PointInCameraSpace != NULL );
	assert( ProjectedPoint != NULL );

	Z = -PointInCameraSpace->Z;   

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ProjectedPoint->Z = Z*Camera->ZScale;

	Z = Camera->Scale / Z;

	ProjectedPoint->X = Camera->XCenter + ( PointInCameraSpace->X * Z );
	ProjectedPoint->Y = Camera->YCenter - ( PointInCameraSpace->Y * Z );
}

//========================================================================================
//	grCamera_ProjectArray
//========================================================================================
GRAPI void GRCC grCamera_ProjectArray(const grCamera	*Camera, 
												const grVec3d	*FmPoints, 
												int32			FmStride,
												grVec3d			*ToPoints, 
												int32			ToStride, 
												int32			Count)
{
float Scale,XCenter,YCenter;
float Z,ZScale;

	assert( Camera != NULL );
	assert( FmPoints != NULL );
	assert( ToPoints != NULL );

	Scale = Camera->Scale;
	ZScale = Camera->ZScale;
	XCenter = Camera->XCenter;
	YCenter = Camera->YCenter;

	if ( Count & 1 )	// catch the odd one
	{
		Z = - FmPoints->Z;  
		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);
		ToPoints->Z = Z * ZScale;
		Z = Scale / Z;
		ToPoints->X = XCenter + ( FmPoints->X * Z );
		ToPoints->Y = YCenter - ( FmPoints->Y * Z );
		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
	}

	Count >>= 1;	// do two at a time!

	while(Count--)
	{

		// ProjectArray is a bottleneck!
		#pragma message("Camera : ProjectArray needs assembly!")
		// this is currently taking as much time as the XFormArray,
		//	but we have no fancy assembly versions of this!!

		Z = - FmPoints->Z;   

		// use FCMOV!! Critical !!
		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

		ToPoints->Z = Z * ZScale;

		Z = Scale / Z;

		// parrallelize these!
		//	the optimizer doesn't do it!
		ToPoints->X = XCenter + ( FmPoints->X * Z );
		ToPoints->Y = YCenter - ( FmPoints->Y * Z );

		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
		
		// AND AGAIN:

		Z = - FmPoints->Z;   
		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);
		ToPoints->Z = Z * ZScale;
		Z = Scale / Z;
		ToPoints->X = XCenter + ( FmPoints->X * Z );
		ToPoints->Y = YCenter - ( FmPoints->Y * Z );
		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
	}
}

//========================================================================================
//	grCamera_ProjectAndClampArray
//========================================================================================
GRAPI void GRCC grCamera_ProjectAndClampArray(const grCamera	*Camera, 
														const grVec3d	*FmPoints, 
														int32			FmStride,
														grVec3d			*ToPoints, 
														int32			ToStride, 
														int32			Count)
{
float Scale,XCenter,YCenter;
float X,Y,Z,ZScale;

	assert( Camera != NULL );
	assert( FmPoints != NULL );
	assert( ToPoints != NULL );

	Scale = Camera->Scale;
	ZScale = Camera->ZScale;
	XCenter = Camera->XCenter;
	YCenter = Camera->YCenter;

	if ( Count & 1 )	// catch the odd one
	{
		Z = - FmPoints->Z;  
		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);
		ToPoints->Z = Z * ZScale;
		Z = Scale / Z;

		X = XCenter + ( FmPoints->X * Z );
		ToPoints->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);
		Y = YCenter - ( FmPoints->Y * Z );
		ToPoints->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);

		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
	}

	Count >>= 1;	// do two at a time!

	// see optimize notes in ProjectArray

	while(Count--)
	{
		Z = - FmPoints->Z;   

		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

		ToPoints->Z = Z * ZScale;

		Z = Scale / Z;

		X = XCenter + ( FmPoints->X * Z );
		ToPoints->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);

		Y = YCenter - ( FmPoints->Y * Z );
		ToPoints->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);

		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
		
		// AND AGAIN:

		Z = - FmPoints->Z;   
		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);
		ToPoints->Z = Z * ZScale;
		Z = Scale / Z;
		X = XCenter + ( FmPoints->X * Z );
		ToPoints->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);
		Y = YCenter - ( FmPoints->Y * Z );
		ToPoints->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);
		FmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		ToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
	}
}
//========================================================================================
//	grCamera_ProjectZ
//========================================================================================
GRAPI void GRCC grCamera_ProjectZ(const grCamera	*Camera, 
											const grVec3d	*PointInCameraSpace, 
											grVec3d			*ProjectedPoint)
	// project from camera space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
	// projected point.z is set to 1/Z
{
	grFloat OneOverZ;
	grFloat ScaleOverZ;
	grFloat Z;
	assert( Camera != NULL );
	assert( PointInCameraSpace != NULL );
	assert( ProjectedPoint != NULL );

	Z = -PointInCameraSpace->Z;   
	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	OneOverZ = 1.0f / Z;
	ScaleOverZ = Camera->Scale *  (OneOverZ);

	ProjectedPoint->Z = Camera->ZScale*OneOverZ;   

	ProjectedPoint->X = ( PointInCameraSpace->X * ScaleOverZ ) + Camera->XCenter;
	
	ProjectedPoint->Y = Camera->YCenter - ( PointInCameraSpace->Y * ScaleOverZ );
}




//========================================================================================
//	grCamera_ProjectAndClamp
//========================================================================================
GRAPI void GRCC grCamera_ProjectAndClamp(const grCamera	*Camera, 
										const grVec3d	*PointInCameraSpace, 
										grVec3d			*ProjectedPoint)
	// project from camera space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
	// points outside the clipping rect are clamped to the clipping rect
{
	grFloat ScaleOverZ;
	grFloat X,Y,Z;
	assert( Camera != NULL );
	assert( PointInCameraSpace != NULL );
	assert( ProjectedPoint != NULL );

	Z = -PointInCameraSpace->Z;   

	if (Z < CAMERA_MINIMUM_PROJECTION_DISTANCE)
	{
		Z = CAMERA_MINIMUM_PROJECTION_DISTANCE; 
	}

	ScaleOverZ = Camera->Scale / Z;

	ProjectedPoint->Z = Z*Camera->ZScale;   

	X = ( PointInCameraSpace->X * ScaleOverZ ) + Camera->XCenter;
	
	ProjectedPoint->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);
	
	Y = Camera->YCenter - ( PointInCameraSpace->Y * ScaleOverZ );

	ProjectedPoint->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);
}

//========================================================================================
//	grCamera_GetViewAngleXSinCos
//========================================================================================
void GRCF grCamera_GetViewAngleXSinCos( const grCamera *Camera, grFloat *SinAngle, grFloat *CosAngle )
{
	assert( Camera != NULL );
	assert( SinAngle );
	assert( CosAngle );
	*SinAngle = Camera->SinViewAngleX;
	*CosAngle = Camera->CosViewAngleX;
}

//========================================================================================
//	grCamera_GetViewAngleYSinCos
//========================================================================================
void GRCF grCamera_GetViewAngleYSinCos( const grCamera *Camera, grFloat *SinAngle, grFloat *CosAngle )
{
	assert( Camera != NULL );
	assert( SinAngle );
	assert( CosAngle );
	*SinAngle = Camera->SinViewAngleY;
	*CosAngle = Camera->CosViewAngleY;
}

//========================================================================================
//	grCamera_Transform
//========================================================================================
GRAPI void GRCC grCamera_Transform(	const grCamera	*Camera, 
												const grVec3d	*WorldSpacePoint, 
												grVec3d			*CameraSpacePoint)
{
	assert( Camera );
	assert( WorldSpacePoint );
	assert( CameraSpacePoint );

	// would be better if xform3d_transform was assembly, or a macro, or anything

	grXForm3d_Transform(&(Camera->XForm),WorldSpacePoint,CameraSpacePoint);
}


//========================================================================================
//	grCamera_TransformVecArray
//========================================================================================
GRAPI void GRCC grCamera_TransformVecArray(	const grCamera	*Camera, 
														const grVec3d	*WorldSpacePointPtr, 
														grVec3d			*CameraSpacePointPtr,
														int32			Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( CameraSpacePointPtr );

	grXForm3d_TransformVecArray(&(Camera->XForm), WorldSpacePointPtr,CameraSpacePointPtr,Count);
}

//========================================================================================
//	grCamera_TransformAndProjectVecArray
//========================================================================================
GRAPI void GRCC grCamera_TransformAndProjectVecArray(	const grCamera *Camera, 
																const grVec3d *WorldSpacePointPtr, 
																grVec3d *ProjectedSpacePointPtr,
																int32 Count)
{
	grCamera_TransformAndProjectArray(	Camera, 
										WorldSpacePointPtr, 
										sizeof(grVec3d),
										ProjectedSpacePointPtr,
										sizeof(grVec3d),
										Count);
}

//========================================================================================
//	grCamera_TransformAndProjectArray
//========================================================================================

#if 1	// <> use the assembly XFormArray
		// can't tell the difference!

GRAPI void GRCC grCamera_TransformAndProjectArray(const grCamera	*Camera, 
															const grVec3d	*WorldSpacePointPtr, 
															int32			WorldStride,
															grVec3d			*ProjectedSpacePointPtr, 
															int32			ProjectedStride,
															int32			Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( ProjectedSpacePointPtr );

	grXForm3d_TransformArray(	&(Camera->XForm),
								WorldSpacePointPtr, WorldStride,
								ProjectedSpacePointPtr,ProjectedStride,
								Count);

	grCamera_ProjectArray(	Camera,
							ProjectedSpacePointPtr,
							ProjectedStride,
							ProjectedSpacePointPtr,
							ProjectedStride, 
							Count);
}

//========================================================================================================
//	grCamera_TransformAndProjectAndClampArray
//========================================================================================================
GRAPI void GRCC grCamera_TransformAndProjectAndClampArray(const	grCamera	*Camera, 
															const grVec3d	*WorldSpacePointPtr, 
															int32			WorldStride,
															grVec3d			*ProjectedSpacePointPtr, 
															int32			ProjectedStride,
															int32			Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( ProjectedSpacePointPtr );

	grXForm3d_TransformArray(	&(Camera->XForm),
								WorldSpacePointPtr, WorldStride,
								ProjectedSpacePointPtr,ProjectedStride,
								Count);

	grCamera_ProjectAndClampArray(	Camera,
							ProjectedSpacePointPtr,
							ProjectedStride,
							ProjectedSpacePointPtr,
							ProjectedStride, 
							Count);
}
#else // let the compiler do its best

//========================================================================================================
//	grCamera_TransformAndProjectArray
//========================================================================================================
GRAPI void GRCC grCamera_TransformAndProjectArray(const grCamera	*Camera, 
															const grVec3d	*InFmPoints, 
															int32			FmStride,
															grVec3d			*InToPoints, 
															int32			ToStride,
															int32			Count)
{
float Scale,ZScale,XCenter,YCenter;
grXForm3d XF;
const grVec3d	*NextFmPoints,*FmPoints; 
grVec3d			*NextToPoints,*ToPoints;
int c;

	assert( Camera );
	assert( FmPoints );
	assert( ToPoints );

	Scale   = Camera->Scale;
	ZScale  = Camera->ZScale;
	XCenter = Camera->XCenter;
	YCenter = Camera->YCenter;
	XF		= Camera->XForm;
	FmPoints= InFmPoints;
	ToPoints= InToPoints;

	c = Count;
	while(c--)
	{
	grVec3d Point;
	float Z;

		NextFmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		NextToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
		// prefetch !

		{
		float X,Y,Z;
			X = FmPoints->X;
			Y = FmPoints->Y;
			Z = FmPoints->Z;
			Point.X = (X * XF.AX) + (Y * XF.AY) + (Z * XF.AZ) + XF.Translation.X;
			Point.Y = (X * XF.BX) + (Y * XF.BY) + (Z * XF.BZ) + XF.Translation.Y;
			Point.Z = (X * XF.CX) + (Y * XF.CY) + (Z * XF.CZ) + XF.Translation.Z;
		}

		Z = - Point.Z;   

		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

		ToPoints->Z = Z * ZScale;

		Z = Scale / Z;

		ToPoints->X = XCenter + ( Point.X * Z );
		ToPoints->Y = YCenter - ( Point.Y * Z );

		FmPoints = NextFmPoints;
		ToPoints = NextToPoints;
	}
}

//========================================================================================================
//	grCamera_TransformAndProjectAndClampArray
//========================================================================================================
GRAPI void GRCC grCamera_TransformAndProjectAndClampArray(const grCamera	*Camera, 
																	const grVec3d	*InFmPoints, 
																	int32			FmStride,
																	grVec3d			*InToPoints, 
																	int32			ToStride,
																	int32			Count)
{
float Scale,ZScale,XCenter,YCenter;
grXForm3d XF;
const grVec3d	*NextFmPoints,*FmPoints; 
grVec3d			*NextToPoints,*ToPoints;
int c;

	assert( Camera );
	assert( FmPoints );
	assert( ToPoints );

	Scale   = Camera->Scale;
	ZScale  = Camera->ZScale;
	XCenter = Camera->XCenter;
	YCenter = Camera->YCenter;
	XF		= Camera->XForm;
	FmPoints= InFmPoints;
	ToPoints= InToPoints;

	c = Count;
	while(c--)
	{
	grVec3d Point;
	float X,Y,Z;

		NextFmPoints = (const grVec3d *)(((uint32)FmPoints) + FmStride);
		NextToPoints = (      grVec3d *)(((uint32)ToPoints) + ToStride);
		// prefetch !

		{
		float X,Y,Z;
			X = FmPoints->X;
			Y = FmPoints->Y;
			Z = FmPoints->Z;
			Point.X = (X * XF.AX) + (Y * XF.AY) + (Z * XF.AZ) + XF.Translation.X;
			Point.Y = (X * XF.BX) + (Y * XF.BY) + (Z * XF.BZ) + XF.Translation.Y;
			Point.Z = (X * XF.CX) + (Y * XF.CY) + (Z * XF.CZ) + XF.Translation.Z;
		}

		Z = - Point.Z;   

		Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

		ToPoints->Z = Z * ZScale;

		Z = Scale / Z;

		X = XCenter + ( Point.X * Z );
		Y = YCenter - ( Point.Y * Z );

		ToPoints->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);
		ToPoints->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);

		FmPoints = NextFmPoints;
		ToPoints = NextToPoints;
	}
}

#endif

//========================================================================================
//	grCamera_TransformAndProjectLArray
//========================================================================================
GRAPI void GRCC grCamera_TransformAndProjectLArray(	const grCamera	*Camera, 
																const grLVertex	*WorldSpacePointPtr, 
																grTLVertex		*ProjectedSpacePointPtr,
																int32			Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( ProjectedSpacePointPtr );

	while(Count--)
	{
		grCamera_TransformAndProjectL(Camera,WorldSpacePointPtr++,ProjectedSpacePointPtr++);
	}
}

//========================================================================================
//	grCamera_TransformLArray
//	Tansforms a point to camera space
//========================================================================================
GRAPI void GRCC grCamera_TransformLArray(	const grCamera	*Camera, 
													const grLVertex	*WorldSpacePointPtr, 
													grLVertex		*CameraSpacePointPtr,
													int32			Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( CameraSpacePointPtr );

	while(Count--)
	{
		grCamera_TransformL(Camera, WorldSpacePointPtr++, CameraSpacePointPtr++);
	}
}

//========================================================================================
//	grCamera_ProjectAndClampLArray
//	Take a CameraSpace point array, and projects them flat onto the camera plane
//========================================================================================
GRAPI void GRCC grCamera_ProjectAndClampLArray(	const grCamera		*Camera, 
															const grLVertex		*CameraSpacePointPtr, 
															grTLVertex			*ProjectedSpacePointPtr,
															int32				Count)
{
	assert( Camera );
	assert( CameraSpacePointPtr );
	assert( ProjectedSpacePointPtr );

	while(Count--)
	{
		grCamera_ProjectAndClampL(Camera, CameraSpacePointPtr++,ProjectedSpacePointPtr++);
	}
}

//========================================================================================
//	grCamera_TransformAndProjectAndClampLArray
//========================================================================================
GRAPI void GRCC grCamera_TransformAndProjectAndClampLArray(	const grCamera		*Camera, 
																		const grLVertex		*WorldSpacePointPtr, 
																		grTLVertex			*ProjectedSpacePointPtr,
																		int32				Count)
{
	assert( Camera );
	assert( WorldSpacePointPtr );
	assert( ProjectedSpacePointPtr );

	while(Count--)
	{
		grCamera_TransformAndProjectAndClampL(Camera,WorldSpacePointPtr++,ProjectedSpacePointPtr++);
	}
}

//============================================================================================
//	grCamera_TransformAndProject
//============================================================================================
GRAPI void GRCC grCamera_TransformAndProject(	const	grCamera *Camera,
														const	grVec3d *Point, 
														grVec3d	*ProjectedPoint)
	// project from *WORLD* space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
{
	grFloat Z;

	assert( Camera );
	assert( Point );
	assert( ProjectedPoint );

	grXForm3d_Transform(&(Camera->XForm),Point,ProjectedPoint);

	Z = - ProjectedPoint->Z;

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ProjectedPoint->Z = Z*Camera->ZScale;

	Z = Camera->Scale / Z;

	ProjectedPoint->X =   ( ProjectedPoint->X * Z ) + Camera->XCenter;
	ProjectedPoint->Y = - ( ProjectedPoint->Y * Z ) + Camera->YCenter;
}

GRAPI void GRCC grCamera_TransformAndProjectAndClamp(	const	grCamera *Camera,
																const	grVec3d *Point, 
																grVec3d	*ProjectedPoint)
	// project from *WORLD* space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
{
	grFloat X,Y,Z;

	assert( Camera );
	assert( Point );
	assert( ProjectedPoint );

	grXForm3d_Transform(&(Camera->XForm),Point,ProjectedPoint);

	Z = - ProjectedPoint->Z;

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ProjectedPoint->Z = Z*Camera->ZScale;

	Z = Camera->Scale / Z;

	X =   ( ProjectedPoint->X * Z ) + Camera->XCenter;

	ProjectedPoint->X = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);

	Y = - ( ProjectedPoint->Y * Z ) + Camera->YCenter;
		
	ProjectedPoint->Y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);
}


//============================================================================================
//	grCamera_TransformAndProjectL
//============================================================================================
GRAPI void GRCC grCamera_TransformAndProjectL(const		grCamera *Camera,
														const		grLVertex *Point, 
														grTLVertex	*ProjectedPoint)
	// project from *WORLD* space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
{
	grFloat ScaleOverZ;
	grFloat Z;

	assert( Camera );
	assert( Point );
	assert( ProjectedPoint );

	grXForm3d_Transform(&(Camera->XForm),(grVec3d *)Point,(grVec3d *)ProjectedPoint);

	Z = - ProjectedPoint->z;

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ScaleOverZ = Camera->Scale / Z;

	ProjectedPoint->z = Z*Camera->ZScale;
	ProjectedPoint->x =   ( ProjectedPoint->x * ScaleOverZ ) + Camera->XCenter;
	ProjectedPoint->y = - ( ProjectedPoint->y * ScaleOverZ ) + Camera->YCenter;

	ProjectedPoint->u = Point->u;
	ProjectedPoint->v = Point->v;
	ProjectedPoint->r = Point->r;
	ProjectedPoint->g = Point->g;
	ProjectedPoint->b = Point->b;
	ProjectedPoint->a = Point->a;
}


//========================================================================================================
//	grCamera_TransformL
//========================================================================================================
GRAPI void GRCC grCamera_TransformL(	const grCamera	*Camera,
												const grLVertex *Point, 
												grLVertex		*TransformedPoint)
{
	assert( Camera );
	assert( Point );
	assert( TransformedPoint );

	grXForm3d_Transform(&(Camera->XForm),(grVec3d *)Point,(grVec3d *)TransformedPoint);

	// This kind of sucks, sigh...
	TransformedPoint->u = Point->u;
	TransformedPoint->v = Point->v;
	TransformedPoint->r = Point->r;
	TransformedPoint->g = Point->g;
	TransformedPoint->b = Point->b;
	TransformedPoint->a = Point->a;
}


//========================================================================================================
//	grCamera_ProjectAndClampL
//========================================================================================================
GRAPI void GRCC grCamera_ProjectAndClampL(const grCamera	*Camera,
													const grLVertex *Point, 
													grTLVertex		*ProjectedPoint)
{
	grFloat ScaleOverZ;
	grFloat X,Y,Z;

	Z = - Point->Z;

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ScaleOverZ = Camera->Scale / Z;

	ProjectedPoint->z = Z*Camera->ZScale;

	X =   ( Point->X * ScaleOverZ ) + Camera->XCenter;
	ProjectedPoint->x = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);

	Y = - ( Point->Y * ScaleOverZ ) + Camera->YCenter;
	ProjectedPoint->y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);

	ProjectedPoint->u = Point->u;
	ProjectedPoint->v = Point->v;
	ProjectedPoint->r = Point->r;
	ProjectedPoint->g = Point->g;
	ProjectedPoint->b = Point->b;
	ProjectedPoint->a = Point->a;
}

//========================================================================================================
//	grCamera_TransformAndProjectAndClampL
//========================================================================================================
GRAPI void GRCC grCamera_TransformAndProjectAndClampL(const		grCamera *Camera,
																const		grLVertex *Point, 
																grTLVertex	*ProjectedPoint)
	// project from *WORLD* space to projected space
	// projected space is not right-handed.
	// projection is onto x-y plane  x is right, y is down, z is in
{
	grFloat ScaleOverZ;
	grFloat X,Y,Z;

	assert( Camera );
	assert( Point );
	assert( ProjectedPoint );

	grXForm3d_Transform(&(Camera->XForm),(grVec3d *)Point,(grVec3d *)ProjectedPoint);

	Z = - ProjectedPoint->z;

	Z = max(Z,CAMERA_MINIMUM_PROJECTION_DISTANCE);

	ScaleOverZ = Camera->Scale / Z;

	ProjectedPoint->z = Z*Camera->ZScale;
	X =   ( ProjectedPoint->x * ScaleOverZ ) + Camera->XCenter;
	
	ProjectedPoint->x = GR_CLAMP(X,Camera->Left-0.5f,Camera->Right+0.5f);

	Y = - ( ProjectedPoint->y * ScaleOverZ ) + Camera->YCenter;
		
	ProjectedPoint->y = GR_CLAMP(Y,Camera->Top-0.5f,Camera->Bottom+0.5f);

	ProjectedPoint->u = Point->u;
	ProjectedPoint->v = Point->v;
	ProjectedPoint->r = Point->r;
	ProjectedPoint->g = Point->g;
	ProjectedPoint->b = Point->b;
	ProjectedPoint->a = Point->a;
}

