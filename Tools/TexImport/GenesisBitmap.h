//=====================================================================================
//	GenesisBitmap.h
//
//	Genesis3D bitmaps for G3DTexImport, read through the (delay-loaded) engine DLL.
//=====================================================================================
#ifndef GENESISBITMAP_H
#define GENESISBITMAP_H

#include <vector>

// True for a file in the engine's own bitmap format (which WIC cannot read).
bool IsGenesisBitmapFile(const wchar_t* Path);
// Decodes it to 8-bit RGBA, top row first; alpha is 0 on color-keyed texels.
bool LoadGenesisBitmap(const wchar_t* Path, std::vector<unsigned char>& RGBA, int& Width, int& Height, bool& HasColorKey);

#endif // GENESISBITMAP_H
