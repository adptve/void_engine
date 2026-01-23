# void_scene - Scene loading and management
void_add_module(NAME void_scene
    SOURCES
        scene_parser.cpp
        scene_instantiator.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_render
        tomlplusplus::tomlplusplus
)
