#pragma once

#include "entities/Components.hpp"

#include <cmath>

inline float vecLength(Vec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline Vec2 vecNormalize(Vec2 v) {
    float len = vecLength(v);
    return (len <= 0.0001F) ? Vec2{} : Vec2{v.x / len, v.y / len};
}

inline float distanceSquared(Vec2 a, Vec2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

inline float wrapAngle(float r) {
    r = std::fmod(r + 3.14159265358979F, 6.28318530717959F);
    if (r < 0.0F) r += 6.28318530717959F;
    return r - 3.14159265358979F;
}

float randomFloat(float lo, float hi);

namespace RNG {
void seed(unsigned int s);
int uniformInt(int lo, int hi);
}  // namespace RNG
