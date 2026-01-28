# void_runtime - Runtime application owner (Phase 12 architecture)
# New Runtime class following doc/review architecture
void_add_module(NAME void_runtime
    SOURCES
        runtime.cpp
    DEPENDENCIES
        void_core
        void_event
        void_kernel
        void_ecs
        void_scene
        spdlog::spdlog
)

# Legacy runtime components (preserved for compatibility/reference)
# These support the bootstrap harness and may be migrated to new Runtime
void_add_module(NAME void_runtime_legacy
    SOURCES
        runtime_legacy.cpp
        window.cpp
        input.cpp
        scene_loader.cpp
        scene_parser.cpp
        layer.cpp
        crash_handler.cpp
    DEPENDENCIES
        void_core
        void_event
        spdlog::spdlog
)

# Platform-specific libraries for legacy runtime
if(WIN32)
    target_link_libraries(void_runtime_legacy PRIVATE
        dbghelp
        shell32
        xinput
        dwmapi
    )
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(void_runtime_legacy PRIVATE
        ${X11_LIBRARIES}
        pthread
        dl
    )
endif()
