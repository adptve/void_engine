# void_cpp - C++ runtime compilation and hot-reload support
void_add_module(NAME void_cpp
    SOURCES
        types.cpp
        # compiler.cpp  # Excluded: API mismatch
        module.cpp
        # instance.cpp  # Excluded: API mismatch
        hot_reload.cpp
        system.cpp
    DEPENDENCIES
        void_core
        void_event
)

if(VOID_HOT_RELOAD)
    target_compile_definitions(void_cpp PUBLIC VOID_HOT_RELOAD=1)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(void_cpp PRIVATE dbghelp)
endif()
