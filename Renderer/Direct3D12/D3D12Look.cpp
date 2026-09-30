/****************************************************************************************/
/*  D3D12LOOK.CPP                                                                       */
/*                                                                                      */
/*  Look Profiles and fog settings (see D3D12Look.h).                                  */
/****************************************************************************************/
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "D3D12Look.h"
#include "D3D12Config.h"
#include "D3D12Log.h"

namespace
{
	DRV_LookSettings	Look = {};
	D3D12Fog			Fog = {};
	bool				Loaded = false;

	float GetFloat(const char* Key, float Default)
	{
		char Value[64];
		if (!D3D12Config_GetString("Look", Key, Value, sizeof(Value)))
			return Default;
		return static_cast<float>(std::atof(Value));
	}

	int32 GetProfile()
	{
		char Value[64];
		if (!D3D12Config_GetString("Look", "Profile", Value, sizeof(Value)))
			return DRV_LOOK_CLASSIC;
		if (_stricmp(Value, "Enhanced") == 0)
			return DRV_LOOK_ENHANCED;
		if (_stricmp(Value, "Stylized") == 0)
			return DRV_LOOK_STYLIZED;
		if (_stricmp(Value, "Classic") == 0)
			return DRV_LOOK_CLASSIC;
		return std::atoi(Value);
	}

	int32 GetTonemapper()
	{
		char Value[64];
		if (!D3D12Config_GetString("Look", "Tonemapper", Value, sizeof(Value)))
			return DRV_TONEMAP_ACES;
		if (_stricmp(Value, "AgX") == 0)
			return DRV_TONEMAP_AGX;
		if (_stricmp(Value, "ACES") == 0)
			return DRV_TONEMAP_ACES;
		if (_stricmp(Value, "None") == 0)
			return DRV_TONEMAP_NONE;
		return std::atoi(Value);
	}

	void Clamp(DRV_LookSettings& L)
	{
		L.Profile = (std::max)(static_cast<int32>(DRV_LOOK_CLASSIC), (std::min)(L.Profile, static_cast<int32>(DRV_LOOK_STYLIZED)));
		L.Tonemapper = (std::max)(static_cast<int32>(DRV_TONEMAP_NONE), (std::min)(L.Tonemapper, static_cast<int32>(DRV_TONEMAP_AGX)));
		L.ExposureEV = (std::max)(-16.0f, (std::min)(L.ExposureEV, 16.0f));
		L.BloomIntensity = (std::max)(0.0f, (std::min)(L.BloomIntensity, 1.0f));
		L.SSAORadius = (std::max)(1.0f, L.SSAORadius);
		L.SSAOIntensity = (std::max)(0.0f, (std::min)(L.SSAOIntensity, 1.0f));
		L.HeightFogDensity = (std::max)(0.0f, L.HeightFogDensity);
		L.HeightFogFalloff = (std::max)(0.0f, L.HeightFogFalloff);
		for (int i = 0; i < 3; ++i)
			L.HeightFogColor[i] = (std::max)(0.0f, (std::min)(L.HeightFogColor[i], 1.0f));
		L.RenderScale = (std::max)(0.125f, (std::min)(L.RenderScale, 1.0f));
		L.PaletteBits = (std::max)(static_cast<int32>(0), (std::min)(L.PaletteBits, static_cast<int32>(8)));
		L.VertexSnap = (std::max)(static_cast<int32>(0), L.VertexSnap);
	}

	void LoadDefaults()
	{
		Look = {};
		Look.Profile = GetProfile();
		Look.Tonemapper = GetTonemapper();
		Look.AutoExposure = D3D12Config_GetInt("Look", "AutoExposure", 1);
		Look.ExposureEV = GetFloat("ExposureEV", 0.0f);
		Look.Bloom = D3D12Config_GetInt("Look", "Bloom", 1);
		Look.BloomIntensity = GetFloat("BloomIntensity", 0.05f);
		Look.SSAO = D3D12Config_GetInt("Look", "SSAO", 0);
		Look.SSAORadius = GetFloat("SSAORadius", 48.0f);
		Look.SSAOIntensity = GetFloat("SSAOIntensity", 0.6f);
		Look.Shadows = D3D12Config_GetInt("Look", "Shadows", 1);
		Look.HeightFogDensity = GetFloat("HeightFogDensity", 0.0f);
		Look.HeightFogBase = GetFloat("HeightFogBase", 0.0f);
		Look.HeightFogFalloff = GetFloat("HeightFogFalloff", 0.002f);
		Look.HeightFogColor[0] = GetFloat("HeightFogRed", 0.5f);
		Look.HeightFogColor[1] = GetFloat("HeightFogGreen", 0.55f);
		Look.HeightFogColor[2] = GetFloat("HeightFogBlue", 0.6f);
		Look.RenderScale = GetFloat("RenderScale", 0.5f);
		Look.PaletteBits = D3D12Config_GetInt("Look", "PaletteBits", 5);
		Look.Dither = D3D12Config_GetInt("Look", "Dither", 1);
		Look.CRT = D3D12Config_GetInt("Look", "CRT", 0);
		Look.VertexSnap = D3D12Config_GetInt("Look", "VertexSnap", 0);
		Look.SnapLighting = D3D12Config_GetInt("Look", "SnapLighting", 0);
		Clamp(Look);
		Loaded = true;

		static const char* Names[] = { "Classic", "Enhanced", "Stylized" };
		D3D12Log::GetPtr()->Printf("Look: %s profile (tonemapper %d, auto exposure %d, bloom %d, SSAO %d, shadows %d)",
			Names[Look.Profile], Look.Tonemapper, Look.AutoExposure, Look.Bloom, Look.SSAO, Look.Shadows);
	}
}

void D3D12Look_Startup()
{
	LoadDefaults();
}

const DRV_LookSettings& D3D12Look_Get()
{
	if (!Loaded)
		LoadDefaults();
	return Look;
}

bool D3D12Look_IsHDR()
{
	return D3D12Look_Get().Profile != DRV_LOOK_CLASSIC;
}

bool D3D12Look_IsStylized()
{
	return D3D12Look_Get().Profile == DRV_LOOK_STYLIZED;
}

float D3D12Look_RenderScale()
{
	return D3D12Look_IsStylized() ? D3D12Look_Get().RenderScale : 1.0f;
}

const D3D12Fog& D3D12Look_GetFog()
{
	return Fog;
}

grBoolean DRIVERCC D3D12Look_SetLook(const DRV_LookSettings* NewLook)
{
	if (!NewLook)
		return GR_FALSE;
	D3D12Look_Get();
	const int32 OldProfile = Look.Profile;
	Look = *NewLook;
	Clamp(Look);
	if (Look.Profile != OldProfile)
		D3D12Log::GetPtr()->Printf("Look: profile %d", Look.Profile);
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Look_GetLook(DRV_LookSettings* Out)
{
	if (!Out)
		return GR_FALSE;
	*Out = D3D12Look_Get();
	return GR_TRUE;
}

grBoolean DRIVERCC D3D12Look_SetFog(float r, float g, float b, float Start, float End, grBoolean Enable)
{
	// The engine's colors are 0..255
	const float Scale = (r > 1.0f || g > 1.0f || b > 1.0f) ? (1.0f / 255.0f) : 1.0f;
	Fog.Enabled = Enable != GR_FALSE && End > Start;
	Fog.Color[0] = (std::max)(0.0f, (std::min)(r * Scale, 1.0f));
	Fog.Color[1] = (std::max)(0.0f, (std::min)(g * Scale, 1.0f));
	Fog.Color[2] = (std::max)(0.0f, (std::min)(b * Scale, 1.0f));
	Fog.Start = Start;
	Fog.End = End;
	return GR_TRUE;
}
