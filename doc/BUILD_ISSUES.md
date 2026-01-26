# Void Engine Build Issues - Migration Plan

## Overview
This document tracks remaining build errors and the plan to resolve them during the migration.
The goal is to have FULL implementations - NO STUBS.

## Current Status
- gl_renderer.cpp: **RE-ENABLED** (compiles successfully)
- physics world.cpp: **RE-ENABLED** (has compile errors - see below)
- SceneRenderer: **WORKING**
- GlCamera: **WORKING**

---

## Physics Module Issues

### 1. TransformedShape API Mismatch
**Files:** `query.hpp:256, 257, 351, 363, 392, 404, 770`

**Error:**
```
cannot convert 'const void_physics::IShape' to 'const void_physics::IShape*' in initialization
TransformedShape cast_shape{shape, start.position, start.rotation};
```

**Root Cause:**
TransformedShape struct expects `const IShape*` but code passes `const IShape&`.

**Fix Required:**
Check TransformedShape definition in types.hpp and fix either:
- The struct definition to accept reference, OR
- The callsites to pass `&shape` instead of `shape`

---

### 2. ContactManifold Missing `normal` Member
**File:** `query.hpp:276`

**Error:**
```
'struct void_physics::ContactManifold' has no member named 'normal'
```

**Fix Required:**
Add `normal` member to ContactManifold struct in types.hpp OR use existing member name.

---

### 3. SphereShape Missing `center()` Method
**File:** `query.hpp:574`

**Error:**
```
'const class void_physics::SphereShape' has no member named 'center'
```

**Fix Required:**
Add `center()` method to SphereShape class in shape.hpp. Likely should return `Vec3(0,0,0)` or the local offset.

---

### 4. IShape Missing `local_transform()` Getter
**Files:** `snapshot.hpp:417,418`, `world.cpp:777`

**Error:**
```
'const class void_physics::IShape' has no member named 'local_transform'; did you mean 'set_local_transform'?
```

**Root Cause:**
There's `set_local_transform()` but no getter.

**Fix Required:**
Add `local_transform()` getter method to IShape in shape.hpp:
```cpp
[[nodiscard]] Transform local_transform() const {
    return Transform{m_local_offset, m_local_rotation};
}
```

---

### 5. CylinderShape Class Missing
**Files:** `snapshot.hpp:434, 467`

**Error:**
```
'CylinderShape' was not declared in this scope
```

**Fix Required:**
Either:
- Add CylinderShape class to shape.hpp
- Remove Cylinder case from snapshot.hpp if cylinders not yet supported

---

### 6. set_local_transform API Mismatch
**File:** `snapshot.hpp:481`

**Error:**
```
cannot convert 'void_math::Transform' to 'const void_math::Vec3&'
shape->set_local_transform(t);
```

**Fix Required:**
Change to:
```cpp
shape->set_local_transform(t.position, t.rotation);
```

---

### 7. Result Error Type Mismatch
**File:** `snapshot.hpp:786`

**Error:**
```
could not convert 'Result<void>' to 'Result<HotReloadSnapshot>'
```

**Fix Required:**
```cpp
return void_core::Err<void_core::HotReloadSnapshot>(snap_result.error());
```

---

### 8. CharacterController Missing m_impl
**Files:** `world.cpp:570, 577, 589, 598`

**Error:**
```
class 'void_physics::CharacterController' does not have any field named 'm_impl'
```

**Fix Required:**
Add to CharacterController class in world.hpp:
```cpp
private:
    std::unique_ptr<CharacterControllerImpl> m_impl;
```

---

### 9. CharacterControllerImpl Incomplete Type
**File:** `world.cpp:570`

**Error:**
```
invalid use of incomplete type 'class void_physics::CharacterControllerImpl'
```

**Fix Required:**
Define CharacterControllerImpl class, either:
- In world.cpp (implementation detail), OR
- In a separate header for shared use

---

## Asset Module Issues

### 10. stb_image Functions Undefined
**Files:** `texture_loader.cpp`

**Error:**
```
undefined reference to 'stbi_loadf_from_memory'
undefined reference to 'stbi_failure_reason'
undefined reference to 'stbi_image_free'
undefined reference to 'stbi_load_from_memory'
```

**Root Cause:**
stb_image.h is header-only and requires `#define STB_IMAGE_IMPLEMENTATION` in exactly one .cpp file.

**Fix Required:**
Create `src/asset/stb_impl.cpp`:
```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```
Add to void_asset CMakeLists.txt.

---

## Priority Order

1. **stb_image fix** - Simple, enables linking
2. **TransformedShape API** - Multiple files affected
3. **IShape::local_transform()** - Multiple files affected
4. **CharacterController/Impl** - Needed for physics completion
5. **ContactManifold::normal** - Single file
6. **SphereShape::center()** - Single file
7. **CylinderShape** - Can defer if not critical
8. **Result error type** - Single line fix
9. **set_local_transform** - Single line fix

---

## Files to Modify

| File | Changes Needed |
|------|----------------|
| `include/void_engine/physics/types.hpp` | Add TransformedShape reference support, ContactManifold::normal |
| `include/void_engine/physics/shape.hpp` | Add IShape::local_transform(), SphereShape::center(), (CylinderShape) |
| `include/void_engine/physics/world.hpp` | Add CharacterController::m_impl |
| `include/void_engine/physics/query.hpp` | Fix TransformedShape usage |
| `include/void_engine/physics/snapshot.hpp` | Fix set_local_transform call, Result error type, CylinderShape |
| `src/physics/world.cpp` | Define CharacterControllerImpl |
| `src/asset/stb_impl.cpp` | Create new file for STB_IMAGE_IMPLEMENTATION |
| `src/asset/CMakeLists.txt` | Add stb_impl.cpp |

---

## Notes

- This is a **MIGRATION** - preserve all existing functionality
- Do NOT create stubs - implement FULL working code
- When fixing API mismatches, the .cpp files are often "source of truth"
- Multi-backend physics system (Jolt, PhysX, Bullet support planned)
