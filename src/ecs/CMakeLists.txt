# void_ecs - Entity Component System
void_add_module(NAME void_ecs
    SOURCES
        stub.cpp
        # TODO: Add during migration:
        # world.cpp
        # archetype.cpp
        # entity.cpp
        # component.cpp
        # query.cpp
    DEPENDENCIES
        void_core
        void_structures
        void_event
)
