# void_scripting - WASM runtime and plugin system
void_add_module(NAME void_scripting
    SOURCES
        types.cpp
        wasm.cpp
        wasm_interpreter.cpp
        plugin.cpp
        system.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
