/// @file volumes.cpp
/// @brief Trigger volume implementation for void_triggers module

#include <void_engine/triggers/volumes.hpp>

#include <algorithm>
#include <cmath>

namespace void_triggers {

// =============================================================================
// BoxVolume Implementation
// =============================================================================

BoxVolume::BoxVolume() = default;

BoxVolume::BoxVolume(const Vec3& center, const Vec3& half_extents)
    : m_center(center)
    , m_half_extents(half_extents) {
}

BoxVolume::~BoxVolume() = default;

bool BoxVolume::contains(const Vec3& point) const {
    return std::abs(point.x - m_center.x) <= m_half_extents.x &&
           std::abs(point.y - m_center.y) <= m_half_extents.y &&
           std::abs(point.z - m_center.z) <= m_half_extents.z;
}

bool BoxVolume::intersects_sphere(const Vec3& center, float radius) const {
    // Find closest point on box to sphere center
    float closest_x = std::max(m_center.x - m_half_extents.x,
                               std::min(center.x, m_center.x + m_half_extents.x));
    float closest_y = std::max(m_center.y - m_half_extents.y,
                               std::min(center.y, m_center.y + m_half_extents.y));
    float closest_z = std::max(m_center.z - m_half_extents.z,
                               std::min(center.z, m_center.z + m_half_extents.z));

    float dx = center.x - closest_x;
    float dy = center.y - closest_y;
    float dz = center.z - closest_z;

    return (dx * dx + dy * dy + dz * dz) <= radius * radius;
}

bool BoxVolume::intersects_aabb(const AABB& box) const {
    AABB my_box = bounds();
    return my_box.intersects(box);
}

AABB BoxVolume::bounds() const {
    return {
        {m_center.x - m_half_extents.x, m_center.y - m_half_extents.y, m_center.z - m_half_extents.z},
        {m_center.x + m_half_extents.x, m_center.y + m_half_extents.y, m_center.z + m_half_extents.z}
    };
}

std::unique_ptr<ITriggerVolume> BoxVolume::clone() const {
    return std::make_unique<BoxVolume>(m_center, m_half_extents);
}

// =============================================================================
// SphereVolume Implementation
// =============================================================================

SphereVolume::SphereVolume() = default;

SphereVolume::SphereVolume(const Vec3& center, float radius)
    : m_center(center)
    , m_radius(radius) {
}

SphereVolume::~SphereVolume() = default;

bool SphereVolume::contains(const Vec3& point) const {
    float dx = point.x - m_center.x;
    float dy = point.y - m_center.y;
    float dz = point.z - m_center.z;
    return (dx * dx + dy * dy + dz * dz) <= m_radius * m_radius;
}

bool SphereVolume::intersects_sphere(const Vec3& center, float radius) const {
    float dx = center.x - m_center.x;
    float dy = center.y - m_center.y;
    float dz = center.z - m_center.z;
    float combined_radius = m_radius + radius;
    return (dx * dx + dy * dy + dz * dz) <= combined_radius * combined_radius;
}

bool SphereVolume::intersects_aabb(const AABB& box) const {
    // Find closest point on box to sphere center
    float closest_x = std::max(box.min.x, std::min(m_center.x, box.max.x));
    float closest_y = std::max(box.min.y, std::min(m_center.y, box.max.y));
    float closest_z = std::max(box.min.z, std::min(m_center.z, box.max.z));

    float dx = m_center.x - closest_x;
    float dy = m_center.y - closest_y;
    float dz = m_center.z - closest_z;

    return (dx * dx + dy * dy + dz * dz) <= m_radius * m_radius;
}

AABB SphereVolume::bounds() const {
    return {
        {m_center.x - m_radius, m_center.y - m_radius, m_center.z - m_radius},
        {m_center.x + m_radius, m_center.y + m_radius, m_center.z + m_radius}
    };
}

std::unique_ptr<ITriggerVolume> SphereVolume::clone() const {
    return std::make_unique<SphereVolume>(m_center, m_radius);
}

// =============================================================================
// CapsuleVolume Implementation
// =============================================================================

CapsuleVolume::CapsuleVolume() = default;

CapsuleVolume::CapsuleVolume(const Vec3& start, const Vec3& end, float radius)
    : m_start(start)
    , m_end(end)
    , m_radius(radius) {
}

CapsuleVolume::~CapsuleVolume() = default;

float CapsuleVolume::point_to_segment_distance(const Vec3& point) const {
    Vec3 segment = m_end - m_start;
    Vec3 to_point = point - m_start;

    float segment_length_sq = segment.length_squared();
    if (segment_length_sq < 0.0001f) {
        // Degenerate capsule - just a sphere
        return std::sqrt(to_point.length_squared());
    }

    float t = to_point.dot(segment) / segment_length_sq;
    t = std::max(0.0f, std::min(1.0f, t));

    Vec3 closest = {
        m_start.x + t * segment.x,
        m_start.y + t * segment.y,
        m_start.z + t * segment.z
    };

    Vec3 diff = point - closest;
    return std::sqrt(diff.length_squared());
}

bool CapsuleVolume::contains(const Vec3& point) const {
    return point_to_segment_distance(point) <= m_radius;
}

bool CapsuleVolume::intersects_sphere(const Vec3& center, float radius) const {
    return point_to_segment_distance(center) <= m_radius + radius;
}

bool CapsuleVolume::intersects_aabb(const AABB& box) const {
    // Approximate with expanded AABB test
    AABB expanded = {
        {box.min.x - m_radius, box.min.y - m_radius, box.min.z - m_radius},
        {box.max.x + m_radius, box.max.y + m_radius, box.max.z + m_radius}
    };

    // Check if line segment intersects expanded box
    Vec3 mid = center();
    return expanded.contains(mid) || expanded.contains(m_start) || expanded.contains(m_end);
}

AABB CapsuleVolume::bounds() const {
    float min_x = std::min(m_start.x, m_end.x) - m_radius;
    float min_y = std::min(m_start.y, m_end.y) - m_radius;
    float min_z = std::min(m_start.z, m_end.z) - m_radius;
    float max_x = std::max(m_start.x, m_end.x) + m_radius;
    float max_y = std::max(m_start.y, m_end.y) + m_radius;
    float max_z = std::max(m_start.z, m_end.z) + m_radius;
    return {{min_x, min_y, min_z}, {max_x, max_y, max_z}};
}

Vec3 CapsuleVolume::center() const {
    return {
        (m_start.x + m_end.x) * 0.5f,
        (m_start.y + m_end.y) * 0.5f,
        (m_start.z + m_end.z) * 0.5f
    };
}

void CapsuleVolume::set_center(const Vec3& new_center) {
    Vec3 current = center();
    Vec3 offset = new_center - current;
    m_start = m_start + offset;
    m_end = m_end + offset;
}

void CapsuleVolume::set_endpoints(const Vec3& start, const Vec3& end) {
    m_start = start;
    m_end = end;
}

std::unique_ptr<ITriggerVolume> CapsuleVolume::clone() const {
    return std::make_unique<CapsuleVolume>(m_start, m_end, m_radius);
}

// =============================================================================
// OrientedBoxVolume Implementation
// =============================================================================

OrientedBoxVolume::OrientedBoxVolume() = default;

OrientedBoxVolume::OrientedBoxVolume(const Vec3& center, const Vec3& half_extents, const Quat& orientation)
    : m_center(center)
    , m_half_extents(half_extents)
    , m_orientation(orientation) {
}

OrientedBoxVolume::~OrientedBoxVolume() = default;

Vec3 OrientedBoxVolume::rotate_by_quat_inverse(const Vec3& v) const {
    // Conjugate of quaternion for inverse rotation
    Quat q_inv = {-m_orientation.x, -m_orientation.y, -m_orientation.z, m_orientation.w};

    // Rotate vector by quaternion: q * v * q^-1
    // Simplified for unit quaternion
    float qx = q_inv.x, qy = q_inv.y, qz = q_inv.z, qw = q_inv.w;

    float ix = qw * v.x + qy * v.z - qz * v.y;
    float iy = qw * v.y + qz * v.x - qx * v.z;
    float iz = qw * v.z + qx * v.y - qy * v.x;
    float iw = -qx * v.x - qy * v.y - qz * v.z;

    return {
        ix * qw + iw * -qx + iy * -qz - iz * -qy,
        iy * qw + iw * -qy + iz * -qx - ix * -qz,
        iz * qw + iw * -qz + ix * -qy - iy * -qx
    };
}

Vec3 OrientedBoxVolume::world_to_local(const Vec3& point) const {
    Vec3 relative = point - m_center;
    return rotate_by_quat_inverse(relative);
}

bool OrientedBoxVolume::contains(const Vec3& point) const {
    Vec3 local = world_to_local(point);
    return std::abs(local.x) <= m_half_extents.x &&
           std::abs(local.y) <= m_half_extents.y &&
           std::abs(local.z) <= m_half_extents.z;
}

bool OrientedBoxVolume::intersects_sphere(const Vec3& center, float radius) const {
    Vec3 local = world_to_local(center);

    // Find closest point on local box
    float closest_x = std::max(-m_half_extents.x, std::min(local.x, m_half_extents.x));
    float closest_y = std::max(-m_half_extents.y, std::min(local.y, m_half_extents.y));
    float closest_z = std::max(-m_half_extents.z, std::min(local.z, m_half_extents.z));

    float dx = local.x - closest_x;
    float dy = local.y - closest_y;
    float dz = local.z - closest_z;

    return (dx * dx + dy * dy + dz * dz) <= radius * radius;
}

bool OrientedBoxVolume::intersects_aabb(const AABB& box) const {
    // Simplified: check if OBB's AABB intersects
    return bounds().intersects(box);
}

AABB OrientedBoxVolume::bounds() const {
    // Compute AABB that encloses the OBB
    // This is approximate - rotate each corner and find min/max
    Vec3 corners[8];
    float hx = m_half_extents.x, hy = m_half_extents.y, hz = m_half_extents.z;

    // Local corners
    Vec3 local_corners[8] = {
        {-hx, -hy, -hz}, {hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz},
        {-hx, -hy, hz}, {hx, -hy, hz}, {-hx, hy, hz}, {hx, hy, hz}
    };

    // Rotate and translate to world
    Vec3 min_bound = {1e30f, 1e30f, 1e30f};
    Vec3 max_bound = {-1e30f, -1e30f, -1e30f};

    // Simplified: use the max extent in all directions
    float max_extent = std::sqrt(hx * hx + hy * hy + hz * hz);
    return {
        {m_center.x - max_extent, m_center.y - max_extent, m_center.z - max_extent},
        {m_center.x + max_extent, m_center.y + max_extent, m_center.z + max_extent}
    };
}

std::unique_ptr<ITriggerVolume> OrientedBoxVolume::clone() const {
    return std::make_unique<OrientedBoxVolume>(m_center, m_half_extents, m_orientation);
}

// =============================================================================
// CompositeVolume Implementation
// =============================================================================

CompositeVolume::CompositeVolume() = default;

CompositeVolume::CompositeVolume(Operation op)
    : m_operation(op) {
}

CompositeVolume::~CompositeVolume() = default;

bool CompositeVolume::contains(const Vec3& point) const {
    if (m_volumes.empty()) {
        return false;
    }

    switch (m_operation) {
        case Operation::Union:
            for (const auto& vol : m_volumes) {
                if (vol->contains(point)) {
                    return true;
                }
            }
            return false;

        case Operation::Intersection:
            for (const auto& vol : m_volumes) {
                if (!vol->contains(point)) {
                    return false;
                }
            }
            return true;

        case Operation::Difference:
            if (!m_volumes[0]->contains(point)) {
                return false;
            }
            for (std::size_t i = 1; i < m_volumes.size(); ++i) {
                if (m_volumes[i]->contains(point)) {
                    return false;
                }
            }
            return true;
    }

    return false;
}

bool CompositeVolume::intersects_sphere(const Vec3& center, float radius) const {
    for (const auto& vol : m_volumes) {
        if (vol->intersects_sphere(center, radius)) {
            return true;
        }
    }
    return false;
}

bool CompositeVolume::intersects_aabb(const AABB& box) const {
    for (const auto& vol : m_volumes) {
        if (vol->intersects_aabb(box)) {
            return true;
        }
    }
    return false;
}

AABB CompositeVolume::bounds() const {
    if (m_volumes.empty()) {
        return {};
    }

    AABB result = m_volumes[0]->bounds();
    for (std::size_t i = 1; i < m_volumes.size(); ++i) {
        AABB vol_bounds = m_volumes[i]->bounds();
        result.min.x = std::min(result.min.x, vol_bounds.min.x);
        result.min.y = std::min(result.min.y, vol_bounds.min.y);
        result.min.z = std::min(result.min.z, vol_bounds.min.z);
        result.max.x = std::max(result.max.x, vol_bounds.max.x);
        result.max.y = std::max(result.max.y, vol_bounds.max.y);
        result.max.z = std::max(result.max.z, vol_bounds.max.z);
    }
    return result;
}

Vec3 CompositeVolume::center() const {
    AABB b = bounds();
    return b.center();
}

void CompositeVolume::set_center(const Vec3& new_center) {
    Vec3 current = center();
    Vec3 offset = new_center - current;
    for (auto& vol : m_volumes) {
        Vec3 vol_center = vol->center();
        vol->set_center(vol_center + offset);
    }
}

void CompositeVolume::add_volume(std::unique_ptr<ITriggerVolume> volume) {
    m_volumes.push_back(std::move(volume));
}

void CompositeVolume::clear() {
    m_volumes.clear();
}

ITriggerVolume* CompositeVolume::get_volume(std::size_t index) {
    return index < m_volumes.size() ? m_volumes[index].get() : nullptr;
}

const ITriggerVolume* CompositeVolume::get_volume(std::size_t index) const {
    return index < m_volumes.size() ? m_volumes[index].get() : nullptr;
}

std::unique_ptr<ITriggerVolume> CompositeVolume::clone() const {
    auto result = std::make_unique<CompositeVolume>(m_operation);
    for (const auto& vol : m_volumes) {
        result->add_volume(vol->clone());
    }
    return result;
}

// =============================================================================
// VolumeFactory Implementation
// =============================================================================

std::unique_ptr<ITriggerVolume> VolumeFactory::create_box(const Vec3& center, const Vec3& half_extents) {
    return std::make_unique<BoxVolume>(center, half_extents);
}

std::unique_ptr<ITriggerVolume> VolumeFactory::create_sphere(const Vec3& center, float radius) {
    return std::make_unique<SphereVolume>(center, radius);
}

std::unique_ptr<ITriggerVolume> VolumeFactory::create_capsule(const Vec3& start, const Vec3& end, float radius) {
    return std::make_unique<CapsuleVolume>(start, end, radius);
}

std::unique_ptr<ITriggerVolume> VolumeFactory::create_oriented_box(const Vec3& center,
                                                                    const Vec3& half_extents,
                                                                    const Quat& orientation) {
    return std::make_unique<OrientedBoxVolume>(center, half_extents, orientation);
}

std::unique_ptr<ITriggerVolume> VolumeFactory::create_from_config(const ZoneConfig& config) {
    switch (config.volume_type) {
        case VolumeType::Box:
            return create_box(config.position, config.half_extents);

        case VolumeType::Sphere:
            return create_sphere(config.position, config.radius);

        case VolumeType::Capsule: {
            Vec3 end = config.position;
            end.y += config.capsule_height;
            return create_capsule(config.position, end, config.capsule_radius);
        }

        case VolumeType::OrientedBox:
            return create_oriented_box(config.position, config.half_extents, config.rotation);

        default:
            return create_box(config.position, config.half_extents);
    }
}

} // namespace void_triggers
