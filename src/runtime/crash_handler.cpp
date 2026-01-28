/// @file crash_handler.cpp
/// @brief Crash handling implementation for void_runtime

#include "crash_handler.hpp"
#include "runtime_legacy.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <csignal>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <cxxabi.h>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace void_runtime {

// =============================================================================
// Platform-Specific Data
// =============================================================================

#ifdef _WIN32
struct CrashHandler::PlatformData {
    LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;
    void (*previous_signal_handler)(int) = nullptr;
};
#else
struct CrashHandler::PlatformData {
    struct sigaction previous_sigabrt;
    struct sigaction previous_sigsegv;
    struct sigaction previous_sigfpe;
    struct sigaction previous_sigbus;
    struct sigaction previous_sigill;
};
#endif

CrashHandler* CrashHandler::instance_ = nullptr;

// =============================================================================
// CrashHandler Implementation
// =============================================================================

CrashHandler::CrashHandler() : platform_data_(std::make_unique<PlatformData>()) {
    instance_ = this;

    // Default dump directory
    dump_directory_ = std::filesystem::temp_directory_path() / "void_engine_crashes";
}

CrashHandler::~CrashHandler() {
    uninstall();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

bool CrashHandler::install() {
    if (installed_) {
        return true;
    }

    // Create dump directory
    std::filesystem::create_directories(dump_directory_);

#ifdef _WIN32
    // Install Windows SEH handler
    platform_data_->previous_filter = SetUnhandledExceptionFilter(
        [](EXCEPTION_POINTERS* info) -> LONG {
            if (instance_) {
                CrashType type = CrashType::Unknown;

                switch (info->ExceptionRecord->ExceptionCode) {
                    case EXCEPTION_ACCESS_VIOLATION:
                        type = CrashType::AccessViolation;
                        break;
                    case EXCEPTION_STACK_OVERFLOW:
                        type = CrashType::StackOverflow;
                        break;
                    case EXCEPTION_INT_DIVIDE_BY_ZERO:
                    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                        type = CrashType::DivisionByZero;
                        break;
                    case EXCEPTION_ILLEGAL_INSTRUCTION:
                        type = CrashType::IllegalInstruction;
                        break;
                }

                instance_->process_crash(type, "Unhandled exception", info);
            }
            return EXCEPTION_EXECUTE_HANDLER;
        });

    // Install signal handlers
    signal(SIGABRT, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGSEGV, signal_handler);

#else
    // Unix signal handlers
    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;

    sigaction(SIGABRT, &sa, &platform_data_->previous_sigabrt);
    sigaction(SIGSEGV, &sa, &platform_data_->previous_sigsegv);
    sigaction(SIGFPE, &sa, &platform_data_->previous_sigfpe);
    sigaction(SIGBUS, &sa, &platform_data_->previous_sigbus);
    sigaction(SIGILL, &sa, &platform_data_->previous_sigill);
#endif

    installed_ = true;
    return true;
}

void CrashHandler::uninstall() {
    if (!installed_) {
        return;
    }

#ifdef _WIN32
    if (platform_data_->previous_filter) {
        SetUnhandledExceptionFilter(platform_data_->previous_filter);
    }
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
#else
    sigaction(SIGABRT, &platform_data_->previous_sigabrt, nullptr);
    sigaction(SIGSEGV, &platform_data_->previous_sigsegv, nullptr);
    sigaction(SIGFPE, &platform_data_->previous_sigfpe, nullptr);
    sigaction(SIGBUS, &platform_data_->previous_sigbus, nullptr);
    sigaction(SIGILL, &platform_data_->previous_sigill, nullptr);
#endif

    installed_ = false;
}

void CrashHandler::set_dump_directory(const std::filesystem::path& path) {
    dump_directory_ = path;
    std::filesystem::create_directories(dump_directory_);
}

void CrashHandler::add_custom_data(const std::string& key, const std::string& value) {
    custom_data_[key] = value;
}

void CrashHandler::remove_custom_data(const std::string& key) {
    custom_data_.erase(key);
}

void CrashHandler::clear_custom_data() {
    custom_data_.clear();
}

void CrashHandler::set_pre_crash_callback(PreCrashCallback callback) {
    pre_crash_callback_ = std::move(callback);
}

void CrashHandler::set_crash_callback(CrashCallback callback) {
    crash_callback_ = std::move(callback);
}

void CrashHandler::set_report_callback(CrashReportCallback callback) {
    report_callback_ = std::move(callback);
}

CrashReport CrashHandler::generate_report(const std::string& message) {
    return process_crash(CrashType::Exception, message);
}

CrashReport CrashHandler::report_exception(const std::exception& e) {
    return process_crash(CrashType::Exception, e.what());
}

[[noreturn]] void CrashHandler::fatal_error(const std::string& message) {
    process_crash(CrashType::Abort, message);
    std::abort();
}

std::vector<StackFrame> CrashHandler::capture_stack_trace() const {
    std::vector<StackFrame> frames;

#ifdef _WIN32
    void* stack[64];
    WORD count = CaptureStackBackTrace(0, 64, stack, nullptr);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    SYMBOL_INFO* symbol = static_cast<SYMBOL_INFO*>(
        calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1));
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);

    for (WORD i = 0; i < count && i < max_stack_depth_; ++i) {
        StackFrame frame;
        frame.address = reinterpret_cast<std::uint64_t>(stack[i]);

        if (SymFromAddr(process, frame.address, nullptr, symbol)) {
            frame.function_name = symbol->Name;
            frame.offset = frame.address - symbol->Address;
        }

        DWORD displacement = 0;
        if (SymGetLineFromAddr64(process, frame.address, &displacement, &line)) {
            frame.file_name = line.FileName;
            frame.line_number = line.LineNumber;
        }

        HMODULE module = nullptr;
        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              reinterpret_cast<LPCTSTR>(stack[i]), &module)) {
            char module_name[MAX_PATH];
            if (GetModuleFileNameA(module, module_name, MAX_PATH)) {
                frame.module_name = module_name;
            }
        }

        frames.push_back(frame);
    }

    free(symbol);
    SymCleanup(process);

#else
    void* stack[64];
    int count = backtrace(stack, 64);
    char** symbols = backtrace_symbols(stack, count);

    for (int i = 0; i < count && i < static_cast<int>(max_stack_depth_); ++i) {
        StackFrame frame;
        frame.address = reinterpret_cast<std::uint64_t>(stack[i]);

        if (symbols && symbols[i]) {
            std::string symbol_str(symbols[i]);

            // Try to demangle C++ symbol
            std::size_t start = symbol_str.find('(');
            std::size_t end = symbol_str.find('+', start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string mangled = symbol_str.substr(start + 1, end - start - 1);
                int status;
                char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
                if (status == 0 && demangled) {
                    frame.function_name = demangled;
                    free(demangled);
                } else {
                    frame.function_name = mangled;
                }
            }

            frame.module_name = symbol_str.substr(0, symbol_str.find('('));
        }

        frames.push_back(frame);
    }

    if (symbols) {
        free(symbols);
    }
#endif

    return frames;
}

std::string CrashHandler::format_stack_trace(const std::vector<StackFrame>& frames) {
    std::ostringstream ss;

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& frame = frames[i];

        ss << "#" << i << "  ";
        ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << frame.address;
        ss << std::dec;

        if (!frame.function_name.empty()) {
            ss << " in " << frame.function_name;
            if (frame.offset > 0) {
                ss << "+0x" << std::hex << frame.offset << std::dec;
            }
        }

        if (!frame.file_name.empty()) {
            ss << " at " << frame.file_name;
            if (frame.line_number > 0) {
                ss << ":" << frame.line_number;
            }
        }

        if (!frame.module_name.empty()) {
            ss << " [" << frame.module_name << "]";
        }

        ss << "\n";
    }

    return ss.str();
}

std::vector<std::filesystem::path> CrashHandler::previous_crash_reports() const {
    std::vector<std::filesystem::path> reports;

    if (!std::filesystem::exists(dump_directory_)) {
        return reports;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dump_directory_)) {
        if (entry.path().extension() == ".txt" || entry.path().extension() == ".crash") {
            reports.push_back(entry.path());
        }
    }

    // Sort by modification time (newest first)
    std::sort(reports.begin(), reports.end(),
        [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        });

    return reports;
}

CrashReport CrashHandler::load_report(const std::filesystem::path& path) {
    CrashReport report;
    report.report_file = path;

    std::ifstream file(path);
    if (!file) {
        return report;
    }

    // Simple parsing
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Type: ") == 0) {
            report.info.message = line.substr(6);
        } else if (line.find("Message: ") == 0) {
            report.info.message = line.substr(9);
        }
    }

    return report;
}

void CrashHandler::cleanup_old_reports(int days) {
    if (!std::filesystem::exists(dump_directory_)) {
        return;
    }

    auto cutoff = std::filesystem::file_time_type::clock::now() -
                  std::chrono::hours(24 * days);

    for (const auto& entry : std::filesystem::directory_iterator(dump_directory_)) {
        if (entry.last_write_time() < cutoff) {
            std::filesystem::remove(entry.path());
        }
    }
}

void CrashHandler::assert_failed(const char* expression, const char* file, int line,
                                  const char* message) {
    std::ostringstream ss;
    ss << "Assertion failed: " << expression << "\n";
    ss << "  File: " << file << ":" << line << "\n";
    if (message) {
        ss << "  Message: " << message << "\n";
    }

    std::cerr << ss.str();

    if (instance_) {
        instance_->process_crash(CrashType::Assertion, ss.str());
    }

    std::abort();
}

void CrashHandler::set_auto_restart(bool enable, const std::string& restart_args) {
    auto_restart_ = enable;
    restart_args_ = restart_args;
}

bool CrashHandler::was_restarted_after_crash() {
    // Check for marker file
    auto marker = std::filesystem::temp_directory_path() / "void_engine_crash_marker";
    if (std::filesystem::exists(marker)) {
        std::filesystem::remove(marker);
        return true;
    }
    return false;
}

CrashInfo CrashHandler::get_previous_crash_info() {
    auto reports = instance_ ? instance_->previous_crash_reports()
                             : std::vector<std::filesystem::path>{};

    if (!reports.empty()) {
        return load_report(reports[0]).info;
    }
    return CrashInfo{};
}

void CrashHandler::signal_handler(int signal) {
    CrashType type = CrashType::Signal;
    std::string message;

    switch (signal) {
        case SIGABRT:
            type = CrashType::Abort;
            message = "SIGABRT: Abnormal termination";
            break;
        case SIGFPE:
            type = CrashType::DivisionByZero;
            message = "SIGFPE: Floating-point exception";
            break;
        case SIGSEGV:
            type = CrashType::AccessViolation;
            message = "SIGSEGV: Segmentation fault";
            break;
#ifndef _WIN32
        case SIGBUS:
            type = CrashType::AccessViolation;
            message = "SIGBUS: Bus error";
            break;
        case SIGILL:
            type = CrashType::IllegalInstruction;
            message = "SIGILL: Illegal instruction";
            break;
#endif
        default:
            message = "Signal " + std::to_string(signal);
            break;
    }

    if (instance_) {
        instance_->process_crash(type, message);
    }

    // Re-raise signal for default handling
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

CrashInfo CrashHandler::gather_crash_info(CrashType type, const std::string& message,
                                           void* exception_info) {
    CrashInfo info;
    info.type = type;
    info.message = message;
    info.timestamp = std::chrono::system_clock::now();

    // Get thread info
#ifdef _WIN32
    info.thread_id = GetCurrentThreadId();
#else
    info.thread_id = static_cast<std::uint64_t>(pthread_self());
#endif

    // Get OS info
#ifdef _WIN32
    OSVERSIONINFOW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    info.os_version = "Windows";
#else
    info.os_version = "Unix";
#endif

    // Get app info
    info.app_version = app_version_;
    info.build_info = __DATE__ " " __TIME__;

    // Get runtime stats if available
    if (auto* app = Application::instance_ptr()) {
        info.uptime_seconds = app->time_since_start();
        info.frame_count = app->frame_count();
    }

    // Capture stack trace
    if (capture_stack_trace_) {
        info.stack_trace = capture_stack_trace();
    }

    // Add custom data
    info.custom_data = custom_data_;

#ifdef _WIN32
    // Get exception-specific info
    if (exception_info) {
        auto* info_ptr = static_cast<EXCEPTION_POINTERS*>(exception_info);
        info.exc_code = info_ptr->ExceptionRecord->ExceptionCode;
        info.fault_address = reinterpret_cast<std::uint64_t>(
            info_ptr->ExceptionRecord->ExceptionAddress);
    }
#endif

    return info;
}

void CrashHandler::write_dump(const CrashInfo& info, const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = nullptr;  // Would use actual exception info
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            file,
            MiniDumpNormal,
            nullptr,
            nullptr,
            nullptr);

        CloseHandle(file);
    }
#endif
}

void CrashHandler::write_log(const CrashInfo& info, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) {
        return;
    }

    // Write header
    file << "===========================================\n";
    file << "Void Engine Crash Report\n";
    file << "===========================================\n\n";

    // Write timestamp
    auto time_t = std::chrono::system_clock::to_time_t(info.timestamp);
    file << "Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";

    // Write crash info
    file << "Type: ";
    switch (info.type) {
        case CrashType::AccessViolation: file << "Access Violation"; break;
        case CrashType::StackOverflow: file << "Stack Overflow"; break;
        case CrashType::DivisionByZero: file << "Division By Zero"; break;
        case CrashType::IllegalInstruction: file << "Illegal Instruction"; break;
        case CrashType::Assertion: file << "Assertion Failed"; break;
        case CrashType::Exception: file << "Exception"; break;
        case CrashType::OutOfMemory: file << "Out Of Memory"; break;
        case CrashType::Abort: file << "Abort"; break;
        case CrashType::Signal: file << "Signal"; break;
        default: file << "Unknown"; break;
    }
    file << "\n";

    file << "Message: " << info.message << "\n\n";

    // Write system info
    file << "System Information:\n";
    file << "  OS: " << info.os_version << "\n";
    file << "  App Version: " << info.app_version << "\n";
    file << "  Build: " << info.build_info << "\n";
    file << "  Thread ID: " << info.thread_id << "\n";
    if (!info.thread_name.empty()) {
        file << "  Thread Name: " << info.thread_name << "\n";
    }
    file << "\n";

    // Write runtime info
    file << "Runtime Information:\n";
    file << "  Uptime: " << std::fixed << std::setprecision(2) << info.uptime_seconds << " seconds\n";
    file << "  Frame Count: " << info.frame_count << "\n";
    file << "\n";

    // Write stack trace
    if (!info.stack_trace.empty()) {
        file << "Stack Trace:\n";
        file << format_stack_trace(info.stack_trace);
        file << "\n";
    }

    // Write custom data
    if (!info.custom_data.empty()) {
        file << "Custom Data:\n";
        for (const auto& [key, value] : info.custom_data) {
            file << "  " << key << ": " << value << "\n";
        }
        file << "\n";
    }

    file << "===========================================\n";
}

CrashReport CrashHandler::process_crash(CrashType type, const std::string& message,
                                         void* exception_info) {
    CrashReport report;

    // Call pre-crash callback
    if (pre_crash_callback_) {
        pre_crash_callback_();
    }

    // Gather crash info
    report.info = gather_crash_info(type, message, exception_info);

    // Generate unique filename
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream filename;
    filename << "crash_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

    // Write minidump
    if (generate_dump_) {
        report.dump_file = dump_directory_ / (filename.str() + ".dmp");
        write_dump(report.info, report.dump_file);
    }

    // Write log
    if (generate_log_) {
        report.report_file = dump_directory_ / (filename.str() + ".txt");
        write_log(report.info, report.report_file);
    }

    // Call crash callback
    if (crash_callback_) {
        crash_callback_(report.info);
    }

    // Call report callback
    if (report_callback_) {
        report.successfully_reported = true;
        report_callback_(report);
    }

    // Handle auto-restart
    if (auto_restart_) {
        // Write marker file
        auto marker = std::filesystem::temp_directory_path() / "void_engine_crash_marker";
        std::ofstream marker_file(marker);
        marker_file << "crashed\n";
        marker_file.close();

        // Restart application
#ifdef _WIN32
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);

        std::string cmd = std::string(path) + " " + restart_args_;
        CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0,
                       nullptr, nullptr, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
#else
        char* argv[] = {nullptr, nullptr};
        execv("/proc/self/exe", argv);
#endif
    }

    return report;
}

} // namespace void_runtime
