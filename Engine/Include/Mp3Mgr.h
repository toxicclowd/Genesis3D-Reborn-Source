//grMp3Mgr - CyRiuS

#ifndef Mp3Mgr_H
#define Mp3Mgr_H

#include "Genesis3D.h"
#include <windows.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MP3			1024
#define MP3_LOAD_SUCCESS	0
#define MP3_LOAD_FAIL	1
	

typedef struct grMp3_Def_
{
	LPSTR			szFileName;
	grBoolean		isCutList; //Not yet implemented
	// add other stuff here later
} grMp3_Def;
	
typedef struct //the grMp3Mgr holds info about the vids you wanna play. files[] allows easy access
{			   //to all of your vids
	int				num_mp3s;
	int				cur_mp3;
	grMp3_Def		files[MAX_MP3];
	HWND			mwh;

} grMp3Mgr;

GRAPI grMp3Mgr * GRCC grMp3_CreateManager(HWND mainwindowhandle);
GRAPI grBoolean GRCC grMp3_DestroyManager(grMp3Mgr **Mp3Mgr);

#ifdef __cplusplus
}
#endif

#endif

