# void_hud - Heads-up display system
void_add_module(NAME void_hud
    SOURCES
        types.cpp
        elements.cpp
        binding.cpp
        animation.cpp
        hud.cpp
    DEPENDENCIES
        void_core
        void_ui
        void_ecs
        void_event
)
