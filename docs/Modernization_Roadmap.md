# Genesis3D: Reborn — Modernization Roadmap

This roadmap covers the next stage of the engine and tools now that everything builds and runs on the DirectX 12 driver. The goals are:

- PBR materials
- Advanced lighting and global illumination
- Lua scripting
- Modern editor navigation
- Other modern features

All of this has to happen **without losing what makes Genesis3D feel like Genesis3D**: brush/CSG level building, BSP + PVS worlds, compiled lighting and a 4-view CAD editor.

---

## 1. Ground rules for keeping the retro feel

1. **Brushes stay the way levels are built.** New features add to CSG + BSP; they don't replace it. Meshes stay props, and brushes stay the world.
2. **Lighting is still compiled.** Baked lightmaps are the "retro-authentic" GI. The main GI path upgrades the bake (bounces, HDR, direction) instead of switching to fully dynamic GI. Runtime GI is an optional extra tier.
3. **Existing content keeps loading, and old levels look the same by default.** Every file-format change is a new chunk version. A material with no PBR maps renders like today's diffuse × lightmap.
4. **A "Look Profile" switch is built in from day one.** `Classic` (point filtering, LDR lightmaps, no post-processing), `Enhanced` (PBR, HDR, shadows, baked GI) and `Stylized` (Enhanced lighting plus texel-snapped lighting, palette/dither, low internal resolution, optional vertex snapping). The same level works in all three.
5. **Old editor habits keep working.** New navigation ships as the default keymap, and the current controls remain as a `Classic Genesis` preset.

---

## 2. The key architectural fact: the world is drawn in screen space

Everything below depends on this.

Today the engine transforms and clips world polygons **on the CPU** and sends them to the driver as `grTLVertex` (`Engine/Include/grTypes.h`): screen-space `x, y, z`, a pre-lit color, one UV set and a specular color. The world path is `RENDER_W_POLY` (`Engine/Genesis3D/Engine/Drivers/Dcommon.h`), which the D3D12 driver batches in `D3D12PolyCache`. Its shaders (`Renderer/Direct3D12/Shaders/TLPoly.hlsl`) just compute `texture × lightmap × vertex color`.

Screen-space vertices have **no world position, normal or tangent**, so per-pixel PBR, shadow maps, light probes and reflections **can't be built on this path**.

The good news is that a starting point already exists: the "Hardware T&L" and "Static Mesh" additions (`SET_MATRIX`, `SET_CAMERA`, `ADD_STATIC_MESH` / `RENDER_STATIC_MESH` with `grHWVertex { Pos, Normal, Diffuse, u, v, lu, lv }`) already give the driver world-space geometry plus view and projection matrices. **Phase 1 extends that path to BSP world geometry.**

---

## 3. Phases

The phases form a dependency chain (0 → 1 → 2 → 3 → 4). **Tracks A (editor) and B (Lua) are independent** and can run in parallel with the renderer work.

### Phase 0 — Renderer foundations

| Item | Detail |
|---|---|
| Shader build | Move the HLSL out of the string literal in `D3D12PSOManager.cpp` into real `.hlsl` files. Compile them with DXC (SM 6.x) as a build step, with runtime compile kept as a dev fallback. |
| Frame structure | A small "frame graph": depth buffer created as a typeless resource (so it can be sampled), an HDR scene target (`R16G16B16A16_FLOAT`), a final tonemap/present pass, and per-frame/per-view constant buffers (replacing the single 32-bit-constant root parameter). |
| Bindless-lite | A single large shader-visible SRV heap indexed by material, replacing per-draw descriptor tables. PBR needs this because it adds 4–5 textures per material. |
| Debugging | PIX markers, the D3D12 debug layer toggle in `.ini`, and GPU timestamp queries shown in the Game Shell overlay. |
| Regression safety | A `-screenshot` mode in GameShell that loads a level, sets a fixed camera and saves a frame, plus a small set of reference levels so every later phase can be compared image-to-image. |

**Done when:** the current levels render identically through the HDR target plus a pass-through tonemap.

### Phase 1 — World-space GPU world path

- **At level load:** turn every BSP draw face into a GPU vertex format like `{ pos, normal, tangent(sign), uv, lightmapUV }`. Pack lightmaps into atlases instead of uploading them per face through the `SETUP_LIGHTMAP_CB` callback. Upload static vertex/index buffers grouped **per leaf × material**.
- **Each frame:** the engine still does PVS and frustum culling per leaf in `grBSP` (this is the BSP feel, and it's still a big culling win). Instead of emitting TL polygons, it builds a list of `(leaf, material) → index range` and hands it to the driver in one call.
- **Tangents come for free:** brush faces already have texture vectors (`grTexVec`), so the tangent basis is exact and needs no mesh-processing library.
- **Driver interface:** add new entry points at the **end** of `DRV_Driver`, for example `WORLD_UPLOAD`, `WORLD_RENDER_LIST` and `SET_VIEW_CONSTANTS`. Bump `DRV_VERSION_MINOR` to 5. Keep `RENDER_G_POLY` / `RENDER_MT_POLY` / `DRAW_DECAL` for UI, 2D, particles, editor wireframe and the Classic fallback.
- **Dynamic content:** actors and models move to the same world-space path (skinned on the CPU at first, then in a compute shader) so they receive the same lighting as the world.
- **Mirrors, portals and alpha faces:** portals render as a second view through the same list API. Alpha faces stay sorted back-to-front, as `DRV_PREFERENCE_DRAW_WALPHA_IN_BSP` does today.

**Done when:** every reference level matches Phase 0 screenshots (within tolerance) using the new path, with fewer draw calls and no CPU-side polygon transforms.

### Phase 2 — PBR materials

- **Material model:** metallic/roughness GGX (Cook-Torrance), Lambert diffuse, and energy-conserving Fresnel (Schlick).
- **Data:** `grMaterialSpec` already has typed layers (`LAYER_TYPE_BASE / LIGHTMAP / BUMPMAP`). Add these layer types:
  - `NORMAL` (BC5)
  - `ORM` (occlusion/roughness/metal packed, BC7)
  - `EMISSIVE`
  - `HEIGHT` (optional parallax)

  Bump `GR_MATSPEC_VERSION`. A missing layer falls back to a constant value (roughness 1, metal 0, flat normal), so **every existing `.mat` renders as it does today**.
- **Scalar parameters:** base color tint, roughness/metal scale, emissive intensity, alpha mode (opaque / cutout / blend), two-sided, and a **"Retro" flag** (point sampling and texel-snapped shading for that material).
- **Texture pipeline:**
  - Import PNG/TGA/DDS.
  - Compress to BC1/3/5/7 with mipmaps at import time in the tools (DirectXTex), not at load.
  - Handle sRGB correctly (base color/emissive are sRGB; normal/ORM are linear).
  - The legacy 8-bit palettized bitmaps keep working through the existing `Bitmap` code.
- **Tools:**
  - A Material Editor panel in the World Editor: one slot per channel, drag-and-drop textures, a live preview sphere/cube/brush, and a "generate ORM from grayscale" helper.
  - "Auto-find maps" by suffix (`_n`, `_orm`, `_e`) so texture packs import in one click.

**Done when:** a PBR sample level shows correct metals, rough/smooth surfaces and normal detail, and the original levels are unchanged.

### Phase 3 — Advanced lighting

- **Technique: clustered forward.** Forward+ fits best here: the BSP world is already sorted and drawn forward, alpha faces and portals work naturally, MSAA remains an option, and it's cheaper than deferred on low-end GPUs. A compute pass bins lights into a froxel grid, and the PBR shader loops over each cluster's lights.
- **Light types:** point, spot and directional (sun), plus area/tube lights as a later extra. Existing light entities (including pulsing/flicker styles from `PulsingLightObj` and `DynLightObj`) map onto these. The flicker "style strings" stay because they are part of the retro look.
- **Shadows:**
  - Cascaded shadow maps for the sun, and cube or dual-paraboloid maps for point lights, all in a shadow atlas with a budget.
  - Each light has a per-light "cast shadows" flag.
  - Static lights stay **baked** (see Phase 4). Only lights marked *dynamic* get real-time shadows. This is the Quake/Half-Life model: baked world, dynamic accents.
- **HDR and post-processing:** tonemapping (AgX or ACES, selectable), auto-exposure with manual override per level, bloom, height/volume fog (extending the existing `SET_FOG`), and optional SSAO. Every post effect is off in the `Classic` profile.
- **Stylized pass (optional):** palette quantization plus ordered dither, lighting snapped to lightmap texel size, a render-scale slider (for example 320×240 upscaled with nearest-neighbour), CRT/scanline filter.

### Phase 4 — Global illumination (tiered)

The main GI is a **better baked lightmap compiler**. It stays true to BSP-era workflows ("build lighting" in the editor) while producing modern-quality results.

| Tier | Technique | Notes |
|---|---|---|
| **A. Baked** (default) | Multi-bounce path-traced lightmap baker | Replaces the current direct-only CPU pass (`grBSPNode_LightmapCalcLight` in `Engine/Genesis3D/Bsp/grBSPNode_Light.cpp`). Uses a CPU BVH (multithreaded, works on any machine) and a DXR back-end when available. Includes bounce light, sky/sun, emissive surfaces as lights, and AO. |
| | HDR + directional lightmaps | Stored as `RGB9E5`/`BC6H` plus a dominant-direction or SH-L1 map, so normal maps respond to baked light. New lightmap chunk version; old LDR lightmaps still load. |
| | Light probes | An SH-L2 probe grid baked into the BSP, with **BSP leaves as natural probe cells**. Actors, models and particles sample it, so moving objects sit in the baked GI. |
| | Reflection probes | Cubemap probe entities placed in the editor (or automatically one per area) with box projection. They provide PBR specular reflections. |
| **B. Screen-space** (optional) | SSR, SSAO/GTAO, and optionally SSGI | Cheap supplements, off in Classic. |
| **C. Real-time** (optional, high-end) | DDGI-style probe volumes via DXR 1.1 ray queries | For levels with fully dynamic lighting (day/night). Uses the same probe data layout as Tier A, so shaders see one GI interface. |

**Editor integration:**
- `Build Lighting` gets `Preview` / `Final` quality presets.
- The bake runs in a background process with progress, and the result shows in the 3D viewport without a restart.
- Per-brush-face lightmap scale gives cheap walls and detailed hero areas.

### Track A — Modern editor navigation and UX (parallel)

Current state: the World Editor's 3D view (`Tools/WorldEditor/G3DView.cpp`) uses LMB-drag to rotate/move in and out, RMB-drag to look and LMB+RMB to pan. WASD exists only in the separate full-screen preview loop. The 2D views (`View.cpp`) use Space+drag to pan.

**Navigation (new default keymap)**

| Action | Binding |
|---|---|
| Fly mode | **Hold RMB** + WASD / Q E (down/up); mouse look; Shift = fast; scroll while flying = change speed |
| Orbit | **Alt + LMB** around the selection or the point under the cursor |
| Pan | **MMB drag** (3D and 2D views) |
| Dolly / zoom | Scroll = dolly in 3D and **zoom to cursor** in 2D |
| Frame selection | **F**; **Shift+F** frames all |
| Views | Numpad view snapping; maximize a view with a key; the camera stays in sync between views when you ask for it |

- The keymap lives in `.ini` with a `Classic Genesis` preset that reproduces today's behavior exactly.
- Movement is time-based (it runs on a timer, not in mouse-move steps) and smoothly accelerates.

**Editing modernization**
- Translate/rotate/scale gizmos in the 3D view, with grid and angle snapping.
- A clipping tool, vertex/edge/face editing, texture lock, and face alignment (fit / align to neighbour / justify).
- A real-time lit 3D viewport: the editor uses the Phase 1–3 renderer with an "unlit / lightmaps / full" toggle.
- A searchable entity and texture browser, a property grid with tooltips from the object definitions, and a Lua console dock (Track B).
- Undo/redo covering every operation (extending `Core/Undo.c`).
- A high-DPI aware MFC shell and a dark theme.

**Done when:** a new user who knows Hammer++, TrenchBroom or Unreal can get around without reading docs, and a veteran can switch to `Classic Genesis`.

### Track B — Lua scripting (parallel)

- **Runtime:** Lua 5.4 (vendored source under `Engine/External/lua`, built as a static library), bound with **sol2** (header-only C++17).
- **Architecture:** add a small `IScriptVM` interface behind `Tools/GameShell/ScriptMgr.cpp` with two back-ends, **Eos (existing)** and **Lua (new)**. `G3DMain.eos` keeps working during the transition. New samples and docs target Lua.
- **Move the script host into the engine:** a new `grScript` module in `Genesis3D.dll` rather than per tool, so the Game Shell, the editor and any game share it.
- **API surface (v1):** `engine`, `world` (trace/ray, find entities, spawn), `entity` (position, properties, targets), `actor` (motions, sockets), `camera`, `light`, `sound`, `input`, `timer`, `log`, `ui` (text/decals).
- **Entity-attached scripts in classic style:** any entity can have a `script` property pointing to a `.lua` file with hooks `OnSpawn`, `OnThink(dt)`, `OnTouch(other)`, `OnTrigger(activator)`, `OnUse(user)` and `OnDamage`. The `target`/`targetname` wiring from BSP-era games stays the main way to connect a level; Lua adds logic on top.
- **Workflow:**
  - Hot-reload on file change.
  - Scripts are loaded through `VFile` so they work from packs.
  - The sandbox has no `os`/`io` by default.
  - Per-script CPU budget and clear error reporting with file:line in the Game Shell log and editor console.
- **Tests:** a headless Lua test harness that runs scripts against a loaded level with no window.

### Other modern nice-to-haves (backlog, roughly by value)

1. **glTF 2.0 import** for static meshes and skinned actors (via cgltf). It replaces the unbuilt 3ds Max exporter in `legacy/` as the main art path, and glTF PBR materials map directly onto Phase 2.
2. **Audio:** XAudio2 in place of DirectSound; keep MP3/Ogg streaming; HRTF optional.
3. **Input:** XInput/GameInput controller support and rebindable actions.
4. **Packages:** a zip-based `.pk3`-style package mounted by `VFile`, alongside the existing formats.
5. **Physics:** replace the collision pieces with **Jolt Physics** for rigid bodies and character controllers, keeping BSP hulls for world collision (ODE is in `Engine/External` but ageing).
6. **Anti-aliasing:** MSAA, FXAA or TAA, selectable (TAA off by default to keep crisp pixels).
7. **Build and CI:** GitHub Actions on `windows-latest` building the solution in Debug/Release, plus the Phase 0 screenshot regression.
8. **Tooling:** asset hot-reload for textures, materials and levels in the Game Shell; an in-game Dear ImGui debug overlay (engine side only; the editor stays MFC).

---

## 4. Suggested order and milestones

```
M1  Phase 0 (foundations)            ─┐
M2  Phase 1 (world-space GPU path)    │   Track A: editor navigation  ──  can start now
M3  Phase 2 (PBR materials + editor)  │   Track B: Lua host + API v1  ──  can start now
M4  Phase 3 (clustered lights, HDR, shadows, Look Profiles)
M5  Phase 4A (new lightmap baker, probes, reflection probes)
M6  Phase 4B/C + backlog
```

**Recommended first step:** Phase 0 together with Track A's fly-camera. Phase 0 is needed by every rendering feature and is low risk to existing content. The fly-camera is a small, self-contained change to `G3DView.cpp` that improves the editor right away.

---

## 4a. Progress

**Track A — navigation (first step done).** The World Editor 3D view has RMB fly (WASD, Q/E, Shift, wheel speed), Alt+LMB orbit, MMB pan, wheel dolly and F / Shift+F framing, with a time-based, eased fly camera. "Classic Genesis Navigation" in the 3D view's context menu restores the original controls; the choice is stored in the app profile (`Navigation\Keymap`) rather than a `.ini`, like the editor's other settings. Still to do: numpad view snapping, maximize-view key, camera sync between views, gizmos and the rest of the editing list.

**Phase 0 — done.**
- *Shader build:* HLSL lives in `Renderer/Direct3D12/Shaders/`. The `G3DCompileShaders` target compiles each entry point with the Windows SDK's DXC (SM 6.0) into the driver. The source is also embedded and compiled at run time (SM 5.0) when the device lacks SM 6.0, when `[Shaders] RuntimeCompile=1`, or from loose files via `[Shaders] SourceDir` for iterating on shaders.
- *Frame structure:* the scene renders into an `R16G16B16A16_FLOAT` target, and `Shaders/Present.hlsl` copies it to the back buffer (pass-through for now; tonemapping and Look Profiles go there in Phase 3). Depth is `R32_TYPELESS` so later passes can sample it. `[Render] SceneTarget=0` draws straight to the back buffer. A per-frame upload ring (`D3D12UploadRing`) holds the per-frame constant buffer (`FrameConstants`, b1: viewport size, frame number, time) and the poly cache's vertices, replacing a committed upload buffer per flush. Per-draw data is four root constants (flags and texture indices). Per-view constants will use the same ring in Phase 1, which is the first path with a view to describe.
- *Debugging:* `bin/Direct3D12Driver.ini` controls the debug layer, GPU-based validation and WARP; any key can be overridden with `G3D_D3D12_<KEY>`. Debug-layer messages are written to `Direct3D12Driver.log`, and the command list carries PIX markers for the scene, each poly-cache flush and the present pass. Timestamp queries measure the scene and present passes; the times reach the engine through `DRV_Driver::GPUTimings` (driver interface version 5) and show as a `GPU` line in the debug overlay. The overlay itself was blank on D3D12: the engine sent all debug text to the driver's `DrawText`, which was a stub. The engine now draws its built-in bitmap font through `DrawDecal` when a driver has no `DrawText`.
- *Regression safety:* `G3DGameShell -screenshot` plus `tests/render/RenderRegression.ps1` (reference shots in `tests/render/shots.txt`, baseline recorded locally with `-Update`). Repeat runs are pixel-identical, and WARP matches the hardware image on the reference shots. The HDR path differs from direct rendering by at most 1/255 per channel (fp16 storage and unquantized blending), within the default tolerance of 2.
- *Bindless-lite:* on resource binding tier 2+ hardware the pixel shaders index the texture heap directly (`Textures[]`, t0 space1) with the texture index from the draw constants, so draws no longer switch descriptor tables. Tier 1 hardware, or `[Render] Bindless=0`, keeps per-draw tables. The heap holds one slot per texture handle plus reserved driver slots (the scene target's SRV lives there, so the frame uses a single heap), every slot always holds a valid view, and a destroyed texture keeps its resource and slot until the GPU fence passes. The old code released the resource immediately, even if a queued frame still used it. Both paths match the baseline, and GPU-based validation reports nothing.

## 5. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Breaking old `.j3d` / `.mat` content | Versioned chunks only, fallbacks for every new field, and a reference-level screenshot regression. |
| `DRV_Driver` ABI change | Append-only function pointers plus a version bump. The engine checks the version before calling the new entry points. |
| Losing the retro look | Look Profiles and a per-material `Retro` flag. Classic is always one switch away and is tested in CI. |
| Scope creep in GI | Tier A (baked) is the product, and Tiers B/C are optional extras behind capability checks. |
| No Windows build or test in cloud sessions | Changes are validated locally in Visual Studio. The CI workflow (backlog item 7) closes this gap. |
