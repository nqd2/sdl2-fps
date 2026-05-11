#pragma once

#include "entities/Components.hpp"

#include <cstdint>

namespace ParticleSystem {

void spawnWorld(World& world, Vec2 pos, uint32_t color, int count, float speed, float life);
void spawnScreen(World& world, float sx, float sy, uint32_t color, int count, float speed, float life);
void update(World& world, float dt);

}  // namespace ParticleSystem
