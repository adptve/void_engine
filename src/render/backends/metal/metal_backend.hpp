/// @file metal_backend.hpp
/// @brief Metal GPU backend implementation (macOS/iOS only)
///
/// STATUS: STUB (2026-01-28)
/// - Class structure and interface in place
/// - SACRED hot-reload patterns implemented
///
/// TODO to reach PRODUCTION:
/// - [ ] Objective-C++ integration (this file should be .mm)
/// - [ ] MTLCreateSystemDefaultDevice() for device creation
/// - [ ] MTLCommandQueue and MTLCommandBuffer
/// - [ ] MTLBuffer, MTLTexture creation with proper storage modes
/// - [ ] MTLRenderPipelineState and MTLComputePipelineState
/// - [ ] CAMetalLayer integration for window presentation
/// - [ ] MSL shader compilation or SPIRV-Cross for SPIR-V conversion
///
#pragma once

#include "void_engine/render/backend.hpp"

#ifdef __APPLE__
#define VOID_PLATFORM_MACOS 1

namespace void_render {
namespace backends {

// Metal types (Objective-C objects are void* in C++)
typedef void* MTLDevice;
typedef void* MTLCommandQueue;
typedef void* MTLCommandBuffer;
typedef void* MTLBuffer;
typedef void* MTLTexture;
typedef void* MTLSamplerState;
typedef void* MTLRenderPipelineState;
typedef void* MTLComputePipelineState;
typedef void* MTLLibrary;
typedef void* MTLFunction;

class MetalBackend : public gpu::IGpuBackend {
public:
    MetalBackend() = default;
    ~MetalBackend() override { shutdown(); }

    gpu::BackendError init(const gpu::BackendConfig& config) override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Metal; }
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

    MTLDevice m_device = nullptr;
    MTLCommandQueue m_command_queue = nullptr;
};

/// Factory function to create Metal backend
[[nodiscard]] std::unique_ptr<gpu::IGpuBackend> create_metal_backend();

/// Check if Metal is available on this system
[[nodiscard]] bool check_metal_available();

} // namespace backends
} // namespace void_render

#endif // __APPLE__
