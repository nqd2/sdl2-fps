#include "systems/UpdateSystem.hpp"
#include "GameConstants.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/CombatSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"
#include "systems/ParticleSystem.hpp"
#include "systems/RenderUtils.hpp"
#include "systems/SaveSystem.hpp"
#include "data/GameData.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace UpdateSystem {

void update(World& world, StateMachine& states, float dt) {
    if (states.current() != GameStateId::Playing) return;
    const DifficultySettings diff = getDifficultySettings(world.difficulty);

    world.elapsedRunSeconds += dt;
    world.player.weaponCooldown = std::max(0.0F, world.player.weaponCooldown - dt);
    world.player.invulnerabilityTimer = std::max(0.0F, world.player.invulnerabilityTimer - dt);
    world.player.recoilTimer = std::max(0.0F, world.player.recoilTimer - dt);
    world.screenFlashTimer = std::max(0.0F, world.screenFlashTimer - dt);
    world.player.damageBoostTimer = std::max(0.0F, world.player.damageBoostTimer - dt);
    world.player.speedBoostTimer = std::max(0.0F, world.player.speedBoostTimer - dt);
    world.player.shieldTimer = std::max(0.0F, world.player.shieldTimer - dt);

    world.player.speed = world.player.baseSpeed + (world.player.speedBoostTimer > 0.0F ? 1.5F : 0.0F);

    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
    if (keyboard[SDL_SCANCODE_LEFT])
        world.player.facingRadians = wrapAngle(world.player.facingRadians - world.player.turnSpeed * dt);
    if (keyboard[SDL_SCANCODE_RIGHT])
        world.player.facingRadians = wrapAngle(world.player.facingRadians + world.player.turnSpeed * dt);

    Vec2 forward {std::cos(world.player.facingRadians), std::sin(world.player.facingRadians)};
    Vec2 right {-forward.y, forward.x};
    Vec2 dir {};
    if (keyboard[SDL_SCANCODE_W]) { dir.x += forward.x; dir.y += forward.y; }
    if (keyboard[SDL_SCANCODE_S]) { dir.x -= forward.x; dir.y -= forward.y; }
    if (keyboard[SDL_SCANCODE_A]) { dir.x -= right.x; dir.y -= right.y; }
    if (keyboard[SDL_SCANCODE_D]) { dir.x += right.x; dir.y += right.y; }
    dir = vecNormalize(dir);
    world.player.velocity = {dir.x * world.player.speed, dir.y * world.player.speed};

    float vel = vecLength(world.player.velocity);
    if (vel > 0.1F) world.player.weaponBobPhase += dt * vel * 2.5F;

    Vec2 proposal = world.player.position;
    proposal.x += world.player.velocity.x * dt;
    if (Map::canMoveTo(proposal)) world.player.position.x = proposal.x;
    proposal = world.player.position;
    proposal.y += world.player.velocity.y * dt;
    if (Map::canMoveTo(proposal)) world.player.position.y = proposal.y;

    Uint32 mouseMask = SDL_GetMouseState(nullptr, nullptr);
    if (((mouseMask & SDL_BUTTON_LMASK) || keyboard[SDL_SCANCODE_SPACE]) && world.player.weaponCooldown <= 0.0F)
        CombatSystem::firePlayerWeapon(world);

    for (auto& enemy : world.enemies) {
        Vec2 toPlayer {world.player.position.x - enemy.position.x, world.player.position.y - enemy.position.y};
        Vec2 towardPlayer = vecNormalize(toPlayer);
        enemy.velocity = {towardPlayer.x * enemy.speed, towardPlayer.y * enemy.speed};
        Vec2 ep = enemy.position;
        ep.x += enemy.velocity.x * dt;
        ep.y += enemy.velocity.y * dt;
        if (!Map::isWall(ep.x, ep.y)) enemy.position = ep;

        if (enemy.type == EnemyType::Shooter) {
            enemy.fireTimer += dt;
            if (enemy.fireTimer >= enemy.fireCooldown && Map::hasLineOfSight(enemy.position, world.player.position)) {
                enemy.fireTimer = 0.0F;
                bool canDamage = world.player.invulnerabilityTimer <= 0.0F && world.player.shieldTimer <= 0.0F;
                if (canDamage && distanceSquared(enemy.position, world.player.position) < 36.0F) {
                    int dmg = std::max(1, static_cast<int>((1 + world.wave / 5) * diff.damageTakenMul));
                    world.player.hp -= dmg;
                    world.player.invulnerabilityTimer = GameConstants::kInvulnerabilitySeconds;
                    world.screenFlashTimer = 0.15F;
                    world.screenFlashColor = RenderUtils::packRGBA(255, 40, 40);
                    AudioSystem::play(SoundId::PlayerHurt);
                }
            }
        }
        if (enemy.type == EnemyType::Healer) {
            enemy.healTimer += dt;
            if (enemy.healTimer >= enemy.fireCooldown) {
                enemy.healTimer = 0.0F;
                for (auto& other : world.enemies) {
                    if (&other == &enemy || other.hp <= 0 || other.hp >= other.maxHp) continue;
                    if (distanceSquared(enemy.position, other.position) < 4.0F) {
                        other.hp = std::min(other.maxHp, other.hp + 2);
                        ParticleSystem::spawnWorld(world, other.position, RenderUtils::packRGBA(255, 200, 220), 3, 0.8F, 0.3F);
                    }
                }
            }
        }
    }

    for (auto& enemy : world.enemies) {
        if (enemy.contactDamage <= 0) continue;
        float minDist = GameConstants::kEnemySize * enemy.spriteScale + GameConstants::kPlayerRadius;
        if (distanceSquared(enemy.position, world.player.position) <= minDist * minDist &&
            world.player.invulnerabilityTimer <= 0.0F && world.player.shieldTimer <= 0.0F) {
            int dmg = std::max(1, static_cast<int>(enemy.contactDamage * diff.damageTakenMul));
            world.player.hp -= dmg;
            world.player.invulnerabilityTimer = GameConstants::kInvulnerabilitySeconds;
            world.screenFlashTimer = 0.15F;
            world.screenFlashColor = RenderUtils::packRGBA(255, 40, 40);
            AudioSystem::play(SoundId::PlayerHurt);
        }
    }

    for (auto& enemy : world.enemies) {
        if (enemy.hp <= 0 && enemy.type == EnemyType::Exploder) {
            AudioSystem::play(SoundId::Explosion);
            ParticleSystem::spawnWorld(world, enemy.position, RenderUtils::packRGBA(80, 255, 80), 22, 3.5F, 0.5F);
            if (distanceSquared(enemy.position, world.player.position) < GameConstants::kExplodeRadius * GameConstants::kExplodeRadius) {
                if (world.player.shieldTimer <= 0.0F) {
                    world.player.hp -= std::max(1, static_cast<int>(2 * diff.damageTakenMul));
                    world.screenFlashTimer = 0.2F;
                    world.screenFlashColor = RenderUtils::packRGBA(80, 255, 80);
                }
            }
            for (auto& other : world.enemies) {
                if (&other == &enemy || other.hp <= 0) continue;
                if (distanceSquared(enemy.position, other.position) < GameConstants::kExplodeRadius * GameConstants::kExplodeRadius)
                    other.hp -= 3;
            }
        }
    }

    world.enemies.erase(std::remove_if(world.enemies.begin(), world.enemies.end(),
        [](const Enemy& e) { return e.hp <= 0; }), world.enemies.end());

    for (auto& p : world.pickups) {
        p.lifetime -= dt;
        p.bobPhase += dt * 3.0F;
    }
    for (auto it = world.pickups.begin(); it != world.pickups.end(); ) {
        if (it->lifetime <= 0.0F) { it = world.pickups.erase(it); continue; }
        if (distanceSquared(it->position, world.player.position) < GameConstants::kPickupRadius * GameConstants::kPickupRadius) {
            AudioSystem::play(SoundId::PickupCollect);
            switch (it->type) {
                case PickupType::Health:
                    world.player.hp = std::min(world.player.maxHp, world.player.hp + 3);
                    break;
                case PickupType::DamageBoost: world.player.damageBoostTimer = 10.0F; break;
                case PickupType::SpeedBoost:  world.player.speedBoostTimer = 10.0F; break;
                case PickupType::Shield:      world.player.shieldTimer = 3.0F; break;
            }
            ParticleSystem::spawnWorld(world, it->position, RenderUtils::packRGBA(255, 255, 255), 6, 1.5F, 0.3F);
            it = world.pickups.erase(it);
        } else {
            ++it;
        }
    }

    ParticleSystem::update(world, dt);

    if (world.player.hp <= 0) {
        world.runActive = false;
        world.bestWave = std::max(world.bestWave, world.wave);
        CombatSystem::insertLeaderboard(world);
        SaveSystem::save(world);
        InputSystem::setMouseCapture(world, false);
        states.clearAndSet(GameStateId::GameOver);
        return;
    }

    if (world.killsInWave >= world.killsToClearWave || world.enemies.empty()) {
        AudioSystem::play(SoundId::WaveClear);
        world.pendingUpgrades = pickUpgradeChoices(world.wave);
        states.clearAndSet(GameStateId::UpgradeSelection);
    }
}

}  // namespace UpdateSystem
