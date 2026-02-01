# void_engine - Main engine facade
void_add_module(NAME void_engine
    SOURCES
        types.cpp
        app.cpp
        lifecycle.cpp
        config.cpp
        engine.cpp
    DEPENDENCIES
        void_core
        void_kernel
        void_ecs
        void_render
        void_event
        void_asset
        void_services
        void_spdlog
)
