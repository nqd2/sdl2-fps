#pragma once

#include "core/StateMachine.hpp"
#include "entities/Components.hpp"

#include <SDL.h>

namespace InputSystem {

void handleInput(World& world, StateMachine& states, const SDL_Event& event);
void setMouseCapture(World& world, bool capture);

}  // namespace InputSystem
