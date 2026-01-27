/// @file opengl_backend.cpp
/// @brief OpenGL GPU backend implementation

#include "opengl_backend.hpp"
#include <cstring>

namespace void_render {
namespace backends {

using namespace gpu;

// =============================================================================
// Availability Check
// =============================================================================

bool check_opengl_available() {
#ifdef VOID_PLATFORM_WINDOWS
    // OpenGL is always available on Windows via opengl32.dll
    return true;
#elif defined(VOID_PLATFORM_LINUX)
    void* gl = dlopen("libGL.so.1", RTLD_LAZY);
    if (gl) {
        dlclose(gl);
        return true;
    }
    return false;
#elif defined(VOID_PLATFORM_MACOS)
    // OpenGL is deprecated but available on macOS
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<IGpuBackend> create_opengl_backend() {
    return std::make_unique<OpenGLBackend>();
}

// =============================================================================
// OpenGLBackend Implementation
// =============================================================================

BackendError OpenGLBackend::init(const BackendConfig& config) {
    if (m_initialized) return BackendError::AlreadyInitialized;

    // Load OpenGL function pointers
    if (!load_gl_functions()) {
        return BackendError::UnsupportedBackend;
    }

    // Query capabilities
    query_capabilities();

    m_capabilities.gpu_backend = GpuBackend::OpenGL;
    m_config = config;
    m_initialized = true;
    return BackendError::None;
}

void OpenGLBackend::shutdown() {
    if (!m_initialized) return;

    // Destroy all resources
    for (auto& [id, handle] : m_gl_buffers) {
        if (glDeleteBuffers_ptr) glDeleteBuffers_ptr(1, &handle);
    }
    m_gl_buffers.clear();

    for (auto& [id, handle] : m_gl_textures) {
        glDeleteTextures(1, &handle);
    }
    m_gl_textures.clear();

    for (auto& [id, handle] : m_gl_samplers) {
        if (glDeleteSamplers_ptr) glDeleteSamplers_ptr(1, &handle);
    }
    m_gl_samplers.clear();

    for (auto& [id, handle] : m_gl_programs) {
        if (glDeleteProgram_ptr) glDeleteProgram_ptr(handle);
    }
    m_gl_programs.clear();

    m_buffer_targets.clear();
    m_texture_targets.clear();
    m_shader_modules.clear();

    m_initialized = false;
}

BufferHandle OpenGLBackend::create_buffer(const BufferDesc& desc) {
    if (!m_initialized || !glGenBuffers_ptr) return BufferHandle::invalid();

    GLuint buffer = 0;
    glGenBuffers_ptr(1, &buffer);

    GLenum target = GL_ARRAY_BUFFER;
    if (desc.usage & BufferUsage::Index) target = GL_ELEMENT_ARRAY_BUFFER;
    else if (desc.usage & BufferUsage::Uniform) target = GL_UNIFORM_BUFFER;
    else if (desc.usage & BufferUsage::Storage) target = GL_SHADER_STORAGE_BUFFER;

    glBindBuffer_ptr(target, buffer);
    glBufferData_ptr(target, desc.size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer_ptr(target, 0);

    BufferHandle handle{++m_next_handle};
    m_gl_buffers[handle.id] = buffer;
    m_buffer_targets[handle.id] = target;
    return handle;
}

TextureHandle OpenGLBackend::create_texture(const TextureDesc& desc) {
    if (!m_initialized) return TextureHandle::invalid();

    GLuint texture = 0;
    glGenTextures(1, &texture);

    GLenum target = GL_TEXTURE_2D;
    switch (desc.dimension) {
        case TextureDimension::D1: target = GL_TEXTURE_1D; break;
        case TextureDimension::D2: target = GL_TEXTURE_2D; break;
        case TextureDimension::D3: target = GL_TEXTURE_3D; break;
        case TextureDimension::Cube: target = GL_TEXTURE_CUBE_MAP; break;
        case TextureDimension::D2Array: target = GL_TEXTURE_2D_ARRAY; break;
        case TextureDimension::CubeArray: target = GL_TEXTURE_CUBE_MAP_ARRAY; break;
    }

    glBindTexture(target, texture);

    // Set up texture storage
    GLenum internal_format = texture_format_to_gl_internal(desc.format);
    GLenum format = texture_format_to_gl_format(desc.format);
    GLenum type = texture_format_to_gl_type(desc.format);

    if (target == GL_TEXTURE_2D) {
        glTexImage2D(target, 0, internal_format, desc.width, desc.height, 0, format, type, nullptr);
    }

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(target, 0);

    TextureHandle handle{++m_next_handle};
    m_gl_textures[handle.id] = texture;
    m_texture_targets[handle.id] = target;
    return handle;
}

SamplerHandle OpenGLBackend::create_sampler(const SamplerDesc& desc) {
    if (!m_initialized) return SamplerHandle::invalid();

    GLuint sampler = 0;
    if (glGenSamplers_ptr) glGenSamplers_ptr(1, &sampler);

    auto address_to_gl = [](SamplerDesc::AddressMode mode) -> GLint {
        switch (mode) {
            case SamplerDesc::AddressMode::Repeat: return GL_REPEAT;
            case SamplerDesc::AddressMode::MirrorRepeat: return GL_MIRRORED_REPEAT;
            case SamplerDesc::AddressMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
            case SamplerDesc::AddressMode::ClampToBorder: return GL_CLAMP_TO_BORDER;
            default: return GL_REPEAT;
        }
    };

    if (glSamplerParameteri_ptr) {
        glSamplerParameteri_ptr(sampler, GL_TEXTURE_MIN_FILTER,
            desc.min_filter == SamplerDesc::Filter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri_ptr(sampler, GL_TEXTURE_MAG_FILTER,
            desc.mag_filter == SamplerDesc::Filter::Nearest ? GL_NEAREST : GL_LINEAR);
        glSamplerParameteri_ptr(sampler, GL_TEXTURE_WRAP_S, address_to_gl(desc.address_mode_u));
        glSamplerParameteri_ptr(sampler, GL_TEXTURE_WRAP_T, address_to_gl(desc.address_mode_v));
        glSamplerParameteri_ptr(sampler, GL_TEXTURE_WRAP_R, address_to_gl(desc.address_mode_w));
    }

    if (desc.max_anisotropy > 1.0f && glSamplerParameterf_ptr) {
        glSamplerParameterf_ptr(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.max_anisotropy);
    }

    SamplerHandle handle{++m_next_handle};
    m_gl_samplers[handle.id] = sampler;
    return handle;
}

ShaderModuleHandle OpenGLBackend::create_shader_module(const ShaderModuleDesc& desc) {
    if (!m_initialized) return ShaderModuleHandle::invalid();

    // Note: OpenGL uses GLSL, would need SPIRV-Cross to convert SPIR-V
    // For now, store the SPIR-V and convert on pipeline creation
    ShaderModuleHandle handle{++m_next_handle};
    m_shader_modules[handle.id] = desc;
    return handle;
}

PipelineHandle OpenGLBackend::create_render_pipeline(const RenderPipelineDesc& desc) {
    if (!m_initialized || !glCreateProgram_ptr) return PipelineHandle::invalid();

    // Create program from shaders
    GLuint program = glCreateProgram_ptr();

    // Compile and attach vertex shader
    if (m_shader_modules.count(desc.vertex_shader.id)) {
        // Would convert SPIR-V to GLSL here using SPIRV-Cross
        // For production, integrate SPIRV-Cross library
    }

    // Compile and attach fragment shader
    if (m_shader_modules.count(desc.fragment_shader.id)) {
        // Would convert SPIR-V to GLSL here
    }

    glLinkProgram_ptr(program);

    GLint success = 0;
    glGetProgramiv_ptr(program, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram_ptr(program);
        return PipelineHandle::invalid();
    }

    PipelineHandle handle{++m_next_handle};
    m_gl_programs[handle.id] = program;
    return handle;
}

PipelineHandle OpenGLBackend::create_compute_pipeline(const ComputePipelineDesc& desc) {
    if (!m_initialized || !glCreateProgram_ptr) return PipelineHandle::invalid();

    GLuint program = glCreateProgram_ptr();

    // Would compile compute shader here

    glLinkProgram_ptr(program);

    PipelineHandle handle{++m_next_handle};
    m_gl_programs[handle.id] = program;
    return handle;
}

void OpenGLBackend::destroy_buffer(BufferHandle handle) {
    auto it = m_gl_buffers.find(handle.id);
    if (it != m_gl_buffers.end()) {
        if (glDeleteBuffers_ptr) glDeleteBuffers_ptr(1, &it->second);
        m_gl_buffers.erase(it);
        m_buffer_targets.erase(handle.id);
    }
}

void OpenGLBackend::destroy_texture(TextureHandle handle) {
    auto it = m_gl_textures.find(handle.id);
    if (it != m_gl_textures.end()) {
        glDeleteTextures(1, &it->second);
        m_gl_textures.erase(it);
        m_texture_targets.erase(handle.id);
    }
}

void OpenGLBackend::destroy_sampler(SamplerHandle handle) {
    auto it = m_gl_samplers.find(handle.id);
    if (it != m_gl_samplers.end()) {
        if (glDeleteSamplers_ptr) glDeleteSamplers_ptr(1, &it->second);
        m_gl_samplers.erase(it);
    }
}

void OpenGLBackend::destroy_shader_module(ShaderModuleHandle handle) {
    m_shader_modules.erase(handle.id);
}

void OpenGLBackend::destroy_pipeline(PipelineHandle handle) {
    auto it = m_gl_programs.find(handle.id);
    if (it != m_gl_programs.end()) {
        if (glDeleteProgram_ptr) glDeleteProgram_ptr(it->second);
        m_gl_programs.erase(it);
    }
}

void OpenGLBackend::write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) {
    auto it = m_gl_buffers.find(handle.id);
    if (it == m_gl_buffers.end() || !glBindBuffer_ptr || !glBufferSubData_ptr) return;

    GLenum target = m_buffer_targets[handle.id];
    glBindBuffer_ptr(target, it->second);
    glBufferSubData_ptr(target, offset, size, data);
    glBindBuffer_ptr(target, 0);
}

void* OpenGLBackend::map_buffer(BufferHandle handle, std::size_t offset, std::size_t size) {
    auto it = m_gl_buffers.find(handle.id);
    if (it == m_gl_buffers.end() || !glBindBuffer_ptr || !glMapBufferRange_ptr) return nullptr;

    GLenum target = m_buffer_targets[handle.id];
    glBindBuffer_ptr(target, it->second);
    void* ptr = glMapBufferRange_ptr(target, offset, size, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    return ptr;
}

void OpenGLBackend::unmap_buffer(BufferHandle handle) {
    auto it = m_gl_buffers.find(handle.id);
    if (it == m_gl_buffers.end() || !glBindBuffer_ptr || !glUnmapBuffer_ptr) return;

    GLenum target = m_buffer_targets[handle.id];
    glBindBuffer_ptr(target, it->second);
    glUnmapBuffer_ptr(target);
    glBindBuffer_ptr(target, 0);
}

void OpenGLBackend::write_texture(TextureHandle handle, const void* data, std::size_t,
                                   std::uint32_t mip_level, std::uint32_t) {
    auto it = m_gl_textures.find(handle.id);
    if (it == m_gl_textures.end()) return;

    GLenum target = m_texture_targets[handle.id];
    glBindTexture(target, it->second);

    // Would need texture desc to determine format and size
    // glTexSubImage2D(target, mip_level, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    (void)mip_level;
    (void)data;

    glBindTexture(target, 0);
}

void OpenGLBackend::generate_mipmaps(TextureHandle handle) {
    auto it = m_gl_textures.find(handle.id);
    if (it == m_gl_textures.end() || !glGenerateMipmap_ptr) return;

    GLenum target = m_texture_targets[handle.id];
    glBindTexture(target, it->second);
    glGenerateMipmap_ptr(target);
    glBindTexture(target, 0);
}

BackendError OpenGLBackend::begin_frame() {
    m_frame_number++;
    return BackendError::None;
}

BackendError OpenGLBackend::end_frame() {
    return BackendError::None;
}

void OpenGLBackend::present() {
    // Swap is handled by presenter/window system
}

void OpenGLBackend::wait_idle() {
    glFinish();
}

void OpenGLBackend::resize(std::uint32_t width, std::uint32_t height) {
    m_config.initial_width = width;
    m_config.initial_height = height;
    glViewport(0, 0, width, height);
}

RehydrationState OpenGLBackend::get_rehydration_state() const {
    RehydrationState state;
    state.width = m_config.initial_width;
    state.height = m_config.initial_height;
    state.fullscreen = m_config.fullscreen;
    state.vsync = m_config.vsync;
    state.frame_count = m_frame_number;
    return state;
}

BackendError OpenGLBackend::rehydrate(const RehydrationState& state) {
    resize(state.width, state.height);
    m_config.fullscreen = state.fullscreen;
    m_config.vsync = state.vsync;
    m_frame_number = state.frame_count;
    return BackendError::None;
}

FrameTiming OpenGLBackend::get_frame_timing() const {
    FrameTiming timing;
    timing.frame_number = m_frame_number;
    return timing;
}

std::uint64_t OpenGLBackend::get_allocated_memory() const {
    // OpenGL doesn't provide easy memory tracking
    return 0;
}

bool OpenGLBackend::load_gl_functions() {
#ifdef VOID_PLATFORM_WINDOWS
    #define LOAD_GL(name) name##_ptr = (decltype(name##_ptr))wglGetProcAddress(#name)
#elif defined(VOID_PLATFORM_LINUX)
    #define LOAD_GL(name) name##_ptr = (decltype(name##_ptr))glXGetProcAddress((const GLubyte*)#name)
#else
    #define LOAD_GL(name) name##_ptr = nullptr
#endif

    LOAD_GL(glGenBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glDeleteBuffers);
    LOAD_GL(glMapBufferRange);
    LOAD_GL(glUnmapBuffer);
    LOAD_GL(glBufferSubData);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glGenerateMipmap);
    LOAD_GL(glGenSamplers);
    LOAD_GL(glDeleteSamplers);
    LOAD_GL(glSamplerParameteri);
    LOAD_GL(glSamplerParameterf);

#undef LOAD_GL

    return glGenBuffers_ptr != nullptr;
}

void OpenGLBackend::query_capabilities() {
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    m_capabilities.device_name = renderer ? renderer : "Unknown";
    m_capabilities.driver_version = version ? version : "Unknown";

    // Query limits
    GLint max_tex_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
    m_capabilities.limits.max_texture_size_2d = max_tex_size;

    GLint max_uniform_size = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_size);
    m_capabilities.limits.max_uniform_buffer_size = max_uniform_size;

    // Query features
    m_capabilities.features.compute_shaders = true;  // GL 4.3+
    m_capabilities.features.multi_draw_indirect = true;  // GL 4.3+
    m_capabilities.features.sampler_anisotropy = true;
    m_capabilities.features.texture_compression_bc = true;
}

GLenum OpenGLBackend::texture_format_to_gl_internal(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm: return GL_R8;
        case TextureFormat::Rg8Unorm: return GL_RG8;
        case TextureFormat::Rgba8Unorm: return GL_RGBA8;
        case TextureFormat::Rgba8UnormSrgb: return GL_SRGB8_ALPHA8;
        case TextureFormat::Bgra8Unorm: return GL_RGBA8;
        case TextureFormat::Rgba16Float: return GL_RGBA16F;
        case TextureFormat::Rgba32Float: return GL_RGBA32F;
        case TextureFormat::Depth16Unorm: return GL_DEPTH_COMPONENT16;
        case TextureFormat::Depth24Plus: return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth32Float: return GL_DEPTH_COMPONENT32F;
        case TextureFormat::Depth24PlusStencil8: return GL_DEPTH24_STENCIL8;
        default: return GL_RGBA8;
    }
}

GLenum OpenGLBackend::texture_format_to_gl_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm: return GL_RED;
        case TextureFormat::Rg8Unorm: return GL_RG;
        case TextureFormat::Rgba8Unorm:
        case TextureFormat::Rgba8UnormSrgb:
        case TextureFormat::Rgba16Float:
        case TextureFormat::Rgba32Float: return GL_RGBA;
        case TextureFormat::Bgra8Unorm: return GL_BGRA;
        case TextureFormat::Depth16Unorm:
        case TextureFormat::Depth24Plus:
        case TextureFormat::Depth32Float: return GL_DEPTH_COMPONENT;
        case TextureFormat::Depth24PlusStencil8: return GL_DEPTH_STENCIL;
        default: return GL_RGBA;
    }
}

GLenum OpenGLBackend::texture_format_to_gl_type(TextureFormat format) {
    switch (format) {
        case TextureFormat::Rgba16Float: return GL_HALF_FLOAT;
        case TextureFormat::Rgba32Float: return GL_FLOAT;
        case TextureFormat::Depth16Unorm: return GL_UNSIGNED_SHORT;
        case TextureFormat::Depth32Float: return GL_FLOAT;
        case TextureFormat::Depth24PlusStencil8: return GL_UNSIGNED_INT_24_8;
        default: return GL_UNSIGNED_BYTE;
    }
}

} // namespace backends
} // namespace void_render
