# DirectX 12 Renderer Implementation Plan for Jet3D

## Overview
Create a modern DirectX 12 renderer that implements the Jet3D driver interface (DRV_Driver) as a drop-in replacement for the legacy Direct3D9Driver.

## Driver Interface Analysis

### Core Interface (from DCommon.h)
The `DRV_Driver` structure contains ~40 function pointers that must be implemented:

#### 1. **Initialization & Management**
- `DRV_INIT *Init` - Initialize D3D12 device, command queue, swap chain
- `DRV_SHUTDOWN *Shutdown` - Clean up all D3D12 resources
- `DRV_RESET *Reset` - Handle device resets
- `DRV_UPDATE_WINDOW *UpdateWindow` - Handle window resizing
- `DRV_SET_ACTIVE *SetActive` - Activate/deactivate rendering

#### 2. **Enumeration**
- `DRV_ENUM_DRIVER *EnumSubDrivers` - Report "DirectX 12 Driver"
- `DRV_ENUM_MODES *EnumModes` - Enumerate display modes via DXGI
- `DRV_ENUM_PFORMAT *EnumPixelFormats` - Report supported pixel formats

#### 3. **Texture Management**
- `CREATE_TEXTURE *THandle_Create` - Create D3D12 textures
- `CREATE_TEXTURE_FROM_FILE *THandle_CreateFromFile` - Load from file
- `DESTROY_TEXTURE *THandle_Destroy` - Release textures
- `LOCK_THANDLE *THandle_Lock` - Map texture data (upload heap)
- `UNLOCK_THANDLE *THandle_UnLock` - Unmap and copy to default heap

#### 4. **Scene Rendering**
- `BEGIN_SCENE *BeginScene` - Begin frame, reset command list
- `END_SCENE *EndScene` - Submit commands, present
- `BEGIN_BATCH *BeginBatch` - Begin batching geometry
- `END_BATCH *EndBatch` - Flush batched geometry

#### 5. **Geometry Rendering**
- `RENDER_G_POLY *RenderGouraudPoly` - Render untextured polygons
- `RENDER_W_POLY *RenderWorldPoly` - Render world geometry with lightmaps
- `RENDER_MT_POLY *RenderMiscTexturePoly` - Render textured polygons

#### 6. **Additional Features**
- `DRAW_DECAL *DrawDecal` - 2D sprite rendering
- `SCREEN_SHOT *ScreenShot` - Capture framebuffer
- `SET_GAMMA / GET_GAMMA` - Gamma correction
- `SET_MATRIX / GET_MATRIX` - Transform matrices
- Font rendering, static meshes, fog, etc.

## DirectX 12 Architecture

### Components Needed

#### 1. **Core D3D12 Objects**
```cpp
- ID3D12Device
- IDXGISwapChain3
- ID3D12CommandQueue
- ID3D12DescriptorHeap (for RTV, DSV, CBV/SRV/UAV)
- ID3D12CommandAllocator (per frame)
- ID3D12GraphicsCommandList
```

#### 2. **Resource Management**
```cpp
- Texture upload heap (staging)
- Vertex buffer upload heap
- Constant buffers (for matrices, etc.)
- Descriptor heap management
```

#### 3. **Pipeline State Objects (PSOs)**
```cpp
- Gouraud shading PSO
- Textured PSO (single texture)
- Lightmap PSO (multi-texture: base + lightmap)
- Decal/2D PSO
```

#### 4. **Synchronization**
```cpp
- Fence for CPU/GPU sync
- Frame resource management (triple buffering)
```

## Implementation Strategy

### Phase 1: Basic Infrastructure
**Goal:** Get a clear screen rendering

**Tasks:**
1. Create project structure (Direct3D12Driver directory)
2. Set up D3D12 device and swap chain
3. Implement Init/Shutdown
4. Implement BeginScene/EndScene with clear
5. Create DriverHook entry point

**Files:**
- `Direct3D12Driver.h`
- `Direct3D12Driver.cpp`
- `D3D12Common.h` (helper utilities)
- `Direct3D12Driver.vcxproj`

### Phase 2: Texture Support
**Goal:** Load and display textures

**Tasks:**
1. Implement THandle_Create with upload heap
2. Implement Lock/Unlock for texture updates
3. Create descriptor heaps for SRVs
4. Implement basic texture sampler

### Phase 3: Geometry Rendering
**Goal:** Render 3D geometry

**Tasks:**
1. Implement vertex buffer batching system
2. Create PSO for gouraud shading
3. Implement RenderGouraudPoly
4. Set up constant buffers for matrices
5. Implement SetMatrix/GetMatrix

### Phase 4: Advanced Rendering
**Goal:** Full feature parity with D3D9 driver

**Tasks:**
1. Multi-texture support (lightmaps)
2. Implement RenderWorldPoly
3. Alpha blending and color keying
4. Fog support
5. Static mesh rendering
6. Font rendering (DirectWrite integration)

### Phase 5: Optimization
**Goal:** Performance and polish

**Tasks:**
1. GPU resource pooling
2. Command list bundling
3. Asynchronous texture uploads
4. PSO caching
5. Memory management optimization

## Shader Strategy

### Required Shaders (HLSL)

#### 1. **Gouraud Vertex Shader**
```hlsl
struct VS_INPUT {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

cbuffer Constants : register(b0) {
    float4x4 WorldViewProj;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = mul(float4(input.Position, 1.0), WorldViewProj);
    output.Color = input.Color;
    return output;
}
```

#### 2. **Textured Vertex Shader**
```hlsl
struct VS_INPUT {
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

// Similar structure...
```

#### 3. **Pixel Shaders**
- Simple passthrough for gouraud
- Textured with alpha testing
- Multi-texture for lightmaps

## Key Challenges & Solutions

### Challenge 1: Texture Upload
**D3D9:** Lock/Unlock directly maps memory  
**D3D12:** Use upload heap ? copy to default heap

**Solution:**
- Create staging upload heap for Lock
- On Unlock, record copy command to default heap
- Track pending uploads per frame

### Challenge 2: Immediate Mode vs Command Lists
**D3D9:** Immediate rendering  
**D3D12:** Command list recording

**Solution:**
- Buffer all rendering calls in BeginBatch
- Execute command list in EndBatch/EndScene
- Maintain per-frame command allocators

### Challenge 3: Fixed Function to Shaders
**D3D9:** Fixed-function pipeline  
**D3D12:** Programmable pipeline only

**Solution:**
- Emulate fixed-function with simple shaders
- Use constant buffers for matrices
- Vertex color for gouraud shading

### Challenge 4: Descriptor Heaps
**D3D9:** Texture handles are simple  
**D3D12:** Descriptor heaps required

**Solution:**
- Pre-allocate large descriptor heap
- Map jeTexture* to descriptor heap offsets
- Implement descriptor allocation/recycling

## Project Structure

```
source/Drivers/Direct3D12Driver/
??? Direct3D12Driver.h          # Main header
??? Direct3D12Driver.cpp        # Driver implementation
??? D3D12TextureManager.h       # Texture management
??? D3D12TextureManager.cpp
??? D3D12GeometryBatcher.h      # Geometry batching
??? D3D12GeometryBatcher.cpp
??? D3D12DescriptorHeap.h       # Descriptor management
??? D3D12DescriptorHeap.cpp
??? D3D12Common.h               # Shared utilities
??? D3D12Log.h                  # Logging system
??? D3D12Log.cpp
??? Shaders/
?   ??? Gouraud_VS.hlsl
?   ??? Gouraud_PS.hlsl
?   ??? Textured_VS.hlsl
?   ??? Textured_PS.hlsl
?   ??? Lightmap_VS.hlsl
?   ??? Lightmap_PS.hlsl
??? Direct3D12Driver.vcxproj
```

## Dependencies

### Required Libraries
- **d3d12.lib** - DirectX 12 core
- **dxgi.lib** - Display management
- **d3dcompiler.lib** - Shader compilation
- **dxguid.lib** - DirectX GUIDs

### Headers (Windows SDK 10.0+)
- `<d3d12.h>`
- `<dxgi1_6.h>`
- `<d3dcompiler.h>`
- `<DirectXMath.h>` (optional, for math helpers)

### Minimum Requirements
- Windows 10 version 1809 (October 2018 Update)
- DirectX 12 compatible GPU
- Visual Studio 2017+

## Vertex Format Mapping

### Jet3D Vertex Types

**jeTLVertex** (Transformed & Lit):
```cpp
struct jeTLVertex {
    float x, y, z;        // Position
    float rhw;            // Reciprocal homogeneous W
    float r, g, b, a;     // Color
    float u, v;           // Texture coords
    float u2, v2;         // Second texture coords (lightmap)
};
```

**D3D12 Input Layout:**
```cpp
D3D12_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, ... },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, ... },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, ... },
    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 40, ... }
};
```

## Testing Strategy

### Phase 1 Tests
1. Driver loads and initializes
2. Clear screen with solid color
3. Window resizing works

### Phase 2 Tests
1. Load simple texture
2. Display textured quad
3. Texture mipmapping

### Phase 3 Tests
1. Render colored triangle
2. Render textured polygon
3. Matrix transformations

### Phase 4 Tests
1. Multi-texture rendering
2. Alpha blending
3. Fog effects

## Performance Targets

- **Initialization:** < 200ms
- **Frame time:** < 16.6ms (60 FPS)
- **Texture upload:** < 5ms per 1024x1024 texture
- **Draw calls:** Support 5000+ per frame

## Backward Compatibility

### Compatibility Layer
The driver will be binary-compatible with existing Jet3D applications:
- Same DRV_Driver structure layout
- Same calling conventions (DRIVERCC = _fastcall)
- Same error codes
- Same DriverHook signature

### Registration
```cpp
extern "C" DRIVERAPI BOOL DriverHook(DRV_Driver** Driver) {
    *Driver = &g_D3D12Driver;
    return TRUE;
}

extern "C" JETAPI void* JETCC jeEngine_D3D12Driver(void) {
    return (void*)DriverHook;
}
```

## Migration Path for Users

### Option 1: Direct Replacement
1. Build Direct3D12Driver.dll
2. Place in same directory as Direct3D9Driver.dll
3. Engine will enumerate both drivers
4. User selects "DirectX 12 Driver" from list

### Option 2: Fallback Chain
1. Try D3D12 first
2. Fall back to D3D9 if D3D12 unavailable
3. Transparent to end user

## Next Steps

1. **Create skeleton project** with Init/Shutdown only
2. **Test driver enumeration** in existing JStudio
3. **Implement basic rendering** (clear screen)
4. **Add texture support** (single texture)
5. **Complete geometry pipeline**
6. **Add advanced features**

## Resources

- DirectX 12 Programming Guide: https://docs.microsoft.com/en-us/windows/win32/direct3d12/
- DXGI Overview: https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/
- D3D12 Samples: https://github.com/microsoft/DirectX-Graphics-Samples

## Estimated Timeline

- **Phase 1:** 1-2 weeks
- **Phase 2:** 1 week
- **Phase 3:** 2-3 weeks
- **Phase 4:** 2-3 weeks
- **Phase 5:** 1-2 weeks

**Total:** 7-11 weeks for full implementation
