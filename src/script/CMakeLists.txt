# void_script - Script runtime
void_add_module(NAME void_script
    SOURCES
        types.cpp
        lexer.cpp
        parser.cpp
        # interpreter.cpp  # Excluded: API mismatch
        # engine.cpp  # Excluded: API mismatch
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
