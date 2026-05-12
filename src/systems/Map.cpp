#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"

#include <cmath>
#include <cstdio>

namespace Map {

char cellAt(const World& world, int x, int y) {
    if (x < 0 || y < 0 || x >= world.mapWidth || y >= world.mapHeight) return '#';
    return world.mapGrid[y * world.mapWidth + x];
}

bool isWall(const World& world, float x, float y) {
    int mx = static_cast<int>(std::floor(x));
    int my = static_cast<int>(std::floor(y));
    char c = cellAt(world, mx, my);
    return c == '#';
}

bool isBlocker(const World& world, float x, float y) {
    int mx = static_cast<int>(std::floor(x));
    int my = static_cast<int>(std::floor(y));
    char c = cellAt(world, mx, my);
    return c == '#' || c == 'D';
}

bool canMoveTo(const World& world, const Vec2& p) {
    return !isBlocker(world, p.x - kPlayerRadius, p.y - kPlayerRadius) &&
           !isBlocker(world, p.x + kPlayerRadius, p.y - kPlayerRadius) &&
           !isBlocker(world, p.x - kPlayerRadius, p.y + kPlayerRadius) &&
           !isBlocker(world, p.x + kPlayerRadius, p.y + kPlayerRadius);
}

Vec2 randomOpenCell(const World& world) {
    constexpr int kMaxAttempts = 1000;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        int mx = RNG::uniformInt(1, world.mapWidth - 2);
        int my = RNG::uniformInt(1, world.mapHeight - 2);
        char c = cellAt(world, mx, my);
        if (c == '.' || c == 'E') return {mx + 0.5F, my + 0.5F};
    }
    std::fprintf(stderr, "randomOpenCell: no open cell found after %d attempts\n", kMaxAttempts);
    return {world.mapWidth / 2.0F, world.mapHeight / 2.0F};
}

bool hasLineOfSight(const World& world, Vec2 from, Vec2 to) {
    Vec2 ray = {to.x - from.x, to.y - from.y};
    float dist = vecLength(ray);
    if (dist < 0.001F) return true;
    ray = {ray.x / dist, ray.y / dist};
    for (float t = 0.0F; t < dist; t += 0.08F) {
        if (isBlocker(world, from.x + ray.x * t, from.y + ray.y * t)) return false;
    }
    return true;
}

void setCell(World& world, int x, int y, char c) {
    if (x < 0 || y < 0 || x >= world.mapWidth || y >= world.mapHeight) return;
    world.mapGrid[y * world.mapWidth + x] = c;
}

}  // namespace Map
