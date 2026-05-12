#include "data/GameData.hpp"
#include "GameConstants.hpp"

#include <algorithm>
#include <cmath>

WeaponSpec getWeaponSpec(WeaponType type) {
    switch (type) {
        case WeaponType::Shotgun:
            return {type, "Shotgun", 6, 0.45F, 0.75F, 1, 420.0F};
        case WeaponType::Rapid:
            return {type, "Rapid", 1, 0.05F, 0.09F, 1, 520.0F};
        case WeaponType::Pistol:
        default:
            return {WeaponType::Pistol, "Pistol", 1, 0.0F, 0.28F, 2, 480.0F};
    }
}

Enemy makeEnemyForWave(EnemyType type, int wave, const Vec2& spawn, const DifficultySettings& diff) {
    Enemy e {};
    e.type = type;
    e.position = spawn;
    e.velocity = {};
    const float waveScale = 1.0F + 0.12F * static_cast<float>(std::max(0, wave - 1));
    switch (type) {
        case EnemyType::Fast:
            e.hp = 2 + wave / 3;
            e.speed = 105.0F * waveScale;
            e.contactDamage = 1;
            e.scoreValue = 18;
            break;
        case EnemyType::Tank:
            e.hp = 7 + wave;
            e.speed = 42.0F * waveScale;
            e.contactDamage = 2;
            e.scoreValue = 35;
            break;
        case EnemyType::Shooter:
            e.hp = 4 + wave / 2;
            e.speed = 52.0F * waveScale;
            e.contactDamage = 1;
            e.fireCooldown = std::max(0.7F, 1.45F - 0.04F * wave);
            e.scoreValue = 28;
            break;
        case EnemyType::Boss:
            e.hp = 20 + wave * 3;
            e.speed = 35.0F * waveScale;
            e.contactDamage = 3;
            e.scoreValue = 100;
            e.spriteScale = 1.5F;
            break;
        case EnemyType::Exploder:
            e.hp = 1 + wave / 4;
            e.speed = 90.0F * waveScale;
            e.contactDamage = 1;
            e.scoreValue = 22;
            break;
        case EnemyType::Healer:
            e.hp = 3 + wave / 3;
            e.speed = 50.0F * waveScale;
            e.contactDamage = 0;
            e.fireCooldown = 2.0F;
            e.scoreValue = 30;
            break;
        case EnemyType::Grunt:
        default:
            e.hp = 3 + wave / 2;
            e.speed = 62.0F * waveScale;
            e.contactDamage = 1;
            e.scoreValue = 12;
            break;
    }
    e.hp = std::max(1, static_cast<int>(static_cast<float>(e.hp) * diff.enemyHpMul));
    e.maxHp = e.hp;
    e.speed *= diff.enemySpeedMul;
    e.scoreValue = static_cast<int>(static_cast<float>(e.scoreValue) * diff.scoreMul);
    return e;
}

std::array<UpgradeOption, 6> allUpgradeOptions() {
    return {UpgradeOption {UpgradeId::HighCaliber,    "High Caliber",    "+25% damage",                       1.25F, 1.0F, 0.0F, 0},
            UpgradeOption {UpgradeId::HairTrigger,    "Hair Trigger",    "+20% fire rate",                    1.0F, 1.2F, 0.0F, 0},
            UpgradeOption {UpgradeId::KineticBoots,   "Kinetic Boots",   "+0.35 move speed",                  1.0F, 1.0F, 0.35F, 0},
            UpgradeOption {UpgradeId::ReinforcedSuit, "Reinforced Suit", "+4 max HP and heal 4",              1.0F, 1.0F, 0.0F, 4},
            UpgradeOption {UpgradeId::FullArsenal,    "Full Arsenal",    "Unlock all weapons",                1.0F, 1.0F, 0.0F, 0},
            UpgradeOption {UpgradeId::Overdrive,      "Overdrive",       "+15% damage and +10% fire rate",    1.15F, 1.1F, 0.0F, 0}};
}

std::vector<UpgradeOption> pickUpgradeChoices(int wave) {
    const auto pool = allUpgradeOptions();
    const int offset = wave % static_cast<int>(pool.size());
    return {pool[offset], pool[(offset + 2) % pool.size()], pool[(offset + 4) % pool.size()]};
}
