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
    // Time
    register_function("get_time", 0, [](Interpreter&, const std::vector<Value>&) {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return Value(static_cast<double>(seconds) / 1000.0);
    });

    // Logging
    register_function("log", 1, [](Interpreter& interp, const std::vector<Value>& args) {
        if (!args.empty()) {
            interp.print("[LOG] " + args[0].to_string());
        }
        return Value(nullptr);
    });

    register_function("warn", 1, [](Interpreter& interp, const std::vector<Value>& args) {
        if (!args.empty()) {
            interp.print("[WARN] " + args[0].to_string());
        }
        return Value(nullptr);
    });

    register_function("error", 1, [](Interpreter& interp, const std::vector<Value>& args) {
        if (!args.empty()) {
            interp.print("[ERROR] " + args[0].to_string());
        }
        return Value(nullptr);
    });

    // Math constants
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

    // Re-execute in all contexts using this script
    for (auto& [entity_id, comp] : entity_components_) {
        if (comp.script_id == id && comp.context) {
            try {
                comp.context->interpreter().execute(*asset->ast);
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
