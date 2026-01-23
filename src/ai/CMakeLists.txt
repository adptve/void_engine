# void_ai - AI and behavior systems
void_add_module(NAME void_ai
    SOURCES
        types.cpp
        blackboard.cpp
        behavior_tree.cpp
        navmesh.cpp
        steering.cpp
        perception.cpp
        ai.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)

# Optional: Recast/Detour for production navmesh building
option(VOID_AI_USE_RECAST "Use Recast/Detour for navmesh building" OFF)
if(VOID_AI_USE_RECAST)
    find_package(recastnavigation REQUIRED)
    target_compile_definitions(void_ai PRIVATE VOID_AI_RECAST)
    target_link_libraries(void_ai PRIVATE RecastNavigation::Recast RecastNavigation::Detour)
endif()
