/****************************************************************************************/
/*  D3D12CONFIG.H                                                                       */
/*                                                                                      */
/*  Driver settings from Direct3D12Driver.ini, next to the driver DLL.                  */
/*                                                                                      */
/*  Every setting can be overridden with an environment variable named                  */
/*  G3D_D3D12_<KEY> (for example G3D_D3D12_WARP=1), so scripts and CI can change        */
/*  them without editing the file.                                                      */
/****************************************************************************************/
#ifndef D3D12CONFIG_H
#define D3D12CONFIG_H

#include <windows.h>
#include <cstdint>

int32_t	D3D12Config_GetInt(const char* Section, const char* Key, int32_t Default);
bool	D3D12Config_GetBool(const char* Section, const char* Key, bool Default);
// Returns false (and an empty string) when the setting is missing or blank.
bool	D3D12Config_GetString(const char* Section, const char* Key, char* Out, DWORD OutSize);
// The driver DLL itself (for its resources).
HMODULE	D3D12Config_Module();
// Resolves a path relative to the driver DLL's directory.
void	D3D12Config_ResolvePath(const char* Relative, char* Out, DWORD OutSize);

#endif // D3D12CONFIG_H
