/*!
	@file GameMgr.h
	@author Anthony Rufrano (paradoxnj)
	@brief The game manager
*/
#ifndef GAMEMGR_H
#define GAMEMGR_H

#include <windows.h>
#include "Genesis3D.h"
#include "eosscript.h"

#define MAX_DRIVER_NAME				64

class CGameMgr;

class CGameMgr : public eosobject
{
public:
	CGameMgr();
	virtual ~CGameMgr();

public:
	grEngine						*m_pEngine;
	HWND							m_hWnd;

	grPtrMgr						*m_pPtrMgr;
	grResourceMgr					*m_pResMgr;

	char							m_DriverName[MAX_DRIVER_NAME];
	int32							m_Width, m_Height, m_BPP;

	grWorld							*m_pWorld;

	grCamera						*m_pCamera;
	grXForm3d						m_CameraXForm;

	DWORD							m_LastTime;

public:
	bool							Release();

	grBoolean						Initialize(HWND hWnd);
	void							Shutdown();

	void							SetDriver(const char *drivername);
	grBoolean						SetDriverMode(int32 w, int32 h, int32 b);

	grBoolean						LoadWorld(const char *filename);

	grBoolean						Frame();

public:
	static CGameMgr					*Singleton;
	static CGameMgr					*GetPtr();

public:
	// Script exports
	void							EOSSetDriver();
	void							EOSSetDriverMode();

	void							EOSEnableFrameRateCounter();
	void							EOSSetGamma();

	void							EOSLoadWorld();
};

#endif