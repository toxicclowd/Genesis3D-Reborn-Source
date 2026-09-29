/****************************************************************************************/
/*  VERTLIST.H                                                                          */
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
#pragma once
#ifndef VERTLIST_H
#define VERTLIST_H

#include "grList.h"
#include "grBrush.h"

typedef struct Vert_Struct 
{
	grVertArray_Index Index;
	grVec3d			  LastPos;
} Vert_Struct;

#ifdef __cplusplus
extern "C" {
#endif


typedef List VertList ;
typedef ListIterator VertIterator ;
typedef List_DestroyCallback VertList_DestroyCallback ;
typedef grBoolean (*VertListCB)( Vert_Struct *pVert, void * pVoid ) ;

VertList *		VertList_Create( void ) ;
void			VertList_Destroy( VertList **ppList, VertList_DestroyCallback DestroyFcn ) ;

// ACCESSORS
Vert_Struct		*	VertList_GetVert( VertIterator pMI ) ;
Vert_Struct		*	VertList_GetFirstVert( VertList * pList, VertIterator * pMI ) ;
Vert_Struct		*	VertList_GetNextVert( VertList * pList, VertIterator * pMI ) ;
int32				VertList_GetNumVert( VertList * pList );


// MODIFIERS
VertIterator	VertList_Append( VertList * pList, Vert_Struct	* pVert ) ;
void			VertList_Remove( VertList * pList, grVertArray_Index Index, VertList_DestroyCallback Callback) ;

// SEARCH
grBoolean		VertList_Search( VertList * pList, Vert_Struct	* pVert, VertIterator * vI );
grBoolean		VertList_SearchByIndex( VertList * pList, grVertArray_Index * Index, VertIterator * vI );

// ENUMERATION
int32			VertList_Enum( VertList * pList, void * pVoid, VertListCB Callback ) ;

#ifdef __cplusplus
}
#endif

#endif // Prevent multiple inclusion
/* EOF: VertList.h */
