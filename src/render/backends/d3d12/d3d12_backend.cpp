/// @file d3d12_backend.cpp
/// @brief Direct3D 12 GPU backend implementation

#include "d3d12_backend.hpp"
#include <cstring>

#ifdef VOID_PLATFORM_WINDOWS

namespace void_render {
namespace backends {

using namespace gpu;

bool check_d3d12_available() {
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    if (d3d12) {
        FreeLibrary(d3d12);
        return true;
    }
    return false;
}

std::unique_ptr<IGpuBackend> create_d3d12_backend() {
    return std::make_unique<D3D12Backend>();
}

BackendError D3D12Backend::init(const BackendConfig& config) {
    if (m_initialized) return BackendError::AlreadyInitialized;

    m_d3d12_library = LoadLibraryA("d3d12.dll");
    if (!m_d3d12_library) return BackendError::UnsupportedBackend;

    m_dxgi_library = LoadLibraryA("dxgi.dll");
    if (!m_dxgi_library) {
        FreeLibrary(m_d3d12_library);
        return BackendError::UnsupportedBackend;
    }

    if (!create_device(config)) {
        FreeLibrary(m_dxgi_library);
        FreeLibrary(m_d3d12_library);
        return BackendError::UnsupportedBackend;
    }

    m_config = config;
    m_capabilities.gpu_backend = GpuBackend::Direct3D12;
    m_capabilities.device_name = "Direct3D 12 Device";

    // D3D12 advanced features
    m_capabilities.features.compute_shaders = true;
    m_capabilities.features.tessellation = true;
    m_capabilities.features.mesh_shaders = true;
    m_capabilities.features.ray_tracing = true;
    m_capabilities.features.variable_rate_shading = true;

    m_initialized = true;
    return BackendError::None;
}

void D3D12Backend::shutdown() {
    if (!m_initialized) return;

    wait_idle();

    m_buffers.clear();
    m_textures.clear();
    m_pipelines.clear();

    m_device = nullptr;
    m_command_queue = nullptr;

    if (m_dxgi_library) FreeLibrary(m_dxgi_library);
    if (m_d3d12_library) FreeLibrary(m_d3d12_library);

    m_initialized = false;
}

BufferHandle D3D12Backend::create_buffer(const BufferDesc& desc) {
    if (!m_initialized) return BufferHandle::invalid();

    D3D12Buffer d3d_buf;
    d3d_buf.size = desc.size;
    // Would call ID3D12Device::CreateCommittedResource

    BufferHandle handle{++m_next_handle};
    m_buffers[handle.id] = d3d_buf;
    return handle;
}

TextureHandle D3D12Backend::create_texture(const TextureDesc& desc) {
    if (!m_initialized) return TextureHandle::invalid();

    D3D12Texture d3d_tex;
    d3d_tex.width = desc.width;
    d3d_tex.height = desc.height;
    // Would call ID3D12Device::CreateCommittedResource

    TextureHandle handle{++m_next_handle};
    m_textures[handle.id] = d3d_tex;
    return handle;
}

SamplerHandle D3D12Backend::create_sampler(const SamplerDesc&) {
    if (!m_initialized) return SamplerHandle::invalid();
    return SamplerHandle{++m_next_handle};
}

ShaderModuleHandle D3D12Backend::create_shader_module(const ShaderModuleDesc& desc) {
    if (!m_initialized) return ShaderModuleHandle::invalid();
    ShaderModuleHandle handle{++m_next_handle};
    m_shaders[handle.id] = desc.spirv;  // Would convert to DXIL
    return handle;
}

PipelineHandle D3D12Backend::create_render_pipeline(const RenderPipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    return PipelineHandle{++m_next_handle};
}

PipelineHandle D3D12Backend::create_compute_pipeline(const ComputePipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    return PipelineHandle{++m_next_handle};
}

void D3D12Backend::destroy_buffer(BufferHandle handle) { m_buffers.erase(handle.id); }
void D3D12Backend::destroy_texture(TextureHandle handle) { m_textures.erase(handle.id); }
void D3D12Backend::destroy_sampler(SamplerHandle) {}
void D3D12Backend::destroy_shader_module(ShaderModuleHandle handle) { m_shaders.erase(handle.id); }
void D3D12Backend::destroy_pipeline(PipelineHandle handle) { m_pipelines.erase(handle.id); }

void D3D12Backend::write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end() && it->second.mapped) {
        std::memcpy(static_cast<char*>(it->second.mapped) + offset, data, size);
    }
}

void* D3D12Backend::map_buffer(BufferHandle handle, std::size_t, std::size_t) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end()) {
        return it->second.mapped;
    }
    return nullptr;
}

void D3D12Backend::unmap_buffer(BufferHandle handle) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end()) {
        it->second.mapped = nullptr;
    }
}

void D3D12Backend::write_texture(TextureHandle, const void*, std::size_t, std::uint32_t, std::uint32_t) {}
void D3D12Backend::generate_mipmaps(TextureHandle) {}

BackendError D3D12Backend::begin_frame() {
    m_frame_number++;
    return BackendError::None;
}

BackendError D3D12Backend::end_frame() {
    return BackendError::None;
}

void D3D12Backend::present() {}
void D3D12Backend::wait_idle() {}

void D3D12Backend::resize(std::uint32_t width, std::uint32_t height) {
    m_config.initial_width = width;
    m_config.initial_height = height;
}

RehydrationState D3D12Backend::get_rehydration_state() const {
    RehydrationState state;
    state.width = m_config.initial_width;
    state.height = m_config.initial_height;
    state.frame_count = m_frame_number;
    return state;
}

BackendError D3D12Backend::rehydrate(const RehydrationState& state) {
    resize(state.width, state.height);
    m_frame_number = state.frame_count;
    return BackendError::None;
}

FrameTiming D3D12Backend::get_frame_timing() const {
    FrameTiming timing;
    timing.frame_number = m_frame_number;
    return timing;
}

std::uint64_t D3D12Backend::get_allocated_memory() const {
    std::uint64_t total = 0;
    for (const auto& [id, buf] : m_buffers) total += buf.size;
    return total;
}

bool D3D12Backend::create_device(const BackendConfig& config) {
    (void)config;
    // Would use D3D12CreateDevice
    return true;
}

} // namespace backends
} // namespace void_render

#endif // VOID_PLATFORM_WINDOWS
