/****************************************************************************************/
/*  PalCreate                                                                           */
/*                                                                                      */
/*  Author: Charles Bloom                                                               */
/*  Description:  Palette Creation code                                                 */
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
#ifndef GR_BRANDO_PALCREATE_H
#define GR_BRANDO_PALCREATE_H

#include "BaseType.h"
#include "Bitmap.h"
#include "Image.h"

#ifdef __cplusplus
extern "C" {
#endif

extern grBitmap_Palette * createPaletteGood(const grBitmap_Info * Info,const void * Bits);
extern grBitmap_Palette * createPaletteFast(const grBitmap_Info * Info,const void * Bits);

typedef grBitmap_Palette * (*paletteCreater) (const grBitmap_Info * Info,const void * Bits);
extern void setCreatePaletteFunc(paletteCreater func);

extern grBitmap_Palette * createPalette(const grBitmap_Info * Info,const void * Bits);

extern grBitmap_Palette * createPaletteFromBitmap(const grBitmap * Bitmap,grBoolean Optimize);

grBitmap_Palette * createPaletteFromImage(const image *im);

extern void PalCreate_Start(void);
extern void PalCreate_Stop(void);

#ifdef __cplusplus
}
#endif

#endif
