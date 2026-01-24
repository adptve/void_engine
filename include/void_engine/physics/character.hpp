/// @file character.hpp
/// @brief Character controller for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "shape.hpp"
#include "world.hpp"
#include "query.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>

#include <cmath>
#include <algorithm>

namespace void_physics {

// =============================================================================
// Character Controller State
// =============================================================================

/// Character movement state
enum class CharacterState : std::uint8_t {
    Grounded,       ///< On ground, can walk/run
    Falling,        ///< In air, falling
    Jumping,        ///< In air, going up
    Sliding,        ///< On steep slope
    Swimming,       ///< In water volume
};

/// Character collision flags
struct CharacterCollisionFlags {
    bool below = false;     ///< Collision below (ground)
    bool above = false;     ///< Collision above (ceiling)
    bool sides = false;     ///< Collision to sides (walls)
    bool step = false;      ///< Stepped up obstacle
};

// =============================================================================
// Character Controller Implementation
// =============================================================================

/// Full character controller implementation
class CharacterControllerImpl {
public:
    CharacterControllerImpl(IPhysicsWorld& world, const CharacterControllerConfig& config)
        : m_world(world)
        , m_config(config)
        , m_capsule(config.radius, config.height - 2.0f * config.radius)
    {
        m_query.set_broadphase(&dynamic_cast<PhysicsWorld&>(world).broadphase());
        m_query.set_body_accessor([&world](BodyId id) -> IRigidbody* {
            return world.get_body(id);
        });
    }

    /// Move the character
    void move(const void_math::Vec3& displacement, float dt) {
        m_collision_flags = {};

        // Apply gravity if not grounded
        if (!m_grounded) {
            m_velocity.y += m_config.gravity * dt;
        }

        // Combine input displacement with velocity
        auto total_disp = displacement + m_velocity * dt;

        // Slide move with collision detection
        auto result_pos = slide_move(m_position, total_disp);

        // Step up logic
        if (m_collision_flags.sides && !m_collision_flags.above) {
            auto step_pos = m_position + void_math::Vec3{0, m_config.step_height, 0};
            auto step_result = slide_move(step_pos, total_disp);

            // Check if step got us further
            auto horizontal_disp = void_math::Vec3{total_disp.x, 0, total_disp.z};
            float orig_dist = void_math::length(void_math::Vec3{
                result_pos.x - m_position.x, 0, result_pos.z - m_position.z});
            float step_dist = void_math::length(void_math::Vec3{
                step_result.x - step_pos.x, 0, step_result.z - step_pos.z});

            if (step_dist > orig_dist + 0.01f) {
                // Step up was successful, now step down
                auto down_result = slide_move(step_result, void_math::Vec3{0, -m_config.step_height * 1.5f, 0});

                if (m_collision_flags.below) {
                    result_pos = down_result;
                    m_collision_flags.step = true;
                }
            }
        }

        m_position = result_pos;

        // Update grounded state
        update_grounded();

        // Update velocity based on collision
        if (m_collision_flags.below && m_velocity.y < 0.0f) {
            m_velocity.y = 0.0f;
        }
        if (m_collision_flags.above && m_velocity.y > 0.0f) {
            m_velocity.y = 0.0f;
        }

        // Update state
        if (m_grounded) {
            if (is_slope_too_steep()) {
                m_state = CharacterState::Sliding;
            } else {
                m_state = CharacterState::Grounded;
            }
        } else {
            m_state = m_velocity.y > 0.0f ? CharacterState::Jumping : CharacterState::Falling;
        }
    }

    /// Jump
    void jump() {
        if (m_grounded || m_coyote_time > 0.0f) {
            m_velocity.y = m_config.jump_speed;
            m_grounded = false;
            m_state = CharacterState::Jumping;
            m_coyote_time = 0.0f;
        }
    }

    /// Set velocity directly
    void set_velocity(const void_math::Vec3& vel) { m_velocity = vel; }

    /// Get position
    [[nodiscard]] void_math::Vec3 position() const { return m_position; }

    /// Set position (teleport)
    void set_position(const void_math::Vec3& pos) { m_position = pos; }

    /// Get velocity
    [[nodiscard]] void_math::Vec3 velocity() const { return m_velocity; }

    /// Check if grounded
    [[nodiscard]] bool is_grounded() const { return m_grounded; }

    /// Get ground normal
    [[nodiscard]] void_math::Vec3 ground_normal() const { return m_ground_normal; }

    /// Check collision flags
    [[nodiscard]] bool collides_above() const { return m_collision_flags.above; }
    [[nodiscard]] bool collides_below() const { return m_collision_flags.below; }
    [[nodiscard]] bool collides_sides() const { return m_collision_flags.sides; }
    [[nodiscard]] bool stepped_up() const { return m_collision_flags.step; }

    /// Get current state
    [[nodiscard]] CharacterState state() const { return m_state; }

    /// Get collision flags
    [[nodiscard]] const CharacterCollisionFlags& collision_flags() const { return m_collision_flags; }

    /// Resize the controller
    void resize(float height, float radius) {
        m_config.height = height;
        m_config.radius = radius;
        m_capsule = CapsuleShape(radius, height - 2.0f * radius);
    }

    /// Get configuration
    [[nodiscard]] const CharacterControllerConfig& config() const { return m_config; }

    /// Update coyote time (call each frame)
    void update(float dt) {
        if (m_grounded) {
            m_coyote_time = 0.1f; // Allow jump shortly after leaving ground
        } else {
            m_coyote_time = std::max(0.0f, m_coyote_time - dt);
        }
    }

private:
    /// Slide move with collision response
    void_math::Vec3 slide_move(const void_math::Vec3& start, const void_math::Vec3& displacement) {
        constexpr int max_iterations = 4;
        constexpr float min_move = 0.001f;

        auto pos = start;
        auto remaining = displacement;

        for (int i = 0; i < max_iterations; ++i) {
            float move_len = void_math::length(remaining);
            if (move_len < min_move) break;

            auto dir = remaining / move_len;

            // Shape cast
            void_math::Transform transform;
            transform.position = pos;

            auto hit = m_query.shape_cast(m_capsule, transform, dir, move_len + m_config.skin_width,
                                         QueryFilter::Default, m_config.collision_mask.collides_with);

            if (!hit.hit) {
                // No hit, move full distance
                pos = pos + remaining;
                break;
            }

            // Move to just before hit
            float safe_dist = std::max(0.0f, hit.distance - m_config.skin_width);
            pos = pos + dir * safe_dist;

            // Update collision flags based on normal
            update_collision_flags(hit.normal);

            // Compute slide direction
            float remaining_dist = move_len - safe_dist;
            if (remaining_dist < min_move) break;

            // Project remaining velocity onto surface
            auto into = dir * remaining_dist;
            float dot = void_math::dot(into, hit.normal);
            remaining = into - hit.normal * dot;

            // Prevent moving into surface
            if (void_math::dot(remaining, hit.normal) < 0.0f) {
                remaining = remaining - hit.normal * void_math::dot(remaining, hit.normal);
            }
        }

        return pos;
    }

    /// Update collision flags based on hit normal
    void update_collision_flags(const void_math::Vec3& normal) {
        float vertical = void_math::dot(normal, void_math::Vec3{0, 1, 0});

        if (vertical > 0.7f) {
            m_collision_flags.below = true;
            m_ground_normal = normal;
        } else if (vertical < -0.7f) {
            m_collision_flags.above = true;
        } else {
            m_collision_flags.sides = true;
        }
    }

    /// Update grounded state
    void update_grounded() {
        void_math::Transform transform;
        transform.position = m_position;

        // Cast down slightly
        auto hit = m_query.shape_cast(m_capsule, transform, void_math::Vec3{0, -1, 0},
                                     m_config.skin_width * 2.0f + 0.1f,
                                     QueryFilter::Default, m_config.collision_mask.collides_with);

        if (hit.hit && hit.distance < m_config.skin_width * 2.0f + 0.05f) {
            m_grounded = true;
            m_ground_normal = hit.normal;
            m_collision_flags.below = true;
        } else {
            m_grounded = false;
            m_ground_normal = void_math::Vec3{0, 1, 0};
        }
    }

    /// Check if current slope is too steep
    [[nodiscard]] bool is_slope_too_steep() const {
        if (!m_grounded) return false;

        float angle = std::acos(void_math::dot(m_ground_normal, void_math::Vec3{0, 1, 0}));
        float max_angle = m_config.max_slope * (3.14159265f / 180.0f);
        return angle > max_angle;
    }

private:
    IPhysicsWorld& m_world;
    CharacterControllerConfig m_config;
    QuerySystem m_query;
    CapsuleShape m_capsule;

    void_math::Vec3 m_position{0, 0, 0};
    void_math::Vec3 m_velocity{0, 0, 0};
    void_math::Vec3 m_ground_normal{0, 1, 0};

    CharacterState m_state = CharacterState::Falling;
    CharacterCollisionFlags m_collision_flags;

    bool m_grounded = false;
    float m_coyote_time = 0.0f;
};

// =============================================================================
// Kinematic Character Controller
// =============================================================================

/// Kinematic-based character controller using physics body
class KinematicCharacterController {
public:
    KinematicCharacterController(IPhysicsWorld& world, const CharacterControllerConfig& config)
        : m_world(world)
        , m_config(config)
    {
        // Create kinematic body
        BodyConfig body_config;
        body_config.type = BodyType::Kinematic;
        body_config.position = void_math::Vec3{0, config.height * 0.5f, 0};
        body_config.collision_mask = config.collision_mask;

        m_body_id = world.create_body(body_config);

        // Add capsule shape
        auto* body = world.get_body(m_body_id);
        if (body) {
            auto capsule = std::make_unique<CapsuleShape>(config.radius, config.height - 2.0f * config.radius);
            body->add_shape(std::move(capsule));
        }
    }

    ~KinematicCharacterController() {
        if (m_body_id.is_valid()) {
            m_world.destroy_body(m_body_id);
        }
    }

    // Non-copyable
    KinematicCharacterController(const KinematicCharacterController&) = delete;
    KinematicCharacterController& operator=(const KinematicCharacterController&) = delete;

    /// Move the character
    void move(const void_math::Vec3& velocity, float dt) {
        auto* body = m_world.get_body(m_body_id);
        if (!body) return;

        auto pos = body->position();

        // Apply gravity
        if (!m_grounded) {
            m_vertical_velocity += m_config.gravity * dt;
        } else {
            m_vertical_velocity = 0.0f;
        }

        // Compute target position
        auto horizontal = void_math::Vec3{velocity.x, 0, velocity.z};
        auto target = pos + horizontal * dt + void_math::Vec3{0, m_vertical_velocity * dt, 0};

        // Move kinematic body
        body->move_kinematic(target, body->rotation());

        // Update grounded check after physics step
        update_grounded();
    }

    /// Jump
    void jump() {
        if (m_grounded) {
            m_vertical_velocity = m_config.jump_speed;
            m_grounded = false;
        }
    }

    /// Get position
    [[nodiscard]] void_math::Vec3 position() const {
        auto* body = m_world.get_body(m_body_id);
        return body ? body->position() : void_math::Vec3{0, 0, 0};
    }

    /// Set position
    void set_position(const void_math::Vec3& pos) {
        auto* body = m_world.get_body(m_body_id);
        if (body) {
            body->set_position(pos);
        }
    }

    /// Check if grounded
    [[nodiscard]] bool is_grounded() const { return m_grounded; }

    /// Get body ID
    [[nodiscard]] BodyId body_id() const { return m_body_id; }

private:
    void update_grounded() {
        auto* body = m_world.get_body(m_body_id);
        if (!body) return;

        // Raycast down
        auto pos = body->position();
        auto hit = m_world.raycast(
            pos,
            void_math::Vec3{0, -1, 0},
            m_config.height * 0.5f + 0.1f,
            QueryFilter::Default,
            m_config.collision_mask.collides_with);

        m_grounded = hit.hit && hit.distance < m_config.height * 0.5f + 0.05f;
    }

private:
    IPhysicsWorld& m_world;
    CharacterControllerConfig m_config;
    BodyId m_body_id;

    float m_vertical_velocity = 0.0f;
    bool m_grounded = false;
};

// =============================================================================
// First Person Controller
// =============================================================================

/// First-person controller with mouse look
class FirstPersonController {
public:
    FirstPersonController(IPhysicsWorld& world, const CharacterControllerConfig& config)
        : m_character(world, config)
    {}

    /// Update with input
    void update(float forward, float right, float dt, bool jump_pressed) {
        // Compute movement direction based on yaw
        float yaw_rad = m_yaw * (3.14159265f / 180.0f);
        float cos_yaw = std::cos(yaw_rad);
        float sin_yaw = std::sin(yaw_rad);

        void_math::Vec3 forward_dir{sin_yaw, 0, cos_yaw};
        void_math::Vec3 right_dir{cos_yaw, 0, -sin_yaw};

        auto move_dir = forward_dir * forward + right_dir * right;
        float speed = m_sprinting ? m_character.config().run_speed : m_character.config().walk_speed;

        auto displacement = move_dir * speed * dt;
        m_character.move(displacement, dt);

        if (jump_pressed) {
            m_character.jump();
        }

        m_character.update(dt);
    }

    /// Look (mouse input)
    void look(float delta_yaw, float delta_pitch) {
        m_yaw += delta_yaw * m_sensitivity;
        m_pitch += delta_pitch * m_sensitivity;
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    }

    /// Set sprint state
    void set_sprinting(bool sprinting) { m_sprinting = sprinting; }

    /// Get position
    [[nodiscard]] void_math::Vec3 position() const { return m_character.position(); }

    /// Get eye position (camera position)
    [[nodiscard]] void_math::Vec3 eye_position() const {
        auto pos = m_character.position();
        pos.y += m_character.config().height * 0.4f; // Eye height
        return pos;
    }

    /// Get yaw (degrees)
    [[nodiscard]] float yaw() const { return m_yaw; }

    /// Get pitch (degrees)
    [[nodiscard]] float pitch() const { return m_pitch; }

    /// Get forward direction
    [[nodiscard]] void_math::Vec3 forward_direction() const {
        float yaw_rad = m_yaw * (3.14159265f / 180.0f);
        float pitch_rad = m_pitch * (3.14159265f / 180.0f);
        return void_math::Vec3{
            std::sin(yaw_rad) * std::cos(pitch_rad),
            std::sin(pitch_rad),
            std::cos(yaw_rad) * std::cos(pitch_rad)
        };
    }

    /// Get underlying character controller
    [[nodiscard]] CharacterControllerImpl& character() { return m_character; }

    /// Set sensitivity
    void set_sensitivity(float sensitivity) { m_sensitivity = sensitivity; }

private:
    CharacterControllerImpl m_character;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_sensitivity = 0.1f;
    bool m_sprinting = false;
};

// =============================================================================
// Third Person Controller
// =============================================================================

/// Third-person controller with camera orbit
class ThirdPersonController {
public:
    ThirdPersonController(IPhysicsWorld& world, const CharacterControllerConfig& config)
        : m_character(world, config)
    {}

    /// Update with input
    void update(float forward, float right, float dt, bool jump_pressed) {
        // Movement relative to camera
        float yaw_rad = m_camera_yaw * (3.14159265f / 180.0f);
        float cos_yaw = std::cos(yaw_rad);
        float sin_yaw = std::sin(yaw_rad);

        void_math::Vec3 forward_dir{sin_yaw, 0, cos_yaw};
        void_math::Vec3 right_dir{cos_yaw, 0, -sin_yaw};

        auto move_dir = forward_dir * forward + right_dir * right;
        float move_len = void_math::length(move_dir);

        if (move_len > 0.01f) {
            move_dir = move_dir / move_len;
            // Rotate character to face movement direction
            m_character_yaw = std::atan2(move_dir.x, move_dir.z) * (180.0f / 3.14159265f);
        }

        float speed = m_sprinting ? m_character.config().run_speed : m_character.config().walk_speed;
        auto displacement = move_dir * speed * dt;
        m_character.move(displacement, dt);

        if (jump_pressed) {
            m_character.jump();
        }

        m_character.update(dt);
    }

    /// Orbit camera
    void orbit(float delta_yaw, float delta_pitch) {
        m_camera_yaw += delta_yaw * m_sensitivity;
        m_camera_pitch += delta_pitch * m_sensitivity;
        m_camera_pitch = std::clamp(m_camera_pitch, -60.0f, 60.0f);
    }

    /// Zoom camera
    void zoom(float delta) {
        m_camera_distance = std::clamp(m_camera_distance + delta, m_min_distance, m_max_distance);
    }

    /// Get character position
    [[nodiscard]] void_math::Vec3 character_position() const { return m_character.position(); }

    /// Get camera position
    [[nodiscard]] void_math::Vec3 camera_position() const {
        auto target = m_character.position() + void_math::Vec3{0, m_camera_height, 0};

        float yaw_rad = m_camera_yaw * (3.14159265f / 180.0f);
        float pitch_rad = m_camera_pitch * (3.14159265f / 180.0f);

        void_math::Vec3 offset{
            -std::sin(yaw_rad) * std::cos(pitch_rad) * m_camera_distance,
            std::sin(pitch_rad) * m_camera_distance,
            -std::cos(yaw_rad) * std::cos(pitch_rad) * m_camera_distance
        };

        return target + offset;
    }

    /// Get camera target (look-at point)
    [[nodiscard]] void_math::Vec3 camera_target() const {
        return m_character.position() + void_math::Vec3{0, m_camera_height, 0};
    }

    /// Get character yaw (facing direction)
    [[nodiscard]] float character_yaw() const { return m_character_yaw; }

    /// Set sprint state
    void set_sprinting(bool sprinting) { m_sprinting = sprinting; }

    /// Get underlying character controller
    [[nodiscard]] CharacterControllerImpl& character() { return m_character; }

    /// Set camera distance limits
    void set_distance_limits(float min_dist, float max_dist) {
        m_min_distance = min_dist;
        m_max_distance = max_dist;
        m_camera_distance = std::clamp(m_camera_distance, min_dist, max_dist);
    }

private:
    CharacterControllerImpl m_character;
    float m_character_yaw = 0.0f;
    float m_camera_yaw = 0.0f;
    float m_camera_pitch = 20.0f;
    float m_camera_distance = 5.0f;
    float m_camera_height = 1.5f;
    float m_sensitivity = 0.1f;
    float m_min_distance = 2.0f;
    float m_max_distance = 20.0f;
    bool m_sprinting = false;
};

} // namespace void_physics
