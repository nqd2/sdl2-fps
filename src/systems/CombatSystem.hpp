#pragma once

#include "entities/Components.hpp"

namespace CombatSystem {

void firePlayerWeapon(World& world);
void applyUpgrade(World& world, const UpgradeOption& up);
void applyStatEffects(World& world);
void awardXp(World& world, int amount);
void spawnLevel(World& world);
void resetRun(World& world);
void insertLeaderboard(World& world);
void prepareShop(World& world);
void tryOpenDoor(World& world);

}  // namespace CombatSystem
