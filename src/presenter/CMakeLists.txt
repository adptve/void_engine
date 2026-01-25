# void_presenter - Presentation layer for render output
#
# Provides:
# - Multi-backend support (wgpu-native, WebGPU, OpenXR, WebXR)
# - Surface/swapchain management with triple buffering
# - Frame timing and pacing
# - XR/VR stereo rendering with hand tracking
# - Hot-swap between backends at runtime
#
void_add_module(NAME void_presenter
    SOURCES
        stub.cpp
        backend_factory.cpp
        presenter.cpp
        multi_backend_presenter.cpp
        swapchain.cpp
        surface.cpp
        frame.cpp
        timing.cpp
        rehydration.cpp
        backends/null_backend.cpp
        backends/wgpu_backend.cpp
    DEPENDENCIES
        void_core
        void_render  # For GLFW/glad when using OpenGL backend
)

# Optional: wgpu-native support
find_package(wgpu QUIET)
if(wgpu_FOUND)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_WGPU)
    target_link_libraries(void_presenter PRIVATE wgpu::wgpu)
    target_sources(void_presenter PRIVATE wgpu_backend.cpp)
endif()

# Optional: OpenXR support
find_package(OpenXR QUIET)
if(OpenXR_FOUND)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_OPENXR)
    target_link_libraries(void_presenter PRIVATE OpenXR::openxr_loader)
    target_sources(void_presenter PRIVATE openxr_backend.cpp xr_system.cpp)
endif()

# Optional: Vulkan support (for direct Vulkan backend)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_VULKAN)
    target_link_libraries(void_presenter PRIVATE Vulkan::Vulkan)
endif()

# OpenGL backend support (uses GLFW + glad from void_render)
# This is the primary fallback when wgpu is not available
if(NOT EMSCRIPTEN AND NOT wgpu_FOUND)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_OPENGL)
    target_sources(void_presenter PRIVATE opengl_backend.cpp)

    # Link to GLFW and glad (provided by void_render dependency)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(void_presenter PRIVATE glfw)
    endif()

    # Link to glad (if available as separate target)
    if(TARGET glad)
        target_link_libraries(void_presenter PRIVATE glad)
    endif()
endif()

# Web platform (Emscripten)
if(EMSCRIPTEN)
    target_compile_definitions(void_presenter PRIVATE VOID_HAS_WEBGPU VOID_HAS_WEBXR)
endif()
