# Modules.cmake
# Module registration macros for void_engine

include(CMakeParseArguments)

# ============================================================================
# void_add_module - Add a compiled module (static library)
# ============================================================================
# Usage:
#   void_add_module(NAME void_ecs
#       SOURCES
#           world.cpp
#           archetype.cpp
#       DEPENDENCIES
#           void_core
#           void_structures
#   )
function(void_add_module)
    cmake_parse_arguments(
        MODULE
        ""                          # Options
        "NAME"                      # Single value args
        "SOURCES;DEPENDENCIES"      # Multi value args
        ${ARGN}
    )

    if(NOT MODULE_NAME)
        message(FATAL_ERROR "void_add_module: NAME is required")
    endif()

    # Determine module short name (e.g., "ecs" from "void_ecs")
    string(REPLACE "void_" "" MODULE_SHORT_NAME ${MODULE_NAME})

    # Collect source files with full paths
    set(FULL_SOURCES "")
    foreach(SRC ${MODULE_SOURCES})
        list(APPEND FULL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
    endforeach()

    # Create static library
    add_library(${MODULE_NAME} STATIC ${FULL_SOURCES})

    # Set include directories
    target_include_directories(${MODULE_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${VOID_ENGINE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    # Link dependencies
    if(MODULE_DEPENDENCIES)
        target_link_libraries(${MODULE_NAME} PUBLIC ${MODULE_DEPENDENCIES})
    endif()

    # Apply compiler warnings
    void_set_compiler_warnings(${MODULE_NAME})

    # Set C++ standard
    target_compile_features(${MODULE_NAME} PUBLIC cxx_std_20)

    # Add to global module list
    set_property(GLOBAL APPEND PROPERTY VOID_MODULES ${MODULE_NAME})

    message(STATUS "Added module: ${MODULE_NAME}")
endfunction()

# ============================================================================
# void_add_header_module - Add a header-only module (interface library)
# ============================================================================
# Usage:
#   void_add_header_module(NAME void_math
#       DEPENDENCIES
#           glm
#   )
function(void_add_header_module)
    cmake_parse_arguments(
        MODULE
        ""                      # Options
        "NAME"                  # Single value args
        "DEPENDENCIES"          # Multi value args
        ${ARGN}
    )

    if(NOT MODULE_NAME)
        message(FATAL_ERROR "void_add_header_module: NAME is required")
    endif()

    # Create interface library (header-only)
    add_library(${MODULE_NAME} INTERFACE)

    # Set include directories
    target_include_directories(${MODULE_NAME}
        INTERFACE
            $<BUILD_INTERFACE:${VOID_ENGINE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    # Link dependencies
    if(MODULE_DEPENDENCIES)
        target_link_libraries(${MODULE_NAME} INTERFACE ${MODULE_DEPENDENCIES})
    endif()

    # Set C++ standard
    target_compile_features(${MODULE_NAME} INTERFACE cxx_std_20)

    # Add to global module list
    set_property(GLOBAL APPEND PROPERTY VOID_MODULES ${MODULE_NAME})

    message(STATUS "Added header-only module: ${MODULE_NAME}")
endfunction()

# ============================================================================
# void_add_module_tests - Add tests for a module
# ============================================================================
# Usage:
#   void_add_module_tests(NAME void_ecs
#       SOURCES
#           test_entity.cpp
#           test_world.cpp
#   )
function(void_add_module_tests)
    cmake_parse_arguments(
        TEST
        ""                      # Options
        "NAME"                  # Single value args
        "SOURCES"               # Multi value args
        ${ARGN}
    )

    if(NOT TEST_NAME)
        message(FATAL_ERROR "void_add_module_tests: NAME is required")
    endif()

    string(REPLACE "void_" "" MODULE_SHORT_NAME ${TEST_NAME})
    set(TEST_TARGET "test_${MODULE_SHORT_NAME}")

    # Collect test source files
    set(TEST_SOURCES "")
    foreach(SRC ${TEST_SOURCES})
        list(APPEND TEST_SOURCES "${VOID_ENGINE_SOURCE_DIR}/tests/${MODULE_SHORT_NAME}/${SRC}")
    endforeach()

    # Create test executable
    add_executable(${TEST_TARGET} ${TEST_SOURCES})

    target_link_libraries(${TEST_TARGET}
        PRIVATE
            ${TEST_NAME}
            Catch2::Catch2WithMain
    )

    void_set_compiler_warnings(${TEST_TARGET})

    # Register with CTest
    include(Catch)
    catch_discover_tests(${TEST_TARGET})

    message(STATUS "Added tests for: ${TEST_NAME}")
endfunction()
