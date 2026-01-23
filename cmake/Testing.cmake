# Testing.cmake
# Test configuration for void_engine

include(CTest)

# ============================================================================
# Test configuration
# ============================================================================
function(void_configure_testing)
    if(NOT BUILD_TESTING)
        return()
    endif()

    # Fetch Catch2
    void_fetch_test_dependencies()

    # Include Catch2 cmake helpers
    include(Catch)

    message(STATUS "Testing enabled")
endfunction()

# ============================================================================
# Add test executable with Catch2
# ============================================================================
function(void_add_test)
    cmake_parse_arguments(
        TEST
        ""                          # Options
        "NAME"                      # Single value args
        "SOURCES;DEPENDENCIES"      # Multi value args
        ${ARGN}
    )

    if(NOT TEST_NAME)
        message(FATAL_ERROR "void_add_test: NAME is required")
    endif()

    if(NOT TEST_SOURCES)
        message(FATAL_ERROR "void_add_test: SOURCES is required")
    endif()

    add_executable(${TEST_NAME} ${TEST_SOURCES})

    target_link_libraries(${TEST_NAME}
        PRIVATE
            Catch2::Catch2WithMain
            ${TEST_DEPENDENCIES}
    )

    void_set_compiler_warnings(${TEST_NAME})

    # Register with CTest
    catch_discover_tests(${TEST_NAME}
        WORKING_DIRECTORY ${VOID_ENGINE_SOURCE_DIR}
    )
endfunction()

# ============================================================================
# Code coverage (optional, GCC/Clang only)
# ============================================================================
option(VOID_ENABLE_COVERAGE "Enable code coverage" OFF)

function(void_enable_coverage target)
    if(NOT VOID_ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
        message(STATUS "Coverage enabled for ${target}")
    else()
        message(WARNING "Coverage only supported on GCC/Clang")
    endif()
endfunction()
