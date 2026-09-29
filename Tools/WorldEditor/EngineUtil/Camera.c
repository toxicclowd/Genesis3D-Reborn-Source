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
#include <Assert.h>

#include "Genesis3D.h"
#include "Ram.h"

#include "Camera.h"

#define SIGNATURE	(0x12345678)

typedef struct tagCamera
{
#ifdef _DEBUG
	int			nSignature ;
#endif
	grCamera *	pCamera ;
} Camera ;


Camera * Camera_Create( void )
{
	grRect rect = {0, 0, 1, 1} ;
	grXForm3d M = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, {0, 0, 0}} ;
	Camera * pCamera ;

	
	pCamera = GR_RAM_ALLOCATE_STRUCT( Camera ) ;
	if( pCamera != NULL )
	{
		assert( (pCamera->nSignature = SIGNATURE) == SIGNATURE ) ;	// ASSIGN
	
		pCamera->pCamera = grCamera_Create( 2.0f, &rect ) ;
		if( !grCamera_SetXForm( pCamera->pCamera, &M) )
		{
			Camera_Destroy( &pCamera ) ;
			return NULL ;
		}
	}
	return pCamera ;
}// Camera_Create


void Camera_Destroy( Camera ** ppCamera ) 
{
	assert( ppCamera != NULL ) ;
	assert( (*ppCamera)->nSignature == SIGNATURE ) ;
	assert( (*ppCamera)->pCamera != NULL ) ;

	grCamera_Destroy( &(*ppCamera)->pCamera ) ;

	assert( ((*ppCamera)->nSignature = 0) == 0 ) ;	// CLEAR
	grRam_Free( *ppCamera ) ;
}// Camera_Destroy