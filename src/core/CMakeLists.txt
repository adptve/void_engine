# void_core - Core utilities and types
void_add_module(NAME void_core
    SOURCES
        stub.cpp
        error.cpp
        handle.cpp
        id.cpp
        log.cpp
        type_registry.cpp
        hot_reload.cpp
        plugin.cpp
        version.cpp
    DEPENDENCIES
        void_math
        void_memory
        spdlog::spdlog
)
