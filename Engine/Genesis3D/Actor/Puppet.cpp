/****************************************************************************************/
/*  PUPPET.C																			*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Puppet implementation.									.				*/
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
//#define CIRCULAR_SHADOW
#define SHADOW_MAP
//#define PROJECTED_SHADOW

#include <math.h>  //fabs()
#include <assert.h>

#include "XFArray.h"
#include "Puppet.h"
#include "Pose.h"
#include "Errorlog.h"
#include "Ram.h"
#include "TClip.h"

#include "ExtBox.h"
#include "BodyInst.h"

#ifdef PROFILE
//#include "rdtsc.h"
#endif

#include "Bitmap.h"
#include "Bitmap._h"

#include "Engine._h" // for Engine->DebugInfo
#include "Camera._h"

#include "grMaterial._h"

#define PUPPET_DEFAULT_MAX_DYNAMIC_LIGHTS 3
#define PUPPET_DEFAULT_MAX_STATIC_LIGHTS 3

typedef struct grPuppet_Color
{
	grFloat				Red,Green,Blue;
} grPuppet_Color;

typedef struct grPuppet_Material
{
	grPuppet_Color		 Color;
	grBoolean			 UseTexture;
	//grBitmap			*Bitmap;
	grMaterialSpec		*Material;
	grUVMapper			Mapper;
	const char			*TextureName;
	const char			*AlphaName;
} grPuppet_Material;

#define MAX_DYNAMIC_LIGHTS			(32)
#define MAX_STATIC_LIGHTS				(32)

typedef struct
{
	grVec3d			Normal;
	grPuppet_Color	Color;
	grFloat			Distance;
	grFloat			Radius;
} grPuppet_Light;

typedef struct
{
	grPuppet_Light DLights[MAX_DYNAMIC_LIGHTS];
	grPuppet_Light SLights[MAX_STATIC_LIGHTS];
	int DLightCount;
	int SLightCount;
} grPuppet_BoneLight;

typedef struct grPuppet
{
	grVFile *			 TextureFileContext;
	//grXFArray			*JointTransforms;	
	grBodyInst			*BodyInstance;
	int					 MaterialCount;
	grPuppet_Material	*MaterialArray;
	int					 MaxDynamicLightsToUse;
	int						MaxStaticLightsToUse;
	int					 LightReferenceBoneIndex;
		
	grVec3d				 FillLightNormal;
	grPuppet_Color		 FillLightColor;			// 0..255
	grBoolean			 UseFillLight;				// use fill light normal
	
	grPuppet_Color		 AmbientLightIntensity;		// 0..1
	grBoolean			 AmbientLightFromFloor;		// use local lighting from floor

	grBoolean			 PerBoneLighting;

// @@
	// for the case of non- per-bone lighting
	grPuppet_Light	SLights[MAX_STATIC_LIGHTS]; // cached static lights
	int							SLightCount; // cached static light count

	// for the case of per-bone lighting
	grPuppet_BoneLight *BoneLightArray;
	int BoneLightArraySize;

	grBoolean			 DoShadow;
	grFloat				 ShadowScale;
	const grMaterialSpec *ShadowMap;
	int					 ShadowBoneIndex;

	grEngine*			pEngine;

//	[MacroArt::Begin]
//	Thanks Dee(cryscan@home.net)	
	float				 fOverallAlpha;
//	[MacroArt::End]

} grPuppet;

typedef struct
{
	grBoolean		UseFillLight;
	grVec3d			FillLightNormal;
	grPuppet_Color	MaterialColor;
	grPuppet_Color  FillLightColor;
	grPuppet_Color	Ambient;
	grVec3d			SurfaceNormal;
	grPuppet_Light	DLights[MAX_DYNAMIC_LIGHTS];
	int				DLightCount;
	grBoolean		PerBoneLighting;
} grPuppet_LightParamGroup;

//	[MacroArt::Begin]

float GRCF grPuppet_GetAlpha(const grPuppet *P)
{
	assert( P );
	return P->fOverallAlpha;
}

void GRCF grPuppet_SetAlpha(grPuppet *P, float Alpha)
{
	assert( P );
	P->fOverallAlpha = Alpha;
}

grEngine* GRCF grPuppet_GetEngine(grPuppet *P)
{
	return P->pEngine;
}

//	[MacroArt::End]

// Local info stored across multiple puppets to avoid resource waste.

grPuppet_LightParamGroup grPuppet_StaticLightGrp;
/*
grPuppet_BoneLight		 *grPuppet_StaticBoneLightArray=NULL;
int						  grPuppet_StaticBoneLightArraySize=0;
*/
int						  grPuppet_StaticPuppetCount=0;
int						  grPuppet_StaticFlags[2]={1768710981,560296816};

static grBoolean GRCF grPuppet_FetchTextures(grPuppet *P, const grBody *B)
{
	int i;
	assert( P );
	
	P->MaterialCount = grBody_GetMaterialCount(B);
	if (P->MaterialCount <= 0)
	{
		return GR_TRUE;
	}
	
	P->MaterialArray = GR_RAM_ALLOCATE_ARRAY_CLEAR(grPuppet_Material, P->MaterialCount);
	if (P->MaterialArray == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grPuppet_FetchTextures: Failed to allocate puppet material array");
		return GR_FALSE;
	}
	
	for (i=0; i<P->MaterialCount; i++)
	{
		const char *Name;
		grMaterialSpec *Bitmap;
		grUVMapper Mapper;
		grPuppet_Material *M;

		M = P->MaterialArray + i;

		grBody_GetMaterial( B, i, &(Name), &(Bitmap),
						&(M->Color.Red),&(M->Color.Green),&(M->Color.Blue), 
						&Mapper);

		if (Bitmap == NULL )
		{
			M->Material     = NULL;
			M->UseTexture = GR_FALSE;
		}
		else
		{
			M->UseTexture = GR_TRUE;
			assert( P->pEngine );

			M->Material = Bitmap;
			grMaterialSpec_CreateRef(Bitmap);

/*
			if ( ! grEngine_AddBitmap(P->pEngine,Bitmap, GR_ENGINE_BITMAP_TYPE_3D) )
			{
				grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_FetchTextures : Engine_AddBitmap", NULL);
				grRam_Free(P->MaterialArray);
				P->MaterialArray = NULL;
				P->MaterialCount = 0;
				return GR_FALSE;
			}
*/
		}
	}

	return GR_TRUE;
}	

int GRCF grPuppet_GetMaterialCount( grPuppet *P )
{
	assert( P );
	return P->MaterialCount;
}

grBoolean     grPuppet_GetMaterial( grPuppet *P, int MaterialIndex,
									grMaterialSpec **Bitmap, 
									grFloat *Red, grFloat *Green, grFloat *Blue, grUVMapper * pMapper)
{
	assert( P      );
	assert( Red    );
	assert( Green  );
	assert( Blue   );
	assert(pMapper);
	assert( Bitmap );
	assert( MaterialIndex >= 0 );
	assert( MaterialIndex < P->MaterialCount );

	{
		grPuppet_Material *M = &(P->MaterialArray[MaterialIndex]);
		*Bitmap = M->Material;
		*Red    = M->Color.Red;
		*Green  = M->Color.Green;
		*Blue   = M->Color.Blue;
		*pMapper = M->Mapper;
#ifdef _DEBUG
		if ( M->Material )
			assert(M->UseTexture);
#endif
	}

	return GR_TRUE;
}


grBoolean	grPuppet_SetMaterial(grPuppet *P, int MaterialIndex, grMaterialSpec *Bitmap, 
								 grFloat Red, grFloat Green, grFloat Blue, 
								 grUVMapper Mapper)
{
	assert( P );
	assert( MaterialIndex >= 0 );
	assert( MaterialIndex < P->MaterialCount );

	{
		grMaterialSpec * OldBitmap;
		grPuppet_Material *M = P->MaterialArray + MaterialIndex;

		OldBitmap = M->Material;

		M->Material		= Bitmap;
		M->Color.Red    = Red;
		M->Color.Green  = Green;
		M->Color.Blue   = Blue;
		M->Mapper = Mapper;

		if ( OldBitmap != Bitmap ) 
		{		
/*
			if ( OldBitmap )
			{
				assert( M->UseTexture );		
				grEngine_RemoveBitmap( P->pEngine, OldBitmap );
				grMaterialSpec_Destroy( &(OldBitmap) );
			}
*/			
			M->UseTexture = GR_FALSE;

			if ( Bitmap )
			{
				grMaterialSpec_CreateRef(Bitmap);
						
				M->UseTexture = GR_TRUE;

/*
				if ( ! grEngine_AddBitmap(P->pEngine,Bitmap, GR_ENGINE_BITMAP_TYPE_3D) )
				{
					grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_SetMaterial : Engine_AddBitmap", NULL);
					return GR_FALSE;
				}
*/
			}
		}
	}

	return GR_TRUE;
}
	

grPuppet* GRCF grPuppet_Create(grVFile *TextureFS, const grBody *B, grEngine *pEngine)
{
	grPuppet *P;

	assert( grBody_IsValid(B)!=GR_FALSE );
	
	P = GR_RAM_ALLOCATE_STRUCT_CLEAR(grPuppet);
	if (P==NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grPuppet_Create: Failed to allocate instance");
		return NULL;
	}

	//P->JointTransforms = NULL;
	P->BodyInstance = NULL;
	P->MaxDynamicLightsToUse = PUPPET_DEFAULT_MAX_DYNAMIC_LIGHTS;
	P->MaxStaticLightsToUse = PUPPET_DEFAULT_MAX_STATIC_LIGHTS;
	P->LightReferenceBoneIndex = GR_POSE_ROOT_JOINT;

	P->FillLightNormal.X = -0.2f;
	P->FillLightNormal.Y = 1.0f;
	P->FillLightNormal.Z = 0.4f;
	grVec3d_Normalize(&(P->FillLightNormal));
	P->FillLightColor.Red    = 0.25f;
	P->FillLightColor.Green  = 0.25f;
	P->FillLightColor.Blue   = 0.25f;
	P->UseFillLight = GR_TRUE;

	P->AmbientLightIntensity.Red   = 0.1f;
	P->AmbientLightIntensity.Green = 0.1f;
	P->AmbientLightIntensity.Blue  = 0.1f;
	P->AmbientLightFromFloor = GR_TRUE;

	P->DoShadow = GR_FALSE;
	P->ShadowScale = 0.0f;
	P->ShadowBoneIndex =  GR_POSE_ROOT_JOINT;
	P->TextureFileContext = TextureFS;

	P->BoneLightArray = NULL;
	P->BoneLightArraySize = 0;
//	[MacroArt::Begin]
	P->fOverallAlpha=255.0f;
//	[MacroArt::End]

// @@
	P->pEngine = pEngine;
				
	if (grPuppet_FetchTextures(P,B)==GR_FALSE)
	{
		grRam_Free(P);
		return NULL;
	}

	P->BodyInstance = grBodyInst_Create(B);
	if (P->BodyInstance == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grPuppet_Create: Failed to allocate body");
		grPuppet_Destroy( &P );
		return NULL;
	}

	return P;
}


void GRCF grPuppet_Destroy(grPuppet **P)
{
	assert( P  );
	assert( *P );
	if ( (*P)->BodyInstance )
	{
		grBodyInst_Destroy( &((*P)->BodyInstance) );
		(*P)->BodyInstance = NULL;
	}
	if ( (*P)->MaterialArray )
	{
		grPuppet_Material *M;
		int i;

		for (i=0; i<(*P)->MaterialCount; i++)
		{
			M = &((*P)->MaterialArray[i]);
			if (M->UseTexture )
			{					
				assert( M->Material );
				//grEngine_RemoveBitmap( (*P)->pEngine, M->Bitmap );
				grMaterialSpec_Destroy( &(M->Material) );
				M->UseTexture = GR_FALSE;
			}
		}


		grRam_Free( (*P)->MaterialArray );
		(*P)->BodyInstance = NULL;
	}
	if ( (*P)->ShadowMap )
	{
		grBitmap_Destroy((grBitmap **)&((*P)->ShadowMap));
		(*P)->ShadowMap = NULL;
	}

	if ( (*P)->BoneLightArray!=NULL)
		{
			grRam_Free((*P)->BoneLightArray);
		}

	grRam_Free( (*P) );
	*P = NULL;

	// clean up any shared resources.
	grPuppet_StaticPuppetCount--;
	if (grPuppet_StaticPuppetCount==0)
	{
		/*
		if (grPuppet_StaticBoneLightArray!=NULL)
			grRam_Free(grPuppet_StaticBoneLightArray);
		grPuppet_StaticBoneLightArray=NULL;
		grPuppet_StaticBoneLightArraySize = 0;
		*/
	}	
}


void grPuppet_GetLightingOptions(const grPuppet *P,
	grBoolean *UseFillLight,
	grVec3d *FillLightNormal,
	grFloat *FillLightRed,				
	grFloat *FillLightGreen,				
	grFloat *FillLightBlue,				
	grFloat *AmbientLightRed,			
	grFloat *AmbientLightGreen,			
	grFloat *AmbientLightBlue,			
	grBoolean *UseAmbientLightFromFloor,
	int32 *MaximumDynamicLightsToUse,
	int32 *MaximumStaticLightsToUse,
	int32 *LightReferenceBoneIndex,
	grBoolean *PerBoneLighting
	)
{
	grFloat Scaler;
	assert( P != NULL);
	assert( UseFillLight );
	assert( FillLightNormal );
	assert( FillLightRed );	
	assert( FillLightGreen );	
	assert( FillLightBlue );	
	assert( AmbientLightRed );
	assert( AmbientLightGreen );			
	assert( AmbientLightBlue );			
	assert( UseAmbientLightFromFloor );
	assert( MaximumDynamicLightsToUse );	
	assert( LightReferenceBoneIndex );
		
	*UseFillLight = P->UseFillLight;

	*FillLightNormal = P->FillLightNormal;
	
	Scaler = 255.0f;
	*FillLightRed   = P->FillLightColor.Red * Scaler;
	*FillLightGreen = P->FillLightColor.Green * Scaler;
	*FillLightBlue  = P->FillLightColor.Blue * Scaler;
	
	*AmbientLightRed    = P->AmbientLightIntensity.Red * Scaler;
	*AmbientLightGreen  = P->AmbientLightIntensity.Green * Scaler;
	*AmbientLightBlue   = P->AmbientLightIntensity.Blue * Scaler;
	
	*UseAmbientLightFromFloor  = P->AmbientLightFromFloor;
	*MaximumDynamicLightsToUse = P->MaxDynamicLightsToUse;
	*MaximumStaticLightsToUse = P->MaxStaticLightsToUse;
	*LightReferenceBoneIndex   = P->LightReferenceBoneIndex;
	*PerBoneLighting		   = P->PerBoneLighting;
}	

void grPuppet_SetLightingOptions(grPuppet *P,
	grBoolean UseFillLight,
	const grVec3d *FillLightNormal,
	grFloat FillLightRed,				// 0 .. 255
	grFloat FillLightGreen,				// 0 .. 255
	grFloat FillLightBlue,				// 0 .. 255
	grFloat AmbientLightRed,			// 0 .. 255
	grFloat AmbientLightGreen,			// 0 .. 255
	grFloat AmbientLightBlue,			// 0 .. 255
	grBoolean UseAmbientLightFromFloor,
	int MaximumDynamicLightsToUse,		// 0 for none
	int MaximumStaticLightsToUse, // 0 for none
	int LightReferenceBoneIndex,
	grBoolean PerBoneLighting
	)
{
	grFloat Scaler;
	assert( P!= NULL);
	assert( FillLightNormal );
	assert( grVec3d_IsNormalized(FillLightNormal) );
	assert( MaximumDynamicLightsToUse >= 0 );
	assert( (LightReferenceBoneIndex >=0) || (LightReferenceBoneIndex==GR_POSE_ROOT_JOINT));
		
	P->UseFillLight = UseFillLight;

	P->FillLightNormal = *FillLightNormal;
	
	Scaler = 1.0f/255.0f;

	P->FillLightColor.Red   = FillLightRed   * Scaler;
	P->FillLightColor.Green = FillLightGreen * Scaler;
	P->FillLightColor.Blue  = FillLightBlue  * Scaler;
	
	P->AmbientLightIntensity.Red   = AmbientLightRed   * Scaler;
	P->AmbientLightIntensity.Green = AmbientLightGreen * Scaler;
	P->AmbientLightIntensity.Blue  = AmbientLightBlue  * Scaler;
	
	P->AmbientLightFromFloor =UseAmbientLightFromFloor;
	P->MaxDynamicLightsToUse = MaximumDynamicLightsToUse;
	P->MaxStaticLightsToUse = MaximumStaticLightsToUse;
	P->LightReferenceBoneIndex = LightReferenceBoneIndex;
	P->PerBoneLighting		 = 	PerBoneLighting;
}	

// LP = array of lights
// ReferencePoint = world space location of attachment point
static int GRCC grPuppet_PrepDynamicLights(const grPuppet *P, 
	const grWorld *World,
	grPuppet_Light *LP,
	const grVec3d *ReferencePoint)
{
	int				i,j,cnt;
	grChain			*DLightChain;
	grChain_Link	*Link;


	assert( P );
	assert( LP );

	DLightChain = grWorld_GetDLightChain(World);
	
	cnt=0;

	for (Link = grChain_GetFirstLink(DLightChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grLight		*L;
		grVec3d		Position; 
		grVec3d		Color;
		grVec3d		Normal;
		grFloat		Radius; 
		grFloat		Brightness;
		uint32		Flags;

		L = (grLight*)grChain_LinkGetLinkData(Link);

		if (!grLight_GetAttributes(	L, &Position,&Color,&Radius,&Brightness, &Flags))
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_PrepDynamicLights: failed to get light attributes",NULL);
			continue;
		}

		if (!(Flags & GR_LIGHT_FLAG_FAST_LIGHTING_MODEL))
			continue;

		grVec3d_Subtract(&Position,ReferencePoint,&Normal);

		LP[cnt].Distance =	Normal.X * Normal.X + 
							Normal.Y * Normal.Y +
							Normal.Z * Normal.Z;

		if (LP[cnt].Distance < Radius*Radius)
		{
			LP[cnt].Color.Red   = Color.X;
			LP[cnt].Color.Green = Color.Y;
			LP[cnt].Color.Blue  = Color.Z;
			LP[cnt].Radius = Radius;
			LP[cnt].Normal = Normal;
			cnt++;
		}
	}

	// sort dynamic lights by distance (squared)
	for (i=0; i<cnt; i++)
		for (j=0; j<cnt-1; j++)
			{
				if (LP[j].Distance > LP[j+1].Distance)
					{
						grPuppet_Light Swap = LP[j];
						LP[j] = LP[j+1];
						LP[j+1] = Swap;
					}
			}

	if (cnt > P->MaxDynamicLightsToUse)
		cnt = P->MaxDynamicLightsToUse;

	// go back and finish setting up closest lights
	for (i=0; i<cnt; i++)
		{
			grFloat Distance = (grFloat)sqrt(LP[i].Distance);
			grFloat OneOverDistance;
			grFloat Scale;
			if (Distance < 1.0f)
				Distance = 1.0f;
			OneOverDistance = 1.0f / Distance;
			LP[i].Normal.X *= OneOverDistance;
			LP[i].Normal.Y *= OneOverDistance;
			LP[i].Normal.Z *= OneOverDistance;

			LP[i].Distance = Distance;

			//assert( Distance  < LP[i].Radius );

			Scale = 1.0f - Distance / LP[i].Radius ;
			Scale *= (1.0f/255.0f);
			LP[i].Color.Red   *= Scale;
			LP[i].Color.Green *= Scale;
			LP[i].Color.Blue  *= Scale;
		}
			
	return cnt;			
}

// @@
// LP = array of lights
// ReferencePoint = world space location of attachment point
// recache = indicate whether to scan thru all static lights to see which are
//						closest to actor

static int GRCC grPuppet_PrepStaticLights(const grPuppet *P, 
	const grWorld *World,
	grPuppet_Light *LP,
	const grVec3d *ReferencePoint)
{
	int				i,j, cnt;
	grChain			*SLightChain;
	grChain_Link	*Link;


	assert( P );
	assert( LP );

	
#pragma message("**************************************************************************")
#pragma message("puppet.c: grPuppet_PrepStaticLights()")
#pragma message("Instead of cycling thru all the static lights in the world, find out what area the")
#pragma message("ReferencePoint is in and use the lights that are in that (and possibly)")
#pragma message("neighboring areas!")
#pragma message("**************************************************************************")

	SLightChain = grWorld_GetLightChain(World);
	
	cnt=0;

	for (Link = grChain_GetFirstLink(SLightChain); Link; Link = grChain_LinkGetNext(Link))
	{
		grLight		*L;
		grVec3d		Position; 
		grVec3d		Color;
		grVec3d		Normal;
		grFloat		Radius; 
		grFloat		Brightness;
		uint32		Flags;

		L = (grLight*)grChain_LinkGetLinkData(Link);

		if (!grLight_GetAttributes(	L, &Position,&Color,&Radius,&Brightness, &Flags))
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_PrepStaticLights: failed to get light attributes",NULL);
			continue;
		}

		/*
		if (!(Flags & GR_LIGHT_FLAG_FAST_LIGHTING_MODEL))
			continue;
		*/

		Normal.X = Position.X - ReferencePoint->X;
		Normal.Y = Position.Y - ReferencePoint->Y;
		Normal.Z = Position.Z - ReferencePoint->Z;

		LP[cnt].Distance =	Normal.X * Normal.X + 
							Normal.Y * Normal.Y +
							Normal.Z * Normal.Z;

		if (LP[cnt].Distance < Radius*Radius)
			{
				LP[cnt].Color.Red   = Color.X;
				LP[cnt].Color.Green = Color.Y;
				LP[cnt].Color.Blue  = Color.Z;
				LP[cnt].Radius = Radius;
				LP[cnt].Normal = Normal;
				cnt++;
			}
	}

	// sort static lights by distance (squared)
	for (i = 0; i < cnt; i++)
		for (j = 0; j < (cnt - 1); j++)
			{
				if (LP[j].Distance > LP[j+1].Distance)
					{
						grPuppet_Light Swap = LP[j];
						LP[j] = LP[j+1];
						LP[j+1] = Swap;
					}
			}

	if (cnt > P->MaxStaticLightsToUse)
		cnt = P->MaxStaticLightsToUse;

	// go back and finish setting up closest static lights
	for (i = 0; i < cnt; i ++)
		{
			grFloat Distance = (grFloat)sqrt(LP[i].Distance);
			grFloat OneOverDistance;
			grFloat Scale;

			if (Distance < 1.0f)
				Distance = 1.0f;
			OneOverDistance = 1.0f / Distance;
			LP[i].Normal.X *= OneOverDistance;
			LP[i].Normal.Y *= OneOverDistance;
			LP[i].Normal.Z *= OneOverDistance;

			LP[i].Distance = Distance;

			//assert( Distance  < LP[i].Radius );

			Scale = 1.0f - Distance / LP[i].Radius ;
			Scale *= (1.0f/255.0f);
			LP[i].Color.Red   *= Scale;
			LP[i].Color.Green *= Scale;
			LP[i].Color.Blue  *= Scale;
		}

	return cnt;
}

	
static void GRCC grPuppet_ComputeAmbientLight(
		const grPuppet *P, 
		grPuppet_Color *Ambient,
		const grVec3d *ReferencePoint)
{
	assert( P );
	assert( Ambient );

#if 0
	if (P->AmbientLightFromFloor != GR_FALSE)
		{
			#define GR_PUPPET_MAX_AMBIENT (0.3f)
			int32			Node, Plane, i;
			grVec3d			Pos1, Pos2, Impact;
			GFX_Node		*GFXNodes;
			Surf_SurfInfo	*Surf;
			GR_RGBA			RGBA;
			grBoolean		Col1, Col2;
			
			GFXNodes = World->CurrentBSP->BSPData.GFXNodes;
			
			Pos1 = *ReferencePoint;
			
			Pos2 = Pos1;

			Pos2.Y -= 30000.0f;

			// Get shadow hit plane impact point
			Col1 = Trace_WorldCollisionExact2((grWorld*)World, &Pos1, &Pos1, &Impact, &Node, &Plane, NULL);
			Col2 = Trace_WorldCollisionExact2((grWorld*)World, &Pos1, &Pos2, &Impact, &Node, &Plane, NULL);

			// Now find the color of the mesh by getting the lightmap point he is standing on...
			if (!Col1 && Col2)
				{
					Surf = &(World)->CurrentBSP->SurfInfo[GFXNodes[Node].FirstFace];
					if (Surf->LInfo.Face<0)
						{	// FIXME?  surface has no light...
							Ambient->Red = Ambient->Green = Ambient->Blue = 0.0f;
							return;
						}

					for (i=0; i< GFXNodes[Node].NumFaces; i++)
						{
							if (Surf_InSurfBoundingBox(Surf, &Impact, 20.0f))
								{
									Light_SetupLightmap(&Surf->LInfo, NULL);			

									if (Light_GetLightmapRGB(Surf, &Impact, &RGBA))
										{
											grFloat Scale = 1.0f / 255.0f;
											Ambient->Red   = RGBA.r * Scale;
											Ambient->Green = RGBA.g * Scale;
											Ambient->Blue  = RGBA.b * Scale;
											if (Ambient->Red > GR_PUPPET_MAX_AMBIENT) 
												{
													Ambient->Red = GR_PUPPET_MAX_AMBIENT;
												}
											if (Ambient->Green > GR_PUPPET_MAX_AMBIENT) 
												{
													Ambient->Green = GR_PUPPET_MAX_AMBIENT;
												}
											if (Ambient->Blue > GR_PUPPET_MAX_AMBIENT) 
												{
													Ambient->Blue = GR_PUPPET_MAX_AMBIENT;
												}
											break;
										}
								}
							Surf++;
						}
				}
			else
				{
					*Ambient = P->AmbientLightIntensity;
				}
		}
	else
#endif
	{
		*Ambient = P->AmbientLightIntensity;
	}
}


// @@
static void GRCC grPuppet_SetVertexColor(grPuppet* P,
	grLVertex *v,int BoneIndex)
{
	grFloat RedIntensity,GreenIntensity,BlueIntensity;
	grFloat Color;						
	int l;
	grVec3d surfaceNormal;

	assert(v != NULL);
	
	RedIntensity   = grPuppet_StaticLightGrp.Ambient.Red;
	GreenIntensity = grPuppet_StaticLightGrp.Ambient.Green;
	BlueIntensity  = grPuppet_StaticLightGrp.Ambient.Blue;

	surfaceNormal = grPuppet_StaticLightGrp.SurfaceNormal;

	if (grPuppet_StaticLightGrp.UseFillLight)
	{
		grFloat Intensity;
		Intensity = grPuppet_StaticLightGrp.FillLightNormal.X * surfaceNormal.X + 
					grPuppet_StaticLightGrp.FillLightNormal.Y * surfaceNormal.Y + 
					grPuppet_StaticLightGrp.FillLightNormal.Z * surfaceNormal.Z;
		if (Intensity > 0.0)
		{
			RedIntensity   += Intensity * grPuppet_StaticLightGrp.FillLightColor.Red;
			GreenIntensity += Intensity * grPuppet_StaticLightGrp.FillLightColor.Green;
			BlueIntensity  += Intensity * grPuppet_StaticLightGrp.FillLightColor.Blue;
		}
	}

	if (grPuppet_StaticLightGrp.PerBoneLighting)
	{
		grPuppet_BoneLight *L;

		L = &P->BoneLightArray[BoneIndex];

		// accumulate dynamic lighting
		for (l = 0; l < L->DLightCount; l ++)
		{
			grVec3d *LightNormal;
			float Intensity;
		
			LightNormal = &(L->DLights[l].Normal);

			Intensity=	LightNormal->X * surfaceNormal.X + 
						LightNormal->Y * surfaceNormal.Y + 
						LightNormal->Z * surfaceNormal.Z;
			if (Intensity > 0.0f)
			{
				RedIntensity   += Intensity * L->DLights[l].Color.Red;
				GreenIntensity += Intensity * L->DLights[l].Color.Green;
				BlueIntensity  += Intensity * L->DLights[l].Color.Blue;
			}
		}

		// accumulate static lighting
		for (l = 0; l < L->SLightCount; l ++)
		{
			grVec3d *LightNormal;
			float Intensity;
		
			LightNormal = &(L->SLights[l].Normal);

			Intensity=	LightNormal->X * surfaceNormal.X + 
						LightNormal->Y * surfaceNormal.Y + 
						LightNormal->Z * surfaceNormal.Z;
			if (Intensity > 0.0f)
			{
				RedIntensity   += Intensity * L->SLights[l].Color.Red;
				GreenIntensity += Intensity * L->SLights[l].Color.Green;
				BlueIntensity  += Intensity * L->SLights[l].Color.Blue;
			}
		}
	}
	else // not doing per-bone lighting
	{
		// accumulate dynamic lighting
		for (l = 0; l < grPuppet_StaticLightGrp.DLightCount; l ++)
		{
			grVec3d *LightNormal;
			float Intensity;
		
			LightNormal = &(grPuppet_StaticLightGrp.DLights[l].Normal);

			Intensity=	LightNormal->X * surfaceNormal.X + 
						LightNormal->Y * surfaceNormal.Y + 
						LightNormal->Z * surfaceNormal.Z;
			if (Intensity > 0.0f)
			{
				RedIntensity   += Intensity * grPuppet_StaticLightGrp.DLights[l].Color.Red;
				GreenIntensity += Intensity * grPuppet_StaticLightGrp.DLights[l].Color.Green;
				BlueIntensity  += Intensity * grPuppet_StaticLightGrp.DLights[l].Color.Blue;
			}
		}

		// accumulate static lighting
		for (l = 0; l < P->SLightCount; l ++)
		{
			grVec3d *LightNormal;
			float Intensity;
		
			LightNormal = &P->SLights[l].Normal;

			Intensity=	LightNormal->X * surfaceNormal.X + 
						LightNormal->Y * surfaceNormal.Y + 
						LightNormal->Z * surfaceNormal.Z;
			if (Intensity > 0.0f)
			{
				RedIntensity   += Intensity * P->SLights[l].Color.Red;
				GreenIntensity += Intensity * P->SLights[l].Color.Green;
				BlueIntensity  += Intensity * P->SLights[l].Color.Blue;
			}
		}
	}

	Color = grPuppet_StaticLightGrp.MaterialColor.Red * RedIntensity;
	v->r = GR_CLAMP(Color, 0.0f, 255.0f);

	Color = grPuppet_StaticLightGrp.MaterialColor.Green * GreenIntensity;
	v->g = GR_CLAMP(Color, 0.0f, 255.0f);

	Color = grPuppet_StaticLightGrp.MaterialColor.Blue * BlueIntensity;
	v->b = GR_CLAMP(Color, 0.0f, 255.0f);
}

#pragma message ("Make a grPuppet_SetShadowPosition(...) ")

#pragma warning (disable:4100)
static void GRCC grPuppet_DrawShadow(const grPuppet *P, 
						const grPose *Joints, 
						grEngine *Engine, 
						const grCamera *Camera)
{
#if 0
	grLVertex v[3];
	
	grVec3d		Impact;
	grXForm3d	RootTransform;
	
	assert( P );
	assert( Camera );
	assert( Joints );

	assert( (P->ShadowBoneIndex < grPose_GetJointCount(Joints)) || (P->ShadowBoneIndex ==GR_POSE_ROOT_JOINT));
	assert( (P->ShadowBoneIndex >=0)					    	|| (P->ShadowBoneIndex ==GR_POSE_ROOT_JOINT));

	grPose_GetJointTransform(Joints,P->ShadowBoneIndex,&RootTransform);
	
	{
		GFX_Plane		Plane;
		grVec3d			Pos1, Pos2;
		GFX_Node		*GFXNodes;
		grWorld_Model	*Model;
		Mesh_RenderQ	*Mesh;
		grActor         *Actor;
		grBoolean		GoodImpact;

			
		GFXNodes = (World)->CurrentBSP->BSPData.GFXNodes;
		
		Pos1 = RootTransform.Translation;
			
		Pos2 = Pos1;

		Pos2.Y -= 30000.0f;

		// Get shadow hit plane impact point
		GoodImpact = Trace_WorldCollisionExact(World, 
									&Pos1,&Pos2,GR_COLLIDE_MODELS,&Impact,&Plane,&Model,&Mesh,&Actor,0, NULL, NULL);

	}
	Impact.Y += 1.0f;

	v[0].r = v[0].b = v[0].g = 0.0f;
	v[1].r = v[1].b = v[1].g = 0.0f;
	v[2].r = v[2].b = v[2].g = 0.0f;
	
#ifdef SHADOW_MAP
	{
		int i;
		grLVertex s[4];
		grVec3d ws[4];
		grVec3d In,Left;
		grVec3d Up;
		grVec3d Zero = {0.0f,0.0f,0.0f};
		
		grVec3d_Subtract(&Impact,&(RootTransform.Translation),&Up);
		grVec3d_Normalize(&Up);
		grVec3d_CrossProduct(&(Plane.Normal),&Up,&Left);
		if (grVec3d_Compare(&Left,&Zero,0.001f)!=GR_FALSE)
			{
				grXForm3d_GetLeft(&(RootTransform),&Left);
			}
		grVec3d_CrossProduct(&Left,&(Plane.Normal),&In);

		grVec3d_Normalize(&Left);
		grVec3d_Normalize(&In);

		s[0].r = s[0].b = s[0].g = 0.0f;
		s[1].r = s[1].b = s[1].g = 0.0f;
		s[2].r = s[2].b = s[2].g = 0.0f;
		s[3].r = s[3].b = s[3].g = 0.0f;

		grVec3d_Scale(&In  ,P->ShadowScale,&In);
		grVec3d_Scale(&Left,P->ShadowScale,&Left);

		s[0].a = s[1].a = s[2].a = s[3].a  = 160.0f;

		s[0].u = 0.0f; s[0].v = 0.0f;
		s[1].u = 1.0f; s[1].v = 0.0f;
		s[2].u = 1.0f; s[2].v = 1.0f;
		s[3].u = 0.0f; s[3].v = 1.0f;
		ws[0].Y = ws[1].Y = ws[2].Y = ws[3].Y = Impact.Y;

		ws[0].X = RootTransform.Translation.X + Left.X - In.X;
		ws[0].Z = RootTransform.Translation.Z + Left.Z - In.Z;

		ws[1].X = RootTransform.Translation.X - Left.X - In.X;
		ws[1].Z = RootTransform.Translation.Z - Left.Z - In.Z;

		ws[2].X = RootTransform.Translation.X - Left.X + In.X;
		ws[2].Z = RootTransform.Translation.Z - Left.Z + In.Z;
		
		ws[3].X = RootTransform.Translation.X + Left.X + In.X;
		ws[3].Z = RootTransform.Translation.Z + Left.Z + In.Z;

		for (i=0; i<4; i++)
			{
				grCamera_Transform(Camera,&ws[i],&ws[i]);
				grCamera_Project(Camera,&ws[i],&ws[i]);
			}

		
		s[0].X = ws[0].X; s[0].Y = ws[0].Y; s[0].Z = ws[0].Z;
		s[1].X = ws[1].X; s[1].Y = ws[1].Y; s[1].Z = ws[1].Z;
		s[2].X = ws[2].X; s[2].Y = ws[2].Y; s[2].Z = ws[2].Z;
		s[3].X = ws[3].X; s[3].Y = ws[3].Y; s[3].Z = ws[3].Z;
		
		grTClip_SetTexture(P->ShadowMap);

		grTClip_Triangle(s);
		s[1] = s[2];
		s[2] = s[3];

		grTClip_Triangle(s);
		
	}
#endif
	
#ifdef CIRCULAR_SHADOW
	v[0].a = v[1].a = v[2].a = 160.0f;
	v[0].u = v[1].u = v[2].u = 0.5f;
	v[0].v = v[1].v = v[2].v = 0.5f;
	
	v[0].X = Impact.X;
	v[0].Y = v[1].Y = v[2].Y = Impact.Y;
	v[0].Z = Impact.Z;
	
	{
		int steps = 30;
		int i;
		grVec3d V;
		grFloat Angle = 0.0f;
		grFloat DAngleDStep = -(2.0f * 3.14159f / (grFloat)steps);
		grFloat Radius = P->ShadowScale;

		V = Impact;
		grCamera_Transform(Camera,&V,&V);
		grCamera_Project(Camera,&V,&V);
		v[0].X = V.X;
		v[0].Y = V.Y;
		v[0].Z = V.Z;

		grTClip_SetTexture(NULL);

		V = Impact;
		V.Z += Radius;
		grCamera_Transform(Camera,&V,&V);
		grCamera_Project(Camera,&V,&V);
		v[1].X = V.X;
		v[1].Y = V.Y;
		v[1].Z = V.Z;
		for (i=0; i<steps+1; i++)
			{
				v[2] = v[1];

				V = Impact;
				V.X += (grFloat)(sin( Angle ) * Radius);
				V.Z += (grFloat)(cos( Angle ) * Radius);
				grCamera_Transform(Camera,&V,&V);
				grCamera_Project(Camera,&V,&V);
				v[1].X = V.X;
				v[1].Y = V.Y;
				v[1].Z = V.Z;

				Angle = Angle + DAngleDStep;
				grTClip_Triangle(v);
			}
	}
#endif

#ifdef PROJECTED_SHADOW			
	{
		int i,j,Count;
		grBodyInst_Index *List;
		grBodyInst_Index Command;
		grBody_SkinVertex *SV;
		
		G = grBodyInst_GetShadowGeometry(P->BodyInstance,
						grPose_GetAllJointTransforms(Joints),0,Camera,&Impact);

		if ( G == NULL )
			{
				grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_DrawShadow:  Failed to get shadow geometry",NULL);
				return GR_FALSE;
			}

		grTClip_SetTexture(NULL);

		Count = G->FaceCount;
		List  = G->FaceList;

		for (i=0; i<Count; i++)
			{	
				Command = *List;
				List ++;
				//Material = *List;
				List ++;

				assert( Command == GR_BODY_FACE_TRIANGLE );

				{
					float AX,AY,BXMinusAX,BYMinusAY,CYMinusAY,CXMinusAX;
					grBodyInst_Index *List2;
					
					List2 = List;
					SV = &(G->SkinVertexArray[ *List2 ]);
					AX = SV->SVPoint.X;
					AY = SV->SVPoint.Y;
					List2++;
					List2++;
					
					SV = &(G->SkinVertexArray[ *List2 ]);
					BXMinusAX = SV->SVPoint.X - AX;
					BYMinusAY = SV->SVPoint.Y - AY;
					List2++;
					List2++;

					SV = &(G->SkinVertexArray[ *List2 ]);
					CXMinusAX = SV->SVPoint.X - AX;
					CYMinusAY = SV->SVPoint.Y - AY;
					List2++;
					List2++;

					// ZCROSS is z the component of a 2d vector cross product of ABxAC
					//#define ZCROSS(Ax,Ay,Bx,By,Cx,Cy)  ((((Bx)-(Ax))*((Cy)-(Ay))) - (((By)-(Ay))*((Cx)-(Ax))))

					// 2d cross product of AB cross AC   (A is vtx[0], B is vtx[1], C is vtx[2]
					if ( ((BXMinusAX * CYMinusAY) - (BYMinusAY * CXMinusAX)) > 0.0f )
						{
							List = List2;
							continue;
						}
				}						

			#define SOME_SCALE (  255.0f / 40.0f )
				for (j=0; j<3; j++)
					{
						SV = &(G->SkinVertexArray[ *List ]);
						List++;

						v[j].X = SV->SVPoint.X;
						v[j].Y = SV->SVPoint.Y;

						v[j].Z = SV->SVPoint.Z;
						v[j].u = SV->SVU;
						v[j].v = SV->SVV;
						
						List++;
						v[j].a = (255.0f- (SV->SVU * SOME_SCALE));
					}
			
				if ((v[0].a > 0) && (v[1].a > 0) && (v[2].a > 0))
					{
						grTClip_Triangle(v);
					}
			}
		assert( ((uint32)List) - ((uint32)G->FaceList) == (uint32)(G->FaceListSize) );
	}
#endif

#endif

}

#pragma warning (default:4100)

extern grBoolean	h_LeftHanded;		// Hack of all mothers, need to check camera to see if left/right handed...

#define	DO_UV_MAPPING

extern grWorld_DebugInfo g_WorldDebugInfo;

//=====================================================================================
//	GPU world meshes (roadmap Phase 1): with a driver that has DRV_Driver::WorldMesh_Render,
//	a puppet is still skinned and lit here, in world space, but its triangles are projected
//	and clipped by the GPU with the same DRV_WorldView the world path uses, instead of by
//	grTClip / grFrustum and grCamera on the CPU. Triangles are sent in runs of one texture.
//=====================================================================================
typedef struct
{
	DRV_Driver		*Driver;
	DRV_WorldView	View;
	grRDriver_Layer	Layer;
	uint32			Flags;
	float			Alpha;			// fOverallAlpha, for every vertex
	int32			NumVerts;
} grPuppet_MeshBatch;

static DRV_MeshVertex	*grPuppet_MeshVerts = NULL;
static int32			grPuppet_MeshVertsMax = 0;

static grTexture *grPuppet_MaterialTexture(const grMaterialSpec *Spec)
{
	grTexture	*TH;
	grBitmap	*Bmp;

	if (!Spec)
		return NULL;
	TH = grMaterialSpec_GetLayerTexture(Spec, 0);
	if (TH)
		return TH;
	Bmp = grMaterialSpec_GetLayerBitmap(Spec, 0);
	return Bmp ? grBitmap_GetTHandle(Bmp) : NULL;
}

// Returns GR_FALSE when the puppet must use the CPU path: no driver support, a material
// without a texture (drawn Gouraud there), or a frustum with too many planes.
// CameraSpaceFrustum is NULL for the camera's own view.
static grBoolean grPuppet_MeshBegin(const grPuppet *P, const grEngine *Engine, const grCamera *Camera,
									const grFrustum *CameraSpaceFrustum, uint32 RenderFlags,
									grPuppet_MeshBatch *Batch)
{
	DRV_Driver	*Driver = Engine->DriverInfo.RDriver;
	int32		i;

	if (!Driver || !Driver->WorldMesh_Render ||
		Driver->WorldMesh_Render(NULL, 0, NULL, NULL, 0) != DRV_WORLD_FACE_DRAWN)
		return GR_FALSE;
	if (CameraSpaceFrustum && CameraSpaceFrustum->NumPlanes > DRV_WORLD_MAX_CLIP_PLANES)
		return GR_FALSE;
	for (i = 0; i < P->MaterialCount; i++)
	{
		if (!grPuppet_MaterialTexture(P->MaterialArray[i].Material))
			return GR_FALSE;
	}

	memset(Batch, 0, sizeof(*Batch));
	Batch->Driver = Driver;
	Batch->Alpha = P->fOverallAlpha;
	Batch->Flags = RenderFlags | Engine->DefaultRenderFlags;		// as grEngine_RenderPoly

	// Same view as grBSP_RenderGpuWorld, for world-space vertices. The CPU path clips
	// actors to the camera rect (or the portal frustum) but not to the far plane.
	Batch->View.ModelToCamera = *grCamera_XForm(Camera);
	grCamera_GetScreenProjection(Camera, &Batch->View.Scale, &Batch->View.XCenter, &Batch->View.YCenter);
	Batch->View.ZScale = grCamera_GetZScale(Camera);
	grCamera_GetScreenSize(Camera, &Batch->View.HalfWidth, &Batch->View.HalfHeight);
	Batch->View.HalfWidth *= 0.5f;
	Batch->View.HalfHeight *= 0.5f;
	if (CameraSpaceFrustum)
	{
		// Same planes and sides as grFrustum_ClipLVerts* (inside: N.p - Dist >= 0)
		for (i = 0; i < CameraSpaceFrustum->NumPlanes; i++)
		{
			const grPlane	*Plane = &CameraSpaceFrustum->Planes[i];

			Batch->View.ClipPlanes[i][0] = Plane->Normal.X;
			Batch->View.ClipPlanes[i][1] = Plane->Normal.Y;
			Batch->View.ClipPlanes[i][2] = Plane->Normal.Z;
			Batch->View.ClipPlanes[i][3] = -Plane->Dist;
		}
		Batch->View.NumClipPlanes = CameraSpaceFrustum->NumPlanes;
	}
	return GR_TRUE;
}

// Room for NumFaces triangles.
static grBoolean grPuppet_MeshReserve(int32 NumFaces)
{
	if (grPuppet_MeshVertsMax < NumFaces * 3)
	{
		DRV_MeshVertex *Verts = (DRV_MeshVertex *)grRam_Realloc(grPuppet_MeshVerts, sizeof(DRV_MeshVertex) * NumFaces * 3);
		if (!Verts)
			return GR_FALSE;
		grPuppet_MeshVerts = Verts;
		grPuppet_MeshVertsMax = NumFaces * 3;
	}
	return GR_TRUE;
}

static void grPuppet_MeshFlush(grPuppet_MeshBatch *Batch)
{
	if (Batch->NumVerts > 0 && Batch->Layer.THandle)
		Batch->Driver->WorldMesh_Render(grPuppet_MeshVerts, Batch->NumVerts, &Batch->View, &Batch->Layer, Batch->Flags);
	Batch->NumVerts = 0;
}

static void grPuppet_MeshSetMaterial(grPuppet_MeshBatch *Batch, const grMaterialSpec *Spec)
{
	grTexture	*TH = grPuppet_MaterialTexture(Spec);

	if (TH != Batch->Layer.THandle)
	{
		grPuppet_MeshFlush(Batch);
		Batch->Layer.THandle = TH;
	}
}

// Adds a world-space triangle if it faces the camera (the test grPuppet_RenderThroughFrustum uses).
static void grPuppet_MeshAddTriangle(grPuppet_MeshBatch *Batch, const grLVertex *Verts, const grVec3d *Pov)
{
	grVec3d			v1, v2, v3;
	int32			i;
	DRV_MeshVertex	*Dst;

	grVec3d_Subtract((grVec3d*)&Verts[2], (grVec3d*)&Verts[1], &v1);
	grVec3d_Subtract((grVec3d*)&Verts[0], (grVec3d*)&Verts[1], &v2);
	grVec3d_CrossProduct(&v1, &v2, &v3);
	if (grVec3d_DotProduct(&v3, Pov) - grVec3d_DotProduct(&v3, (grVec3d*)&Verts[0]) <= 0.0f)
		return;		// Backfaced to camera

	Dst = &grPuppet_MeshVerts[Batch->NumVerts];
	for (i = 0; i < 3; i++, Dst++)
	{
		Dst->Pos[0] = Verts[i].X;
		Dst->Pos[1] = Verts[i].Y;
		Dst->Pos[2] = Verts[i].Z;
		Dst->u = Verts[i].u;
		Dst->v = Verts[i].v;
		Dst->r = Verts[i].r;
		Dst->g = Verts[i].g;
		Dst->b = Verts[i].b;
		Dst->a = Batch->Alpha;
	}
	Batch->NumVerts += 3;
}

grBoolean grPuppet_RenderThroughFrustum(const grPuppet		*P,
										const grPose		*Joints,
										const grExtBox		*Box, 
										grEngine			*Engine, 
										const grWorld		*World,
										const grCamera		*Camera, 
										const grFrustum		*Frustum,
										grBoolean updateStaticLightingFlag)
{
	//	TOM 05-24-03 This function needs to be rewritten to make it easier to handle
	//	pointer assignment errors -- especially for local var PM.

	uint32						ClipFlags;
	grVec3d						Scale;
	const grXFArray				*JointTransforms = NULL;
	const grBodyInst_Geometry	*G = NULL;
	grFrustum					WorldSpaceFrustum;
	const grFrustum				*CameraSpaceFrustum;
	grPuppet					*LP = NULL;
	grPuppet_MeshBatch			Mesh;
	grBoolean					UseMesh;

	assert( P      );
	assert( Engine );
	assert( Camera );
	assert( Joints );

	LP = (grPuppet *)P;

	JointTransforms = grPose_GetAllJointTransforms(Joints);

#pragma message ("Level of detail hacked:")

	grPose_GetScale(Joints,&Scale);
	G = grBodyInst_GetGeometry(P->BodyInstance, &Scale, JointTransforms, 0, NULL);

	grFrustum_TransformToWorldSpace(Frustum, Camera, &WorldSpaceFrustum);
	CameraSpaceFrustum = Frustum;
	Frustum = &WorldSpaceFrustum;

	// Setup clip flags to clip to all frustum planes...
	ClipFlags = 0xffff;

	// Now either totally reject actor against frustum, or remove planes that don't need to be clipped against, etc...
//#if 1
	{
		grPlane		*pPlane = NULL;
		int32		k;

		pPlane = ((grFrustum*)Frustum)->Planes;

		for (k=0; k< Frustum->NumPlanes; k++, pPlane++)
		{
			grPlane_Side	Side;

			pPlane->Type = Type_Any;

			Side = grPlane_BoxSide(pPlane, Box, 0.01f);

			if (Side == PSIDE_BACK)
				return GR_TRUE;			// Actor not in view frustum

			if (Side == PSIDE_FRONT)
				ClipFlags ^= (1<<k);	// Don't need to clip to this plane
		}
	}
//#endif

	if (G == NULL)
	{
		grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_RenderThroughFrustum: Failed to get draw geometry");
		return GR_FALSE;
	}

	{
		int32				NumFaces;
		int32				i;
		grBodyInst_Index	*List = NULL;
		grXForm3d			RootTransform;
		const grXForm3d		*pActorToWorldXForm = NULL;
		grXForm3d			MapperXForm, *pMapperXForm;
		uint32				RenderFlags;
		grBodyInst_Index	LastMaterial;

		if (h_LeftHanded)
			RenderFlags = 0;
		else
			RenderFlags = GR_RENDER_FLAG_COUNTER_CLOCKWISE;

		UseMesh = grPuppet_MeshBegin(P, Engine, Camera, CameraSpaceFrustum, RenderFlags, &Mesh) &&
			grPuppet_MeshReserve(G->FaceCount);

		grPuppet_StaticLightGrp.UseFillLight		 = P->UseFillLight;
		grPuppet_StaticLightGrp.FillLightNormal		 = P->FillLightNormal;
		grPuppet_StaticLightGrp.FillLightColor.Red	 = P->FillLightColor.Red;
		grPuppet_StaticLightGrp.FillLightColor.Green = P->FillLightColor.Green;
		grPuppet_StaticLightGrp.FillLightColor.Blue  = P->FillLightColor.Blue;
		grPuppet_StaticLightGrp.PerBoneLighting      = P->PerBoneLighting;

		grPose_GetJointTransform(Joints,P->LightReferenceBoneIndex,&(RootTransform));

		pActorToWorldXForm = grCamera_XForm(Camera);

		// do dynamic lighting pass

		if (P->MaxDynamicLightsToUse > 0)
		{
			if (P->PerBoneLighting)
			{
				int BoneCount;
				const grXForm3d *XFA = grXFArray_GetElements(JointTransforms, &BoneCount);
				if (BoneCount>0)
				{
					if (P->BoneLightArraySize < BoneCount)
					{
						// realloc light array to correct size
						grPuppet_BoneLight *LG;

						LG = (grPuppet_BoneLight *)grRam_Realloc(P->BoneLightArray, sizeof(grPuppet_BoneLight) * BoneCount);
						if (LG==NULL)
						{
							grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_Render: Failed to allocate space for bone lighting info cache");
							return GR_FALSE;
						}
						LP->BoneLightArray = LG;
						LP->BoneLightArraySize = BoneCount;
					}
					for (i=0; i<BoneCount; i++) // loop thru the bones
					{
						// for all dynamic lights, accumulate onto bone i
						P->BoneLightArray[i].DLightCount = grPuppet_PrepDynamicLights(P,World,
							P->BoneLightArray[i].DLights,&(XFA[i].Translation));
					}
				}
			}
			else
			{
				grPuppet_StaticLightGrp.DLightCount = grPuppet_PrepDynamicLights(P,World,
					grPuppet_StaticLightGrp.DLights,&(RootTransform.Translation));
			}
		}

		else
		{
			grPuppet_StaticLightGrp.DLightCount = 0;
		}

		// do static lighting pass

		if (P->MaxStaticLightsToUse > 0)
		{
			if (updateStaticLightingFlag) // need to re-cache static lighting for this puppet
			{
				if (P->PerBoneLighting)
				{
					int BoneCount;
					const grXForm3d *XFA = grXFArray_GetElements(JointTransforms, &BoneCount);
					if (BoneCount>0)
					{
						if (P->BoneLightArraySize < BoneCount)
						{
							// realloc light array to correct size
							grPuppet_BoneLight *LG = NULL;

							LG = (grPuppet_BoneLight *)grRam_Realloc(P->BoneLightArray, sizeof(grPuppet_BoneLight) * BoneCount);
							if (LG==NULL)
							{
								grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_Render: Failed to allocate space for bone lighting info cache");
								return GR_FALSE;
							}
							LP->BoneLightArray = LG;
							LP->BoneLightArraySize = BoneCount;
						}
						for (i=0; i<BoneCount; i++) // loop thru the bones
						{
							// for all static lights, accumulate onto bone i
							P->BoneLightArray[i].SLightCount = grPuppet_PrepStaticLights(P,
								World,
								P->BoneLightArray[i].SLights,
								&(XFA[i].Translation));
						}
					}
				}
				else // not doing per-bone lighting
				{
					LP->SLightCount = grPuppet_PrepStaticLights(P,
						World,
						LP->SLights,
						&(RootTransform.Translation));
				}
			}
		}

		else
		{
			LP->SLightCount = 0;
		}

		// @@
		grPuppet_ComputeAmbientLight(P, &(grPuppet_StaticLightGrp.Ambient), &(RootTransform.Translation));

		NumFaces	= G->FaceCount;
		List		= G->FaceList;

		LastMaterial = -1;

		// For each face, clip it to the view frustum 
		grPuppet_Material	*PM = NULL;
		for (i=0; i<NumFaces; i++)
		{
#define MAX_TEMP_VERTS		64		

			int32				v;
			grBodyInst_Index	Command, Material;
			float				Dist;
			grLVertex			LVerts1[MAX_TEMP_VERTS], LVerts2[MAX_TEMP_VERTS], *pLVert;
			grTLVertex			TLVerts[MAX_TEMP_VERTS];
			grVec3d				v1, v2, v3;
			grFrustum_LClipInfo	ClipInfo;

			Command	= *List;
			List++;
			Material = *List;
			List ++;

			assert( Command == GR_BODYINST_FACE_TRIANGLE );
			assert( Material>=0 );
			assert( Material<P->MaterialCount);

#ifdef DO_UV_MAPPING
			if (Material != LastMaterial)
			{
				PM = &(P->MaterialArray[Material]);
				if (PM)
				{
					grPuppet_StaticLightGrp.MaterialColor = PM->Color;

					if (PM->Mapper != grUVMap_Projection)
					{
						pMapperXForm = (grXForm3d*)pActorToWorldXForm;
					}
					else
					{
#pragma message("Puppet.c: hard-coded default projection matrix vals for case of grUVMap_Projection")
						grXForm3d		ProjXForm;

						ProjXForm.AX = 0.03f; ProjXForm.AY = 0.02f; ProjXForm.AZ = 0.0f;
						ProjXForm.BX = 0.01f; ProjXForm.BY = 0.09f; ProjXForm.BZ = 0.0f;
						ProjXForm.CX = 0.06f; ProjXForm.CY = 0.08f; ProjXForm.CZ = 0.0f;

						grVec3d_Clear(&ProjXForm.Translation);

						pMapperXForm = &MapperXForm;

						grXForm3d_Multiply(&ProjXForm, pActorToWorldXForm, pMapperXForm);
					}	//	else...

					LastMaterial = Material;		// Make LastMaterial current
				}	//	if (PM)...
//				else
//				{
//					return GR_FALSE;
//				}
			}	//	if (Material != LastMaterial)...
#else
			PM = &(P->MaterialArray[Material]);
			if (PM)
			{
				grPuppet_StaticLightGrp.MaterialColor = PM->Color;
			}
			else
			{
				return GR_FALSE;
			}
#endif

				// Fill in the LVert array
				pLVert = LVerts1;

				for (v=0; v< 3; v++, pLVert++)
				{
					grBodyInst_SkinVertex	*SVert;

					SVert = &G->SkinVertexArray[*List];
					List++;

					// Get XYZ
					*((grVec3d*)pLVert) = SVert->SVPoint;

					// Get UV
#ifdef DO_UV_MAPPING
#pragma message("Puppet : UVMapper should act on an array of verts!")
					if (PM)
					{
						if (PM->Mapper != NULL)
						{
							grLVertex		MapVert;

							*((grVec3d*)&MapVert) = SVert->SVW;

							PM->Mapper(pMapperXForm, &MapVert, &G->NormalArray[*List], 1);

							pLVert->u = MapVert.u;
							pLVert->v = MapVert.v;
						}
						else
	#endif
						{
							pLVert->u = SVert->SVU;
							pLVert->v = SVert->SVV;
						}
					}	//	if (PM)...

					assert( ((float)fabs(1.0-grVec3d_Length( &(G->NormalArray[ *List ] ))))< 0.001f );

					grPuppet_StaticLightGrp.SurfaceNormal = (G->NormalArray[ *List ]);
					List++;

					// Get RGB
					grPuppet_SetVertexColor(LP, pLVert,SVert->ReferenceBoneIndex);
				}	//	for...

				if (UseMesh)
				{
					if (PM)
					{
						grPuppet_MeshSetMaterial(&Mesh, PM->Material);
						grPuppet_MeshAddTriangle(&Mesh, LVerts1, grCamera_GetPov(Camera));
						g_WorldDebugInfo.NumActorPolys++;
					}
					continue;
				}

#pragma message ("This backface rejection code should go above uv/lighting computations...")
				grVec3d_Subtract((grVec3d*)&LVerts1[2], (grVec3d*)&LVerts1[1], &v1);
				grVec3d_Subtract((grVec3d*)&LVerts1[0], (grVec3d*)&LVerts1[1], &v2);
				grVec3d_CrossProduct(&v1, &v2, &v3);
				grVec3d_Normalize(&v3);

				Dist = grVec3d_DotProduct(&v3, (grVec3d*)&LVerts1[0]);

				Dist = grVec3d_DotProduct(&v3, grCamera_GetPov(Camera)) - Dist;

				if (Dist <= 0)
					continue;		// Backfaced to camera

				ClipInfo.NumSrcVerts = 3;
				ClipInfo.SrcVerts = LVerts1;

				ClipInfo.Work1 = LVerts1;
				ClipInfo.Work2 = LVerts2;

				ClipInfo.ClipFlags = ClipFlags;

				// Clip UVRGB
				if (!grFrustum_ClipLVertsXYZUVRGB(Frustum, &ClipInfo))
					continue;		// Poly was clipped away

				// Transform to world space...
				for (pLVert = ClipInfo.DstVerts, v=0; v< ClipInfo.NumDstVerts; v++, pLVert++)
					grXForm3d_Transform(pActorToWorldXForm, (grVec3d*)pLVert, (grVec3d*)pLVert);

				// Project to screenspace
				grCamera_ProjectAndClampLArray(Camera, ClipInfo.DstVerts, TLVerts, ClipInfo.NumDstVerts);

				//TLVerts[0].a = 255.0f;
				//	[MacroArt::Begin]
				TLVerts[0].a = P->fOverallAlpha;
				//			if(P->fOverallAlpha<255.0f) RenderFlags=RenderFlags|GR_RENDER_FLAG_ALPHA;
				//	[MacroArt::End]

				g_WorldDebugInfo.NumActorPolys++;
			if (PM)
			{
				grEngine_RenderPoly(Engine, TLVerts, ClipInfo.NumDstVerts, PM->Material, RenderFlags);
			}	//	if (PM)...
//			else
//			{
//				return GR_FALSE;
//			}

		}
		if (UseMesh)
			grPuppet_MeshFlush(&Mesh);
	}

	//"Need to write a RenderShadowThroughFrustum...")
	/*
	if (P->DoShadow)
	{
	grPuppet_DrawShadow(P,Engine,World,Camera);
	}
	*/
	return GR_TRUE;
}

#ifdef PROFILE
#define PUPPET_AVERAGE_ACROSS 60
double Puppet_AverageCount[PUPPET_AVERAGE_ACROSS]={
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,
	0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
int Puppet_AverageIndex = 0;
#endif


grBoolean	grPuppet_Render(const grPuppet	*P, 
							const grPose	*Joints,
							grEngine		*Engine, 
							const grWorld	*World,
							const grCamera	*Camera, 
							grExtBox		*TestBox,
							grBoolean		updateStaticLightingFlag)
{
	grPuppet *LP;
	const grXFArray *JointTransforms;
	grVec3d Scale;
	#ifdef PROFILE
	rdtsc_timer_type RDTSCStart,RDTSCEnd;
	#endif
	grRect ClippingRect;
	grBoolean Clipping = GR_TRUE;

	// BEGIN - Fixed far clip plane for actors - paradoxnj 4/21/2005
	grBoolean Enable;
	float ZFar;
	// END - Fixed far clip plane for actors - paradoxnj 4/21/2005

	#define BACK_EDGE (1.0f)

	const grBodyInst_Geometry *G;
	grPuppet_MeshBatch Mesh;
	grBoolean UseMesh;
//	[MacroArt::Begin]
	uint32	RenderFlags;
//	[MacroArt::End]

	assert( P      );
	assert( Engine );
	assert( Camera );

	#ifdef PROFILE
	rdtsc_read(&RDTSCStart);
    rdtsc_zero(&RDTSCEnd);
	#endif


	LP = (grPuppet*)P;
	grCamera_GetClippingRect(Camera,&ClippingRect);
	
	// BEGIN - Fixed far clip plane for actors - paradoxnj 4/21/2005
	grCamera_GetFarClipPlane(Camera, &Enable, &ZFar);
	if (!Enable)
		ZFar = BACK_EDGE;
	// END - Fixed far clip plane for actor - paradoxnj 4/21/2005

	if (TestBox != NULL)
	{
		// see if the test box is visible on the screen.  If not: don't draw actor.
		// (transform and project it to the screen, then check extents of that projection
		//  against the clipping rect)
		grVec3d BoxCorners[8];
		const grXForm3d *ObjectToCamera;
		grVec3d Maxs,Mins;
		#define BIG_NUMBER (99e9f)  
		int i;

		BoxCorners[0] = TestBox->Min;
		BoxCorners[1] = BoxCorners[0];  BoxCorners[1].X = TestBox->Max.X;
		BoxCorners[2] = BoxCorners[0];  BoxCorners[2].Y = TestBox->Max.Y;
		BoxCorners[3] = BoxCorners[0];  BoxCorners[3].Z = TestBox->Max.Z;
		BoxCorners[4] = TestBox->Max;
		BoxCorners[5] = BoxCorners[4];  BoxCorners[5].X = TestBox->Min.X;
		BoxCorners[6] = BoxCorners[4];  BoxCorners[6].Y = TestBox->Min.Y;
		BoxCorners[7] = BoxCorners[4];  BoxCorners[7].Z = TestBox->Min.Z;

		ObjectToCamera = grCamera_XForm(Camera);
		assert( ObjectToCamera );

		grVec3d_Set(&Maxs,-BIG_NUMBER,-BIG_NUMBER,-BIG_NUMBER);
		grVec3d_Set(&Mins, BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
		for (i=0; i<8; i++)
		{
			grVec3d V;
			grXForm3d_Transform(  ObjectToCamera,&(BoxCorners[i]),&(BoxCorners[i]));
			grCamera_Project(  Camera,&(BoxCorners[i]),&V);
			if (V.X > Maxs.X ) Maxs.X = V.X;
			if (V.X < Mins.X ) Mins.X = V.X;
			if (V.Y > Maxs.Y ) Maxs.Y = V.Y;
			if (V.Y < Mins.Y ) Mins.Y = V.Y;
			if (V.Z > Maxs.Z ) Maxs.Z = V.Z;
			if (V.Z < Mins.Z ) Mins.Z = V.Z;
		}

		if (   (Maxs.X < ClippingRect.Left) 
			|| (Mins.X > ClippingRect.Right)
			|| (Maxs.Y < ClippingRect.Top) 
			|| (Mins.Y > ClippingRect.Bottom)
			|| (Maxs.Z < BACK_EDGE) )
		{
			// not gonna draw: box is not visible.
			return GR_TRUE;
		}

		// BEGIN - Fixed far clip plane for actors - paradoxnj 4/21/2005
		if (Enable)
		{
			if (Mins.Z > ZFar)
				return GR_TRUE;				// Beyond ZFar ClipPlane
		}
		// END - Fixed far clip plane for actors - paradoxnj 4/21/2005
	}

	// Now actor is in the camera field - test it against the BSP area
	//extern grBSPNode_Area *grBSP_FindArea(grBSP *BSP, const grVec3d *Pos);


	Engine->DebugInfo.NumActors++;
	grTClip_SetupEdges(Engine,
						(grFloat)ClippingRect.Left,
						(grFloat)ClippingRect.Right,
						(grFloat)ClippingRect.Top,
						(grFloat)ClippingRect.Bottom,
						BACK_EDGE);
		
	JointTransforms = grPose_GetAllJointTransforms(Joints);

//#pragma message ("Level of detail hacked:")
	grPose_GetScale(Joints,&Scale);

	// The GPU path takes world-space vertices; the CPU path projects them here.
	UseMesh = grPuppet_MeshBegin(P, Engine, Camera, NULL, GR_RENDER_FLAG_COUNTER_CLOCKWISE, &Mesh);
	G = grBodyInst_GetGeometry(P->BodyInstance, &Scale, JointTransforms, 0, UseMesh ? NULL : Camera);
	if (G && UseMesh && !grPuppet_MeshReserve(G->FaceCount))
	{
		UseMesh = GR_FALSE;
		G = grBodyInst_GetGeometry(P->BodyInstance, &Scale, JointTransforms, 0, Camera);
	}

	if ( G == NULL )
	{
		grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_Render: Failed to get draw geometry");
		return GR_FALSE;
	}

#ifdef ONE_OVER_Z_PIPELINE
#define TEST_Z_OUT(zzz, edge) 		((zzz) > (edge)) 
#define TEST_Z_IN(zzz, edge) 		((zzz) < (edge)) 
#pragma message ("test this! this is untested")
#else
#define TEST_Z_OUT(zzz, edge) 		((zzz) < (edge)) 
#define TEST_Z_IN(zzz, edge) 		((zzz) > (edge)) 
#endif


	// check for trivial rejection (screen space: CPU path only)
	if (!UseMesh)
	{
		if (   (G->Maxs.X < ClippingRect.Left) 
			|| (G->Mins.X > ClippingRect.Right)
			|| (G->Maxs.Y < ClippingRect.Top) 
			|| (G->Mins.Y > ClippingRect.Bottom)
			|| ( TEST_Z_OUT( G->Maxs.Z, BACK_EDGE) ) )
		{
			// not gonna draw
			return GR_TRUE;
		}

		if (   (G->Maxs.X < ClippingRect.Right) 
			&& (G->Mins.X > ClippingRect.Left)
			&& (G->Maxs.Y < ClippingRect.Bottom) 
			&& (G->Mins.Y > ClippingRect.Top)
			&& ( TEST_Z_IN( G->Mins.Z, BACK_EDGE) ) )
		{
			// not gonna clip
			Clipping = GR_FALSE;
		}
		else
		{
			Clipping = GR_TRUE;
		}
	}

	{
		grLVertex v[3], mapVert;
		int i,j,Count;
		grBodyInst_Index *List;
		grBodyInst_Index Command;
		grBodyInst_SkinVertex *SV;
		grXForm3d RootTransform;
		grPuppet_Material *PM;
		grBodyInst_Index Material,LastMaterial;
		grXForm3d CamXForm, projXForm, mapperXForm;

		grCamera_GetTransposeXForm(Camera, &CamXForm);
		PM = NULL;

		grPuppet_StaticLightGrp.UseFillLight		 = P->UseFillLight;
		grPuppet_StaticLightGrp.FillLightNormal		 = P->FillLightNormal;
		grPuppet_StaticLightGrp.FillLightColor.Red	 = P->FillLightColor.Red;
		grPuppet_StaticLightGrp.FillLightColor.Green = P->FillLightColor.Green;
		grPuppet_StaticLightGrp.FillLightColor.Blue  = P->FillLightColor.Blue;
		grPuppet_StaticLightGrp.PerBoneLighting		 = P->PerBoneLighting;

		grPose_GetJointTransform(Joints,P->LightReferenceBoneIndex,&(RootTransform));

		// do dynamic lighting pass

		if (P->MaxDynamicLightsToUse > 0)
		{
			if (P->PerBoneLighting)
			{
				int BoneCount;
				const grXForm3d *XFA = grXFArray_GetElements(JointTransforms, &BoneCount);
				if (BoneCount>0)
				{
					if (P->BoneLightArraySize < BoneCount)
					{
						// realloc light array to correct size
						grPuppet_BoneLight *LG;
				
						LG = (grPuppet_BoneLight *)grRam_Realloc(P->BoneLightArray, sizeof(grPuppet_BoneLight) * BoneCount);
						if (LG==NULL)
						{
							grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_Render: Failed to allocate space for bone lighting info cache");
							return GR_FALSE;
						}
						LP->BoneLightArray = LG;
						LP->BoneLightArraySize = BoneCount;
					}
					for (i=0; i<BoneCount; i++) // loop thru the bones
					{
						// for all dynamic lights, accumulate onto bone i
						LP->BoneLightArray[i].DLightCount = grPuppet_PrepDynamicLights(P,World,
							P->BoneLightArray[i].DLights,&(XFA[i].Translation));
					}
				}
			}
			else
			{
				grPuppet_StaticLightGrp.DLightCount = grPuppet_PrepDynamicLights(P,World,
							grPuppet_StaticLightGrp.DLights,&(RootTransform.Translation));
			}
		}

		else
		{
			grPuppet_StaticLightGrp.DLightCount = 0;
		}

		// do static lighting pass

		if (P->MaxStaticLightsToUse > 0)
		{
			if (updateStaticLightingFlag) // need to re-cache static lighting for this puppet
			{
				if (P->PerBoneLighting)
				{
					int BoneCount;
					const grXForm3d *XFA = grXFArray_GetElements(JointTransforms, &BoneCount);
					if (BoneCount>0)
					{
						if (P->BoneLightArraySize < BoneCount)
						{
							// realloc light array to correct size
							grPuppet_BoneLight *LG;
					
							LG = (grPuppet_BoneLight *)grRam_Realloc(P->BoneLightArray, sizeof(grPuppet_BoneLight) * BoneCount);
							if (LG==NULL)
							{
								grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grPuppet_Render: Failed to allocate space for bone lighting info cache");
								return GR_FALSE;
							}
							LP->BoneLightArray = LG;
							LP->BoneLightArraySize = BoneCount;
						}
						for (i=0; i<BoneCount; i++) // loop thru the bones
						{
							// for all static lights, accumulate onto bone i
							LP->BoneLightArray[i].SLightCount = grPuppet_PrepStaticLights(P,
								World,
								P->BoneLightArray[i].SLights,
								&(XFA[i].Translation));
						}
					}
				}
				else // not doing per-bone lighting
				{
					LP->SLightCount = grPuppet_PrepStaticLights(P,
						World,
						LP->SLights,
						&(RootTransform.Translation));
				}
			}
		}

		else
		{
			LP->SLightCount = 0;
		}

// @@
		grPuppet_ComputeAmbientLight(P, &(grPuppet_StaticLightGrp.Ambient),&(RootTransform.Translation));
		
		Count = G->FaceCount;
		List  = G->FaceList;
		//v[0].a = v[1].a= v[2].a = 255.0f;

//	[MacroArt::Begin]
	v[0].a = v[1].a= v[2].a =P->fOverallAlpha;
	RenderFlags=GR_RENDER_FLAG_COUNTER_CLOCKWISE;
//	if(P->fOverallAlpha<255.0f) RenderFlags=RenderFlags|GR_RENDER_FLAG_ALPHA;
//	[MacroArt::End]


		LastMaterial = -1;

		for (i=0; i<Count; i++)
		{	

			Command = *List;
			List ++;
			Material = *List;
			List ++;

			assert( Command == GR_BODYINST_FACE_TRIANGLE );
			assert( Material>=0 );
			assert( Material<P->MaterialCount);

			{
				float AX,AY,BXMinusAX,BYMinusAY,CYMinusAY,CXMinusAX;
				grBodyInst_Index *List2;
				
				List2 = List;
				SV = &(G->SkinVertexArray[ *List2 ]);
				AX = SV->SVPoint.X;
				AY = SV->SVPoint.Y;
				List2++;
				List2++;
				
				SV = &(G->SkinVertexArray[ *List2 ]);
				BXMinusAX = SV->SVPoint.X - AX;
				BYMinusAY = SV->SVPoint.Y - AY;
				List2++;
				List2++;

				SV = &(G->SkinVertexArray[ *List2 ]);
				CXMinusAX = SV->SVPoint.X - AX;
				CYMinusAY = SV->SVPoint.Y - AY;
				List2++;
				List2++;

				// ZCROSS is z the component of a 2d vector cross product of ABxAC
				//#define ZCROSS(Ax,Ay,Bx,By,Cx,Cy)  ((((Bx)-(Ax))*((Cy)-(Ay))) - (((By)-(Ay))*((Cx)-(Ax))))
				// 2d cross product of AB cross AC   (A is vtx[0], B is vtx[1], C is vtx[2]
				// (screen space; the GPU path tests in world space in grPuppet_MeshAddTriangle)
				if ( !UseMesh && ((BXMinusAX * CYMinusAY) - (BYMinusAY * CXMinusAX)) > 0.0f )
				{
					List = List2;
					continue;
				}
				
			}

			if (Material != LastMaterial)
			{
				PM = &(P->MaterialArray[Material]);
				grTClip_SetTexture(PM->Material,0);
				if (UseMesh)
					grPuppet_MeshSetMaterial(&Mesh, PM->Material);
				grPuppet_StaticLightGrp.MaterialColor = PM->Color;

				if (PM->Mapper != grUVMap_Projection)
				{
					mapperXForm = CamXForm;
				}
				else
				{
#pragma message("Puppet.c: hard-coded default projection matrix vals for case of grUVMap_Projection")
					projXForm.AX = 0.03f; projXForm.AY = 0.02f; projXForm.AZ = 0.0f;
					projXForm.BX = 0.01f; projXForm.BY = 0.09f; projXForm.BZ = 0.0f;
					projXForm.CX = 0.06f; projXForm.CY = 0.08f; projXForm.CZ = 0.0f;
					grVec3d_Clear(&projXForm.Translation);

					grXForm3d_Multiply(&projXForm, &CamXForm, &mapperXForm);
				}

				LastMaterial = Material;		// Make LastMaterial current
			}

			for (j=0; j<3; j++)
			{
				SV = &(G->SkinVertexArray[ *List ]);
				List++;

				v[j].X = SV->SVPoint.X;
				v[j].Y = SV->SVPoint.Y;
				v[j].Z = SV->SVPoint.Z;

#pragma message("Puppet : UVMapper should act on an array of verts!")
				if (PM->Mapper != NULL)
				{
					*((grVec3d *)&mapVert) = SV->SVW;

					PM->Mapper(&mapperXForm, &mapVert, &G->NormalArray[*List], 1);

					v[j].u = mapVert.u;
					v[j].v = mapVert.v;
				}
				else
				{
					v[j].u = SV->SVU;
					v[j].v = SV->SVV;
				}
				assert( ((float)fabs(1.0-grVec3d_Length( &(G->NormalArray[ *List ] ))))< 0.001f );
				
				grPuppet_StaticLightGrp.SurfaceNormal = (G->NormalArray[ *List ]);
				List++;

				grPuppet_SetVertexColor(LP, &v[j], SV->ReferenceBoneIndex);

			}
		
			g_WorldDebugInfo.NumActorPolys++;

			if (UseMesh)
			{
				grPuppet_MeshAddTriangle(&Mesh, v, grCamera_GetPov(Camera));
			}
			else if (Clipping)
			{
				grTClip_Triangle(v);
			}
			else
			{
				assert (PM != NULL);

//	[MacroArt::Begin]
				// BEGIN - Get rid of JE_ crap - paradoxnj 4/21/2005
				grEngine_RenderPoly(Engine, (grTLVertex *)v, 3, PM->Material,RenderFlags);
				// END - Get rid of JE_ crap - paradoxnj 4/21/2005
//				grEngine_RenderPoly(Engine, (GR_TLVertex *)v, 3, PM->Bitmap,GR_RENDER_FLAG_COUNTER_CLOCKWISE );
//	[MacroArt::End]

			}

		}
		if (UseMesh)
			grPuppet_MeshFlush(&Mesh);
		assert( ((uint32)List) - ((uint32)G->FaceList) == (uint32)(G->FaceListSize) );
	}

// @@
#pragma message("Puppet.c Line 2057:  Why are shadows not implemented - paradoxnj 4/21/2005")
/*
	if (P->DoShadow)
	{
		grPuppet_DrawShadow(P,Joints,Engine, Camera);
	}
*/

	#ifdef PROFILE
	{
		double Count=0.0;
		int i;

		rdtsc_read(&RDTSCEnd);
		rdtsc_delta(&RDTSCStart,&RDTSCEnd,&RDTSCEnd);
		//grEngine_Printf(Engine, 320,10,"Puppet Render Time=%f",(double)(rdtsc_cycles(&RDTSCEnd)/200000000.0));
		//grEngine_Printf(Engine, 320,30,"Puppet Render Cycles=%f",(double)(rdtsc_cycles(&RDTSCEnd)));
		Puppet_AverageCount[(Puppet_AverageIndex++)%PUPPET_AVERAGE_ACROSS] = rdtsc_cycles(&RDTSCEnd);
		for (i=0; i<PUPPET_AVERAGE_ACROSS; i++)
			{	
				Count+=Puppet_AverageCount[i];
			}
		Count /= (double)PUPPET_AVERAGE_ACROSS;

		//grEngine_Printf(Engine, 320,60,"Puppet AVG Render Time=%f",(double)(Count/200000000.0));
		//grEngine_Printf(Engine, 320,90,"Puppet AVG Render Cycles=%f",(double)(Count));
				
	}
	#endif

	return GR_TRUE;
}

void grPuppet_SetShadow(grPuppet *P, grBoolean DoShadow, 
		grFloat Scale, const grMaterialSpec *ShadowMap,
		int BoneIndex)
{
	assert( P );
	assert( (DoShadow==GR_FALSE) || (DoShadow==GR_TRUE));

	if ( P->ShadowMap )
//		grBitmap_Destroy((grBitmap **)&(P->ShadowMap));
		grMaterialSpec_Destroy((grMaterialSpec **)&(P->ShadowMap));

	P->DoShadow = DoShadow;
	P->ShadowScale = Scale;
	P->ShadowMap = ShadowMap;
	P->ShadowBoneIndex = BoneIndex;

	if ( P->ShadowMap )
		grMaterialSpec_CreateRef((grMaterialSpec *)P->ShadowMap);
}
