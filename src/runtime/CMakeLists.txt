# void_runtime - Runtime application wrapper
void_add_module(NAME void_runtime_lib
    SOURCES
        runtime.cpp
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

# Platform-specific libraries
if(WIN32)
    target_link_libraries(void_runtime_lib PRIVATE
        dbghelp
        shell32
        xinput
        dwmapi
    )
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(void_runtime_lib PRIVATE
        ${X11_LIBRARIES}
        pthread
        dl
    )
endif()
