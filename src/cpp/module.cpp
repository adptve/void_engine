/// @file module.cpp
/// @brief Dynamic module loading and management implementation

#include "module.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>
#include <fstream>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
// SymTag values from cvconst.h if not available
#ifndef SymTagFunction
#define SymTagFunction 5
#define SymTagData 7
#endif
#else
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#endif

namespace void_cpp {

// =============================================================================
// Platform-Specific Implementation
// =============================================================================

namespace platform {

const char* shared_library_extension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

const char* shared_library_prefix() {
#ifdef _WIN32
    return "";
#else
    return "lib";
#endif
}

std::string format_library_name(const std::string& name) {
    std::string result = shared_library_prefix();
    result += name;
    result += shared_library_extension();
    return result;
}

void* load_library(const std::filesystem::path& path) {
#ifdef _WIN32
    return LoadLibraryW(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

bool unload_library(void* handle) {
    if (!handle) return false;
#ifdef _WIN32
    return FreeLibrary(static_cast<HMODULE>(handle)) != 0;
#else
    return dlclose(handle) == 0;
#endif
}

void* get_symbol(void* handle, const char* name) {
    if (!handle || !name) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

std::string get_last_error() {
#ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) return "";

    LPSTR buffer = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer,
        0,
        nullptr);

    std::string message(buffer ? buffer : "Unknown error");
    LocalFree(buffer);
    return message;
#else
    const char* error = dlerror();
    return error ? error : "";
#endif
}

std::vector<SymbolInfo> enumerate_symbols(void* handle) {
    std::vector<SymbolInfo> symbols;

#ifdef _WIN32
    // Use DbgHelp to enumerate symbols
    HMODULE hmod = static_cast<HMODULE>(handle);

    // Get module info
    MODULEINFO mod_info;
    if (!GetModuleInformation(GetCurrentProcess(), hmod, &mod_info, sizeof(mod_info))) {
        return symbols;
    }

    // Initialize symbol handler
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        return symbols;
    }

    // Enumerate symbols
    struct EnumContext {
        std::vector<SymbolInfo>* symbols;
        std::uint32_t next_id;
    };

    EnumContext ctx{&symbols, 1};

    SymEnumSymbols(
        GetCurrentProcess(),
        reinterpret_cast<DWORD64>(hmod),
        "*",
        [](PSYMBOL_INFO info, ULONG size, PVOID context) -> BOOL {
            auto* ctx = static_cast<EnumContext*>(context);

            SymbolInfo sym;
            sym.id = SymbolId::create(ctx->next_id++, 0);
            sym.name = info->Name;
            sym.demangled_name = info->Name;  // Already demangled by SYMOPT_UNDNAME
            sym.address = reinterpret_cast<void*>(info->Address);
            sym.size = info->Size;

            if (info->Tag == SymTagFunction) {
                sym.type = SymbolType::Function;
            } else if (info->Tag == SymTagData) {
                sym.type = SymbolType::Variable;
            } else {
                sym.type = SymbolType::Unknown;
            }

            ctx->symbols->push_back(std::move(sym));
            return TRUE;
        },
        &ctx);

    SymCleanup(GetCurrentProcess());

#else
    // Unix: Enumerate exported symbols using dlinfo and ELF parsing
    struct link_map* map = nullptr;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || !map) {
        return symbols;
    }

    // Get the library path from link_map
    const char* lib_path = map->l_name;
    if (!lib_path || lib_path[0] == '\0') {
        return symbols;
    }

    // Parse ELF to get dynamic symbol table
    std::ifstream file(lib_path, std::ios::binary);
    if (!file) return symbols;

    // Read ELF header
    Elf64_Ehdr ehdr;
    file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

    // Check ELF magic
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        return symbols;
    }

    // Read section headers
    file.seekg(ehdr.e_shoff);
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    file.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * sizeof(Elf64_Shdr));

    // Find .dynsym and .dynstr sections
    const Elf64_Shdr* dynsym = nullptr;
    const Elf64_Shdr* dynstr = nullptr;

    for (const auto& shdr : shdrs) {
        if (shdr.sh_type == SHT_DYNSYM) {
            dynsym = &shdr;
        }
        if (shdr.sh_type == SHT_STRTAB && !dynstr) {
            // First string table is usually .dynstr
            dynstr = &shdr;
        }
    }

    if (!dynsym || !dynstr) {
        return symbols;
    }

    // Read symbol table
    file.seekg(dynsym->sh_offset);
    std::size_t num_symbols = dynsym->sh_size / sizeof(Elf64_Sym);

    for (std::size_t i = 0; i < num_symbols; ++i) {
        Elf64_Sym sym_entry;
        file.read(reinterpret_cast<char*>(&sym_entry), sizeof(sym_entry));

        // Skip undefined and local symbols
        if (sym_entry.st_shndx == SHN_UNDEF) continue;
        if (ELF64_ST_BIND(sym_entry.st_info) == STB_LOCAL) continue;

        // Read symbol name
        auto pos = file.tellg();
        file.seekg(dynstr->sh_offset + sym_entry.st_name);

        char name[512];
        file.getline(name, sizeof(name), '\0');
        file.seekg(pos);

        if (name[0] == '\0') continue;

        SymbolInfo sym;
        sym.id = SymbolId::create(static_cast<std::uint32_t>(i), 0);
        sym.name = name;
        sym.demangled_name = name; // Could demangle with abi::__cxa_demangle
        sym.size = sym_entry.st_size;

        // Get address via dlsym
        sym.address = dlsym(handle, name);

        // Determine type
        unsigned char type = ELF64_ST_TYPE(sym_entry.st_info);
        if (type == STT_FUNC) {
            sym.type = SymbolType::Function;
        } else if (type == STT_OBJECT) {
            sym.type = SymbolType::Variable;
        } else {
            sym.type = SymbolType::Unknown;
        }

        symbols.push_back(std::move(sym));
    }
#endif

    return symbols;
}

} // namespace platform

// =============================================================================
// DynamicModule Implementation
// =============================================================================

DynamicModule::DynamicModule(ModuleId id, std::string name)
    : id_(id)
    , name_(std::move(name)) {}

DynamicModule::~DynamicModule() {
    if (is_loaded()) {
        unload();
    }
}

DynamicModule::DynamicModule(DynamicModule&& other) noexcept
    : id_(other.id_)
    , name_(std::move(other.name_))
    , path_(std::move(other.path_))
    , state_(other.state_)
    , handle_(other.handle_)
    , info_(std::move(other.info_))
    , symbols_(std::move(other.symbols_))
    , symbol_cache_(std::move(other.symbol_cache_))
    , loaded_file_time_(other.loaded_file_time_)
    , error_message_(std::move(other.error_message_)) {
    other.handle_ = nullptr;
    other.state_ = ModuleState::Unloaded;
}

DynamicModule& DynamicModule::operator=(DynamicModule&& other) noexcept {
    if (this != &other) {
        if (is_loaded()) {
            unload();
        }

        id_ = other.id_;
        name_ = std::move(other.name_);
        path_ = std::move(other.path_);
        state_ = other.state_;
        handle_ = other.handle_;
        info_ = std::move(other.info_);
        symbols_ = std::move(other.symbols_);
        symbol_cache_ = std::move(other.symbol_cache_);
        loaded_file_time_ = other.loaded_file_time_;
        error_message_ = std::move(other.error_message_);

        other.handle_ = nullptr;
        other.state_ = ModuleState::Unloaded;
    }
    return *this;
}

CppResult<void> DynamicModule::load(const std::filesystem::path& path) {
    if (is_loaded()) {
        return CppError::InvalidModule;
    }

    if (!std::filesystem::exists(path)) {
        error_message_ = "File not found: " + path.string();
        state_ = ModuleState::Error;
        return CppError::InvalidPath;
    }

    state_ = ModuleState::Loading;
    path_ = path;

    handle_ = platform::load_library(path);
    if (!handle_) {
        error_message_ = platform::get_last_error();
        state_ = ModuleState::Error;
        return CppError::LoadFailed;
    }

    loaded_file_time_ = std::filesystem::last_write_time(path);

    // Enumerate symbols
    enumerate_symbols_impl();

    // Update info
    info_.id = id_;
    info_.name = name_;
    info_.path = path_;
    info_.state = ModuleState::Loaded;
    info_.load_time = std::chrono::system_clock::now();
    info_.file_time = std::chrono::system_clock::now();
    info_.symbols = symbols_;

    state_ = ModuleState::Loaded;

    VOID_LOG_INFO("[DynamicModule] Loaded '{}' from {}", name_, path.string());

    return CppResult<void>::ok();
}

CppResult<void> DynamicModule::unload() {
    if (!is_loaded()) {
        return CppResult<void>::ok();
    }

    state_ = ModuleState::Unloading;

    if (handle_) {
        if (!platform::unload_library(handle_)) {
            error_message_ = platform::get_last_error();
            state_ = ModuleState::Error;
            return CppError::UnloadFailed;
        }
        handle_ = nullptr;
    }

    symbols_.clear();
    symbol_cache_.clear();

    state_ = ModuleState::Unloaded;

    VOID_LOG_INFO("[DynamicModule] Unloaded '{}'", name_);

    return CppResult<void>::ok();
}

CppResult<void> DynamicModule::reload() {
    auto path = path_;

    auto unload_result = unload();
    if (!unload_result) {
        return unload_result;
    }

    return load(path);
}

void* DynamicModule::get_symbol(const std::string& name) {
    if (!is_loaded()) {
        return nullptr;
    }

    // Check cache
    auto it = symbol_cache_.find(name);
    if (it != symbol_cache_.end()) {
        return it->second;
    }

    // Lookup
    void* sym = platform::get_symbol(handle_, name.c_str());
    if (sym) {
        symbol_cache_[name] = sym;
    }

    return sym;
}

bool DynamicModule::has_symbol(const std::string& name) {
    return get_symbol(name) != nullptr;
}

std::vector<SymbolInfo> DynamicModule::enumerate_symbols() const {
    if (!is_loaded()) {
        return {};
    }
    return platform::enumerate_symbols(handle_);
}

std::filesystem::file_time_type DynamicModule::file_time() const {
    if (std::filesystem::exists(path_)) {
        return std::filesystem::last_write_time(path_);
    }
    return {};
}

bool DynamicModule::has_file_changed() const {
    if (!is_loaded() || !std::filesystem::exists(path_)) {
        return false;
    }
    return std::filesystem::last_write_time(path_) > loaded_file_time_;
}

void DynamicModule::enumerate_symbols_impl() {
    symbols_ = platform::enumerate_symbols(handle_);
}

// =============================================================================
// ModuleRegistry Implementation
// =============================================================================

namespace {
    ModuleRegistry* g_registry_instance = nullptr;
}

ModuleRegistry::ModuleRegistry() {
    g_registry_instance = this;
}

ModuleRegistry::~ModuleRegistry() {
    unload_all();
    if (g_registry_instance == this) {
        g_registry_instance = nullptr;
    }
}

ModuleRegistry& ModuleRegistry::instance() {
    if (!g_registry_instance) {
        static ModuleRegistry default_instance;
        g_registry_instance = &default_instance;
    }
    return *g_registry_instance;
}

CppResult<DynamicModule*> ModuleRegistry::load(const std::filesystem::path& path) {
    std::string name = path.stem().string();
    return load(name, path);
}

CppResult<DynamicModule*> ModuleRegistry::load(const std::string& name, const std::filesystem::path& path) {
    // Check if already loaded
    if (auto* existing = find(name)) {
        return existing;
    }

    ModuleId id = ModuleId::create(next_module_id_++, 0);
    auto module = std::make_unique<DynamicModule>(id, name);

    auto result = module->load(path);
    if (!result) {
        return result.error();
    }

    auto* module_ptr = module.get();
    modules_[id] = std::move(module);
    module_names_[name] = id;

    return module_ptr;
}

bool ModuleRegistry::unload(ModuleId id) {
    auto it = modules_.find(id);
    if (it == modules_.end()) {
        return false;
    }

    std::string name = it->second->name();
    it->second->unload();

    module_names_.erase(name);
    modules_.erase(it);

    return true;
}

bool ModuleRegistry::unload(const std::string& name) {
    auto it = module_names_.find(name);
    if (it == module_names_.end()) {
        return false;
    }
    return unload(it->second);
}

CppResult<DynamicModule*> ModuleRegistry::reload(ModuleId id) {
    auto* module = get(id);
    if (!module) {
        return CppError::ModuleNotFound;
    }

    auto result = module->reload();
    if (!result) {
        return result.error();
    }

    return module;
}

DynamicModule* ModuleRegistry::get(ModuleId id) {
    auto it = modules_.find(id);
    return (it != modules_.end()) ? it->second.get() : nullptr;
}

const DynamicModule* ModuleRegistry::get(ModuleId id) const {
    auto it = modules_.find(id);
    return (it != modules_.end()) ? it->second.get() : nullptr;
}

DynamicModule* ModuleRegistry::find(const std::string& name) {
    auto it = module_names_.find(name);
    if (it == module_names_.end()) {
        return nullptr;
    }
    return get(it->second);
}

std::vector<DynamicModule*> ModuleRegistry::modules() {
    std::vector<DynamicModule*> result;
    result.reserve(modules_.size());
    for (auto& [id, module] : modules_) {
        result.push_back(module.get());
    }
    return result;
}

bool ModuleRegistry::exists(ModuleId id) const {
    return modules_.find(id) != modules_.end();
}

bool ModuleRegistry::exists(const std::string& name) const {
    return module_names_.find(name) != module_names_.end();
}

void* ModuleRegistry::find_symbol(const std::string& name) {
    for (auto& [id, module] : modules_) {
        if (auto* sym = module->get_symbol(name)) {
            return sym;
        }
    }
    return nullptr;
}

void* ModuleRegistry::find_symbol(ModuleId module_id, const std::string& name) {
    auto* module = get(module_id);
    return module ? module->get_symbol(name) : nullptr;
}

void ModuleRegistry::unload_all() {
    for (auto& [id, module] : modules_) {
        module->unload();
    }
    modules_.clear();
    module_names_.clear();
}

std::vector<DynamicModule*> ModuleRegistry::get_changed_modules() {
    std::vector<DynamicModule*> changed;
    for (auto& [id, module] : modules_) {
        if (module->has_file_changed()) {
            changed.push_back(module.get());
        }
    }
    return changed;
}

void ModuleRegistry::add_search_path(const std::filesystem::path& path) {
    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        search_paths_.push_back(path);
    }
}

void ModuleRegistry::clear_search_paths() {
    search_paths_.clear();
}

std::filesystem::path ModuleRegistry::resolve_path(const std::string& name) const {
    // Try name as-is first
    std::filesystem::path path(name);
    if (std::filesystem::exists(path)) {
        return path;
    }

    // Try with library name formatting
    std::string lib_name = platform::format_library_name(name);

    // Search in paths
    for (const auto& search_path : search_paths_) {
        auto full_path = search_path / lib_name;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }

        // Try without prefix
        full_path = search_path / (name + platform::shared_library_extension());
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
    }

    return {};
}

// =============================================================================
// ModuleLoader Implementation
// =============================================================================

ModuleLoader::ModuleLoader()
    : registry_(ModuleRegistry::instance()) {}

ModuleLoader::ModuleLoader(ModuleRegistry& registry)
    : registry_(registry) {}

CppResult<DynamicModule*> ModuleLoader::load_with_dependencies(const std::filesystem::path& path) {
    // Get load order
    auto order = get_load_order(path);

    // Load in order
    DynamicModule* result = nullptr;
    for (const auto& dep_path : order) {
        auto load_result = registry_.load(dep_path);
        if (!load_result) {
            return load_result;
        }
        if (dep_path == path) {
            result = load_result.value();
        }
    }

    return result;
}

std::vector<std::filesystem::path> ModuleLoader::get_load_order(const std::filesystem::path& path) const {
    std::vector<std::filesystem::path> order;

    // Get dependencies
    auto deps = get_dependencies(path);

    // Add dependencies first
    for (const auto& dep : deps) {
        auto resolved = registry_.resolve_path(dep);
        if (!resolved.empty() && !registry_.exists(dep)) {
            // Recursively get load order for dependency
            auto dep_order = get_load_order(resolved);
            order.insert(order.end(), dep_order.begin(), dep_order.end());
        }
    }

    // Add self
    order.push_back(path);

    return order;
}

std::vector<std::string> ModuleLoader::get_dependencies(const std::filesystem::path& path) const {
    std::vector<std::string> deps;

    if (!std::filesystem::exists(path)) {
        return deps;
    }

#ifdef _WIN32
    // Parse PE import table
    std::ifstream file(path, std::ios::binary);
    if (!file) return deps;

    // Read DOS header
    IMAGE_DOS_HEADER dos_header;
    file.read(reinterpret_cast<char*>(&dos_header), sizeof(dos_header));
    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) return deps;

    // Read PE header
    file.seekg(dos_header.e_lfanew);
    DWORD pe_signature;
    file.read(reinterpret_cast<char*>(&pe_signature), sizeof(pe_signature));
    if (pe_signature != IMAGE_NT_SIGNATURE) return deps;

    IMAGE_FILE_HEADER file_header;
    file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));

    // Read optional header to get import directory RVA
    IMAGE_OPTIONAL_HEADER64 opt_header;
    file.read(reinterpret_cast<char*>(&opt_header), sizeof(opt_header));

    if (opt_header.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) {
        return deps;
    }

    DWORD import_rva = opt_header.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (import_rva == 0) return deps;

    // Find section containing imports
    std::vector<IMAGE_SECTION_HEADER> sections(file_header.NumberOfSections);
    file.read(reinterpret_cast<char*>(sections.data()),
              file_header.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));

    for (const auto& section : sections) {
        if (import_rva >= section.VirtualAddress &&
            import_rva < section.VirtualAddress + section.Misc.VirtualSize) {

            DWORD import_offset = section.PointerToRawData + (import_rva - section.VirtualAddress);
            file.seekg(import_offset);

            // Read import descriptors
            while (true) {
                IMAGE_IMPORT_DESCRIPTOR desc;
                file.read(reinterpret_cast<char*>(&desc), sizeof(desc));
                if (desc.Name == 0) break;

                // Read DLL name
                DWORD name_offset = section.PointerToRawData + (desc.Name - section.VirtualAddress);
                auto current_pos = file.tellg();
                file.seekg(name_offset);

                char dll_name[256];
                file.getline(dll_name, sizeof(dll_name), '\0');
                deps.push_back(dll_name);

                file.seekg(current_pos);
            }
            break;
        }
    }

#else
    // Unix: Read ELF dynamic section using readelf-like parsing
    std::ifstream file(path, std::ios::binary);
    if (!file) return deps;

    // Read ELF header
    unsigned char e_ident[16];
    file.read(reinterpret_cast<char*>(e_ident), 16);

    // Check ELF magic
    if (e_ident[0] != 0x7f || e_ident[1] != 'E' ||
        e_ident[2] != 'L' || e_ident[3] != 'F') {
        return deps;
    }

    bool is_64bit = (e_ident[4] == 2);
    file.seekg(0);

    if (is_64bit) {
        Elf64_Ehdr ehdr;
        file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

        // Read section headers to find .dynamic
        file.seekg(ehdr.e_shoff);
        std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
        file.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * sizeof(Elf64_Shdr));

        // Find string table
        Elf64_Shdr& shstrtab = shdrs[ehdr.e_shstrndx];

        // Find .dynstr and .dynamic sections
        const Elf64_Shdr* dynstr = nullptr;
        const Elf64_Shdr* dynamic = nullptr;

        for (const auto& shdr : shdrs) {
            if (shdr.sh_type == SHT_STRTAB) {
                // Could be .dynstr - check name
                if (!dynstr) dynstr = &shdr;
            }
            if (shdr.sh_type == SHT_DYNAMIC) {
                dynamic = &shdr;
            }
        }

        if (dynamic && dynstr) {
            // Read dynamic entries
            file.seekg(dynamic->sh_offset);
            std::size_t num_entries = dynamic->sh_size / sizeof(Elf64_Dyn);

            for (std::size_t i = 0; i < num_entries; ++i) {
                Elf64_Dyn dyn;
                file.read(reinterpret_cast<char*>(&dyn), sizeof(dyn));

                if (dyn.d_tag == DT_NULL) break;
                if (dyn.d_tag == DT_NEEDED) {
                    // Read library name from dynstr
                    auto pos = file.tellg();
                    file.seekg(dynstr->sh_offset + dyn.d_un.d_val);

                    char name[256];
                    file.getline(name, sizeof(name), '\0');
                    deps.push_back(name);

                    file.seekg(pos);
                }
            }
        }
    }
#endif

    return deps;
}

std::vector<std::string> ModuleLoader::get_missing_dependencies(const std::filesystem::path& path) const {
    std::vector<std::string> missing;

    auto deps = get_dependencies(path);
    for (const auto& dep : deps) {
        auto resolved = registry_.resolve_path(dep);
        if (resolved.empty()) {
            missing.push_back(dep);
        }
    }

    return missing;
}

} // namespace void_cpp
