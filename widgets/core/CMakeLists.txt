# Core Widget Plugin
# Provides basic UI elements: panel, button, text, checkbox, slider, etc.

add_library(core_widget SHARED
    core_widget.hpp
    core_widget.cpp
)

target_link_libraries(core_widget PRIVATE
    void_widget
    void_core
)

target_include_directories(core_widget PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(core_widget PROPERTIES
    # Output to widgets directory
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/widgets
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/widgets

    # Remove 'lib' prefix on Linux/macOS
    PREFIX ""

    # C++20
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(core_widget PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()
