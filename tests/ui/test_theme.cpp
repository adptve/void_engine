/// @file test_theme.cpp
/// @brief Tests for UI theme system

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <void_engine/ui/theme.hpp>

using namespace void_ui;
using Catch::Approx;

TEST_CASE("Theme colors interpolation", "[ui][theme]") {
    SECTION("Lerp at t=0") {
        ThemeColors a = Theme::dark().colors;
        ThemeColors b = Theme::light().colors;

        ThemeColors result = ThemeColors::lerp(a, b, 0.0f);
        REQUIRE(result.panel_bg.r == Approx(a.panel_bg.r));
        REQUIRE(result.text.r == Approx(a.text.r));
    }

    SECTION("Lerp at t=1") {
        ThemeColors a = Theme::dark().colors;
        ThemeColors b = Theme::light().colors;

        ThemeColors result = ThemeColors::lerp(a, b, 1.0f);
        REQUIRE(result.panel_bg.r == Approx(b.panel_bg.r));
        REQUIRE(result.text.r == Approx(b.text.r));
    }

    SECTION("Lerp at t=0.5") {
        ThemeColors a = Theme::dark().colors;
        ThemeColors b = Theme::light().colors;

        ThemeColors result = ThemeColors::lerp(a, b, 0.5f);

        // Should be midway between dark and light
        float expected_bg = (a.panel_bg.r + b.panel_bg.r) / 2.0f;
        REQUIRE(result.panel_bg.r == Approx(expected_bg));
    }
}

TEST_CASE("Built-in themes", "[ui][theme]") {
    SECTION("Dark theme") {
        Theme theme = Theme::dark();

        REQUIRE(theme.name == "dark");
        REQUIRE(theme.colors.panel_bg.r < 0.3f); // Dark background
        REQUIRE(theme.colors.text.r > 0.7f);       // Light text
    }

    SECTION("Light theme") {
        Theme theme = Theme::light();

        REQUIRE(theme.name == "light");
        REQUIRE(theme.colors.panel_bg.r > 0.8f); // Light background
        REQUIRE(theme.colors.text.r < 0.3f);       // Dark text
    }

    SECTION("High contrast theme") {
        Theme theme = Theme::high_contrast();

        REQUIRE(theme.name == "high_contrast");
        // High contrast has pure black background
        REQUIRE(theme.colors.panel_bg.r == 0.0f);
        REQUIRE(theme.colors.panel_bg.g == 0.0f);
        REQUIRE(theme.colors.panel_bg.b == 0.0f);
        // And pure white text
        REQUIRE(theme.colors.text.r == 1.0f);
    }

    SECTION("Retro theme") {
        Theme theme = Theme::retro();

        REQUIRE(theme.name == "retro");
        // Retro has amber text color
        REQUIRE(theme.colors.text.r > theme.colors.text.b);
    }

    SECTION("Solarized dark theme") {
        Theme theme = Theme::solarized_dark();

        REQUIRE(theme.name == "solarized_dark");
        // Solarized uses specific base colors
        REQUIRE(theme.colors.panel_bg.r > 0.0f);
        REQUIRE(theme.colors.panel_bg.r < 0.1f);
    }

    SECTION("Solarized light theme") {
        Theme theme = Theme::solarized_light();

        REQUIRE(theme.name == "solarized_light");
        // Light solarized has lighter background
        REQUIRE(theme.colors.panel_bg.r > 0.9f);
    }
}

TEST_CASE("Theme interpolation", "[ui][theme]") {
    SECTION("Theme lerp") {
        Theme dark = Theme::dark();
        Theme light = Theme::light();

        Theme mid = Theme::lerp(dark, light, 0.5f);

        // Colors should be interpolated
        float expected = (dark.colors.panel_bg.r + light.colors.panel_bg.r) / 2.0f;
        REQUIRE(mid.colors.panel_bg.r == Approx(expected));

        // Scale values should be interpolated
        float expected_scale = (dark.text_scale + light.text_scale) / 2.0f;
        REQUIRE(mid.text_scale == Approx(expected_scale));
    }

    SECTION("Theme lerp preserves name") {
        Theme dark = Theme::dark();
        Theme light = Theme::light();

        Theme mid = Theme::lerp(dark, light, 0.5f);
        REQUIRE(mid.name == dark.name); // Preserves first theme's name
    }
}

TEST_CASE("ThemeRegistry basic operations", "[ui][theme][registry]") {
    SECTION("Default themes registered") {
        ThemeRegistry registry;

        REQUIRE(registry.has_theme("dark"));
        REQUIRE(registry.has_theme("light"));
        REQUIRE(registry.has_theme("high_contrast"));
        REQUIRE(registry.has_theme("retro"));
        REQUIRE(registry.has_theme("solarized_dark"));
        REQUIRE(registry.has_theme("solarized_light"));
    }

    SECTION("Default active theme is dark") {
        ThemeRegistry registry;

        REQUIRE(registry.active_theme_name() == "dark");
    }

    SECTION("Get theme by name") {
        ThemeRegistry registry;

        const Theme* dark = registry.get_theme("dark");
        REQUIRE(dark != nullptr);
        REQUIRE(dark->name == "dark");

        const Theme* nonexistent = registry.get_theme("nonexistent");
        REQUIRE(nonexistent == nullptr);
    }

    SECTION("Set active theme") {
        ThemeRegistry registry;

        registry.set_active_theme("light");
        REQUIRE(registry.active_theme_name() == "light");
        REQUIRE(registry.active_theme().name == "light");
    }

    SECTION("Set active theme ignores invalid name") {
        ThemeRegistry registry;

        registry.set_active_theme("nonexistent");
        REQUIRE(registry.active_theme_name() == "dark"); // Unchanged
    }

    SECTION("List theme names") {
        ThemeRegistry registry;

        auto names = registry.theme_names();
        REQUIRE(names.size() >= 6);
        REQUIRE(std::find(names.begin(), names.end(), "dark") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "light") != names.end());
    }
}

TEST_CASE("ThemeRegistry custom themes", "[ui][theme][registry]") {
    SECTION("Register custom theme") {
        ThemeRegistry registry;

        Theme custom = Theme::dark();
        custom.name = "custom";
        custom.colors.accent = Color::red();

        registry.register_theme("custom", custom);

        REQUIRE(registry.has_theme("custom"));
        const Theme* retrieved = registry.get_theme("custom");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->colors.accent.r == 1.0f);
    }

    SECTION("Unregister theme") {
        ThemeRegistry registry;

        Theme custom = Theme::dark();
        custom.name = "custom";
        registry.register_theme("custom", custom);
        REQUIRE(registry.has_theme("custom"));

        registry.unregister_theme("custom");
        REQUIRE_FALSE(registry.has_theme("custom"));
    }

    SECTION("Cannot unregister active theme") {
        ThemeRegistry registry;

        registry.set_active_theme("light");
        registry.unregister_theme("light"); // Should be ignored
        REQUIRE(registry.has_theme("light"));
    }

    SECTION("Unregistering active theme falls back to dark") {
        ThemeRegistry registry;

        Theme custom = Theme::dark();
        custom.name = "custom";
        registry.register_theme("custom", custom);
        registry.set_active_theme("custom");

        registry.unregister_theme("custom");
        REQUIRE(registry.active_theme_name() == "dark");
    }
}

TEST_CASE("ThemeRegistry transitions", "[ui][theme][registry]") {
    SECTION("Instant transition") {
        ThemeRegistry registry;
        registry.set_active_theme("dark");

        registry.transition_to("light", 0.0f);
        REQUIRE(registry.active_theme_name() == "light");
    }

    SECTION("Timed transition starts") {
        ThemeRegistry registry;
        registry.set_active_theme("dark");

        registry.transition_to("light", 1.0f);

        // Should still be "dark" until update is called
        // But transition is in progress
        REQUIRE(registry.is_transitioning());
    }

    SECTION("Update advances transition") {
        ThemeRegistry registry;
        registry.set_active_theme("dark");

        registry.transition_to("light", 1.0f);
        REQUIRE(registry.is_transitioning());

        // Update with full duration should complete transition
        registry.update_transition(1.0f);
        REQUIRE_FALSE(registry.is_transitioning());
        REQUIRE(registry.active_theme_name() == "light");
    }

    SECTION("Partial transition update") {
        ThemeRegistry registry;
        registry.set_active_theme("dark");

        Theme dark_theme = registry.active_theme();
        float dark_bg = dark_theme.colors.panel_bg.r;

        registry.transition_to("light", 1.0f);
        registry.update_transition(0.5f);

        // Theme should be partially transitioned
        const Theme& current = registry.active_theme();
        REQUIRE(current.colors.panel_bg.r > dark_bg);
        REQUIRE(registry.is_transitioning());
    }
}

TEST_CASE("ThemeRegistry callbacks", "[ui][theme][registry]") {
    SECTION("Theme changed callback is called") {
        ThemeRegistry registry;

        std::string changed_to;
        registry.set_theme_changed_callback([&changed_to](const std::string& name) {
            changed_to = name;
        });

        registry.set_active_theme("light");
        REQUIRE(changed_to == "light");
    }

    SECTION("Callback called after transition completes") {
        ThemeRegistry registry;

        std::string changed_to;
        registry.set_theme_changed_callback([&changed_to](const std::string& name) {
            changed_to = name;
        });

        registry.transition_to("light", 0.5f);
        REQUIRE(changed_to.empty()); // Not called yet

        registry.update_transition(0.5f);
        REQUIRE(changed_to == "light"); // Called when transition completes
    }
}
