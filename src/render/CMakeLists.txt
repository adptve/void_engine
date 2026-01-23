# void_render - Rendering backend abstraction
# Find OpenGL
find_package(OpenGL REQUIRED)

void_add_module(NAME void_render
    SOURCES
        stub.cpp
        gl_renderer.cpp
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
