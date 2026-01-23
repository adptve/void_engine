/// @file types.cpp
/// @brief Core type implementations for void_physics

#include <void_engine/physics/types.hpp>

namespace void_physics {

// =============================================================================
// String Conversions
// =============================================================================

const char* to_string(PhysicsBackend backend) {
    switch (backend) {
        case PhysicsBackend::Null: return "Null";
        case PhysicsBackend::Jolt: return "Jolt";
        case PhysicsBackend::PhysX: return "PhysX";
        case PhysicsBackend::Bullet: return "Bullet";
        case PhysicsBackend::Custom: return "Custom";
    }
    return "Unknown";
}

const char* to_string(BodyType type) {
    switch (type) {
        case BodyType::Static: return "Static";
        case BodyType::Kinematic: return "Kinematic";
        case BodyType::Dynamic: return "Dynamic";
    }
    return "Unknown";
}

const char* to_string(ActivationState state) {
    switch (state) {
        case ActivationState::Active: return "Active";
        case ActivationState::Sleeping: return "Sleeping";
        case ActivationState::AlwaysActive: return "AlwaysActive";
        case ActivationState::Disabled: return "Disabled";
    }
    return "Unknown";
}

const char* to_string(ShapeType type) {
    switch (type) {
        case ShapeType::Box: return "Box";
        case ShapeType::Sphere: return "Sphere";
        case ShapeType::Capsule: return "Capsule";
        case ShapeType::Cylinder: return "Cylinder";
        case ShapeType::Plane: return "Plane";
        case ShapeType::ConvexHull: return "ConvexHull";
        case ShapeType::TriangleMesh: return "TriangleMesh";
        case ShapeType::Heightfield: return "Heightfield";
        case ShapeType::Compound: return "Compound";
    }
    return "Unknown";
}

const char* to_string(JointType type) {
    switch (type) {
        case JointType::Fixed: return "Fixed";
        case JointType::Hinge: return "Hinge";
        case JointType::Slider: return "Slider";
        case JointType::Ball: return "Ball";
        case JointType::Distance: return "Distance";
        case JointType::Spring: return "Spring";
        case JointType::Cone: return "Cone";
        case JointType::Generic: return "Generic";
    }
    return "Unknown";
}

// =============================================================================
// PhysicsMaterialData Presets
// =============================================================================

PhysicsMaterialData PhysicsMaterialData::ice() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.05f;
    mat.dynamic_friction = 0.02f;
    mat.restitution = 0.1f;
    mat.density = 917.0f; // kg/m^3
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::rubber() {
    PhysicsMaterialData mat;
    mat.static_friction = 1.0f;
    mat.dynamic_friction = 0.8f;
    mat.restitution = 0.8f;
    mat.density = 1100.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::metal() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.6f;
    mat.dynamic_friction = 0.4f;
    mat.restitution = 0.2f;
    mat.density = 7800.0f; // Steel
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::wood() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.5f;
    mat.dynamic_friction = 0.4f;
    mat.restitution = 0.3f;
    mat.density = 700.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::concrete() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.8f;
    mat.dynamic_friction = 0.6f;
    mat.restitution = 0.1f;
    mat.density = 2400.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::glass() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.4f;
    mat.dynamic_friction = 0.3f;
    mat.restitution = 0.4f;
    mat.density = 2500.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::flesh() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.6f;
    mat.dynamic_friction = 0.5f;
    mat.restitution = 0.2f;
    mat.density = 1000.0f; // Similar to water
    return mat;
}

// =============================================================================
// MassProperties
// =============================================================================

MassProperties MassProperties::from_mass(float mass) {
    MassProperties props;
    props.mass = mass;
    // Assume unit box for default inertia
    float i = mass / 6.0f;
    props.inertia_diagonal = {i, i, i};
    return props;
}

MassProperties MassProperties::infinite() {
    MassProperties props;
    props.mass = std::numeric_limits<float>::max();
    props.inertia_diagonal = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    return props;
}

// =============================================================================
// BodyConfig Factory Methods
// =============================================================================

BodyConfig BodyConfig::make_static(const void_math::Vec3& pos) {
    BodyConfig config;
    config.type = BodyType::Static;
    config.position = pos;
    config.mass = MassProperties::infinite();
    config.allow_sleep = false;
    return config;
}

BodyConfig BodyConfig::make_kinematic(const void_math::Vec3& pos) {
    BodyConfig config;
    config.type = BodyType::Kinematic;
    config.position = pos;
    config.mass = MassProperties::infinite();
    return config;
}

BodyConfig BodyConfig::make_dynamic(const void_math::Vec3& pos, float mass) {
    BodyConfig config;
    config.type = BodyType::Dynamic;
    config.position = pos;
    config.mass = MassProperties::from_mass(mass);
    return config;
}

// =============================================================================
// PhysicsConfig Presets
// =============================================================================

PhysicsConfig PhysicsConfig::defaults() {
    return PhysicsConfig{};
}

PhysicsConfig PhysicsConfig::high_fidelity() {
    PhysicsConfig config;
    config.fixed_timestep = 1.0f / 120.0f;
    config.max_substeps = 8;
    config.velocity_iterations = 16;
    config.position_iterations = 6;
    config.enable_ccd = true;
    config.ccd_motion_threshold = 0.05f;
    return config;
}

PhysicsConfig PhysicsConfig::performance() {
    PhysicsConfig config;
    config.fixed_timestep = 1.0f / 30.0f;
    config.max_substeps = 2;
    config.velocity_iterations = 4;
    config.position_iterations = 2;
    config.enable_ccd = false;
    return config;
}

} // namespace void_physics
