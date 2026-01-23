#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_event

#include <cstdint>

namespace void_event {

// Priority
enum class Priority : std::uint8_t;

// IDs
struct SubscriberId;

// Core types
class EventEnvelope;
class EventBus;

template<typename E>
class EventChannel;

} // namespace void_event
