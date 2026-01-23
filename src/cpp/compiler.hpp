#pragma once

/// @file compiler.hpp
/// @brief C++ compiler abstraction

#include "types.hpp"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_cpp {

// =============================================================================
// Compile Job
// =============================================================================

/// @brief A single compilation job
class CompileJob {
public:
    CompileJob(CompileJobId id, std::vector<std::filesystem::path> sources, std::string output_name);

    // Identity
    [[nodiscard]] CompileJobId id() const { return id_; }
    [[nodiscard]] const std::string& output_name() const { return output_name_; }

    // Sources
    [[nodiscard]] const std::vector<std::filesystem::path>& sources() const { return sources_; }
    void add_source(const std::filesystem::path& path) { sources_.push_back(path); }

    // Status
    [[nodiscard]] CompileStatus status() const { return status_; }
    [[nodiscard]] float progress() const { return progress_; }

    // Result
    [[nodiscard]] const CompileResult& result() const { return result_; }
    [[nodiscard]] bool is_complete() const;

    // Wait
    void wait();
    bool wait_for(std::chrono::milliseconds timeout);

    // Cancel
    void cancel();
    [[nodiscard]] bool is_cancelled() const { return cancelled_; }

private:
    friend class Compiler;
    friend class CompileQueue;

    CompileJobId id_;
    std::vector<std::filesystem::path> sources_;
    std::string output_name_;

    std::atomic<CompileStatus> status_{CompileStatus::Pending};
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> cancelled_{false};

    CompileResult result_;

    std::promise<void> completion_promise_;
    std::shared_future<void> completion_future_;
};

// =============================================================================
// Compiler Interface
// =============================================================================

/// @brief Abstract compiler interface
class ICompiler {
public:
    virtual ~ICompiler() = default;

    // Identification
    [[nodiscard]] virtual CompilerType type() const = 0;
    [[nodiscard]] virtual std::string version() const = 0;
    [[nodiscard]] virtual std::filesystem::path path() const = 0;

    // Compilation
    [[nodiscard]] virtual CppResult<CompileResult> compile(
        const std::vector<std::filesystem::path>& sources,
        const std::filesystem::path& output,
        const CompilerConfig& config) = 0;

    // Build command generation
    [[nodiscard]] virtual std::vector<std::string> build_compile_command(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const CompilerConfig& config) const = 0;

    [[nodiscard]] virtual std::vector<std::string> build_link_command(
        const std::vector<std::filesystem::path>& objects,
        const std::filesystem::path& output,
        const CompilerConfig& config) const = 0;

    // Validation
    [[nodiscard]] virtual bool is_available() const = 0;
    [[nodiscard]] virtual bool supports_standard(CppStandard std) const = 0;

protected:
    // Parse diagnostic output
    [[nodiscard]] virtual std::vector<CompileDiagnostic> parse_output(
        const std::string& output) const = 0;
};

// =============================================================================
// MSVC Compiler
// =============================================================================

/// @brief Microsoft Visual C++ compiler
class MSVCCompiler : public ICompiler {
public:
    MSVCCompiler();
    explicit MSVCCompiler(const std::filesystem::path& compiler_path);

    [[nodiscard]] CompilerType type() const override { return CompilerType::MSVC; }
    [[nodiscard]] std::string version() const override { return version_; }
    [[nodiscard]] std::filesystem::path path() const override { return cl_path_; }

    [[nodiscard]] CppResult<CompileResult> compile(
        const std::vector<std::filesystem::path>& sources,
        const std::filesystem::path& output,
        const CompilerConfig& config) override;

    [[nodiscard]] std::vector<std::string> build_compile_command(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] std::vector<std::string> build_link_command(
        const std::vector<std::filesystem::path>& objects,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] bool is_available() const override { return available_; }
    [[nodiscard]] bool supports_standard(CppStandard std) const override;

    // MSVC-specific
    [[nodiscard]] const std::filesystem::path& link_path() const { return link_path_; }
    [[nodiscard]] const std::vector<std::filesystem::path>& include_dirs() const { return include_dirs_; }
    [[nodiscard]] const std::vector<std::filesystem::path>& lib_dirs() const { return lib_dirs_; }

protected:
    [[nodiscard]] std::vector<CompileDiagnostic> parse_output(
        const std::string& output) const override;

private:
    void detect_compiler();
    void detect_from_vcvars();
    void detect_from_registry();

    std::filesystem::path cl_path_;
    std::filesystem::path link_path_;
    std::vector<std::filesystem::path> include_dirs_;
    std::vector<std::filesystem::path> lib_dirs_;
    std::string version_;
    bool available_ = false;
};

// =============================================================================
// Clang Compiler
// =============================================================================

/// @brief Clang/LLVM compiler
class ClangCompiler : public ICompiler {
public:
    ClangCompiler();
    explicit ClangCompiler(const std::filesystem::path& compiler_path);

    [[nodiscard]] CompilerType type() const override { return CompilerType::Clang; }
    [[nodiscard]] std::string version() const override { return version_; }
    [[nodiscard]] std::filesystem::path path() const override { return clang_path_; }

    [[nodiscard]] CppResult<CompileResult> compile(
        const std::vector<std::filesystem::path>& sources,
        const std::filesystem::path& output,
        const CompilerConfig& config) override;

    [[nodiscard]] std::vector<std::string> build_compile_command(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] std::vector<std::string> build_link_command(
        const std::vector<std::filesystem::path>& objects,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] bool is_available() const override { return available_; }
    [[nodiscard]] bool supports_standard(CppStandard std) const override;

protected:
    [[nodiscard]] std::vector<CompileDiagnostic> parse_output(
        const std::string& output) const override;

private:
    void detect_compiler();

    std::filesystem::path clang_path_;
    std::string version_;
    bool available_ = false;
};

// =============================================================================
// GCC Compiler
// =============================================================================

/// @brief GNU Compiler Collection
class GCCCompiler : public ICompiler {
public:
    GCCCompiler();
    explicit GCCCompiler(const std::filesystem::path& compiler_path);

    [[nodiscard]] CompilerType type() const override { return CompilerType::GCC; }
    [[nodiscard]] std::string version() const override { return version_; }
    [[nodiscard]] std::filesystem::path path() const override { return gcc_path_; }

    [[nodiscard]] CppResult<CompileResult> compile(
        const std::vector<std::filesystem::path>& sources,
        const std::filesystem::path& output,
        const CompilerConfig& config) override;

    [[nodiscard]] std::vector<std::string> build_compile_command(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] std::vector<std::string> build_link_command(
        const std::vector<std::filesystem::path>& objects,
        const std::filesystem::path& output,
        const CompilerConfig& config) const override;

    [[nodiscard]] bool is_available() const override { return available_; }
    [[nodiscard]] bool supports_standard(CppStandard std) const override;

protected:
    [[nodiscard]] std::vector<CompileDiagnostic> parse_output(
        const std::string& output) const override;

private:
    void detect_compiler();

    std::filesystem::path gcc_path_;
    std::string version_;
    bool available_ = false;
};

// =============================================================================
// Compile Queue
// =============================================================================

/// @brief Asynchronous compilation queue
class CompileQueue {
public:
    explicit CompileQueue(std::size_t num_workers = 0);
    ~CompileQueue();

    // Non-copyable
    CompileQueue(const CompileQueue&) = delete;
    CompileQueue& operator=(const CompileQueue&) = delete;

    // Submit jobs
    std::shared_ptr<CompileJob> submit(
        std::vector<std::filesystem::path> sources,
        std::string output_name,
        const CompilerConfig& config);

    // Cancel
    void cancel(CompileJobId id);
    void cancel_all();

    // Status
    [[nodiscard]] std::size_t pending_count() const;
    [[nodiscard]] std::size_t active_count() const;

    // Wait
    void wait_all();

    // Set compiler
    void set_compiler(std::unique_ptr<ICompiler> compiler);

private:
    void worker_thread();

    struct QueueEntry {
        std::shared_ptr<CompileJob> job;
        CompilerConfig config;
    };

    std::queue<QueueEntry> queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_{false};
    std::atomic<std::size_t> active_jobs_{0};

    std::unique_ptr<ICompiler> compiler_;
    std::uint32_t next_job_id_ = 1;
};

// =============================================================================
// Main Compiler Class
// =============================================================================

/// @brief Main compiler facade
class Compiler {
public:
    Compiler();
    explicit Compiler(const CompilerConfig& config);
    ~Compiler();

    // Singleton access
    [[nodiscard]] static Compiler& instance();

    // Configuration
    [[nodiscard]] const CompilerConfig& config() const { return config_; }
    void set_config(const CompilerConfig& config);

    // Compiler selection
    [[nodiscard]] ICompiler* compiler() const { return compiler_.get(); }
    void set_compiler(CompilerType type);
    void set_compiler(std::unique_ptr<ICompiler> compiler);

    // Detection
    [[nodiscard]] std::vector<CompilerType> available_compilers() const;
    [[nodiscard]] CompilerType detect_best_compiler() const;

    // ==========================================================================
    // Synchronous Compilation
    // ==========================================================================

    /// @brief Compile source files synchronously
    CppResult<CompileResult> compile(
        const std::vector<std::filesystem::path>& sources,
        const std::string& output_name);

    /// @brief Compile single source file
    CppResult<CompileResult> compile(
        const std::filesystem::path& source,
        const std::string& output_name);

    // ==========================================================================
    // Asynchronous Compilation
    // ==========================================================================

    /// @brief Submit compilation job
    std::shared_ptr<CompileJob> compile_async(
        const std::vector<std::filesystem::path>& sources,
        const std::string& output_name);

    /// @brief Cancel pending job
    void cancel(CompileJobId id);

    /// @brief Wait for all jobs
    void wait_all();

    // ==========================================================================
    // Utilities
    // ==========================================================================

    /// @brief Check if source file needs recompilation
    [[nodiscard]] bool needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& object) const;

    /// @brief Get object file path for source
    [[nodiscard]] std::filesystem::path get_object_path(
        const std::filesystem::path& source) const;

    /// @brief Get output file path
    [[nodiscard]] std::filesystem::path get_output_path(
        const std::string& name) const;

private:
    CompilerConfig config_;
    std::unique_ptr<ICompiler> compiler_;
    std::unique_ptr<CompileQueue> queue_;
};

} // namespace void_cpp
