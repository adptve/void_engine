# ============================================================================
# void_engine Plugins Build System
# ============================================================================
# This file provides the add_void_plugin() function for building plugins
# and includes all plugin subdirectories.
# ============================================================================

# ============================================================================
# Plugin Build Function
# ============================================================================

# add_void_plugin(NAME plugin_name [SOURCES file1.cpp file2.cpp ...])
#
# Creates a shared library plugin with standard configuration:
# - Links to void_plugin_api, void_core, void_ecs, void_package
# - Sets output directory to ${CMAKE_BINARY_DIR}/plugins
# - Enables MSVC hot-reload settings
# - Copies built plugin to source plugins directory for development
#
# Arguments:
#   NAME      - Required. Plugin name (e.g., "base.health")
#   SOURCES   - Optional. Source files. Defaults to ${NAME}_plugin.cpp
#
function(add_void_plugin)
    cmake_parse_arguments(PLUGIN "" "NAME" "SOURCES" ${ARGN})

    if(NOT PLUGIN_NAME)
        message(FATAL_ERROR "add_void_plugin requires NAME argument")
    endif()

    # Convert dots to underscores for valid target names
    string(REPLACE "." "_" TARGET_NAME ${PLUGIN_NAME})

    # Default sources if not specified
    if(NOT PLUGIN_SOURCES)
        set(PLUGIN_SOURCES ${TARGET_NAME}_plugin.cpp)
    endif()

    # Create shared library
    add_library(${TARGET_NAME} SHARED ${PLUGIN_SOURCES})

    # Export symbol for DLL export
    target_compile_definitions(${TARGET_NAME} PRIVATE VOID_PLUGIN_EXPORT)

    # Link required libraries
    target_link_libraries(${TARGET_NAME} PRIVATE
        void_plugin_api
        void_core
        void_ecs
        void_package
        void_kernel
        nlohmann_json::nlohmann_json
        void_spdlog
    )

    # Include directories
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Output settings
    set_target_properties(${TARGET_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        # Remove 'lib' prefix on Linux/Mac for consistency
        PREFIX ""
        # Use plugin name with dots for output
        OUTPUT_NAME ${PLUGIN_NAME}
    )

    # Enable hot-reload friendly settings
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /ZI)
        target_link_options(${TARGET_NAME} PRIVATE /INCREMENTAL)
    endif()

    # Copy to source plugins directory for development
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_SOURCE_DIR}/plugins/bin
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${TARGET_NAME}>
            ${CMAKE_SOURCE_DIR}/plugins/bin/${PLUGIN_NAME}$<TARGET_FILE_SUFFIX:${TARGET_NAME}>
        COMMENT "Copying ${PLUGIN_NAME} plugin to plugins/bin/"
    )

    # Copy manifest to build output directory (where the DLL goes)
    # This allows the package scanner to find the plugin
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/manifest.plugin.json)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/manifest.plugin.json
                $<TARGET_FILE_DIR:${TARGET_NAME}>/${PLUGIN_NAME}.plugin.json
            COMMENT "Copying ${PLUGIN_NAME} manifest to build directory"
        )
    endif()

    message(STATUS "  Plugin: ${PLUGIN_NAME} (target: ${TARGET_NAME})")
endfunction()

# ============================================================================
# Plugin Subdirectories
# ============================================================================
message(STATUS "")
message(STATUS "Building Plugins:")

# Example AI plugin (existing)
add_subdirectory(example_ai)

# Base plugins (core gameplay functionality)
add_subdirectory(base.health)

message(STATUS "")
