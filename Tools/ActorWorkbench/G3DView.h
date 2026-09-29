#pragma once


// CJet3DView view
#include "Genesis3D.h"

class CG3DView : public CView
{
	DECLARE_DYNCREATE(CG3DView)

protected:
	CG3DView();           // protected constructor used by dynamic creation
	virtual ~CG3DView();

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	grBoolean							m_bInitialized;

	grEngine							*m_pEngine;
	grCamera							*m_pCamera;

	grObject							*m_pActorObject;
	grActor_Def							*m_pActorDef;
	grActor								*m_pActor;
	grXForm3d							m_ActorXForm;

	grRect								m_CameraRect;
	grFloat								m_FOV;
	grXForm3d							m_CameraXForm;

	grWorld								*m_pWorld;
	grResourceMgr						*m_pResMgr;

protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnTimer(UINT_PTR nIDEvent);
	virtual void OnInitialUpdate();
	grEngine * GetEngine(void);
	void SetActiveActor(grObject * Actor);
	grBoolean InitWorld(void);
};


