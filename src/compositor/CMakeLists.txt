# void_compositor - Layer composition and display management
#
# Provides:
# - Frame scheduling with VRR (Variable Refresh Rate) support
# - HDR (High Dynamic Range) management
# - Input handling (keyboard, mouse, touch)
# - Multi-display output management
# - Platform-specific compositor backends
#
void_add_module(NAME void_compositor
    SOURCES
        stub.cpp
        compositor.cpp
    DEPENDENCIES
        void_core
        void_render
        void_presenter
)

# Optional: Smithay support (Linux Wayland compositor)
# This would require building against Smithay/libdrm/libinput
# find_package(Smithay QUIET)
# if(Smithay_FOUND)
#     target_compile_definitions(void_compositor PRIVATE VOID_HAS_SMITHAY)
#     target_link_libraries(void_compositor PRIVATE Smithay::smithay)
#     target_sources(void_compositor PRIVATE smithay_compositor.cpp)
# endif()

# Optional: DirectComposition support (Windows)
if(WIN32)
    # DirectComposition is part of Windows SDK
    target_compile_definitions(void_compositor PRIVATE VOID_HAS_DCOMP)
endif()

# Web platform (Emscripten)
if(EMSCRIPTEN)
    target_compile_definitions(void_compositor PRIVATE VOID_HAS_WEBCANVAS)
endif()
