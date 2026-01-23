# void_combat - Combat system
void_add_module(NAME void_combat
    SOURCES
        types.cpp
        health.cpp
        weapons.cpp
        status_effects.cpp
        combat.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
