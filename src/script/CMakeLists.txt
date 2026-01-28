# void_script - Script runtime
void_add_module(NAME void_script
    SOURCES
        types.cpp
        lexer.cpp
        parser.cpp
        interpreter.cpp
        engine.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
