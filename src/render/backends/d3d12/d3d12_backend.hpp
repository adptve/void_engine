/// @file d3d12_backend.hpp
/// @brief Direct3D 12 GPU backend implementation (Windows only)
///
/// STATUS: STUB (2026-01-28)
/// - DLL loading works (d3d12.dll, dxgi.dll)
/// - Class structure and interface in place
/// - SACRED hot-reload patterns implemented
///
/// TODO to reach PRODUCTION:
/// - [ ] D3D12CreateDevice with proper adapter enumeration
/// - [ ] IDXGISwapChain3 creation for window presentation
/// - [ ] ID3D12CommandQueue, ID3D12CommandAllocator, ID3D12GraphicsCommandList
/// - [ ] ID3D12DescriptorHeap for CBV/SRV/UAV and RTV/DSV
/// - [ ] ID3D12Resource creation with proper heap types
/// - [ ] Root signature and PSO creation
/// - [ ] Frame synchronization with fences
///
#pragma once

#include "void_engine/render/backend.hpp"

#ifdef _WIN32
#define VOID_PLATFORM_WINDOWS 1
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <Windows.h>
#include <unordered_map>
#include <vector>

namespace void_render {
namespace backends {

// D3D12 types (forward declarations to avoid including d3d12.h)
typedef void* ID3D12Device;
typedef void* ID3D12CommandQueue;
typedef void* ID3D12CommandAllocator;
typedef void* ID3D12GraphicsCommandList;
typedef void* ID3D12Resource;
typedef void* ID3D12DescriptorHeap;
typedef void* ID3D12PipelineState;
typedef void* ID3D12RootSignature;
typedef void* IDXGIFactory4;
typedef void* IDXGISwapChain3;

class D3D12Backend : public gpu::IGpuBackend {
public:
    D3D12Backend() = default;
    ~D3D12Backend() override { shutdown(); }

    gpu::BackendError init(const gpu::BackendConfig& config) override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Direct3D12; }
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
    struct D3D12Buffer {
        ID3D12Resource resource = nullptr;
        std::uint64_t size = 0;
        void* mapped = nullptr;
    };

    struct D3D12Texture {
        ID3D12Resource resource = nullptr;
        std::uint32_t width = 0, height = 0;
    };

    bool m_initialized = false;
    gpu::BackendCapabilities m_capabilities;
    gpu::BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    HMODULE m_d3d12_library = nullptr;
    HMODULE m_dxgi_library = nullptr;

    ID3D12Device m_device = nullptr;
    ID3D12CommandQueue m_command_queue = nullptr;
    ID3D12CommandAllocator m_command_allocator = nullptr;

    std::unordered_map<std::uint64_t, D3D12Buffer> m_buffers;
    std::unordered_map<std::uint64_t, D3D12Texture> m_textures;
    std::unordered_map<std::uint64_t, ID3D12PipelineState> m_pipelines;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_shaders;

    bool create_device(const gpu::BackendConfig& config);
};

/// Factory function to create D3D12 backend
[[nodiscard]] std::unique_ptr<gpu::IGpuBackend> create_d3d12_backend();

/// Check if D3D12 is available on this system
[[nodiscard]] bool check_d3d12_available();

} // namespace backends
} // namespace void_render

#endif // _WIN32
