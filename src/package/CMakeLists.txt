# void_package - Package system for content management
void_add_module(NAME void_package
    SOURCES
        # Phase 1 - Core package system
        fwd.cpp
        version.cpp
        manifest.cpp
        resolver.cpp
        loader.cpp
        registry.cpp
        # Phase 2 - Asset bundles
        asset_bundle.cpp
        asset_bundle_loader.cpp
        prefab_registry.cpp
        definition_registry.cpp
        component_schema.cpp
        # Phase 3 - Plugin packages
        plugin_package.cpp
        dynamic_library.cpp
        plugin_package_loader.cpp
        # Phase 4 - Widget packages
        widget_package.cpp
        widget_manager.cpp
        widget_package_loader.cpp
        # Phase 5 - Layer packages
        layer_package.cpp
        layer_package_loader.cpp
        # Phase 6 - World packages
        world_package.cpp
        world_package_loader.cpp
        world_composer.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_kernel
        void_render
        void_event
        nlohmann_json::nlohmann_json
)
