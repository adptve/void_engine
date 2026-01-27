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
# PHASE 4: PLATFORM
# Modules: presenter, render, compositor
# Dependencies: glfw, glad, OpenGL
# ============================================================================
# FetchContent_Declare(glfw
#     GIT_REPOSITORY https://github.com/glfw/glfw.git
#     GIT_TAG        3.4
#     GIT_SHALLOW    TRUE
# )
# set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(glfw)
#
# find_package(OpenGL REQUIRED)
#
# add_subdirectory(src/presenter)
# add_subdirectory(src/render)
# add_subdirectory(src/compositor)

# ============================================================================
# PHASE 5: I/O
# Modules: audio
# Dependencies: miniaudio, dr_libs
# ============================================================================
# FetchContent_Declare(miniaudio
#     GIT_REPOSITORY https://github.com/mackron/miniaudio.git
#     GIT_TAG        0.11.21
#     GIT_SHALLOW    TRUE
# )
# FetchContent_MakeAvailable(miniaudio)
# add_library(miniaudio INTERFACE)
# target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})
#
# FetchContent_Declare(dr_libs
#     GIT_REPOSITORY https://github.com/mackron/dr_libs.git
#     GIT_TAG        master
#     GIT_SHALLOW    TRUE
# )
# FetchContent_MakeAvailable(dr_libs)
# add_library(dr_libs INTERFACE)
# target_include_directories(dr_libs INTERFACE ${dr_libs_SOURCE_DIR})
#
# add_subdirectory(src/audio)

# ============================================================================
# PHASE 6: SIMULATION
# Modules: ecs, physics, triggers
# ============================================================================
# add_subdirectory(src/ecs)
# add_subdirectory(src/physics)
# add_subdirectory(src/triggers)

# ============================================================================
# PHASE 7: SCENE
# Modules: scene, graph
# ============================================================================
# add_subdirectory(src/scene)
# add_subdirectory(src/graph)

# ============================================================================
# PHASE 8: SCRIPTING
# Modules: script, scripting, cpp, shell
# ============================================================================
# add_subdirectory(src/script)
# add_subdirectory(src/scripting)
# add_subdirectory(src/cpp)
# add_subdirectory(src/shell)

# ============================================================================
# PHASE 9: GAMEPLAY
# Modules: ai, combat, inventory, gamestate
# ============================================================================
# add_subdirectory(src/ai)
# add_subdirectory(src/combat)
# add_subdirectory(src/inventory)
# add_subdirectory(src/gamestate)

# ============================================================================
# PHASE 10: UI
# Modules: ui, hud
# ============================================================================
# add_subdirectory(src/ui)
# add_subdirectory(src/hud)

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
# add_subdirectory(src/runtime)
# add_subdirectory(src/engine)

# ============================================================================
# EXECUTABLE
# ============================================================================
add_executable(void_engine src/main.cpp)

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
# target_link_libraries(void_engine PRIVATE glfw)               # Phase 4
# target_link_libraries(void_engine PRIVATE OpenGL::GL)         # Phase 4
# target_link_libraries(void_engine PRIVATE void_presenter)     # Phase 4
# target_link_libraries(void_engine PRIVATE void_render)        # Phase 4
# target_link_libraries(void_engine PRIVATE void_compositor)    # Phase 4
# target_link_libraries(void_engine PRIVATE void_audio)         # Phase 5
# target_link_libraries(void_engine PRIVATE void_ecs)           # Phase 6
# target_link_libraries(void_engine PRIVATE void_physics)       # Phase 6
# target_link_libraries(void_engine PRIVATE void_triggers)      # Phase 6
# target_link_libraries(void_engine PRIVATE void_scene)         # Phase 7
# target_link_libraries(void_engine PRIVATE void_graph)         # Phase 7
# target_link_libraries(void_engine PRIVATE void_script)        # Phase 8
# target_link_libraries(void_engine PRIVATE void_scripting)     # Phase 8
# target_link_libraries(void_engine PRIVATE void_cpp)           # Phase 8
# target_link_libraries(void_engine PRIVATE void_shell)         # Phase 8
# target_link_libraries(void_engine PRIVATE void_ai)            # Phase 9
# target_link_libraries(void_engine PRIVATE void_combat)        # Phase 9
# target_link_libraries(void_engine PRIVATE void_inventory)     # Phase 9
# target_link_libraries(void_engine PRIVATE void_gamestate)     # Phase 9
# target_link_libraries(void_engine PRIVATE void_ui)            # Phase 10
# target_link_libraries(void_engine PRIVATE void_hud)           # Phase 10
# target_link_libraries(void_engine PRIVATE void_xr)            # Phase 11
# target_link_libraries(void_engine PRIVATE void_editor)        # Phase 11
# target_link_libraries(void_engine PRIVATE void_runtime)       # Phase 12
# target_link_libraries(void_engine PRIVATE void_engine_lib)    # Phase 12

target_include_directories(void_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)

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
    add_subdirectory(tests)
endif()

# ============================================================================
# BUILD SUMMARY
# ============================================================================
message(STATUS "")
message(STATUS "void_engine ${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "Active Phase:    3 (Resources)")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "")
