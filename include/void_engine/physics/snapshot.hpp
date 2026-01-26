/// @file snapshot.hpp
/// @brief Hot-reload snapshot system for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "body.hpp"
#include "shape.hpp"
#include "world.hpp"

#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/version.hpp>
#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace void_physics {

// =============================================================================
// Binary Serialization Helpers
// =============================================================================

/// Binary writer for snapshot serialization
class BinaryWriter {
public:
    /// Write primitive type
    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        const auto* ptr = reinterpret_cast<const std::uint8_t*>(&value);
        m_data.insert(m_data.end(), ptr, ptr + sizeof(T));
    }

    /// Write vector
    void write(const void_math::Vec3& v) {
        write(v.x);
        write(v.y);
        write(v.z);
    }

    /// Write quaternion
    void write(const void_math::Quat& q) {
        write(q.x);
        write(q.y);
        write(q.z);
        write(q.w);
    }

    /// Write string
    void write(const std::string& s) {
        write(static_cast<std::uint32_t>(s.size()));
        m_data.insert(m_data.end(), s.begin(), s.end());
    }

    /// Write raw bytes
    void write_bytes(const void* data, std::size_t size) {
        const auto* ptr = static_cast<const std::uint8_t*>(data);
        m_data.insert(m_data.end(), ptr, ptr + size);
    }

    /// Get data
    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return m_data; }

    /// Move data out
    [[nodiscard]] std::vector<std::uint8_t> take_data() { return std::move(m_data); }

    /// Get current size
    [[nodiscard]] std::size_t size() const { return m_data.size(); }

private:
    std::vector<std::uint8_t> m_data;
};

/// Binary reader for snapshot deserialization
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& data)
        : m_data(data)
        , m_pos(0)
    {}

    /// Read primitive type
    template<typename T>
    [[nodiscard]] T read() {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        if (m_pos + sizeof(T) > m_data.size()) {
            return T{};
        }
        T value;
        std::memcpy(&value, m_data.data() + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return value;
    }

    /// Read vector
    [[nodiscard]] void_math::Vec3 read_vec3() {
        return void_math::Vec3{read<float>(), read<float>(), read<float>()};
    }

    /// Read quaternion
    [[nodiscard]] void_math::Quat read_quat() {
        return void_math::Quat{read<float>(), read<float>(), read<float>(), read<float>()};
    }

    /// Read string
    [[nodiscard]] std::string read_string() {
        auto len = read<std::uint32_t>();
        if (m_pos + len > m_data.size()) {
            return "";
        }
        std::string s(reinterpret_cast<const char*>(m_data.data() + m_pos), len);
        m_pos += len;
        return s;
    }

    /// Read raw bytes
    void read_bytes(void* dest, std::size_t size) {
        if (m_pos + size > m_data.size()) {
            std::memset(dest, 0, size);
            return;
        }
        std::memcpy(dest, m_data.data() + m_pos, size);
        m_pos += size;
    }

    /// Check if at end
    [[nodiscard]] bool at_end() const { return m_pos >= m_data.size(); }

    /// Get remaining size
    [[nodiscard]] std::size_t remaining() const {
        return m_pos < m_data.size() ? m_data.size() - m_pos : 0;
    }

    /// Get current position
    [[nodiscard]] std::size_t position() const { return m_pos; }

private:
    const std::vector<std::uint8_t>& m_data;
    std::size_t m_pos;
};

// =============================================================================
// Body Snapshot
// =============================================================================

/// Snapshot of a single rigidbody
struct BodySnapshot {
    BodyId id;
    BodyType type;
    std::string name;

    // Transform
    void_math::Vec3 position;
    void_math::Quat rotation;

    // Velocity
    void_math::Vec3 linear_velocity;
    void_math::Vec3 angular_velocity;

    // Mass
    MassProperties mass_props;

    // Damping
    float linear_damping;
    float angular_damping;

    // Gravity
    float gravity_scale;
    bool gravity_enabled;

    // Collision
    CollisionMask collision_mask;
    CollisionResponse collision_response;

    // CCD
    bool ccd_enabled;

    // Sleep
    ActivationState activation_state;
    bool can_sleep;

    // Constraints
    bool linear_lock[3];
    bool angular_lock[3];
    bool fixed_rotation;

    // User data
    std::uint64_t user_id;

    // State
    bool enabled;

    /// Serialize to writer
    void serialize(BinaryWriter& writer) const {
        writer.write(id.value);
        writer.write(static_cast<std::uint8_t>(type));
        writer.write(name);

        writer.write(position);
        writer.write(rotation);

        writer.write(linear_velocity);
        writer.write(angular_velocity);

        writer.write(mass_props.mass);
        writer.write(mass_props.center_of_mass);
        writer.write(mass_props.inertia_diagonal);
        writer.write(mass_props.inertia_rotation);

        writer.write(linear_damping);
        writer.write(angular_damping);

        writer.write(gravity_scale);
        writer.write(gravity_enabled);

        writer.write(collision_mask.layer);
        writer.write(collision_mask.collides_with);
        writer.write(static_cast<std::uint8_t>(collision_response));

        writer.write(ccd_enabled);

        writer.write(static_cast<std::uint8_t>(activation_state));
        writer.write(can_sleep);

        for (int i = 0; i < 3; ++i) writer.write(linear_lock[i]);
        for (int i = 0; i < 3; ++i) writer.write(angular_lock[i]);
        writer.write(fixed_rotation);

        writer.write(user_id);
        writer.write(enabled);
    }

    /// Deserialize from reader
    static BodySnapshot deserialize(BinaryReader& reader) {
        BodySnapshot snap;

        snap.id.value = reader.read<std::uint64_t>();
        snap.type = static_cast<BodyType>(reader.read<std::uint8_t>());
        snap.name = reader.read_string();

        snap.position = reader.read_vec3();
        snap.rotation = reader.read_quat();

        snap.linear_velocity = reader.read_vec3();
        snap.angular_velocity = reader.read_vec3();

        snap.mass_props.mass = reader.read<float>();
        snap.mass_props.center_of_mass = reader.read_vec3();
        snap.mass_props.inertia_diagonal = reader.read_vec3();
        snap.mass_props.inertia_rotation = reader.read_quat();

        snap.linear_damping = reader.read<float>();
        snap.angular_damping = reader.read<float>();

        snap.gravity_scale = reader.read<float>();
        snap.gravity_enabled = reader.read<bool>();

        snap.collision_mask.layer = reader.read<std::uint32_t>();
        snap.collision_mask.collides_with = reader.read<std::uint32_t>();
        snap.collision_response = static_cast<CollisionResponse>(reader.read<std::uint8_t>());

        snap.ccd_enabled = reader.read<bool>();

        snap.activation_state = static_cast<ActivationState>(reader.read<std::uint8_t>());
        snap.can_sleep = reader.read<bool>();

        for (int i = 0; i < 3; ++i) snap.linear_lock[i] = reader.read<bool>();
        for (int i = 0; i < 3; ++i) snap.angular_lock[i] = reader.read<bool>();
        snap.fixed_rotation = reader.read<bool>();

        snap.user_id = reader.read<std::uint64_t>();
        snap.enabled = reader.read<bool>();

        return snap;
    }

    /// Capture from body
    static BodySnapshot capture(const IRigidbody& body) {
        BodySnapshot snap;

        snap.id = body.id();
        snap.type = body.type();
        snap.name = body.name();

        snap.position = body.position();
        snap.rotation = body.rotation();

        snap.linear_velocity = body.linear_velocity();
        snap.angular_velocity = body.angular_velocity();

        snap.mass_props = body.mass_properties();

        snap.linear_damping = body.linear_damping();
        snap.angular_damping = body.angular_damping();

        snap.gravity_scale = body.gravity_scale();
        snap.gravity_enabled = body.gravity_enabled();

        snap.collision_mask = body.collision_mask();
        snap.collision_response = body.collision_response();

        snap.ccd_enabled = body.continuous_detection();

        snap.activation_state = body.activation_state();
        snap.can_sleep = body.can_sleep();

        snap.fixed_rotation = body.fixed_rotation();

        snap.user_id = body.user_id();
        snap.enabled = body.is_enabled();

        return snap;
    }

    /// Restore to body
    void restore_to(IRigidbody& body) const {
        body.set_position(position);
        body.set_rotation(rotation);

        body.set_linear_velocity(linear_velocity);
        body.set_angular_velocity(angular_velocity);

        body.set_mass_properties(mass_props);

        body.set_linear_damping(linear_damping);
        body.set_angular_damping(angular_damping);

        body.set_gravity_scale(gravity_scale);
        body.set_gravity_enabled(gravity_enabled);

        body.set_collision_mask(collision_mask);
        body.set_collision_response(collision_response);

        body.set_continuous_detection(ccd_enabled);

        body.set_activation_state(activation_state);
        body.set_can_sleep(can_sleep);

        body.set_fixed_rotation(fixed_rotation);

        body.set_user_id(user_id);
        body.set_enabled(enabled);
    }
};

// =============================================================================
// Shape Snapshot
// =============================================================================

/// Snapshot of a shape
struct ShapeSnapshot {
    ShapeId id;
    ShapeType type;

    // Shape-specific data
    void_math::Vec3 half_extents;  // Box
    float radius;                   // Sphere, Capsule
    float height;                   // Capsule, Cylinder
    void_math::Vec3 normal;         // Plane
    float distance;                 // Plane

    // Material
    MaterialId material;

    // Local transform
    void_math::Vec3 local_position;
    void_math::Quat local_rotation;

    /// Serialize to writer
    void serialize(BinaryWriter& writer) const {
        writer.write(id.value);
        writer.write(static_cast<std::uint8_t>(type));

        writer.write(half_extents);
        writer.write(radius);
        writer.write(height);
        writer.write(normal);
        writer.write(distance);

        writer.write(material.value);

        writer.write(local_position);
        writer.write(local_rotation);
    }

    /// Deserialize from reader
    static ShapeSnapshot deserialize(BinaryReader& reader) {
        ShapeSnapshot snap;

        snap.id.value = reader.read<std::uint64_t>();
        snap.type = static_cast<ShapeType>(reader.read<std::uint8_t>());

        snap.half_extents = reader.read_vec3();
        snap.radius = reader.read<float>();
        snap.height = reader.read<float>();
        snap.normal = reader.read_vec3();
        snap.distance = reader.read<float>();

        snap.material.value = reader.read<std::uint64_t>();

        snap.local_position = reader.read_vec3();
        snap.local_rotation = reader.read_quat();

        return snap;
    }

    /// Capture from shape
    static ShapeSnapshot capture(const IShape& shape, ShapeId id) {
        ShapeSnapshot snap;
        snap.id = id;
        snap.type = shape.type();
        snap.material = shape.material();
        snap.local_position = shape.local_transform().position;
        snap.local_rotation = shape.local_transform().rotation;

        switch (shape.type()) {
            case ShapeType::Box:
                snap.half_extents = static_cast<const BoxShape&>(shape).half_extents();
                break;
            case ShapeType::Sphere:
                snap.radius = static_cast<const SphereShape&>(shape).radius();
                break;
            case ShapeType::Capsule: {
                const auto& capsule = static_cast<const CapsuleShape&>(shape);
                snap.radius = capsule.radius();
                snap.height = capsule.half_height() * 2.0f;
                break;
            }
            case ShapeType::Cylinder: {
                const auto& cyl = static_cast<const CylinderShape&>(shape);
                snap.radius = cyl.radius();
                snap.height = cyl.half_height() * 2.0f;
                break;
            }
            case ShapeType::Plane: {
                const auto& plane = static_cast<const PlaneShape&>(shape);
                snap.normal = plane.normal();
                snap.distance = plane.distance();
                break;
            }
            default:
                break;
        }

        return snap;
    }

    /// Create shape from snapshot
    [[nodiscard]] std::unique_ptr<IShape> create_shape() const {
        std::unique_ptr<IShape> shape;

        switch (type) {
            case ShapeType::Box:
                shape = std::make_unique<BoxShape>(half_extents);
                break;
            case ShapeType::Sphere:
                shape = std::make_unique<SphereShape>(radius);
                break;
            case ShapeType::Capsule:
                shape = std::make_unique<CapsuleShape>(radius, height);
                break;
            case ShapeType::Cylinder:
                shape = std::make_unique<CylinderShape>(radius, height);
                break;
            case ShapeType::Plane:
                shape = std::make_unique<PlaneShape>(normal, distance);
                break;
            default:
                return nullptr;
        }

        if (shape) {
            shape->set_material(material);
            void_math::Transform t;
            t.position = local_position;
            t.rotation = local_rotation;
            shape->set_local_transform(t);
        }

        return shape;
    }
};

// =============================================================================
// Joint Snapshot
// =============================================================================

/// Snapshot of a joint
struct JointSnapshot {
    JointId id;
    JointType type;
    std::string name;

    BodyId body_a;
    BodyId body_b;

    void_math::Vec3 anchor_a;
    void_math::Vec3 anchor_b;

    bool collision_enabled;
    float break_force;
    float break_torque;

    // Type-specific data
    void_math::Vec3 axis;
    bool use_limits;
    float lower_limit;
    float upper_limit;
    bool use_motor;
    float motor_speed;
    float max_motor_force;
    bool use_spring;
    float spring_stiffness;
    float spring_damping;
    float rest_length;
    float min_distance;
    float max_distance;

    /// Serialize to writer
    void serialize(BinaryWriter& writer) const {
        writer.write(id.value);
        writer.write(static_cast<std::uint8_t>(type));
        writer.write(name);

        writer.write(body_a.value);
        writer.write(body_b.value);

        writer.write(anchor_a);
        writer.write(anchor_b);

        writer.write(collision_enabled);
        writer.write(break_force);
        writer.write(break_torque);

        writer.write(axis);
        writer.write(use_limits);
        writer.write(lower_limit);
        writer.write(upper_limit);
        writer.write(use_motor);
        writer.write(motor_speed);
        writer.write(max_motor_force);
        writer.write(use_spring);
        writer.write(spring_stiffness);
        writer.write(spring_damping);
        writer.write(rest_length);
        writer.write(min_distance);
        writer.write(max_distance);
    }

    /// Deserialize from reader
    static JointSnapshot deserialize(BinaryReader& reader) {
        JointSnapshot snap;

        snap.id.value = reader.read<std::uint64_t>();
        snap.type = static_cast<JointType>(reader.read<std::uint8_t>());
        snap.name = reader.read_string();

        snap.body_a.value = reader.read<std::uint64_t>();
        snap.body_b.value = reader.read<std::uint64_t>();

        snap.anchor_a = reader.read_vec3();
        snap.anchor_b = reader.read_vec3();

        snap.collision_enabled = reader.read<bool>();
        snap.break_force = reader.read<float>();
        snap.break_torque = reader.read<float>();

        snap.axis = reader.read_vec3();
        snap.use_limits = reader.read<bool>();
        snap.lower_limit = reader.read<float>();
        snap.upper_limit = reader.read<float>();
        snap.use_motor = reader.read<bool>();
        snap.motor_speed = reader.read<float>();
        snap.max_motor_force = reader.read<float>();
        snap.use_spring = reader.read<bool>();
        snap.spring_stiffness = reader.read<float>();
        snap.spring_damping = reader.read<float>();
        snap.rest_length = reader.read<float>();
        snap.min_distance = reader.read<float>();
        snap.max_distance = reader.read<float>();

        return snap;
    }
};

// =============================================================================
// Material Snapshot
// =============================================================================

/// Snapshot of a physics material
struct MaterialSnapshot {
    MaterialId id;
    PhysicsMaterialData data;

    void serialize(BinaryWriter& writer) const {
        writer.write(id.value);
        writer.write(data.static_friction);
        writer.write(data.dynamic_friction);
        writer.write(data.restitution);
        writer.write(data.density);
        writer.write(static_cast<std::uint8_t>(data.friction_combine));
        writer.write(static_cast<std::uint8_t>(data.restitution_combine));
    }

    static MaterialSnapshot deserialize(BinaryReader& reader) {
        MaterialSnapshot snap;
        snap.id.value = reader.read<std::uint64_t>();
        snap.data.static_friction = reader.read<float>();
        snap.data.dynamic_friction = reader.read<float>();
        snap.data.restitution = reader.read<float>();
        snap.data.density = reader.read<float>();
        snap.data.friction_combine = static_cast<PhysicsMaterialData::CombineMode>(reader.read<std::uint8_t>());
        snap.data.restitution_combine = static_cast<PhysicsMaterialData::CombineMode>(reader.read<std::uint8_t>());
        return snap;
    }
};

// =============================================================================
// Physics World Snapshot
// =============================================================================

/// Complete snapshot of physics world state
struct PhysicsWorldSnapshot {
    static constexpr std::uint32_t k_magic = 0x50485953; // "PHYS"
    static constexpr std::uint32_t k_version = 1;

    PhysicsConfig config;
    std::vector<BodySnapshot> bodies;
    std::vector<std::pair<BodyId, std::vector<ShapeSnapshot>>> body_shapes;
    std::vector<JointSnapshot> joints;
    std::vector<MaterialSnapshot> materials;
    MaterialId default_material;

    std::uint64_t next_body_id;
    std::uint64_t next_joint_id;
    std::uint64_t next_material_id;

    float time_accumulator;

    /// Serialize to binary data
    [[nodiscard]] std::vector<std::uint8_t> serialize() const {
        BinaryWriter writer;

        // Header
        writer.write(k_magic);
        writer.write(k_version);

        // Config
        writer.write(static_cast<std::uint8_t>(config.backend));
        writer.write(config.gravity);
        writer.write(config.max_substeps);
        writer.write(config.fixed_timestep);
        writer.write(config.velocity_iterations);
        writer.write(config.position_iterations);

        // Bodies
        writer.write(static_cast<std::uint32_t>(bodies.size()));
        for (const auto& body : bodies) {
            body.serialize(writer);
        }

        // Shapes per body
        writer.write(static_cast<std::uint32_t>(body_shapes.size()));
        for (const auto& [body_id, shapes] : body_shapes) {
            writer.write(body_id.value);
            writer.write(static_cast<std::uint32_t>(shapes.size()));
            for (const auto& shape : shapes) {
                shape.serialize(writer);
            }
        }

        // Joints
        writer.write(static_cast<std::uint32_t>(joints.size()));
        for (const auto& joint : joints) {
            joint.serialize(writer);
        }

        // Materials
        writer.write(static_cast<std::uint32_t>(materials.size()));
        for (const auto& mat : materials) {
            mat.serialize(writer);
        }

        // IDs
        writer.write(default_material.value);
        writer.write(next_body_id);
        writer.write(next_joint_id);
        writer.write(next_material_id);
        writer.write(time_accumulator);

        return writer.take_data();
    }

    /// Deserialize from binary data
    [[nodiscard]] static std::optional<PhysicsWorldSnapshot> deserialize(const std::vector<std::uint8_t>& data) {
        if (data.size() < 8) return std::nullopt;

        BinaryReader reader(data);

        // Header
        auto magic = reader.read<std::uint32_t>();
        auto version = reader.read<std::uint32_t>();

        if (magic != k_magic) return std::nullopt;
        if (version != k_version) return std::nullopt;

        PhysicsWorldSnapshot snap;

        // Config
        snap.config.backend = static_cast<PhysicsBackend>(reader.read<std::uint8_t>());
        snap.config.gravity = reader.read_vec3();
        snap.config.max_substeps = reader.read<std::uint32_t>();
        snap.config.fixed_timestep = reader.read<float>();
        snap.config.velocity_iterations = reader.read<std::uint32_t>();
        snap.config.position_iterations = reader.read<std::uint32_t>();

        // Bodies
        auto body_count = reader.read<std::uint32_t>();
        snap.bodies.reserve(body_count);
        for (std::uint32_t i = 0; i < body_count; ++i) {
            snap.bodies.push_back(BodySnapshot::deserialize(reader));
        }

        // Shapes
        auto shape_group_count = reader.read<std::uint32_t>();
        snap.body_shapes.reserve(shape_group_count);
        for (std::uint32_t i = 0; i < shape_group_count; ++i) {
            BodyId body_id{reader.read<std::uint64_t>()};
            auto shape_count = reader.read<std::uint32_t>();
            std::vector<ShapeSnapshot> shapes;
            shapes.reserve(shape_count);
            for (std::uint32_t j = 0; j < shape_count; ++j) {
                shapes.push_back(ShapeSnapshot::deserialize(reader));
            }
            snap.body_shapes.emplace_back(body_id, std::move(shapes));
        }

        // Joints
        auto joint_count = reader.read<std::uint32_t>();
        snap.joints.reserve(joint_count);
        for (std::uint32_t i = 0; i < joint_count; ++i) {
            snap.joints.push_back(JointSnapshot::deserialize(reader));
        }

        // Materials
        auto mat_count = reader.read<std::uint32_t>();
        snap.materials.reserve(mat_count);
        for (std::uint32_t i = 0; i < mat_count; ++i) {
            snap.materials.push_back(MaterialSnapshot::deserialize(reader));
        }

        // IDs
        snap.default_material.value = reader.read<std::uint64_t>();
        snap.next_body_id = reader.read<std::uint64_t>();
        snap.next_joint_id = reader.read<std::uint64_t>();
        snap.next_material_id = reader.read<std::uint64_t>();
        snap.time_accumulator = reader.read<float>();

        return snap;
    }
};

// =============================================================================
// Physics World Hot-Reloadable Wrapper
// =============================================================================

/// Makes PhysicsWorld hot-reloadable
class HotReloadablePhysicsWorld : public void_core::HotReloadable {
public:
    explicit HotReloadablePhysicsWorld(std::unique_ptr<IPhysicsWorld> world)
        : m_world(std::move(world))
    {}

    /// Get underlying world
    [[nodiscard]] IPhysicsWorld& world() { return *m_world; }
    [[nodiscard]] const IPhysicsWorld& world() const { return *m_world; }

    /// Capture current state
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        auto snap_result = m_world->snapshot();
        if (!snap_result) {
            return void_core::Err<void_core::HotReloadSnapshot>(snap_result.error());
        }
        return void_core::Ok(std::move(snap_result).value());
    }

    /// Restore state
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override {
        return m_world->restore(std::move(snapshot));
    }

    /// Check version compatibility
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override {
        // Compatible if major version matches
        return new_version.major == current_version().major;
    }

    /// Get current version
    [[nodiscard]] void_core::Version current_version() const override {
        return void_core::Version{1, 0, 0};
    }

    /// Get type name
    [[nodiscard]] std::string type_name() const override {
        return "void_physics::PhysicsWorld";
    }

private:
    std::unique_ptr<IPhysicsWorld> m_world;
};

} // namespace void_physics
