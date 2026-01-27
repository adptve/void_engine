/// @file webgpu_backend.cpp
/// @brief WebGPU GPU backend implementation

#include "webgpu_backend.hpp"

namespace void_render {
namespace backends {

using namespace gpu;

bool check_webgpu_available() {
#ifdef __EMSCRIPTEN__
    return true;  // WebGPU available in browsers with support
#else
    // Native WebGPU via Dawn or wgpu-native
    // Would check for library availability
    return false;
#endif
}

std::unique_ptr<IGpuBackend> create_webgpu_backend() {
    return std::make_unique<WebGPUBackend>();
}

BackendError WebGPUBackend::init(const BackendConfig& config) {
    if (m_initialized) return BackendError::AlreadyInitialized;

    // WebGPU initialization
    // Would use wgpuCreateInstance, wgpuInstanceRequestAdapter, wgpuAdapterRequestDevice

    m_config = config;
    m_capabilities.gpu_backend = GpuBackend::WebGPU;
    m_capabilities.device_name = "WebGPU Device";

    // WebGPU features
    m_capabilities.features.compute_shaders = true;
    m_capabilities.features.texture_compression_bc = true;
    m_capabilities.features.sampler_anisotropy = true;

    m_initialized = true;
    return BackendError::None;
}

void WebGPUBackend::shutdown() {
    if (!m_initialized) return;

    m_buffers.clear();
    m_textures.clear();
    m_samplers.clear();
    m_shader_modules.clear();
    m_render_pipelines.clear();
    m_compute_pipelines.clear();

    m_initialized = false;
}

BufferHandle WebGPUBackend::create_buffer(const BufferDesc& desc) {
    if (!m_initialized) return BufferHandle::invalid();
    // Would use wgpuDeviceCreateBuffer
    BufferHandle handle{++m_next_handle};
    m_buffers[handle.id] = nullptr;
    return handle;
}

TextureHandle WebGPUBackend::create_texture(const TextureDesc& desc) {
    if (!m_initialized) return TextureHandle::invalid();
    // Would use wgpuDeviceCreateTexture
    TextureHandle handle{++m_next_handle};
    m_textures[handle.id] = nullptr;
    return handle;
}

SamplerHandle WebGPUBackend::create_sampler(const SamplerDesc&) {
    if (!m_initialized) return SamplerHandle::invalid();
    SamplerHandle handle{++m_next_handle};
    m_samplers[handle.id] = nullptr;
    return handle;
}

ShaderModuleHandle WebGPUBackend::create_shader_module(const ShaderModuleDesc&) {
    if (!m_initialized) return ShaderModuleHandle::invalid();
    // WebGPU uses WGSL or SPIR-V
    ShaderModuleHandle handle{++m_next_handle};
    m_shader_modules[handle.id] = nullptr;
    return handle;
}

PipelineHandle WebGPUBackend::create_render_pipeline(const RenderPipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    PipelineHandle handle{++m_next_handle};
    m_render_pipelines[handle.id] = nullptr;
    return handle;
}

PipelineHandle WebGPUBackend::create_compute_pipeline(const ComputePipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    PipelineHandle handle{++m_next_handle};
    m_compute_pipelines[handle.id] = nullptr;
    return handle;
}

void WebGPUBackend::destroy_buffer(BufferHandle handle) { m_buffers.erase(handle.id); }
void WebGPUBackend::destroy_texture(TextureHandle handle) { m_textures.erase(handle.id); }
void WebGPUBackend::destroy_sampler(SamplerHandle handle) { m_samplers.erase(handle.id); }
void WebGPUBackend::destroy_shader_module(ShaderModuleHandle handle) { m_shader_modules.erase(handle.id); }
void WebGPUBackend::destroy_pipeline(PipelineHandle handle) {
    m_render_pipelines.erase(handle.id);
    m_compute_pipelines.erase(handle.id);
}

void WebGPUBackend::write_buffer(BufferHandle, std::size_t, const void*, std::size_t) {}
void* WebGPUBackend::map_buffer(BufferHandle, std::size_t, std::size_t) { return nullptr; }
void WebGPUBackend::unmap_buffer(BufferHandle) {}
void WebGPUBackend::write_texture(TextureHandle, const void*, std::size_t, std::uint32_t, std::uint32_t) {}
void WebGPUBackend::generate_mipmaps(TextureHandle) {}

BackendError WebGPUBackend::begin_frame() {
    m_frame_number++;
    return BackendError::None;
}

BackendError WebGPUBackend::end_frame() { return BackendError::None; }
void WebGPUBackend::present() {}
void WebGPUBackend::wait_idle() {}

void WebGPUBackend::resize(std::uint32_t width, std::uint32_t height) {
    m_config.initial_width = width;
    m_config.initial_height = height;
}

RehydrationState WebGPUBackend::get_rehydration_state() const {
    RehydrationState state;
    state.width = m_config.initial_width;
    state.height = m_config.initial_height;
    state.frame_count = m_frame_number;
    return state;
}

BackendError WebGPUBackend::rehydrate(const RehydrationState& state) {
    resize(state.width, state.height);
    m_frame_number = state.frame_count;
    return BackendError::None;
}

FrameTiming WebGPUBackend::get_frame_timing() const {
    FrameTiming timing;
    timing.frame_number = m_frame_number;
    return timing;
}

std::uint64_t WebGPUBackend::get_allocated_memory() const { return 0; }

} // namespace backends
} // namespace void_render
