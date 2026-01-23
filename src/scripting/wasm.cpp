/// @file wasm.cpp
/// @brief WASM module and instance management implementation

#include "wasm.hpp"
#include "wasm_interpreter.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace void_scripting {

// =============================================================================
// WasmMemory Implementation
// =============================================================================

WasmMemory::WasmMemory(std::size_t initial_pages, std::optional<std::size_t> max_pages)
    : max_pages_(max_pages) {
    size_ = initial_pages * page_size;
    if (size_ > 0) {
        data_ = std::make_unique<std::uint8_t[]>(size_);
        std::memset(data_.get(), 0, size_);
    }
}

WasmMemory::~WasmMemory() = default;

WasmMemory::WasmMemory(WasmMemory&& other) noexcept
    : data_(std::move(other.data_))
    , size_(other.size_)
    , max_pages_(other.max_pages_) {
    other.size_ = 0;
}

WasmMemory& WasmMemory::operator=(WasmMemory&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        size_ = other.size_;
        max_pages_ = other.max_pages_;
        other.size_ = 0;
    }
    return *this;
}

WasmResult<std::size_t> WasmMemory::grow(std::size_t delta_pages) {
    std::size_t current_pages = pages();
    std::size_t new_pages = current_pages + delta_pages;

    if (max_pages_ && new_pages > *max_pages_) {
        return void_core::Error{void_core::ErrorCode::OutOfMemory, "WASM out of memory"};
    }

    std::size_t new_size = new_pages * page_size;
    auto new_data = std::make_unique<std::uint8_t[]>(new_size);

    // Copy existing data
    if (data_ && size_ > 0) {
        std::memcpy(new_data.get(), data_.get(), size_);
    }

    // Zero new pages
    std::memset(new_data.get() + size_, 0, new_size - size_);

    data_ = std::move(new_data);
    std::size_t old_pages = pages();
    size_ = new_size;

    return old_pages;
}

void WasmMemory::read_bytes(std::size_t offset, std::span<std::uint8_t> buffer) const {
    if (!check_bounds(offset, buffer.size())) {
        throw WasmException(WasmError::OutOfBounds, "Memory read out of bounds");
    }
    std::memcpy(buffer.data(), data_.get() + offset, buffer.size());
}

void WasmMemory::write_bytes(std::size_t offset, std::span<const std::uint8_t> data) {
    if (!check_bounds(offset, data.size())) {
        throw WasmException(WasmError::OutOfBounds, "Memory write out of bounds");
    }
    std::memcpy(data_.get() + offset, data.data(), data.size());
}

std::string WasmMemory::read_string(std::size_t offset, std::size_t max_len) const {
    std::string result;
    result.reserve(256);

    for (std::size_t i = 0; i < max_len && offset + i < size_; ++i) {
        char c = static_cast<char>(data_[offset + i]);
        if (c == '\0') break;
        result.push_back(c);
    }

    return result;
}

std::size_t WasmMemory::write_string(std::size_t offset, const std::string& str) {
    std::size_t len = str.size() + 1;  // Include null terminator
    if (!check_bounds(offset, len)) {
        throw WasmException(WasmError::OutOfBounds, "String write out of bounds");
    }
    std::memcpy(data_.get() + offset, str.c_str(), len);
    return len;
}

bool WasmMemory::check_bounds(std::size_t offset, std::size_t size) const {
    return offset + size <= size_ && offset + size >= offset;  // Check overflow
}

// =============================================================================
// WasmModule Implementation
// =============================================================================

WasmModule::WasmModule(WasmModuleId id, std::string name)
    : id_(id)
    , name_(std::move(name)) {}

WasmModule::~WasmModule() {
    // Clean up backend-specific compiled module
    compiled_module_ = nullptr;
}

WasmModule::WasmModule(WasmModule&& other) noexcept
    : id_(other.id_)
    , name_(std::move(other.name_))
    , info_(std::move(other.info_))
    , binary_(std::move(other.binary_))
    , valid_(other.valid_)
    , compiled_module_(other.compiled_module_) {
    other.valid_ = false;
    other.compiled_module_ = nullptr;
}

WasmModule& WasmModule::operator=(WasmModule&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        name_ = std::move(other.name_);
        info_ = std::move(other.info_);
        binary_ = std::move(other.binary_);
        valid_ = other.valid_;
        compiled_module_ = other.compiled_module_;

        other.valid_ = false;
        other.compiled_module_ = nullptr;
    }
    return *this;
}

const WasmExport* WasmModule::find_export(const std::string& name) const {
    for (const auto& exp : info_.exports) {
        if (exp.name == name) {
            return &exp;
        }
    }
    return nullptr;
}

std::optional<std::span<const std::uint8_t>>
WasmModule::get_custom_section(const std::string& name) const {
    for (const auto& [section_name, data] : info_.custom_sections) {
        if (section_name == name) {
            return std::span<const std::uint8_t>(data);
        }
    }
    return std::nullopt;
}

namespace {

// Simple WASM binary parser for validation and info extraction
class WasmParser {
public:
    WasmParser(std::span<const std::uint8_t> binary)
        : data_(binary)
        , pos_(0) {}

    WasmResult<WasmModuleInfo> parse() {
        // Check magic number
        if (data_.size() < 8) {
            return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
        }

        // WASM magic: \0asm
        if (data_[0] != 0x00 || data_[1] != 0x61 ||
            data_[2] != 0x73 || data_[3] != 0x6d) {
            return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
        }

        // Version (1)
        std::uint32_t version = data_[4] | (data_[5] << 8) |
                               (data_[6] << 16) | (data_[7] << 24);
        if (version != 1) {
            return void_core::Error{void_core::ErrorCode::NotSupported, "Unsupported WASM feature"};
        }

        pos_ = 8;
        WasmModuleInfo info;

        // Parse sections
        while (pos_ < data_.size()) {
            std::uint8_t section_id = read_byte();
            std::uint32_t section_size = read_leb128_u32();

            std::size_t section_end = pos_ + section_size;
            if (section_end > data_.size()) {
                return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
            }

            switch (section_id) {
                case 0: // Custom section
                    parse_custom_section(info, section_size);
                    break;
                case 1: // Type section
                    parse_type_section(info);
                    break;
                case 2: // Import section
                    parse_import_section(info);
                    break;
                case 3: // Function section
                    info.num_functions = read_leb128_u32();
                    break;
                case 4: // Table section
                    info.num_tables = read_leb128_u32();
                    break;
                case 5: // Memory section
                    info.num_memories = read_leb128_u32();
                    break;
                case 6: // Global section
                    info.num_globals = read_leb128_u32();
                    break;
                case 7: // Export section
                    parse_export_section(info);
                    break;
                case 8: // Start section
                    info.start_function = read_leb128_u32();
                    break;
                default:
                    // Skip unknown section
                    break;
            }

            pos_ = section_end;
        }

        return std::move(info);
    }

private:
    std::uint8_t read_byte() {
        if (pos_ >= data_.size()) {
            throw WasmException(WasmError::InvalidModule, "Unexpected end of binary");
        }
        return data_[pos_++];
    }

    std::uint32_t read_leb128_u32() {
        std::uint32_t result = 0;
        std::uint32_t shift = 0;
        std::uint8_t byte;

        do {
            byte = read_byte();
            result |= static_cast<std::uint32_t>(byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);

        return result;
    }

    std::string read_name() {
        std::uint32_t len = read_leb128_u32();
        if (pos_ + len > data_.size()) {
            throw WasmException(WasmError::InvalidModule, "Name too long");
        }
        std::string name(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        return name;
    }

    WasmValType read_valtype() {
        std::uint8_t type = read_byte();
        switch (type) {
            case 0x7f: return WasmValType::I32;
            case 0x7e: return WasmValType::I64;
            case 0x7d: return WasmValType::F32;
            case 0x7c: return WasmValType::F64;
            case 0x7b: return WasmValType::V128;
            case 0x70: return WasmValType::FuncRef;
            case 0x6f: return WasmValType::ExternRef;
            default:
                throw WasmException(WasmError::InvalidModule, "Unknown value type");
        }
    }

    void parse_custom_section(WasmModuleInfo& info, std::uint32_t section_size) {
        std::size_t start = pos_;
        std::string name = read_name();
        std::size_t name_size = pos_ - start;
        std::size_t data_size = section_size - name_size;

        std::vector<std::uint8_t> data(data_.data() + pos_, data_.data() + pos_ + data_size);
        info.custom_sections.emplace_back(std::move(name), std::move(data));
        pos_ += data_size;
    }

    void parse_type_section(WasmModuleInfo& info) {
        std::uint32_t count = read_leb128_u32();
        function_types_.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint8_t form = read_byte();
            if (form != 0x60) {
                throw WasmException(WasmError::InvalidModule, "Invalid function type");
            }

            WasmFunctionType func_type;

            std::uint32_t param_count = read_leb128_u32();
            func_type.params.reserve(param_count);
            for (std::uint32_t j = 0; j < param_count; ++j) {
                func_type.params.push_back(read_valtype());
            }

            std::uint32_t result_count = read_leb128_u32();
            func_type.results.reserve(result_count);
            for (std::uint32_t j = 0; j < result_count; ++j) {
                func_type.results.push_back(read_valtype());
            }

            function_types_.push_back(std::move(func_type));
        }
    }

    void parse_import_section(WasmModuleInfo& info) {
        std::uint32_t count = read_leb128_u32();
        info.imports.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            WasmImport import;
            import.module = read_name();
            import.name = read_name();

            std::uint8_t kind = read_byte();
            switch (kind) {
                case 0x00: { // Function
                    import.kind = WasmExternKind::Func;
                    std::uint32_t type_idx = read_leb128_u32();
                    if (type_idx < function_types_.size()) {
                        import.func_type = function_types_[type_idx];
                    }
                    break;
                }
                case 0x01: { // Table
                    import.kind = WasmExternKind::Table;
                    WasmTableType table;
                    table.element_type = read_valtype();
                    std::uint8_t limits_flags = read_byte();
                    table.limits.min = read_leb128_u32();
                    if (limits_flags & 0x01) {
                        table.limits.max = read_leb128_u32();
                    }
                    import.table_type = table;
                    break;
                }
                case 0x02: { // Memory
                    import.kind = WasmExternKind::Memory;
                    WasmMemoryType memory;
                    std::uint8_t limits_flags = read_byte();
                    memory.limits.min = read_leb128_u32();
                    if (limits_flags & 0x01) {
                        memory.limits.max = read_leb128_u32();
                    }
                    memory.shared = (limits_flags & 0x02) != 0;
                    import.memory_type = memory;
                    break;
                }
                case 0x03: { // Global
                    import.kind = WasmExternKind::Global;
                    WasmGlobalType global;
                    global.value_type = read_valtype();
                    global.mutable_ = read_byte() != 0;
                    import.global_type = global;
                    break;
                }
                default:
                    throw WasmException(WasmError::InvalidModule, "Unknown import kind");
            }

            info.imports.push_back(std::move(import));
        }
    }

    void parse_export_section(WasmModuleInfo& info) {
        std::uint32_t count = read_leb128_u32();
        info.exports.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            WasmExport exp;
            exp.name = read_name();

            std::uint8_t kind = read_byte();
            switch (kind) {
                case 0x00: exp.kind = WasmExternKind::Func; break;
                case 0x01: exp.kind = WasmExternKind::Table; break;
                case 0x02: exp.kind = WasmExternKind::Memory; break;
                case 0x03: exp.kind = WasmExternKind::Global; break;
                default:
                    throw WasmException(WasmError::InvalidModule, "Unknown export kind");
            }

            exp.index = read_leb128_u32();
            info.exports.push_back(std::move(exp));
        }
    }

    std::span<const std::uint8_t> data_;
    std::size_t pos_;
    std::vector<WasmFunctionType> function_types_;
};

} // anonymous namespace

WasmResult<std::unique_ptr<WasmModule>> WasmModule::compile(
    WasmModuleId id,
    const std::string& name,
    std::span<const std::uint8_t> binary,
    const WasmConfig& config) {

    auto module = std::make_unique<WasmModule>(id, name);
    module->binary_.assign(binary.begin(), binary.end());

    // Parse and validate
    try {
        WasmParser parser(binary);
        auto result = parser.parse();
        if (!result) {
            return result.error();
        }
        module->info_ = std::move(result.value());
        module->info_.name = name;
    } catch (const WasmException& e) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, e.message()};
    }

    // TODO: Backend-specific compilation (wasmtime, wasmer, etc.)
    // For now, we just store the binary and info
    module->valid_ = true;

    return std::move(module);
}

WasmResult<std::unique_ptr<WasmModule>> WasmModule::compile_file(
    WasmModuleId id,
    const std::filesystem::path& path,
    const WasmConfig& config) {

    // Read file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    std::size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> binary(size);
    if (!file.read(reinterpret_cast<char*>(binary.data()), size)) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    std::string name = path.stem().string();
    return compile(id, name, binary, config);
}

// =============================================================================
// WasmInstance Implementation
// =============================================================================

WasmInstance::WasmInstance(WasmInstanceId id, const WasmModule& module)
    : id_(id)
    , module_(&module) {

    // Create default memory if module has memory
    if (module.info().num_memories > 0 || !module.info().imports.empty()) {
        // Find memory import or use default
        std::size_t initial_pages = 1;
        std::optional<std::size_t> max_pages;

        for (const auto& import : module.info().imports) {
            if (import.kind == WasmExternKind::Memory && import.memory_type) {
                initial_pages = import.memory_type->limits.min;
                if (import.memory_type->limits.max) {
                    max_pages = *import.memory_type->limits.max;
                }
                break;
            }
        }

        memories_.push_back(std::make_unique<WasmMemory>(initial_pages, max_pages));
    }

    // Build export map
    for (const auto& exp : module.info().exports) {
        export_map_[exp.name] = exp.index;
    }

    initialized_ = true;
}

WasmInstance::~WasmInstance() {
    // Clean up backend-specific instance
    instance_ = nullptr;
}

WasmInstance::WasmInstance(WasmInstance&& other) noexcept
    : id_(other.id_)
    , module_(other.module_)
    , initialized_(other.initialized_)
    , memories_(std::move(other.memories_))
    , export_map_(std::move(other.export_map_))
    , instance_(other.instance_)
    , fuel_(other.fuel_) {
    other.initialized_ = false;
    other.instance_ = nullptr;
}

WasmInstance& WasmInstance::operator=(WasmInstance&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        module_ = other.module_;
        initialized_ = other.initialized_;
        memories_ = std::move(other.memories_);
        export_map_ = std::move(other.export_map_);
        instance_ = other.instance_;
        fuel_ = other.fuel_;

        other.initialized_ = false;
        other.instance_ = nullptr;
    }
    return *this;
}

WasmMemory* WasmInstance::memory(std::size_t index) {
    if (index >= memories_.size()) return nullptr;
    return memories_[index].get();
}

const WasmMemory* WasmInstance::memory(std::size_t index) const {
    if (index >= memories_.size()) return nullptr;
    return memories_[index].get();
}

WasmResult<std::vector<WasmValue>> WasmInstance::call(
    const std::string& function_name,
    std::span<const WasmValue> args) {

    auto it = export_map_.find(function_name);
    if (it == export_map_.end()) {
        return void_core::Error{void_core::ErrorCode::NotFound, "WASM export not found"};
    }

    return call(it->second, args);
}

WasmResult<std::vector<WasmValue>> WasmInstance::call(
    std::uint32_t function_index,
    std::span<const WasmValue> args) {

    if (!initialized_ || memories_.empty()) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    // Create interpreter (thread-local for thread safety)
    thread_local WasmInterpreter interpreter;

    // Parse the module for interpretation
    auto parse_result = interpreter.parse_module(module_->binary());
    if (!parse_result) {
        return parse_result.error();
    }

    const ParsedModule& parsed = parse_result.value();

    // Register host functions from the runtime
    if (auto* runtime = WasmRuntime::instance_ptr()) {
        for (const auto& imp : parsed.imports) {
            if (imp.kind == WasmExternKind::Func) {
                auto callback = runtime->get_host_function(imp.module, imp.name);
                if (callback) {
                    WasmInterpreter::HostFunctionEntry entry;
                    entry.module = imp.module;
                    entry.name = imp.name;
                    if (imp.func_type) {
                        entry.signature = *imp.func_type;
                    }
                    entry.callback = [callback](std::span<const WasmValue> args, void* ud)
                        -> WasmResult<std::vector<WasmValue>> {
                        return callback(args, ud);
                    };
                    interpreter.register_host_function(std::move(entry));
                }
            }
        }
    }

    // Set fuel limit
    if (fuel_ > 0) {
        interpreter.set_fuel(fuel_);
    }

    // Execute the function
    auto result = interpreter.execute(parsed, *memories_[0], function_index, args);

    // Update remaining fuel
    fuel_ = interpreter.remaining_fuel();

    return result;
}

WasmResult<WasmValue> WasmInstance::get_global(const std::string& name) const {
    // TODO: Implement global access
    return void_core::Error{void_core::ErrorCode::NotFound, "WASM export not found"};
}

WasmResult<void> WasmInstance::set_global(const std::string& name, WasmValue value) {
    // TODO: Implement global access
    return void_core::Error{void_core::ErrorCode::NotFound, "WASM export not found"};
}

WasmResult<WasmValue> WasmInstance::table_get(std::size_t table_index, std::uint32_t elem_index) const {
    // TODO: Implement table access
    return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM out of bounds"};
}

WasmResult<void> WasmInstance::table_set(std::size_t table_index, std::uint32_t elem_index, WasmValue value) {
    // TODO: Implement table access
    return void_core::Error{void_core::ErrorCode::InvalidArgument, "WASM out of bounds"};
}

void WasmInstance::set_fuel(std::uint64_t fuel) {
    fuel_ = fuel;
}

std::uint64_t WasmInstance::remaining_fuel() const {
    return fuel_;
}

// =============================================================================
// WasmRuntime Implementation
// =============================================================================

namespace {
    WasmRuntime* g_runtime_instance = nullptr;
}

WasmRuntime::WasmRuntime(const WasmConfig& config)
    : config_(config) {
    VOID_LOG_INFO("[WasmRuntime] Initialized with backend: {}",
                  static_cast<int>(config.backend));
}

WasmRuntime::~WasmRuntime() {
    // Clean up all instances first
    instances_.clear();

    // Then modules
    modules_.clear();

    // Then host functions
    host_functions_.clear();

    VOID_LOG_INFO("[WasmRuntime] Shutdown complete");
}

WasmRuntime& WasmRuntime::instance() {
    if (!g_runtime_instance) {
        static WasmRuntime default_instance;
        g_runtime_instance = &default_instance;
    }
    return *g_runtime_instance;
}

WasmRuntime* WasmRuntime::instance_ptr() {
    return g_runtime_instance;
}

WasmResult<WasmModule*> WasmRuntime::compile_module(
    const std::string& name,
    std::span<const std::uint8_t> binary) {

    // Check if already loaded
    if (auto* existing = find_module(name)) {
        return existing;
    }

    WasmModuleId id = WasmModuleId::create(next_module_id_++, 0);
    auto result = WasmModule::compile(id, name, binary, config_);

    if (!result) {
        return result.error();
    }

    auto* module = result.value().get();
    modules_[id] = std::move(result.value());
    module_names_[name] = id;
    stats_.modules_loaded++;

    VOID_LOG_INFO("[WasmRuntime] Compiled module '{}' ({} imports, {} exports)",
                  name, module->imports().size(), module->exports().size());

    return module;
}

WasmResult<WasmModule*> WasmRuntime::compile_module(
    const std::string& name,
    const std::filesystem::path& path) {

    WasmModuleId id = WasmModuleId::create(next_module_id_++, 0);
    auto result = WasmModule::compile_file(id, path, config_);

    if (!result) {
        return result.error();
    }

    auto* module = result.value().get();
    std::string module_name = name.empty() ? path.stem().string() : name;

    modules_[id] = std::move(result.value());
    module_names_[module_name] = id;
    stats_.modules_loaded++;

    VOID_LOG_INFO("[WasmRuntime] Compiled module '{}' from {}",
                  module_name, path.string());

    return module;
}

WasmModule* WasmRuntime::get_module(WasmModuleId id) {
    auto it = modules_.find(id);
    return (it != modules_.end()) ? it->second.get() : nullptr;
}

const WasmModule* WasmRuntime::get_module(WasmModuleId id) const {
    auto it = modules_.find(id);
    return (it != modules_.end()) ? it->second.get() : nullptr;
}

WasmModule* WasmRuntime::find_module(const std::string& name) {
    auto it = module_names_.find(name);
    if (it == module_names_.end()) return nullptr;
    return get_module(it->second);
}

bool WasmRuntime::unload_module(WasmModuleId id) {
    auto it = modules_.find(id);
    if (it == modules_.end()) return false;

    // Remove from name map
    for (auto name_it = module_names_.begin(); name_it != module_names_.end(); ++name_it) {
        if (name_it->second == id) {
            module_names_.erase(name_it);
            break;
        }
    }

    // Destroy all instances of this module
    std::vector<WasmInstanceId> to_destroy;
    for (const auto& [inst_id, inst] : instances_) {
        if (inst->module().id() == id) {
            to_destroy.push_back(inst_id);
        }
    }
    for (auto inst_id : to_destroy) {
        destroy_instance(inst_id);
    }

    modules_.erase(it);
    stats_.modules_loaded--;

    return true;
}

std::vector<WasmModule*> WasmRuntime::modules() {
    std::vector<WasmModule*> result;
    result.reserve(modules_.size());
    for (auto& [id, module] : modules_) {
        result.push_back(module.get());
    }
    return result;
}

WasmResult<WasmInstance*> WasmRuntime::instantiate(WasmModuleId module_id) {
    auto* module = get_module(module_id);
    if (!module) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Invalid WASM module"};
    }

    // Check instance limit
    if (instances_.size() >= config_.max_instances) {
        return void_core::Error{void_core::ErrorCode::OutOfMemory, "WASM out of memory"};
    }

    WasmInstanceId id = WasmInstanceId::create(next_instance_id_++, 0);
    auto instance = std::make_unique<WasmInstance>(id, *module);

    // Set fuel if configured
    if (config_.fuel_limit > 0) {
        instance->set_fuel(config_.fuel_limit);
    }

    auto* inst_ptr = instance.get();
    instances_[id] = std::move(instance);
    stats_.instances_active++;

    VOID_LOG_DEBUG("[WasmRuntime] Instantiated module '{}'", module->name());

    return inst_ptr;
}

WasmResult<WasmInstance*> WasmRuntime::instantiate(
    WasmModuleId module_id,
    const std::unordered_map<std::string, HostFunctionCallback>& imports) {

    // TODO: Resolve custom imports
    return instantiate(module_id);
}

WasmInstance* WasmRuntime::get_instance(WasmInstanceId id) {
    auto it = instances_.find(id);
    return (it != instances_.end()) ? it->second.get() : nullptr;
}

const WasmInstance* WasmRuntime::get_instance(WasmInstanceId id) const {
    auto it = instances_.find(id);
    return (it != instances_.end()) ? it->second.get() : nullptr;
}

bool WasmRuntime::destroy_instance(WasmInstanceId id) {
    auto it = instances_.find(id);
    if (it == instances_.end()) return false;

    instances_.erase(it);
    stats_.instances_active--;

    return true;
}

std::vector<WasmInstance*> WasmRuntime::instances() {
    std::vector<WasmInstance*> result;
    result.reserve(instances_.size());
    for (auto& [id, inst] : instances_) {
        result.push_back(inst.get());
    }
    return result;
}

HostFunctionId WasmRuntime::register_host_function(
    const std::string& module,
    const std::string& name,
    WasmFunctionType signature,
    HostFunctionCallback callback,
    void* user_data) {

    HostFunctionId id = HostFunctionId::create(next_host_function_id_++, 0);

    HostFunctionEntry entry{
        module,
        name,
        std::move(signature),
        std::move(callback),
        user_data
    };

    host_functions_[id] = std::move(entry);

    std::string full_name = module + "." + name;
    host_function_names_[full_name] = id;
    stats_.host_functions++;

    VOID_LOG_DEBUG("[WasmRuntime] Registered host function {}.{}", module, name);

    return id;
}

bool WasmRuntime::unregister_host_function(HostFunctionId id) {
    auto it = host_functions_.find(id);
    if (it == host_functions_.end()) return false;

    std::string full_name = it->second.module + "." + it->second.name;
    host_function_names_.erase(full_name);
    host_functions_.erase(it);
    stats_.host_functions--;

    return true;
}

HostFunctionCallback WasmRuntime::get_host_function(
    const std::string& module,
    const std::string& name) const {

    std::string full_name = module + "." + name;
    auto it = host_function_names_.find(full_name);
    if (it == host_function_names_.end()) return nullptr;

    auto func_it = host_functions_.find(it->second);
    if (func_it == host_functions_.end()) return nullptr;

    return func_it->second.callback;
}

void WasmRuntime::register_wasi_imports() {
    // WASI preview1 imports

    // args_get
    register_host_function("wasi_snapshot_preview1", "args_get",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // args_sizes_get
    register_host_function("wasi_snapshot_preview1", "args_sizes_get",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // environ_get
    register_host_function("wasi_snapshot_preview1", "environ_get",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // environ_sizes_get
    register_host_function("wasi_snapshot_preview1", "environ_sizes_get",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // clock_time_get
    register_host_function("wasi_snapshot_preview1", "clock_time_get",
        WasmFunctionType{{WasmValType::I32, WasmValType::I64, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // fd_write (for console output)
    register_host_function("wasi_snapshot_preview1", "fd_write",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32, WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Implement actual fd_write
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // fd_close
    register_host_function("wasi_snapshot_preview1", "fd_close",
        WasmFunctionType{{WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // fd_seek
    register_host_function("wasi_snapshot_preview1", "fd_seek",
        WasmFunctionType{{WasmValType::I32, WasmValType::I64, WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{WasmValue{std::int32_t(0)}};
        });

    // proc_exit
    register_host_function("wasi_snapshot_preview1", "proc_exit",
        WasmFunctionType{{WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            return std::vector<WasmValue>{};
        });

    VOID_LOG_INFO("[WasmRuntime] Registered WASI imports");
}

void WasmRuntime::register_engine_imports() {
    // Engine API imports

    // void_log
    register_host_function("void", "log",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Read string from memory and log
            return std::vector<WasmValue>{};
        });

    // void_get_time
    register_host_function("void", "get_time",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            auto now = std::chrono::steady_clock::now();
            double time = std::chrono::duration<double>(now.time_since_epoch()).count();
            return std::vector<WasmValue>{WasmValue{time}};
        });

    // void_get_delta_time
    register_host_function("void", "get_delta_time",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Get actual delta time from engine
            return std::vector<WasmValue>{WasmValue{0.016}};
        });

    // void_create_entity
    register_host_function("void", "create_entity",
        WasmFunctionType{{}, {WasmValType::I64}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Integrate with ECS
            static std::uint64_t next_entity = 1;
            return std::vector<WasmValue>{WasmValue{static_cast<std::int64_t>(next_entity++)}};
        });

    // void_destroy_entity
    register_host_function("void", "destroy_entity",
        WasmFunctionType{{WasmValType::I64}, {}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Integrate with ECS
            return std::vector<WasmValue>{};
        });

    // void_random_f64
    register_host_function("void", "random_f64",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void*) -> WasmResult<std::vector<WasmValue>> {
            double value = static_cast<double>(rand()) / RAND_MAX;
            return std::vector<WasmValue>{WasmValue{value}};
        });

    VOID_LOG_INFO("[WasmRuntime] Registered engine imports");
}

WasmRuntime::Stats WasmRuntime::stats() const {
    Stats s = stats_;

    // Calculate total memory
    s.total_memory_bytes = 0;
    for (const auto& [id, inst] : instances_) {
        for (std::size_t i = 0; i < inst->memory_count(); ++i) {
            if (auto* mem = inst->memory(i)) {
                s.total_memory_bytes += mem->size();
            }
        }
    }

    return s;
}

} // namespace void_scripting
