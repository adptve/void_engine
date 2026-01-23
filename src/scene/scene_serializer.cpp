/// @file scene_serializer.cpp
/// @brief Scene serialization to TOML format

#include <void_engine/scene/scene_serializer.hpp>

#include <fstream>
#include <iomanip>
#include <cmath>

namespace void_scene {

// =============================================================================
// SceneSerializer Implementation
// =============================================================================

void_core::Result<void> SceneSerializer::save(
    const SceneData& scene,
    const std::filesystem::path& path)
{
    std::string content = serialize(scene);

    std::ofstream file(path);
    if (!file.is_open()) {
        m_last_error = "Failed to open file for writing: " + path.string();
        return void_core::Err(void_core::Error(m_last_error));
    }

    file << content;
    file.close();

    if (!file.good()) {
        m_last_error = "Failed to write to file: " + path.string();
        return void_core::Err(void_core::Error(m_last_error));
    }

    return void_core::Ok();
}

std::string SceneSerializer::serialize(const SceneData& scene) {
    std::ostringstream ss;

    // Header comment
    ss << "# =============================================================================\n";
    ss << "# " << scene.metadata.name << " - Scene Definition\n";
    ss << "# =============================================================================\n";
    if (!scene.metadata.description.empty()) {
        ss << "# " << scene.metadata.description << "\n";
    }
    ss << "\n";

    // [scene] metadata
    serialize_metadata(ss, scene.metadata);

    // [[cameras]]
    if (!scene.cameras.empty()) {
        ss << "# =============================================================================\n";
        ss << "# Cameras\n";
        ss << "# =============================================================================\n\n";
        for (const auto& camera : scene.cameras) {
            serialize_camera(ss, camera);
        }
    }

    // [[lights]]
    if (!scene.lights.empty()) {
        ss << "# =============================================================================\n";
        ss << "# Lighting\n";
        ss << "# =============================================================================\n\n";
        for (const auto& light : scene.lights) {
            serialize_light(ss, light);
        }
    }

    // [environment]
    if (scene.environment) {
        ss << "# =============================================================================\n";
        ss << "# Environment\n";
        ss << "# =============================================================================\n\n";
        serialize_environment(ss, *scene.environment);
    }

    // [shadows]
    if (scene.shadows) {
        ss << "# =============================================================================\n";
        ss << "# Shadows\n";
        ss << "# =============================================================================\n\n";
        serialize_shadows(ss, *scene.shadows);
    }

    // [[entities]]
    if (!scene.entities.empty()) {
        ss << "# =============================================================================\n";
        ss << "# Entities\n";
        ss << "# =============================================================================\n\n";
        for (const auto& entity : scene.entities) {
            serialize_entity(ss, entity);
        }
    }

    // [[textures]]
    if (!scene.textures.empty()) {
        ss << "# =============================================================================\n";
        ss << "# Textures\n";
        ss << "# =============================================================================\n\n";
        for (const auto& texture : scene.textures) {
            serialize_texture(ss, texture);
        }
    }

    // [[particle_emitters]]
    if (!scene.particle_emitters.empty()) {
        ss << "# =============================================================================\n";
        ss << "# Particle Emitters\n";
        ss << "# =============================================================================\n\n";
        for (const auto& emitter : scene.particle_emitters) {
            serialize_particle_emitter(ss, emitter);
        }
    }

    // [picking]
    if (scene.picking) {
        serialize_picking(ss, *scene.picking);
    }

    // [spatial]
    if (scene.spatial) {
        serialize_spatial(ss, *scene.spatial);
    }

    // [input]
    if (scene.input) {
        serialize_input(ss, *scene.input);
    }

    // [debug]
    if (scene.debug) {
        serialize_debug(ss, *scene.debug);
    }

    return ss.str();
}

void SceneSerializer::write_header(std::ostringstream& ss, const std::string& section) {
    ss << "[" << section << "]\n";
}

void SceneSerializer::write_array_header(std::ostringstream& ss, const std::string& section) {
    ss << "[[" << section << "]]\n";
}

void SceneSerializer::serialize_metadata(std::ostringstream& ss, const SceneMetadata& meta) {
    write_header(ss, "scene");
    ss << "name = " << format_string(meta.name) << "\n";
    if (!meta.description.empty()) {
        ss << "description = " << format_string(meta.description) << "\n";
    }
    if (!meta.version.empty()) {
        ss << "version = " << format_string(meta.version) << "\n";
    }
    ss << "\n";
}

void SceneSerializer::serialize_camera(std::ostringstream& ss, const CameraData& camera) {
    write_array_header(ss, "cameras");
    ss << "name = " << format_string(camera.name) << "\n";
    ss << "active = " << format_bool(camera.active) << "\n";
    ss << "type = " << format_string(camera.type == CameraType::Perspective ? "perspective" : "orthographic") << "\n";

    // Control mode
    std::string mode_str = "orbit";
    if (camera.control_mode == CameraControlMode::Fps) mode_str = "fps";
    else if (camera.control_mode == CameraControlMode::Fly) mode_str = "fly";
    ss << "control_mode = " << format_string(mode_str) << "\n";

    ss << "\n[cameras.transform]\n";
    ss << "position = " << format_vec3(camera.transform.position) << "\n";
    ss << "target = " << format_vec3(camera.transform.target) << "\n";
    ss << "up = " << format_vec3(camera.transform.up) << "\n";

    if (camera.type == CameraType::Perspective) {
        ss << "\n[cameras.perspective]\n";
        ss << "fov = " << camera.perspective.fov << "\n";
        ss << "near = " << camera.perspective.near_plane << "\n";
        ss << "far = " << camera.perspective.far_plane << "\n";
        if (!camera.perspective.aspect.empty()) {
            ss << "aspect = " << format_string(camera.perspective.aspect) << "\n";
        }
    } else {
        ss << "\n[cameras.orthographic]\n";
        ss << "left = " << camera.orthographic.left << "\n";
        ss << "right = " << camera.orthographic.right << "\n";
        ss << "bottom = " << camera.orthographic.bottom << "\n";
        ss << "top = " << camera.orthographic.top << "\n";
        ss << "near = " << camera.orthographic.near_plane << "\n";
        ss << "far = " << camera.orthographic.far_plane << "\n";
    }
    ss << "\n";
}

void SceneSerializer::serialize_light(std::ostringstream& ss, const LightData& light) {
    write_array_header(ss, "lights");
    ss << "name = " << format_string(light.name) << "\n";
    ss << "enabled = " << format_bool(light.enabled) << "\n";

    std::string type_str = "directional";
    if (light.type == LightType::Point) type_str = "point";
    else if (light.type == LightType::Spot) type_str = "spot";
    ss << "type = " << format_string(type_str) << "\n";

    switch (light.type) {
        case LightType::Directional:
            ss << "\n[lights.directional]\n";
            ss << "direction = " << format_vec3(light.directional.direction) << "\n";
            ss << "color = " << format_color3(light.directional.color) << "\n";
            ss << "intensity = " << light.directional.intensity << "\n";
            ss << "cast_shadows = " << format_bool(light.directional.cast_shadows) << "\n";
            break;
        case LightType::Point:
            ss << "\n[lights.point]\n";
            ss << "position = " << format_vec3(light.point.position) << "\n";
            ss << "color = " << format_color3(light.point.color) << "\n";
            ss << "intensity = " << light.point.intensity << "\n";
            ss << "range = " << light.point.range << "\n";
            ss << "cast_shadows = " << format_bool(light.point.cast_shadows) << "\n";
            ss << "\n[lights.point.attenuation]\n";
            ss << "constant = " << light.point.attenuation.constant << "\n";
            ss << "linear = " << light.point.attenuation.linear << "\n";
            ss << "quadratic = " << light.point.attenuation.quadratic << "\n";
            break;
        case LightType::Spot:
            ss << "\n[lights.spot]\n";
            ss << "position = " << format_vec3(light.spot.position) << "\n";
            ss << "direction = " << format_vec3(light.spot.direction) << "\n";
            ss << "color = " << format_color3(light.spot.color) << "\n";
            ss << "intensity = " << light.spot.intensity << "\n";
            ss << "range = " << light.spot.range << "\n";
            ss << "inner_angle = " << light.spot.inner_angle << "\n";
            ss << "outer_angle = " << light.spot.outer_angle << "\n";
            ss << "cast_shadows = " << format_bool(light.spot.cast_shadows) << "\n";
            break;
    }
    ss << "\n";
}

void SceneSerializer::serialize_entity(std::ostringstream& ss, const EntityData& entity) {
    write_array_header(ss, "entities");
    ss << "name = " << format_string(entity.name) << "\n";

    // Handle mesh - could be simple string or path table
    if (entity.mesh.find('/') != std::string::npos || entity.mesh.find('\\') != std::string::npos) {
        ss << "mesh = { path = " << format_string(entity.mesh) << " }\n";
    } else {
        ss << "mesh = " << format_string(entity.mesh) << "\n";
    }

    if (entity.layer != "world") {
        ss << "layer = " << format_string(entity.layer) << "\n";
    }
    if (!entity.visible) {
        ss << "visible = false\n";
    }

    serialize_transform(ss, entity.transform, 0);

    if (entity.material) {
        serialize_material(ss, *entity.material, 0);
    }

    if (entity.animation && entity.animation->type != AnimationType::None) {
        serialize_animation(ss, *entity.animation, 0);
    }

    if (entity.pickable) {
        ss << "\n[entities.pickable]\n";
        ss << "enabled = " << format_bool(entity.pickable->enabled) << "\n";
        if (entity.pickable->priority != 0) {
            ss << "priority = " << entity.pickable->priority << "\n";
        }
        if (entity.pickable->highlight_on_hover) {
            ss << "highlight_on_hover = true\n";
        }
    }

    if (entity.input_events) {
        if (!entity.input_events->on_click.empty() ||
            !entity.input_events->on_pointer_enter.empty() ||
            !entity.input_events->on_pointer_exit.empty()) {
            ss << "\n[entities.input_events]\n";
            if (!entity.input_events->on_click.empty()) {
                ss << "on_click = " << format_string(entity.input_events->on_click) << "\n";
            }
            if (!entity.input_events->on_pointer_enter.empty()) {
                ss << "on_pointer_enter = " << format_string(entity.input_events->on_pointer_enter) << "\n";
            }
            if (!entity.input_events->on_pointer_exit.empty()) {
                ss << "on_pointer_exit = " << format_string(entity.input_events->on_pointer_exit) << "\n";
            }
        }
    }

    ss << "\n";
}

void SceneSerializer::serialize_transform(std::ostringstream& ss, const TransformData& transform, int) {
    ss << "\n[entities.transform]\n";
    ss << "position = " << format_vec3(transform.position) << "\n";

    bool has_rotation = std::abs(transform.rotation[0]) > 0.001f ||
                        std::abs(transform.rotation[1]) > 0.001f ||
                        std::abs(transform.rotation[2]) > 0.001f;
    if (has_rotation) {
        ss << "rotation = " << format_vec3(transform.rotation) << "\n";
    }

    Vec3 scale_v = transform.scale_vec3();
    bool is_uniform = std::abs(scale_v[0] - scale_v[1]) < 0.001f &&
                      std::abs(scale_v[1] - scale_v[2]) < 0.001f;
    bool is_unit = std::abs(scale_v[0] - 1.0f) < 0.001f;

    if (!is_unit) {
        if (is_uniform) {
            ss << "scale = " << scale_v[0] << "\n";
        } else {
            ss << "scale = " << format_vec3(scale_v) << "\n";
        }
    }
}

void SceneSerializer::serialize_material(std::ostringstream& ss, const MaterialData& material, int) {
    ss << "\n[entities.material]\n";

    // Albedo
    if (material.albedo.has_texture()) {
        ss << "albedo = { texture = " << format_string(*material.albedo.texture_path) << " }\n";
    } else if (material.albedo.has_color()) {
        const auto& c = *material.albedo.color;
        if (std::abs(c[3] - 1.0f) < 0.001f) {
            ss << "albedo = " << format_color3({c[0], c[1], c[2]}) << "\n";
        } else {
            ss << "albedo = " << format_vec4(c) << "\n";
        }
    }

    // Normal map
    if (material.normal_map) {
        ss << "normal_map = " << format_string(*material.normal_map) << "\n";
    }

    // Metallic
    if (material.metallic.has_texture()) {
        ss << "metallic = { texture = " << format_string(*material.metallic.texture_path) << " }\n";
    } else if (material.metallic.has_value()) {
        ss << "metallic = " << *material.metallic.value << "\n";
    }

    // Roughness
    if (material.roughness.has_texture()) {
        ss << "roughness = { texture = " << format_string(*material.roughness.texture_path) << " }\n";
    } else if (material.roughness.has_value()) {
        ss << "roughness = " << *material.roughness.value << "\n";
    }

    // Emissive
    if (material.emissive) {
        ss << "emissive = " << format_color3(*material.emissive) << "\n";
    }

    // Transmission
    if (material.transmission && material.transmission->factor > 0.001f) {
        ss << "\n[entities.material.transmission]\n";
        ss << "factor = " << material.transmission->factor << "\n";
        ss << "ior = " << material.transmission->ior << "\n";
        if (material.transmission->thickness > 0.001f) {
            ss << "thickness = " << material.transmission->thickness << "\n";
        }
    }

    // Sheen
    if (material.sheen) {
        ss << "\n[entities.material.sheen]\n";
        ss << "color = " << format_color3(material.sheen->color) << "\n";
        ss << "roughness = " << material.sheen->roughness << "\n";
    }

    // Clearcoat
    if (material.clearcoat && material.clearcoat->intensity > 0.001f) {
        ss << "\n[entities.material.clearcoat]\n";
        ss << "intensity = " << material.clearcoat->intensity << "\n";
        ss << "roughness = " << material.clearcoat->roughness << "\n";
    }

    // Anisotropy
    if (material.anisotropy && std::abs(material.anisotropy->strength) > 0.001f) {
        ss << "\n[entities.material.anisotropy]\n";
        ss << "strength = " << material.anisotropy->strength << "\n";
        ss << "rotation = " << material.anisotropy->rotation << "\n";
    }
}

void SceneSerializer::serialize_animation(std::ostringstream& ss, const AnimationData& anim, int) {
    ss << "\n[entities.animation]\n";

    std::string type_str;
    switch (anim.type) {
        case AnimationType::Rotate: type_str = "rotate"; break;
        case AnimationType::Oscillate: type_str = "oscillate"; break;
        case AnimationType::Orbit: type_str = "orbit"; break;
        case AnimationType::Pulse: type_str = "pulse"; break;
        case AnimationType::Path: type_str = "path"; break;
        default: return;
    }
    ss << "type = " << format_string(type_str) << "\n";

    switch (anim.type) {
        case AnimationType::Rotate:
            ss << "axis = " << format_vec3(anim.axis) << "\n";
            ss << "speed = " << anim.speed << "\n";
            break;

        case AnimationType::Oscillate:
            ss << "axis = " << format_vec3(anim.axis) << "\n";
            ss << "amplitude = " << anim.amplitude << "\n";
            ss << "frequency = " << anim.frequency << "\n";
            if (std::abs(anim.phase) > 0.001f) {
                ss << "phase = " << anim.phase << "\n";
            }
            if (anim.rotate) {
                ss << "rotate = true\n";
            }
            break;

        case AnimationType::Orbit:
            ss << "center = " << format_vec3(anim.center) << "\n";
            ss << "radius = " << anim.radius << "\n";
            ss << "speed = " << anim.speed << "\n";
            if (std::abs(anim.start_angle) > 0.001f) {
                ss << "start_angle = " << anim.start_angle << "\n";
            }
            if (anim.face_center) {
                ss << "face_center = true\n";
            }
            break;

        case AnimationType::Pulse:
            ss << "min_scale = " << anim.min_scale << "\n";
            ss << "max_scale = " << anim.max_scale << "\n";
            ss << "frequency = " << anim.frequency << "\n";
            break;

        case AnimationType::Path:
            ss << "points = [\n";
            for (const auto& pt : anim.points) {
                ss << "    " << format_vec3(pt) << ",\n";
            }
            ss << "]\n";
            ss << "duration = " << anim.duration << "\n";
            if (anim.loop_animation) {
                ss << "loop_animation = true\n";
            }
            if (anim.ping_pong) {
                ss << "ping_pong = true\n";
            }
            if (anim.orient_to_path) {
                ss << "orient_to_path = true\n";
            }
            if (anim.interpolation != "linear") {
                ss << "interpolation = " << format_string(anim.interpolation) << "\n";
            }
            break;

        default:
            break;
    }
}

void SceneSerializer::serialize_environment(std::ostringstream& ss, const EnvironmentData& env) {
    write_header(ss, "environment");
    if (env.environment_map) {
        ss << "environment_map = " << format_string(*env.environment_map) << "\n";
    }
    ss << "ambient_intensity = " << env.ambient_intensity << "\n";

    ss << "\n[environment.sky]\n";
    ss << "zenith_color = " << format_color3(env.sky.zenith_color) << "\n";
    ss << "horizon_color = " << format_color3(env.sky.horizon_color) << "\n";
    ss << "ground_color = " << format_color3(env.sky.ground_color) << "\n";
    ss << "sun_size = " << env.sky.sun_size << "\n";
    ss << "sun_intensity = " << env.sky.sun_intensity << "\n";
    ss << "sun_falloff = " << env.sky.sun_falloff << "\n";
    if (env.sky.fog_density > 0.001f) {
        ss << "fog_density = " << env.sky.fog_density << "\n";
    }
    ss << "\n";
}

void SceneSerializer::serialize_shadows(std::ostringstream& ss, const ShadowData& shadows) {
    write_header(ss, "shadows");
    ss << "enabled = " << format_bool(shadows.enabled) << "\n";
    ss << "atlas_size = " << shadows.atlas_size << "\n";
    ss << "max_shadow_distance = " << shadows.max_shadow_distance << "\n";
    ss << "shadow_fade_distance = " << shadows.shadow_fade_distance << "\n";

    ss << "\n[shadows.cascades]\n";
    ss << "count = " << shadows.cascades.count << "\n";
    ss << "split_scheme = " << format_string(shadows.cascades.split_scheme) << "\n";
    ss << "lambda = " << shadows.cascades.lambda << "\n";

    for (const auto& level : shadows.cascades.levels) {
        ss << "\n[[shadows.cascades.levels]]\n";
        ss << "resolution = " << level.resolution << "\n";
        ss << "distance = " << level.distance << "\n";
        ss << "bias = " << level.bias << "\n";
    }

    ss << "\n[shadows.filtering]\n";
    ss << "method = " << format_string(shadows.filtering.method) << "\n";
    ss << "pcf_samples = " << shadows.filtering.pcf_samples << "\n";
    ss << "pcf_radius = " << shadows.filtering.pcf_radius << "\n";
    ss << "soft_shadows = " << format_bool(shadows.filtering.soft_shadows) << "\n";
    ss << "contact_hardening = " << format_bool(shadows.filtering.contact_hardening) << "\n";
    ss << "\n";
}

void SceneSerializer::serialize_picking(std::ostringstream& ss, const PickingData& picking) {
    ss << "# =============================================================================\n";
    ss << "# Picking Configuration\n";
    ss << "# =============================================================================\n\n";
    write_header(ss, "picking");
    ss << "enabled = " << format_bool(picking.enabled) << "\n";
    ss << "method = " << format_string(picking.method) << "\n";
    ss << "max_distance = " << picking.max_distance << "\n";

    if (!picking.layer_mask.empty()) {
        ss << "layer_mask = [";
        for (std::size_t i = 0; i < picking.layer_mask.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << format_string(picking.layer_mask[i]);
        }
        ss << "]\n";
    }

    ss << "\n[picking.gpu]\n";
    ss << "buffer_size = " << format_vec2(picking.gpu.buffer_size) << "\n";
    ss << "readback_delay = " << picking.gpu.readback_delay << "\n";
    ss << "\n";
}

void SceneSerializer::serialize_spatial(std::ostringstream& ss, const SpatialData& spatial) {
    ss << "# =============================================================================\n";
    ss << "# Spatial Configuration\n";
    ss << "# =============================================================================\n\n";
    write_header(ss, "spatial");
    ss << "structure = " << format_string(spatial.structure) << "\n";
    ss << "auto_rebuild = " << format_bool(spatial.auto_rebuild) << "\n";
    ss << "rebuild_threshold = " << spatial.rebuild_threshold << "\n";

    ss << "\n[spatial.bvh]\n";
    ss << "max_leaf_size = " << spatial.bvh.max_leaf_size << "\n";
    ss << "build_quality = " << format_string(spatial.bvh.build_quality) << "\n";

    ss << "\n[spatial.queries]\n";
    ss << "frustum_culling = " << format_bool(spatial.queries.frustum_culling) << "\n";
    ss << "occlusion_culling = " << format_bool(spatial.queries.occlusion_culling) << "\n";
    ss << "max_query_results = " << spatial.queries.max_query_results << "\n";
    ss << "\n";
}

void SceneSerializer::serialize_debug(std::ostringstream& ss, const DebugData& debug) {
    ss << "# =============================================================================\n";
    ss << "# Debug Configuration\n";
    ss << "# =============================================================================\n\n";
    write_header(ss, "debug");
    ss << "enabled = " << format_bool(debug.enabled) << "\n";

    ss << "\n[debug.stats]\n";
    ss << "enabled = " << format_bool(debug.stats.enabled) << "\n";
    ss << "position = " << format_string(debug.stats.position) << "\n";
    ss << "font_size = " << debug.stats.font_size << "\n";
    ss << "background_alpha = " << debug.stats.background_alpha << "\n";

    ss << "\n[debug.stats.display]\n";
    ss << "fps = " << format_bool(debug.stats.fps) << "\n";
    ss << "frame_time = " << format_bool(debug.stats.frame_time) << "\n";
    ss << "draw_calls = " << format_bool(debug.stats.draw_calls) << "\n";
    ss << "triangles = " << format_bool(debug.stats.triangles) << "\n";

    ss << "\n[debug.visualization]\n";
    ss << "enabled = " << format_bool(debug.visualization.enabled) << "\n";
    ss << "bounds = " << format_bool(debug.visualization.bounds) << "\n";
    ss << "wireframe = " << format_bool(debug.visualization.wireframe) << "\n";
    ss << "normals = " << format_bool(debug.visualization.normals) << "\n";

    ss << "\n[debug.controls]\n";
    ss << "toggle_key = " << format_string(debug.controls.toggle_key) << "\n";
    ss << "cycle_mode_key = " << format_string(debug.controls.cycle_mode_key) << "\n";
    ss << "reload_shaders_key = " << format_string(debug.controls.reload_shaders_key) << "\n";
    ss << "\n";
}

void SceneSerializer::serialize_input(std::ostringstream& ss, const InputData& input) {
    ss << "# =============================================================================\n";
    ss << "# Input Configuration\n";
    ss << "# =============================================================================\n\n";
    write_header(ss, "input");

    ss << "\n[input.camera]\n";
    ss << "orbit_button = " << format_string(input.camera.orbit_button) << "\n";
    ss << "pan_button = " << format_string(input.camera.pan_button) << "\n";
    ss << "zoom_scroll = " << format_bool(input.camera.zoom_scroll) << "\n";
    ss << "orbit_sensitivity = " << input.camera.orbit_sensitivity << "\n";
    ss << "pan_sensitivity = " << input.camera.pan_sensitivity << "\n";
    ss << "zoom_sensitivity = " << input.camera.zoom_sensitivity << "\n";
    ss << "invert_y = " << format_bool(input.camera.invert_y) << "\n";
    ss << "invert_x = " << format_bool(input.camera.invert_x) << "\n";
    ss << "min_distance = " << input.camera.min_distance << "\n";
    ss << "max_distance = " << input.camera.max_distance << "\n";

    if (!input.bindings.empty()) {
        ss << "\n[input.bindings]\n";
        for (const auto& [key, value] : input.bindings) {
            ss << key << " = " << format_string(value) << "\n";
        }
    }
    ss << "\n";
}

void SceneSerializer::serialize_texture(std::ostringstream& ss, const TextureData& texture) {
    write_array_header(ss, "textures");
    ss << "name = " << format_string(texture.name) << "\n";
    ss << "path = " << format_string(texture.path) << "\n";
    ss << "srgb = " << format_bool(texture.srgb) << "\n";
    ss << "mipmap = " << format_bool(texture.mipmap) << "\n";
    if (texture.hdr) {
        ss << "hdr = true\n";
    }
    ss << "\n";
}

void SceneSerializer::serialize_particle_emitter(std::ostringstream& ss, const ParticleEmitterData& emitter) {
    write_array_header(ss, "particle_emitters");
    ss << "name = " << format_string(emitter.name) << "\n";
    ss << "position = " << format_vec3(emitter.position) << "\n";
    ss << "emit_rate = " << emitter.emit_rate << "\n";
    ss << "max_particles = " << emitter.max_particles << "\n";
    ss << "lifetime = " << format_vec2(emitter.lifetime) << "\n";
    ss << "speed = " << format_vec2(emitter.speed) << "\n";
    ss << "size = " << format_vec2(emitter.size) << "\n";
    ss << "color_start = " << format_vec4(emitter.color_start) << "\n";
    ss << "color_end = " << format_vec4(emitter.color_end) << "\n";
    ss << "gravity = " << format_vec3(emitter.gravity) << "\n";
    ss << "spread = " << emitter.spread << "\n";
    ss << "direction = " << format_vec3(emitter.direction) << "\n";
    ss << "enabled = " << format_bool(emitter.enabled) << "\n";
    ss << "\n";
}

// Value formatting helpers

std::string SceneSerializer::format_vec2(const Vec2& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << v[0] << ", " << v[1] << "]";
    return ss.str();
}

std::string SceneSerializer::format_vec3(const Vec3& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << v[0] << ", " << v[1] << ", " << v[2] << "]";
    return ss.str();
}

std::string SceneSerializer::format_vec4(const Vec4& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "[" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "]";
    return ss.str();
}

std::string SceneSerializer::format_color3(const Color3& c) {
    return format_vec3(c);
}

std::string SceneSerializer::format_string(const std::string& s) {
    std::ostringstream ss;
    ss << "\"";
    for (char c : s) {
        if (c == '\\') ss << "\\\\";
        else if (c == '"') ss << "\\\"";
        else if (c == '\n') ss << "\\n";
        else if (c == '\t') ss << "\\t";
        else ss << c;
    }
    ss << "\"";
    return ss.str();
}

std::string SceneSerializer::format_bool(bool b) {
    return b ? "true" : "false";
}

std::string SceneSerializer::indent_str(int level) {
    return std::string(level * 4, ' ');
}

// =============================================================================
// SceneDiffer Implementation
// =============================================================================

SceneDiffer::SceneDiff SceneDiffer::diff(const SceneData& old_scene, const SceneData& new_scene) {
    SceneDiff result;

    // Build entity maps
    std::unordered_map<std::string, const EntityData*> old_entities;
    std::unordered_map<std::string, const EntityData*> new_entities;

    for (const auto& e : old_scene.entities) {
        old_entities[e.name] = &e;
    }
    for (const auto& e : new_scene.entities) {
        new_entities[e.name] = &e;
    }

    // Find added and modified entities
    for (const auto& [name, new_ent] : new_entities) {
        auto it = old_entities.find(name);
        if (it == old_entities.end()) {
            EntityDiff diff;
            diff.name = name;
            diff.added = true;
            result.entity_diffs.push_back(diff);
        } else {
            const EntityData* old_ent = it->second;
            EntityDiff diff;
            diff.name = name;
            diff.transform_changed = !compare_transforms(old_ent->transform, new_ent->transform);

            if (old_ent->material && new_ent->material) {
                diff.material_changed = !compare_materials(*old_ent->material, *new_ent->material);
            } else if (old_ent->material || new_ent->material) {
                diff.material_changed = true;
            }

            if (old_ent->animation && new_ent->animation) {
                diff.animation_changed = !compare_animations(*old_ent->animation, *new_ent->animation);
            } else if (old_ent->animation || new_ent->animation) {
                diff.animation_changed = true;
            }

            if (diff.transform_changed || diff.material_changed || diff.animation_changed) {
                result.entity_diffs.push_back(diff);
            }
        }
    }

    // Find removed entities
    for (const auto& [name, old_ent] : old_entities) {
        if (new_entities.find(name) == new_entities.end()) {
            EntityDiff diff;
            diff.name = name;
            diff.removed = true;
            result.entity_diffs.push_back(diff);
        }
    }

    // Compare other sections
    result.cameras_changed = old_scene.cameras.size() != new_scene.cameras.size();
    result.lights_changed = old_scene.lights.size() != new_scene.lights.size();

    return result;
}

bool SceneDiffer::compare_transforms(const TransformData& a, const TransformData& b) {
    const float epsilon = 0.0001f;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(a.position[i] - b.position[i]) > epsilon) return false;
        if (std::abs(a.rotation[i] - b.rotation[i]) > epsilon) return false;
    }

    Vec3 scale_a = a.scale_vec3();
    Vec3 scale_b = b.scale_vec3();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(scale_a[i] - scale_b[i]) > epsilon) return false;
    }

    return true;
}

bool SceneDiffer::compare_materials(const MaterialData& a, const MaterialData& b) {
    // Basic comparison - could be more thorough
    if (a.albedo.texture_path != b.albedo.texture_path) return false;
    if (a.albedo.color != b.albedo.color) return false;
    if (a.metallic.value != b.metallic.value) return false;
    if (a.roughness.value != b.roughness.value) return false;
    return true;
}

bool SceneDiffer::compare_animations(const AnimationData& a, const AnimationData& b) {
    if (a.type != b.type) return false;
    if (std::abs(a.speed - b.speed) > 0.001f) return false;
    return true;
}

} // namespace void_scene
