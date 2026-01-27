# Phased Build Context

Use this document to restore context after compaction.

## Current State
- **Active Phase:** 5 (I/O) - PRODUCTION READY
- **Last Successful Phase:** 4 (Platform) - PRODUCTION READY
  - Real GLFW window with OpenGL 4.5 context
  - Multi-backend GPU abstraction (OpenGL production, Vulkan/D3D12/Metal/WebGPU stubs)
  - 4 services: asset, shader, presenter, compositor
  - Render loop with 170+ FPS on RTX 3060 Ti
  - Hot-reload patterns (SACRED) preserved

### Phase 5 - PRODUCTION READY
  **Audio:**
  - Real audio output via miniaudio (WASAPI/CoreAudio/ALSA/PulseAudio)
  - 3D spatialization with distance attenuation models
  - Audio bus hierarchy (master, music, sfx, voice, ambient)
  - DSP effects chain (reverb, delay, filter, compressor, etc.)
  - Software mixing with volume/pan/pitch per source
  - SACRED hot-reload patterns (snapshot/restore)
  - AudioService integrated with ServiceRegistry

  **Input:**
  - Keyboard, mouse, gamepad input via GLFW
  - Action-based input mapping with multiple bindings per action
  - Input contexts with priority (gameplay, menu)
  - Gamepad support with deadzone and axis mapping
  - Mouse capture/release for FPS controls
  - SACRED hot-reload patterns (snapshot/restore)

## Architecture

### Two Files Control the Build
1. **CMakeLists.txt** - Dependencies, modules, linking
2. **src/main.cpp** - Includes and initialization code

Both files have matching Phase 0-12 sections. To activate a phase:
1. Uncomment that phase's section in CMakeLists.txt
2. Uncomment that phase's section in main.cpp
3. Build and test

### 12-Phase Structure

| Phase | Name | Modules | Dependencies |
|-------|------|---------|--------------|
| 0 | Skeleton | (none) | spdlog, nlohmann_json |
| 1 | Foundation | memory, core, math, structures | glm |
| 2 | Infrastructure | event, services, ir, kernel | - |
| 3 | Resources | asset, shader | stb, tinygltf |
| 4 | Platform | presenter, render, compositor | glfw, OpenGL |
| 5 | I/O | audio | miniaudio, dr_libs |
| 6 | Simulation | ecs, physics, triggers | - |
| 7 | Scene | scene, graph | - |
| 8 | Scripting | script, scripting, cpp, shell | - |
| 9 | Gameplay | ai, combat, inventory, gamestate | - |
| 10 | UI | ui, hud | - |
| 11 | Extensions | xr, editor | - |
| 12 | Application | runtime, engine | - |

## Key Files
- `CMakeLists.txt` - Root build config with phased sections
- `src/main.cpp` - Entry point with phased initialization
- `cmake/Modules.cmake` - `void_add_module()` and `void_add_header_module()`
- `cmake/CompilerWarnings.cmake` - `void_set_compiler_warnings()`
- `schemas/manifest.schema.json` - JSON schema for project manifests
- `schemas/scene.schema.json` - JSON schema for scene files
- `examples/model-viewer/` - Test project with manifest.json and scene.json

## Build Commands
```powershell
# From VS Developer PowerShell
cd C:\Users\ShaneRobinson\Documents\GitHub\void_engine
cmake -B build
cmake --build build
.\build\bin\Debug\void_engine.exe examples/model-viewer
```

## Process to Activate Next Phase
1. Read current CMakeLists.txt Phase N section
2. Uncomment dependencies (FetchContent)
3. Uncomment add_subdirectory() calls
4. Uncomment target_link_libraries() for the phase
5. Read current main.cpp Phase N section
6. Uncomment includes
7. Add initialization/validation code
8. Update phase number in summary message
9. Build and test

## Rules (from CLAUDE.md)
- NEVER delete code to fix builds
- Keep most advanced implementation when duplicates exist
- Hot-reload patterns are SACRED (snapshot, restore, dehydrate, rehydrate)
- JSON for all configuration (not TOML)
- Headers/cpp already exist - just uncomment and wire up

## Continuation Prompt
```
Continue the phased build of void_engine. We are on Phase [N].

Files to modify:
- CMakeLists.txt (uncomment Phase N section)
- src/main.cpp (uncomment Phase N includes, add init code)

Build with: cmake --build build
Test with: .\build\bin\Debug\void_engine.exe examples/model-viewer

See doc/PHASED_BUILD_CONTEXT.md for full context.
```
