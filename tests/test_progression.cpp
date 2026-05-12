#include "data/GameData.hpp"
#include "GameConstants.hpp"
#include "systems/Map.hpp"
#include "systems/MathUtils.hpp"
#include "systems/MazeGen.hpp"
#include "systems/ParticleSystem.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

static int gPass = 0;
static int gFail = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (expr) { ++gPass; }                                                 \
        else { ++gFail; std::fprintf(stderr, "FAIL: %s  (%s:%d)\n",           \
                   #expr, __FILE__, __LINE__); }                               \
    } while (0)

#define APPROX(a, b) (std::fabs((a) - (b)) < 0.001F)

// Helper: create a World with the old 16x16 static map for backwards-compat tests
static World makeStaticMapWorld() {
    const char* rows[16] = {
        "################",
        "#......#.......#",
        "#......#.......#",
        "#..............#",
        "#..##..........#",
        "#..............#",
        "#......####....#",
        "#..............#",
        "#..............#",
        "#....##........#",
        "#..............#",
        "#...........#..#",
        "#..............#",
        "#...#..........#",
        "#..............#",
        "################",
    };
    World w {};
    w.mapWidth = 16;
    w.mapHeight = 16;
    w.mapGrid.resize(16 * 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            w.mapGrid[y * 16 + x] = rows[y][x];
    w.explored.assign(16 * 16, true);
    return w;
}

// ── DifficultySettings ──────────────────────────────────────────────────────

void testDifficultySettings() {
    auto easy = getDifficultySettings(Difficulty::Easy);
    auto norm = getDifficultySettings(Difficulty::Normal);
    auto hard = getDifficultySettings(Difficulty::Hard);

    CHECK(easy.enemyHpMul < norm.enemyHpMul);
    CHECK(norm.enemyHpMul < hard.enemyHpMul);
    CHECK(easy.scoreMul < norm.scoreMul);
    CHECK(norm.scoreMul < hard.scoreMul);
    CHECK(APPROX(norm.enemyHpMul, 1.0F));
    CHECK(APPROX(norm.enemySpeedMul, 1.0F));
    CHECK(APPROX(norm.damageTakenMul, 1.0F));
    CHECK(APPROX(norm.scoreMul, 1.0F));
    CHECK(std::strcmp(easy.name, "Easy") == 0);
    CHECK(std::strcmp(norm.name, "Normal") == 0);
    CHECK(std::strcmp(hard.name, "Hard") == 0);
}

void testIsValidDifficulty() {
    CHECK(isValidDifficulty(0));
    CHECK(isValidDifficulty(1));
    CHECK(isValidDifficulty(2));
    CHECK(!isValidDifficulty(-1));
    CHECK(!isValidDifficulty(3));
    CHECK(!isValidDifficulty(100));
}

// ── Weapons ─────────────────────────────────────────────────────────────────

void testWeaponSpecs() {
    auto pistol  = getWeaponSpec(WeaponType::Pistol);
    auto shotgun = getWeaponSpec(WeaponType::Shotgun);
    auto rapid   = getWeaponSpec(WeaponType::Rapid);

    CHECK(pistol.pellets == 1);
    CHECK(shotgun.pellets == 6);
    CHECK(rapid.pellets == 1);

    CHECK(pistol.damagePerPellet > rapid.damagePerPellet);
    CHECK(shotgun.cooldownSeconds > pistol.cooldownSeconds);
    CHECK(rapid.cooldownSeconds < pistol.cooldownSeconds);

    CHECK(pistol.spreadRadians == 0.0F);
    CHECK(shotgun.spreadRadians > 0.0F);
    CHECK(rapid.spreadRadians > 0.0F);
    CHECK(shotgun.spreadRadians > rapid.spreadRadians);

    CHECK(std::strcmp(pistol.name, "Pistol") == 0);
    CHECK(std::strcmp(shotgun.name, "Shotgun") == 0);
    CHECK(std::strcmp(rapid.name, "Rapid") == 0);
}

// ── Enemy factories ─────────────────────────────────────────────────────────

void testEnemyScaling() {
    const Vec2 spawn {5.0F, 5.0F};
    const auto diff = getDifficultySettings(Difficulty::Normal);

    auto w1 = makeEnemyForWave(EnemyType::Grunt, 1, spawn, diff);
    auto w5 = makeEnemyForWave(EnemyType::Grunt, 5, spawn, diff);
    auto w10 = makeEnemyForWave(EnemyType::Grunt, 10, spawn, diff);

    CHECK(w5.hp >= w1.hp);
    CHECK(w10.hp >= w5.hp);
    CHECK(w5.speed >= w1.speed);
    CHECK(w10.speed >= w5.speed);
    CHECK(APPROX(w1.position.x, spawn.x));
    CHECK(APPROX(w1.position.y, spawn.y));
    CHECK(w1.maxHp == w1.hp);
}

void testAllEnemyTypes() {
    const Vec2 spawn {3.0F, 3.0F};
    const auto diff = getDifficultySettings(Difficulty::Normal);

    auto grunt    = makeEnemyForWave(EnemyType::Grunt, 5, spawn, diff);
    auto fast     = makeEnemyForWave(EnemyType::Fast, 5, spawn, diff);
    auto tank     = makeEnemyForWave(EnemyType::Tank, 5, spawn, diff);
    auto shooter  = makeEnemyForWave(EnemyType::Shooter, 5, spawn, diff);
    auto boss     = makeEnemyForWave(EnemyType::Boss, 5, spawn, diff);
    auto exploder = makeEnemyForWave(EnemyType::Exploder, 5, spawn, diff);
    auto healer   = makeEnemyForWave(EnemyType::Healer, 5, spawn, diff);

    CHECK(fast.speed > grunt.speed);
    CHECK(tank.hp > grunt.hp);
    CHECK(tank.contactDamage > grunt.contactDamage);
    CHECK(boss.hp > tank.hp);
    CHECK(boss.spriteScale > 1.0F);
    CHECK(boss.scoreValue > tank.scoreValue);
    CHECK(healer.contactDamage == 0);
    CHECK(exploder.speed > grunt.speed);

    CHECK(grunt.type == EnemyType::Grunt);
    CHECK(fast.type == EnemyType::Fast);
    CHECK(tank.type == EnemyType::Tank);
    CHECK(shooter.type == EnemyType::Shooter);
    CHECK(boss.type == EnemyType::Boss);
    CHECK(exploder.type == EnemyType::Exploder);
    CHECK(healer.type == EnemyType::Healer);
}

void testDifficultyAffectsEnemies() {
    const Vec2 spawn {};
    auto easy = getDifficultySettings(Difficulty::Easy);
    auto hard = getDifficultySettings(Difficulty::Hard);

    auto easyGrunt = makeEnemyForWave(EnemyType::Grunt, 5, spawn, easy);
    auto hardGrunt = makeEnemyForWave(EnemyType::Grunt, 5, spawn, hard);

    CHECK(hardGrunt.hp > easyGrunt.hp);
    CHECK(hardGrunt.speed > easyGrunt.speed);
    CHECK(hardGrunt.scoreValue > easyGrunt.scoreValue);
}

// ── Upgrades ────────────────────────────────────────────────────────────────

void testUpgradePool() {
    auto pool = allUpgradeOptions();
    CHECK(pool.size() == 6U);

    bool foundArsenal = false;
    for (const auto& u : pool) {
        CHECK(!u.title.empty());
        CHECK(!u.description.empty());
        if (u.id == UpgradeId::FullArsenal) foundArsenal = true;
    }
    CHECK(foundArsenal);

    CHECK(pool[0].id == UpgradeId::HighCaliber);
    CHECK(pool[0].damageMultiplier > 1.0F);
    CHECK(pool[1].id == UpgradeId::HairTrigger);
    CHECK(pool[1].fireRateMultiplier > 1.0F);
    CHECK(pool[2].id == UpgradeId::KineticBoots);
    CHECK(pool[2].moveSpeedBonus > 0.0F);
    CHECK(pool[3].id == UpgradeId::ReinforcedSuit);
    CHECK(pool[3].hpBonus > 0);
}

void testPickUpgradeChoices() {
    for (int w = 1; w <= 20; ++w) {
        auto choices = pickUpgradeChoices(w);
        CHECK(choices.size() == 3U);
        CHECK(choices[0].id != choices[1].id);
        CHECK(choices[1].id != choices[2].id);
        CHECK(choices[0].id != choices[2].id);
    }
}

// ── MathUtils ───────────────────────────────────────────────────────────────

void testVecLength() {
    CHECK(APPROX(vecLength({0, 0}), 0.0F));
    CHECK(APPROX(vecLength({3, 4}), 5.0F));
    CHECK(APPROX(vecLength({1, 0}), 1.0F));
    CHECK(APPROX(vecLength({0, -1}), 1.0F));
}

void testVecNormalize() {
    Vec2 n = vecNormalize({3, 4});
    CHECK(APPROX(vecLength(n), 1.0F));
    CHECK(APPROX(n.x, 0.6F));
    CHECK(APPROX(n.y, 0.8F));

    Vec2 zero = vecNormalize({0, 0});
    CHECK(APPROX(zero.x, 0.0F));
    CHECK(APPROX(zero.y, 0.0F));
}

void testDistanceSquared() {
    CHECK(APPROX(distanceSquared({0, 0}, {3, 4}), 25.0F));
    CHECK(APPROX(distanceSquared({1, 1}, {1, 1}), 0.0F));
    CHECK(APPROX(distanceSquared({0, 0}, {1, 0}), 1.0F));
}

void testWrapAngle() {
    CHECK(APPROX(wrapAngle(0.0F), 0.0F));
    CHECK(APPROX(std::fabs(wrapAngle(GameConstants::kPi)), GameConstants::kPi));
    CHECK(APPROX(wrapAngle(-GameConstants::kPi), -GameConstants::kPi));
    CHECK(APPROX(wrapAngle(GameConstants::kTwoPi), 0.0F));
    CHECK(APPROX(wrapAngle(-GameConstants::kTwoPi), 0.0F));
    CHECK(APPROX(wrapAngle(3.0F * GameConstants::kTwoPi), 0.0F));
    CHECK(APPROX(wrapAngle(GameConstants::kPi + 0.1F), -GameConstants::kPi + 0.1F));

    float result = wrapAngle(100.0F);
    CHECK(result >= -GameConstants::kPi && result <= GameConstants::kPi);
    result = wrapAngle(-100.0F);
    CHECK(result >= -GameConstants::kPi && result <= GameConstants::kPi);
}

void testRandomFloat() {
    RNG::seed(42);
    for (int i = 0; i < 100; ++i) {
        float v = randomFloat(0.0F, 1.0F);
        CHECK(v >= 0.0F && v <= 1.0F);
    }
    for (int i = 0; i < 100; ++i) {
        float v = randomFloat(-5.0F, 5.0F);
        CHECK(v >= -5.0F && v <= 5.0F);
    }
}

void testRNGUniformInt() {
    RNG::seed(42);
    for (int i = 0; i < 200; ++i) {
        int v = RNG::uniformInt(1, 14);
        CHECK(v >= 1 && v <= 14);
    }
}

void testRNGSeedDeterminism() {
    RNG::seed(999);
    float a1 = randomFloat(0, 1);
    float a2 = randomFloat(0, 1);
    RNG::seed(999);
    float b1 = randomFloat(0, 1);
    float b2 = randomFloat(0, 1);
    CHECK(a1 == b1);
    CHECK(a2 == b2);
}

// ── Map ─────────────────────────────────────────────────────────────────────

void testIsWall() {
    World w = makeStaticMapWorld();
    CHECK(Map::isWall(w, 0.5F, 0.5F));
    CHECK(Map::isWall(w, 15.5F, 0.5F));
    CHECK(Map::isWall(w, 0.5F, 15.5F));
    CHECK(!Map::isWall(w, 1.5F, 1.5F));
    CHECK(!Map::isWall(w, 5.5F, 5.5F));

    CHECK(Map::isWall(w, -1.0F, 5.0F));
    CHECK(Map::isWall(w, 5.0F, -1.0F));
    CHECK(Map::isWall(w, 16.0F, 5.0F));
    CHECK(Map::isWall(w, 5.0F, 16.0F));
}

void testCanMoveTo() {
    World w = makeStaticMapWorld();
    CHECK(Map::canMoveTo(w, {5.5F, 5.5F}));
    CHECK(!Map::canMoveTo(w, {0.2F, 0.2F}));
}

void testRandomOpenCell() {
    World w = makeStaticMapWorld();
    RNG::seed(42);
    for (int i = 0; i < 50; ++i) {
        Vec2 cell = Map::randomOpenCell(w);
        CHECK(!Map::isWall(w, cell.x, cell.y));
        CHECK(cell.x >= 1.0F && cell.x <= 15.0F);
        CHECK(cell.y >= 1.0F && cell.y <= 15.0F);
    }
}

void testHasLineOfSight() {
    World w = makeStaticMapWorld();
    CHECK(Map::hasLineOfSight(w, {1.5F, 1.5F}, {1.5F, 1.5F}));
    CHECK(Map::hasLineOfSight(w, {1.5F, 1.5F}, {3.5F, 1.5F}));
    CHECK(!Map::hasLineOfSight(w, {1.5F, 1.5F}, {8.5F, 1.5F}));
}

// ── MazeGen ─────────────────────────────────────────────────────────────────

void testMazeGenBasic() {
    auto maze = MazeGen::generate(1, 42);

    CHECK(maze.width >= 21);
    CHECK(maze.height >= 21);
    CHECK(maze.width % 2 == 1);
    CHECK(maze.height % 2 == 1);
    CHECK(static_cast<int>(maze.grid.size()) == maze.width * maze.height);

    CHECK(maze.startX == 1);
    CHECK(maze.startY == 1);
    CHECK(maze.grid[maze.startY * maze.width + maze.startX] == '.');

    CHECK(maze.exitX > 0 && maze.exitX < maze.width - 1);
    CHECK(maze.exitY > 0 && maze.exitY < maze.height - 1);
    CHECK(maze.grid[maze.exitY * maze.width + maze.exitX] == 'E');

    // Borders must be walls
    for (int x = 0; x < maze.width; ++x) {
        CHECK(maze.grid[0 * maze.width + x] == '#');
        CHECK(maze.grid[(maze.height - 1) * maze.width + x] == '#');
    }
    for (int y = 0; y < maze.height; ++y) {
        CHECK(maze.grid[y * maze.width + 0] == '#');
        CHECK(maze.grid[y * maze.width + maze.width - 1] == '#');
    }
}

void testMazeGenSeedDeterminism() {
    auto m1 = MazeGen::generate(3, 12345);
    auto m2 = MazeGen::generate(3, 12345);

    CHECK(m1.width == m2.width);
    CHECK(m1.height == m2.height);
    CHECK(m1.exitX == m2.exitX);
    CHECK(m1.exitY == m2.exitY);
    CHECK(m1.grid == m2.grid);
}

void testMazeGenScaling() {
    auto m1 = MazeGen::generate(1, 100);
    auto m5 = MazeGen::generate(5, 100);
    auto m10 = MazeGen::generate(10, 100);

    CHECK(m5.width >= m1.width);
    CHECK(m10.width >= m5.width);
    CHECK(m10.width <= 51);
    CHECK(m10.height <= 51);
}

void testMazeGenDoors() {
    auto m = MazeGen::generate(6, 42);
    int doorCount = 0;
    for (char c : m.grid)
        if (c == 'D') ++doorCount;
    CHECK(doorCount > 0);
    CHECK(doorCount <= 5);
}

void testMazeGenPathExists() {
    auto maze = MazeGen::generate(3, 77);
    // BFS from start to verify exit is reachable (ignoring doors)
    std::vector<bool> visited(maze.width * maze.height, false);
    std::vector<std::pair<int, int>> queue;
    queue.push_back({maze.startX, maze.startY});
    visited[maze.startY * maze.width + maze.startX] = true;
    size_t head = 0;
    while (head < queue.size()) {
        auto [x, y] = queue[head++];
        const int ndx[] = {1, -1, 0, 0};
        const int ndy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            int nx = x + ndx[d], ny = y + ndy[d];
            if (nx < 0 || nx >= maze.width || ny < 0 || ny >= maze.height) continue;
            if (visited[ny * maze.width + nx]) continue;
            char c = maze.grid[ny * maze.width + nx];
            if (c == '.' || c == 'E' || c == 'D') {
                visited[ny * maze.width + nx] = true;
                queue.push_back({nx, ny});
            }
        }
    }
    CHECK(visited[maze.exitY * maze.width + maze.exitX]);
}

// ── Map with dynamic maze ───────────────────────────────────────────────────

void testMapDynamic() {
    auto maze = MazeGen::generate(2, 55);
    World w {};
    w.mapGrid = maze.grid;
    w.mapWidth = maze.width;
    w.mapHeight = maze.height;
    w.explored.assign(w.mapWidth * w.mapHeight, true);

    CHECK(Map::isWall(w, 0.5F, 0.5F));
    CHECK(!Map::isWall(w, 1.5F, 1.5F));
    CHECK(Map::canMoveTo(w, {1.5F, 1.5F}));

    Vec2 exitPos {maze.exitX + 0.5F, maze.exitY + 0.5F};
    CHECK(!Map::isWall(w, exitPos.x, exitPos.y));
}

// ── ParticleSystem ──────────────────────────────────────────────────────────

void testParticleSpawnWorld() {
    RNG::seed(42);
    World world {};
    ParticleSystem::spawnWorld(world, {5, 5}, 0xFFFFFFFF, 10, 1.0F, 0.5F);
    CHECK(world.particles.size() == 10U);
    for (const auto& p : world.particles) {
        CHECK(p.worldSpace);
        CHECK(p.lifetime > 0.0F);
        CHECK(APPROX(p.lifetime, p.maxLifetime));
        CHECK(p.color == 0xFFFFFFFF);
    }
}

void testParticleSpawnScreen() {
    RNG::seed(42);
    World world {};
    ParticleSystem::spawnScreen(world, 640, 360, 0xFF0000FF, 5, 200.0F, 0.3F);
    CHECK(world.particles.size() == 5U);
    for (const auto& p : world.particles) {
        CHECK(!p.worldSpace);
    }
}

void testParticleUpdate() {
    RNG::seed(42);
    World world {};
    ParticleSystem::spawnWorld(world, {5, 5}, 0xFFFFFFFF, 5, 1.0F, 0.2F);
    CHECK(world.particles.size() == 5U);

    ParticleSystem::update(world, 0.1F);
    CHECK(world.particles.size() == 5U);
    for (const auto& p : world.particles)
        CHECK(p.lifetime < p.maxLifetime);

    ParticleSystem::update(world, 0.15F);
    CHECK(world.particles.empty());
}

void testParticleEviction() {
    RNG::seed(42);
    World world {};
    ParticleSystem::spawnWorld(world, {5, 5}, 0xFFFFFFFF, GameConstants::kMaxParticles + 50, 1.0F, 10.0F);
    CHECK(static_cast<int>(world.particles.size()) <= GameConstants::kMaxParticles);
}

// ── World defaults ──────────────────────────────────────────────────────────

void testWorldDefaults() {
    World w {};
    CHECK(w.width == 1280);
    CHECK(w.height == 720);
    CHECK(w.score == 0);
    CHECK(w.wave == 1);
    CHECK(w.level == 1);
    CHECK(w.bestWave == 1);
    CHECK(w.bestLevel == 1);
    CHECK(w.difficulty == Difficulty::Normal);
    CHECK(w.player.hp == 10);
    CHECK(w.player.maxHp == 10);
    CHECK(w.player.currentWeapon == WeaponType::Pistol);
    CHECK(w.player.unlockedWeapons == 1);
    CHECK(w.player.keys == 0);
    CHECK(w.leaderboardCount == 0);
    CHECK(w.enemies.empty());
    CHECK(w.particles.empty());
    CHECK(w.pickups.empty());
    CHECK(w.decals.empty());
    CHECK(w.mapGrid.empty());
    CHECK(w.explored.empty());
}

void testPlayerDefaults() {
    Player p {};
    CHECK(APPROX(p.speed, 3.5F));
    CHECK(APPROX(p.baseSpeed, 3.5F));
    CHECK(APPROX(p.damageMultiplier, 1.0F));
    CHECK(APPROX(p.fireRateMultiplier, 1.0F));
    CHECK(APPROX(p.damageBoostTimer, 0.0F));
    CHECK(APPROX(p.speedBoostTimer, 0.0F));
    CHECK(APPROX(p.shieldTimer, 0.0F));
    CHECK(p.keys == 0);
    CHECK(p.xp == 0);
    CHECK(p.playerLevel == 0);
    CHECK(p.statPoints == 0);
    for (int i = 0; i < kNumStats; ++i) CHECK(p.stats[i] == 0);
    CHECK(APPROX(p.hpRegenAccum, 0.0F));
    CHECK(APPROX(p.levelUpFlash, 0.0F));
}

// ── main ────────────────────────────────────────────────────────────────────

int main() {
    testDifficultySettings();
    testIsValidDifficulty();
    testWeaponSpecs();
    testEnemyScaling();
    testAllEnemyTypes();
    testDifficultyAffectsEnemies();
    testUpgradePool();
    testPickUpgradeChoices();
    testVecLength();
    testVecNormalize();
    testDistanceSquared();
    testWrapAngle();
    testRandomFloat();
    testRNGUniformInt();
    testRNGSeedDeterminism();
    testIsWall();
    testCanMoveTo();
    testRandomOpenCell();
    testHasLineOfSight();
    testMazeGenBasic();
    testMazeGenSeedDeterminism();
    testMazeGenScaling();
    testMazeGenDoors();
    testMazeGenPathExists();
    testMapDynamic();
    testParticleSpawnWorld();
    testParticleSpawnScreen();
    testParticleUpdate();
    testParticleEviction();
    testWorldDefaults();
    testPlayerDefaults();

    std::printf("%d passed, %d failed\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}
