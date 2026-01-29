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
    DEPENDENCIES
        void_core
        void_ecs
        nlohmann_json::nlohmann_json
)
