#define SDL_MAIN_HANDLED
#include "core/Game.hpp"

int main() {
    Game game;
    if (!game.init()) {
        return 1;
    }
    game.run();
    game.shutdown();
    return 0;
}
