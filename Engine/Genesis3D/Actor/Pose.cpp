/****************************************************************************************/
/*  POSE.C																				*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Bone hierarchy implementation.							.				*/
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

#pragma message ("could optimize a name binded setPose by caching the mapping from motionpath[i] to joint[j]")

#include <assert.h>
#include <string.h>

#include "Ram.h"
#include "Errorlog.h"
#include "Pose.h"
#include "StrBlock.h"

#define GR_POSE_STARTING_JOINT_COUNT (1)


/* this object maintains a hierarchy of joints.
   the hierarchy is stored as an array of joints, with each joint having an number
   that is it's parent's index in the array.  
   This code assumes:  
   **The parent's index is always smaller than the child**
*/

typedef struct grPose_Joint
{
	int			 ParentJoint;		// parent of path
	grXForm3d    *Transform;		// matrix for path	(pointer into TransformArray)
	grQuaternion Rotation;			// quaternion representation for orientation of above Transform

	grVec3d		 UnscaledAttachmentTranslation;	
					// point of Attachment to parent (in parent frame of ref) **Unscaled
	grQuaternion AttachmentRotation;// rotation of attachement to parent (in parent frame of ref)
	grXForm3d    AttachmentTransform;	//------------

	grVec3d		 LocalTranslation;	// translation relative to attachment 
	grQuaternion LocalRotation;		// rotation relative to attachment 

	grBoolean    Touched;			// if this joint has been touched and needs recomputation
	grBoolean    NoAttachmentRotation; // GR_TRUE if there is no attachment rotation.
	int			 Covered;			// if joint has been 100% set (no blending)
} grPose_Joint;						// structure to bind a name and a path for a joint

typedef struct grPose
{
	int				  JointCount;	// number of joints in the motion
	int32			  NameChecksum;	// checksum based on joint names and list order
	grBoolean		  Touched;		// if any joint has been touched & needs recomputation	
	grStrBlock		 *JointNames;
	grVec3d			  Scale;		// current scaling. Used for scaling motion samples

	grBoolean		  Slave;			// if pose is 'slaved' to parent -vs- attached.
	int				  SlaveJointIndex;	// index of 'slaved' joint
	grPose			 *Parent;		
	grPose_Joint	  RootJoint;		
	grXForm3d		  ParentsLastTransform;	// Compared to parent's transform to see if it changed: recompute is needed
	grXForm3d		  RootTransform;
	grXFArray		 *TransformArray;	
	grPose_Joint	 *JointArray;
	int				  OnlyThisJoint;		// update only this joint (and it's parents) if this is >0
} grPose;



static void grPose_ReattachTransforms(grPose *P)
{
	int XFormCount;
	int JointCount;
	grXForm3d *XForms;
	int i;

	assert( P != NULL );

	JointCount = P->JointCount;
	if (JointCount > 0)
		{
			assert( P->TransformArray != NULL );

			XForms = grXFArray_GetElements(P->TransformArray,&XFormCount);
			
			assert( XForms != NULL );
			assert( XFormCount == JointCount );
			
			for (i=0; i<JointCount; i++)
				{
					P->JointArray[i].Transform=&(XForms[i]);
				}
		}
	P->RootJoint.Transform = &(P->RootTransform);
}
	

static const grPose_Joint *grPose_JointByIndex(const grPose *P, int Index)
{
	assert( P != NULL );
	assert( (Index >=0)                 || (Index==(GR_POSE_ROOT_JOINT)));
	assert( (Index < P->JointCount)     || (Index==(GR_POSE_ROOT_JOINT)));

	if (Index == GR_POSE_ROOT_JOINT)
		{
			return &(P->RootJoint);
		}
	else
		{
			return &(P->JointArray[Index]);
		}
}

static void GRCF grPose_SetAttachmentRotationFlag( grPose_Joint *Joint)
{
	grQuaternion Q = Joint->AttachmentRotation;
#define GR_POSE_ROTATION_THRESHOLD (0.0001)  // if the rotation is closer than this to zero for
										     // quaterion elements X,Y,Z -> no rotation computed
	if (     (  (Q.X<GR_POSE_ROTATION_THRESHOLD) && (Q.X>-GR_POSE_ROTATION_THRESHOLD) ) 
		  && (  (Q.Y<GR_POSE_ROTATION_THRESHOLD) && (Q.Y>-GR_POSE_ROTATION_THRESHOLD) ) 
		  && (  (Q.Z<GR_POSE_ROTATION_THRESHOLD) && (Q.Z>-GR_POSE_ROTATION_THRESHOLD) )  )
		{
			Joint->NoAttachmentRotation = GR_TRUE;
		}
	else
		{
			Joint->NoAttachmentRotation = GR_FALSE;
		}
}

static void GRCF grPose_InitializeJoint(grPose_Joint *Joint, int ParentJointIndex, const grXForm3d *Attachment)
{
	assert( Joint != NULL );
	
	Joint->ParentJoint = ParentJointIndex;
	if (Attachment != NULL)
		{
			grQuaternion_FromMatrix(Attachment,&(Joint->AttachmentRotation));
			Joint->AttachmentTransform = *Attachment;
			Joint->UnscaledAttachmentTranslation = Joint->AttachmentTransform.Translation;
		}
	else
		{
			grQuaternion_SetNoRotation(&(Joint->AttachmentRotation));
			grXForm3d_SetIdentity(&(Joint->AttachmentTransform));
			Joint->UnscaledAttachmentTranslation = Joint->AttachmentTransform.Translation;
		}

	grQuaternion_SetNoRotation(&(Joint->LocalRotation));
	
	grXForm3d_SetIdentity(Joint->Transform);
	grQuaternion_SetNoRotation(&(Joint->Rotation));
	
	grVec3d_Set( (&Joint->LocalTranslation),0.0f,0.0f,0.0f);
	grQuaternion_SetNoRotation(&(Joint->LocalRotation));
	Joint->Touched = GR_TRUE;		
	grPose_SetAttachmentRotationFlag(Joint);
}



grPose *GRCF grPose_Create(void)
{
	grPose *P;

	P = GR_RAM_ALLOCATE_STRUCT_CLEAR(grPose);

	if ( P == NULL )
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grPose_Create.");
			goto PoseCreateFailure;
		}
	P->JointCount = 0;
	P->OnlyThisJoint = GR_POSE_ROOT_JOINT-1;		
	P->JointNames = grStrBlock_Create();
	P->Touched = GR_FALSE;
	if ( P->JointNames == NULL )
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPose_Create: failed to create string block.");
			goto PoseCreateFailure;
		}
	P->JointArray = GR_RAM_ALLOCATE_STRUCT_CLEAR( grPose_Joint );
	if (P->JointArray == NULL)
		{
			grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grPose_Create.");
			goto PoseCreateFailure;
		}
	P->TransformArray=NULL; //grXFArray_Create(0);

	P->Slave = GR_FALSE;
	P->Parent = NULL;
	grPose_ReattachTransforms(P);
	grPose_InitializeJoint(&(P->RootJoint),GR_POSE_ROOT_JOINT,NULL);

	P->Scale.X = P->Scale.Y = P->Scale.Z = 1.0f;
	return P;
	PoseCreateFailure:
	if (P!=NULL)
		{
			if (P->JointNames != NULL)
				grStrBlock_Destroy(&(P->JointNames));
			if (P->JointArray != NULL)
				grRam_Free(P->JointArray);
			grRam_Free(P);
		}
	return NULL;
}

void GRCF grPose_Destroy(grPose **PP)
{
	assert(PP   != NULL );
	assert(*PP  != NULL );

	assert( (*PP)->JointNames != NULL );
	assert( grStrBlock_GetCount((*PP)->JointNames) == (*PP)->JointCount );
	grStrBlock_Destroy( &( (*PP)->JointNames ) );
	if ((*PP)->TransformArray!=NULL)
		{
			grXFArray_Destroy(&( (*PP)->TransformArray) );
		}
	if ((*PP)->JointArray != NULL)
		grRam_Free((*PP)->JointArray);
	grRam_Free( *PP );

	*PP = NULL;
}

// uses J->LocalRotation and J->LocalTranslation to compute 
//    J->Rotation,J->Translation and J->Transform
static void GRCF grPose_JointRelativeToParent(
		 const grPose_Joint *Parent,
		 grPose_Joint *J)
{
	
	#if 0
		// the math in clearer (but slower) matrix form.
		// W = PAK
		grXForm3d X;
		grXForm3d K;

		grQuaternion_ToMatrix(&(J->LocalRotation),&K);
		K.Translation = J->LocalTranslation;

		grXForm3d_Multiply((Parent->Transform),&(J->AttachmentTransform),&X);
		grXForm3d_Multiply(&X,&(K),J->Transform);
		
		J->LocalTranslation = K.Translation;
		grQuaternion_FromMatrix(J->Transform,&(J->LocalRotation));

	#endif


	grVec3d *Translation = &(J->Transform->Translation);
	if (J->NoAttachmentRotation != GR_FALSE)
		{
			//    ( no attachment rotation )
			//ROTATION:
			// concatenate local rotation to parent rotation for complete rotation
			grQuaternion_Multiply(&(Parent->Rotation), &(J->LocalRotation), &(J->Rotation));
			
			grQuaternion_ToMatrix(&(J->Rotation), (J->Transform));
			//TRANSLATION:
			grVec3d_Add(&(J->LocalTranslation),&(J->AttachmentTransform.Translation),Translation);
			grXForm3d_Transform((Parent->Transform),Translation,Translation);
		}
	else
		{
			//  (there is an attachment rotation)
			
			grQuaternion BaseRotation; // attachement transform applied to the parent transform:
			//ROTATION:
			// concatenate attachment rotation to parent rotation for base rotation
			grQuaternion_Multiply(&(Parent->Rotation),&(J->AttachmentRotation),&BaseRotation);
			// concatenate base rotation with local rotation for complete rotation
			grQuaternion_Multiply(&BaseRotation, &(J->LocalRotation), &(J->Rotation));

			grQuaternion_ToMatrix(&(J->Rotation), (J->Transform));

			//TRANSLATION:
			grXForm3d_Transform(&(J->AttachmentTransform),&(J->LocalTranslation),Translation);
			grXForm3d_Transform((Parent->Transform),Translation,Translation);
		}
}


grBoolean GRCF grPose_Attach(grPose *Slave, int SlaveBoneIndex,
				  grPose *Master, int MasterBoneIndex, 
				  const grXForm3d *Attachment)
{
	grPose *P;
	P = Master;

	assert( Slave != NULL );
	assert( Master != NULL );
	assert( MasterBoneIndex >= 0);
	assert( MasterBoneIndex < Master->JointCount);
	assert( Attachment != NULL );
	assert( Master != Slave );


	assert( (SlaveBoneIndex >=0)                 || (SlaveBoneIndex==(GR_POSE_ROOT_JOINT)));
	assert( (SlaveBoneIndex < Slave->JointCount) || (SlaveBoneIndex==(GR_POSE_ROOT_JOINT)));

	while (P!=NULL)
		{
			if (P==Slave)
				{
					grErrorLog_Add(GR_ERR_BAD_PARAMETER, "grPose_Attach: circular loop of attachments not allowed");
					return GR_FALSE;
				}
			P=P->Parent;
		}

	Slave->SlaveJointIndex = SlaveBoneIndex;
	Slave->Parent = Master;
	if (SlaveBoneIndex == GR_POSE_ROOT_JOINT)
		{
			Slave->Slave = GR_FALSE;
		}
	else
		{
			Slave->Slave = GR_TRUE;
		}

	grPose_InitializeJoint(&(Slave->RootJoint),MasterBoneIndex,Attachment);
	Slave->Touched = GR_TRUE;
	Slave->ParentsLastTransform = *(Master->RootJoint.Transform);
	
	return GR_TRUE;
}


void GRCF grPose_Detach(grPose *P)
{
	P->Parent = NULL;
	P->Slave = GR_FALSE;
	grPose_InitializeJoint(&(P->RootJoint),GR_POSE_ROOT_JOINT,NULL);
}


static grBoolean GRCF grPose_TransformCompare(const grXForm3d *T1, const grXForm3d *T2)
{
	if (T1->AX != T2->AX) return GR_FALSE;
	if (T1->BX != T2->BX) return GR_FALSE;
	if (T1->CX != T2->CX) return GR_FALSE;
	if (T1->AY != T2->AY) return GR_FALSE;
	if (T1->BY != T2->BY) return GR_FALSE;
	if (T1->CY != T2->CY) return GR_FALSE;
	if (T1->AZ != T2->AZ) return GR_FALSE;
	if (T1->BZ != T2->BZ) return GR_FALSE;
	if (T1->CZ != T2->CZ) return GR_FALSE;
	
	if (T1->Translation.X != T2->Translation.X) return GR_FALSE;
	if (T1->Translation.Y != T2->Translation.Y) return GR_FALSE;
	if (T1->Translation.Z != T2->Translation.Z) return GR_FALSE;
	return GR_TRUE;
}
	
	
static void GRCF grPose_UpdateRecursively(grPose *P,int Joint)
{
	grPose_Joint *J;
	assert( P != NULL );
	assert( Joint >= GR_POSE_ROOT_JOINT );

	J=&(P->JointArray[Joint]);

	assert( J->ParentJoint < Joint);

	if (J->ParentJoint != GR_POSE_ROOT_JOINT)
		grPose_UpdateRecursively(P,J->ParentJoint);

	grPose_JointRelativeToParent(grPose_JointByIndex(P, J->ParentJoint) ,J);
}

//  updates a node if node->touched or if any of it's parents have been touched.
//  returns GR_TRUE if any updates were made.
static void GRCF grPose_UpdateRelativeToParent(grPose *P)
{
	int i;
	grPose_Joint *J;
	const grPose_Joint *Parent;
	assert( P != NULL );
	
	if ( P->Parent != NULL )
		{
			grPose_UpdateRelativeToParent(P->Parent);
			if (grPose_TransformCompare(
						(P->Parent->RootJoint.Transform),&(P->ParentsLastTransform)) != GR_FALSE)
				{
					P->Touched = GR_TRUE;
					P->RootJoint.Touched = GR_TRUE;  // bubble touched down entire hierarchy
					P->ParentsLastTransform = *(P->Parent->RootJoint.Transform);
				}
				
			if (P->Slave == GR_FALSE)
				{
					Parent = grPose_JointByIndex(P->Parent, P->RootJoint.ParentJoint);
					grPose_JointRelativeToParent(Parent,&(P->RootJoint));
				}
			else
				{
					grXForm3d_SetIdentity(P->RootJoint.Transform);
					grQuaternion_SetNoRotation(&(P->RootJoint.Rotation));
				}
		}
	else
		{
			// No parent.  RootJoint is relative to nothing.
			J = &(P->RootJoint);
			if (J->Touched)
				{
					grQuaternion_Multiply(&(J->AttachmentRotation),&(J->LocalRotation),&(J->Rotation));
					grQuaternion_ToMatrix(&(J->Rotation), (J->Transform));
					grXForm3d_Transform(&(J->AttachmentTransform),&(J->LocalTranslation),&(J->Transform->Translation));
				}
		}


	if (P->Touched == GR_FALSE)
		{
			return;
		}


	if (P->OnlyThisJoint>=GR_POSE_ROOT_JOINT)
		{
			grPose_UpdateRecursively(P,P->OnlyThisJoint);
		}
	else
		{
			for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
				{
					assert( J->ParentJoint < i);

					Parent = grPose_JointByIndex(P, J->ParentJoint);
					if (J->Touched == GR_TRUE)
						{
							grPose_JointRelativeToParent(Parent ,J);
						}
					else
						{
							if (Parent->Touched)
								{
									J->Touched = GR_TRUE;
									grPose_JointRelativeToParent(Parent,J);
								}
						}
				}
			// touched flags don't mean anything when recursing backwards.  
			P->RootJoint.Touched = GR_FALSE;
			for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
				{
					J->Touched = GR_FALSE;
				}
		}

	if (P->Slave != GR_FALSE)
		{
			grXForm3d SlavedJointInverse;
			grXForm3d FullSlaveTransform;
			grXForm3d *MasterTransform;
			grXForm3d MasterAttachment;
			
			MasterTransform = (P->Parent->JointArray[P->RootJoint.ParentJoint].Transform);
			grXForm3d_GetTranspose((P->JointArray[P->SlaveJointIndex].Transform), &SlavedJointInverse);

			grQuaternion_ToMatrix(&(P->RootJoint.AttachmentRotation), &MasterAttachment);
			//MasterAttachment.Translation = P->RootJoint.AttachmentTranslation;
			MasterAttachment.Translation = P->RootJoint.AttachmentTransform.Translation;

			grXForm3d_Multiply(MasterTransform,&MasterAttachment,&FullSlaveTransform);
			
			*(P->RootJoint.Transform) = FullSlaveTransform;
			
			grXForm3d_Multiply(&FullSlaveTransform,&SlavedJointInverse,&FullSlaveTransform);

			for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
				{
					grXForm3d_Multiply(&FullSlaveTransform,
										(P->JointArray[i].Transform),
										(P->JointArray[i].Transform));
				}
			
		}
	P->Touched = GR_FALSE;
}	


grBoolean GRCF grPose_FindNamedJointIndex(const grPose *P, const char *JointName, int *Index)
{
	int i;

	assert( P != NULL );
	assert( Index!= NULL );
	if (JointName == NULL )
		return GR_FALSE;

	for (i=0; i<P->JointCount; i++)
		{
			const char *NthName = grStrBlock_GetString(P->JointNames,i);
			assert( NthName!= NULL );
			if ( strcmp(JointName,NthName)==0 )
				{
					*Index = i;
					return GR_TRUE;
				}	
		}
	return GR_FALSE;
}
	

grBoolean GRCF grPose_AddJoint(
	grPose *P,
	int ParentJointIndex,
	const char *JointName,
	const grXForm3d *Attachment,
	int *JointIndex)
{
	int JointCount;
	grPose_Joint *Joint;

	assert(  P != NULL );
	assert( JointIndex != NULL );
	assert( P->JointCount >= 0 );
	assert( (ParentJointIndex == GR_POSE_ROOT_JOINT) || 
			((ParentJointIndex >=0) && (ParentJointIndex <P->JointCount) ) );

	// Duplicate names ARE allowed

	JointCount = P->JointCount;
	{
		grPose_Joint *NewJoints;
		NewJoints = GR_RAM_REALLOC_ARRAY(P->JointArray,grPose_Joint,JointCount+1);
		if (NewJoints == NULL)
			{
				grErrorLog_Add(GR_ERR_MEMORY_RESOURCE, "grPose_AddJoint.");
				return GR_FALSE;
			}
		P->JointArray = NewJoints;
	}
	
	assert( P->JointNames != NULL );
	assert( grStrBlock_GetCount(P->JointNames) == P->JointCount );

	if (grStrBlock_Append( &(P->JointNames), (JointName==NULL)?"":JointName )==GR_FALSE)
		{
			grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPose_AddJoint: failed to append into string block.");
			return GR_FALSE;
		}
	

	{
		grXFArray *NewXFA;
		NewXFA = grXFArray_Create(JointCount+1);
		if (NewXFA == NULL)
			{
				grErrorLog_Add(GR_ERR_SUBSYSTEM_FAILURE, "grPose_AddJoint: failed to create XFArray.");
				return GR_FALSE;
			}
		if (P->TransformArray != NULL)
			{
				grXFArray_Destroy(&(P->TransformArray));
			}
		P->TransformArray = NewXFA;
	}

	P->JointCount = JointCount+1;
	grPose_ReattachTransforms(P);
	
	Joint = &( P->JointArray[JointCount] );
	grPose_InitializeJoint(Joint,ParentJointIndex, Attachment);
	P->Touched = GR_TRUE;

	*JointIndex = JointCount;

	P->NameChecksum = grStrBlock_GetChecksum( P->JointNames );
	return GR_TRUE;
}

void GRCF grPose_GetJointAttachment(const grPose *P,int JointIndex, grXForm3d *AttachmentTransform)
{
	assert( P != NULL );
	assert( AttachmentTransform != NULL );
	{
		const grPose_Joint *J;
		J = grPose_JointByIndex(P, JointIndex);
		*AttachmentTransform = J->AttachmentTransform;
	}
}

void GRCF grPose_SetJointAttachment(grPose *P,
	int JointIndex, 
	const grXForm3d *AttachmentTransform)
{
	assert( P != NULL );
	assert( AttachmentTransform != NULL );
	{
		grPose_Joint *J;
		J = (grPose_Joint *)grPose_JointByIndex(P, JointIndex);
		grQuaternion_FromMatrix(AttachmentTransform,&(J->AttachmentRotation));
		J->Touched = GR_TRUE;
		J->AttachmentTransform = *AttachmentTransform;
		J->UnscaledAttachmentTranslation = J->AttachmentTransform.Translation;
		grPose_SetAttachmentRotationFlag(J);
	}
	P->Touched = GR_TRUE;
}

void GRCF grPose_GetJointTransform(const grPose *P, int JointIndex,grXForm3d *Transform)
{
	assert( P != NULL );
	assert( Transform != NULL );
	
	grPose_UpdateRelativeToParent((grPose *)P);
	
	{
		const grPose_Joint *J;
		J = grPose_JointByIndex(P, JointIndex);
		*Transform = *(J->Transform);
	}
}

void GRCF grPose_GetJointLocalTransform(const grPose *P, int JointIndex,grXForm3d *Transform)
{
	assert( P != NULL );
	assert( Transform != NULL );
	{
		const grPose_Joint *J;
		J = grPose_JointByIndex(P, JointIndex);
		grQuaternion_ToMatrix(&(J->LocalRotation), Transform);
		Transform->Translation = J->LocalTranslation;
	}
}

void GRCF grPose_SetJointLocalTransform(grPose *P, int JointIndex,const grXForm3d *Transform)
{
	assert( P != NULL );
	assert( Transform != NULL );
	{
		grPose_Joint *J;
		J = (grPose_Joint *)grPose_JointByIndex(P, JointIndex);
		grQuaternion_FromMatrix(Transform,&(J->LocalRotation));
		J->LocalTranslation = Transform->Translation;
		J->Touched = GR_TRUE;
	}
	P->Touched = GR_TRUE;
}

int GRCF grPose_GetJointCount(const grPose *P)
{
	assert( P != NULL );
	assert( P->JointCount >= 0 );
	
	return P->JointCount;
}

grBoolean GRCF grPose_MatchesMotionExactly(const grPose *P, const grMotion *M)
{
	if (grMotion_HasNames(M) != GR_FALSE)
		{
			if (grMotion_GetNameChecksum(M) == P->NameChecksum)
				return GR_TRUE;
			else
				return GR_FALSE;
		}
	return GR_FALSE;
}

// sets pose to it's base position: applies no modifier to the joints: only
// it's attachment positioning is used.
void GRCF grPose_Clear(grPose *P,const grXForm3d *Transform)
{
	int i;
	grPose_Joint *J;
	
	assert( P != NULL );
	assert( P->JointCount >= 0 );
	P->OnlyThisJoint = GR_POSE_ROOT_JOINT-1;		// calling this function disables one-joint optimizations
	if (P->Parent==NULL)
		{
			if (Transform!=NULL)
				{
					grQuaternion_FromMatrix(Transform,&(P->RootJoint.LocalRotation));
					P->RootJoint.LocalTranslation = Transform->Translation;
				}
			P->RootJoint.Touched = GR_TRUE;
		}
			
	for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
		{
			grVec3d_Set( (&J->LocalTranslation),0.0f,0.0f,0.0f);
			grQuaternion_SetNoRotation(&(J->LocalRotation));
			assert( J->ParentJoint < i);
			P->Touched = GR_TRUE;
		}	
	P->Touched = GR_TRUE;
}	

void GRCF grPose_SetMotion(grPose *P, const grMotion *M, grFloat Time,
							const grXForm3d *Transform)
{
	grBoolean NameBinding;
	int i;
	grPose_Joint *J;
	grXForm3d RootTransform;
	
	assert( P != NULL );

	P->OnlyThisJoint = GR_POSE_ROOT_JOINT-1;		// calling this function disables one-joint optimizations

    if (P->Parent==NULL)
    {
        grBoolean SetRoot = GR_FALSE;
        if (grMotion_GetTransform(M,Time,&RootTransform)!=GR_FALSE)
        {
            SetRoot = GR_TRUE;

            if ( Transform != NULL )
            {
                grXForm3d_Multiply(Transform,&RootTransform,&RootTransform);
            }
        }
        else
        {
            if ( Transform != NULL )
            {
                SetRoot = GR_TRUE;
                RootTransform = *Transform;
            }
        }

        if (SetRoot != GR_FALSE)
        {
            grQuaternion_FromMatrix(&RootTransform,&(P->RootJoint.LocalRotation));
            P->RootJoint.LocalTranslation = RootTransform.Translation;
            P->RootJoint.Touched = GR_TRUE;
        }
    }

    if (M==NULL)
    {
        return;
    }

	if (grPose_MatchesMotionExactly(P,M)==GR_TRUE)
		NameBinding = GR_FALSE;
	else
		NameBinding = GR_TRUE;

	P->Touched = GR_TRUE;

#pragma message("could optimize this by looping two ways (min(jointcount,pathcount))")
	for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
    {
        if (NameBinding == GR_FALSE)
        {
            grMotion_SampleChannels(M,i,Time,&(J->LocalRotation),&(J->LocalTranslation));
        }
        else
        {
            if (grMotion_SampleChannelsNamed(M,
                grStrBlock_GetString(P->JointNames,i),
                Time,&(J->LocalRotation),&(J->LocalTranslation))==GR_FALSE)
                continue;

        }
        J->Touched = GR_TRUE;
        J->LocalTranslation.X *= P->Scale.X;
        J->LocalTranslation.Y *= P->Scale.Y;
        J->LocalTranslation.Z *= P->Scale.Z;
    }
}

static void GRCF grPose_SetMotionForABoneRecursion(grPose *P, const grMotion *M, grFloat Time,
							int BoneIndex,grBoolean NameBinding)
{
	grPose_Joint *J;
	grBoolean Touched = GR_FALSE;
	assert(P!=NULL);
	assert(M!=NULL);
	assert( BoneIndex >= 0);

	J=&(P->JointArray[BoneIndex]);

	if (NameBinding == GR_FALSE)
		{
			grMotion_SampleChannels(M,BoneIndex,Time,&(J->LocalRotation),&(J->LocalTranslation));
			Touched = GR_TRUE;
		}
	else
		{
			if (grMotion_SampleChannelsNamed(M,
				grStrBlock_GetString(P->JointNames,BoneIndex),
				Time,&(J->LocalRotation),&(J->LocalTranslation))!=GR_FALSE)
				Touched = GR_TRUE;
		}
	if (Touched != GR_FALSE)
		{
			J->Touched = GR_TRUE;
			J->LocalTranslation.X *= P->Scale.X;
			J->LocalTranslation.Y *= P->Scale.Y;
			J->LocalTranslation.Z *= P->Scale.Z;
		}
	if (J->ParentJoint != GR_POSE_ROOT_JOINT)
		grPose_SetMotionForABoneRecursion(P,M,Time,J->ParentJoint,NameBinding);
	
}

void GRCF grPose_SetMotionForABone(grPose *P, const grMotion *M, grFloat Time,
							const grXForm3d *Transform,int BoneIndex)
{
	grBoolean NameBinding;
	grXForm3d RootTransform;
	
	assert( P != NULL );
	//assert( M != NULL );
	P->OnlyThisJoint = BoneIndex;		// calling this function enables single-joint optimizations
	
	if (P->Parent==NULL)
		{
			grBoolean SetRoot = GR_FALSE;
			if (grMotion_GetTransform(M,Time,&RootTransform)!=GR_FALSE)
				{
					SetRoot = GR_TRUE;

					if ( Transform != NULL )
						{
							grXForm3d_Multiply(Transform,&RootTransform,&RootTransform);
						}
				}
			else
				{
					if ( Transform != NULL )
						{
							SetRoot = GR_TRUE;
							RootTransform = *Transform;
						}
				}

			if (SetRoot != GR_FALSE)
				{
					grQuaternion_FromMatrix(&RootTransform,&(P->RootJoint.LocalRotation));
					P->RootJoint.LocalTranslation = RootTransform.Translation;
					P->RootJoint.Touched = GR_TRUE;
				}
		}

	if (M==NULL)
		{
			return;
		}
	if (BoneIndex == GR_POSE_ROOT_JOINT)
		{
			return;
		}

	if (grPose_MatchesMotionExactly(P,M)==GR_TRUE)
		NameBinding = GR_FALSE;
	else
		NameBinding = GR_TRUE;

	P->Touched = GR_TRUE;

	grPose_SetMotionForABoneRecursion(P, M, Time, BoneIndex, NameBinding);
}
	



#define LINEAR_BLEND(a,b,t)  ( (t)*((b)-(a)) + (a) )	
			// linear blend of a and b  0<t<1 where  t=0 ->a and t=1 ->b



void GRCF grPose_BlendMotion(	
	grPose *P, const grMotion *M, grFloat Time,
	const grXForm3d *Transform,
	grFloat BlendAmount, grPose_BlendingType BlendingType)
{
	int i;
	grBoolean NameBinding;
	grPose_Joint *J;
	grQuaternion R1;
	grVec3d      T1;
	grXForm3d    RootTransform;
	
	assert( P != NULL );
	//assert( M != NULL );  // M can be NULL
	assert( BlendingType == GR_POSE_BLEND_HERMITE || BlendingType == GR_POSE_BLEND_LINEAR);
	assert( BlendAmount >= 0.0f );
	assert( BlendAmount <= 1.0f );

	P->OnlyThisJoint = GR_POSE_ROOT_JOINT-1;		// calling this function disables one-joint optimizations
	if (BlendingType == GR_POSE_BLEND_HERMITE)
		{
			grFloat t2,t3;
			t2 = BlendAmount * BlendAmount;
			t3 = t2 * BlendAmount;
			BlendAmount = t2*3.0f -t3-t3;
		}

	if (P->Parent==NULL)
		{
			grBoolean SetRoot = GR_FALSE;
			if (grMotion_GetTransform(M,Time,&RootTransform)!=GR_FALSE)
				{
					SetRoot = GR_TRUE;

					if ( Transform != NULL )
						{
							grXForm3d_Multiply(Transform,&RootTransform,&RootTransform);
						}
				}
			else
				{
					if ( Transform != NULL )
						{
							SetRoot = GR_TRUE;
							RootTransform = *Transform;
						}
				}

			if (SetRoot != GR_FALSE)
				{
					grQuaternion_FromMatrix(&RootTransform,&R1);
					T1 = RootTransform.Translation;
					J  = &(P->RootJoint);
					grQuaternion_Slerp(&(J->LocalRotation),&(R1),BlendAmount,&(J->LocalRotation));
					{
						grVec3d      *LT = &(J->LocalTranslation);
						LT->X = LINEAR_BLEND(LT->X,T1.X,BlendAmount);
						LT->Y = LINEAR_BLEND(LT->Y,T1.Y,BlendAmount);
						LT->Z = LINEAR_BLEND(LT->Z,T1.Z,BlendAmount);
					}
					J->Touched = GR_TRUE;
				}
		}

	if (M==NULL)
		{
			return;
		}

	
	if (grPose_MatchesMotionExactly(P,M)==GR_TRUE)
		NameBinding = GR_FALSE;
	else
		NameBinding = GR_TRUE;
	
	P->Touched = GR_TRUE;

	for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
		{
			//grPath *JointPath;
							
			if (NameBinding == GR_FALSE)
				{
					grMotion_SampleChannels(M,i,Time,&R1,&T1);
					//JointPath = grMotion_GetPath(M,i);
					//assert( JointPath != NULL );
				}
			else
				{
					//JointPath = grMotion_GetPathNamed(M, grStrBlock_GetString(P->JointNames,i));
					//if (JointPath == NULL)
					//	continue;
					if (grMotion_SampleChannelsNamed(M,
						grStrBlock_GetString(P->JointNames,i),
						Time,&R1,&T1)==GR_FALSE)
						continue;

				}
			J->Touched = GR_TRUE;

			//grPath_SampleChannels(JointPath,Time,&(R1),&(T1));
			
			T1.X *= P->Scale.X;
			T1.Y *= P->Scale.Y;
			T1.Z *= P->Scale.Z;
			
			grQuaternion_Slerp(&(J->LocalRotation),&(R1),BlendAmount,&(J->LocalRotation));
						
			{
				grVec3d      *LT = &(J->LocalTranslation);
				LT->X = LINEAR_BLEND(LT->X,T1.X,BlendAmount);
				LT->Y = LINEAR_BLEND(LT->Y,T1.Y,BlendAmount);
				LT->Z = LINEAR_BLEND(LT->Z,T1.Z,BlendAmount);
			}
		}
}

const char* GRCF grPose_GetJointName(const grPose* P, int JointIndex)
{
	return grStrBlock_GetString(P->JointNames, JointIndex);
}

const grXFArray * GRCF grPose_GetAllJointTransforms(const grPose *P)
{
	assert( P != NULL );

	grPose_UpdateRelativeToParent((grPose *)P);
	return P->TransformArray;
}

void GRCF grPose_GetScale(const grPose *P, grVec3d *Scale)
{
	assert( P     != NULL );
	assert( Scale != NULL );
	*Scale = P->Scale;
}
	

void GRCF grPose_SetScale(grPose *P, const grVec3d *Scale )
{
	assert( P != NULL );
	assert( grVec3d_IsValid(Scale) != GR_FALSE );

	{
		int i;
		grPose_Joint *J;

		P->Scale = *Scale;
		//grVec3d_Set(&(P->Scale),ScaleX,ScaleY,ScaleZ);

		for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
			{	
				J->AttachmentTransform.Translation.X = J->UnscaledAttachmentTranslation.X * Scale->X;
				J->AttachmentTransform.Translation.Y = J->UnscaledAttachmentTranslation.Y * Scale->Y;
				J->AttachmentTransform.Translation.Z = J->UnscaledAttachmentTranslation.Z * Scale->Z;
				//J->AttachmentTransform.Translation = J->AttachmentTranslation;
				J->Touched = GR_TRUE;
			}
		P->Touched = GR_TRUE;
	}
}

void GRCF grPose_ClearCoverage(grPose *P, int ClearTo)
{
	int i;
	grPose_Joint *J;

	assert( P != NULL );
	assert( (ClearTo == GR_FALSE) || (ClearTo == GR_TRUE) );

	for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
		{	
			J->Covered = ClearTo;
		}
}

int GRCF grPose_AccumulateCoverage(grPose *P, const grMotion *M, grBoolean QueryOnly)
{
	int i,SubMotions;
	grBoolean NameBinding;
	int Covers=0;
	grPose_Joint *J;
	
	assert( P != NULL );
	if (M==NULL)
		{
			return P->JointCount;
		}

	SubMotions = grMotion_GetSubMotionCount(M);
	if (SubMotions>0)
		{
			for (i=0; i<SubMotions; i++)
				{
					int c = grPose_AccumulateCoverage(P, grMotion_GetSubMotion(M,i),QueryOnly);
					if (c > Covers)
						{
							Covers = c;
						}
				}
			return Covers;
		}
	
	if (grPose_MatchesMotionExactly(P,M)==GR_TRUE)
		NameBinding = GR_FALSE;
	else
		NameBinding = GR_TRUE;

	for (i=0, J=&(P->JointArray[0]); i<P->JointCount; i++,J++)
		{
			grPath *JointPath;
			if (J->Covered == GR_FALSE)
				{
					if (NameBinding == GR_TRUE)
						{
							JointPath = grMotion_GetPathNamed(M, grStrBlock_GetString(P->JointNames,i));
							if (JointPath == NULL)
								continue;
						}
					if (QueryOnly == GR_FALSE)
						{
							J->Covered = GR_TRUE;
						}
					Covers ++;
				}
		}
	return Covers;
}
