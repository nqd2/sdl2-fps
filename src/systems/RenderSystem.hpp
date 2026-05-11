#pragma once

#include "core/StateMachine.hpp"
#include "entities/Components.hpp"

#include <SDL.h>

namespace RenderSystem {

void render(const World& world, GameStateId state, SDL_Renderer* renderer);

}  // namespace RenderSystem
