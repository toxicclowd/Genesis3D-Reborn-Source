/****************************************************************************************/
/*  JETVIEW.H                                                                           */
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
#if !defined(AFX_G3DVIEW_H__E0B7E841_C1C6_11D2_8B41_00104B70D76D__INCLUDED_)
#define AFX_G3DVIEW_H__E0B7E841_C1C6_11D2_8B41_00104B70D76D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "G3DMfcView.h"
#include "Doc.h"
/////////////////////////////////////////////////////////////////////////////
// CJetView view

class CG3DView : public CG3DMfcView
{
protected:
	CG3DView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CG3DView)

// Attributes
public:
	// 3D view navigation keymaps. Modern adds RMB fly (WASD/QE), Alt+LMB orbit, MMB pan,
	// wheel dolly and F framing; Classic Genesis keeps only the original mouse controls.
	enum NavKeymap
	{
		NavKeymap_Modern = 0,
		NavKeymap_Classic = 1
	};
	static NavKeymap GetNavKeymap(void);
	static void SetNavKeymap(NavKeymap Keymap);

// Operations
public:
	CGweDoc* GetDocument();
	grCamera* GetCamera(void);
	void OnViewType( UINT nID );
	void SetCameraPos( grVec3d * Pos );
	grBoolean  RegisterBitmap( grBitmap * pBitmap );
	grBoolean	FullscreenView( void );
	grBoolean	ChooseWindowVideoSettings( void );
	grBoolean	ChooseFullscreenVideoSettings( void );
	grBoolean	UpdateWindow( void );
	void		Animate( grBoolean bAnimate );

	grBoolean	SetFullscreenModeByString(char	*sDriverMode); // Added JH 7.3.2000
	grBoolean	SetWindowModeByString(char	*sDriverMode);	// Added JH 7.3.2000

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CJetView)
	public:
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CG3DView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CJetView)
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnViewCenterselction();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void On3dviewLines();
	afx_msg void On3dviewTextured();
	afx_msg void On3dviewTexturedwlights();
	afx_msg void On3dviewFlat();
	afx_msg void On3dviewBspsplits();
	afx_msg void OnBilinear();
	//}}AFX_MSG
	afx_msg void OnUpdateViewType( CCmdUI* pCmdUI ) ;
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void On3dviewClassicNav();
	afx_msg void On3dviewLook(UINT nID);
	DECLARE_MESSAGE_MAP()
private:
	void RotateCameraUpDown(long Delta);
	void RotateCameraLeftRight(long Delta);
	void MoveCameraInOut(long Delta);
	void MoveCameraLeftRight(long Delta);
	void MoveCameraUpDown(long Delta);
	char * GetModeName( );
	void ShowMenu( CPoint point);

	// Modern navigation
	void BeginFly(CPoint point);
	void EndFly(void);
	void UpdateFly(void);
	void FlyLook(long DeltaX, long DeltaY);
	void BeginOrbit(CPoint point);
	void OrbitCamera(long DeltaX, long DeltaY);
	void EndNavDrag(void);
	void FrameBounds(const grExtBox* pBounds);

	CPoint m_Anchor;
	UINT m_nViewType;

	// Camera stuff
	grVec3d m_CameraPos;
	grVec3d m_CameraLeft;
	grVec3d m_CameraUp;
	grVec3d m_CameraIn;
	float	m_CameraRotX;
	float	m_CameraRotY;
	grXForm3d m_CameraXForm;
	BOOL m_bRecalcCamera;
	grCamera* m_pCamera;
	bool m_bDragging;
	bool m_bAnimate;
	static int m_CXDRAG ;
	static int m_CYDRAG ;
	int		m_RenderMode;
	float	m_LastTime;

	bool	m_bFlying;			// RMB held in the Modern keymap
	bool	m_bFlyMoved;		// a movement key was used, so RMB-up is not a context-menu click
	bool	m_bOrbiting;
	bool	m_bPanning;
	bool	m_bCursorHidden;
	bool	m_bEatAltUp;		// Alt+LMB orbit must not open the menu bar on Alt release
	CPoint	m_FlyCursorHome;
	grVec3d	m_FlyVelocity;
	float	m_FlySpeedScale;
	LARGE_INTEGER m_FlyLastTick;
	grVec3d	m_OrbitPivot;

};

#ifndef _DEBUG  // debug version in J3DView.cpp
inline CGweDoc* CG3DView::GetDocument()
   { return (CGweDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_JETVIEW_H__E0B7E841_C1C6_11D2_8B41_00104B70D76D__INCLUDED_)

