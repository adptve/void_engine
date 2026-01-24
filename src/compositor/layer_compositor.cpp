/// @file layer_compositor.cpp
/// @brief Layer compositor implementation

#include <void_engine/compositor/layer_compositor.hpp>
#include <void_engine/compositor/layer.hpp>

namespace void_compositor {

// The layer compositor is primarily header-only with template implementations.
// This file provides a compilation unit and any non-inline implementations.

// Ensure templates are instantiated
template void LayerManager::for_each<std::function<void(Layer&)>>(std::function<void(Layer&)>&&);
template void LayerManager::for_each<std::function<void(const Layer&)>>(std::function<void(const Layer&)>&&) const;

} // namespace void_compositor
