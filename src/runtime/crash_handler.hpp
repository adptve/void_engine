/// @file crash_handler.hpp
/// @brief Crash handling and reporting for void_runtime

#pragma once

#include "fwd.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_runtime {

// =============================================================================
// Crash Types
// =============================================================================

/// @brief Crash type enumeration
enum class CrashType {
    Unknown,
    AccessViolation,
    StackOverflow,
    DivisionByZero,
    IllegalInstruction,
    Assertion,
    Exception,
    OutOfMemory,
    Abort,
    Signal
};

/// @brief Stack frame information
struct StackFrame {
    std::uint64_t address = 0;
    std::string function_name;
    std::string file_name;
    int line_number = 0;
    std::string module_name;
    std::uint64_t offset = 0;
};

/// @brief Crash information
struct CrashInfo {
    CrashType type = CrashType::Unknown;
    std::string message;
    std::uint64_t exc_code = 0;
    std::uint64_t fault_address = 0;

    // Stack trace
    std::vector<StackFrame> stack_trace;

    // Context
    std::string thread_name;
    std::uint64_t thread_id = 0;

    // System info
    std::string os_version;
    std::string app_version;
    std::string build_info;

    // Timing
    std::chrono::system_clock::time_point timestamp;
    double uptime_seconds = 0.0;
    std::uint64_t frame_count = 0;

    // Memory state
    std::size_t memory_used = 0;
    std::size_t memory_available = 0;

    // Custom data
    std::unordered_map<std::string, std::string> custom_data;
};

/// @brief Crash report
struct CrashReport {
    CrashInfo info;
    std::filesystem::path dump_file;
    std::filesystem::path log_file;
    std::filesystem::path report_file;
    bool successfully_reported = false;
};

// =============================================================================
// Callbacks
// =============================================================================

using CrashCallback = std::function<void(const CrashInfo& info)>;
using CrashReportCallback = std::function<void(const CrashReport& report)>;
using PreCrashCallback = std::function<void()>;

// =============================================================================
// Crash Handler
// =============================================================================

/// @brief Crash handling and reporting system
class CrashHandler {
public:
    CrashHandler();
    ~CrashHandler();

    // Non-copyable
    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Install crash handlers
    bool install();

    /// @brief Uninstall crash handlers
    void uninstall();

    /// @brief Check if handlers are installed
    bool is_installed() const { return installed_; }

    // ==========================================================================
    // Configuration
    // ==========================================================================

    /// @brief Set crash dump output directory
    void set_dump_directory(const std::filesystem::path& path);

    /// @brief Get crash dump directory
    const std::filesystem::path& dump_directory() const { return dump_directory_; }

    /// @brief Enable/disable minidump generation
    void set_generate_dump(bool enable) { generate_dump_ = enable; }

    /// @brief Enable/disable crash log generation
    void set_generate_log(bool enable) { generate_log_ = enable; }

    /// @brief Enable/disable stack trace capture
    void set_capture_stack_trace(bool enable) { capture_stack_trace_ = enable; }

    /// @brief Set maximum stack trace depth
    void set_max_stack_depth(std::size_t depth) { max_stack_depth_ = depth; }

    /// @brief Set application version for reports
    void set_app_version(const std::string& version) { app_version_ = version; }

    /// @brief Set application name for reports
    void set_app_name(const std::string& name) { app_name_ = name; }

    // ==========================================================================
    // Custom Data
    // ==========================================================================

    /// @brief Add custom data to crash reports
    void add_custom_data(const std::string& key, const std::string& value);

    /// @brief Remove custom data
    void remove_custom_data(const std::string& key);

    /// @brief Clear all custom data
    void clear_custom_data();

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    /// @brief Set callback for when crash is detected (before processing)
    void set_pre_crash_callback(PreCrashCallback callback);

    /// @brief Set callback for when crash info is gathered
    void set_crash_callback(CrashCallback callback);

    /// @brief Set callback for when crash report is complete
    void set_report_callback(CrashReportCallback callback);

    // ==========================================================================
    // Manual Crash Reporting
    // ==========================================================================

    /// @brief Manually trigger a crash report
    CrashReport generate_report(const std::string& message);

    /// @brief Report an exception
    CrashReport report_exception(const std::exception& e);

    /// @brief Report a fatal error (then terminate)
    [[noreturn]] void fatal_error(const std::string& message);

    // ==========================================================================
    // Stack Trace
    // ==========================================================================

    /// @brief Capture current stack trace
    std::vector<StackFrame> capture_stack_trace() const;

    /// @brief Format stack trace as string
    static std::string format_stack_trace(const std::vector<StackFrame>& frames);

    // ==========================================================================
    // Previous Crashes
    // ==========================================================================

    /// @brief Get list of previous crash reports
    std::vector<std::filesystem::path> previous_crash_reports() const;

    /// @brief Load a crash report
    static CrashReport load_report(const std::filesystem::path& path);

    /// @brief Delete old crash reports (older than days)
    void cleanup_old_reports(int days);

    // ==========================================================================
    // Assertions
    // ==========================================================================

    /// @brief Custom assert handler
    static void assert_failed(const char* expression, const char* file, int line,
                              const char* message = nullptr);

    // ==========================================================================
    // Recovery
    // ==========================================================================

    /// @brief Enable auto-restart on crash
    void set_auto_restart(bool enable, const std::string& restart_args = "");

    /// @brief Check if app was restarted after crash
    static bool was_restarted_after_crash();

    /// @brief Get previous crash info (if restarted)
    static CrashInfo get_previous_crash_info();

private:
    bool installed_ = false;
    std::filesystem::path dump_directory_;
    std::string app_name_;
    std::string app_version_;

    bool generate_dump_ = true;
    bool generate_log_ = true;
    bool capture_stack_trace_ = true;
    std::size_t max_stack_depth_ = 64;

    std::unordered_map<std::string, std::string> custom_data_;

    PreCrashCallback pre_crash_callback_;
    CrashCallback crash_callback_;
    CrashReportCallback report_callback_;

    bool auto_restart_ = false;
    std::string restart_args_;

    // Platform-specific implementation
    struct PlatformData;
    std::unique_ptr<PlatformData> platform_data_;

    // Internal methods
    void install_signal_handlers();
    void install_exception_handler();
    CrashInfo gather_crash_info(CrashType type, const std::string& message,
                                 void* exception_info = nullptr);
    void write_dump(const CrashInfo& info, const std::filesystem::path& path);
    void write_log(const CrashInfo& info, const std::filesystem::path& path);
    CrashReport process_crash(CrashType type, const std::string& message,
                              void* exception_info = nullptr);

    // Signal handler (static for C callback)
    static void signal_handler(int signal);
    static CrashHandler* instance_;
};

// =============================================================================
// Assert Macros
// =============================================================================

#define VOID_ASSERT(expr) \
    ((expr) ? (void)0 : void_runtime::CrashHandler::assert_failed(#expr, __FILE__, __LINE__))

#define VOID_ASSERT_MSG(expr, msg) \
    ((expr) ? (void)0 : void_runtime::CrashHandler::assert_failed(#expr, __FILE__, __LINE__, msg))

#define VOID_VERIFY(expr) \
    ([&]() -> bool { \
        bool result = (expr); \
        if (!result) { \
            void_runtime::CrashHandler::assert_failed(#expr, __FILE__, __LINE__); \
        } \
        return result; \
    }())

#ifdef NDEBUG
#define VOID_DEBUG_ASSERT(expr) ((void)0)
#else
#define VOID_DEBUG_ASSERT(expr) VOID_ASSERT(expr)
#endif

// =============================================================================
// Guard Macros
// =============================================================================

/// @brief Exception guard for function boundaries
#define VOID_EXCEPTION_GUARD_BEGIN try {

#define VOID_EXCEPTION_GUARD_END \
    } catch (const std::exception& e) { \
        void_runtime::CrashHandler::instance().report_exception(e); \
    } catch (...) { \
        void_runtime::CrashHandler::instance().generate_report("Unknown exception caught"); \
    }

} // namespace void_runtime
