/// @file test_presenter.cpp
/// @brief Tests for void_presenter presenter types

#include <void_engine/presenter/presenter.hpp>
#include <cassert>
#include <iostream>

using namespace void_presenter;

void test_presenter_id() {
    std::cout << "  test_presenter_id...";

    PresenterId invalid;
    assert(!invalid.is_valid());
    assert(invalid.id == 0);

    PresenterId valid(42);
    assert(valid.is_valid());
    assert(valid.id == 42);

    // Comparison
    PresenterId a(1);
    PresenterId b(2);
    PresenterId c(1);

    assert(a == c);
    assert(a != b);
    assert(a < b);

    std::cout << " PASSED\n";
}

void test_presenter_config() {
    std::cout << "  test_presenter_config...";

    PresenterConfig config;

    // Defaults
    assert(config.format == SurfaceFormat::Bgra8UnormSrgb);
    assert(config.present_mode == PresentMode::Fifo);
    assert(config.width == 1920);
    assert(config.height == 1080);
    assert(config.target_frame_rate == 60);

    // Builder pattern
    auto custom = PresenterConfig{}
        .with_size(2560, 1440)
        .with_format(SurfaceFormat::Rgba16Float)
        .with_present_mode(PresentMode::Mailbox)
        .with_hdr(true)
        .with_target_fps(144);

    assert(custom.width == 2560);
    assert(custom.height == 1440);
    assert(custom.format == SurfaceFormat::Rgba16Float);
    assert(custom.present_mode == PresentMode::Mailbox);
    assert(custom.enable_hdr);
    assert(custom.target_frame_rate == 144);

    std::cout << " PASSED\n";
}

void test_presenter_capabilities() {
    std::cout << "  test_presenter_capabilities...";

    auto caps = PresenterCapabilities::default_caps();

    assert(!caps.formats.empty());
    assert(!caps.present_modes.empty());
    assert(caps.max_width >= 4096);
    assert(caps.max_height >= 4096);

    auto [max_w, max_h] = caps.max_resolution();
    assert(max_w >= 4096);
    assert(max_h >= 4096);

    std::cout << " PASSED\n";
}

void test_null_presenter() {
    std::cout << "  test_null_presenter...";

    PresenterId id(1);
    NullPresenter presenter(id);

    // ID
    assert(presenter.id() == id);
    assert(presenter.is_valid());

    // Capabilities
    const auto& caps = presenter.capabilities();
    assert(!caps.formats.empty());

    // Reconfigure
    auto config = PresenterConfig{}.with_size(1920, 1080);
    assert(presenter.reconfigure(config));
    assert(presenter.config().width == 1920);

    // Resize
    assert(presenter.resize(2560, 1440));
    auto [w, h] = presenter.size();
    assert(w == 2560 && h == 1440);

    std::cout << " PASSED\n";
}

void test_null_presenter_frames() {
    std::cout << "  test_null_presenter_frames...";

    PresenterId id(1);
    NullPresenter presenter(id);
    presenter.reconfigure(PresenterConfig{}.with_size(800, 600).with_target_fps(60));

    // Begin frame
    Frame frame(0, 0, 0);  // Will be replaced
    assert(presenter.begin_frame(frame));

    assert(frame.number() == 1);
    assert(frame.width() == 800);
    assert(frame.height() == 600);
    assert(frame.deadline().has_value());  // Target FPS was set

    // Present
    assert(presenter.present(frame));
    assert(frame.state() == FrameState::Presented);

    // Second frame
    Frame frame2(0, 0, 0);
    assert(presenter.begin_frame(frame2));
    assert(frame2.number() == 2);

    std::cout << " PASSED\n";
}

void test_null_presenter_rehydration() {
    std::cout << "  test_null_presenter_rehydration...";

    PresenterId id(1);
    NullPresenter presenter1(id);
    presenter1.reconfigure(PresenterConfig{}.with_size(1920, 1080));

    // Advance frames
    for (int i = 0; i < 10; ++i) {
        Frame frame(0, 0, 0);
        presenter1.begin_frame(frame);
        presenter1.present(frame);
    }

    // Dehydrate
    auto state = presenter1.dehydrate();

    // Create new presenter and rehydrate
    NullPresenter presenter2(id);
    assert(presenter2.rehydrate(state));

    // Frame numbers should continue
    Frame frame(0, 0, 0);
    presenter2.begin_frame(frame);
    assert(frame.number() == 11);

    // Size should be restored
    auto [w, h] = presenter2.size();
    assert(w == 1920 && h == 1080);

    std::cout << " PASSED\n";
}

void test_presenter_manager() {
    std::cout << "  test_presenter_manager...";

    PresenterManager manager;

    // Allocate IDs
    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();
    assert(id1.is_valid());
    assert(id2.is_valid());
    assert(id1 != id2);

    // Register presenters
    auto p1 = std::make_unique<NullPresenter>(id1);
    auto p2 = std::make_unique<NullPresenter>(id2);

    manager.register_presenter(std::move(p1));
    manager.register_presenter(std::move(p2));

    assert(manager.count() == 2);

    // Access
    auto* presenter1 = manager.get(id1);
    assert(presenter1 != nullptr);
    assert(presenter1->id() == id1);

    auto* presenter2 = manager.get(id2);
    assert(presenter2 != nullptr);
    assert(presenter2->id() == id2);

    // Primary (first registered)
    auto* primary = manager.primary();
    assert(primary != nullptr);
    assert(primary->id() == id1);

    std::cout << " PASSED\n";
}

void test_presenter_manager_primary() {
    std::cout << "  test_presenter_manager_primary...";

    PresenterManager manager;

    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();

    manager.register_presenter(std::make_unique<NullPresenter>(id1));
    manager.register_presenter(std::make_unique<NullPresenter>(id2));

    // First is primary by default
    assert(manager.primary()->id() == id1);

    // Change primary
    assert(manager.set_primary(id2));
    assert(manager.primary()->id() == id2);

    // Invalid ID
    PresenterId invalid(999);
    assert(!manager.set_primary(invalid));

    std::cout << " PASSED\n";
}

void test_presenter_manager_all_ids() {
    std::cout << "  test_presenter_manager_all_ids...";

    PresenterManager manager;

    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();
    auto id3 = manager.allocate_id();

    manager.register_presenter(std::make_unique<NullPresenter>(id1));
    manager.register_presenter(std::make_unique<NullPresenter>(id2));
    manager.register_presenter(std::make_unique<NullPresenter>(id3));

    auto ids = manager.all_ids();
    assert(ids.size() == 3);

    // All IDs should be present
    bool found1 = false, found2 = false, found3 = false;
    for (const auto& id : ids) {
        if (id == id1) found1 = true;
        if (id == id2) found2 = true;
        if (id == id3) found3 = true;
    }
    assert(found1 && found2 && found3);

    std::cout << " PASSED\n";
}

void test_presenter_manager_unregister() {
    std::cout << "  test_presenter_manager_unregister...";

    PresenterManager manager;

    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();

    manager.register_presenter(std::make_unique<NullPresenter>(id1));
    manager.register_presenter(std::make_unique<NullPresenter>(id2));

    assert(manager.count() == 2);

    // Unregister first
    auto removed = manager.unregister(id1);
    assert(removed != nullptr);
    assert(removed->id() == id1);
    assert(manager.count() == 1);
    assert(manager.get(id1) == nullptr);

    // Primary should update to second
    assert(manager.primary()->id() == id2);

    // Unregister nonexistent
    auto none = manager.unregister(id1);
    assert(none == nullptr);

    std::cout << " PASSED\n";
}

void test_presenter_manager_batch_frames() {
    std::cout << "  test_presenter_manager_batch_frames...";

    PresenterManager manager;

    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();

    manager.register_presenter(std::make_unique<NullPresenter>(id1));
    manager.register_presenter(std::make_unique<NullPresenter>(id2));

    // Begin all frames
    auto frames = manager.begin_all_frames();
    assert(frames.size() == 2);

    // Present all
    manager.present_all(frames);

    // Check frames were presented
    for (const auto& [id, frame] : frames) {
        assert(frame.state() == FrameState::Presented);
    }

    std::cout << " PASSED\n";
}

void test_presenter_manager_rehydration() {
    std::cout << "  test_presenter_manager_rehydration...";

    PresenterManager manager;

    auto id1 = manager.allocate_id();
    auto id2 = manager.allocate_id();

    auto p1 = std::make_unique<NullPresenter>(id1);
    auto p2 = std::make_unique<NullPresenter>(id2);

    p1->reconfigure(PresenterConfig{}.with_size(1920, 1080));
    p2->reconfigure(PresenterConfig{}.with_size(2560, 1440));

    manager.register_presenter(std::move(p1));
    manager.register_presenter(std::move(p2));

    // Get rehydration states
    auto states = manager.rehydration_states();
    assert(states.size() == 2);

    // Verify states contain data
    for (const auto& [id, state] : states) {
        assert(state.get_uint("width").has_value());
    }

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "Running presenter tests...\n";

    test_presenter_id();
    test_presenter_config();
    test_presenter_capabilities();
    test_null_presenter();
    test_null_presenter_frames();
    test_null_presenter_rehydration();
    test_presenter_manager();
    test_presenter_manager_primary();
    test_presenter_manager_all_ids();
    test_presenter_manager_unregister();
    test_presenter_manager_batch_frames();
    test_presenter_manager_rehydration();

    std::cout << "All presenter tests passed!\n";
    return 0;
}
