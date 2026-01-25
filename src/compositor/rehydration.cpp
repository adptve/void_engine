/// @file rehydration.cpp
/// @brief Hot-reload rehydration implementation
///
/// Provides state serialization, deserialization, and hot-reload support
/// for the compositor subsystem.

#include <void_engine/compositor/rehydration.hpp>
#include <void_engine/compositor/snapshot.hpp>
#include <void_engine/compositor/frame.hpp>
#include <void_engine/compositor/vrr.hpp>
#include <void_engine/compositor/hdr.hpp>
#include <void_engine/compositor/layer.hpp>
#include <void_engine/compositor/types.hpp>

#include <algorithm>
#include <sstream>

namespace void_compositor {

// =============================================================================
// Rehydration State Validation
// =============================================================================

/// Validate rehydration state has required fields
[[nodiscard]] bool validate_rehydration_state(
    const RehydrationState& state,
    const std::vector<std::string>& required_fields) {

    for (const auto& field : required_fields) {
        // Check all value types
        if (state.get_string(field).has_value()) continue;
        if (state.get_int(field).has_value()) continue;
        if (state.get_float(field).has_value()) continue;
        if (state.get_bool(field).has_value()) continue;
        if (state.get_binary(field) != nullptr) continue;
        if (state.get_nested(field) != nullptr) continue;

        return false;  // Field not found
    }
    return true;
}

/// Get missing fields from rehydration state
[[nodiscard]] std::vector<std::string> get_missing_fields(
    const RehydrationState& state,
    const std::vector<std::string>& required_fields) {

    std::vector<std::string> missing;
    for (const auto& field : required_fields) {
        if (!state.get_string(field).has_value() &&
            !state.get_int(field).has_value() &&
            !state.get_float(field).has_value() &&
            !state.get_bool(field).has_value() &&
            state.get_binary(field) == nullptr &&
            state.get_nested(field) == nullptr) {
            missing.push_back(field);
        }
    }
    return missing;
}

// =============================================================================
// Frame Scheduler Rehydration
// =============================================================================

/// Restore frame scheduler state from snapshot
bool restore_frame_scheduler_state(
    FrameScheduler& scheduler,
    const FrameSchedulerSnapshot& snapshot) {

    if (!snapshot.is_compatible()) {
        return false;
    }

    // Restore target FPS
    scheduler.set_target_fps(snapshot.target_fps);

    // Restore VRR config if present
    if (snapshot.has_vrr_config) {
        scheduler.set_vrr_config(snapshot.vrr_config);
    }

    // Update content velocity
    scheduler.update_content_velocity(snapshot.content_velocity);

    return true;
}

/// Create frame scheduler snapshot with extended info
[[nodiscard]] RehydrationState create_frame_scheduler_state(const FrameScheduler& scheduler) {
    RehydrationState state;

    state.set_u32("target_fps", scheduler.target_fps());
    state.set_uint("frame_number", scheduler.frame_number());
    state.set_uint("dropped_count", scheduler.dropped_frame_count());
    state.set_float("content_velocity", static_cast<double>(scheduler.content_velocity()));
    state.set_int("state", static_cast<std::int64_t>(scheduler.state()));

    // Frame budget
    auto budget = scheduler.frame_budget();
    state.set_int("frame_budget_ns", budget.count());

    // Statistics
    auto avg = scheduler.average_frame_time();
    state.set_int("avg_frame_time_ns", avg.count());
    state.set_float("current_fps", scheduler.current_fps());
    state.set_bool("hitting_target", scheduler.hitting_target());

    // VRR config
    if (scheduler.vrr_config()) {
        state.set_nested("vrr_config", serialize_vrr_config(*scheduler.vrr_config()));
    }

    return state;
}

// =============================================================================
// Layer Manager Rehydration
// =============================================================================

/// Create layer state for single layer
[[nodiscard]] RehydrationState create_layer_state(const Layer& layer) {
    RehydrationState state;

    state.set_uint("id", layer.id().id);
    state.set_string("name", layer.config().name);
    state.set_int("priority", layer.config().priority);
    state.set_int("blend_mode", static_cast<std::int64_t>(layer.config().blend_mode));
    state.set_float("opacity", static_cast<double>(layer.config().opacity));
    state.set_bool("visible", layer.config().visible);
    state.set_bool("clip_to_bounds", layer.config().clip_to_bounds);

    // Bounds
    state.set_float("bounds_x", static_cast<double>(layer.bounds().x));
    state.set_float("bounds_y", static_cast<double>(layer.bounds().y));
    state.set_float("bounds_w", static_cast<double>(layer.bounds().width));
    state.set_float("bounds_h", static_cast<double>(layer.bounds().height));

    // Transform
    const auto& transform = layer.transform();
    if (!transform.is_identity()) {
        RehydrationState transform_state;
        transform_state.set_float("translate_x", static_cast<double>(transform.translate_x));
        transform_state.set_float("translate_y", static_cast<double>(transform.translate_y));
        transform_state.set_float("scale_x", static_cast<double>(transform.scale_x));
        transform_state.set_float("scale_y", static_cast<double>(transform.scale_y));
        transform_state.set_float("rotation", static_cast<double>(transform.rotation));
        transform_state.set_float("anchor_x", static_cast<double>(transform.anchor_x));
        transform_state.set_float("anchor_y", static_cast<double>(transform.anchor_y));
        state.set_nested("transform", std::move(transform_state));
    }

    // Parent
    if (layer.parent()) {
        state.set_uint("parent_id", layer.parent()->id);
    }

    // Content type
    state.set_int("content_type", static_cast<std::int64_t>(layer.content().type));

    // Content color (if solid)
    if (layer.content().type == LayerContentType::SolidColor) {
        state.set_float("color_r", static_cast<double>(layer.content().color.r));
        state.set_float("color_g", static_cast<double>(layer.content().color.g));
        state.set_float("color_b", static_cast<double>(layer.content().color.b));
        state.set_float("color_a", static_cast<double>(layer.content().color.a));
    }

    return state;
}

/// Restore layer from state
bool restore_layer_state(Layer& layer, const RehydrationState& state) {
    // Priority and config
    if (auto priority = state.get_int("priority")) {
        layer.config().priority = static_cast<std::int32_t>(*priority);
    }
    if (auto blend = state.get_int("blend_mode")) {
        layer.config().blend_mode = static_cast<BlendMode>(*blend);
    }
    if (auto opacity = state.get_float("opacity")) {
        layer.config().opacity = static_cast<float>(*opacity);
    }
    if (auto visible = state.get_bool("visible")) {
        layer.config().visible = *visible;
    }
    if (auto clip = state.get_bool("clip_to_bounds")) {
        layer.config().clip_to_bounds = *clip;
    }

    // Bounds
    LayerBounds bounds;
    if (auto x = state.get_float("bounds_x")) bounds.x = static_cast<float>(*x);
    if (auto y = state.get_float("bounds_y")) bounds.y = static_cast<float>(*y);
    if (auto w = state.get_float("bounds_w")) bounds.width = static_cast<float>(*w);
    if (auto h = state.get_float("bounds_h")) bounds.height = static_cast<float>(*h);
    layer.set_bounds(bounds);

    // Transform
    if (auto* transform_state = state.get_nested("transform")) {
        LayerTransform transform;
        if (auto v = transform_state->get_float("translate_x")) transform.translate_x = static_cast<float>(*v);
        if (auto v = transform_state->get_float("translate_y")) transform.translate_y = static_cast<float>(*v);
        if (auto v = transform_state->get_float("scale_x")) transform.scale_x = static_cast<float>(*v);
        if (auto v = transform_state->get_float("scale_y")) transform.scale_y = static_cast<float>(*v);
        if (auto v = transform_state->get_float("rotation")) transform.rotation = static_cast<float>(*v);
        if (auto v = transform_state->get_float("anchor_x")) transform.anchor_x = static_cast<float>(*v);
        if (auto v = transform_state->get_float("anchor_y")) transform.anchor_y = static_cast<float>(*v);
        layer.set_transform(transform);
    }

    // Content
    if (auto content_type = state.get_int("content_type")) {
        auto type = static_cast<LayerContentType>(*content_type);
        if (type == LayerContentType::SolidColor) {
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            if (auto v = state.get_float("color_r")) r = static_cast<float>(*v);
            if (auto v = state.get_float("color_g")) g = static_cast<float>(*v);
            if (auto v = state.get_float("color_b")) b = static_cast<float>(*v);
            if (auto v = state.get_float("color_a")) a = static_cast<float>(*v);
            layer.set_content(LayerContent::solid_color(r, g, b, a));
        }
    }

    return true;
}

// =============================================================================
// Compositor State Rehydration
// =============================================================================

/// Create compositor configuration state
[[nodiscard]] RehydrationState create_compositor_config_state(const CompositorConfig& config) {
    RehydrationState state;

    state.set_u32("target_fps", config.target_fps);
    state.set_bool("vsync", config.vsync);
    state.set_bool("allow_tearing", config.allow_tearing);
    state.set_bool("xwayland", config.xwayland);
    state.set_bool("enable_vrr", config.enable_vrr);
    state.set_bool("enable_hdr", config.enable_hdr);
    state.set_int("preferred_format", static_cast<std::int64_t>(config.preferred_format));

    return state;
}

/// Restore compositor configuration from state
[[nodiscard]] CompositorConfig restore_compositor_config_state(const RehydrationState& state) {
    CompositorConfig config;

    if (auto v = state.get_u32("target_fps")) config.target_fps = *v;
    if (auto v = state.get_bool("vsync")) config.vsync = *v;
    if (auto v = state.get_bool("allow_tearing")) config.allow_tearing = *v;
    if (auto v = state.get_bool("xwayland")) config.xwayland = *v;
    if (auto v = state.get_bool("enable_vrr")) config.enable_vrr = *v;
    if (auto v = state.get_bool("enable_hdr")) config.enable_hdr = *v;
    if (auto v = state.get_int("preferred_format")) {
        config.preferred_format = static_cast<RenderFormat>(*v);
    }

    return config;
}

// =============================================================================
// Binary State Serialization
// =============================================================================

/// Serialize RehydrationState to binary
[[nodiscard]] std::vector<std::uint8_t> serialize_rehydration_state(
    const RehydrationState& state) {

    BinaryWriter writer;

    // Magic and version
    writer.write_u32(0x52485953);  // "RHYS"
    writer.write_u32(1);  // Version

    // For simplicity, serialize as nested key-value structure
    // This is a basic implementation - production would use a proper format

    // Count (placeholder for now)
    writer.write_u32(static_cast<std::uint32_t>(state.count()));

    return writer.take();
}

/// Deserialize RehydrationState from binary
[[nodiscard]] std::optional<RehydrationState> deserialize_rehydration_state(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    // Check magic
    std::uint32_t magic = reader.read_u32();
    if (magic != 0x52485953) {
        return std::nullopt;
    }

    // Check version
    std::uint32_t version = reader.read_u32();
    if (version != 1) {
        return std::nullopt;
    }

    // Read count
    // std::uint32_t count = reader.read_u32();

    RehydrationState state;
    // Basic implementation - would need full deserialization logic

    return state;
}

// =============================================================================
// Rehydration Store Utilities
// =============================================================================

/// Get store statistics
struct RehydrationStoreStats {
    std::size_t total_entries = 0;
    std::size_t total_bytes = 0;
};

[[nodiscard]] RehydrationStoreStats get_store_stats(const RehydrationStore& store) {
    RehydrationStoreStats stats;
    stats.total_entries = store.size();
    // Note: byte count would require iterating and summing
    return stats;
}

/// Export store to binary
[[nodiscard]] std::vector<std::uint8_t> export_store(const RehydrationStore& store) {
    BinaryWriter writer;

    auto keys = store.keys();
    writer.write_u32(static_cast<std::uint32_t>(keys.size()));

    for (const auto& key : keys) {
        writer.write_string(key);
        auto state = store.retrieve(key);
        if (state) {
            auto state_data = serialize_rehydration_state(*state);
            writer.write_bytes(state_data);
        }
    }

    return writer.take();
}

/// Import store from binary
bool import_store(RehydrationStore& store, const std::vector<std::uint8_t>& data) {
    if (data.size() < 4) {
        return false;
    }

    BinaryReader reader(data);
    std::uint32_t count = reader.read_u32();

    for (std::uint32_t i = 0; i < count; ++i) {
        std::string key = reader.read_string();
        auto state_data = reader.read_bytes();
        auto state = deserialize_rehydration_state(state_data);
        if (state) {
            store.store(key, std::move(*state));
        }
    }

    return true;
}

// =============================================================================
// Rehydration Diagnostics
// =============================================================================

/// Format RehydrationState for debugging
[[nodiscard]] std::string format_rehydration_state(const RehydrationState& state) {
    std::ostringstream ss;

    ss << "RehydrationState {\n";
    ss << "  entries: " << state.count() << "\n";
    ss << "  empty: " << (state.is_empty() ? "true" : "false") << "\n";
    ss << "}\n";

    return ss.str();
}

/// Format RehydrationError for debugging
[[nodiscard]] std::string format_rehydration_error(const RehydrationError& error) {
    std::ostringstream ss;

    const char* kind_str = "Unknown";
    switch (error.kind) {
        case RehydrationErrorKind::MissingField: kind_str = "MissingField"; break;
        case RehydrationErrorKind::InvalidData: kind_str = "InvalidData"; break;
        case RehydrationErrorKind::VersionMismatch: kind_str = "VersionMismatch"; break;
        case RehydrationErrorKind::SerializationError: kind_str = "SerializationError"; break;
        case RehydrationErrorKind::BackendMismatch: kind_str = "BackendMismatch"; break;
        case RehydrationErrorKind::OutputMismatch: kind_str = "OutputMismatch"; break;
    }

    ss << "RehydrationError[" << kind_str << "]: " << error.message;
    return ss.str();
}

} // namespace void_compositor
