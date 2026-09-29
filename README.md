# Genesis3D: Reborn

**Genesis3D: Reborn** is a modernized, high-performance C/C++ 3D game engine and content-creation suite based on the classic Genesis3D / Jet3D engine architecture, updated with a modern **DirectX 12 hardware rasterizer**, 32-bit/64-bit modern toolset compatibility, enhanced virtual file system, skeletal animation pipeline, and integrated scripting VM.

---

## Key Features

- **Modern DirectX 12 Hardware Renderer (`Direct3D12Driver.dll`)**:
  - Full implementation of the `DRV_Driver` hardware rasterizer interface.
  - Pipeline State Object (PSO) caching for Gouraud shaded, textured, and multi-layer world lightmap geometry.
  - Dynamic polygon batching cache (`D3D12PolyCache`) to minimize draw call overhead and fence synchronization.
  - Staging upload heaps and SRV descriptor heap management for high-density texture workflows.
  - Real-time frame synchronization using D3D12 fences and DXGI swap chain buffer flipping.

- **Core Engine Subsystems (`Jet3DClassic11.dll` / `Genesis3DReborn`)**:
  - **BSP & Portal Visibility**: Binary Space Partitioning with CSG brush operations, leaf clustering, PVS (Potential Visible Set) occlusion culling, and camera frustum clipping.
  - **Skeletal Actor Pipeline**: Bone hierarchies, blend trees, multi-track keyframe motions, vertex skinning deformers, attachment sockets, and `.ACT` / `.BDY` / `.MOT` binary formats.
  - **Lightmap & Lighting Engine**: Surface lightmaps with Gouraud vertex interpolation, ambient occlusion volumes, dynamic light point sources, and pulsing/flickering lights.
  - **Heightfield Terrain Subsystem**: Continuous Level of Detail (CLOD) heightmap rendering with quadtree partitioning, dynamic vertex morphing, and ray-cast ground queries.
  - **High-Performance Particle System**: Billboard quad particles, physics simulation, emitter spouts, and alpha blending.
  - **Virtual File System (VFile)**: Transparent unified file access across physical directories, packed archives, and memory streams.
  - **Audio System**: 3D spatialized DirectSound positioning, MP3 audio stream management, and Ogg Vorbis stream playback.

- **Integrated Developer Tools & Editors**:
  - **Genesis World Editor / Designer (`jwe.exe` / `jDesigner`)**: Full-featured 4-view CAD editor (Top, Front, Side, 3D Textured Perspective) for CSG brush modeling, lighting compilation, texture alignment, entity placement, and BSP generation.
  - **Actor Studio & ActBuild (`AStudio.exe` / `jActBuildClassic11.exe`)**: Skeletal mesh authoring, 3DS Max / BVH import, bone mapping, and actor file compilation.
  - **Actor Workbench (`ActorWorkbench.exe`)**: Interactive actor previewer, motion debugger, and socket inspector.
  - **Eos Script Engine & Compiler (`eosscript`)**: Native bytecode scripting language compiler and VM for level events, entity AI, and gameplay scripting.
  - **Genesis Game Shell (`jGameShellClassic11.exe`)**: Standalone game launcher with DirectX 12 rendering, level loading, camera controllers, and script integration.

---

## Directory Layout

```
Genesis3D-Reborn-Source/
├── bin/                       # Compiled executables, engine DLLs, and object plugins
│   ├── objects/               # Modular entity plugin DLLs (ActorObj, CamObject, etc.)
│   └── Levels/                # Sample levels and map assets
├── docs/                      # Technical documentation and architecture indices
│   ├── CODEBASE_INDEX.md      # Master Codebase Architecture & Symbol Index
│   └── DirectX12_Renderer_Implementation_Plan.md
├── include/                   # Public engine API headers (Genesis3D.h, Engine.h, Actor.h, etc.)
├── lib/                       # Static and import libraries
└── source/
    ├── Drivers/
    │   └── Direct3D12Driver/  # DirectX 12 hardware rasterizer implementation
    ├── Engine/
    │   └── JetEngine/         # Core engine implementation (Actor, BSP, Terrain, Math, etc.)
    ├── Objects/               # Runtime entity DLL plugins (Light, Camera, Model, Terrain, etc.)
    └── Tools/                 # World Editor, Actor Studio, Game Shell, Eos Script, Workbench
```

---

## Building the Project

### Prerequisites
- **Visual Studio 2022 / 2026** (or MSBuild) with C++ Desktop Development workload.
- **Windows 10 / 11 SDK** (includes DirectX 12 headers and `d3d12.lib`, `dxgi.lib`).
- *(Optional)* **MFC Libraries** for GUI tools (`jDesigner`, `AStudio`, `ActorWorkbench`, `jMinApp`).

### Build Steps
1. Open `Genesis3DReborn.sln` in Visual Studio or use MSBuild via the command line:
   ```cmd
   MSBuild Genesis3DReborn.sln /p:Configuration=Release /p:Platform=Win32 /m
   ```
2. The compilation artifacts will be output directly into the root `bin/` directory.
3. Run the Game Shell or World Editor directly from `bin/` to ensure `Direct3D12Driver.dll` and object DLLs are resolved.

---

## Documentation Index

For an exhaustive, file-by-file catalog of every module, class, data structure, and implementation detail across the entire codebase, consult:
- **[Codebase Master Index & Architecture Reference](file:///C:/Users/Administrator/source/repos/toxicclowd/Genesis3D-Reborn-Source/docs/CODEBASE_INDEX.md)**
