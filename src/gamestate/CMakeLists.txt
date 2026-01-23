# void_gamestate - Game state management
void_add_module(NAME void_gamestate
    SOURCES
        types.cpp
        variables.cpp
        saveload.cpp
        objectives.cpp
        gamestate.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
