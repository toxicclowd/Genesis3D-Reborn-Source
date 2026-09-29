# DirectX 12 Driver - Quick Reference

## Build Commands
```bash
# From Visual Studio
Build ? Build Direct3D12Driver

# From command line
msbuild Direct3D12Driver.vcxproj /p:Configuration=Debug
msbuild Direct3D12Driver.vcxproj /p:Configuration=Release
```

## Test Commands
```powershell
# Verify build
.\VerifyBuild.ps1

# Run test app
cd ..\..\..\bin
.\jMinApp.exe

# View log
notepad Direct3D12Driver.log
```

## File Locations
```
Source:  source/Drivers/Direct3D12Driver/
Output:  bin/Direct3D12Driver.dll
Log:     bin/Direct3D12Driver.log
Include: include/ (from solution)
```

## Key Functions (Phase 1)

### Initialization
| Function | Status | Description |
|----------|--------|-------------|
| `D3D12Drv_Init` | ? | Initialize D3D12 device, swap chain, etc. |
| `D3D12Drv_Shutdown` | ? | Clean up all resources |
| `D3D12Drv_Reset` | ? | Reset device state |
| `D3D12Drv_SetActive` | ? | Activate/deactivate rendering |

### Enumeration
| Function | Status | Description |
|----------|--------|-------------|
| `D3D12Drv_EnumSubDrivers` | ? | Report driver name |
| `D3D12Drv_EnumModes` | ? | Enumerate display modes |
| `D3D12Drv_EnumPixelFormats` | ? | Report supported formats |

### Rendering
| Function | Status | Description |
|----------|--------|-------------|
| `D3D12Drv_BeginScene` | ? | Start frame, clear screen |
| `D3D12Drv_EndScene` | ? | End frame, present |
| `D3D12Drv_BeginBatch` | ? | Begin geometry batch |
| `D3D12Drv_EndBatch` | ? | End geometry batch |

### Textures (Stub)
| Function | Status | Description |
|----------|--------|-------------|
| `D3D12_THandle_Create` | ?? | Create texture (Phase 2) |
| `D3D12_THandle_Lock` | ?? | Lock texture for update |
| `D3D12_THandle_Unlock` | ?? | Unlock texture |
| `D3D12_THandle_Destroy` | ?? | Destroy texture |

### Geometry (Stub)
| Function | Status | Description |
|----------|--------|-------------|
| `D3D12Drv_RenderGouraudPoly` | ?? | Render colored poly (Phase 3) |
| `D3D12Drv_RenderWorldPoly` | ?? | Render world geometry |
| `D3D12Drv_RenderMiscTexturePoly` | ?? | Render textured poly |

## Code Snippets

### Matrix Conversion
```cpp
// Jet3D XForm ? D3D12 Matrix
D3D12Matrix mat;
jeXForm3d xform;
jeXForm3d_ToD3D12Matrix(&xform, &mat);

// D3D12 Matrix ? Jet3D XForm
D3D12Matrix_ToXForm3d(&mat, &xform);
```

### Logging
```cpp
// Simple log
D3D12Log::GetPtr()->Printf("Message");

// Formatted log
D3D12Log::GetPtr()->Printf("Value: %d", value);

// Destroy log (on shutdown)
D3D12Log::Destroy();
```

### Resource Transition
```cpp
// Transition to render target
TransitionResource(
    g_pCommandList,
    resource,
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_RENDER_TARGET
);
```

## Common Issues

### Issue: Build Error - Cannot find DCommon.h
**Solution:** Check include path  
```xml
<AdditionalIncludeDirectories>
  ..\..\Engine\JetEngine\Engine\Drivers;
  ..\..\..\include;
</AdditionalIncludeDirectories>
```

### Issue: Driver not in list
**Solution:**  
1. Check DLL in `bin` folder
2. Verify `DriverHook` export
3. Review log file

### Issue: Crash on Init
**Solution:**  
1. Enable D3D12 debug layer
2. Check GPU supports D3D12
3. Review `Direct3D12Driver.log`

### Issue: Black screen
**Solution:**  
1. Check `BeginScene` called
2. Verify clear color set
3. Check Present() succeeds

## Debug Tips

### Enable Debug Layer
```cpp
// In Direct3D12Driver.cpp
#define ENABLE_D3D12_DEBUG_LAYER
```

### View D3D12 Messages
- Run from Visual Studio debugger
- Check Output window
- Look for D3D12 debug messages

### Check Resource Leaks
```cpp
// On shutdown, check for unreleased objects
// D3D12 debug layer will report leaks
```

## Performance Tips (Future)

Phase 1 focuses on correctness. Future optimizations:
- PSO caching (Phase 3)
- Resource pooling (Phase 5)
- Async uploads (Phase 5)
- Command list bundles (Phase 5)

## Constants

```cpp
#define FRAME_COUNT 3                  // Triple buffering
#define MAX_TEXTURES 4096              // Max texture slots
#define MAX_SRV_DESCRIPTORS 4096       // SRV heap size
#define MAX_RTV_DESCRIPTORS 16         // RTV heap size
```

## Globals

```cpp
extern HWND g_hWnd;                    // Render window
extern ComPtr<ID3D12Device> g_pDevice; // D3D12 device
extern ComPtr<ID3D12CommandQueue> g_pCommandQueue;
extern ComPtr<IDXGISwapChain3> g_pSwapChain;
extern float g_fGamma;                 // Gamma value
```

## Entry Points

```cpp
// Driver registration
extern "C" DRIVERAPI BOOL DriverHook(DRV_Driver** Driver);

// Engine interface
extern "C" DRIVERAPI void* jeEngine_D3D12Driver(void);
```

## Next Steps

1. ? Phase 1 Complete - Basic infrastructure
2. ?? Phase 2 Next - Texture support
3. ? Phase 3 - Geometry rendering
4. ? Phase 4 - Advanced features
5. ? Phase 5 - Optimization

---

**Version:** 1.0 (Phase 1)  
**Last Updated:** 2024  
**Status:** Phase 1 Complete ?
