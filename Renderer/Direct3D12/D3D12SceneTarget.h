/****************************************************************************************/
/*  D3D12SCENETARGET.H                                                                  */
/*                                                                                      */
/*  The HDR scene color target (roadmap Phase 0 frame structure).                       */
/*                                                                                      */
/*  Everything the engine draws in a scene goes into an R16G16B16A16_FLOAT target.     */
/*  EndScene then runs the present pass (Shaders\Present.hlsl), which writes the        */
/*  swap-chain back buffer. [Render] SceneTarget=0 in Direct3D12Driver.ini draws        */
/*  straight to the back buffer instead, as the driver did before.                      */
/****************************************************************************************/
#ifndef D3D12SCENETARGET_H
#define D3D12SCENETARGET_H

#include <d3d12.h>
#include "DCommon.h"

// Creates (or re-creates after a resize) the scene target. With the scene target
// disabled this succeeds without allocating anything.
grBoolean		D3D12Scene_Create(UINT Width, UINT Height);
// Releases the size-dependent resources (before a swap-chain resize).
void			D3D12Scene_Release();
// Releases everything, including the present pipeline.
void			D3D12Scene_Shutdown();

bool			D3D12Scene_IsEnabled();
// Format the scene's draw pipelines must render to.
DXGI_FORMAT		D3D12Scene_GetFormat();

// Makes the scene target renderable and returns its RTV.
D3D12_CPU_DESCRIPTOR_HANDLE	D3D12Scene_Begin(ID3D12GraphicsCommandList* CommandList);
// Draws the scene into BackBufferRTV (already in the render-target state).
grBoolean		D3D12Scene_Present(ID3D12GraphicsCommandList* CommandList, D3D12_CPU_DESCRIPTOR_HANDLE BackBufferRTV);

#endif // D3D12SCENETARGET_H
