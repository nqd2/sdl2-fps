#pragma once

#include "entities/Components.hpp"

namespace Map {

constexpr float kPlayerRadius = 0.22F;

char cellAt(const World& world, int x, int y);
bool isWall(const World& world, float x, float y);
bool isBlocker(const World& world, float x, float y);
bool canMoveTo(const World& world, const Vec2& p);
Vec2 randomOpenCell(const World& world);
bool hasLineOfSight(const World& world, Vec2 from, Vec2 to);
void setCell(World& world, int x, int y, char c);

}  // namespace Map
