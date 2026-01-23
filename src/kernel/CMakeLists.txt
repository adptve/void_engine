# void_kernel - Engine kernel and hot-reload orchestration
void_add_module(NAME void_kernel
    SOURCES
        types.cpp
        module_loader.cpp
        supervisor.cpp
        sandbox.cpp
        kernel.cpp
    DEPENDENCIES
        void_core
        void_services
        void_event
        spdlog::spdlog
)

if(VOID_HOT_RELOAD)
    target_compile_definitions(void_kernel PUBLIC VOID_HOT_RELOAD=1)
endif()

# Platform-specific dynamic loading
if(UNIX)
    target_link_libraries(void_kernel PRIVATE ${CMAKE_DL_LIBS})
endif()
