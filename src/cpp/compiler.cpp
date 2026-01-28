/// @file compiler.cpp
/// @brief C++ compiler abstraction implementation

#include "compiler.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace void_cpp {

// =============================================================================
// Process Execution Helper
// =============================================================================

namespace {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
    std::string stderr_output;
};

ProcessResult execute_process(const std::string& command, const std::vector<std::string>& args) {
    ProcessResult result;

#ifdef _WIN32
    // Build command line
    std::string cmdline = command;
    for (const auto& arg : args) {
        cmdline += " ";
        // Quote argument if it contains spaces
        if (arg.find(' ') != std::string::npos) {
            cmdline += "\"" + arg + "\"";
        } else {
            cmdline += arg;
        }
    }

    // Create pipes for stdout/stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;

    CreatePipe(&stdout_read, &stdout_write, &sa, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(&stderr_read, &stderr_write, &sa, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // Setup process
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    std::vector<char> cmdline_buf(cmdline.begin(), cmdline.end());
    cmdline_buf.push_back('\0');

    if (CreateProcessA(
            nullptr,
            cmdline_buf.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {

        CloseHandle(stdout_write);
        CloseHandle(stderr_write);

        // Read output
        std::array<char, 4096> buffer;
        DWORD bytes_read;

        while (ReadFile(stdout_read, buffer.data(), buffer.size() - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result.stdout_output += buffer.data();
        }

        while (ReadFile(stderr_read, buffer.data(), buffer.size() - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result.stderr_output += buffer.data();
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        result.exit_code = static_cast<int>(exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
    } else {
        CloseHandle(stdout_write);
        CloseHandle(stderr_write);
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        result.exit_code = -1;
    }

#else
    // Unix implementation
    int stdout_pipe[2];
    int stderr_pipe[2];

    pipe(stdout_pipe);
    pipe(stderr_pipe);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(command.c_str(), argv.data());
        _exit(127);
    } else if (pid > 0) {
        // Parent process
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::array<char, 4096> buffer;
        ssize_t n;

        while ((n = read(stdout_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
            buffer[n] = '\0';
            result.stdout_output += buffer.data();
        }

        while ((n = read(stderr_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
            buffer[n] = '\0';
            result.stderr_output += buffer.data();
        }

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        }
    }
#endif

    return result;
}

} // anonymous namespace

// =============================================================================
// CompileJob Implementation
// =============================================================================

CompileJob::CompileJob(CompileJobId id, std::vector<std::filesystem::path> sources, std::string output_name)
    : id_(id)
    , sources_(std::move(sources))
    , output_name_(std::move(output_name))
    , completion_future_(completion_promise_.get_future()) {}

bool CompileJob::is_complete() const {
    auto s = status_.load();
    return s == CompileStatus::Success ||
           s == CompileStatus::Warning ||
           s == CompileStatus::Error ||
           s == CompileStatus::Cancelled;
}

void CompileJob::wait() {
    completion_future_.wait();
}

bool CompileJob::wait_for(std::chrono::milliseconds timeout) {
    return completion_future_.wait_for(timeout) == std::future_status::ready;
}

void CompileJob::cancel() {
    cancelled_ = true;
}

// =============================================================================
// MSVCCompiler Implementation
// =============================================================================

MSVCCompiler::MSVCCompiler() {
    detect_compiler();
}

MSVCCompiler::MSVCCompiler(const std::filesystem::path& compiler_path)
    : cl_path_(compiler_path) {
    if (std::filesystem::exists(cl_path_)) {
        available_ = true;
        // Get version
        auto result = execute_process(cl_path_.string(), {});
        // Parse version from output
        std::regex version_regex(R"(Version (\d+\.\d+\.\d+))");
        std::smatch match;
        if (std::regex_search(result.stderr_output, match, version_regex)) {
            version_ = match[1].str();
        }
    }
}

void MSVCCompiler::detect_compiler() {
    // Try to find cl.exe in PATH
#ifdef _WIN32
    const char* path = std::getenv("PATH");
    if (path) {
        std::string path_str(path);
        std::string::size_type start = 0;
        std::string::size_type end;
        while ((end = path_str.find(';', start)) != std::string::npos) {
            std::filesystem::path dir(path_str.substr(start, end - start));
            auto cl = dir / "cl.exe";
            if (std::filesystem::exists(cl)) {
                cl_path_ = cl;
                link_path_ = dir / "link.exe";
                available_ = true;
                break;
            }
            start = end + 1;
        }
    }

    if (available_) {
        detect_from_vcvars();
    }
#endif
}

void MSVCCompiler::detect_from_vcvars() {
#ifdef _WIN32
    // Get include/lib dirs from environment
    const char* include = std::getenv("INCLUDE");
    if (include) {
        std::string include_str(include);
        std::string::size_type start = 0;
        std::string::size_type end;
        while ((end = include_str.find(';', start)) != std::string::npos) {
            std::filesystem::path dir(include_str.substr(start, end - start));
            if (std::filesystem::exists(dir)) {
                include_dirs_.push_back(dir);
            }
            start = end + 1;
        }
    }

    const char* lib = std::getenv("LIB");
    if (lib) {
        std::string lib_str(lib);
        std::string::size_type start = 0;
        std::string::size_type end;
        while ((end = lib_str.find(';', start)) != std::string::npos) {
            std::filesystem::path dir(lib_str.substr(start, end - start));
            if (std::filesystem::exists(dir)) {
                lib_dirs_.push_back(dir);
            }
            start = end + 1;
        }
    }
#endif
}

void MSVCCompiler::detect_from_registry() {
#ifdef _WIN32
    // Try to find Visual Studio through vswhere.exe (VS 2017+)
    // vswhere is installed with VS and provides JSON output for installed VS versions

    // Common vswhere locations
    std::vector<std::filesystem::path> vswhere_paths = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe"
    };

    std::filesystem::path vswhere_path;
    for (const auto& path : vswhere_paths) {
        if (std::filesystem::exists(path)) {
            vswhere_path = path;
            break;
        }
    }

    if (!vswhere_path.empty()) {
        // Run vswhere to find latest VS installation
        std::vector<std::string> args = {
            "-latest", "-products", "*",
            "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property", "installationPath"
        };
        auto result = execute_process(vswhere_path.string(), args);

        if (result.exit_code == 0 && !result.stdout_output.empty()) {
            // Extract installation path
            std::string install_path = result.stdout_output;
            // Remove trailing whitespace/newlines
            while (!install_path.empty() &&
                   (install_path.back() == '\n' || install_path.back() == '\r' ||
                    install_path.back() == ' ')) {
                install_path.pop_back();
            }

            if (!install_path.empty() && std::filesystem::exists(install_path)) {
                std::filesystem::path vc_tools = install_path;
                vc_tools /= "VC\\Tools\\MSVC";

                // Find latest version in VC/Tools/MSVC
                if (std::filesystem::exists(vc_tools)) {
                    std::filesystem::path latest_version;
                    for (const auto& entry : std::filesystem::directory_iterator(vc_tools)) {
                        if (entry.is_directory()) {
                            if (latest_version.empty() || entry.path() > latest_version) {
                                latest_version = entry.path();
                            }
                        }
                    }

                    if (!latest_version.empty()) {
                        // Set cl.exe path
                        std::filesystem::path cl = latest_version / "bin\\Hostx64\\x64\\cl.exe";
                        if (std::filesystem::exists(cl)) {
                            cl_path_ = cl;
                            link_path_ = latest_version / "bin\\Hostx64\\x64\\link.exe";
                            include_dirs_.push_back(latest_version / "include");
                            lib_dirs_.push_back(latest_version / "lib\\x64");
                            VOID_LOG_INFO("[MSVCCompiler] Found VS via vswhere: {}", cl.string());
                        }
                    }
                }
            }
        }
    }

    // Fallback: try legacy registry paths for older VS versions
    if (cl_path_.empty()) {
        HKEY hKey;
        // Try VS 2019/2017 registry key
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {

            char value[MAX_PATH];
            DWORD size = sizeof(value);
            DWORD type;

            // Try to find VS 2019 (16.0) or 2017 (15.0)
            const char* versions[] = {"16.0", "15.0", "14.0"};
            for (const char* ver : versions) {
                if (RegQueryValueExA(hKey, ver, nullptr, &type,
                    reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {

                    std::filesystem::path vs_path = value;
                    std::filesystem::path cl = vs_path / "VC\\bin\\amd64\\cl.exe";
                    if (std::filesystem::exists(cl)) {
                        cl_path_ = cl;
                        link_path_ = vs_path / "VC\\bin\\amd64\\link.exe";
                        include_dirs_.push_back(vs_path / "VC\\include");
                        lib_dirs_.push_back(vs_path / "VC\\lib\\amd64");
                        VOID_LOG_INFO("[MSVCCompiler] Found VS via registry: {}", cl.string());
                        break;
                    }
                }
                size = sizeof(value);
            }
            RegCloseKey(hKey);
        }
    }
#endif
}

CppResult<CompileResult> MSVCCompiler::compile(
    const std::vector<std::filesystem::path>& sources,
    const std::filesystem::path& output,
    const CompilerConfig& config) {

    CompileResult result;
    auto start_time = std::chrono::steady_clock::now();

    // Create intermediate directory
    std::filesystem::create_directories(config.intermediate_dir);

    // Compile each source file
    std::vector<std::filesystem::path> objects;
    for (const auto& source : sources) {
        auto obj_path = config.intermediate_dir / (source.stem().string() + ".obj");
        auto cmd = build_compile_command(source, obj_path, config);

        auto proc_result = execute_process(cl_path_.string(), cmd);
        auto diagnostics = parse_output(proc_result.stdout_output + proc_result.stderr_output);
        result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());

        if (proc_result.exit_code != 0) {
            result.status = CompileStatus::Error;
            result.error_count++;
            return std::move(result);
        }

        objects.push_back(obj_path);
    }

    auto compile_end = std::chrono::steady_clock::now();
    result.compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - start_time);

    // Link
    auto link_cmd = build_link_command(objects, output, config);
    auto link_result = execute_process(link_path_.string(), link_cmd);
    auto link_diagnostics = parse_output(link_result.stdout_output + link_result.stderr_output);
    result.diagnostics.insert(result.diagnostics.end(), link_diagnostics.begin(), link_diagnostics.end());

    auto end_time = std::chrono::steady_clock::now();
    result.link_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - compile_end);

    if (link_result.exit_code != 0) {
        result.status = CompileStatus::Error;
        return std::move(result);
    }

    result.status = result.warning_count > 0 ? CompileStatus::Warning : CompileStatus::Success;
    result.output_path = output;

    // Count errors and warnings
    for (const auto& d : result.diagnostics) {
        if (d.severity == DiagnosticSeverity::Error || d.severity == DiagnosticSeverity::Fatal) {
            result.error_count++;
        } else if (d.severity == DiagnosticSeverity::Warning) {
            result.warning_count++;
        }
    }

    return std::move(result);
}

std::vector<std::string> MSVCCompiler::build_compile_command(
    const std::filesystem::path& source,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    // Compile only
    cmd.push_back("/c");

    // Output
    cmd.push_back("/Fo" + output.string());

    // C++ standard
    switch (config.standard) {
        case CppStandard::Cpp17: cmd.push_back("/std:c++17"); break;
        case CppStandard::Cpp20: cmd.push_back("/std:c++20"); break;
        case CppStandard::Cpp23: cmd.push_back("/std:c++latest"); break;
    }

    // Optimization
    switch (config.optimization) {
        case OptimizationLevel::O0: cmd.push_back("/Od"); break;
        case OptimizationLevel::O1: cmd.push_back("/O1"); break;
        case OptimizationLevel::O2: cmd.push_back("/O2"); break;
        case OptimizationLevel::O3: cmd.push_back("/Ox"); break;
        case OptimizationLevel::Os: cmd.push_back("/Os"); break;
        case OptimizationLevel::Oz: cmd.push_back("/Os"); break;
        default: break;
    }

    // Debug info
    if (config.debug_info) {
        cmd.push_back("/Zi");
    }

    // Warnings
    switch (config.warnings) {
        case WarningLevel::Off: cmd.push_back("/w"); break;
        case WarningLevel::Low: cmd.push_back("/W1"); break;
        case WarningLevel::Default: cmd.push_back("/W3"); break;
        case WarningLevel::High: cmd.push_back("/W4"); break;
        case WarningLevel::All: cmd.push_back("/Wall"); break;
        case WarningLevel::Error: cmd.push_back("/WX"); break;
        default: break;
    }

    // RTTI
    if (!config.rtti) {
        cmd.push_back("/GR-");
    }

    // Exceptions
    if (config.exceptions) {
        cmd.push_back("/EHsc");
    } else {
        cmd.push_back("/EHs-c-");
    }

    // Defines
    for (const auto& def : config.defines) {
        cmd.push_back("/D" + def);
    }

    // Include paths
    for (const auto& inc : config.include_paths) {
        cmd.push_back("/I" + inc.string());
    }

    // Additional flags
    for (const auto& flag : config.compiler_flags) {
        cmd.push_back(flag);
    }

    // Source file
    cmd.push_back(source.string());

    return cmd;
}

std::vector<std::string> MSVCCompiler::build_link_command(
    const std::vector<std::filesystem::path>& objects,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    // Output
    cmd.push_back("/OUT:" + output.string());

    // Output type
    switch (config.output_type) {
        case OutputType::SharedLibrary:
            cmd.push_back("/DLL");
            break;
        case OutputType::Executable:
            // Default
            break;
        default:
            break;
    }

    // Debug
    if (config.debug_info) {
        cmd.push_back("/DEBUG");
        if (config.generate_pdb) {
            auto pdb = output;
            pdb.replace_extension(".pdb");
            cmd.push_back("/PDB:" + pdb.string());
        }
    }

    // Incremental
    if (config.incremental_link) {
        cmd.push_back("/INCREMENTAL");
    } else {
        cmd.push_back("/INCREMENTAL:NO");
    }

    // Library paths
    for (const auto& lib_path : config.library_paths) {
        cmd.push_back("/LIBPATH:" + lib_path.string());
    }

    // Libraries
    for (const auto& lib : config.libraries) {
        cmd.push_back(lib);
    }

    // Additional flags
    for (const auto& flag : config.linker_flags) {
        cmd.push_back(flag);
    }

    // Object files
    for (const auto& obj : objects) {
        cmd.push_back(obj.string());
    }

    return cmd;
}

bool MSVCCompiler::supports_standard(CppStandard std) const {
    // MSVC 19.14+ supports C++17
    // MSVC 19.29+ supports C++20
    return true;  // Simplified for now
}

std::vector<CompileDiagnostic> MSVCCompiler::parse_output(const std::string& output) const {
    std::vector<CompileDiagnostic> diagnostics;

    // MSVC format: file(line): error/warning code: message
    std::regex diag_regex(R"(([^(]+)\((\d+)\):\s*(error|warning|note)\s+(\w+):\s*(.+))");

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, diag_regex)) {
            CompileDiagnostic diag;
            diag.file = match[1].str();
            diag.line = std::stoul(match[2].str());
            diag.column = 0;

            std::string severity = match[3].str();
            if (severity == "error") {
                diag.severity = DiagnosticSeverity::Error;
            } else if (severity == "warning") {
                diag.severity = DiagnosticSeverity::Warning;
            } else {
                diag.severity = DiagnosticSeverity::Note;
            }

            diag.code = match[4].str();
            diag.message = match[5].str();

            diagnostics.push_back(std::move(diag));
        }
    }

    return diagnostics;
}

// =============================================================================
// ClangCompiler Implementation
// =============================================================================

ClangCompiler::ClangCompiler() {
    detect_compiler();
}

ClangCompiler::ClangCompiler(const std::filesystem::path& compiler_path)
    : clang_path_(compiler_path) {
    if (std::filesystem::exists(clang_path_)) {
        available_ = true;
        auto result = execute_process(clang_path_.string(), {"--version"});
        std::regex version_regex(R"(version (\d+\.\d+\.\d+))");
        std::smatch match;
        if (std::regex_search(result.stdout_output, match, version_regex)) {
            version_ = match[1].str();
        }
    }
}

void ClangCompiler::detect_compiler() {
    // Try common names
    std::vector<std::string> names = {"clang++", "clang++-15", "clang++-14", "clang++-13"};

    for (const auto& name : names) {
        auto result = execute_process("which", {name});
        if (result.exit_code == 0 && !result.stdout_output.empty()) {
            clang_path_ = result.stdout_output;
            // Trim newline
            while (!clang_path_.empty() &&
                   (clang_path_.native().back() == '\n' || clang_path_.native().back() == '\r')) {
                auto p = clang_path_.string();
                p.pop_back();
                clang_path_ = p;
            }
            available_ = true;
            break;
        }
    }

#ifdef _WIN32
    // Also check for clang-cl on Windows
    if (!available_) {
        const char* path = std::getenv("PATH");
        if (path) {
            std::string path_str(path);
            std::string::size_type start = 0;
            std::string::size_type end;
            while ((end = path_str.find(';', start)) != std::string::npos) {
                std::filesystem::path dir(path_str.substr(start, end - start));
                auto clang = dir / "clang++.exe";
                if (std::filesystem::exists(clang)) {
                    clang_path_ = clang;
                    available_ = true;
                    break;
                }
                start = end + 1;
            }
        }
    }
#endif
}

CppResult<CompileResult> ClangCompiler::compile(
    const std::vector<std::filesystem::path>& sources,
    const std::filesystem::path& output,
    const CompilerConfig& config) {

    CompileResult result;
    auto start_time = std::chrono::steady_clock::now();

    // Create intermediate directory
    std::filesystem::create_directories(config.intermediate_dir);

    // Compile each source file
    std::vector<std::filesystem::path> objects;
    for (const auto& source : sources) {
        auto obj_path = config.intermediate_dir / (source.stem().string() + ".o");
        auto cmd = build_compile_command(source, obj_path, config);

        auto proc_result = execute_process(clang_path_.string(), cmd);
        auto diagnostics = parse_output(proc_result.stderr_output);
        result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());

        if (proc_result.exit_code != 0) {
            result.status = CompileStatus::Error;
            result.error_count++;
            return std::move(result);
        }

        objects.push_back(obj_path);
    }

    auto compile_end = std::chrono::steady_clock::now();
    result.compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - start_time);

    // Link
    auto link_cmd = build_link_command(objects, output, config);
    auto link_result = execute_process(clang_path_.string(), link_cmd);
    auto link_diagnostics = parse_output(link_result.stderr_output);
    result.diagnostics.insert(result.diagnostics.end(), link_diagnostics.begin(), link_diagnostics.end());

    auto end_time = std::chrono::steady_clock::now();
    result.link_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - compile_end);

    if (link_result.exit_code != 0) {
        result.status = CompileStatus::Error;
        return std::move(result);
    }

    result.status = result.warning_count > 0 ? CompileStatus::Warning : CompileStatus::Success;
    result.output_path = output;

    // Count diagnostics
    for (const auto& d : result.diagnostics) {
        if (d.severity == DiagnosticSeverity::Error || d.severity == DiagnosticSeverity::Fatal) {
            result.error_count++;
        } else if (d.severity == DiagnosticSeverity::Warning) {
            result.warning_count++;
        }
    }

    return std::move(result);
}

std::vector<std::string> ClangCompiler::build_compile_command(
    const std::filesystem::path& source,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    // Compile only
    cmd.push_back("-c");

    // Output
    cmd.push_back("-o");
    cmd.push_back(output.string());

    // C++ standard
    switch (config.standard) {
        case CppStandard::Cpp17: cmd.push_back("-std=c++17"); break;
        case CppStandard::Cpp20: cmd.push_back("-std=c++20"); break;
        case CppStandard::Cpp23: cmd.push_back("-std=c++2b"); break;
    }

    // Optimization
    switch (config.optimization) {
        case OptimizationLevel::O0: cmd.push_back("-O0"); break;
        case OptimizationLevel::O1: cmd.push_back("-O1"); break;
        case OptimizationLevel::O2: cmd.push_back("-O2"); break;
        case OptimizationLevel::O3: cmd.push_back("-O3"); break;
        case OptimizationLevel::Os: cmd.push_back("-Os"); break;
        case OptimizationLevel::Oz: cmd.push_back("-Oz"); break;
        default: break;
    }

    // Debug info
    if (config.debug_info) {
        cmd.push_back("-g");
    }

    // Position independent code for shared libraries
    if (config.output_type == OutputType::SharedLibrary) {
        cmd.push_back("-fPIC");
    }

    // Warnings
    switch (config.warnings) {
        case WarningLevel::Off: cmd.push_back("-w"); break;
        case WarningLevel::Low: cmd.push_back("-W"); break;
        case WarningLevel::Default: cmd.push_back("-Wall"); break;
        case WarningLevel::High: cmd.push_back("-Wall"); cmd.push_back("-Wextra"); break;
        case WarningLevel::All: cmd.push_back("-Weverything"); break;
        case WarningLevel::Error: cmd.push_back("-Werror"); break;
        default: break;
    }

    // RTTI
    if (!config.rtti) {
        cmd.push_back("-fno-rtti");
    }

    // Exceptions
    if (!config.exceptions) {
        cmd.push_back("-fno-exceptions");
    }

    // Defines
    for (const auto& def : config.defines) {
        cmd.push_back("-D" + def);
    }

    // Include paths
    for (const auto& inc : config.include_paths) {
        cmd.push_back("-I" + inc.string());
    }

    // Additional flags
    for (const auto& flag : config.compiler_flags) {
        cmd.push_back(flag);
    }

    // Source file
    cmd.push_back(source.string());

    return cmd;
}

std::vector<std::string> ClangCompiler::build_link_command(
    const std::vector<std::filesystem::path>& objects,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    // Output
    cmd.push_back("-o");
    cmd.push_back(output.string());

    // Output type
    if (config.output_type == OutputType::SharedLibrary) {
        cmd.push_back("-shared");
    }

    // Library paths
    for (const auto& lib_path : config.library_paths) {
        cmd.push_back("-L" + lib_path.string());
    }

    // Libraries
    for (const auto& lib : config.libraries) {
        cmd.push_back("-l" + lib);
    }

    // Additional flags
    for (const auto& flag : config.linker_flags) {
        cmd.push_back(flag);
    }

    // Object files
    for (const auto& obj : objects) {
        cmd.push_back(obj.string());
    }

    return cmd;
}

bool ClangCompiler::supports_standard(CppStandard std) const {
    return true;  // Modern Clang supports all standards
}

std::vector<CompileDiagnostic> ClangCompiler::parse_output(const std::string& output) const {
    std::vector<CompileDiagnostic> diagnostics;

    // Clang format: file:line:column: error/warning: message
    std::regex diag_regex(R"(([^:]+):(\d+):(\d+):\s*(error|warning|note|fatal error):\s*(.+))");

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, diag_regex)) {
            CompileDiagnostic diag;
            diag.file = match[1].str();
            diag.line = std::stoul(match[2].str());
            diag.column = std::stoul(match[3].str());

            std::string severity = match[4].str();
            if (severity == "error") {
                diag.severity = DiagnosticSeverity::Error;
            } else if (severity == "fatal error") {
                diag.severity = DiagnosticSeverity::Fatal;
            } else if (severity == "warning") {
                diag.severity = DiagnosticSeverity::Warning;
            } else {
                diag.severity = DiagnosticSeverity::Note;
            }

            diag.message = match[5].str();
            diagnostics.push_back(std::move(diag));
        }
    }

    return diagnostics;
}

// =============================================================================
// GCCCompiler Implementation
// =============================================================================

GCCCompiler::GCCCompiler() {
    detect_compiler();
}

GCCCompiler::GCCCompiler(const std::filesystem::path& compiler_path)
    : gcc_path_(compiler_path) {
    if (std::filesystem::exists(gcc_path_)) {
        available_ = true;
        auto result = execute_process(gcc_path_.string(), {"--version"});
        std::regex version_regex(R"((\d+\.\d+\.\d+))");
        std::smatch match;
        if (std::regex_search(result.stdout_output, match, version_regex)) {
            version_ = match[1].str();
        }
    }
}

void GCCCompiler::detect_compiler() {
    std::vector<std::string> names = {"g++", "g++-13", "g++-12", "g++-11"};

    for (const auto& name : names) {
        auto result = execute_process("which", {name});
        if (result.exit_code == 0 && !result.stdout_output.empty()) {
            gcc_path_ = result.stdout_output;
            while (!gcc_path_.empty() &&
                   (gcc_path_.native().back() == '\n' || gcc_path_.native().back() == '\r')) {
                auto p = gcc_path_.string();
                p.pop_back();
                gcc_path_ = p;
            }
            available_ = true;
            break;
        }
    }
}

CppResult<CompileResult> GCCCompiler::compile(
    const std::vector<std::filesystem::path>& sources,
    const std::filesystem::path& output,
    const CompilerConfig& config) {

    // GCC compile is very similar to Clang
    CompileResult result;
    auto start_time = std::chrono::steady_clock::now();

    std::filesystem::create_directories(config.intermediate_dir);

    std::vector<std::filesystem::path> objects;
    for (const auto& source : sources) {
        auto obj_path = config.intermediate_dir / (source.stem().string() + ".o");
        auto cmd = build_compile_command(source, obj_path, config);

        auto proc_result = execute_process(gcc_path_.string(), cmd);
        auto diagnostics = parse_output(proc_result.stderr_output);
        result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());

        if (proc_result.exit_code != 0) {
            result.status = CompileStatus::Error;
            result.error_count++;
            return std::move(result);
        }

        objects.push_back(obj_path);
    }

    auto compile_end = std::chrono::steady_clock::now();
    result.compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - start_time);

    auto link_cmd = build_link_command(objects, output, config);
    auto link_result = execute_process(gcc_path_.string(), link_cmd);
    auto link_diagnostics = parse_output(link_result.stderr_output);
    result.diagnostics.insert(result.diagnostics.end(), link_diagnostics.begin(), link_diagnostics.end());

    auto end_time = std::chrono::steady_clock::now();
    result.link_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - compile_end);

    if (link_result.exit_code != 0) {
        result.status = CompileStatus::Error;
        return std::move(result);
    }

    result.status = result.warning_count > 0 ? CompileStatus::Warning : CompileStatus::Success;
    result.output_path = output;

    for (const auto& d : result.diagnostics) {
        if (d.severity == DiagnosticSeverity::Error || d.severity == DiagnosticSeverity::Fatal) {
            result.error_count++;
        } else if (d.severity == DiagnosticSeverity::Warning) {
            result.warning_count++;
        }
    }

    return std::move(result);
}

std::vector<std::string> GCCCompiler::build_compile_command(
    const std::filesystem::path& source,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    cmd.push_back("-c");
    cmd.push_back("-o");
    cmd.push_back(output.string());

    switch (config.standard) {
        case CppStandard::Cpp17: cmd.push_back("-std=c++17"); break;
        case CppStandard::Cpp20: cmd.push_back("-std=c++20"); break;
        case CppStandard::Cpp23: cmd.push_back("-std=c++23"); break;
    }

    switch (config.optimization) {
        case OptimizationLevel::O0: cmd.push_back("-O0"); break;
        case OptimizationLevel::O1: cmd.push_back("-O1"); break;
        case OptimizationLevel::O2: cmd.push_back("-O2"); break;
        case OptimizationLevel::O3: cmd.push_back("-O3"); break;
        case OptimizationLevel::Os: cmd.push_back("-Os"); break;
        case OptimizationLevel::Oz: cmd.push_back("-Os"); break;
        default: break;
    }

    if (config.debug_info) {
        cmd.push_back("-g");
    }

    if (config.output_type == OutputType::SharedLibrary) {
        cmd.push_back("-fPIC");
    }

    switch (config.warnings) {
        case WarningLevel::Off: cmd.push_back("-w"); break;
        case WarningLevel::Low: cmd.push_back("-W"); break;
        case WarningLevel::Default: cmd.push_back("-Wall"); break;
        case WarningLevel::High: cmd.push_back("-Wall"); cmd.push_back("-Wextra"); break;
        case WarningLevel::All: cmd.push_back("-Wall"); cmd.push_back("-Wextra"); cmd.push_back("-pedantic"); break;
        case WarningLevel::Error: cmd.push_back("-Werror"); break;
        default: break;
    }

    if (!config.rtti) {
        cmd.push_back("-fno-rtti");
    }

    if (!config.exceptions) {
        cmd.push_back("-fno-exceptions");
    }

    for (const auto& def : config.defines) {
        cmd.push_back("-D" + def);
    }

    for (const auto& inc : config.include_paths) {
        cmd.push_back("-I" + inc.string());
    }

    for (const auto& flag : config.compiler_flags) {
        cmd.push_back(flag);
    }

    cmd.push_back(source.string());

    return cmd;
}

std::vector<std::string> GCCCompiler::build_link_command(
    const std::vector<std::filesystem::path>& objects,
    const std::filesystem::path& output,
    const CompilerConfig& config) const {

    std::vector<std::string> cmd;

    cmd.push_back("-o");
    cmd.push_back(output.string());

    if (config.output_type == OutputType::SharedLibrary) {
        cmd.push_back("-shared");
    }

    for (const auto& lib_path : config.library_paths) {
        cmd.push_back("-L" + lib_path.string());
    }

    for (const auto& lib : config.libraries) {
        cmd.push_back("-l" + lib);
    }

    for (const auto& flag : config.linker_flags) {
        cmd.push_back(flag);
    }

    for (const auto& obj : objects) {
        cmd.push_back(obj.string());
    }

    return cmd;
}

bool GCCCompiler::supports_standard(CppStandard std) const {
    return true;
}

std::vector<CompileDiagnostic> GCCCompiler::parse_output(const std::string& output) const {
    // GCC output is same format as Clang
    std::vector<CompileDiagnostic> diagnostics;

    std::regex diag_regex(R"(([^:]+):(\d+):(\d+):\s*(error|warning|note):\s*(.+))");

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, diag_regex)) {
            CompileDiagnostic diag;
            diag.file = match[1].str();
            diag.line = std::stoul(match[2].str());
            diag.column = std::stoul(match[3].str());

            std::string severity = match[4].str();
            if (severity == "error") {
                diag.severity = DiagnosticSeverity::Error;
            } else if (severity == "warning") {
                diag.severity = DiagnosticSeverity::Warning;
            } else {
                diag.severity = DiagnosticSeverity::Note;
            }

            diag.message = match[5].str();
            diagnostics.push_back(std::move(diag));
        }
    }

    return diagnostics;
}

// =============================================================================
// CompileQueue Implementation
// =============================================================================

CompileQueue::CompileQueue(std::size_t num_workers) {
    if (num_workers == 0) {
        num_workers = std::max(1u, std::thread::hardware_concurrency());
    }

    for (std::size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&CompileQueue::worker_thread, this);
    }
}

CompileQueue::~CompileQueue() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
    }
    queue_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::shared_ptr<CompileJob> CompileQueue::submit(
    std::vector<std::filesystem::path> sources,
    std::string output_name,
    const CompilerConfig& config) {

    auto job = std::make_shared<CompileJob>(
        CompileJobId::create(next_job_id_++, 0),
        std::move(sources),
        std::move(output_name));

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(QueueEntry{job, config});
    }
    queue_cv_.notify_one();

    return job;
}

void CompileQueue::cancel(CompileJobId id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    // Mark job as cancelled if found
    std::queue<QueueEntry> new_queue;
    while (!queue_.empty()) {
        auto entry = std::move(queue_.front());
        queue_.pop();
        if (entry.job->id() == id) {
            entry.job->cancel();
        }
        new_queue.push(std::move(entry));
    }
    queue_ = std::move(new_queue);
}

void CompileQueue::cancel_all() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!queue_.empty()) {
        queue_.front().job->cancel();
        queue_.pop();
    }
}

std::size_t CompileQueue::pending_count() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
    return queue_.size();
}

std::size_t CompileQueue::active_count() const {
    return active_jobs_.load();
}

void CompileQueue::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] {
        return queue_.empty() && active_jobs_ == 0;
    });
}

void CompileQueue::set_compiler(std::unique_ptr<ICompiler> compiler) {
    compiler_ = std::move(compiler);
}

void CompileQueue::worker_thread() {
    while (true) {
        QueueEntry entry;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return shutdown_ || !queue_.empty();
            });

            if (shutdown_ && queue_.empty()) {
                return;
            }

            entry = std::move(queue_.front());
            queue_.pop();
        }

        if (entry.job->is_cancelled()) {
            entry.job->status_ = CompileStatus::Cancelled;
            entry.job->completion_promise_.set_value();
            continue;
        }

        active_jobs_++;
        entry.job->status_ = CompileStatus::Compiling;

        // Compile
        if (compiler_) {
            auto output_path = entry.config.output_dir / entry.job->output_name();
            auto result = compiler_->compile(entry.job->sources(), output_path, entry.config);

            if (result) {
                entry.job->result_ = std::move(result.value());
                entry.job->status_ = entry.job->result_.status;
            } else {
                entry.job->status_ = CompileStatus::Error;
            }
        } else {
            entry.job->status_ = CompileStatus::Error;
        }

        entry.job->completion_promise_.set_value();
        active_jobs_--;

        queue_cv_.notify_all();
    }
}

// =============================================================================
// Compiler Implementation
// =============================================================================

namespace {
    Compiler* g_compiler_instance = nullptr;
}

Compiler::Compiler() {
    g_compiler_instance = this;

    // Auto-detect best compiler
    auto best = detect_best_compiler();
    set_compiler(best);
}

Compiler::Compiler(const CompilerConfig& config)
    : config_(config) {
    g_compiler_instance = this;

    if (config.compiler == CompilerType::Auto) {
        set_compiler(detect_best_compiler());
    } else {
        set_compiler(config.compiler);
    }
}

Compiler::~Compiler() {
    if (queue_) {
        queue_->wait_all();
    }
    if (g_compiler_instance == this) {
        g_compiler_instance = nullptr;
    }
}

Compiler& Compiler::instance() {
    if (!g_compiler_instance) {
        static Compiler default_instance;
        g_compiler_instance = &default_instance;
    }
    return *g_compiler_instance;
}

void Compiler::set_config(const CompilerConfig& config) {
    config_ = config;
}

void Compiler::set_compiler(CompilerType type) {
    switch (type) {
        case CompilerType::MSVC:
            compiler_ = std::make_unique<MSVCCompiler>();
            break;
        case CompilerType::Clang:
        case CompilerType::ClangCL:
            compiler_ = std::make_unique<ClangCompiler>();
            break;
        case CompilerType::GCC:
            compiler_ = std::make_unique<GCCCompiler>();
            break;
        default:
            // Auto-detect
            compiler_ = std::make_unique<MSVCCompiler>();
            if (!compiler_->is_available()) {
                compiler_ = std::make_unique<ClangCompiler>();
            }
            if (!compiler_->is_available()) {
                compiler_ = std::make_unique<GCCCompiler>();
            }
            break;
    }

    if (queue_) {
        queue_->set_compiler(nullptr);  // Queue will use our compiler
    }
}

void Compiler::set_compiler(std::unique_ptr<ICompiler> compiler) {
    compiler_ = std::move(compiler);
}

std::vector<CompilerType> Compiler::available_compilers() const {
    std::vector<CompilerType> result;

    MSVCCompiler msvc;
    if (msvc.is_available()) result.push_back(CompilerType::MSVC);

    ClangCompiler clang;
    if (clang.is_available()) result.push_back(CompilerType::Clang);

    GCCCompiler gcc;
    if (gcc.is_available()) result.push_back(CompilerType::GCC);

    return result;
}

CompilerType Compiler::detect_best_compiler() const {
#ifdef _WIN32
    MSVCCompiler msvc;
    if (msvc.is_available()) return CompilerType::MSVC;
#endif

    ClangCompiler clang;
    if (clang.is_available()) return CompilerType::Clang;

    GCCCompiler gcc;
    if (gcc.is_available()) return CompilerType::GCC;

#ifdef _WIN32
    return CompilerType::MSVC;
#else
    return CompilerType::GCC;
#endif
}

CppResult<CompileResult> Compiler::compile(
    const std::vector<std::filesystem::path>& sources,
    const std::string& output_name) {

    if (!compiler_ || !compiler_->is_available()) {
        return CppError::CompilerNotFound;
    }

    auto output_path = get_output_path(output_name);
    return compiler_->compile(sources, output_path, config_);
}

CppResult<CompileResult> Compiler::compile(
    const std::filesystem::path& source,
    const std::string& output_name) {
    return compile(std::vector{source}, output_name);
}

std::shared_ptr<CompileJob> Compiler::compile_async(
    const std::vector<std::filesystem::path>& sources,
    const std::string& output_name) {

    if (!queue_) {
        queue_ = std::make_unique<CompileQueue>(config_.max_parallel_jobs);
        queue_->set_compiler(std::move(compiler_));
    }

    return queue_->submit(sources, output_name, config_);
}

void Compiler::cancel(CompileJobId id) {
    if (queue_) {
        queue_->cancel(id);
    }
}

void Compiler::wait_all() {
    if (queue_) {
        queue_->wait_all();
    }
}

bool Compiler::needs_recompile(
    const std::filesystem::path& source,
    const std::filesystem::path& object) const {

    if (!std::filesystem::exists(object)) {
        return true;
    }

    auto source_time = std::filesystem::last_write_time(source);
    auto object_time = std::filesystem::last_write_time(object);

    return source_time > object_time;
}

std::filesystem::path Compiler::get_object_path(const std::filesystem::path& source) const {
    auto obj_name = source.stem().string();
#ifdef _WIN32
    obj_name += ".obj";
#else
    obj_name += ".o";
#endif
    return config_.intermediate_dir / obj_name;
}

std::filesystem::path Compiler::get_output_path(const std::string& name) const {
    std::string output_name = name;

    switch (config_.output_type) {
        case OutputType::SharedLibrary:
#ifdef _WIN32
            output_name += ".dll";
#elif defined(__APPLE__)
            output_name = "lib" + output_name + ".dylib";
#else
            output_name = "lib" + output_name + ".so";
#endif
            break;
        case OutputType::StaticLibrary:
#ifdef _WIN32
            output_name += ".lib";
#else
            output_name = "lib" + output_name + ".a";
#endif
            break;
        case OutputType::Executable:
#ifdef _WIN32
            output_name += ".exe";
#endif
            break;
        case OutputType::Object:
#ifdef _WIN32
            output_name += ".obj";
#else
            output_name += ".o";
#endif
            break;
        default:
            break;
    }

    return config_.output_dir / output_name;
}

} // namespace void_cpp
