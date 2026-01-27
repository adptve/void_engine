/// @file webgpu_backend.hpp
/// @brief WebGPU GPU backend implementation
///
/// STATUS: STUB (2026-01-28)
/// - Class structure and interface in place
/// - SACRED hot-reload patterns implemented
///
/// TODO to reach PRODUCTION:
/// - [ ] Link Dawn (Google) or wgpu-native (Mozilla) library
/// - [ ] wgpuCreateInstance, wgpuInstanceRequestAdapter, wgpuAdapterRequestDevice
/// - [ ] WGPUBuffer, WGPUTexture creation
/// - [ ] WGPURenderPipeline, WGPUComputePipeline
/// - [ ] Surface creation for native window presentation
/// - [ ] WGSL shader support or SPIR-V passthrough
/// - [ ] For web: Emscripten integration with browser WebGPU API
///
#pragma once

#include "void_engine/render/backend.hpp"
#include <unordered_map>

namespace void_render {
namespace backends {

// WebGPU types
typedef void* WGPUInstance;
typedef void* WGPUAdapter;
typedef void* WGPUDevice;
typedef void* WGPUQueue;
typedef void* WGPUBuffer;
typedef void* WGPUTexture;
typedef void* WGPUTextureView;
typedef void* WGPUSampler;
typedef void* WGPUShaderModule;
typedef void* WGPURenderPipeline;
typedef void* WGPUComputePipeline;
typedef void* WGPUCommandEncoder;
typedef void* WGPUCommandBuffer;

class WebGPUBackend : public gpu::IGpuBackend {
public:
    WebGPUBackend() = default;
    ~WebGPUBackend() override { shutdown(); }

    gpu::BackendError init(const gpu::BackendConfig& config) override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::WebGPU; }
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

    WGPUInstance m_instance = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;

    std::unordered_map<std::uint64_t, WGPUBuffer> m_buffers;
    std::unordered_map<std::uint64_t, WGPUTexture> m_textures;
    std::unordered_map<std::uint64_t, WGPUSampler> m_samplers;
    std::unordered_map<std::uint64_t, WGPUShaderModule> m_shader_modules;
    std::unordered_map<std::uint64_t, WGPURenderPipeline> m_render_pipelines;
    std::unordered_map<std::uint64_t, WGPUComputePipeline> m_compute_pipelines;
};

/// Factory function to create WebGPU backend
[[nodiscard]] std::unique_ptr<gpu::IGpuBackend> create_webgpu_backend();

/// Check if WebGPU is available on this system
[[nodiscard]] bool check_webgpu_available();

} // namespace backends
} // namespace void_render
