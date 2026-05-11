#pragma once

#include "entities/Components.hpp"
#include "GameConstants.hpp"

#include <cstdint>

namespace RenderUtils {

struct Texture {
    uint32_t pixels[GameConstants::kTexSize * GameConstants::kTexSize];
};

inline uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

inline uint32_t dimColor(uint32_t color, float f) {
    return packRGBA(static_cast<uint8_t>((color & 0xFF) * f),
                    static_cast<uint8_t>(((color >> 8) & 0xFF) * f),
                    static_cast<uint8_t>(((color >> 16) & 0xFF) * f));
}

inline uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
    float it = 1.0F - t;
    return packRGBA(
        static_cast<uint8_t>((a & 0xFF) * it + (b & 0xFF) * t),
        static_cast<uint8_t>(((a >> 8) & 0xFF) * it + ((b >> 8) & 0xFF) * t),
        static_cast<uint8_t>(((a >> 16) & 0xFF) * it + ((b >> 16) & 0xFF) * t));
}

inline uint32_t enemyColor(EnemyType type) {
    switch (type) {
        case EnemyType::Fast:     return packRGBA(255, 195, 66);
        case EnemyType::Tank:     return packRGBA(193, 97, 255);
        case EnemyType::Shooter:  return packRGBA(88, 190, 255);
        case EnemyType::Boss:     return packRGBA(255, 140, 40);
        case EnemyType::Exploder: return packRGBA(80, 255, 80);
        case EnemyType::Healer:   return packRGBA(255, 200, 220);
        default:                  return packRGBA(255, 90, 90);
    }
}

inline uint32_t sampleTexture(const Texture& tex, float u, float v) {
    return tex.pixels[(static_cast<int>(v * GameConstants::kTexSize) & (GameConstants::kTexSize - 1)) *
                          GameConstants::kTexSize +
                      (static_cast<int>(u * GameConstants::kTexSize) & (GameConstants::kTexSize - 1))];
}

void generateTextures();
const Texture& wallTex1();
const Texture& wallTex2();
const Texture& floorTex();
const Texture& ceilTex();

}  // namespace RenderUtils
