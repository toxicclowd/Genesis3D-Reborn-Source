/****************************************************************************************/
/*  D3D12PSOMANAGER.H                                                                   */
/*                                                                                      */
/*  DirectX 12 Pipeline State Object Manager                                           */
/*  Manages PSOs and shaders for different rendering modes                             */
/*                                                                                      */
/****************************************************************************************/
#ifndef D3D12_PSO_MANAGER_H
#define D3D12_PSO_MANAGER_H

#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "DCommon.h"

using Microsoft::WRL::ComPtr;

// PSO types
enum D3D12_PSO_TYPE
{
	PSO_GOURAUD = 0,      // Colored polygons
	PSO_TEXTURE,          // Single textured
	PSO_MULTITEX,         // Texture + Lightmap
	PSO_ALPHA_GOURAUD,    // Colored with alpha blending
	PSO_ALPHA_TEXTURE,    // Textured with alpha blending
	PSO_COUNT
};

// Root parameter slots of the TLPoly root signature.
enum D3D12_ROOT_PARAM
{
	ROOT_PARAM_DRAW = 0,		// 4 x 32-bit constants, b0
	ROOT_PARAM_FRAME,			// frame constants CBV, b1
	ROOT_PARAM_TEXTURES,		// bindless: the whole SRV heap (t0, space1)
	ROOT_PARAM_BASE_TABLE = ROOT_PARAM_TEXTURES,	// descriptor tables: t0
	ROOT_PARAM_LIGHT_TABLE						// descriptor tables: t1
};

// Per-draw root constants (DrawConstants in Shaders/TLPoly.hlsl).
struct D3D12DrawConstants
{
	uint32 Flags;
	uint32 BaseTextureIndex;
	uint32 LightTextureIndex;
	uint32 Padding;
};

// Per-frame constant buffer (FrameConstants in Shaders/TLPoly.hlsl).
struct D3D12FrameConstants
{
	float ViewportSize[2];
	float InvViewportSize[2];
	uint32 FrameNumber;
	float TimeSeconds;
	float Padding[2];
};

// PSO Manager class
class D3D12PSOManager
{
public:
	D3D12PSOManager();
	~D3D12PSOManager();

	// Initialize PSO manager
	grBoolean Initialize();
	void Shutdown();

	// Get a PSO by type
	ID3D12PipelineState* GetPSO(D3D12_PSO_TYPE type, uint32 flags, grBoolean sceneWireframe);
	ID3D12RootSignature* GetRootSignature();
	// True when pixel shaders read textures by heap index (ROOT_PARAM_TEXTURES).
	bool IsBindless() const { return m_bBindless; }

private:
	// Create all PSOs
	grBoolean CreateRootSignature();
	grBoolean CreatePSO(D3D12_PSO_TYPE type, uint32 stateIndex);

	// Root signature and PSOs
	ComPtr<ID3D12RootSignature> m_pRootSignature;
	static const uint32 PSO_STATE_COUNT = 16;
	ComPtr<ID3D12PipelineState> m_PSOs[PSO_COUNT][PSO_STATE_COUNT];

	bool m_bInitialized;
	bool m_bBindless;
};

#endif // D3D12_PSO_MANAGER_H
