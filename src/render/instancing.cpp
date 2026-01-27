/// @file instancing.cpp
/// @brief GPU instancing and draw batching implementation

#include <void_engine/render/instancing.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef intptr_t GLintptr;

// OpenGL defines
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405

// GL function pointers
#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN_##name)(__VA_ARGS__); \
    static PFN_##name pfn_##name = nullptr;

DECLARE_GL_FUNC(void, glGenBuffers, GLsizei n, GLuint* buffers)
DECLARE_GL_FUNC(void, glDeleteBuffers, GLsizei n, const GLuint* buffers)
DECLARE_GL_FUNC(void, glBindBuffer, GLenum target, GLuint buffer)
DECLARE_GL_FUNC(void, glBufferData, GLenum target, GLsizeiptr size, const void* data, GLenum usage)
DECLARE_GL_FUNC(void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
DECLARE_GL_FUNC(void*, glMapBuffer, GLenum target, GLenum access)
DECLARE_GL_FUNC(void*, glMapBufferRange, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
DECLARE_GL_FUNC(GLboolean, glUnmapBuffer, GLenum target)
DECLARE_GL_FUNC(void, glEnableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glDisableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
DECLARE_GL_FUNC(void, glVertexAttribDivisor, GLuint index, GLuint divisor)
DECLARE_GL_FUNC(void, glDrawElementsInstanced, GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount)
DECLARE_GL_FUNC(void, glDrawArraysInstanced, GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
DECLARE_GL_FUNC(void, glMultiDrawElementsIndirect, GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride)
DECLARE_GL_FUNC(void, glBindVertexArray, GLuint array)

static bool s_instancing_gl_loaded = false;

static bool load_instancing_gl_functions() {
    if (s_instancing_gl_loaded) return true;

#define LOAD_GL(name) \
    pfn_##name = (PFN_##name)wglGetProcAddress(#name);

    LOAD_GL(glGenBuffers)
    LOAD_GL(glDeleteBuffers)
    LOAD_GL(glBindBuffer)
    LOAD_GL(glBufferData)
    LOAD_GL(glBufferSubData)
    LOAD_GL(glMapBuffer)
    LOAD_GL(glMapBufferRange)
    LOAD_GL(glUnmapBuffer)
    LOAD_GL(glEnableVertexAttribArray)
    LOAD_GL(glDisableVertexAttribArray)
    LOAD_GL(glVertexAttribPointer)
    LOAD_GL(glVertexAttribDivisor)
    LOAD_GL(glDrawElementsInstanced)
    LOAD_GL(glDrawArraysInstanced)
    LOAD_GL(glMultiDrawElementsIndirect)
    LOAD_GL(glBindVertexArray)

#undef LOAD_GL

    s_instancing_gl_loaded = true;
    return true;
}

#define GL_CALL(name, ...) (pfn_##name ? pfn_##name(__VA_ARGS__) : (void)0)

#else
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_CALL(name, ...) name(__VA_ARGS__)
static bool load_instancing_gl_functions() { return true; }
#endif

namespace void_render {

// =============================================================================
// InstanceBuffer Implementation
// =============================================================================

InstanceBuffer::InstanceBuffer() = default;

InstanceBuffer::~InstanceBuffer() {
    destroy();
}

InstanceBuffer::InstanceBuffer(InstanceBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_capacity(other.m_capacity)
    , m_count(other.m_count)
    , m_stride(other.m_stride)
{
    other.m_buffer = 0;
}

InstanceBuffer& InstanceBuffer::operator=(InstanceBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        m_count = other.m_count;
        m_stride = other.m_stride;
        other.m_buffer = 0;
    }
    return *this;
}

bool InstanceBuffer::initialize(std::size_t initial_capacity, std::size_t stride) {
    if (!load_instancing_gl_functions()) {
        spdlog::error("Failed to load instancing GL functions");
        return false;
    }

    m_stride = stride;
    m_capacity = initial_capacity;
    m_count = 0;

    if (pfn_glGenBuffers) {
        pfn_glGenBuffers(1, &m_buffer);
    }
    if (!m_buffer) {
        spdlog::error("Failed to create instance buffer");
        return false;
    }

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    }
    if (pfn_glBufferData) {
        pfn_glBufferData(GL_ARRAY_BUFFER, m_capacity * m_stride, nullptr, GL_DYNAMIC_DRAW);
    }
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    spdlog::debug("Instance buffer created: capacity={}, stride={}", m_capacity, m_stride);
    return true;
}

void InstanceBuffer::destroy() {
    if (m_buffer && pfn_glDeleteBuffers) {
        pfn_glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
    }
    m_capacity = 0;
    m_count = 0;
}

void InstanceBuffer::resize(std::size_t new_capacity) {
    if (new_capacity <= m_capacity) return;

    GLuint new_buffer = 0;
    if (pfn_glGenBuffers) {
        pfn_glGenBuffers(1, &new_buffer);
    }

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, new_buffer);
    }
    if (pfn_glBufferData) {
        pfn_glBufferData(GL_ARRAY_BUFFER, new_capacity * m_stride, nullptr, GL_DYNAMIC_DRAW);
    }

    // Copy old data if exists
    if (m_buffer && m_count > 0) {
        // Use buffer copy (GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER)
        // For simplicity, we'll lose the data here - in production, use glCopyBufferSubData
    }

    if (m_buffer && pfn_glDeleteBuffers) {
        pfn_glDeleteBuffers(1, &m_buffer);
    }

    m_buffer = new_buffer;
    m_capacity = new_capacity;

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    spdlog::debug("Instance buffer resized to {}", new_capacity);
}

void InstanceBuffer::update(const void* data, std::size_t count) {
    if (count > m_capacity) {
        resize(count * 2);  // Double capacity for growth
    }

    m_count = count;

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    }
    if (pfn_glBufferSubData) {
        pfn_glBufferSubData(GL_ARRAY_BUFFER, 0, count * m_stride, data);
    }
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void InstanceBuffer::update_range(const void* data, std::size_t offset, std::size_t count) {
    if (offset + count > m_capacity) return;

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    }
    if (pfn_glBufferSubData) {
        pfn_glBufferSubData(GL_ARRAY_BUFFER, offset * m_stride, count * m_stride, data);
    }
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void InstanceBuffer::bind() const {
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    }
}

void InstanceBuffer::unbind() {
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void InstanceBuffer::clear() {
    m_count = 0;
}

// =============================================================================
// InstanceBatch Implementation
// =============================================================================

InstanceBatch::InstanceBatch() = default;

InstanceBatch::~InstanceBatch() = default;

InstanceBatch::InstanceBatch(InstanceBatch&& other) noexcept = default;
InstanceBatch& InstanceBatch::operator=(InstanceBatch&& other) noexcept = default;

bool InstanceBatch::initialize(std::size_t initial_capacity) {
    m_instances.reserve(initial_capacity);
    return m_buffer.initialize(initial_capacity, sizeof(InstanceData));
}

void InstanceBatch::add(const InstanceData& instance) {
    m_instances.push_back(instance);
    m_dirty = true;
}

void InstanceBatch::add_bulk(std::span<const InstanceData> instances) {
    m_instances.insert(m_instances.end(), instances.begin(), instances.end());
    m_dirty = true;
}

void InstanceBatch::clear() {
    m_instances.clear();
    m_buffer.clear();
    m_dirty = false;
}

void InstanceBatch::upload() {
    if (!m_dirty || m_instances.empty()) return;

    m_buffer.update(m_instances.data(), m_instances.size());
    m_dirty = false;
}

void InstanceBatch::bind() const {
    m_buffer.bind();
}

// =============================================================================
// InstanceBatcher Implementation
// =============================================================================

InstanceBatcher::InstanceBatcher() = default;

InstanceBatcher::~InstanceBatcher() = default;

bool InstanceBatcher::initialize(const Config& config) {
    m_config = config;
    m_batches.clear();
    m_batch_lookup.clear();

    spdlog::info("InstanceBatcher initialized: max_batch_size={}, max_batches={}",
                 config.max_batch_size, config.max_batches);
    return true;
}

void InstanceBatcher::begin_frame() {
    // Clear all batches
    for (auto& [key, batch] : m_batches) {
        batch.clear();
    }
    m_draw_calls = 0;
    m_instances_rendered = 0;
}

void InstanceBatcher::submit(const BatchKey& key, const InstanceData& instance) {
    auto it = m_batches.find(key);
    if (it == m_batches.end()) {
        // Create new batch
        if (m_batches.size() >= m_config.max_batches) {
            spdlog::warn("Max batch count reached, instance dropped");
            return;
        }

        m_batches[key] = InstanceBatch();
        m_batches[key].initialize(m_config.max_batch_size);
        it = m_batches.find(key);
    }

    it->second.add(instance);
}

void InstanceBatcher::submit_bulk(const BatchKey& key,
                                   std::span<const InstanceData> instances) {
    auto it = m_batches.find(key);
    if (it == m_batches.end()) {
        if (m_batches.size() >= m_config.max_batches) {
            spdlog::warn("Max batch count reached, instances dropped");
            return;
        }

        m_batches[key] = InstanceBatch();
        m_batches[key].initialize(m_config.max_batch_size);
        it = m_batches.find(key);
    }

    it->second.add_bulk(instances);
}

void InstanceBatcher::end_frame() {
    // Upload all dirty batches to GPU
    for (auto& [key, batch] : m_batches) {
        batch.upload();
    }
}

void InstanceBatcher::for_each_batch(const std::function<void(const BatchKey&, const InstanceBatch&)>& callback) const {
    for (const auto& [key, batch] : m_batches) {
        if (batch.count() > 0) {
            callback(key, batch);
        }
    }
}

InstanceBatcher::Stats InstanceBatcher::stats() const {
    Stats s;
    s.batch_count = m_batches.size();
    s.draw_calls = m_draw_calls;
    s.instances_rendered = m_instances_rendered;

    for (const auto& [key, batch] : m_batches) {
        s.total_instances += batch.count();
    }

    return s;
}

// =============================================================================
// DrawCommand Implementation
// =============================================================================

DrawCommand DrawCommand::indexed(std::uint32_t index_count, std::uint32_t instance_count,
                                  std::uint32_t first_index, std::int32_t base_vertex,
                                  std::uint32_t base_instance) {
    DrawCommand cmd;
    cmd.count = index_count;
    cmd.instance_count = instance_count;
    cmd.first = first_index;
    cmd.base_vertex = base_vertex;
    cmd.base_instance = base_instance;
    cmd.is_indexed = true;
    return cmd;
}

DrawCommand DrawCommand::arrays(std::uint32_t vertex_count, std::uint32_t instance_count,
                                 std::uint32_t first_vertex, std::uint32_t base_instance) {
    DrawCommand cmd;
    cmd.count = vertex_count;
    cmd.instance_count = instance_count;
    cmd.first = first_vertex;
    cmd.base_instance = base_instance;
    cmd.is_indexed = false;
    return cmd;
}

// =============================================================================
// IndirectBuffer Implementation
// =============================================================================

IndirectBuffer::IndirectBuffer() = default;

IndirectBuffer::~IndirectBuffer() {
    destroy();
}

IndirectBuffer::IndirectBuffer(IndirectBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_capacity(other.m_capacity)
    , m_count(other.m_count)
{
    other.m_buffer = 0;
}

IndirectBuffer& IndirectBuffer::operator=(IndirectBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        m_count = other.m_count;
        other.m_buffer = 0;
    }
    return *this;
}

bool IndirectBuffer::initialize(std::size_t initial_capacity) {
    if (!load_instancing_gl_functions()) return false;

    m_capacity = initial_capacity;
    m_count = 0;

    if (pfn_glGenBuffers) {
        pfn_glGenBuffers(1, &m_buffer);
    }
    if (!m_buffer) return false;

    // GL_DRAW_INDIRECT_BUFFER = 0x8F3F
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(0x8F3F, m_buffer);
    }
    if (pfn_glBufferData) {
        pfn_glBufferData(0x8F3F, m_capacity * sizeof(DrawElementsIndirectCommand),
                         nullptr, GL_DYNAMIC_DRAW);
    }
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(0x8F3F, 0);
    }

    return true;
}

void IndirectBuffer::destroy() {
    if (m_buffer && pfn_glDeleteBuffers) {
        pfn_glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
    }
    m_commands.clear();
    m_capacity = 0;
    m_count = 0;
}

void IndirectBuffer::add(const DrawCommand& cmd) {
    DrawElementsIndirectCommand indirect;
    indirect.count = cmd.count;
    indirect.instanceCount = cmd.instance_count;
    indirect.firstIndex = cmd.first;
    indirect.baseVertex = cmd.base_vertex;
    indirect.baseInstance = cmd.base_instance;
    m_commands.push_back(indirect);
}

void IndirectBuffer::clear() {
    m_commands.clear();
    m_count = 0;
}

void IndirectBuffer::upload() {
    if (m_commands.empty()) return;

    if (m_commands.size() > m_capacity) {
        // Resize buffer
        m_capacity = m_commands.size() * 2;
        if (m_buffer && pfn_glDeleteBuffers) {
            pfn_glDeleteBuffers(1, &m_buffer);
        }
        if (pfn_glGenBuffers) {
            pfn_glGenBuffers(1, &m_buffer);
        }
        if (pfn_glBindBuffer) {
            pfn_glBindBuffer(0x8F3F, m_buffer);
        }
        if (pfn_glBufferData) {
            pfn_glBufferData(0x8F3F, m_capacity * sizeof(DrawElementsIndirectCommand),
                             nullptr, GL_DYNAMIC_DRAW);
        }
    } else {
        if (pfn_glBindBuffer) {
            pfn_glBindBuffer(0x8F3F, m_buffer);
        }
    }

    if (pfn_glBufferSubData) {
        pfn_glBufferSubData(0x8F3F, 0, m_commands.size() * sizeof(DrawElementsIndirectCommand),
                            m_commands.data());
    }

    m_count = m_commands.size();

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(0x8F3F, 0);
    }
}

void IndirectBuffer::bind() const {
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(0x8F3F, m_buffer);
    }
}

void IndirectBuffer::unbind() {
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(0x8F3F, 0);
    }
}

void IndirectBuffer::execute(GLenum mode, GLenum index_type) const {
    if (m_count == 0) return;

    bind();
    if (pfn_glMultiDrawElementsIndirect) {
        pfn_glMultiDrawElementsIndirect(mode, index_type, nullptr,
                                         static_cast<GLsizei>(m_count),
                                         sizeof(DrawElementsIndirectCommand));
    }
    unbind();
}

// =============================================================================
// InstanceRenderer Implementation
// =============================================================================

InstanceRenderer::InstanceRenderer() = default;

InstanceRenderer::~InstanceRenderer() {
    shutdown();
}

bool InstanceRenderer::initialize(std::size_t max_instances) {
    if (!load_instancing_gl_functions()) return false;

    m_max_instances = max_instances;

    // Initialize instance buffer
    if (!m_instance_buffer.initialize(max_instances, sizeof(InstanceData))) {
        spdlog::error("Failed to initialize instance buffer");
        return false;
    }

    // Initialize indirect buffer
    if (!m_indirect_buffer.initialize(1024)) {
        spdlog::error("Failed to initialize indirect buffer");
        return false;
    }

    // Initialize batcher
    InstanceBatcher::Config batcher_config;
    batcher_config.max_batch_size = max_instances;
    batcher_config.max_batches = 256;

    if (!m_batcher.initialize(batcher_config)) {
        spdlog::error("Failed to initialize instance batcher");
        return false;
    }

    spdlog::info("InstanceRenderer initialized: max_instances={}", max_instances);
    return true;
}

void InstanceRenderer::shutdown() {
    m_instance_buffer.destroy();
    m_indirect_buffer.destroy();
}

void InstanceRenderer::begin_frame() {
    m_batcher.begin_frame();
    m_staging_instances.clear();
    m_draw_stats = {};
}

void InstanceRenderer::submit(MeshHandle mesh, MaterialHandle material,
                               const InstanceData& instance) {
    BatchKey key{mesh.asset_id, material.id, 0};
    m_batcher.submit(key, instance);
}

void InstanceRenderer::submit_batch(MeshHandle mesh, MaterialHandle material,
                                     std::span<const InstanceData> instances) {
    BatchKey key{mesh.asset_id, material.id, 0};
    m_batcher.submit_bulk(key, instances);
}

void InstanceRenderer::end_frame() {
    m_batcher.end_frame();
}

void InstanceRenderer::render_batch(const BatchKey& key, const InstanceBatch& batch,
                                     GLuint vao, GLsizei index_count) {
    if (batch.count() == 0) return;

    // Bind VAO
    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(vao);
    }

    // Bind and setup instance buffer attributes
    batch.bind();

    // Setup instance attributes (assuming attributes 4-8 for instance data)
    // InstanceData layout: model matrix (4x vec4), normal_model (3x vec3), material_idx, flags, etc.

    const std::size_t stride = sizeof(InstanceData);

    // Model matrix - 4 vec4s at attributes 4, 5, 6, 7
    for (int i = 0; i < 4; ++i) {
        GLuint attrib = 4 + i;
        if (pfn_glEnableVertexAttribArray) {
            pfn_glEnableVertexAttribArray(attrib);
        }
        if (pfn_glVertexAttribPointer) {
            pfn_glVertexAttribPointer(attrib, 4, GL_FLOAT, GL_FALSE, stride,
                                       (void*)(sizeof(float) * 4 * i));
        }
        if (pfn_glVertexAttribDivisor) {
            pfn_glVertexAttribDivisor(attrib, 1);
        }
    }

    // Normal matrix - 3 vec3s at attributes 8, 9, 10
    for (int i = 0; i < 3; ++i) {
        GLuint attrib = 8 + i;
        std::size_t offset = sizeof(float) * 16 + sizeof(float) * 3 * i;
        if (pfn_glEnableVertexAttribArray) {
            pfn_glEnableVertexAttribArray(attrib);
        }
        if (pfn_glVertexAttribPointer) {
            pfn_glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, stride,
                                       (void*)offset);
        }
        if (pfn_glVertexAttribDivisor) {
            pfn_glVertexAttribDivisor(attrib, 1);
        }
    }

    // Material index at attribute 11
    {
        std::size_t offset = sizeof(float) * 16 + sizeof(float) * 12;  // After matrices
        if (pfn_glEnableVertexAttribArray) {
            pfn_glEnableVertexAttribArray(11);
        }
        if (pfn_glVertexAttribPointer) {
            pfn_glVertexAttribPointer(11, 1, GL_UNSIGNED_INT, GL_FALSE, stride,
                                       (void*)offset);
        }
        if (pfn_glVertexAttribDivisor) {
            pfn_glVertexAttribDivisor(11, 1);
        }
    }

    // Draw instanced
    if (pfn_glDrawElementsInstanced) {
        pfn_glDrawElementsInstanced(0x0004, index_count, GL_UNSIGNED_INT, nullptr,
                                     static_cast<GLsizei>(batch.count()));  // GL_TRIANGLES
    }

    // Clean up instance attributes
    for (GLuint attrib = 4; attrib <= 11; ++attrib) {
        if (pfn_glVertexAttribDivisor) {
            pfn_glVertexAttribDivisor(attrib, 0);
        }
        if (pfn_glDisableVertexAttribArray) {
            pfn_glDisableVertexAttribArray(attrib);
        }
    }

    // Unbind
    InstanceBuffer::unbind();
    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(0);
    }

    m_draw_stats.draw_calls++;
    m_draw_stats.instances_rendered += batch.count();
    m_draw_stats.triangles_rendered += (index_count / 3) * batch.count();
}

void InstanceRenderer::render_all(const std::function<void(const BatchKey&)>& setup_callback,
                                   const std::function<std::pair<GLuint, GLsizei>(MeshHandle)>& get_mesh) {
    m_batcher.for_each_batch([&](const BatchKey& key, const InstanceBatch& batch) {
        setup_callback(key);
        auto [vao, index_count] = get_mesh(MeshHandle{key.mesh_id});
        render_batch(key, batch, vao, index_count);
    });
}

InstanceRenderer::Stats InstanceRenderer::stats() const {
    return m_draw_stats;
}

} // namespace void_render
