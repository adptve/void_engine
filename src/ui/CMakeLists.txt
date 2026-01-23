# void_ui - User interface system
#
# Provides an immediate-mode UI toolkit with:
# - Theming (built-in + hot-reloadable custom themes from JSON)
# - Fonts (built-in 8x16 bitmap font + custom font loading)
# - Widgets (DebugPanel, Label, ProgressBar, Button, Slider, etc.)
# - Renderer interface (WGSL, GLSL, HLSL shader sources)

void_add_module(NAME void_ui
    SOURCES
        theme.cpp
        font.cpp
        context.cpp
        widgets.cpp
    DEPENDENCIES
        void_core
)
