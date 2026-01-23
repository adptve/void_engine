# Dependencies.cmake
# Third-party dependency management for void_engine

include(FetchContent)

# ============================================================================
# GLM - OpenGL Mathematics
# ============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# ============================================================================
# spdlog - Fast C++ logging library
# ============================================================================
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# ============================================================================
# tomlplusplus - TOML parser
# ============================================================================
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)

# ============================================================================
# Catch2 - Testing framework
# ============================================================================
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)

# ============================================================================
# GLFW - Window and OpenGL context management
# ============================================================================
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# ============================================================================
# glad - OpenGL loader
# ============================================================================
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.8
    GIT_SHALLOW    TRUE
)

# ============================================================================
# stb - Single-file public domain libraries (image loading, etc.)
# ============================================================================
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# ============================================================================
# tinygltf - Header-only glTF 2.0 loader
# ============================================================================
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_SHALLOW    TRUE
)

# ============================================================================
# Make dependencies available
# ============================================================================
function(void_fetch_dependencies)
    message(STATUS "Fetching dependencies...")

    # GLM (header-only)
    FetchContent_MakeAvailable(glm)

    # spdlog
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)

    # toml++
    FetchContent_MakeAvailable(tomlplusplus)

    # GLFW (disable docs, tests, examples)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    # stb (header-only image library)
    FetchContent_MakeAvailable(stb)
    # Create an interface library for stb
    if(NOT TARGET stb)
        add_library(stb INTERFACE)
        target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    endif()
    set(STB_SOURCE_DIR ${stb_SOURCE_DIR} PARENT_SCOPE)

    # tinygltf (header-only glTF loader)
    set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
    set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
    set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinygltf)
    set(TINYGLTF_SOURCE_DIR ${tinygltf_SOURCE_DIR} PARENT_SCOPE)

    message(STATUS "Dependencies fetched successfully")
endfunction()

function(void_fetch_test_dependencies)
    message(STATUS "Fetching test dependencies...")
    FetchContent_MakeAvailable(Catch2)
    # Make Catch2 extras available to parent scope
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${catch2_SOURCE_DIR}/extras PARENT_SCOPE)
    message(STATUS "Test dependencies fetched successfully")
endfunction()

# ============================================================================
# Optional: Vulkan SDK
# ============================================================================
function(void_find_vulkan)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan found: ${Vulkan_LIBRARY}")
        set(VOID_HAS_VULKAN TRUE PARENT_SCOPE)
    else()
        message(STATUS "Vulkan not found - Vulkan backend will not be built")
        set(VOID_HAS_VULKAN FALSE PARENT_SCOPE)
    endif()
endfunction()
