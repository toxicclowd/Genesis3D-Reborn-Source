/****************************************************************************************/
/*  D3D12LOOK.H                                                                         */
/*                                                                                      */
/*  Look Profiles (roadmap Phase 3): Classic, Enhanced and Stylized, with their         */
/*  settings (DRV_LookSettings). Defaults come from [Look] in Direct3D12Driver.ini;     */
/*  the engine can change them with DRV_Driver::SetLook. Also the legacy fog            */
/*  (DRV_Driver::SetFog), which the post-processing pass applies.                       */
/****************************************************************************************/
#ifndef D3D12LOOK_H
#define D3D12LOOK_H

#include <windows.h>
#include "DCommon.h"

// The settings as given, clamped to what the driver can do.
const DRV_LookSettings&	D3D12Look_Get();

// True for Enhanced and Stylized: HDR (linear) scene, per-pixel dynamic lights, post-processing.
bool	D3D12Look_IsHDR();
bool	D3D12Look_IsStylized();
// The scene's resolution relative to the back buffer (below 1 only in Stylized).
float	D3D12Look_RenderScale();

// Legacy distance fog (DRV_Driver::SetFog); Enabled is false until the engine turns it on.
struct D3D12Fog
{
	bool	Enabled;
	float	Color[3];		// 0..1
	float	Start;
	float	End;
};
const D3D12Fog&	D3D12Look_GetFog();

grBoolean DRIVERCC D3D12Look_SetLook(const DRV_LookSettings* Look);
grBoolean DRIVERCC D3D12Look_GetLook(DRV_LookSettings* Look);
grBoolean DRIVERCC D3D12Look_SetFog(float r, float g, float b, float Start, float End, grBoolean Enable);

// Reads the ini defaults again (driver start-up).
void	D3D12Look_Startup();

#endif // D3D12LOOK_H
