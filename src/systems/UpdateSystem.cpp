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

namespace {

void revealFog(World& world) {
    int px = static_cast<int>(std::floor(world.player.position.x));
    int py = static_cast<int>(std::floor(world.player.position.y));
    constexpr int R = GameConstants::kFogRevealRadius;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            if (dx * dx + dy * dy > R * R) continue;
            int cx = px + dx, cy = py + dy;
            if (cx >= 0 && cx < world.mapWidth && cy >= 0 && cy < world.mapHeight) {
                world.explored[cy * world.mapWidth + cx] = true;
            }
        }
    }
}

void updateDecals(World& world, float dt) {
    for (auto& d : world.decals) d.alpha -= dt * 0.05F;
    world.decals.erase(
        std::remove_if(world.decals.begin(), world.decals.end(),
            [](const Decal& d) { return d.alpha <= 0.0F; }),
        world.decals.end());
}

}  // namespace

void update(World& world, StateMachine& states, float dt) {
    if (states.current() != GameStateId::Playing) return;
    const DifficultySettings diff = getDifficultySettings(world.difficulty);

    world.elapsedRunSeconds += dt;
    world.player.weaponCooldown = std::max(0.0F, world.player.weaponCooldown - dt);
    world.player.invulnerabilityTimer = std::max(0.0F, world.player.invulnerabilityTimer - dt);
    world.player.recoilTimer = std::max(0.0F, world.player.recoilTimer - dt);
    world.screenFlashTimer = std::max(0.0F, world.screenFlashTimer - dt);
    world.shakeTimer = std::max(0.0F, world.shakeTimer - dt);
    world.player.damageBoostTimer = std::max(0.0F, world.player.damageBoostTimer - dt);
    world.player.speedBoostTimer = std::max(0.0F, world.player.speedBoostTimer - dt);
    world.player.shieldTimer = std::max(0.0F, world.player.shieldTimer - dt);

    world.player.speed = world.player.baseSpeed
        + world.player.stats[static_cast<int>(StatId::MoveSpeed)] * GameConstants::kStatSpeedPerPoint
        + (world.player.speedBoostTimer > 0.0F ? 1.5F : 0.0F);

    world.player.levelUpFlash = std::max(0.0F, world.player.levelUpFlash - dt);

    // HP regen from stat
    {
        float regenRate = world.player.stats[static_cast<int>(StatId::HpRegen)] * GameConstants::kStatRegenPerPoint;
        if (regenRate > 0.0F && world.player.hp < world.player.maxHp) {
            world.player.hpRegenAccum += regenRate * dt;
            while (world.player.hpRegenAccum >= 1.0F) {
                world.player.hpRegenAccum -= 1.0F;
                world.player.hp = std::min(world.player.maxHp, world.player.hp + 1);
            }
        }
    }

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
    if (Map::canMoveTo(world, proposal)) world.player.position.x = proposal.x;
    proposal = world.player.position;
    proposal.y += world.player.velocity.y * dt;
    if (Map::canMoveTo(world, proposal)) world.player.position.y = proposal.y;

    revealFog(world);

    // Check exit
    {
        float dx = world.player.position.x - (world.exitX + 0.5F);
        float dy = world.player.position.y - (world.exitY + 0.5F);
        if (dx * dx + dy * dy < GameConstants::kExitReachDist * GameConstants::kExitReachDist) {
            AudioSystem::play(SoundId::LevelClear);
            world.bestLevel = std::max(world.bestLevel, world.level);
            CombatSystem::prepareShop(world);
            InputSystem::setMouseCapture(world, false);
            states.clearAndSet(GameStateId::Shop);
            return;
        }
    }

    Uint32 mouseMask = SDL_GetMouseState(nullptr, nullptr);
    if (((mouseMask & SDL_BUTTON_LMASK) || keyboard[SDL_SCANCODE_SPACE]) && world.player.weaponCooldown <= 0.0F)
        CombatSystem::firePlayerWeapon(world);

    // Enemy projectiles update
    for (auto& proj : world.projectiles) {
        proj.position.x += proj.velocity.x * dt;
        proj.position.y += proj.velocity.y * dt;
        proj.lifeRemaining -= dt;
    }
    // Projectile-wall collision
    world.projectiles.erase(
        std::remove_if(world.projectiles.begin(), world.projectiles.end(),
            [&](const Projectile& p) {
                if (p.lifeRemaining <= 0.0F) return true;
                if (Map::isWall(world, p.position.x, p.position.y)) return true;
                return false;
            }),
        world.projectiles.end());
    // Enemy projectile-player collision
    for (auto it = world.projectiles.begin(); it != world.projectiles.end(); ) {
        if (it->fromPlayer) { ++it; continue; }
        float d2 = distanceSquared(it->position, world.player.position);
        float hitR = GameConstants::kProjectileRadius + GameConstants::kPlayerRadius;
        if (d2 < hitR * hitR && world.player.invulnerabilityTimer <= 0.0F && world.player.shieldTimer <= 0.0F) {
            float armorReduction = 1.0F - world.player.stats[static_cast<int>(StatId::BodyArmor)] * GameConstants::kStatArmorPerPoint;
            int dmg = std::max(1, static_cast<int>(it->damage * diff.damageTakenMul * armorReduction));
            world.player.hp -= dmg;
            world.player.invulnerabilityTimer = GameConstants::kInvulnerabilitySeconds;
            world.screenFlashTimer = 0.15F;
            world.screenFlashColor = RenderUtils::packRGBA(255, 40, 40);
            world.shakeTimer = 0.12F;
            world.shakeIntensity = 3.0F;
            AudioSystem::play(SoundId::PlayerHurt);
            it = world.projectiles.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& enemy : world.enemies) {
        Vec2 toPlayer {world.player.position.x - enemy.position.x, world.player.position.y - enemy.position.y};
        Vec2 towardPlayer = vecNormalize(toPlayer);
        enemy.velocity = {towardPlayer.x * enemy.speed, towardPlayer.y * enemy.speed};
        Vec2 ep = enemy.position;
        ep.x += enemy.velocity.x * dt;
        ep.y += enemy.velocity.y * dt;
        if (!Map::isWall(world, ep.x, ep.y)) enemy.position = ep;

        if (enemy.type == EnemyType::Shooter) {
            enemy.fireTimer += dt;
            if (enemy.fireTimer >= enemy.fireCooldown && Map::hasLineOfSight(world, enemy.position, world.player.position)) {
                enemy.fireTimer = 0.0F;
                Vec2 toP = vecNormalize(toPlayer);
                Projectile proj {};
                proj.position = enemy.position;
                proj.velocity = {toP.x * GameConstants::kProjectileSpeed, toP.y * GameConstants::kProjectileSpeed};
                proj.damage = std::max(1, static_cast<int>((1 + world.level / 5) * diff.damageTakenMul));
                proj.fromPlayer = false;
                proj.lifeRemaining = 3.0F;
                proj.radius = GameConstants::kProjectileRadius;
                world.projectiles.push_back(proj);
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
            float armorReduction = 1.0F - world.player.stats[static_cast<int>(StatId::BodyArmor)] * GameConstants::kStatArmorPerPoint;
            int dmg = std::max(1, static_cast<int>(enemy.contactDamage * diff.damageTakenMul * armorReduction));
            world.player.hp -= dmg;
            world.player.invulnerabilityTimer = GameConstants::kInvulnerabilitySeconds;
            world.screenFlashTimer = 0.15F;
            world.screenFlashColor = RenderUtils::packRGBA(255, 40, 40);
            world.shakeTimer = 0.1F;
            world.shakeIntensity = 3.0F;
            AudioSystem::play(SoundId::PlayerHurt);
        }
    }

    for (auto& enemy : world.enemies) {
        if (enemy.hp <= 0 && enemy.type == EnemyType::Exploder) {
            AudioSystem::play(SoundId::Explosion);
            ParticleSystem::spawnWorld(world, enemy.position, RenderUtils::packRGBA(80, 255, 80), 22, 3.5F, 0.5F);
            world.shakeTimer = 0.2F;
            world.shakeIntensity = 5.0F;
            if (distanceSquared(enemy.position, world.player.position) < GameConstants::kExplodeRadius * GameConstants::kExplodeRadius) {
                if (world.player.shieldTimer <= 0.0F) {
                    float armorReduction = 1.0F - world.player.stats[static_cast<int>(StatId::BodyArmor)] * GameConstants::kStatArmorPerPoint;
                    world.player.hp -= std::max(1, static_cast<int>(2 * diff.damageTakenMul * armorReduction));
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
                case PickupType::Key:         world.player.keys += 1; break;
            }
            ParticleSystem::spawnWorld(world, it->position, RenderUtils::packRGBA(255, 255, 255), 6, 1.5F, 0.3F);
            it = world.pickups.erase(it);
        } else {
            ++it;
        }
    }

    ParticleSystem::update(world, dt);
    updateDecals(world, dt);

    if (world.player.hp <= 0) {
        world.runActive = false;
        world.bestWave = std::max(world.bestWave, world.wave);
        world.bestLevel = std::max(world.bestLevel, world.level);
        CombatSystem::insertLeaderboard(world);
        SaveSystem::save(world);
        InputSystem::setMouseCapture(world, false);
        states.clearAndSet(GameStateId::GameOver);
        return;
    }
}

}  // namespace UpdateSystem
