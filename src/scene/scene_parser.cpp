/// @file scene_parser.cpp
/// @brief Scene parser implementation (JSON format)

#include <void_engine/scene/scene_parser.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace void_scene {

using json = nlohmann::json;

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/// Parse Vec2 from JSON array
Vec2 parse_vec2(const json& arr, Vec2 default_val = {0.0f, 0.0f}) {
    if (!arr.is_array() || arr.size() < 2) return default_val;
    return {
        arr[0].get<float>(),
        arr[1].get<float>()
    };
}

/// Parse Vec3 from JSON array
Vec3 parse_vec3(const json& arr, Vec3 default_val = {0.0f, 0.0f, 0.0f}) {
    if (!arr.is_array() || arr.size() < 3) return default_val;
    return {
        arr[0].get<float>(),
        arr[1].get<float>(),
        arr[2].get<float>()
    };
}

/// Parse Vec4/Color4 from JSON array
Vec4 parse_vec4(const json& arr, Vec4 default_val = {0.0f, 0.0f, 0.0f, 1.0f}) {
    if (!arr.is_array() || arr.size() < 4) return default_val;
    return {
        arr[0].get<float>(),
        arr[1].get<float>(),
        arr[2].get<float>(),
        arr[3].get<float>()
    };
}

/// Parse Color3 from JSON array
Color3 parse_color3(const json& arr, Color3 default_val = {1.0f, 1.0f, 1.0f}) {
    if (!arr.is_array() || arr.size() < 3) return default_val;
    return {
        arr[0].get<float>(),
        arr[1].get<float>(),
        arr[2].get<float>()
    };
}

/// Parse TextureOrValue from JSON node
TextureOrValue parse_texture_or_value(const json& node) {
    TextureOrValue result;

    if (node.is_null()) return result;

    // Check if it's an object with "texture" key
    if (node.is_object()) {
        if (node.contains("texture")) {
            result.texture_path = node["texture"].get<std::string>();
        }
    }
    // Check if it's an array (color)
    else if (node.is_array()) {
        if (node.size() == 3) {
            auto c = parse_color3(node);
            result.color = {c[0], c[1], c[2], 1.0f};
        } else if (node.size() >= 4) {
            result.color = parse_vec4(node);
        }
    }
    // Check if it's a number (value)
    else if (node.is_number()) {
        result.value = node.get<float>();
    }

    return result;
}

/// Parse transform from JSON object
TransformData parse_transform(const json& obj) {
    TransformData result;

    if (!obj.is_object()) return result;

    if (obj.contains("position") && obj["position"].is_array()) {
        result.position = parse_vec3(obj["position"]);
    }

    if (obj.contains("rotation") && obj["rotation"].is_array()) {
        result.rotation = parse_vec3(obj["rotation"]);
    }

    // Scale can be uniform (number) or non-uniform (array)
    if (obj.contains("scale")) {
        if (obj["scale"].is_array()) {
            result.scale = parse_vec3(obj["scale"], {1.0f, 1.0f, 1.0f});
        } else if (obj["scale"].is_number()) {
            result.scale = obj["scale"].get<float>();
        }
    }

    return result;
}

/// Parse material from JSON object
MaterialData parse_material(const json& obj) {
    MaterialData result;

    if (!obj.is_object()) return result;

    if (obj.contains("albedo")) {
        result.albedo = parse_texture_or_value(obj["albedo"]);
    }

    if (obj.contains("normal_map") && obj["normal_map"].is_string()) {
        result.normal_map = obj["normal_map"].get<std::string>();
    }

    if (obj.contains("metallic")) {
        result.metallic = parse_texture_or_value(obj["metallic"]);
    }

    if (obj.contains("roughness")) {
        result.roughness = parse_texture_or_value(obj["roughness"]);
    }

    if (obj.contains("emissive") && obj["emissive"].is_array()) {
        result.emissive = parse_color3(obj["emissive"]);
    }

    // Transmission (Phase 7)
    if (obj.contains("transmission") && obj["transmission"].is_object()) {
        const auto& trans = obj["transmission"];
        TransmissionData td;
        td.factor = trans.value("factor", 0.0f);
        td.ior = trans.value("ior", 1.5f);
        td.thickness = trans.value("thickness", 0.0f);
        if (trans.contains("attenuation_color") && trans["attenuation_color"].is_array()) {
            td.attenuation_color = parse_color3(trans["attenuation_color"]);
        }
        td.attenuation_distance = trans.value("attenuation_distance", 1.0f);
        result.transmission = td;
    }

    // Sheen (Phase 7)
    if (obj.contains("sheen") && obj["sheen"].is_object()) {
        const auto& sheen = obj["sheen"];
        SheenData sd;
        if (sheen.contains("color") && sheen["color"].is_array()) {
            sd.color = parse_color3(sheen["color"]);
        }
        sd.roughness = sheen.value("roughness", 0.5f);
        result.sheen = sd;
    }

    // Clearcoat (Phase 7)
    if (obj.contains("clearcoat") && obj["clearcoat"].is_object()) {
        const auto& clearcoat = obj["clearcoat"];
        ClearcoatData cd;
        cd.intensity = clearcoat.value("intensity", 0.0f);
        cd.roughness = clearcoat.value("roughness", 0.0f);
        result.clearcoat = cd;
    }

    // Anisotropy (Phase 7)
    if (obj.contains("anisotropy") && obj["anisotropy"].is_object()) {
        const auto& aniso = obj["anisotropy"];
        AnisotropyData ad;
        ad.strength = aniso.value("strength", 0.0f);
        ad.rotation = aniso.value("rotation", 0.0f);
        result.anisotropy = ad;
    }

    return result;
}

/// Parse animation from JSON object
AnimationData parse_animation(const json& obj) {
    AnimationData result;

    if (!obj.is_object()) return result;

    if (obj.contains("type") && obj["type"].is_string()) {
        std::string type_str = obj["type"].get<std::string>();
        if (type_str == "rotate") result.type = AnimationType::Rotate;
        else if (type_str == "oscillate") result.type = AnimationType::Oscillate;
        else if (type_str == "orbit") result.type = AnimationType::Orbit;
        else if (type_str == "pulse") result.type = AnimationType::Pulse;
        else if (type_str == "path") result.type = AnimationType::Path;
    }

    if (obj.contains("axis") && obj["axis"].is_array()) {
        result.axis = parse_vec3(obj["axis"], {0.0f, 1.0f, 0.0f});
    }

    result.speed = obj.value("speed", 1.0f);
    result.amplitude = obj.value("amplitude", 1.0f);
    result.frequency = obj.value("frequency", 1.0f);
    result.phase = obj.value("phase", 0.0f);

    // Oscillate specific
    result.rotate = obj.value("rotate", false);

    // Orbit
    if (obj.contains("center") && obj["center"].is_array()) {
        result.center = parse_vec3(obj["center"]);
    }
    result.radius = obj.value("radius", 1.0f);
    result.start_angle = obj.value("start_angle", 0.0f);
    result.face_center = obj.value("face_center", false);

    // Pulse
    result.min_scale = obj.value("min_scale", 1.0f);
    result.max_scale = obj.value("max_scale", 1.0f);

    // Path
    if (obj.contains("points") && obj["points"].is_array()) {
        for (const auto& pt : obj["points"]) {
            if (pt.is_array()) {
                result.points.push_back(parse_vec3(pt));
            }
        }
    }
    result.duration = obj.value("duration", 1.0f);
    result.loop_animation = obj.value("loop_animation", false);
    result.ping_pong = obj.value("ping_pong", false);
    if (obj.contains("interpolation") && obj["interpolation"].is_string()) {
        result.interpolation = obj["interpolation"].get<std::string>();
    }
    result.orient_to_path = obj.value("orient_to_path", false);
    if (obj.contains("easing") && obj["easing"].is_string()) {
        result.easing = obj["easing"].get<std::string>();
    }

    return result;
}

/// Parse entity from JSON object
EntityData parse_entity(const json& obj) {
    EntityData result;

    if (!obj.is_object()) return result;

    if (obj.contains("name") && obj["name"].is_string()) {
        result.name = obj["name"].get<std::string>();
    }

    // Mesh can be a simple string "cube" or an object { "path": "models/Fox.glb" }
    if (obj.contains("mesh")) {
        if (obj["mesh"].is_string()) {
            result.mesh = obj["mesh"].get<std::string>();
        } else if (obj["mesh"].is_object() && obj["mesh"].contains("path")) {
            result.mesh = obj["mesh"]["path"].get<std::string>();
        }
    }

    if (obj.contains("layer") && obj["layer"].is_string()) {
        result.layer = obj["layer"].get<std::string>();
    }
    result.visible = obj.value("visible", true);

    // Transform
    if (obj.contains("transform") && obj["transform"].is_object()) {
        result.transform = parse_transform(obj["transform"]);
    }

    // Material
    if (obj.contains("material") && obj["material"].is_object()) {
        result.material = parse_material(obj["material"]);
    }

    // Animation
    if (obj.contains("animation") && obj["animation"].is_object()) {
        result.animation = parse_animation(obj["animation"]);
    }

    // Pickable
    if (obj.contains("pickable") && obj["pickable"].is_object()) {
        const auto& pickable = obj["pickable"];
        PickableData pd;
        pd.enabled = pickable.value("enabled", true);
        pd.priority = pickable.value("priority", 0);
        if (pickable.contains("bounds") && pickable["bounds"].is_string()) {
            pd.bounds = pickable["bounds"].get<std::string>();
        }
        pd.highlight_on_hover = pickable.value("highlight_on_hover", false);
        result.pickable = pd;
    }

    // Input events
    if (obj.contains("input_events") && obj["input_events"].is_object()) {
        const auto& events = obj["input_events"];
        InputEventsData ie;
        if (events.contains("on_click") && events["on_click"].is_string()) {
            ie.on_click = events["on_click"].get<std::string>();
        }
        if (events.contains("on_pointer_enter") && events["on_pointer_enter"].is_string()) {
            ie.on_pointer_enter = events["on_pointer_enter"].get<std::string>();
        }
        if (events.contains("on_pointer_exit") && events["on_pointer_exit"].is_string()) {
            ie.on_pointer_exit = events["on_pointer_exit"].get<std::string>();
        }
        result.input_events = ie;
    }

    return result;
}

/// Parse camera from JSON object
CameraData parse_camera(const json& obj) {
    CameraData result;

    if (!obj.is_object()) return result;

    if (obj.contains("name") && obj["name"].is_string()) {
        result.name = obj["name"].get<std::string>();
    }
    result.active = obj.value("active", false);

    if (obj.contains("type") && obj["type"].is_string()) {
        std::string type_str = obj["type"].get<std::string>();
        if (type_str == "perspective") result.type = CameraType::Perspective;
        else if (type_str == "orthographic") result.type = CameraType::Orthographic;
    }

    if (obj.contains("control_mode") && obj["control_mode"].is_string()) {
        std::string mode_str = obj["control_mode"].get<std::string>();
        if (mode_str == "fps") result.control_mode = CameraControlMode::Fps;
        else if (mode_str == "orbit") result.control_mode = CameraControlMode::Orbit;
        else if (mode_str == "fly") result.control_mode = CameraControlMode::Fly;
    }

    // Transform
    if (obj.contains("transform") && obj["transform"].is_object()) {
        const auto& transform = obj["transform"];
        if (transform.contains("position") && transform["position"].is_array()) {
            result.transform.position = parse_vec3(transform["position"], {0.0f, 0.0f, 5.0f});
        }
        if (transform.contains("target") && transform["target"].is_array()) {
            result.transform.target = parse_vec3(transform["target"]);
        }
        if (transform.contains("up") && transform["up"].is_array()) {
            result.transform.up = parse_vec3(transform["up"], {0.0f, 1.0f, 0.0f});
        }
    }

    // Perspective
    if (obj.contains("perspective") && obj["perspective"].is_object()) {
        const auto& persp = obj["perspective"];
        result.perspective.fov = persp.value("fov", 60.0f);
        result.perspective.near_plane = persp.value("near", 0.1f);
        result.perspective.far_plane = persp.value("far", 1000.0f);
        if (persp.contains("aspect") && persp["aspect"].is_string()) {
            result.perspective.aspect = persp["aspect"].get<std::string>();
        }
    }

    // Orthographic
    if (obj.contains("orthographic") && obj["orthographic"].is_object()) {
        const auto& ortho = obj["orthographic"];
        result.orthographic.left = ortho.value("left", -10.0f);
        result.orthographic.right = ortho.value("right", 10.0f);
        result.orthographic.bottom = ortho.value("bottom", -10.0f);
        result.orthographic.top = ortho.value("top", 10.0f);
        result.orthographic.near_plane = ortho.value("near", 0.1f);
        result.orthographic.far_plane = ortho.value("far", 1000.0f);
    }

    return result;
}

/// Parse light from JSON object
LightData parse_light(const json& obj) {
    LightData result;

    if (!obj.is_object()) return result;

    if (obj.contains("name") && obj["name"].is_string()) {
        result.name = obj["name"].get<std::string>();
    }
    result.enabled = obj.value("enabled", true);

    if (obj.contains("type") && obj["type"].is_string()) {
        std::string type_str = obj["type"].get<std::string>();
        if (type_str == "directional") result.type = LightType::Directional;
        else if (type_str == "point") result.type = LightType::Point;
        else if (type_str == "spot") result.type = LightType::Spot;
    }

    // Directional
    if (obj.contains("directional") && obj["directional"].is_object()) {
        const auto& dir = obj["directional"];
        if (dir.contains("direction") && dir["direction"].is_array()) {
            result.directional.direction = parse_vec3(dir["direction"], {0.0f, -1.0f, 0.0f});
        }
        if (dir.contains("color") && dir["color"].is_array()) {
            result.directional.color = parse_color3(dir["color"]);
        }
        result.directional.intensity = dir.value("intensity", 1.0f);
        result.directional.cast_shadows = dir.value("cast_shadows", false);
    }

    // Point
    if (obj.contains("point") && obj["point"].is_object()) {
        const auto& pt = obj["point"];
        if (pt.contains("position") && pt["position"].is_array()) {
            result.point.position = parse_vec3(pt["position"]);
        }
        if (pt.contains("color") && pt["color"].is_array()) {
            result.point.color = parse_color3(pt["color"]);
        }
        result.point.intensity = pt.value("intensity", 1.0f);
        result.point.range = pt.value("range", 10.0f);
        result.point.cast_shadows = pt.value("cast_shadows", false);

        if (pt.contains("attenuation") && pt["attenuation"].is_object()) {
            const auto& atten = pt["attenuation"];
            result.point.attenuation.constant = atten.value("constant", 1.0f);
            result.point.attenuation.linear = atten.value("linear", 0.09f);
            result.point.attenuation.quadratic = atten.value("quadratic", 0.032f);
        }
    }

    // Spot
    if (obj.contains("spot") && obj["spot"].is_object()) {
        const auto& spot = obj["spot"];
        if (spot.contains("position") && spot["position"].is_array()) {
            result.spot.position = parse_vec3(spot["position"]);
        }
        if (spot.contains("direction") && spot["direction"].is_array()) {
            result.spot.direction = parse_vec3(spot["direction"], {0.0f, -1.0f, 0.0f});
        }
        if (spot.contains("color") && spot["color"].is_array()) {
            result.spot.color = parse_color3(spot["color"]);
        }
        result.spot.intensity = spot.value("intensity", 1.0f);
        result.spot.range = spot.value("range", 10.0f);
        result.spot.inner_angle = spot.value("inner_angle", 30.0f);
        result.spot.outer_angle = spot.value("outer_angle", 45.0f);
        result.spot.cast_shadows = spot.value("cast_shadows", false);
    }

    return result;
}

/// Parse particle emitter from JSON object
ParticleEmitterData parse_particle_emitter(const json& obj) {
    ParticleEmitterData result;

    if (!obj.is_object()) return result;

    if (obj.contains("name") && obj["name"].is_string()) {
        result.name = obj["name"].get<std::string>();
    }
    if (obj.contains("position") && obj["position"].is_array()) {
        result.position = parse_vec3(obj["position"]);
    }
    result.emit_rate = obj.value("emit_rate", 100.0f);
    result.max_particles = obj.value("max_particles", 1000u);
    if (obj.contains("lifetime") && obj["lifetime"].is_array()) {
        result.lifetime = parse_vec2(obj["lifetime"], {1.0f, 2.0f});
    }
    if (obj.contains("speed") && obj["speed"].is_array()) {
        result.speed = parse_vec2(obj["speed"], {1.0f, 2.0f});
    }
    if (obj.contains("size") && obj["size"].is_array()) {
        result.size = parse_vec2(obj["size"], {0.1f, 0.2f});
    }
    if (obj.contains("color_start") && obj["color_start"].is_array()) {
        result.color_start = parse_vec4(obj["color_start"], {1.0f, 1.0f, 1.0f, 1.0f});
    }
    if (obj.contains("color_end") && obj["color_end"].is_array()) {
        result.color_end = parse_vec4(obj["color_end"], {1.0f, 1.0f, 1.0f, 0.0f});
    }
    if (obj.contains("gravity") && obj["gravity"].is_array()) {
        result.gravity = parse_vec3(obj["gravity"], {0.0f, -9.8f, 0.0f});
    }
    result.spread = obj.value("spread", 0.5f);
    if (obj.contains("direction") && obj["direction"].is_array()) {
        result.direction = parse_vec3(obj["direction"], {0.0f, 1.0f, 0.0f});
    }
    result.enabled = obj.value("enabled", true);

    return result;
}

/// Parse texture from JSON object
TextureData parse_texture(const json& obj) {
    TextureData result;

    if (!obj.is_object()) return result;

    if (obj.contains("name") && obj["name"].is_string()) {
        result.name = obj["name"].get<std::string>();
    }
    if (obj.contains("path") && obj["path"].is_string()) {
        result.path = obj["path"].get<std::string>();
    }
    result.srgb = obj.value("srgb", true);
    result.mipmap = obj.value("mipmap", true);
    result.hdr = obj.value("hdr", false);

    return result;
}

} // anonymous namespace

// =============================================================================
// SceneParser Implementation
// =============================================================================

void_core::Result<SceneData> SceneParser::parse(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_last_error = "Failed to open file: " + path.string();
        return void_core::Err<SceneData>(void_core::Error(m_last_error));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parse_string(buffer.str(), path.string());
}

void_core::Result<SceneData> SceneParser::parse_string(
    const std::string& content,
    const std::string& source_name)
{
    SceneData scene;

    try {
        json root = json::parse(content);

        // Parse "scene" metadata
        if (root.contains("scene") && root["scene"].is_object()) {
            const auto& scene_obj = root["scene"];
            if (scene_obj.contains("name") && scene_obj["name"].is_string()) {
                scene.metadata.name = scene_obj["name"].get<std::string>();
            }
            if (scene_obj.contains("description") && scene_obj["description"].is_string()) {
                scene.metadata.description = scene_obj["description"].get<std::string>();
            }
            if (scene_obj.contains("version") && scene_obj["version"].is_string()) {
                scene.metadata.version = scene_obj["version"].get<std::string>();
            }
        }

        // Parse "cameras" array
        if (root.contains("cameras") && root["cameras"].is_array()) {
            for (const auto& cam : root["cameras"]) {
                scene.cameras.push_back(parse_camera(cam));
            }
        }

        // Parse "lights" array
        if (root.contains("lights") && root["lights"].is_array()) {
            for (const auto& light : root["lights"]) {
                scene.lights.push_back(parse_light(light));
            }
        }

        // Parse "shadows"
        if (root.contains("shadows") && root["shadows"].is_object()) {
            const auto& shadows = root["shadows"];
            ShadowData sd;
            sd.enabled = shadows.value("enabled", true);
            sd.atlas_size = shadows.value("atlas_size", 4096u);
            sd.max_shadow_distance = shadows.value("max_shadow_distance", 50.0f);
            sd.shadow_fade_distance = shadows.value("shadow_fade_distance", 5.0f);

            if (shadows.contains("cascades") && shadows["cascades"].is_object()) {
                const auto& cascades = shadows["cascades"];
                sd.cascades.count = cascades.value("count", 3u);
                if (cascades.contains("split_scheme") && cascades["split_scheme"].is_string()) {
                    sd.cascades.split_scheme = cascades["split_scheme"].get<std::string>();
                }
                sd.cascades.lambda = cascades.value("lambda", 0.5f);

                if (cascades.contains("levels") && cascades["levels"].is_array()) {
                    for (const auto& level : cascades["levels"]) {
                        ShadowCascadeLevel lvl;
                        lvl.resolution = level.value("resolution", 1024u);
                        lvl.distance = level.value("distance", 50.0f);
                        lvl.bias = level.value("bias", 0.001f);
                        sd.cascades.levels.push_back(lvl);
                    }
                }
            }

            if (shadows.contains("filtering") && shadows["filtering"].is_object()) {
                const auto& filtering = shadows["filtering"];
                if (filtering.contains("method") && filtering["method"].is_string()) {
                    sd.filtering.method = filtering["method"].get<std::string>();
                }
                sd.filtering.pcf_samples = filtering.value("pcf_samples", 16u);
                sd.filtering.pcf_radius = filtering.value("pcf_radius", 1.5f);
                sd.filtering.soft_shadows = filtering.value("soft_shadows", true);
                sd.filtering.contact_hardening = filtering.value("contact_hardening", false);
            }

            scene.shadows = sd;
        }

        // Parse "environment"
        if (root.contains("environment") && root["environment"].is_object()) {
            const auto& env = root["environment"];
            EnvironmentData ed;
            if (env.contains("environment_map") && env["environment_map"].is_string()) {
                ed.environment_map = env["environment_map"].get<std::string>();
            }
            ed.ambient_intensity = env.value("ambient_intensity", 0.1f);

            if (env.contains("sky") && env["sky"].is_object()) {
                const auto& sky = env["sky"];
                if (sky.contains("zenith_color") && sky["zenith_color"].is_array()) {
                    ed.sky.zenith_color = parse_color3(sky["zenith_color"]);
                }
                if (sky.contains("horizon_color") && sky["horizon_color"].is_array()) {
                    ed.sky.horizon_color = parse_color3(sky["horizon_color"]);
                }
                if (sky.contains("ground_color") && sky["ground_color"].is_array()) {
                    ed.sky.ground_color = parse_color3(sky["ground_color"]);
                }
                ed.sky.sun_size = sky.value("sun_size", 0.03f);
                ed.sky.sun_intensity = sky.value("sun_intensity", 50.0f);
                ed.sky.sun_falloff = sky.value("sun_falloff", 3.0f);
                ed.sky.fog_density = sky.value("fog_density", 0.0f);
            }

            scene.environment = ed;
        }

        // Parse "picking" (Phase 10)
        if (root.contains("picking") && root["picking"].is_object()) {
            const auto& picking = root["picking"];
            PickingData pd;
            pd.enabled = picking.value("enabled", true);
            if (picking.contains("method") && picking["method"].is_string()) {
                pd.method = picking["method"].get<std::string>();
            }
            pd.max_distance = picking.value("max_distance", 100.0f);

            if (picking.contains("layer_mask") && picking["layer_mask"].is_array()) {
                for (const auto& layer : picking["layer_mask"]) {
                    if (layer.is_string()) {
                        pd.layer_mask.push_back(layer.get<std::string>());
                    }
                }
            }

            if (picking.contains("gpu") && picking["gpu"].is_object()) {
                const auto& gpu = picking["gpu"];
                if (gpu.contains("buffer_size") && gpu["buffer_size"].is_array()) {
                    pd.gpu.buffer_size = parse_vec2(gpu["buffer_size"], {256.0f, 256.0f});
                }
                pd.gpu.readback_delay = gpu.value("readback_delay", 1);
            }

            scene.picking = pd;
        }

        // Parse "spatial" (Phase 14)
        if (root.contains("spatial") && root["spatial"].is_object()) {
            const auto& spatial = root["spatial"];
            SpatialData sd;
            if (spatial.contains("structure") && spatial["structure"].is_string()) {
                sd.structure = spatial["structure"].get<std::string>();
            }
            sd.auto_rebuild = spatial.value("auto_rebuild", true);
            sd.rebuild_threshold = spatial.value("rebuild_threshold", 0.3f);

            if (spatial.contains("bvh") && spatial["bvh"].is_object()) {
                const auto& bvh = spatial["bvh"];
                sd.bvh.max_leaf_size = bvh.value("max_leaf_size", 4);
                if (bvh.contains("build_quality") && bvh["build_quality"].is_string()) {
                    sd.bvh.build_quality = bvh["build_quality"].get<std::string>();
                }
            }

            if (spatial.contains("queries") && spatial["queries"].is_object()) {
                const auto& queries = spatial["queries"];
                sd.queries.frustum_culling = queries.value("frustum_culling", true);
                sd.queries.occlusion_culling = queries.value("occlusion_culling", false);
                sd.queries.max_query_results = queries.value("max_query_results", 500);
            }

            scene.spatial = sd;
        }

        // Parse "entities" array
        if (root.contains("entities") && root["entities"].is_array()) {
            for (const auto& entity : root["entities"]) {
                scene.entities.push_back(parse_entity(entity));
            }
        }

        // Parse "particle_emitters" array
        if (root.contains("particle_emitters") && root["particle_emitters"].is_array()) {
            for (const auto& emitter : root["particle_emitters"]) {
                scene.particle_emitters.push_back(parse_particle_emitter(emitter));
            }
        }

        // Parse "textures" array
        if (root.contains("textures") && root["textures"].is_array()) {
            for (const auto& tex : root["textures"]) {
                scene.textures.push_back(parse_texture(tex));
            }
        }

        // Parse "debug"
        if (root.contains("debug") && root["debug"].is_object()) {
            const auto& debug = root["debug"];
            DebugData dd;
            dd.enabled = debug.value("enabled", false);

            // Parse debug stats
            if (debug.contains("stats") && debug["stats"].is_object()) {
                const auto& stats = debug["stats"];
                dd.stats.enabled = stats.value("enabled", false);
                if (stats.contains("position") && stats["position"].is_string()) {
                    dd.stats.position = stats["position"].get<std::string>();
                }
                dd.stats.font_size = stats.value("font_size", 14);
                dd.stats.background_alpha = stats.value("background_alpha", 0.7f);

                if (stats.contains("display") && stats["display"].is_object()) {
                    const auto& display = stats["display"];
                    dd.stats.fps = display.value("fps", true);
                    dd.stats.frame_time = display.value("frame_time", true);
                    dd.stats.draw_calls = display.value("draw_calls", true);
                    dd.stats.triangles = display.value("triangles", true);
                    dd.stats.entities_total = display.value("entities_total", true);
                    dd.stats.entities_visible = display.value("entities_visible", true);
                    dd.stats.gpu_memory = display.value("gpu_memory", false);
                    dd.stats.cpu_time = display.value("cpu_time", true);
                }
            }

            if (debug.contains("visualization") && debug["visualization"].is_object()) {
                const auto& viz = debug["visualization"];
                dd.visualization.enabled = viz.value("enabled", false);
                dd.visualization.bounds = viz.value("bounds", false);
                dd.visualization.wireframe = viz.value("wireframe", false);
                dd.visualization.normals = viz.value("normals", false);
                dd.visualization.light_volumes = viz.value("light_volumes", false);
                dd.visualization.shadow_cascades = viz.value("shadow_cascades", false);
                dd.visualization.lod_levels = viz.value("lod_levels", false);
                dd.visualization.skeleton = viz.value("skeleton", false);
            }

            if (debug.contains("controls") && debug["controls"].is_object()) {
                const auto& controls = debug["controls"];
                if (controls.contains("toggle_key") && controls["toggle_key"].is_string()) {
                    dd.controls.toggle_key = controls["toggle_key"].get<std::string>();
                }
                if (controls.contains("cycle_mode_key") && controls["cycle_mode_key"].is_string()) {
                    dd.controls.cycle_mode_key = controls["cycle_mode_key"].get<std::string>();
                }
                if (controls.contains("reload_shaders_key") && controls["reload_shaders_key"].is_string()) {
                    dd.controls.reload_shaders_key = controls["reload_shaders_key"].get<std::string>();
                }
            }

            scene.debug = dd;
        }

        // Parse "input"
        if (root.contains("input") && root["input"].is_object()) {
            const auto& input = root["input"];
            InputData id;

            if (input.contains("camera") && input["camera"].is_object()) {
                const auto& camera = input["camera"];
                if (camera.contains("orbit_button") && camera["orbit_button"].is_string()) {
                    id.camera.orbit_button = camera["orbit_button"].get<std::string>();
                }
                if (camera.contains("pan_button") && camera["pan_button"].is_string()) {
                    id.camera.pan_button = camera["pan_button"].get<std::string>();
                }
                id.camera.zoom_scroll = camera.value("zoom_scroll", true);
                id.camera.orbit_sensitivity = camera.value("orbit_sensitivity", 0.005f);
                id.camera.pan_sensitivity = camera.value("pan_sensitivity", 0.01f);
                id.camera.zoom_sensitivity = camera.value("zoom_sensitivity", 0.1f);
                id.camera.invert_y = camera.value("invert_y", false);
                id.camera.invert_x = camera.value("invert_x", false);
                id.camera.min_distance = camera.value("min_distance", 0.5f);
                id.camera.max_distance = camera.value("max_distance", 50.0f);
            }

            if (input.contains("bindings") && input["bindings"].is_object()) {
                for (auto& [key, value] : input["bindings"].items()) {
                    if (value.is_string()) {
                        id.bindings[key] = value.get<std::string>();
                    }
                }
            }

            scene.input = id;
        }

    } catch (const json::parse_error& err) {
        m_last_error = "JSON parse error: " + std::string(err.what());
        return void_core::Err<SceneData>(void_core::Error(m_last_error));
    } catch (const json::type_error& err) {
        m_last_error = "JSON type error: " + std::string(err.what());
        return void_core::Err<SceneData>(void_core::Error(m_last_error));
    }

    m_last_error.clear();
    return scene;
}

// =============================================================================
// HotReloadableScene Implementation
// =============================================================================

HotReloadableScene::HotReloadableScene(std::filesystem::path path)
    : m_path(std::move(path))
{
    // Initial load
    auto result = reload();
    if (!result) {
        // Log error but don't throw
    }
}

void_core::Result<void> HotReloadableScene::reload() {
    auto result = m_parser.parse(m_path);
    if (!result) {
        return void_core::Err(result.error());
    }

    m_data = std::move(result).value();

    // Notify callback
    if (m_on_reload) {
        m_on_reload(m_data);
    }

    return void_core::Ok();
}

void_core::Result<void_core::HotReloadSnapshot> HotReloadableScene::snapshot() {
    // For now, just serialize the path - we'll reload from disk
    void_core::HotReloadSnapshot snap;
    snap.type_id = std::type_index(typeid(HotReloadableScene));
    snap.type_name = "HotReloadableScene";
    snap.version = m_version;

    // Store path in data
    snap.data.assign(m_path.string().begin(), m_path.string().end());

    return snap;
}

void_core::Result<void> HotReloadableScene::restore(void_core::HotReloadSnapshot snapshot) {
    // Restore path from snapshot
    std::string path_str(snapshot.data.begin(), snapshot.data.end());
    m_path = path_str;

    // Reload from disk
    return reload();
}

bool HotReloadableScene::is_compatible(const void_core::Version& new_version) const {
    // Allow any version for now
    return true;
}

void_core::Version HotReloadableScene::current_version() const {
    return m_version;
}

// =============================================================================
// SceneManager Implementation
// =============================================================================

SceneManager::SceneManager() = default;
SceneManager::~SceneManager() = default;

void_core::Result<void> SceneManager::initialize() {
    return void_core::Ok();
}

void SceneManager::shutdown() {
    m_scenes.clear();
    m_current_scene_path.clear();
}

void_core::Result<void> SceneManager::load_scene(const std::filesystem::path& path) {
    // Check if already loaded
    auto it = m_scenes.find(path);
    if (it != m_scenes.end()) {
        m_current_scene_path = path;

        // Notify callback
        if (m_on_scene_loaded) {
            m_on_scene_loaded(path, it->second->data());
        }

        return void_core::Ok();
    }

    // Create new scene
    auto scene = std::make_unique<HotReloadableScene>(path);

    // Check if parse succeeded
    SceneParser parser;
    auto result = parser.parse(path);
    if (!result) {
        return void_core::Err(result.error());
    }

    // Set up hot-reload callback
    scene->on_reload([this, path](const SceneData& data) {
        if (m_on_scene_loaded) {
            m_on_scene_loaded(path, data);
        }
    });

    // Track file modification time for hot-reload polling
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        m_file_timestamps[path] = mtime;
    }

    m_scenes[path] = std::move(scene);
    m_current_scene_path = path;

    // Notify callback
    if (m_on_scene_loaded) {
        m_on_scene_loaded(path, m_scenes[path]->data());
    }

    return void_core::Ok();
}

const SceneData* SceneManager::current_scene() const {
    auto it = m_scenes.find(m_current_scene_path);
    return it != m_scenes.end() ? &it->second->data() : nullptr;
}

const SceneData* SceneManager::get_scene(const std::filesystem::path& path) const {
    auto it = m_scenes.find(path);
    return it != m_scenes.end() ? &it->second->data() : nullptr;
}

void SceneManager::set_hot_reload_enabled(bool enabled) {
    m_hot_reload_enabled = enabled;
}

void SceneManager::update() {
    if (!m_hot_reload_enabled) {
        return;
    }

    // Poll file timestamps for changes
    for (auto& [path, scene] : m_scenes) {
        std::error_code ec;
        auto current_mtime = std::filesystem::last_write_time(path, ec);
        if (ec) {
            continue;  // File might not exist temporarily
        }

        auto& stored_mtime = m_file_timestamps[path];
        if (current_mtime != stored_mtime) {
            // File has been modified, reload it
            stored_mtime = current_mtime;

            auto reload_result = scene->reload();
            if (reload_result) {
                // Reload succeeded, callback will be invoked by scene->on_reload
            }
            // If reload fails, we silently ignore (file might be in mid-write)
        }
    }
}

void SceneManager::on_scene_loaded(
    std::function<void(const std::filesystem::path&, const SceneData&)> callback)
{
    m_on_scene_loaded = std::move(callback);
}

} // namespace void_scene
