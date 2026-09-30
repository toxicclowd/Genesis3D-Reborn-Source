//=====================================================================================
//	GenesisBitmap.cpp
//
//	Reads Genesis3D bitmaps (the engine's own compressed format inside a .bmp, and
//	any other file grBitmap reads) through the engine, so G3DTexImport can convert the
//	shipped library textures. The engine DLL is delay-loaded: only these files need it.
//=====================================================================================
#include <windows.h>
#include <cstdio>
#include <vector>

#include "GenesisBitmap.h"
#include "Genesis3D.h"

bool IsGenesisBitmapFile(const wchar_t* Path)
{
	// The engine's bitmaps live in a VFS container ("VFHH") holding a "GeBm" chunk
	FILE* File = nullptr;
	if (_wfopen_s(&File, Path, L"rb") != 0 || !File)
		return false;
	char Magic[4] = {};
	const bool Read = fread(Magic, 1, 4, File) == 4;
	fclose(File);
	return Read && Magic[0] == 'V' && Magic[1] == 'F' && Magic[2] == 'H' && Magic[3] == 'H';
}

bool LoadGenesisBitmap(const wchar_t* Path, std::vector<unsigned char>& RGBA, int& Width, int& Height, bool& HasColorKey)
{
	char Name[MAX_PATH * 2] = {};
	if (!WideCharToMultiByte(CP_ACP, 0, Path, -1, Name, sizeof(Name), nullptr, nullptr))
		return false;

	__try
	{
		grVFile* File = grVFile_OpenNewSystem(nullptr, GR_VFILE_TYPE_DOS, Name, nullptr, GR_VFILE_OPEN_READONLY);
		if (!File)
			return false;
		grBitmap* Bitmap = grBitmap_CreateFromFile(File);
		grVFile_Close(File);
		if (!Bitmap)
			return false;

		grBitmap_Info Info, Secondary;
		grBitmap_GetInfo(Bitmap, &Info, &Secondary);
		HasColorKey = Info.HasColorKey != GR_FALSE;

		grBitmap* Lock = nullptr;
		bool Ok = grBitmap_LockForRead(Bitmap, &Lock, 0, 0, GR_PIXELFORMAT_32BIT_ARGB, Info.HasColorKey, Info.ColorKey) && Lock;
		if (Ok)
		{
			grBitmap_Info LockInfo, Unused;
			grBitmap_GetInfo(Lock, &LockInfo, &Unused);
			const uint32* Bits = static_cast<const uint32*>(grBitmap_GetBits(Lock));
			Ok = Bits != nullptr;
			if (Ok)
			{
				Width = LockInfo.Width;
				Height = LockInfo.Height;
				RGBA.resize(static_cast<size_t>(Width) * Height * 4);
				for (int y = 0; y < Height; ++y)
				{
					for (int x = 0; x < Width; ++x)
					{
						const uint32 p = Bits[static_cast<size_t>(y) * LockInfo.Stride + x];
						unsigned char* d = &RGBA[(static_cast<size_t>(y) * Width + x) * 4];
						d[0] = static_cast<unsigned char>(p >> 16);
						d[1] = static_cast<unsigned char>(p >> 8);
						d[2] = static_cast<unsigned char>(p);
						d[3] = static_cast<unsigned char>(p >> 24);
					}
				}
			}
			grBitmap_UnLock(Lock);
		}
		grBitmap_Destroy(&Bitmap);
		return Ok;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// The engine DLL is delay-loaded; without it (or on a bad file) just report failure
		return false;
	}
}
