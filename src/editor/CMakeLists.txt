# void_editor - Editor tools
void_add_module(NAME void_editor
    SOURCES
        stub.cpp
        # TODO: Add during migration:
        # editor.cpp
        # inspector.cpp
        # hierarchy.cpp
        # viewport.cpp
    DEPENDENCIES
        void_core
        void_engine
        void_ui
        void_ecs
        void_render
        void_shell
)
