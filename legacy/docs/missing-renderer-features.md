# Missing Renderer Features — User Stories & Requirements

This document lists **only the features your system is missing** relative to three.js (and indirectly Unreal), expressed as **user stories with concrete requirements**.

It is intentionally scoped to *what you need to add*, not what already exists.

---

## 1. Scene Graph (Hierarchical Entities)

### User Story
As a scene author, I want entities to be parented to other entities so that transforms, visibility, and motion propagate hierarchically.

### Requirements
- Entities may optionally reference a `parent` entity
- Local transforms are evaluated relative to the parent
- World transforms are derived automatically
- Visibility propagates from parent to children
- Support arbitrary depth (not limited to one level)
- Cycles must be prevented at validation time

---

## 2. Camera System

### User Story
As a scene author, I want to define and control cameras as first-class scene entities.

### Requirements
- Introduce `camera` entities
- Support camera types:
  - Perspective
  - Orthographic
- Configurable properties:
  - FOV (perspective)
  - Near / far clip planes
  - Aspect ratio override (optional)
- Support multiple cameras per scene
- Allow runtime camera switching
- Allow camera animation (position, rotation, FOV)
- Allow cameras to be parented to entities

---

## 3. Mesh Import (External Geometry)

### User Story
As a scene author, I want to load external mesh assets instead of being limited to primitives.

### Requirements
- Support mesh loading from files (minimum: glTF)
- Allow entities to reference mesh assets by path
- Support vertex attributes:
  - Position
  - Normal
  - UVs
  - Tangents (optional)
- Allow reuse of mesh assets across entities
- Validate mesh compatibility at load time

---

## 4. Instancing & Repetition

### User Story
As a scene author, I want to efficiently render many copies of the same mesh.

### Requirements
- Support instanced entities referencing a single mesh
- Per-instance transform overrides
- Optional per-instance material overrides
- GPU instancing where supported
- Graceful fallback when instancing is unavailable

---

## 5. Lighting Entities (Multiple Lights)

### User Story
As a scene author, I want to place multiple lights in my scene to shape lighting locally.

### Requirements
- Introduce `light` entities
- Supported light types:
  - Directional
  - Point
  - Spot
- Per-light properties:
  - Color
  - Intensity
  - Range (where applicable)
  - Direction / cone angles
- Lights may be parented to entities
- Support multiple active lights per frame
- Define hard limits via resource configuration

---

## 6. Shadow Mapping

### User Story
As a scene author, I want objects to cast and receive shadows from lights.

### Requirements
- Enable shadow casting per light
- Enable shadow receiving per entity
- Configurable shadow resolution
- Support directional and spot light shadows
- Allow shadows to be globally enabled/disabled
- Reasonable defaults for quality vs performance

---

## 7. Advanced Material Features

### User Story
As a scene author, I want materials to represent a wider range of real-world surfaces.

### Requirements
- Add optional material parameters:
  - Clearcoat
  - Clearcoat roughness
  - Transmission (glass)
  - Opacity / alpha mode
- Support alpha modes:
  - Opaque
  - Masked
  - Blended
- Allow per-entity material overrides
- Maintain backward compatibility with existing PBR schema

---

## 8. Keyframe Animation System

### User Story
As a scene author, I want to play authored animations instead of only procedural motion.

### Requirements
- Support animation clips
- Support keyframe tracks for:
  - Translation
  - Rotation
  - Scale
- Allow multiple clips per entity
- Support looping and one-shot playback
- Support animation time scaling
- Allow animation binding to imported meshes

---

## 9. Animation Blending

### User Story
As a scene author, I want to blend between animations smoothly.

### Requirements
- Support blending between two or more animation clips
- Configurable blend durations
- Support additive animation layers
- Deterministic blending behavior

---

## 10. Object Picking & Raycasting

### User Story
As an application author, I want to interact with scene objects using mouse or touch.

### Requirements
- Raycast from screen space into the scene
- Detect entity intersections
- Support hit filtering by layer or tag
- Return hit position, normal, and entity ID
- Allow entities to opt in/out of picking

---

## 11. Entity-Level Input Events

### User Story
As an application author, I want entities to respond directly to user input.

### Requirements
- Define per-entity input handlers:
  - Hover
  - Click
  - Press
  - Release
- Support mouse and touch input
- Allow event bubbling through parent hierarchy
- Allow exclusive input capture

---

## 12. Multiple Render Passes per Entity

### User Story
As a renderer author, I want entities to participate in more than one render pass.

### Requirements
- Allow entities to render in:
  - Main pass
  - Depth-only pass
  - Shadow pass
- Explicit pass participation flags
- Stable ordering guarantees

---

## 13. Custom Render Passes

### User Story
As an advanced user, I want to inject custom render passes into the pipeline.

### Requirements
- Define named render passes
- Control pass execution order
- Allow passes to read previous pass outputs
- Allow passes to write render targets
- Enforce resource limits per pass

---

## 14. Spatial Queries & Bounds

### User Story
As an engine/system author, I want spatial data for culling and interaction.

### Requirements
- Compute bounding volumes per entity
- Support:
  - Bounding box
  - Bounding sphere
- Expose bounds for:
  - Frustum culling
  - Raycasting
- Update bounds when transforms change

---

## 15. Level of Detail (LOD)

### User Story
As a scene author, I want objects to change detail based on distance.

### Requirements
- Allow multiple LOD meshes per entity
- Distance-based switching
- Optional hysteresis to avoid popping
- Graceful fallback when LODs are missing

---

## 16. Scene Streaming (Foundational)

### User Story
As an application author, I want to load and unload scene content dynamically.

### Requirements
- Allow scenes to be split into chunks
- Load/unload entities at runtime
- Preserve entity IDs across loads
- Explicit control over streaming boundaries
- No implicit threading assumptions

---

## 17. World-Space Precision Management

### User Story
As a large-scene author, I want stable precision over large distances.

### Requirements
- Support origin rebasing
- Maintain camera-relative rendering
- Avoid precision loss in shaders
- Keep transforms numerically stable

---

## 18. Debug & Introspection Hooks

### User Story
As a developer, I want visibility into what the renderer is doing.

### Requirements
- Expose entity counts per frame
- Expose draw call counts
- Expose GPU memory usage estimates
- Allow debug visualization toggles:
  - Bounds
  - Normals
  - Light volumes

---

## Closing Notes

This list represents the **minimum feature surface required to reach functional parity with three.js for real-world scenes**, while remaining far simpler than Unreal.

Nothing here assumes:
- Physics
- AI
- Networking
- Gameplay systems

It is strictly a **rendering + interaction engine evolution path**.
