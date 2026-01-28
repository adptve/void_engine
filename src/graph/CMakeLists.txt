# void_graph - Visual scripting graph system
void_add_module(NAME void_graph
    SOURCES
        types.cpp
        node.cpp
        graph.cpp
        execution.cpp
        registry.cpp
        system.cpp
        snapshot.cpp
    DEPENDENCIES
        void_core
        void_structures
        void_event
        void_ecs
)
