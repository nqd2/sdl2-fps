#pragma once

#include "core/StateMachine.hpp"
#include "entities/Components.hpp"

namespace UpdateSystem {

void update(World& world, StateMachine& states, float dt);

}  // namespace UpdateSystem
