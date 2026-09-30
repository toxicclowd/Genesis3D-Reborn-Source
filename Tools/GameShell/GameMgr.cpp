#include <assert.h>
#include "GameMgr.h"
#include "GameLog.h"
#include "ScriptMgr.h"

CGameMgr									*CGameMgr::Singleton = NULL;

CGameMgr * CGameMgr::GetPtr()
{
	if (CGameMgr::Singleton == NULL)
		CGameMgr::Singleton = new CGameMgr();

	return CGameMgr::Singleton;
}

CGameMgr::CGameMgr()
{
	m_pEngine = NULL;
	m_pCamera = NULL;
	m_pWorld = NULL;

	m_pPtrMgr = NULL;
	m_pResMgr = NULL;

	m_hWnd = NULL;
	m_Width = m_Height = m_BPP = -1;
	strcpy(m_DriverName, "(D3D) DirectX 12");

	this->register_func("EnableFrameRateCounter", cpp_method(this, CGameMgr, EOSEnableFrameRateCounter));
	this->register_func("SetGamma", cpp_method(this, CGameMgr, EOSSetGamma));
	this->register_func("SetDriver", cpp_method(this, CGameMgr, EOSSetDriver));
	this->register_func("SetDriverMode", cpp_method(this, CGameMgr, EOSSetDriverMode));
	this->register_func("LoadWorld", cpp_method(this, CGameMgr, EOSLoadWorld));
}

CGameMgr::~CGameMgr()
{
	Shutdown();
}

bool CGameMgr::Release()
{
	GLOG("CGameMgr - Releasing memory...");

	delete this;
	return true;
}

grBoolean CGameMgr::Initialize(HWND hWnd)
{
	GLOG("CGameMgr - Initializing game manager...");

	m_pEngine = grEngine_Create(hWnd, "G3DGameShell", ".");
	if (!m_pEngine)
	{
		GLOG("CGameMgr - Could not create engine!!");
		return GR_FALSE;
	}

	m_hWnd = hWnd;

	//CScriptMgr::GetPtr()->SetGlobal("Game", (eosobject*)this);

	GLOG("CGameMgr - Loading main script...");
	CScriptMgr::GetPtr()->LoadScript(".\\Scripts\\G3DMain.eos");
	CScriptMgr::GetPtr()->Call("G3DMain");

	return GR_TRUE;
}

void CGameMgr::Shutdown()
{
	GLOG("CGameMgr - Shutting down game manager...");

	if (m_pWorld)
		grWorld_Destroy(&m_pWorld);

	if (m_pCamera)
		grCamera_Destroy(&m_pCamera);

	if (m_pResMgr)
		grResource_MgrDestroy(&m_pResMgr);

	if (m_pPtrMgr)
		grPtrMgr_Destroy(&m_pPtrMgr);

	if (m_pEngine)
		grEngine_Destroy(&m_pEngine);

	m_pEngine = NULL;
}

void CGameMgr::SetDriver(const char *drivername)
{
	CGameLog::GetPtr()->Printf("CGameMgr - Setting driver to %s...", drivername);
	strcpy(m_DriverName, drivername);
}

grBoolean CGameMgr::SetDriverMode(int32 w, int32 h, int32 b)
{
	grDriver_System						*DrvSys = NULL;
	grDriver							*Driver = NULL;
	grDriver_Mode						*Mode = NULL;
	int32								width, height, bpp;

	GLOG("CGameMgr - Preparing video mode...");
	assert(m_pEngine != NULL);

	if (w == 0)
		w = -1;

	if (h == 0)
		h = -1;

	if (b == 0)
		b = -1;

	DrvSys = grEngine_GetDriverSystem(m_pEngine);
	if (!DrvSys)
	{
		GLOG("CGameMgr - Could not get driver system!!");
		return GR_FALSE;
	}

	for (Driver = grDriver_SystemGetNextDriver(DrvSys, NULL); Driver != NULL; Driver = grDriver_SystemGetNextDriver(DrvSys, Driver))
	{
		const char						*drvname = NULL;

		grDriver_GetName(Driver, &drvname);
		if (drvname && !strcmp(drvname, m_DriverName))
			break;
	}

	if (Driver == NULL)
	{
		GLOG("Could not find requested driver!!");
		DrvSys = NULL;
		return GR_FALSE;
	}

	for (Mode = grDriver_GetNextMode(Driver, NULL); Mode != NULL; Mode = grDriver_GetNextMode(Driver, Mode))
	{
		grDriver_ModeGetAttributes(Mode, &width, &height, &bpp);
		if (width == w && height == h && bpp == b)
			break;
	}

	if (Mode == NULL)
	{
		GLOG("Could not find a valid video mode!!");
		Driver = NULL;
		DrvSys = NULL;
		return GR_FALSE;
	}

	if (!grEngine_SetDriverAndMode(m_pEngine, m_hWnd, Driver, Mode))
	{
		GLOG("Could not activate engine!!");
		Mode = NULL;
		Driver = NULL;
		DrvSys = NULL;
		return GR_FALSE;
	}

	grEngine_RegisterObjects("Objects");

	grRect						r;
	RECT						Client;

	GetClientRect(m_hWnd, &Client);
	r.Left = 0;
	r.Top = 0;
	r.Right = Client.right - 1;
	r.Bottom = Client.bottom - 1;

	m_pCamera = grCamera_Create(2.0f, &r);

	grXForm3d_SetIdentity(&m_CameraXForm);
	grCamera_SetXForm(m_pCamera, &m_CameraXForm);

	m_pPtrMgr = grPtrMgr_Create();
	if (!m_pPtrMgr)
		return GR_FALSE;

	m_pResMgr = grResource_MgrCreateDefault(m_pEngine);
	if (!m_pResMgr)
		return GR_FALSE;

	m_LastTime = timeGetTime();

	return GR_TRUE;
}

grBoolean CGameMgr::LoadWorld(const char *filename)
{
	grVFile						*File = NULL;

	if (!m_pPtrMgr || !m_pResMgr)
		return GR_FALSE;

	m_pWorld = grWorld_CreateFromEditorFile(filename, m_pPtrMgr, m_pResMgr);
	if (!m_pWorld)
		return GR_FALSE;

	grWorld_SetEngine(m_pWorld, m_pEngine);

	grWorld_RebuildBSP(m_pWorld, BSP_OPTIONS_CSG_BRUSHES, Logic_Smart, 5);
	grWorld_RebuildLights(m_pWorld);

	return GR_TRUE;
}

grBoolean CGameMgr::Frame()
{
	DWORD						currTime;
	float						deltaTime;

	assert(m_pEngine != NULL);

	currTime = timeGetTime();
	deltaTime = ((float)(currTime - m_LastTime)) * 0.001f;

	if (!grEngine_BeginFrame(m_pEngine, m_pCamera, GR_TRUE))
		return GR_FALSE;

	if (m_pWorld)
	{
		grWorld_Frame(m_pWorld, deltaTime);
		grWorld_Render(m_pWorld, m_pCamera, NULL);
	}

	if (!grEngine_EndFrame(m_pEngine))
		return GR_FALSE;

	m_LastTime = currTime;

	return GR_TRUE;
}

void CGameMgr::EOSEnableFrameRateCounter()
{
	grBoolean					enable;

	enable = (grBoolean)exe->pop()->geti(0);
	grEngine_EnableFrameRateCounter(m_pEngine, enable);
}

void CGameMgr::EOSSetGamma()
{
	float						gamma;

	gamma = exe->pop()->getd(0);
	grEngine_SetGamma(m_pEngine, gamma);
}

void CGameMgr::EOSSetDriver()
{
	std::string					name;

	name = exe->pop()->gets(0);
	SetDriver(name.c_str());
}

void CGameMgr::EOSSetDriverMode()
{
	int32						w, h, b;

	w = exe->pop()->geti(0);
	h = exe->pop()->geti(0);
	b = exe->pop()->geti(0);

	SetDriverMode(w, h, b);
}

void CGameMgr::EOSLoadWorld()
{
	std::string					filename;

	filename = exe->pop()->gets(0);
	LoadWorld(filename.c_str());
}
