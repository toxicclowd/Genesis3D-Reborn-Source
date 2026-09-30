/****************************************************************************************/
/*  D3D12CONFIG.CPP                                                                     */
/*                                                                                      */
/*  Driver settings from Direct3D12Driver.ini, next to the driver DLL.                  */
/****************************************************************************************/
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "D3D12Config.h"

namespace
{
	const char* IniName = "Direct3D12Driver.ini";

	void ModuleDirectory(char* Out, DWORD OutSize)
	{
		HMODULE Module = D3D12Config_Module();
		Out[0] = '\0';
		if (!Module)
			return;

		const DWORD Length = GetModuleFileNameA(Module, Out, OutSize);
		if (Length == 0 || Length >= OutSize)
		{
			Out[0] = '\0';
			return;
		}
		char* Slash = std::strrchr(Out, '\\');
		if (Slash)
			Slash[1] = '\0';
	}

	const char* IniPath()
	{
		static char Path[MAX_PATH] = "";
		if (!Path[0])
			D3D12Config_ResolvePath(IniName, Path, MAX_PATH);
		return Path;
	}

	// G3D_D3D12_<KEY>, upper-cased.
	bool EnvOverride(const char* Key, char* Out, DWORD OutSize)
	{
		char Name[128] = "G3D_D3D12_";
		size_t Length = std::strlen(Name);
		for (const char* c = Key; *c && Length < sizeof(Name) - 1; ++c)
			Name[Length++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
		Name[Length] = '\0';

		const DWORD Result = GetEnvironmentVariableA(Name, Out, OutSize);
		return Result > 0 && Result < OutSize;
	}
}

HMODULE D3D12Config_Module()
{
	HMODULE Module = nullptr;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(&D3D12Config_Module),
		&Module);
	return Module;
}

void D3D12Config_ResolvePath(const char* Relative, char* Out, DWORD OutSize)
{
	if (!Relative || !Out || OutSize == 0)
		return;

	// Absolute paths (drive or UNC) pass through unchanged.
	if ((Relative[0] && Relative[1] == ':') || (Relative[0] == '\\' && Relative[1] == '\\'))
	{
		strncpy_s(Out, OutSize, Relative, _TRUNCATE);
		return;
	}

	ModuleDirectory(Out, OutSize);
	strncat_s(Out, OutSize, Relative, _TRUNCATE);
}

bool D3D12Config_GetString(const char* Section, const char* Key, char* Out, DWORD OutSize)
{
	if (!Out || OutSize == 0)
		return false;
	Out[0] = '\0';

	if (!EnvOverride(Key, Out, OutSize))
		GetPrivateProfileStringA(Section, Key, "", Out, OutSize, IniPath());
	return Out[0] != '\0';
}

int32_t D3D12Config_GetInt(const char* Section, const char* Key, int32_t Default)
{
	char Value[64];
	if (!D3D12Config_GetString(Section, Key, Value, sizeof(Value)))
		return Default;
	return static_cast<int32_t>(std::strtol(Value, nullptr, 0));
}

bool D3D12Config_GetBool(const char* Section, const char* Key, bool Default)
{
	return D3D12Config_GetInt(Section, Key, Default ? 1 : 0) != 0;
}
