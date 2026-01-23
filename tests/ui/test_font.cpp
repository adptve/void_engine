/// @file test_font.cpp
/// @brief Tests for UI font system

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <void_engine/ui/font.hpp>

using namespace void_ui;
using Catch::Approx;

TEST_CASE("Glyph pixel access", "[ui][font][glyph]") {
    SECTION("Pixel at within bounds") {
        Glyph glyph;
        glyph.width = 8;
        glyph.height = 16;
        glyph.bitmap = {0xFF, 0x00, 0xAA}; // Row 0: all set, Row 1: none, Row 2: alternating

        // Row 0 (0xFF = 11111111) - all pixels set
        REQUIRE(glyph.pixel_at(0, 0));
        REQUIRE(glyph.pixel_at(7, 0));

        // Row 1 (0x00) - no pixels set
        REQUIRE_FALSE(glyph.pixel_at(0, 1));
        REQUIRE_FALSE(glyph.pixel_at(7, 1));

        // Row 2 (0xAA = 10101010)
        REQUIRE(glyph.pixel_at(0, 2));       // MSB set
        REQUIRE_FALSE(glyph.pixel_at(1, 2)); // Next bit not set
        REQUIRE(glyph.pixel_at(2, 2));       // Set
        REQUIRE_FALSE(glyph.pixel_at(3, 2)); // Not set
    }

    SECTION("Pixel at out of bounds returns false") {
        Glyph glyph;
        glyph.width = 8;
        glyph.height = 16;
        glyph.bitmap.resize(16, 0xFF);

        REQUIRE_FALSE(glyph.pixel_at(8, 0));   // x out of bounds
        REQUIRE_FALSE(glyph.pixel_at(0, 16));  // y out of bounds
        REQUIRE_FALSE(glyph.pixel_at(100, 0)); // Way out of bounds
    }
}

TEST_CASE("BitmapFont constants", "[ui][font]") {
    REQUIRE(BitmapFont::GLYPH_WIDTH == 8);
    REQUIRE(BitmapFont::GLYPH_HEIGHT == 16);
}

TEST_CASE("BitmapFont creation", "[ui][font]") {
    SECTION("Create builtin font") {
        BitmapFont font = BitmapFont::create_builtin();

        REQUIRE(font.name() == "builtin");
        REQUIRE(font.glyph_width() == 8);
        REQUIRE(font.glyph_height() == 16);
    }

    SECTION("Builtin font has ASCII glyphs") {
        BitmapFont font = BitmapFont::create_builtin();

        REQUIRE(font.has_glyph(' '));  // Space
        REQUIRE(font.has_glyph('A'));
        REQUIRE(font.has_glyph('Z'));
        REQUIRE(font.has_glyph('a'));
        REQUIRE(font.has_glyph('z'));
        REQUIRE(font.has_glyph('0'));
        REQUIRE(font.has_glyph('9'));
        REQUIRE(font.has_glyph('~'));  // Last printable ASCII
    }

    SECTION("Font is movable") {
        BitmapFont font = BitmapFont::create_builtin();
        BitmapFont moved = std::move(font);

        REQUIRE(moved.has_glyph('A'));
    }
}

TEST_CASE("BitmapFont glyph access", "[ui][font]") {
    SECTION("Get glyph for known character") {
        BitmapFont font = BitmapFont::create_builtin();

        const Glyph* glyph = font.get_glyph('A');
        REQUIRE(glyph != nullptr);
        REQUIRE(glyph->codepoint == U'A');
        REQUIRE(glyph->width == 8);
        REQUIRE(glyph->height == 16);
        REQUIRE(glyph->advance == 8);
    }

    SECTION("Get glyph for unknown character returns space") {
        BitmapFont font = BitmapFont::create_builtin();

        // Extended characters should fall back to space
        const Glyph* glyph = font.get_glyph(0x1234);
        REQUIRE(glyph != nullptr);
        REQUIRE(glyph->codepoint == U' ');
    }

    SECTION("Get builtin glyph static method") {
        const auto& glyph_a = BitmapFont::get_builtin_glyph('A');
        REQUIRE(glyph_a.size() == 16);

        // 'A' should have some set pixels
        bool has_pixels = false;
        for (auto byte : glyph_a) {
            if (byte != 0) {
                has_pixels = true;
                break;
            }
        }
        REQUIRE(has_pixels);

        // Space should be empty
        const auto& glyph_space = BitmapFont::get_builtin_glyph(' ');
        bool all_zero = true;
        for (auto byte : glyph_space) {
            if (byte != 0) {
                all_zero = false;
                break;
            }
        }
        REQUIRE(all_zero);
    }

    SECTION("Get builtin glyph for out of range returns space") {
        const auto& glyph = BitmapFont::get_builtin_glyph(0); // Control character
        // Should return space glyph (all zeros)
        bool all_zero = true;
        for (auto byte : glyph) {
            if (byte != 0) {
                all_zero = false;
                break;
            }
        }
        REQUIRE(all_zero);
    }
}

TEST_CASE("BitmapFont text measurement", "[ui][font]") {
    SECTION("Measure simple text") {
        BitmapFont font = BitmapFont::create_builtin();

        float width = font.measure_text("Hello", 1.0f);
        REQUIRE(width == Approx(5 * 8.0f)); // 5 chars * 8 pixels
    }

    SECTION("Measure text with scale") {
        BitmapFont font = BitmapFont::create_builtin();

        float width = font.measure_text("Hello", 2.0f);
        REQUIRE(width == Approx(5 * 8.0f * 2.0f));
    }

    SECTION("Measure empty text") {
        BitmapFont font = BitmapFont::create_builtin();

        float width = font.measure_text("", 1.0f);
        REQUIRE(width == 0.0f);
    }

    SECTION("Measure text with tabs") {
        BitmapFont font = BitmapFont::create_builtin();

        float width_no_tab = font.measure_text("A", 1.0f);
        float width_with_tab = font.measure_text("\t", 1.0f);

        REQUIRE(width_with_tab == Approx(width_no_tab * 4.0f)); // Tab = 4 spaces
    }

    SECTION("Newlines don't add width") {
        BitmapFont font = BitmapFont::create_builtin();

        float width1 = font.measure_text("Hello", 1.0f);
        float width2 = font.measure_text("Hello\n", 1.0f);

        REQUIRE(width1 == width2);
    }

    SECTION("Text height") {
        BitmapFont font = BitmapFont::create_builtin();

        float height = font.text_height(1.0f);
        REQUIRE(height == 16.0f);

        float scaled_height = font.text_height(2.0f);
        REQUIRE(scaled_height == 32.0f);
    }

    SECTION("Line height includes spacing") {
        BitmapFont font = BitmapFont::create_builtin();

        float text_h = font.text_height(1.0f);
        float line_h = font.line_height(1.0f, 1.4f);

        REQUIRE(line_h == Approx(text_h * 1.4f));
    }
}

TEST_CASE("FontRegistry basic operations", "[ui][font][registry]") {
    SECTION("Builtin font is registered") {
        FontRegistry registry;

        REQUIRE(registry.has_font("builtin"));
    }

    SECTION("Default active font is builtin") {
        FontRegistry registry;

        REQUIRE(registry.active_font_name() == "builtin");
    }

    SECTION("Get font by name") {
        FontRegistry registry;

        const BitmapFont* font = registry.get_font("builtin");
        REQUIRE(font != nullptr);
        REQUIRE(font->name() == "builtin");
    }

    SECTION("Get nonexistent font returns nullptr") {
        FontRegistry registry;

        const BitmapFont* font = registry.get_font("nonexistent");
        REQUIRE(font == nullptr);
    }

    SECTION("Active font reference is valid") {
        FontRegistry registry;

        const BitmapFont& font = registry.active_font();
        REQUIRE(font.has_glyph('A'));
    }

    SECTION("List font names") {
        FontRegistry registry;

        auto names = registry.font_names();
        REQUIRE(std::find(names.begin(), names.end(), "builtin") != names.end());
    }
}

TEST_CASE("FontRegistry custom fonts", "[ui][font][registry]") {
    SECTION("Register custom font") {
        FontRegistry registry;

        auto font = std::make_unique<BitmapFont>(BitmapFont::create_builtin());
        registry.register_font("custom", std::move(font));

        REQUIRE(registry.has_font("custom"));

        const BitmapFont* retrieved = registry.get_font("custom");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->name() == "custom");
    }

    SECTION("Unregister font") {
        FontRegistry registry;

        auto font = std::make_unique<BitmapFont>(BitmapFont::create_builtin());
        registry.register_font("custom", std::move(font));
        REQUIRE(registry.has_font("custom"));

        registry.unregister_font("custom");
        REQUIRE_FALSE(registry.has_font("custom"));
    }

    SECTION("Cannot unregister builtin font") {
        FontRegistry registry;

        registry.unregister_font("builtin");
        REQUIRE(registry.has_font("builtin")); // Should still exist
    }

    SECTION("Set active font") {
        FontRegistry registry;

        auto font = std::make_unique<BitmapFont>(BitmapFont::create_builtin());
        registry.register_font("custom", std::move(font));

        registry.set_active_font("custom");
        REQUIRE(registry.active_font_name() == "custom");
    }

    SECTION("Set active font ignores invalid name") {
        FontRegistry registry;

        registry.set_active_font("nonexistent");
        REQUIRE(registry.active_font_name() == "builtin"); // Unchanged
    }

    SECTION("Unregistering active font falls back to builtin") {
        FontRegistry registry;

        auto font = std::make_unique<BitmapFont>(BitmapFont::create_builtin());
        registry.register_font("custom", std::move(font));
        registry.set_active_font("custom");

        registry.unregister_font("custom");
        REQUIRE(registry.active_font_name() == "builtin");
    }
}

TEST_CASE("Builtin font data integrity", "[ui][font]") {
    SECTION("All 96 glyphs are present") {
        const auto& font_data = get_builtin_font_data();
        REQUIRE(font_data.size() == 96);
    }

    SECTION("Each glyph has 16 rows") {
        const auto& font_data = get_builtin_font_data();
        for (size_t i = 0; i < font_data.size(); ++i) {
            REQUIRE(font_data[i].size() == 16);
        }
    }

    SECTION("Some visible characters have non-zero data") {
        const auto& font_data = get_builtin_font_data();

        // 'A' is at index 33 (65 - 32)
        const auto& glyph_a = font_data[33];
        bool has_data = false;
        for (auto byte : glyph_a) {
            if (byte != 0) {
                has_data = true;
                break;
            }
        }
        REQUIRE(has_data);
    }

    SECTION("Space character is empty") {
        const auto& font_data = get_builtin_font_data();

        // Space is at index 0
        const auto& glyph_space = font_data[0];
        bool is_empty = true;
        for (auto byte : glyph_space) {
            if (byte != 0) {
                is_empty = false;
                break;
            }
        }
        REQUIRE(is_empty);
    }
}
