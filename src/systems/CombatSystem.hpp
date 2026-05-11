#pragma once

#include "entities/Components.hpp"

namespace CombatSystem {

void firePlayerWeapon(World& world);
void applyUpgrade(World& world, const UpgradeOption& up);
void spawnWave(World& world);
void resetRun(World& world);
void insertLeaderboard(World& world);

}  // namespace CombatSystem
