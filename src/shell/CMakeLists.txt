# void_shell - Debug shell and console
void_add_module(NAME void_shell
    SOURCES
        types.cpp
        command.cpp
        parser.cpp
        session.cpp
        shell.cpp
        remote.cpp
        builtins.cpp
    DEPENDENCIES
        void_core
        void_event
        spdlog::spdlog
)

# Platform-specific socket libraries
if(WIN32)
    target_link_libraries(void_shell PRIVATE ws2_32 userenv psapi)
endif()
