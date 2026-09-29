# Genesis3D: Reborn

**Genesis3D: Reborn** is a modernized C/C++ 3D game engine and content-creation suite based on the classic Genesis3D / Jet3D engine architecture, updated with a **DirectX 12 hardware rasterizer**, a modern Visual Studio toolset, an enhanced virtual file system, a skeletal animation pipeline, and an integrated scripting VM.

---

## Key Features

- **DirectX 12 Hardware Renderer (`Direct3D12Driver.dll`)**:
  - Full implementation of the `DRV_Driver` hardware rasterizer interface.
  - Pipeline State Object (PSO) caching for Gouraud shaded, textured, and multi-layer world lightmap geometry.
  - Dynamic polygon batching cache (`D3D12PolyCache`) to minimize draw call overhead and fence synchronization.
  - Staging upload heaps and SRV descriptor heap management for high-density texture workflows.
  - Real-time frame synchronization using D3D12 fences and DXGI swap chain buffer flipping.

- **Core Engine (`Genesis3D.dll`)**:
  - **BSP & Portal Visibility**: Binary Space Partitioning with CSG brush operations, leaf clustering, PVS (Potential Visible Set) occlusion culling, and camera frustum clipping.
  - **Skeletal Actor Pipeline**: Bone hierarchies, blend trees, multi-track keyframe motions, vertex skinning deformers, attachment sockets, and `.ACT` / `.BDY` / `.MOT` binary formats.
  - **Lightmap & Lighting Engine**: Surface lightmaps with Gouraud vertex interpolation, dynamic light point sources, and pulsing/flickering lights.
  - **Heightfield Terrain Subsystem**: Continuous Level of Detail (CLOD) heightmap rendering with quadtree partitioning, dynamic vertex morphing, and ray-cast ground queries.
  - **Particle System**: Billboard quad particles, physics simulation, emitter spouts, and alpha blending.
  - **Virtual File System (VFile)**: Transparent unified file access across physical directories, packed archives, and memory streams.
  - **Audio System**: 3D spatialized DirectSound positioning, MP3 audio stream management, and Ogg Vorbis stream playback.

- **Developer Tools & Editors**:
  - **World Editor (`G3DWorldEditor.exe`)**: 4-view CAD editor (Top, Front, Side, 3D Textured Perspective) for CSG brush modeling, lighting compilation, texture alignment, entity placement, and BSP generation.
  - **Actor Studio & ActBuild (`G3DActorStudio.exe` / `G3DActBuild.exe`)**: Skeletal mesh authoring, 3DS Max / BVH import, bone mapping, and actor file compilation.
  - **Actor Workbench (`G3DActorWorkbench.exe`)**: Interactive actor previewer, motion debugger, and socket inspector.
  - **Game Shell (`G3DGameShell.exe`)**: Standalone game launcher with DirectX 12 rendering, level loading, camera controllers, and Eos script integration (`bin/Scripts/G3DMain.eos`).
  - **MinApp (`G3DMinApp.exe`)**: Minimal MFC sample application hosting the engine.
  - **Eos Script (`EosScript`)**: Bytecode scripting language compiler and VM used by the Game Shell.

---

## Directory Layout

```
Genesis3D-Reborn-Source/
├── Genesis3DReborn.sln        # Solution: Engine / Renderer / Objects / Tools folders
├── Directory.Build.props      # Shared build settings; defines $(G3DRoot) for every project
├── Engine/
│   ├── Genesis3D/             # Core engine DLL (Actor, BSP, Bitmap, Terrain, VFile, ...)
│   ├── Include/               # Public engine API headers (Genesis3D.h, Engine.h, gr*.h)
│   └── External/              # Third-party SDKs (DirectX 8.1 headers, EAX, ODE, Vorbis)
├── Renderer/
│   └── Direct3D12/            # DirectX 12 hardware rasterizer (Direct3D12Driver.dll)
├── Objects/                   # Runtime entity plugins, one project per DLL
│   ├── ActorObj/  AmbientObj/  BoxObj/  CameraObj/  CoronaObj/  DynLightObj/
│   └── ModelObj/  PathObj/  PortalObj/  PulsingLightObj/  SpoutObj/  StaticMeshObj/  TerrainObj/
├── Tools/
│   ├── WorldEditor/           # G3DWorldEditor (MFC)
│   ├── ActorTools/            # G3DActorStudio (MFC) and G3DActBuild (console)
│   ├── ActorWorkbench/        # G3DActorWorkbench (MFC)
│   ├── GameShell/             # G3DGameShell
│   ├── MinApp/                # G3DMinApp (MFC)
│   └── EosScript/             # Eos script compiler/VM static library
├── bin/                       # Runtime folder: build output plus Levels, Actors, GlobalMaterials, Scripts
├── lib/                       # Import libraries produced by the build (not tracked)
├── docs/                      # Architecture index and renderer design notes
└── legacy/                    # Code that is not built: old VC6/VS2003 project files, the
                               # 3ds Max exporter, unused engine modules, DirectX 8 runtime
```

Every project that lives in the solution uses the DirectX 12 renderer: the engine loads only `Direct3D12Driver.dll`, and the tools select the `(D3D) DirectX 12` driver.

### Naming

The Jet3D names have been replaced throughout the code:

| Old | New |
|-----|-----|
| `jeEngine_Create`, `jeBoolean`, `JE_TRUE` | `grEngine_Create`, `grBoolean`, `GR_TRUE` |
| `JETAPI` / `JETCC` | `GRAPI` / `GRCC` |
| `Jet3DClassic11.dll` | `Genesis3D.dll` (`Genesis3Dd.dll` in Debug) |
| `jDesignerClassic11.exe`, `jAStudioClassic11.exe`, `jActBuildClassic11.exe` | `G3DWorldEditor.exe`, `G3DActorStudio.exe`, `G3DActBuild.exe` |
| `jActorWorkbenchClassic11.exe`, `jGameShellClassic11.exe`, `jMinAppClassic11.exe` | `G3DActorWorkbench.exe`, `G3DGameShell.exe`, `G3DMinApp.exe` |

On-disk formats are unchanged so existing content keeps loading: `.j3d` levels (including their internal `Jet3D` directory name), the `jet3d` default material in `bin/GlobalMaterials`, `.jetpak` static meshes and the binary chunk tags.

---

## Building the Project

### Prerequisites
- **Visual Studio 2022 / 2026** (or MSBuild) with the *Desktop development with C++* workload.
- **Windows 10 / 11 SDK** (DirectX 12 headers and `d3d12.lib`, `dxgi.lib`).
- **C++ MFC for latest build tools (x86 & x64)** — required for the MFC tools
  (World Editor, Actor Studio, Actor Workbench, MinApp). Without it those four projects fail with `MSB8041`
  and everything else still builds.

### Build Steps
1. Open `Genesis3DReborn.sln` in Visual Studio, or build from the command line:
   ```cmd
   MSBuild Genesis3DReborn.sln /p:Configuration=Release /p:Platform=Win32 /m
   ```
   The projects are 32-bit (`Win32`) only.
2. Executables, `Genesis3D.dll` and `Direct3D12Driver.dll` are written to `bin/`, object plugins to `bin/objects/`
   (`.dll` in Release, `.ddl` in Debug), and import libraries to `lib/`.
3. Run the Game Shell or World Editor from `bin/` so the renderer and object plugins are found.

---

## Documentation Index

- **[Codebase Master Index & Architecture Reference](docs/CODEBASE_INDEX.md)**
- **[DirectX 12 Renderer Implementation Plan](docs/DirectX12_Renderer_Implementation_Plan.md)**
