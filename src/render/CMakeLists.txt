# void_render - Rendering backend abstraction
# Find OpenGL
find_package(OpenGL REQUIRED)

# Collect backend sources based on platform
set(BACKEND_SOURCES
    backends/null/null_backend.cpp
    backends/opengl/opengl_backend.cpp
    backends/webgpu/webgpu_backend.cpp
)

# Platform-specific backends
if(WIN32)
    list(APPEND BACKEND_SOURCES
        backends/vulkan/vulkan_backend.cpp
        backends/d3d12/d3d12_backend.cpp
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND BACKEND_SOURCES
        backends/vulkan/vulkan_backend.cpp
    )
elseif(APPLE)
    list(APPEND BACKEND_SOURCES
        backends/metal/metal_backend.mm
    )
endif()

void_add_module(NAME void_render
    SOURCES
        backend.cpp          # Multi-backend GPU abstraction - factory functions and BackendManager
        ${BACKEND_SOURCES}   # Individual backend implementations
        gl_renderer.cpp
        render_graph.cpp
        spatial.cpp
        animation.cpp
        texture.cpp
        gltf_loader.cpp      # glTF model loading (implements header pimpl)
        shadow_renderer.cpp
        debug_renderer.cpp
        lod.cpp
        temporal_effects.cpp
        instancing.cpp
        post_process.cpp
        # ECS render integration
        components.cpp       # ECS render components
        render_assets.cpp    # Hot-reloadable asset manager (integrates GltfLoader)
        render_systems.cpp   # ECS render systems
    DEPENDENCIES
        void_core
        void_ir
        void_shader
        void_asset
        void_ecs
)

# Add include paths
target_include_directories(void_render
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}  # For backends/* includes
)

# Link OpenGL and GLFW for the GL renderer
target_link_libraries(void_render
    PUBLIC
        glfw
        OpenGL::GL
        glm::glm
)

# Add stb for image loading (header-only)
if(TARGET stb)
    target_link_libraries(void_render PRIVATE stb)
elseif(DEFINED STB_SOURCE_DIR)
    target_include_directories(void_render PRIVATE ${STB_SOURCE_DIR})
endif()

# Add tinygltf for model loading (header-only)
if(TARGET tinygltf)
    target_link_libraries(void_render PRIVATE tinygltf)
elseif(DEFINED TINYGLTF_SOURCE_DIR)
    target_include_directories(void_render PRIVATE ${TINYGLTF_SOURCE_DIR})
endif()

# Optional Vulkan SDK linking (for full Vulkan support with validation layers)
# The backend dynamically loads Vulkan, but linking provides full headers
if(VOID_HAS_VULKAN)
    target_link_libraries(void_render PRIVATE Vulkan::Vulkan)
    target_compile_definitions(void_render PRIVATE VOID_HAS_VULKAN=1)
endif()
