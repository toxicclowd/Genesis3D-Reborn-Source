#include "sizecbar.h"

class CG3DBar : public CSizingControlBar
{
public:
                                    CG3DBar() 
                                    { m_wndChild = NULL; };
                                   ~CG3DBar() 
                                    {};

protected:
    //{{AFX_MSG(CMyBar)
        afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
public:
        CDialog *                   m_wndChild;
};

