# void_plugin_api - Plugin API for gameplay systems
# Note: This module does NOT depend on void_gamestate to avoid circular dependency.
# The PluginAPIImpl is fully implemented in plugin_api.cpp using forward declarations.
# The PluginWatcher provides automatic plugin discovery and hot-reload.
#
# The module also provides:
# - IPlugin interface for package-based plugins (plugin.hpp)
# - PluginContext for ECS/Kernel/Render integration (context.hpp)
# - RenderableDesc for the render contract (renderable.hpp)
void_add_module(NAME void_plugin_api
    SOURCES
        plugin_api.cpp
        commands.cpp
        state_stores.cpp
        plugin_watcher.cpp
        context.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_kernel
        void_render
        void_package
)
