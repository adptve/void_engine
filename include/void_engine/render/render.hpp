#pragma once

/// @file render.hpp
/// @brief Main include file for void_render module
///
/// This header includes all void_render subsystems:
/// - Resource types (textures, buffers, samplers)
/// - Mesh and geometry system
/// - GPU instancing and batching
/// - Material system (PBR)
/// - Lighting system (directional, point, spot)
/// - Shadow mapping (cascaded, atlas)
/// - Camera system (perspective, orthographic, controllers)
/// - Render pass system
/// - Compositor and layer management
/// - Spatial acceleration (BVH, picking)
/// - Debug visualization and statistics

#include "fwd.hpp"
#include "resource.hpp"
#include "mesh.hpp"
#include "instancing.hpp"
#include "material.hpp"
#include "light.hpp"
#include "shadow.hpp"
#include "camera.hpp"
#include "pass.hpp"
#include "compositor.hpp"
#include "spatial.hpp"
#include "debug.hpp"

namespace void_render {

/// void_render version
struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 1;
    static constexpr int PATCH = 0;

    [[nodiscard]] static constexpr const char* string() noexcept {
        return "0.1.0";
    }
};

} // namespace void_render
