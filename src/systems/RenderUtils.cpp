#include "systems/RenderUtils.hpp"

#include <algorithm>
#include <cmath>

namespace RenderUtils {

namespace {
Texture gWallTex;
Texture gWallTex2;
Texture gFloorTex;
Texture gCeilTex;
bool gGenerated = false;
}

void generateTextures() {
    if (gGenerated) return;
    gGenerated = true;

    constexpr int S = GameConstants::kTexSize;

    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        int bx = x / 8, by = y / 8;
        bool border = (x % 8 == 0) || (y % 8 == 0);
        bool checker = ((bx + by) & 1) != 0;
        uint8_t base = checker ? 145 : 125;
        if (border) base -= 30;
        gWallTex.pixels[y * S + x] = packRGBA(base, static_cast<uint8_t>(base * 0.88F), static_cast<uint8_t>(base * 0.72F));
    }
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        bool borderH = (y % 16 < 2);
        bool borderV = (x % 32 < 2) && ((y / 16) % 2 == 0);
        bool borderV2 = ((x + 16) % 32 < 2) && ((y / 16) % 2 == 1);
        uint8_t base = (borderH || borderV || borderV2) ? 72 : 115;
        int noise = ((x * 7 + y * 13) % 11) - 5;
        base = static_cast<uint8_t>(std::max(40, std::min(160, static_cast<int>(base) + noise)));
        gWallTex2.pixels[y * S + x] = packRGBA(static_cast<uint8_t>(base * 0.85F), static_cast<uint8_t>(base * 0.78F), static_cast<uint8_t>(base * 0.65F));
    }
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        int noise = ((x * 17 + y * 31 + x * y) % 19) - 9;
        uint8_t base = static_cast<uint8_t>(std::max(28, std::min(60, 44 + noise)));
        gFloorTex.pixels[y * S + x] = packRGBA(static_cast<uint8_t>(base * 0.9F), static_cast<uint8_t>(base * 0.85F), base);
    }
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        int noise = ((x * 11 + y * 23) % 13) - 6;
        uint8_t base = static_cast<uint8_t>(std::max(18, std::min(50, 32 + noise)));
        gCeilTex.pixels[y * S + x] = packRGBA(static_cast<uint8_t>(base * 0.7F), static_cast<uint8_t>(base * 0.75F), base);
    }
}

const Texture& wallTex1() { return gWallTex; }
const Texture& wallTex2() { return gWallTex2; }
const Texture& floorTex() { return gFloorTex; }
const Texture& ceilTex()  { return gCeilTex; }

}  // namespace RenderUtils
