# void_ecs - Entity Component System
#
# High-performance archetype-based ECS with:
# - Compile-time templated component storage
# - Cache-efficient archetype iteration
# - Type-safe queries with read/write access
# - System scheduling with dependency resolution
# - Hot-reload support (snapshot/restore)
#
# Header-only by design for maximum performance through inlining.
# The module.cpp provides library linkage and utilities.

void_add_module(NAME void_ecs
    SOURCES
        module.cpp
    DEPENDENCIES
        void_core
        void_structures
        void_event
)
