/****************************************************************************************/
/*  J3DVIEW.CPP                                                                         */
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

#include "stdafx.h"

/*
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
*/

#include "G3DMfcApp.h"
#include "G3DMfcDoc.h"
#include "G3DMfcView.h"

#define G3DMFCVIEW_GAMMA		1.2f

/////////////////////////////////////////////////////////////////////////////
// J3DVIEW_FULLSCREEN
// 
// Code to support a fullscreen window is here, but needs some support from
// Jet3D before completion.  See OnCreate for more details.

#define xxG3DMFCVIEW_FULLSCREEN

/////////////////////////////////////////////////////////////////////////////
// Private registered message (globals).  Send one of the message ids enum'd
// below as the wParam argument.

static UINT g_WMPrivateMessage = 0;

enum
{
	G3DMFCVIEW_FIRST_MESSAGE = 1,
	G3DMFCVIEW_ENABLE_ENGINE = G3DMFCVIEW_FIRST_MESSAGE,
	G3DMFCVIEW_UPDATEENGINE,
	G3DMFCVIEW_LAST_MESSAGE = G3DMFCVIEW_UPDATEENGINE
};


#ifdef G3DMFCVIEW_FULLSCREEN

#define FULLWNDCLASS "FullScreenJ3D"

// WndProc for separate full screen window
//---------------------------------------------------------------------------
static LRESULT CALLBACK FullScreenWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
	case WM_CREATE:
		break;

	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
		TRACE("KeyDown/LButtonDown\n");
		{
			CG3DMfcFullFrame* pFrame = (CG3DMfcFullFrame*)GetWindowLong(hWnd, GWL_USERDATA);
 			ASSERT(pFrame != nullptr);

			CloseWindow(pFrame->GetSafeHwnd());
		}
		break;

	case WM_SYSCOMMAND:
		TRACE("SysCommand %d\n", wParam);
		return 0;
/*
	case WM_SYSKEYDOWN:
		TRACE("SysKeyDown\n");
		return 0;

	case WM_INITMENU:
		TRACE("InitMenu\n");
		return 0;
*/
	case WM_DESTROY:
		TRACE("Destroy\n");
		uMsg = WM_DESTROY;
		break;

	case WM_PAINT:
		TRACE("WM_PAINT\n");
		{
			CG3DMfcFullFrame* pFrame = (CG3DMfcFullFrame*)GetWindowLong(hWnd, GWL_USERDATA);
 			ASSERT(pFrame != nullptr);

			pFrame->Render();
		}
		break;
	}

    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#endif // J3DVIEW_FULLSCREEN

/////////////////////////////////////////////////////////////////////////////
// s_J3DView_WndClass a special wndclass for this view with nullptr background
// to minimize any flash.
static const char* s_G3DMfcView_WndClass = "J3DView WndClass";
static ATOM s_G3DMfcView_Atom = 0;

/////////////////////////////////////////////////////////////////////////////
// CJ3DView

IMPLEMENT_DYNCREATE(CG3DMfcView, CView)

CG3DMfcView::CG3DMfcView()
{
	m_bFullScreen = FALSE;

	m_pEngine = nullptr;
	m_bEngineEnabled = GR_FALSE;
	m_pDriver = nullptr;
	m_pDriverMode = nullptr;

	m_hRenderWnd = nullptr;
	m_hFullWnd = nullptr;
}

CG3DMfcView::~CG3DMfcView()
{
	// Make sure everything is clean
	ASSERT(m_pEngine == nullptr);
	ASSERT(m_hFullWnd == nullptr);
}


BEGIN_MESSAGE_MAP(CG3DMfcView, CView)
	//{{AFX_MSG_MAP(CJ3DView)
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_REGISTERED_MESSAGE( g_WMPrivateMessage , OnPrivateMessage)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_WM_ACTIVATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CJ3DView drawing

void CG3DMfcView::OnDraw(CDC* pDC)
{
	Render();
	pDC;
}

/////////////////////////////////////////////////////////////////////////////
// CJ3DView diagnostics

#ifdef _DEBUG
void CG3DMfcView::AssertValid() const
{
	CView::AssertValid();
}

void CG3DMfcView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CG3DMfcDoc* CG3DMfcView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CG3DMfcDoc)));
	return (CG3DMfcDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CJ3DView message handlers


void CG3DMfcView::OnInitialUpdate() 
{
	CView::OnInitialUpdate();
}

void CG3DMfcView::OnDestroy() 
{
	CView::OnDestroy();

	if(m_pEngine != nullptr)
	{
		// Shutting down the driver and then freeing the engine
		//grEngine_ShutdownDriver(m_pEngine);

		grEngine_Free(m_pEngine);
		m_pEngine = nullptr;
		m_bEngineEnabled = GR_FALSE;
	}
}

int CG3DMfcView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	const char* pDrvName{};
	const char* pPath{};

	// The view is added to the document in CView::OnCreate and this must succeed.
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(GetDocument() == nullptr)
		return -1;

	// Private message specific to the J3DView class
	if(g_WMPrivateMessage == 0)
		g_WMPrivateMessage = RegisterWindowMessage("J3DView Private Message");
	if(g_WMPrivateMessage == 0)
		return(-1);

	m_hRenderWnd = GetSafeHwnd();
	ASSERT(m_hRenderWnd != nullptr);

	// Awaiting support from Jet3D.  Here's what is supposed to happen:  
	// 1) User chooses driver
	// 2) If the driver is a fullscreen mode (most are), register the window
	// class and create a fullscreen window and set m_bFullScreen to TRUE.
	// 3) Otherwise, use the current m_hRenderWnd.
#ifdef G3DMFCVIEW_FULLSCREEN

    WNDCLASSEX wcex;

    wcex.cbSize           =    sizeof(WNDCLASSEX);
    wcex.hInstance        =    AfxGetInstanceHandle();
    wcex.lpszClassName    =    FULLWNDCLASS;
    wcex.lpfnWndProc      =    FullScreenWndProc;
    wcex.style            =    CS_VREDRAW | CS_HREDRAW | CS_NOCLOSE;

    wcex.hIcon            =    LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm          =    LoadIcon(nullptr, IDI_WINLOGO);
    wcex.hCursor          =    LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszMenuName     =    nullptr;
    wcex.cbClsExtra       =    0;
    wcex.cbWndExtra       =    0;
    wcex.hbrBackground    =    (HBRUSH)GetStockObject(NULL_BRUSH);

    RegisterClassEx(&wcex);

	if(!CreateFullWnd())
		return(-1);

	PostEnableEngine();

#endif // J3DVIEW_FULLSCREEN

	// Create the engine
	ASSERT(m_pEngine == nullptr);
	pPath = ((CG3DMfcApp*)AfxGetApp())->GetDriverPath();
	if( (pPath == nullptr) || (strlen(pPath) <= 0) ) // must be something here
		return(-1);
	m_pEngine = grEngine_Create(m_hRenderWnd, "", pPath);
	if(m_pEngine == nullptr)
		return(-1);
	m_bEngineEnabled = GR_FALSE;
	
	grEngine_EnableFrameRateCounter(m_pEngine, GR_FALSE );
    //grEngine_SetGamma(m_pEngine, 2.5f);	//trilobite orig  
	grEngine_SetGamma(m_pEngine, G3DMFCVIEW_GAMMA);	//trilobite revise	
	grEngine_UpdateGamma(m_pEngine);	//trilobite add
	// Set up the driver and mode
	ASSERT(m_pDriver == nullptr);
	ASSERT(m_pDriverMode == nullptr);
	if(GR_FALSE == ((CG3DMfcApp*)AfxGetApp())->GetDriverAndMode(m_pEngine, &m_pDriver, &m_pDriverMode))
	{
		return(-1);
	}

	// should have a valid mode and driver at this point
	if( (m_pDriver == nullptr) || (m_pDriverMode == nullptr) )
		return(-1);

	// Use "document name : driver name" as default title format
	if(grDriver_GetName(m_pDriver, &pDrvName) == GR_FALSE)
		return(-1);
	EnableEngine();
	return 0;
}

BOOL CG3DMfcView::PreCreateWindow(CREATESTRUCT& cs) 
{
	WNDCLASS wndclass{};

	if(s_G3DMfcView_Atom == 0)
	{
		memset(&wndclass, 0, sizeof(wndclass));

		wndclass.hInstance        =    AfxGetInstanceHandle();
		wndclass.lpszClassName    =    s_G3DMfcView_WndClass;
		wndclass.lpfnWndProc      =    (WNDPROC)(::DefWindowProc);
		wndclass.style            =    CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;

		wndclass.hCursor          =    LoadCursor(nullptr, IDC_ARROW);
		wndclass.hbrBackground    =    (HBRUSH)GetStockObject(NULL_BRUSH);

		s_G3DMfcView_Atom = RegisterClass(&wndclass);
		if(s_G3DMfcView_Atom == 0)
			return(FALSE);
	}

	cs.lpszClass = (LPCTSTR)s_G3DMfcView_Atom;
	
	return CView::PreCreateWindow(cs);
}

void CG3DMfcView::Render()
{
	if(m_bEngineEnabled)
	{
		CG3DMfcDoc* pDoc{};
		pDoc = GetDocument();
		if(pDoc->Render(this) == FALSE)
		{
			DestroyWindow();
		}
	}
}

void CG3DMfcView::OnSize(UINT nType, int cx, int cy) 
{
	TRACE("OnSize %d %d\n", cx, cy);
	
	if( (cx <= 0) || (cy <= 0) )
	{
		CView::OnSize(nType, cx, cy);
		return;
	}

	if(m_bFullScreen)
	{
		switch(nType)
		{
		case SIZE_RESTORED:
		case SIZE_MAXIMIZED:
			TRACE("CJ3DView::OnSize MAX\n");
			if(m_hFullWnd == 0)
			{
				if(!CreateFullWnd())
					DestroyWindow();
			}
			break;
		}
	}

	CView::OnSize(nType, cx, cy);
	
	switch(nType)
	{
	case SIZE_MINIMIZED:
	case SIZE_RESTORED:
	case SIZE_MAXIMIZED:
		if(m_pEngine != nullptr) 
		{

			SendMessage(g_WMPrivateMessage, G3DMFCVIEW_UPDATEENGINE);
		}
		break;
	}

	if(m_bFullScreen)
	{
		switch(nType)
		{
		case SIZE_MINIMIZED:
			if(m_hFullWnd != nullptr)
			{
				::DestroyWindow(m_hFullWnd);
				m_hFullWnd = nullptr;
			}
			break;
		}
	}
}

grBoolean CG3DMfcView::EnableEngine()
{
	if(!m_bEngineEnabled && (m_pEngine != nullptr) )
	{
		ASSERT(m_pEngine != nullptr);
		TRACE("EnableEngine: Enabling driver\n");
		if(!grEngine_SetDriverAndMode(m_pEngine, GetSafeHwnd(), m_pDriver, m_pDriverMode))
		{
			return(GR_FALSE);
		}
		TRACE("EnableEngine: Driver now enabled\n");
		m_bEngineEnabled = TRUE;
		::SetFocus(m_hRenderWnd);
		Invalidate(FALSE);
	}

	return(GR_TRUE);
}

LRESULT CG3DMfcView::OnPrivateMessage(WPARAM wParam, LPARAM)
{
	ASSERT(wParam >= G3DMFCVIEW_FIRST_MESSAGE);
	ASSERT(wParam <= G3DMFCVIEW_LAST_MESSAGE);

	// Can use a switch statement when it gets more complicated

	if(wParam == G3DMFCVIEW_UPDATEENGINE )
	{
		grEngine_UpdateWindow(m_pEngine);
	}

	if(wParam == G3DMFCVIEW_ENABLE_ENGINE)
	{
		ASSERT(m_hRenderWnd != 0);

		TRACE("OnEnableEngine\n");

		if(!EnableEngine())
		{
			DestroyWindow();
		}
	}
	return 0;
}

void CG3DMfcView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
{
	if(!IsIconic() && !m_bEngineEnabled)
	{
		if(!EnableEngine())
		{
			DestroyWindow();
			return;
		}
	}

	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

void CG3DMfcView::OnClose() 
{
	CView::OnClose();
}

void CG3DMfcView::PostEnableEngine(void)
{
	PostMessage(g_WMPrivateMessage, G3DMFCVIEW_ENABLE_ENGINE, 0);
}

void CG3DMfcView::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	CView::OnActivate(nState, pWndOther, bMinimized);
	
	if( !grEngine_Activate(m_pEngine, nState & (WA_ACTIVE | WA_CLICKACTIVE) ) )
	{
		DestroyWindow();
		return;
	}
}

BOOL CG3DMfcView::DestroyWindow() 
{
	//////////////////
	
	if(m_hFullWnd != nullptr)
	{
		::DestroyWindow(m_hFullWnd);
			
		m_hFullWnd = nullptr;
	}
	
	return CView::DestroyWindow();
}

BOOL CG3DMfcView::CreateFullWnd()
{
#ifdef G3DMFCVIEW_FULLSCREEN

	ASSERT(m_bFullScreen != FALSE);
	ASSERT(m_hFullWnd == 0);

    m_hFullWnd = CreateWindowEx(0, //WS_EX_TOPMOST,
								FULLWNDCLASS,
								"",
								WS_VISIBLE | WS_POPUPWINDOW,
								0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
								GetSafeHwnd(),
								nullptr,
								AfxGetApp()->m_hInstance,
								nullptr);

	if(m_hFullWnd == 0)
	{
		return(FALSE);
	}

	::SetWindowLong(m_hFullWnd, GWL_USERDATA, (LONG)this);

	::ShowWindow(m_hFullWnd, SW_SHOWNORMAL);
	::UpdateWindow(m_hFullWnd);
	
	m_hRenderWnd = m_hFullWnd;

#endif // J3DVIEW_FULLSCREEN

	return(TRUE);
}
