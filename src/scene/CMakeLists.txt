# void_scene - Scene loading and management
void_add_module(NAME void_scene
    SOURCES
        world.cpp
        scene_parser.cpp
        scene_instantiator.cpp
        scene_serializer.cpp
        manifest_parser.cpp
        asset_loader.cpp
        app_manager.cpp
        transform_system.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
        void_render
        nlohmann_json::nlohmann_json
        void_spdlog
)
