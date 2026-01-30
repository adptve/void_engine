# Package System Demo
#
# Demonstrates the void_engine package system with:
# - Plugin packages (components, systems)
# - Asset bundles (prefabs, meshes, materials)
# - Layer packages (lighting, spawners, modifiers)
# - Widget packages (HUD elements)
# - World packages (complete game levels)

add_executable(package_demo
    main.cpp
)

target_link_libraries(package_demo
    PRIVATE
        void_runtime
        void_package
        void_plugin_api
        void_ecs
        void_core
        spdlog::spdlog
)

target_compile_features(package_demo PRIVATE cxx_std_20)

# Copy packages to build directory
add_custom_command(TARGET package_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/packages"
        "$<TARGET_FILE_DIR:package_demo>/packages"
    COMMENT "Copying package demo packages to build directory"
)

# Set working directory for debugging
set_target_properties(package_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:package_demo>"
)
