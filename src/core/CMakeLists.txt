# void_core - Core utilities and types
void_add_module(NAME void_core
    SOURCES
        stub.cpp
        # TODO: Add during migration:
        # types.cpp
        # handle.cpp
        # result.cpp
    DEPENDENCIES
        void_math
        void_memory
        spdlog::spdlog
)
