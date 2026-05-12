#pragma once

#include "core/StateMachine.hpp"
#include "entities/Components.hpp"

struct SDL_Window;
struct SDL_Renderer;

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();
    void shutdown();

    SDL_Window* window() const { return window_; }

private:
    World world_ {};
    StateMachine states_ {};
    SDL_Window* window_ {nullptr};
    SDL_Renderer* renderer_ {nullptr};
    bool initialized_ {false};
};
