#include "systems/RenderSystem.hpp"
#include "GameConstants.hpp"
#include "systems/BitmapFont.hpp"
#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"
#include "systems/RenderUtils.hpp"
#include "data/GameData.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace RenderSystem {

namespace {

using RenderUtils::packRGBA;
using RenderUtils::dimColor;
using RenderUtils::lerpColor;
using RenderUtils::blendColor;
using RenderUtils::enemyColor;
using RenderUtils::sampleTexture;

SDL_Texture* gFramebuffer = nullptr;
int gFbWidth = 0;
int gFbHeight = 0;

std::vector<float> gZBuffer;

struct EnemySprite {
    float distance, screenX, halfSize;
    EnemyType type;
    int hp, maxHp;
};
std::vector<EnemySprite> gSprites;

// Per-frame decal grid for O(1) floor-pixel lookup instead of O(n)
struct DecalCell { uint32_t color; float alpha; bool active; };
std::vector<DecalCell> gDecalGrid;
int gDecalGridW = 0, gDecalGridH = 0;

void buildDecalGrid(const World& world) {
    gDecalGridW = world.mapWidth;
    gDecalGridH = world.mapHeight;
    gDecalGrid.assign(gDecalGridW * gDecalGridH, {0, 0.0F, false});
    for (const auto& d : world.decals) {
        if (d.cellX >= 0 && d.cellX < gDecalGridW && d.cellY >= 0 && d.cellY < gDecalGridH) {
            auto& cell = gDecalGrid[d.cellY * gDecalGridW + d.cellX];
            cell.color = d.color;
            cell.alpha = std::max(cell.alpha, d.alpha);
            cell.active = true;
        }
    }
}

const uint32_t kWhite    = packRGBA(255, 255, 255);
const uint32_t kOffWhite = packRGBA(210, 215, 225);
const uint32_t kGold     = packRGBA(255, 210, 80);
const uint32_t kCyan     = packRGBA(88, 200, 255);
const uint32_t kDimWhite = packRGBA(160, 165, 175);
const uint32_t kGreen    = packRGBA(80, 220, 115);
const uint32_t kRed      = packRGBA(255, 90, 90);
const uint32_t kPanelBg  = packRGBA(20, 24, 40);
const uint32_t kPanelEdge = packRGBA(55, 75, 130);
const uint32_t kKeyYellow = packRGBA(255, 230, 50);
const uint32_t kExitGreen = packRGBA(40, 255, 120);
const uint32_t kDoorBlue  = packRGBA(60, 100, 200);

struct Canvas {
    uint32_t* px;
    int pp, w, h;

    void fill(int rx, int ry, int rw, int rh, uint32_t col) const {
        for (int y = std::max(0, ry); y < std::min(h, ry + rh); ++y)
            for (int x = std::max(0, rx); x < std::min(w, rx + rw); ++x)
                px[y * pp + x] = col;
    }

    void fillAlpha(int rx, int ry, int rw, int rh, uint32_t col, float alpha) const {
        uint8_t sr = col & 0xFF, sg = (col >> 8) & 0xFF, sb = (col >> 16) & 0xFF;
        float ia = 1.0F - alpha;
        for (int y = std::max(0, ry); y < std::min(h, ry + rh); ++y)
            for (int x = std::max(0, rx); x < std::min(w, rx + rw); ++x) {
                uint32_t d = px[y * pp + x];
                px[y * pp + x] = packRGBA(static_cast<uint8_t>(sr * alpha + (d & 0xFF) * ia),
                    static_cast<uint8_t>(sg * alpha + ((d >> 8) & 0xFF) * ia),
                    static_cast<uint8_t>(sb * alpha + ((d >> 16) & 0xFF) * ia));
            }
    }

    void outlinedRect(int rx, int ry, int rw, int rh, uint32_t f, uint32_t b, int bw) const {
        fill(rx, ry, rw, rh, f);
        fill(rx, ry, rw, bw, b); fill(rx, ry + rh - bw, rw, bw, b);
        fill(rx, ry, bw, rh, b); fill(rx + rw - bw, ry, bw, rh, b);
    }

    void txt(const char* t, int cx, int cy, int s, uint32_t c) const {
        BitmapFont::drawTextCentered(px, pp, w, h, t, cx, cy, s, c);
    }

    void txtL(const char* t, int lx, int ly, int s, uint32_t c) const {
        BitmapFont::drawText(px, pp, w, h, t, lx, ly, s, c);
    }

    void gradient(uint32_t topColor, uint32_t bottomColor) const {
        for (int y = 0; y < h; ++y) {
            float t = static_cast<float>(y) / static_cast<float>(h);
            uint8_t r0 = topColor & 0xFF, g0 = (topColor >> 8) & 0xFF, b0 = (topColor >> 16) & 0xFF;
            uint8_t r1 = bottomColor & 0xFF, g1 = (bottomColor >> 8) & 0xFF, b1 = (bottomColor >> 16) & 0xFF;
            uint32_t c = packRGBA(
                static_cast<uint8_t>(r0 + (r1 - r0) * t),
                static_cast<uint8_t>(g0 + (g1 - g0) * t),
                static_cast<uint8_t>(b0 + (b1 - b0) * t));
            for (int x = 0; x < w; ++x) px[y * pp + x] = c;
        }
    }
};

void renderMainMenu(const Canvas& c, const World& world) {
    c.gradient(packRGBA(10, 14, 30), packRGBA(28, 34, 65));
    c.outlinedRect(c.w / 2 - 280, c.h / 2 - 180, 560, 360, packRGBA(18, 22, 42), kPanelEdge, 2);
    c.txt("IRON MAZE", c.w / 2, c.h / 2 - 140, 5, kGold);
    c.txt("V2", c.w / 2, c.h / 2 - 92, 3, kDimWhite);
    c.fill(c.w / 2 - 160, c.h / 2 - 70, 320, 2, kPanelEdge);
    c.txt("WASD - Move    Mouse - Look", c.w / 2, c.h / 2 - 45, 2, kDimWhite);
    c.txt("LMB/Space - Shoot    1/2/3 - Weapons", c.w / 2, c.h / 2 - 20, 2, kDimWhite);
    c.txt("E - Open Doors    TAB - Minimap", c.w / 2, c.h / 2 + 5, 2, kDimWhite);
    c.fill(c.w / 2 - 160, c.h / 2 + 30, 320, 2, kPanelEdge);
    c.txt("[ENTER] START GAME", c.w / 2, c.h / 2 + 60, 3, kCyan);
    c.txt("[L] LEADERBOARD   [S] SETTINGS", c.w / 2, c.h / 2 + 95, 2, kOffWhite);
    char bestBuf[48];
    std::snprintf(bestBuf, sizeof(bestBuf), "Best Level: %d", world.bestLevel);
    c.txt(bestBuf, c.w / 2, c.h / 2 + 130, 2, kDimWhite);
}

void renderDifficultySelect(const Canvas& c) {
    c.gradient(packRGBA(10, 14, 30), packRGBA(28, 34, 65));
    c.outlinedRect(c.w / 2 - 260, c.h / 2 - 140, 520, 280, kPanelBg, kPanelEdge, 2);
    c.txt("SELECT DIFFICULTY", c.w / 2, c.h / 2 - 110, 4, kGold);
    c.fill(c.w / 2 - 200, c.h / 2 - 78, 400, 2, kPanelEdge);
    c.outlinedRect(c.w / 2 - 200, c.h / 2 - 60, 400, 50, packRGBA(30, 50, 30), packRGBA(80, 220, 115), 2);
    c.txt("[1] EASY", c.w / 2 - 50, c.h / 2 - 35, 3, kGreen);
    c.txtL("0.7x HP  0.5x Score", c.w / 2 + 40, c.h / 2 - 40, 2, kDimWhite);
    c.outlinedRect(c.w / 2 - 200, c.h / 2 + 0, 400, 50, packRGBA(35, 35, 50), packRGBA(88, 140, 255), 2);
    c.txt("[2] NORMAL", c.w / 2 - 40, c.h / 2 + 25, 3, kCyan);
    c.txtL("Standard", c.w / 2 + 55, c.h / 2 + 20, 2, kDimWhite);
    c.outlinedRect(c.w / 2 - 200, c.h / 2 + 60, 400, 50, packRGBA(50, 30, 30), packRGBA(255, 90, 90), 2);
    c.txt("[3] HARD", c.w / 2 - 50, c.h / 2 + 85, 3, kRed);
    c.txtL("1.4x HP  1.5x Score", c.w / 2 + 40, c.h / 2 + 80, 2, kDimWhite);
    c.txt("[ESC] Back", c.w / 2, c.h / 2 + 125, 2, kDimWhite);
}

void renderLeaderboard(const Canvas& c, const World& world) {
    c.gradient(packRGBA(10, 14, 30), packRGBA(28, 34, 65));
    c.outlinedRect(c.w / 2 - 320, c.h / 2 - 200, 640, 400, kPanelBg, kPanelEdge, 2);
    c.txt("LEADERBOARD", c.w / 2, c.h / 2 - 170, 4, kGold);
    c.fill(c.w / 2 - 280, c.h / 2 - 142, 560, 2, kPanelEdge);
    c.txt("Rank  Score   Level Diff    Time", c.w / 2, c.h / 2 - 125, 2, kCyan);
    for (int i = 0; i < world.leaderboardCount && i < kMaxLeaderboard; ++i) {
        const auto& e = world.leaderboard[i];
        const char* dname = getDifficultySettings(e.difficulty).name;
        int mins = static_cast<int>(e.timeSeconds) / 60;
        int secs = static_cast<int>(e.timeSeconds) % 60;
        char line[80];
        std::snprintf(line, sizeof(line), " %2d   %5d    %2d   %-6s  %d:%02d", i + 1, e.score, e.wave, dname, mins, secs);
        c.txt(line, c.w / 2, c.h / 2 - 105 + i * 22, 2, (i == 0) ? kGold : kOffWhite);
    }
    if (world.leaderboardCount == 0)
        c.txt("No entries yet. Go play!", c.w / 2, c.h / 2, 2, kDimWhite);
    c.txt("[ESC] Back", c.w / 2, c.h / 2 + 170, 2, kDimWhite);
}

void renderGameOver(const Canvas& c, const World& world) {
    c.gradient(packRGBA(18, 8, 8), packRGBA(30, 16, 18));
    c.outlinedRect(c.w / 2 - 280, c.h / 2 - 150, 560, 300, packRGBA(25, 12, 12), packRGBA(160, 50, 50), 2);
    c.txt("GAME OVER", c.w / 2, c.h / 2 - 110, 5, kRed);
    c.fill(c.w / 2 - 180, c.h / 2 - 72, 360, 2, packRGBA(120, 40, 40));
    char fs[48]; std::snprintf(fs, sizeof(fs), "Final Score: %d", world.score);
    c.txt(fs, c.w / 2, c.h / 2 - 45, 3, kGold);
    char fw[48]; std::snprintf(fw, sizeof(fw), "Reached Level %d", world.level);
    c.txt(fw, c.w / 2, c.h / 2 - 10, 2, kOffWhite);
    char pl[48]; std::snprintf(pl, sizeof(pl), "Player Lv: %d", world.player.playerLevel);
    c.txt(pl, c.w / 2, c.h / 2 + 18, 2, packRGBA(180, 120, 255));
    char bw[48]; std::snprintf(bw, sizeof(bw), "Best Level: %d", std::max(world.bestLevel, world.level));
    c.txt(bw, c.w / 2, c.h / 2 + 42, 2, kDimWhite);
    int mins = static_cast<int>(world.elapsedRunSeconds) / 60, secs = static_cast<int>(world.elapsedRunSeconds) % 60;
    char tb[48]; std::snprintf(tb, sizeof(tb), "Time: %d:%02d   [%s]", mins, secs, getDifficultySettings(world.difficulty).name);
    c.txt(tb, c.w / 2, c.h / 2 + 46, 2, kDimWhite);
    c.fill(c.w / 2 - 180, c.h / 2 + 68, 360, 2, packRGBA(120, 40, 40));
    c.txt("[ENTER] Play Again", c.w / 2, c.h / 2 + 95, 3, kCyan);
    c.txt("[ESC] Quit", c.w / 2, c.h / 2 + 125, 2, kDimWhite);
}

void renderSettings(const Canvas& c, const World& world) {
    c.gradient(packRGBA(10, 14, 30), packRGBA(28, 34, 65));
    c.outlinedRect(c.w / 2 - 300, c.h / 2 - 160, 600, 320, kPanelBg, kPanelEdge, 2);
    c.txt("SETTINGS", c.w / 2, c.h / 2 - 130, 4, kGold);
    c.fill(c.w / 2 - 260, c.h / 2 - 100, 520, 2, kPanelEdge);

    char volBuf[48];
    int volPct = static_cast<int>(world.settings.masterVolume * 100.0F + 0.5F);
    std::snprintf(volBuf, sizeof(volBuf), "Master Volume: %d%%", volPct);
    c.txt(volBuf, c.w / 2, c.h / 2 - 70, 3, kOffWhite);
    c.txt("[1] +   [2] -", c.w / 2, c.h / 2 - 45, 2, kDimWhite);

    char sensBuf[48];
    std::snprintf(sensBuf, sizeof(sensBuf), "Mouse Sensitivity: %.3f", static_cast<double>(world.settings.mouseSensitivity));
    c.txt(sensBuf, c.w / 2, c.h / 2 - 10, 3, kOffWhite);
    c.txt("[3] +   [4] -", c.w / 2, c.h / 2 + 15, 2, kDimWhite);

    char resBuf[48];
    int rw = GameConstants::kResolutions[world.settings.resolutionIndex][0];
    int rh = GameConstants::kResolutions[world.settings.resolutionIndex][1];
    std::snprintf(resBuf, sizeof(resBuf), "Resolution: %dx%d", rw, rh);
    c.txt(resBuf, c.w / 2, c.h / 2 + 50, 3, kOffWhite);
    c.txt("[5] Cycle", c.w / 2, c.h / 2 + 75, 2, kDimWhite);

    c.txt("[ESC] Back", c.w / 2, c.h / 2 + 120, 2, kDimWhite);
}

void renderShop(const Canvas& c, const World& world) {
    c.gradient(packRGBA(10, 18, 14), packRGBA(18, 34, 28));
    c.outlinedRect(c.w / 2 - 320, c.h / 2 - 200, 640, 400, kPanelBg, kPanelEdge, 2);

    char titleBuf[48];
    std::snprintf(titleBuf, sizeof(titleBuf), "LEVEL %d CLEARED!", world.level);
    c.txt(titleBuf, c.w / 2, c.h / 2 - 175, 4, kGold);

    char scoreBuf[48];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", world.score);
    c.txt(scoreBuf, c.w / 2, c.h / 2 - 145, 2, kGold);
    c.fill(c.w / 2 - 280, c.h / 2 - 130, 560, 2, kPanelEdge);
    c.txt("SHOP - Use W/S to browse, ENTER to buy", c.w / 2, c.h / 2 - 115, 2, kDimWhite);

    int numItems = static_cast<int>(world.shopItems.size());
    int itemH = 38;
    int listTop = c.h / 2 - 90;

    for (int i = 0; i < numItems; ++i) {
        const auto& item = world.shopItems[i];
        int iy = listTop + i * itemH;
        bool selected = (i == world.shopCursor);
        bool canAfford = (world.score >= item.cost);

        uint32_t bg = selected ? packRGBA(40, 60, 90) : packRGBA(25, 30, 42);
        uint32_t border = selected ? kCyan : packRGBA(50, 55, 70);
        c.outlinedRect(c.w / 2 - 260, iy, 520, itemH - 4, bg, border, 1);

        uint32_t nameCol = canAfford ? kOffWhite : packRGBA(100, 100, 110);
        c.txtL(item.name.c_str(), c.w / 2 - 245, iy + 6, 2, nameCol);
        c.txtL(item.description.c_str(), c.w / 2 - 245, iy + 22, 2, kDimWhite);

        char costBuf[24];
        std::snprintf(costBuf, sizeof(costBuf), "$%d", item.cost);
        int costW = BitmapFont::textPixelWidth(costBuf, 2);
        c.txtL(costBuf, c.w / 2 + 240 - costW, iy + 12, 2, canAfford ? kGold : kRed);

        if (selected) c.txtL(">", c.w / 2 - 270, iy + 10, 2, kCyan);
    }

    if (numItems == 0) c.txt("Shop is empty!", c.w / 2, c.h / 2, 2, kDimWhite);

    c.txt("[N/ESC] Next Level", c.w / 2, c.h / 2 + 160, 2, kCyan);
}

void renderRaycasterScene(const Canvas& c, const World& world) {
    buildDecalGrid(world);
    constexpr float kFov = GameConstants::kFovRadians;
    float shakeOfsX = 0.0F, shakeOfsY = 0.0F;
    if (world.shakeTimer > 0.0F) {
        shakeOfsX = randomFloat(-world.shakeIntensity, world.shakeIntensity) * 0.01F;
        shakeOfsY = randomFloat(-world.shakeIntensity, world.shakeIntensity) * 0.01F;
    }

    float dirX = std::cos(world.player.facingRadians), dirY = std::sin(world.player.facingRadians);
    float planeScale = std::tan(kFov * 0.5F);
    float planeX = -dirY * planeScale, planeY = dirX * planeScale;
    float posX = world.player.position.x + shakeOfsX;
    float posY = world.player.position.y + shakeOfsY;
    const int w = c.w, h = c.h;

    gZBuffer.assign(w, 1e9F);
    for (int x = 0; x < w; ++x) {
        float cameraX = (2.0F * static_cast<float>(x) / static_cast<float>(w)) - 1.0F;
        float rayDirX = dirX + planeX * cameraX, rayDirY = dirY + planeY * cameraX;
        int mapX = static_cast<int>(std::floor(posX)), mapY = static_cast<int>(std::floor(posY));
        float deltaDistX = (rayDirX == 0.0F) ? 1e30F : std::fabs(1.0F / rayDirX);
        float deltaDistY = (rayDirY == 0.0F) ? 1e30F : std::fabs(1.0F / rayDirY);
        int stepX, stepY; float sideDistX, sideDistY;
        if (rayDirX < 0.0F) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; }
        else { stepX = 1; sideDistX = (mapX + 1.0F - posX) * deltaDistX; }
        if (rayDirY < 0.0F) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; }
        else { stepY = 1; sideDistY = (mapY + 1.0F - posY) * deltaDistY; }
        bool hit = false; int side = 0;
        char hitCell = '#';
        while (!hit) {
            if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
            else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
            char cell = Map::cellAt(world, mapX, mapY);
            if (cell == '#' || cell == 'D') { hit = true; hitCell = cell; }
            if (mapX < 0 || mapY < 0 || mapX >= world.mapWidth || mapY >= world.mapHeight) { hit = true; hitCell = '#'; }
        }
        float perpWallDist = std::max(0.001F, (side == 0) ? sideDistX - deltaDistX : sideDistY - deltaDistY);
        gZBuffer[x] = perpWallDist;
        int lineHeight = static_cast<int>(static_cast<float>(h) / perpWallDist);
        int drawStart = (h - lineHeight) / 2, drawEnd = drawStart + lineHeight;
        float wallHitX = (side == 0) ? posY + perpWallDist * rayDirY : posX + perpWallDist * rayDirX;
        wallHitX -= std::floor(wallHitX);

        const RenderUtils::Texture* tex = nullptr;
        if (hitCell == 'D') {
            tex = &RenderUtils::doorTex();
        } else {
            tex = (side == 0) ? &RenderUtils::wallTex1() : &RenderUtils::wallTex2();
        }

        float fog = std::max(0.12F, 1.0F - perpWallDist * 0.08F) * ((side == 1) ? 0.75F : 1.0F);
        int cs = std::max(0, drawStart), ce = std::min(h - 1, drawEnd);
        for (int y = cs; y <= ce; ++y) {
            float v = static_cast<float>(y - drawStart) / static_cast<float>(lineHeight);
            c.px[y * c.pp + x] = dimColor(sampleTexture(*tex, wallHitX, v), fog);
        }

        for (int y = ce + 1; y < h; ++y) {
            float rowDist = static_cast<float>(h) / (2.0F * y - static_cast<float>(h));
            float floorX = posX + rowDist * rayDirX;
            float floorY = posY + rowDist * rayDirY;
            uint32_t floorCol = dimColor(sampleTexture(RenderUtils::floorTex(), floorX, floorY),
                                          std::max(0.08F, 1.0F - rowDist * 0.09F));
            int fcx = static_cast<int>(std::floor(floorX));
            int fcy = static_cast<int>(std::floor(floorY));
            if (fcx >= 0 && fcx < gDecalGridW && fcy >= 0 && fcy < gDecalGridH) {
                const auto& dc = gDecalGrid[fcy * gDecalGridW + fcx];
                if (dc.active) floorCol = blendColor(floorCol, dc.color, dc.alpha * 0.5F);
            }
            c.px[y * c.pp + x] = floorCol;
        }
        for (int y = 0; y < cs; ++y) {
            float rowDist = static_cast<float>(h) / (static_cast<float>(h) - 2.0F * y);
            c.px[y * c.pp + x] = dimColor(sampleTexture(RenderUtils::ceilTex(), posX + rowDist * rayDirX, posY + rowDist * rayDirY),
                                           std::max(0.06F, 1.0F - rowDist * 0.1F));
        }
    }

    // Exit marker
    {
        Vec2 exitPos {world.exitX + 0.5F, world.exitY + 0.5F};
        Vec2 rel {exitPos.x - posX, exitPos.y - posY};
        float dist = vecLength(rel);
        float angle = wrapAngle(std::atan2(rel.y, rel.x) - world.player.facingRadians);
        if (std::fabs(angle) < kFov * 0.66F && dist > 0.2F && dist < gZBuffer[std::min(w - 1, std::max(0, static_cast<int>(((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w)))]) {
            float screenXf = ((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w;
            float pSize = std::max(4.0F, (h / std::max(0.1F, dist)) * 0.2F);
            int sx = static_cast<int>(screenXf), sy = static_cast<int>(h * 0.5F);
            int psz = static_cast<int>(pSize);
            for (int dy = -psz; dy <= psz; ++dy) for (int dx = -psz; dx <= psz; ++dx) {
                if (std::abs(dx) + std::abs(dy) <= psz) {
                    int fx = sx + dx, fy = sy + dy;
                    if (fx >= 0 && fx < w && fy >= 0 && fy < h)
                        c.px[fy * c.pp + fx] = dimColor(kExitGreen, std::max(0.3F, 1.0F - dist * 0.05F));
                }
            }
        }
    }

    // Pickup sprites
    for (const auto& pk : world.pickups) {
        Vec2 rel {pk.position.x - posX, pk.position.y - posY};
        float dist = vecLength(rel);
        float angle = wrapAngle(std::atan2(rel.y, rel.x) - world.player.facingRadians);
        if (std::fabs(angle) > kFov * 0.66F || dist < 0.2F) continue;
        float screenXf = ((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w;
        float pSize = std::max(3.0F, (h / std::max(0.1F, dist)) * 0.12F);
        float bobOff = std::sin(pk.bobPhase) * pSize * 0.3F;
        uint32_t pc = packRGBA(80, 220, 115);
        switch (pk.type) {
            case PickupType::DamageBoost: pc = packRGBA(255, 90, 90); break;
            case PickupType::SpeedBoost:  pc = packRGBA(255, 210, 80); break;
            case PickupType::Shield:      pc = packRGBA(88, 200, 255); break;
            case PickupType::Key:         pc = kKeyYellow; break;
            default: break;
        }
        int sx = static_cast<int>(screenXf), sy = static_cast<int>(h * 0.65F + bobOff);
        int psz = static_cast<int>(pSize);
        if (sx >= 0 && sx < w && dist < gZBuffer[std::min(w - 1, std::max(0, sx))]) {
            for (int dy = -psz; dy <= psz; ++dy) for (int dx = -psz; dx <= psz; ++dx) {
                if (std::abs(dx) + std::abs(dy) <= psz) {
                    int fx = sx + dx, fy = sy + dy;
                    if (fx >= 0 && fx < w && fy >= 0 && fy < h)
                        c.px[fy * c.pp + fx] = dimColor(pc, std::max(0.2F, 1.0F - dist * 0.07F));
                }
            }
        }
    }

    // Enemy projectile sprites
    for (const auto& proj : world.projectiles) {
        if (proj.fromPlayer) continue;
        Vec2 rel {proj.position.x - posX, proj.position.y - posY};
        float dist = vecLength(rel);
        float angle = wrapAngle(std::atan2(rel.y, rel.x) - world.player.facingRadians);
        if (std::fabs(angle) > kFov * 0.66F || dist < 0.1F) continue;
        float screenXf = ((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w;
        float projSize = std::max(2.0F, (h / std::max(0.1F, dist)) * 0.06F);
        int sx = static_cast<int>(screenXf), sy = h / 2;
        int psz = static_cast<int>(projSize);
        uint32_t projColor = packRGBA(255, 100, 50);
        if (sx >= 0 && sx < w && dist < gZBuffer[std::min(w - 1, std::max(0, sx))]) {
            for (int dy = -psz; dy <= psz; ++dy) for (int dx = -psz; dx <= psz; ++dx) {
                int fx = sx + dx, fy = sy + dy;
                if (fx >= 0 && fx < w && fy >= 0 && fy < h)
                    c.px[fy * c.pp + fx] = dimColor(projColor, std::max(0.3F, 1.0F - dist * 0.08F));
            }
        }
    }

    // Enemy sprites + HP bars
    gSprites.clear();
    for (const auto& e : world.enemies) {
        Vec2 rel {e.position.x - posX, e.position.y - posY};
        float dist = vecLength(rel);
        float angle = wrapAngle(std::atan2(rel.y, rel.x) - world.player.facingRadians);
        if (std::fabs(angle) > kFov * 0.66F) continue;
        float screenXf = ((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w;
        float spriteHalf = std::max(4.0F, (h / std::max(0.1F, dist)) * 0.35F * e.spriteScale);
        gSprites.push_back({dist, screenXf, spriteHalf, e.type, e.hp, e.maxHp});
    }
    std::sort(gSprites.begin(), gSprites.end(), [](const EnemySprite& a, const EnemySprite& b) { return a.distance > b.distance; });
    for (const auto& s : gSprites) {
        uint32_t sc = enemyColor(s.type);
        float sf = std::max(0.2F, 1.0F - s.distance * 0.07F);
        uint32_t dimmed = dimColor(sc, sf);
        int startX = std::max(0, static_cast<int>(s.screenX - s.halfSize));
        int endX = std::min(w - 1, static_cast<int>(s.screenX + s.halfSize));
        int startY = std::max(0, static_cast<int>(h * 0.5F - s.halfSize));
        int endY = std::min(h - 1, static_cast<int>(h * 0.5F + s.halfSize));
        float sw2 = s.halfSize * 2.0F, sh2 = s.halfSize * 2.0F;
        for (int sx = startX; sx <= endX; ++sx) {
            if (s.distance >= gZBuffer[sx]) continue;
            float u = (sx - (s.screenX - s.halfSize)) / sw2;
            for (int sy = startY; sy <= endY; ++sy) {
                float v = (sy - (h * 0.5F - s.halfSize)) / sh2;
                float dx = u - 0.5F, dy = v - 0.5F;
                if (dx * dx + dy * dy > 0.2F) continue;
                c.px[sy * c.pp + sx] = dimmed;
            }
        }
        if (s.hp < s.maxHp) {
            int barW = static_cast<int>(s.halfSize * 2.0F);
            int barH = 3;
            int barX = static_cast<int>(s.screenX) - barW / 2;
            int barY = startY - 6;
            if (barY >= 0) {
                c.fill(barX, barY, barW, barH, packRGBA(40, 40, 40));
                float hpFrac = static_cast<float>(s.hp) / std::max(1.0F, static_cast<float>(s.maxHp));
                int fillW = std::max(1, static_cast<int>(barW * hpFrac));
                uint32_t hpColor = lerpColor(kRed, kGreen, hpFrac);
                c.fill(barX, barY, fillW, barH, dimColor(hpColor, sf));
            }
        }
    }

    // World-space particles
    for (const auto& p : world.particles) {
        if (!p.worldSpace) continue;
        Vec2 rel {p.position.x - posX, p.position.y - posY};
        float dist = vecLength(rel);
        float angle = wrapAngle(std::atan2(rel.y, rel.x) - world.player.facingRadians);
        if (std::fabs(angle) > kFov * 0.66F || dist < 0.1F) continue;
        float screenXf = ((angle / (kFov * 0.5F)) * 0.5F + 0.5F) * w;
        float alpha = p.lifetime / p.maxLifetime;
        int psz = std::max(1, static_cast<int>(p.size * (h / std::max(0.1F, dist)) * 0.02F));
        int sx = static_cast<int>(screenXf), sy = h / 2;
        uint32_t pc = dimColor(p.color, alpha * std::max(0.2F, 1.0F - dist * 0.07F));
        for (int dy = -psz; dy <= psz; ++dy) for (int dx = -psz; dx <= psz; ++dx) {
            int fx = sx + dx, fy = sy + dy;
            if (fx >= 0 && fx < w && fy >= 0 && fy < h) c.px[fy * c.pp + fx] = pc;
        }
    }
}

void renderHUD(const Canvas& c, const World& world) {
    const int w = c.w, h = c.h;

    c.fillAlpha(12, 12, 280, 108, packRGBA(0, 0, 0), 0.55F);
    c.fill(12, 12, 280, 2, kPanelEdge); c.fill(12, 12, 2, 108, kPanelEdge);
    c.fill(12, 118, 280, 2, kPanelEdge); c.fill(290, 12, 2, 108, kPanelEdge);

    char hpBuf[32];
    std::snprintf(hpBuf, sizeof(hpBuf), "HP %d/%d", world.player.hp, world.player.maxHp);
    c.txtL(hpBuf, 22, 20, 2, kGreen);
    c.fill(22, 38, 260, 14, packRGBA(40, 40, 48));
    int hpBarW = static_cast<int>(256.0F * static_cast<float>(world.player.hp) / std::max(1.0F, static_cast<float>(world.player.maxHp)));
    uint32_t hpCol = kGreen;
    if (world.player.hp * 3 < world.player.maxHp) hpCol = kRed;
    else if (world.player.hp * 2 < world.player.maxHp) hpCol = kGold;
    c.fill(24, 40, std::max(0, hpBarW), 10, hpCol);

    // XP bar
    {
        int xpNeeded = (world.player.playerLevel >= kPlayerLevelCap)
            ? 1 : (20 + world.player.playerLevel * 15);
        float xpFrac = (world.player.playerLevel >= kPlayerLevelCap)
            ? 1.0F : static_cast<float>(world.player.xp) / static_cast<float>(xpNeeded);
        char xpBuf[48];
        std::snprintf(xpBuf, sizeof(xpBuf), "Lv %d  XP %d/%d", world.player.playerLevel, world.player.xp, xpNeeded);
        if (world.player.playerLevel >= kPlayerLevelCap)
            std::snprintf(xpBuf, sizeof(xpBuf), "Lv %d  MAX", world.player.playerLevel);
        c.txtL(xpBuf, 22, 56, 2, packRGBA(180, 120, 255));
        c.fill(22, 72, 260, 10, packRGBA(30, 20, 50));
        int xpBarW = static_cast<int>(256.0F * xpFrac);
        c.fill(24, 73, std::max(0, xpBarW), 8, packRGBA(140, 80, 220));
    }

    char levelBuf[48];
    std::snprintf(levelBuf, sizeof(levelBuf), "Level %d", world.level);
    c.txtL(levelBuf, 22, 84, 2, kCyan);

    char keyBuf[32];
    std::snprintf(keyBuf, sizeof(keyBuf), "Keys: %d", world.player.keys);
    c.txtL(keyBuf, 22, 100, 2, kKeyYellow);

    const WeaponSpec curSpec = getWeaponSpec(world.player.currentWeapon);
    char scoreBuf[32];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", world.score);
    int scoreTextW = BitmapFont::textPixelWidth(scoreBuf, 2);
    c.fillAlpha(w - scoreTextW - 28, 12, scoreTextW + 20, 22, packRGBA(0, 0, 0), 0.55F);
    c.txtL(scoreBuf, w - scoreTextW - 18, 16, 2, kGold);

    char wpnBuf[32];
    std::snprintf(wpnBuf, sizeof(wpnBuf), "[%s]", curSpec.name);
    int wpnW = BitmapFont::textPixelWidth(wpnBuf, 2);
    c.fillAlpha(w - wpnW - 28, 40, wpnW + 20, 22, packRGBA(0, 0, 0), 0.55F);
    c.txtL(wpnBuf, w - wpnW - 18, 44, 2, kOffWhite);

    const char* diffName = getDifficultySettings(world.difficulty).name;
    int diffW = BitmapFont::textPixelWidth(diffName, 2);
    c.fillAlpha(w - diffW - 28, 68, diffW + 20, 18, packRGBA(0, 0, 0), 0.55F);
    c.txtL(diffName, w - diffW - 18, 70, 2, kDimWhite);

    int buffY = 126;
    if (world.player.damageBoostTimer > 0.0F) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "DMG+  %.0fs", world.player.damageBoostTimer);
        c.txtL(buf, 22, buffY, 2, kRed); buffY += 18;
    }
    if (world.player.speedBoostTimer > 0.0F) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "SPD+  %.0fs", world.player.speedBoostTimer);
        c.txtL(buf, 22, buffY, 2, kGold); buffY += 18;
    }
    if (world.player.shieldTimer > 0.0F) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "SHIELD %.0fs", world.player.shieldTimer);
        c.txtL(buf, 22, buffY, 2, kCyan);
    }

    const char* weaponNames[] = {"1:Pistol", "2:Shotgun", "3:Rapid"};
    int slotW2 = 110, slotH2 = 24, slotY = h - 40;
    int totalSlotsW = world.player.unlockedWeapons * slotW2 + (world.player.unlockedWeapons - 1) * 6;
    int slotStartX = w / 2 - totalSlotsW / 2;
    for (int i = 0; i < world.player.unlockedWeapons; ++i) {
        bool active = (i == 0 && world.player.currentWeapon == WeaponType::Pistol) ||
                      (i == 1 && world.player.currentWeapon == WeaponType::Shotgun) ||
                      (i == 2 && world.player.currentWeapon == WeaponType::Rapid);
        int sx = slotStartX + i * (slotW2 + 6);
        c.outlinedRect(sx, slotY, slotW2, slotH2,
            active ? packRGBA(50, 80, 140) : packRGBA(25, 28, 38),
            active ? kCyan : packRGBA(60, 65, 80), 1);
        c.txt(weaponNames[i], sx + slotW2 / 2, slotY + slotH2 / 2, 2, active ? kWhite : kDimWhite);
    }

    // Stat bars panel (left side, below buffs)
    {
        const char* statNames[] = {"HP+", "REG", "DMG", "ROF", "SPD", "RNG", "ARM"};
        const char* statKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7"};
        const uint32_t statColors[] = {
            packRGBA(80, 220, 80), packRGBA(255, 180, 200), packRGBA(255, 80, 80),
            packRGBA(255, 200, 60), packRGBA(80, 180, 255), packRGBA(200, 140, 255),
            packRGBA(160, 160, 180)
        };
        int panelX = 12, panelY = buffY + 6;
        int rowH = 16;
        int panelW = 180, panelH = kNumStats * rowH + 6;
        bool hasPoints = world.player.statPoints > 0;

        c.fillAlpha(panelX, panelY, panelW, panelH, packRGBA(0, 0, 0), 0.50F);
        if (hasPoints) {
            uint32_t pulseCol = packRGBA(255, 255, 100);
            c.fill(panelX, panelY, panelW, 1, pulseCol);
            c.fill(panelX, panelY + panelH - 1, panelW, 1, pulseCol);
            c.fill(panelX, panelY, 1, panelH, pulseCol);
            c.fill(panelX + panelW - 1, panelY, 1, panelH, pulseCol);
            char ptsBuf[32];
            std::snprintf(ptsBuf, sizeof(ptsBuf), "+%d pts", world.player.statPoints);
            c.txtL(ptsBuf, panelX + panelW + 6, panelY + 2, 2, packRGBA(255, 255, 100));
        }

        for (int s = 0; s < kNumStats; ++s) {
            int ry = panelY + 3 + s * rowH;
            c.txtL(statKeys[s], panelX + 4, ry + 1, 1, kDimWhite);
            c.txtL(statNames[s], panelX + 22, ry + 1, 1, statColors[s]);
            int barX = panelX + 52;
            for (int b = 0; b < kMaxStatLevel; ++b) {
                uint32_t col = (b < world.player.stats[s]) ? statColors[s] : packRGBA(40, 40, 48);
                c.fill(barX + b * 14, ry, 12, 12, col);
                c.fill(barX + b * 14, ry, 12, 1, packRGBA(80, 80, 90));
            }
        }
    }

    // "LEVEL UP!" flash
    if (world.player.levelUpFlash > 0.0F) {
        float alpha = std::min(1.0F, world.player.levelUpFlash);
        uint8_t r = static_cast<uint8_t>(255.0F * alpha);
        uint8_t g = static_cast<uint8_t>(255.0F * alpha);
        uint8_t b = static_cast<uint8_t>(100.0F * alpha);
        c.txt("LEVEL UP!", w / 2, h / 2 - 60, 3, packRGBA(r, g, b));
    }

    // Crosshair with weapon bob
    float bobX = 0.0F, bobY = 0.0F;
    float vel2 = vecLength(world.player.velocity);
    if (vel2 > 0.1F) {
        bobX = std::sin(world.player.weaponBobPhase) * 4.0F;
        bobY = std::fabs(std::sin(world.player.weaponBobPhase * 2.0F)) * 3.0F;
    }
    float recoilY = world.player.recoilTimer * 40.0F;
    int chCX = w / 2 + static_cast<int>(bobX), chCY = h / 2 + static_cast<int>(bobY - recoilY);
    int chLen = 6, chGap = 3;
    for (int cx2 = chCX - chLen - chGap; cx2 <= chCX - chGap; ++cx2)
        if (cx2 >= 0 && cx2 < w) c.px[std::max(0, std::min(h - 1, chCY)) * c.pp + cx2] = kWhite;
    for (int cx2 = chCX + chGap; cx2 <= chCX + chLen + chGap; ++cx2)
        if (cx2 >= 0 && cx2 < w) c.px[std::max(0, std::min(h - 1, chCY)) * c.pp + cx2] = kWhite;
    for (int cy2 = chCY - chLen - chGap; cy2 <= chCY - chGap; ++cy2)
        if (cy2 >= 0 && cy2 < h) c.px[cy2 * c.pp + std::max(0, std::min(w - 1, chCX))] = kWhite;
    for (int cy2 = chCY + chGap; cy2 <= chCY + chLen + chGap; ++cy2)
        if (cy2 >= 0 && cy2 < h) c.px[cy2 * c.pp + std::max(0, std::min(w - 1, chCX))] = kWhite;

    // Screen-space particles
    for (const auto& p : world.particles) {
        if (p.worldSpace) continue;
        float alpha2 = p.lifetime / p.maxLifetime;
        int psz = static_cast<int>(p.size * alpha2);
        int sx = static_cast<int>(p.position.x), sy = static_cast<int>(p.position.y);
        uint32_t pc = dimColor(p.color, alpha2);
        for (int dy = -psz; dy <= psz; ++dy) for (int dx = -psz; dx <= psz; ++dx) {
            int fx = sx + dx, fy = sy + dy;
            if (fx >= 0 && fx < w && fy >= 0 && fy < h) c.px[fy * c.pp + fx] = pc;
        }
    }
}

void renderMinimap(const Canvas& c, const World& world) {
    const int w = c.w, h = c.h;
    float posX = world.player.position.x, posY = world.player.position.y;
    float dirX = std::cos(world.player.facingRadians), dirY = std::sin(world.player.facingRadians);

    constexpr int kMmPixels = 140;
    constexpr int kMmViewRadius = 12;
    int mmX = w - kMmPixels - 16, mmY = h - kMmPixels - 50;

    c.fillAlpha(mmX - 2, mmY - 2, kMmPixels + 4, kMmPixels + 4, packRGBA(0, 0, 0), 0.7F);
    c.fill(mmX - 2, mmY - 2, kMmPixels + 4, 1, kPanelEdge);
    c.fill(mmX - 2, mmY + kMmPixels + 2, kMmPixels + 4, 1, kPanelEdge);
    c.fill(mmX - 2, mmY - 2, 1, kMmPixels + 4, kPanelEdge);
    c.fill(mmX + kMmPixels + 2, mmY - 2, 1, kMmPixels + 4, kPanelEdge);

    float tileSize = static_cast<float>(kMmPixels) / static_cast<float>(kMmViewRadius * 2);
    int centerMmX = mmX + kMmPixels / 2;
    int centerMmY = mmY + kMmPixels / 2;

    for (int dy = -kMmViewRadius; dy <= kMmViewRadius; ++dy) {
        for (int dx = -kMmViewRadius; dx <= kMmViewRadius; ++dx) {
            int mx = static_cast<int>(std::floor(posX)) + dx;
            int my = static_cast<int>(std::floor(posY)) + dy;
            if (mx < 0 || mx >= world.mapWidth || my < 0 || my >= world.mapHeight) continue;

            bool isExplored = world.explored[my * world.mapWidth + mx];
            if (!isExplored) continue;

            float screenX = centerMmX + (mx + 0.5F - posX) * tileSize;
            float screenY = centerMmY + (my + 0.5F - posY) * tileSize;
            int sx = static_cast<int>(screenX - tileSize * 0.5F);
            int sy = static_cast<int>(screenY - tileSize * 0.5F);
            int sz = std::max(1, static_cast<int>(tileSize));

            if (sx < mmX || sx + sz > mmX + kMmPixels || sy < mmY || sy + sz > mmY + kMmPixels) continue;

            float d2 = static_cast<float>(dx * dx + dy * dy);
            bool inView = d2 <= static_cast<float>(GameConstants::kFogRevealRadius * GameConstants::kFogRevealRadius);
            float brightness = inView ? 1.0F : 0.4F;

            char cell = world.mapGrid[my * world.mapWidth + mx];
            uint32_t tileCol;
            if (cell == '#') tileCol = dimColor(packRGBA(100, 100, 110), brightness);
            else if (cell == 'D') tileCol = dimColor(kDoorBlue, brightness);
            else if (cell == 'E') tileCol = dimColor(kExitGreen, brightness);
            else tileCol = dimColor(packRGBA(30, 32, 38), brightness);
            c.fill(sx, sy, sz, sz, tileCol);
        }
    }

    for (const auto& pk : world.pickups) {
        float rx = centerMmX + (pk.position.x - posX) * tileSize;
        float ry = centerMmY + (pk.position.y - posY) * tileSize;
        int pkx = static_cast<int>(rx), pky = static_cast<int>(ry);
        if (pkx < mmX || pkx >= mmX + kMmPixels || pky < mmY || pky >= mmY + kMmPixels) continue;
        uint32_t pc2 = packRGBA(80, 220, 115);
        if (pk.type == PickupType::DamageBoost) pc2 = kRed;
        else if (pk.type == PickupType::SpeedBoost) pc2 = kGold;
        else if (pk.type == PickupType::Shield) pc2 = kCyan;
        else if (pk.type == PickupType::Key) pc2 = kKeyYellow;
        c.fill(pkx - 1, pky - 1, 3, 3, pc2);
    }

    for (const auto& e : world.enemies) {
        float dx = e.position.x - posX, dy2 = e.position.y - posY;
        if (dx * dx + dy2 * dy2 > static_cast<float>(kMmViewRadius * kMmViewRadius)) continue;
        float rx = centerMmX + dx * tileSize;
        float ry = centerMmY + dy2 * tileSize;
        int ex = static_cast<int>(rx), ey = static_cast<int>(ry);
        if (ex < mmX || ex >= mmX + kMmPixels || ey < mmY || ey >= mmY + kMmPixels) continue;
        int esz = (e.type == EnemyType::Boss) ? 3 : 2;
        c.fill(ex - esz / 2, ey - esz / 2, esz, esz, enemyColor(e.type));
    }

    c.fill(centerMmX - 2, centerMmY - 2, 4, 4, kWhite);
    int lx = centerMmX + static_cast<int>(dirX * 6);
    int ly = centerMmY + static_cast<int>(dirY * 6);
    int steps = 6;
    for (int s = 0; s <= steps; ++s) {
        int fx = centerMmX + (lx - centerMmX) * s / steps;
        int fy = centerMmY + (ly - centerMmY) * s / steps;
        if (fx >= mmX && fx < mmX + kMmPixels && fy >= mmY && fy < mmY + kMmPixels)
            c.px[fy * c.pp + fx] = kWhite;
    }
}

void renderScreenEffects(const Canvas& c, const World& world) {
    if (world.screenFlashTimer > 0.0F) {
        float alpha2 = std::min(1.0F, world.screenFlashTimer / 0.15F) * 0.35F;
        c.fillAlpha(0, 0, c.w, c.h, world.screenFlashColor, alpha2);
    }
    if (world.player.shieldTimer > 0.0F) {
        c.fillAlpha(0, 0, c.w, 3, kCyan, 0.5F);
        c.fillAlpha(0, c.h - 3, c.w, 3, kCyan, 0.5F);
        c.fillAlpha(0, 0, 3, c.h, kCyan, 0.5F);
        c.fillAlpha(c.w - 3, 0, 3, c.h, kCyan, 0.5F);
    }
}

void renderPauseOverlay(const Canvas& c, const World& world) {
    for (int y = 0; y < c.h; ++y)
        for (int x = 0; x < c.w; ++x)
            c.px[y * c.pp + x] = dimColor(c.px[y * c.pp + x], 0.35F);
    c.outlinedRect(c.w / 2 - 200, c.h / 2 - 90, 400, 180, kPanelBg, kPanelEdge, 2);
    c.txt("PAUSED", c.w / 2, c.h / 2 - 50, 5, kWhite);
    c.fill(c.w / 2 - 120, c.h / 2 - 15, 240, 2, kPanelEdge);
    c.txt("[ESC] Resume", c.w / 2, c.h / 2 + 10, 2, kOffWhite);
    c.txt("[ESC] again to Quit", c.w / 2, c.h / 2 + 35, 2, kDimWhite);
    char pauseScore[48];
    std::snprintf(pauseScore, sizeof(pauseScore), "Score: %d   Level: %d", world.score, world.level);
    c.txt(pauseScore, c.w / 2, c.h / 2 + 65, 2, kGold);
}

void renderUpgradeOverlay(const Canvas& c, const World& world) {
    for (int y = 0; y < c.h; ++y)
        for (int x = 0; x < c.w; ++x)
            c.px[y * c.pp + x] = dimColor(c.px[y * c.pp + x], 0.25F);
    c.outlinedRect(c.w / 2 - 370, c.h / 2 - 160, 740, 320, kPanelBg, kPanelEdge, 2);
    char wcBuf[48]; std::snprintf(wcBuf, sizeof(wcBuf), "LEVEL %d CLEARED!", world.level);
    c.txt(wcBuf, c.w / 2, c.h / 2 - 130, 4, kGold);
    c.txt("Choose an upgrade:", c.w / 2, c.h / 2 - 95, 2, kDimWhite);
    c.fill(c.w / 2 - 340, c.h / 2 - 78, 680, 2, kPanelEdge);
    int cardW2 = 200, cardH2 = 160, cardGap = 20, cardTop = c.h / 2 - 65;
    int numCards = static_cast<int>(world.pendingUpgrades.size());
    int totalCW = numCards * cardW2 + (numCards - 1) * cardGap;
    int cardBaseX = c.w / 2 - totalCW / 2;
    for (int i = 0; i < numCards && i < 3; ++i) {
        int cx = cardBaseX + i * (cardW2 + cardGap), cy = cardTop;
        c.outlinedRect(cx, cy, cardW2, cardH2, packRGBA(30, 42, 80), packRGBA(90, 140, 255), 2);
        char keyBuf[4]; std::snprintf(keyBuf, sizeof(keyBuf), "%d", i + 1);
        c.fill(cx + 4, cy + 4, 24, 24, packRGBA(90, 140, 255));
        c.txt(keyBuf, cx + 16, cy + 16, 3, kWhite);
        c.txt(world.pendingUpgrades[i].title.c_str(), cx + cardW2 / 2, cy + 50, 2, kWhite);
        c.fill(cx + 20, cy + 66, cardW2 - 40, 1, packRGBA(70, 100, 170));
        const std::string& desc = world.pendingUpgrades[i].description;
        if (BitmapFont::textPixelWidth(desc.c_str(), 2) > cardW2 - 20) {
            size_t mid = desc.size() / 2, split = desc.rfind(' ', mid);
            if (split == std::string::npos) split = mid;
            c.txt(desc.substr(0, split).c_str(), cx + cardW2 / 2, cy + 90, 2, kOffWhite);
            c.txt(desc.substr(split + 1).c_str(), cx + cardW2 / 2, cy + 110, 2, kOffWhite);
        } else {
            c.txt(desc.c_str(), cx + cardW2 / 2, cy + 95, 2, kOffWhite);
        }
        char hintBuf[24]; std::snprintf(hintBuf, sizeof(hintBuf), "Press [%d]", i + 1);
        c.txt(hintBuf, cx + cardW2 / 2, cy + cardH2 - 18, 2, kCyan);
    }
}

}  // namespace

void render(const World& world, GameStateId state, SDL_Renderer* renderer) {
    RenderUtils::generateTextures();

    // Internal render resolution: window / kRenderScale for 3D scenes,
    // full resolution for menus. GPU upscales via SDL_RenderCopy.
    constexpr int kScale = GameConstants::kRenderScale;
    bool is3D = (state == GameStateId::Playing || state == GameStateId::Paused ||
                 state == GameStateId::UpgradeSelection);
    const int rw = is3D ? (world.width / kScale) : world.width;
    const int rh = is3D ? (world.height / kScale) : world.height;

    if (gFramebuffer == nullptr || gFbWidth != rw || gFbHeight != rh) {
        if (gFramebuffer != nullptr) SDL_DestroyTexture(gFramebuffer);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        gFramebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STREAMING, rw, rh);
        if (gFramebuffer == nullptr) {
            std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
            return;
        }
        gFbWidth = rw;
        gFbHeight = rh;
    }

    uint32_t* px = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(gFramebuffer, nullptr, reinterpret_cast<void**>(&px), &pitch) != 0) {
        std::fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return;
    }
    const int pp = pitch / static_cast<int>(sizeof(uint32_t));
    for (int i = 0; i < pp * rh; ++i) px[i] = packRGBA(14, 18, 25);

    Canvas canvas {px, pp, rw, rh};

    switch (state) {
        case GameStateId::MainMenu:
            renderMainMenu(canvas, world);
            break;
        case GameStateId::DifficultySelect:
            renderDifficultySelect(canvas);
            break;
        case GameStateId::Leaderboard:
            renderLeaderboard(canvas, world);
            break;
        case GameStateId::GameOver:
            renderGameOver(canvas, world);
            break;
        case GameStateId::Settings:
            renderSettings(canvas, world);
            break;
        case GameStateId::Shop:
            renderShop(canvas, world);
            break;
        case GameStateId::Playing:
        case GameStateId::Paused:
        case GameStateId::UpgradeSelection:
            renderRaycasterScene(canvas, world);
            renderHUD(canvas, world);
            if (world.showMinimap && state == GameStateId::Playing)
                renderMinimap(canvas, world);
            renderScreenEffects(canvas, world);
            if (state == GameStateId::Paused)
                renderPauseOverlay(canvas, world);
            else if (state == GameStateId::UpgradeSelection)
                renderUpgradeOverlay(canvas, world);
            break;
    }

    SDL_UnlockTexture(gFramebuffer);
    SDL_RenderCopy(renderer, gFramebuffer, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

}  // namespace RenderSystem
