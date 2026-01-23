/// @file backend.cpp
/// @brief Multi-backend GPU abstraction implementation

#include "void_engine/render/backend.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

// Platform detection
#ifdef _WIN32
    #define VOID_PLATFORM_WINDOWS 1
    #define NOMINMAX
    #include <Windows.h>
    #include <GL/gl.h>
    // Vulkan header would be included here
    // #include <vulkan/vulkan.h>
#elif defined(__APPLE__)
    #define VOID_PLATFORM_MACOS 1
    // Metal headers would be included here
#elif defined(__linux__)
    #define VOID_PLATFORM_LINUX 1
    #include <GL/gl.h>
    #include <GL/glx.h>
    // Check for DRM/KMS
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
#elif defined(__EMSCRIPTEN__)
    #define VOID_PLATFORM_WEB 1
#endif

namespace void_render {

// =============================================================================
// Backend Detection
// =============================================================================

namespace {

#ifdef VOID_PLATFORM_WINDOWS
bool check_vulkan_available() {
    // Try to load Vulkan DLL
    HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
    if (vulkan) {
        FreeLibrary(vulkan);
        return true;
    }
    return false;
}

bool check_d3d12_available() {
    // Try to load D3D12 DLL
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    if (d3d12) {
        FreeLibrary(d3d12);
        return true;
    }
    return false;
}

bool check_opengl_available() {
    // OpenGL is always available on Windows via opengl32.dll
    return true;
}
#endif

#ifdef VOID_PLATFORM_LINUX
bool check_vulkan_available() {
    // Try to dlopen libvulkan
    void* vulkan = dlopen("libvulkan.so.1", RTLD_LAZY);
    if (vulkan) {
        dlclose(vulkan);
        return true;
    }
    return false;
}

bool check_opengl_available() {
    // Check for libGL
    void* gl = dlopen("libGL.so.1", RTLD_LAZY);
    if (gl) {
        dlclose(gl);
        return true;
    }
    return false;
}

bool check_drm_available() {
    // Check for DRM device
    return std::filesystem::exists("/dev/dri/card0");
}

bool check_wayland_available() {
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
}

bool check_x11_available() {
    return std::getenv("DISPLAY") != nullptr;
}
#endif

#ifdef VOID_PLATFORM_MACOS
bool check_metal_available() {
    // Metal is always available on macOS 10.11+
    return true;
}
#endif

#ifdef VOID_PLATFORM_WEB
bool check_webgpu_available() {
    // WebGPU availability is determined at runtime in browser
    return true;
}
#endif

} // anonymous namespace

std::vector<BackendAvailability> detect_available_backends() {
    std::vector<BackendAvailability> result;

#ifdef VOID_PLATFORM_WINDOWS
    // Check Vulkan
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Vulkan;
        ba.available = check_vulkan_available();
        ba.reason = ba.available ? "" : "vulkan-1.dll not found";
        result.push_back(ba);
    }

    // Check D3D12
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Direct3D12;
        ba.available = check_d3d12_available();
        ba.reason = ba.available ? "" : "d3d12.dll not found";
        result.push_back(ba);
    }

    // Check OpenGL
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::OpenGL;
        ba.available = check_opengl_available();
        ba.reason = ba.available ? "" : "OpenGL not available";
        result.push_back(ba);
    }
#endif

#ifdef VOID_PLATFORM_LINUX
    // Check Vulkan
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Vulkan;
        ba.available = check_vulkan_available();
        ba.reason = ba.available ? "" : "libvulkan.so.1 not found";
        result.push_back(ba);
    }

    // Check OpenGL
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::OpenGL;
        ba.available = check_opengl_available();
        ba.reason = ba.available ? "" : "libGL.so.1 not found";
        result.push_back(ba);
    }
#endif

#ifdef VOID_PLATFORM_MACOS
    // Check Metal
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Metal;
        ba.available = check_metal_available();
        ba.reason = ba.available ? "" : "Metal not available";
        result.push_back(ba);
    }
#endif

#ifdef VOID_PLATFORM_WEB
    // Check WebGPU
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::WebGPU;
        ba.available = check_webgpu_available();
        ba.reason = ba.available ? "" : "WebGPU not available";
        result.push_back(ba);
    }
#endif

    // Null backend always available
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Null;
        ba.available = true;
        result.push_back(ba);
    }

    return result;
}

GpuBackend select_gpu_backend(const BackendConfig& config,
                               const std::vector<BackendAvailability>& available) {
    // If specific backend requested
    if (config.preferred_gpu_backend != GpuBackend::Auto) {
        for (const auto& ba : available) {
            if (ba.gpu_backend == config.preferred_gpu_backend) {
                if (ba.available) {
                    return ba.gpu_backend;
                } else if (config.gpu_selector == BackendSelector::Require) {
                    return GpuBackend::Null;  // Required but not available
                }
                break;
            }
        }
    }

    // Auto-select priority order
    // Windows: Vulkan > D3D12 > OpenGL
    // Linux: Vulkan > OpenGL
    // macOS: Metal
    // Web: WebGPU

    std::vector<GpuBackend> priority;

#ifdef VOID_PLATFORM_WINDOWS
    priority = {GpuBackend::Vulkan, GpuBackend::Direct3D12, GpuBackend::OpenGL};
#elif defined(VOID_PLATFORM_LINUX)
    priority = {GpuBackend::Vulkan, GpuBackend::OpenGL};
#elif defined(VOID_PLATFORM_MACOS)
    priority = {GpuBackend::Metal};
#elif defined(VOID_PLATFORM_WEB)
    priority = {GpuBackend::WebGPU};
#else
    priority = {GpuBackend::OpenGL};
#endif

    for (GpuBackend backend : priority) {
        for (const auto& ba : available) {
            if (ba.gpu_backend == backend && ba.available) {
                return backend;
            }
        }
    }

    return GpuBackend::Null;
}

// =============================================================================
// Null Backend Implementation (for testing/headless)
// =============================================================================

class NullBackend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
        if (m_initialized) return BackendError::AlreadyInitialized;

        m_capabilities.gpu_backend = GpuBackend::Null;
        m_capabilities.display_backend = DisplayBackend::Headless;
        m_capabilities.device_name = "Null Device";
        m_capabilities.driver_version = "1.0.0";

        m_config = config;
        m_initialized = true;
        return BackendError::None;
    }

    void shutdown() override {
        m_initialized = false;
        m_buffers.clear();
        m_textures.clear();
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Null; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
        BufferHandle handle{++m_next_handle};
        m_buffers[handle.id] = std::vector<std::uint8_t>(desc.size, 0);
        return handle;
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        TextureHandle handle{++m_next_handle};
        // Calculate size based on format and dimensions
        std::size_t size = desc.width * desc.height * desc.depth_or_layers * 4;  // Assume RGBA8
        m_textures[handle.id] = std::vector<std::uint8_t>(size, 0);
        return handle;
    }

    SamplerHandle create_sampler(const SamplerDesc&) override {
        return SamplerHandle{++m_next_handle};
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc&) override {
        return ShaderModuleHandle{++m_next_handle};
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    void destroy_buffer(BufferHandle handle) override {
        m_buffers.erase(handle.id);
    }

    void destroy_texture(TextureHandle handle) override {
        m_textures.erase(handle.id);
    }

    void destroy_sampler(SamplerHandle) override {}
    void destroy_shader_module(ShaderModuleHandle) override {}
    void destroy_pipeline(PipelineHandle) override {}

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end() && offset + size <= it->second.size()) {
            std::memcpy(it->second.data() + offset, data, size);
        }
    }

    void* map_buffer(BufferHandle handle, std::size_t offset, std::size_t) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end() && offset < it->second.size()) {
            return it->second.data() + offset;
        }
        return nullptr;
    }

    void unmap_buffer(BufferHandle) override {}

    void write_texture(TextureHandle handle, const void* data, std::size_t size,
                       std::uint32_t, std::uint32_t) override {
        auto it = m_textures.find(handle.id);
        if (it != m_textures.end()) {
            std::size_t copy_size = std::min(size, it->second.size());
            std::memcpy(it->second.data(), data, copy_size);
        }
    }

    void generate_mipmaps(TextureHandle) override {}

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override {
        return BackendError::None;
    }

    void present() override {}
    void wait_idle() override {}

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.fullscreen = m_config.fullscreen;
        state.vsync = m_config.vsync;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        m_config.initial_width = state.width;
        m_config.initial_height = state.height;
        m_config.fullscreen = state.fullscreen;
        m_config.vsync = state.vsync;
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        std::uint64_t total = 0;
        for (const auto& [id, buf] : m_buffers) {
            total += buf.size();
        }
        for (const auto& [id, tex] : m_textures) {
            total += tex.size();
        }
        return total;
    }

private:
    bool m_initialized = false;
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> m_buffers;
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> m_textures;
};

// =============================================================================
// OpenGL Backend Implementation
// =============================================================================

// OpenGL function pointer types
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

typedef void (*PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void* (*PFNGLMAPBUFFERRANGEPROC)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
typedef GLboolean (*PFNGLUNMAPBUFFERPROC)(GLenum);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (*PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (*PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
typedef void (*PFNGLGENERATEMIPMAPPROC)(GLenum);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef const GLubyte* (*PFNGLGETSTRINGIPROC)(GLenum, GLuint);

class OpenGLBackend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
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

    void shutdown() override {
        // Destroy all resources
        for (auto& [id, handle] : m_gl_buffers) {
            glDeleteBuffers(1, &handle);
        }
        m_gl_buffers.clear();

        for (auto& [id, handle] : m_gl_textures) {
            glDeleteTextures(1, &handle);
        }
        m_gl_textures.clear();

        for (auto& [id, handle] : m_gl_samplers) {
            glDeleteSamplers(1, &handle);
        }
        m_gl_samplers.clear();

        for (auto& [id, handle] : m_gl_programs) {
            if (glDeleteProgram_ptr) glDeleteProgram_ptr(handle);
        }
        m_gl_programs.clear();

        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::OpenGL; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
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

    TextureHandle create_texture(const TextureDesc& desc) override {
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

    SamplerHandle create_sampler(const SamplerDesc& desc) override {
        GLuint sampler = 0;
        glGenSamplers(1, &sampler);

        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
            desc.min_filter == SamplerDesc::Filter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER,
            desc.mag_filter == SamplerDesc::Filter::Nearest ? GL_NEAREST : GL_LINEAR);

        auto address_to_gl = [](SamplerDesc::AddressMode mode) -> GLint {
            switch (mode) {
                case SamplerDesc::AddressMode::Repeat: return GL_REPEAT;
                case SamplerDesc::AddressMode::MirrorRepeat: return GL_MIRRORED_REPEAT;
                case SamplerDesc::AddressMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
                case SamplerDesc::AddressMode::ClampToBorder: return GL_CLAMP_TO_BORDER;
                default: return GL_REPEAT;
            }
        };

        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, address_to_gl(desc.address_mode_u));
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, address_to_gl(desc.address_mode_v));
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, address_to_gl(desc.address_mode_w));

        if (desc.max_anisotropy > 1.0f) {
            glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.max_anisotropy);
        }

        SamplerHandle handle{++m_next_handle};
        m_gl_samplers[handle.id] = sampler;
        return handle;
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) override {
        // Note: OpenGL uses GLSL, would need SPIRV-Cross to convert SPIR-V
        // For now, store the SPIR-V and convert on pipeline creation
        ShaderModuleHandle handle{++m_next_handle};
        m_shader_modules[handle.id] = desc;
        return handle;
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc& desc) override {
        // Create program from shaders
        GLuint program = glCreateProgram_ptr();

        // Compile and attach vertex shader
        if (m_shader_modules.count(desc.vertex_shader.id)) {
            // Would convert SPIR-V to GLSL here
            // For now, create stub
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

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc& desc) override {
        GLuint program = glCreateProgram_ptr();

        // Would compile compute shader here

        glLinkProgram_ptr(program);

        PipelineHandle handle{++m_next_handle};
        m_gl_programs[handle.id] = program;
        return handle;
    }

    void destroy_buffer(BufferHandle handle) override {
        auto it = m_gl_buffers.find(handle.id);
        if (it != m_gl_buffers.end()) {
            glDeleteBuffers(1, &it->second);
            m_gl_buffers.erase(it);
            m_buffer_targets.erase(handle.id);
        }
    }

    void destroy_texture(TextureHandle handle) override {
        auto it = m_gl_textures.find(handle.id);
        if (it != m_gl_textures.end()) {
            glDeleteTextures(1, &it->second);
            m_gl_textures.erase(it);
            m_texture_targets.erase(handle.id);
        }
    }

    void destroy_sampler(SamplerHandle handle) override {
        auto it = m_gl_samplers.find(handle.id);
        if (it != m_gl_samplers.end()) {
            glDeleteSamplers(1, &it->second);
            m_gl_samplers.erase(it);
        }
    }

    void destroy_shader_module(ShaderModuleHandle handle) override {
        m_shader_modules.erase(handle.id);
    }

    void destroy_pipeline(PipelineHandle handle) override {
        auto it = m_gl_programs.find(handle.id);
        if (it != m_gl_programs.end()) {
            glDeleteProgram_ptr(it->second);
            m_gl_programs.erase(it);
        }
    }

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        auto it = m_gl_buffers.find(handle.id);
        if (it == m_gl_buffers.end()) return;

        GLenum target = m_buffer_targets[handle.id];
        glBindBuffer_ptr(target, it->second);
        glBufferSubData_ptr(target, offset, size, data);
        glBindBuffer_ptr(target, 0);
    }

    void* map_buffer(BufferHandle handle, std::size_t offset, std::size_t size) override {
        auto it = m_gl_buffers.find(handle.id);
        if (it == m_gl_buffers.end()) return nullptr;

        GLenum target = m_buffer_targets[handle.id];
        glBindBuffer_ptr(target, it->second);
        void* ptr = glMapBufferRange_ptr(target, offset, size, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
        return ptr;
    }

    void unmap_buffer(BufferHandle handle) override {
        auto it = m_gl_buffers.find(handle.id);
        if (it == m_gl_buffers.end()) return;

        GLenum target = m_buffer_targets[handle.id];
        glBindBuffer_ptr(target, it->second);
        glUnmapBuffer_ptr(target);
        glBindBuffer_ptr(target, 0);
    }

    void write_texture(TextureHandle handle, const void* data, std::size_t,
                       std::uint32_t mip_level, std::uint32_t) override {
        auto it = m_gl_textures.find(handle.id);
        if (it == m_gl_textures.end()) return;

        GLenum target = m_texture_targets[handle.id];
        glBindTexture(target, it->second);

        // Would need texture desc to determine format and size
        // Simplified: assume RGBA8
        // glTexSubImage2D(target, mip_level, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glBindTexture(target, 0);
    }

    void generate_mipmaps(TextureHandle handle) override {
        auto it = m_gl_textures.find(handle.id);
        if (it == m_gl_textures.end()) return;

        GLenum target = m_texture_targets[handle.id];
        glBindTexture(target, it->second);
        glGenerateMipmap_ptr(target);
        glBindTexture(target, 0);
    }

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override {
        return BackendError::None;
    }

    void present() override {
        // Swap is handled by presenter/window system
    }

    void wait_idle() override {
        glFinish();
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
        glViewport(0, 0, width, height);
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.fullscreen = m_config.fullscreen;
        state.vsync = m_config.vsync;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        resize(state.width, state.height);
        m_config.fullscreen = state.fullscreen;
        m_config.vsync = state.vsync;
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        // OpenGL doesn't provide easy memory tracking
        return 0;
    }

private:
    bool m_initialized = false;
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    std::unordered_map<std::uint64_t, GLuint> m_gl_buffers;
    std::unordered_map<std::uint64_t, GLenum> m_buffer_targets;
    std::unordered_map<std::uint64_t, GLuint> m_gl_textures;
    std::unordered_map<std::uint64_t, GLenum> m_texture_targets;
    std::unordered_map<std::uint64_t, GLuint> m_gl_samplers;
    std::unordered_map<std::uint64_t, GLuint> m_gl_programs;
    std::unordered_map<std::uint64_t, ShaderModuleDesc> m_shader_modules;

    // GL function pointers
    PFNGLGENBUFFERSPROC glGenBuffers_ptr = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer_ptr = nullptr;
    PFNGLBUFFERDATAPROC glBufferData_ptr = nullptr;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers_ptr = nullptr;
    PFNGLMAPBUFFERRANGEPROC glMapBufferRange_ptr = nullptr;
    PFNGLUNMAPBUFFERPROC glUnmapBuffer_ptr = nullptr;
    PFNGLBUFFERSUBDATAPROC glBufferSubData_ptr = nullptr;
    PFNGLCREATEPROGRAMPROC glCreateProgram_ptr = nullptr;
    PFNGLLINKPROGRAMPROC glLinkProgram_ptr = nullptr;
    PFNGLGETPROGRAMIVPROC glGetProgramiv_ptr = nullptr;
    PFNGLDELETEPROGRAMPROC glDeleteProgram_ptr = nullptr;
    PFNGLGENERATEMIPMAPPROC glGenerateMipmap_ptr = nullptr;

    bool load_gl_functions() {
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

#undef LOAD_GL

        return glGenBuffers_ptr != nullptr;
    }

    void query_capabilities() {
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

    static GLenum texture_format_to_gl_internal(TextureFormat format) {
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

    static GLenum texture_format_to_gl_format(TextureFormat format) {
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

    static GLenum texture_format_to_gl_type(TextureFormat format) {
        switch (format) {
            case TextureFormat::Rgba16Float: return GL_HALF_FLOAT;
            case TextureFormat::Rgba32Float: return GL_FLOAT;
            case TextureFormat::Depth16Unorm: return GL_UNSIGNED_SHORT;
            case TextureFormat::Depth32Float: return GL_FLOAT;
            case TextureFormat::Depth24PlusStencil8: return GL_UNSIGNED_INT_24_8;
            default: return GL_UNSIGNED_BYTE;
        }
    }
};

// =============================================================================
// Vulkan Backend Implementation
// =============================================================================

#if defined(VOID_PLATFORM_WINDOWS) || defined(VOID_PLATFORM_LINUX)

// Vulkan types and constants
#ifndef VK_API_VERSION_1_3
#define VK_API_VERSION_1_3 0x00403000
#define VK_SUCCESS 0
#define VK_INCOMPLETE 5
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO 14
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define VK_QUEUE_GRAPHICS_BIT 0x00000001
#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 0x00000080
#define VK_BUFFER_USAGE_INDEX_BUFFER_BIT 0x00000040
#define VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 0x00000010
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x00000020
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 0x00000001
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x00000002
#define VK_FORMAT_R8G8B8A8_UNORM 37
#define VK_FORMAT_B8G8R8A8_UNORM 44
#define VK_FORMAT_D32_SFLOAT 126
#define VK_IMAGE_TYPE_2D 1
#define VK_IMAGE_TILING_OPTIMAL 0
#define VK_IMAGE_USAGE_SAMPLED_BIT 0x00000004
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#endif

typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;
typedef void* VkCommandPool;
typedef void* VkCommandBuffer;
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
typedef void* VkImage;
typedef void* VkImageView;
typedef void* VkSampler;
typedef void* VkShaderModule;
typedef void* VkPipeline;
typedef void* VkPipelineLayout;
typedef void* VkRenderPass;
typedef void* VkFramebuffer;
typedef void* VkDescriptorSetLayout;
typedef void* VkDescriptorPool;
typedef void* VkDescriptorSet;
typedef void* VkFence;
typedef void* VkSemaphore;
typedef std::uint32_t VkFlags;
typedef std::int32_t VkResult;
typedef std::uint64_t VkDeviceSize;

// Vulkan function pointer types
typedef VkResult (*PFN_vkCreateInstance)(const void*, const void*, VkInstance*);
typedef void (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, std::uint32_t*, VkPhysicalDevice*);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceFeatures)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, std::uint32_t*, void*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const void*, const void*, VkDevice*);
typedef void (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice, std::uint32_t, std::uint32_t, VkQueue*);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice, const void*, const void*, VkBuffer*);
typedef void (*PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const void*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const void*, const void*, VkDeviceMemory*);
typedef void (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const void*);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkFlags, void**);
typedef void (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
typedef VkResult (*PFN_vkCreateImage)(VkDevice, const void*, const void*, VkImage*);
typedef void (*PFN_vkDestroyImage)(VkDevice, VkImage, const void*);
typedef VkResult (*PFN_vkCreateImageView)(VkDevice, const void*, const void*, VkImageView*);
typedef void (*PFN_vkDestroyImageView)(VkDevice, VkImageView, const void*);
typedef VkResult (*PFN_vkCreateSampler)(VkDevice, const void*, const void*, VkSampler*);
typedef void (*PFN_vkDestroySampler)(VkDevice, VkSampler, const void*);
typedef VkResult (*PFN_vkCreateShaderModule)(VkDevice, const void*, const void*, VkShaderModule*);
typedef void (*PFN_vkDestroyShaderModule)(VkDevice, VkShaderModule, const void*);
typedef VkResult (*PFN_vkCreateGraphicsPipelines)(VkDevice, void*, std::uint32_t, const void*, const void*, VkPipeline*);
typedef VkResult (*PFN_vkCreateComputePipelines)(VkDevice, void*, std::uint32_t, const void*, const void*, VkPipeline*);
typedef void (*PFN_vkDestroyPipeline)(VkDevice, VkPipeline, const void*);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const void*, const void*, VkCommandPool*);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const void*, VkCommandBuffer*);
typedef void (*PFN_vkFreeCommandBuffers)(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const void*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue, std::uint32_t, const void*, VkFence);
typedef VkResult (*PFN_vkQueueWaitIdle)(VkQueue);
typedef VkResult (*PFN_vkDeviceWaitIdle)(VkDevice);
typedef VkResult (*PFN_vkCreateFence)(VkDevice, const void*, const void*, VkFence*);
typedef void (*PFN_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice, std::uint32_t, const VkFence*, std::uint32_t, std::uint64_t);
typedef VkResult (*PFN_vkResetFences)(VkDevice, std::uint32_t, const VkFence*);

class VulkanBackend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
        if (m_initialized) return BackendError::AlreadyInitialized;

        // Load Vulkan library
        if (!load_vulkan_library()) {
            return BackendError::UnsupportedBackend;
        }

        // Create Vulkan instance
        if (!create_instance(config)) {
            return BackendError::UnsupportedBackend;
        }

        // Select physical device
        if (!select_physical_device()) {
            destroy_instance();
            return BackendError::UnsupportedBackend;
        }

        // Create logical device
        if (!create_device()) {
            destroy_instance();
            return BackendError::UnsupportedBackend;
        }

        // Create command pool
        if (!create_command_pool()) {
            destroy_device();
            destroy_instance();
            return BackendError::UnsupportedBackend;
        }

        query_capabilities();
        m_config = config;
        m_capabilities.gpu_backend = GpuBackend::Vulkan;
        m_initialized = true;
        return BackendError::None;
    }

    void shutdown() override {
        if (!m_initialized) return;

        wait_idle();

        // Destroy all resources
        for (auto& [id, res] : m_buffers) {
            if (vkDestroyBuffer && res.buffer) vkDestroyBuffer(m_device, res.buffer, nullptr);
            if (vkFreeMemory && res.memory) vkFreeMemory(m_device, res.memory, nullptr);
        }
        m_buffers.clear();

        for (auto& [id, res] : m_textures) {
            if (vkDestroyImageView && res.view) vkDestroyImageView(m_device, res.view, nullptr);
            if (vkDestroyImage && res.image) vkDestroyImage(m_device, res.image, nullptr);
            if (vkFreeMemory && res.memory) vkFreeMemory(m_device, res.memory, nullptr);
        }
        m_textures.clear();

        for (auto& [id, sampler] : m_samplers) {
            if (vkDestroySampler && sampler) vkDestroySampler(m_device, sampler, nullptr);
        }
        m_samplers.clear();

        for (auto& [id, pipeline] : m_pipelines) {
            if (vkDestroyPipeline && pipeline) vkDestroyPipeline(m_device, pipeline, nullptr);
        }
        m_pipelines.clear();

        for (auto& [id, module] : m_shader_modules) {
            if (vkDestroyShaderModule && module) vkDestroyShaderModule(m_device, module, nullptr);
        }
        m_shader_modules.clear();

        if (vkDestroyCommandPool && m_command_pool) {
            vkDestroyCommandPool(m_device, m_command_pool, nullptr);
            m_command_pool = nullptr;
        }

        destroy_device();
        destroy_instance();
        unload_vulkan_library();

        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Vulkan; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
        if (!m_initialized) return BufferHandle::invalid();

        VulkanBuffer vk_buf;
        vk_buf.size = desc.size;

        // Create buffer
        struct VkBufferCreateInfo {
            std::uint32_t sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            const void* pNext = nullptr;
            VkFlags flags = 0;
            VkDeviceSize size = 0;
            VkFlags usage = 0;
            std::uint32_t sharingMode = 0;
            std::uint32_t queueFamilyIndexCount = 0;
            const std::uint32_t* pQueueFamilyIndices = nullptr;
        } create_info;

        create_info.size = desc.size;
        if (desc.usage & BufferUsage::Vertex) create_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (desc.usage & BufferUsage::Index) create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (desc.usage & BufferUsage::Uniform) create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (desc.usage & BufferUsage::Storage) create_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        if (!vkCreateBuffer || vkCreateBuffer(m_device, &create_info, nullptr, &vk_buf.buffer) != VK_SUCCESS) {
            return BufferHandle::invalid();
        }

        // Allocate and bind memory (simplified - real impl would query memory requirements)
        struct VkMemoryAllocateInfo {
            std::uint32_t sType = 46;  // VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
            const void* pNext = nullptr;
            VkDeviceSize allocationSize = 0;
            std::uint32_t memoryTypeIndex = 0;
        } alloc_info;

        alloc_info.allocationSize = desc.size;
        alloc_info.memoryTypeIndex = find_memory_type(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        if (!vkAllocateMemory || vkAllocateMemory(m_device, &alloc_info, nullptr, &vk_buf.memory) != VK_SUCCESS) {
            vkDestroyBuffer(m_device, vk_buf.buffer, nullptr);
            return BufferHandle::invalid();
        }

        if (vkBindBufferMemory) {
            vkBindBufferMemory(m_device, vk_buf.buffer, vk_buf.memory, 0);
        }

        BufferHandle handle{++m_next_handle};
        m_buffers[handle.id] = vk_buf;
        return handle;
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        if (!m_initialized) return TextureHandle::invalid();

        VulkanTexture vk_tex;
        vk_tex.width = desc.width;
        vk_tex.height = desc.height;
        vk_tex.format = desc.format;

        // Create image (simplified)
        struct VkImageCreateInfo {
            std::uint32_t sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            const void* pNext = nullptr;
            VkFlags flags = 0;
            std::uint32_t imageType = VK_IMAGE_TYPE_2D;
            std::uint32_t format = VK_FORMAT_R8G8B8A8_UNORM;
            std::uint32_t width = 1, height = 1, depth = 1;
            std::uint32_t mipLevels = 1;
            std::uint32_t arrayLayers = 1;
            std::uint32_t samples = 1;
            std::uint32_t tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            std::uint32_t sharingMode = 0;
            std::uint32_t queueFamilyIndexCount = 0;
            const std::uint32_t* pQueueFamilyIndices = nullptr;
            std::uint32_t initialLayout = 0;
        } create_info;

        create_info.width = desc.width;
        create_info.height = desc.height;
        create_info.depth = desc.dimension == TextureDimension::D3 ? desc.depth_or_layers : 1;
        create_info.mipLevels = desc.mip_levels;
        create_info.arrayLayers = desc.dimension == TextureDimension::D2Array ? desc.depth_or_layers : 1;
        create_info.format = texture_format_to_vk(desc.format);

        if (!vkCreateImage || vkCreateImage(m_device, &create_info, nullptr, &vk_tex.image) != VK_SUCCESS) {
            return TextureHandle::invalid();
        }

        // Allocate memory and create image view (simplified)
        struct VkMemoryAllocateInfo {
            std::uint32_t sType = 46;
            const void* pNext = nullptr;
            VkDeviceSize allocationSize = 0;
            std::uint32_t memoryTypeIndex = 0;
        } alloc_info;

        alloc_info.allocationSize = desc.width * desc.height * 4;  // Simplified
        alloc_info.memoryTypeIndex = find_memory_type(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory) {
            vkAllocateMemory(m_device, &alloc_info, nullptr, &vk_tex.memory);
        }

        TextureHandle handle{++m_next_handle};
        m_textures[handle.id] = vk_tex;
        return handle;
    }

    SamplerHandle create_sampler(const SamplerDesc& desc) override {
        if (!m_initialized) return SamplerHandle::invalid();

        VkSampler sampler = nullptr;
        // Simplified sampler creation - real impl would use VkSamplerCreateInfo

        SamplerHandle handle{++m_next_handle};
        m_samplers[handle.id] = sampler;
        return handle;
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) override {
        if (!m_initialized || desc.spirv.empty()) return ShaderModuleHandle::invalid();

        struct VkShaderModuleCreateInfo {
            std::uint32_t sType = 16;  // VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
            const void* pNext = nullptr;
            VkFlags flags = 0;
            std::size_t codeSize = 0;
            const std::uint32_t* pCode = nullptr;
        } create_info;

        create_info.codeSize = desc.spirv.size() * sizeof(std::uint32_t);
        create_info.pCode = desc.spirv.data();

        VkShaderModule module = nullptr;
        if (!vkCreateShaderModule || vkCreateShaderModule(m_device, &create_info, nullptr, &module) != VK_SUCCESS) {
            return ShaderModuleHandle::invalid();
        }

        ShaderModuleHandle handle{++m_next_handle};
        m_shader_modules[handle.id] = module;
        return handle;
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc& desc) override {
        if (!m_initialized) return PipelineHandle::invalid();

        // Real implementation would create full graphics pipeline
        // Simplified: just store the pipeline handle
        VkPipeline pipeline = nullptr;

        PipelineHandle handle{++m_next_handle};
        m_pipelines[handle.id] = pipeline;
        return handle;
    }

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc& desc) override {
        if (!m_initialized) return PipelineHandle::invalid();

        VkPipeline pipeline = nullptr;

        PipelineHandle handle{++m_next_handle};
        m_pipelines[handle.id] = pipeline;
        return handle;
    }

    void destroy_buffer(BufferHandle handle) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end()) {
            if (vkDestroyBuffer && it->second.buffer) vkDestroyBuffer(m_device, it->second.buffer, nullptr);
            if (vkFreeMemory && it->second.memory) vkFreeMemory(m_device, it->second.memory, nullptr);
            m_buffers.erase(it);
        }
    }

    void destroy_texture(TextureHandle handle) override {
        auto it = m_textures.find(handle.id);
        if (it != m_textures.end()) {
            if (vkDestroyImageView && it->second.view) vkDestroyImageView(m_device, it->second.view, nullptr);
            if (vkDestroyImage && it->second.image) vkDestroyImage(m_device, it->second.image, nullptr);
            if (vkFreeMemory && it->second.memory) vkFreeMemory(m_device, it->second.memory, nullptr);
            m_textures.erase(it);
        }
    }

    void destroy_sampler(SamplerHandle handle) override {
        auto it = m_samplers.find(handle.id);
        if (it != m_samplers.end()) {
            if (vkDestroySampler && it->second) vkDestroySampler(m_device, it->second, nullptr);
            m_samplers.erase(it);
        }
    }

    void destroy_shader_module(ShaderModuleHandle handle) override {
        auto it = m_shader_modules.find(handle.id);
        if (it != m_shader_modules.end()) {
            if (vkDestroyShaderModule && it->second) vkDestroyShaderModule(m_device, it->second, nullptr);
            m_shader_modules.erase(it);
        }
    }

    void destroy_pipeline(PipelineHandle handle) override {
        auto it = m_pipelines.find(handle.id);
        if (it != m_pipelines.end()) {
            if (vkDestroyPipeline && it->second) vkDestroyPipeline(m_device, it->second, nullptr);
            m_pipelines.erase(it);
        }
    }

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        auto it = m_buffers.find(handle.id);
        if (it == m_buffers.end() || !it->second.memory) return;

        void* mapped = nullptr;
        if (vkMapMemory && vkMapMemory(m_device, it->second.memory, offset, size, 0, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, data, size);
            if (vkUnmapMemory) vkUnmapMemory(m_device, it->second.memory);
        }
    }

    void* map_buffer(BufferHandle handle, std::size_t offset, std::size_t size) override {
        auto it = m_buffers.find(handle.id);
        if (it == m_buffers.end() || !it->second.memory) return nullptr;

        void* mapped = nullptr;
        if (vkMapMemory && vkMapMemory(m_device, it->second.memory, offset, size, 0, &mapped) == VK_SUCCESS) {
            it->second.mapped = mapped;
            return mapped;
        }
        return nullptr;
    }

    void unmap_buffer(BufferHandle handle) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end() && it->second.memory && it->second.mapped) {
            if (vkUnmapMemory) vkUnmapMemory(m_device, it->second.memory);
            it->second.mapped = nullptr;
        }
    }

    void write_texture(TextureHandle handle, const void* data, std::size_t size,
                       std::uint32_t mip_level, std::uint32_t array_layer) override {
        // Real implementation would use staging buffer and vkCmdCopyBufferToImage
        (void)handle; (void)data; (void)size; (void)mip_level; (void)array_layer;
    }

    void generate_mipmaps(TextureHandle handle) override {
        // Real implementation would use vkCmdBlitImage
        (void)handle;
    }

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override {
        return BackendError::None;
    }

    void present() override {
        // Would use VkSwapchain
    }

    void wait_idle() override {
        if (vkDeviceWaitIdle && m_device) {
            vkDeviceWaitIdle(m_device);
        }
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
        // Would recreate swapchain
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.fullscreen = m_config.fullscreen;
        state.vsync = m_config.vsync;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        resize(state.width, state.height);
        m_config.fullscreen = state.fullscreen;
        m_config.vsync = state.vsync;
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        std::uint64_t total = 0;
        for (const auto& [id, buf] : m_buffers) {
            total += buf.size;
        }
        for (const auto& [id, tex] : m_textures) {
            total += tex.width * tex.height * 4;  // Approximate
        }
        return total;
    }

private:
    struct VulkanBuffer {
        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
        VkDeviceSize size = 0;
        void* mapped = nullptr;
    };

    struct VulkanTexture {
        VkImage image = nullptr;
        VkImageView view = nullptr;
        VkDeviceMemory memory = nullptr;
        std::uint32_t width = 0, height = 0;
        TextureFormat format = TextureFormat::Rgba8Unorm;
    };

    bool m_initialized = false;
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    // Vulkan objects
    VkInstance m_instance = nullptr;
    VkPhysicalDevice m_physical_device = nullptr;
    VkDevice m_device = nullptr;
    VkQueue m_graphics_queue = nullptr;
    VkQueue m_compute_queue = nullptr;
    VkCommandPool m_command_pool = nullptr;
    std::uint32_t m_graphics_family = 0;
    std::uint32_t m_compute_family = 0;

    // Resources
    std::unordered_map<std::uint64_t, VulkanBuffer> m_buffers;
    std::unordered_map<std::uint64_t, VulkanTexture> m_textures;
    std::unordered_map<std::uint64_t, VkSampler> m_samplers;
    std::unordered_map<std::uint64_t, VkShaderModule> m_shader_modules;
    std::unordered_map<std::uint64_t, VkPipeline> m_pipelines;

    // Memory type info
    std::uint32_t m_memory_type_count = 0;
    std::array<std::uint32_t, 32> m_memory_types;

    // Vulkan library handle
#ifdef _WIN32
    HMODULE m_vulkan_library = nullptr;
#else
    void* m_vulkan_library = nullptr;
#endif

    // Function pointers
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkCreateSampler vkCreateSampler = nullptr;
    PFN_vkDestroySampler vkDestroySampler = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;

    bool load_vulkan_library() {
#ifdef _WIN32
        m_vulkan_library = LoadLibraryA("vulkan-1.dll");
        if (!m_vulkan_library) return false;

        #define LOAD_VK(name) name = (PFN_##name)GetProcAddress(m_vulkan_library, #name)
#else
        m_vulkan_library = dlopen("libvulkan.so.1", RTLD_NOW);
        if (!m_vulkan_library) return false;

        #define LOAD_VK(name) name = (PFN_##name)dlsym(m_vulkan_library, #name)
#endif

        LOAD_VK(vkCreateInstance);
        LOAD_VK(vkDestroyInstance);
        LOAD_VK(vkEnumeratePhysicalDevices);
        LOAD_VK(vkGetPhysicalDeviceProperties);
        LOAD_VK(vkGetPhysicalDeviceFeatures);
        LOAD_VK(vkGetPhysicalDeviceMemoryProperties);
        LOAD_VK(vkGetPhysicalDeviceQueueFamilyProperties);
        LOAD_VK(vkCreateDevice);
        LOAD_VK(vkDestroyDevice);
        LOAD_VK(vkGetDeviceQueue);
        LOAD_VK(vkCreateBuffer);
        LOAD_VK(vkDestroyBuffer);
        LOAD_VK(vkAllocateMemory);
        LOAD_VK(vkFreeMemory);
        LOAD_VK(vkMapMemory);
        LOAD_VK(vkUnmapMemory);
        LOAD_VK(vkBindBufferMemory);
        LOAD_VK(vkCreateImage);
        LOAD_VK(vkDestroyImage);
        LOAD_VK(vkCreateImageView);
        LOAD_VK(vkDestroyImageView);
        LOAD_VK(vkCreateSampler);
        LOAD_VK(vkDestroySampler);
        LOAD_VK(vkCreateShaderModule);
        LOAD_VK(vkDestroyShaderModule);
        LOAD_VK(vkCreateGraphicsPipelines);
        LOAD_VK(vkCreateComputePipelines);
        LOAD_VK(vkDestroyPipeline);
        LOAD_VK(vkCreateCommandPool);
        LOAD_VK(vkDestroyCommandPool);
        LOAD_VK(vkAllocateCommandBuffers);
        LOAD_VK(vkFreeCommandBuffers);
        LOAD_VK(vkBeginCommandBuffer);
        LOAD_VK(vkEndCommandBuffer);
        LOAD_VK(vkQueueSubmit);
        LOAD_VK(vkQueueWaitIdle);
        LOAD_VK(vkDeviceWaitIdle);

        #undef LOAD_VK

        return vkCreateInstance != nullptr;
    }

    void unload_vulkan_library() {
#ifdef _WIN32
        if (m_vulkan_library) FreeLibrary(m_vulkan_library);
#else
        if (m_vulkan_library) dlclose(m_vulkan_library);
#endif
        m_vulkan_library = nullptr;
    }

    bool create_instance(const BackendConfig& config) {
        struct VkApplicationInfo {
            std::uint32_t sType = 0;
            const void* pNext = nullptr;
            const char* pApplicationName = "void_engine";
            std::uint32_t applicationVersion = 1;
            const char* pEngineName = "void_render";
            std::uint32_t engineVersion = 1;
            std::uint32_t apiVersion = VK_API_VERSION_1_3;
        } app_info;

        struct VkInstanceCreateInfo {
            std::uint32_t sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            const void* pNext = nullptr;
            VkFlags flags = 0;
            const VkApplicationInfo* pApplicationInfo = nullptr;
            std::uint32_t enabledLayerCount = 0;
            const char* const* ppEnabledLayerNames = nullptr;
            std::uint32_t enabledExtensionCount = 0;
            const char* const* ppEnabledExtensionNames = nullptr;
        } create_info;

        create_info.pApplicationInfo = reinterpret_cast<const VkApplicationInfo*>(&app_info);

        std::vector<const char*> layers;
        if (config.enable_validation) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }
        create_info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        create_info.ppEnabledLayerNames = layers.data();

        return vkCreateInstance && vkCreateInstance(&create_info, nullptr, &m_instance) == VK_SUCCESS;
    }

    void destroy_instance() {
        if (vkDestroyInstance && m_instance) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = nullptr;
        }
    }

    bool select_physical_device() {
        std::uint32_t device_count = 0;
        if (!vkEnumeratePhysicalDevices) return false;
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
        if (device_count == 0) return false;

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

        // Select first suitable device (real impl would score devices)
        m_physical_device = devices[0];

        // Get queue families
        std::uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);

        struct VkQueueFamilyProperties {
            VkFlags queueFlags = 0;
            std::uint32_t queueCount = 0;
            std::uint32_t timestampValidBits = 0;
            std::uint32_t minImageTransferGranularity[3] = {0, 0, 0};
        };

        std::vector<VkQueueFamilyProperties> families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, families.data());

        for (std::uint32_t i = 0; i < queue_family_count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_graphics_family = i;
            }
            if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_compute_family = i;
            }
        }

        // Get memory properties
        struct VkPhysicalDeviceMemoryProperties {
            std::uint32_t memoryTypeCount = 0;
            struct {
                VkFlags propertyFlags = 0;
                std::uint32_t heapIndex = 0;
            } memoryTypes[32];
            std::uint32_t memoryHeapCount = 0;
            struct {
                VkDeviceSize size = 0;
                VkFlags flags = 0;
            } memoryHeaps[16];
        } mem_props;

        if (vkGetPhysicalDeviceMemoryProperties) {
            vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_props);
            m_memory_type_count = mem_props.memoryTypeCount;
            for (std::uint32_t i = 0; i < m_memory_type_count; ++i) {
                m_memory_types[i] = mem_props.memoryTypes[i].propertyFlags;
            }
        }

        return true;
    }

    bool create_device() {
        float queue_priority = 1.0f;

        struct VkDeviceQueueCreateInfo {
            std::uint32_t sType = 2;  // VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
            const void* pNext = nullptr;
            VkFlags flags = 0;
            std::uint32_t queueFamilyIndex = 0;
            std::uint32_t queueCount = 1;
            const float* pQueuePriorities = nullptr;
        };

        std::vector<VkDeviceQueueCreateInfo> queue_infos;

        VkDeviceQueueCreateInfo graphics_queue_info;
        graphics_queue_info.queueFamilyIndex = m_graphics_family;
        graphics_queue_info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(graphics_queue_info);

        if (m_compute_family != m_graphics_family) {
            VkDeviceQueueCreateInfo compute_queue_info;
            compute_queue_info.queueFamilyIndex = m_compute_family;
            compute_queue_info.pQueuePriorities = &queue_priority;
            queue_infos.push_back(compute_queue_info);
        }

        struct VkDeviceCreateInfo {
            std::uint32_t sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            const void* pNext = nullptr;
            VkFlags flags = 0;
            std::uint32_t queueCreateInfoCount = 0;
            const VkDeviceQueueCreateInfo* pQueueCreateInfos = nullptr;
            std::uint32_t enabledLayerCount = 0;
            const char* const* ppEnabledLayerNames = nullptr;
            std::uint32_t enabledExtensionCount = 0;
            const char* const* ppEnabledExtensionNames = nullptr;
            const void* pEnabledFeatures = nullptr;
        } create_info;

        create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
        create_info.pQueueCreateInfos = queue_infos.data();

        if (!vkCreateDevice || vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
            return false;
        }

        // Get queues
        if (vkGetDeviceQueue) {
            vkGetDeviceQueue(m_device, m_graphics_family, 0, &m_graphics_queue);
            vkGetDeviceQueue(m_device, m_compute_family, 0, &m_compute_queue);
        }

        return true;
    }

    void destroy_device() {
        if (vkDestroyDevice && m_device) {
            vkDestroyDevice(m_device, nullptr);
            m_device = nullptr;
        }
    }

    bool create_command_pool() {
        struct VkCommandPoolCreateInfo {
            std::uint32_t sType = 39;  // VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
            const void* pNext = nullptr;
            VkFlags flags = 2;  // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            std::uint32_t queueFamilyIndex = 0;
        } create_info;

        create_info.queueFamilyIndex = m_graphics_family;

        return vkCreateCommandPool && vkCreateCommandPool(m_device, &create_info, nullptr, &m_command_pool) == VK_SUCCESS;
    }

    void query_capabilities() {
        struct VkPhysicalDeviceProperties {
            std::uint32_t apiVersion = 0;
            std::uint32_t driverVersion = 0;
            std::uint32_t vendorID = 0;
            std::uint32_t deviceID = 0;
            std::uint32_t deviceType = 0;
            char deviceName[256] = {};
            std::uint8_t pipelineCacheUUID[16] = {};
            // VkPhysicalDeviceLimits and VkPhysicalDeviceSparseProperties follow
        } props;

        if (vkGetPhysicalDeviceProperties) {
            vkGetPhysicalDeviceProperties(m_physical_device, &props);
            m_capabilities.device_name = props.deviceName;
            m_capabilities.vendor_id = props.vendorID;
            m_capabilities.device_id = props.deviceID;
        }

        m_capabilities.features.compute_shaders = true;
        m_capabilities.features.tessellation = true;
        m_capabilities.features.geometry_shaders = true;
        m_capabilities.features.multi_draw_indirect = true;
        m_capabilities.features.bindless_resources = true;
        m_capabilities.features.timeline_semaphores = true;
        m_capabilities.features.dynamic_rendering = true;
    }

    std::uint32_t find_memory_type(VkFlags required_flags) {
        for (std::uint32_t i = 0; i < m_memory_type_count; ++i) {
            if ((m_memory_types[i] & required_flags) == required_flags) {
                return i;
            }
        }
        return 0;
    }

    static std::uint32_t texture_format_to_vk(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8Unorm: return 9;   // VK_FORMAT_R8_UNORM
            case TextureFormat::Rg8Unorm: return 16;  // VK_FORMAT_R8G8_UNORM
            case TextureFormat::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case TextureFormat::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case TextureFormat::Rgba16Float: return 97;  // VK_FORMAT_R16G16B16A16_SFLOAT
            case TextureFormat::Rgba32Float: return 109; // VK_FORMAT_R32G32B32A32_SFLOAT
            case TextureFormat::Depth32Float: return VK_FORMAT_D32_SFLOAT;
            default: return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }
};

#endif // VOID_PLATFORM_WINDOWS || VOID_PLATFORM_LINUX

// =============================================================================
// Direct3D 12 Backend Implementation (Windows only)
// =============================================================================

#ifdef VOID_PLATFORM_WINDOWS

// D3D12 types
typedef void* ID3D12Device;
typedef void* ID3D12CommandQueue;
typedef void* ID3D12CommandAllocator;
typedef void* ID3D12GraphicsCommandList;
typedef void* ID3D12Resource;
typedef void* ID3D12DescriptorHeap;
typedef void* ID3D12RootSignature;
typedef void* ID3D12PipelineState;
typedef void* ID3D12Fence;
typedef void* IDXGIFactory4;
typedef void* IDXGISwapChain3;

class D3D12Backend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
        if (m_initialized) return BackendError::AlreadyInitialized;

        // Load D3D12 library
        m_d3d12_library = LoadLibraryA("d3d12.dll");
        if (!m_d3d12_library) return BackendError::UnsupportedBackend;

        m_dxgi_library = LoadLibraryA("dxgi.dll");
        if (!m_dxgi_library) {
            FreeLibrary(m_d3d12_library);
            return BackendError::UnsupportedBackend;
        }

        // Create D3D12 device
        if (!create_device(config)) {
            FreeLibrary(m_dxgi_library);
            FreeLibrary(m_d3d12_library);
            return BackendError::UnsupportedBackend;
        }

        m_config = config;
        m_capabilities.gpu_backend = GpuBackend::Direct3D12;
        m_capabilities.features.ray_tracing = true;  // D3D12 supports DXR
        m_capabilities.features.mesh_shaders = true;
        m_capabilities.features.variable_rate_shading = true;
        m_initialized = true;
        return BackendError::None;
    }

    void shutdown() override {
        if (!m_initialized) return;

        wait_idle();

        // Release all resources (COM Release pattern)
        m_buffers.clear();
        m_textures.clear();
        m_pipelines.clear();

        // Release device (would call IUnknown::Release)
        m_device = nullptr;
        m_command_queue = nullptr;

        if (m_dxgi_library) FreeLibrary(m_dxgi_library);
        if (m_d3d12_library) FreeLibrary(m_d3d12_library);

        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Direct3D12; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
        if (!m_initialized) return BufferHandle::invalid();

        D3D12Buffer d3d_buf;
        d3d_buf.size = desc.size;
        // Would call ID3D12Device::CreateCommittedResource

        BufferHandle handle{++m_next_handle};
        m_buffers[handle.id] = d3d_buf;
        return handle;
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        if (!m_initialized) return TextureHandle::invalid();

        D3D12Texture d3d_tex;
        d3d_tex.width = desc.width;
        d3d_tex.height = desc.height;
        // Would call ID3D12Device::CreateCommittedResource

        TextureHandle handle{++m_next_handle};
        m_textures[handle.id] = d3d_tex;
        return handle;
    }

    SamplerHandle create_sampler(const SamplerDesc&) override {
        return SamplerHandle{++m_next_handle};
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) override {
        // D3D12 uses DXBC/DXIL, would need to compile SPIR-V to DXIL
        ShaderModuleHandle handle{++m_next_handle};
        m_shaders[handle.id] = desc.spirv;
        return handle;
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc&) override {
        // Would call ID3D12Device::CreateGraphicsPipelineState
        return PipelineHandle{++m_next_handle};
    }

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc&) override {
        // Would call ID3D12Device::CreateComputePipelineState
        return PipelineHandle{++m_next_handle};
    }

    void destroy_buffer(BufferHandle handle) override { m_buffers.erase(handle.id); }
    void destroy_texture(TextureHandle handle) override { m_textures.erase(handle.id); }
    void destroy_sampler(SamplerHandle) override {}
    void destroy_shader_module(ShaderModuleHandle handle) override { m_shaders.erase(handle.id); }
    void destroy_pipeline(PipelineHandle handle) override { m_pipelines.erase(handle.id); }

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end() && it->second.mapped) {
            std::memcpy(static_cast<char*>(it->second.mapped) + offset, data, size);
        }
    }

    void* map_buffer(BufferHandle handle, std::size_t, std::size_t) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end()) {
            // Would call ID3D12Resource::Map
            return it->second.mapped;
        }
        return nullptr;
    }

    void unmap_buffer(BufferHandle handle) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end()) {
            // Would call ID3D12Resource::Unmap
            it->second.mapped = nullptr;
        }
    }

    void write_texture(TextureHandle, const void*, std::size_t, std::uint32_t, std::uint32_t) override {}
    void generate_mipmaps(TextureHandle) override {}

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override {
        return BackendError::None;
    }

    void present() override {}
    void wait_idle() override {}

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        resize(state.width, state.height);
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        std::uint64_t total = 0;
        for (const auto& [id, buf] : m_buffers) total += buf.size;
        return total;
    }

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
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
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

    bool create_device(const BackendConfig& config) {
        // Would use D3D12CreateDevice
        m_capabilities.device_name = "Direct3D 12 Device";
        m_capabilities.features.compute_shaders = true;
        m_capabilities.features.tessellation = true;
        m_capabilities.features.mesh_shaders = true;
        m_capabilities.features.ray_tracing = true;
        (void)config;
        return true;
    }
};

#endif // VOID_PLATFORM_WINDOWS

// =============================================================================
// Metal Backend Implementation (macOS/iOS only)
// =============================================================================

#ifdef VOID_PLATFORM_MACOS

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

class MetalBackend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
        if (m_initialized) return BackendError::AlreadyInitialized;

        // Metal initialization would use MTLCreateSystemDefaultDevice()
        // This requires Objective-C runtime, simplified here

        m_config = config;
        m_capabilities.gpu_backend = GpuBackend::Metal;
        m_capabilities.device_name = "Metal Device";
        m_capabilities.features.compute_shaders = true;
        m_capabilities.features.tessellation = true;  // Metal tessellation
        m_capabilities.features.mesh_shaders = true;  // Metal 3 mesh shaders
        m_capabilities.features.ray_tracing = true;   // Metal Ray Tracing
        m_initialized = true;
        return BackendError::None;
    }

    void shutdown() override {
        if (!m_initialized) return;
        wait_idle();
        m_buffers.clear();
        m_textures.clear();
        m_pipelines.clear();
        m_device = nullptr;
        m_command_queue = nullptr;
        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Metal; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
        if (!m_initialized) return BufferHandle::invalid();

        MetalBuffer mtl_buf;
        mtl_buf.size = desc.size;
        // Would call [device newBufferWithLength:options:]

        BufferHandle handle{++m_next_handle};
        m_buffers[handle.id] = mtl_buf;
        return handle;
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        if (!m_initialized) return TextureHandle::invalid();

        MetalTexture mtl_tex;
        mtl_tex.width = desc.width;
        mtl_tex.height = desc.height;
        // Would use MTLTextureDescriptor and [device newTextureWithDescriptor:]

        TextureHandle handle{++m_next_handle};
        m_textures[handle.id] = mtl_tex;
        return handle;
    }

    SamplerHandle create_sampler(const SamplerDesc&) override {
        return SamplerHandle{++m_next_handle};
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) override {
        // Metal uses MSL, would need SPIRV-Cross to convert
        ShaderModuleHandle handle{++m_next_handle};
        m_shaders[handle.id] = desc.spirv;
        return handle;
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    void destroy_buffer(BufferHandle handle) override { m_buffers.erase(handle.id); }
    void destroy_texture(TextureHandle handle) override { m_textures.erase(handle.id); }
    void destroy_sampler(SamplerHandle) override {}
    void destroy_shader_module(ShaderModuleHandle handle) override { m_shaders.erase(handle.id); }
    void destroy_pipeline(PipelineHandle handle) override { m_pipelines.erase(handle.id); }

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end() && it->second.contents) {
            std::memcpy(static_cast<char*>(it->second.contents) + offset, data, size);
        }
    }

    void* map_buffer(BufferHandle handle, std::size_t, std::size_t) override {
        auto it = m_buffers.find(handle.id);
        if (it != m_buffers.end()) {
            // Metal buffers with shared/managed storage are always mapped
            return it->second.contents;
        }
        return nullptr;
    }

    void unmap_buffer(BufferHandle) override {
        // Metal doesn't require explicit unmap for shared/managed storage
    }

    void write_texture(TextureHandle, const void*, std::size_t, std::uint32_t, std::uint32_t) override {}
    void generate_mipmaps(TextureHandle) override {}

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override {
        return BackendError::None;
    }

    void present() override {}
    void wait_idle() override {}

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        resize(state.width, state.height);
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        std::uint64_t total = 0;
        for (const auto& [id, buf] : m_buffers) total += buf.size;
        return total;
    }

private:
    struct MetalBuffer {
        MTLBuffer buffer = nullptr;
        std::uint64_t size = 0;
        void* contents = nullptr;
    };

    struct MetalTexture {
        MTLTexture texture = nullptr;
        std::uint32_t width = 0, height = 0;
    };

    bool m_initialized = false;
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    MTLDevice m_device = nullptr;
    MTLCommandQueue m_command_queue = nullptr;

    std::unordered_map<std::uint64_t, MetalBuffer> m_buffers;
    std::unordered_map<std::uint64_t, MetalTexture> m_textures;
    std::unordered_map<std::uint64_t, MTLRenderPipelineState> m_pipelines;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_shaders;
};

#endif // VOID_PLATFORM_MACOS

// =============================================================================
// WebGPU Backend Implementation
// =============================================================================

#ifdef VOID_PLATFORM_WEB

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

class WebGPUBackend : public IGpuBackend {
public:
    BackendError init(const BackendConfig& config) override {
        if (m_initialized) return BackendError::AlreadyInitialized;

        // WebGPU initialization via wgpu-native or Emscripten
        // Simplified for this implementation

        m_config = config;
        m_capabilities.gpu_backend = GpuBackend::WebGPU;
        m_capabilities.device_name = "WebGPU Device";
        m_capabilities.features.compute_shaders = true;
        m_initialized = true;
        return BackendError::None;
    }

    void shutdown() override {
        if (!m_initialized) return;
        m_buffers.clear();
        m_textures.clear();
        m_pipelines.clear();
        m_initialized = false;
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::WebGPU; }
    [[nodiscard]] const BackendCapabilities& capabilities() const override { return m_capabilities; }

    BufferHandle create_buffer(const BufferDesc& desc) override {
        if (!m_initialized) return BufferHandle::invalid();

        WebGPUBuffer wgpu_buf;
        wgpu_buf.size = desc.size;
        // Would use wgpuDeviceCreateBuffer

        BufferHandle handle{++m_next_handle};
        m_buffers[handle.id] = wgpu_buf;
        return handle;
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        if (!m_initialized) return TextureHandle::invalid();

        WebGPUTexture wgpu_tex;
        wgpu_tex.width = desc.width;
        wgpu_tex.height = desc.height;
        // Would use wgpuDeviceCreateTexture

        TextureHandle handle{++m_next_handle};
        m_textures[handle.id] = wgpu_tex;
        return handle;
    }

    SamplerHandle create_sampler(const SamplerDesc&) override {
        return SamplerHandle{++m_next_handle};
    }

    ShaderModuleHandle create_shader_module(const ShaderModuleDesc& desc) override {
        // WebGPU accepts SPIR-V directly
        ShaderModuleHandle handle{++m_next_handle};
        m_shaders[handle.id] = desc.spirv;
        return handle;
    }

    PipelineHandle create_render_pipeline(const RenderPipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    PipelineHandle create_compute_pipeline(const ComputePipelineDesc&) override {
        return PipelineHandle{++m_next_handle};
    }

    void destroy_buffer(BufferHandle handle) override { m_buffers.erase(handle.id); }
    void destroy_texture(TextureHandle handle) override { m_textures.erase(handle.id); }
    void destroy_sampler(SamplerHandle) override {}
    void destroy_shader_module(ShaderModuleHandle handle) override { m_shaders.erase(handle.id); }
    void destroy_pipeline(PipelineHandle handle) override { m_pipelines.erase(handle.id); }

    void write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override {
        // Would use wgpuQueueWriteBuffer
        (void)handle; (void)offset; (void)data; (void)size;
    }

    void* map_buffer(BufferHandle, std::size_t, std::size_t) override {
        // WebGPU uses async mapping, simplified here
        return nullptr;
    }

    void unmap_buffer(BufferHandle) override {}

    void write_texture(TextureHandle, const void*, std::size_t, std::uint32_t, std::uint32_t) override {}
    void generate_mipmaps(TextureHandle) override {}

    BackendError begin_frame() override {
        m_frame_number++;
        return BackendError::None;
    }

    BackendError end_frame() override { return BackendError::None; }
    void present() override {}
    void wait_idle() override {}

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.initial_width = width;
        m_config.initial_height = height;
    }

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_config.initial_width;
        state.height = m_config.initial_height;
        state.frame_count = m_frame_number;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        resize(state.width, state.height);
        m_frame_number = state.frame_count;
        return BackendError::None;
    }

    FrameTiming get_frame_timing() const override {
        FrameTiming timing;
        timing.frame_number = m_frame_number;
        return timing;
    }

    std::uint64_t get_allocated_memory() const override {
        std::uint64_t total = 0;
        for (const auto& [id, buf] : m_buffers) total += buf.size;
        return total;
    }

private:
    struct WebGPUBuffer {
        WGPUBuffer buffer = nullptr;
        std::uint64_t size = 0;
    };

    struct WebGPUTexture {
        WGPUTexture texture = nullptr;
        std::uint32_t width = 0, height = 0;
    };

    bool m_initialized = false;
    BackendCapabilities m_capabilities;
    BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;

    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;

    std::unordered_map<std::uint64_t, WebGPUBuffer> m_buffers;
    std::unordered_map<std::uint64_t, WebGPUTexture> m_textures;
    std::unordered_map<std::uint64_t, WGPURenderPipeline> m_pipelines;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_shaders;
};

#endif // VOID_PLATFORM_WEB

// =============================================================================
// Backend Factory
// =============================================================================

std::unique_ptr<IGpuBackend> create_backend(GpuBackend backend) {
    switch (backend) {
#if defined(VOID_PLATFORM_WINDOWS) || defined(VOID_PLATFORM_LINUX)
        case GpuBackend::Vulkan:
            return std::make_unique<VulkanBackend>();
#endif

#ifdef VOID_PLATFORM_WINDOWS
        case GpuBackend::Direct3D12:
            return std::make_unique<D3D12Backend>();
#endif

#ifdef VOID_PLATFORM_MACOS
        case GpuBackend::Metal:
            return std::make_unique<MetalBackend>();
#endif

#ifdef VOID_PLATFORM_WEB
        case GpuBackend::WebGPU:
            return std::make_unique<WebGPUBackend>();
#endif

        case GpuBackend::OpenGL:
            return std::make_unique<OpenGLBackend>();

        case GpuBackend::Null:
        case GpuBackend::Auto:
        default:
            return std::make_unique<NullBackend>();
    }
}

// =============================================================================
// Null Presenter Implementation
// =============================================================================

class NullPresenter : public IPresenter {
public:
    NullPresenter(PresenterId id, const BackendConfig& config)
        : m_id(id) {
        m_capabilities.backend = DisplayBackend::Headless;
        m_capabilities.width = config.initial_width;
        m_capabilities.height = config.initial_height;
    }

    PresenterId id() const override { return m_id; }
    const PresenterCapabilities& capabilities() const override { return m_capabilities; }

    BackendError resize(std::uint32_t width, std::uint32_t height) override {
        m_capabilities.width = width;
        m_capabilities.height = height;
        return BackendError::None;
    }

    BackendError set_fullscreen(bool) override { return BackendError::None; }
    BackendError set_vsync(bool) override { return BackendError::None; }

    TextureHandle acquire_next_texture() override {
        return TextureHandle{1};  // Dummy handle
    }

    void present(TextureHandle) override {}

    RehydrationState get_rehydration_state() const override {
        RehydrationState state;
        state.width = m_capabilities.width;
        state.height = m_capabilities.height;
        return state;
    }

    BackendError rehydrate(const RehydrationState& state) override {
        m_capabilities.width = state.width;
        m_capabilities.height = state.height;
        return BackendError::None;
    }

private:
    PresenterId m_id;
    PresenterCapabilities m_capabilities;
};

std::unique_ptr<IPresenter> create_presenter(DisplayBackend backend,
                                              IGpuBackend*,
                                              const BackendConfig& config) {
    // For now, return null presenter
    // Full implementation would create platform-specific presenters
    static PresenterId next_id = 1;
    return std::make_unique<NullPresenter>(next_id++, config);
}

// =============================================================================
// Backend Manager Implementation
// =============================================================================

BackendManager::~BackendManager() {
    shutdown();
}

BackendError BackendManager::init(const BackendConfig& config) {
    m_config = config;

    // Detect available backends
    auto available = detect_available_backends();

    // Select best backend
    GpuBackend selected = select_gpu_backend(config, available);
    if (selected == GpuBackend::Null && config.gpu_selector == BackendSelector::Require) {
        return BackendError::UnsupportedBackend;
    }

    // Create GPU backend
    m_gpu_backend = create_backend(selected);
    if (!m_gpu_backend) {
        return BackendError::UnsupportedBackend;
    }

    // Initialize GPU backend
    BackendError err = m_gpu_backend->init(config);
    if (err != BackendError::None) {
        m_gpu_backend.reset();
        return err;
    }

    // Create primary presenter
    DisplayBackend display_backend = config.preferred_display_backend;
    if (display_backend == DisplayBackend::Auto) {
#ifdef VOID_PLATFORM_WINDOWS
        display_backend = DisplayBackend::Win32;
#elif defined(VOID_PLATFORM_LINUX)
        if (check_wayland_available()) display_backend = DisplayBackend::Wayland;
        else if (check_x11_available()) display_backend = DisplayBackend::X11;
        else display_backend = DisplayBackend::Headless;
#elif defined(VOID_PLATFORM_MACOS)
        display_backend = DisplayBackend::Cocoa;
#elif defined(VOID_PLATFORM_WEB)
        display_backend = DisplayBackend::Web;
#else
        display_backend = DisplayBackend::Headless;
#endif
    }

    auto presenter = create_presenter(display_backend, m_gpu_backend.get(), config);
    if (presenter) {
        m_presenters.push_back(std::move(presenter));
    }

    return BackendError::None;
}

void BackendManager::shutdown() {
    m_presenters.clear();

    if (m_gpu_backend) {
        m_gpu_backend->shutdown();
        m_gpu_backend.reset();
    }
}

IPresenter* BackendManager::get_presenter(PresenterId id) const {
    for (const auto& p : m_presenters) {
        if (p->id() == id) return p.get();
    }
    return nullptr;
}

PresenterId BackendManager::add_presenter(DisplayBackend backend) {
    auto presenter = create_presenter(backend, m_gpu_backend.get(), m_config);
    if (!presenter) return 0;

    PresenterId id = presenter->id();
    m_presenters.push_back(std::move(presenter));
    return id;
}

void BackendManager::remove_presenter(PresenterId id) {
    m_presenters.erase(
        std::remove_if(m_presenters.begin(), m_presenters.end(),
            [id](const auto& p) { return p->id() == id; }),
        m_presenters.end());
}

const BackendCapabilities& BackendManager::capabilities() const {
    static BackendCapabilities empty;
    return m_gpu_backend ? m_gpu_backend->capabilities() : empty;
}

BackendError BackendManager::begin_frame() {
    if (!m_gpu_backend) return BackendError::NotInitialized;
    return m_gpu_backend->begin_frame();
}

BackendError BackendManager::end_frame() {
    if (!m_gpu_backend) return BackendError::NotInitialized;
    return m_gpu_backend->end_frame();
}

BackendError BackendManager::hot_swap_backend(GpuBackend new_backend) {
    if (!m_gpu_backend) return BackendError::NotInitialized;

    // Capture rehydration state
    RehydrationState state = m_gpu_backend->get_rehydration_state();

    // Shutdown old backend
    m_gpu_backend->shutdown();

    // Create new backend
    m_gpu_backend = create_backend(new_backend);
    if (!m_gpu_backend) {
        return BackendError::UnsupportedBackend;
    }

    // Initialize new backend
    BackendError err = m_gpu_backend->init(m_config);
    if (err != BackendError::None) {
        return err;
    }

    // Restore state
    return m_gpu_backend->rehydrate(state);
}

BackendError BackendManager::hot_swap_presenter(PresenterId id, DisplayBackend new_backend) {
    // Find existing presenter
    for (std::size_t i = 0; i < m_presenters.size(); ++i) {
        if (m_presenters[i]->id() == id) {
            // Capture state
            RehydrationState state = m_presenters[i]->get_rehydration_state();

            // Create new presenter
            auto new_presenter = create_presenter(new_backend, m_gpu_backend.get(), m_config);
            if (!new_presenter) {
                return BackendError::UnsupportedBackend;
            }

            // Restore state
            BackendError err = new_presenter->rehydrate(state);
            if (err != BackendError::None) {
                return err;
            }

            // Swap
            m_presenters[i] = std::move(new_presenter);
            return BackendError::None;
        }
    }

    return BackendError::InvalidHandle;
}

} // namespace void_render
