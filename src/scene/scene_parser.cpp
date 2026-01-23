/// @file scene_parser.cpp
/// @brief Scene parser implementation

#include <void_engine/scene/scene_parser.hpp>

#include <toml++/toml.hpp>

#include <fstream>
#include <sstream>

namespace void_scene {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/// Parse Vec2 from TOML array
Vec2 parse_vec2(const toml::array* arr, Vec2 default_val = {0.0f, 0.0f}) {
    if (!arr || arr->size() < 2) return default_val;
    return {
        arr->get(0)->value_or(default_val[0]),
        arr->get(1)->value_or(default_val[1])
    };
}

/// Parse Vec3 from TOML array
Vec3 parse_vec3(const toml::array* arr, Vec3 default_val = {0.0f, 0.0f, 0.0f}) {
    if (!arr || arr->size() < 3) return default_val;
    return {
        arr->get(0)->value_or(default_val[0]),
        arr->get(1)->value_or(default_val[1]),
        arr->get(2)->value_or(default_val[2])
    };
}

/// Parse Vec4/Color4 from TOML array
Vec4 parse_vec4(const toml::array* arr, Vec4 default_val = {0.0f, 0.0f, 0.0f, 1.0f}) {
    if (!arr || arr->size() < 4) return default_val;
    return {
        arr->get(0)->value_or(default_val[0]),
        arr->get(1)->value_or(default_val[1]),
        arr->get(2)->value_or(default_val[2]),
        arr->get(3)->value_or(default_val[3])
    };
}

/// Parse Color3 from TOML array
Color3 parse_color3(const toml::array* arr, Color3 default_val = {1.0f, 1.0f, 1.0f}) {
    if (!arr || arr->size() < 3) return default_val;
    return {
        arr->get(0)->value_or(default_val[0]),
        arr->get(1)->value_or(default_val[1]),
        arr->get(2)->value_or(default_val[2])
    };
}

/// Parse TextureOrValue from TOML node
TextureOrValue parse_texture_or_value(const toml::node* node) {
    TextureOrValue result;

    if (!node) return result;

    // Check if it's a table with "texture" key
    if (auto tbl = node->as_table()) {
        if (auto tex = (*tbl)["texture"].value<std::string>()) {
            result.texture_path = *tex;
        }
    }
    // Check if it's an array (color)
    else if (auto arr = node->as_array()) {
        if (arr->size() == 3) {
            auto c = parse_color3(arr);
            result.color = {c[0], c[1], c[2], 1.0f};
        } else if (arr->size() >= 4) {
            result.color = parse_vec4(arr);
        }
    }
    // Check if it's a number (value)
    else if (auto val = node->value<double>()) {
        result.value = static_cast<float>(*val);
    }

    return result;
}

/// Parse transform from TOML table
TransformData parse_transform(const toml::table* tbl) {
    TransformData result;

    if (!tbl) return result;

    if (auto pos = (*tbl)["position"].as_array()) {
        result.position = parse_vec3(pos);
    }

    if (auto rot = (*tbl)["rotation"].as_array()) {
        result.rotation = parse_vec3(rot);
    }

    // Scale can be uniform (float) or non-uniform (array)
    if (auto scale_arr = (*tbl)["scale"].as_array()) {
        result.scale = parse_vec3(scale_arr, {1.0f, 1.0f, 1.0f});
    } else if (auto scale_val = (*tbl)["scale"].value<double>()) {
        result.scale = static_cast<float>(*scale_val);
    }

    return result;
}

/// Parse material from TOML table
MaterialData parse_material(const toml::table* tbl) {
    MaterialData result;

    if (!tbl) return result;

    result.albedo = parse_texture_or_value(tbl->get("albedo"));

    if (auto normal = (*tbl)["normal_map"].value<std::string>()) {
        result.normal_map = *normal;
    }

    result.metallic = parse_texture_or_value(tbl->get("metallic"));
    result.roughness = parse_texture_or_value(tbl->get("roughness"));

    if (auto emissive = (*tbl)["emissive"].as_array()) {
        result.emissive = parse_color3(emissive);
    }

    // Transmission (Phase 7)
    if (auto trans = (*tbl)["transmission"].as_table()) {
        TransmissionData td;
        td.factor = (*trans)["factor"].value_or(0.0f);
        td.ior = (*trans)["ior"].value_or(1.5f);
        td.thickness = (*trans)["thickness"].value_or(0.0f);
        if (auto att_color = (*trans)["attenuation_color"].as_array()) {
            td.attenuation_color = parse_color3(att_color);
        }
        td.attenuation_distance = (*trans)["attenuation_distance"].value_or(1.0f);
        result.transmission = td;
    }

    // Sheen (Phase 7)
    if (auto sheen = (*tbl)["sheen"].as_table()) {
        SheenData sd;
        if (auto color = (*sheen)["color"].as_array()) {
            sd.color = parse_color3(color);
        }
        sd.roughness = (*sheen)["roughness"].value_or(0.5f);
        result.sheen = sd;
    }

    // Clearcoat (Phase 7)
    if (auto clearcoat = (*tbl)["clearcoat"].as_table()) {
        ClearcoatData cd;
        cd.intensity = (*clearcoat)["intensity"].value_or(0.0f);
        cd.roughness = (*clearcoat)["roughness"].value_or(0.0f);
        result.clearcoat = cd;
    }

    // Anisotropy (Phase 7)
    if (auto aniso = (*tbl)["anisotropy"].as_table()) {
        AnisotropyData ad;
        ad.strength = (*aniso)["strength"].value_or(0.0f);
        ad.rotation = (*aniso)["rotation"].value_or(0.0f);
        result.anisotropy = ad;
    }

    return result;
}

/// Parse animation from TOML table
AnimationData parse_animation(const toml::table* tbl) {
    AnimationData result;

    if (!tbl) return result;

    if (auto type_str = (*tbl)["type"].value<std::string>()) {
        if (*type_str == "rotate") result.type = AnimationType::Rotate;
        else if (*type_str == "oscillate") result.type = AnimationType::Oscillate;
        else if (*type_str == "orbit") result.type = AnimationType::Orbit;
        else if (*type_str == "pulse") result.type = AnimationType::Pulse;
        else if (*type_str == "path") result.type = AnimationType::Path;
    }

    if (auto axis = (*tbl)["axis"].as_array()) {
        result.axis = parse_vec3(axis, {0.0f, 1.0f, 0.0f});
    }

    result.speed = (*tbl)["speed"].value_or(1.0f);
    result.amplitude = (*tbl)["amplitude"].value_or(1.0f);
    result.frequency = (*tbl)["frequency"].value_or(1.0f);
    result.phase = (*tbl)["phase"].value_or(0.0f);

    // Oscillate specific
    result.rotate = (*tbl)["rotate"].value_or(false);

    // Orbit
    if (auto center = (*tbl)["center"].as_array()) {
        result.center = parse_vec3(center);
    }
    result.radius = (*tbl)["radius"].value_or(1.0f);
    result.start_angle = (*tbl)["start_angle"].value_or(0.0f);
    result.face_center = (*tbl)["face_center"].value_or(false);

    // Pulse
    result.min_scale = (*tbl)["min_scale"].value_or(1.0f);
    result.max_scale = (*tbl)["max_scale"].value_or(1.0f);

    // Path
    if (auto points = (*tbl)["points"].as_array()) {
        for (const auto& pt : *points) {
            if (auto pt_arr = pt.as_array()) {
                result.points.push_back(parse_vec3(pt_arr));
            }
        }
    }
    result.duration = (*tbl)["duration"].value_or(1.0f);
    result.loop_animation = (*tbl)["loop_animation"].value_or(false);
    result.ping_pong = (*tbl)["ping_pong"].value_or(false);
    if (auto interp = (*tbl)["interpolation"].value<std::string>()) {
        result.interpolation = *interp;
    }
    result.orient_to_path = (*tbl)["orient_to_path"].value_or(false);
    if (auto ease = (*tbl)["easing"].value<std::string>()) {
        result.easing = *ease;
    }

    return result;
}

/// Parse entity from TOML table
EntityData parse_entity(const toml::table* tbl) {
    EntityData result;

    if (!tbl) return result;

    if (auto name = (*tbl)["name"].value<std::string>()) {
        result.name = *name;
    }

    // Mesh can be a simple string "cube" or a table { path = "models/Fox.glb" }
    if (auto mesh_str = (*tbl)["mesh"].value<std::string>()) {
        result.mesh = *mesh_str;
    } else if (auto mesh_tbl = (*tbl)["mesh"].as_table()) {
        if (auto path = (*mesh_tbl)["path"].value<std::string>()) {
            result.mesh = *path;
        }
    }

    if (auto layer = (*tbl)["layer"].value<std::string>()) {
        result.layer = *layer;
    }
    result.visible = (*tbl)["visible"].value_or(true);

    // Transform
    if (auto transform = (*tbl)["transform"].as_table()) {
        result.transform = parse_transform(transform);
    }

    // Material
    if (auto material = (*tbl)["material"].as_table()) {
        result.material = parse_material(material);
    }

    // Animation
    if (auto animation = (*tbl)["animation"].as_table()) {
        result.animation = parse_animation(animation);
    }

    // Pickable
    if (auto pickable = (*tbl)["pickable"].as_table()) {
        PickableData pd;
        pd.enabled = (*pickable)["enabled"].value_or(true);
        pd.priority = (*pickable)["priority"].value_or(0);
        if (auto bounds = (*pickable)["bounds"].value<std::string>()) {
            pd.bounds = *bounds;
        }
        pd.highlight_on_hover = (*pickable)["highlight_on_hover"].value_or(false);
        result.pickable = pd;
    }

    // Input events
    if (auto events = (*tbl)["input_events"].as_table()) {
        InputEventsData ie;
        if (auto on_click = (*events)["on_click"].value<std::string>()) {
            ie.on_click = *on_click;
        }
        if (auto on_enter = (*events)["on_pointer_enter"].value<std::string>()) {
            ie.on_pointer_enter = *on_enter;
        }
        if (auto on_exit = (*events)["on_pointer_exit"].value<std::string>()) {
            ie.on_pointer_exit = *on_exit;
        }
        result.input_events = ie;
    }

    return result;
}

/// Parse camera from TOML table
CameraData parse_camera(const toml::table* tbl) {
    CameraData result;

    if (!tbl) return result;

    if (auto name = (*tbl)["name"].value<std::string>()) {
        result.name = *name;
    }
    result.active = (*tbl)["active"].value_or(false);

    if (auto type_str = (*tbl)["type"].value<std::string>()) {
        if (*type_str == "perspective") result.type = CameraType::Perspective;
        else if (*type_str == "orthographic") result.type = CameraType::Orthographic;
    }

    if (auto mode_str = (*tbl)["control_mode"].value<std::string>()) {
        if (*mode_str == "fps") result.control_mode = CameraControlMode::Fps;
        else if (*mode_str == "orbit") result.control_mode = CameraControlMode::Orbit;
        else if (*mode_str == "fly") result.control_mode = CameraControlMode::Fly;
    }

    // Transform
    if (auto transform = (*tbl)["transform"].as_table()) {
        if (auto pos = (*transform)["position"].as_array()) {
            result.transform.position = parse_vec3(pos, {0.0f, 0.0f, 5.0f});
        }
        if (auto target = (*transform)["target"].as_array()) {
            result.transform.target = parse_vec3(target);
        }
        if (auto up = (*transform)["up"].as_array()) {
            result.transform.up = parse_vec3(up, {0.0f, 1.0f, 0.0f});
        }
    }

    // Perspective
    if (auto persp = (*tbl)["perspective"].as_table()) {
        result.perspective.fov = (*persp)["fov"].value_or(60.0f);
        result.perspective.near_plane = (*persp)["near"].value_or(0.1f);
        result.perspective.far_plane = (*persp)["far"].value_or(1000.0f);
        if (auto aspect = (*persp)["aspect"].value<std::string>()) {
            result.perspective.aspect = *aspect;
        }
    }

    // Orthographic
    if (auto ortho = (*tbl)["orthographic"].as_table()) {
        result.orthographic.left = (*ortho)["left"].value_or(-10.0f);
        result.orthographic.right = (*ortho)["right"].value_or(10.0f);
        result.orthographic.bottom = (*ortho)["bottom"].value_or(-10.0f);
        result.orthographic.top = (*ortho)["top"].value_or(10.0f);
        result.orthographic.near_plane = (*ortho)["near"].value_or(0.1f);
        result.orthographic.far_plane = (*ortho)["far"].value_or(1000.0f);
    }

    return result;
}

/// Parse light from TOML table
LightData parse_light(const toml::table* tbl) {
    LightData result;

    if (!tbl) return result;

    if (auto name = (*tbl)["name"].value<std::string>()) {
        result.name = *name;
    }
    result.enabled = (*tbl)["enabled"].value_or(true);

    if (auto type_str = (*tbl)["type"].value<std::string>()) {
        if (*type_str == "directional") result.type = LightType::Directional;
        else if (*type_str == "point") result.type = LightType::Point;
        else if (*type_str == "spot") result.type = LightType::Spot;
    }

    // Directional
    if (auto dir = (*tbl)["directional"].as_table()) {
        if (auto direction = (*dir)["direction"].as_array()) {
            result.directional.direction = parse_vec3(direction, {0.0f, -1.0f, 0.0f});
        }
        if (auto color = (*dir)["color"].as_array()) {
            result.directional.color = parse_color3(color);
        }
        result.directional.intensity = (*dir)["intensity"].value_or(1.0f);
        result.directional.cast_shadows = (*dir)["cast_shadows"].value_or(false);
    }

    // Point
    if (auto pt = (*tbl)["point"].as_table()) {
        if (auto pos = (*pt)["position"].as_array()) {
            result.point.position = parse_vec3(pos);
        }
        if (auto color = (*pt)["color"].as_array()) {
            result.point.color = parse_color3(color);
        }
        result.point.intensity = (*pt)["intensity"].value_or(1.0f);
        result.point.range = (*pt)["range"].value_or(10.0f);
        result.point.cast_shadows = (*pt)["cast_shadows"].value_or(false);

        if (auto atten = (*pt)["attenuation"].as_table()) {
            result.point.attenuation.constant = (*atten)["constant"].value_or(1.0f);
            result.point.attenuation.linear = (*atten)["linear"].value_or(0.09f);
            result.point.attenuation.quadratic = (*atten)["quadratic"].value_or(0.032f);
        }
    }

    // Spot
    if (auto spot = (*tbl)["spot"].as_table()) {
        if (auto pos = (*spot)["position"].as_array()) {
            result.spot.position = parse_vec3(pos);
        }
        if (auto dir = (*spot)["direction"].as_array()) {
            result.spot.direction = parse_vec3(dir, {0.0f, -1.0f, 0.0f});
        }
        if (auto color = (*spot)["color"].as_array()) {
            result.spot.color = parse_color3(color);
        }
        result.spot.intensity = (*spot)["intensity"].value_or(1.0f);
        result.spot.range = (*spot)["range"].value_or(10.0f);
        result.spot.inner_angle = (*spot)["inner_angle"].value_or(30.0f);
        result.spot.outer_angle = (*spot)["outer_angle"].value_or(45.0f);
        result.spot.cast_shadows = (*spot)["cast_shadows"].value_or(false);
    }

    return result;
}

/// Parse particle emitter from TOML table
ParticleEmitterData parse_particle_emitter(const toml::table* tbl) {
    ParticleEmitterData result;

    if (!tbl) return result;

    if (auto name = (*tbl)["name"].value<std::string>()) {
        result.name = *name;
    }
    if (auto pos = (*tbl)["position"].as_array()) {
        result.position = parse_vec3(pos);
    }
    result.emit_rate = (*tbl)["emit_rate"].value_or(100.0f);
    result.max_particles = static_cast<std::uint32_t>((*tbl)["max_particles"].value_or(1000));
    if (auto lifetime = (*tbl)["lifetime"].as_array()) {
        result.lifetime = parse_vec2(lifetime, {1.0f, 2.0f});
    }
    if (auto speed = (*tbl)["speed"].as_array()) {
        result.speed = parse_vec2(speed, {1.0f, 2.0f});
    }
    if (auto size = (*tbl)["size"].as_array()) {
        result.size = parse_vec2(size, {0.1f, 0.2f});
    }
    if (auto color_start = (*tbl)["color_start"].as_array()) {
        result.color_start = parse_vec4(color_start, {1.0f, 1.0f, 1.0f, 1.0f});
    }
    if (auto color_end = (*tbl)["color_end"].as_array()) {
        result.color_end = parse_vec4(color_end, {1.0f, 1.0f, 1.0f, 0.0f});
    }
    if (auto gravity = (*tbl)["gravity"].as_array()) {
        result.gravity = parse_vec3(gravity, {0.0f, -9.8f, 0.0f});
    }
    result.spread = (*tbl)["spread"].value_or(0.5f);
    if (auto direction = (*tbl)["direction"].as_array()) {
        result.direction = parse_vec3(direction, {0.0f, 1.0f, 0.0f});
    }
    result.enabled = (*tbl)["enabled"].value_or(true);

    return result;
}

/// Parse texture from TOML table
TextureData parse_texture(const toml::table* tbl) {
    TextureData result;

    if (!tbl) return result;

    if (auto name = (*tbl)["name"].value<std::string>()) {
        result.name = *name;
    }
    if (auto path = (*tbl)["path"].value<std::string>()) {
        result.path = *path;
    }
    result.srgb = (*tbl)["srgb"].value_or(true);
    result.mipmap = (*tbl)["mipmap"].value_or(true);
    result.hdr = (*tbl)["hdr"].value_or(false);

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
        toml::table tbl = toml::parse(content, source_name);

        // Parse [scene] metadata
        if (auto scene_tbl = tbl["scene"].as_table()) {
            if (auto name = (*scene_tbl)["name"].value<std::string>()) {
                scene.metadata.name = *name;
            }
            if (auto desc = (*scene_tbl)["description"].value<std::string>()) {
                scene.metadata.description = *desc;
            }
            if (auto ver = (*scene_tbl)["version"].value<std::string>()) {
                scene.metadata.version = *ver;
            }
        }

        // Parse [[cameras]] array
        if (auto cameras = tbl["cameras"].as_array()) {
            for (const auto& cam_node : *cameras) {
                if (auto cam_tbl = cam_node.as_table()) {
                    scene.cameras.push_back(parse_camera(cam_tbl));
                }
            }
        }

        // Parse [[lights]] array
        if (auto lights = tbl["lights"].as_array()) {
            for (const auto& light_node : *lights) {
                if (auto light_tbl = light_node.as_table()) {
                    scene.lights.push_back(parse_light(light_tbl));
                }
            }
        }

        // Parse [shadows]
        if (auto shadows = tbl["shadows"].as_table()) {
            ShadowData sd;
            sd.enabled = (*shadows)["enabled"].value_or(true);
            sd.atlas_size = static_cast<std::uint32_t>((*shadows)["atlas_size"].value_or(4096));
            sd.max_shadow_distance = (*shadows)["max_shadow_distance"].value_or(50.0f);
            sd.shadow_fade_distance = (*shadows)["shadow_fade_distance"].value_or(5.0f);

            if (auto cascades = (*shadows)["cascades"].as_table()) {
                sd.cascades.count = static_cast<std::uint32_t>((*cascades)["count"].value_or(3));
                if (auto scheme = (*cascades)["split_scheme"].value<std::string>()) {
                    sd.cascades.split_scheme = *scheme;
                }
                sd.cascades.lambda = (*cascades)["lambda"].value_or(0.5f);

                if (auto levels = (*cascades)["levels"].as_array()) {
                    for (const auto& level_node : *levels) {
                        if (auto level_tbl = level_node.as_table()) {
                            ShadowCascadeLevel level;
                            level.resolution = static_cast<std::uint32_t>((*level_tbl)["resolution"].value_or(1024));
                            level.distance = (*level_tbl)["distance"].value_or(50.0f);
                            level.bias = (*level_tbl)["bias"].value_or(0.001f);
                            sd.cascades.levels.push_back(level);
                        }
                    }
                }
            }

            if (auto filtering = (*shadows)["filtering"].as_table()) {
                if (auto method = (*filtering)["method"].value<std::string>()) {
                    sd.filtering.method = *method;
                }
                sd.filtering.pcf_samples = static_cast<std::uint32_t>((*filtering)["pcf_samples"].value_or(16));
                sd.filtering.pcf_radius = (*filtering)["pcf_radius"].value_or(1.5f);
                sd.filtering.soft_shadows = (*filtering)["soft_shadows"].value_or(true);
                sd.filtering.contact_hardening = (*filtering)["contact_hardening"].value_or(false);
            }

            scene.shadows = sd;
        }

        // Parse [environment]
        if (auto env = tbl["environment"].as_table()) {
            EnvironmentData ed;
            if (auto env_map = (*env)["environment_map"].value<std::string>()) {
                ed.environment_map = *env_map;
            }
            ed.ambient_intensity = (*env)["ambient_intensity"].value_or(0.1f);

            if (auto sky = (*env)["sky"].as_table()) {
                if (auto zenith = (*sky)["zenith_color"].as_array()) {
                    ed.sky.zenith_color = parse_color3(zenith);
                }
                if (auto horizon = (*sky)["horizon_color"].as_array()) {
                    ed.sky.horizon_color = parse_color3(horizon);
                }
                if (auto ground = (*sky)["ground_color"].as_array()) {
                    ed.sky.ground_color = parse_color3(ground);
                }
                ed.sky.sun_size = (*sky)["sun_size"].value_or(0.03f);
                ed.sky.sun_intensity = (*sky)["sun_intensity"].value_or(50.0f);
                ed.sky.sun_falloff = (*sky)["sun_falloff"].value_or(3.0f);
                ed.sky.fog_density = (*sky)["fog_density"].value_or(0.0f);
            }

            scene.environment = ed;
        }

        // Parse [picking] (Phase 10)
        if (auto picking = tbl["picking"].as_table()) {
            PickingData pd;
            pd.enabled = (*picking)["enabled"].value_or(true);
            if (auto method = (*picking)["method"].value<std::string>()) {
                pd.method = *method;
            }
            pd.max_distance = (*picking)["max_distance"].value_or(100.0f);

            if (auto layer_mask = (*picking)["layer_mask"].as_array()) {
                for (const auto& layer : *layer_mask) {
                    if (auto str = layer.value<std::string>()) {
                        pd.layer_mask.push_back(*str);
                    }
                }
            }

            if (auto gpu = (*picking)["gpu"].as_table()) {
                if (auto buffer_size = (*gpu)["buffer_size"].as_array()) {
                    pd.gpu.buffer_size = parse_vec2(buffer_size, {256.0f, 256.0f});
                }
                pd.gpu.readback_delay = (*gpu)["readback_delay"].value_or(1);
            }

            scene.picking = pd;
        }

        // Parse [spatial] (Phase 14)
        if (auto spatial = tbl["spatial"].as_table()) {
            SpatialData sd;
            if (auto structure = (*spatial)["structure"].value<std::string>()) {
                sd.structure = *structure;
            }
            sd.auto_rebuild = (*spatial)["auto_rebuild"].value_or(true);
            sd.rebuild_threshold = (*spatial)["rebuild_threshold"].value_or(0.3f);

            if (auto bvh = (*spatial)["bvh"].as_table()) {
                sd.bvh.max_leaf_size = (*bvh)["max_leaf_size"].value_or(4);
                if (auto quality = (*bvh)["build_quality"].value<std::string>()) {
                    sd.bvh.build_quality = *quality;
                }
            }

            if (auto queries = (*spatial)["queries"].as_table()) {
                sd.queries.frustum_culling = (*queries)["frustum_culling"].value_or(true);
                sd.queries.occlusion_culling = (*queries)["occlusion_culling"].value_or(false);
                sd.queries.max_query_results = (*queries)["max_query_results"].value_or(500);
            }

            scene.spatial = sd;
        }

        // Parse [[entities]] array
        if (auto entities = tbl["entities"].as_array()) {
            for (const auto& entity_node : *entities) {
                if (auto entity_tbl = entity_node.as_table()) {
                    scene.entities.push_back(parse_entity(entity_tbl));
                }
            }
        }

        // Parse [[particle_emitters]] array
        if (auto emitters = tbl["particle_emitters"].as_array()) {
            for (const auto& emitter_node : *emitters) {
                if (auto emitter_tbl = emitter_node.as_table()) {
                    scene.particle_emitters.push_back(parse_particle_emitter(emitter_tbl));
                }
            }
        }

        // Parse [[textures]] array
        if (auto textures = tbl["textures"].as_array()) {
            for (const auto& tex_node : *textures) {
                if (auto tex_tbl = tex_node.as_table()) {
                    scene.textures.push_back(parse_texture(tex_tbl));
                }
            }
        }

        // Parse [debug]
        if (auto debug = tbl["debug"].as_table()) {
            DebugData dd;
            dd.enabled = (*debug)["enabled"].value_or(false);

            // Parse debug stats
            if (auto stats = (*debug)["stats"].as_table()) {
                dd.stats.enabled = (*stats)["enabled"].value_or(false);
                if (auto pos = (*stats)["position"].value<std::string>()) {
                    dd.stats.position = *pos;
                }
                dd.stats.font_size = (*stats)["font_size"].value_or(14);
                dd.stats.background_alpha = (*stats)["background_alpha"].value_or(0.7f);

                if (auto display = (*stats)["display"].as_table()) {
                    dd.stats.fps = (*display)["fps"].value_or(true);
                    dd.stats.frame_time = (*display)["frame_time"].value_or(true);
                    dd.stats.draw_calls = (*display)["draw_calls"].value_or(true);
                    dd.stats.triangles = (*display)["triangles"].value_or(true);
                    dd.stats.entities_total = (*display)["entities_total"].value_or(true);
                    dd.stats.entities_visible = (*display)["entities_visible"].value_or(true);
                    dd.stats.gpu_memory = (*display)["gpu_memory"].value_or(false);
                    dd.stats.cpu_time = (*display)["cpu_time"].value_or(true);
                }
            }

            if (auto viz = (*debug)["visualization"].as_table()) {
                dd.visualization.enabled = (*viz)["enabled"].value_or(false);
                dd.visualization.bounds = (*viz)["bounds"].value_or(false);
                dd.visualization.wireframe = (*viz)["wireframe"].value_or(false);
                dd.visualization.normals = (*viz)["normals"].value_or(false);
                dd.visualization.light_volumes = (*viz)["light_volumes"].value_or(false);
                dd.visualization.shadow_cascades = (*viz)["shadow_cascades"].value_or(false);
                dd.visualization.lod_levels = (*viz)["lod_levels"].value_or(false);
                dd.visualization.skeleton = (*viz)["skeleton"].value_or(false);
            }

            if (auto controls = (*debug)["controls"].as_table()) {
                if (auto toggle = (*controls)["toggle_key"].value<std::string>()) {
                    dd.controls.toggle_key = *toggle;
                }
                if (auto cycle = (*controls)["cycle_mode_key"].value<std::string>()) {
                    dd.controls.cycle_mode_key = *cycle;
                }
                if (auto reload = (*controls)["reload_shaders_key"].value<std::string>()) {
                    dd.controls.reload_shaders_key = *reload;
                }
            }

            scene.debug = dd;
        }

        // Parse [input]
        if (auto input = tbl["input"].as_table()) {
            InputData id;

            if (auto camera = (*input)["camera"].as_table()) {
                if (auto orbit = (*camera)["orbit_button"].value<std::string>()) {
                    id.camera.orbit_button = *orbit;
                }
                if (auto pan = (*camera)["pan_button"].value<std::string>()) {
                    id.camera.pan_button = *pan;
                }
                id.camera.zoom_scroll = (*camera)["zoom_scroll"].value_or(true);
                id.camera.orbit_sensitivity = (*camera)["orbit_sensitivity"].value_or(0.005f);
                id.camera.pan_sensitivity = (*camera)["pan_sensitivity"].value_or(0.01f);
                id.camera.zoom_sensitivity = (*camera)["zoom_sensitivity"].value_or(0.1f);
                id.camera.invert_y = (*camera)["invert_y"].value_or(false);
                id.camera.invert_x = (*camera)["invert_x"].value_or(false);
                id.camera.min_distance = (*camera)["min_distance"].value_or(0.5f);
                id.camera.max_distance = (*camera)["max_distance"].value_or(50.0f);
            }

            if (auto bindings = (*input)["bindings"].as_table()) {
                for (const auto& [key, value] : *bindings) {
                    if (auto str = value.value<std::string>()) {
                        id.bindings[std::string(key.str())] = *str;
                    }
                }
            }

            scene.input = id;
        }

    } catch (const toml::parse_error& err) {
        m_last_error = "TOML parse error: " + std::string(err.what());
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
