// Jet3DView.cpp : implementation file
//

#include "stdafx.h"
#include "ActorWorkbench.h"
#include "Jet3DView.h"
#include "ActorWorkbenchDoc.h"
#include ".\jet3dview.h"

#define TIMER_ID							1
#define TIMER_INTERVAL						40

// CJet3DView

IMPLEMENT_DYNCREATE(CJet3DView, CView)

CJet3DView::CJet3DView()
{
	m_pEngine = NULL;

	m_pCamera = NULL;
	m_pResMgr = NULL;

	m_pActorObject = NULL;
	m_pActor = NULL;
	m_pActorDef = NULL;

	m_pWorld = NULL;
	m_bInitialized = GR_FALSE;
}

CJet3DView::~CJet3DView()
{
	if (m_pActorObject)
	{
		if (m_pActorDef)
			grActor_DefDestroy(&m_pActorDef);

		if (m_pActor)
			m_pActor = NULL;

		grObject_DettachEngine(m_pActorObject, m_pEngine);
		grObject_Destroy(&m_pActorObject);
	}

	if (m_pWorld)
		grWorld_Destroy(&m_pWorld);

	if (m_pResMgr)
		grResource_MgrDestroy(&m_pResMgr);

	if (m_pCamera)
		grCamera_Destroy(&m_pCamera);

	if (m_pEngine)
		grEngine_Destroy(&m_pEngine);
}

BEGIN_MESSAGE_MAP(CJet3DView, CView)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CJet3DView drawing

void CJet3DView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	if (m_bInitialized)
	{
		// TODO: add draw code here
		if (!grEngine_BeginFrame(m_pEngine, m_pCamera, GR_TRUE))
			return;

		if (m_pActor)
		{
			grXForm3d				temp;

			temp = m_ActorXForm;
			grXForm3d_PostRotateX(&temp, GR_HALFPI);
			grXForm3d_PostRotateY(&temp, -GR_PI);

			grActor_ClearPose(m_pActor, &temp);
			grActor_Render(m_pActor, m_pEngine, m_pWorld, m_pCamera);
		}

		if (!grEngine_EndFrame(m_pEngine))
			return;
	}
}


// CJet3DView diagnostics

#ifdef _DEBUG
void CJet3DView::AssertValid() const
{
	CView::AssertValid();
}

void CJet3DView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG


// CJet3DView message handlers

void CJet3DView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	// TODO: Add your specialized code here and/or call the base class
	m_pEngine = grEngine_Create(this->GetSafeHwnd(), "ActorWorkbench", ".");
	if (!m_pEngine)
	{
		TRACE0("Could not create engine object!!");
		return;
	}

	grEngine_EnableFrameRateCounter(m_pEngine, GR_FALSE);
	grEngine_RegisterObjects("Objects");
	//grEngine_SetGamma(m_pEngine, 1.0f);	//trilobite orig
	grEngine_SetGamma(m_pEngine, 1.5f);	//trilobite revise


	grDriver_System *DrvSys = grEngine_GetDriverSystem(m_pEngine);
	if (!DrvSys)
	{
		TRACE0("Could not get driver system!!");
		return;
	}

	grDriver *Driver = NULL;
	for (Driver = grDriver_SystemGetNextDriver(DrvSys, NULL); Driver != NULL; Driver = grDriver_SystemGetNextDriver(DrvSys, Driver))
	{
		const char					*drvname = NULL;

		grDriver_GetName(Driver, &drvname);
		if (drvname && !strcmp(drvname, "(D3D) DirectX 12"))
			break;
	}

	if (Driver == NULL)
	{
		TRACE0("Could not find a valid driver!!");
		return;
	}

	//	by trilobite	Jan. 2011
	grDriver_Mode *Mode = NULL;
	//for (grDriver_Mode *Mode = grDriver_GetNextMode(Driver, NULL); Mode != NULL; Mode = grDriver_GetNextMode(Driver, Mode))
	for (Mode = grDriver_GetNextMode(Driver, NULL); Mode != NULL; Mode = grDriver_GetNextMode(Driver, Mode))
	//	
	{
		int32					w, h, b;

		grDriver_ModeGetAttributes(Mode, &w, &h, &b);
		if (w == -1 && h == -1 && b == -1)
			break;
	}

	if (Mode == NULL)
	{
		TRACE0("Driver does not support windowed mode!!");
		return;
	}

	if (!grEngine_SetDriverAndMode(m_pEngine, this->GetSafeHwnd(), Driver, Mode))
	{
		TRACE0("Could not start engine!!");
		return;
	}

	grEngine_SetRenderMode(m_pEngine, RenderMode_TexturedAndLit);

	m_pResMgr = grResource_MgrCreateDefault(m_pEngine);
	if (!m_pResMgr)
	{
		TRACE0("Could not create resource manager!!");
		return;
	}

	if (!InitWorld())
		return;
    
	this->SetTimer(TIMER_ID, TIMER_INTERVAL, NULL);
	m_bInitialized = GR_TRUE;
}

void CJet3DView::OnTimer(UINT_PTR nIDEvent)
{
	CView::OnTimer(nIDEvent);
	if (nIDEvent == TIMER_ID)
	{
		if (m_bInitialized)
			Invalidate(FALSE);
		//this->SetTimer(TIMER_ID, TIMER_INTERVAL, NULL);
	}
}

grEngine * CJet3DView::GetEngine(void)
{
	return m_pEngine;
}

void CJet3DView::SetActiveActor(grObject * Object)
{
	if (m_pActorObject)
	{
		if (m_pActorDef)
			grActor_DefDestroy(&m_pActorDef);

		if (m_pActor)
			m_pActor = NULL;

		grObject_DettachEngine(m_pActorObject, m_pEngine);
		grObject_Destroy(&m_pActorObject);

		m_pActorDef = NULL;
		m_pActorObject = NULL;
	}

	m_pActorObject = Object;
	m_pActor = (grActor*)grObject_GetInstance(m_pActorObject);
	m_pActorDef = grActor_GetActorDef(m_pActor);

	//grActor_SetScale(m_pActor, 0.5f, 0.5f, 0.5f);

	//grXForm3d_SetIdentity(&m_ActorXForm);
	grVec3d					Pos;

	grActor_GetXForm(m_pActor, &m_ActorXForm);
	grVec3d_Copy(&m_ActorXForm.Translation, &Pos);
	grVec3d_Set(&m_ActorXForm.Translation, 0.0f, 0.0f, 0.0f);
	grXForm3d_PostRotateX(&m_ActorXForm, GR_HALFPI);
	grXForm3d_PostRotateY(&m_ActorXForm, -GR_PI);
	grVec3d_Copy(&Pos, &m_ActorXForm.Translation);
	grActor_SetXForm(m_pActor, &m_ActorXForm);

	grVec3d					in;

	m_CameraXForm = m_ActorXForm;
	grXForm3d_GetIn(&m_CameraXForm, &in);
	grVec3d_MA(&m_CameraXForm.Translation, -50.0f, &in, &m_CameraXForm.Translation);
	
	if (grActor_GetMotionCount(m_pActorDef) > 0)
		m_CameraXForm.Translation.Y += 50.0f;

	grCamera_SetXForm(m_pCamera, &m_CameraXForm);

	grActor_AttachEngine(m_pActor, m_pEngine);
	//grActor_ClearPose(m_pActor, &m_ActorXForm);

	grVec3d FillLightNormal;

	grVec3d_Set( &FillLightNormal, -0.3f, 1.0f, 0.4f );
	grVec3d_Normalize( &FillLightNormal );

	grActor_SetLightingOptions( m_pActor, GR_TRUE, &FillLightNormal,
		512.0f, 512.0f, 512.0f,		// Fill light
		512.0f, 512.0f, 512.0f,		// Ambient light
		GR_TRUE,					// Ambient light from floor
		0,		// no dynamic lights,
		0,
		NULL, FALSE );

	//grWorld_AddObject(m_pWorld, m_pActorObject);
}

grBoolean CJet3DView::InitWorld(void)
{
	RECT							r;

	if (!m_pResMgr || !m_pEngine)
		return GR_FALSE;

	m_pWorld = grWorld_Create(m_pResMgr);
	if (!m_pWorld)
	{
		AfxMessageBox("Could not create world!!", 48, 0);
		return GR_FALSE;
	}

	grWorld_SetEngine(m_pWorld, m_pEngine);

	m_FOV = grFloat_DegToRad(90.0f);

	this->GetClientRect(&r);
	m_CameraRect.Top = r.top;
	m_CameraRect.Bottom = r.bottom - 1;
	m_CameraRect.Left = r.left;
	m_CameraRect.Right = r.right - 1;

	m_pCamera = grCamera_Create(m_FOV, &m_CameraRect);
	if (!m_pCamera)
	{
		AfxMessageBox("Could not create camera!!");
		return GR_FALSE;
	}

	grXForm3d_SetIdentity(&m_CameraXForm);
	grXForm3d_SetTranslation(&m_CameraXForm, 0.0f, 25.0f, 50.0f);

	grCamera_SetXForm(m_pCamera, &m_CameraXForm);

	return GR_TRUE;
}
