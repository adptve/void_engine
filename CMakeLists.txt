# ============================================================================
# void_engine - Hot-Reloadable Game/Render Engine
# ============================================================================
cmake_minimum_required(VERSION 3.20)

project(void_engine
    VERSION 0.1.0
    DESCRIPTION "Hot-reloadable game and render engine"
    LANGUAGES CXX
)

# Store source directory for submodules
set(VOID_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Global Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Options
# ============================================================================
option(VOID_BUILD_TESTS "Build unit tests" ON)
option(VOID_BUILD_EXAMPLES "Build examples" ON)
option(VOID_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(VOID_BUILD_TOOLS "Build tools" OFF)
option(VOID_HOT_RELOAD "Enable hot-reload support" ON)
option(VOID_ENABLE_VULKAN "Enable Vulkan backend" ON)

# ============================================================================
# CMake Modules
# ============================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CompilerWarnings)
include(Dependencies)
include(Modules)

# ============================================================================
# Fetch Dependencies
# ============================================================================
void_fetch_dependencies()

if(VOID_ENABLE_VULKAN)
    void_find_vulkan()
endif()

# ============================================================================
# Phase 1: Foundation Modules (Header-Only)
# ============================================================================
# void_math - Linear algebra (header-only, uses GLM)
void_add_header_module(NAME void_math
    DEPENDENCIES glm::glm
)

# void_structures - Data structures (header-only)
void_add_header_module(NAME void_structures)

# ============================================================================
# Phase 1: Foundation Modules (Compiled)
# ============================================================================
add_subdirectory(src/memory)
add_subdirectory(src/core)

# ============================================================================
# Phase 2: Data Layer Modules
# ============================================================================
add_subdirectory(src/event)
add_subdirectory(src/ecs)
add_subdirectory(src/ir)
add_subdirectory(src/asset)
add_subdirectory(src/services)

# ============================================================================
# Phase 3: Rendering Modules
# ============================================================================
add_subdirectory(src/shader)
add_subdirectory(src/render)
add_subdirectory(src/ui)
add_subdirectory(src/presenter)
add_subdirectory(src/compositor)
add_subdirectory(src/xr)

# ============================================================================
# Phase 4: Engine Core Modules
# ============================================================================
add_subdirectory(src/kernel)
add_subdirectory(src/engine)

# ============================================================================
# Phase 5: Gameplay Modules
# ============================================================================
add_subdirectory(src/physics)
add_subdirectory(src/triggers)
add_subdirectory(src/combat)
add_subdirectory(src/inventory)
add_subdirectory(src/audio)
add_subdirectory(src/ai)
add_subdirectory(src/gamestate)
add_subdirectory(src/hud)

# ============================================================================
# Phase 5.5: Scene Loading
# ============================================================================
add_subdirectory(src/scene)

# ============================================================================
# Phase 6: Scripting Modules
# ============================================================================
add_subdirectory(src/graph)
add_subdirectory(src/script)
add_subdirectory(src/scripting)
add_subdirectory(src/cpp)

# ============================================================================
# Phase 7: Tools Modules
# ============================================================================
add_subdirectory(src/shell)
add_subdirectory(src/editor)
add_subdirectory(src/runtime)

# ============================================================================
# Main Executable
# ============================================================================
find_package(OpenGL REQUIRED)

add_executable(void_runtime src/main.cpp)

target_link_libraries(void_runtime
    PRIVATE
        void_render
        void_scene
        glfw
        OpenGL::GL
        glm::glm
        spdlog::spdlog
        tomlplusplus::tomlplusplus
)

target_include_directories(void_runtime
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

void_set_compiler_warnings(void_runtime)

# ============================================================================
# Testing
# ============================================================================
if(VOID_BUILD_TESTS)
    enable_testing()
    include(Testing)
    void_fetch_test_dependencies()
    add_subdirectory(tests)
endif()

# ============================================================================
# Examples
# ============================================================================
if(VOID_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(VOID_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Tools
# ============================================================================
if(VOID_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "void_engine ${PROJECT_VERSION}")
message(STATUS "=====================================")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests:       ${VOID_BUILD_TESTS}")
message(STATUS "Build examples:    ${VOID_BUILD_EXAMPLES}")
message(STATUS "Hot-reload:        ${VOID_HOT_RELOAD}")
message(STATUS "Vulkan backend:    ${VOID_ENABLE_VULKAN}")
if(VOID_HAS_VULKAN)
message(STATUS "Vulkan found:      YES")
else()
message(STATUS "Vulkan found:      NO")
endif()
message(STATUS "")
