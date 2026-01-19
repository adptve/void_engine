#pragma once

namespace void_engine {

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = default;
    Engine& operator=(Engine&&) = default;

    bool init();
    void run();
    void shutdown();

private:
    bool m_running = false;
};

} // namespace void_engine
