# void_triggers - Trigger volumes and zones
void_add_module(NAME void_triggers
    SOURCES
        types.cpp
        volumes.cpp
        conditions.cpp
        actions.cpp
        triggers.cpp
    DEPENDENCIES
        void_core
        void_physics
        void_ecs
        void_event
)
