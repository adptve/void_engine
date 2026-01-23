/// @file volumes.hpp
/// @brief Trigger volume shapes for void_triggers module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <memory>
#include <vector>

namespace void_triggers {

// =============================================================================
// ITriggerVolume Interface
// =============================================================================

/// @brief Interface for trigger volumes
class ITriggerVolume {
public:
    virtual ~ITriggerVolume() = default;

    /// @brief Get volume type
    virtual VolumeType type() const = 0;

    /// @brief Check if point is inside volume
    virtual bool contains(const Vec3& point) const = 0;

    /// @brief Check if sphere intersects volume
    virtual bool intersects_sphere(const Vec3& center, float radius) const = 0;

    /// @brief Check if AABB intersects volume
    virtual bool intersects_aabb(const AABB& box) const = 0;

    /// @brief Get bounding AABB
    virtual AABB bounds() const = 0;

    /// @brief Get center position
    virtual Vec3 center() const = 0;

    /// @brief Set center position
    virtual void set_center(const Vec3& center) = 0;

    /// @brief Clone the volume
    virtual std::unique_ptr<ITriggerVolume> clone() const = 0;
};

// =============================================================================
// BoxVolume
// =============================================================================

/// @brief Axis-aligned box volume
class BoxVolume : public ITriggerVolume {
public:
    BoxVolume();
    BoxVolume(const Vec3& center, const Vec3& half_extents);
    ~BoxVolume() override;

    VolumeType type() const override { return VolumeType::Box; }
    bool contains(const Vec3& point) const override;
    bool intersects_sphere(const Vec3& center, float radius) const override;
    bool intersects_aabb(const AABB& box) const override;
    AABB bounds() const override;
    Vec3 center() const override { return m_center; }
    void set_center(const Vec3& center) override { m_center = center; }
    std::unique_ptr<ITriggerVolume> clone() const override;

    const Vec3& half_extents() const { return m_half_extents; }
    void set_half_extents(const Vec3& extents) { m_half_extents = extents; }

private:
    Vec3 m_center;
    Vec3 m_half_extents{1, 1, 1};
};

// =============================================================================
// SphereVolume
// =============================================================================

/// @brief Sphere volume
class SphereVolume : public ITriggerVolume {
public:
    SphereVolume();
    SphereVolume(const Vec3& center, float radius);
    ~SphereVolume() override;

    VolumeType type() const override { return VolumeType::Sphere; }
    bool contains(const Vec3& point) const override;
    bool intersects_sphere(const Vec3& center, float radius) const override;
    bool intersects_aabb(const AABB& box) const override;
    AABB bounds() const override;
    Vec3 center() const override { return m_center; }
    void set_center(const Vec3& center) override { m_center = center; }
    std::unique_ptr<ITriggerVolume> clone() const override;

    float radius() const { return m_radius; }
    void set_radius(float radius) { m_radius = radius; }

private:
    Vec3 m_center;
    float m_radius{1.0f};
};

// =============================================================================
// CapsuleVolume
// =============================================================================

/// @brief Capsule volume (cylinder with hemispherical caps)
class CapsuleVolume : public ITriggerVolume {
public:
    CapsuleVolume();
    CapsuleVolume(const Vec3& start, const Vec3& end, float radius);
    ~CapsuleVolume() override;

    VolumeType type() const override { return VolumeType::Capsule; }
    bool contains(const Vec3& point) const override;
    bool intersects_sphere(const Vec3& center, float radius) const override;
    bool intersects_aabb(const AABB& box) const override;
    AABB bounds() const override;
    Vec3 center() const override;
    void set_center(const Vec3& center) override;
    std::unique_ptr<ITriggerVolume> clone() const override;

    const Vec3& start() const { return m_start; }
    const Vec3& end() const { return m_end; }
    float radius() const { return m_radius; }

    void set_endpoints(const Vec3& start, const Vec3& end);
    void set_radius(float radius) { m_radius = radius; }

private:
    float point_to_segment_distance(const Vec3& point) const;

    Vec3 m_start;
    Vec3 m_end{0, 2, 0};
    float m_radius{0.5f};
};

// =============================================================================
// OrientedBoxVolume
// =============================================================================

/// @brief Oriented bounding box volume
class OrientedBoxVolume : public ITriggerVolume {
public:
    OrientedBoxVolume();
    OrientedBoxVolume(const Vec3& center, const Vec3& half_extents, const Quat& orientation);
    ~OrientedBoxVolume() override;

    VolumeType type() const override { return VolumeType::OrientedBox; }
    bool contains(const Vec3& point) const override;
    bool intersects_sphere(const Vec3& center, float radius) const override;
    bool intersects_aabb(const AABB& box) const override;
    AABB bounds() const override;
    Vec3 center() const override { return m_center; }
    void set_center(const Vec3& center) override { m_center = center; }
    std::unique_ptr<ITriggerVolume> clone() const override;

    const Vec3& half_extents() const { return m_half_extents; }
    const Quat& orientation() const { return m_orientation; }

    void set_half_extents(const Vec3& extents) { m_half_extents = extents; }
    void set_orientation(const Quat& orientation) { m_orientation = orientation; }

private:
    Vec3 world_to_local(const Vec3& point) const;
    Vec3 rotate_by_quat_inverse(const Vec3& v) const;

    Vec3 m_center;
    Vec3 m_half_extents{1, 1, 1};
    Quat m_orientation{0, 0, 0, 1};
};

// =============================================================================
// CompositeVolume
// =============================================================================

/// @brief Composite volume made of multiple sub-volumes
class CompositeVolume : public ITriggerVolume {
public:
    enum class Operation {
        Union,          ///< Point in any sub-volume
        Intersection,   ///< Point in all sub-volumes
        Difference      ///< Point in first but not subsequent
    };

    CompositeVolume();
    explicit CompositeVolume(Operation op);
    ~CompositeVolume() override;

    VolumeType type() const override { return VolumeType::Composite; }
    bool contains(const Vec3& point) const override;
    bool intersects_sphere(const Vec3& center, float radius) const override;
    bool intersects_aabb(const AABB& box) const override;
    AABB bounds() const override;
    Vec3 center() const override;
    void set_center(const Vec3& center) override;
    std::unique_ptr<ITriggerVolume> clone() const override;

    /// @brief Add a sub-volume
    void add_volume(std::unique_ptr<ITriggerVolume> volume);

    /// @brief Remove all sub-volumes
    void clear();

    /// @brief Get sub-volume count
    std::size_t volume_count() const { return m_volumes.size(); }

    /// @brief Get sub-volume
    ITriggerVolume* get_volume(std::size_t index);
    const ITriggerVolume* get_volume(std::size_t index) const;

    /// @brief Set operation
    void set_operation(Operation op) { m_operation = op; }
    Operation operation() const { return m_operation; }

private:
    std::vector<std::unique_ptr<ITriggerVolume>> m_volumes;
    Operation m_operation{Operation::Union};
};

// =============================================================================
// Volume Factory
// =============================================================================

/// @brief Factory for creating trigger volumes
class VolumeFactory {
public:
    /// @brief Create a box volume
    static std::unique_ptr<ITriggerVolume> create_box(const Vec3& center, const Vec3& half_extents);

    /// @brief Create a sphere volume
    static std::unique_ptr<ITriggerVolume> create_sphere(const Vec3& center, float radius);

    /// @brief Create a capsule volume
    static std::unique_ptr<ITriggerVolume> create_capsule(const Vec3& start, const Vec3& end, float radius);

    /// @brief Create an oriented box volume
    static std::unique_ptr<ITriggerVolume> create_oriented_box(const Vec3& center,
                                                                const Vec3& half_extents,
                                                                const Quat& orientation);

    /// @brief Create from zone configuration
    static std::unique_ptr<ITriggerVolume> create_from_config(const ZoneConfig& config);
};

} // namespace void_triggers
