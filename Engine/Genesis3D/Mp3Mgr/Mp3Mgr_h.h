//Mp3Mgr
//It lets your app play MP3s

#ifndef Mp3MgrH_H
#define Mp3MgrH_H

//includes
#include "Genesis3D.h"
#include "Mp3Mgr.h"
#include <windows.h>
#include <string.h>
#include <evcode.h>
#include <objbase.h>
#include <strmif.h>
#include <control.h>
#include <uuids.h>
#include "Ram.h"
#include "Errorlog.h"
#include <direct.h>	


#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagState {Uninitialized, Stopped, Paused, Playing } State;

typedef struct tagMedia
{
    State				state;
    IGraphBuilder		*pGraph;
    HANDLE				hGraphNotifyEvent;
} Media;


//	FUNCTION PROTOTYPES	=================================================

grBoolean	InitMedia(HWND mainwindowhandle);
HANDLE		GetGraphEventHandle( void );


BOOL	CanPlay();
BOOL	CanStop();
BOOL	CanPause();
BOOL	IsInitialized();
void	DeleteContentsMp3();

// Event handlers
void		PlayMp3(long volume, grBoolean loop);
void		OnMediaPause();
void		StopMp3();
void		OnMediaAbortStop();
void		OnGraphNotify();
grBoolean	Mp3Playing();

#ifdef NDEBUG
  #define ASSERT( exp ) ((void)0)
#else
#define ASSERT( exp ) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
