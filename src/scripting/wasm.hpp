#pragma once

/// @file wasm.hpp
/// @brief WASM module and instance management

#include "types.hpp"

#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_scripting {

// =============================================================================
// WASM Memory
// =============================================================================

/// @brief WASM linear memory
class WasmMemory {
public:
    WasmMemory(std::size_t initial_pages, std::optional<std::size_t> max_pages = std::nullopt);
    ~WasmMemory();

    // Non-copyable
    WasmMemory(const WasmMemory&) = delete;
    WasmMemory& operator=(const WasmMemory&) = delete;

    // Movable
    WasmMemory(WasmMemory&&) noexcept;
    WasmMemory& operator=(WasmMemory&&) noexcept;

    // Access
    [[nodiscard]] std::uint8_t* data() { return data_.get(); }
    [[nodiscard]] const std::uint8_t* data() const { return data_.get(); }
    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] std::size_t pages() const { return size_ / page_size; }
    [[nodiscard]] std::size_t max_pages() const { return max_pages_.value_or(65536); }

    // Growth
    WasmResult<std::size_t> grow(std::size_t delta_pages);

    // Read/Write
    template <typename T>
    [[nodiscard]] T read(std::size_t offset) const;

    template <typename T>
    void write(std::size_t offset, T value);

    void read_bytes(std::size_t offset, std::span<std::uint8_t> buffer) const;
    void write_bytes(std::size_t offset, std::span<const std::uint8_t> data);

    // String helpers
    [[nodiscard]] std::string read_string(std::size_t offset, std::size_t max_len = 4096) const;
    std::size_t write_string(std::size_t offset, const std::string& str);

    // Bounds checking
    [[nodiscard]] bool check_bounds(std::size_t offset, std::size_t size) const;

    static constexpr std::size_t page_size = 65536;  // 64 KB

private:
    std::unique_ptr<std::uint8_t[]> data_;
    std::size_t size_ = 0;
    std::optional<std::size_t> max_pages_;
};

// =============================================================================
// WASM Module
// =============================================================================

/// @brief Compiled WASM module
class WasmModule {
public:
    WasmModule(WasmModuleId id, std::string name);
    ~WasmModule();

    // Non-copyable, movable
    WasmModule(const WasmModule&) = delete;
    WasmModule& operator=(const WasmModule&) = delete;
    WasmModule(WasmModule&&) noexcept;
    WasmModule& operator=(WasmModule&&) noexcept;

    // Identity
    [[nodiscard]] WasmModuleId id() const { return id_; }
    [[nodiscard]] const std::string& name() const { return name_; }

    // Module info
    [[nodiscard]] const WasmModuleInfo& info() const { return info_; }
    [[nodiscard]] bool is_valid() const { return valid_; }

    // Imports/Exports
    [[nodiscard]] std::span<const WasmImport> imports() const { return info_.imports; }
    [[nodiscard]] std::span<const WasmExport> exports() const { return info_.exports; }
    [[nodiscard]] const WasmExport* find_export(const std::string& name) const;

    // Custom sections
    [[nodiscard]] std::optional<std::span<const std::uint8_t>>
    get_custom_section(const std::string& name) const;

    // Binary
    [[nodiscard]] std::span<const std::uint8_t> binary() const { return binary_; }

    // Compilation
    static WasmResult<std::unique_ptr<WasmModule>> compile(
        WasmModuleId id,
        const std::string& name,
        std::span<const std::uint8_t> binary,
        const WasmConfig& config = {});

    static WasmResult<std::unique_ptr<WasmModule>> compile_file(
        WasmModuleId id,
        const std::filesystem::path& path,
        const WasmConfig& config = {});

private:
    WasmModuleId id_;
    std::string name_;
    WasmModuleInfo info_;
    std::vector<std::uint8_t> binary_;
    bool valid_ = false;

    // Backend-specific compiled module (opaque)
    void* compiled_module_ = nullptr;
};

// =============================================================================
// WASM Instance
// =============================================================================

/// @brief Instantiated WASM module
class WasmInstance {
public:
    WasmInstance(WasmInstanceId id, const WasmModule& module);
    ~WasmInstance();

    // Non-copyable, movable
    WasmInstance(const WasmInstance&) = delete;
    WasmInstance& operator=(const WasmInstance&) = delete;
    WasmInstance(WasmInstance&&) noexcept;
    WasmInstance& operator=(WasmInstance&&) noexcept;

    // Identity
    [[nodiscard]] WasmInstanceId id() const { return id_; }
    [[nodiscard]] const WasmModule& module() const { return *module_; }

    // Memory
    [[nodiscard]] WasmMemory* memory(std::size_t index = 0);
    [[nodiscard]] const WasmMemory* memory(std::size_t index = 0) const;
    [[nodiscard]] std::size_t memory_count() const { return memories_.size(); }

    // Function calls
    WasmResult<std::vector<WasmValue>> call(
        const std::string& function_name,
        std::span<const WasmValue> args = {});

    WasmResult<std::vector<WasmValue>> call(
        std::uint32_t function_index,
        std::span<const WasmValue> args = {});

    // Typed call helpers
    template <typename R, typename... Args>
    WasmResult<R> call_typed(const std::string& function_name, Args... args);

    // Globals
    [[nodiscard]] WasmResult<WasmValue> get_global(const std::string& name) const;
    WasmResult<void> set_global(const std::string& name, WasmValue value);

    // Tables
    [[nodiscard]] WasmResult<WasmValue> table_get(std::size_t table_index, std::uint32_t elem_index) const;
    WasmResult<void> table_set(std::size_t table_index, std::uint32_t elem_index, WasmValue value);

    // Fuel (execution limiting)
    void set_fuel(std::uint64_t fuel);
    [[nodiscard]] std::uint64_t remaining_fuel() const;

    // State
    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    friend class WasmRuntime;

    WasmInstanceId id_;
    const WasmModule* module_;
    bool initialized_ = false;

    std::vector<std::unique_ptr<WasmMemory>> memories_;
    std::unordered_map<std::string, std::uint32_t> export_map_;

    // Globals and tables (for interpreter-based execution)
    mutable std::vector<WasmValue> globals_;
    mutable std::vector<std::vector<std::uint32_t>> tables_;

    // Backend-specific instance (opaque)
    void* instance_ = nullptr;
    std::uint64_t fuel_ = 0;
};

// =============================================================================
// WASM Runtime
// =============================================================================

/// @brief WASM runtime engine
class WasmRuntime {
public:
    explicit WasmRuntime(const WasmConfig& config = {});
    ~WasmRuntime();

    // Singleton access
    [[nodiscard]] static WasmRuntime& instance();
    [[nodiscard]] static WasmRuntime* instance_ptr();

    // Configuration
    [[nodiscard]] const WasmConfig& config() const { return config_; }
    [[nodiscard]] WasmBackend backend() const { return config_.backend; }

    // ==========================================================================
    // Module Management
    // ==========================================================================

    /// @brief Compile a module from binary
    WasmResult<WasmModule*> compile_module(
        const std::string& name,
        std::span<const std::uint8_t> binary);

    /// @brief Compile a module from file
    WasmResult<WasmModule*> compile_module(
        const std::string& name,
        const std::filesystem::path& path);

    /// @brief Get a compiled module
    [[nodiscard]] WasmModule* get_module(WasmModuleId id);
    [[nodiscard]] const WasmModule* get_module(WasmModuleId id) const;

    /// @brief Find a module by name
    [[nodiscard]] WasmModule* find_module(const std::string& name);

    /// @brief Unload a module
    bool unload_module(WasmModuleId id);

    /// @brief Get all modules
    [[nodiscard]] std::vector<WasmModule*> modules();

    // ==========================================================================
    // Instance Management
    // ==========================================================================

    /// @brief Instantiate a module
    WasmResult<WasmInstance*> instantiate(WasmModuleId module_id);

    /// @brief Instantiate with custom imports
    WasmResult<WasmInstance*> instantiate(
        WasmModuleId module_id,
        const std::unordered_map<std::string, HostFunctionCallback>& imports);

    /// @brief Get an instance
    [[nodiscard]] WasmInstance* get_instance(WasmInstanceId id);
    [[nodiscard]] const WasmInstance* get_instance(WasmInstanceId id) const;

    /// @brief Destroy an instance
    bool destroy_instance(WasmInstanceId id);

    /// @brief Get all instances
    [[nodiscard]] std::vector<WasmInstance*> instances();

    // ==========================================================================
    // Host Functions
    // ==========================================================================

    /// @brief Register a host function
    HostFunctionId register_host_function(
        const std::string& module,
        const std::string& name,
        WasmFunctionType signature,
        HostFunctionCallback callback,
        void* user_data = nullptr);

    /// @brief Unregister a host function
    bool unregister_host_function(HostFunctionId id);

    /// @brief Get host function by name
    [[nodiscard]] HostFunctionCallback get_host_function(
        const std::string& module,
        const std::string& name) const;

    // ==========================================================================
    // Default Imports
    // ==========================================================================

    /// @brief Register WASI imports
    void register_wasi_imports();

    /// @brief Register engine API imports
    void register_engine_imports();

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t modules_loaded = 0;
        std::size_t instances_active = 0;
        std::size_t total_memory_bytes = 0;
        std::size_t host_functions = 0;
        std::uint64_t total_calls = 0;
    };

    [[nodiscard]] Stats stats() const;

private:
    WasmConfig config_;

    std::unordered_map<WasmModuleId, std::unique_ptr<WasmModule>> modules_;
    std::unordered_map<std::string, WasmModuleId> module_names_;

    std::unordered_map<WasmInstanceId, std::unique_ptr<WasmInstance>> instances_;

    struct HostFunctionEntry {
        std::string module;
        std::string name;
        WasmFunctionType signature;
        HostFunctionCallback callback;
        void* user_data;
    };
    std::unordered_map<HostFunctionId, HostFunctionEntry> host_functions_;
    std::unordered_map<std::string, HostFunctionId> host_function_names_;

    Stats stats_;

    inline static std::uint32_t next_module_id_ = 1;
    inline static std::uint32_t next_instance_id_ = 1;
    inline static std::uint32_t next_host_function_id_ = 1;
};

// =============================================================================
// Template Implementations
// =============================================================================

template <typename T>
T WasmMemory::read(std::size_t offset) const {
    if (!check_bounds(offset, sizeof(T))) {
        throw WasmException(WasmError::OutOfBounds, "Memory read out of bounds");
    }
    T value;
    std::memcpy(&value, data_.get() + offset, sizeof(T));
    return value;
}

template <typename T>
void WasmMemory::write(std::size_t offset, T value) {
    if (!check_bounds(offset, sizeof(T))) {
        throw WasmException(WasmError::OutOfBounds, "Memory write out of bounds");
    }
    std::memcpy(data_.get() + offset, &value, sizeof(T));
}

template <typename R, typename... Args>
WasmResult<R> WasmInstance::call_typed(const std::string& function_name, Args... args) {
    std::vector<WasmValue> wasm_args;
    (wasm_args.push_back(WasmValue(args)), ...);

    auto result = call(function_name, wasm_args);
    if (!result) {
        return WasmResult<R>::error(result.error());
    }

    if (result.value().empty()) {
        if constexpr (std::is_void_v<R>) {
            return WasmResult<R>::ok();
        } else {
            return WasmResult<R>::error(WasmError::InvalidConversion);
        }
    }

    if constexpr (std::is_same_v<R, std::int32_t>) {
        return WasmResult<R>::ok(result.value()[0].i32);
    } else if constexpr (std::is_same_v<R, std::int64_t>) {
        return WasmResult<R>::ok(result.value()[0].i64);
    } else if constexpr (std::is_same_v<R, float>) {
        return WasmResult<R>::ok(result.value()[0].f32);
    } else if constexpr (std::is_same_v<R, double>) {
        return WasmResult<R>::ok(result.value()[0].f64);
    } else {
        return WasmResult<R>::error(WasmError::InvalidConversion);
    }
}

} // namespace void_scripting
