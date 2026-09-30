/****************************************************************************************/
/*  TEXIMPORT.CPP                                                                       */
/*                                                                                      */
/*  G3DTexImport (roadmap Phase 2): turns source images into the engine's DDS textures  */
/*  and PBR materials.                                                                  */
/*                                                                                      */
/*  Textures are compressed with mipmaps here, at import time, not when a level loads: */
/*    base, emissive  BC7 sRGB      (color data)                                         */
/*    normal          BC5           (tangent space, DirectX convention: +Y down)        */
/*    ORM             BC7 linear    (R occlusion, G roughness, B metalness)             */
/*    height          BC4           (linear)                                            */
/*  The renderer loads sRGB formats as their UNORM equivalents and converts in the     */
/*  shader, so legacy bitmaps and imported textures go through the same math.          */
/*                                                                                      */
/*  Usage:                                                                              */
/*    G3DTexImport <image> [-type base|normal|orm|emissive|height] [-o out.dds]         */
/*    G3DTexImport -material <name> <base image> [-outdir dir] [-pak Pak] [options]     */
/*                                                                                      */
/*  -material finds the other maps next to the base image by suffix (_n / _normal,      */
/*  _orm / _arm, _e / _emissive, _h / _height; or separate _ao, _rough / _roughness,     */
/*  _gloss, _metal / _metallic maps, which are packed into an ORM map), writes          */
/*  <outdir>/<Pak>/<name>[_n|_orm|_e|_h].dds and <outdir>/<name>.jmat (version 2) whose */
/*  texture layers are named "<Pak>:<name>...". With -outdir bin\GlobalMaterials a      */
/*  level material called <name> then uses it. -matdir puts the .jmat elsewhere (e.g.  */
/*  a G3D_MATERIAL_OVERRIDES directory).                                                 */
/****************************************************************************************/
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>
#include "DirectXTex.h"

namespace fs = std::filesystem;
using namespace DirectX;

namespace
{
	enum MapType { MAP_BASE, MAP_NORMAL, MAP_ORM, MAP_EMISSIVE, MAP_HEIGHT, MAP_COUNT };

	const wchar_t* const MapNames[MAP_COUNT] = { L"base", L"normal", L"orm", L"emissive", L"height" };
	const wchar_t* const MapSuffixes[MAP_COUNT] = { L"", L"_n", L"_orm", L"_e", L"_h" };

	// Layer types (grMaterialSpec_LayerType) and the .jmat layout (grMaterialSpec.cpp).
	const uint8_t LayerTypes[MAP_COUNT] = { 0, 3, 4, 5, 6 };
	const uint16_t RESOURCE_TEXTURE = 0x0011;
	const uint16_t MATSPEC_SIZE_FLAG = 0x0040;
	const uint16_t MATSPEC_PBR_FLAG = 0x0080;
	const size_t NAME_SIZE = 256;
	const size_t XFORM_SIZE = 64;		// grXForm3d, 16-byte aligned

	struct Options
	{
		bool Fast = false;
		bool NoMips = false;
		bool FlipGreen = false;
		float Tint[3] = { 1.0f, 1.0f, 1.0f };
		float Roughness = -1.0f;		// < 0: default
		float Metal = -1.0f;
		float Emissive[3] = { -1.0f, -1.0f, -1.0f };
		float Intensity = 1.0f;
		float Cutoff = 0.5f;
		uint8_t AlphaMode = 0;
		uint16_t Flags = 0;
	};

	void Error(const wchar_t* Format, ...)
	{
		va_list Args;
		va_start(Args, Format);
		fwprintf(stderr, L"G3DTexImport: ");
		vfwprintf(stderr, Format, Args);
		fwprintf(stderr, L"\n");
		va_end(Args);
	}

	bool HasExtension(const fs::path& Path, const wchar_t* Extension)
	{
		return _wcsicmp(Path.extension().c_str(), Extension) == 0;
	}

	// Any image as R8G8B8A8_UNORM with a single level.
	bool LoadImage(const fs::path& Path, ScratchImage& Out)
	{
		ScratchImage Loaded;
		TexMetadata Info;
		HRESULT Hr;
		if (HasExtension(Path, L".dds"))
			Hr = LoadFromDDSFile(Path.c_str(), DDS_FLAGS_NONE, &Info, Loaded);
		else if (HasExtension(Path, L".tga"))
			Hr = LoadFromTGAFile(Path.c_str(), TGA_FLAGS_IGNORE_SRGB, &Info, Loaded);
		else
			Hr = LoadFromWICFile(Path.c_str(), WIC_FLAGS_IGNORE_SRGB, &Info, Loaded);
		if (FAILED(Hr))
		{
			Error(L"cannot read %ls (0x%08X)", Path.c_str(), static_cast<unsigned>(Hr));
			return false;
		}

		// No gamma conversion anywhere: texels keep their encoding, the output format says which.
		if (IsSRGB(Info.format))
			Loaded.OverrideFormat(MakeLinear(Info.format));
		const bool Gray = (Info.format == DXGI_FORMAT_R8_UNORM || Info.format == DXGI_FORMAT_R16_UNORM ||
			Info.format == DXGI_FORMAT_R16_FLOAT || Info.format == DXGI_FORMAT_R32_FLOAT ||
			Info.format == DXGI_FORMAT_BC4_UNORM);

		const Image* Base = Loaded.GetImage(0, 0, 0);
		ScratchImage Decompressed;
		if (IsCompressed(Info.format))
		{
			if (FAILED(Decompress(*Base, DXGI_FORMAT_R8G8B8A8_UNORM, Decompressed)))
			{
				Error(L"cannot decompress %ls", Path.c_str());
				return false;
			}
			Base = Decompressed.GetImage(0, 0, 0);
		}
		if (Base->format == DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			if (FAILED(Out.InitializeFromImage(*Base)))
				return false;
		}
		else if (FAILED(Convert(*Base, DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, Out)))
		{
			Error(L"cannot convert %ls", Path.c_str());
			return false;
		}
		if (Gray)
		{
			// Grayscale sources convert to red only
			const Image* Pixels = Out.GetImage(0, 0, 0);
			for (size_t y = 0; y < Pixels->height; ++y)
			{
				uint8_t* Row = Pixels->pixels + y * Pixels->rowPitch;
				for (size_t x = 0; x < Pixels->width; ++x)
					Row[x * 4 + 1] = Row[x * 4 + 2] = Row[x * 4];
			}
		}
		return true;
	}

	bool IsPowerOfTwo(size_t Value)
	{
		return Value && !(Value & (Value - 1));
	}

	// Mipmaps, compression and the DDS file for one map (Image is R8G8B8A8_UNORM).
	bool WriteMap(ScratchImage& Image, MapType Type, const fs::path& Out, const Options& Opts)
	{
		const TexMetadata& Info = Image.GetMetadata();
		if (!IsPowerOfTwo(Info.width) || !IsPowerOfTwo(Info.height))
			wprintf(L"  warning: %ls is %zux%zu; the engine's texture coordinates expect power-of-two sizes\n",
				Out.filename().c_str(), Info.width, Info.height);

		const bool Color = (Type == MAP_BASE || Type == MAP_EMISSIVE);
		if (Color)
			Image.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);	// mipmaps filter in linear light

		ScratchImage Mips;
		ScratchImage* Source = &Image;
		if (!Opts.NoMips && (Info.width > 1 || Info.height > 1))
		{
			if (FAILED(GenerateMipMaps(*Image.GetImage(0, 0, 0), TEX_FILTER_BOX | TEX_FILTER_FORCE_NON_WIC, 0, Mips)))
			{
				Error(L"cannot generate mipmaps for %ls", Out.c_str());
				return false;
			}
			Source = &Mips;
		}

		DXGI_FORMAT Format = DXGI_FORMAT_BC7_UNORM;
		switch (Type)
		{
		case MAP_BASE:
		case MAP_EMISSIVE:	Format = DXGI_FORMAT_BC7_UNORM_SRGB; break;
		case MAP_NORMAL:	Format = DXGI_FORMAT_BC5_UNORM; break;
		case MAP_ORM:		Format = DXGI_FORMAT_BC7_UNORM; break;
		case MAP_HEIGHT:	Format = DXGI_FORMAT_BC4_UNORM; break;
		default: break;
		}
		TEX_COMPRESS_FLAGS Compress_ = TEX_COMPRESS_PARALLEL;
		if (Opts.Fast)
			Compress_ |= TEX_COMPRESS_BC7_QUICK;
		ScratchImage Compressed;
		if (FAILED(Compress(Source->GetImages(), Source->GetImageCount(), Source->GetMetadata(), Format,
			Compress_, TEX_THRESHOLD_DEFAULT, Compressed)))
		{
			Error(L"cannot compress %ls", Out.c_str());
			return false;
		}

		std::error_code Ignored;
		if (Out.has_parent_path())
			fs::create_directories(Out.parent_path(), Ignored);
		if (FAILED(SaveToDDSFile(Compressed.GetImages(), Compressed.GetImageCount(), Compressed.GetMetadata(),
			DDS_FLAGS_NONE, Out.c_str())))
		{
			Error(L"cannot write %ls", Out.c_str());
			return false;
		}
		wprintf(L"  %-8ls %ls (%zux%zu, %zu mips)\n", MapNames[Type], Out.c_str(), Info.width, Info.height,
			Compressed.GetMetadata().mipLevels);
		return true;
	}

	bool FlipGreen(ScratchImage& Scratch)
	{
		const Image* Pixels = Scratch.GetImage(0, 0, 0);
		for (size_t y = 0; y < Pixels->height; ++y)
		{
			uint8_t* Row = Pixels->pixels + y * Pixels->rowPitch;
			for (size_t x = 0; x < Pixels->width; ++x)
				Row[x * 4 + 1] = static_cast<uint8_t>(255 - Row[x * 4 + 1]);
		}
		return true;
	}

	// The first existing <Stem><suffix>.<ext> next to Base.
	fs::path FindSibling(const fs::path& Dir, const std::wstring& Stem, std::initializer_list<const wchar_t*> Suffixes)
	{
		static const wchar_t* const Extensions[] = { L".png", L".tga", L".dds", L".jpg", L".jpeg", L".bmp", L".tif", L".tiff" };
		for (const wchar_t* Suffix : Suffixes)
			for (const wchar_t* Extension : Extensions)
			{
				fs::path Candidate = Dir / (Stem + Suffix + Extension);
				if (fs::exists(Candidate))
					return Candidate;
			}
		return fs::path();
	}

	// Packs separate grayscale maps (red channel) into one ORM image; missing ones get
	// occlusion 1, roughness 1 and metalness 0. Gloss is inverted into roughness.
	bool PackORM(const fs::path& AO, const fs::path& Rough, bool Gloss, const fs::path& Metal, ScratchImage& Out)
	{
		ScratchImage Maps[3];
		const fs::path* Paths[3] = { &AO, &Rough, &Metal };
		size_t Width = 0, Height = 0;
		for (int i = 0; i < 3; ++i)
		{
			if (Paths[i]->empty())
				continue;
			if (!LoadImage(*Paths[i], Maps[i]))
				return false;
			const TexMetadata& Info = Maps[i].GetMetadata();
			if (Width && (Info.width != Width || Info.height != Height))
			{
				Error(L"%ls is not the same size as the other ORM inputs", Paths[i]->c_str());
				return false;
			}
			Width = Info.width;
			Height = Info.height;
		}
		if (!Width || FAILED(Out.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, Width, Height, 1, 1)))
			return false;
		const uint8_t Defaults[3] = { 255, 255, 0 };
		const Image* Dst = Out.GetImage(0, 0, 0);
		for (size_t y = 0; y < Height; ++y)
			for (size_t x = 0; x < Width; ++x)
			{
				uint8_t* Pixel = Dst->pixels + y * Dst->rowPitch + x * 4;
				for (int i = 0; i < 3; ++i)
				{
					const Image* Src = Maps[i].GetImage(0, 0, 0);
					uint8_t Value = Src ? Src->pixels[y * Src->rowPitch + x * 4] : Defaults[i];
					if (i == 1 && Src && Gloss)
						Value = static_cast<uint8_t>(255 - Value);
					Pixel[i] = Value;
				}
				Pixel[3] = 255;
			}
		return true;
	}

	void PutName(std::vector<uint8_t>& Data, const std::string& Name)
	{
		size_t Start = Data.size();
		Data.resize(Start + NAME_SIZE, 0);
		std::memcpy(Data.data() + Start, Name.c_str(), (std::min)(Name.size(), NAME_SIZE - 1));
	}

	template <typename T> void Put(std::vector<uint8_t>& Data, const T& Value)
	{
		const uint8_t* Bytes = reinterpret_cast<const uint8_t*>(&Value);
		Data.insert(Data.end(), Bytes, Bytes + sizeof(T));
	}

	std::string Narrow(const std::wstring& Text)
	{
		std::string Out;
		for (wchar_t c : Text)
			Out += static_cast<char>((c < 128) ? c : '_');
		return Out;
	}

	// .jmat version 2: size, PBR block (grMaterialSpec_PBR), then one texture layer per map.
	bool WriteMaterial(const fs::path& Path, const std::wstring& Name, const std::wstring& Pak,
		const bool (&Present)[MAP_COUNT], size_t Width, size_t Height, const Options& Opts)
	{
		std::vector<uint8_t> Data;
		Data.insert(Data.end(), { 'J', 'M', 'A', 'T' });
		Put<uint8_t>(Data, 2);
		Put<uint16_t>(Data, MATSPEC_SIZE_FLAG | MATSPEC_PBR_FLAG);
		uint8_t Count = 0;
		for (int i = 0; i < MAP_COUNT; ++i)
			Count = static_cast<uint8_t>(Count + (Present[i] ? 1 : 0));
		Put<uint8_t>(Data, Count);
		Put<uint16_t>(Data, static_cast<uint16_t>((std::min<size_t>)(Width, 65535)));
		Put<uint16_t>(Data, static_cast<uint16_t>((std::min<size_t>)(Height, 65535)));

		// Without an ORM map the factors are the values: dielectric, fully rough.
		const float Roughness = (Opts.Roughness >= 0.0f) ? Opts.Roughness : 1.0f;
		const float Metal = (Opts.Metal >= 0.0f) ? Opts.Metal : (Present[MAP_ORM] ? 1.0f : 0.0f);
		float Emissive[3];
		for (int i = 0; i < 3; ++i)
			Emissive[i] = (Opts.Emissive[0] >= 0.0f) ? Opts.Emissive[i] : (Present[MAP_EMISSIVE] ? 1.0f : 0.0f);
		Put(Data, Opts.Tint[0]); Put(Data, Opts.Tint[1]); Put(Data, Opts.Tint[2]); Put(Data, 1.0f);
		Put(Data, Roughness);
		Put(Data, Metal);
		Put(Data, Emissive[0]); Put(Data, Emissive[1]); Put(Data, Emissive[2]);
		Put(Data, Opts.Intensity);
		Put(Data, Opts.Cutoff);
		Put<uint8_t>(Data, Opts.AlphaMode);
		Put<uint8_t>(Data, 0);
		Put<uint16_t>(Data, Opts.Flags);

		float Identity[XFORM_SIZE / sizeof(float)] = {};
		Identity[0] = Identity[5] = Identity[10] = 1.0f;		// AX, BY, CZ
		for (int i = 0; i < MAP_COUNT; ++i)
		{
			if (!Present[i])
				continue;
			std::wstring Resource = Name + MapSuffixes[i];
			if (!Pak.empty())
				Resource = Pak + L":" + Resource;
			Put<uint16_t>(Data, RESOURCE_TEXTURE);
			Put<uint8_t>(Data, LayerTypes[i]);
			Put<uint8_t>(Data, 0);
			PutName(Data, Narrow(Resource));
			Put(Data, Identity);
		}

		FILE* File = _wfopen(Path.c_str(), L"wb");
		if (!File)
		{
			Error(L"cannot write %ls", Path.c_str());
			return false;
		}
		fwrite(Data.data(), 1, Data.size(), File);
		fclose(File);
		wprintf(L"  material %ls\n", Path.c_str());
		return true;
	}

	int ImportMaterial(const std::wstring& Name, const fs::path& BasePath, const fs::path& OutDir,
		const fs::path& MatDir, const std::wstring& Pak, const Options& Opts)
	{
		const fs::path Dir = BasePath.parent_path();
		std::wstring Stem = BasePath.stem().wstring();
		for (const wchar_t* Suffix : { L"_albedo", L"_basecolor", L"_base", L"_diffuse", L"_color", L"_col", L"_d" })
		{
			const size_t Length = wcslen(Suffix);
			if (Stem.size() > Length && _wcsicmp(Stem.c_str() + Stem.size() - Length, Suffix) == 0)
			{
				Stem.resize(Stem.size() - Length);
				break;
			}
		}

		fs::path Inputs[MAP_COUNT];
		Inputs[MAP_BASE] = BasePath;
		Inputs[MAP_NORMAL] = FindSibling(Dir, Stem, { L"_n", L"_normal", L"_nrm", L"_nor" });
		Inputs[MAP_ORM] = FindSibling(Dir, Stem, { L"_orm", L"_arm" });
		Inputs[MAP_EMISSIVE] = FindSibling(Dir, Stem, { L"_e", L"_emissive", L"_emit", L"_emission" });
		Inputs[MAP_HEIGHT] = FindSibling(Dir, Stem, { L"_h", L"_height", L"_disp", L"_displacement" });
		fs::path AO, Rough, Metal;
		bool Gloss = false;
		if (Inputs[MAP_ORM].empty())
		{
			AO = FindSibling(Dir, Stem, { L"_ao", L"_occlusion" });
			Rough = FindSibling(Dir, Stem, { L"_rough", L"_roughness" });
			if (Rough.empty() && !(Rough = FindSibling(Dir, Stem, { L"_gloss", L"_glossiness" })).empty())
				Gloss = true;
			Metal = FindSibling(Dir, Stem, { L"_metal", L"_metallic", L"_metalness" });
		}

		wprintf(L"material %ls\n", Name.c_str());
		const fs::path TextureDir = Pak.empty() ? OutDir : OutDir / Pak;
		bool Present[MAP_COUNT] = {};
		size_t Width = 0, Height = 0;
		for (int i = 0; i < MAP_COUNT; ++i)
		{
			ScratchImage Image;
			if (i == MAP_ORM && Inputs[i].empty() && (!AO.empty() || !Rough.empty() || !Metal.empty()))
			{
				if (!PackORM(AO, Rough, Gloss, Metal, Image))
					return 1;
			}
			else if (Inputs[i].empty())
				continue;
			else if (!LoadImage(Inputs[i], Image))
				return 1;
			if (i == MAP_NORMAL && Opts.FlipGreen)
				FlipGreen(Image);
			if (i == MAP_BASE)
			{
				Width = Image.GetMetadata().width;
				Height = Image.GetMetadata().height;
			}
			if (!WriteMap(Image, static_cast<MapType>(i), TextureDir / (Name + MapSuffixes[i] + L".dds"), Opts))
				return 1;
			Present[i] = true;
		}
		std::error_code Ignored;
		fs::create_directories(MatDir, Ignored);
		return WriteMaterial(MatDir / (Name + L".jmat"), Name, Pak, Present, Width, Height, Opts) ? 0 : 1;
	}

	void Usage()
	{
		wprintf(
			L"G3DTexImport - converts images to the engine's DDS textures and PBR materials\n"
			L"\n"
			L"  G3DTexImport <image> [-type base|normal|orm|emissive|height] [-o out.dds]\n"
			L"  G3DTexImport -material <name> <base image> [-outdir dir] [-pak Pak] [-matdir dir]\n"
			L"\n"
			L"Inputs: PNG, TGA, DDS, JPEG, BMP, TIFF. Maps next to the base image are found by suffix:\n"
			L"  _n/_normal  _orm/_arm  _e/_emissive  _h/_height, or _ao _rough/_gloss _metal (packed into ORM)\n"
			L"\n"
			L"Options:\n"
			L"  -fast               quicker, lower quality BC7\n"
			L"  -nomips             no mipmaps\n"
			L"  -flipgreen          the normal map is OpenGL style (+Y up)\n"
			L"  -tint r g b         base color tint (linear, 0..1)\n"
			L"  -roughness f        roughness (times the ORM map's G when there is one)\n"
			L"  -metal f            metalness (times the ORM map's B when there is one)\n"
			L"  -emissive r g b     emissive color; -intensity f scales it\n"
			L"  -cutout f | -blend  alpha mode (default opaque)\n"
			L"  -twosided -retro    material flags\n");
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		Usage();
		return 1;
	}

	Options Opts;
	std::wstring MaterialName, Pak;
	fs::path Input, Output, OutDir = L".", MatDir;
	MapType Type = MAP_COUNT;
	for (int i = 1; i < argc; ++i)
	{
		const wchar_t* Arg = argv[i];
		const int Left = argc - i - 1;
		auto Float = [&]() { return static_cast<float>(_wtof(argv[++i])); };
		if (!_wcsicmp(Arg, L"-material") && Left >= 2)
		{
			MaterialName = argv[++i];
			Input = argv[++i];
		}
		else if (!_wcsicmp(Arg, L"-type") && Left >= 1)
		{
			++i;
			for (int t = 0; t < MAP_COUNT; ++t)
				if (!_wcsicmp(argv[i], MapNames[t]))
					Type = static_cast<MapType>(t);
			if (Type == MAP_COUNT)
			{
				Error(L"unknown type %ls", argv[i]);
				return 1;
			}
		}
		else if (!_wcsicmp(Arg, L"-o") && Left >= 1)
			Output = argv[++i];
		else if (!_wcsicmp(Arg, L"-outdir") && Left >= 1)
			OutDir = argv[++i];
		else if (!_wcsicmp(Arg, L"-matdir") && Left >= 1)
			MatDir = argv[++i];
		else if (!_wcsicmp(Arg, L"-pak") && Left >= 1)
			Pak = argv[++i];
		else if (!_wcsicmp(Arg, L"-fast"))
			Opts.Fast = true;
		else if (!_wcsicmp(Arg, L"-nomips"))
			Opts.NoMips = true;
		else if (!_wcsicmp(Arg, L"-flipgreen"))
			Opts.FlipGreen = true;
		else if (!_wcsicmp(Arg, L"-tint") && Left >= 3)
		{
			Opts.Tint[0] = Float(); Opts.Tint[1] = Float(); Opts.Tint[2] = Float();
		}
		else if (!_wcsicmp(Arg, L"-roughness") && Left >= 1)
			Opts.Roughness = Float();
		else if (!_wcsicmp(Arg, L"-metal") && Left >= 1)
			Opts.Metal = Float();
		else if (!_wcsicmp(Arg, L"-emissive") && Left >= 3)
		{
			Opts.Emissive[0] = Float(); Opts.Emissive[1] = Float(); Opts.Emissive[2] = Float();
		}
		else if (!_wcsicmp(Arg, L"-intensity") && Left >= 1)
			Opts.Intensity = Float();
		else if (!_wcsicmp(Arg, L"-cutout") && Left >= 1)
		{
			Opts.AlphaMode = 1;
			Opts.Cutoff = Float();
		}
		else if (!_wcsicmp(Arg, L"-blend"))
			Opts.AlphaMode = 2;
		else if (!_wcsicmp(Arg, L"-twosided"))
			Opts.Flags |= 0x0001;
		else if (!_wcsicmp(Arg, L"-retro"))
			Opts.Flags |= 0x0002;
		else if (Arg[0] != L'-' && Input.empty())
			Input = Arg;
		else
		{
			Error(L"unknown or incomplete option %ls", Arg);
			Usage();
			return 1;
		}
	}
	if (Input.empty())
	{
		Usage();
		return 1;
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		Error(L"COM initialization failed");
		return 1;
	}

	int Result = 0;
	if (!MaterialName.empty())
		Result = ImportMaterial(MaterialName, Input, OutDir, MatDir.empty() ? OutDir : MatDir, Pak, Opts);
	else
	{
		if (Type == MAP_COUNT)
		{
			// Guess from the suffix, else base color
			const std::wstring Stem = Input.stem().wstring();
			Type = MAP_BASE;
			for (int t = MAP_NORMAL; t < MAP_COUNT; ++t)
			{
				const size_t Length = wcslen(MapSuffixes[t]);
				if (Stem.size() > Length && _wcsicmp(Stem.c_str() + Stem.size() - Length, MapSuffixes[t]) == 0)
					Type = static_cast<MapType>(t);
			}
		}
		if (Output.empty())
			Output = OutDir / Input.filename().replace_extension(L".dds");
		ScratchImage Image;
		if (!LoadImage(Input, Image))
			Result = 1;
		else
		{
			if (Type == MAP_NORMAL && Opts.FlipGreen)
				FlipGreen(Image);
			Result = WriteMap(Image, Type, Output, Opts) ? 0 : 1;
		}
	}

	CoUninitialize();
	return Result;
}
