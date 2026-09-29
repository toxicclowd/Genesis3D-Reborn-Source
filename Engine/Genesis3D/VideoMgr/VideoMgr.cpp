#include "VideoMgr_h.h"
#include "assert.h"


//	DEFINES AND DECLARATIONS	==============================================

static VIDEOMEDIA VideoMedia;

//
// ChangeStateTo
//
void ChangeStateToVideo( VideoState newState )
{
    VideoMedia.state = newState;
}

grBoolean InitVideoMgr(HWND mainwindowhandle)
{
    // Filter interface initialize?
    if( SUCCEEDED( CoInitialize( NULL )))
		return GR_TRUE;
	
    return GR_FALSE;
}

grBoolean InitVideoMedia(HWND mainwindowhandle)
{
    ChangeStateToVideo( UninitializedV );
	
    VideoMedia.hGraphNotifyEvent = NULL;
    //VideoMedia.pGraph = NULL;
	
    return GR_TRUE;
}


//
//	UnInitVideoMgr
//	Unloads VideoMgr
//
void UnInitVideoMgr(HWND mainwindowhandle)
{
    CoUninitialize( );
}

//
//	VideoMgr
//

GRAPI grVidMgr * GRCC grVideo_CreateManager(HWND mainwindowhandle)
{
	grVidMgr *VideoMgr;
	
	VideoMgr = GR_RAM_ALLOCATE_STRUCT(grVidMgr);
	
	memset(VideoMgr, 0, sizeof(grVidMgr));
	
	// Initialise COM and the application
    InitVideoMgr(mainwindowhandle);
	
	InitVideoMedia(mainwindowhandle);

	VideoMgr->numvids = 0;
	VideoMgr->curvid = 0;
	VideoMgr->mwh = mainwindowhandle;

	return(VideoMgr);
		
}	// VideoMgr

GRAPI grBoolean GRCC grVideo_DestroyManager(grVidMgr **VideoMgr)
{
	//destroys the VideoManager
	grVidMgr *	Vid;

	assert(VideoMgr);

	Vid = *VideoMgr;

	if ( Vid )
	{
		if (VideoMedia.pimc)
			VideoMedia.pimc->Stop();
		
		if (VideoMedia.pivw)
		{
			// Relinquish ownership (IMPORTANT!) after hiding
			VideoMedia.pivw->put_Visible(0);
			VideoMedia.pivw->put_Owner((OAHWND)NULL);
			HELPER_RELEASE(VideoMedia.pivw);
		} // Relinquish ownership (IMPORTANT!) after hiding

		HELPER_RELEASE(VideoMedia.pif);
		HELPER_RELEASE(VideoMedia.pimc);
		HELPER_RELEASE(VideoMedia.pGraph);
		HELPER_RELEASE(VideoMedia.pimex);

		grRam_Free(Vid);
		//*Vid = NULL;
		return GR_TRUE;

	}

return GR_FALSE;
}


//
// CanPlayVideo
//
// Return true if we can go to a playing state from our current state
//
BOOL CanPlayVideo()
{
    return (VideoMedia.state == StoppedV || VideoMedia.state == PausedV);
}


//
// CanStop
//
// Return true if we can go to a stopped state from our current state
//
BOOL CanStopVideo()
{
    return (VideoMedia.state == PlayingV || VideoMedia.state == PausedV);
}


//
// IsInitialized
//
// Return true if we have loaded and initialized a multiVideoMedia file
//
BOOL IsInitializedVideo()
{
    return (VideoMedia.state != UninitializedV);
}


// Destruction
//
// DeleteContents
//
void DeleteContentsVideo()
{
    if (VideoMedia.pGraph != NULL) 
	{
		VideoMedia.pGraph->Release();
		VideoMedia.pGraph = NULL;
    }
	
    // this event is owned by the filter graph and is thus invalid
    VideoMedia.hGraphNotifyEvent = NULL;
	
    ChangeStateToVideo( UninitializedV );
	
}	//	Delete Contents

//
//	OpenVideoFile
//	You may specify any of the following video file types:
//	*.avi, *.mpg, *.mpeg, *.mov, *.qt
//
GRAPI void GRCC grVideo_Open(grVidMgr *VidMgr, LPSTR szFile )
{
    //if( szFile != NULL && RenderFileVideo( szFile ))
    //{
	
	ChangeStateToVideo( StoppedV );
	VidMgr->numvids++;
	VidMgr->files[VidMgr->numvids].szFileName = szFile;

    //}

} // OpenVideoFile



//
// PlayVideo
//

GRAPI void GRCC grVideo_Play (grVidMgr *VidMgr, int vid)
{
	WCHAR wFile[MAX_PATH];
    LPSTR szFile;
	HRESULT   hr;

	RECT      grc;
	  
	szFile = VidMgr->files[vid].szFileName;
	  
	MultiByteToWideChar( CP_ACP, 0, szFile, -1, wFile, MAX_PATH );

    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder,	(void **) &VideoMedia.pGraph);

    if (SUCCEEDED(hr))

    { // Graphbuilder instance
		
		// QueryInterface for some basic interfaces
		hr = VideoMedia.pGraph->QueryInterface(IID_IMediaControl, (void **) &VideoMedia.pimc);
		hr = VideoMedia.pGraph->QueryInterface(IID_IMediaEventEx, (void **) &VideoMedia.pimex);
		hr = VideoMedia.pGraph->QueryInterface(IID_IVideoWindow, (void **) &VideoMedia.pivw);
        
		// Have the graph construct its the appropriate graph automatically
		hr = VideoMedia.pGraph->RenderFile(wFile, NULL);
        
        VideoMedia.pivw->put_Owner((OAHWND)VidMgr->mwh);
		VideoMedia.pivw->put_WindowStyle(WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);

        // Have the graph signal event via window callbacks for performance
        VideoMedia.pimex->SetNotifyWindow((OAHWND)VidMgr->mwh, JEMSG_VIDEO_NOTIFY, 0);
		
        GetClientRect(VidMgr->mwh, &grc);
        VideoMedia.pivw->SetWindowPosition(grc.left, grc.top, grc.right, grc.bottom);

        // Run the graph if RenderFile succeeded
        if (SUCCEEDED(hr))
          VideoMedia.pimc->Run();
		
      } // Graphbuilder instance

} // Play //

//
// StopVideo
//
//

void StopVideo()
{
	HRESULT hr;
	IMediaControl *pMC;
	
	// Obtain the interface to our filter graph
	hr = VideoMedia.pGraph->QueryInterface(IID_IMediaControl, 
		(void **) &pMC);
	if( SUCCEEDED(hr) )
	{
		pMC->Stop();	//	stop it!
		pMC->Release();
		ChangeStateToVideo( StoppedV );
	}
	
	return ;
} //StopVideo


//
// GetGraphEventHandle
//
// We use this to check for graph events
//
HANDLE GetGraphEventHandleVideo()
{
    return VideoMedia.hGraphNotifyEvent;
	
} // GetGraphEventHandle


//
// OnGraphNotify
//
// If the event handle is valid, then ask the graph if
// anything has happened (eg the graph has stopped...)
//

GRAPI void GRCC grVideo_Notify()
{
	
	long      evCode;
	long      evParam1;
	long      evParam2;

	HRESULT hr;
	if(VideoMedia.pimex)
	{
	while (SUCCEEDED(VideoMedia.pimex->GetEvent(&evCode, &evParam1, &evParam2, 0)))
    { // Spin through the events
		
		hr = VideoMedia.pimex->FreeEventParams(evCode, evParam1, evParam2);
		if (EC_COMPLETE == evCode)
        { // Finished
			if (VideoMedia.pivw)
			{ // Relinquish ownership (IMPORTANT!) after hiding
				
				//VideoMedia.pimc->Stop(VideoMedia.pimc);

				VideoMedia.pivw->put_Visible(GR_FALSE);
				//VideoMedia.pivw->put_Owner(VideoMedia.pivw, (OAHWND)NULL);
								
                HELPER_RELEASE(VideoMedia.pivw);
				HELPER_RELEASE(VideoMedia.pif);
				HELPER_RELEASE(VideoMedia.pimc);
				HELPER_RELEASE(VideoMedia.pGraph);
				HELPER_RELEASE(VideoMedia.pimex);

			}
			
			break;
		}// Finished
	}
	}
}


/*===================================================================================
VIDEOPLAYING
This function checks to see if the current Video file is still playing.
=====================================================================================*/
GRAPI grBoolean GRCC grVideo_IsPlaying()
{
	HRESULT				hr;
	IMediaPosition		*pMP;
	//	query the interface
	if(VideoMedia.pGraph)
	{
		hr = VideoMedia.pGraph->QueryInterface(IID_IMediaPosition,
		(void**) &pMP);
				if (SUCCEEDED(hr)) 
				{
					REFTIME tCurrent, tLength;		//	find the max playtime
					hr = pMP->get_Duration(&tLength);
					if (SUCCEEDED(hr)) 
					{
						hr = pMP->get_CurrentPosition(&tCurrent);
						if (SUCCEEDED(hr)) 
						{
							
							//	Test to see if there is any time left
							if ((tLength - tCurrent) > 0) 
							{
								pMP->Release();	//	release our access to the interface
								return GR_TRUE;	// if so, still playing, buddy.
							}
							else
							{
								pMP->Release();	//	release our access to the interface
								return GR_FALSE;
							}
						}
					}
				}
		}

				
				return GR_FALSE;	//	Video file is all done playing.				
}	//VideoPlaying
