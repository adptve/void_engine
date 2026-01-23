# void_render - Rendering backend abstraction
# Find OpenGL
find_package(OpenGL REQUIRED)

void_add_module(NAME void_render
    SOURCES
        stub.cpp
        backend.cpp
        gl_renderer.cpp
        texture.cpp
        shadow_renderer.cpp
        instancing.cpp
        render_graph.cpp
        post_process.cpp
        spatial.cpp
        debug_renderer.cpp
        gltf_loader.cpp
        lod.cpp
        temporal_effects.cpp
        animation.cpp
    DEPENDENCIES
        void_core
        void_ir
        void_shader
        void_asset
)

# Add scene include path (without linking to avoid circular dependency)
# scene_data.hpp is header-only for data structures
target_include_directories(void_render
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
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

# Vulkan backend
if(VOID_HAS_VULKAN)
    target_sources(void_render PRIVATE
        # backends/vulkan/device.cpp
        # backends/vulkan/buffer.cpp
        # backends/vulkan/texture.cpp
    )
    target_link_libraries(void_render PRIVATE Vulkan::Vulkan)
    target_compile_definitions(void_render PRIVATE VOID_HAS_VULKAN=1)
endif()
