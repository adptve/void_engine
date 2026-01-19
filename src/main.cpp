#include <void_engine/core/engine.hpp>
#include <iostream>

int main() {
    void_engine::Engine engine;

    if (!engine.init()) {
        std::cerr << "Failed to initialize void_engine\n";
        return 1;
    }

    engine.run();
    engine.shutdown();

    return 0;
}
