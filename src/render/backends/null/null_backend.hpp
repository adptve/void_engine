/// @file null_backend.hpp
/// @brief Null GPU backend implementation for testing and headless operation
///
/// STATUS: PRODUCTION (2026-01-28)
/// - Full IGpuBackend implementation with no GPU operations
/// - CPU-side storage for buffer/texture data validation
/// - SACRED hot-reload patterns implemented
///
/// USE CASES:
/// - Headless operation (CI/CD, servers, batch processing)
/// - Testing without GPU hardware
/// - Fallback when no other backend is available
/// - Performance profiling of CPU-side code
///
#pragma once

#include "void_engine/render/backend.hpp"
#include <unordered_map>
#include <vector>

namespace void_render {
namespace backends {

/// Null backend - provides a complete IGpuBackend implementation that
/// performs no actual GPU operations. Used for:
/// - Headless operation (servers, CI/CD)
/// - Testing without GPU
/// - Fallback when no GPU is available
class NullBackend : public gpu::IGpuBackend {
public:
    NullBackend() = default;
    ~NullBackend() override { shutdown(); }

    // IGpuBackend interface
    gpu::BackendError init(const gpu::BackendConfig& config) override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Null; }
    [[nodiscard]] const gpu::BackendCapabilities& capabilities() const override { return m_capabilities; }

    gpu::BufferHandle create_buffer(const gpu::BufferDesc& desc) override;
    gpu::TextureHandle create_texture(const gpu::TextureDesc& desc) override;
    gpu::SamplerHandle create_sampler(const gpu::SamplerDesc& desc) override;
    gpu::ShaderModuleHandle create_shader_module(const gpu::ShaderModuleDesc& desc) override;
    gpu::PipelineHandle create_render_pipeline(const gpu::RenderPipelineDesc& desc) override;
    gpu::PipelineHandle create_compute_pipeline(const gpu::ComputePipelineDesc& desc) override;

    void destroy_buffer(gpu::BufferHandle handle) override;
    void destroy_texture(gpu::TextureHandle handle) override;
    void destroy_sampler(gpu::SamplerHandle handle) override;
    void destroy_shader_module(gpu::ShaderModuleHandle handle) override;
    void destroy_pipeline(gpu::PipelineHandle handle) override;

    void write_buffer(gpu::BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override;
    void* map_buffer(gpu::BufferHandle handle, std::size_t offset, std::size_t size) override;
    void unmap_buffer(gpu::BufferHandle handle) override;

    void write_texture(gpu::TextureHandle handle, const void* data, std::size_t size,
                       std::uint32_t mip_level, std::uint32_t array_layer) override;
    void generate_mipmaps(gpu::TextureHandle handle) override;

    gpu::BackendError begin_frame() override;
    gpu::BackendError end_frame() override;
    void present() override;
    void wait_idle() override;

    void resize(std::uint32_t width, std::uint32_t height) override;

    // SACRED hot-reload patterns
    gpu::RehydrationState get_rehydration_state() const override;
    gpu::BackendError rehydrate(const gpu::RehydrationState& state) override;

    gpu::FrameTiming get_frame_timing() const override;
    std::uint64_t get_allocated_memory() const override;

private:
    bool m_initialized = false;
    gpu::BackendCapabilities m_capabilities;
    gpu::BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    // CPU-side storage for null backend
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> m_buffers;
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> m_textures;
};

/// Factory function to create Null backend
[[nodiscard]] std::unique_ptr<gpu::IGpuBackend> create_null_backend();

/// Check if Null backend is available (always true)
[[nodiscard]] constexpr bool check_null_available() { return true; }

} // namespace backends
} // namespace void_render
