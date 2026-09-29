/****************************************************************************************/
/*  CSNETMGR.H                                                                          */
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
#ifndef GR_CSNETMGR_H
#define GR_CSNETMGR_H

#include "BaseType.h"

#ifdef __cplusplus
extern "C" {
#endif


//================================================================================
//	Structure defines
//================================================================================

// GR_PUBLIC_APIS

typedef struct		grCSNetMgr	grCSNetMgr;

typedef uint32		grCSNetMgr_NetID;

// Types for messages received from _ReceiveSystemMessage
typedef enum 
{
	NET_MSG_USER,					// User message
	NET_MSG_CREATE_CLIENT,			// A new client has joined in
	NET_MSG_DESTROY_CLIENT,			// An existing client has left
	NET_MSG_HOST					// We are the server now
} grCSNetMgr_NetMsgType;

typedef struct
{
	char		Name[32];
	grCSNetMgr_NetID	Id;
} grCSNetMgr_NetClient;


#ifdef _INC_WINDOWS
	// Windows.h must be included previously for this api to be exposed.

	typedef struct grCSNetMgr_NetSession
	{
		char		SessionName[200];					// Description of Service provider
		GUID		Guid;								// Service Provider GUID
		#pragma message("define a grGUID?.. wouldn't need a windows dependency here...")
	} grCSNetMgr_NetSession;

GRAPI grBoolean		GRCC grCSNetMgr_FindSession(grCSNetMgr *M, const char *IPAdress, grCSNetMgr_NetSession **SessionList, int32 *SessionNum );
GRAPI grBoolean		GRCC grCSNetMgr_JoinSession(grCSNetMgr *M, const char *Name, const grCSNetMgr_NetSession* Session);
#endif

GRAPI grCSNetMgr *		GRCC grCSNetMgr_Create(void);
GRAPI void				GRCC grCSNetMgr_Destroy(grCSNetMgr **ppM);
GRAPI grCSNetMgr_NetID	GRCC grCSNetMgr_GetServerID(grCSNetMgr *M);
GRAPI grCSNetMgr_NetID	GRCC grCSNetMgr_GetOurID(grCSNetMgr *M);
GRAPI grCSNetMgr_NetID	GRCC grCSNetMgr_GetAllPlayerID(grCSNetMgr *M);
GRAPI grBoolean GRCC		grCSNetMgr_ReceiveFromServer(grCSNetMgr *M, uint32 *Type, int32 *Size, uint8 **Data);
GRAPI grBoolean GRCC		grCSNetMgr_ReceiveFromClient(grCSNetMgr *M, grCSNetMgr_NetMsgType *Type, grCSNetMgr_NetID *IdClient, int32 *Size, uint8 **Data);
GRAPI grBoolean GRCC		grCSNetMgr_ReceiveSystemMessage(grCSNetMgr *M, grCSNetMgr_NetID IdFor, grCSNetMgr_NetMsgType *Type, grCSNetMgr_NetClient *Client);
GRAPI grBoolean GRCC		grCSNetMgr_ReceiveAllMessages(grCSNetMgr *M, grCSNetMgr_NetID *IdFrom, grCSNetMgr_NetID *IdTo, grCSNetMgr_NetMsgType *Type, int32 *Size, uint8 **Data);
GRAPI grBoolean GRCC		grCSNetMgr_WeAreTheServer(grCSNetMgr *M);
GRAPI grBoolean GRCC		grCSNetMgr_StartSession(grCSNetMgr *M, const char *SessionName, const char *PlayerName );
GRAPI grBoolean GRCC		grCSNetMgr_StopSession(grCSNetMgr *M);
GRAPI grBoolean GRCC		grCSNetMgr_SendToServer(grCSNetMgr *M, grBoolean Guaranteed, uint8 *Data, int32 DataSize);
GRAPI grBoolean GRCC		grCSNetMgr_SendToClient(grCSNetMgr *M, grCSNetMgr_NetID To, grBoolean Guaranteed, uint8 *Data, int32 DataSize);


// GR_PRIVATE_APIS
#ifdef __cplusplus
}
#endif

#endif
