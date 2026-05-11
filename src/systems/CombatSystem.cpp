#include "systems/CombatSystem.hpp"
#include "GameConstants.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"
#include "systems/ParticleSystem.hpp"
#include "systems/RenderUtils.hpp"
#include "data/GameData.hpp"

#include <algorithm>
#include <cmath>

namespace CombatSystem {

void firePlayerWeapon(World& world) {
    const WeaponSpec spec = getWeaponSpec(world.player.currentWeapon);
    float dmgMul = world.player.damageMultiplier;
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

        Enemy* bestTarget = nullptr;
        float bestDistance = 99.0F;
        for (auto& enemy : world.enemies) {
            if (enemy.hp <= 0) continue;
            Vec2 toEnemy {enemy.position.x - world.player.position.x, enemy.position.y - world.player.position.y};
            float dist = vecLength(toEnemy);
            if (dist > 14.0F) continue;
            float enemyAngle = std::atan2(toEnemy.y, toEnemy.x);
            float delta = std::fabs(wrapAngle(enemyAngle - shotAngle));
            float cone = 0.06F + (GameConstants::kEnemySize * enemy.spriteScale / std::max(0.3F, dist));
            if (delta <= cone && Map::hasLineOfSight(world.player.position, enemy.position) && dist < bestDistance) {
                bestDistance = dist;
                bestTarget = &enemy;
            }
        }
        if (bestTarget != nullptr) {
            bestTarget->hp -= pelletDamage;
            AudioSystem::play(SoundId::EnemyHit);
            ParticleSystem::spawnWorld(world, bestTarget->position, RenderUtils::packRGBA(255, 255, 200), 4, 1.5F, 0.2F);
            if (bestTarget->hp <= 0) {
                world.score += bestTarget->scoreValue;
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

                if (randomFloat(0.0F, 1.0F) < 0.2F) {
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
    world.player.weaponCooldown = spec.cooldownSeconds / world.player.fireRateMultiplier;
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

void spawnWave(World& world) {
    const DifficultySettings diff = getDifficultySettings(world.difficulty);
    world.enemies.clear();
    world.projectiles.clear();
    world.pickups.clear();
    world.killsInWave = 0;
    world.killsToClearWave = GameConstants::kBaseWaveKills + (world.wave - 1) * 4;
    world.waveCleared = false;
    world.pendingUpgrades.clear();
    world.player.position = {GameConstants::kPlayerStartX, GameConstants::kPlayerStartY};
    world.player.facingRadians = 0.0F;
    world.player.weaponCooldown = 0.0F;

    int enemyCount = 8 + world.wave * 3;
    bool bossWave = (world.wave % 5 == 0) && world.wave > 0;

    if (bossWave) {
        Enemy boss = makeEnemyForWave(EnemyType::Boss, world.wave, Map::randomOpenCell(), diff);
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
        Enemy e = makeEnemyForWave(t, world.wave, Map::randomOpenCell(), diff);
        e.speed *= GameConstants::kEnemySpeedScale;
        world.enemies.push_back(e);
    }
}

void resetRun(World& world) {
    world.score = 0;
    world.wave = 1;
    world.elapsedRunSeconds = 0.0F;
    world.runActive = true;
    world.particles.clear();
    world.pickups.clear();
    world.screenFlashTimer = 0.0F;
    world.player = Player {};
    spawnWave(world);
}

void insertLeaderboard(World& world) {
    LeaderboardEntry entry {world.score, world.wave, world.difficulty, world.elapsedRunSeconds};
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

}  // namespace CombatSystem
