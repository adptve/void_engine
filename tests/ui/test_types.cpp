/// @file test_types.cpp
/// @brief Tests for UI core types

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <void_engine/ui/types.hpp>

using namespace void_ui;
using Catch::Approx;

TEST_CASE("Color construction and conversion", "[ui][types][color]") {
    SECTION("Default constructor") {
        Color c;
        REQUIRE(c.r == 0.0f);
        REQUIRE(c.g == 0.0f);
        REQUIRE(c.b == 0.0f);
        REQUIRE(c.a == 1.0f);
    }

    SECTION("RGBA constructor") {
        Color c(0.5f, 0.6f, 0.7f, 0.8f);
        REQUIRE(c.r == Approx(0.5f));
        REQUIRE(c.g == Approx(0.6f));
        REQUIRE(c.b == Approx(0.7f));
        REQUIRE(c.a == Approx(0.8f));
    }

    SECTION("From RGB8") {
        Color c = Color::from_rgb8(255, 128, 0, 255);
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(128.0f / 255.0f));
        REQUIRE(c.b == Approx(0.0f));
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("From hex RGB") {
        Color c = Color::from_hex(0xFF8000);
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(128.0f / 255.0f));
        REQUIRE(c.b == Approx(0.0f));
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("From hex RGBA") {
        Color c = Color::from_hex(0xFF800080);
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(128.0f / 255.0f));
        REQUIRE(c.b == Approx(0.0f));
        REQUIRE(c.a == Approx(128.0f / 255.0f));
    }

    SECTION("To array") {
        Color c(0.1f, 0.2f, 0.3f, 0.4f);
        auto arr = c.to_array();
        REQUIRE(arr[0] == Approx(0.1f));
        REQUIRE(arr[1] == Approx(0.2f));
        REQUIRE(arr[2] == Approx(0.3f));
        REQUIRE(arr[3] == Approx(0.4f));
    }

    SECTION("Common colors") {
        REQUIRE(Color::white().r == 1.0f);
        REQUIRE(Color::black().r == 0.0f);
        REQUIRE(Color::red().r == 1.0f);
        REQUIRE(Color::red().g == 0.0f);
        REQUIRE(Color::transparent().a == 0.0f);
    }
}

TEST_CASE("Color operations", "[ui][types][color]") {
    SECTION("Lerp") {
        Color a(0.0f, 0.0f, 0.0f, 1.0f);
        Color b(1.0f, 1.0f, 1.0f, 1.0f);

        Color mid = Color::lerp(a, b, 0.5f);
        REQUIRE(mid.r == Approx(0.5f));
        REQUIRE(mid.g == Approx(0.5f));
        REQUIRE(mid.b == Approx(0.5f));

        Color start = Color::lerp(a, b, 0.0f);
        REQUIRE(start.r == Approx(0.0f));

        Color end = Color::lerp(a, b, 1.0f);
        REQUIRE(end.r == Approx(1.0f));
    }

    SECTION("Lerp clamping") {
        Color a(0.0f, 0.0f, 0.0f, 1.0f);
        Color b(1.0f, 1.0f, 1.0f, 1.0f);

        Color under = Color::lerp(a, b, -1.0f);
        REQUIRE(under.r == Approx(0.0f));

        Color over = Color::lerp(a, b, 2.0f);
        REQUIRE(over.r == Approx(1.0f));
    }

    SECTION("Brighten") {
        Color c(0.5f, 0.5f, 0.5f, 1.0f);
        Color bright = c.brighten(0.2f);
        REQUIRE(bright.r == Approx(0.7f));
        REQUIRE(bright.a == Approx(1.0f)); // Alpha unchanged
    }

    SECTION("Brighten clamping") {
        Color c(0.9f, 0.9f, 0.9f, 1.0f);
        Color bright = c.brighten(0.5f);
        REQUIRE(bright.r == Approx(1.0f));
    }

    SECTION("Darken") {
        Color c(0.5f, 0.5f, 0.5f, 1.0f);
        Color dark = c.darken(0.2f);
        REQUIRE(dark.r == Approx(0.3f));
    }

    SECTION("Darken clamping") {
        Color c(0.1f, 0.1f, 0.1f, 1.0f);
        Color dark = c.darken(0.5f);
        REQUIRE(dark.r == Approx(0.0f));
    }

    SECTION("With alpha") {
        Color c(1.0f, 0.5f, 0.25f, 1.0f);
        Color transparent = c.with_alpha(0.5f);
        REQUIRE(transparent.r == Approx(1.0f));
        REQUIRE(transparent.g == Approx(0.5f));
        REQUIRE(transparent.b == Approx(0.25f));
        REQUIRE(transparent.a == Approx(0.5f));
    }
}

TEST_CASE("Point operations", "[ui][types][point]") {
    SECTION("Default constructor") {
        Point p;
        REQUIRE(p.x == 0.0f);
        REQUIRE(p.y == 0.0f);
    }

    SECTION("XY constructor") {
        Point p(10.0f, 20.0f);
        REQUIRE(p.x == 10.0f);
        REQUIRE(p.y == 20.0f);
    }

    SECTION("Addition") {
        Point a(1.0f, 2.0f);
        Point b(3.0f, 4.0f);
        Point c = a + b;
        REQUIRE(c.x == 4.0f);
        REQUIRE(c.y == 6.0f);
    }

    SECTION("Subtraction") {
        Point a(5.0f, 7.0f);
        Point b(2.0f, 3.0f);
        Point c = a - b;
        REQUIRE(c.x == 3.0f);
        REQUIRE(c.y == 4.0f);
    }

    SECTION("Scalar multiplication") {
        Point p(2.0f, 3.0f);
        Point scaled = p * 2.0f;
        REQUIRE(scaled.x == 4.0f);
        REQUIRE(scaled.y == 6.0f);
    }
}

TEST_CASE("Size operations", "[ui][types][size]") {
    SECTION("Default constructor") {
        Size s;
        REQUIRE(s.width == 0.0f);
        REQUIRE(s.height == 0.0f);
    }

    SECTION("WH constructor") {
        Size s(100.0f, 50.0f);
        REQUIRE(s.width == 100.0f);
        REQUIRE(s.height == 50.0f);
    }

    SECTION("Area") {
        Size s(10.0f, 20.0f);
        REQUIRE(s.area() == 200.0f);
    }

    SECTION("Is empty") {
        REQUIRE(Size(0.0f, 10.0f).is_empty());
        REQUIRE(Size(10.0f, 0.0f).is_empty());
        REQUIRE(Size(-1.0f, 10.0f).is_empty());
        REQUIRE_FALSE(Size(10.0f, 10.0f).is_empty());
    }
}

TEST_CASE("Rect operations", "[ui][types][rect]") {
    SECTION("Default constructor") {
        Rect r;
        REQUIRE(r.x == 0.0f);
        REQUIRE(r.y == 0.0f);
        REQUIRE(r.width == 0.0f);
        REQUIRE(r.height == 0.0f);
    }

    SECTION("XYWH constructor") {
        Rect r(10.0f, 20.0f, 100.0f, 50.0f);
        REQUIRE(r.x == 10.0f);
        REQUIRE(r.y == 20.0f);
        REQUIRE(r.width == 100.0f);
        REQUIRE(r.height == 50.0f);
    }

    SECTION("Point/Size constructor") {
        Rect r(Point(10.0f, 20.0f), Size(100.0f, 50.0f));
        REQUIRE(r.x == 10.0f);
        REQUIRE(r.y == 20.0f);
        REQUIRE(r.width == 100.0f);
        REQUIRE(r.height == 50.0f);
    }

    SECTION("Position and size getters") {
        Rect r(10.0f, 20.0f, 100.0f, 50.0f);
        REQUIRE(r.position().x == 10.0f);
        REQUIRE(r.position().y == 20.0f);
        REQUIRE(r.size().width == 100.0f);
        REQUIRE(r.size().height == 50.0f);
    }

    SECTION("Edge getters") {
        Rect r(10.0f, 20.0f, 100.0f, 50.0f);
        REQUIRE(r.left() == 10.0f);
        REQUIRE(r.right() == 110.0f);
        REQUIRE(r.top() == 20.0f);
        REQUIRE(r.bottom() == 70.0f);
    }

    SECTION("Center") {
        Rect r(0.0f, 0.0f, 100.0f, 50.0f);
        Point c = r.center();
        REQUIRE(c.x == 50.0f);
        REQUIRE(c.y == 25.0f);
    }

    SECTION("Contains point") {
        Rect r(10.0f, 10.0f, 100.0f, 50.0f);

        REQUIRE(r.contains(Point(50.0f, 30.0f)));
        REQUIRE(r.contains(10.0f, 10.0f));    // Top-left corner
        REQUIRE(r.contains(110.0f, 60.0f));   // Bottom-right corner

        REQUIRE_FALSE(r.contains(Point(0.0f, 0.0f)));
        REQUIRE_FALSE(r.contains(Point(200.0f, 100.0f)));
    }

    SECTION("Intersects") {
        Rect a(0.0f, 0.0f, 100.0f, 100.0f);
        Rect b(50.0f, 50.0f, 100.0f, 100.0f);
        Rect c(200.0f, 200.0f, 100.0f, 100.0f);

        REQUIRE(a.intersects(b));
        REQUIRE(b.intersects(a));
        REQUIRE_FALSE(a.intersects(c));
    }

    SECTION("Expand") {
        Rect r(10.0f, 10.0f, 100.0f, 50.0f);
        Rect expanded = r.expand(5.0f);

        REQUIRE(expanded.x == 5.0f);
        REQUIRE(expanded.y == 5.0f);
        REQUIRE(expanded.width == 110.0f);
        REQUIRE(expanded.height == 60.0f);
    }

    SECTION("Shrink") {
        Rect r(10.0f, 10.0f, 100.0f, 50.0f);
        Rect shrunk = r.shrink(5.0f);

        REQUIRE(shrunk.x == 15.0f);
        REQUIRE(shrunk.y == 15.0f);
        REQUIRE(shrunk.width == 90.0f);
        REQUIRE(shrunk.height == 40.0f);
    }
}

TEST_CASE("UiDrawData operations", "[ui][types][drawdata]") {
    SECTION("Default is empty") {
        UiDrawData data;
        REQUIRE(data.empty());
    }

    SECTION("Clear") {
        UiDrawData data;
        data.vertices.push_back(UiVertex{});
        data.indices.push_back(0);
        data.commands.push_back(UiDrawCommand{});

        REQUIRE_FALSE(data.empty());

        data.clear();
        REQUIRE(data.empty());
        REQUIRE(data.vertices.empty());
        REQUIRE(data.indices.empty());
        REQUIRE(data.commands.empty());
    }
}

TEST_CASE("LayoutConstraints", "[ui][types][layout]") {
    SECTION("Constrain within bounds") {
        LayoutConstraints c{50.0f, 200.0f, 30.0f, 100.0f};

        Size s1 = c.constrain(Size(100.0f, 50.0f));
        REQUIRE(s1.width == 100.0f);
        REQUIRE(s1.height == 50.0f);
    }

    SECTION("Constrain to minimum") {
        LayoutConstraints c{50.0f, 200.0f, 30.0f, 100.0f};

        Size s = c.constrain(Size(10.0f, 10.0f));
        REQUIRE(s.width == 50.0f);
        REQUIRE(s.height == 30.0f);
    }

    SECTION("Constrain to maximum") {
        LayoutConstraints c{50.0f, 200.0f, 30.0f, 100.0f};

        Size s = c.constrain(Size(500.0f, 500.0f));
        REQUIRE(s.width == 200.0f);
        REQUIRE(s.height == 100.0f);
    }
}
