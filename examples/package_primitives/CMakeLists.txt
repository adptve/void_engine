# Package Primitives Demo
#
# Minimal package-driven world that renders a cube and sphere from model files.

add_executable(package_primitives
    main.cpp
)

target_link_libraries(package_primitives
    PRIVATE
        void_runtime
        void_package
        void_plugin_api
        void_ecs
        void_core
        void_spdlog
)

target_compile_features(package_primitives PRIVATE cxx_std_20)

# Copy packages to build directory
add_custom_command(TARGET package_primitives POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/packages"
        "$<TARGET_FILE_DIR:package_primitives>/packages"
    COMMENT "Copying package primitives packages to build directory"
)

set_target_properties(package_primitives PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:package_primitives>"
)
