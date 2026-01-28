# Example AI Plugin - Demonstrates hot-swappable gameplay plugin
# This builds as a shared library (.dll/.so/.dylib) that can be loaded at runtime

add_library(example_ai SHARED
    example_ai_plugin.cpp
    example_ai_plugin.hpp
)

target_link_libraries(example_ai PRIVATE
    void_plugin_api
    void_core
)

target_include_directories(example_ai PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set output directory to plugins folder
set_target_properties(example_ai PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    # Remove 'lib' prefix on Linux/Mac for consistency
    PREFIX ""
)

# Enable hot-reload friendly settings
if(MSVC)
    # Enable Edit and Continue for debugging
    target_compile_options(example_ai PRIVATE /ZI)
    target_link_options(example_ai PRIVATE /INCREMENTAL)
endif()

# Copy to source plugins directory for development
add_custom_command(TARGET example_ai POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:example_ai>
        ${CMAKE_SOURCE_DIR}/plugins/$<TARGET_FILE_NAME:example_ai>
    COMMENT "Copying example_ai plugin to source plugins directory"
)
