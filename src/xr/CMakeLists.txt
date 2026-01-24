# void_xr - Extended Reality (VR/AR) support
void_add_module(NAME void_xr
    SOURCES
        xr_system.cpp
        xr_snapshot.cpp
    DEPENDENCIES
        void_core
        void_render
        void_presenter
)

# OpenXR support (optional)
find_package(OpenXR QUIET)
if(OpenXR_FOUND)
    target_sources(void_xr PRIVATE openxr_backend.cpp)
    target_link_libraries(void_xr PRIVATE OpenXR::openxr_loader)
    target_compile_definitions(void_xr PRIVATE VOID_HAS_OPENXR=1)
    message(STATUS "void_xr: OpenXR support enabled")
else()
    message(STATUS "void_xr: OpenXR not found, using stub backend")
endif()
