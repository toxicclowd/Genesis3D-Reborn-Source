/****************************************************************************************/
/*  J3DMAINFRM.CPP                                                                      */
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

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#include "G3DMfcApp.h"
#include "G3DMfcDoc.h"

#include "G3DMfcMainFrm.h"

/////////////////////////////////////////////////////////////////////////////
// CJ3DMainFrame

IMPLEMENT_DYNAMIC(CG3DMfcMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CG3DMfcMainFrame, CMDIFrameWnd)

	//{{AFX_MSG_MAP(CJ3DMainFrame)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CG3DMfcMainFrame::CG3DMfcMainFrame()
{
}

CG3DMfcMainFrame::~CG3DMfcMainFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// CJ3DMainFrame Implementation

/////////////////////////////////////////////////////////////////////////////
// CJ3DMainFrame message handlers

void CG3DMfcMainFrame::OpenG3DWindow() 
{
	CG3DMfcDoc* pDocument;
	CMultiDocTemplate* pTemplate{};
	
	CFrameWnd* pFrame{};
	CMDIChildWnd* pActiveChild{};

	// Get the active document
	
	pDocument = nullptr;
	
	pActiveChild = MDIGetActive();
	
	if( pActiveChild != nullptr )
	{
		pDocument = (CG3DMfcDoc*)pActiveChild->GetActiveDocument();
		ASSERT(pDocument->IsKindOf(RUNTIME_CLASS(CG3DMfcDoc)));
	}

	if( pDocument == nullptr )
	{
		TRACE0("Warning: No active document for OpenJ3DWindow command.\n");
		AfxMessageBox(AFX_IDP_COMMAND_FAILURE);
		return;
	}

	// Retrieve the CJ3DView document template.

	pTemplate = ((CG3DMfcApp*)AfxGetApp())->GetG3DMfcViewDocTemplate();
	ASSERT(pTemplate != nullptr);

	// Create a new document frame containing a CJ3DView.

	pFrame = pTemplate->CreateNewFrame(pDocument, pActiveChild);
	if(pFrame == nullptr)
	{
		TRACE0("Warning: failed to create new frame.\n");
		return;
	}

	pTemplate->InitialUpdateFrame(pFrame, pDocument);
}


