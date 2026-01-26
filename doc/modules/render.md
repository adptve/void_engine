# Module: render

## Overview
- **Location**: `include/void_engine/render/` and `src/render/`
- **Status**: CRITICAL ISSUES
- **Grade**: D

## File Inventory

### Headers
| File | Purpose |
|------|---------|
| `fwd.hpp` | Forward declarations |
| `pass.hpp` | Render pass definitions |
| `render_graph.hpp` | RenderGraph, LayerManager, Compositor, View, RenderQueue |
| `renderer.hpp` | IRenderer interface |
| `resource.hpp` | ResourceId, texture/buffer resources |

### Implementations
| File | Purpose |
|------|---------|
| `gl_renderer.cpp` | OpenGL renderer implementation |
| `pass.cpp` | Render pass implementations |
| `render_graph.cpp` | RenderGraph implementation |
| `resource.cpp` | Resource management |
| `stub.cpp` | Stub/fallback implementations |

## Issues Found

### CRITICAL: Duplicate Implementations (ODR Violation)
**Severity**: Critical
**Files**: `stub.cpp` and `gl_renderer.cpp`

Both files define implementations for the same `IRenderer` methods, which violates the One Definition Rule (ODR). This will cause linker errors or undefined behavior.

**Affected Functions**:
- `IRenderer::init()`
- `IRenderer::shutdown()`
- `IRenderer::begin_frame()`
- `IRenderer::end_frame()`
- `IRenderer::present()`

**Recommendation**: Remove duplicates from `stub.cpp` or use conditional compilation to ensure only one implementation is linked.

### CRITICAL: Missing InstanceData Implementations
**Severity**: Critical
**Location**: `renderer.hpp` declares `InstanceData` struct with methods

**Missing Implementations** (10 functions):
1. `InstanceData::InstanceData()` - constructor
2. `InstanceData::~InstanceData()` - destructor
3. `InstanceData::reserve()`
4. `InstanceData::clear()`
5. `InstanceData::add_instance()`
6. `InstanceData::update_instance()`
7. `InstanceData::remove_instance()`
8. `InstanceData::instance_count()`
9. `InstanceData::data()`
10. `InstanceData::stride()`

**Recommendation**: Either implement these methods in a .cpp file or mark them as inline/header-only.

## Consistency Matrix

| Component | Header | Implementation | Status |
|-----------|--------|----------------|--------|
| RenderGraph | render_graph.hpp | render_graph.cpp | OK |
| RenderPass | pass.hpp | pass.cpp | OK |
| RenderLayer | render_graph.hpp | render_graph.cpp | OK |
| LayerManager | render_graph.hpp | render_graph.cpp | OK |
| View | render_graph.hpp | render_graph.cpp | OK |
| Compositor | render_graph.hpp | render_graph.cpp | OK |
| RenderQueue | render_graph.hpp | render_graph.cpp | OK |
| IRenderer | renderer.hpp | gl_renderer.cpp + stub.cpp | DUPLICATE |
| InstanceData | renderer.hpp | MISSING | MISSING |
| ResourceId | resource.hpp | resource.cpp | OK |

## Action Items

1. [ ] **URGENT**: Resolve ODR violation between stub.cpp and gl_renderer.cpp
2. [ ] **URGENT**: Implement InstanceData methods or convert to header-only
3. [ ] Consider using factory pattern for renderer selection instead of dual implementations
