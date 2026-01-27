# void_input - Input system
# STATUS: PRODUCTION (2026-01-28)
# - Keyboard, mouse, gamepad input handling
# - Action-based input mapping with contexts
# - GLFW integration for native input events
# - SACRED hot-reload patterns (snapshot/restore)
#
void_add_module(NAME void_input
    SOURCES
        types.cpp
        action.cpp
        input.cpp
    DEPENDENCIES
        void_core
        void_math
)

# Link GLFW for input handling
target_link_libraries(void_input PRIVATE glfw)
