#pragma once

#include "entities/Components.hpp"

#include <array>
#include <vector>

struct WeaponSpec {
    WeaponType type;
    const char* name;
    int pellets;
    float spreadRadians;
    float cooldownSeconds;
    int damagePerPellet;
    float projectileSpeed;
};

WeaponSpec getWeaponSpec(WeaponType type);
Enemy makeEnemyForWave(EnemyType type, int wave, const Vec2& spawn, const DifficultySettings& diff);
std::array<UpgradeOption, 6> allUpgradeOptions();
std::vector<UpgradeOption> pickUpgradeChoices(int wave);
