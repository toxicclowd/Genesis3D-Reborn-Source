/****************************************************************************************/
/*  D3D12COMMON.H                                                                       */
/*                                                                                      */
/*  Common utilities and helper functions for DirectX 12 Driver                        */
/*                                                                                      */
/****************************************************************************************/
#ifndef D3D12COMMON_H
#define D3D12COMMON_H

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

// Helper macro for checking HRESULT values
#ifndef ThrowIfFailed
#define ThrowIfFailed(hr) \
    if (FAILED(hr)) { \
        D3D12Log::GetPtr()->Printf("ERROR: HRESULT failed at %s:%d - HR: 0x%08X", __FILE__, __LINE__, hr); \
    }
#endif

// Alignment helper
inline UINT Align(UINT size, UINT alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// Get aligned size for constant buffers (must be 256-byte aligned)
inline UINT GetConstantBufferAlignedSize(UINT size)
{
    return Align(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

// Helper to transition resource states
inline void TransitionResource(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    commandList->ResourceBarrier(1, &barrier);
}

// Helper to create a committed resource
inline HRESULT CreateCommittedResource(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_RESOURCE_DESC* desc,
    ID3D12Resource** resource,
    const D3D12_CLEAR_VALUE* clearValue = nullptr)
{
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    return device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        desc,
        initialState,
        clearValue,
        IID_PPV_ARGS(resource));
}

// Get DXGI format from Jet3D pixel format  
inline DXGI_FORMAT GetDXGIFormat(const jeRDriver_PixelFormat* drvFormat)
{
	if (!drvFormat)
		return DXGI_FORMAT_B8G8R8A8_UNORM; // Default
	
	// jePixelFormat is a uint32, check its value against the format constants
	jePixelFormat pixelFormat = drvFormat->PixelFormat;
	
	// Check for common formats based on the pixel format value
	if (pixelFormat == JE_PIXELFORMAT_32BIT_ARGB)
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	else if (pixelFormat == JE_PIXELFORMAT_24BIT_RGB)
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	else if (pixelFormat == JE_PIXELFORMAT_16BIT_565_RGB)
		return DXGI_FORMAT_B5G6R5_UNORM;
	else if (pixelFormat == JE_PIXELFORMAT_16BIT_4444_ARGB)
		return DXGI_FORMAT_B4G4R4A4_UNORM;
	
	return DXGI_FORMAT_B8G8R8A8_UNORM; // Default
}

#endif // D3D12COMMON_H
