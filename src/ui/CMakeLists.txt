# void_ui - User interface system
#
# Provides an immediate-mode UI toolkit with:
# - Theming (built-in + hot-reloadable custom themes from JSON)
# - Fonts (built-in 8x16 bitmap font + custom font loading)
# - Widgets (DebugPanel, Label, ProgressBar, Button, Slider, TextInput, etc.)
# - Renderer interface (OpenGL, WGSL, GLSL, HLSL shader sources)

find_package(OpenGL REQUIRED)

void_add_module(NAME void_ui
    SOURCES
        theme.cpp
        font.cpp
        context.cpp
        widgets.cpp
        gl_renderer.cpp
    DEPENDENCIES
        void_core
        nlohmann_json::nlohmann_json
)

# Link OpenGL for the GL renderer
target_link_libraries(void_ui
    PUBLIC
        OpenGL::GL
)
