/****************************************************************************************/
/*  ACTOR.C                                                                             */
/*                                                                                      */
/*  Authors: Mike Sandige	                                                            */
/*				  Aaron Oneal (Incarnadine) - aoneal@ij.net                             */
/*  Description:  Actor implementation                                                  */
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
/*

	TODO:
	  make cued motions keyed to a 'root' bone.  Register the root bone, and then 
	  all requests are relative to that bone, rather than the current 'anchor' point.
	  actually, this doesn't really change much, just _AnimationCue() - and it allows
	  a more efficient _TestStep()

	  convert BoundingBoxMinCorner, BoundingBoxMaxCorner to use extbox.
	
*/

#include <assert.h>
#include <string.h>  // _stricmp()
#include <malloc.h> // free
#include <math.h>

#include "Actor.h"
#include "Ram.h"
#include "Puppet.h"
#include "Body.h"
#include "Motion.h"
#include "Errorlog.h"
#include "StrBlock.h"
#include "Log.h"

#include "Actor._h"

#ifdef WIN32
#pragma warning ( disable : 4115 )
#include <windows.h>
#pragma warning ( default : 4115 )
#endif

#ifdef BUILD_BE
#define _stricmp strcasecmp
#endif

#define ACTOR_MOTIONS_MAX 0x0FFFF		// really arbitrary. just for sanity checking
#define ACTOR_CUES_MAX    0x0FFFF		// arbitrary. 

/*
typedef struct ActorObj {
		char *ActorDefName, *MotionName;
		grBoolean CollisionExtBoxDisplay;
		grExtBox		CollisionExtBox;		
		grBoolean RenderExtBoxDisplay;		
		grExtBox			RenderHintExtBox;
		char				**MotionList;
		int				MotionListSize;
		grMotion		*Motion;
		float MotionTime;
		float ScaleX,ScaleY,ScaleZ;
		float			MotionTimeScale;
		grWorld *World;
		grEngine *Engine;
		grResourceMgr *ResourceMgr;
} ActorObj;*/

	// these are useful globals to monitor resources
int grActor_Count       = 0;
int grActor_RefCount    = 0;
int grActor_DefCount    = 0;
int grActor_DefRefCount = 0;

//	[MacroArt::Begin]
//	Thanks Dee(cryscan@home.net)	
GRAPI float GRCC grActor_GetAlpha(const grActor *A)
{
	assert( A != NULL ) ;
	assert( A->Puppet != NULL ) ;
	return grPuppet_GetAlpha(A->Puppet);
}

GRAPI void GRCC grActor_SetAlpha(grActor *A, float Alpha)
{
	assert( A != NULL ) ;
	assert( A->Puppet != NULL ) ;
	grPuppet_SetAlpha( A->Puppet, Alpha ) ;
}
//	[MacroArt::End]

	// returns number of actors that are currently created.
GRAPI int32 GRCC grActor_GetCount(void)
{
	return grActor_Count;
}

GRAPI grBoolean GRCC grActor_IsValid(const grActor *A)
{
	if (A==NULL)
		return GR_FALSE;

	if(A->ActorDefinition)
	{
		if (A->Pose == NULL)
			return GR_FALSE;
		if (A->CueMotion == NULL)
			return GR_FALSE;
		if (grActor_DefIsValid(A->ActorDefinition)==GR_FALSE)
			return GR_FALSE;
		if (grBody_IsValid(A->ActorDefinition->Body) == GR_FALSE )
			return GR_FALSE;
	}
	return GR_TRUE;
}
	
GRAPI grBoolean GRCC grActor_DefIsValid(const grActor_Def *A)
{
	if (A==NULL)
		return GR_FALSE;
	if (A->ValidityCheck != A)
		return GR_FALSE;	

	return GR_TRUE;
}

static grBoolean GRCF grActor_GetBoneIndex(const grActor *A, const char *BoneName, int *BoneIndex)
{
	grXForm3d Dummy;
	int ParentBoneIndex;
	assert( grActor_IsValid(A) != GR_FALSE);	
	assert( grActor_DefIsValid(A->ActorDefinition) != GR_FALSE );
	assert( grBody_IsValid(A->ActorDefinition->Body) != GR_FALSE );
	assert( BoneIndex != NULL );	

	if ( BoneName != NULL )
	{
		if (grBody_GetBoneByName(A->ActorDefinition->Body,
								 BoneName,
								 BoneIndex,
								 &Dummy,
								 &ParentBoneIndex) ==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneIndex: Named bone not found:", BoneName);
			return GR_FALSE;			
		}
	}
	else
	{
		*BoneIndex = GR_POSE_ROOT_JOINT;
	}
	return GR_TRUE;
}


GRAPI grActor_Def *GRCC grActor_GetActorDef(const grActor *A)
{
	assert( grActor_DefIsValid(A->ActorDefinition) != GR_FALSE );
	return A->ActorDefinition;
}

GRAPI void GRCC grActor_DefCreateRef(grActor_Def *A)
{
	assert( grActor_DefIsValid(A) != GR_FALSE );
	A->RefCount++;
	grActor_DefRefCount++;
}

GRAPI grActor_Def *GRCC grActor_DefCreate(void)
{
	grActor_Def *Ad;

	Ad = GR_RAM_ALLOCATE_STRUCT_CLEAR( grActor_Def );
	if ( Ad == NULL )
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE,"grActor_DefCreateRef: Failed to allocate Actor_Def");
			return NULL;
		}

	Ad->Body				= NULL;
	Ad->MotionCount			= 0;
	Ad->MotionArray			= NULL;
	Ad->ValidityCheck		= Ad;
	Ad->RefCount            = 0;
	grActor_DefCount++;
	return Ad;
}

GRAPI void GRCC grActor_CreateRef(grActor *Actor)
{
	assert( grActor_IsValid(Actor) );	
	Actor->RefCount ++;
	grActor_RefCount++;
}

GRAPI grActor * GRCC grActor_CreateFromDef(grActor_Def *ActorDefinition)
{
	grActor *A;	

	A = GR_RAM_ALLOCATE_STRUCT_CLEAR( grActor );
	if ( A == NULL )
		{
			grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grActor_Create: Failed to allocate grActor struct");
			return NULL;
		}

	A->Puppet = NULL;
	A->Pose   = NULL;
	A->CueMotion = NULL;
	A->ActorDefinition = NULL;	
	A->RefCount          = 0;
	A->CanFree = GR_TRUE;
	grExtBox_Set(&(A->CollisionExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grExtBox_Set(&(A->RenderHintExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grXForm3d_SetIdentity(&A->Xf);
	grActor_Count++;

	if(ActorDefinition != NULL)
		grActor_SetActorDef(A,ActorDefinition);

	return A;
}

GRAPI grActor *GRCC grActor_Create()
{
	grActor *A;	

	A = GR_RAM_ALLOCATE_STRUCT_CLEAR( grActor );
	if ( A == NULL )
	{
		grErrorLog_Add( GR_ERR_MEMORY_RESOURCE , "grActor_Create: Failed to allocate grActor struct");
		return NULL;
	}

	A->Puppet = NULL;
	A->Pose   = NULL;
	A->CueMotion = NULL;
	A->ActorDefinition = NULL;	
	A->RefCount          = 0;
	A->CanFree = GR_TRUE;
	A->RenderNextTime = GR_TRUE;
	InitializeCriticalSection(&A->RenderLock);
	grExtBox_Set(&(A->CollisionExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grExtBox_Set(&(A->RenderHintExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grXForm3d_SetIdentity(&A->Xf);
	grActor_Count++;

	return A;
}

GRAPI grBoolean GRCC grActor_DefDestroy(grActor_Def **pActorDefinition)
{
	int i;
	grActor_Def *Ad;
	assert(  pActorDefinition != NULL );
	assert( *pActorDefinition != NULL );
	assert( grActor_DefIsValid( *pActorDefinition ) != GR_FALSE );

	Ad = *pActorDefinition;

	if (Ad->RefCount > 0)
	{
		Ad->RefCount--;
		grActor_DefRefCount--;
		return GR_FALSE;
	}

	if (Ad->Body != NULL)
	{
		grBody_Destroy( &(Ad->Body) );
		Ad->Body = NULL;
	}
	if (Ad->MotionArray != NULL)
	{
		for (i=0; i<Ad->MotionCount; i++)
		{
			if (Ad->MotionArray[i]!=NULL)
			{
				grMotion_Destroy( &(Ad->MotionArray[i]) );
			}
			Ad->MotionArray[i] = NULL;
		}
		grRam_Free( Ad->MotionArray );
		Ad->MotionArray = NULL;
	}
				
	Ad->MotionCount = 0;

	grRam_Free(*pActorDefinition);
	*pActorDefinition = NULL;
	grActor_DefCount--;
	return GR_TRUE;
}


GRAPI grBoolean GRCC grActor_Destroy(grActor **pA)
{
	grActor *A;
	grChain_Link *Link;

	assert(  pA != NULL );
	assert( *pA != NULL );
	assert( grActor_IsValid(*pA) != GR_FALSE );	
	
	A = *pA;
	if (A->RefCount > 0)
	{
		A->RefCount --;
		grActor_RefCount--;
		return GR_FALSE;
	}

	if (A->Puppet != NULL)
	{
		grPuppet_Destroy( &(A->Puppet) );
		A->Puppet = NULL;
	}
	if ( A->Pose != NULL )
	{
		grPose_Destroy( &( A->Pose ) );
		A->Pose = NULL;
	}
	if ( A->CueMotion != NULL )
	{
		grMotion_Destroy(&(A->CueMotion));
		A->CueMotion = NULL;
	}

	// destroy bone collision list -- Incarnadine
	if(A->BoneCollisionChain != NULL)
	{
		for (Link = grChain_GetFirstLink(A->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
		{
			// Icestorm: Added BoneExtBoxes
			grCollisionBone *Bone;
							
			Bone = (grCollisionBone*)grChain_LinkGetLinkData( Link );
			if(Bone)
			{
				free(Bone->BoneName);
				grRam_Free(Bone->CurrExtBox);
				grRam_Free(Bone->PrevExtBox);
				grRam_Free(Bone);
			}
		}

		// destroy object chain
		grChain_Destroy( &( A->BoneCollisionChain ) );
		A->BoneCollisionChain = NULL;
	}
		
	if(A->ActorDefinition != NULL)
	{
		grActor_DefDestroy(&(A->ActorDefinition));
		A->ActorDefinition = NULL;
	}

	if(A->Object != NULL)
	{
		grRam_Free(A->Object);
		A->Object = NULL;
	}

	DeleteCriticalSection(&A->RenderLock);
	
	if(A->CanFree == GR_TRUE)
	{
		grActor_Count--;
		grRam_Free(*pA);	
		*pA = NULL;
	}


	return GR_TRUE;
}

// Incarnadine
// INCNOTE: This function isn't right, it needs to totally kill the actor
// Do a new create basically.  Look into making a separate function
// that calls the old actor create function.
GRAPI void GRCC grActor_SetActorDef(grActor *A, grActor_Def *Def)
{
	ActorObj *pObj;

	assert(A);
	assert(Def);
	assert( grActor_DefIsValid(Def) != GR_FALSE );
	assert( grBody_IsValid(Def->Body)  != GR_FALSE );
		
	if(A->Pose != NULL)
	{
		pObj = A->Object;
		A->Object = NULL;
		A->CanFree = GR_FALSE;
		grActor_Destroy(&A);
		A->CanFree = GR_TRUE;		
		A->Object = pObj;
	}	
	
	A->ActorDefinition = Def;

	grExtBox_Set(&(A->CollisionExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grExtBox_Set(&(A->RenderHintExtBox), 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
	grXForm3d_SetIdentity(&A->Xf);

	A->Pose = grPose_Create();
	if (A->Pose == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE , "grActor_Create: Failed to allocate Pose");
		goto ActorCreateFailure;
	}
	
	A->CueMotion		 = grMotion_Create(GR_TRUE);
	
	A->BlendingType		 = GR_ACTOR_BLEND_HERMITE;
	A->BoundingBoxCenterBoneIndex = GR_POSE_ROOT_JOINT;
	A->RenderHintExtBoxCenterBoneIndex = GR_POSE_ROOT_JOINT;
	A->RenderHintExtBoxEnabled = GR_FALSE;
	A->StepBoneIndex     = GR_POSE_ROOT_JOINT;

	if (A->CueMotion == NULL)
	{
		grErrorLog_Add(GR_ERR_MEMORY_RESOURCE , "grActor_Create: Failed to allocate CueMotion");
		goto ActorCreateFailure;
	}


	A->BoneCollisionChain = grChain_Create(); // Incarnadine
	A->CollisionFlags = COLLIDE_SOLID; // Incarnadine
	A->LastUsedCollisionBone=NULL;	// Icestorm
	A->LastUsedCollisionBoneName=NULL;	// Icestorm
	A->needsRelighting = GR_TRUE;

	assert( grActor_IsValid(A) != GR_FALSE );		
	
	{
		int i; 
		int BoneCount;

		BoneCount = grBody_GetBoneCount(A->ActorDefinition->Body);
		for (i=0; i<BoneCount; i++)
		{
			const char *Name;
			grXForm3d Attachment;
			int ParentBone;
			int Index;
			grBody_GetBone( A->ActorDefinition->Body, i, &Name,&Attachment, &ParentBone );
			if (grPose_AddJoint( A->Pose,
								ParentBone,Name,&Attachment,&Index)==GR_FALSE)
			{
				grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grActor_Create: grPose_AddJoint failed");
				A->ActorDefinition;
				return;
			}
		}
	}

	grActor_DefCreateRef(Def);

	if(A->Object && A->Object->Engine)
		grActor_AttachEngine(A, A->Object->Engine );

	return;

ActorCreateFailure:
	if ( A!= NULL)
	{
		if (A->Pose != NULL)
			grPose_Destroy(&(A->Pose));
		if (A->CueMotion != NULL)
			grMotion_Destroy(&(A->CueMotion));
		grRam_Free( A );
	}
}

GRAPI grBoolean GRCC grActor_SetBody( grActor_Def *ActorDefinition, grBody *BodyGeometry)
{
	assert( grBody_IsValid(BodyGeometry) != GR_FALSE );
	
	if (ActorDefinition->RefCount > 0)
	{	
		grErrorLog_Add(GR_ERR_BAD_PARAMETER,"grActor_SetBody: ActorDef in use, can't modify body");
		return GR_FALSE;
	}

	if (ActorDefinition->Body != NULL)
	{
		grBody_Destroy( &(ActorDefinition->Body) );
	}
	
	ActorDefinition->Body          = BodyGeometry;
	return GR_TRUE;
}


#pragma message ("consider removing this and related parameters to setpose")
GRAPI void GRCC grActor_SetBlendingType( grActor *A, grActor_BlendingType BlendingType )
{
	assert( grActor_IsValid(A) != GR_FALSE );

	assert( (BlendingType == GR_ACTOR_BLEND_LINEAR) || 
			(BlendingType == GR_ACTOR_BLEND_HERMITE) );

	if (BlendingType == GR_ACTOR_BLEND_LINEAR)
		{
			A->BlendingType = (grActor_BlendingType)GR_POSE_BLEND_LINEAR;
		}
	else
		{
			A->BlendingType = (grActor_BlendingType)GR_POSE_BLEND_HERMITE;
		}
}

grVFile *grActor_DefGetFileContext(const grActor_Def *A)
{
	assert( grActor_DefIsValid(A) != GR_FALSE );
	return A->TextureFileContext;
}



GRAPI grBoolean GRCC grActor_AddMotion(grActor_Def *Ad, grMotion *NewMotion, int32 *Index)
{
	grMotion **NewMArray;
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( NewMotion != NULL );
	assert( Index != NULL );

	if (Ad->MotionCount >= ACTOR_MOTIONS_MAX)
		{
			grErrorLog_Add(GR_ERR_LIST_FULL,"grActor_AddMotion: Too many motions");
			return GR_FALSE;
		}
	NewMArray = GR_RAM_REALLOC_ARRAY( Ad->MotionArray, grMotion*, Ad->MotionCount +1 );
	if ( NewMArray == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE,"grActor_AddMotion: Failed to reallocate motion array");
			return GR_FALSE;
		}

	Ad->MotionArray = NewMArray;

	Ad->MotionArray[Ad->MotionCount]= NewMotion;
	Ad->MotionCount++;
	*Index = Ad->MotionCount;
	return GR_TRUE;
};

GRAPI void GRCC grActor_ClearPose(grActor *A, const grXForm3d *Transform)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert ( (Transform==NULL) || (grXForm3d_IsOrthonormal(Transform) != GR_FALSE) );
	grPose_Clear( A->Pose ,Transform);
	A->Xf = *Transform; //Incarnadine
	A->needsRelighting = GR_TRUE;
}

GRAPI void GRCC grActor_SetPose(grActor *A, const grMotion *M, 
								grFloat Time, const grXForm3d *Transform)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( M != NULL );
	assert ( (Transform==NULL) || (grXForm3d_IsOrthonormal(Transform) != GR_FALSE) );

	grPose_SetMotion( A->Pose,M,Time,Transform);
	A->Xf = *Transform; //Incarnadine
	A->needsRelighting = GR_TRUE;
}

GRAPI void GRCC grActor_BlendPose(grActor *A, const grMotion *M, 
								grFloat Time,  
								const grXForm3d *Transform,
								grFloat BlendAmount)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( M != NULL );
	assert ( (Transform==NULL) || (grXForm3d_IsOrthonormal(Transform) != GR_FALSE) );

	grPose_BlendMotion( A->Pose,M,Time,Transform,
						BlendAmount,(grPose_BlendingType)A->BlendingType);
	A->Xf = *Transform; //Incarnadine
	A->needsRelighting = GR_TRUE;
}


GRAPI int32 GRCC grActor_GetMotionCount(const grActor_Def *Ad)
{
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	return Ad->MotionCount;
}
	

GRAPI grMotion *GRCC grActor_GetMotionByIndex(const grActor_Def *Ad, int32 Index )
{
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( Index >= 0 );
	assert( Index < Ad->MotionCount );
	assert( Ad->MotionArray != NULL );

	return Ad->MotionArray[Index];
}

GRAPI grMotion *GRCC grActor_GetMotionByName(const grActor_Def *Ad, const char *Name )
{
	int i;
	const char *TestName;
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( Name != NULL );
	for (i=0; i<Ad->MotionCount; i++)
		{
			TestName = grMotion_GetName(Ad->MotionArray[i]);
			if (TestName != NULL)
				{
					if (_stricmp(TestName,Name)==0) // Case insensitive compare -- Incarnadine
						return Ad->MotionArray[i];
				}

		}
	return NULL;
}

GRAPI const char *GRCC grActor_GetMotionName(const grActor_Def *Ad, int32 Index )
{
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( Index >= 0 );
	assert( Index < Ad->MotionCount );
	assert( Ad->MotionArray != NULL );
	return grMotion_GetName(Ad->MotionArray[Index]);
}
	
GRAPI grBody *GRCC grActor_GetBody(const grActor_Def *Ad)
{
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	return Ad->Body;
}
	
#pragma message ("consider removing the function: grActor_DefHasBoneNamed")
// Returns GR_TRUE if the actor definition has a bone named 'Name'
GRAPI grBoolean GRCC grActor_DefHasBoneNamed(const grActor_Def *Ad, const char *Name )
{
	int DummyIndex,DummyParent;
	grXForm3d DummyAttachment;

	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( Name != NULL );
	if (grBody_GetBoneByName(grActor_GetBody(Ad),Name,
			&DummyIndex, &DummyAttachment, &DummyParent ) == GR_FALSE )
		{
			return GR_FALSE;
		}
	return GR_TRUE;
}


#define GR_ACTOR_BODY_NAME       "Body"
#define GR_ACTOR_HEADER_NAME     "Header"
#define GR_MOTION_DIRECTORY_NAME "Motions"

#define ACTOR_FILE_TYPE 0x52544341      // 'ACTR'
#define ACTOR_FILE_VERSION 0x00F2		// Restrict version to 16 bits



static grActor_Def * GRCF grActor_DefCreateHeader(grVFile *pFile, grBoolean *HasBody)
{
	uint32 u;
	uint32 version;
	grActor_Def *Ad;

	assert( pFile != NULL );
	assert( HasBody != NULL );

	if( ! grVFile_Read(pFile, &u, sizeof(u)) )
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateHeader - Failed to read header tag");	grActor_DefDestroy(&Ad); return NULL;}

	if (u != ACTOR_FILE_TYPE)
		{	grErrorLog_Add( GR_ERR_FILEIO_FORMAT , "grActor_DefCreateHeader - Failed to recognize format");	return NULL;}

	if(grVFile_Read(pFile, &version, sizeof(version)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateHeader - Read Failed");	return NULL;}
	if ( (version != ACTOR_FILE_VERSION) )
		{	grErrorLog_Add( GR_ERR_FILEIO_VERSION , "grActor_DefCreateHeader - Bad version");	return NULL;}

	Ad = grActor_DefCreate();
	if (Ad==NULL)
		{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE, "grActor_DefCreateHeader - Failed to create Def struct"); return NULL; }

	if(grVFile_Read(pFile, HasBody, sizeof(*HasBody)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateHeader - Read Failed");	grActor_DefDestroy(&Ad); return NULL;}

	if(grVFile_Read(pFile, &(Ad->MotionCount), sizeof(Ad->MotionCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateHeader - Read failed");	grActor_DefDestroy(&Ad); return NULL;}

	return Ad;
}


static grBoolean GRCF grActor_DefWriteHeader(const grActor_Def *Ad, grVFile *pFile)
{
	uint32 u;
	grBoolean Flag;
	
	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( pFile != NULL );

	// Write the format flag
	u = ACTOR_FILE_TYPE;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteHeader - Write failed");	return GR_FALSE; }


	u = ACTOR_FILE_VERSION;
	if(grVFile_Write(pFile, &u, sizeof(u)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteHeader - Write failed");	return GR_FALSE;}

	if (Ad->Body != NULL)
		Flag = GR_TRUE;
	else 
		Flag = GR_FALSE;

	if(grVFile_Write(pFile, &Flag, sizeof(Flag)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteHeader - Write failed");	return GR_FALSE;}

	if(grVFile_Write(pFile, &(Ad->MotionCount), sizeof(Ad->MotionCount)) == GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteHeader - Write failed");	return GR_FALSE;}

	#ifdef COUNT_HEADER_SIZES
		Header_Sizes += 16;
	#endif

	return GR_TRUE;
}
	


GRAPI grActor_Def *GRCC grActor_DefCreateFromFile(grVFile *pFile)
{
	int i;
	grActor_Def *Ad   = NULL;
	grVFile *VFile    = NULL;
	grVFile *SubFile  = NULL;
	grVFile *MotionDirectory = NULL;	
	grBoolean HasBody = GR_FALSE;
	grBody * Body     = NULL;
			
	assert( pFile != NULL );

	VFile = grVFile_OpenNewSystem(pFile,GR_VFILE_TYPE_VIRTUAL, NULL, 
									NULL, GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_READONLY);
	if (VFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefCreateFromFile - Failed to open actor vfile system");	goto CreateError;}
	

	SubFile = grVFile_Open(VFile,GR_ACTOR_HEADER_NAME,GR_VFILE_OPEN_READONLY);
	if (SubFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefCreateFromFile - Failed to open header subfile");	goto CreateError;}

#if 1
	{
		grVFile * HFile;
		HFile = grVFile_GetHintsFile(SubFile);
		if ( ! HFile ) // <> backwards compatibility
			HFile = SubFile;
		Ad = grActor_DefCreateHeader(HFile, &HasBody);
	}
#else
	Ad = grActor_DefCreateHeader( SubFile, &HasBody);
#endif

	if (Ad == NULL)
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grActor_DefCreateFromFile -");
		goto CreateError;
	}
	if (!grVFile_Close(SubFile))
	{
		grErrorLog_Add( GR_ERR_FILEIO_CLOSE ,"grActor_DefCreateFromFile - Failed to close header subfile");
		goto CreateError;
	}
		

	Ad->TextureFileContext = VFile;
	assert(Ad->TextureFileContext);

	if (HasBody != GR_FALSE)
	{
		SubFile = grVFile_Open(VFile,GR_ACTOR_BODY_NAME,GR_VFILE_OPEN_READONLY);
		if (SubFile == NULL)
			{	grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateFromFile - Read failed");	goto CreateError;}

		Body = grBody_CreateFromFile(SubFile);
		if (Body == NULL)
		{
			grErrorLog_Add( GR_ERR_FILEIO_READ , "grActor_DefCreateFromFile - Read failed");
			goto CreateError;
		}
		if (grActor_SetBody(Ad,Body)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grActor_DefCreateFromFile -");
			goto CreateError;
		}
		grVFile_Close(SubFile);
	}

	MotionDirectory = grVFile_Open(VFile,GR_MOTION_DIRECTORY_NAME, 
									GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_READONLY);
	if (MotionDirectory == NULL)
		{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE,"grActor_DefCreateFromFile -");	return NULL;}

	if (Ad->MotionCount>0)
		{
			Ad->MotionArray = GR_RAM_ALLOCATE_ARRAY( grMotion*, Ad->MotionCount);
			if (Ad->MotionArray == NULL)
				{	grErrorLog_Add( GR_ERR_MEMORY_RESOURCE,"grActor_DefCreateFromFile - Failed to allocate motion array");	return NULL; }
			for (i=0; i<Ad->MotionCount; i++)
				Ad->MotionArray[i] = NULL;
	
			for (i=0; i<Ad->MotionCount; i++)
				{
					char FName[1000];
					sprintf(FName,"%d",i);

					SubFile = grVFile_Open(MotionDirectory,FName,GR_VFILE_OPEN_READONLY);
					if (SubFile == NULL)
						{	grErrorLog_Add( GR_ERR_FILEIO_OPEN ,"grActor_DefCreateFromFile - Failed to open motion subdirectory");	goto CreateError;}

					#if 0	
						Ad->MotionArray[i] = grMotion_CreateFromFile(SubFile);
					#else
						{
							grVFile * LZFS;

							LZFS = grVFile_OpenNewSystem(SubFile,GR_VFILE_TYPE_LZ, NULL, NULL,GR_VFILE_OPEN_READONLY);
			
							if ( !LZFS )
								{	grErrorLog_Add( GR_ERR_FILEIO_OPEN ,"grActor_DefCreateFromFile - Failed to open compressed vfile");	goto CreateError;}
					
							Ad->MotionArray[i] = grMotion_CreateFromFile(LZFS);

							Log_Printf("Actor : Motions : ");
							if ( ! grVFile_Close(LZFS) )
								{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE ,"grActor_DefCreateFromFile - Failed to close compressed vfile");	goto CreateError;}

						}
					#endif

					if (Ad->MotionArray[i] == NULL)
						{	grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE,"grActor_DefCreateFromFile - Failed to read motion #",grErrorLog_IntToString(i));	goto CreateError;}

					if (!grVFile_Close(SubFile) )
						{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE ,"grActor_DefCreateFromFile - Failed to close sub motion file");	goto CreateError;}

				}
		}
	else
		{
			Ad->MotionArray = NULL;
		}
	if (!grVFile_Close(MotionDirectory))
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE ,"grActor_DefCreateFromFile - Failed to close motion directory");	goto CreateError;}

	if (!grVFile_Close(VFile))
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE ,"grActor_DefCreateFromFile - Failed to close actor vfile system");	goto CreateError;}

	return Ad;

	CreateError:
		if (SubFile != NULL)
			grVFile_Close(SubFile);
		if (MotionDirectory != NULL)
			grVFile_Close(MotionDirectory);
		if (VFile != NULL)
			grVFile_Close(VFile);
		if (Ad != NULL)
			grActor_DefDestroy(&Ad);
		return NULL;
}


GRAPI grBoolean GRCC grActor_DefWriteToFile(const grActor_Def *Ad, grVFile *pFile)
{
	int i;
	grVFile *VFile;
	grVFile *SubFile;
	grVFile *MotionDirectory;

	assert( grActor_DefIsValid(Ad) != GR_FALSE );
	assert( pFile != NULL );

	VFile = grVFile_OpenNewSystem(pFile,GR_VFILE_TYPE_VIRTUAL, NULL, 
									NULL, GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_CREATE);
	if (VFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefWriteToFile - Failed to open actor vfile system");	goto WriteError;}
	
	SubFile = grVFile_Open(VFile,GR_ACTOR_HEADER_NAME,GR_VFILE_OPEN_CREATE);
	if (SubFile == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefWriteToFile - Failed to open actor header subfile");	goto WriteError;}

#if 1
	{
	grVFile * HFile;
	HFile = grVFile_GetHintsFile(SubFile);
	if (grActor_DefWriteHeader(Ad,HFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to write hints");	goto WriteError;}
	}
#else
	if (grActor_DefWriteHeader(Ad,SubFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to write header");	goto WriteError;}
#endif

	if (grVFile_Close(SubFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grActor_DefWriteToFile - Failed to close header");	goto WriteError;}

	if (Ad->Body != NULL)
		{
			SubFile = grVFile_Open(VFile,GR_ACTOR_BODY_NAME,GR_VFILE_OPEN_CREATE);
			if (SubFile == NULL)
				{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to open body subfile");	goto WriteError;}

			if (grBody_WriteToFile(Ad->Body,SubFile)==GR_FALSE)
				{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_DefWriteToFile - Failed to write body");	goto WriteError;}

			if (grVFile_Close(SubFile)==GR_FALSE)
				{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grActor_DefWriteToFile - Failed to close body subfile");	goto WriteError;}
		}

	MotionDirectory = grVFile_Open(VFile,GR_MOTION_DIRECTORY_NAME, 
									GR_VFILE_OPEN_DIRECTORY | GR_VFILE_OPEN_CREATE);
	if (MotionDirectory == NULL)
		{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefWriteToFile - Failed to open motion subdirectory");	goto WriteError;}
	
	// <> CB note : could save some by combining these motions in one LZ file

	for (i=0; i<Ad->MotionCount; i++)
	{
		char FName[1000];
		sprintf(FName,"%d",i);

		SubFile = grVFile_Open(MotionDirectory,FName,GR_VFILE_OPEN_CREATE);
		if (SubFile == NULL)
			{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefWriteToFile - Failed to open motion subfile");	goto WriteError;}

		#if 0 //{
				if (grMotion_WriteToFile(Ad->MotionArray[i],SubFile)==GR_FALSE)
					{	grErrorLog_AddString( GR_ERR_SUBSYSTEM_FAILURE , "grActor_DefWriteToFile - Failed to write motion");	goto WriteError;}
		#else //}{
			{
				grVFile * LZFS;

				LZFS = grVFile_OpenNewSystem(SubFile,GR_VFILE_TYPE_LZ, NULL, NULL, GR_VFILE_OPEN_CREATE);
				if ( ! LZFS )
					{	grErrorLog_Add( GR_ERR_FILEIO_OPEN , "grActor_DefWriteToFile - Failed to open compressed system");	goto WriteError;}

				if (grMotion_WriteToFile(Ad->MotionArray[i],LZFS)==GR_FALSE)
					{	grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_DefWriteToFile - Failed to write motion");	goto WriteError;}

				if ( ! grVFile_Close(LZFS) )
					{	grErrorLog_Add( GR_ERR_FILEIO_CLOSE , "grActor_DefWriteToFile - Failed to close compressed system");	goto WriteError;}

			}
		#endif //}

		if (grVFile_Close(SubFile)==GR_FALSE)
			{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to close motion subfile");	goto WriteError;}
	}

	if (grVFile_Close(MotionDirectory)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to close motion subdirectory");	goto WriteError;}
	if (grVFile_Close(VFile)==GR_FALSE)
		{	grErrorLog_Add( GR_ERR_FILEIO_WRITE , "grActor_DefWriteToFile - Failed to close actor subsystem");	goto WriteError;}
	
	return GR_TRUE;
	WriteError:
		return GR_FALSE;
}

GRAPI grBoolean GRCC grActor_GetBoneTransform(const grActor *A, const char *BoneName, grXForm3d *Transform)
{
	int BoneIndex;

	assert( grActor_IsValid(A)!=GR_FALSE );
	assert( Transform!= NULL );
	
	if (grActor_GetBoneIndex(A,BoneName,&BoneIndex)==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneTransform: Named bone not found", BoneName);
			return GR_FALSE;
		}

	grPose_GetJointTransform(   A->Pose, BoneIndex,	Transform);
	assert ( grXForm3d_IsOrthonormal(Transform) != GR_FALSE );

	return GR_TRUE;
}


static void GRCF grActor_AccumulateMinMax(
	grVec3d *P,grVec3d *Mins,grVec3d *Maxs)
{
	assert( grVec3d_IsValid( P  ) != GR_FALSE );
	assert( grVec3d_IsValid(Mins) != GR_FALSE );
	assert( grVec3d_IsValid(Maxs) != GR_FALSE );
	
	if (P->X < Mins->X) Mins->X = P->X;
	if (P->Y < Mins->Y) Mins->Y = P->Y;
	if (P->Z < Mins->Z) Mins->Z = P->Z;

	if (P->X > Maxs->X) Maxs->X = P->X;
	if (P->Y > Maxs->Y) Maxs->Y = P->Y;
	if (P->Z > Maxs->Z) Maxs->Z = P->Z;
}



static grBoolean GRCF grActor_GetBoneBoundingBoxByIndex(
	const grActor *A, 
	int BoneIndex,
	grVec3d   *Corner,
	grVec3d   *DX,
	grVec3d   *DY,
	grVec3d   *DZ)
{
	grVec3d Min,Max;
	grVec3d Orientation;
	grXForm3d Transform;
	
	assert( grActor_IsValid(A) != GR_FALSE );	
	assert( grActor_DefIsValid(A->ActorDefinition) != GR_FALSE );
	assert( A->ActorDefinition->Body   != NULL );

	assert( Corner      );
	assert( DX          );
	assert( DY          );
	assert( DZ          );
	assert( (BoneIndex < grPose_GetJointCount(A->Pose)) || (BoneIndex ==GR_POSE_ROOT_JOINT));
	assert( (BoneIndex >=0)                             || (BoneIndex ==GR_POSE_ROOT_JOINT));
	
	if (grBody_GetBoundingBox( A->ActorDefinition->Body, BoneIndex, &Min, &Max )==GR_FALSE)
		{
			// not probably a real error.  It's possible that the bone has no geometry, so it
			// has no bounding box.
			return GR_FALSE;
		}

	// scale bounding box:
	{
		grVec3d Scale;
		grPose_GetScale(A->Pose, &Scale);
		assert( grVec3d_IsValid(&Scale) != GR_FALSE );

		Min.X *= Scale.X;
		Min.Y *= Scale.Y;
		Min.Z *= Scale.Z;
		
		Max.X *= Scale.X;
		Max.Y *= Scale.Y;
		Max.Z *= Scale.Z;
	}


	grPose_GetJointTransform(A->Pose,BoneIndex,&(Transform));

	grVec3d_Subtract(&Max,&Min,&Orientation);
			
	DX->X = Orientation.X;	DX->Y = DX->Z = 0.0f;
	DY->Y = Orientation.Y;	DY->X = DY->Z = 0.0f;
	DZ->Z = Orientation.Z;	DZ->X = DZ->Y = 0.0f;
			
	// transform into world space
	grXForm3d_Transform(&(Transform),&Min,&Min);
	grXForm3d_Rotate(&(Transform),DX,DX);
	grXForm3d_Rotate(&(Transform),DY,DY);
	grXForm3d_Rotate(&(Transform),DZ,DZ);

	*Corner = Min;
	return GR_TRUE;
}



static grBoolean GRCF grActor_GetBoneExtBoxByIndex(
	const grActor *A, 
	int BoneIndex,
	grExtBox *ExtBox)
{
	grVec3d Min;
	grVec3d DX,DY,DZ,Corner;

	assert( ExtBox );
		
	if (grActor_GetBoneBoundingBoxByIndex(A,BoneIndex,&Min,&DX,&DY,&DZ)==GR_FALSE)
		{
			// Commented out by Incarnadine:  This happens frequently when dealing
			// with boned objects if a bone has no geometry (like any physiqued object
		    // with BIP01).  It was slowing things down to make this call several times / tick.
			//grErrorLog_Add(GR_ERR_BAD_PARAMETER,"grActor_GetBoneExtBoxByIndex - ");			
			return GR_FALSE;
		}

	ExtBox->Min = Min;
	ExtBox->Max = Min;
	Corner = Min;
	// should use extent box (extbox) methods rather than this
	grVec3d_Add(&Corner,&DX,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Add(&Corner,&DZ,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Subtract(&Corner,&DX,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Add(&Corner,&DY,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Add(&Corner,&DX,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Subtract(&Corner,&DZ,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));
	grVec3d_Subtract(&Corner,&DX,&Corner);
	grActor_AccumulateMinMax(&Corner,&(ExtBox->Min),&(ExtBox->Max));

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_GetBoneExtBox(const grActor *A,
									 const char *BoneName,
									 grExtBox *ExtBox)
{
	int BoneIndex;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( ExtBox != NULL );
	
	if (grActor_GetBoneIndex(A,BoneName,&BoneIndex)==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneExtBox: Named bone for bounding box not found: ", BoneName);
			return GR_FALSE;
		}
	return grActor_GetBoneExtBoxByIndex(A,BoneIndex,ExtBox);
}


GRAPI grBoolean GRCC grActor_GetBoneBoundingBox(const grActor *A,
								 const char *BoneName,
								 grVec3d *Corner,
								 grVec3d *DX,
								 grVec3d *DY,
								 grVec3d *DZ)
{
	int BoneIndex;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( Corner    != NULL );
	assert( DX        != NULL );
	assert( DY        != NULL );
	assert( DZ        != NULL );

	if (grActor_GetBoneIndex(A,BoneName,&BoneIndex)==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneBoundingBox - Named bone for bounding box not found: ", BoneName);
			return GR_FALSE;
		}
	if (grActor_GetBoneBoundingBoxByIndex(A,BoneIndex,Corner,DX,DY,DZ)==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneBoundingBox - Failed to get bounding box named: ", BoneName);
			//grErrorLog_AppendString(BoneName);
			return GR_FALSE;
		}
	return GR_TRUE;
}



GRAPI grBoolean GRCC grActor_GetExtBox(const grActor *A, grExtBox *ExtBox)
{
	grXForm3d Transform;
	
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( ExtBox != NULL );
	
	grPose_GetJointTransform(   A->Pose,
								A->BoundingBoxCenterBoneIndex,
								&Transform);	
	assert ( grXForm3d_IsOrthonormal(&Transform) != GR_FALSE );
	grVec3d_Add( &(Transform.Translation), &(A->CollisionExtBox.Min), &(ExtBox->Min));
	grVec3d_Add( &(Transform.Translation), &(A->CollisionExtBox.Max), &(ExtBox->Max));
	return GR_TRUE;
}


GRAPI grBoolean GRCC grActor_SetExtBox(grActor *A,
												 const grExtBox *ExtBox,
												 const char *CenterOnThisNamedBone)
{
	
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( grExtBox_IsValid(ExtBox) != GR_FALSE);
	
	A->CollisionExtBox.Min = ExtBox->Min;
	A->CollisionExtBox.Max = ExtBox->Max;
	
	if (grActor_GetBoneIndex(A,CenterOnThisNamedBone,&(A->BoundingBoxCenterBoneIndex))==GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_SetExtBox: Named bone for bounding box not found ", CenterOnThisNamedBone);
		return GR_FALSE;
	}
	
	return GR_TRUE;
}


	// Gets the rendering hint bounding box from the actor
GRAPI grBoolean GRCC grActor_GetRenderHintExtBox(const grActor *A, grExtBox *Box, grBoolean *Enabled)
{
	grXForm3d Transform;

	assert( grActor_IsValid(A) != GR_FALSE);
	assert( Box != NULL );
	assert( Enabled != NULL );

	grPose_GetJointTransform( A->Pose,
								A->RenderHintExtBoxCenterBoneIndex,
								&Transform);	
	assert ( grXForm3d_IsOrthonormal(&Transform) != GR_FALSE );

	*Box = A->RenderHintExtBox;
	grExtBox_Translate ( Box, Transform.Translation.X,
							  Transform.Translation.Y,
							  Transform.Translation.Z );
	
	*Enabled = A->RenderHintExtBoxEnabled;
	return GR_TRUE;
}

	// Sets a rendering hint bounding box from the actor
GRAPI grBoolean GRCC grActor_SetRenderHintExtBox(grActor *A, const grExtBox *Box, 
												const char *CenterOnThisNamedBone)
{
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( Box != NULL );
	assert( Box->Max.X >= Box->Min.X );
	assert( Box->Max.Y >= Box->Min.Y );
	assert( Box->Max.Z >= Box->Min.Z );
	
	if (grActor_GetBoneIndex(A,CenterOnThisNamedBone,&(A->RenderHintExtBoxCenterBoneIndex))==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_SetRenderHintExtBox: Named bone for render hint box not found: ", CenterOnThisNamedBone);
			return GR_FALSE;
		}
		
	A->RenderHintExtBox = *Box;
	if (   (Box->Min.X == 0.0f) && (Box->Max.X == 0.0f)
		&& (Box->Min.Y == 0.0f) && (Box->Max.Y == 0.0f) 
		&& (Box->Min.Z == 0.0f) && (Box->Max.Z == 0.0f) )
		{
			A->RenderHintExtBoxEnabled = GR_FALSE;
		}
	else
		{
			A->RenderHintExtBoxEnabled = GR_TRUE;
		}

	return GR_TRUE;
}


GRAPI void *GRCC grActor_GetUserData(const grActor *A)
{
	assert( grActor_IsValid(A) != GR_FALSE);
	return A->UserData;
}

GRAPI void GRCC grActor_SetUserData(grActor *A, void *UserData)
{
	assert( grActor_IsValid(A) != GR_FALSE);
	A->UserData = UserData;
}

#define MAX(aa,bb)   ( (aa)>(bb)?(aa):(bb) )
#define MIN(aa,bb)   ( (aa)<(bb)?(aa):(bb) )

static void GRCF grActor_StretchBoundingBox( grVec3d *Min, grVec3d *Max,
							const grVec3d *Corner, 
							const grVec3d *DX, const grVec3d *DY, const grVec3d *DZ)
{
	grVec3d P;

	P = *Corner;
	Min->X = MIN(Min->X,P.X),	Min->Y = MIN(Min->Y,P.Y),	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Add     (Corner ,DX,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Add     (&P, DZ,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Subtract(&P,DX,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Add     (&P,DY,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Add     (&P,DX,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Subtract(&P,DZ,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);

	grVec3d_Subtract(&P,DX,&P);
	Min->X = MIN(Min->X,P.X);	Min->Y = MIN(Min->Y,P.Y);	Min->Z = MIN(Min->Z,P.Z);
	Max->X = MAX(Max->X,P.X);	Max->Y = MAX(Max->Y,P.Y);	Max->Z = MAX(Max->Z,P.Z);
}

GRAPI grBoolean GRCC grActor_GetDynamicExtBox( const grActor *A, grExtBox *ExtBox)
{
#define GR_ACTOR_REALLY_BIG_NUMBER (9e9f)

	grVec3d Corner;
	grVec3d DX;
	grVec3d DY;
	grVec3d DZ;
	int Count,i,BCount;

	assert( grActor_IsValid(A) != GR_FALSE);
	assert( A->ActorDefinition->Body   != NULL );

	grVec3d_Set(&(ExtBox->Min),
			GR_ACTOR_REALLY_BIG_NUMBER,GR_ACTOR_REALLY_BIG_NUMBER,GR_ACTOR_REALLY_BIG_NUMBER);
	grVec3d_Set(&(ExtBox->Max),
			-GR_ACTOR_REALLY_BIG_NUMBER,-GR_ACTOR_REALLY_BIG_NUMBER,-GR_ACTOR_REALLY_BIG_NUMBER);
		
	BCount = 0;
	Count = grBody_GetBoneCount( A->ActorDefinition->Body );
	for (i=0; i< Count; i++)
		{
			if (grActor_GetBoneBoundingBoxByIndex(A,i,&Corner,&DX,&DY,&DZ)!=GR_FALSE)
				{
					grActor_StretchBoundingBox( &(ExtBox->Min),
												&(ExtBox->Max),&Corner,&DX,&DY,&DZ);
					BCount ++;
				}
		}
	if (BCount>0)
		{
			return GR_TRUE;
		}
	return GR_FALSE;
}



GRAPI grBoolean GRCC grActor_Attach( grActor *Slave,  const char *SlaveBoneName,
						const grActor *Master, const char *MasterBoneName, 
						const grXForm3d *Attachment)
{
	int SlaveBoneIndex,MasterBoneIndex;

	assert( grActor_IsValid(Slave) != GR_FALSE);
	assert( grActor_IsValid(Master) != GR_FALSE);
	assert( grXForm3d_IsOrthonormal(Attachment) != GR_FALSE );
	
	assert( MasterBoneName != NULL );		// might this be possible?
	
	if (grActor_GetBoneIndex(Slave,SlaveBoneName,&(SlaveBoneIndex))==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_Attach: Named bone for slave not found: ", SlaveBoneName);
			return GR_FALSE;
		}
	
	if (grActor_GetBoneIndex(Master,MasterBoneName,&(MasterBoneIndex))==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"Named bone for master not found: ", MasterBoneName);
			return GR_FALSE;
		}
	
	return grPose_Attach(   Slave->Pose,   SlaveBoneIndex,
							Master->Pose, MasterBoneIndex, 
							Attachment);
}


GRAPI void GRCC grActor_Detach(grActor *A)
{
	assert( grActor_IsValid(A) != GR_FALSE);

	grPose_Detach( A->Pose );
}


GRAPI grBoolean GRCC grActor_GetBoneAttachment(const grActor *A,
								const char *BoneName,
								grXForm3d *Attachment)
{

	int BoneIndex;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( Attachment != NULL );
	
	if (grActor_GetBoneIndex(A,BoneName,&(BoneIndex))==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_GetBoneAttachment: Named bone not found: ", BoneName);
			return GR_FALSE;
		}
	
	grPose_GetJointAttachment(A->Pose,BoneIndex, Attachment);
	assert ( grXForm3d_IsOrthonormal(Attachment) != GR_FALSE );

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_SetBoneAttachment(grActor *A,
								const char *BoneName,
								grXForm3d *Attachment)
{

	int BoneIndex;

	assert( grActor_IsValid(A) != GR_FALSE);
	assert( grXForm3d_IsOrthonormal(Attachment) != GR_FALSE );
	
	if (grActor_GetBoneIndex(A,BoneName,&(BoneIndex))==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_SetBoneAttachment: Named bone not found: ", BoneName);
			return GR_FALSE;
		}
	
	grPose_SetJointAttachment(A->Pose,BoneIndex, Attachment);
	return GR_TRUE;
}


//-------------------------------------------------------------------------------------------------
// Actor Cuing system
//-------------------------------------------------------------------------------------------------
#define ACTOR_CUE_MINIMUM_BLEND (0.0001f)
#define ACTOR_CUE_MAXIMUM_BLEND (0.999f)


static grBoolean GRCF grActor_IsAnimationCueDead(grActor *A, int Index)
{
	grBoolean Kill= GR_FALSE;
	grFloat BlendAmount;
	grMotion *M;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( (Index>=0) && (Index<grMotion_GetSubMotionCount(A->CueMotion)));

	M = A->CueMotion;

	BlendAmount = grMotion_GetBlendAmount(M,Index,0.0f);
	if (BlendAmount <= ACTOR_CUE_MINIMUM_BLEND)
		{
			int KeyCount;
			grPath *P; 
			grFloat KeyTime;

			P = grMotion_GetBlendPath(M,Index);
			assert( P != NULL );
			KeyCount = grPath_GetKeyframeCount(P,GR_PATH_TRANSLATION_CHANNEL);
			if (KeyCount>0)
				{
					grXForm3d Dummy;
					grFloat TimeOffset = -grMotion_GetTimeOffset( M, Index);
					grPath_GetKeyframe( P, KeyCount-1, GR_PATH_TRANSLATION_CHANNEL, &KeyTime, &Dummy );
	
					if ( KeyTime <= TimeOffset )
						{
							Kill = GR_TRUE;
						}
				}
			else
				{
					Kill = GR_TRUE;
				}
		}
	return Kill;
}


static void GRCF grActor_KillCue( grActor *A, int Index )
{
	grMotion *M;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( (Index>=0) && (Index<grMotion_GetSubMotionCount(A->CueMotion)));
	M  = grMotion_RemoveSubMotion(A->CueMotion,Index);
}

GRAPI grBoolean GRCC grActor_AnimationNudge(grActor *A, grXForm3d *Offset)
{
	grMotion *M;
	int i,Count;
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( grXForm3d_IsOrthonormal(Offset) != GR_FALSE );
	M = A->CueMotion;
	Count = grMotion_GetSubMotionCount(M);
	
	for (i=Count-1; i>=0; i--)	
		{
			grXForm3d Transform;
			const grXForm3d *pTransform;
			pTransform = grMotion_GetBaseTransform( M, i );
			if ( pTransform != NULL )
				{
					Transform = *pTransform;
			
					grXForm3d_Multiply(Offset,&Transform,&Transform);
					grXForm3d_Orthonormalize(&Transform);

					grMotion_SetBaseTransform( M, i, &Transform);
				}
		}
	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_AnimationRemoveLastCue( grActor *A )
{
	int Count;
	assert( grActor_IsValid(A) != GR_FALSE);
	Count = grMotion_GetSubMotionCount(A->CueMotion);
	if (Count>0)
		{
			grActor_KillCue( A, Count-1 );
			return GR_TRUE;
		}
	return GR_FALSE;
}

GRAPI grBoolean GRCC grActor_AnimationCue( grActor *A, 
								grMotion *Motion,
								grFloat TimeScaleFactor,
								grFloat TimeIntoMotion,
								grFloat BlendTime, 
								grFloat BlendFromAmount, 
								grFloat BlendToAmount,
								const grXForm3d *MotionTransform)
{
	int Index;

	assert( grActor_IsValid(A) != GR_FALSE);

	assert( (BlendFromAmount>=0.0f) && (BlendFromAmount<=1.0f));
	assert( (  BlendToAmount>=0.0f) && (  BlendToAmount<=1.0f));
	assert( (MotionTransform==NULL) || (grXForm3d_IsOrthonormal(MotionTransform)) != GR_FALSE );

	assert( Motion != NULL );
	
	assert( BlendTime >= 0.0f);
	if (BlendTime==0.0f)
		{
			BlendFromAmount = BlendToAmount;
			BlendTime = 1.0f;	// anything that is > GR_TKA_TIME_TOLERANCE
		}

	if (grMotion_AddSubMotion( A->CueMotion, TimeScaleFactor, -TimeIntoMotion, Motion, 
							TimeIntoMotion, BlendFromAmount, 
							TimeIntoMotion + BlendTime, BlendToAmount, 
							MotionTransform, &Index )==GR_FALSE)
		{	
			return GR_FALSE;
		}
		
	return GR_TRUE;
}


GRAPI grBoolean GRCC grActor_AnimationStep(grActor *A, grFloat DeltaTime )
{
	int i,Coverage,Count;
	grMotion *M;
	grMotion *SubM;
	
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( DeltaTime >= 0.0f );
	
	grPose_ClearCoverage(A->Pose,0);

	M = A->CueMotion;

	Count = grMotion_GetSubMotionCount(M);

	for (i=Count-1; i>=0; i--)	
		{
			grFloat TimeOffset = grMotion_GetTimeOffset( M, i );
			TimeOffset = TimeOffset - DeltaTime;
			grMotion_SetTimeOffset( M, i, TimeOffset);

			if (grActor_IsAnimationCueDead(A,i))
				{
					grActor_KillCue(A,i);
				}
			else
				{
					grBoolean SetWithBlending= GR_TRUE;
					grFloat BlendAmount;
					
					SubM = grMotion_GetSubMotion(M,i);
					assert( SubM != NULL );
					
					BlendAmount = grMotion_GetBlendAmount( M,i,0.0f );
					
					if (BlendAmount >= ACTOR_CUE_MAXIMUM_BLEND)
						{
							SetWithBlending = GR_FALSE;
						}
					Coverage = grPose_AccumulateCoverage(A->Pose,SubM, SetWithBlending);
					if ( Coverage == 0 )
						{
							grActor_KillCue(A,i);
						}
				}
		}

	grPose_SetMotion( A->Pose, M, 0.0f, NULL );
	grMotion_SetupEventIterator(M,-DeltaTime,0.0f);

	return GR_TRUE;
}


GRAPI grBoolean GRCC grActor_AnimationStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName )
{
	int i,Coverage,Count;
	grMotion *M;
	grMotion *SubM;
	
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( DeltaTime >= 0.0f );
	
	if (BoneName == NULL)
		{
			A->StepBoneIndex = GR_POSE_ROOT_JOINT;
		}
	else
		{
			grBoolean LookupBoneName= GR_TRUE;
			const char *LastBoneName;
			grXForm3d Attachment;
			int LastParentBoneIndex;
			if (A->StepBoneIndex >= 0)
				{
					grBody_GetBone(	A->ActorDefinition->Body,A->StepBoneIndex,&LastBoneName,&Attachment,&LastParentBoneIndex);
					if (  (LastBoneName != NULL) )
						if (_stricmp(LastBoneName,BoneName)==0)  // Case insensitive compare -- Incarnadine
							LookupBoneName = GR_FALSE;
				}
			if (LookupBoneName != GR_FALSE)
				{
					if (grActor_GetBoneIndex(A,BoneName,&(A->StepBoneIndex))==GR_FALSE)
						{
							grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_AnimationStepBoneOptimized: Named bone not found: ", BoneName);
							return GR_FALSE;
						}
				}
		}
			

	grPose_ClearCoverage(A->Pose,0);

	M = A->CueMotion;

	Count = grMotion_GetSubMotionCount(M);

	for (i=Count-1; i>=0; i--)	
		{
			grFloat TimeOffset = grMotion_GetTimeOffset( M, i );
			TimeOffset = TimeOffset - DeltaTime;
			grMotion_SetTimeOffset( M, i, TimeOffset);

			if (grActor_IsAnimationCueDead(A,i))
				{
					grActor_KillCue(A,i);
				}
			else
				{
					grBoolean SetWithBlending= GR_TRUE;
					grFloat BlendAmount;
					
					SubM = grMotion_GetSubMotion(M,i);
					assert( SubM != NULL );
					
					BlendAmount = grMotion_GetBlendAmount( M,i,0.0f );
					
					if (BlendAmount >= ACTOR_CUE_MAXIMUM_BLEND)
						{
							SetWithBlending = GR_FALSE;
						}
					Coverage = grPose_AccumulateCoverage(A->Pose,SubM, SetWithBlending);
					if ( Coverage == 0 )
						{
							grActor_KillCue(A,i);
						}
				}
		}

	grPose_SetMotionForABone( A->Pose, M, 0.0f, NULL, A->StepBoneIndex );
	grMotion_SetupEventIterator(M,-DeltaTime,0.0f);

	return GR_TRUE;
}


		
GRAPI grBoolean GRCC grActor_AnimationTestStep(grActor *A, grFloat DeltaTime)
{
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( DeltaTime >= 0.0f );

	grPose_SetMotion( A->Pose, A->CueMotion , DeltaTime, NULL );

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_AnimationTestStepBoneOptimized(grActor *A, grFloat DeltaTime, const char *BoneName)
{
	assert( grActor_IsValid(A) != GR_FALSE);
	assert( DeltaTime >= 0.0f );

	if (BoneName == NULL)
		{
			A->StepBoneIndex = GR_POSE_ROOT_JOINT;
		}
	else
		{
			grBoolean LookupBoneName= GR_TRUE;
			const char *LastBoneName;
			grXForm3d Attachment;
			int LastParentBoneIndex;
			if (A->StepBoneIndex >= 0)
				{
					grBody_GetBone(	A->ActorDefinition->Body,A->StepBoneIndex,&LastBoneName,&Attachment,&LastParentBoneIndex);
					if (  (LastBoneName != NULL) )
						if (_stricmp(LastBoneName,BoneName)==0)  // Case insensitive compare -- Incarnadine
							LookupBoneName = GR_FALSE;
				}
			if (LookupBoneName != GR_FALSE)
				{
					if (grActor_GetBoneIndex(A,BoneName,&(A->StepBoneIndex))==GR_FALSE)
						{
							grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_AnimationTestStepBoneOptimized: Named bone not found:", BoneName);
							return GR_FALSE;
						}
				}
		}
	grPose_SetMotionForABone( A->Pose, A->CueMotion , DeltaTime, NULL,A->StepBoneIndex );

	return GR_TRUE;
}



GRAPI grBoolean GRCC grActor_GetAnimationEvent(
	grActor *A,						
	const char **ppEventString)		// Return data, if found
	// returns the event string for the 'next' event that occured during the last 
	// animation step time delta.
	// if the return value is GR_FALSE, there are no more events, and ppEventString will be Empty
{
	grFloat Time;
	assert( grActor_IsValid(A) != GR_FALSE);

	return grMotion_GetNextEvent(A->CueMotion, &Time, ppEventString );
}

GRAPI grBoolean GRCC grActor_GetLightingOptions(const grActor *Actor,
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
	const char **LightReferenceBoneName,
	grBoolean *PerBoneLighting)
{
	int32 BoneIndex;
	assert( grActor_IsValid(Actor)!=GR_FALSE );

	assert( UseFillLight != NULL );
	assert( FillLightNormal != NULL );
	assert( FillLightRed != NULL );	
	assert( FillLightGreen != NULL );	
	assert( FillLightBlue != NULL );	
	assert( AmbientLightRed != NULL );
	assert( AmbientLightGreen != NULL );			
	assert( AmbientLightBlue != NULL );			
	assert( UseAmbientLightFromFloor != NULL );
	assert( MaximumDynamicLightsToUse != NULL );	
	assert( LightReferenceBoneName != NULL );

	assert( Actor->Puppet );
	
	grPuppet_GetLightingOptions(	Actor->Puppet,
									UseFillLight,
									FillLightNormal,
									FillLightRed,	
									FillLightGreen,	
									FillLightBlue,	
									AmbientLightRed,
									AmbientLightGreen,		
									AmbientLightBlue,		
									UseAmbientLightFromFloor,
									MaximumDynamicLightsToUse,
									MaximumStaticLightsToUse,
									&BoneIndex,
									PerBoneLighting);

	if (BoneIndex>=0 && (BoneIndex < grBody_GetBoneCount(Actor->ActorDefinition->Body)))
		{
			grXForm3d DummyAttachment;
			int DummyParentBoneIndex;
			grBody_GetBone(	Actor->ActorDefinition->Body,
							BoneIndex,
							LightReferenceBoneName,
							&DummyAttachment,
							&DummyParentBoneIndex);
		}
	else
		{
			LightReferenceBoneName = NULL;
		}

	return GR_TRUE; // CB
}

GRAPI grBoolean GRCC grActor_SetLightingOptions(grActor *A,
	grBoolean UseFillLight,
	const grVec3d *FillLightNormal,
	grFloat FillLightRed,				// 0 .. 255
	grFloat FillLightGreen,				// 0 .. 255
	grFloat FillLightBlue,				// 0 .. 255
	grFloat AmbientLightRed,			// 0 .. 255
	grFloat AmbientLightGreen,			// 0 .. 255
	grFloat AmbientLightBlue,			// 0 .. 255
	grBoolean AmbientLightFromFloor,
	int32 MaximumDynamicLightsToUse,		// 0 for none
	int32 MaximumStaticLightsToUse, // 0 for none
	const char *LightReferenceBoneName,
	grBoolean PerBoneLighting	)
{
	int BoneIndex;

	assert( grActor_IsValid(A)!=GR_FALSE );
	assert( FillLightNormal != NULL );
	assert( A->Puppet );

	if (LightReferenceBoneName && strcmp(LightReferenceBoneName, "< none >") == 0)
	{
		grActor_GetBoneIndex(A,NULL,&BoneIndex);
	}
	else
	if (grActor_GetBoneIndex(A,LightReferenceBoneName,&BoneIndex)==GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_SetLightingOptions: Named bone for light reference not found: ", LightReferenceBoneName);
		return GR_FALSE;
	}
	if (!grVec3d_IsNormalized(FillLightNormal)) 
	{
		grVec3d_Normalize((grVec3d*)FillLightNormal);
	}

	grPuppet_SetLightingOptions(	A->Puppet,
									UseFillLight,
									FillLightNormal,
									FillLightRed,	
									FillLightGreen,	
									FillLightBlue,	
									AmbientLightRed,
									AmbientLightGreen,		
									AmbientLightBlue,		
									AmbientLightFromFloor,
									MaximumDynamicLightsToUse,
									MaximumStaticLightsToUse,
									BoneIndex,
									PerBoneLighting);
	return GR_TRUE;
}

GRAPI void GRCC grActor_SetScale(grActor *A, grFloat ScaleX,grFloat ScaleY,grFloat ScaleZ)
{
	grVec3d S;
	assert( A != NULL );
		
	grVec3d_Set(&S,ScaleX,ScaleY,ScaleZ);
	grPose_SetScale(A->Pose,&S);

	A->needsRelighting = GR_TRUE;
}



GRAPI grBoolean GRCC grActor_SetShadow(grActor *A, 
		grBoolean DoShadow, 
		grFloat Radius,
		const grMaterialSpec *ShadowMap,
		const char *BoneName)
{
	int BoneIndex;

	assert( grActor_IsValid(A)!=GR_FALSE );
	assert( (DoShadow==GR_FALSE) || (DoShadow==GR_TRUE));
	assert( Radius >= 0.0f);
	assert( A->Puppet );
	
	if (grActor_GetBoneIndex(A,BoneName,&BoneIndex)==GR_FALSE)
		{
			grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE,"grActor_SetShadow: Named bone for shadow not found: ", BoneName);
			return GR_FALSE;
		}

	grPuppet_SetShadow(A->Puppet,DoShadow,Radius,ShadowMap,BoneIndex);

	return GR_TRUE;
}


GRAPI grBoolean GRCC grActor_AttachEngine(grActor *A, grEngine *pEngine)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( grActor_DefIsValid(A->ActorDefinition) != GR_FALSE );
	assert( grBody_IsValid(A->ActorDefinition->Body) != GR_FALSE );
	assert(pEngine);


	if (A->Puppet!=NULL)
	{
		grEngine* pe = grPuppet_GetEngine(A->Puppet);
		if (pEngine == pe) {
			OutputDebugString("No Attach engine twice\n");
			return GR_TRUE;
		}
		grPuppet_Destroy(&(A->Puppet));
		A->Puppet =NULL;
	}
		
	A->Puppet = grPuppet_Create(A->ActorDefinition->TextureFileContext, A->ActorDefinition->Body, pEngine);

	if ( A->Puppet == NULL )
	{
		grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "grActor_AttachEngine: failed to create puppet");
		return GR_FALSE;
	}
	
	// Get box of puppet
	{
		grExtBox EB;

		if (grActor_GetBoneExtBoxByIndex(A,GR_POSE_ROOT_JOINT,&EB) == GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE,"grActor_AttachEngine: failed to get root extent box");
			return GR_FALSE;			
		}
		
		A->CollisionExtBox.Min = EB.Min;
		A->CollisionExtBox.Max = EB.Max;
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_DetachEngine(grActor *A, grEngine *pEngine)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( grActor_DefIsValid(A->ActorDefinition) != GR_FALSE );
	assert( grBody_IsValid(A->ActorDefinition->Body) != GR_FALSE );

	assert(pEngine);

	assert(A->Puppet);

	grPuppet_Destroy(&A->Puppet);

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_RenderThroughFrustum(
		const grActor	*A, 
		grEngine		*Engine, 
		grWorld			*World, 
		grCamera		*Camera, 
		const grFrustum *Frustum)
{
	grExtBox	Box;
	grBoolean	Enabled;
		
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( A->Puppet != NULL );

//	tom morris feb 2005
//	this switch commented out because it prevents rendering of actors in jDesigner3d
	if (!A->RenderNextTime) {
//		Log_Printf("KROUER: grActor_RenderThroughFrustum: Actor in invisible area");
		return GR_TRUE;
	}
//	end tom morris feb 2005

	if (A->RenderHintExtBoxEnabled)
	{
		if (grActor_GetRenderHintExtBox(A, &Box, &Enabled)==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grActor_RenderThroughFrustum: Failed to get render hint box");
			return GR_FALSE;
		}
	}
	else
	{
		if (!grActor_GetDynamicExtBox( A, &Box))
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grActor_RenderThroughFrustum: Failed to get dynamic ext box.");
			return GR_FALSE;
		}
	}

	if (A->needsRelighting)
	{
		if (grPuppet_RenderThroughFrustum( A->Puppet, A->Pose, &Box, Engine, World, Camera, Frustum, GR_TRUE)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_RenderThroughFrustum: Failed to render puppet");
			return GR_FALSE;
		}

		((grActor *)A)->needsRelighting = GR_FALSE;
	}
	else
	{
		if (grPuppet_RenderThroughFrustum( A->Puppet, A->Pose, &Box, Engine, World, Camera, Frustum, GR_FALSE)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_RenderThroughFrustum: Failed to render puppet");
			return GR_FALSE;
		}
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_Render(
		const grActor	*A, 
		grEngine		*Engine, 
		grWorld			*World, 
		const grCamera	*Camera)
{
	grExtBox Box;
	grExtBox *pBox = &Box;
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( A->Puppet != NULL );


//	tom morris feb 2005
//	this switch commented out because it prevents rendering of actors in jDesigner3d
	if (!A->RenderNextTime) {
//		//Log_Printf("KROUER: grActor_Render: Actor in invisible area\n");
		return GR_TRUE;
	}
//	end tom morris feb 2005

	if (A->RenderHintExtBoxEnabled)
	{
		grBoolean Enabled;
		if (grActor_GetRenderHintExtBox(A, pBox, &Enabled)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_Render: failed to get render hint box");	
			return GR_FALSE;
		}
	}
	else
		pBox = NULL;


	if (A->needsRelighting)
	{	
		if (grPuppet_Render( A->Puppet, A->Pose, Engine, World, Camera, pBox, GR_TRUE)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_Render: failed to render puppet");
			return GR_FALSE;
		}

		((grActor *)A)->needsRelighting = GR_FALSE;
	}

	else
	{
		if (grPuppet_Render( A->Puppet, A->Pose, Engine, World, Camera, pBox, GR_FALSE)==GR_FALSE)
		{
			grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE , "grActor_Render: failed to render puppet");
			return GR_FALSE;
		}
	}

	return GR_TRUE;
}

// force the actor to be re-lit with static lighting
GRAPI void GRCC grActor_ForceStaticRelighting(grActor* pActor)
{
	assert(grActor_IsValid(pActor) != GR_FALSE);

	pActor->needsRelighting = GR_TRUE;
}


GRAPI int32 GRCC grActor_GetMaterialCount(const grActor *A)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( A->Puppet != NULL );

	return grPuppet_GetMaterialCount( A->Puppet );
}

GRAPI grBoolean GRCC grActor_GetMaterial(const grActor *A, int32 MaterialIndex,
										grMaterialSpec **Bitmap, grFloat *Red, grFloat *Green, grFloat *Blue, grUVMapper* pMapper)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( A->Puppet != NULL );

	return grPuppet_GetMaterial(A->Puppet, MaterialIndex, Bitmap, Red, Green, Blue, pMapper);
}


GRAPI grBoolean GRCC grActor_SetMaterial(grActor *A, int32 MaterialIndex,
										grMaterialSpec *Bitmap,  grFloat Red,  grFloat Green,  grFloat Blue, grUVMapper Mapper)
{
	assert( grActor_IsValid(A) != GR_FALSE );
	assert( A->Puppet != NULL );

	return grPuppet_SetMaterial(A->Puppet,MaterialIndex, Bitmap, Red, Green, Blue, Mapper);
}

///////////////////////////////////////////////////////////////////////////////////////
// exposed geometry APIs

GRAPI grBoolean GRCC grActor_GetIndexedBoneWorldSpaceVertexLocations(const grActor* pActor, int32 boneIndex, int32 aSize,
	grVec3d* pVerts)
{
	int i;
	grXForm3d xform;

	if (! grBody_GetIndexedBoneVertexLocations(pActor->ActorDefinition->Body, boneIndex, aSize, pVerts))
		return GR_FALSE;

	grPose_GetJointTransform(pActor->Pose, boneIndex, &xform);

	for (i = 0; i < aSize; i ++)
	{
		grXForm3d_Transform(&xform, &pVerts[i], &pVerts[i]);
	}

	return GR_TRUE;
}

GRAPI grBoolean GRCC grActor_GetNamedBoneWorldSpaceVertexLocations(const grActor* pActor, const char* pBoneName, int32 aSize,
	grVec3d* pVerts)
{
	int i;
	grXForm3d xform;

	if (! grBody_GetNamedBoneVertexLocations(pActor->ActorDefinition->Body, pBoneName, aSize, pVerts))
		return GR_FALSE;

	grActor_GetBoneTransform(pActor, pBoneName, &xform);

	for (i = 0; i < aSize; i ++)
	{
		grXForm3d_Transform(&xform, &pVerts[i], &pVerts[i]);
	}

	return GR_TRUE;
}

//========================================================================================
// --- Incarnadine: Begin Actor Collision ---

//========================================================================================
//	grActor_Collision
//      Tests to see if a Box moving along a path from Front to Back collides with the Actor.
//  The Actor's bounding box must have previously been set and Box must be relative to
//  the path (meaning not in world-space coordinates).
//
//  Bonelevel collision is set is specified, a bone level
//  collision is done based on bones that have been added with AddCollisionBone().
//  CollidedBone gets set if there is a collision.
//========================================================================================
GRAPI grBoolean GRCC grActor_Collision( grActor	*Actor, const grWorld* World, const grExtBox	*Box, 
		const grVec3d *Front, const grVec3d *Back, grCollisionInfo *CollisionInfo)
{
	grExtBox ActorBox, BoneBox, xSweepBox;
	grFloat T, BestT;
	grVec3d Normal, BestNormal, Correction;
	char *BoneName = NULL, *BestBoneName = NULL;	

	assert(Actor != NULL);
	assert(Front != NULL);
	assert(Back != NULL);
		
	// Get actor box and make sure it's valid
	grActor_GetExtBox(Actor,&ActorBox);
	if(!grExtBox_IsValid(&ActorBox)) return GR_FALSE;

	if(grWorld_GetCollisionLevel(World) != COLLIDE_BONES  || grActor_GetNextCollisionBone(Actor, NULL) == NULL)
	{
		// Not bone level collision
		if (CollisionInfo)
		{
			if(grExtBox_Collision(&ActorBox, Box, Front, Back, &T, &Normal))
			{
				// Get path vector
				grVec3d_Subtract(Back, Front, &CollisionInfo->Impact);

				// Impact is too precise, back off a little
				Correction = CollisionInfo->Impact;
				grVec3d_Normalize(&Correction);
				grVec3d_Scale(&Correction,-1.5f,&Correction);

				// Scale the path vector based on impact
				grVec3d_Scale(&CollisionInfo->Impact, T, &CollisionInfo->Impact);

				// Calculate the plane info from this
				CollisionInfo->Plane.Dist = grVec3d_Length(&CollisionInfo->Impact);
				CollisionInfo->Plane.Normal = Normal;
				CollisionInfo->Plane.Type = Type_Any;

				// Get the true impact point by adding the start position and correction
				grVec3d_Add(Front, &CollisionInfo->Impact, &CollisionInfo->Impact);	
				grVec3d_Add(&Correction, &CollisionInfo->Impact, &CollisionInfo->Impact);	

				CollisionInfo->IsValid = GR_TRUE;
				grActor_SetCollidedBone(Actor,NULL);
		
				return GR_TRUE;
			}
		} else
			return grExtBox_Collision(&ActorBox, Box, Front, Back, NULL, NULL);
	}
	else
	{
		// Verify the box sweep intersects the actor box		
		grExtBox_LinearSweep(Box, Front, Back, &xSweepBox);
		if(!grExtBox_Intersection(&ActorBox, &xSweepBox, NULL)) return GR_FALSE;

		// Bone level collision
		grActor_RecalcCollisionBones(Actor);
		BoneName = NULL;				
		while ( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, NULL, &BoneBox)) != NULL )
		{
			assert(grExtBox_IsValid( &BoneBox) && !grExtBox_IsPoint(&BoneBox));
			if(CollisionInfo)
			{
				if(grExtBox_Collision(&BoneBox, Box, Front, Back, &T, &Normal))
				{
					if(BestBoneName == NULL || (BestBoneName != NULL && T < BestT))
					{
						BestBoneName = BoneName;
						BestT = T;
						BestNormal = Normal;
					}
				}
			} else
				if(grExtBox_Collision(&BoneBox, Box, Front, Back, NULL, NULL))
					return GR_TRUE;
		}		

		if(BestBoneName != NULL)
		{
			T = BestT;
			Normal = BestNormal;

			// Get path vector
			grVec3d_Subtract(Back, Front, &CollisionInfo->Impact);

			// Impact is too precise, back off a little
			Correction = CollisionInfo->Impact;
			grVec3d_Normalize(&Correction);
			grVec3d_Scale(&Correction,-1.5f,&Correction);

			// Scale the path vector based on impact
			grVec3d_Scale(&CollisionInfo->Impact, T, &CollisionInfo->Impact);

			// Calculate the plane info from this
			CollisionInfo->Plane.Dist = grVec3d_Length(&CollisionInfo->Impact);
			CollisionInfo->Plane.Normal = Normal;
			CollisionInfo->Plane.Type = Type_Any;

			// Get the true impact point by adding the start position and correction
			grVec3d_Add(Front, &CollisionInfo->Impact, &CollisionInfo->Impact);	
			grVec3d_Add(&Correction, &CollisionInfo->Impact, &CollisionInfo->Impact);	
			CollisionInfo->IsValid = GR_TRUE;

			grActor_SetCollidedBone(Actor,BestBoneName);

			return GR_TRUE;
		}		
	}

	return GR_FALSE;
}

// Added by Icestorm
//========================================================================================
//	grActor_ChangeBoxCollision
//      Tests to see if a Box changing from FrontBox to BackBox collides with the Actor.
//  The Actor's bounding box must have previously been set and boxes must be relative to
//  the path (meaning not in world-space coordinates).
//
//  Bonelevel collision is set is specified, a bone level
//  collision is done based on bones that have been added with AddCollisionBone().
//  CollidedBone gets set if there is a collision.
//========================================================================================
GRAPI grBoolean GRCC grActor_ChangeBoxCollision( grActor	*Actor, const grWorld *World, const grVec3d *Pos, const grExtBox	*FrontBox,
	const grExtBox	*BackBox, grChangeBoxCollisionInfo *CollisionInfo)
{
	grExtBox ActorBox, BoneBox, xChangeBox;
	grFloat T, BestT;
	grVec3d Normal, BestNormal, Impact, BestImpact;
	char *BoneName = NULL, *BestBoneName = NULL;	

	assert(Actor != NULL);
	assert(FrontBox != NULL);
	assert(BackBox != NULL);
	assert(Pos != NULL);
	
	// Get actor box and make sure it's valid
	grActor_GetExtBox(Actor,&ActorBox);
	if(!grExtBox_IsValid(&ActorBox)||grExtBox_IsPoint(&ActorBox)) return GR_FALSE;

	if(grWorld_GetCollisionLevel(World) != COLLIDE_BONES  || grActor_GetNextCollisionBone(Actor, NULL) == NULL)
	{
		// Not bone level collision
		if (CollisionInfo)
		{
			if(grExtBox_ChangeBoxCollision(&ActorBox, Pos, FrontBox, BackBox, &T, &Normal, &Impact))
			{
				CollisionInfo->ImpactBox.Min.X=FrontBox->Min.X+T*(BackBox->Min.X-FrontBox->Min.X);
				CollisionInfo->ImpactBox.Min.Y=FrontBox->Min.Y+T*(BackBox->Min.Y-FrontBox->Min.Y);
				CollisionInfo->ImpactBox.Min.Z=FrontBox->Min.Z+T*(BackBox->Min.Z-FrontBox->Min.Z);

				CollisionInfo->ImpactBox.Max.X=FrontBox->Max.X+T*(BackBox->Max.X-FrontBox->Max.X);
				CollisionInfo->ImpactBox.Max.Y=FrontBox->Max.Y+T*(BackBox->Max.Y-FrontBox->Max.Y);
				CollisionInfo->ImpactBox.Max.Z=FrontBox->Max.Z+T*(BackBox->Max.Z-FrontBox->Max.Z);

				grActor_SetCollidedBone(Actor,NULL);

				CollisionInfo->Plane.Normal=Normal;
				CollisionInfo->Plane.Type=Type_Any;
				CollisionInfo->Plane.Dist=grVec3d_DotProduct(&Normal,&Impact)+1.5f;
		
				return GR_TRUE;
			}
		} else
			return grExtBox_ChangeBoxCollision(&ActorBox, Pos, FrontBox, BackBox, NULL, NULL, NULL);
	}
	else
	{
		// Verify the box "sweep" intersects the actor box		
		grExtBox_Union(FrontBox,BackBox,&xChangeBox);
		if(!grExtBox_Intersection(&ActorBox, &xChangeBox, NULL)) return GR_FALSE;

		// Bone level collision
		grActor_RecalcCollisionBones(Actor);
		BoneName = NULL;				
		while ( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, NULL, &BoneBox)) != NULL )
		{
			assert(grExtBox_IsValid(&BoneBox) && !grExtBox_IsPoint(&BoneBox));
			if(grExtBox_ChangeBoxCollision(&BoneBox, Pos, FrontBox, BackBox, &T, &Impact, &Normal))
			{
				if(BestBoneName == NULL || (BestBoneName != NULL && T < BestT))
				{
					BestBoneName = BoneName;
					BestT = T;
					BestNormal = Normal;
					BestImpact = Impact;
				}
			} else
				if (grExtBox_ChangeBoxCollision(&BoneBox, Pos, FrontBox, BackBox, NULL, NULL, NULL))
					return GR_TRUE;
		}		

		if(BestBoneName != NULL)
		{
			T = BestT;
			Normal = BestNormal;

			CollisionInfo->ImpactBox.Min.X=FrontBox->Min.X+T*(BackBox->Min.X-FrontBox->Min.X);
			CollisionInfo->ImpactBox.Min.Y=FrontBox->Min.Y+T*(BackBox->Min.Y-FrontBox->Min.Y);
			CollisionInfo->ImpactBox.Min.Z=FrontBox->Min.Z+T*(BackBox->Min.Z-FrontBox->Min.Z);

			CollisionInfo->ImpactBox.Max.X=FrontBox->Max.X+T*(BackBox->Max.X-FrontBox->Max.X);
			CollisionInfo->ImpactBox.Max.Y=FrontBox->Max.Y+T*(BackBox->Max.Y-FrontBox->Max.Y);
			CollisionInfo->ImpactBox.Max.Z=FrontBox->Max.Z+T*(BackBox->Max.Z-FrontBox->Max.Z);

			grActor_SetCollidedBone(Actor,BestBoneName);

			CollisionInfo->Plane.Normal=Normal;
			CollisionInfo->Plane.Type=Type_Any;
			CollisionInfo->Plane.Dist=grVec3d_DotProduct(&Normal,&BestImpact)+1.5f;

			return GR_TRUE;
		}		
	}

	return GR_FALSE;
}

//========================================================================================
//	grActor_AddCollisionBone
//========================================================================================
GRAPI void GRCC grActor_AddCollisionBone(grActor *Actor, const char* BoneName)
{
	grExtBox BExtBox;
	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	assert(BoneName != NULL);

	if(!grActor_HasCollisionBone(Actor,BoneName))
		//Icestorm: Better don't add non-valid-collision bones ;)
		if(grActor_GetBoneExtBox(Actor,BoneName,&BExtBox)&&
			grExtBox_IsValid(&BExtBox) && !grExtBox_IsPoint(&BExtBox))
		{
			// Icestorm: Save ExtBox
			grCollisionBone		*Bone;

			Bone=GR_RAM_ALLOCATE_STRUCT(grCollisionBone);
			Bone->BoneName=strdup(BoneName);
			grActor_GetBoneIndex(Actor, BoneName, &(Bone->BoneIndex) );
			*(Bone->CurrExtBox=GR_RAM_ALLOCATE_STRUCT(grExtBox))=BExtBox;
			*(Bone->PrevExtBox=GR_RAM_ALLOCATE_STRUCT(grExtBox))=BExtBox;
			grChain_AddLinkData(Actor->BoneCollisionChain, (void*)(Bone));	
		}
}

//========================================================================================
//	grActor_RemoveCollisionBone
//========================================================================================
GRAPI void GRCC grActor_RemoveCollisionBone(grActor *Actor, const char* BoneName)
{
	grChain_Link *Link;	

	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	assert(BoneName != NULL);
	
	for (Link = grChain_GetFirstLink(Actor->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
	{
		// locals
		grCollisionBone		*LinkBone;
			
		LinkBone = (grCollisionBone*)grChain_LinkGetLinkData( Link );		
		if(LinkBone && _stricmp(BoneName,LinkBone->BoneName)==0)
		{
			// Icestorm: Free ExtBoxes (& Name)
			if (Link==Actor->LastUsedCollisionBone)
			{
				Actor->LastUsedCollisionBone=NULL;
				Actor->LastUsedCollisionBoneName=NULL;
			}
			free(LinkBone->BoneName);
			grRam_Free(LinkBone->CurrExtBox);
			grRam_Free(LinkBone->PrevExtBox);
			grRam_Free(LinkBone);
			grChain_RemoveLink(Actor->BoneCollisionChain, Link);
			return;
		}
	}				
}

//========================================================================================
//	grActor_HasCollisionBone
//========================================================================================
GRAPI grBoolean GRCC grActor_HasCollisionBone(const grActor *Actor, const char* BoneName)
{
	grChain_Link *Link;	

	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	assert(BoneName != NULL);
	
	for (Link = grChain_GetFirstLink(Actor->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
	{
		// locals
		grCollisionBone		*LinkBone;
			
		LinkBone = (grCollisionBone *)grChain_LinkGetLinkData( Link );		
		if(LinkBone && _stricmp(BoneName,LinkBone->BoneName)==0)
			return GR_TRUE;
	}

	return GR_FALSE;
}

//========================================================================================
//	grActor_GetNextCollisionBone ( rewritten by Icestorm [hybrid:grChain_GetNextLinkData] )
//========================================================================================
GRAPI char* GRCC grActor_GetNextCollisionBone(grActor *Actor, char *BoneName)
{
	grChain_Link *Link;	
	grCollisionBone		*LinkBone=NULL;

	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	
	if(BoneName == NULL)
		Link = grChain_GetFirstLink(Actor->BoneCollisionChain);
	else
	// Icestorm: _stricmp would be more general, but for lin. search this is better ;)
	if(BoneName==Actor->LastUsedCollisionBoneName)
		Link = grChain_LinkGetNext(Actor->LastUsedCollisionBone);
	else
		for (Link = grChain_GetFirstLink(Actor->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
		{
			// locals
			
			LinkBone = (grCollisionBone *)grChain_LinkGetLinkData( Link );		
			if(LinkBone && _stricmp(BoneName,LinkBone->BoneName)==0)
			{
				Link = grChain_LinkGetNext(Link);
				break;
			}
		}
	
	if (Link)
	{
		LinkBone=(grCollisionBone*)grChain_LinkGetLinkData(Link);
		Actor->LastUsedCollisionBone=Link;
		Actor->LastUsedCollisionBoneName=LinkBone->BoneName;
		return(LinkBone->BoneName);
	}
	else
		return NULL;
}

//========================================================================================
//	grActor_GetNextCollisionBoneWithExtBoxes - By Icestorm
//========================================================================================
GRAPI char* GRCC grActor_GetNextCollisionBoneWithExtBoxes(	grActor		*Actor,
																char		*BoneName,
																grExtBox	*PrevBox,
																grExtBox	*CurrBox)
{
	grChain_Link *Link;	
	grCollisionBone		*LinkBone=NULL;

	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	
	if(BoneName == NULL)
		Link = grChain_GetFirstLink(Actor->BoneCollisionChain);
	else
	// Icestorm: _stricmp would be more general, but for lin. search this is better ;)
	if(BoneName==Actor->LastUsedCollisionBoneName)
		Link = grChain_LinkGetNext(Actor->LastUsedCollisionBone);
	else
		for (Link = grChain_GetFirstLink(Actor->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
		{
			LinkBone = (grCollisionBone *)grChain_LinkGetLinkData( Link );		
			if(LinkBone && _stricmp(BoneName,LinkBone->BoneName)==0)
			{
				Link = grChain_LinkGetNext(Link);
				break;
			}
		}
	
	if (Link)
	{
		LinkBone=(grCollisionBone*)grChain_LinkGetLinkData(Link);
		Actor->LastUsedCollisionBone=Link;
		Actor->LastUsedCollisionBoneName=LinkBone->BoneName;
		if (PrevBox) *PrevBox=*(LinkBone->PrevExtBox);
		if (CurrBox) *CurrBox=*(LinkBone->CurrExtBox);
		return(LinkBone->BoneName);
	}
	else
		return NULL;
}

//========================================================================================
//	grActor_RecalcCollisionBones - By Icestorm
//========================================================================================
GRAPI void GRCC grActor_RecalcCollisionBones(grActor *Actor)
{
	grChain_Link		*Link;	
	grCollisionBone		*LinkBone=NULL;
	grExtBox			*BoneBox;

	assert(Actor != NULL);
	assert(Actor->BoneCollisionChain != NULL);
	
	for (Link = grChain_GetFirstLink(Actor->BoneCollisionChain); Link; Link = grChain_LinkGetNext(Link))
	{
		LinkBone = (grCollisionBone *)grChain_LinkGetLinkData( Link );		
		if (LinkBone)
		{
			BoneBox=LinkBone->PrevExtBox;
			LinkBone->PrevExtBox=LinkBone->CurrExtBox;
			LinkBone->CurrExtBox=BoneBox;
			#ifndef NDEBUG
				assert(grActor_GetBoneExtBoxByIndex(Actor, LinkBone->BoneIndex, BoneBox));
			#else
				grActor_GetBoneExtBoxByIndex(Actor, LinkBone->BoneIndex, BoneBox);
			#endif
		}
	}
}

//========================================================================================
//	grActor_MoveCollision - By Incarnadine
//========================================================================================
#define LARGE_NUMBER 9999999.0f

// This function checks an Actor to see if it collides with other Actors in the world or
// with the world itself.  The current Actor position is irrelevant as it assumes the actor
// is moving on a path from Front to Back.  The actor's collision box must have previously
// been properly set.
// (Modified by Icestorm)
GRAPI grBoolean GRCC grActor_MoveCollision(	grActor *Actor,
											const grWorld *World, 
											const grVec3d *Front, 
											const grVec3d *Back, 											
											grCollisionInfo *CollisionInfo)
{
	char *BoneName;
	grExtBox ActorBox, BoneBox;
	grVec3d vActorBoxPos;
	grXForm3d ActorTransform;	
	grCollisionInfo WorldCollisionInfo;
	grFloat BestDist = LARGE_NUMBER, Dist;	
	grBoolean Hit = GR_FALSE;
	int32 iContents;
	grObject *Obj;

	// Get the name of the first collision bone (if there is one)
	BoneName = grActor_GetNextCollisionBone(Actor, NULL);	

	// Get the actor's bounding box
	grActor_GetExtBox(Actor,&ActorBox);

	// We need the actor's box to be relative to the path Front-->Back
	grExtBox_GetTranslation(&ActorBox,&vActorBoxPos);  // Get box pos (world)
	grActor_GetBoneTransform(Actor, NULL, &ActorTransform); // Get actor pos (world)
	grVec3d_Subtract(&vActorBoxPos, &ActorTransform.Translation, &vActorBoxPos); // New box pos (relative)
	grExtBox_SetTranslation(&ActorBox,&vActorBoxPos); // Set the box to relative pos

	// Make the actor empty so it doesn't collide with itself	
	Obj = NULL;	
	while( ( Obj = grWorld_GetNextObject((grWorld*)World, Obj) ) != NULL)
		if(grObject_GetInstance(Obj) == Actor) break;
		
	if(Obj != NULL)
	{
		iContents = grObject_GetContents(Obj);
		grObject_SetContents(Obj, CONTENTS_EMPTY);
	}

	if( grWorld_GetCollisionLevel(World) != COLLIDE_BONES || BoneName == NULL )
	{
		if(grWorld_Collision(World, &ActorBox, Front, Back, CollisionInfo))
			Hit = GR_TRUE;
	}
	else
	{				
		BestDist = LARGE_NUMBER;

		grActor_RecalcCollisionBones(Actor);
		grActor_GetBoneTransform(Actor, NULL, &ActorTransform); // Get actor pos (world)

		BoneName = NULL;			

		// Get the name of the first collision bone (if there is one)
		while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, NULL, &BoneBox)) != NULL )
		{
			assert(grExtBox_IsValid(&BoneBox) && !grExtBox_IsPoint(&BoneBox));
			grExtBox_SetNewOrigin(&BoneBox, &ActorTransform.Translation);

			if (CollisionInfo)
			{
				if(grWorld_Collision(World, &BoneBox, Front, Back, &WorldCollisionInfo))
				{
					Dist = grVec3d_DistanceBetween(Front, &WorldCollisionInfo.Impact);

					if(Dist < BestDist)
					{
						BestDist = Dist;
						*CollisionInfo = WorldCollisionInfo;
						grActor_SetCollidedBone(Actor,BoneName);
						Hit = GR_TRUE;
					}
				}
			} else
				if(grWorld_Collision(World, &BoneBox, Front, Back, NULL))
				{
					// Restore actor contents
					if(Obj != NULL)
						grObject_SetContents(Obj, iContents);

					return GR_TRUE;
				}
		} 
	} 

	// Restore actor contents
	if(Obj != NULL)
		grObject_SetContents(Obj, iContents);

	return Hit;
}

//Icestorm: Need this for pushing a growing extbox
static void MoveBoxForPlaneOut(const grVec3d *Pos, const grExtBox *Box, const grPlane *Plane, grVec3d *MoveVec)
{
	grVec3d		Normal;
	grFloat		Dist;

	Normal = Plane->Normal;
	Dist=0.0f;
	
	if (Normal.X < 0)
		Dist += Normal.X * Box->Max.X;
	else	 
		Dist += Normal.X * Box->Min.X;
	
	if (Normal.Y < 0)
		Dist += Normal.Y * Box->Max.Y;
	else
		Dist += Normal.Y * Box->Min.Y;

	if (Normal.Z < 0)
		Dist += Normal.Z * Box->Max.Z;
	else							 
		Dist += Normal.Z * Box->Min.Z;
	Dist+=grPlane_PointDistanceFast(Plane,Pos);

	MoveVec->X=-Dist*Plane->Normal.X;
	MoveVec->Y=-Dist*Plane->Normal.Y;
	MoveVec->Z=-Dist*Plane->Normal.Z;
}

//========================================================================================
//	grActor_RotateCollision
//     This function checks an Actor to see if it collides with other Actors in the world or
// with the world itself during a rotation (orientation) change.  This isn't really needed if 
// you're not using bone level collisions.  If you are, it's very necessary in order to keep 
// your actors in the world.
// (Modified by Icestorm)
//========================================================================================
GRAPI grBoolean GRCC grActor_RotateCollision(grActor *Actor,
											const grWorld *World,											
											const grXForm3d *NewTransform, 
											grCollisionInfo *CollisionInfo)
{
	char *BoneName;
	grExtBox BoneBox, NewBoneBox;
	grVec3d vPos, vBoneBoxPos, vNewBoneBoxPos, vNormal, vMoved;
	grXForm3d ActorTransform, OldTransform;	
	grCollisionInfo WorldCollisionInfo;
	grChangeBoxCollisionInfo WorldChangeBoxCollisionInfo;
	grFloat Dist;	
	grBoolean Hit = GR_FALSE;
	int32 iContents;
	grObject *Obj;

	// Get the name of the first collision bone (if there is one)
	BoneName = grActor_GetNextCollisionBone(Actor, NULL);	

	// Make the actor empty so it doesn't collide with itself	
	Obj = NULL;	
	while( ( Obj = grWorld_GetNextObject((grWorld*)World, Obj) ) != NULL)
		if(grObject_GetInstance(Obj) == Actor) break;
		
	if(Obj != NULL)
	{
		iContents = grObject_GetContents(Obj);
		grObject_SetContents(Obj, CONTENTS_EMPTY);
	}


	if( grWorld_GetCollisionLevel(World) != COLLIDE_BONES || BoneName == NULL )
	{
			Hit = GR_FALSE;
	}
	else
	{				
		grActor_GetBoneTransform(Actor, NULL, &OldTransform); // Get actor pos (world)	
		grVec3d_Set(&vMoved, 0.0f, 0.0f, 0.0f);

		// --- Setup for old pos
		// Initialize actor's orientation
		grActor_ClearPose(Actor,&OldTransform);
		grActor_AnimationTestStep(Actor, 0.0f);
		// GetBoneExtBoxes
		grActor_RecalcCollisionBones(Actor);

		// ---- Setup for new pos
		// Initialize actor's orientation
		ActorTransform = *NewTransform;
		ActorTransform.Translation = OldTransform.Translation;
		grActor_ClearPose(Actor,&ActorTransform);
		grActor_AnimationTestStep(Actor, 0.0f);
		// GetNewBoneExtBoxes
		grActor_RecalcCollisionBones(Actor);

		BoneName = NULL;			

		// Get the name of the first collision bone (if there is one)
		while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, &NewBoneBox)) != NULL )
		{
			assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
			assert(grExtBox_IsValid(&NewBoneBox)&&!grExtBox_IsPoint(&NewBoneBox));
							
			// Get Position of ExtBoxes and make them relative
			grExtBox_TranslateAndMoveToOrigin(&BoneBox, &vMoved, &vBoneBoxPos);
			grExtBox_TranslateAndMoveToOrigin(&NewBoneBox, &vMoved, &vNewBoneBoxPos);
			
			// Grow BoneBox to NewBoneBox and test on collision
			if (CollisionInfo)
			{
				if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, &WorldChangeBoxCollisionInfo))
				{
					// Modify vBoneBoxPos to new pos. and get movedist.
					MoveBoxForPlaneOut(&vBoneBoxPos, &NewBoneBox, &WorldChangeBoxCollisionInfo.Plane, &vPos);
					grVec3d_Add(&vBoneBoxPos, &vPos, &vBoneBoxPos);
					grVec3d_Add(&vNewBoneBoxPos, &vPos, &vNewBoneBoxPos);
					grVec3d_Add(&vMoved, &vPos, &vMoved);

					grActor_SetCollidedBone(Actor,BoneName);
					CollisionInfo->Plane  = WorldChangeBoxCollisionInfo.Plane;
					CollisionInfo->Object = WorldChangeBoxCollisionInfo.Object;
					Hit = GR_TRUE;
				}
				// Now test on movingcollision
				if(grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, &WorldCollisionInfo))
				{
					// Adjust impact point to where we could complete the rotation
					vNormal = WorldCollisionInfo.Plane.Normal;										
					grVec3d_Subtract(&WorldCollisionInfo.Impact,&vNewBoneBoxPos,&vPos);						
					Dist = grVec3d_DotProduct(&vNormal,&vPos);
					grVec3d_AddScaled(&vMoved, &vNormal, Dist, &vMoved);
					
					*CollisionInfo = WorldCollisionInfo;
					grActor_SetCollidedBone(Actor,BoneName);
					
					Hit = GR_TRUE;
				}							 
			} else
				if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, NULL) ||
					grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, NULL))
				{
					// Restore actor contents
					if(Obj != NULL)
						grObject_SetContents(Obj, iContents);

					return GR_TRUE;
				}

		}

		if (Hit)
		{
			// Icestorm: Can we move to there?
			grVec3d Front,Back;

			Front = OldTransform.Translation;
			grVec3d_Add(&Front, &vMoved, &Back);

			grVec3d_Add(&OldTransform.Translation, &vMoved, &CollisionInfo->Impact);
			CollisionInfo->IsValid = GR_TRUE;

			BoneName = NULL;

			// Get the name of the first collision bone (if there is one)
			while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, NULL)) != NULL )
			{
			
				assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
				grExtBox_SetNewOrigin(&BoneBox, &OldTransform.Translation);
	
				if(grWorld_Collision(World, &BoneBox, &Front, &Back, NULL))
				{
					CollisionInfo->IsValid = GR_FALSE;
					break;
				}
			}

			// Icestorm: Can we rotate there?

			if (CollisionInfo->IsValid!=GR_FALSE)
			{
				BoneName = NULL;			

				// Get the name of the first collision bone (if there is one)
				while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, &NewBoneBox)) != NULL )
				{
					assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
					assert(grExtBox_IsValid(&NewBoneBox)&&!grExtBox_IsPoint(&NewBoneBox));
							
					grExtBox_TranslateAndMoveToOrigin(&BoneBox, &vMoved, &vBoneBoxPos);
					grExtBox_TranslateAndMoveToOrigin(&NewBoneBox, &vMoved, &vNewBoneBoxPos);
			
					// Grow BoneBox to NewBoneBox and test on collision
					// And test on movingcollision
					if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, NULL)||
					    grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, NULL))
					{
						CollisionInfo->IsValid = GR_FALSE;
						break;
					}
				}
			}
		}

		// Initialize actor's orientation
		grActor_ClearPose(Actor,&OldTransform);
		grActor_AnimationTestStep(Actor, 0.0f);
	}

	// Restore actor contents
	if(Obj != NULL)
		grObject_SetContents(Obj, iContents);

	return Hit;
}

//========================================================================================
//	grActor_AnimationCollision
//     This function checks an Actor to see if it collides with other Actors in the world or
// with the world itself during an animation change.  This isn't really needed if 
// you're not using bone level collisions.  If you are, it's very necessary in order to keep 
// your actors in the world.
//
// (Modified by Icestorm)
//========================================================================================
GRAPI grBoolean GRCC grActor_AnimationCollision(grActor *Actor,
											const grWorld *World,											
											const grFloat DeltaTime, 
											grCollisionInfo *CollisionInfo)
{
	char *BoneName;
	grExtBox BoneBox, NewBoneBox;
	grVec3d vPos, vBoneBoxPos, vNewBoneBoxPos, vNormal, vMoved;
	grXForm3d OldTransform;	
	grCollisionInfo WorldCollisionInfo;
	grChangeBoxCollisionInfo WorldChangeBoxCollisionInfo;
	grFloat Dist;	
	grBoolean Hit = GR_FALSE;
	int32 iContents;
	grObject *Obj;

	// Get the name of the first collision bone (if there is one)
	BoneName = grActor_GetNextCollisionBone(Actor, NULL);	

	// Make the actor empty so it doesn't collide with itself	
	Obj = NULL;	
	while( ( Obj = grWorld_GetNextObject((grWorld*)World, Obj) ) != NULL)
		if(grObject_GetInstance(Obj) == Actor) break;
		
	if(Obj != NULL)
	{
		iContents = grObject_GetContents(Obj);
		grObject_SetContents(Obj, CONTENTS_EMPTY);
	}


	if( grWorld_GetCollisionLevel(World) != COLLIDE_BONES || BoneName == NULL )
	{
			Hit = GR_FALSE;
	}
	else
	{			
		grActor_GetBoneTransform(Actor, NULL, &OldTransform); // Get actor pos (world)
		grVec3d_Set(&vMoved, 0.0f, 0.0f, 0.0f);

		// --- Setup for old pos
		// Initialize actor's animation		
		grActor_ClearPose(Actor,&OldTransform);
		grActor_AnimationTestStep(Actor, 0.0f);
		grActor_RecalcCollisionBones(Actor);

		// ---- Setup for new pos
		// Initialize actor's animation
		grActor_AnimationTestStep(Actor, DeltaTime);
		grActor_RecalcCollisionBones(Actor);

		BoneName = NULL;			

		// Get the name of the first collision bone (if there is one)
		while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, &NewBoneBox)) != NULL )
		{
			
			assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
			assert(grExtBox_IsValid(&NewBoneBox)&&!grExtBox_IsPoint(&NewBoneBox));
							
			grExtBox_TranslateAndMoveToOrigin(&BoneBox, &vMoved, &vBoneBoxPos);
			grExtBox_TranslateAndMoveToOrigin(&NewBoneBox, &vMoved, &vNewBoneBoxPos);
	
			if (CollisionInfo)
			{
				// Grow BoneBox to NewBoneBox and test on collision
				if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, &WorldChangeBoxCollisionInfo))
				{
					// Modify vBoneBoxPos to new pos. and get movedist.
					MoveBoxForPlaneOut(&vBoneBoxPos, &NewBoneBox, &WorldChangeBoxCollisionInfo.Plane, &vPos);
					grVec3d_Add(&vBoneBoxPos, &vPos, &vBoneBoxPos);
					grVec3d_Add(&vNewBoneBoxPos, &vPos, &vNewBoneBoxPos);
					grVec3d_Add(&vMoved, &vPos, &vMoved);

					grActor_SetCollidedBone(Actor,BoneName);
					CollisionInfo->Plane  = WorldChangeBoxCollisionInfo.Plane;
						
					Hit = GR_TRUE;
				}
				// Now test on movingcollision
				if(grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, &WorldCollisionInfo))
				{
					// Adjust impact point to where we could complete the rotation
					vNormal = WorldCollisionInfo.Plane.Normal;										
					grVec3d_Subtract(&WorldCollisionInfo.Impact,&vNewBoneBoxPos,&vPos);						
					Dist = grVec3d_DotProduct(&vNormal,&vPos);
					grVec3d_AddScaled(&vMoved,&vNormal,Dist,&vMoved);
				
					*CollisionInfo = WorldCollisionInfo;
					grActor_SetCollidedBone(Actor,BoneName);
			
					Hit = GR_TRUE;
				}				
			} else
				if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, NULL) ||
					grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, NULL))
				{
					// Restore actor contents
					if(Obj != NULL)
						grObject_SetContents(Obj, iContents);

					return GR_TRUE;
				}
		} 

		if (Hit)
		{
			// Icestorm: Can we move to there?
			grVec3d Front,Back;

			Front = OldTransform.Translation;
			grVec3d_Add(&Front, &vMoved, &Back);

			grVec3d_Add(&OldTransform.Translation, &vMoved, &CollisionInfo->Impact);
			CollisionInfo->IsValid = GR_TRUE;

			BoneName = NULL;

			// Get the name of the first collision bone (if there is one)
			while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, NULL)) != NULL )
			{
			
				assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
				grExtBox_SetNewOrigin(&BoneBox, &OldTransform.Translation);
	
				if(grWorld_Collision(World, &BoneBox, &Front, &Back, NULL))
				{
					CollisionInfo->IsValid = GR_FALSE;
					break;
				}
			}

			// Icestorm: Can we animate there?

			if (CollisionInfo->IsValid!=GR_FALSE)
			{
				BoneName = NULL;			

				// Get the name of the first collision bone (if there is one)
				while( (BoneName = grActor_GetNextCollisionBoneWithExtBoxes(Actor, BoneName, &BoneBox, &NewBoneBox)) != NULL )
				{
					assert(grExtBox_IsValid(&BoneBox)&&!grExtBox_IsPoint(&BoneBox));
					assert(grExtBox_IsValid(&NewBoneBox)&&!grExtBox_IsPoint(&NewBoneBox));
							
					grExtBox_TranslateAndMoveToOrigin(&BoneBox, &vMoved, &vBoneBoxPos);
					grExtBox_TranslateAndMoveToOrigin(&NewBoneBox, &vMoved, &vNewBoneBoxPos);
			
					// Grow BoneBox to NewBoneBox and test on collision
					// And test on movingcollision
					if (grWorld_ChangeBoxCollision(World, &vBoneBoxPos, &BoneBox, &NewBoneBox, NULL)||
					    grWorld_Collision(World, &NewBoneBox, &vBoneBoxPos, &vNewBoneBoxPos, NULL))
					{
						CollisionInfo->IsValid = GR_FALSE;
						break;
					}
				}
			}
		}
		// Initialize actor's animation		
		grActor_ClearPose(Actor,&OldTransform);
		grActor_AnimationTestStep(Actor, 0.0f);
	}

	// Restore actor contents
	if(Obj != NULL)
		grObject_SetContents(Obj, iContents);

	return Hit;
}

// This function is used to set whether or not a particular Actor should be checked
// during a collision test.  Three Flags are possible, you should set ONLY one:
//   COLLIDE_EMPTY - This actor can not collide against anything or have other objects collide with it.
//   COLLIDE_SOLID - This actor can collide with other objects and have other objects collide with it.
//   COLLIDE_INVISIBLE - This actor can collide with other objects, but other objects can not collide with it.
// Actors are initialized with a COLLIDE_SOLID state.
GRAPI void GRCC grActor_SetCollisionFlags(grActor* Actor, const uint32 Flags)
{
	Actor->CollisionFlags = Flags;
}

GRAPI uint32 GRCC grActor_GetCollisionFlags(const grActor* Actor)
{
	return Actor->CollisionFlags;
}

GRAPI void GRCC grActor_SetCollidedBone(grActor* Actor, char *BoneName)
{
	Actor->CollidedBone = BoneName;
}

GRAPI char* GRCC grActor_GetCollidedBone(const grActor* Actor)
{
	return Actor->CollidedBone;
}


// --- Incarnadine: End Actor Collision ---
//========================================================================================

//========================================================================================
// --- Incarnadine: Begin New Support Functions for ActorObj
GRAPI grBoolean GRCC grActor_GetXForm(
	const grActor *Actor,	// object instance data
	grXForm3d	*Xf )		// where to store xform
{

	// ensure valid data
	assert( Actor != NULL );
	assert( Xf != NULL );

	// save xform
	//return grActor_GetBoneTransform(Object, NULL, Xf);
	*Xf = Actor->Xf;
	return GR_TRUE;
} // GetXForm()

GRAPI grBoolean GRCC grActor_SetXForm(
	grActor* Actor,	// object instance data
	const grXForm3d	*Xf )		// where to store xform
{
	ActorObj *Object;

	// ensure valid data
	assert( Actor != NULL );
	assert( Xf != NULL );
	Object = Actor->Object;

	Actor->Xf = *Xf;

	if(Actor->ActorDefinition)
	{
		grActor_ClearPose( Actor,  Xf );
		grActor_AnimationTestStep(Actor,0.0f);

		if(Object->Motion)
		{
			grActor_SetPose( Actor, Object->Motion, Object->MotionTime, &Actor->Xf );
		}
	}
	return 	GR_TRUE;
} // SetXForm()
// --- Incarnadine: End New Support Functions for ActorObj
//========================================================================================

//KROUER
// Actor VIS problem
GRAPI void GRCC grActor_SetRenderNextTime(grActor* Actor, grBoolean RenderNextTime)
{
	//EnterCriticalSection(&Actor->RenderLock);
	Actor->RenderNextTime = RenderNextTime;
	//LeaveCriticalSection(&Actor->RenderLock);
}

//========================================================================================
// --- Incarnadine: Begin ActorObj ---
#include "ActorObj.h"
//extern grObjectDef grActor_ObjectDef;

GRAPI void	GRCC grActor_InitObject(const grActor *A,grObject *O)
{
	assert( grActor_IsValid(A) );	
	assert( O );
	O->Name = NULL;
	O->Methods = &grActor_ObjectDef;
	O->Instance = (void *)A;
	O->RefCnt = 0;
}

GRAPI grBoolean GRCC grActor_RegisterObjectDef(void)
{
	return grObject_RegisterGlobalObjectDef( &grActor_ObjectDef );
}
// --- Incarnadine: End ActorObj ---
//========================================================================================
