/****************************************************************************************/
/*  D3D12SHADERS.H                                                                      */
/*                                                                                      */
/*  Shader bytecode for the DirectX 12 driver.                                          */
/*                                                                                      */
/*  Shaders live in Shaders\*.hlsl and are compiled by DXC (SM 6.0) when the driver is  */
/*  built. The HLSL is also embedded as a resource and compiled at run time with       */
/*  D3DCompile (SM 5.0) when:                                                           */
/*    - the device has no SM 6.0 support,                                               */
/*    - [Shaders] RuntimeCompile=1 is set in Direct3D12Driver.ini, or                   */
/*    - [Shaders] SourceDir points at a folder of .hlsl files (for iterating on         */
/*      shaders without rebuilding the driver).                                         */
/****************************************************************************************/
#ifndef D3D12SHADERS_H
#define D3D12SHADERS_H

#include <d3d12.h>
#include "DCommon.h"

enum D3D12_SHADER_ID
{
	SHADER_TLPOLY_VS = 0,
	SHADER_TLPOLY_PS_GOURAUD,
	SHADER_TLPOLY_PS_TEXTURE,
	SHADER_TLPOLY_PS_MULTITEX,
	SHADER_COUNT
};

grBoolean				D3D12Shaders_Load(ID3D12Device* Device);
void					D3D12Shaders_Unload();
D3D12_SHADER_BYTECODE	D3D12Shaders_Get(D3D12_SHADER_ID Id);

#endif // D3D12SHADERS_H
