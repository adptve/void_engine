/// @file test_widgets.cpp
/// @brief Tests for UI widgets

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <void_engine/ui/widgets.hpp>

using namespace void_ui;
using Catch::Approx;

TEST_CASE("Label widget", "[ui][widgets][label]") {
    SECTION("Draw simple label") {
        UiContext ctx;
        ctx.begin_frame();

        Label::draw(ctx, 10, 20, "Hello");

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw colored label") {
        UiContext ctx;
        ctx.begin_frame();

        Label::draw(ctx, 10, 20, "Hello", Color::red());

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw scaled label") {
        UiContext ctx;
        ctx.begin_frame();

        Label::draw(ctx, 10, 20, "Hello", Color::white(), 2.0f);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("DebugPanel widget", "[ui][widgets][debugpanel]") {
    SECTION("Draw with DebugStat vector") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<DebugStat> stats = {
            {"FPS:", "60.0", StatType::Good},
            {"Frame:", "16.6ms", StatType::Normal},
            {"Memory:", "256 MB", StatType::Info},
        };

        DebugPanel::draw(ctx, 10, 10, "Stats", stats);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw with tuple vector (legacy)") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<std::tuple<std::string, std::string, StatType>> stats = {
            {"FPS:", "60.0", StatType::Good},
            {"Frame:", "16.6ms", StatType::Normal},
        };

        DebugPanel::draw(ctx, 10, 10, "Stats", stats);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw empty panel") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<DebugStat> stats;
        DebugPanel::draw(ctx, 10, 10, "Empty", stats);

        REQUIRE_FALSE(ctx.draw_data().empty()); // Still draws title
    }
}

TEST_CASE("ProgressBar widget", "[ui][widgets][progressbar]") {
    SECTION("Draw default progress bar") {
        UiContext ctx;
        ctx.begin_frame();

        ProgressBar::draw(ctx, 10, 10, 0.5f);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw with config") {
        UiContext ctx;
        ctx.begin_frame();

        ProgressBarConfig config;
        config.width = 300.0f;
        config.height = 30.0f;
        config.fill_color = Color::green();

        ProgressBar::draw(ctx, 10, 10, 0.75f, config);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Progress is clamped") {
        UiContext ctx;
        ctx.begin_frame();

        // Should not crash with out-of-range values
        ProgressBar::draw(ctx, 10, 10, -0.5f);
        ProgressBar::draw(ctx, 10, 50, 1.5f);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw with explicit dimensions") {
        UiContext ctx;
        ctx.begin_frame();

        ProgressBar::draw(ctx, 10, 10, 200.0f, 20.0f, 0.5f, Color::blue());

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("FrameTimeGraph widget", "[ui][widgets][frametimegraph]") {
    SECTION("Draw with frame times") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<float> times = {16.0f, 17.0f, 15.0f, 16.5f, 33.0f};
        FrameTimeGraph::draw(ctx, 10, 10, times);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw empty graph") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<float> times;
        FrameTimeGraph::draw(ctx, 10, 10, times);

        // Should draw background even with no data
        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw with config") {
        UiContext ctx;
        ctx.begin_frame();

        std::vector<float> times = {16.0f, 17.0f};
        FrameTimeGraphConfig config;
        config.width = 400.0f;
        config.height = 150.0f;
        config.target_fps = 144.0f;

        FrameTimeGraph::draw(ctx, 10, 10, times, config);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("Toast widget", "[ui][widgets][toast]") {
    SECTION("Draw centered toast") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);
        ctx.begin_frame();

        Toast::draw(ctx, 100.0f, "Information message", ToastType::Info);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw positioned toast") {
        UiContext ctx;
        ctx.begin_frame();

        Toast::draw(ctx, 50.0f, 50.0f, "Error!", ToastType::Error);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Different toast types") {
        UiContext ctx;
        ctx.begin_frame();

        Toast::draw(ctx, 10, 10, "Info", ToastType::Info);
        Toast::draw(ctx, 10, 50, "Success", ToastType::Success);
        Toast::draw(ctx, 10, 90, "Warning", ToastType::Warning);
        Toast::draw(ctx, 10, 130, "Error", ToastType::Error);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("HelpModal widget", "[ui][widgets][helpmodal]") {
    SECTION("Draw help modal") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);
        ctx.begin_frame();

        std::vector<HelpControl> controls = {
            {"F1", "Toggle help"},
            {"Esc", "Close modal"},
            {"Tab", "Next item"},
        };

        HelpModal::draw(ctx, "Help", controls, "Press Esc to close");

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Draw with legacy pair format") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);
        ctx.begin_frame();

        std::vector<std::pair<std::string, std::string>> controls = {
            {"F1", "Help"},
            {"Esc", "Close"},
        };

        HelpModal::draw(ctx, "Controls", controls);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("Button widget", "[ui][widgets][button]") {
    SECTION("Draw button") {
        UiContext ctx;
        ctx.begin_frame();

        ButtonResult result = Button::draw(ctx, 10, 10, "Click Me");

        REQUIRE_FALSE(ctx.draw_data().empty());
        REQUIRE_FALSE(result.clicked);
        REQUIRE_FALSE(result.hovered);
        REQUIRE_FALSE(result.held);
    }

    SECTION("Button hover state") {
        UiContext ctx;
        ctx.set_mouse_position(50.0f, 20.0f);
        ctx.begin_frame();

        ButtonResult result = Button::draw(ctx, 10, 10, "Hover Test");

        REQUIRE(result.hovered);
    }

    SECTION("Button click state") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 20.0f);
        ctx.end_frame();

        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 20.0f);
        ctx.set_mouse_button(0, true);

        ButtonResult result = Button::draw(ctx, 10, 10, "Click Test");

        REQUIRE(result.clicked);
        REQUIRE(result.hovered);
    }

    SECTION("Disabled button") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 20.0f);
        ctx.end_frame();

        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 20.0f);
        ctx.set_mouse_button(0, true);

        ButtonConfig config;
        config.enabled = false;

        ButtonResult result = Button::draw(ctx, 10, 10, "Disabled", config);

        REQUIRE_FALSE(result.clicked); // Click ignored
    }

    SECTION("Button with ID") {
        UiContext ctx;
        ctx.begin_frame();

        ButtonResult result = Button::draw(ctx, 12345, 10, 10, "ID Button");

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("Checkbox widget", "[ui][widgets][checkbox]") {
    SECTION("Draw unchecked checkbox") {
        UiContext ctx;
        ctx.begin_frame();

        CheckboxResult result = Checkbox::draw(ctx, 10, 10, "Option", false);

        REQUIRE_FALSE(ctx.draw_data().empty());
        REQUIRE_FALSE(result.checked);
        REQUIRE_FALSE(result.changed);
    }

    SECTION("Draw checked checkbox") {
        UiContext ctx;
        ctx.begin_frame();

        CheckboxResult result = Checkbox::draw(ctx, 10, 10, "Option", true);

        REQUIRE(result.checked);
    }

    SECTION("Checkbox toggle on click") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_mouse_position(20.0f, 15.0f);
        ctx.end_frame();

        ctx.begin_frame();
        ctx.set_mouse_position(20.0f, 15.0f);
        ctx.set_mouse_button(0, true);

        CheckboxResult result = Checkbox::draw(ctx, 10, 10, "Toggle", false);

        REQUIRE(result.changed);
        REQUIRE(result.checked); // Was false, now true
    }
}

TEST_CASE("Slider widget", "[ui][widgets][slider]") {
    SECTION("Draw slider") {
        UiContext ctx;
        ctx.begin_frame();

        SliderResult result = Slider::draw(ctx, 10, 10, "Volume", 0.5f);

        REQUIRE_FALSE(ctx.draw_data().empty());
        REQUIRE(result.value == Approx(0.5f));
        REQUIRE_FALSE(result.changed);
        REQUIRE_FALSE(result.dragging);
    }

    SECTION("Slider with config") {
        UiContext ctx;
        ctx.begin_frame();

        SliderConfig config;
        config.width = 300.0f;
        config.min_value = 0.0f;
        config.max_value = 100.0f;
        config.format = "%.0f%%";

        SliderResult result = Slider::draw(ctx, 10, 10, "Percent", 50.0f, config);

        REQUIRE(result.value == Approx(50.0f));
    }

    SECTION("Slider value clamping") {
        UiContext ctx;
        ctx.begin_frame();

        SliderConfig config;
        config.min_value = 0.0f;
        config.max_value = 1.0f;

        SliderResult result = Slider::draw(ctx, 10, 10, "Clamp", 2.0f, config);

        REQUIRE(result.value == Approx(1.0f));
    }
}

TEST_CASE("TextInput widget", "[ui][widgets][textinput]") {
    SECTION("Draw text input") {
        UiContext ctx;
        ctx.begin_frame();

        TextInputResult result = TextInput::draw(ctx, 10, 10, "Hello");

        REQUIRE_FALSE(ctx.draw_data().empty());
        REQUIRE(result.text == "Hello");
        REQUIRE_FALSE(result.changed);
        REQUIRE_FALSE(result.submitted);
        REQUIRE_FALSE(result.focused);
    }

    SECTION("Draw with placeholder") {
        UiContext ctx;
        ctx.begin_frame();

        TextInputConfig config;
        config.placeholder = "Enter text...";

        TextInputResult result = TextInput::draw(ctx, 10, 10, "", config);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Focus on click") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 15.0f);
        ctx.end_frame();

        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 15.0f);
        ctx.set_mouse_button(0, true);

        TextInputResult result = TextInput::draw(ctx, 10, 10, "Focus test");

        REQUIRE(result.focused);
    }

    SECTION("Password mode") {
        UiContext ctx;
        ctx.begin_frame();

        TextInputConfig config;
        config.password = true;

        TextInputResult result = TextInput::draw(ctx, 10, 10, "secret", config);

        REQUIRE_FALSE(ctx.draw_data().empty());
        // Text should be drawn as asterisks (visual verification)
    }
}

TEST_CASE("Panel widget", "[ui][widgets][panel]") {
    SECTION("Begin and end panel") {
        UiContext ctx;
        ctx.begin_frame();

        PanelConfig config;
        config.width = 300.0f;
        config.height = 200.0f;

        Rect content = Panel::begin(ctx, 10, 10, config);
        Panel::end(ctx);

        REQUIRE_FALSE(ctx.draw_data().empty());
        REQUIRE(content.width < config.width);  // Content area minus padding
        REQUIRE(content.height < config.height);
    }

    SECTION("Panel with title") {
        UiContext ctx;
        ctx.begin_frame();

        PanelConfig config;
        config.width = 300.0f;
        config.height = 200.0f;
        config.show_title = true;
        config.title = "Panel Title";

        Rect content = Panel::begin(ctx, 10, 10, config);
        Panel::end(ctx);

        // Content area should be smaller due to title
        REQUIRE(content.y > 10.0f);
    }

    SECTION("Panel without border") {
        UiContext ctx;
        ctx.begin_frame();

        PanelConfig config;
        config.width = 300.0f;
        config.height = 200.0f;
        config.show_border = false;

        Panel::begin(ctx, 10, 10, config);
        Panel::end(ctx);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("Separator widget", "[ui][widgets][separator]") {
    SECTION("Draw separator at cursor") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_cursor(10.0f, 50.0f);

        float old_y = ctx.cursor_y();
        Separator::draw(ctx);

        REQUIRE(ctx.cursor_y() > old_y);
    }

    SECTION("Draw separator at position") {
        UiContext ctx;
        ctx.begin_frame();

        Separator::draw(ctx, 10.0f, 50.0f, 200.0f);

        REQUIRE_FALSE(ctx.draw_data().empty());
    }
}

TEST_CASE("Spacing widget", "[ui][widgets][spacing]") {
    SECTION("Vertical spacing") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_cursor(10.0f, 50.0f);

        Spacing::vertical(ctx, 20.0f);

        REQUIRE(ctx.cursor_x() == 10.0f);
        REQUIRE(ctx.cursor_y() == 70.0f);
    }

    SECTION("Horizontal spacing") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_cursor(10.0f, 50.0f);

        Spacing::horizontal(ctx, 30.0f);

        REQUIRE(ctx.cursor_x() == 40.0f);
        REQUIRE(ctx.cursor_y() == 50.0f);
    }
}
