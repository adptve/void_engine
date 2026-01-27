# Prompt: Fix Render Module

Copy everything below the line into a fresh Claude Code session.

---

Fix the render module ODR violation in void_engine.

## The Problem

`src/render/stub.cpp` (218 lines) and `src/render/gl_renderer.cpp` (1587 lines) both define the same 45+ functions. This is an ODR violation.

## The Solution

Remove stub.cpp from the build. Keep gl_renderer.cpp untouched - it has all the real implementations including hot-reload support.

## What You Must Do

1. Read `src/render/CMakeLists.txt`
2. Remove `stub.cpp` from the SOURCES list
3. Show me the exact change (before/after of CMakeLists.txt)
4. That's it. One line change.

## What You Must NOT Do

- Do NOT modify gl_renderer.cpp
- Do NOT modify any header files
- Do NOT "fix" any other errors that appear
- Do NOT comment out code anywhere
- Do NOT add stubs anywhere
- Do NOT run cmake or build until I say so

## Critical Code to Preserve (in gl_renderer.cpp)

These hot-reload functions MUST remain untouched:
- `ShaderProgram::reload()` (lines 248-274) - 27 lines of hot-reload logic
- `ShaderProgram::snapshot()` (lines 322-330) - captures shader paths
- `ShaderProgram::restore()` (lines 332-340) - restores from snapshot
- `m_on_reloaded` callback (line 272) - notifies listeners

## Verification

After the change, gl_renderer.cpp should still have:
- 1587 lines (unchanged)
- All ShaderProgram methods
- All SceneRenderer methods
- All mesh generation functions
- All hot-reload support

Show me the CMakeLists.txt change, then wait for my approval before any build.
