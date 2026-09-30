///////////////////////////////////////////////////////////////////////////////////
//	jMinApp ver. 1.0.b for Jet3D ver.2.x
//	July 23, 2002
//	by Tom Morris
//
//	The target for this application is PROGRAMMERS interested in  real time
//	3D rendering application development. jMinApp is an application shell that
//	exploits the rendering capabilities of Jet3D -- a real time rendering engine.
//	jMinApp demonstrates how to start your own custom applcation shell (game shell)
//	for Jet3D. jMinApp shows how to do this as an MFC application. This MFC approach
//	makes all the huge power of MFC available to you including a large variety
//	of container classes, serialization, document management, and much more...
//
//	You may freely modify this code to transform it into anything you want. Or use bits 
//	of it to build your own, unique shell.
//
//	Good luck and have fun.
//	-Tom
//	www.jet3d.com
/////////////////////////////////////////////////////////////////////////////////////


// MainFrm.cpp : implementation of the CMainFrame class
//

//	NOTE: this is just a CFrame class for creating and hosting
//	the JetView class. Otherwise, it does nothing else important
//	for Jet3D operations

#include "stdafx.h"
#include "MinApp.h"
#include "MainFrm.h"




//	timer for cycling render loops -- in milliseconds
#define TIMER_SPEED		40
//	width and height for windowed _DEBUG display -- in pixels
#define	DEBUG_WIDTH		400
#define	DEBUG_HEIGHT	300


//	for calcing radians from degrees -- for rotations
#define RAD(a) ((a)*0.0174524064372835128194189785163162f)	
// CMainFrame construction/destruction



// CJetView

////////////////////////////////////////////////////////////////////////////////
//	SubLarge
//	for timing calculations
////////////////////////////////////////////////////////////////////////////////
static void SubLarge(LARGE_INTEGER *start, LARGE_INTEGER *end, LARGE_INTEGER *delta)
{
	_asm 
	{
		mov ebx,dword ptr [start]
		mov esi,dword ptr [end]

		mov eax,dword ptr [esi+0]
		sub eax,dword ptr [ebx+0]

		mov edx,dword ptr [esi+4]
		sbb edx,dword ptr [ebx+4]

		mov ebx,dword ptr [delta]
		mov dword ptr [ebx+0],eax
		mov dword ptr [ebx+4],edx
	}
}





#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_CLOSE()
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()

	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_MOVE_FORWARD, OnMoveForward)
	ON_COMMAND(ID_MOVE_BACK, OnMoveBack)
	ON_COMMAND(ID_MOVE_LEFT, OnMoveLeft)
	ON_COMMAND(ID_MOVE_RIGHT, OnMoveRight)
	ON_WM_MOVE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
	//	*** initialize member variables ***	//

	m_bG3DInitializationDone = false;
	m_bReadyToRender = false;
	m_bShuttingDown = false;

m_strGameName = _T("Genesis3D_Reborn_MinApp");
	
	char		tempPathString[MAX_PATH+1];
	DWORD		dwcNameSize = MAX_PATH+1;
	::GetCurrentDirectory(dwcNameSize, tempPathString);
	m_strBaseDir = tempPathString;				

	m_strMaterialDir = _T("GlobalMaterials");			
	m_strActorDir = _T("Actors");				
	m_strSoundDir = _T("Sounds");				
	m_strLevelDir = _T("Levels");				
	m_strObjectDir = _T("objects");			

	m_pEngine = NULL;
	m_pvFileSys = NULL;
	m_pResourceMgr = NULL;
	m_pPtrMgr = NULL;
	m_pCamera = NULL;
	m_pvfMaterialFile = NULL;
	m_pvfActorFile = NULL;
	m_pvfSoundFile = NULL;
	m_pvfLevelFile = NULL;
	m_pDrvSys = NULL;
	m_pDriver = NULL;
	m_pMode = NULL;
	m_pSoundSys = NULL;
	m_pWorld = NULL;

	m_ptOldMousePoint.SetPoint(0,0);
	m_vecCameraPos.X = m_vecCameraPos.Y = m_vecCameraPos.Z = 0.0f;
	m_fSpeedMultiplier = 0.30f;

	m_bBrowseLevel = false;
	m_bJustMoved = true;


#ifdef NDEBUG
	m_strDriverName = _T("(D3D) DirectX 12");
	m_strDesiredMode = _T("");
#endif

#ifdef _DEBUG
	m_strDriverName = _T("(D3D) DirectX 12");
#endif

}

CMainFrame::~CMainFrame()
{
}


/*
BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	// TODO: Add your specialized code here and/or call the base class

	CCreateContext context;
	pContext = &context;

  
    // Assign custom view.

    pContext->m_pNewViewClass = RUNTIME_CLASS(CJetView);

    // Create the view.

	m_pwndView = new CJetView;

 	m_pwndView = (CJetView*)CreateView(pContext);
	if (!m_pwndView)
		return FALSE;

	if (!m_pwndView->ShowWindow(SW_SHOW))
		return FALSE;

	// remove the caption of this window:
		LONG style = ::GetWindowLong(m_pwndView->GetSafeHwnd(),GWL_STYLE);
		style&=~WS_CAPTION;
		style&=~WS_THICKFRAME;

		::SetWindowLong(m_pwndView->GetSafeHwnd(),GWL_STYLE,style);



	return CFrameWnd::OnCreateClient(lpcs, pContext);
}
*/

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect	rectMainView;
	int iScreenWidth = ::GetSystemMetrics(SM_CXFULLSCREEN);
	int iScreenHeight = ::GetSystemMetrics(SM_CYFULLSCREEN);

	rectMainView.top = 0;
	rectMainView.left = 0;
	rectMainView.bottom = iScreenHeight;
	rectMainView.right = iScreenWidth;


		m_rectMainView.SetRect(0, 0, iScreenWidth, iScreenHeight);


		//	modify the window attributes... one last time
		//windowed display

		m_Rect.Top = m_rectMainView.top;
		m_Rect.Left = m_rectMainView.left;
		m_Rect.Bottom = m_rectMainView.bottom;
		m_Rect.Right =  m_rectMainView.right;




/////

#ifdef NDEBUG	//	fullscreen

		int	iModeWidth, iModeHeight;

		if (iScreenWidth > 1600)
		{
			iModeWidth = 1600;
			iModeHeight = 1024;
		}
		else
		{
			iModeWidth = iScreenWidth;
			iModeHeight = iScreenHeight;
		}

		//	now change our res string to reflfect the monitor's native res
		m_strDesiredMode.Format(_T("%dx%dx16"), iModeWidth, iModeHeight);

		// remove the caption of this window:
		LONG style = ::GetWindowLong(m_hWnd,GWL_STYLE);
		style&=~WS_CAPTION;
		style&=~WS_THICKFRAME;
		::SetWindowLong(m_hWnd,GWL_STYLE,style);

		ShowWindow(SW_SHOWMAXIMIZED);
		ShowCursor(FALSE);

		m_Rect.Top = 0;
		m_Rect.Left = 0;
		m_Rect.Bottom = iScreenHeight -1;
		m_Rect.Right = iScreenWidth - 1;

		SetWindowPos(&wndTopMost, 0, 0,
			iScreenWidth,
			iScreenHeight,
			/*SWP_NOCOPYBITS | SWP_NOZORDER |*/ WS_EX_TOPMOST | SW_SHOWMAXIMIZED);	

#endif

////


	// create a JetView to occupy the client area of the frame
#ifdef _DEBUG	//	windowed
//	if (!m_wndView.CreateEx(WS_EX_TOPMOST, AfxRegisterWndClass(CS_DBLCLKS /*| CS_OWNDC*/ | CS_SAVEBITS, NULL, 
//		(HBRUSH)GetStockObject(NULL_BRUSH)), NULL, WS_CHILD | WS_VISIBLE /*| WS_POPUP */ | WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
//		CRect(0,10,0,10), this, 1000, NULL))
//	  CWnd* pWnd = new CJetView;
//   if (!m_wndView.CreateEx( WS_EX_TRANSPARENT /*| WS_EX_TOPMOST */ /*| WS_EX_TOOLWINDOW*/, AfxRegisterWndClass(CS_DBLCLKS  | CS_OWNDC| CS_SAVEBITS, NULL, 
//		(HBRUSH)GetStockObject(NULL_BRUSH)), NULL, WS_CHILD  |/*| WS_VISIBLE | /*WS_POPUP |*/ WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
 //    CRect(0,800,0,600), this, 1, NULL))



	
#endif
#ifdef NDEBUG	//	fullscreen
//	if (!m_wndView.CreateEx(WS_EX_TOPMOST, AfxRegisterWndClass(CS_DBLCLKS | CS_OWNDC | CS_SAVEBITS, NULL, 
//		(HBRUSH)GetStockObject(NULL_BRUSH)), NULL, WS_CHILD,
//		rectMainView, this, 1000, NULL))

//if (!m_wndView.CreateEx(WS_EX_TOPMOST, AfxRegisterWndClass(CS_DBLCLKS /*| CS_OWNDC*/ | CS_SAVEBITS, NULL, 
//		(HBRUSH)GetStockObject(NULL_BRUSH)), NULL, WS_CHILD | WS_VISIBLE /*| WS_POPUP*/  |WS_OVERLAPPEDWINDOW  | WS_MINIMIZE,
//		CRect(0,10,0,10), this, 1000, NULL))


#endif
//	{
//		TRACE0("Failed to create view window\n");
//		return -1;
//	}
//		HWND	hWndThis = NULL;
//		HWND	hWndView = NULL;

//	hWndThis = GetSafeHwnd();
//	hWndView = m_wndView.GetSafeHwnd();



	if (m_menuMain.LoadMenuA(IDR_MAINFRAME))
	{
		this->SetMenu(&m_menuMain);

	//	ShowMainMenu(false);
	}





if (this->IsWindowVisible())
{
//if (InitializeJet3D())

}
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	//	mods of this window's characteristics so we can't see it.
//	cs.hMenu = NULL;
//	cs.x = 0;
//	cs.y = 0;
//	cs.cx = 800;
//	cs.cy = 600;
#ifdef NDEBUG
	cs.style&=~WS_BORDER;
	cs.style&=~WS_CAPTION;
	cs.style&=~WS_THICKFRAME;
	cs.dwExStyle&=~WS_EX_CLIENTEDGE;
#endif
	//cs.lpszClass = AfxRegisterWndClass(0);
	cs.lpszClass = AfxRegisterWndClass(NULL, NULL, 
		(HBRUSH)GetStockObject(NULL_BRUSH));
	return TRUE;
}


// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG


// CMainFrame message handlers

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	// forward focus to the view window
//	m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
//	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))	//	needed in order for CJetView to handle keydowns
//		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}



void CMainFrame::OnClose()
{
	// TODO: Add your message handler code here and/or call default
		
	ShutdownAll();	//	stops timer
//#ifdef _DEBUG
//	AfxMessageBox(_T("jMinApp_Debug \n\nJet3D engine Shutdown and app closed without error."), MB_ICONINFORMATION);
//#endif

	CFrameWnd::OnClose();
}


void CMainFrame::OnDestroy()
{
	CFrameWnd::OnDestroy();

	// TODO: Add your message handler code here
//	ShutdownAll();	//	stops timer
}



void CMainFrame::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CFrameWnd::OnShowWindow(bShow, nStatus);

	// TODO: Add your message handler code here
}



void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);

	if (!m_bShuttingDown)
	{
		if (GetSafeHwnd())
		{
			if (!m_bG3DInitializationDone)
			{
				m_bG3DInitializationDone = true;			
				if (!InitializeG3D())
					m_bG3DInitializationDone = false;

			}
		}
	}
	// TODO: Add your message handler code here
}




/////////////////////////////////////////////////////////////////////////////
//	ShowMainMenu
//	
/////////////////////////////////////////////////////////////////////////////
bool CMainFrame::ShowMainMenu(bool bShow)
{
	if (!bShow)
	{
		CMenu*	pOldMenu = NULL;
		pOldMenu = GetMenu();
		if (pOldMenu)
		{
			m_menuMain.Attach(pOldMenu->Detach());
			if (SetMenu((CMenu*)NULL))
				return true;
		}
	}
	else
	{
		if (m_menuMain.GetSafeHmenu()!= NULL)
		{
			SetMenu(&m_menuMain);
			m_menuMain.Detach();
			return true;
		}
	}
	return false;
}





bool	CMainFrame::InitializeG3D()
{

//	CMainFrame	*pMainFrame = NULL;
//	pMainFrame = (CMainFrame*)AfxGetMainWnd();
	CRect	rectMainView;

	if (m_hWnd)
	{
		//	get the prefs from the G3DMinApp.ini and set prefs accordingly

//		if (pMainFrame)
		{
			if (!SetAppPreferences())
			{			
				AfxMessageBox("Failed to set app preferences.\n\n Shutting Down...");
				SendMessage(WM_CLOSE);
				return false;
			}

			//	if the BROWSE line is set to 'true' in G3DMinApp.ini,
			//	open a CFileDialog and browse for the level we want.
			if (m_bBrowseLevel)
			{
				CString		strBlankPath = _T("");
				CString		strBrowsePath = strBlankPath;

				strBrowsePath = BrowseForWorld();
				if (strBrowsePath != strBlankPath)
					m_strLevel = strBrowsePath;
			}

			if (!m_pEngine)	
			{	
				//	initialize the engine
//				if (!InitEngine(this->GetSafeHwnd()))
				if (!InitEngine(GetSafeHwnd()))
				{			
					AfxMessageBox("Failed to initialize Engine.\n\n Max. supported resolution is 1920 x 1080. \n\n Shutting Down...");
					SendMessage(WM_CLOSE);
					return false;
				}
			}

			//	register world objects located in the 'objects' dir
			if (!grEngine_RegisterObjects(m_strObjectDir.GetBuffer(m_strObjectDir.GetLength())))
			{			
				AfxMessageBox("Failed to register objects.\n\n Shutting Down...");
				SendMessage(WM_CLOSE);
				return false;
			}

			//	initialize the many different file systems
			if (!InitFileSystem(m_pEngine))
			{			
				AfxMessageBox("Failed to initialize file system.\n\n Shutting Down...");
				SendMessage(WM_CLOSE);
				return false;
			}

			//	load the world
			if (!LoadWorld())
			{
				AfxMessageBox("Failed to load world.\n\n Shutting Down...");
				//	ShutdownAll();
				SendMessage(WM_CLOSE);
				return false;
			}
		}	//	if (pMainFrame...

		//	start off with cursor at window center
		GetClientRect(&rectMainView);
		ClientToScreen(rectMainView);
		::SetCursorPos(rectMainView.Width()/2, rectMainView.Height()/2);

		//	initialize the cusor point member at screen center
		CPoint	point(0,0);
		GetCursorPos(&point);
		m_ptOldMousePoint = point;



//ShowWindow(SW_SHOW);

	

		//	start the timer (necessary with this style of MFC app).
		//	the timer triggers each render cycle -- sorta like the Main Loop.
		if (!StartTimer())
		{
			AfxMessageBox("Failed to start timer.\n\n Shutting Down...");
			//ShutdownAll();
			SendMessage(WM_CLOSE);
			return false;
		}
		return true;
	}	//	if (m_hWnd...


}



///////////////////////////////////////////////////////////////////////////////
//	SetAppPreferences
//	Reads the G3DMinApp.ini file for config settings
///////////////////////////////////////////////////////////////////////////////
bool	CMainFrame::SetAppPreferences()
{
	CString	pathString, iniString;
	char	tempPathString[MAX_PATH+1];
	DWORD	dwcNameSize = MAX_PATH+1;

	//	get the current path location
	::GetCurrentDirectory(dwcNameSize, tempPathString);
	pathString = tempPathString;
	iniString = pathString;
	iniString += _T("\\G3DMinApp.ini");

	CStdioFile		file;			//	treat G3DMinApp.ini as a CSdtioFile
	CFileException	exception;		//	our exception receiver

	//	try to open G3DMinApp.ini				
	if (file.Open(iniString, CFile::modeRead, &exception))
	{
		CString	tempString = _T("");	//	initialize, just to be safe

		BOOL	stillMoreFile = TRUE;	//	for EOF checking

		while (stillMoreFile)
		{	
			//	read file one-line-at-a-time
			stillMoreFile = file.ReadString(tempString);	
			if (stillMoreFile)
			{
				//	when we find the [LOAD_LEVEL] line...
				if (tempString.Find("[LOAD_LEVEL]") != (-1))
				{	
					//	when we find the LEVEL line...
					stillMoreFile = file.ReadString(tempString);
					if ((stillMoreFile) && (tempString.Find(_T("LEVEL")) != (-1)))
					{
						tempString.TrimRight();					//	trim excess fat
						int i = tempString.Find("=");			//	locate the =
						CString getString = tempString.Mid(i+1);//	extract the value
	
						//	set the level variable accordingly
						m_strLevel = m_strLevelDir + _T("\\") + getString;
					}
				}

				//	when we find the [BROWSE_LEVEL] line...
				if (tempString.Find("[BROWSE_LEVEL]") != (-1))
				{						//	when we find the BROSWE line...
					stillMoreFile = file.ReadString(tempString);
					if ((stillMoreFile) && (tempString.Find(_T("BROWSE")) != (-1)))
					{
						tempString.TrimRight();					//	trim excess fat
						int i = tempString.Find("=");			//	locate the =
						CString getString = tempString.Mid(i+1);//	extract the value

						//	set the option for level browsing
						if (getString.Find("true") != (-1))
							m_bBrowseLevel = true;
						if (getString.Find("false") != (-1))
							m_bBrowseLevel = false;
					}
				}

				//	DISPLAY SETTINGS NOW ARE SET AUTOMATICALLY AT NATIVE SCREEN
				//	RESOLUTION WITHOUT THE NECESSITY OF GETTING PREFS FROM THE *.INI
				//	when we find the [DISPLAY_SETTINGS] line...
//				if (tempString.Find("[DISPLAY_SETTINGS]") != (-1))
//				{						//	when we find the RESOLUTION_BIT_DEPTH line...
//					stillMoreFile = file.ReadString(tempString);
//					if ((stillMoreFile) && (tempString.Find(_T("RESOLUTION_BIT_DEPTH")) != (-1)))
//					{
//						tempString.TrimRight();					//	trim excess fat
//						int i = tempString.Find("=");			//	locate the =
//						CString getString = tempString.Mid(i+1);//	extract the value
//
//						m_strDesiredMode = getString;
//					}
//				}
			}
		}
		file.Close();
		return true;
	}
	else
		return false;
}



////////////////////////////////////////////////////////////////////////////////
//	ShutdownAll
//	Critical to prevent memory leaks and to prevent sys stability problems
////////////////////////////////////////////////////////////////////////////////				
bool CMainFrame::ShutdownAll()
{
	m_bShuttingDown = true;
	
	if (m_hWnd)
	{
		StopTimer();

		if (m_pEngine)
		{
			// stop it for now. we'll kill it later...
			grEngine_Activate(m_pEngine, FALSE);
		}

		//	destroy world, engine, etc...
		if (m_pWorld)
		{
			grWorld_Destroy(&m_pWorld);
		}

//		if (m_pResourceMgr)
//		{
//			grResource_MgrDestroy(&m_pResourceMgr);
//		}

		if (m_pCamera)
		{
			grCamera_Destroy(&m_pCamera);
		}

		if (m_pSoundSys)
		{
			grSound_DestroySoundSystem(m_pSoundSys);
		}

		if (m_pEngine)
		{	
			if (m_pDriver)
			{
				grEngine_ShutdownDriver(m_pEngine);
			}

			grEngine_Destroy(&m_pEngine);
		}

		//	destroy all the virtual file systems
//		if (m_pvFileSys)
//		{
//			grVFile_Close(m_pvFileSys);
//		}

		if (m_pvfMaterialFile)
		{
			grVFile_Close(m_pvfMaterialFile);
		}

		if (m_pvfActorFile)
		{
			grVFile_Close(m_pvfActorFile);
		}

		if (m_pvfSoundFile)
		{
			grVFile_Close(m_pvfSoundFile);
		}

		if (m_pvfLevelFile)
		{
			grVFile_Close(m_pvfLevelFile);
		}

		/////////////////
		if (m_pResourceMgr)
		{
			grResource_MgrDestroy(&m_pResourceMgr);
		}

		//	destroy all the virtual file systems
		if (m_pvFileSys)
		{
			grVFile_Close(m_pvFileSys);
		}
		////////////////////

		//	Destroy our pointer manager
		if (m_pPtrMgr)
		{
			grPtrMgr_Destroy(&m_pPtrMgr);
		}

		//	nullify all pointers at once -- just for style reasons
		m_pWorld = NULL;
		m_pEngine = NULL;
		m_pSoundSys = NULL;
		m_pDrvSys = NULL;
		m_pCamera = NULL;
		m_pDriver = NULL;
		m_pMode = NULL;
		m_pvfMaterialFile = NULL;
		m_pvfActorFile = NULL;
		m_pvfSoundFile = NULL;
		m_pvfLevelFile = NULL;
		m_pvFileSys = NULL;
		m_pResourceMgr = NULL;
		m_pPtrMgr = NULL;

		//	redisplay it (not really needed here, but good form)
		ShowCursor(TRUE);

		//	Goodbye

		return true;
	}
	return true;
}



////////////////////////////////////////////////////////////////////////////////
//	InitFileSystem
//	
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::InitFileSystem(grEngine* pEngine)
{
	//	main virtual file system
	//	NOTE: We are storing Dir string info in CString vars. But we cannot pass the
	//	CString vars directly because the params are supposed to be char types -- not CString types.
	//	So, we pass the BUFFER from the CString var. This IS a slight inconvenience. BUT overall,
	//	CString is such a powerful and elegant class (when compared to the char type),
	//	that the benefits far outweigh the difficulties.
	m_pvFileSys = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS,
		m_strBaseDir.GetBuffer(m_strBaseDir.GetLength()), NULL, GR_VFILE_OPEN_DIRECTORY);

	if (!m_pvFileSys)
		return false;

	//	create resource manager
	m_pResourceMgr = grResource_MgrCreate(pEngine);

	if (m_pResourceMgr)
	{	
		//	now, create specialized virtual file systems.

		m_pvfMaterialFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS,
			m_strMaterialDir.GetBuffer(m_strMaterialDir.GetLength()),
			NULL, GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);

		if (!m_pvfMaterialFile)
			return false;

		m_pvfSoundFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS,
			m_strSoundDir.GetBuffer(m_strSoundDir.GetLength()), NULL,
			GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);

		if (!m_pvfSoundFile)
			return false;

		m_pvfActorFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS,
			m_strActorDir.GetBuffer(m_strActorDir.GetLength()),
			NULL, GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);

		if (!m_pvfActorFile)
			return false;

		m_pvfLevelFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS,
			m_strLevelDir.GetBuffer(m_strLevelDir.GetLength()),
			NULL, GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);

		if (!m_pvfLevelFile)
			return false;

		//	add file systems to resourcemgr so the world loader 
		//	can resolve references in a world file
		if (!grResource_AddVFile(m_pResourceMgr,
			m_strMaterialDir.GetBuffer(m_strMaterialDir.GetLength()),
			m_pvfMaterialFile )) 
			return false;

		if( !grResource_AddVFile(m_pResourceMgr,
			m_strSoundDir.GetBuffer(m_strSoundDir.GetLength()),
			m_pvfSoundFile ))
			return false;

		if(!grResource_AddVFile(m_pResourceMgr, 
			m_strActorDir.GetBuffer(m_strActorDir.GetLength()),
			m_pvfActorFile))
			return false;

		if(!grResource_AddVFile(m_pResourceMgr,
			m_strLevelDir.GetBuffer(m_strLevelDir.GetLength()),
			m_pvfLevelFile))
			return false;

	}	//	if (m_pResourceMgr)...
	else
	{
		return false;
	}

	//	create pointer manager (for world loader)
	m_pPtrMgr = grPtrMgr_Create();

	if (!m_pPtrMgr)
		return false;
	else
		return true;
}



////////////////////////////////////////////////////////////////////////////////
//	InitEngine
//	
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::InitEngine(HWND hWnd)
{
	//	create our engine. Notice the way to pass CString buffers in place
	//	of char variable types.
	if (!m_pEngine)
	{
		m_pEngine = grEngine_Create(hWnd, (const char*)m_strGameName.GetBuffer(m_strGameName.GetLength()), (const char*)m_strBaseDir.GetBuffer(m_strBaseDir.GetLength()));
		if (!m_pEngine)
		{
			return false;
		}
		else
		{
			//	turn off the frame rate counter and display
			grEngine_EnableFrameRateCounter(m_pEngine, GR_FALSE);

			//	brighten things a bit
			grEngine_SetGamma(m_pEngine, 2.5f);
			if (!grEngine_Activate(m_pEngine, GR_FALSE))
				return false;
		}
	}

	//	now load a video driver and set display params
	if (!m_pDriver)
	{
		if (LoadDriver() == GR_FALSE)
		{
			return false;	
		}
	}

	//	create the camera 
	if (!m_pCamera)
	{
		//	set the FOV and give the camera a rendering rect
		m_pCamera = grCamera_Create(2.0f, &m_Rect);
	
		if (!m_pCamera)
		{
			return false;	
		}
	}

	//	now, the sound system
	if (!m_pSoundSys)
	{
		m_pSoundSys = grSound_CreateSoundSystem(hWnd);
		if (!m_pSoundSys)
		{
			return false;	
		}
	}

	//	initialize the transform and translation for the camera.  
	grXForm3d_SetIdentity(&m_xfCamera);
	m_xfCamera.Translation.Y = 40.0f;

	//	give the camera a transform so it has a place in the world
	if (!grCamera_SetXForm(m_pCamera, &m_xfCamera))
	{	
		return false;
	}
	return true;
}



///////////////////////////////////////////////////////////////////////////////
//	LoadDriver
//
///////////////////////////////////////////////////////////////////////////////
bool	CMainFrame::LoadDriver()
{
	long	tempwidth;
	long	tempheight;
	const	char	*drvname = NULL;	
	const	char	*ModeName = NULL;
	CString strModeName = _T("");

	if (m_pEngine)
	{
		if (!m_pDrvSys)
		{
			SetCurrentDirectory(m_strBaseDir);

			m_pDrvSys = grEngine_GetDriverSystem(m_pEngine);
			if (!m_pDrvSys)
				return false;
		}	//	if (!m_pDrvSys..

		if (!m_pDriver)	
		{
			m_pDriver = grDriver_SystemGetNextDriver(m_pDrvSys, NULL);

			if (!m_pDriver)
				return false;
		}// if(!m_pDriver...

		while (m_pDriver != NULL)
		{
			grDriver_GetName(m_pDriver, &drvname);

			//	if it's the driver name we want, then move to the next task...
			if (strstr(drvname, m_strDriverName.GetBuffer(m_strDriverName.GetLength())) != NULL)
			{
				break;
			}

			m_pDriver = grDriver_SystemGetNextDriver(m_pDrvSys, m_pDriver);
		}	//	while(m_pDriver...

		if (m_pDriver)
		{	
			if (!m_pMode)
			{	
				m_pMode = grDriver_GetNextMode(m_pDriver, NULL);
				while (m_pMode != NULL)
				{
					grDriver_ModeGetWidthHeight(m_pMode, &tempwidth, &tempheight);

#ifdef _DEBUG
					if ((tempwidth == -1) && (tempheight == -1))	//	windowed
						break;
#endif


#ifdef NDEBUG
					if ((tempwidth == -1) && (tempheight == -1))	//	windowed but looks fullscreen
						break;										//	because window fram is removed
																	//	also solves problems with large screens that are not
																	//	supported by the video drivers (modes aren't big enough)
//					grDriver_ModeGetName(m_pMode, &ModeName);
					
//					strModeName = (CString)ModeName;

//					if (strModeName == m_strDesiredMode)			//	Fullscreen	
//						break;
#endif
					m_pMode = grDriver_GetNextMode(m_pDriver, m_pMode);
				}	//	while (m_pMode...
			}	//	if (!m_pMode...

			if (m_pMode)
			{
				grBoolean	result;
				HWND		hWnd = NULL;
				
				hWnd = GetSafeHwnd();
				
				if (hWnd)
				{
					result = grEngine_SetDriverAndMode(m_pEngine, hWnd , m_pDriver, m_pMode);

					if (result == GR_FALSE)
					{
						return false;
					}
				}	//	if (hWnd...
				else
				{
					return false;
				}

				if (!grEngine_SetRenderMode(m_pEngine, RenderMode_TexturedAndLit))
				{
					return false;
				}
			}	//	if (m_pMode...
			else
			{
				return false;
			}
		}	//	if (m_pDriver...
		else
		{
			return false;
		}
	}	//	if (m_pEngine)...
	else
	{
		return false;
	}
	return true;
}



///////////////////////////////////////////////////////////////////////////////
//	BrowseForWorld
//	This is called if the BROWSE line of G3DMinApp.ini is set to true
///////////////////////////////////////////////////////////////////////////////
CString CMainFrame::BrowseForWorld() 
{
	//	we need to display the cursor while we're doing this operation 
	ShowCursor(TRUE);

	//	Setup a CFileDialog for browsing
	CString			strUserWorldFile, strBlank = _T("");	
	int				nItem = 0;
	int				itemtext = 0;
	LPCTSTR			strDialogTitle = _T("Browse for *.j3d world");
	LPCTSTR			bsp = NULL;
	CFileDialog		fdG3D
		(
		TRUE,
		bsp,
		NULL,
		OFN_EXTENSIONDIFFERENT	|
		OFN_FILEMUSTEXIST		|
		OFN_PATHMUSTEXIST,
		_T("j3d files(.j3d)|*.j3d|")
		);

	fdG3D.m_ofn.lpstrTitle = strDialogTitle;
	fdG3D.m_ofn.lpstrInitialDir = m_strBaseDir;	

	//	open it 
	if(IDOK !=fdG3D.DoModal())
	{
		ShowCursor(FALSE);
		return strBlank;
	}
	
	//	the dialog box is closed. Get the results of our browse
	POSITION pos = fdG3D.GetStartPosition();
	while (pos)
	{
		strUserWorldFile = fdG3D.GetNextPathName(pos);
	}

	//	hide the cursor again, and off we go...
	ShowCursor(FALSE);

	return strUserWorldFile;
}



////////////////////////////////////////////////////////////////////////////////
//	LoadWorld
//	
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::LoadWorld()
{
	bool			bNullWorld = false;
	grVFile			*pvfTemp = NULL;
	grVFile			*pvfG3dFork = NULL;
	CString			strNullPath = _T("NULL");
	CString			strBlankPath = _T("");

	SetCurrentDirectory(m_strBaseDir);

	//	Choose between two different methods for loading the world

	//	New Method -- thanks to Incarnadine

	if (m_pEngine)		//	don't do anything unless there's a valid engine
	{
		if (m_strLevel != strBlankPath)
		{
			CFileFind fileFind;
			if (!fileFind.FindFile(m_strLevel))	//	if no such file
			{
				return false;
			}
			else  // Found File
			{
				char* levelpath = NULL;
				levelpath = m_strLevel.GetBuffer(m_strLevel.GetLength());

				if (levelpath)
				{
					m_pWorld = grWorld_CreateFromEditorFile(levelpath, m_pPtrMgr, m_pResourceMgr);

					if (m_pWorld)
					{
						grWorld_CreateRef(m_pWorld);	// SHOULD be done automatically... but ain't

						if (!grWorld_SetEngine(m_pWorld, m_pEngine))
						{
							grWorld_Destroy(&m_pWorld);
							return false;
						}

						if (!grWorld_AttachSoundSystem(m_pWorld, m_pSoundSys))
							return false;

						grWorld_RebuildBSP(m_pWorld, BSP_OPTIONS_CSG_BRUSHES, Logic_Super, 3);
						grWorld_RebuildLights(m_pWorld); 
					}	//	if (m_pWorld..
					else
						return false;
				}	//	if (levelPath)...
				else
					return false;
			}	//	else fileFound...
		}	//	if (m_strLevel != strBlankPath)...
		else
			return false;
	}	//	if (m_pEngine
	else
		return false;

	return true;



//	Old Method
/*
	if (m_pEngine)		//	don't do anything unless there's a valid engine
	{
		if (m_strLevel != strBlankPath)
		{
			CFileFind fileFind;
			if (!fileFind.FindFile(m_strLevel))	//	if no such file
			{
				return false;
			}
			else  // Found File
			{
				char* levelpath = NULL;
				levelpath = m_strLevel.GetBuffer(m_strLevel.GetLength());

				if (levelpath)
				{
					pvfTemp = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_VIRTUAL, levelpath, 
						NULL, GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);

					if (pvfTemp)
					{
						//	get the J3D vfile from the world file
						pvfJ3dFork = grVFile_Open( pvfTemp, "Jet3D", GR_VFILE_OPEN_READONLY);

						if (pvfJ3dFork && m_pPtrMgr && m_pResourceMgr)
						{
							m_pWorld = grWorld_CreateFromFile(pvfJ3dFork, m_pPtrMgr, m_pResourceMgr);						

							if (m_pWorld)
							{
								grVFile_Close(pvfTemp);
								grVFile_Close(pvfJ3dFork);

								grWorld_CreateRef(m_pWorld);	// SHOULD be done automatically... but ain't

								if (!grWorld_SetEngine(m_pWorld, m_pEngine))
								{
									grWorld_Destroy(&m_pWorld);
									return false;
								}

								if (!grWorld_AttachSoundSystem(m_pWorld, m_pSoundSys))
									return false;

								grWorld_RebuildBSP(m_pWorld, BSP_OPTIONS_CSG_BRUSHES, Logic_Super, 3);
								grWorld_RebuildLights(m_pWorld); 
							}
							else if (!m_pWorld)	// if no world from Level...
							{
								grVFile_Close(pvfTemp);
								grVFile_Close(pvfJ3dFork);

								return false;

							}
						}
						else	//	we can't even read the file
						{
							grVFile_Close(pvfTemp);
							grVFile_Close(pvfJ3dFork);

							return false;
						}
					}	//	if (Level)...
					else	// we have no Level
					{
						grVFile_Close(pvfTemp);
						grVFile_Close(pvfJ3dFork);

						return false;
					}
				}	//	if (levelpath)...
				else	//	create NULL world since we can't get levelpath
				{
					return false;
				}	
			}	//	else // Found File... 
		}	//	else if (strLevelPath != strNullPath)...	
		else	//	this is a default in case we have trouble testing levelpath
		{
			return false;
		}

	}	//	if (m_pEngine)
	else
	{
		return false;
	}	
	return true;
	*/
}



////////////////////////////////////////////////////////////////////////////////
//	RenderView
//	to be called ONLY by OnTimer()
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::RenderView(grFloat fElapsedTime)
{	
	if ((m_pEngine) && (m_pWorld) && (m_pCamera))
	{


#ifdef _DEBUG	//	update window size and position





		CRect	rectTest;
		CRect	rectCamera;


		GetWindowRect(&rectTest);
		GetClientRect(&rectCamera);




//		GetWindowRect(&rectTest);
//		GetClientRect(&rectCamera);

		m_Rect.Top = rectCamera.top;
		m_Rect.Left = rectCamera.left;
		m_Rect.Bottom = rectCamera.bottom;
		m_Rect.Right =  rectCamera.right;

		if ((rectTest != m_rectMainView) || m_bJustMoved)
		{
			if (GetCapture() != this)
			{
				m_rectMainView = rectTest;
				grEngine_UpdateWindow(m_pEngine);
				m_bJustMoved = false;
				grCamera_SetAttributes(m_pCamera, 2.0f, &m_Rect);
			}
		}
#endif

		if (!grCamera_SetXForm(m_pCamera, &m_xfCamera))
		{
			return true;
		}

		if ((m_pEngine) && (m_pCamera))
		{
			//	get mouse movement and rotate camera accordingly
			GetMouseInput();

			//	prepare the engine to render current world
			if (!grEngine_BeginFrame(m_pEngine, m_pCamera, GR_TRUE))
			{
				SendMessage(WM_CLOSE);
			}
		}

		if ((m_pWorld) && (m_pCamera))
		{
			//	go through the list of world objects, and deliver the time.
			//	They will adjust themselves accordingly...
			grWorld_Frame(m_pWorld, fElapsedTime);

			//	now that all changes have been calculated, render the world
			//	to reflect these changes
			if (!grWorld_Render(m_pWorld, m_pCamera, (grFrustum*)NULL))
			{
				ShutdownAll();
			}
		}

		if (m_pEngine)
		{
			grEngine_Printf(m_pEngine, 0, 10, 10, GR_COLOR_COLORVALUE(100,100,100,100), "Genesis3D: Reborn - Press ESC to close.");

			//	flip the new rendered frame to the screen
			if (!grEngine_EndFrame(m_pEngine))
			{
				SendMessage(WM_CLOSE);
			}
		}
	}	//	if (m_pEngine && m_pWorld && m_pCamera)...

	return true;
}



////////////////////////////////////////////////////////////////////////////////
//	MoveCamera
//	Moves our camera forward or backward
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::MoveCamera(float speed, grVec3d *Direction)
{
	if ((grXForm3d_IsValid(&m_xfCamera)) &&
		(grVec3d_IsValid(&m_xfCamera.Translation)))
	{
		//	new position	
		m_vecCameraPos = m_xfCamera.Translation;		

		//	Move forward or backward
		grVec3d_AddScaled(&m_vecCameraPos, Direction, speed*m_fSpeedMultiplier, &m_vecCameraPos); 
		m_xfCamera.Translation = m_vecCameraPos;
	}
}



////////////////////////////////////////////////////////////////////////////////
//	ControlCamera	-- called by GetMouseInput()
//	calcs rotation based on mouse pos
////////////////////////////////////////////////////////////////////////////////
void CMainFrame:: ControlCamera(UINT nFlags, CPoint MouseDelta, short ZoomDelta)		
{
	grXForm3d		cTempXForm;				//	for holding rotational info
	grVec3d			tempUp, tempLeft, oldCameraTranslation;
	grQuaternion	Quat;					//	our quaternion variables
	grFloat			TURN_SPEED = 0.0f;		//	speed camera will move if turning left/right
	grFloat			MOVE_SPEED = 0.0f;		//	speed camera will move forward or backward
	int				Movement = 0;
	float			Sensitivity = 0.4f;
	float			NormalSpeed = 60.0f;

	//	must be pressing left mouse button in DEBUG mode
#ifdef _DEBUG
	if (nFlags != MK_LBUTTON)
		return;
#endif

	//	initialize
	grXForm3d_SetIdentity(&cTempXForm);

	//	save current camera vector, we'll need it later	
	oldCameraTranslation = m_xfCamera.Translation;

	if(MouseDelta.x != 0)		//	is mouse moving left or right?
	{
		grXForm3d_Copy(&m_xfCamera, &cTempXForm);

		grXForm3d_SetZRotation(&cTempXForm, 0.0f);	//	Prevent camera roll
		grXForm3d_GetUp(&cTempXForm, &tempUp);		//	Get Y axis
		TURN_SPEED   =  -(float)MouseDelta.x * Sensitivity;
		grQuaternion_SetFromAxisAngle(&Quat, &tempUp, -RAD(TURN_SPEED));
		grQuaternion_ToMatrix(&Quat, &cTempXForm);

		grXForm3d_Multiply(&cTempXForm,  &m_xfCamera, &m_xfCamera);

		//	restore camera to original vector
		m_xfCamera.Translation = oldCameraTranslation;	

	}	//	if (MouseDelta.x...

	if(MouseDelta.y != 0)			//	is mouse moving up or down?
	{
		grXForm3d_SetIdentity(&cTempXForm);
		grXForm3d_Copy(&m_xfCamera, &cTempXForm);
		grXForm3d_GetLeft(&cTempXForm, &tempLeft);		//	Get X axis

		TURN_SPEED   =  -(float)MouseDelta.y * Sensitivity;
		grQuaternion_SetFromAxisAngle(&Quat, &tempLeft, RAD(TURN_SPEED));
		grQuaternion_ToMatrix(&Quat, &cTempXForm);
		grXForm3d_Multiply(&cTempXForm, &m_xfCamera,  &m_xfCamera);

		//	restore camera to original vector
		m_xfCamera.Translation = oldCameraTranslation;	

	}	//	if (MouseDelta.y...

	if (m_pCamera)
	{
		//	update the camera's transform
		if (!grCamera_SetXForm(m_pCamera, &m_xfCamera))
			return;
	}
}



////////////////////////////////////////////////////////////////////////////////
//	OnRButtonDown
//	
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRButtonDown(UINT nFlags, CPoint point)
{

	// TODO: Add your message handler code here and/or call default

	CWnd::OnRButtonDown(nFlags, point);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMouseMove
//	
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	
#ifdef _DEBUG		
		SetCursor(LoadCursor(NULL, IDC_ARROW));
#endif
	CWnd::OnMouseMove(nFlags, point);
}



////////////////////////////////////////////////////////////////////////////////
//	OnLButtonDown
//	
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (GetCapture() != this)
		SetCapture();

	// TODO: Add your message handler code here and/or call default

	CWnd::OnLButtonDown(nFlags, point);
}


////////////////////////////////////////////////////////////////////////////////
//	OnLButtonUp
//	
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this)
		ReleaseCapture();

	// TODO: Add your message handler code here and/or call default

	CWnd::OnLButtonUp(nFlags, point);
}



////////////////////////////////////////////////////////////////////////////////
//	GetMouseInput -- directly called by RenderView()
//	DO NOT USE CWnd::OnMouseMove(... -- too slow -- also, infinite loops if
//	you call SetCursorPos(... from within CWnd::OnMouseMove(...
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::GetMouseInput()
{
	CPoint	point;
	CPoint	RotateDelta;
	CRect	rectClient;

	point.SetPoint(0,0);
	RotateDelta.SetPoint(0,0);

	GetCursorPos(&point);
	GetClientRect(&rectClient);
	
	RotateDelta = m_ptOldMousePoint - point; // the order is important to generate the desired motion
		
#ifdef _DEBUG
	if (GetCapture() == this)
		ControlCamera(MK_LBUTTON, RotateDelta, 0);
#endif

#ifdef NDEBUG
	ControlCamera(NULL, RotateDelta, 0);
	//	put the cursor back at screen center -- ready for the next cycle
	SetCursorPos(rectClient.Width()/2, rectClient.Height()/2);
	GetCursorPos(&point);
#endif
	m_ptOldMousePoint.x = point.x;
	m_ptOldMousePoint.y =  point.y;
}



////////////////////////////////////////////////////////////////////////////////
//	OnKeyDown -- initializes all FWD-BK-LFT-RT movement
//	called by the MFC framework ONLY for predefined virtual keys -- NOT
//	for individual letter keys. 
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (!m_bShuttingDown)
	{	
		//	NOTE: we are speccing two keyset combinations for movement.
		//	1. arrow keys
		//	2. WASD keys
		//	The user can use whichever he prefers
		switch (nChar)
		{
		case VK_UP:
		case ID_MOVE_FORWARD:	//	(W)
			{
				grVec3d		In;
				In.X = In.Y = In.Z = 0.0f;
				grXForm3d_GetIn(&m_xfCamera, &In);
				MoveCamera(20.0f, &In);	
				break;
			}

		case VK_DOWN:
		case ID_MOVE_BACK:		//	(S)
			{
				grVec3d		In;
				In.X = In.Y = In.Z = 0.0f;
				grXForm3d_GetIn(&m_xfCamera, &In);
				MoveCamera(-10.0f, &In);
				break;
			}

		case VK_LEFT:
		case ID_MOVE_LEFT:		//	(A)
			{
				grVec3d		Left;
				Left.X = Left.Y = Left.Z = 0.0f;
				grXForm3d_GetLeft(&m_xfCamera, &Left);
				MoveCamera(10.0f, &Left);	
				break;
			}

		case VK_RIGHT:
		case ID_MOVE_RIGHT:		//	(D)
			{
				grVec3d		Left;
				Left.X = Left.Y = Left.Z = 0.0f;
				grXForm3d_GetLeft(&m_xfCamera, &Left);
				MoveCamera(-10.0f, &Left);	
				break;
			}

			// Pressing ESC will terminate the app
		case VK_ESCAPE:
			{
				m_bShuttingDown = true;
				SendMessage(WM_CLOSE);
				break;
			}
		default:
			{}
		}
	}

	// TODO: Add your message handler code here and/or call default
	if (!m_bShuttingDown)
		CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMoveForward
//	Event handler custom-prepared in this project's resource manager
//	gets letter key presses
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMoveForward()
{
	//	send this info to OnKeyDown -- which initializes all movement
	OnKeyDown(ID_MOVE_FORWARD, NULL, NULL);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMoveBack
//	Event handler custom-prepared in this project's resource manager
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMoveBack()
{
		OnKeyDown(ID_MOVE_BACK, NULL, NULL);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMoveLeft
//	Event handler custom-prepared in this project's resource manager
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMoveLeft()
{
	OnKeyDown(ID_MOVE_LEFT, NULL, NULL);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMoveRight
//	Event handler custom-prepared in this project's resource manager
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMoveRight()
{
	OnKeyDown(ID_MOVE_RIGHT, NULL, NULL);
}



////////////////////////////////////////////////////////////////////////////////
//	OnMove
//	
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMove(int x, int y)
{
	CWnd::OnMove(x, y);

	// TODO: Add your message handler code here

	m_bJustMoved = true;
}



////////////////////////////////////////////////////////////////////////////////
//	StartTimer
//	used for cycling calls to render and render and render and render...
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::StartTimer()
{
			//	initialize timer member variables
		QueryPerformanceCounter(&m_LIOldTick);
		QueryPerformanceFrequency(&m_LIFreq);

			//	set rendering gatekeeper
		m_bReadyToRender = true;

	if (SetTimer(IDT_G3DVIEW_TIMER, TIMER_SPEED, NULL) == 0)
		return false;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
//	StopTimer
//
////////////////////////////////////////////////////////////////////////////////
bool CMainFrame::StopTimer()
{
	if (!KillTimer(IDT_G3DVIEW_TIMER))
	{
		return false;
	}
	return true;
}



////////////////////////////////////////////////////////////////////////////////
//	OnTimer	-- Basically, this is the MAIN LOOP
//	called by the MFC framework
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{

	//	CMainFrame*	pMainFrame = NULL;
	//	pMainFrame = (CMainFrame*)AfxGetMainWnd();
	//	if (pMainFrame)
	{
		if (nIDEvent == IDT_G3DVIEW_TIMER)
		{
			if (m_bReadyToRender)
			{
				grFloat			fElapsedTime(0.0f);
				LARGE_INTEGER	LICurTick, LIDeltaTick;

				//	close the gate -- so no interruptions until we're completely
				//	done with this rendering cycle
				m_bReadyToRender = false;

				//	Get timing info, get current tick
				QueryPerformanceCounter(&LICurTick);

				//	Get the difference between current tick and oldtick
				SubLarge(&m_LIOldTick, &LICurTick, &LIDeltaTick);

				//	OldTick now = CurTick
				m_LIOldTick = LICurTick;

				//	Get the time (in seconds) that past since last frame iteration
				if (LIDeltaTick.LowPart > 0)
				{
					fElapsedTime =  1.0f / (((float)m_LIFreq.LowPart / (float)LIDeltaTick.LowPart));
				}
				else 
				{		
					fElapsedTime = 0.001f;
				}

				//	render the view and modify the the gatekeeper flag
				//	as a result
				m_bReadyToRender = RenderView(fElapsedTime);
			}
		}
	}
	CFrameWnd::OnTimer(nIDEvent);
}


