# void_xr - Extended Reality (VR/AR) support
void_add_module(NAME void_xr
    SOURCES
        stub.cpp
        # TODO: Add during migration:
        # session.cpp
        # view.cpp
        # input.cpp
    DEPENDENCIES
        void_core
        void_render
        void_presenter
)

# OpenXR support (optional)
# find_package(OpenXR)
# if(OpenXR_FOUND)
#     target_link_libraries(void_xr PRIVATE OpenXR::openxr_loader)
#     target_compile_definitions(void_xr PRIVATE VOID_HAS_OPENXR=1)
# endif()
