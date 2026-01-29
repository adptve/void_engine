/// @file components.cpp
/// @brief ECS render components implementation

#include <void_engine/render/components.hpp>
#include <void_engine/ecs/world.hpp>

#include <cmath>

namespace void_render {

// =============================================================================
// TransformComponent Implementation
// =============================================================================

std::array<float, 16> TransformComponent::local_matrix() const noexcept {
    // Quaternion to rotation matrix
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    float sx = scale[0], sy = scale[1], sz = scale[2];
    float tx = position[0], ty = position[1], tz = position[2];

    return {{
        (1 - (yy + zz)) * sx,  (xy + wz) * sx,         (xz - wy) * sx,         0,
        (xy - wz) * sy,         (1 - (xx + zz)) * sy,  (yz + wx) * sy,         0,
        (xz + wy) * sz,         (yz - wx) * sz,         (1 - (xx + yy)) * sz,  0,
        tx,                      ty,                      tz,                      1
    }};
}

void TransformComponent::set_rotation_euler(float pitch, float yaw, float roll) noexcept {
    // Convert degrees to radians
    constexpr float deg_to_rad = 3.14159265358979323846f / 180.0f;
    float p = pitch * deg_to_rad * 0.5f;
    float y = yaw * deg_to_rad * 0.5f;
    float r = roll * deg_to_rad * 0.5f;

    float cp = std::cos(p);
    float sp = std::sin(p);
    float cy = std::cos(y);
    float sy = std::sin(y);
    float cr = std::cos(r);
    float sr = std::sin(r);

    // Quaternion from Euler angles (YXZ order)
    rotation[0] = sr * cp * cy - cr * sp * sy;  // x
    rotation[1] = cr * sp * cy + sr * cp * sy;  // y
    rotation[2] = cr * cp * sy - sr * sp * cy;  // z
    rotation[3] = cr * cp * cy + sr * sp * sy;  // w

    dirty = true;
}

// =============================================================================
// Component Registration
// =============================================================================

void register_render_components(void_ecs::World& world) {
    world.register_component<TransformComponent>();
    world.register_component<MeshComponent>();
    world.register_component<MaterialComponent>();
    world.register_component<ModelComponent>();
    world.register_component<LightComponent>();
    world.register_component<CameraComponent>();
    world.register_component<RenderableTag>();
    world.register_component<HierarchyComponent>();
    world.register_component<AnimationComponent>();
}

} // namespace void_render
