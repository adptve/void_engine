/// @file test_timing.cpp
/// @brief Tests for void_presenter timing types

#include <void_engine/presenter/timing.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

using namespace void_presenter;

void test_frame_timing_creation() {
    std::cout << "  test_frame_timing_creation...";

    // Default 60 FPS
    FrameTiming timing(60);
    assert(std::abs(timing.target_fps() - 60.0) < 0.1);
    assert(timing.target_frame_time() > std::chrono::milliseconds(16));
    assert(timing.target_frame_time() < std::chrono::milliseconds(17));

    // Unlimited
    auto unlimited = FrameTiming::unlimited();
    assert(std::isinf(unlimited.target_fps()));
    assert(unlimited.target_frame_time() == FrameTiming::Duration::zero());

    std::cout << " PASSED\n";
}

void test_frame_timing_tracking() {
    std::cout << "  test_frame_timing_tracking...";

    FrameTiming timing(1000);  // High FPS for testing

    // First frame
    timing.begin_frame();
    assert(timing.frame_count() == 1);

    // Wait and second frame
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    timing.begin_frame();
    assert(timing.frame_count() == 2);

    // Check frame duration
    auto last_duration = timing.last_frame_duration();
    assert(last_duration > std::chrono::milliseconds(1));

    // Delta time should be in seconds
    float dt = timing.delta_time();
    assert(dt > 0.001f);

    std::cout << " PASSED\n";
}

void test_frame_timing_averages() {
    std::cout << "  test_frame_timing_averages...";

    FrameTiming timing(1000);

    // Generate some frames
    for (int i = 0; i < 10; ++i) {
        timing.begin_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Check averages
    auto avg_duration = timing.average_frame_duration();
    assert(avg_duration > std::chrono::microseconds(500));

    double avg_fps = timing.average_fps();
    assert(avg_fps > 0.0);

    // Total elapsed should be positive
    auto total = timing.total_elapsed();
    assert(total > std::chrono::milliseconds(5));

    std::cout << " PASSED\n";
}

void test_frame_timing_percentiles() {
    std::cout << "  test_frame_timing_percentiles...";

    FrameTiming timing(1000);

    // Generate frames with varying times
    for (int i = 0; i < 20; ++i) {
        timing.begin_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1 + (i % 3)));
    }

    // Percentiles
    auto p50 = timing.frame_time_percentile(50);
    auto p95 = timing.frame_time_percentile(95);
    auto p99 = timing.frame_time_percentile(99);

    // P99 should be >= P95 >= P50
    assert(p99 >= p95);
    assert(p95 >= p50);

    std::cout << " PASSED\n";
}

void test_frame_timing_wait() {
    std::cout << "  test_frame_timing_wait...";

    FrameTiming timing(100);  // 10ms per frame

    timing.begin_frame();

    // Time to wait should be close to target
    auto wait = timing.time_to_wait();
    assert(wait > std::chrono::milliseconds(5));

    // After waiting, time to wait should be less
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto wait_after = timing.time_to_wait();
    assert(wait_after < wait);

    std::cout << " PASSED\n";
}

void test_frame_timing_unlimited() {
    std::cout << "  test_frame_timing_unlimited...";

    auto timing = FrameTiming::unlimited();

    // Time to wait should always be zero
    timing.begin_frame();
    assert(timing.time_to_wait() == FrameTiming::Duration::zero());

    std::cout << " PASSED\n";
}

void test_frame_timing_reset() {
    std::cout << "  test_frame_timing_reset...";

    FrameTiming timing(60);

    // Generate some frames
    for (int i = 0; i < 5; ++i) {
        timing.begin_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    assert(timing.frame_count() > 0);

    // Reset
    timing.reset();

    assert(timing.frame_count() == 0);
    assert(timing.total_elapsed() == FrameTiming::Duration::zero());
    assert(timing.last_frame_duration() == FrameTiming::Duration::zero());

    std::cout << " PASSED\n";
}

void test_frame_limiter() {
    std::cout << "  test_frame_limiter...";

    FrameLimiter limiter(100);  // 10ms per frame

    auto start = std::chrono::steady_clock::now();

    // Run a few frames
    for (int i = 0; i < 5; ++i) {
        limiter.wait();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should have taken at least ~40ms (5 frames at 10ms each, minus first)
    assert(elapsed > std::chrono::milliseconds(35));

    std::cout << " PASSED\n";
}

void test_frame_limiter_unlimited() {
    std::cout << "  test_frame_limiter_unlimited...";

    auto limiter = FrameLimiter::unlimited();

    auto start = std::chrono::steady_clock::now();

    // Run many frames
    for (int i = 0; i < 100; ++i) {
        limiter.wait();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should be very fast (no limiting)
    assert(elapsed < std::chrono::milliseconds(10));

    std::cout << " PASSED\n";
}

void test_frame_limiter_set_fps() {
    std::cout << "  test_frame_limiter_set_fps...";

    FrameLimiter limiter(60);
    assert(std::abs(limiter.target_fps() - 60.0) < 0.1);

    limiter.set_target_fps(30);
    assert(std::abs(limiter.target_fps() - 30.0) < 0.1);

    limiter.set_target_fps(0);
    assert(std::isinf(limiter.target_fps()));

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "Running presenter timing tests...\n";

    test_frame_timing_creation();
    test_frame_timing_tracking();
    test_frame_timing_averages();
    test_frame_timing_percentiles();
    test_frame_timing_wait();
    test_frame_timing_unlimited();
    test_frame_timing_reset();
    test_frame_limiter();
    test_frame_limiter_unlimited();
    test_frame_limiter_set_fps();

    std::cout << "All presenter timing tests passed!\n";
    return 0;
}
