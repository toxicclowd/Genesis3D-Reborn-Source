/*!
	@file main.cpp
	@author Anthony Rufrano (paradoxnj)
	@brief The program's entry point
*/
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "GameMgr.h"
#include "ScriptMgr.h"
#include "GameLog.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

/*!
	Render-regression mode (roadmap Phase 0):

	G3DGameShell -screenshot out.bmp [-level Levels\x.j3d] [-camera x y z yaw pitch]
	             [-frames n] [-size w h] [-overlay] [-dlight x y z radius r g b]...
	             [-shadowlight x y z radius r g b]... [-spotlight x y z radius r g b dx dy dz angle]...
	             [-look classic|enhanced|stylized] [-restartdriver]

	Runs the startup script, optionally swaps in another level, places the camera,
	renders a fixed number of frames with a fixed timestep (so animated lights and
	textures land in the same state every run), saves the last frame and exits.
	The exit code is 0 on success and 1 on failure. -overlay keeps the engine's debug
	text (FPS, GPU times), which is left out of reference images by default. -dlight
	adds a dynamic light (color 0..255), up to four, e.g. to show PBR specular.
	-restartdriver shuts the driver down and starts it again halfway through (as a
	display mode change does), so textures must survive it. -shadowlight is a -dlight
	that casts shadows, -spotlight one shaped by a cone (direction and half angle in
	degrees) that casts shadows too; both need the Enhanced or Stylized look (-look,
	or [Look] Profile in Direct3D12Driver.ini) to cast them.
*/
struct ShellOptions
{
	const char						*Screenshot;
	const char						*Level;
	bool							HasCamera;
	bool							Overlay;
	bool							RestartDriver;
	float							Camera[5];			// x y z yaw pitch (degrees)
	int								NumDLights;
	float							DLights[4][11];		// x y z radius r g b, spot: dx dy dz angle
	uint32							DLightFlags[4];		// GR_LIGHT_FLAG_*
	int								Look;				// GR_LOOK_*, -1 = the driver's
	int								Frames;
	int								Width, Height;
};

static void ParseOptions(ShellOptions *Opts)
{
	memset(Opts, 0, sizeof(*Opts));
	Opts->Look = -1;
	Opts->Frames = 30;
	Opts->Width = 800;
	Opts->Height = 600;

	for (int i = 1; i < __argc; i++)
	{
		const char					*Arg = __argv[i];
		const int					Left = __argc - i - 1;

		if (!_stricmp(Arg, "-screenshot") && Left >= 1)
			Opts->Screenshot = __argv[++i];
		else if (!_stricmp(Arg, "-level") && Left >= 1)
			Opts->Level = __argv[++i];
		else if (!_stricmp(Arg, "-overlay"))
			Opts->Overlay = true;
		else if (!_stricmp(Arg, "-restartdriver"))
			Opts->RestartDriver = true;
		else if (!_stricmp(Arg, "-frames") && Left >= 1)
			Opts->Frames = atoi(__argv[++i]);
		else if (!_stricmp(Arg, "-size") && Left >= 2)
		{
			Opts->Width = atoi(__argv[++i]);
			Opts->Height = atoi(__argv[++i]);
		}
		else if (!_stricmp(Arg, "-camera") && Left >= 5)
		{
			for (int c = 0; c < 5; c++)
				Opts->Camera[c] = (float)atof(__argv[++i]);
			Opts->HasCamera = true;
		}
		else if ((!_stricmp(Arg, "-dlight") || !_stricmp(Arg, "-shadowlight")) && Left >= 7)
		{
			const int				Index = (Opts->NumDLights < 4) ? Opts->NumDLights : 3;
			float					*Light = Opts->DLights[Index];

			for (int c = 0; c < 7; c++)
				Light[c] = (float)atof(__argv[++i]);
			Opts->DLightFlags[Index] = !_stricmp(Arg, "-shadowlight") ? GR_LIGHT_FLAG_CAST_SHADOWS : 0;
			if (Opts->NumDLights < 4)
				Opts->NumDLights++;
		}
		else if (!_stricmp(Arg, "-spotlight") && Left >= 11)
		{
			const int				Index = (Opts->NumDLights < 4) ? Opts->NumDLights : 3;
			float					*Light = Opts->DLights[Index];

			for (int c = 0; c < 11; c++)
				Light[c] = (float)atof(__argv[++i]);
			Opts->DLightFlags[Index] = GR_LIGHT_FLAG_SPOT | GR_LIGHT_FLAG_CAST_SHADOWS;
			if (Opts->NumDLights < 4)
				Opts->NumDLights++;
		}
		else if (!_stricmp(Arg, "-look") && Left >= 1)
		{
			const char				*Name = __argv[++i];

			Opts->Look = !_stricmp(Name, "enhanced") ? GR_LOOK_ENHANCED : !_stricmp(Name, "stylized") ? GR_LOOK_STYLIZED : GR_LOOK_CLASSIC;
		}
	}

	if (Opts->Frames < 1)
		Opts->Frames = 1;
	if (Opts->Width < 64)
		Opts->Width = 64;
	if (Opts->Height < 64)
		Opts->Height = 64;
}

static bool PumpMessages()
{
	MSG								msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return false;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

static int RunScreenshot(const ShellOptions *Opts, HWND hWnd)
{
	CGameMgr						*Game = CGameMgr::GetPtr();

	if (!Game->m_pEngine || !Game->m_pCamera)
	{
		GLOG("Screenshot - the startup script did not set a driver mode");
		return 1;
	}

	if (Opts->Level && !Game->LoadWorld(Opts->Level))
	{
		CGameLog::GetPtr()->Printf("Screenshot - could not load %s", Opts->Level);
		return 1;
	}

	if (Opts->HasCamera)
	{
		grVec3d						Pos;

		grVec3d_Set(&Pos, Opts->Camera[0], Opts->Camera[1], Opts->Camera[2]);
		Game->SetCamera(&Pos, Opts->Camera[3], Opts->Camera[4]);
	}

	for (int i = 0; i < Opts->NumDLights; i++)
	{
		const float					*L = Opts->DLights[i];
		grLight						*Light = grLight_Create();
		grVec3d						Pos, Color;

		grVec3d_Set(&Pos, L[0], L[1], L[2]);
		grVec3d_Set(&Color, L[4], L[5], L[6]);
		if (!Light || !grLight_SetAttributes(Light, &Pos, &Color, L[3], 1.0f, Opts->DLightFlags[i]))
		{
			GLOG("Screenshot - could not add a dynamic light");
			return 1;
		}
		if (Opts->DLightFlags[i] & GR_LIGHT_FLAG_SPOT)
		{
			grVec3d					Dir;

			grVec3d_Set(&Dir, L[7], L[8], L[9]);
			grLight_SetSpot(Light, &Dir, L[10] * 0.75f, L[10]);
		}
		if (!grWorld_AddDLight(Game->m_pWorld, Light))
		{
			GLOG("Screenshot - could not add a dynamic light");
			return 1;
		}
	}

	if (Opts->Look >= 0)
		grEngine_SetLookProfile(Game->m_pEngine, Opts->Look);

	// The frame-rate text changes every run, so keep it out of reference images.
	grEngine_EnableFrameRateCounter(Game->m_pEngine, Opts->Overlay ? GR_TRUE : GR_FALSE);
	Game->SetFixedTimeStep(1.0f / 60.0f);

	for (int i = 0; i < Opts->Frames; i++)
	{
		if (!PumpMessages())
			return 1;
		if (Opts->RestartDriver && i == Opts->Frames / 2)
		{
			grDriver				*Driver;
			grDriver_Mode			*Mode;

			if (!grEngine_GetDriverAndMode(Game->m_pEngine, &Driver, &Mode) ||
				!grEngine_SetDriverAndMode(Game->m_pEngine, hWnd, Driver, Mode))
			{
				GLOG("Screenshot - driver restart failed");
				return 1;
			}
		}
		if (!Game->Frame())
		{
			GLOG("Screenshot - frame failed");
			return 1;
		}
	}

	if (!grEngine_ScreenShot(Game->m_pEngine, Opts->Screenshot))
	{
		CGameLog::GetPtr()->Printf("Screenshot - could not write %s", Opts->Screenshot);
		return 1;
	}

	CGameLog::GetPtr()->Printf("Screenshot - wrote %s (camera at %.1f %.1f %.1f)", Opts->Screenshot,
		Game->m_CameraXForm.Translation.X, Game->m_CameraXForm.Translation.Y, Game->m_CameraXForm.Translation.Z);
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASS						wc;
	HWND							hWnd;
	bool							running = true;
	int								ExitCode = 0;
	ShellOptions					Opts;
	int								WindowWidth = 800, WindowHeight = 600;

	ParseOptions(&Opts);

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "Genesis3D: Reborn Game Shell";
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wc);

	// Reference images need an exact client size, not an exact window size.
	if (Opts.Screenshot)
	{
		RECT						Rect = { 0, 0, Opts.Width, Opts.Height };

		AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);
		WindowWidth = Rect.right - Rect.left;
		WindowHeight = Rect.bottom - Rect.top;
	}

	hWnd = CreateWindow(wc.lpszClassName, wc.lpszClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, NULL, NULL, hInstance, NULL);
	if (!hWnd)
	{
		MessageBox(NULL, "Could not create main window!!", "Genesis3D: Reborn Game Shell Error...", 48);
		return 0;
	}

	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);
	SetFocus(hWnd);

	InitializeGameLog("Genesis3D_Reborn_GameShell.log");
	CScriptMgr::GetPtr()->Initialize();

	if (!CGameMgr::GetPtr()->Initialize(hWnd))
	{
		if (!Opts.Screenshot)
			MessageBox(hWnd, "Could not start game manager!!", "Genesis3D: Reborn Game Shell Error...", 48);
		CGameMgr::GetPtr()->Release();
		DestroyWindow(hWnd);
		return 1;
	}

	if (Opts.Screenshot)
	{
		ExitCode = RunScreenshot(&Opts, hWnd);
		running = false;
	}

	while (running)
	{
		if (!PumpMessages())
			running = false;
		else if (!CGameMgr::GetPtr()->Frame())
			running = false;
	}

	CGameMgr::GetPtr()->Release();
	CScriptMgr::GetPtr()->Release();

	DestroyWindow(hWnd);
	UnregisterClass(wc.lpszClassName, hInstance);
	return ExitCode;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
	case WM_CLOSE:
		{
			PostQuitMessage(0);
			return FALSE;
		}
	case WM_KEYDOWN:
		{
			switch (wParam)
			{
			case VK_ESCAPE:
				{
					PostQuitMessage(0);
					break;
				}
			}

			return FALSE;
		}
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}
