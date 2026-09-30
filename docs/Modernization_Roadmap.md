# Genesis3D: Reborn — Roadmap

Genesis3D: Reborn is growing from an engine with separate tools into a **game authoring system**: one application, the World Editor, in which a game is made from start to finish and then built into a package that players run. The goals are:

- **Engine:** PBR materials, advanced lighting and global illumination, and other modern features (sections 2–3).
- **Authoring (section 4):**
  - projects and templates,
  - the World Editor as the hub for every tool: levels, actors, materials, scripts, UI and game settings,
  - Lua scripting, and visual scripting that compiles to Lua,
  - a menu and screen editor (main menu, loading, pause, options, HUD...),
  - a game framework, so a game needs scripts and data but no C++,
  - play-in-editor, and building and packaging the finished game from the editor.
- **Editor UX:** modern navigation and editing (Track A).

All of this has to happen **without losing what makes Genesis3D feel like Genesis3D**: brush/CSG level building, BSP + PVS worlds, compiled lighting and a 4-view CAD editor.

---

## 1. Ground rules

### Keeping the retro feel

1. **Brushes stay the way levels are built.** New features add to CSG + BSP; they don't replace it. Meshes stay props, and brushes stay the world.
2. **Lighting is still compiled.** Baked lightmaps are the "retro-authentic" GI. The main GI path upgrades the bake (bounces, HDR, direction) instead of switching to fully dynamic GI. Runtime GI is an optional extra tier.
3. **Existing content keeps loading, and old levels look the same by default.** Every file-format change is a new chunk version. A material with no PBR maps renders like today's diffuse × lightmap.
4. **A "Look Profile" switch is built in from day one.** `Classic` (point filtering, LDR lightmaps, no post-processing), `Enhanced` (PBR, HDR, shadows, baked GI) and `Stylized` (Enhanced lighting plus texel-snapped lighting, palette/dither, low internal resolution, optional vertex snapping). The same level works in all three.
5. **Old editor habits keep working.** New navigation ships as the default keymap, and the current controls remain as a `Classic Genesis` preset.

### Authoring system

6. **The World Editor is the hub.** Every tool (actor editor, material editor, script and visual script editors, UI editor, project settings, build) opens from it and works on the open project. Stand-alone tools may remain, but only as command-line back ends the editor drives (as it already drives G3DTexImport).
7. **What you play in the editor is what ships.** Play-in-editor, the packaged game and the Game Shell run the same runtime and the same scripts; nothing is emulated in the editor. Editor previews (UI canvas, actor viewer, material preview) render through the real engine.
8. **Games are content, scripts and settings.** A complete game can be made without writing or compiling C++. C++ remains possible through the existing object DLL plug-ins (`Objects/`), and Lua can define entity classes too.
9. **Project files are text where they can be,** and diff-friendly: the project file, UI screens, visual script graphs, input maps and settings. Levels and binary assets keep their chunked formats, under the versioning rule above.
10. **Templates are real projects.** Each template is an ordinary project that ships with the editor and is copied on *New Project*, so templates are built and tested with the same tools as games.

---

## 2. The key architectural fact: the world is drawn in screen space

Everything below depends on this.

Today the engine transforms and clips world polygons **on the CPU** and sends them to the driver as `grTLVertex` (`Engine/Include/grTypes.h`): screen-space `x, y, z`, a pre-lit color, one UV set and a specular color. The world path is `RENDER_W_POLY` (`Engine/Genesis3D/Engine/Drivers/Dcommon.h`), which the D3D12 driver batches in `D3D12PolyCache`. Its shaders (`Renderer/Direct3D12/Shaders/TLPoly.hlsl`) just compute `texture × lightmap × vertex color`.

Screen-space vertices have **no world position, normal or tangent**, so per-pixel PBR, shadow maps, light probes and reflections **can't be built on this path**.

The good news is that a starting point already exists: the "Hardware T&L" and "Static Mesh" additions (`SET_MATRIX`, `SET_CAMERA`, `ADD_STATIC_MESH` / `RENDER_STATIC_MESH` with `grHWVertex { Pos, Normal, Diffuse, u, v, lu, lv }`) already give the driver world-space geometry plus view and projection matrices. **Phase 1 extends that path to BSP world geometry.**

---

## 3. Phases

The engine phases form a dependency chain (0 → 1 → 2 → 3 → 4). **Track A (editor UX)** runs in parallel with them and with the authoring system (§4), which builds on the engine but not on Phase 4.

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
- A searchable entity and texture browser, a property grid with tooltips from the object definitions, and a Lua console dock (these become panels of the editor hub, §4.2).
- Undo/redo covering every operation (extending `Core/Undo.c`).
- A high-DPI aware MFC shell and a dark theme.

**Done when:** a new user who knows Hammer++, TrenchBroom or Unreal can get around without reading docs, and a veteran can switch to `Classic Genesis`.

### Track B — Lua scripting

Now part of the authoring system: see §4.3.

### Other modern nice-to-haves (backlog, roughly by value)

1. **glTF 2.0 import** for static meshes and skinned actors (via cgltf). It replaces the unbuilt 3ds Max exporter in `legacy/` as the main art path, and glTF PBR materials map directly onto Phase 2. Surfaces in the editor as an importer in the asset browser (§4.2).
2. **Audio:** XAudio2 in place of DirectSound; keep MP3/Ogg streaming; HRTF optional. A mixer with buses (music, effects, voice, UI) that the options screen and scripts control.
3. ~~Input~~ → moved into the game framework (§4.5): action mapping, controllers, rebinding.
4. ~~Packages~~ → moved into build and packaging (§4.6).
5. **Physics:** replace the collision pieces with **Jolt Physics** for rigid bodies and character controllers, keeping BSP hulls for world collision (ODE is in `Engine/External` but ageing). The game framework's character controller (§4.5) is designed so it can switch to Jolt later.
6. **Anti-aliasing:** MSAA, FXAA or TAA, selectable (TAA off by default to keep crisp pixels).
7. **Build and CI:** GitHub Actions on `windows-latest` building the solution in Debug/Release, plus the screenshot regression and a command-line build of every template (§4.6).
8. **Tooling:** asset hot-reload for textures, materials and levels in the Game Shell and play-in-editor; an in-game Dear ImGui debug overlay, sharing the editor's ImGui host (§4.2).
9. **Phase 3 follow-ups:** a per-level exposure setting stored in the level (the API exists), volume fog, and actors receiving real-time shadows (they cast them).

---

## 4. Game Authoring System

The engine becomes the runtime of an authoring system. Today a game is assembled by hand: levels come from the World Editor, actors from Actor Studio and ActBuild, textures from G3DTexImport, logic from Eos scripts run by the Game Shell, and shipping means copying `bin/` and editing scripts. The goal is one workflow in one application: **create a project from a template → build levels, actors, materials, scripts and screens → play in the editor → build and package the game.**

What exists to build on: the MFC World Editor (`Tools/WorldEditor`, already running the Phase 1–3 renderer in its 3D view), the Game Shell and its script manager (`Tools/GameShell`, Eos), object DLL plug-ins (`Objects/`), the property system (`grProperty`) that drives entity panels, VFile (directories, packs, virtual files), the actor tools (`Tools/ActorTools`), G3DTexImport, and the Material Editor dialog.

### 4.1 Projects and templates

- **Project file** `<Name>.g3dproj` (JSON): name, version, start screen or level, window and video defaults, Look Profile and look settings, input map, audio buses, the content folders and the engine library it builds on.
- **Project layout:**
  ```
  MyGame/
    MyGame.g3dproj
    Levels/  Actors/  Materials/  Textures/  Scripts/  Graphs/  UI/  Fonts/  Audio/
    Config/        input map, game settings, localized string tables
    Build/         package output (not source)
  ```
- **Content search path:** VFile mounts the project's folders first and the engine library (today's `bin/GlobalMaterials`, actors, sounds and the PBR library) under them, read-only. A project overrides an engine asset by adding one with the same name, as `G3D_MATERIAL_OVERRIDES` does today. The editor's hard-coded `bin/` paths (`Level_CreateResourceMgr` and friends) become project-relative.
- **Asset database:** a per-project index (names, types, dependencies, thumbnails, import settings) kept in `Build/` and rebuilt when missing, so the asset browser, references ("where is this material used?") and packaging don't rescan everything.
- **Templates** (ordinary projects shipped under `Templates/`):
  - *Blank*: one room, a player start and the default screens.
  - *Retro FPS*: first-person movement, weapons and pickups, doors, lifts, triggers and a HUD; the genre Genesis3D is built for.
  - *Third-person adventure*: an actor with a camera boom, interaction and a dialogue box.
  - *Top-down*: an orthographic or angled camera with click-to-move.
  - *Walkthrough*: a showcase or architectural fly-through with a menu and no gameplay.
- **New Project wizard:** pick a template, name and folder; the editor copies the template, renames it and opens it. *Recent projects* and an optional start page.

**Done when:** a new project made from any template opens in the editor and plays straight away, and the existing sample levels work as an "Engine Samples" project.

### 4.2 The World Editor as the hub

- **UI toolkit (decided):** the editor stays **MFC**, and new complex editors are built with **Dear ImGui**.
  - *MFC* keeps everything that exists (the 4 views, dialogs, property panels, ~37k lines) and hosts the shell. Its editing logic is not rewritten.
  - *Dear ImGui* (with its docking branch, plus a node-editor extension such as imgui-node-editor) is used for the editors MFC is weak at: the visual script graph, the UI/screen editor, the actor editor, and later tools of the same kind. Each is a panel the MFC shell hosts: a child window with its own D3D12 swap chain on the engine's device, drawn by the engine each frame, with input forwarded from MFC. Previews inside them (actor, UI canvas, materials) are drawn by the engine too (rule 7).
  - The ImGui host is shared by the editor and the engine's debug overlay (backlog item 8), so there is one integration to maintain.
  - A full port of the editor to another toolkit is not planned; the core (`Tools/WorldEditor/Core`) is kept UI-free so the option stays open.
- **Shell:** move the main frame to the MFC Feature Pack (`CFrameWndEx`, `CDockablePane`, `CMFCVisualManager`): dockable, saved layouts, the dark theme and high DPI from Track A. The 4-view layout stays the default workspace. ImGui panels dock like any other pane.
- **Panels:** Project/asset browser (folders, filters, search, thumbnails, drag and drop into the level), Properties (the `grProperty` grid, with tooltips from object definitions), Outliner (entities and brushes in the level), Output/Console (log plus a Lua REPL), and the existing Textures/Groups/Models lists folded into the asset browser.
- **Editors opened from the hub**, each as a document tab or dockable window working on the project:
  - *Level editor* (today's views),
  - *Actor editor*: Actor Studio and Actor Workbench functionality (motions, materials, sockets, preview) moved into the World Editor, with ActBuild as its back end,
  - *Material editor*: the Phase 2 dialog, docked, with a preview sphere,
  - *Script editor* and *visual script editor* (§4.3),
  - *UI editor* (§4.4),
  - *Project settings* and *Build* (§4.6),
  - importers (textures via G3DTexImport, glTF from the backlog, sounds) run from the asset browser.
- **Play-in-editor:** Play runs the game runtime (§4.6) in the 3D view or in its own window, from the current camera or the player start, with the project's start screen optional. Stop returns to editing with the level untouched. Script, material and texture edits hot-reload while playing. Errors show in the console with file:line links.
- **Undo/redo** across every editor (extending `Core/Undo.c`), and autosave.

**Done when:** a game can be made from a template without leaving the World Editor or opening another tool.

### 4.3 Scripting: Lua and visual scripting

*Lua runtime* (formerly Track B):
- **Runtime:** Lua 5.4 (vendored under `Engine/External/lua`, a static library), bound with **sol2** (header-only C++17).
- **In the engine:** a `grScript` module in `Genesis3D.dll`, so the Game Shell, play-in-editor, the packaged game and the editor's console share one host. An `IScriptVM` interface keeps **Eos** working for the existing samples; Eos is frozen, and new samples, templates and docs use Lua.
- **API (v1):** `engine`, `world` (trace/ray, find entities, spawn), `entity` (position, properties, targets), `actor` (motions, sockets), `camera`, `light`, `sound`, `input` (actions, §4.5), `timer`, `log`, `ui` (§4.4), `game` (modes, save/load, levels, §4.5), `settings`. Documented from the binding definitions, so the docs and the editor's autocomplete never drift.
- **Entity scripts in classic style:** any entity can have a `script` property pointing to a `.lua` file with hooks `OnSpawn`, `OnThink(dt)`, `OnTouch(other)`, `OnTrigger(activator)`, `OnUse(user)` and `OnDamage`. `target`/`targetname` wiring stays the main way to connect a level; Lua adds logic on top. Lua can also declare new entity classes, with properties that appear in the editor's property grid like an object DLL's.
- **Workflow:** hot reload; scripts load through VFile so they work from packages; a sandbox without `os`/`io`; a per-script time budget; errors with file:line in the log and the editor console.
- **Tests:** a headless harness that runs scripts against a loaded level without a window (also used by CI on templates).

*Script editor:* a Scintilla-based editor tab with Lua highlighting, API autocomplete and signatures, go-to-definition for project scripts, and a debugger (breakpoints, stepping, locals and a watch list, through Lua debug hooks) connected to play-in-editor.

*Visual scripting:*
- **Graphs compile to Lua**, so there is one runtime, one debugger and one API: a graph is a generated `.lua` file plus its `.g3dgraph` source (JSON), and graphs and hand-written scripts can call each other.
- **Kinds of graph:** entity graphs (the same hooks as entity scripts), level graphs (level start, triggers, sequences) and screen graphs for UI logic (§4.4).
- **Nodes:** events, flow (branch, sequence, loop, delay, gate, do-once), variables, math, and every API function, generated from the same binding definitions as the docs; custom nodes can be written in Lua.
- **Editor:** a Dear ImGui node canvas in an editor panel (§4.2), with pan/zoom, search-to-add, comments and groups, validation while editing, and the running node highlighted while playing (through the generated code's line map).

**Done when:** each template's gameplay is written in Lua or graphs with no C++, and a breakpoint set in the editor stops a running play-in-editor session.

### 4.4 UI and screens

- **UI runtime (engine):** a retained widget tree drawn in the overlay pass after `World_EndPass`: panel, image (with 9-slice), text (TrueType through stb_truetype into signed-distance-field atlases, so text stays sharp at any size), button, toggle, slider, list, scroll view, progress bar, text entry, and a 3D view widget (a camera into the world, e.g. for a menu background). Anchors and layout groups adapt to resolution and aspect; styles and themes; animation (tweened properties, timelines); mouse, keyboard and controller navigation with focus; localized strings from string tables.
- **Screens** are `.g3dui` files (JSON), with their logic in Lua or a screen graph and data binding to script values (health, ammo, options).
- **Screen flow:** the project defines its screens and how they connect: splash, main menu, options (video, audio, controls with rebinding), loading screen, HUD, pause, game over, credits, and custom ones. The runtime handles the stack (pause over gameplay, dialogs over pause), transitions, and pausing the game world.
- **Loading screens are real:** level loading reports progress (BSP, textures, lightmaps, actors, scripts) to the loading screen, which keeps animating while the level loads.
- **UI editor:** a WYSIWYG canvas drawn by the engine's own UI runtime (rule 7), with a widget palette, hierarchy, anchors/layout gizmos, a property grid, style editing, preview at chosen resolutions and languages, and a screen-flow view of how screens connect.
- **Templates** ship a default set of screens, so every new project has working menus, options and a pause screen from the start.

**Done when:** the Blank template's splash, main menu, options, loading, HUD and pause screens are made entirely in the UI editor, and work with mouse, keyboard and controller.

### 4.5 Game framework

Common game systems, in the engine and its Lua API, so templates and games don't each re-invent them:
- **Game flow:** game modes (rules for a level or session: spawn, win/lose, respawn), level transitions with persistent data, and save/load of entity state, player state and script variables.
- **Player and cameras:** character controllers (first-person, third-person, top-down, fly) on the BSP collision, designed to move to Jolt later (backlog); camera rigs (boom with collision, fixed, rail, cutscene) with blending.
- **Level building blocks** as entities with editor properties: triggers, doors, movers and lifts, buttons, pickups, spawners, teleporters, sound emitters, cameras, info points, and particle effects.
- **Input:** named actions and axes mapped to keyboard, mouse and controllers (XInput/GameInput), in the project's input map, rebindable by players in the options screen.
- **Audio:** the mixer buses from the backlog, music playlists and ambience zones.
- **AI basics:** waypoint graphs (or BSP-leaf-based navigation), steering, and simple behaviours scriptable in Lua or graphs.
- **Settings:** video, audio, controls and gameplay settings saved per player, and applied by the options screen.

**Done when:** the Retro FPS and Third-person templates are complete, playable games built only from these systems, scripts and content.

### 4.6 Runtime, build and packaging

- **Game runtime:** a player executable derived from the Game Shell that boots a project or a package: reads the project settings, mounts content, shows the start screen and runs the game. Play-in-editor and the packaged game use it (rule 7); the Game Shell stays as the developer runner.
- **Build** (from the editor's Build menu, or the command line for CI):
  1. validate: missing or unused assets, broken references, script and graph errors, levels with stale lighting;
  2. cook: build BSP and lighting where needed, compress textures (G3DTexImport), compile graphs to Lua and optionally scripts to bytecode, pack UI and fonts;
  3. package: content in `.g3dpak` files (zip-based, mounted by VFile; the backlog "packages" item), with the Release runtime, the D3D12 driver and its ini, the game's name, icon and version, and the licenses of the third-party libraries;
  4. output: a folder or a zip, ready to run. An installer is optional later.
- **Configurations:** *Development* (console, logging, hot reload) and *Shipping* (no console, content packed, scripts precompiled).
- **Incremental builds:** only changed assets are re-cooked, using the asset database (§4.1).

**Done when:** *Build → Package* turns each template into a folder that runs on a machine without the editor, and CI builds every template from the command line.

---

## 5. Suggested order and milestones

```
M1  Phase 0 (foundations)                                  done
M2  Phase 1 (world-space GPU path)                         done
M3  Phase 2 (PBR materials + editor)                       done
M4  Phase 3 (clustered lights, HDR, shadows, Look Profiles) done, wrap-up below
─── authoring system ─────────────────────────────────────────────────────────
M5  Authoring foundations: grScript (Lua in the engine, API v1), project file and
    content search path, game runtime, package MVP (a Blank project runs as a
    packaged folder)                                        §4.1 §4.3 §4.6
M6  Editor hub: docking shell, ImGui panel host, project/asset browser, play-in-editor, console,
    script editor and debugger, actor editor in the hub     §4.2 §4.3
M7  UI and screens: UI runtime, UI editor, screen flow, loading/pause/options,
    input actions                                           §4.4 §4.5
M8  Game framework and templates (Retro FPS, Third-person, Top-down, Walkthrough),
    full Build (validate, cook, .g3dpak, Shipping), CI      §4.1 §4.5 §4.6
M9  Visual scripting                                        §4.3
─── in parallel ──────────────────────────────────────────────────────────────
    Track A: editor navigation and editing (continues through M6's shell work)
    Phase 4: GI (can start any time after M5; the bake is an editor feature, so
             its UI lands in the hub)
    Backlog
```

Why this order: everything in the authoring system runs on the script host, the project model and the runtime, so M5 comes first and delivers something usable on its own (a packaged game). The hub (M6) then gives every later editor a home. UI (M7) comes before the game framework because every template needs menus, a HUD and a loading screen. Visual scripting (M9) comes last because it compiles to Lua and generates its nodes from the API, which must be stable first.

**Phase 3 wrap-up before M5:** a Release build and test of everything, and a hands-on check of the World Editor's Look menu and the new light properties. The remaining Phase 3 extras (per-level exposure, volume fog, actors receiving shadows) are in the backlog.

**Recommended first step for M5:** `grScript` with the Lua runtime and API v1 inside `Genesis3D.dll`, driven by the Game Shell, with one sample converted from Eos. The project file and content search path come next, then the runtime and package MVP.

---

## 5a. Progress

**Track A — navigation (first step done).** The World Editor 3D view has RMB fly (WASD, Q/E, Shift, wheel speed), Alt+LMB orbit, MMB pan, wheel dolly and F / Shift+F framing, with a time-based, eased fly camera. "Classic Genesis Navigation" in the 3D view's context menu restores the original controls; the choice is stored in the app profile (`Navigation\Keymap`) rather than a `.ini`, like the editor's other settings. Still to do: numpad view snapping, maximize-view key, camera sync between views, gizmos and the rest of the editing list.

**Phase 0 — done.**
- *Shader build:* HLSL lives in `Renderer/Direct3D12/Shaders/`. The `G3DCompileShaders` target compiles each entry point with the Windows SDK's DXC (SM 6.0) into the driver. The source is also embedded and compiled at run time (SM 5.0) when the device lacks SM 6.0, when `[Shaders] RuntimeCompile=1`, or from loose files via `[Shaders] SourceDir` for iterating on shaders.
- *Frame structure:* the scene renders into an `R16G16B16A16_FLOAT` target, and `Shaders/Present.hlsl` copies it to the back buffer (pass-through for now; tonemapping and Look Profiles go there in Phase 3). Depth is `R32_TYPELESS` so later passes can sample it. `[Render] SceneTarget=0` draws straight to the back buffer. A per-frame upload ring (`D3D12UploadRing`) holds the per-frame constant buffer (`FrameConstants`, b1: viewport size, frame number, time) and the poly cache's vertices, replacing a committed upload buffer per flush. Per-draw data is four root constants (flags and texture indices). Per-view constants will use the same ring in Phase 1, which is the first path with a view to describe.
- *Debugging:* `bin/Direct3D12Driver.ini` controls the debug layer, GPU-based validation and WARP; any key can be overridden with `G3D_D3D12_<KEY>`. Debug-layer messages are written to `Direct3D12Driver.log`, and the command list carries PIX markers for the scene, each poly-cache flush and the present pass. Timestamp queries measure the scene and present passes; the times reach the engine through `DRV_Driver::GPUTimings` (driver interface version 5) and show as a `GPU` line in the debug overlay. The overlay itself was blank on D3D12: the engine sent all debug text to the driver's `DrawText`, which was a stub. The engine now draws its built-in bitmap font through `DrawDecal` when a driver has no `DrawText`.
- *Regression safety:* `G3DGameShell -screenshot` plus `tests/render/RenderRegression.ps1` (reference shots in `tests/render/shots.txt`, baseline recorded locally with `-Update`). Repeat runs are pixel-identical, and WARP matches the hardware image on the reference shots. The HDR path differs from direct rendering by at most 1/255 per channel (fp16 storage and unquantized blending), within the default tolerance of 2.
- *Bindless-lite:* on resource binding tier 2+ hardware the pixel shaders index the texture heap directly (`Textures[]`, t0 space1) with the texture index from the draw constants, so draws no longer switch descriptor tables. Tier 1 hardware, or `[Render] Bindless=0`, keeps per-draw tables. The heap holds one slot per texture handle plus reserved driver slots (the scene target's SRV lives there, so the frame uses a single heap), every slot always holds a valid view, and a destroyed texture keeps its resource and slot until the GPU fence passes. The old code released the resource immediately, even if a queued frame still used it. Both paths match the baseline, and GPU-based validation reports nothing.

**Phase 1 — in progress.**
- *GPU world path:* each BSP (the world and every model) uploads its draw faces once through `DRV_Driver::WorldGeometry_*` (driver interface version 6). The engine still does PVS and frustum culling per node. Visible textured faces are queued by index instead of being clipped, transformed and projected on the CPU, and `Shaders/World.hlsl` reproduces `grCamera`'s projection. Runs of faces that share geometry, view, flags and layer count become one indexed draw. `[Render] WorldPath=0` returns to the transformed-poly path. Untextured faces, editor-callback faces and the portal faces themselves stay on that path.
- *Portals and mirrors:* nested views (portal, mirror and sky-box cameras) use the GPU path too. `DRV_WorldView` carries the view's camera-space frustum (the portal polygon's edges, its front plane and the far plane, at most 8) as `SV_ClipDistance` planes, the same planes the CPU clipper uses. A view with more planes falls back to the CPU path (interface version 7).
- *Draw statistics:* `DRV_GPUTimings` also reports draw calls, world-path draws and the faces they cover, shown as a `Draws` line in the debug overlay. The poly cache now also merges consecutive transformed polys that share flags and textures and whose vertices are contiguous. This is order-preserving, so it is pixel-identical. Draws per frame: StreetScene 2054 → about 45, tutorial2_complete 487 → 44.
- *Regression:* the GPU path matches the CPU path on all 10 shots, including two new portal shots (`sky_portal`, `portal`). No debug-layer messages.
- *Lightmap atlases:* on the GPU path each face's lightmap is shelf-packed into a shared 1024×1024 atlas page (`D3D12LightmapAtlas`, with a 1-texel replicated border so filtering matches a clamped texture) instead of its own texture. Texels are staged on the CPU and go out through the upload ring as one packed copy source per flush, recorded before the flush's draws, instead of one committed upload buffer and two barriers per lightmap. Dynamic lightmaps are re-staged when the engine flags them. `[Render] LightmapAtlas=0` and `LightmapAtlasSize` control it; lightmaps that don't fit use their own handles.
- *Lightmap UV fix:* both paths normalized lightmap UVs by `1 << Log`, which the D3D12 driver rounds down, while its lightmap textures are exactly Width × Height. That stretched every non-power-of-two lightmap by up to 2×. (The original drivers used square power-of-two lightmap textures rounded up.) Both paths now divide by the real size, and the local baseline was re-recorded with `WorldPath=0`. The GPU path matches it on all 10 shots with the atlas on and off.
- *Actors:* puppets are still skinned and vertex-lit on the CPU, but in world space. Their triangles go to the GPU through `DRV_Driver::WorldMesh_Render` (interface version 8, `DRV_MeshVertex`) with the same `DRV_WorldView` as the world, including portal and mirror clip planes, and `VSMesh` in `Shaders/World.hlsl` projects them. The CPU no longer clips (`grTClip` / `grFrustum`) or projects them. Back faces are rejected in world space, and each run of triangles that share a texture is one draw. A puppet with an untextured material, or a frustum with more than 8 planes, stays on the CPU path. This also fixes world-space skinning (`grBodyInst_GetGeometry` without a camera), which ignored multi-bone blended vertices, so actors seen through a frustum were distorted. The `dancer` baseline was re-recorded for that. GPU and CPU actors differ by at most 1/255 on a few pixels, and all 10 shots pass with no debug-layer messages.
- *Still to do:* a manual edit/rebuild pass in the World Editor. Face create, destroy and UV changes already mark the BSP's GPU geometry dirty, and it is rebuilt at the next outermost render.

**Phase 2 — done.**
- *Material data:* new layer types `NORMAL` (3), `ORM` (4), `EMISSIVE` (5) and `HEIGHT` (6), and an optional PBR block in `.jmat` version 2 (`grMaterialSpec_PBR`: base color tint, roughness and metal factors, emissive color and intensity, alpha mode and cutoff, two-sided and Retro flags; `grMaterialSpec_Get/SetPBR`, `grMaterialSpec_FindLayer`, `grMaterialSpec_IsPBR`). A material without PBR data is still written as version 1, so older engines keep reading it, and all 389 shipped materials load unchanged. PBR layer bitmaps are attached to the driver together with the base layer.
- *Driver interface (version 9):* `WorldGeometry_RenderFacePBR` takes a `DRV_WorldMaterial` (normal, ORM and emissive maps plus the scalars), and `WorldGeometry_SetLights` gives the driver the BSP's visible dynamic lights (at most 8, model space). The engine uses them for GPU-path faces whose material is PBR. All other faces are drawn exactly as before.
- *Shading:* `VSWorldPBR` / `PSWorldPBR` in `Shaders/World.hlsl` implement metallic/roughness GGX with height-correlated Smith visibility, Schlick Fresnel and a Lambert diffuse, in linear space. The lightmap is the diffuse irradiance and also feeds the specular through a split-sum approximation (Karis' analytic environment BRDF, with specular occlusion from the ORM occlusion). Dynamic lights add GGX specular and the normal map's diffuse detail, with the engine's falloff (`Color * (Radius - d)`); their flat-face diffuse is already in the lightmap. Color conversion uses a pure 2.2 gamma, so base × lightmap matches the gamma-space path. Missing maps fall back to constants: a flat normal, the factors alone for roughness and metalness, and no emission. Normal maps use the DirectX convention (+Y down the image). *Retro* point-samples every map and snaps shading to the base texture's texel centers. *Cutout* clips below the cutoff, and *blend* sets the face's alpha flag.
- *Fix found on the way:* the Phase 1 GPU vertex normals pointed into the face, the opposite side convention from `grBSPNode_Light`. Only the PBR shader reads them.
- *Sample and tests:* `G3D_MATERIAL_OVERRIDES=<dir>` makes a `.jmat` in that directory replace the material of the same name, including a level's own bitmap materials, without editing levels or shipped content. `tests/render/pbr/make_pbr_sample.py` generates PBR versions of five `tutorial1` materials into `bin/GlobalMaterials/PBRSample/` (git-ignored): brass, silver and blue metals, polished marble and a rough wall. Each gets a normal map derived from the base texture's luminance and an ORM map with occlusion from cavities. `G3DGameShell -dlight x y z radius r g b` adds dynamic lights to screenshots, and `shots.txt` takes an optional fourth column of shell arguments and `NAME=value` environment variables. New shots: `dlight` (the CPU and GPU paths match) and `pbr_tutorial1`. All 12 shots pass with no debug-layer messages.
- *Texture import:* `G3DTexImport` (`Tools/TexImport`, built on DirectXTex, which is a git submodule in `ThirdParty/DirectXTex` pinned to the may2026 release) converts PNG, TGA, DDS, JPEG, BMP and TIFF into DDS files with full mip chains: base and emissive to BC7 sRGB (mipmaps filtered in linear light), normal maps to BC5, ORM to BC7 and height to BC4. `-material <name> <base image>` finds the other maps by suffix (`_n`/`_normal`, `_orm`/`_arm`, `_e`/`_emissive`, `_h`/`_height`), packs separate `_ao`, `_rough` (or inverted `_gloss`) and `_metal` grayscale maps into one ORM map, and writes a version 2 `.jmat` whose texture layers point at the DDS files in a pak subdirectory of `GlobalMaterials`. Options cover the PBR scalars and flags, OpenGL-style normal maps (`-flipgreen`) and a quick BC7 mode.
- *DDS in the engine:* driver interface version 10 adds `THandle_CreateFromDDS`. The engine reads a `.dds` into memory and passes it on, and texture resources now try `.dds` before `.png` and `.bmp`. The D3D12 driver loads it with DirectXTex's `DDSTextureLoader12` (sRGB formats as their UNORM equivalents, since the shaders do the gamma conversion) and uploads every mip in one copy. Texture-kind layers now work as the base layer on both the GPU and the CPU path (previously they drew untextured) and as PBR maps.
- *Fixes:* `Pak:Name` resource names never worked: the pak subdirectory was opened relative to the current directory instead of `GlobalMaterials` (and asserted). The first Phase 2 sample also wrote `.jmat` layers with a 48-byte transform instead of `grXForm3d`'s 64, so its normal and ORM maps never loaded and its baseline showed factor-only shading. The sample is now built with `G3DTexImport` (DDS base, normal, ORM, one packed ORM and one emissive map), and `pbr_tutorial1` was re-recorded. The DDS base textures render within 0.5/255 of the original bitmaps on the CPU path.
- *Actors:* driver interface version 11 adds `WorldMesh_RenderPBR` (`DRV_MeshVertexPBR`: the mesh vertex plus normal and tangent). Puppets with a PBR material send their skinned vertex normals and a tangent per triangle from the UVs. The vertex color, which is the engine's CPU lighting, is the irradiance, and the world's dynamic lights add specular and normal-map detail through the same `PSWorldPBR` (a one-entry material table per run). Body materials take `G3D_MATERIAL_OVERRIDES` by name (the names are written to the debug output while overrides are active). Models were already covered, since each model is a BSP on the world path.
- *Parallax:* a `HEIGHT` layer drives parallax occlusion mapping (8–32 steps depending on the view angle, with a linear refinement), applied before every map is sampled. `.jmat` version 3 adds `HeightScale` to the PBR block; version 2 files load with the default of 0.04.
- *Material Editor:* right-clicking a texture in the World Editor's Textures panel opens *Edit Material (PBR)*. It has one slot per channel (base, normal, ORM, emissive, height, and grayscale occlusion, roughness or gloss, and metalness, which are packed into ORM), filled by browsing or by dropping files, which land in the slot their suffix names. *Auto-find maps* searches next to the base image. The dialog also covers every PBR scalar and flag. *Import & Apply* runs `G3DTexImport` (which now also writes the editor's thumbnail and accepts explicit map paths), then reloads the material and swaps it into the open level, so the 3D view is the live preview.
- *Tests:* the sample adds a parallax height map (`tech_blue`) and a gold PBR material for the dancer actor. New shot: `pbr_dancer`. All 13 shots pass.
- *Left for later:* a preview sphere in the Material Editor (the level itself is the preview for now), true sRGB texture formats once the pipeline is linear (Phase 3), and a purpose-built PBR sample level (the `tutorial1` and `dancer` overrides serve for now).
- *Static lights on PBR surfaces (interface version 12):* lightmaps hold static lights without direction, so normal maps and specular only answered dynamic lights. Static lights now add the normal map's detail and specular too, faded by how much of the light the lightmap shows at that point (a shadow estimate at lightmap resolution).

**Phase 3 — done.**
- *Look Profiles (interface version 13):* `DRV_Driver::SetLook` / `GetLook` take a `DRV_LookSettings`; the engine exposes it as `grEngine_SetLookProfile` / `grEngine_SetLookSettings` (`GR_LOOK_CLASSIC`, `ENHANCED`, `STYLIZED`) and keeps it across driver restarts. Defaults come from `[Look]` in `Direct3D12Driver.ini` (documented there, every key overridable with `G3D_D3D12_<KEY>`). **Classic is the default and draws exactly as before**: all 15 earlier shots match their baselines except two PBR actor shots, whose highlights now also see static lights. The World Editor's 3D view has a *Look* submenu (right-click), remembered in the app profile; `G3DGameShell -look` sets it for screenshots.
- *Frame lighting, clustered forward:* the engine gives the driver every light of the frame once, in world space, with the main camera (`World_SetFrame`, `DRV_Light`: point, spot and directional, static or dynamic, cast-shadows flag). A compute pass (`Shaders/Cluster.hlsl`) bins point and spot lights into a 16×9×24 froxel grid of the main camera (up to 64 per cluster, 1024 per frame); `PSWorldPBR` loops over its pixel's cluster, directional lights reach every pixel, and other cameras (portals, mirrors, detected by comparing each view's world-to-camera transform) test every light. Shading moved to world space (`DRV_WorldView.ModelToWorld`). The per-draw `WorldGeometry_SetLights` list is gone from the engine (the entry stays for the ABI).
- *Light types:* `grLight` gains spot lights (`GR_LIGHT_FLAG_SPOT`, `grLight_SetSpot`, smoothstep cone) and `GR_LIGHT_FLAG_CAST_SHADOWS`. The light record is version 1 only for spot lights, so other lights stay readable by older engines. Spot cones apply to the lightmap baker, the CPU dynamic lights and the GPU. Dynamic suns are lit by the GPU only (the CPU lightmap path never handled them). `DynLightObj` and `PulsingLightObj` map their existing (unused) *Cast Shadow* property to the flag, and the World Editor's light entity gains *Spot angle / pitch / yaw* and *Cast shadows*.
- *Shadows:* in the Enhanced and Stylized looks, dynamic lights marked to cast shadows are left out of the CPU lightmap combine and shaded on the GPU with real-time shadows: 6 cube faces for a point light, one view for a spot, 4 stable (texel-snapped, bounding-sphere) cascades for a directional light, all in one depth atlas (`[Shadows] AtlasSize`, `MaxLights`, `Distance`; the nearest lights win), 3×3 PCF with slope bias and normal offset. Casters are every world geometry (a static index buffer per geometry, culled by bounding sphere) and the frame's actor meshes. Dynamic lights without the flag keep the CPU's per-lightmap-texel shadows, as in Classic. Static lights stay baked.
- *Frame recording:* the poly cache now records the whole frame at `EndScene` (it grows instead of flushing mid-frame), so the shadow and cluster passes see every draw before the scene is recorded. `World_EndPass` (called by `grWorld_Render`) marks where the 3D scene ends; post-processing runs there and the overlay is drawn after it, untonemapped.
- *HDR and post-processing (`D3D12Post`, `Shaders/Post.hlsl`):* in Enhanced and Stylized the scene is linear HDR (every shader writes linear light; frames without a world stay in display space) and the post pass composites into its own target: auto exposure from a 256-bin luminance histogram (compute, adapted per frame so screenshots are deterministic; `ExposureEV` is the manual exposure or the compensation), SSAO from depth (normals from depth, depth-aware blur; off by default), 6-level bloom (Jimenez 13-tap down / tent up, Karis average on the first step), height fog, the engine's distance fog (`grEngine_SetFog` finally works on D3D12, in every look), then ACES (default) or AgX tonemapping. Legacy materials in these looks go through the PBR shader with default parameters; their fullbright faces stay unlit, and lights never exceed lightmap brightness, so levels keep their balance.
- *Stylized:* render scale (default ½, nearest-neighbour upscale), palette quantization (bits per channel) with a 4×4 ordered dither, CRT scanlines and vignette, vertex snapping to a coarse screen grid, and dynamic lights shaded per lightmap texel (off by default: coarse lightmaps turn it blocky).
- *Fixes found on the way:* the run-time shader fallback (SM 5.x) did not compile `World.hlsl` (an uninitialized `inout` struct in `ProjectToView`), so devices without SM 6.0 had no world path; every shader now compiles on both paths. The `grResource` singleton could dangle (asserts and a crash on exit), and a full driver restart lost DDS textures (they are re-created and remapped).
- *Tests:* new shots `p3_enhanced`, `p3_stylized`, `p3_shadow` (point light shadows of the dancer's pillars), `p3_spot`, `p3_effects` (AgX, SSAO, height fog), `p3_retro` (vertex snap, CRT, per-texel lighting) and `p3_portal`; `G3DGameShell` gains `-look`, `-shadowlight` and `-spotlight`. All 22 shots pass, repeat runs are pixel-identical, and the debug layer reports nothing.
- *Left for later:* area and tube lights; actors receive shadows only through their CPU lighting (they cast them); a per-level exposure override stored in the level (the API is there); editor controls for the other look settings (the ini covers them); alpha-tested and sky faces cast solid shadows.

## 6. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Breaking old `.j3d` / `.mat` content | Versioned chunks only, fallbacks for every new field, and a reference-level screenshot regression. |
| `DRV_Driver` ABI change | Append-only function pointers plus a version bump. The engine checks the version before calling the new entry points. |
| Losing the retro look | Look Profiles and a per-material `Retro` flag. Classic is always one switch away and is tested in CI. |
| Scope creep in GI | Tier A (baked) is the product, and Tiers B/C are optional extras behind capability checks. |
| No Windows build or test in cloud sessions | Changes are validated locally in Visual Studio. The CI workflow (backlog item 7) closes this gap. |
| The authoring scope is much larger than the engine work | Milestones M5–M9 each end with something usable (a packaged game, a hub, menus, templates, graphs), and templates double as end-to-end tests, so the project is shippable at every step. |
| MFC for complex editors (node graphs, UI canvas, docking) | Decided: the MFC Feature Pack covers docking and theming, and the complex editors are Dear ImGui panels the engine draws inside the MFC shell (§4.2), which also keeps previews true to the runtime (rule 7). No rewrite of the existing editor. |
| Two UI toolkits in one editor | Clear split: MFC for the shell and the existing editors, ImGui only for new canvas-style editors. One shared ImGui host, one theme mapped from the MFC visual manager's colors, and input routing between the two handled in one place. |
| Two script languages | Eos is frozen for the existing samples; everything new is Lua, and graphs compile to Lua, so there is only one runtime to support. |
| Editor and runtime drifting apart | Play-in-editor and packages run the same runtime; CI builds and runs every template headless and packaged (§4.6). |
| Third-party licenses in shipped games | Only permissive dependencies (MIT/BSD/zlib, e.g. Lua, sol2, stb, Scintilla, Jolt, Dear ImGui); packages include their license texts. |
| Hard-coded `bin/` paths throughout the tools | Moving to the project content search path (§4.1) happens early in M5, behind VFile, with the engine library as the fallback so existing levels keep loading. |
