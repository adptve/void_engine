/// @file test_surface.cpp
/// @brief Tests for void_presenter surface types

#include <void_engine/presenter/surface.hpp>
#include <cassert>
#include <iostream>

using namespace void_presenter;

void test_surface_config() {
    std::cout << "  test_surface_config...";

    // Default config
    SurfaceConfig config;
    assert(config.width == 800);
    assert(config.height == 600);
    assert(config.format == SurfaceFormat::Bgra8UnormSrgb);
    assert(config.present_mode == PresentMode::Fifo);

    // Builder pattern
    auto custom = SurfaceConfig{}
        .with_size(1920, 1080)
        .with_format(SurfaceFormat::Rgba8UnormSrgb)
        .with_present_mode(PresentMode::Mailbox);

    assert(custom.width == 1920);
    assert(custom.height == 1080);
    assert(custom.format == SurfaceFormat::Rgba8UnormSrgb);
    assert(custom.present_mode == PresentMode::Mailbox);

    // Aspect ratio
    auto ratio = custom.aspect_ratio();
    assert(ratio > 1.7f && ratio < 1.8f);  // 16:9 ≈ 1.777

    std::cout << " PASSED\n";
}

void test_surface_capabilities() {
    std::cout << "  test_surface_capabilities...";

    SurfaceCapabilities caps = {
        .formats = {SurfaceFormat::Bgra8UnormSrgb, SurfaceFormat::Rgba8Unorm},
        .present_modes = {PresentMode::Fifo, PresentMode::Mailbox},
        .alpha_modes = {AlphaMode::Opaque, AlphaMode::PreMultiplied},
        .min_width = 1,
        .min_height = 1,
        .max_width = 8192,
        .max_height = 8192,
    };

    // Format support
    assert(caps.supports_format(SurfaceFormat::Bgra8UnormSrgb));
    assert(caps.supports_format(SurfaceFormat::Rgba8Unorm));
    assert(!caps.supports_format(SurfaceFormat::Rgba16Float));

    // Present mode support
    assert(caps.supports_present_mode(PresentMode::Fifo));
    assert(caps.supports_present_mode(PresentMode::Mailbox));
    assert(!caps.supports_present_mode(PresentMode::Immediate));

    // Preferred format (should be sRGB)
    assert(caps.preferred_format() == SurfaceFormat::Bgra8UnormSrgb);

    // Preferred present modes
    assert(caps.preferred_present_mode_low_latency() == PresentMode::Mailbox);
    assert(caps.preferred_present_mode_vsync() == PresentMode::Fifo);

    // Extent clamping
    auto [w1, h1] = caps.clamp_extent(0, 0);
    assert(w1 == 1 && h1 == 1);

    auto [w2, h2] = caps.clamp_extent(10000, 10000);
    assert(w2 == 8192 && h2 == 8192);

    auto [w3, h3] = caps.clamp_extent(1920, 1080);
    assert(w3 == 1920 && h3 == 1080);

    std::cout << " PASSED\n";
}

void test_surface_texture() {
    std::cout << "  test_surface_texture...";

    auto texture = SurfaceTexture::create(
        42, 1920, 1080, SurfaceFormat::Bgra8UnormSrgb
    );

    assert(texture.id == 42);
    assert(texture.width == 1920);
    assert(texture.height == 1080);
    assert(texture.format == SurfaceFormat::Bgra8UnormSrgb);
    assert(!texture.suboptimal);

    auto suboptimal = texture.with_suboptimal(true);
    assert(suboptimal.suboptimal);

    auto [w, h] = texture.size();
    assert(w == 1920 && h == 1080);

    std::cout << " PASSED\n";
}

void test_null_surface() {
    std::cout << "  test_null_surface...";

    NullSurface surface;

    // Initial state
    assert(surface.state() == SurfaceState::Ready);
    assert(surface.is_ready());

    // Configure
    auto config = SurfaceConfig{}.with_size(1920, 1080);
    assert(surface.configure(config));
    assert(surface.config().width == 1920);
    assert(surface.config().height == 1080);

    // Get texture
    SurfaceTexture texture;
    assert(surface.get_current_texture(texture));
    assert(texture.width == 1920);
    assert(texture.height == 1080);
    assert(texture.id == 1);

    // Get another texture
    assert(surface.get_current_texture(texture));
    assert(texture.id == 2);

    // Present (no-op)
    surface.present();

    // Size helper
    auto [w, h] = surface.size();
    assert(w == 1920 && h == 1080);

    std::cout << " PASSED\n";
}

void test_surface_state_transitions() {
    std::cout << "  test_surface_state_transitions...";

    NullSurface surface;

    // Initial state
    assert(surface.state() == SurfaceState::Ready);

    // Simulate state changes
    surface.set_state(SurfaceState::NeedsReconfigure);
    assert(surface.state() == SurfaceState::NeedsReconfigure);
    assert(!surface.is_ready());

    surface.set_state(SurfaceState::Lost);
    assert(surface.state() == SurfaceState::Lost);

    surface.set_state(SurfaceState::Minimized);
    assert(surface.state() == SurfaceState::Minimized);

    // Configure should reset to Ready
    assert(surface.configure(SurfaceConfig{}));
    assert(surface.state() == SurfaceState::Ready);
    assert(surface.is_ready());

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "Running presenter surface tests...\n";

    test_surface_config();
    test_surface_capabilities();
    test_surface_texture();
    test_null_surface();
    test_surface_state_transitions();

    std::cout << "All presenter surface tests passed!\n";
    return 0;
}
