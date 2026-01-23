/// @file test_compositor.cpp
/// @brief Tests for void_compositor

#include <void_engine/compositor/compositor_module.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace void_compositor;

// =============================================================================
// VRR Tests
// =============================================================================

TEST(VrrTest, ConfigCreation) {
    auto config = VrrConfig::create(48, 144);
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.min_refresh_rate, 48);
    EXPECT_EQ(config.max_refresh_rate, 144);
    EXPECT_EQ(config.mode, VrrMode::Disabled);
}

TEST(VrrTest, EnableDisable) {
    auto config = VrrConfig::create(48, 144);
    EXPECT_FALSE(config.is_active());

    config.enable(VrrMode::Auto);
    EXPECT_TRUE(config.is_active());
    EXPECT_EQ(config.mode, VrrMode::Auto);
    EXPECT_EQ(config.current_refresh_rate, 144); // Max when in Auto

    config.enable(VrrMode::PowerSaving);
    EXPECT_EQ(config.current_refresh_rate, 48); // Min when in PowerSaving

    config.disable();
    EXPECT_FALSE(config.is_active());
}

TEST(VrrTest, FrameTime) {
    auto config = VrrConfig::create(48, 144);
    config.enable(VrrMode::MaximumPerformance);

    auto frame_time = config.frame_time();
    // At 144Hz, frame time should be ~6.94ms
    EXPECT_GT(frame_time.count(), 6'000'000); // > 6ms
    EXPECT_LT(frame_time.count(), 7'500'000); // < 7.5ms
}

TEST(VrrTest, AdaptRefreshRate) {
    auto config = VrrConfig::create(48, 144);
    config.enable(VrrMode::Auto);

    // Fast content -> max refresh
    config.adapt_refresh_rate(0.8f);
    EXPECT_EQ(config.current_refresh_rate, 144);

    // Static content -> min refresh
    config.adapt_refresh_rate(0.05f);
    EXPECT_EQ(config.current_refresh_rate, 48);
}

TEST(VrrTest, Capability) {
    auto cap = VrrCapability::create_supported(48, 144, "FreeSync");
    EXPECT_TRUE(cap.supported);
    EXPECT_EQ(cap.min_refresh_rate.value_or(0), 48);
    EXPECT_EQ(cap.max_refresh_rate.value_or(0), 144);

    auto cfg = cap.to_config();
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->min_refresh_rate, 48);
    EXPECT_EQ(cfg->max_refresh_rate, 144);
}

// =============================================================================
// HDR Tests
// =============================================================================

TEST(HdrTest, Hdr10Config) {
    auto config = HdrConfig::hdr10(1000);
    EXPECT_TRUE(config.is_active());
    EXPECT_EQ(config.transfer_function, TransferFunction::Pq);
    EXPECT_EQ(config.color_primaries, ColorPrimaries::Rec2020);
    EXPECT_EQ(config.max_luminance, 1000);
}

TEST(HdrTest, HlgConfig) {
    auto config = HdrConfig::hlg(600);
    EXPECT_TRUE(config.is_active());
    EXPECT_EQ(config.transfer_function, TransferFunction::Hlg);
    EXPECT_EQ(config.max_luminance, 600);
}

TEST(HdrTest, EnableDisable) {
    auto config = HdrConfig::sdr();
    EXPECT_FALSE(config.is_active());

    config.enable(TransferFunction::Pq);
    EXPECT_TRUE(config.is_active());
    EXPECT_EQ(config.color_primaries, ColorPrimaries::Rec2020);

    config.disable();
    EXPECT_FALSE(config.is_active());
    EXPECT_EQ(config.transfer_function, TransferFunction::Sdr);
}

TEST(HdrTest, TransferFunctionEotf) {
    EXPECT_EQ(eotf_id(TransferFunction::Sdr), 0);
    EXPECT_EQ(eotf_id(TransferFunction::Pq), 2);
    EXPECT_EQ(eotf_id(TransferFunction::Hlg), 3);
    EXPECT_EQ(eotf_id(TransferFunction::Linear), 1);
}

TEST(HdrTest, ColorPrimaries) {
    auto srgb = to_cie_xy(ColorPrimaries::Srgb);
    EXPECT_NEAR(srgb.red_x, 0.640f, 0.001f);

    auto rec2020 = to_cie_xy(ColorPrimaries::Rec2020);
    EXPECT_NEAR(rec2020.red_x, 0.708f, 0.001f);
}

TEST(HdrTest, Capability) {
    auto cap = HdrCapability::hdr10_capable(1000, 0.0001f);
    EXPECT_TRUE(cap.supported);
    EXPECT_TRUE(cap.supports_transfer_function(TransferFunction::Pq));
    EXPECT_TRUE(cap.supports_color_gamut(ColorPrimaries::Rec2020));

    auto config = cap.to_config(true);
    EXPECT_TRUE(config.is_active());
    EXPECT_EQ(config.transfer_function, TransferFunction::Pq);
}

TEST(HdrTest, DrmMetadata) {
    auto config = HdrConfig::hdr10(1000);
    auto metadata = config.to_drm_metadata();

    EXPECT_EQ(metadata.eotf, 2); // PQ
    EXPECT_EQ(metadata.max_display_mastering_luminance, 1000);
    EXPECT_GT(metadata.max_content_light_level, 0u);
}

// =============================================================================
// Frame Scheduler Tests
// =============================================================================

TEST(FrameSchedulerTest, Creation) {
    FrameScheduler scheduler(60);
    EXPECT_EQ(scheduler.target_fps(), 60);
    EXPECT_EQ(scheduler.frame_number(), 0);
}

TEST(FrameSchedulerTest, FrameLifecycle) {
    FrameScheduler scheduler(60);

    // Initially waiting
    EXPECT_EQ(scheduler.state(), FrameState::WaitingForCallback);
    EXPECT_FALSE(scheduler.should_render());

    // Callback received
    scheduler.on_frame_callback();
    EXPECT_EQ(scheduler.state(), FrameState::ReadyToRender);
    EXPECT_TRUE(scheduler.should_render());

    // Begin frame
    auto frame = scheduler.begin_frame();
    EXPECT_EQ(frame, 1);
    EXPECT_EQ(scheduler.state(), FrameState::Rendering);

    // End frame
    scheduler.end_frame();
    EXPECT_EQ(scheduler.state(), FrameState::WaitingForPresent);
}

TEST(FrameSchedulerTest, FpsCalculation) {
    FrameScheduler scheduler(60);
    EXPECT_EQ(scheduler.target_fps(), 60);

    auto budget = scheduler.frame_budget();
    auto budget_ms = std::chrono::duration_cast<std::chrono::milliseconds>(budget).count();
    EXPECT_GE(budget_ms, 16);
    EXPECT_LE(budget_ms, 17);
}

TEST(FrameSchedulerTest, VrrIntegration) {
    FrameScheduler scheduler(60);

    auto vrr = VrrConfig::create(48, 144);
    vrr.enable(VrrMode::Auto);
    scheduler.set_vrr_config(vrr);

    EXPECT_TRUE(scheduler.is_vrr_active());
}

// =============================================================================
// Input Tests
// =============================================================================

TEST(InputTest, InputState) {
    InputState state;

    // Press a key
    KeyboardEvent ke;
    ke.keycode = 30; // 'A' on most keyboards
    ke.state = KeyState::Pressed;
    ke.time_ms = 0;
    state.handle_event(InputEvent::keyboard(ke));

    EXPECT_TRUE(state.is_key_pressed(30));

    // Release the key
    ke.state = KeyState::Released;
    ke.time_ms = 10;
    state.handle_event(InputEvent::keyboard(ke));

    EXPECT_FALSE(state.is_key_pressed(30));
}

TEST(InputTest, PointerMotion) {
    InputState state;

    PointerMotionEvent motion;
    motion.position = Vec2{100.0f, 200.0f};
    motion.delta = Vec2{10.0f, 5.0f};
    motion.time_ms = 0;

    state.handle_event(InputEvent::pointer(motion));

    auto pos = state.pointer_position();
    EXPECT_FLOAT_EQ(pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.y, 200.0f);
}

TEST(InputTest, PointerButton) {
    InputState state;

    PointerButtonEvent btn;
    btn.button = PointerButton::Left;
    btn.state = ButtonState::Pressed;
    btn.time_ms = 0;

    state.handle_event(InputEvent::pointer(btn));
    EXPECT_TRUE(state.is_button_pressed(PointerButton::Left));

    btn.state = ButtonState::Released;
    state.handle_event(InputEvent::pointer(btn));
    EXPECT_FALSE(state.is_button_pressed(PointerButton::Left));
}

// =============================================================================
// Output Tests
// =============================================================================

TEST(OutputTest, OutputMode) {
    OutputMode mode;
    mode.width = 1920;
    mode.height = 1080;
    mode.refresh_mhz = 60000;

    EXPECT_EQ(mode.refresh_hz(), 60);
    EXPECT_FLOAT_EQ(mode.refresh_hz_f(), 60.0f);
    EXPECT_EQ(mode.to_string(), "1920x1080@60Hz");
}

TEST(OutputTest, NullOutput) {
    OutputInfo info;
    info.id = 1;
    info.name = "Test";
    info.current_mode = {1920, 1080, 60000};
    info.primary = true;

    NullOutput output(info);

    EXPECT_EQ(output.info().id, 1);
    EXPECT_EQ(output.info().name, "Test");
    EXPECT_TRUE(output.is_enabled());

    // VRR
    EXPECT_TRUE(output.vrr_capability().supported);
    EXPECT_TRUE(output.enable_vrr(VrrMode::Auto));
    EXPECT_TRUE(output.vrr_config().has_value());

    // HDR
    EXPECT_TRUE(output.hdr_capability().supported);
    EXPECT_TRUE(output.enable_hdr(HdrConfig::hdr10(1000)));
    EXPECT_TRUE(output.hdr_config().has_value());
}

// =============================================================================
// Compositor Tests
// =============================================================================

TEST(CompositorTest, NullCompositor) {
    CompositorConfig config;
    config.target_fps = 60;
    config.enable_vrr = true;
    config.enable_hdr = true;

    NullCompositor compositor(config);

    EXPECT_TRUE(compositor.is_running());
    EXPECT_EQ(compositor.frame_number(), 0);

    // Capabilities
    auto caps = compositor.capabilities();
    EXPECT_TRUE(caps.vrr_supported);
    EXPECT_TRUE(caps.hdr_supported);
    EXPECT_EQ(caps.display_count, 1);

    // Outputs
    auto outputs = compositor.outputs();
    EXPECT_EQ(outputs.size(), 1);
    EXPECT_NE(compositor.primary_output(), nullptr);
}

TEST(CompositorTest, FrameLoop) {
    NullCompositor compositor;

    // Dispatch to trigger frame callback
    compositor.dispatch();
    EXPECT_TRUE(compositor.should_render());

    // Begin/end frame
    auto target = compositor.begin_frame();
    EXPECT_NE(target, nullptr);
    EXPECT_EQ(compositor.frame_number(), 1);

    auto [width, height] = target->size();
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 1080u);

    auto err = compositor.end_frame(std::move(target));
    EXPECT_TRUE(err.ok());
}

TEST(CompositorTest, VrrControl) {
    NullCompositor compositor;

    // Check VRR capability
    auto vrr_cap = compositor.vrr_capability();
    ASSERT_NE(vrr_cap, nullptr);
    EXPECT_TRUE(vrr_cap->supported);

    // Enable VRR
    auto err = compositor.enable_vrr(VrrMode::Auto);
    EXPECT_TRUE(err.ok());

    auto vrr_cfg = compositor.vrr_config();
    EXPECT_NE(vrr_cfg, nullptr);
    EXPECT_TRUE(vrr_cfg->is_active());

    // Disable VRR
    err = compositor.disable_vrr();
    EXPECT_TRUE(err.ok());
}

TEST(CompositorTest, HdrControl) {
    NullCompositor compositor;

    // Check HDR capability
    auto hdr_cap = compositor.hdr_capability();
    ASSERT_NE(hdr_cap, nullptr);
    EXPECT_TRUE(hdr_cap->supported);

    // Enable HDR
    auto err = compositor.enable_hdr(HdrConfig::hdr10(1000));
    EXPECT_TRUE(err.ok());

    auto hdr_cfg = compositor.hdr_config();
    EXPECT_NE(hdr_cfg, nullptr);
    EXPECT_TRUE(hdr_cfg->is_active());

    // Disable HDR
    err = compositor.disable_hdr();
    EXPECT_TRUE(err.ok());
}

TEST(CompositorTest, InputInjection) {
    NullCompositor compositor;

    // Inject keyboard event
    KeyboardEvent ke;
    ke.keycode = 30;
    ke.state = KeyState::Pressed;
    compositor.inject_input(InputEvent::keyboard(ke));

    auto events = compositor.poll_input();
    EXPECT_EQ(events.size(), 1);
    EXPECT_TRUE(events[0].is_keyboard());

    // Check input state
    EXPECT_TRUE(compositor.input_state().is_key_pressed(30));
}

TEST(CompositorTest, ContentVelocity) {
    NullCompositor compositor;

    compositor.enable_vrr(VrrMode::Auto);
    compositor.update_content_velocity(0.8f);

    auto& scheduler = compositor.frame_scheduler();
    EXPECT_GT(scheduler.content_velocity(), 0.0f);
}

TEST(CompositorTest, Factory) {
    auto compositor = CompositorFactory::create_null();
    EXPECT_NE(compositor, nullptr);
    EXPECT_TRUE(compositor->is_running());

    EXPECT_TRUE(CompositorFactory::is_available());
    EXPECT_NE(CompositorFactory::backend_name(), nullptr);
}

TEST(CompositorTest, Shutdown) {
    NullCompositor compositor;
    EXPECT_TRUE(compositor.is_running());

    compositor.shutdown();
    EXPECT_FALSE(compositor.is_running());
}
