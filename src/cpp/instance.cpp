/// @file instance.cpp
/// @brief C++ class instance management implementation

#include "instance.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>

namespace void_cpp {

// =============================================================================
// CppLibrary Implementation
// =============================================================================

CppLibrary::CppLibrary(ModuleId module_id, const std::filesystem::path& path)
    : module_id_(module_id)
    , path_(path) {

    auto& registry = ModuleRegistry::instance();
    auto* module = registry.get_module(module_id);
    if (!module) {
        VOID_LOG_ERROR("[CppLibrary] Module not found for ID");
        return;
    }

    // Get library info function
    auto info_result = module->get_symbol<GetLibraryInfoFn>("void_get_library_info");
    if (!info_result) {
        VOID_LOG_ERROR("[CppLibrary] Failed to find void_get_library_info in {}", path.string());
        return;
    }
    get_library_info_ = info_result.value();

    // Get class info function
    auto class_info_result = module->get_symbol<GetClassInfoFn>("void_get_class_info");
    if (!class_info_result) {
        VOID_LOG_ERROR("[CppLibrary] Failed to find void_get_class_info in {}", path.string());
        return;
    }
    get_class_info_ = class_info_result.value();

    // Optional: get vtable function
    auto vtable_result = module->get_symbol<GetClassVTableFn>("void_get_class_vtable");
    if (vtable_result) {
        get_class_vtable_ = vtable_result.value();
    }

    // Optional: set entity ID function
    auto set_entity_result = module->get_symbol<SetEntityIdFn>("void_set_entity_id");
    if (set_entity_result) {
        set_entity_id_ = set_entity_result.value();
    }

    // Optional: set world context function
    auto set_world_result = module->get_symbol<SetWorldContextFn>("void_set_world_context");
    if (set_world_result) {
        set_world_context_ = set_world_result.value();
    }

    // Get library info
    info_ = get_library_info_();

    // Verify API version
    if (info_.api_version != VOID_CPP_API_VERSION) {
        VOID_LOG_ERROR("[CppLibrary] API version mismatch in {}: expected {}, got {}",
                      path.string(), VOID_CPP_API_VERSION, info_.api_version);
        return;
    }

    // Cache class info
    for (std::uint32_t i = 0; i < info_.class_count; ++i) {
        const FfiClassInfo* class_info = get_class_info_(i);
        if (class_info && class_info->name) {
            class_cache_[class_info->name] = class_info;
        }
    }

    valid_ = true;
    VOID_LOG_INFO("[CppLibrary] Loaded {} with {} classes", path.string(), info_.class_count);
}

CppLibrary::~CppLibrary() {
    class_cache_.clear();
    vtable_cache_.clear();
}

CppLibrary::CppLibrary(CppLibrary&& other) noexcept
    : module_id_(other.module_id_)
    , path_(std::move(other.path_))
    , info_(other.info_)
    , valid_(other.valid_)
    , get_library_info_(other.get_library_info_)
    , get_class_info_(other.get_class_info_)
    , get_class_vtable_(other.get_class_vtable_)
    , set_entity_id_(other.set_entity_id_)
    , set_world_context_(other.set_world_context_)
    , class_cache_(std::move(other.class_cache_))
    , vtable_cache_(std::move(other.vtable_cache_)) {
    other.valid_ = false;
}

CppLibrary& CppLibrary::operator=(CppLibrary&& other) noexcept {
    if (this != &other) {
        module_id_ = other.module_id_;
        path_ = std::move(other.path_);
        info_ = other.info_;
        valid_ = other.valid_;
        get_library_info_ = other.get_library_info_;
        get_class_info_ = other.get_class_info_;
        get_class_vtable_ = other.get_class_vtable_;
        set_entity_id_ = other.set_entity_id_;
        set_world_context_ = other.set_world_context_;
        class_cache_ = std::move(other.class_cache_);
        vtable_cache_ = std::move(other.vtable_cache_);
        other.valid_ = false;
    }
    return *this;
}

bool CppLibrary::has_class(const std::string& name) const {
    return class_cache_.find(name) != class_cache_.end();
}

const FfiClassInfo* CppLibrary::get_class_info(const std::string& name) const {
    auto it = class_cache_.find(name);
    return (it != class_cache_.end()) ? it->second : nullptr;
}

const FfiClassVTable* CppLibrary::get_class_vtable(const std::string& name) const {
    // Check cache first
    auto it = vtable_cache_.find(name);
    if (it != vtable_cache_.end()) {
        return it->second;
    }

    // Load from library
    if (get_class_vtable_) {
        const FfiClassVTable* vtable = get_class_vtable_(name.c_str());
        vtable_cache_[name] = vtable;
        return vtable;
    }

    return nullptr;
}

std::vector<std::string> CppLibrary::class_names() const {
    std::vector<std::string> names;
    names.reserve(class_cache_.size());
    for (const auto& [name, info] : class_cache_) {
        names.push_back(name);
    }
    return names;
}

CppHandle CppLibrary::create_instance(const std::string& class_name) const {
    const FfiClassInfo* info = get_class_info(class_name);
    if (!info || !info->create_fn) {
        return CppHandle{nullptr};
    }
    return info->create_fn();
}

void CppLibrary::destroy_instance(const std::string& class_name, CppHandle handle) const {
    if (!handle.is_valid()) return;

    const FfiClassInfo* info = get_class_info(class_name);
    if (info && info->destroy_fn) {
        info->destroy_fn(handle);
    }
}

void CppLibrary::set_instance_entity(CppHandle handle, FfiEntityId entity) const {
    if (!handle.is_valid() || !set_entity_id_) return;
    set_entity_id_(handle, entity);
}

void CppLibrary::set_instance_world_context(CppHandle handle, const FfiWorldContext* context) const {
    if (!handle.is_valid() || !set_world_context_) return;
    set_world_context_(handle, context);
}

// =============================================================================
// CppClassInstance Implementation
// =============================================================================

CppClassInstance::CppClassInstance(InstanceId id, const std::string& class_name,
                                   CppHandle handle, CppLibrary* library)
    : id_(id)
    , class_name_(class_name)
    , handle_(handle)
    , library_(library) {}

CppClassInstance::~CppClassInstance() {
    if (handle_.is_valid() && library_) {
        // Call EndPlay if still active
        if (state_ == InstanceState::Active && begun_) {
            end_play();
        }
        library_->destroy_instance(class_name_, handle_);
    }
}

CppClassInstance::CppClassInstance(CppClassInstance&& other) noexcept
    : id_(other.id_)
    , class_name_(std::move(other.class_name_))
    , handle_(other.handle_)
    , library_(other.library_)
    , state_(other.state_)
    , entity_id_(other.entity_id_)
    , properties_(std::move(other.properties_))
    , begun_(other.begun_) {
    other.handle_ = CppHandle{nullptr};
}

CppClassInstance& CppClassInstance::operator=(CppClassInstance&& other) noexcept {
    if (this != &other) {
        if (handle_.is_valid() && library_) {
            library_->destroy_instance(class_name_, handle_);
        }

        id_ = other.id_;
        class_name_ = std::move(other.class_name_);
        handle_ = other.handle_;
        library_ = other.library_;
        state_ = other.state_;
        entity_id_ = other.entity_id_;
        properties_ = std::move(other.properties_);
        begun_ = other.begun_;

        other.handle_ = CppHandle{nullptr};
    }
    return *this;
}

void CppClassInstance::set_entity(FfiEntityId entity) {
    entity_id_ = entity;
    if (library_) {
        library_->set_instance_entity(handle_, entity);
    }
}

void CppClassInstance::set_property(const std::string& name, PropertyValue value) {
    properties_[name] = std::move(value);
}

const PropertyValue* CppClassInstance::get_property(const std::string& name) const {
    auto it = properties_.find(name);
    return (it != properties_.end()) ? &it->second : nullptr;
}

void CppClassInstance::begin_play() {
    if (begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->begin_play) {
        vtable->begin_play(handle_);
    }

    begun_ = true;
    state_ = InstanceState::Active;
}

void CppClassInstance::tick(float delta_time) {
    if (!begun_ || state_ != InstanceState::Active || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->tick) {
        vtable->tick(handle_, delta_time);
    }
}

void CppClassInstance::fixed_tick(float delta_time) {
    if (!begun_ || state_ != InstanceState::Active || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->fixed_tick) {
        vtable->fixed_tick(handle_, delta_time);
    }
}

void CppClassInstance::end_play() {
    if (!begun_ || !library_) return;

    state_ = InstanceState::Ending;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->end_play) {
        vtable->end_play(handle_);
    }

    begun_ = false;
    state_ = InstanceState::Destroyed;
}

void CppClassInstance::on_collision_enter(FfiEntityId other, FfiHitResult hit) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_collision_enter) {
        vtable->on_collision_enter(handle_, other, hit);
    }
}

void CppClassInstance::on_collision_exit(FfiEntityId other) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_collision_exit) {
        vtable->on_collision_exit(handle_, other);
    }
}

void CppClassInstance::on_trigger_enter(FfiEntityId other) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_trigger_enter) {
        vtable->on_trigger_enter(handle_, other);
    }
}

void CppClassInstance::on_trigger_exit(FfiEntityId other) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_trigger_exit) {
        vtable->on_trigger_exit(handle_, other);
    }
}

void CppClassInstance::on_damage(FfiDamageInfo damage) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_damage) {
        vtable->on_damage(handle_, damage);
    }
}

void CppClassInstance::on_death(FfiEntityId killer) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_death) {
        vtable->on_death(handle_, killer);
    }
}

void CppClassInstance::on_interact(FfiEntityId interactor) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_interact) {
        vtable->on_interact(handle_, interactor);
    }
}

void CppClassInstance::on_input_action(FfiInputAction action) {
    if (!begun_ || !library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_input_action) {
        vtable->on_input_action(handle_, action);
    }
}

std::vector<std::uint8_t> CppClassInstance::serialize() const {
    if (!library_) return {};

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (!vtable || !vtable->get_serialized_size || !vtable->serialize) {
        return {};
    }

    std::size_t size = vtable->get_serialized_size(handle_);
    if (size == 0) return {};

    std::vector<std::uint8_t> buffer(size);
    std::size_t written = vtable->serialize(handle_, buffer.data(), buffer.size());
    buffer.resize(written);

    return buffer;
}

bool CppClassInstance::deserialize(const std::vector<std::uint8_t>& data) {
    if (!library_ || data.empty()) return false;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (!vtable || !vtable->deserialize) {
        return false;
    }

    return vtable->deserialize(handle_, data.data(), data.size());
}

void CppClassInstance::on_reload() {
    if (!library_) return;

    const FfiClassVTable* vtable = library_->get_class_vtable(class_name_);
    if (vtable && vtable->on_reload) {
        vtable->on_reload(handle_);
    }
}

// =============================================================================
// CppClassRegistry Implementation
// =============================================================================

CppClassRegistry::CppClassRegistry() = default;

CppClassRegistry::~CppClassRegistry() {
    // Destroy all instances first
    end_play_all();
    instances_.clear();

    // Then unload libraries
    libraries_.clear();
}

CppClassRegistry& CppClassRegistry::instance() {
    static CppClassRegistry s_instance;
    return s_instance;
}

CppResult<CppLibrary*> CppClassRegistry::load_library(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already loaded
    auto it = libraries_.find(path);
    if (it != libraries_.end()) {
        return it->second.get();
    }

    // Load via ModuleRegistry
    auto& module_registry = ModuleRegistry::instance();
    auto module_result = module_registry.load_module(path);
    if (!module_result) {
        return void_core::Error{void_core::ErrorCode::IoError, "Failed to load module"};
    }

    auto* module = module_result.value();

    // Create CppLibrary wrapper
    auto library = std::make_unique<CppLibrary>(module->id(), path);
    if (!library->is_valid()) {
        module_registry.unload_module(module->id());
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid C++ library"};
    }

    // Register class-to-library mappings
    for (const auto& class_name : library->class_names()) {
        class_to_library_[class_name] = path;
    }

    auto* lib_ptr = library.get();
    libraries_[path] = std::move(library);
    module_to_path_[module->id()] = path;

    VOID_LOG_INFO("[CppClassRegistry] Loaded library {} with {} classes",
                  path.string(), lib_ptr->info().class_count);

    return lib_ptr;
}

bool CppClassRegistry::unload_library(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = libraries_.find(path);
    if (it == libraries_.end()) {
        return false;
    }

    // Destroy all instances from this library
    std::vector<InstanceId> to_destroy;
    for (const auto& [id, instance] : instances_) {
        if (instance->library() == it->second.get()) {
            to_destroy.push_back(id);
        }
    }
    for (auto id : to_destroy) {
        instances_.erase(id);
    }

    // Remove class mappings
    for (const auto& class_name : it->second->class_names()) {
        class_to_library_.erase(class_name);
    }

    // Remove module mapping
    module_to_path_.erase(it->second->module_id());

    // Unload from ModuleRegistry
    ModuleRegistry::instance().unload_module(it->second->module_id());

    libraries_.erase(it);

    VOID_LOG_INFO("[CppClassRegistry] Unloaded library {}", path.string());

    return true;
}

bool CppClassRegistry::unload_library(ModuleId module_id) {
    auto path_it = module_to_path_.find(module_id);
    if (path_it == module_to_path_.end()) {
        return false;
    }
    return unload_library(path_it->second);
}

CppLibrary* CppClassRegistry::get_library(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = libraries_.find(path);
    return (it != libraries_.end()) ? it->second.get() : nullptr;
}

CppLibrary* CppClassRegistry::get_library(ModuleId module_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto path_it = module_to_path_.find(module_id);
    if (path_it == module_to_path_.end()) {
        return nullptr;
    }
    auto lib_it = libraries_.find(path_it->second);
    return (lib_it != libraries_.end()) ? lib_it->second.get() : nullptr;
}

bool CppClassRegistry::is_library_loaded(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return libraries_.find(path) != libraries_.end();
}

std::vector<std::filesystem::path> CppClassRegistry::loaded_libraries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::filesystem::path> paths;
    paths.reserve(libraries_.size());
    for (const auto& [path, lib] : libraries_) {
        paths.push_back(path);
    }
    return paths;
}

bool CppClassRegistry::has_class(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return class_to_library_.find(name) != class_to_library_.end();
}

std::vector<std::string> CppClassRegistry::class_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(class_to_library_.size());
    for (const auto& [name, path] : class_to_library_) {
        names.push_back(name);
    }
    return names;
}

CppResult<CppClassInstance*> CppClassRegistry::create_instance(
    const std::string& class_name,
    FfiEntityId entity,
    const PropertyMap& properties) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Find library for class
    auto lib_it = class_to_library_.find(class_name);
    if (lib_it == class_to_library_.end()) {
        return void_core::Error{void_core::ErrorCode::NotFound, "Class not found: " + class_name};
    }

    auto* library = libraries_[lib_it->second].get();

    // Create C++ object
    CppHandle handle = library->create_instance(class_name);
    if (!handle.is_valid()) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Failed to create instance"};
    }

    // Create wrapper
    InstanceId id = next_instance_id_++;
    auto instance = std::make_unique<CppClassInstance>(id, class_name, handle, library);

    // Set entity
    if (entity.is_valid()) {
        instance->set_entity(entity);
        entity_to_instance_[entity] = id;
    }

    // Set properties
    for (const auto& [name, value] : properties) {
        instance->set_property(name, value);
    }

    // Set world context
    if (world_context_) {
        library->set_instance_world_context(handle, world_context_);
    }

    auto* inst_ptr = instance.get();
    instances_[id] = std::move(instance);

    VOID_LOG_DEBUG("[CppClassRegistry] Created instance {} of class {}", id, class_name);

    return inst_ptr;
}

bool CppClassRegistry::destroy_instance(InstanceId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(id);
    if (it == instances_.end()) {
        return false;
    }

    // Remove entity mapping
    if (it->second->entity_id().is_valid()) {
        entity_to_instance_.erase(it->second->entity_id());
    }

    instances_.erase(it);
    return true;
}

bool CppClassRegistry::destroy_instance_for_entity(FfiEntityId entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto entity_it = entity_to_instance_.find(entity);
    if (entity_it == entity_to_instance_.end()) {
        return false;
    }

    instances_.erase(entity_it->second);
    entity_to_instance_.erase(entity_it);
    return true;
}

CppClassInstance* CppClassRegistry::get_instance(InstanceId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(id);
    return (it != instances_.end()) ? it->second.get() : nullptr;
}

const CppClassInstance* CppClassRegistry::get_instance(InstanceId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(id);
    return (it != instances_.end()) ? it->second.get() : nullptr;
}

CppClassInstance* CppClassRegistry::get_instance_for_entity(FfiEntityId entity) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entity_it = entity_to_instance_.find(entity);
    if (entity_it == entity_to_instance_.end()) {
        return nullptr;
    }
    auto inst_it = instances_.find(entity_it->second);
    return (inst_it != instances_.end()) ? inst_it->second.get() : nullptr;
}

std::vector<CppClassInstance*> CppClassRegistry::instances() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CppClassInstance*> result;
    result.reserve(instances_.size());
    for (auto& [id, inst] : instances_) {
        result.push_back(inst.get());
    }
    return result;
}

std::size_t CppClassRegistry::instance_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.size();
}

void CppClassRegistry::begin_play_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, inst] : instances_) {
        inst->begin_play();
    }
}

void CppClassRegistry::tick_all(float delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, inst] : instances_) {
        inst->tick(delta_time);
    }
}

void CppClassRegistry::fixed_tick_all(float delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, inst] : instances_) {
        inst->fixed_tick(delta_time);
    }
}

void CppClassRegistry::end_play_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, inst] : instances_) {
        inst->end_play();
    }
}

void CppClassRegistry::set_world_context(const FfiWorldContext* context) {
    std::lock_guard<std::mutex> lock(mutex_);
    world_context_ = context;

    // Update all existing instances
    for (auto& [id, inst] : instances_) {
        if (inst->library()) {
            inst->library()->set_instance_world_context(inst->handle(), context);
        }
    }
}

std::vector<CppClassRegistry::SavedInstanceState> CppClassRegistry::prepare_reload(
    const std::filesystem::path& library_path) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SavedInstanceState> saved;

    auto lib_it = libraries_.find(library_path);
    if (lib_it == libraries_.end()) {
        return saved;
    }

    for (auto& [id, inst] : instances_) {
        if (inst->library() == lib_it->second.get()) {
            SavedInstanceState state;
            state.id = id;
            state.entity = inst->entity_id();
            state.class_name = inst->class_name();
            state.properties = inst->properties();
            state.serialized_data = inst->serialize();
            saved.push_back(std::move(state));
        }
    }

    VOID_LOG_INFO("[CppClassRegistry] Saved {} instances for reload", saved.size());

    return saved;
}

void CppClassRegistry::complete_reload(const std::filesystem::path& library_path,
                                       const std::vector<SavedInstanceState>& saved_states) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto lib_it = libraries_.find(library_path);
    if (lib_it == libraries_.end()) {
        VOID_LOG_ERROR("[CppClassRegistry] Library not found for reload: {}", library_path.string());
        return;
    }

    auto* library = lib_it->second.get();

    for (const auto& state : saved_states) {
        // Remove old instance
        instances_.erase(state.id);
        entity_to_instance_.erase(state.entity);

        // Create new instance
        CppHandle handle = library->create_instance(state.class_name);
        if (!handle.is_valid()) {
            VOID_LOG_ERROR("[CppClassRegistry] Failed to recreate instance of {}", state.class_name);
            continue;
        }

        auto instance = std::make_unique<CppClassInstance>(
            state.id, state.class_name, handle, library);

        // Restore entity
        if (state.entity.is_valid()) {
            instance->set_entity(state.entity);
            entity_to_instance_[state.entity] = state.id;
        }

        // Restore properties
        for (const auto& [name, value] : state.properties) {
            instance->set_property(name, value);
        }

        // Set world context
        if (world_context_) {
            library->set_instance_world_context(handle, world_context_);
        }

        // Deserialize state
        if (!state.serialized_data.empty()) {
            instance->deserialize(state.serialized_data);
        }

        // Begin play
        instance->begin_play();

        // Notify of reload
        instance->on_reload();

        instances_[state.id] = std::move(instance);
    }

    VOID_LOG_INFO("[CppClassRegistry] Restored {} instances after reload", saved_states.size());
}

} // namespace void_cpp
