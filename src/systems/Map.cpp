#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"

#include <cmath>
#include <cstdio>

namespace Map {

bool isWall(float x, float y) {
    int mx = static_cast<int>(std::floor(x));
    int my = static_cast<int>(std::floor(y));
    if (mx < 0 || my < 0 || mx >= kWidth || my >= kHeight) return true;
    return kRows[my][mx] == '#';
}

bool canMoveTo(const Vec2& p) {
    return !isWall(p.x - kPlayerRadius, p.y - kPlayerRadius) &&
           !isWall(p.x + kPlayerRadius, p.y - kPlayerRadius) &&
           !isWall(p.x - kPlayerRadius, p.y + kPlayerRadius) &&
           !isWall(p.x + kPlayerRadius, p.y + kPlayerRadius);
}

Vec2 randomOpenCell() {
    constexpr int kMaxAttempts = 1000;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        int mx = RNG::uniformInt(1, kWidth - 2);
        int my = RNG::uniformInt(1, kHeight - 2);
        if (kRows[my][mx] == '.') return {mx + 0.5F, my + 0.5F};
    }
    std::fprintf(stderr, "randomOpenCell: no open cell found after %d attempts\n", kMaxAttempts);
    return {kWidth / 2.0F, kHeight / 2.0F};
}

bool hasLineOfSight(Vec2 from, Vec2 to) {
    Vec2 ray = {to.x - from.x, to.y - from.y};
    float dist = vecLength(ray);
    if (dist < 0.001F) return true;
    ray = {ray.x / dist, ray.y / dist};
    for (float t = 0.0F; t < dist; t += 0.08F) {
        if (isWall(from.x + ray.x * t, from.y + ray.y * t)) return false;
    }
    return true;
}

}  // namespace Map
