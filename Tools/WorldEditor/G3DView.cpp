/****************************************************************************************/
/*  JETVIEW.CPP                                                                         */
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
#include "GWE.H"
#include "Resource.h"
#include "Util.h"
#include "ram.h"
#include "G3DView.h"
#include "ErrorLog.h"
#include "drvlist.h"
#include "mainfrm.h"
#include <math.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define G3DVIEW_TIMER		2
#define G3DVIEW_PERIOD		50

#define G3DVIEW_GAMMA		1.2f

#define G3DVIEW_FLYTIMER	3
#define G3DVIEW_FLYPERIOD	10

// Modern navigation tuning
#define NAV_FLY_SPEED		400.0f				// world units per second at speed scale 1
#define NAV_FAST_SCALE		4.0f				// Shift multiplier for fly, dolly and pan
#define NAV_ACCEL			12.0f				// how quickly fly velocity catches up with input (1/s)
#define NAV_LOOK_SCALE		(1.0f / 150.0f)		// radians per pixel, same as the classic RMB look
#define NAV_PITCH_LIMIT		1.55f
#define NAV_DOLLY_STEP		32.0f				// world units per wheel notch
#define NAV_ORBIT_DISTANCE	256.0f				// orbit pivot distance when nothing is selected
#define NAV_SPEED_MIN		0.05f
#define NAV_SPEED_MAX		20.0f

static const char* s_pszNavSection = "Navigation";
static const char* s_pszNavKeymapKey = "Keymap";
static int s_NavKeymap = -1;

int CG3DView::m_CXDRAG = 2;
int CG3DView::m_CYDRAG = 2;


// video mode settings
static	grDriver* FullscreenDriver = nullptr;
static	grDriver_Mode* FullscreenMode = nullptr;
static	grDriver* WindowDriver = nullptr;
static	grDriver_Mode* WindowMode = nullptr;
static	grFloat			Fullscreen_Framerate = 0;

/////////////////////////////////////////////////////////////////////////////
// CJetView

IMPLEMENT_DYNCREATE(CG3DView, CG3DMfcView)

CG3DView::CG3DView() : m_nViewType(0), m_bDragging(false), m_RenderMode(RenderMode_TexturedAndLit), m_bAnimate(false),
	m_bFlying(false), m_bFlyMoved(false), m_bOrbiting(false), m_bPanning(false), m_bCursorHidden(false), m_bEatAltUp(false),
	m_FlySpeedScale(1.0f)
{
	grVec3d_Clear(&m_FlyVelocity);
	grVec3d_Clear(&m_OrbitPivot);
	m_FlyLastTick.QuadPart = 0;
	m_bRecalcCamera = TRUE;
	m_pCamera = nullptr;

	grVec3d_Set(&m_CameraPos, 0, 0, 0);
	grVec3d_Set(&m_CameraLeft, -1, 0, 0);
	grVec3d_Set(&m_CameraUp, 0, 1, 0);
	grVec3d_Set(&m_CameraIn, 0, 0, -1);
	m_CameraRotX = 0.0f;
	m_CameraRotY = 0.0f;
}

CG3DView::~CG3DView()
{
	// The document owns the world and releases it in CGweDoc::DeleteContents. The view
	// takes no reference of its own, so it must not destroy the world here (doing so
	// freed it before Level_Destroy ran once model BSPs stopped holding extra refs).

	// 
	//
	//KillTimer( JETVIEW_TIMER );	//undone

	//
	if (m_pCamera != nullptr)
		grCamera_Destroy(&m_pCamera);
}


BEGIN_MESSAGE_MAP(CG3DView, CG3DMfcView)
	//{{AFX_MSG_MAP(CJetView)
	ON_WM_LBUTTONUP()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_COMMAND(IDM_VIEW_CENTERSELCTION, OnViewCenterselction)
	ON_WM_TIMER()
	ON_COMMAND(ID_3DVIEW_LINES, On3dviewLines)
	ON_COMMAND(ID_3DVIEW_TEXTURED, On3dviewTextured)
	ON_COMMAND(ID_3DVIEW_TEXTUREDWLIGHTS, On3dviewTexturedwlights)
	ON_COMMAND(ID_3DVIEW_FLAT, On3dviewFlat)
	ON_COMMAND(ID_3DVIEW_BSPSPLITS, On3dviewBspsplits)
	ON_COMMAND(IDM_BILINEAR, OnBilinear)
	//}}AFX_MSG_MAP
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_CAPTURECHANGED()
	ON_COMMAND(ID_3DVIEW_CLASSICNAV, On3dviewClassicNav)
	ON_COMMAND_RANGE(IDM_VIEW_TEXTURED, IDM_VIEW_RESERVED2, OnViewType)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_VIEW_TEXTURED, IDM_VIEW_RESERVED2, OnUpdateViewType)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CJetView drawing

/////////////////////////////////////////////////////////////////////////////
// CJetView diagnostics

#ifdef _DEBUG
void CG3DView::AssertValid() const
{
	CView::AssertValid();
}

void CG3DView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CGweDoc* CG3DView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGweDoc)));
	return (CGweDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CJetView message handlers

void CG3DView::OnViewType(UINT nID)
{
	m_nViewType = nID;		//View Type (Textured, Wire) also Menu ID
}// OnViewType

void CG3DView::OnUpdateViewType(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(pCmdUI->m_nID == m_nViewType);
}// OnUpdateViewType

grCamera* CG3DView::GetCamera()
{
	RECT ClientRect{};
	grRect CameraRect{};
	grXForm3d XRot_XForm{};

	if ((m_bRecalcCamera != FALSE) || (m_pCamera == nullptr))
	{
		GetClientRect(&ClientRect);
		CameraRect.Left = ClientRect.left;
		CameraRect.Right = ClientRect.right - 1;
		CameraRect.Top = ClientRect.top;
		CameraRect.Bottom = ClientRect.bottom - 1;

		if (m_pCamera == nullptr)
		{
			m_pCamera = grCamera_Create(2.0f, &CameraRect);
			//	tom morris feb 2005
			if (m_pCamera)
			{
				//	disable farplane clipping
				grCamera_SetFarClipPlane(m_pCamera, GR_FALSE, NULL);
			}
			else
			{
#pragma message("what happens when GetCamera fails?")
				return(nullptr);
			}
			//	end tom morris feb 2005
		}
		else
		{
			grCamera_SetAttributes(m_pCamera, 2.0f, &CameraRect);
		}

		//grXForm3d_SetFromLeftUpIn(&m_CameraXForm, &m_CameraLeft, &m_CameraUp, &m_CameraIn);
		grXForm3d_SetYRotation(&m_CameraXForm, m_CameraRotY);
		grXForm3d_SetXRotation(&XRot_XForm, m_CameraRotX);
		grXForm3d_Multiply(&m_CameraXForm, &XRot_XForm, &m_CameraXForm);
		grXForm3d_GetUp(&m_CameraXForm, &m_CameraUp);
		grXForm3d_GetLeft(&m_CameraXForm, &m_CameraLeft);
		grXForm3d_GetIn(&m_CameraXForm, &m_CameraIn);
		grXForm3d_Translate(&m_CameraXForm, m_CameraPos.X, m_CameraPos.Y, m_CameraPos.Z);
		if (grCamera_SetXForm(m_pCamera, &m_CameraXForm) == GR_FALSE)
		{
			return(nullptr);
		}

		m_bRecalcCamera = FALSE;
	}

	return(m_pCamera);
}

void CG3DView::OnSize(UINT nType, int cx, int cy)
{
	CG3DMfcView::OnSize(nType, cx, cy);

	m_bRecalcCamera = TRUE;
}

void CG3DView::OnMouseMove(UINT nFlags, CPoint point)
{
	/*	if( !m_bAnimate )
		{
			m_bAnimate = true;
			SetTimer( JETVIEW_TIMER, JETVIEW_PERIOD, nullptr );
			m_LastTime = Util_GetTime();
		}
	*/
	if (GetCapture() == this)
	{
		CPoint delta;

		if (false == m_bDragging &&
			(abs(point.x - m_Anchor.x) > m_CXDRAG ||
				abs(point.y - m_Anchor.y) > m_CYDRAG))
		{
			m_bDragging = true;
		}
		if (m_bDragging && m_bFlying)
		{
			if (!m_bCursorHidden)
			{
				ShowCursor(FALSE);
				m_bCursorHidden = true;
			}

			delta = m_FlyCursorHome - point;
			if (delta.x != 0 || delta.y != 0)
			{
				FlyLook(delta.x, delta.y);

				// Pin the cursor so mouse look never runs into the edge of the screen.
				CPoint ScreenPt = m_FlyCursorHome;
				ClientToScreen(&ScreenPt);
				::SetCursorPos(ScreenPt.x, ScreenPt.y);
			}
			m_Anchor = m_FlyCursorHome;
		}
		else if (m_bDragging && m_bOrbiting)
		{
			delta = m_Anchor - point;
			if (delta.x != 0 || delta.y != 0)
				OrbitCamera(delta.x, delta.y);
			m_Anchor = point;
		}
		else if (m_bDragging && m_bPanning)
		{
			// Grab-style pan: the world follows the cursor.
			long Scale = (nFlags & MK_SHIFT) ? (long)NAV_FAST_SCALE : 1;

			delta = m_Anchor - point;
			if (delta.x != 0)
				MoveCameraLeftRight(-delta.x * Scale);
			if (delta.y != 0)
				MoveCameraUpDown(-delta.y * Scale);
			m_Anchor = point;
		}
		else if (m_bDragging)
		{
			delta = m_Anchor - point; // the order is important to generate the desired motion

			if ((nFlags & (MK_LBUTTON | MK_RBUTTON)) == (MK_LBUTTON | MK_RBUTTON))
			{
				if (delta.x != 0)
				{
					MoveCameraLeftRight(delta.x);
					Invalidate();
				}
				if (delta.y != 0)
				{
					MoveCameraUpDown(delta.y);
					Invalidate();
				}
			}
			else if (nFlags & MK_LBUTTON)
			{
				if (delta.x != 0)
				{
					RotateCameraLeftRight(delta.x);
					Invalidate();
				}
				if (delta.y != 0)
				{
					MoveCameraInOut(delta.y);
					Invalidate();
				}
			}
			else if (nFlags & MK_RBUTTON)
			{
				if (delta.x != 0)
				{
					RotateCameraLeftRight(delta.x);
					Invalidate();
				}
				if (delta.y != 0)
				{
					RotateCameraUpDown(delta.y);
					Invalidate();
				}
			}

			m_Anchor = point;
		}

	}

	CG3DMfcView::OnMouseMove(nFlags, point);
}

void CG3DView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	// Clicking while flying or panning would steal the anchor from the active drag.
	if (m_bFlying || m_bPanning)
		return;

	m_Anchor = point;
	SetCapture();

	if (GetNavKeymap() == NavKeymap_Modern && (GetKeyState(VK_MENU) & 0x8000))
		BeginOrbit(point);

	CG3DMfcView::OnLButtonDown(nFlags, point);
}

void CG3DView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bOrbiting)
	{
		// An Alt+click orbit never selects.
		m_bOrbiting = false;
		m_bDragging = false;
		if (GetCapture() == this)
			ReleaseCapture();
	}
	else if (GetCapture() == this)
	{
		CGweDoc* Doc{};
		grBoolean	bControlHeld{};
		grBoolean	bSpaceHeld{};

		// Release only if the right button is up
		if (nFlags & MK_RBUTTON)
			return;
		ReleaseCapture();

		if (m_bDragging)
		{
			m_bDragging = false;
		}
		else
		{
			bSpaceHeld = Util_IsKeyDown(VK_SPACE);
			if (!bSpaceHeld)
			{
				bControlHeld = Util_IsKeyDown(VK_CONTROL);
				Doc = GetDocument();

				if (GR_FALSE == bControlHeld)
				{
					Doc->DeselectAll(TRUE);
				}
				Doc->Select3d(m_pCamera, (Point*)& point);
			}
		}
	}

	CG3DMfcView::OnLButtonUp(nFlags, point);
}

void CG3DView::ShowMenu(CPoint point)
{
	CMenu			ContextMenu;
	CMenu* SubMenu{};
	grDeviceCaps	DeviceCaps{};

	ClientToScreen(&point);
	ContextMenu.LoadMenu(IDR_3DVIEW);
	switch (m_RenderMode)
	{
	case RenderMode_Lines:
		ContextMenu.CheckMenuItem(ID_3DVIEW_LINES, MF_BYCOMMAND | MF_CHECKED);
		break;

	case RenderMode_Flat:
		ContextMenu.CheckMenuItem(ID_3DVIEW_FLAT, MF_BYCOMMAND | MF_CHECKED);
		break;

	case RenderMode_BSPSplits:
		ContextMenu.CheckMenuItem(ID_3DVIEW_BSPSPLITS, MF_BYCOMMAND | MF_CHECKED);
		break;

	case RenderMode_Textured:
		ContextMenu.CheckMenuItem(ID_3DVIEW_TEXTURED, MF_BYCOMMAND | MF_CHECKED);
		break;

	case RenderMode_TexturedAndLit:
		ContextMenu.CheckMenuItem(ID_3DVIEW_TEXTUREDWLIGHTS, MF_BYCOMMAND | MF_CHECKED);
		break;
	}

	if (grEngine_GetDeviceCaps(this->m_pEngine, &DeviceCaps))
	{
		uint32		DefaultRenderFlags;

		grEngine_GetDefaultRenderFlags(this->m_pEngine, &DefaultRenderFlags);

		// Use the Default Flags that the device wants us to use
		if (DefaultRenderFlags & GR_RENDER_FLAG_BILINEAR_FILTER)
			ContextMenu.CheckMenuItem(IDM_BILINEAR, MF_BYCOMMAND | MF_CHECKED);

		// Let them change it if the device says we can
		if (DeviceCaps.CanChangeRenderFlags & GR_RENDER_FLAG_BILINEAR_FILTER)
			ContextMenu.EnableMenuItem(IDM_BILINEAR, MF_BYCOMMAND | MF_ENABLED);
	}

	if (GetNavKeymap() == NavKeymap_Classic)
		ContextMenu.CheckMenuItem(ID_3DVIEW_CLASSICNAV, MF_BYCOMMAND | MF_CHECKED);


	SubMenu = ContextMenu.GetSubMenu(0);
	SubMenu->TrackPopupMenu(TPM_LEFTALIGN, point.x, point.y, this, nullptr);

}

void CG3DView::OnRButtonDown(UINT nFlags, CPoint point)
{
	//Rect MenuRect = { 4, 4, 128, 20 };  // Need to figure out true text box

	SetFocus();
	if (m_bOrbiting || m_bPanning)
		return;

	m_Anchor = point;
	SetCapture();

	// LMB+RMB stays the classic pan, so only a lone RMB starts flying.
	if (GetNavKeymap() == NavKeymap_Modern && !(nFlags & (MK_LBUTTON | MK_MBUTTON)))
		BeginFly(point);

	CG3DMfcView::OnRButtonDown(nFlags, point);
}

void CG3DView::OnRButtonUp(UINT nFlags, CPoint point)
{
	grBoolean OldAnimate{};

	if (m_bFlying)
	{
		bool bMoved = m_bDragging || m_bFlyMoved;

		EndFly();
		m_bDragging = false;
		if (GetCapture() == this)
			ReleaseCapture();

		// A plain right click (no look, no movement) still opens the context menu.
		if (!bMoved)
		{
			OldAnimate = m_bAnimate;
			Animate(GR_FALSE);
			ShowMenu(point);
			Animate(OldAnimate);
		}
	}
	else if (GetCapture() == this)
	{
		ReleaseCapture();
		if (m_bDragging)
		{
			m_bDragging = false;
		}
		else
		{
			OldAnimate = m_bAnimate;
			Animate(GR_FALSE);
			ShowMenu(point);
			Animate(OldAnimate);
		}
	}

	CG3DMfcView::OnRButtonUp(nFlags, point);
}

void CG3DView::MoveCameraUpDown(long Delta)
{
	CGweDoc* Doc{};
	grVec3d	  Offset{};

	Doc = GetDocument();

	grVec3d_Set(&Offset, 0.0f, (grFloat)Delta, 0.0f);
	Doc->TranslateCurCam(&Offset);
}

void CG3DView::MoveCameraLeftRight(long Delta)
{
	CGweDoc* Doc{};
	grVec3d	  Offset{};

	Doc = GetDocument();

	grVec3d_Set(&Offset, (grFloat)-Delta, 0.0f, 0.0f);
	Doc->TranslateCurCam(&Offset);

}

void CG3DView::MoveCameraInOut(long Delta)
{
	CGweDoc* Doc{};
	grVec3d	  Offset{};

	Doc = GetDocument();

	grVec3d_Set(&Offset, 0.0f, 0.0f, (grFloat)-Delta);
	Doc->TranslateCurCam(&Offset);
}


void CG3DView::RotateCameraLeftRight(long Delta)
{

	CGweDoc* Doc{};

	Doc = GetDocument();

	Doc->RotCurCamY((grFloat)Delta / 150.0f);

}

void CG3DView::RotateCameraUpDown(long Delta)
{
	CGweDoc* Doc{};

	Doc = GetDocument();

	Doc->RotCurCamX((grFloat)Delta / 150.0f);

}

void CG3DView::OnInitialUpdate()
{
	CGweDoc* Doc{};
	grWorld* pWorld{};


	Doc = GetDocument();

	pWorld = Doc->GetWorld();
	ASSERT(pWorld != nullptr);

	if (!grWorld_SetEngine(pWorld, m_pEngine))
	{
		DestroyWindow();
	}
	else
	{
		// FM: This was getting called after DestroyWindow - caused problems
		//grEngine_SetGamma(m_pEngine, 1.0f);	//trilobite orig
		grEngine_SetGamma(m_pEngine, G3DVIEW_GAMMA);	//trilobite revise
		grEngine_UpdateGamma(m_pEngine);	//trilobite add
		CG3DMfcView::OnInitialUpdate();
	}


	// TODO: Add your specialized code here and/or call the base class

}

void CG3DView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	Invalidate();
	pSender; lHint; pHint;
}

void CG3DView::SetCameraPos(grVec3d* Pos)
{
	m_CameraPos = *Pos;
	m_bRecalcCamera = TRUE;
	Invalidate();
}

grBoolean CG3DView::RegisterBitmap(grBitmap* pBitmap)
{
	return(grEngine_AddBitmap(m_pEngine, pBitmap, GR_ENGINE_BITMAP_TYPE_3D));
}

void CG3DView::OnViewCenterselction()
{
	CGweDoc* pDoc{};

	pDoc = GetDocument();
	ASSERT(pDoc != nullptr);

	pDoc->CenterViewsOnSelection();

	// Move the camera on the selection
}

void CG3DView::Animate(grBoolean bAnimate)
{
	if (!this)
		return;
	if (bAnimate && !m_bAnimate)
	{
		m_bAnimate = true;
		SetTimer(G3DVIEW_TIMER, G3DVIEW_PERIOD, nullptr);
		m_LastTime = Util_GetTime();
	}
	else
		if (!bAnimate && m_bAnimate)
		{
			m_bAnimate = false;
			KillTimer(G3DVIEW_TIMER);
			return;
		}
}

void CG3DView::OnTimer(UINT nIDEvent)
{

	// locals
	CGweDoc* pDoc{};
	POINT			ptCursor{};
	CRect	r{};
	float			CurTime{};
	//MSG	Msg;

	if (nIDEvent == G3DVIEW_FLYTIMER)
	{
		UpdateFly();
		return;
	}

	pDoc = GetDocument();
	ASSERT(pDoc != nullptr);

	::GetCursorPos(&ptCursor);
	ScreenToClient(&ptCursor);
	GetClientRect(&r);

	/*
		if( !r.PtInRect( ptCursor ) )
		{
			m_bAnimate = false;
			KillTimer( nIDEvent );
			return;
		}
	*/
	CurTime = Util_GetTime();

	//Update Time Delta
	pDoc->UpdateTimeDelta(CurTime - m_LastTime);
	m_LastTime = CurTime;

	// force redraw
	Invalidate(FALSE);

	// default call
	CG3DMfcView::OnTimer(nIDEvent);
}

char* CG3DView::GetModeName()
{
	char* pszViewName = nullptr;

	switch (m_RenderMode)
	{
	case RenderMode_Lines:
		pszViewName = Util_LoadLocalRcString(IDS_RENDER_LINES);
		break;

	case RenderMode_Flat:
		pszViewName = Util_LoadLocalRcString(IDS_RENDER_FLAT);
		break;

	case RenderMode_BSPSplits:
		pszViewName = Util_LoadLocalRcString(IDS_RENDER_BSPSPLITS);
		break;

	case RenderMode_Textured:
		pszViewName = Util_LoadLocalRcString(IDS_RENDER_TEXTURED);
		break;

	case RenderMode_TexturedAndLit:
		pszViewName = Util_LoadLocalRcString(IDS_RENDER_TEXT_LIT);
		break;

	}
	return(pszViewName);
}

void CG3DView::OnDraw(CDC* pDC)
{
	//	char * pszViewName;
		//int nOldMode;

	CG3DMfcView::OnDraw(pDC);
	/*
		pszViewName = GetModeName( ) ;
		if( pszViewName == nullptr )
			return;
		nOldMode = pDC->SetBkMode( TRANSPARENT ) ;
		pDC->ExtTextOut( 4, 4, 0,  nullptr, pszViewName, strlen( pszViewName ), nullptr );
		pDC->SetBkMode( nOldMode ) ;
		grRam_Free( pszViewName );
	*/
	int32 mkfaces = 0, mgfaces = 0, subfaces = 0, drawfaces = 0;
	CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();
	grEngine_GetBSPDebugInfo(m_pEngine, &mkfaces, &mgfaces, &subfaces, &drawfaces);
	pMainFrame->Set3DViewStats(grEngine_GetFPS(m_pEngine), drawfaces);
}

void CG3DView::On3dviewLines()
{
	CGweDoc* Doc{};

	Doc = GetDocument();
	if (Doc->SetRenderMode(RenderMode_Lines))
	{
		m_RenderMode = RenderMode_Lines;
		Invalidate();
	}
}

void CG3DView::On3dviewTextured()
{
	CGweDoc* Doc{};

	Doc = GetDocument();
	if (Doc->SetRenderMode(RenderMode_Textured))
	{
		m_RenderMode = RenderMode_Textured;
		Invalidate();
	}

}

void CG3DView::On3dviewTexturedwlights()
{
	CGweDoc* Doc{};

	Doc = GetDocument();
	if (Doc->SetRenderMode(RenderMode_TexturedAndLit))
	{
		m_RenderMode = RenderMode_TexturedAndLit;
		Invalidate();
	}

}

void CG3DView::On3dviewFlat()
{
	CGweDoc* Doc{};

	Doc = GetDocument();
	if (Doc->SetRenderMode(RenderMode_Flat))
	{
		m_RenderMode = RenderMode_Flat;
		Invalidate();
	}

}

void CG3DView::On3dviewBspsplits()
{
	CGweDoc* Doc{};

	Doc = GetDocument();
	if (Doc->SetRenderMode(RenderMode_BSPSplits))
	{
		m_RenderMode = RenderMode_BSPSplits;
		Invalidate();
	}

}








/////////////////////////////////////////////////////////////////////////////
// Modern navigation (roadmap Track A)
//
//	Hold RMB		fly: mouse look, WASD move, Q/E down/up, Shift fast, wheel changes speed
//	Alt + LMB		orbit around the selection (or a point ahead of the camera)
//	MMB drag		pan
//	Wheel			dolly
//	F / Shift+F		frame the selection / the whole level
//
// The Classic Genesis keymap turns all of this off and keeps the original controls.

static float Nav_ClampPitch(float XRot)
{
	if (XRot > NAV_PITCH_LIMIT)
		return NAV_PITCH_LIMIT;
	if (XRot < -NAV_PITCH_LIMIT)
		return -NAV_PITCH_LIMIT;
	return XRot;
}

// Same rotation the editor camera object builds from its X/Y angles.
static void Nav_CamXFormFromRot(float XRot, float YRot, grXForm3d* pXForm)
{
	grXForm3d XRot_XForm{};

	grXForm3d_SetYRotation(pXForm, YRot);
	grXForm3d_SetXRotation(&XRot_XForm, XRot);
	grXForm3d_Multiply(pXForm, &XRot_XForm, pXForm);
}

CG3DView::NavKeymap CG3DView::GetNavKeymap(void)
{
	if (s_NavKeymap < 0)
	{
		CString Keymap = AfxGetApp()->GetProfileString(s_pszNavSection, s_pszNavKeymapKey, "Modern");

		s_NavKeymap = (Keymap.CompareNoCase("Classic") == 0) ? NavKeymap_Classic : NavKeymap_Modern;
	}
	return (NavKeymap)s_NavKeymap;
}

void CG3DView::SetNavKeymap(NavKeymap Keymap)
{
	s_NavKeymap = Keymap;
	AfxGetApp()->WriteProfileString(s_pszNavSection, s_pszNavKeymapKey, (Keymap == NavKeymap_Classic) ? "Classic" : "Modern");
}

void CG3DView::On3dviewClassicNav()
{
	SetNavKeymap((GetNavKeymap() == NavKeymap_Classic) ? NavKeymap_Modern : NavKeymap_Classic);
}

BOOL CG3DView::PreTranslateMessage(MSG* pMsg)
{
	// While flying, WASD/QE/Shift drive the camera; keep them away from the accelerators.
	if (m_bFlying && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
		return TRUE;

	if (m_bEatAltUp && pMsg->message == WM_SYSKEYUP && pMsg->wParam == VK_MENU)
	{
		m_bEatAltUp = false;
		return TRUE;
	}

	return CG3DMfcView::PreTranslateMessage(pMsg);
}

void CG3DView::OnCaptureChanged(CWnd* pWnd)
{
	// Losing capture (Alt+Tab, a dialog) must not leave a drag running or the cursor hidden.
	if (pWnd != this)
	{
		EndFly();
		EndNavDrag();
		m_bDragging = false;
	}

	CG3DMfcView::OnCaptureChanged(pWnd);
}

void CG3DView::BeginFly(CPoint point)
{
	m_bFlying = true;
	m_bFlyMoved = false;
	m_FlyCursorHome = point;
	grVec3d_Clear(&m_FlyVelocity);
	QueryPerformanceCounter(&m_FlyLastTick);
	SetTimer(G3DVIEW_FLYTIMER, G3DVIEW_FLYPERIOD, nullptr);
}

void CG3DView::EndFly(void)
{
	if (!m_bFlying)
		return;

	m_bFlying = false;
	KillTimer(G3DVIEW_FLYTIMER);
	grVec3d_Clear(&m_FlyVelocity);

	if (m_bCursorHidden)
	{
		ShowCursor(TRUE);
		m_bCursorHidden = false;
	}
}

void CG3DView::EndNavDrag(void)
{
	m_bOrbiting = false;
	m_bPanning = false;
}

void CG3DView::FlyLook(long DeltaX, long DeltaY)
{
	CGweDoc* pDoc = GetDocument();
	grXForm3d XForm{};
	float XRot = 0.0f, YRot = 0.0f;

	if (!pDoc->GetCurCamXForm(&XForm))
		return;

	pDoc->GetCurCamXYRot(&XRot, &YRot);
	YRot += (float)DeltaX * NAV_LOOK_SCALE;
	XRot = Nav_ClampPitch(XRot + (float)DeltaY * NAV_LOOK_SCALE);
	pDoc->SetCurCam(XRot, YRot, &XForm.Translation);
}

void CG3DView::UpdateFly(void)
{
	CGweDoc* pDoc = GetDocument();
	LARGE_INTEGER Now{}, Freq{};
	grXForm3d XForm{};
	grVec3d In{}, Left{}, Wish{}, Diff{}, Pos{};
	float XRot = 0.0f, YRot = 0.0f;
	float TimeDelta, Blend;
	bool bInput;

	QueryPerformanceCounter(&Now);
	QueryPerformanceFrequency(&Freq);
	TimeDelta = (float)(Now.QuadPart - m_FlyLastTick.QuadPart) / (float)Freq.QuadPart;
	m_FlyLastTick = Now;
	if (TimeDelta <= 0.0f)
		return;
	if (TimeDelta > 0.1f)
		TimeDelta = 0.1f;

	if (!pDoc->GetCurCamXForm(&XForm))
		return;

	grXForm3d_GetIn(&XForm, &In);
	grXForm3d_GetLeft(&XForm, &Left);

	if (Util_IsKeyDown('W'))
		grVec3d_Add(&Wish, &In, &Wish);
	if (Util_IsKeyDown('S'))
		grVec3d_Subtract(&Wish, &In, &Wish);
	if (Util_IsKeyDown('A'))
		grVec3d_Add(&Wish, &Left, &Wish);
	if (Util_IsKeyDown('D'))
		grVec3d_Subtract(&Wish, &Left, &Wish);
	// Up and down are along the world axis, so Q/E never drift with the pitch.
	if (Util_IsKeyDown('E'))
		Wish.Y += 1.0f;
	if (Util_IsKeyDown('Q'))
		Wish.Y -= 1.0f;

	bInput = grVec3d_LengthSquared(&Wish) > 0.0001f;
	if (bInput)
	{
		m_bFlyMoved = true;
		grVec3d_Normalize(&Wish);
		grVec3d_Scale(&Wish, NAV_FLY_SPEED * m_FlySpeedScale * (Util_IsKeyDown(VK_SHIFT) ? NAV_FAST_SCALE : 1.0f), &Wish);
	}

	// Ease toward the wished velocity so starts and stops are smooth but still responsive.
	Blend = 1.0f - expf(-NAV_ACCEL * TimeDelta);
	grVec3d_Subtract(&Wish, &m_FlyVelocity, &Diff);
	grVec3d_AddScaled(&m_FlyVelocity, &Diff, Blend, &m_FlyVelocity);

	if (!bInput && grVec3d_Length(&m_FlyVelocity) < 1.0f)
	{
		grVec3d_Clear(&m_FlyVelocity);
		return;
	}

	grVec3d_AddScaled(&XForm.Translation, &m_FlyVelocity, TimeDelta, &Pos);
	pDoc->GetCurCamXYRot(&XRot, &YRot);
	pDoc->SetCurCam(XRot, YRot, &Pos);
}

void CG3DView::BeginOrbit(CPoint point)
{
	CGweDoc* pDoc = GetDocument();
	grExtBox SelBounds{};
	grXForm3d XForm{};
	grVec3d In{};

	if (pDoc->HasSelections(&SelBounds))
	{
		grExtBox_GetTranslation(&SelBounds, &m_OrbitPivot);
	}
	else if (pDoc->GetCurCamXForm(&XForm))
	{
		grXForm3d_GetIn(&XForm, &In);
		grVec3d_AddScaled(&XForm.Translation, &In, NAV_ORBIT_DISTANCE, &m_OrbitPivot);
	}
	else
	{
		return;
	}

	m_bOrbiting = true;
	m_bEatAltUp = true;
	m_Anchor = point;
}

void CG3DView::OrbitCamera(long DeltaX, long DeltaY)
{
	CGweDoc* pDoc = GetDocument();
	grXForm3d XForm{}, NewXForm{};
	grVec3d Offset{}, Left{}, Up{}, In{}, Pos{};
	float XRot = 0.0f, YRot = 0.0f;
	float OffLeft, OffUp, OffIn;

	if (!pDoc->GetCurCamXForm(&XForm))
		return;

	// Express the camera's offset from the pivot in camera space, turn the camera, then
	// rebuild the offset from the new axes. The pivot keeps its place on screen.
	grVec3d_Subtract(&XForm.Translation, &m_OrbitPivot, &Offset);
	grXForm3d_GetLeft(&XForm, &Left);
	grXForm3d_GetUp(&XForm, &Up);
	grXForm3d_GetIn(&XForm, &In);
	OffLeft = grVec3d_DotProduct(&Offset, &Left);
	OffUp = grVec3d_DotProduct(&Offset, &Up);
	OffIn = grVec3d_DotProduct(&Offset, &In);

	pDoc->GetCurCamXYRot(&XRot, &YRot);
	YRot += (float)DeltaX * NAV_LOOK_SCALE;
	XRot = Nav_ClampPitch(XRot + (float)DeltaY * NAV_LOOK_SCALE);

	Nav_CamXFormFromRot(XRot, YRot, &NewXForm);
	grXForm3d_GetLeft(&NewXForm, &Left);
	grXForm3d_GetUp(&NewXForm, &Up);
	grXForm3d_GetIn(&NewXForm, &In);

	Pos = m_OrbitPivot;
	grVec3d_AddScaled(&Pos, &Left, OffLeft, &Pos);
	grVec3d_AddScaled(&Pos, &Up, OffUp, &Pos);
	grVec3d_AddScaled(&Pos, &In, OffIn, &Pos);
	pDoc->SetCurCam(XRot, YRot, &Pos);
}

void CG3DView::FrameBounds(const grExtBox* pBounds)
{
	CGweDoc* pDoc = GetDocument();
	grXForm3d XForm{};
	grVec3d Center{}, Size{}, In{}, Pos{};
	float XRot = 0.0f, YRot = 0.0f;
	float Radius, FOV, HalfH, HalfV, Half;
	CRect Client;

	if (!pDoc->GetCurCamXForm(&XForm))
		return;

	grExtBox_GetTranslation(pBounds, &Center);
	grVec3d_Subtract(&pBounds->Max, &pBounds->Min, &Size);
	Radius = 0.5f * grVec3d_Length(&Size);
	if (Radius < 16.0f)
		Radius = 16.0f;

	// The camera FOV is horizontal; fit the bounding sphere in the narrower direction.
	FOV = pDoc->GetCurCamFOV();
	if (FOV < 0.2f)
		FOV = 0.2f;
	if (FOV > 3.0f)
		FOV = 3.0f;
	HalfH = FOV * 0.5f;
	HalfV = HalfH;
	GetClientRect(&Client);
	if (Client.Width() > 0 && Client.Height() > 0)
		HalfV = atanf(tanf(HalfH) * (float)Client.Height() / (float)Client.Width());
	Half = (HalfV < HalfH) ? HalfV : HalfH;

	grXForm3d_GetIn(&XForm, &In);
	grVec3d_AddScaled(&Center, &In, -Radius / sinf(Half), &Pos);
	pDoc->GetCurCamXYRot(&XRot, &YRot);
	pDoc->SetCurCam(XRot, YRot, &Pos);
}

void CG3DView::OnMButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();
	if (GetNavKeymap() == NavKeymap_Modern && !m_bFlying && !m_bOrbiting && GetCapture() != this)
	{
		m_bPanning = true;
		m_Anchor = point;
		SetCapture();
	}

	CG3DMfcView::OnMButtonDown(nFlags, point);
}

void CG3DView::OnMButtonUp(UINT nFlags, CPoint point)
{
	if (m_bPanning)
	{
		m_bPanning = false;
		m_bDragging = false;
		if (GetCapture() == this)
			ReleaseCapture();
	}

	CG3DMfcView::OnMButtonUp(nFlags, point);
}

BOOL CG3DView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (GetNavKeymap() == NavKeymap_Modern)
	{
		float Notches = (float)zDelta / (float)WHEEL_DELTA;

		if (m_bFlying)
		{
			m_FlySpeedScale *= powf(1.25f, Notches);
			if (m_FlySpeedScale < NAV_SPEED_MIN)
				m_FlySpeedScale = NAV_SPEED_MIN;
			if (m_FlySpeedScale > NAV_SPEED_MAX)
				m_FlySpeedScale = NAV_SPEED_MAX;
		}
		else
		{
			long Step = (long)(Notches * NAV_DOLLY_STEP * ((nFlags & MK_SHIFT) ? NAV_FAST_SCALE : 1.0f));

			if (Step != 0)
				MoveCameraInOut(Step);
		}
		return TRUE;
	}

	return CG3DMfcView::OnMouseWheel(nFlags, zDelta, pt);
}

void CG3DView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (GetNavKeymap() == NavKeymap_Modern && nChar == 'F' && !(GetKeyState(VK_CONTROL) & 0x8000))
	{
		CGweDoc* pDoc = GetDocument();
		grExtBox Bounds{};

		if (GetKeyState(VK_SHIFT) & 0x8000)
		{
			if (pDoc->GetLevelBounds(&Bounds))
				FrameBounds(&Bounds);
		}
		else if (pDoc->HasSelections(&Bounds))
		{
			FrameBounds(&Bounds);
		}
		return;
	}

	CG3DMfcView::OnKeyDown(nChar, nRepCnt, nFlags);
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	FullscreenWndProc()
//
//	WndProc function for the full screen mode window.
//
////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK FullscreenWndProc(
	HWND	hWnd,
	UINT	iMessage,
	WPARAM	wParam,
	LPARAM	lParam)
{

	// process messages
	switch (iMessage)
	{
	case WM_KEYDOWN:
	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
	case WM_LBUTTONDOWN:
	{
		break;
	}

	default:
	{
		return DefWindowProc(hWnd, iMessage, wParam, lParam);
	}
	}

	// all done
	return 0;

} // FullscreenWndProc()



////////////////////////////////////////////////////////////////////////////////////////
//
//	FullscreenDestroyWindow()
//
//	Destroy the full screen window.
//
////////////////////////////////////////////////////////////////////////////////////////
static void FullscreenDestroyWindow(
	HWND* hWnd,	// window to destroy
	WNDCLASS* WC)	// its info struct
{
	
	// ensure valid data
	ASSERT(hWnd != nullptr);
	ASSERT(WC != nullptr);

	// destroy full screen window
	if (::DestroyWindow(*hWnd))
	{
		hWnd = nullptr;
	}
	else
	{
		MessageBox(NULL, "Failed to destroy Fullscreen Window", "INTERNAL ERROR", MB_OK);
	}
	// unregister its class
	::UnregisterClass(WC->lpszClassName, WC->hInstance);
	memset(WC, 0, sizeof(*WC));

} // FullscreenDestroyWindow()



////////////////////////////////////////////////////////////////////////////////////////
//
//	FullscreenCreateWindow()
//
//	Create a window for full screen mode.
//
////////////////////////////////////////////////////////////////////////////////////////
static grBoolean FullscreenCreateWindow(
	HWND* SavehWnd,
	WNDCLASS* SaveWC,
	int			Width,
	int			Height)
{

	// locals
	HWND		hWnd{};
	WNDCLASS	wc{};
	RECT		WindowRect{};

	// ensure valid data
	ASSERT(SavehWnd != nullptr);
	ASSERT(SaveWC != nullptr);
	ASSERT(Width > 0);
	ASSERT(Height > 0);

	// zap passed data
	*SavehWnd = nullptr;

	// setup wndclass struct
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = FullscreenWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = AfxGetInstanceHandle();
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = "JDesignerClassic11 for Jet3D: Fullscreen";

	// register window
	if (RegisterClass(&wc) == 0)
	{
		return GR_FALSE;
	}

	// create window
	hWnd = CreateWindowEx(0,
		wc.lpszClassName,
		wc.lpszClassName,
		0,
		0, 0, Width - 1, Height - 1,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);
	if (hWnd == nullptr)
	{
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return GR_FALSE;
	}

	UpdateWindow(hWnd);
	// set focus
	::SetFocus(hWnd);

	SetWindowLong(hWnd,
		GWL_STYLE,
		GetWindowLong(hWnd, GWL_STYLE) & ~WS_POPUP);

	SetWindowLong(hWnd,
		GWL_STYLE,
		GetWindowLong(hWnd, GWL_STYLE) | (WS_OVERLAPPED |
			WS_CAPTION |
			WS_SYSMENU |
			WS_MINIMIZEBOX));

	SetWindowLong(hWnd,
		GWL_STYLE,
		GetWindowLong(hWnd, GWL_STYLE) | WS_THICKFRAME |
		WS_MAXIMIZEBOX);

	SetWindowLong(hWnd,
		GWL_EXSTYLE,
		GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_TOPMOST);

	WindowRect.left = 0;
	WindowRect.top = 0;
	WindowRect.right = Width + WindowRect.left - 1;
	WindowRect.bottom = Height + WindowRect.top - 1;

	AdjustWindowRect(&WindowRect, GetWindowLong(hWnd, GWL_STYLE) | GetWindowLong(hWnd, GWL_EXSTYLE), FALSE);

	SetWindowPos(hWnd,
		HWND_TOP,
		40 + WindowRect.left,
		40 + WindowRect.top,
		(WindowRect.right - WindowRect.left) + 1,
		(WindowRect.bottom - WindowRect.top) + 1,
		SWP_NOCOPYBITS | SWP_NOZORDER);

	//
	// Make window visible
	//
	ShowWindow(hWnd, SW_SHOWNORMAL);

	// all done
	*SavehWnd = hWnd;
	*SaveWC = wc;
	return GR_TRUE;

} // FullscreenCreateWindow()



////////////////////////////////////////////////////////////////////////////////////////
//
//	FullscreenProcess()
//
//	Process full screen mode.
//
////////////////////////////////////////////////////////////////////////////////////////
void FullscreenProcess(
	CGweDoc* pDoc,
	grEngine* Engine,
	grWorld* World,
	grObject* CamObject,
	float* XRot,
	float* YRot,
	int		Width,
	int		Height)
{

	// locals
	grBoolean	Result{};
	grVec3d		CameraRot{};
	grBoolean	FullScreen{};
	grBoolean   Wireframe{};
	grBoolean	DisplayInfo{};
	grXForm3d	FSXf{};
	grCamera* FSCamera{};
	float		LastTime{}, CurTime{}, TimeDelta{};

	// setup camera
	{

		// locals
		grRect	CameraRect{};

		// setup camera rect
		CameraRect.Left = 0;
		CameraRect.Right = Width - 1;
		CameraRect.Top = 0;
		CameraRect.Bottom = Height - 1;

		// create camera
		FSCamera = grCamera_Create(2.0f, &CameraRect);
		grCamera_SetAttributes(FSCamera, 2.0f, &CameraRect);

		//	tom morris	feb 2005
		//	disable farplane clipping
		grCamera_SetFarClipPlane(FSCamera, GR_FALSE, NULL);
		//	end tom morris feb 2005

		// set default location
		grVec3d_Set(&CameraRot, *XRot, *YRot, 0.0f);
		grObject_GetXForm(CamObject, &FSXf);
		grCamera_SetXForm(FSCamera, &FSXf);
	}

	// loop untill quit
	DisplayInfo = GR_TRUE;
	FullScreen = GR_TRUE;
	Wireframe = GR_FALSE;
	LastTime = (float)Util_Time() * 0.001f;
	TimeDelta = 0;
	int32 mkfaces = 0; // make faces
	int32 mgfaces = 0; // merge faces
	int32 subfaces = 0; // subdivide faces
	int32 drawfaces = 0; // draw faces

	/* This starts the rendering loop.
	It will continue while the boolean variable, Fullscreen, remains TRUE.
	When the Escape key is pressed, that sets the boolean variable, Fullscreen, to False.
	*/
	while (FullScreen == GR_TRUE)
	{
		// get time delta
		CurTime = (float)Util_Time() * 0.001f;
		TimeDelta = CurTime - LastTime;
		LastTime = CurTime;

		grObject_GetXForm(CamObject, &FSXf);
		// get keyboard input
		
		//	by trilobite jan. 2011
		//if ( Util_IsKeyDown( VK_DOWN ) )
		if (Util_IsKeyDown(VK_DOWN) || Util_IsKeyDown(0x53))	//	S
		{
			grVec3d	In{};
			grXForm3d_GetIn(&FSXf, &In);
			grVec3d_AddScaled(&(FSXf.Translation), &In, -300.0f * TimeDelta, &(FSXf.Translation));
			grCamera_SetXForm(FSCamera, &FSXf);
		}
		//	by trilobite jan. 2011
		//if ( Util_IsKeyDown( VK_UP ) )
		if (Util_IsKeyDown(VK_UP) || Util_IsKeyDown(0x57))	//	W
		{
			grVec3d	In{};
			grXForm3d_GetIn(&FSXf, &In);
			grVec3d_AddScaled(&(FSXf.Translation), &In, 300.0f * TimeDelta, &(FSXf.Translation));
			grCamera_SetXForm(FSCamera, &FSXf);
		}
		//	by trilobite jan. 2011
		//if ( Util_IsKeyDown( VK_RIGHT ) )
		if (Util_IsKeyDown(VK_RIGHT) || Util_IsKeyDown(0x44))	//	D
		{
			grVec3d	In{};
			grXForm3d_GetLeft(&FSXf, &In);
			grVec3d_AddScaled(&(FSXf.Translation), &In, -300.0f * TimeDelta, &(FSXf.Translation));
			grCamera_SetXForm(FSCamera, &FSXf);
		}
		//	by trilobite jan. 2011
		//if ( Util_IsKeyDown( VK_LEFT ) )
		if (Util_IsKeyDown(VK_LEFT) || Util_IsKeyDown(0x41))	//	A
		{
			grVec3d	In{};
			grXForm3d_GetLeft(&FSXf, &In);
			grVec3d_AddScaled(&(FSXf.Translation), &In, 300.0f * TimeDelta, &(FSXf.Translation));
			grCamera_SetXForm(FSCamera, &FSXf);
		}
		if (Util_IsKeyDown(VK_RETURN))
		{
			DisplayInfo = !DisplayInfo;
		}
		if (Util_IsKeyDown(VK_SPACE))
		{
			Wireframe = !Wireframe;
			pDoc->SetRenderMode(Wireframe ? RenderMode_Lines : RenderMode_TexturedAndLit);
		}

		// End the loop here.
		if (Util_IsKeyDown(VK_ESCAPE))
		{
			FullScreen = GR_FALSE;
		}


		// get mouse input
		{

			// locals
			POINT	Pt{};
			int		HalfWidth{}, HalfHeight{};

			// get half sizes
			HalfWidth = Width / 2;
			HalfHeight = Height / 2;

			// get mouse delta
			GetCursorPos(&Pt);
			if (Pt.x != HalfWidth || Pt.y != HalfHeight)
			{
				grXForm3d	XForm{};
				grVec3d		Pos{};

				SetCursorPos(HalfWidth, HalfHeight);
				SetCursor(nullptr);

				// adjust camera rotation
				CameraRot.Y += (((float)(Pt.x - HalfWidth) / (float)HalfWidth * GR_PI) * -0.2f);
				CameraRot.Y = (float)fmod(CameraRot.Y, GR_TWOPI);
				CameraRot.X += (((float)(Pt.y - HalfHeight) / (float)HalfHeight * GR_PI) * -0.2f);
				CameraRot.X = (float)fmod(CameraRot.X, GR_TWOPI);

				// do that funky math
				grVec3d_Copy(&(FSXf.Translation), &Pos);
				grVec3d_Set(&(FSXf.Translation), 0.0f, 0.0f, 0.0f);
				grXForm3d_SetXRotation(&XForm, CameraRot.X);
				grXForm3d_SetYRotation(&FSXf, CameraRot.Y);
				grXForm3d_Multiply(&FSXf, &XForm, &FSXf);
				grXForm3d_Translate(&FSXf, Pos.X, Pos.Y, Pos.Z);
			}
		}

		// update camera xf
		{

			// locals
			grObject_SetXForm(CamObject, &FSXf);
			grCamera_SetXForm(FSCamera, &FSXf);
		}

		// update objects
		Result = grWorld_Frame(World, TimeDelta);

		// output other info -
		//Note: parameters for FONT: Font type?(see ID3DXFONT docs), horizonal pos, vertical pos - Ken Deel
		if (DisplayInfo == GR_TRUE)
		{
			grVec3d	Angles{};
			//	by trilobite jan. 2011 -- colors are not being traanslated as expected. substituting generic that seems to work.
			//grEngine_Printf( Engine, 0, 10, 40, GR_COLOR_XRGB(255, 255, 255), "Loc: %.1f, %.1f, %.1f", FSXf.Translation.X, FSXf.Translation.Y, FSXf.Translation.Z );
			grEngine_Printf(Engine, 0, 10, 40, GR_COLOR_COLORVALUE(100, 100, 100, 100), "Loc: %.1f, %.1f, %.1f", FSXf.Translation.X, FSXf.Translation.Y, FSXf.Translation.Z);
			grXForm3d_GetEulerAngles(&FSXf, &Angles);
			//grEngine_Printf( Engine, 0, 10, 50, GR_COLOR_XRGB(255, 255, 255), "Orient: %.0f, %.0f, %.0f", grFloat_RadToDeg( Angles.X ), grFloat_RadToDeg( Angles.Y ), grFloat_RadToDeg( Angles.Z ) );
			grEngine_Printf(Engine, 0, 10, 50, GR_COLOR_COLORVALUE(100, 100, 100, 100), "Orient: %.0f, %.0f, %.0f", grFloat_RadToDeg(Angles.X), grFloat_RadToDeg(Angles.Y), grFloat_RadToDeg(Angles.Z));
			if (TimeDelta > 0.0f)
			{

				// Not Good, changed JH 25.4.2000
				Fullscreen_Framerate = (Fullscreen_Framerate / 100 * 95) + ((1.0f / TimeDelta) / 100 * 5);
				grEngine_GetBSPDebugInfo(Engine, &mkfaces, &mgfaces, &subfaces, &drawfaces);
				//	by trilobite jan. 2011
				//grEngine_Printf( Engine, 0, 10, 60, GR_COLOR_XRGB(255, 255, 255), "FPS: %.2f    DrawFaces: %d ",Fullscreen_Framerate, drawfaces);
				grEngine_Printf(Engine, 0, 10, 60, GR_COLOR_COLORVALUE(100, 100, 100, 100), "FPS: %.2f    DrawFaces: %d ", Fullscreen_Framerate, drawfaces);
			}
		}
		// render world
		grEngine_BeginFrame(Engine, FSCamera, GR_TRUE);
		Result = grWorld_Render(World, FSCamera, nullptr);
		//	by trilobite jan. 2011
		//grEngine_Printf(Engine, 0, 10, 70, GR_COLOR_XRGB(255, 255, 255), "jDesigner3D 2.5.1   Press <ESC> to return to jDesigner3D.");
		grEngine_Printf(Engine, 0, 10, 70, GR_COLOR_COLORVALUE(100, 100, 100, 100), "jDesignerClassic7 2.7.1.1   Press <ESC> to return to editor.");
		grEngine_EndFrame(Engine);

		// clear message queue	//undone
		{
			MSG	Msg{};
			while (PeekMessage(&Msg, nullptr, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE) != 0);
		}
	}	//	 end while...
	*XRot = CameraRot.X;
	*YRot = CameraRot.Y;
} // FullscreenProcess()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::ChooseWindowVideoSettings()
//
//	Choose video settings for window mode.
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::ChooseWindowVideoSettings()
{

	// locals
	grDriver		*LastDriver{};
	grDriver_Mode	*LastMode{};
	grBoolean		Result{};

	// save last driver and mode
	grEngine_GetDriverAndMode(m_pEngine, &LastDriver, &LastMode);
	/*	WindowDriver = LastDriver;
		WindowMode = LastMode;
	*/
	// display video mode dialog box

	if ((WindowDriver == nullptr) || (WindowMode == nullptr))
	{
		Result = DrvList_PickDriver(AfxGetInstanceHandle(),
			GetSafeHwnd(),
			m_pEngine,
			&WindowDriver,
			&WindowMode,
			GR_TRUE,
			DRVLIST_WINDOW | DRVLIST_SOFTWARE | DRVLIST_HARDWARE);

		// do nothing if no mode was picked
		if (Result == GR_FALSE)
		{
			return GR_TRUE;
		}
	}

	// do nothing if previous and current video settings are the same
	if ((LastDriver == WindowDriver) && (LastMode == WindowMode))
	{
		return GR_TRUE;
	}

	// set new mode
	if (grEngine_SetDriverAndMode(m_pEngine, GetSafeHwnd(), WindowDriver, WindowMode) == GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::ChooseWindowVideoSettings", "Failed to set new video mode");
		grEngine_SetDriverAndMode(m_pEngine, GetSafeHwnd(), LastDriver, LastMode);
		return GR_FALSE;
	}
	Invalidate(TRUE);
	// all done
	return GR_TRUE;

} // CJetView::ChooseWindowVideoSettings()


////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::ChooseFullscreenVideoSettings()
//
//	Choose video settings for fullscreen mode.
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::ChooseFullscreenVideoSettings()
{

	// display video mode dialog box
	if (!DrvList_PickDriver(AfxGetInstanceHandle(),
		GetSafeHwnd(),
		m_pEngine,
		&FullscreenDriver,
		&FullscreenMode,
		GR_TRUE,
		DRVLIST_FULLSCREEN | DRVLIST_SOFTWARE | DRVLIST_HARDWARE))
	{
		FullscreenDriver = nullptr;
		FullscreenMode = nullptr;
		//grErrorLog_Add( GR_ERR_SUBSYSTEM_FAILURE, "CJetView::ChooseFullscreenVideoSettings -Failed to set fullscreen mode");
		//return GR_FALSE;
	}


	// all done
	return GR_TRUE;

} // CJetView::ChooseFullscreenVideoSettings()

////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::SetFullscreenModeByString()
//
//	Set Fullscreenmodus via Textstring
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::SetFullscreenModeByString(char* sDriverMode)
{
	grDriver* Driver{};
	grDriver_Mode* Mode{};

	grBoolean ret = DrvList_GetDriverByName(
		m_pEngine,
		sDriverMode,
		&Driver,
		&Mode);
	if (ret == GR_FALSE)
	{
		FullscreenDriver = nullptr;
		FullscreenMode = nullptr;
		return GR_FALSE;
	}
	else
	{
		FullscreenDriver = Driver;
		FullscreenMode = Mode;
	}

	return GR_TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::SetWindowModeByString()
//
//	Set Windowmodus via Textstring
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::SetWindowModeByString(char* sDriverMode)
{
	grDriver* Driver{};
	grDriver_Mode* Mode{};

	grBoolean ret = DrvList_GetDriverByName(
		m_pEngine,
		sDriverMode,
		&Driver,
		&Mode);
	if (ret == GR_FALSE)
	{
		WindowDriver = nullptr;
		WindowMode = nullptr;
		return GR_FALSE;
	}
	else
	{
		WindowDriver = Driver;
		WindowMode = Mode;
	}

	return GR_TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::FullscreenView()
//
//	Switch into full screen view.
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::FullscreenView()
{

	// locals
	HWND			hFullScreen{};
	WNDCLASS		wc{};
	grDriver* LastDriver{};
	grDriver_Mode* LastMode{};
	int32			Width{}, Height{};

	// get fullscreen settings if none have been picked
	if ((FullscreenDriver == nullptr) || (FullscreenMode == nullptr))  //Added JH
	{
		ChooseFullscreenVideoSettings();
		if ((FullscreenDriver == nullptr) || (FullscreenMode == nullptr))
		{
			return GR_TRUE;
		}
	}

	// save last driver and mode
	grEngine_GetDriverAndMode(m_pEngine, &LastDriver, &LastMode);

	// get selected mode width and height
	grDriver_ModeGetWidthHeight(FullscreenMode, &Width, &Height);
	ASSERT(Width > 0);
	ASSERT(Height > 0);
	if (FullscreenCreateWindow(&hFullScreen, &wc, Width, Height) == GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::FullscreenView", "Failed to create full screen window");
		return GR_FALSE;
	}

	// set new mode
	if (!grSound_SetHwnd(hFullScreen))
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::grSound_SetHwnd", "Failed to set new video mode");
		FullscreenDestroyWindow(&hFullScreen, &wc);
		return GR_FALSE;
	}

	if (grEngine_SetDriverAndMode(m_pEngine, hFullScreen, FullscreenDriver, FullscreenMode) == GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::FullscreenView", "Failed to set new video mode");
		FullscreenDestroyWindow(&hFullScreen, &wc);
		grEngine_SetDriverAndMode(m_pEngine, GetSafeHwnd(), LastDriver, LastMode);
		return GR_FALSE;
	}

	// process full screen mod e
	{
		CGweDoc* Doc{};
		float				XRot{};
		float				YRot{};
		Doc = GetDocument();

		// Disable face selection
		Doc->SetDrawFaceCB(m_pEngine, GR_FALSE);
		Doc->GetCurCamXYRot(&XRot, &YRot);
		FullscreenProcess(Doc, m_pEngine, Doc->GetWorld(), Doc->GetCurCamObject(), &XRot, &YRot, Width, Height);
		Doc->SetCurCamXYRot(XRot, YRot);
	}


	// deactivate full screen mode
	if (grEngine_SetDriverAndMode(m_pEngine, GetSafeHwnd(), LastDriver, LastMode) == GR_FALSE)
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::FullscreenView", "Failed to deactivate full screen mode");
		FullscreenDestroyWindow(&hFullScreen, &wc);
		return GR_FALSE;
	}

	if (!grSound_SetHwnd(AfxGetMainWnd()->GetSafeHwnd()))
	{
		grErrorLog_AddString(GR_ERR_SUBSYSTEM_FAILURE, "CJetView::grSound_SetHwnd", "Failed to set new video mode");
		FullscreenDestroyWindow(&hFullScreen, &wc);
		return GR_FALSE;
	}

	
	// destroy full screen window
	FullscreenDestroyWindow(&hFullScreen, &wc);


	m_RenderMode = GetDocument()->GetRenderMode();

	// all done
	return GR_TRUE;

} // CJetView::FullscreenView()



////////////////////////////////////////////////////////////////////////////////////////
//
//	CJetView::UpdateWindow()
//
//	Update the engine to accomodate a window change.
//
////////////////////////////////////////////////////////////////////////////////////////
grBoolean CG3DView::UpdateWindow()
{

	// if there is an active engine then update it
	if (m_pEngine != nullptr)
	{
		Invalidate(FALSE);
		return grEngine_UpdateWindow(m_pEngine);
	}

	// otherwise do nothing
	return GR_TRUE;

} // CJetView::UpdateWindow()

void CG3DView::OnBilinear()
{
	uint32		DefaultRenderFlags;

	if (grEngine_GetDefaultRenderFlags(this->m_pEngine, &DefaultRenderFlags))
	{
		if (DefaultRenderFlags & GR_RENDER_FLAG_BILINEAR_FILTER)
			DefaultRenderFlags &= ~GR_RENDER_FLAG_BILINEAR_FILTER;
		else
			DefaultRenderFlags |= GR_RENDER_FLAG_BILINEAR_FILTER;
		grEngine_SetDefaultRenderFlags(this->m_pEngine, DefaultRenderFlags);
	}
}
