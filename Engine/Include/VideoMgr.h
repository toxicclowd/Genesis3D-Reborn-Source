//grVidMgr - CyRiuS

#ifndef VideoMgr_H
#define VideoMgr_H

#include "Genesis3D.h"
#include <windows.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VIDS	35
#define JEMSG_VIDEO_NOTIFY  WM_USER+13
	
	

typedef struct grVideo_Def_
{
	LPSTR szFileName;
	// add other stuff here later
} grVideo_Def;
	
typedef struct //the grVidMgr holds info about the vids you wanna play. files[] allows easy access
{			   //to all of your vids
	int				numvids;
	int				curvid;
	grVideo_Def		files[MAX_VIDS];
	HWND			mwh;

} grVidMgr;

GRAPI grVidMgr * GRCC grVideo_CreateManager(HWND mainwindowhandle);
GRAPI grBoolean GRCC grVideo_DestroyManager(grVidMgr **VideoMgr);
GRAPI void GRCC grVideo_Notify();

GRAPI void GRCC grVideo_Open(grVidMgr *VidMgr, LPSTR szFile );
GRAPI void GRCC grVideo_Play (grVidMgr *VidMgr, int vid);
GRAPI grBoolean GRCC grVideo_IsPlaying();

#ifdef __cplusplus
}
#endif

#endif

