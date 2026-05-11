#include "systems/ParticleSystem.hpp"
#include "GameConstants.hpp"
#include "systems/MathUtils.hpp"

#include <algorithm>
#include <cmath>

namespace ParticleSystem {

namespace {

void evictOldest(World& world) {
    if (static_cast<int>(world.particles.size()) >= GameConstants::kMaxParticles) {
        world.particles.front() = world.particles.back();
        world.particles.pop_back();
    }
}

}  // namespace

void spawnWorld(World& world, Vec2 pos, uint32_t color, int count, float speed, float life) {
    for (int i = 0; i < count; ++i) {
        evictOldest(world);
        float angle = randomFloat(0.0F, GameConstants::kTwoPi);
        float spd = randomFloat(speed * 0.3F, speed);
        world.particles.push_back(Particle{
            pos, {std::cos(angle) * spd, std::sin(angle) * spd},
            color, life, life, randomFloat(1.5F, 3.0F), true});
    }
}

void spawnScreen(World& world, float sx, float sy, uint32_t color, int count, float speed, float life) {
    for (int i = 0; i < count; ++i) {
        evictOldest(world);
        float angle = randomFloat(0.0F, GameConstants::kTwoPi);
        float spd = randomFloat(speed * 0.4F, speed);
        world.particles.push_back(Particle{
            {sx, sy}, {std::cos(angle) * spd, std::sin(angle) * spd},
            color, life, life, randomFloat(2.0F, 4.0F), false});
    }
}

void update(World& world, float dt) {
    for (auto& p : world.particles) {
        p.lifetime -= dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
    }
    world.particles.erase(
        std::remove_if(world.particles.begin(), world.particles.end(),
                       [](const Particle& p) { return p.lifetime <= 0.0F; }),
        world.particles.end());
}

}  // namespace ParticleSystem
