/****************************************************************************************/
/*  TCLIP.C                                                                             */
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

#ifdef __ICL
#pragma message("ICL")
#pragma warning(disable : 344 266) // seems to do nothing to intel
#endif

//#define DO_TIMER
//#define USE_OLD

/*********

Cbloom Jan 18
TClip gained 2-3 fps
(not counting the gains from _SetTexture)

I reorganized the TClip_Triangle function flow to early-out 
for triangles all-in or all-out.  The old code considered this
case, but was not as lean as possible for these most-common cases.

To solve these problems, flow was changed to :
	1. do all compares and accumulated the 3 out bits for each of the five faces
		(so we have 15 bit-flags)
	2. then do clips while more clips remain.
	3. as a free benefit, the new structure means that Rasterize is only called once
			in TClip_Triangle, so it was inlined.

Step two results in very fast exiting when no clipping remains.

If/When we get the Intel compiler that can optimize ?: to CMOV, speed will improve
even more!

*** Tested : with /Qxi /Qipo /G6 on the Intel compiler, we cut another 8% off the time!
	The result is a net 63% gain in TClip time !  From 1 ms/frame to 0.37 ms/frame !!!

-----------------------------------

Timer profiling shows:
(with D3DDrv, in ActView, viewing dema.act)
times are seconds per frame

default pose : 58.6 fps
TClip_New            : 0.006749
TClip_Rasterize      : 0.006149

default pose : 52.9 fps
TClip_Old            : 0.007230 : 1.$ %
TClip_Rasterize      : 0.006183 : 1.$ %

***********/

// TClip.c
//  Fast Triangle Clipping
/*}{***********************/

#include <assert.h>
#include <string.h>

#include "Dcommon.h"
#include "Engine.h"

#include "TClip.h"
#include "Bitmap._h"

#include "List.h"
#include "Ram.h"  
#include "Errorlog.h"

#include "Timer.h"

#include "grMaterial.h"

TIMER_VARS(TClip_Triangle);

//#define ONE_OVER_Z_PIPELINE	// this has more accuracy, but the slowness of doing 1/ divides

typedef enum 
{	
	BACK_CLIPPING_PLANE = 0,
	LEFT_CLIPPING_PLANE,
	RIGHT_CLIPPING_PLANE,
	TOP_CLIPPING_PLANE,
	BOTTOM_CLIPPING_PLANE,
	NUM_CLIPPING_PLANES
} grTClip_ClippingPlane;

// 3 bits for V_IN/OUT flags
#define V_ALL_IN (0)
#define V0_OUT	(1)
#define V1_OUT	(2)
#define V2_OUT	(4)

	// at a=0, result is l;  at a=1, result is h
#define LINEAR_INTERPOLATE(a,l,h)     ((l)+(((h)-(l))*(a)))

typedef struct grTClip_StaticsType
{
	grFloat LeftEdge;
	grFloat RightEdge;
	grFloat TopEdge;
	grFloat BottomEdge;
	grFloat BackEdge;

	DRV_Driver * Driver;
	grEngine	*Engine;
	const grMaterialSpec *Material;
	grTexture * THandle;

	int32 RenderFlags;
	uint32 DefaultRenderFlags;
} grTClip_StaticsType;

/*}{************ Protos ***********/

static void GRCF grTClip_Split(GR_LVertex *NewVertex,const GR_LVertex *V1,const GR_LVertex *V2,int ClippingPlane);
static void GRCF grTClip_TrianglePlane(const GR_LVertex * zTriVertex,int ClippingPlane);

/*}{************ The State Statics ***********/

static Link * grTClip_Link = NULL;
static grTClip_StaticsType grTClip_Statics;

/*}{************ Functions ***********/

GRAPI grBoolean GRCC grTClip_Push(void)
{
grTClip_StaticsType * TCI;

	if ( ! grTClip_Link )
	{
		List_Start();
		grTClip_Link = Link_Create();
		if ( ! grTClip_Link ) 
			return GR_FALSE;
	}

	TCI = (grTClip_StaticsType *)grRam_Allocate(sizeof(grTClip_StaticsType));
	if ( ! TCI )
		return GR_FALSE;
	memcpy(TCI,&grTClip_Statics,sizeof(grTClip_StaticsType));

	Link_Push( grTClip_Link , TCI );

	return GR_TRUE;
}

GRAPI grBoolean GRCC grTClip_Pop(void)
{
grTClip_StaticsType * TCI;
	if ( ! grTClip_Link )
		return GR_FALSE;
	TCI = (grTClip_StaticsType *)Link_Pop( grTClip_Link );
	if ( ! TCI )
		return GR_FALSE;
	memcpy(&grTClip_Statics,TCI,sizeof(grTClip_StaticsType));
	grRam_Free(TCI);

	if ( ! Link_Peek(grTClip_Link) )
	{
		Link_Destroy(grTClip_Link);
		grTClip_Link = NULL;
		List_Stop();
	}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grTClip_SetTexture(const grMaterialSpec * Material, int32 RenderFlags)
{
	grTexture* Texture = NULL;
	grBitmap* Bitmap = NULL;
	grTClip_Statics.Material = Material;
	grTClip_Statics.RenderFlags = RenderFlags;
	
    if (Material != NULL) {
	    Texture = grMaterialSpec_GetLayerTexture(Material, 0);
	    grTClip_Statics.THandle = Texture;
	    if ( Texture == NULL) {
		    Bitmap = grMaterialSpec_GetLayerBitmap(Material, 0);
	    }
    }

	if ( Bitmap )
	{
		grTClip_Statics.THandle = grBitmap_GetTHandle(Bitmap);
		assert(grTClip_Statics.THandle);
	}
	else
	{
		grTClip_Statics.THandle = NULL;
	}
	return GR_TRUE;
}

GRAPI void GRCC grTClip_SetupEdges(
	grEngine *Engine,
	grFloat LeftEdge, 
	grFloat RightEdge,
	grFloat TopEdge ,
	grFloat BottomEdge,
	grFloat BackEdge)
{ 
	assert(Engine);
	memset(&grTClip_Statics,0,sizeof(grTClip_Statics));
	grTClip_Statics.Engine		= Engine;
	grTClip_Statics.Driver		= grEngine_GetDriver(Engine);
	grTClip_Statics.LeftEdge	= LeftEdge;
	grTClip_Statics.RightEdge	= RightEdge;
	grTClip_Statics.TopEdge		= TopEdge;
	grTClip_Statics.BottomEdge	= BottomEdge;
	grTClip_Statics.BackEdge	= BackEdge;
	if (grEngine_GetDefaultRenderFlags(Engine, &grTClip_Statics.DefaultRenderFlags)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grTClip_SetupEdges");
			grErrorLog_Clear();
			grTClip_Statics.DefaultRenderFlags = 0;
		}
}

#ifdef DO_TIMER
void grTClip_Done(void)
{
	TIMER_REPORT(TClip_Triangle);
}
#endif

GRAPI void GRCC grTClip_Triangle(const GR_LVertex TriVertex[3])
{

	TIMER_P(TClip_Triangle);

	grTClip_TrianglePlane(TriVertex,BACK_CLIPPING_PLANE);

	TIMER_Q(TClip_Triangle);
}



/*}{************ TClip_Split ***********/

static void GRCF grTClip_Split(GR_LVertex *NewVertex,const GR_LVertex *V1,const GR_LVertex *V2,int ClippingPlane)
{
	grFloat Ratio=0.0f;
	grFloat OneOverZ1,OneOverZ2;
	
	#ifdef ONE_OVER_Z_PIPELINE
		// in here ->Z is really (one over z)
		OneOverZ1 = V1->Z;
		OneOverZ2 = V2->Z;
	#else
		OneOverZ1 = 1.0f/V1->Z;
		OneOverZ2 = 1.0f/V2->Z;
	#endif

	switch (ClippingPlane)
		{
			case (BACK_CLIPPING_PLANE):
				assert((V2->Z - V1->Z)!=0.0f);
				Ratio = ((1.0f/grTClip_Statics.BackEdge) - OneOverZ2)/( OneOverZ1 - OneOverZ2 );

				NewVertex->X = LINEAR_INTERPOLATE(Ratio,(V2->X),(V1->X));
				NewVertex->Y = LINEAR_INTERPOLATE(Ratio,(V2->Y),(V1->Y));
				#ifdef ONE_OVER_Z_PIPELINE
				NewVertex->Z = 1.0f/ grTClip_Statics.BackEdge;
				#else
				NewVertex->Z = grTClip_Statics.BackEdge;
				#endif
			
				break;
			case (LEFT_CLIPPING_PLANE):
				assert((V2->X - V1->X)!=0.0f);
				Ratio = (grTClip_Statics.LeftEdge - V2->X)/( V1->X - V2->X);

				NewVertex->X = grTClip_Statics.LeftEdge;
				NewVertex->Y = LINEAR_INTERPOLATE(Ratio,(V2->Y),(V1->Y));
				#ifdef ONE_OVER_Z_PIPELINE
				NewVertex->Z = LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#else
				NewVertex->Z = 1.0f/LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#endif
		
				break;
			case (RIGHT_CLIPPING_PLANE):
				assert((V2->X - V1->X)!=0.0f);
				Ratio = (grTClip_Statics.RightEdge - V2->X)/( V1->X - V2->X);

				NewVertex->X = grTClip_Statics.RightEdge;
				NewVertex->Y = LINEAR_INTERPOLATE(Ratio,(V2->Y),(V1->Y));
				#ifdef ONE_OVER_Z_PIPELINE
				NewVertex->Z = LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#else
				NewVertex->Z = 1.0f/LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#endif

				break;
			case (TOP_CLIPPING_PLANE):
				assert((V2->Y - V1->Y)!=0.0f);
				Ratio = (grTClip_Statics.TopEdge - V2->Y)/( V1->Y - V2->Y);

				NewVertex->X = LINEAR_INTERPOLATE(Ratio,(V2->X),(V1->X));
				NewVertex->Y = grTClip_Statics.TopEdge;
				#ifdef ONE_OVER_Z_PIPELINE
				NewVertex->Z = LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#else
				NewVertex->Z = 1.0f/LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#endif
				
				break;
			case (BOTTOM_CLIPPING_PLANE):
				assert((V2->Y - V1->Y)!=0.0f);
				Ratio = (grTClip_Statics.BottomEdge - V2->Y)/( V1->Y - V2->Y);

				NewVertex->X = LINEAR_INTERPOLATE(Ratio,(V2->X),(V1->X));
				NewVertex->Y = grTClip_Statics.BottomEdge;
				#ifdef ONE_OVER_Z_PIPELINE
				NewVertex->Z = LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#else
				NewVertex->Z = 1.0f/LINEAR_INTERPOLATE(Ratio,OneOverZ2,OneOverZ1);
				#endif

				break;
		}

	
	{
		grFloat OneOverZ1_Ratio;
		grFloat OneOverZ2_Ratio;
		#ifdef ONE_OVER_Z_PIPELINE
		OneOverZ1 *= 1.0f / NewVertex->Z;
		OneOverZ2 *= 1.0f / NewVertex->Z;
		#else
		OneOverZ1 *= NewVertex->Z;
		OneOverZ2 *= NewVertex->Z;
		#endif
		OneOverZ1_Ratio = OneOverZ1 * Ratio;
		OneOverZ2_Ratio = OneOverZ2 * Ratio;

		//  the following is optimized to get rid of a handfull of multiplies. Read:
		//	NewVertex->r = LINEAR_INTERPOLATE(Ratio,(V2->r * OneOverZ2),(V1->r * OneOverZ1));

		NewVertex->r =(V2->r * OneOverZ2) + (V1->r * OneOverZ1_Ratio) - (V2->r * OneOverZ2_Ratio);
		NewVertex->g =(V2->g * OneOverZ2) + (V1->g * OneOverZ1_Ratio) - (V2->g * OneOverZ2_Ratio);
		NewVertex->b =(V2->b * OneOverZ2) + (V1->b * OneOverZ1_Ratio) - (V2->b * OneOverZ2_Ratio);
		NewVertex->a =(V2->a * OneOverZ2) + (V1->a * OneOverZ1_Ratio) - (V2->a * OneOverZ2_Ratio);
		NewVertex->u =(V2->u * OneOverZ2) + (V1->u * OneOverZ1_Ratio) - (V2->u * OneOverZ2_Ratio);
		NewVertex->v =(V2->v * OneOverZ2) + (V1->v * OneOverZ1_Ratio) - (V2->v * OneOverZ2_Ratio);
	}

}


/*}{************ TClip_TrianglePlane (New) ***********/

static void GRCF grTClip_TrianglePlane(const GR_LVertex * TriVertex,
											int ClippingPlane)
{
uint32 OutBits = 0;

	switch(ClippingPlane)
	{
	case BACK_CLIPPING_PLANE:

		OutBits |= (TriVertex[0].Z < grTClip_Statics.BackEdge) ? V0_OUT : 0;
		OutBits |= (TriVertex[1].Z < grTClip_Statics.BackEdge) ? V1_OUT : 0;
		OutBits |= (TriVertex[2].Z < grTClip_Statics.BackEdge) ? V2_OUT : 0;

	case LEFT_CLIPPING_PLANE:

		OutBits |= (TriVertex[0].X < grTClip_Statics.LeftEdge)  ? (V0_OUT<<3) : 0;
		OutBits |= (TriVertex[1].X < grTClip_Statics.LeftEdge)  ? (V1_OUT<<3) : 0;
		OutBits |= (TriVertex[2].X < grTClip_Statics.LeftEdge)  ? (V2_OUT<<3) : 0;

	case RIGHT_CLIPPING_PLANE:

		OutBits |= (TriVertex[0].X > grTClip_Statics.RightEdge) ? (V0_OUT<<6) : 0;
		OutBits |= (TriVertex[1].X > grTClip_Statics.RightEdge) ? (V1_OUT<<6) : 0;
		OutBits |= (TriVertex[2].X > grTClip_Statics.RightEdge) ? (V2_OUT<<6) : 0;

	case TOP_CLIPPING_PLANE:

		OutBits |= (TriVertex[0].Y < grTClip_Statics.TopEdge) ? (V0_OUT<<9) : 0;
		OutBits |= (TriVertex[1].Y < grTClip_Statics.TopEdge) ? (V1_OUT<<9) : 0;
		OutBits |= (TriVertex[2].Y < grTClip_Statics.TopEdge) ? (V2_OUT<<9) : 0;

	case BOTTOM_CLIPPING_PLANE:

		OutBits |= (TriVertex[0].Y > grTClip_Statics.BottomEdge) ?  (V0_OUT<<12) : 0;
		OutBits |= (TriVertex[1].Y > grTClip_Statics.BottomEdge) ?  (V1_OUT<<12) : 0;
		OutBits |= (TriVertex[2].Y > grTClip_Statics.BottomEdge) ?  (V2_OUT<<12) : 0;

	case NUM_CLIPPING_PLANES:
		break;
	}

	if ( OutBits )
	{
	GR_LVertex NewTriVertex[3];
		ClippingPlane = 0;
		for(;;)
		{
			assert(ClippingPlane < NUM_CLIPPING_PLANES);

			switch ( OutBits & 7 )
			{
				case (V_ALL_IN):  //NOT CLIPPED
					OutBits >>= 3;
					ClippingPlane ++;
					continue;

				// these all return:

				case (V0_OUT):
					NewTriVertex[0] = TriVertex[2];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+2,ClippingPlane);
					NewTriVertex[2] = TriVertex[1];

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);

					NewTriVertex[0] = NewTriVertex[1];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+1,ClippingPlane);

					//<> could gain a little speed like this, but who cares?
					//	if ( ! (OutBits>>3) )
					//		goto Rasterize
					//	else
					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1); 
					return;

				case (V1_OUT):
					NewTriVertex[0] = TriVertex[0];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+1,ClippingPlane);
					NewTriVertex[2] = TriVertex[2];

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);

					NewTriVertex[0] = NewTriVertex[1];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+1,TriVertex+2,ClippingPlane);
					
					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1); 
					return;

				case (V0_OUT + V1_OUT):
					NewTriVertex[0] = TriVertex[2];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+2,ClippingPlane);
					grTClip_Split(&(NewTriVertex[2]),TriVertex+1,TriVertex+2,ClippingPlane);
				
					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1); 
					return;

				case (V2_OUT):
					NewTriVertex[0] = TriVertex[1];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+1,TriVertex+2,ClippingPlane);
					NewTriVertex[2] = TriVertex[0];

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);

					NewTriVertex[0] = NewTriVertex[1];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+2,ClippingPlane);

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);
					return;

				case (V2_OUT + V0_OUT):
					NewTriVertex[0] = TriVertex[1];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+1,TriVertex+2,ClippingPlane);
					grTClip_Split(&(NewTriVertex[2]),TriVertex+0,TriVertex+1,ClippingPlane);

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);
					return;

				case (V2_OUT + V1_OUT):
					NewTriVertex[0] = TriVertex[0];
					grTClip_Split(&(NewTriVertex[1]),TriVertex+0,TriVertex+1,ClippingPlane);
					grTClip_Split(&(NewTriVertex[2]),TriVertex+0,TriVertex+2,ClippingPlane);

					grTClip_TrianglePlane(NewTriVertex,ClippingPlane+1);
					return;

				case (V2_OUT + V1_OUT + V0_OUT):
					/* TOTALLY CLIPPED */
					return;
			}
		}
	}


	if ( grTClip_Statics.THandle )
	{
		grRDriver_Layer		Layer;

		Layer.THandle = grTClip_Statics.THandle;

		assert(grTClip_Statics.Driver);
		grTClip_Statics.Driver->RenderMiscTexturePoly((grTLVertex *)TriVertex,
			3,&Layer, 1, 
			grTClip_Statics.RenderFlags | grTClip_Statics.DefaultRenderFlags | GR_RENDER_FLAG_COUNTER_CLOCKWISE );
	}
	else
	{
		assert(grTClip_Statics.Driver);
		grTClip_Statics.Driver->RenderGouraudPoly((grTLVertex *)TriVertex,3,
			grTClip_Statics.RenderFlags | grTClip_Statics.DefaultRenderFlags | GR_RENDER_FLAG_COUNTER_CLOCKWISE );
	}


}

/*}{*********** EOF ************/
