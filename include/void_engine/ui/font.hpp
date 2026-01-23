#pragma once

/// @file font.hpp
/// @brief Bitmap font system with hot-reload support
///
/// Provides:
/// - Built-in 8x16 bitmap font (ASCII 32-127)
/// - Custom font loading from bitmap files
/// - Font registry with hot-reload

#include "fwd.hpp"
#include "types.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_ui {

// =============================================================================
// Glyph
// =============================================================================

/// Single glyph data
struct Glyph {
    /// Character code
    char32_t codepoint = 0;

    /// Bitmap data (row-major, 1 bit per pixel)
    /// For 8x16 font: 16 bytes, one per row
    std::vector<std::uint8_t> bitmap;

    /// Glyph width in pixels
    std::uint32_t width = 8;

    /// Glyph height in pixels
    std::uint32_t height = 16;

    /// Horizontal advance
    std::int32_t advance = 8;

    /// Bearing X (offset from cursor to left edge)
    std::int32_t bearing_x = 0;

    /// Bearing Y (offset from baseline to top edge)
    std::int32_t bearing_y = 0;

    /// Check if pixel is set
    [[nodiscard]] bool pixel_at(std::uint32_t x, std::uint32_t y) const {
        if (x >= width || y >= height) return false;
        std::size_t row = y;
        if (row >= bitmap.size()) return false;
        return (bitmap[row] >> (7 - x)) & 1;
    }
};

// =============================================================================
// Bitmap Font
// =============================================================================

/// 8x16 bitmap font with ASCII support
class BitmapFont {
public:
    /// Font glyph width in pixels
    static constexpr std::uint32_t GLYPH_WIDTH = 8;

    /// Font glyph height in pixels
    static constexpr std::uint32_t GLYPH_HEIGHT = 16;

    BitmapFont();
    ~BitmapFont();

    // Non-copyable but movable
    BitmapFont(const BitmapFont&) = delete;
    BitmapFont& operator=(const BitmapFont&) = delete;
    BitmapFont(BitmapFont&&) noexcept;
    BitmapFont& operator=(BitmapFont&&) noexcept;

    // =========================================================================
    // Font Loading
    // =========================================================================

    /// Create with built-in font
    static BitmapFont create_builtin();

    /// Load from a bitmap file (PNG, BMP, etc.)
    /// @param path Path to bitmap file
    /// @param glyph_width Width of each glyph
    /// @param glyph_height Height of each glyph
    /// @param chars_per_row Number of characters per row in the bitmap
    /// @param first_char First ASCII character in the bitmap
    static std::unique_ptr<BitmapFont> load_from_file(
        const std::string& path,
        std::uint32_t glyph_width = 8,
        std::uint32_t glyph_height = 16,
        std::uint32_t chars_per_row = 16,
        char first_char = ' ');

    /// Load from raw bitmap data
    static std::unique_ptr<BitmapFont> load_from_data(
        const std::uint8_t* data,
        std::size_t data_size,
        std::uint32_t image_width,
        std::uint32_t image_height,
        std::uint32_t glyph_width = 8,
        std::uint32_t glyph_height = 16,
        std::uint32_t chars_per_row = 16,
        char first_char = ' ');

    // =========================================================================
    // Glyph Access
    // =========================================================================

    /// Get glyph for a character
    [[nodiscard]] const Glyph* get_glyph(char32_t ch) const;

    /// Get glyph data for ASCII character (convenience for built-in font)
    [[nodiscard]] static const std::array<std::uint8_t, 16>& get_builtin_glyph(char ch);

    /// Check if font has a glyph for character
    [[nodiscard]] bool has_glyph(char32_t ch) const;

    // =========================================================================
    // Text Measurement
    // =========================================================================

    /// Measure text width in pixels
    [[nodiscard]] float measure_text(const std::string& text, float scale = 1.0f) const;

    /// Measure text width in pixels (wide string)
    [[nodiscard]] float measure_text(const std::u32string& text, float scale = 1.0f) const;

    /// Get text height (single line)
    [[nodiscard]] float text_height(float scale = 1.0f) const;

    /// Get line height (includes spacing)
    [[nodiscard]] float line_height(float scale = 1.0f, float line_height_mult = 1.4f) const;

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get glyph width
    [[nodiscard]] std::uint32_t glyph_width() const { return m_glyph_width; }

    /// Get glyph height
    [[nodiscard]] std::uint32_t glyph_height() const { return m_glyph_height; }

    /// Get font name
    [[nodiscard]] const std::string& name() const { return m_name; }

    /// Set font name
    void set_name(const std::string& name) { m_name = name; }

private:
    std::string m_name = "builtin";
    std::uint32_t m_glyph_width = GLYPH_WIDTH;
    std::uint32_t m_glyph_height = GLYPH_HEIGHT;
    std::unordered_map<char32_t, Glyph> m_glyphs;
    bool m_use_builtin = true;
};

// =============================================================================
// Font Registry (Hot-Reload Support)
// =============================================================================

/// Font registry with hot-reload support
class FontRegistry {
public:
    using FontChangedCallback = std::function<void(const std::string& font_name)>;

    FontRegistry();
    ~FontRegistry();

    // Non-copyable
    FontRegistry(const FontRegistry&) = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;

    // =========================================================================
    // Font Management
    // =========================================================================

    /// Register a font
    void register_font(const std::string& name, std::unique_ptr<BitmapFont> font);

    /// Unregister a font
    void unregister_font(const std::string& name);

    /// Get a font by name
    [[nodiscard]] const BitmapFont* get_font(const std::string& name) const;

    /// Get mutable font by name
    [[nodiscard]] BitmapFont* get_font_mut(const std::string& name);

    /// Get all registered font names
    [[nodiscard]] std::vector<std::string> font_names() const;

    /// Check if a font exists
    [[nodiscard]] bool has_font(const std::string& name) const;

    // =========================================================================
    // Active Font
    // =========================================================================

    /// Set the active font
    void set_active_font(const std::string& name);

    /// Get the active font
    [[nodiscard]] const BitmapFont& active_font() const;

    /// Get active font name
    [[nodiscard]] const std::string& active_font_name() const;

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Load a font from file and register it
    bool load_font_from_file(
        const std::string& name,
        const std::string& path,
        std::uint32_t glyph_width = 8,
        std::uint32_t glyph_height = 16);

    /// Watch a directory for font changes
    void watch_directory(const std::string& path);

    /// Stop watching for changes
    void stop_watching();

    /// Check for file changes and reload (call periodically)
    void poll_changes();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for font changes
    void set_font_changed_callback(FontChangedCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Built-in Font Data
// =============================================================================

/// Get built-in font data (96 glyphs for ASCII 32-127)
[[nodiscard]] const std::array<std::array<std::uint8_t, 16>, 96>& get_builtin_font_data();

} // namespace void_ui
