#include "stdafx.h"
#include "dockbars.h"

BEGIN_MESSAGE_MAP(CG3DBar, CSizingControlBar)
    //{{AFX_MSG_MAP(CJetBar)
    ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CG3DBar::OnSize(UINT nType, int cx, int cy) 
{
    CSizingControlBar::OnSize(nType, cx, cy);

    if(m_wndChild != NULL) {
        CRect rc;
        GetClientRect(rc);

        m_wndChild->MoveWindow(rc);
    }
}
