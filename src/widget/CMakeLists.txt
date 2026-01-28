# ============================================================================
# void_widget - Widget State Core Module
# ============================================================================
# Phase 10: Widget System
# - WidgetStateCore owns all persistent widget state
# - Widget plugins read state through IWidgetAPI
# - Widget plugins submit commands to modify state
# - State survives widget plugin hot-reloads
# ============================================================================

add_library(void_widget STATIC
    types.cpp
    widget_state_core.cpp
    commands.cpp
)

target_include_directories(void_widget PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(void_widget PUBLIC
    void_core
    void_event
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_compile_features(void_widget PUBLIC cxx_std_20)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(void_widget PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Enable position-independent code for plugin compatibility
set_target_properties(void_widget PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
