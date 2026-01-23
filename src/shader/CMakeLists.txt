# void_shader - Shader compilation and management
void_add_module(NAME void_shader
    SOURCES
        stub.cpp
    DEPENDENCIES
        void_core
        void_ir
)

# Vulkan support
if(VOID_HAS_VULKAN)
    target_compile_definitions(void_shader PRIVATE VOID_HAS_VULKAN=1)
endif()

# Shaderc support (for GLSL/HLSL -> SPIR-V compilation)
find_package(shaderc CONFIG QUIET)
if(shaderc_FOUND)
    target_link_libraries(void_shader PRIVATE shaderc::shaderc)
    target_compile_definitions(void_shader PUBLIC VOID_HAS_SHADERC=1)
    message(STATUS "void_shader: shaderc enabled")
else()
    message(STATUS "void_shader: shaderc not found, using null compiler")
endif()

# SPIRV-Cross support (for SPIR-V -> GLSL/HLSL/MSL transpilation)
find_package(spirv_cross_core CONFIG QUIET)
find_package(spirv_cross_glsl CONFIG QUIET)
find_package(spirv_cross_hlsl CONFIG QUIET)
find_package(spirv_cross_msl CONFIG QUIET)

if(spirv_cross_core_FOUND AND spirv_cross_glsl_FOUND)
    target_link_libraries(void_shader PRIVATE
        spirv-cross-core
        spirv-cross-glsl
    )
    target_compile_definitions(void_shader PUBLIC VOID_HAS_SPIRV_CROSS=1)
    message(STATUS "void_shader: SPIRV-Cross enabled")

    # Optional HLSL and MSL support
    if(spirv_cross_hlsl_FOUND)
        target_link_libraries(void_shader PRIVATE spirv-cross-hlsl)
        target_compile_definitions(void_shader PUBLIC VOID_HAS_SPIRV_CROSS_HLSL=1)
    endif()

    if(spirv_cross_msl_FOUND)
        target_link_libraries(void_shader PRIVATE spirv-cross-msl)
        target_compile_definitions(void_shader PUBLIC VOID_HAS_SPIRV_CROSS_MSL=1)
    endif()
else()
    message(STATUS "void_shader: SPIRV-Cross not found, transpilation disabled")
endif()
