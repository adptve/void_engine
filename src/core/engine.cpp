#include <void_engine/core/engine.hpp>
#include <iostream>

namespace void_engine {

bool Engine::init() {
    std::cout << "void_engine initializing...\n";
    m_running = true;
    std::cout << "void_engine initialized successfully\n";
    return true;
}

void Engine::run() {
    std::cout << "void_engine running...\n";

    while (m_running) {
        // Main loop placeholder
        // TODO: Process input, update, render

        // For now, exit immediately
        m_running = false;
    }
}

void Engine::shutdown() {
    std::cout << "void_engine shutting down...\n";
    m_running = false;
    std::cout << "void_engine shutdown complete\n";
}

} // namespace void_engine
