# ============================================================================
# void_engine - Phased Build System
# ============================================================================
# Phases align with src/main.cpp initialization order.
# Uncomment each phase as you activate it in main.cpp.
# ============================================================================
cmake_minimum_required(VERSION 3.20)

project(void_engine
    VERSION 0.1.0
    DESCRIPTION "Hot-reloadable game and render engine"
    LANGUAGES CXX
)

set(VOID_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Global Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Options (all OFF for phased build)
# ============================================================================
option(VOID_BUILD_TESTS "Build unit tests" OFF)
option(VOID_BUILD_EXAMPLES "Build examples" OFF)
option(VOID_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(VOID_BUILD_TOOLS "Build tools" OFF)

# ============================================================================
# CMake Modules
# ============================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(FetchContent)
include(CompilerWarnings)

# ============================================================================
# PHASE 0: SKELETON (ACTIVE)
# Dependencies: spdlog, nlohmann_json
# ============================================================================
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ============================================================================
# PHASE 1: FOUNDATION (ACTIVE)
# Modules: memory, core, math, structures
# Dependencies: glm
# ============================================================================
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

include(Modules)
void_add_header_module(NAME void_math DEPENDENCIES glm::glm)
void_add_header_module(NAME void_structures)

add_subdirectory(src/memory)
add_subdirectory(src/core)

# ============================================================================
# PHASE 2: INFRASTRUCTURE (ACTIVE)
# Modules: event, services, ir, kernel
# ============================================================================
add_subdirectory(src/event)
add_subdirectory(src/services)
add_subdirectory(src/ir)
add_subdirectory(src/kernel)

# ============================================================================
# PHASE 3: RESOURCES (ACTIVE)
# Modules: asset, shader
# Dependencies: stb, tinygltf
# ============================================================================
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_SHALLOW    TRUE
)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

add_subdirectory(src/asset)
add_subdirectory(src/shader)

# ============================================================================
# PHASE 4: PLATFORM (ACTIVE)
# Modules: presenter, render, compositor
# Dependencies: glfw, glad, OpenGL
# ============================================================================
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

add_subdirectory(src/render)
add_subdirectory(src/presenter)
add_subdirectory(src/compositor)

# ============================================================================
# PHASE 4b: UI SYSTEM (ACTIVE)
# Modules: ui
# Dependencies: void_core, nlohmann_json, OpenGL
# ============================================================================
add_subdirectory(src/ui)

# ============================================================================
# PHASE 5: I/O (ACTIVE)
# Modules: audio, input
# Dependencies: miniaudio, dr_libs
# ============================================================================
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

FetchContent_Declare(dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(dr_libs)
add_library(dr_libs INTERFACE)
target_include_directories(dr_libs INTERFACE ${dr_libs_SOURCE_DIR})

add_subdirectory(src/audio)
add_subdirectory(src/input)

# ============================================================================
# PHASE 6: SIMULATION (ACTIVE)
# Modules: ecs, physics, triggers
# ============================================================================
add_subdirectory(src/ecs)
add_subdirectory(src/physics)
add_subdirectory(src/triggers)

# ============================================================================
# PHASE 6b: PACKAGE SYSTEM (ACTIVE)
# Modules: package
# Dependencies: void_core, void_ecs, nlohmann_json
# ============================================================================
add_subdirectory(src/package)

# ============================================================================
# PHASE 7: SCENE (ACTIVE)
# Modules: scene, graph
# Dependencies: nlohmann_json (already fetched in Phase 0)
# ============================================================================
add_subdirectory(src/scene)
add_subdirectory(src/graph)

# ============================================================================
# PHASE 8: SCRIPTING (ACTIVE)
# Modules: script, scripting, cpp, shell
# ============================================================================
add_subdirectory(src/script)
add_subdirectory(src/scripting)
add_subdirectory(src/cpp)
add_subdirectory(src/shell)

# ============================================================================
# PHASE 9: GAMEPLAY (ACTIVE)
# Modules: plugin_api, gamestate (core owns all state, plugins hot-swap)
# Architecture: GameStateCore owns AI/Combat/Inventory state stores
#               Plugins read state and submit commands through IPluginAPI
# ============================================================================
add_subdirectory(src/plugin_api)
add_subdirectory(src/gamestate)

# Plugins (hot-swappable gameplay plugins)
add_subdirectory(plugins)

# ============================================================================
# PHASE 10: WIDGET SYSTEM (ACTIVE)
# Modules: widget (core owns all state, widget plugins hot-swap)
# Architecture: WidgetStateCore owns all widget state
#               Widget plugins read state and render through IWidgetAPI
# ============================================================================
add_subdirectory(src/widget)

# Widget Plugins (hot-swappable widget renderers)
add_subdirectory(widgets/core)

# ============================================================================
# PHASE 11: EXTENSIONS
# Modules: xr, editor
# ============================================================================
# add_subdirectory(src/xr)
# add_subdirectory(src/editor)

# ============================================================================
# PHASE 12: APPLICATION
# Modules: runtime, engine
# ============================================================================
# PHASE 12: RUNTIME
# ============================================================================
add_subdirectory(src/runtime)
# add_subdirectory(src/engine)

# ============================================================================
# EXECUTABLES
# ============================================================================

# Production executable - thin main, delegates to Runtime
add_executable(void_engine src/main.cpp)

# Validation/bootstrap harness - full integration test (preserved from phases 1-10)
add_executable(void_engine_bootstrap src/main-bootstrap.cpp)

# Phase 0 dependencies only
target_link_libraries(void_engine PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Phase 1 dependencies
target_link_libraries(void_engine PRIVATE glm::glm)
target_link_libraries(void_engine PRIVATE void_math)
target_link_libraries(void_engine PRIVATE void_structures)
target_link_libraries(void_engine PRIVATE void_memory)
target_link_libraries(void_engine PRIVATE void_core)

# Phase 2 dependencies
target_link_libraries(void_engine PRIVATE void_event)
target_link_libraries(void_engine PRIVATE void_services)
target_link_libraries(void_engine PRIVATE void_ir)
target_link_libraries(void_engine PRIVATE void_kernel)

# Phase 3 dependencies
target_link_libraries(void_engine PRIVATE void_asset)
target_link_libraries(void_engine PRIVATE void_shader)

# Phase 4 dependencies
target_link_libraries(void_engine PRIVATE glfw)
target_link_libraries(void_engine PRIVATE OpenGL::GL)
target_link_libraries(void_engine PRIVATE void_render)
target_link_libraries(void_engine PRIVATE void_presenter)
target_link_libraries(void_engine PRIVATE void_compositor)
target_link_libraries(void_engine PRIVATE void_audio)          # Phase 5
target_link_libraries(void_engine PRIVATE void_input)          # Phase 5
target_link_libraries(void_engine PRIVATE void_ecs)            # Phase 6
target_link_libraries(void_engine PRIVATE void_physics)        # Phase 6
target_link_libraries(void_engine PRIVATE void_triggers)       # Phase 6
target_link_libraries(void_engine PRIVATE void_package)        # Phase 6b
target_link_libraries(void_engine PRIVATE void_scene)          # Phase 7
target_link_libraries(void_engine PRIVATE void_graph)          # Phase 7
target_link_libraries(void_engine PRIVATE void_script)         # Phase 8
target_link_libraries(void_engine PRIVATE void_scripting)      # Phase 8
target_link_libraries(void_engine PRIVATE void_cpp)            # Phase 8
target_link_libraries(void_engine PRIVATE void_shell)          # Phase 8
target_link_libraries(void_engine PRIVATE void_plugin_api)     # Phase 9
target_link_libraries(void_engine PRIVATE void_gamestate)      # Phase 9
target_link_libraries(void_engine PRIVATE void_widget)         # Phase 10
# target_link_libraries(void_engine PRIVATE void_xr)            # Phase 11
# target_link_libraries(void_engine PRIVATE void_editor)        # Phase 11
target_link_libraries(void_engine PRIVATE void_runtime)         # Phase 12

target_include_directories(void_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Bootstrap executable - validation harness with all phase dependencies
target_link_libraries(void_engine_bootstrap PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    glm::glm
    void_math
    void_structures
    void_memory
    void_core
    void_event
    void_services
    void_ir
    void_kernel
    void_asset
    void_shader
    glfw
    OpenGL::GL
    void_render
    void_presenter
    void_compositor
    void_audio
    void_input
    void_ecs
    void_physics
    void_triggers
    void_package
    void_scene
    void_graph
    void_script
    void_scripting
    void_cpp
    void_shell
    void_plugin_api
    void_gamestate
    void_widget
    void_runtime_legacy
)
target_include_directories(void_engine_bootstrap PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# TESTING (enable when needed)
# ============================================================================
if(VOID_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    include(Testing)
    add_subdirectory(tests)
endif()

# ============================================================================
# EXAMPLES
# ============================================================================
if(VOID_BUILD_EXAMPLES)
    add_subdirectory(examples/package_demo)
    add_subdirectory(examples/package_primitives)
endif()

# ============================================================================
# BUILD SUMMARY
# ============================================================================
message(STATUS "")
message(STATUS "void_engine ${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "Active Phase:    12 (Runtime + Architecture)")
message(STATUS "Executables:     void_engine (production), void_engine_bootstrap (validation)")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "")
