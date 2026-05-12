#include "systems/CombatSystem.hpp"
#include "GameConstants.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"
#include "systems/MazeGen.hpp"
#include "systems/ParticleSystem.hpp"
#include "systems/RenderUtils.hpp"
#include "data/GameData.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace CombatSystem {

void applyStatEffects(World& world) {
    Player& p = world.player;
    int maxHpStat = p.stats[static_cast<int>(StatId::MaxHp)];
    int newMaxHp = 10 + static_cast<int>(maxHpStat * GameConstants::kStatMaxHpPerPoint);
    if (newMaxHp > p.maxHp) {
        int gained = newMaxHp - p.maxHp;
        p.maxHp = newMaxHp;
        p.hp = std::min(p.maxHp, p.hp + gained);
    } else {
        p.maxHp = newMaxHp;
    }
    p.baseSpeed = GameConstants::kPlayerBaseSpeed + p.stats[static_cast<int>(StatId::MoveSpeed)] * GameConstants::kStatSpeedPerPoint;
}

void awardXp(World& world, int amount) {
    Player& p = world.player;
    if (p.playerLevel >= kPlayerLevelCap) return;
    p.xp += amount;
    while (p.playerLevel < kPlayerLevelCap) {
        int needed = 20 + p.playerLevel * 15;
        if (p.xp < needed) break;
        p.xp -= needed;
        p.playerLevel += 1;
        p.statPoints += 1;
        p.levelUpFlash = 1.5F;
    }
    if (p.playerLevel >= kPlayerLevelCap) p.xp = 0;
}

void firePlayerWeapon(World& world) {
    const WeaponSpec spec = getWeaponSpec(world.player.currentWeapon);
    float dmgMul = world.player.damageMultiplier
        * (1.0F + world.player.stats[static_cast<int>(StatId::BulletDamage)] * GameConstants::kStatDmgMulPerPoint);
    if (world.player.damageBoostTimer > 0.0F) dmgMul *= 1.15F;
    int pelletDamage = std::max(1, static_cast<int>(std::round(static_cast<float>(spec.damagePerPellet) * dmgMul)));
    float spreadHalf = spec.spreadRadians * 0.5F;

    SoundId shootSound = SoundId::ShootPistol;
    if (world.player.currentWeapon == WeaponType::Shotgun) shootSound = SoundId::ShootShotgun;
    else if (world.player.currentWeapon == WeaponType::Rapid) shootSound = SoundId::ShootRapid;
    AudioSystem::play(shootSound);

    world.player.recoilTimer = 0.12F;
    ParticleSystem::spawnScreen(world, static_cast<float>(world.width / 2), static_cast<float>(world.height / 2 + 20),
                                RenderUtils::packRGBA(255, 220, 80), spec.pellets <= 1 ? 4 : 8, 200.0F, 0.12F);

    for (int i = 0; i < spec.pellets; ++i) {
        float t = (spec.pellets <= 1) ? 0.5F : static_cast<float>(i) / static_cast<float>(spec.pellets - 1);
        float shotAngle = world.player.facingRadians + (-spreadHalf + spec.spreadRadians * t);

        float hitscanRange = GameConstants::kBaseHitscanRange
            + world.player.stats[static_cast<int>(StatId::BulletRange)] * GameConstants::kStatRangePerPoint;

        Enemy* bestTarget = nullptr;
        float bestDistance = 99.0F;
        for (auto& enemy : world.enemies) {
            if (enemy.hp <= 0) continue;
            Vec2 toEnemy {enemy.position.x - world.player.position.x, enemy.position.y - world.player.position.y};
            float dist = vecLength(toEnemy);
            if (dist > hitscanRange) continue;
            float enemyAngle = std::atan2(toEnemy.y, toEnemy.x);
            float delta = std::fabs(wrapAngle(enemyAngle - shotAngle));
            float cone = 0.06F + (GameConstants::kEnemySize * enemy.spriteScale / std::max(0.3F, dist));
            if (delta <= cone && Map::hasLineOfSight(world, world.player.position, enemy.position) && dist < bestDistance) {
                bestDistance = dist;
                bestTarget = &enemy;
            }
        }
        if (bestTarget != nullptr) {
            bestTarget->hp -= pelletDamage;
            AudioSystem::play(SoundId::EnemyHit);
            ParticleSystem::spawnWorld(world, bestTarget->position, RenderUtils::packRGBA(255, 255, 200), 4, 1.5F, 0.2F);

            // Blood decal
            if (static_cast<int>(world.decals.size()) < kMaxDecals) {
                Decal decal {};
                decal.cellX = static_cast<int>(std::floor(bestTarget->position.x));
                decal.cellY = static_cast<int>(std::floor(bestTarget->position.y));
                decal.color = RenderUtils::packRGBA(140, 10, 10);
                decal.alpha = 0.8F;
                world.decals.push_back(decal);
            }

            if (bestTarget->hp <= 0) {
                world.score += bestTarget->scoreValue;
                awardXp(world, bestTarget->scoreValue);
                world.killsInWave += 1;
                AudioSystem::play(SoundId::EnemyDeath);

                uint32_t deathColor = RenderUtils::enemyColor(bestTarget->type);
                int deathCount = 12;
                switch (bestTarget->type) {
                    case EnemyType::Tank:     deathCount = 18; break;
                    case EnemyType::Boss:     deathCount = 25; break;
                    case EnemyType::Exploder: deathCount = 22; break;
                    default: break;
                }
                ParticleSystem::spawnWorld(world, bestTarget->position, deathColor, deathCount, 2.5F, 0.4F);

                // Bigger blood decal on death
                if (static_cast<int>(world.decals.size()) < kMaxDecals) {
                    Decal decal {};
                    decal.cellX = static_cast<int>(std::floor(bestTarget->position.x));
                    decal.cellY = static_cast<int>(std::floor(bestTarget->position.y));
                    decal.color = RenderUtils::packRGBA(180, 20, 20);
                    decal.alpha = 1.0F;
                    world.decals.push_back(decal);
                }

                // Key/pickup drops
                float roll = randomFloat(0.0F, 1.0F);
                if (roll < 0.12F) {
                    world.pickups.push_back(Pickup{bestTarget->position, PickupType::Key, 20.0F, randomFloat(0.0F, GameConstants::kTwoPi)});
                } else if (roll < 0.3F) {
                    PickupType pt = PickupType::Health;
                    float r = randomFloat(0.0F, 1.0F);
                    if (r > 0.75F) pt = PickupType::Shield;
                    else if (r > 0.5F) pt = PickupType::SpeedBoost;
                    else if (r > 0.25F) pt = PickupType::DamageBoost;
                    world.pickups.push_back(Pickup{bestTarget->position, pt, 15.0F, randomFloat(0.0F, GameConstants::kTwoPi)});
                }
            }
        }
    }
    float fireRateMul = world.player.fireRateMultiplier
        * (1.0F + world.player.stats[static_cast<int>(StatId::FireRate)] * GameConstants::kStatFireRatePerPoint);
    world.player.weaponCooldown = spec.cooldownSeconds / fireRateMul;
}

void applyUpgrade(World& world, const UpgradeOption& up) {
    world.player.damageMultiplier *= up.damageMultiplier;
    world.player.fireRateMultiplier *= up.fireRateMultiplier;
    world.player.speed += up.moveSpeedBonus;
    world.player.baseSpeed += up.moveSpeedBonus;
    if (up.hpBonus > 0) {
        world.player.maxHp += up.hpBonus;
        world.player.hp = std::min(world.player.maxHp, world.player.hp + up.hpBonus);
    }
    if (up.id == UpgradeId::FullArsenal) world.player.unlockedWeapons = 3;
}

void spawnLevel(World& world) {
    const DifficultySettings diff = getDifficultySettings(world.difficulty);
    auto maze = MazeGen::generate(world.level, world.runSeed + static_cast<unsigned int>(world.level));

    world.mapGrid = std::move(maze.grid);
    world.mapWidth = maze.width;
    world.mapHeight = maze.height;
    world.exitX = maze.exitX;
    world.exitY = maze.exitY;

    world.explored.assign(world.mapWidth * world.mapHeight, false);
    world.enemies.clear();
    world.projectiles.clear();
    world.pickups.clear();
    world.decals.clear();
    world.killsInWave = 0;
    world.waveCleared = false;
    world.pendingUpgrades.clear();

    world.player.position = {maze.startX + 0.5F, maze.startY + 0.5F};
    world.player.facingRadians = 0.0F;
    world.player.weaponCooldown = 0.0F;

    int enemyCount = 6 + world.level * 4;
    bool bossLevel = (world.level % 5 == 0) && world.level > 0;
    world.killsToClearWave = enemyCount;

    if (bossLevel) {
        Enemy boss = makeEnemyForWave(EnemyType::Boss, world.level, Map::randomOpenCell(world), diff);
        boss.speed *= GameConstants::kEnemySpeedScale;
        world.enemies.push_back(boss);
        enemyCount -= 3;
    }

    for (int i = 0; i < enemyCount; ++i) {
        EnemyType t = EnemyType::Grunt;
        int roll = RNG::uniformInt(0, 99);
        if (roll > 92) t = EnemyType::Healer;
        else if (roll > 86) t = EnemyType::Exploder;
        else if (roll > 80) t = EnemyType::Tank;
        else if (roll > 62) t = EnemyType::Shooter;
        else if (roll > 38) t = EnemyType::Fast;
        Enemy e = makeEnemyForWave(t, world.level, Map::randomOpenCell(world), diff);
        e.speed *= GameConstants::kEnemySpeedScale;
        world.enemies.push_back(e);
    }

    // Place some keys in dead-end-ish spots
    int keysToPlace = std::max(1, world.level / 3 + 1);
    for (int i = 0; i < keysToPlace; ++i) {
        Vec2 pos = Map::randomOpenCell(world);
        world.pickups.push_back(Pickup{pos, PickupType::Key, 999.0F, randomFloat(0.0F, GameConstants::kTwoPi)});
    }
}

void resetRun(World& world) {
    world.score = 0;
    world.level = 1;
    world.wave = 1;
    world.elapsedRunSeconds = 0.0F;
    world.runActive = true;
    world.particles.clear();
    world.pickups.clear();
    world.decals.clear();
    world.projectiles.clear();
    world.screenFlashTimer = 0.0F;
    world.shakeTimer = 0.0F;
    world.shakeIntensity = 0.0F;
    world.player = Player {};
    world.player.mouseSensitivity = world.settings.mouseSensitivity;
    world.runSeed = static_cast<unsigned int>(SDL_GetTicks());
    spawnLevel(world);
}

void insertLeaderboard(World& world) {
    LeaderboardEntry entry {world.score, world.level, world.difficulty, world.elapsedRunSeconds};
    int pos = world.leaderboardCount;
    for (int i = 0; i < world.leaderboardCount; ++i) {
        if (entry.score > world.leaderboard[i].score) { pos = i; break; }
    }
    if (pos >= kMaxLeaderboard) return;
    for (int i = std::min(world.leaderboardCount, kMaxLeaderboard - 1); i > pos; --i)
        world.leaderboard[i] = world.leaderboard[i - 1];
    world.leaderboard[pos] = entry;
    if (world.leaderboardCount < kMaxLeaderboard) world.leaderboardCount++;
}

void prepareShop(World& world) {
    world.shopItems.clear();
    world.shopCursor = 0;

    const auto pool = allUpgradeOptions();
    int offset = world.level % static_cast<int>(pool.size());
    for (int i = 0; i < 3; ++i) {
        const auto& up = pool[(offset + i * 2) % pool.size()];
        ShopItem item;
        item.name = up.title;
        item.description = up.description;
        item.cost = 50 + world.level * 15;
        item.type = ShopItem::Type::Upgrade;
        item.upgradeIndex = static_cast<int>((offset + i * 2) % pool.size());
        world.shopItems.push_back(item);
    }
    {
        ShopItem item;
        item.name = "Heal +5 HP";
        item.description = "Restore 5 hit points";
        item.cost = 30;
        item.type = ShopItem::Type::Health;
        world.shopItems.push_back(item);
    }
    {
        ShopItem item;
        item.name = "Key";
        item.description = "Opens a locked door";
        item.cost = 20;
        item.type = ShopItem::Type::Key;
        world.shopItems.push_back(item);
    }
}

void tryOpenDoor(World& world) {
    float px = world.player.position.x;
    float py = world.player.position.y;
    float facing = world.player.facingRadians;
    float checkDist = GameConstants::kDoorInteractDist;

    for (float d = 0.3F; d <= checkDist; d += 0.2F) {
        int cx = static_cast<int>(std::floor(px + std::cos(facing) * d));
        int cy = static_cast<int>(std::floor(py + std::sin(facing) * d));
        if (cx >= 0 && cx < world.mapWidth && cy >= 0 && cy < world.mapHeight) {
            char c = world.mapGrid[cy * world.mapWidth + cx];
            if (c == 'D') {
                if (world.player.keys > 0) {
                    world.player.keys -= 1;
                    Map::setCell(world, cx, cy, '.');
                    AudioSystem::play(SoundId::DoorOpen);
                    ParticleSystem::spawnWorld(world, {cx + 0.5F, cy + 0.5F}, RenderUtils::packRGBA(100, 150, 255), 8, 2.0F, 0.3F);
                }
                return;
            }
        }
    }
}

}  // namespace CombatSystem
