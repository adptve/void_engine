/// @file test_frame.cpp
/// @brief Tests for void_presenter frame types

#include <void_engine/presenter/frame.hpp>
#include <cassert>
#include <iostream>
#include <thread>

using namespace void_presenter;

void test_frame_creation() {
    std::cout << "  test_frame_creation...";

    Frame frame(1, 1920, 1080);

    assert(frame.number() == 1);
    assert(frame.width() == 1920);
    assert(frame.height() == 1080);
    assert(frame.state() == FrameState::Preparing);

    auto [w, h] = frame.size();
    assert(w == 1920 && h == 1080);

    std::cout << " PASSED\n";
}

void test_frame_lifecycle() {
    std::cout << "  test_frame_lifecycle...";

    Frame frame(1, 800, 600);

    // Initial state
    assert(frame.state() == FrameState::Preparing);

    // Begin render
    frame.begin_render();
    assert(frame.state() == FrameState::Rendering);
    assert(frame.render_start().has_value());

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // End render
    frame.end_render();
    assert(frame.state() == FrameState::Ready);
    assert(frame.render_end().has_value());
    assert(frame.render_duration().has_value());
    assert(*frame.render_duration() > std::chrono::microseconds(0));

    // Present
    frame.mark_presented();
    assert(frame.state() == FrameState::Presented);
    assert(frame.presented_at().has_value());
    assert(frame.total_duration().has_value());

    std::cout << " PASSED\n";
}

void test_frame_dropped() {
    std::cout << "  test_frame_dropped...";

    Frame frame(1, 800, 600);

    frame.begin_render();
    frame.mark_dropped();

    assert(frame.state() == FrameState::Dropped);

    std::cout << " PASSED\n";
}

void test_frame_deadline() {
    std::cout << "  test_frame_deadline...";

    Frame frame(1, 800, 600);
    frame.set_target_fps(60);  // ~16.67ms deadline

    assert(frame.deadline().has_value());
    assert(!frame.missed_deadline());

    auto time_left = frame.time_until_deadline();
    assert(time_left.has_value());
    assert(*time_left > std::chrono::milliseconds(0));

    std::cout << " PASSED\n";
}

void test_frame_user_data() {
    std::cout << "  test_frame_user_data...";

    Frame frame(1, 800, 600);

    // No user data initially
    assert(!frame.has_user_data());
    assert(frame.user_data<int>() == nullptr);

    // Set user data
    frame.set_user_data(42);
    assert(frame.has_user_data());

    // Get user data
    const int* data = frame.user_data<int>();
    assert(data != nullptr);
    assert(*data == 42);

    // Take user data
    auto taken = frame.take_user_data<int>();
    assert(taken.has_value());
    assert(*taken == 42);
    assert(!frame.has_user_data());

    // Wrong type returns nullopt
    frame.set_user_data(std::string("hello"));
    auto wrong_type = frame.take_user_data<int>();
    assert(!wrong_type.has_value());

    std::cout << " PASSED\n";
}

void test_frame_output() {
    std::cout << "  test_frame_output...";

    Frame frame(42, 1920, 1080);
    frame.set_target_fps(60);

    frame.begin_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    frame.end_render();
    frame.mark_presented();

    auto output = FrameOutput::from_frame(frame);

    assert(output.frame_number == 42);
    assert(output.width == 1920);
    assert(output.height == 1080);
    assert(output.render_time_us > 0);
    assert(output.total_time_us > 0);
    assert(!output.dropped);

    std::cout << " PASSED\n";
}

void test_frame_stats() {
    std::cout << "  test_frame_stats...";

    FrameStats stats;

    // Initial state
    assert(stats.total_frames == 0);
    assert(stats.presented_frames == 0);
    assert(stats.dropped_frames == 0);
    assert(stats.drop_rate() == 0.0);

    // Add presented frame
    FrameOutput output1 = {
        .frame_number = 1,
        .width = 1920,
        .height = 1080,
        .render_time_us = 1000,
        .total_time_us = 16666,
        .missed_deadline = false,
        .dropped = false,
    };
    stats.update(output1);

    assert(stats.total_frames == 1);
    assert(stats.presented_frames == 1);
    assert(stats.dropped_frames == 0);
    assert(stats.drop_rate() == 0.0);

    // Add dropped frame
    FrameOutput output2 = {
        .frame_number = 2,
        .width = 1920,
        .height = 1080,
        .render_time_us = 2000,
        .total_time_us = 33333,
        .missed_deadline = true,
        .dropped = true,
    };
    stats.update(output2);

    assert(stats.total_frames == 2);
    assert(stats.presented_frames == 1);
    assert(stats.dropped_frames == 1);
    assert(stats.deadline_misses == 1);
    assert(stats.drop_rate() == 0.5);
    assert(stats.deadline_miss_rate() == 0.5);

    // Check FPS calculation
    assert(stats.average_fps() > 0.0);

    // Reset
    stats.reset();
    assert(stats.total_frames == 0);
    assert(stats.presented_frames == 0);

    std::cout << " PASSED\n";
}

void test_frame_latency() {
    std::cout << "  test_frame_latency...";

    Frame frame(1, 800, 600);

    // Current latency should be small but > 0
    auto latency = frame.current_latency();
    assert(latency >= std::chrono::nanoseconds(0));

    // After some time, latency should increase
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto later_latency = frame.current_latency();
    assert(later_latency > latency);

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "Running presenter frame tests...\n";

    test_frame_creation();
    test_frame_lifecycle();
    test_frame_dropped();
    test_frame_deadline();
    test_frame_user_data();
    test_frame_output();
    test_frame_stats();
    test_frame_latency();

    std::cout << "All presenter frame tests passed!\n";
    return 0;
}
