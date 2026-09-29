// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "ActorWorkbench.h"

#include "MainFrm.h"
#include "ActorWorkbenchView.h"
#include "G3DView.h"
#include ".\mainfrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)

	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CFrameWnd::OnHelpFinder)

	//added HELP access in - Ken Deel Jan-4-06
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};


// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
}

CMainFrame::~CMainFrame()
{
}


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	// TODO: Delete these three lines if you don't want the toolbar to be dockable
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	
	DockControlBar(&m_wndToolBar);

	RECT						r, br;
	int							x, y;

	GetClientRect(&r);

	x = r.right - r.left;
	y = r.bottom - r.top;

	br.left = 0;
	br.right = (x >> 1) - 250;
	br.top = 0;
	br.bottom = y;

	m_wndBarBrowser.Create(this, IDD_BAR_BROWSER, WS_CHILD | CBRS_GRIPPER | CBRS_LEFT | CBRS_FLOAT_MULTI, IDD_BAR_BROWSER);
	m_wndBarBrowser.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
	
	m_wndToolBrowser.Create(IDD_ACTOR_BROWSER, &m_wndBarBrowser);
	m_wndToolBrowser.ShowWindow(SW_SHOW);
	m_wndToolBrowser.SetWindowPos(NULL, 5, 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	//m_RootItem = m_wndToolBrowser.m_TreeCtrl.InsertItem("Root");
	DockControlBar(&m_wndBarBrowser);
	
	ShowWindow(SW_SHOWMAXIMIZED);
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.style = WS_OVERLAPPED | WS_CAPTION | FWS_ADDTOTITLE
		 | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;

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


BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	// TODO: Add your specialized code here and/or call the base class
	if (!CFrameWnd::OnCreateClient(lpcs, pContext))
		return FALSE;

	SetActiveView(GetActiveView());
	return TRUE;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);

	CDialog							t(IDD_BAR_BROWSER);
	RECT							r, w;

	t.GetClientRect(&r);
	GetClientRect(&w);

	w.right -= cx - r.right;
	w.bottom -= cy - r.bottom;

	m_wndToolBrowser.SetWindowPos(NULL, 0, 0, w.right - w.left, w.bottom - w.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE); 
}

grBoolean CMainFrame::LoadActor(const char * filename, const char *actorname)
{
	grVFile					*File = NULL;
	grObject				*Object = NULL;
	grActor_Def				*ActorDef = NULL;
	grActor					*Actor = NULL;
	HTREEITEM				BonesItem, MaterialsItem, MotionsItem, Item;
	CTreeCtrl				*pTree = NULL;
	grXForm3d				Attachment;

	File = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_DOS, filename, NULL, GR_VFILE_OPEN_READONLY);
	if (!File)
	{
		AfxMessageBox("Could not open actor file!!", 48, 0);
		return GR_FALSE;
	}

	ActorDef = grActor_DefCreateFromFile(File);
	if (!ActorDef)
	{
		AfxMessageBox("Could not create actor def!!", 48, 0);
		grVFile_Close(File);
		return GR_FALSE;
	}

	grVFile_Close(File);

	Object = grObject_Create("Actor");
	if (!Object)
	{
		AfxMessageBox("Could not create actor object!!", 48, 0);
		grActor_DefDestroy(&ActorDef);
		return GR_FALSE;
	}

	Actor = (grActor*)grObject_GetInstance(Object);
	if (!Actor)
	{
		AfxMessageBox("Could not get actor instance!!", 48, 0);
		grActor_DefDestroy(&ActorDef);
		grObject_Destroy(&Object);
		return GR_FALSE;
	}

	grActor_SetActorDef(Actor, ActorDef);

	pTree = &m_wndToolBrowser.m_TreeCtrl;

	pTree->DeleteItem(m_RootItem);

	m_RootItem = pTree->InsertItem(actorname, TVI_ROOT);
	BonesItem = pTree->InsertItem("Bones", m_RootItem);

	grBody *Body = grActor_GetBody(ActorDef);
	if (!Body)
	{
		AfxMessageBox("Could not get body!!", 48, 0);
		grActor_DefDestroy(&ActorDef);
		grObject_Destroy(&Object);
		return GR_FALSE;
	}

	for (int i = 0; i < grBody_GetBoneCount(Body); i++)
	{
		const char				*bonename = NULL;
		int						parent;

		grBody_GetBone(Body, i, &bonename, &Attachment, &parent);
		Item = pTree->InsertItem(bonename, BonesItem);
	}

	MaterialsItem = pTree->InsertItem("Materials", m_RootItem);

	//	by trilobite	Jan. 2011
	//for (i = 0; i < grBody_GetMaterialCount(Body); i++)
	for (int i = 0; i < grBody_GetMaterialCount(Body); i++)
	//
	{
		const char						*matname = NULL;
		grMaterialSpec					*bmp = NULL;
		float							r, g, b;
		grUVMapper						pMapper;

		grBody_GetMaterial(Body, i, &matname, &bmp, &r, &g, &b, &pMapper);
		Item = pTree->InsertItem(matname, MaterialsItem);
	}

	MotionsItem = pTree->InsertItem("Motions", m_RootItem);

	//	by trilobite	Jan. 2011
	//for (i = 0; i < grActor_GetMotionCount(ActorDef); i++)
	for (int i = 0; i < grActor_GetMotionCount(ActorDef); i++)
	//
	{
		grMotion						*Motion = NULL;

		Motion = grActor_GetMotionByIndex(ActorDef, i);
		if (!Motion)
		{
			AfxMessageBox("Could not get motion!!", 48, 0);
			return GR_FALSE;
		}

		Item = pTree->InsertItem(grMotion_GetName(Motion), MotionsItem);
	}

	((CG3DView*)GetActiveView())->SetActiveActor(Object);

	return GR_TRUE;
}

void CMainFrame::OnFileOpen()
{
	CFileDialog					dlg(TRUE, "act", NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, "Jet3D Actor Files (*.act)|*.act", this, 0);
	
	if (dlg.DoModal() == IDOK)
	{
		if (!LoadActor(dlg.GetPathName(), dlg.GetFileName()))
			AfxMessageBox("Could not load actor!!", 48, 0);
	}
}