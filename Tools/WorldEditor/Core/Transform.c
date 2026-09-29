/****************************************************************************************/
/*  TRANSFORM.C                                                                         */
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
#include <Assert.h>

#include "Util.h"
#include "Ram.h"
#include "Errorlog.h"

#include "Transform.h"

#define GEXFORM3D_SCALE_TOLERANCE (0.00001f)

typedef struct tagMoveBrushInfo
{
	Level			*	pLevel ;
	grExtBox		*	pWorldBounds ;
	const grVec3d	*	pWorldDistance ;
} MoveBrushInfo ;

typedef struct tagRotateBrushInfo
{
	Level			*	pLevel ;
	grExtBox		*	pWorldBounds ;
	ORTHO_AXIS			RAxis ;
	grVec3d				RotationCenter ;
	grFloat				fRadianAngle ;
} RotateBrushInfo ;

typedef struct tagShearBrushInfo
{
	Level			*	pLevel ;
	grExtBox		*	pWorldBounds ;
	SELECT_HANDLE		eSizeType;
	ORTHO_AXIS			HAxis;
	ORTHO_AXIS			VAxis;
	const grVec3d	*	pWorldDistance ;
	const grExtBox	*	pSelectedBounds ;
} ShearBrushInfo ;

typedef struct tagSizeBrushInfo
{
	Level			*	pLevel ;
	grExtBox		*	pWorldBounds ;
	const grVec3d	*	pWorldDistance ;
	ORTHO_AXIS			HAxis ;
	ORTHO_AXIS			VAxis ;
	SELECT_HANDLE		eSizeType ;
	grFloat				fHScale ;
	grFloat				fVScale ;
	const grExtBox	*	pSelectedBounds ;
} SizeBrushInfo ;


typedef struct tagSnapBrushInfo
{
	Level			*	pLevel ;
	grExtBox		*	pWorldBounds ;
	grFloat				fSnapSize ;
} SnapBrushInfo ;


static grBoolean Transform_MoveObject( Object * pObject, void * lParam )
{
	MoveBrushInfo * pmbi ;
	grExtBox		WorldBounds ;
	
	pmbi = (MoveBrushInfo*)lParam ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( pmbi->pWorldBounds, &WorldBounds, pmbi->pWorldBounds ) ;
	Object_Move( pObject, pmbi->pWorldDistance ) ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( pmbi->pWorldBounds, &WorldBounds, pmbi->pWorldBounds ) ;

	return GR_TRUE ;
}// Select_DeselectBrush

static grBoolean Transform_ShearObject( Object * pObject, void * lParam )
{
	ShearBrushInfo * psbi ;
	grExtBox		WorldBounds ;
	
	psbi = (ShearBrushInfo*)lParam ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( psbi->pWorldBounds, &WorldBounds, psbi->pWorldBounds ) ;
	Object_Shear( pObject, psbi->pWorldDistance, psbi->eSizeType, psbi->HAxis, 
		psbi->VAxis, psbi->pSelectedBounds ) ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( psbi->pWorldBounds, &WorldBounds, psbi->pWorldBounds ) ;

	return GR_TRUE ;
}// Select_DeselectBrush

static grBoolean Transform_RotateObject( Object * pObject, void * lParam )
{
	RotateBrushInfo * prbi ;
	grExtBox		WorldBounds ;
	
	prbi = (RotateBrushInfo*)lParam ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( prbi->pWorldBounds, &WorldBounds, prbi->pWorldBounds ) ;
	Object_Rotate( pObject, prbi->RAxis, prbi->fRadianAngle, &prbi->RotationCenter ) ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( prbi->pWorldBounds, &WorldBounds, prbi->pWorldBounds ) ;

	return GR_TRUE ;
}// Transform_RotateBrush


static grBoolean Transform_SizeObject( Object * pObject, void * lParam )
{
	SizeBrushInfo * psbi ;
	grExtBox		WorldBounds ;
	
	psbi = (SizeBrushInfo*)lParam ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( psbi->pWorldBounds, &WorldBounds, psbi->pWorldBounds ) ;
	Object_Size( pObject, psbi->pSelectedBounds, psbi->fHScale, psbi->fVScale, psbi->eSizeType, psbi->HAxis, psbi->VAxis ) ;
	if( !Object_GetWorldDrawBounds( pObject, &WorldBounds ) )
		return( GR_FALSE );
	Util_ExtBox_Union( psbi->pWorldBounds, &WorldBounds, psbi->pWorldBounds ) ;

	return GR_TRUE ;
}// Select_DeselectBrush



// Snap to a multiple of the snap size
static grFloat Transform_SnapCoord( grFloat fCoord, grFloat fSnapSize )
{
	grFloat fRemainder ;

	fRemainder = (grFloat) fmod( fCoord, fSnapSize ) ;
	if( fabs( fRemainder ) < (fSnapSize *0.5f) )
	{
		return fRemainder ;		
	}
	else
	{
		if( fCoord < 0.0f )
		{
			return fSnapSize + fRemainder ;
		}
		else
		{
			return -(fSnapSize - fRemainder) ;
		}
	}
}// Transform_SnapCoord

// Snap to the nearest grid line
static grFloat Transform_SnapCoordLR( grFloat fCoord, grFloat fSnapSize )
{
	grFloat fRemainder ;

	fRemainder = (grFloat) fmod( fCoord, fSnapSize ) ;
	if( fabs( fRemainder ) < (fSnapSize/2.0f) )
	{
		return fRemainder ;		
	}
	else
	{
		if( fCoord < 0.0f )
		{
//			return -(fSnapSize + fRemainder) ;
			return fSnapSize + fRemainder ;
		}
		else
		{
//			return fSnapSize - fRemainder ;
			return fRemainder - fSnapSize ;
		}
	}
}// Transform_Snap

static void Transform_Snap( grFloat fMin, grFloat fMax, grFloat fSnapSize, grFloat * pResult )
{
	grFloat fSide1 ;
	grFloat fSide2 ;

	fSide1 = Transform_SnapCoordLR( fMin, fSnapSize ) ;
	fSide2 = Transform_SnapCoordLR( fMax, fSnapSize ) ;
	
	if( fabs( fSide1 ) < fabs( fSide2 ) )
		*pResult = fSide1 ;
	else
		*pResult = fSide2 ;
}// Transform_Snap


//
// END STATIC
//

void Transform_MoveSelected( Level * pLevel, const grVec3d * pWorldDistance, grExtBox * pWorldBounds )
{
	MoveBrushInfo	mbi ;

	assert( pLevel != NULL ) ;
	assert( pWorldDistance != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	mbi.pWorldBounds = pWorldBounds ;
	mbi.pLevel = pLevel ;
	mbi.pWorldDistance = pWorldDistance ;

	Level_EnumSelected( pLevel, &mbi, Transform_MoveObject ) ;
	Level_SetModifiedSelection( pLevel ) ;

}// Transform_MoveSelected

void Transform_MoveSelectedSub( Level * pLevel, const grVec3d * pWorldDistance, grExtBox * pWorldBounds )
{
	MoveBrushInfo	mbi ;

	assert( pLevel != NULL ) ;
	assert( pWorldDistance != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	mbi.pWorldBounds = pWorldBounds ;
	mbi.pLevel = pLevel ;
	mbi.pWorldDistance = pWorldDistance ;

	Level_EnumSubSelected( pLevel, &mbi, Transform_MoveObject ) ;
	Level_SetModifiedSelection( pLevel ) ;

}// Transform_MoveSelectedSub

void Transform_ShearSelected( Level * pLevel, const grVec3d * pWorldDistance,  SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds )
{
	ShearBrushInfo	sbi ;

	assert( pLevel != NULL ) ;
	assert( pWorldDistance != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	sbi.pWorldBounds = pWorldBounds ;
	sbi.pLevel = pLevel ;
	sbi.pWorldDistance = pWorldDistance ;
	sbi.eSizeType = eSizeType;
	sbi.HAxis = HAxis;
	sbi.VAxis = VAxis;
	sbi.pSelectedBounds = Level_GetSelDrawBounds( pLevel ) ;
	Level_EnumSelected( pLevel, &sbi, Transform_ShearObject ) ;
	Level_SetModifiedSelection( pLevel ) ;

}// Transform_MoveSelected

void Transform_RotateSelected( Level * pLevel, grFloat fRadianAngle, ORTHO_AXIS RAxis, grVec3d *pCenter3d, grExtBox * pWorldBounds )
{
	RotateBrushInfo	rbi ;
	assert( pLevel != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	rbi.pLevel = pLevel ;
	rbi.pWorldBounds = pWorldBounds ;
	rbi.RAxis = RAxis ;
	rbi.fRadianAngle = fRadianAngle ;
	rbi.RotationCenter = *pCenter3d ;

	Level_EnumSelected( pLevel, &rbi, Transform_RotateObject) ;
	Level_SetModifiedSelection( pLevel ) ;
}// Transform_RotateSelected

void Transform_RotateSubSelected( Level * pLevel, grFloat fRadianAngle, ORTHO_AXIS RAxis, grExtBox * pWorldBounds )
{
	RotateBrushInfo	rbi ;
	const grExtBox * pSubDrawBounds;

	assert( pLevel != NULL ) ;
	assert( pWorldBounds != NULL ) ;
	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	rbi.pLevel = pLevel ;
	rbi.pWorldBounds = pWorldBounds ;
	rbi.RAxis = RAxis ;
	rbi.fRadianAngle = fRadianAngle ;
	pSubDrawBounds = Level_GetSubSelDrawBounds( pLevel );
	grExtBox_GetTranslation( pSubDrawBounds, &rbi.RotationCenter ) ; 

	Level_EnumSubSelected( pLevel, &rbi, Transform_RotateObject) ;
	Level_SetModifiedSelection( pLevel ) ;
}// Transform_RotateSubSelected

#define TRANSFORM_MIN_EXTENT	(0.001f)

static grFloat Transform_CalcScale( const grExtBox * pWorldBounds, grBoolean bHorzAxis, ORTHO_AXIS Axis,  const grVec3d * pWorldDistance, SELECT_HANDLE eSizeType )
{
	grFloat fExtent;
	grFloat fScale = 1.0;
	fExtent = Util_geExtBox_GetExtent( pWorldBounds, Axis ) ;
	if( fExtent < TRANSFORM_MIN_EXTENT )
		return 1.0f;

	switch( eSizeType )
	{
	case Select_Top :
		if( bHorzAxis )
			fScale = 1.0f;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	case Select_Bottom :
		if( bHorzAxis )
			fScale = 1.0f;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	case Select_Left :
		if( bHorzAxis )
			fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			fScale = 1.0f;
		break ;

	case Select_Right :
		if( bHorzAxis )
			fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			fScale = 1.0f;
		break ;

	case Select_TopLeft :
		if( bHorzAxis )
			fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	case Select_TopRight :
		if( bHorzAxis )
			fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	case Select_BottomLeft :
		if( bHorzAxis )
			fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	
	case Select_BottomRight :
		if( bHorzAxis )
			fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		else
			if( Axis == Ortho_Axis_Z )
				fScale = ( fExtent + grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
			else
				fScale = ( fExtent - grVec3d_GetElement(pWorldDistance,Axis) )/fExtent ;
		break ;

	}
	if( fScale < GEXFORM3D_MINIMUM_SCALE )
		fScale = 1.0f;
	return( fScale );
}

void Transform_SizeSelected( Level * pLevel, const grVec3d * pWorldDistance, SELECT_HANDLE eSizeType, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds )
{
	grFloat				fExtent ;
	SizeBrushInfo		sbi ;
	const grExtBox	*	pSelectedBounds ;
	assert( pLevel != NULL ) ;
	assert( pWorldDistance != NULL ) ;
	assert( pWorldBounds != NULL ) ;


	pSelectedBounds = Level_GetSelDrawBounds( pLevel ) ;

	sbi.fHScale = Transform_CalcScale( pSelectedBounds, GR_TRUE, HAxis,  pWorldDistance, eSizeType );
	assert( sbi.fHScale >= 0.0f ) ;
	if( sbi.fHScale < GEXFORM3D_SCALE_TOLERANCE )
		sbi.fHScale = GEXFORM3D_SCALE_TOLERANCE ;

	fExtent = Util_geExtBox_GetExtent( pSelectedBounds, VAxis ) ;
	if( fExtent < TRANSFORM_MIN_EXTENT )
		return ;

	sbi.fVScale = Transform_CalcScale( pSelectedBounds, GR_FALSE, VAxis,  pWorldDistance, eSizeType );
	assert( sbi.fVScale >= 0.0f ) ;
	if( sbi.fVScale < GEXFORM3D_SCALE_TOLERANCE )
		sbi.fVScale = GEXFORM3D_SCALE_TOLERANCE ;

	Util_ExtBox_SetInvalid( pWorldBounds ) ;
	sbi.pLevel = pLevel ;
	sbi.pWorldBounds = pWorldBounds ;
	sbi.pWorldDistance = pWorldDistance ;
	sbi.HAxis = HAxis ;
	sbi.VAxis = VAxis ;
	sbi.eSizeType = eSizeType ;
	sbi.pSelectedBounds = pSelectedBounds ;

	Level_EnumSelected( pLevel, &sbi, Transform_SizeObject ) ;
	Level_SetModifiedSelection( pLevel ) ;
	

}// Transform_SizeSelected

void Transform_PlaceSnap( Level * pLevel, grVec3d *placePt, grVec3d * pSnapDelta )
{
	grFloat				fSnapSize ;

	fSnapSize = (Level_IsSnapGrid( pLevel )) ? (grFloat)Level_GetGridSnapSize( pLevel ) : 1.0f ;
	Transform_SnapPointLR( placePt, fSnapSize, pSnapDelta ) ;
	grVec3d_Inverse( pSnapDelta ) ;
}

void Transform_MoveSnapSelected( Level * pLevel, SELECT_HANDLE eCorner, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, grExtBox * pWorldBounds, grVec3d * pSnapDelta )
{
	const grExtBox *	pSelBounds ;
	grVec3d				Corner ;
	grFloat				fSnapSize ;
	assert( pLevel != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	pSelBounds = Level_GetSelBounds( pLevel ) ;
	Transform_GetHandlePoint( eCorner, HAxis, VAxis, pSelBounds, &Corner ) ;

	fSnapSize = (Level_IsSnapGrid( pLevel )) ? (grFloat)Level_GetGridSnapSize( pLevel ) : 1.0f ;

	Transform_SnapPointLR( &Corner, fSnapSize, pSnapDelta ) ;
	grVec3d_Inverse( pSnapDelta ) ;
	Transform_MoveSelected( pLevel, pSnapDelta, pWorldBounds ) ;

}// Transform_MoveSnapSelected

void Transform_SizeSnapSelected
( 
	Level			*	pLevel, 
	SELECT_HANDLE		eCorner, 
	ORTHO_AXIS			HAxis, 
	ORTHO_AXIS			VAxis,
	grExtBox		*	pWorldBounds, 
	grVec3d			*	pSnapDelta
)
{
	const grExtBox *	pSelBounds ;
	grVec3d				Corner ;
	grFloat				fSnapSize ;
	assert( pLevel != NULL ) ;
	assert( pWorldBounds != NULL ) ;

	// This begins a move by snapping the moving handle of selected brushes to the
	// grid, based on the current view.  It doesn't try to go the direction the mouse
	// is, just gets on the grid
	pSelBounds = Level_GetSelBounds( pLevel ) ;

	Transform_GetHandlePoint( eCorner, HAxis, VAxis, pSelBounds, &Corner ) ;
	fSnapSize = (Level_IsSnapGrid( pLevel )) ? (grFloat)Level_GetGridSnapSize( pLevel ) : 1.0f ;

	Transform_SnapPointLR( &Corner, fSnapSize, pSnapDelta ) ;
	grVec3d_Inverse( pSnapDelta ) ;
	Transform_SizeSelected( pLevel, pSnapDelta, eCorner, HAxis, VAxis, pWorldBounds ) ;

}// Transform_SizeSnapSelected

void Transform_SnapBounds( const grExtBox * pBox, const grFloat fSnapSize, grVec3d * pDelta )
{
	Transform_Snap( pBox->Min.X, pBox->Max.X, fSnapSize, &pDelta->X ) ;
	Transform_Snap( pBox->Min.Y, pBox->Max.Y, fSnapSize, &pDelta->Y ) ;
	Transform_Snap( pBox->Min.Z, pBox->Max.Z, fSnapSize, &pDelta->Z ) ;
}// Transform_SnapBounds

void Transform_SnapPoint( const grVec3d * pPoint, const grFloat fSnapSize, grVec3d * pDelta )
{
	assert( pPoint != NULL ) ;
	assert( pDelta != NULL ) ;
	
	pDelta->X = Transform_SnapCoord( pPoint->X, fSnapSize )	;
	pDelta->Y = Transform_SnapCoord( pPoint->Y, fSnapSize )	;
	pDelta->Z = Transform_SnapCoord( pPoint->Z, fSnapSize )	;
}// Transform_SnapPoint

void Transform_SnapPointLR( const grVec3d * pPoint, const grFloat fSnapSize, grVec3d * pDelta )
{
	assert( pPoint != NULL ) ;
	assert( pDelta != NULL ) ;
	
	pDelta->X = Transform_SnapCoordLR( pPoint->X, fSnapSize )	;
	pDelta->Y = Transform_SnapCoordLR( pPoint->Y, fSnapSize )	;
	pDelta->Z = Transform_SnapCoordLR( pPoint->Z, fSnapSize )	;
}// Transform_SnapPoint

void Transform_PointToGrid( Level * pLevel, const grVec3d * pPoint,  grVec3d * pGridPoint )
{
	grFloat		fSnapSize ;
	grVec3d		SnapDelta ;
	assert( pLevel != NULL ) ;

	fSnapSize = (Level_IsSnapGrid( pLevel )) ? (grFloat)Level_GetGridSnapSize( pLevel ) : 1.0f ;
	Transform_SnapPoint( pPoint, fSnapSize, &SnapDelta ) ;
	grVec3d_Subtract( pPoint, &SnapDelta, pGridPoint ) ;
}// Transform_PointToGrid


void Transform_GetHandlePoint( SELECT_HANDLE eCorner, ORTHO_AXIS HAxis, ORTHO_AXIS VAxis, const grExtBox * pBounds, grVec3d * pPoint )
{
	assert( pBounds != NULL ) ;
	assert( pPoint != NULL ) ;
	
	// For this View, fill in the two coordinates of the handle associated with this box
	grVec3d_Clear( pPoint ) ;

	// The Z axis is flipped (down is positive)
	switch( eCorner )
	{
	case Select_Top :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		break ;

	case Select_Bottom :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		break ;

	case Select_Left :
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Min, HAxis ) ) ;
		break ;

	case Select_Right :
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Max, HAxis ) ) ;
		break ;

	case Select_TopLeft :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Min, HAxis ) ) ;
		break ;		

	case Select_TopRight :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Max, HAxis ) ) ;
		break ;

	case Select_BottomLeft :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Min, HAxis ) ) ;
		break ;
	
	case Select_BottomRight :
		if( Ortho_Axis_Z == VAxis )
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Max, VAxis ) ) ;
		else
			grVec3d_SetElement( pPoint, VAxis, grVec3d_GetElement( &pBounds->Min, VAxis ) ) ;
		grVec3d_SetElement( pPoint, HAxis, grVec3d_GetElement( &pBounds->Max, HAxis ) ) ;
		break ;
	}
}// Transform_GetHandlePoint

static grBoolean Transform_AddUndoCB( Object * pObject, void *lParam  )
{
	grXForm3d			*	XFormContext;
	grXForm3d				ObjectXForm;
	Undo				*	pUndo;

	assert( pObject!= NULL );
	assert( lParam != NULL );

	pUndo = (Undo*)lParam;
	if( !Object_GetTransform( pObject, &ObjectXForm ) )
		return( GR_TRUE );
	XFormContext = GR_RAM_ALLOCATE_STRUCT( grXForm3d );
	if( XFormContext  == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "Undable to allocate XForm Context" );
		return( GR_FALSE );
	}
	*XFormContext = ObjectXForm;
	assert( pUndo!= NULL );
	return( Undo_AddSubTransaction( pUndo, UNDO_TRANSFORM, pObject, XFormContext ) );
}// Transform_AddUndoCB

grBoolean	Transform_AddSelectedUndo( Level * pLevel, UNDO_TYPES Type )
{
	Undo *			pUndo;

	assert( pLevel != NULL ) ;

	pUndo = Level_GetUndo( pLevel );
	assert( pUndo );
	Undo_Push( pUndo, Type );
	Level_EnumSelected( pLevel, pUndo, Transform_AddUndoCB ) ;
	return( GR_TRUE );
}// Transform_AddSelectedUndo

static grBoolean Transform_AddShearUndoCB( Object * pObject, void *lParam  )
{
	grBrush			*	pgeBrush;
	Undo				*	pUndo;
	uint32				Contents;

	assert( pObject!= NULL );
	assert( lParam != NULL );

	if( Object_GetKind( pObject ) != KIND_BRUSH )
		Transform_AddUndoCB( pObject, lParam );

	pUndo = (Undo*)lParam;
	pgeBrush = Brush_CopygeBrush( (Brush *)pObject );
	Contents = grBrush_GetContents( Brush_GetgrBrush( (Brush *)pObject) ) ;
	grBrush_SetContents( pgeBrush, Contents ) ;

	return( Undo_AddSubTransaction( pUndo, UNDO_BRUSHSHEAR, pObject, pgeBrush ) );
}// Transform_AddUndoCB

grBoolean	Transform_AddShearSelectedUndo( Level * pLevel )
{
	Undo *			pUndo;

	assert( pLevel != NULL ) ;

	pUndo = Level_GetUndo( pLevel );
	assert( pUndo );
	Undo_Push( pUndo, UNDO_SHEAR );
	Level_EnumSelected( pLevel, pUndo, Transform_AddShearUndoCB ) ;
	return( GR_TRUE );
}// Transform_AddSelectedUndo

#pragma warning (disable:4505)	// unreferenced function

/* EOF: Transform.c */