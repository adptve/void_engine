/// @file null_backend.cpp
/// @brief Null GPU backend implementation

#include "null_backend.hpp"
#include <cstring>
#include <algorithm>

namespace void_render {
namespace backends {

using namespace gpu;

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<IGpuBackend> create_null_backend() {
    return std::make_unique<NullBackend>();
}

// =============================================================================
// NullBackend Implementation
// =============================================================================

BackendError NullBackend::init(const BackendConfig& config) {
    if (m_initialized) return BackendError::AlreadyInitialized;

    m_capabilities.gpu_backend = GpuBackend::Null;
    m_capabilities.display_backend = DisplayBackend::Headless;
    m_capabilities.device_name = "Null Device";
    m_capabilities.driver_version = "1.0.0";

    // Set reasonable limits for null backend
    m_capabilities.limits.max_texture_size_2d = 16384;
    m_capabilities.limits.max_texture_size_3d = 2048;
    m_capabilities.limits.max_texture_size_cube = 16384;
    m_capabilities.limits.max_texture_array_layers = 2048;
    m_capabilities.limits.max_uniform_buffer_size = 65536;
    m_capabilities.limits.max_storage_buffer_size = 134217728;
    m_capabilities.limits.max_compute_workgroup_size_x = 1024;
    m_capabilities.limits.max_compute_workgroup_size_y = 1024;
    m_capabilities.limits.max_compute_workgroup_size_z = 64;

    // Enable all features in null backend for testing
    m_capabilities.features.compute_shaders = true;
    m_capabilities.features.tessellation = true;
    m_capabilities.features.geometry_shaders = true;
    m_capabilities.features.multi_draw_indirect = true;
    m_capabilities.features.sampler_anisotropy = true;
    m_capabilities.features.texture_compression_bc = true;

    m_config = config;
    m_initialized = true;
    return BackendError::None;
}

void NullBackend::shutdown() {
    if (!m_initialized) return;

    m_initialized = false;
    m_buffers.clear();
    m_textures.clear();
}

BufferHandle NullBackend::create_buffer(const BufferDesc& desc) {
    if (!m_initialized) return BufferHandle::invalid();

    BufferHandle handle{++m_next_handle};
    m_buffers[handle.id] = std::vector<std::uint8_t>(desc.size, 0);
    return handle;
}

TextureHandle NullBackend::create_texture(const TextureDesc& desc) {
    if (!m_initialized) return TextureHandle::invalid();

    TextureHandle handle{++m_next_handle};
    // Calculate size based on format and dimensions
    std::size_t size = desc.width * desc.height * desc.depth_or_layers * 4;  // Assume RGBA8
    m_textures[handle.id] = std::vector<std::uint8_t>(size, 0);
    return handle;
}

SamplerHandle NullBackend::create_sampler(const SamplerDesc&) {
    if (!m_initialized) return SamplerHandle::invalid();
    return SamplerHandle{++m_next_handle};
}

ShaderModuleHandle NullBackend::create_shader_module(const ShaderModuleDesc&) {
    if (!m_initialized) return ShaderModuleHandle::invalid();
    return ShaderModuleHandle{++m_next_handle};
}

PipelineHandle NullBackend::create_render_pipeline(const RenderPipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    return PipelineHandle{++m_next_handle};
}

PipelineHandle NullBackend::create_compute_pipeline(const ComputePipelineDesc&) {
    if (!m_initialized) return PipelineHandle::invalid();
    return PipelineHandle{++m_next_handle};
}

void NullBackend::destroy_buffer(BufferHandle handle) {
    m_buffers.erase(handle.id);
}

void NullBackend::destroy_texture(TextureHandle handle) {
    m_textures.erase(handle.id);
}

void NullBackend::destroy_sampler(SamplerHandle) {}
void NullBackend::destroy_shader_module(ShaderModuleHandle) {}
void NullBackend::destroy_pipeline(PipelineHandle) {}

void NullBackend::write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end() && offset + size <= it->second.size()) {
        std::memcpy(it->second.data() + offset, data, size);
    }
}

void* NullBackend::map_buffer(BufferHandle handle, std::size_t offset, std::size_t) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end() && offset < it->second.size()) {
        return it->second.data() + offset;
    }
    return nullptr;
}

void NullBackend::unmap_buffer(BufferHandle) {}

void NullBackend::write_texture(TextureHandle handle, const void* data, std::size_t size,
                                 std::uint32_t, std::uint32_t) {
    auto it = m_textures.find(handle.id);
    if (it != m_textures.end()) {
        std::size_t copy_size = std::min(size, it->second.size());
        std::memcpy(it->second.data(), data, copy_size);
    }
}

void NullBackend::generate_mipmaps(TextureHandle) {}

BackendError NullBackend::begin_frame() {
    m_frame_number++;
    return BackendError::None;
}

BackendError NullBackend::end_frame() {
    return BackendError::None;
}

void NullBackend::present() {}
void NullBackend::wait_idle() {}

void NullBackend::resize(std::uint32_t width, std::uint32_t height) {
    m_config.initial_width = width;
    m_config.initial_height = height;
}

RehydrationState NullBackend::get_rehydration_state() const {
    RehydrationState state;
    state.width = m_config.initial_width;
    state.height = m_config.initial_height;
    state.fullscreen = m_config.fullscreen;
    state.vsync = m_config.vsync;
    state.frame_count = m_frame_number;
    return state;
}

BackendError NullBackend::rehydrate(const RehydrationState& state) {
    m_config.initial_width = state.width;
    m_config.initial_height = state.height;
    m_config.fullscreen = state.fullscreen;
    m_config.vsync = state.vsync;
    m_frame_number = state.frame_count;
    return BackendError::None;
}

FrameTiming NullBackend::get_frame_timing() const {
    FrameTiming timing;
    timing.frame_number = m_frame_number;
    return timing;
}

std::uint64_t NullBackend::get_allocated_memory() const {
    std::uint64_t total = 0;
    for (const auto& [id, buf] : m_buffers) {
        total += buf.size();
    }
    for (const auto& [id, tex] : m_textures) {
        total += tex.size();
    }
    return total;
}

} // namespace backends
} // namespace void_render
