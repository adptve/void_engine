#include "engine.hpp"

#include <fstream>
#include <sstream>

namespace void_script {

// =============================================================================
// ScriptEngine Implementation
// =============================================================================

namespace {
    ScriptEngine* g_instance = nullptr;
}

ScriptEngine::ScriptEngine() {
    g_instance = this;
}

ScriptEngine::~ScriptEngine() {
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

ScriptEngine& ScriptEngine::instance() {
    if (!g_instance) {
        static ScriptEngine default_instance;
        return default_instance;
    }
    return *g_instance;
}

ScriptEngine* ScriptEngine::instance_ptr() {
    return g_instance;
}

void ScriptEngine::initialize() {
    if (initialized_) return;

    global_interpreter_ = std::make_unique<Interpreter>();
    register_engine_api();

    initialized_ = true;
}

void ScriptEngine::shutdown() {
    if (!initialized_) return;

    // Detach all scripts from entities
    entity_components_.clear();

    // Unload all scripts
    scripts_.clear();
    script_names_.clear();

    global_interpreter_.reset();
    bindings_.clear();

    initialized_ = false;
}

ScriptId ScriptEngine::load_script(const std::string& source, const std::string& name) {
    ScriptId id = ScriptId::create(next_script_id_++, 0);

    auto asset = std::make_unique<ScriptAsset>();
    asset->id = id;
    asset->name = name.empty() ? ("script_" + std::to_string(id.index())) : name;
    asset->source = source;

    // Parse
    Parser parser(source, asset->name);
    asset->ast = parser.parse_program();

    if (parser.has_errors()) {
        // Log errors
        for (const auto& err : parser.errors()) {
            if (event_bus_) {
                ScriptErrorEvent event{id, 0, err};
                event_bus_->publish(event);
            }
        }
    }

    script_names_[asset->name] = id;
    scripts_[id] = std::move(asset);

    return id;
}

ScriptId ScriptEngine::load_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        return ScriptId{};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    ScriptId id = load_script(buffer.str(), path.stem().string());

    if (auto* asset = get_script(id)) {
        asset->path = path;
        asset->last_modified = std::filesystem::last_write_time(path);
    }

    return id;
}

bool ScriptEngine::unload_script(ScriptId id) {
    auto it = scripts_.find(id);
    if (it == scripts_.end()) return false;

    script_names_.erase(it->second->name);
    scripts_.erase(it);

    // Detach from entities using this script
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.script_id == id) {
            comp.enabled = false;
            comp.context.reset();
        }
    }

    return true;
}

ScriptAsset* ScriptEngine::get_script(ScriptId id) {
    auto it = scripts_.find(id);
    if (it == scripts_.end()) return nullptr;
    return it->second.get();
}

const ScriptAsset* ScriptEngine::get_script(ScriptId id) const {
    auto it = scripts_.find(id);
    if (it == scripts_.end()) return nullptr;
    return it->second.get();
}

ScriptAsset* ScriptEngine::find_script(const std::string& name) {
    auto it = script_names_.find(name);
    if (it == script_names_.end()) return nullptr;
    return get_script(it->second);
}

std::vector<ScriptAsset*> ScriptEngine::all_scripts() {
    std::vector<ScriptAsset*> result;
    result.reserve(scripts_.size());
    for (auto& [id, asset] : scripts_) {
        result.push_back(asset.get());
    }
    return result;
}

Value ScriptEngine::execute(ScriptId id) {
    auto* asset = get_script(id);
    if (!asset || !asset->ast) {
        return Value(nullptr);
    }

    return global_interpreter_->execute(*asset->ast);
}

Value ScriptEngine::execute(const std::string& source) {
    return global_interpreter_->run(source);
}

Value ScriptEngine::call_function(ScriptId id, const std::string& function_name,
                                   const std::vector<Value>& args) {
    auto* asset = get_script(id);
    if (!asset) {
        return Value(nullptr);
    }

    // Execute the script first to define functions
    if (asset->ast) {
        global_interpreter_->execute(*asset->ast);
    }

    // Get the function
    Value func = global_interpreter_->globals().get(function_name);
    if (!func.is_callable()) {
        return Value(nullptr);
    }

    // Call it
    Callable* callable = func.as_callable();
    return callable->call(*global_interpreter_, args);
}

ScriptComponent* ScriptEngine::attach_script(std::uint64_t entity_id, ScriptId script_id) {
    auto* asset = get_script(script_id);
    if (!asset) return nullptr;

    ScriptComponent& comp = entity_components_[entity_id];
    comp.script_id = script_id;
    comp.context = std::make_unique<ScriptContext>();
    comp.enabled = true;
    comp.auto_tick = true;

    // Execute the script to set up the context
    if (asset->ast) {
        comp.context->interpreter().execute(*asset->ast);
    }

    // Apply bindings
    for (const auto& [name, binding] : bindings_) {
        binding.apply(comp.context->interpreter());
    }

    return &comp;
}

void ScriptEngine::detach_script(std::uint64_t entity_id) {
    entity_components_.erase(entity_id);
}

ScriptComponent* ScriptEngine::get_component(std::uint64_t entity_id) {
    auto it = entity_components_.find(entity_id);
    if (it == entity_components_.end()) return nullptr;
    return &it->second;
}

Value ScriptEngine::call_method(std::uint64_t entity_id, const std::string& method_name,
                                 const std::vector<Value>& args) {
    auto* comp = get_component(entity_id);
    if (!comp || !comp->enabled || !comp->context) {
        return Value(nullptr);
    }

    // Get the method
    Value func = comp->context->interpreter().globals().get(method_name);
    if (!func.is_callable()) {
        return Value(nullptr);
    }

    Callable* callable = func.as_callable();
    return callable->call(comp->context->interpreter(), args);
}

void ScriptEngine::update(float delta_time) {
    if (hot_reload_enabled_) {
        check_hot_reload();
    }

    for (auto& [entity_id, comp] : entity_components_) {
        if (!comp.enabled || !comp.auto_tick || !comp.context) continue;

        try {
            // Set delta time
            comp.context->set_global("delta_time", Value(static_cast<double>(delta_time)));

            // Try to call update/tick function
            Value func = comp.context->interpreter().globals().get("update");
            if (func.is_callable()) {
                func.as_callable()->call(comp.context->interpreter(), {Value(static_cast<double>(delta_time))});
            }
        } catch (const ScriptException& e) {
            if (event_bus_) {
                ScriptErrorEvent event{comp.script_id, entity_id, e};
                event_bus_->publish(event);
            }
        }
    }
}

void ScriptEngine::register_binding(const std::string& name, const NativeBinding& binding) {
    bindings_[name] = binding;

    // Apply to global interpreter
    if (global_interpreter_) {
        binding.apply(*global_interpreter_);
    }

    // Apply to all existing contexts
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.context) {
            binding.apply(comp.context->interpreter());
        }
    }
}

void ScriptEngine::register_function(const std::string& name, std::size_t arity,
                                      NativeFunction::Func func) {
    if (global_interpreter_) {
        global_interpreter_->define_native(name, arity, std::move(func));
    }
}

void ScriptEngine::register_constant(const std::string& name, Value value) {
    if (global_interpreter_) {
        global_interpreter_->define_constant(name, std::move(value));
    }
}

void ScriptEngine::register_engine_api() {
    // =========================================================================
    // Time Functions
    // =========================================================================

    register_function("get_time", 0, [](Interpreter&, const std::vector<Value>&) {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return Value(static_cast<double>(seconds) / 1000.0);
    });

    register_function("get_fps", 0, [this](Interpreter&, const std::vector<Value>&) {
        return Value(static_cast<double>(current_fps_));
    });

    register_function("get_delta_time", 0, [this](Interpreter&, const std::vector<Value>&) {
        return Value(static_cast<double>(current_delta_time_));
    });

    // =========================================================================
    // Logging Functions
    // =========================================================================

    register_function("log", 0, [](Interpreter& interp, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        interp.print("[LOG] " + output);
        return Value(nullptr);
    });

    register_function("warn", 0, [](Interpreter& interp, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        interp.print("[WARN] " + output);
        return Value(nullptr);
    });

    register_function("error", 0, [](Interpreter& interp, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        interp.print("[ERROR] " + output);
        return Value(nullptr);
    });

    register_function("trace", 0, [](Interpreter& interp, const std::vector<Value>& args) {
        std::string output;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].to_string();
        }
        interp.print("[TRACE] " + output);
        return Value(nullptr);
    });

    // =========================================================================
    // Entity Functions
    // =========================================================================

    // Entity creation
    register_function("spawn", 0, [this](Interpreter&, const std::vector<Value>& args) {
        std::uint64_t entity_id = next_entity_id_++;

        // If we have a spawn callback, call it
        if (entity_spawn_callback_) {
            std::string name = args.empty() ? "" : args[0].to_string();
            ValueMap components;
            if (args.size() > 1 && args[1].is_map()) {
                components = args[1].as_map();
            }
            entity_spawn_callback_(entity_id, name, components);
        }

        return Value(static_cast<std::int64_t>(entity_id));
    });

    register_function("destroy", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::uint64_t entity_id = args[0].as_int();

        if (entity_destroy_callback_) {
            entity_destroy_callback_(entity_id);
        }

        // Detach any scripts from this entity
        detach_script(entity_id);
        return Value(true);
    });

    register_function("entity_exists", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::uint64_t entity_id = args[0].as_int();

        if (entity_exists_callback_) {
            return Value(entity_exists_callback_(entity_id));
        }
        return Value(entity_components_.count(entity_id) > 0);
    });

    register_function("clone_entity", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(-1));
        std::uint64_t entity_id = args[0].as_int();
        std::uint64_t new_entity_id = next_entity_id_++;

        if (entity_clone_callback_) {
            entity_clone_callback_(entity_id, new_entity_id);
        }

        return Value(static_cast<std::int64_t>(new_entity_id));
    });

    // Component access
    register_function("get_component", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(nullptr);
        std::uint64_t entity_id = args[0].as_int();
        std::string component_type = args[1].to_string();

        if (get_component_callback_) {
            return get_component_callback_(entity_id, component_type);
        }
        return Value(nullptr);
    });

    register_function("set_component", 3, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 3) return Value(false);
        std::uint64_t entity_id = args[0].as_int();
        std::string component_name = args[1].to_string();
        Value data = args[2];

        if (set_component_callback_) {
            set_component_callback_(entity_id, component_name, data);
        }
        return Value(true);
    });

    register_function("has_component", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();
        std::string component_type = args[1].to_string();

        if (has_component_callback_) {
            return Value(has_component_callback_(entity_id, component_type));
        }
        return Value(false);
    });

    register_function("remove_component", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();
        std::string component_type = args[1].to_string();

        if (remove_component_callback_) {
            return Value(remove_component_callback_(entity_id, component_type));
        }
        return Value(false);
    });

    // Transform functions
    register_function("get_position", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({Value(0.0), Value(0.0), Value(0.0)});
        std::uint64_t entity_id = args[0].as_int();

        if (get_position_callback_) {
            return get_position_callback_(entity_id);
        }
        return Value::make_array({Value(0.0), Value(0.0), Value(0.0)});
    });

    register_function("set_position", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();

        double x = 0, y = 0, z = 0;
        if (args[1].is_array()) {
            const auto& arr = args[1].as_array();
            if (arr.size() > 0) x = arr[0].as_number();
            if (arr.size() > 1) y = arr[1].as_number();
            if (arr.size() > 2) z = arr[2].as_number();
        } else if (args.size() >= 4) {
            x = args[1].as_number();
            y = args[2].as_number();
            z = args[3].as_number();
        }

        if (set_position_callback_) {
            set_position_callback_(entity_id, x, y, z);
        }
        return Value(true);
    });

    register_function("get_rotation", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({Value(0.0), Value(0.0), Value(0.0)});
        std::uint64_t entity_id = args[0].as_int();

        if (get_rotation_callback_) {
            return get_rotation_callback_(entity_id);
        }
        return Value::make_array({Value(0.0), Value(0.0), Value(0.0)});
    });

    register_function("set_rotation", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();

        double x = 0, y = 0, z = 0;
        if (args[1].is_array()) {
            const auto& arr = args[1].as_array();
            if (arr.size() > 0) x = arr[0].as_number();
            if (arr.size() > 1) y = arr[1].as_number();
            if (arr.size() > 2) z = arr[2].as_number();
        } else if (args.size() >= 4) {
            x = args[1].as_number();
            y = args[2].as_number();
            z = args[3].as_number();
        }

        if (set_rotation_callback_) {
            set_rotation_callback_(entity_id, x, y, z);
        }
        return Value(true);
    });

    register_function("get_scale", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({Value(1.0), Value(1.0), Value(1.0)});
        std::uint64_t entity_id = args[0].as_int();

        if (get_scale_callback_) {
            return get_scale_callback_(entity_id);
        }
        return Value::make_array({Value(1.0), Value(1.0), Value(1.0)});
    });

    register_function("set_scale", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();

        double x = 1, y = 1, z = 1;
        if (args[1].is_array()) {
            const auto& arr = args[1].as_array();
            if (arr.size() > 0) x = arr[0].as_number();
            if (arr.size() > 1) y = arr[1].as_number();
            if (arr.size() > 2) z = arr[2].as_number();
        } else if (args.size() >= 4) {
            x = args[1].as_number();
            y = args[2].as_number();
            z = args[3].as_number();
        }

        if (set_scale_callback_) {
            set_scale_callback_(entity_id, x, y, z);
        }
        return Value(true);
    });

    // Hierarchy
    register_function("get_parent", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(-1));
        std::uint64_t entity_id = args[0].as_int();

        if (get_parent_callback_) {
            return Value(static_cast<std::int64_t>(get_parent_callback_(entity_id)));
        }
        return Value(static_cast<std::int64_t>(-1));
    });

    register_function("set_parent", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t entity_id = args[0].as_int();
        std::uint64_t parent_id = args[1].as_int();

        if (set_parent_callback_) {
            set_parent_callback_(entity_id, parent_id);
        }
        return Value(true);
    });

    register_function("get_children", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({});
        std::uint64_t entity_id = args[0].as_int();

        if (get_children_callback_) {
            return get_children_callback_(entity_id);
        }
        return Value::make_array({});
    });

    // Entity queries
    register_function("get_entity", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(-1));
        std::string name = args[0].to_string();

        if (find_entity_callback_) {
            return Value(static_cast<std::int64_t>(find_entity_callback_(name)));
        }
        return Value(static_cast<std::int64_t>(-1));
    });

    register_function("find_entities", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value::make_array({});

        if (find_entities_callback_) {
            return find_entities_callback_(args[0]);
        }
        return Value::make_array({});
    });

    // =========================================================================
    // Layer Functions
    // =========================================================================

    register_function("create_layer", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(static_cast<std::int64_t>(-1));
        std::string name = args[0].to_string();
        std::string layer_type = args[1].to_string();
        std::uint64_t layer_id = next_layer_id_++;

        if (create_layer_callback_) {
            create_layer_callback_(layer_id, name, layer_type);
        }

        return Value(static_cast<std::int64_t>(layer_id));
    });

    register_function("destroy_layer", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::uint64_t layer_id = args[0].as_int();

        if (destroy_layer_callback_) {
            destroy_layer_callback_(layer_id);
        }
        return Value(true);
    });

    register_function("set_layer_visible", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t layer_id = args[0].as_int();
        bool visible = args[1].is_truthy();

        if (set_layer_visible_callback_) {
            set_layer_visible_callback_(layer_id, visible);
        }
        return Value(true);
    });

    register_function("get_layer_visible", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::uint64_t layer_id = args[0].as_int();

        if (get_layer_visible_callback_) {
            return Value(get_layer_visible_callback_(layer_id));
        }
        return Value(true);
    });

    register_function("set_layer_order", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::uint64_t layer_id = args[0].as_int();
        std::int64_t order = args[1].as_int();

        if (set_layer_order_callback_) {
            set_layer_order_callback_(layer_id, order);
        }
        return Value(true);
    });

    // =========================================================================
    // Event Functions
    // =========================================================================

    register_function("on", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[1].is_callable()) return Value(static_cast<std::int64_t>(-1));
        std::string event_name = args[0].to_string();
        std::uint64_t listener_id = next_listener_id_++;

        event_listeners_[event_name].push_back({listener_id, args[1]});
        return Value(static_cast<std::int64_t>(listener_id));
    });

    register_function("once", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2 || !args[1].is_callable()) return Value(static_cast<std::int64_t>(-1));
        std::string event_name = args[0].to_string();
        std::uint64_t listener_id = next_listener_id_++;

        once_listeners_[event_name].push_back({listener_id, args[1]});
        return Value(static_cast<std::int64_t>(listener_id));
    });

    register_function("off", 2, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        std::string event_name = args[0].to_string();
        std::uint64_t listener_id = args[1].as_int();

        auto it = event_listeners_.find(event_name);
        if (it != event_listeners_.end()) {
            auto& listeners = it->second;
            listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                [listener_id](const auto& l) { return l.first == listener_id; }), listeners.end());
        }

        auto it2 = once_listeners_.find(event_name);
        if (it2 != once_listeners_.end()) {
            auto& listeners = it2->second;
            listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                [listener_id](const auto& l) { return l.first == listener_id; }), listeners.end());
        }

        return Value(true);
    });

    register_function("emit", 1, [this](Interpreter& interp, const std::vector<Value>& args) {
        if (args.empty()) return Value(static_cast<std::int64_t>(0));
        std::string event_name = args[0].to_string();
        Value data = args.size() > 1 ? args[1] : Value(nullptr);
        std::int64_t count = 0;

        // Regular listeners
        auto it = event_listeners_.find(event_name);
        if (it != event_listeners_.end()) {
            for (const auto& [id, callback] : it->second) {
                if (callback.is_callable()) {
                    callback.as_callable()->call(interp, {data});
                    ++count;
                }
            }
        }

        // Once listeners
        auto it2 = once_listeners_.find(event_name);
        if (it2 != once_listeners_.end()) {
            for (const auto& [id, callback] : it2->second) {
                if (callback.is_callable()) {
                    callback.as_callable()->call(interp, {data});
                    ++count;
                }
            }
            it2->second.clear();
        }

        return Value(count);
    });

    register_function("has_listeners", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::string event_name = args[0].to_string();

        auto it = event_listeners_.find(event_name);
        if (it != event_listeners_.end() && !it->second.empty()) return Value(true);

        auto it2 = once_listeners_.find(event_name);
        if (it2 != once_listeners_.end() && !it2->second.empty()) return Value(true);

        return Value(false);
    });

    register_function("clear_listeners", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::string event_name = args[0].to_string();

        event_listeners_.erase(event_name);
        once_listeners_.erase(event_name);

        return Value(true);
    });

    // =========================================================================
    // Input Functions
    // =========================================================================

    register_function("get_keyboard_state", 0, [this](Interpreter&, const std::vector<Value>&) {
        if (get_keyboard_state_callback_) {
            return get_keyboard_state_callback_();
        }
        return Value::make_map({});
    });

    register_function("get_mouse_state", 0, [this](Interpreter&, const std::vector<Value>&) {
        if (get_mouse_state_callback_) {
            return get_mouse_state_callback_();
        }
        return Value::make_map({});
    });

    // =========================================================================
    // Viewport Functions
    // =========================================================================

    register_function("get_viewport_size", 0, [this](Interpreter&, const std::vector<Value>&) {
        if (get_viewport_size_callback_) {
            return get_viewport_size_callback_();
        }
        return Value::make_array({Value(1920.0), Value(1080.0)});
    });

    register_function("get_viewport_aspect", 0, [this](Interpreter&, const std::vector<Value>&) {
        if (get_viewport_aspect_callback_) {
            return Value(get_viewport_aspect_callback_());
        }
        return Value(16.0 / 9.0);
    });

    // =========================================================================
    // Script context
    // =========================================================================

    register_function("get_namespace", 0, [](Interpreter&, const std::vector<Value>&) {
        return Value("global");
    });

    // =========================================================================
    // emit_patch - ECS communication
    // =========================================================================

    register_function("emit_patch", 1, [this](Interpreter&, const std::vector<Value>& args) {
        if (args.empty() || !args[0].is_map()) return Value(false);

        if (emit_patch_callback_) {
            emit_patch_callback_(args[0]);
        }

        return Value(true);
    });

    // =========================================================================
    // Math constants (also defined in interpreter stdlib, but added here for engine context)
    // =========================================================================

    register_constant("PI", Value(3.14159265358979323846));
    register_constant("E", Value(2.71828182845904523536));
    register_constant("TAU", Value(6.28318530717958647692));
}

void ScriptEngine::enable_hot_reload(bool enabled) {
    hot_reload_enabled_ = enabled;
}

void ScriptEngine::check_hot_reload() {
    for (auto& [id, asset] : scripts_) {
        if (asset->path.empty()) continue;
        if (!std::filesystem::exists(asset->path)) continue;

        auto current_time = std::filesystem::last_write_time(asset->path);
        if (current_time > asset->last_modified) {
            hot_reload(id);
        }
    }
}

bool ScriptEngine::hot_reload(ScriptId id) {
    auto* asset = get_script(id);
    if (!asset || asset->path.empty()) return false;

    // Reload source
    std::ifstream file(asset->path);
    if (!file) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    asset->source = buffer.str();
    asset->last_modified = std::filesystem::last_write_time(asset->path);

    // Reparse
    Parser parser(asset->source, asset->name);
    asset->ast = parser.parse_program();

    if (parser.has_errors()) {
        for (const auto& err : parser.errors()) {
            if (event_bus_) {
                ScriptErrorEvent event{id, 0, err};
                event_bus_->publish(event);
            }
        }
        return false;
    }

    // Re-execute in all contexts using this script, preserving state
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.script_id == id && comp.context) {
            try {
                // Take snapshot of current state
                auto snapshot = comp.context->interpreter().take_snapshot();

                // Re-execute the script
                comp.context->interpreter().execute(*asset->ast);

                // Restore state from snapshot
                comp.context->interpreter().apply_snapshot(snapshot);
            } catch (const ScriptException& e) {
                if (event_bus_) {
                    ScriptErrorEvent event{id, entity_id, e};
                    event_bus_->publish(event);
                }
            }
        }
    }

    return true;
}

void ScriptEngine::set_debug_mode(bool enabled) {
    debug_mode_ = enabled;
    if (global_interpreter_) {
        global_interpreter_->set_debug(enabled);
    }
}

ScriptEngine::Stats ScriptEngine::stats() const {
    Stats s;
    s.loaded_scripts = scripts_.size();
    s.active_contexts = entity_components_.size();
    return s;
}

// =============================================================================
// NativeBinding Implementation
// =============================================================================

NativeBinding& NativeBinding::constant(const std::string& name, Value value) {
    constants_.emplace_back(name, std::move(value));
    return *this;
}

void NativeBinding::apply(Interpreter& interp) const {
    for (const auto& [name, value] : constants_) {
        interp.define_constant(name, value);
    }

    for (const auto& [name, arity, func] : functions_) {
        interp.define_native(name, arity, func);
    }
}

} // namespace void_script
