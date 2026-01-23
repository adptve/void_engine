/// @file test_context.cpp
/// @brief Tests for UI context

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <void_engine/ui/context.hpp>

using namespace void_ui;
using Catch::Approx;

TEST_CASE("UiContext construction", "[ui][context]") {
    SECTION("Default construction") {
        UiContext ctx;

        REQUIRE(ctx.screen_width() == 1280.0f);
        REQUIRE(ctx.screen_height() == 720.0f);
        REQUIRE(ctx.cursor_x() == 0.0f);
        REQUIRE(ctx.cursor_y() == 0.0f);
    }

    SECTION("Is movable") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);

        UiContext moved = std::move(ctx);
        REQUIRE(moved.screen_width() == 1920.0f);
    }
}

TEST_CASE("UiContext screen size", "[ui][context]") {
    SECTION("Set screen size") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);

        REQUIRE(ctx.screen_width() == 1920.0f);
        REQUIRE(ctx.screen_height() == 1080.0f);
    }

    SECTION("Screen size as Size struct") {
        UiContext ctx;
        ctx.set_screen_size(1920.0f, 1080.0f);

        Size size = ctx.screen_size();
        REQUIRE(size.width == 1920.0f);
        REQUIRE(size.height == 1080.0f);
    }
}

TEST_CASE("UiContext theme management", "[ui][context]") {
    SECTION("Default theme is dark") {
        UiContext ctx;
        REQUIRE(ctx.theme().name == "dark");
    }

    SECTION("Set theme") {
        UiContext ctx;
        ctx.set_theme(Theme::light());

        REQUIRE(ctx.theme().name == "light");
    }

    SECTION("Mutable theme access") {
        UiContext ctx;
        ctx.theme().name = "modified";

        REQUIRE(ctx.theme().name == "modified");
    }
}

TEST_CASE("UiContext font management", "[ui][context]") {
    SECTION("Default font is builtin") {
        UiContext ctx;
        REQUIRE(ctx.font().name() == "builtin");
    }

    SECTION("Set font") {
        UiContext ctx;
        BitmapFont font = BitmapFont::create_builtin();
        font.set_name("custom");

        ctx.set_font(std::move(font));
        REQUIRE(ctx.font().name() == "custom");
    }
}

TEST_CASE("UiContext cursor management", "[ui][context]") {
    SECTION("Set cursor position") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 200.0f);

        REQUIRE(ctx.cursor_x() == 100.0f);
        REQUIRE(ctx.cursor_y() == 200.0f);
    }

    SECTION("Set cursor with Point") {
        UiContext ctx;
        ctx.set_cursor(Point(50.0f, 75.0f));

        REQUIRE(ctx.cursor().x == 50.0f);
        REQUIRE(ctx.cursor().y == 75.0f);
    }

    SECTION("Advance cursor") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 100.0f);
        ctx.advance_cursor(25.0f, 50.0f);

        REQUIRE(ctx.cursor_x() == 125.0f);
        REQUIRE(ctx.cursor_y() == 150.0f);
    }

    SECTION("Newline advances to next line") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 50.0f);

        float line_h = ctx.line_height();
        ctx.newline();

        REQUIRE(ctx.cursor_x() == 0.0f);
        REQUIRE(ctx.cursor_y() == Approx(50.0f + line_h));
    }

    SECTION("Newline with custom height") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 50.0f);
        ctx.newline(30.0f);

        REQUIRE(ctx.cursor_x() == 0.0f);
        REQUIRE(ctx.cursor_y() == 80.0f);
    }

    SECTION("Push and pop cursor") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 200.0f);

        ctx.push_cursor();
        ctx.set_cursor(0.0f, 0.0f);
        REQUIRE(ctx.cursor_x() == 0.0f);

        ctx.pop_cursor();
        REQUIRE(ctx.cursor_x() == 100.0f);
        REQUIRE(ctx.cursor_y() == 200.0f);
    }

    SECTION("Pop cursor on empty stack does nothing") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 200.0f);
        ctx.pop_cursor(); // Should not crash

        REQUIRE(ctx.cursor_x() == 100.0f);
    }
}

TEST_CASE("UiContext frame management", "[ui][context]") {
    SECTION("Begin frame clears draw data") {
        UiContext ctx;
        ctx.begin_frame();

        // Draw something
        ctx.draw_rect(0, 0, 100, 100, Color::red());
        REQUIRE_FALSE(ctx.draw_data().empty());

        ctx.begin_frame();
        REQUIRE(ctx.draw_data().empty());
    }

    SECTION("Begin frame resets cursor") {
        UiContext ctx;
        ctx.set_cursor(100.0f, 200.0f);
        ctx.begin_frame();

        REQUIRE(ctx.cursor_x() == 0.0f);
        REQUIRE(ctx.cursor_y() == 0.0f);
    }
}

TEST_CASE("UiContext clipping", "[ui][context]") {
    SECTION("Default clip rect is full screen") {
        UiContext ctx;
        ctx.begin_frame();

        Rect clip = ctx.current_clip_rect();
        REQUIRE(clip.x == 0.0f);
        REQUIRE(clip.y == 0.0f);
        REQUIRE(clip.width == ctx.screen_width());
        REQUIRE(clip.height == ctx.screen_height());
    }

    SECTION("Push clip rect") {
        UiContext ctx;
        ctx.begin_frame();

        Rect new_clip{100.0f, 100.0f, 200.0f, 150.0f};
        ctx.push_clip_rect(new_clip);

        Rect clip = ctx.current_clip_rect();
        REQUIRE(clip.x == 100.0f);
        REQUIRE(clip.y == 100.0f);
        REQUIRE(clip.width == 200.0f);
        REQUIRE(clip.height == 150.0f);
    }

    SECTION("Pop clip rect") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.push_clip_rect(Rect{100.0f, 100.0f, 200.0f, 150.0f});
        ctx.pop_clip_rect();

        Rect clip = ctx.current_clip_rect();
        REQUIRE(clip.width == ctx.screen_width());
    }

    SECTION("Nested clip rects intersect") {
        UiContext ctx;
        ctx.begin_frame();

        // First clip: 0-500
        ctx.push_clip_rect(Rect{0.0f, 0.0f, 500.0f, 500.0f});

        // Second clip: 200-700 (overlaps 200-500)
        ctx.push_clip_rect(Rect{200.0f, 200.0f, 500.0f, 500.0f});

        Rect clip = ctx.current_clip_rect();
        REQUIRE(clip.x == 200.0f);
        REQUIRE(clip.y == 200.0f);
        REQUIRE(clip.width == 300.0f);  // 500 - 200
        REQUIRE(clip.height == 300.0f);
    }
}

TEST_CASE("UiContext drawing", "[ui][context]") {
    SECTION("Draw rect adds vertices and indices") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_rect(10, 20, 100, 50, Color::red());

        const auto& data = ctx.draw_data();
        REQUIRE(data.vertices.size() == 4);
        REQUIRE(data.indices.size() == 6);
    }

    SECTION("Draw rect with Rect struct") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_rect(Rect{10, 20, 100, 50}, Color::red());

        REQUIRE(ctx.draw_data().vertices.size() == 4);
    }

    SECTION("Draw rect border") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_rect_border(10, 20, 100, 50, Color::red());

        // Border = 4 rectangles
        const auto& data = ctx.draw_data();
        REQUIRE(data.vertices.size() == 16); // 4 rects * 4 verts
    }

    SECTION("Draw line") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_line(Point{0, 0}, Point{100, 100}, Color::red(), 2.0f);

        REQUIRE(ctx.draw_data().vertices.size() == 4);
    }

    SECTION("Transparent color skips drawing") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_rect(0, 0, 100, 100, Color::transparent());

        REQUIRE(ctx.draw_data().empty());
    }

    SECTION("Clipped rect is not drawn") {
        UiContext ctx;
        ctx.begin_frame();

        // Clip to small region
        ctx.push_clip_rect(Rect{0, 0, 50, 50});

        // Draw outside clip region
        ctx.draw_rect(100, 100, 50, 50, Color::red());

        REQUIRE(ctx.draw_data().empty());
    }
}

TEST_CASE("UiContext text drawing", "[ui][context]") {
    SECTION("Draw text adds vertices") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_text("A", 0, 0, Color::white(), 1.0f);

        // 'A' has pixels, should generate vertices
        REQUIRE_FALSE(ctx.draw_data().empty());
    }

    SECTION("Empty text draws nothing") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.draw_text("", 0, 0, Color::white(), 1.0f);

        REQUIRE(ctx.draw_data().empty());
    }

    SECTION("Measure text") {
        UiContext ctx;

        float width = ctx.measure_text("Hello", 1.0f);
        REQUIRE(width > 0.0f);
    }

    SECTION("Text height") {
        UiContext ctx;

        float height = ctx.text_height(1.0f);
        REQUIRE(height > 0.0f);
    }
}

TEST_CASE("UiContext input handling", "[ui][context]") {
    SECTION("Set mouse position") {
        UiContext ctx;
        ctx.set_mouse_position(100.0f, 200.0f);

        Point pos = ctx.mouse_position();
        REQUIRE(pos.x == 100.0f);
        REQUIRE(pos.y == 200.0f);
    }

    SECTION("Mouse button state") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.set_mouse_button(0, true);
        REQUIRE(ctx.is_mouse_down(0));

        ctx.set_mouse_button(0, false);
        REQUIRE_FALSE(ctx.is_mouse_down(0));
    }

    SECTION("Mouse pressed detection") {
        UiContext ctx;

        // Frame 1: button up
        ctx.begin_frame();
        ctx.set_mouse_button(0, false);
        ctx.end_frame();

        // Frame 2: button down
        ctx.begin_frame();
        ctx.set_mouse_button(0, true);

        REQUIRE(ctx.is_mouse_pressed(0));
        REQUIRE_FALSE(ctx.is_mouse_released(0));
    }

    SECTION("Mouse released detection") {
        UiContext ctx;

        // Frame 1: button down
        ctx.begin_frame();
        ctx.set_mouse_button(0, true);
        ctx.end_frame();

        // Frame 2: button up
        ctx.begin_frame();
        ctx.set_mouse_button(0, false);

        REQUIRE_FALSE(ctx.is_mouse_pressed(0));
        REQUIRE(ctx.is_mouse_released(0));
    }

    SECTION("Is hovered") {
        UiContext ctx;
        ctx.set_mouse_position(50.0f, 50.0f);

        Rect inside{0, 0, 100, 100};
        Rect outside{200, 200, 100, 100};

        REQUIRE(ctx.is_hovered(inside));
        REQUIRE_FALSE(ctx.is_hovered(outside));
    }

    SECTION("Is clicked") {
        UiContext ctx;
        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 50.0f);
        ctx.end_frame();

        ctx.begin_frame();
        ctx.set_mouse_position(50.0f, 50.0f);
        ctx.set_mouse_button(0, true);

        Rect rect{0, 0, 100, 100};
        REQUIRE(ctx.is_clicked(rect, 0));
    }
}

TEST_CASE("UiContext widget ID management", "[ui][context]") {
    SECTION("Initial ID is zero") {
        UiContext ctx;
        ctx.begin_frame();

        REQUIRE(ctx.current_id() == 0);
    }

    SECTION("Push numeric ID") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.push_id(42);
        REQUIRE(ctx.current_id() != 0);
        REQUIRE(ctx.current_id() != 42); // Combined with parent

        ctx.pop_id();
        REQUIRE(ctx.current_id() == 0);
    }

    SECTION("Push string ID") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.push_id("button1");
        std::uint64_t id1 = ctx.current_id();
        ctx.pop_id();

        ctx.push_id("button2");
        std::uint64_t id2 = ctx.current_id();

        REQUIRE(id1 != id2);
    }

    SECTION("Nested IDs are combined") {
        UiContext ctx;
        ctx.begin_frame();

        ctx.push_id("parent");
        std::uint64_t parent_id = ctx.current_id();

        ctx.push_id("child");
        std::uint64_t child_id = ctx.current_id();

        REQUIRE(child_id != parent_id);

        ctx.pop_id();
        REQUIRE(ctx.current_id() == parent_id);
    }
}

TEST_CASE("UiContext focus management", "[ui][context]") {
    SECTION("No focus by default") {
        UiContext ctx;
        REQUIRE(ctx.focused_widget() == 0);
        REQUIRE_FALSE(ctx.is_focused(1));
    }

    SECTION("Set focus") {
        UiContext ctx;
        ctx.set_focus(42);

        REQUIRE(ctx.focused_widget() == 42);
        REQUIRE(ctx.is_focused(42));
        REQUIRE_FALSE(ctx.is_focused(0));
        REQUIRE_FALSE(ctx.is_focused(1));
    }

    SECTION("Clear focus") {
        UiContext ctx;
        ctx.set_focus(42);
        ctx.clear_focus();

        REQUIRE(ctx.focused_widget() == 0);
        REQUIRE_FALSE(ctx.is_focused(42));
    }
}
