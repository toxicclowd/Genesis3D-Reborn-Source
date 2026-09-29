/****************************************************************************************/
/*  POSE.H																				*/
/*                                                                                      */
/*  Author: Mike Sandige	                                                            */
/*  Description: Bone hierarchy interface.								.				*/
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
#ifndef GR_POSE_H
#define GR_POSE_H

/*	grPose

	This object is a hierarchical set of attached joints.  The joints can have names.
	A 'grPose' keeps track of which children joints move in the hierarchy when a parent
	joint moves.  A grPose also remembers the position transform matrices for each joint.

	The grPose is set by applying a motion at a specific time.  This queries the motion
	to determine each joint's change and applies them to the hierarchy.  Each joint can
	then be queried for it's world transform (for drawing, etc.)

	Additional motions can modify or be blended into the pose.  A motion that describes 
	only a few joint changes can be applied to only those joints, or a motion can be
	blended with the current pose. 

	Something to watch for:  since setting the pose by applying a motion is powerful
	enough to resolve intentionally mismatched motion-pose sets, this can lead to 
	problems if the motion UNintentionally does not match the pose.  Use 
	grPose_MatchesgrMotionExactly() to test for an exact name-based match.
	

*/

#include <stdio.h>
#include "Motion.h"
#include "XFArray.h"

#ifdef __cplusplus
extern "C" {
#endif


#define GR_POSE_ROOT_JOINT (-1)

typedef enum 
{
		GR_POSE_BLEND_LINEAR,
		GR_POSE_BLEND_HERMITE
} grPose_BlendingType;

typedef struct grPose grPose;

	// Creates a new pose with no joints.
grPose *GRCF grPose_Create(void);

	// Destroys an existing pose.
void GRCF grPose_Destroy(grPose **PM);

	// Adds a new joint to a pose.
grBoolean GRCF grPose_AddJoint(
	grPose *P,
	int ParentJointIndex,
	const char *JointName,
	const grXForm3d *Attachment,
	int *JointIndex);


void GRCF grPose_GetScale(const grPose *P, grVec3d *Scale);
	// Retrieves current joint attachment scaling factors

void GRCF grPose_SetScale(grPose *P, const grVec3d *Scale);
	// Scales all joint attachments by component scaling factors in Scale

	// Returns the index of a joint named JointName.  Returns GR_TRUE if it is
	// located, and Index is set.  Returns GR_FALSE if not, and Index is not changed.
grBoolean GRCF grPose_FindNamedJointIndex(const grPose *P, const char *JointName, int *Index);

	// returns the number of joints in the pose
int GRCF grPose_GetJointCount(const grPose *P);

grBoolean GRCF grPose_MatchesMotionExactly(const grPose *P, const grMotion *M);

void GRCF grPose_Clear(grPose *P, const grXForm3d *Transform);

	// set the pose according to a motion.  Use the motion at time 'Time'.
	// if the motion does not describe motion for all joints, name-based resolution
	// will be used to decide which motion to attach to which joints.
	// joints that are unaffected are unchanged.
	// if Transform is non-NULL, it is applied to the Motion
void GRCF grPose_SetMotion(grPose *P, const grMotion *M,grFloat Time,const grXForm3d *Transform);

	// optimization:  if this is called, then all pose computations are limited to the BoneIndex'th bone, and
	// it's parents (including the root bone).  This is true for all queries until an entire motion is set or blended
	// into the pose.
void GRCF grPose_SetMotionForABone(grPose *P, const grMotion *M, grFloat Time,
							const grXForm3d *Transform,int BoneIndex);


	// blend in the pose according to a motion.  Use the motion at time 'Time'.
	// the blending is between the 'current' pose and the pose described by the motion.
	// a BlendAmount of 0 will result in the 'current' pose, and a BlendAmount of 1.0
	// will result in the pose according to the new motion.
	// if the motion does not describe motion for all joints, name-based resolution
	// will be used to decide which motion to attach to which joints.
	// joints that are unaffected are unchanged.
	// if Transform is non-NULL, it is applied to the Motion prior to blending
void GRCF grPose_BlendMotion(grPose *P, const grMotion *M, grFloat Time, 
					const grXForm3d *Transform,
					grFloat BlendAmount, grPose_BlendingType BlendingType);

	// get a joint's current transform (relative to world space)
void GRCF grPose_GetJointTransform(const grPose *P, int JointIndex,grXForm3d *Transform);

	// get the transforms for the entire pose. *TransformArray must not be changed.
const grXFArray *GRCF grPose_GetAllJointTransforms(const grPose *P);

	// query a joint's current transform relative to it's attachment to it's parent.
void GRCF grPose_GetJointLocalTransform(const grPose *P, int JointIndex,grXForm3d *Transform);

	// adjust a joint's current transform relative to it's attachment to it's parent.
	//   this is like setting a mini-motion into this joint only:  this will only affect
	//   the current pose 
void GRCF grPose_SetJointLocalTransform(grPose *P, int JointIndex,const grXForm3d *Transform);

	// query how a joint is attached to it's parent. (it's base attachment)
void GRCF grPose_GetJointAttachment(const grPose *P,int JointIndex,grXForm3d *AttachmentTransform);

	// adjust how a joint is attached to it's parent.  These changes are permanent:  all
	//  future pose motions will incorporate this joint's new relation to it's parent */
void GRCF grPose_SetJointAttachment(grPose *P,int JointIndex,const grXForm3d *AttachmentTransform);

const char* GRCF grPose_GetJointName(const grPose* P, int JointIndex);

grBoolean GRCF grPose_Attach(grPose *Slave, int SlaveBoneIndex,
				  grPose *Master, int MasterBoneIndex, 
				  const grXForm3d *Attachment);

void GRCF grPose_Detach(grPose *P);

	// a pose can also maintain a record of which joints are touched by a given motion.
	// these funtions set,clear and query the record.
	// ClearCoverage clears the coverage flag for all joints 
void GRCF grPose_ClearCoverage(grPose *P, int ClearTo);
	// AccumulateCoverage returns the number of joints that are not already 'covered' 
	// that will be affected by a motion M,  
	// if QueryOnly is GR_FALSE, affected joints are tagged as 'covered', otherwise no changes
	// are made to the joint coverage flags.
int GRCF grPose_AccumulateCoverage(grPose *P, const grMotion *M, grBoolean QueryOnly);


#ifdef __cplusplus
}
#endif


#endif
