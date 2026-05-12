#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Vec2 {
    float x {0.0F};
    float y {0.0F};
};

enum class WeaponType {
    Pistol,
    Shotgun,
    Rapid,
};

enum class EnemyType {
    Grunt,
    Fast,
    Tank,
    Shooter,
    Boss,
    Exploder,
    Healer,
};

enum class PickupType {
    Health,
    DamageBoost,
    SpeedBoost,
    Shield,
    Key,
};

enum class Difficulty {
    Easy,
    Normal,
    Hard,
    COUNT,
};

enum class SoundId {
    ShootPistol,
    ShootShotgun,
    ShootRapid,
    EnemyHit,
    EnemyDeath,
    PlayerHurt,
    PickupCollect,
    WaveClear,
    Explosion,
    DoorOpen,
    LevelClear,
    Count,
};

enum class UpgradeId {
    HighCaliber,
    HairTrigger,
    KineticBoots,
    ReinforcedSuit,
    FullArsenal,
    Overdrive,
};

enum class StatId {
    MaxHp,
    HpRegen,
    BulletDamage,
    FireRate,
    MoveSpeed,
    BulletRange,
    BodyArmor,
};
constexpr int kNumStats = 7;
constexpr int kMaxStatLevel = 7;
constexpr int kPlayerLevelCap = 45;

struct DifficultySettings {
    float enemyHpMul {1.0F};
    float enemySpeedMul {1.0F};
    float damageTakenMul {1.0F};
    float scoreMul {1.0F};
    const char* name {"Normal"};
};

inline DifficultySettings getDifficultySettings(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:   return {0.7F, 0.8F, 0.7F, 0.5F, "Easy"};
        case Difficulty::Hard:   return {1.4F, 1.2F, 1.3F, 1.5F, "Hard"};
        case Difficulty::Normal:
        default:                 return {1.0F, 1.0F, 1.0F, 1.0F, "Normal"};
    }
}

inline bool isValidDifficulty(int value) {
    return value >= 0 && value < static_cast<int>(Difficulty::COUNT);
}

struct Projectile {
    Vec2 position {};
    Vec2 velocity {};
    float radius {4.0F};
    int damage {1};
    bool fromPlayer {true};
    float lifeRemaining {2.0F};
};

struct Enemy {
    EnemyType type {EnemyType::Grunt};
    Vec2 position {};
    Vec2 velocity {};
    int hp {2};
    int maxHp {2};
    int contactDamage {1};
    float fireCooldown {1.5F};
    float fireTimer {0.0F};
    float speed {60.0F};
    int scoreValue {10};
    float healTimer {0.0F};
    float spriteScale {1.0F};
};

struct Pickup {
    Vec2 position {};
    PickupType type {PickupType::Health};
    float lifetime {15.0F};
    float bobPhase {0.0F};
};

struct Particle {
    Vec2 position {};
    Vec2 velocity {};
    uint32_t color {0xFFFFFFFF};
    float lifetime {0.3F};
    float maxLifetime {0.3F};
    float size {2.0F};
    bool worldSpace {true};
};

struct Decal {
    int cellX {0};
    int cellY {0};
    uint32_t color {0};
    float alpha {1.0F};
};

struct UpgradeOption {
    UpgradeId id {UpgradeId::HighCaliber};
    std::string title;
    std::string description;
    float damageMultiplier {1.0F};
    float fireRateMultiplier {1.0F};
    float moveSpeedBonus {0.0F};
    int hpBonus {0};
    int cost {0};
};

struct ShopItem {
    std::string name;
    std::string description;
    int cost {0};
    enum class Type { Upgrade, Health, Key } type {Type::Upgrade};
    int upgradeIndex {-1};
};

struct LeaderboardEntry {
    int score {0};
    int wave {1};
    Difficulty difficulty {Difficulty::Normal};
    float timeSeconds {0.0F};
};

struct Settings {
    float masterVolume {0.8F};
    float mouseSensitivity {0.003F};
    int resolutionIndex {1};
};

struct Player {
    Vec2 position {};
    Vec2 velocity {};
    int hp {10};
    int maxHp {10};
    float speed {3.5F};
    float facingRadians {0.0F};
    float turnSpeed {2.2F};
    float mouseSensitivity {0.003F};
    WeaponType currentWeapon {WeaponType::Pistol};
    int unlockedWeapons {1};
    float weaponCooldown {0.0F};
    float invulnerabilityTimer {0.0F};
    float damageMultiplier {1.0F};
    float fireRateMultiplier {1.0F};
    float weaponBobPhase {0.0F};
    float recoilTimer {0.0F};
    float damageBoostTimer {0.0F};
    float speedBoostTimer {0.0F};
    float shieldTimer {0.0F};
    float baseSpeed {3.5F};
    int keys {0};
    int xp {0};
    int playerLevel {0};
    int statPoints {0};
    int stats[kNumStats] {};
    float hpRegenAccum {0.0F};
    float levelUpFlash {0.0F};
};

constexpr int kMaxLeaderboard = 10;
constexpr int kMaxDecals = 50;

struct World {
    int width {1280};
    int height {720};
    int score {0};
    int level {1};
    int wave {1};
    int killsInWave {0};
    int killsToClearWave {12};
    bool waveCleared {false};
    bool runActive {true};
    bool quitRequested {false};
    bool mouseCaptured {false};
    bool showMinimap {true};
    float elapsedRunSeconds {0.0F};
    int bestWave {1};
    int bestLevel {1};
    Difficulty difficulty {Difficulty::Normal};
    float screenFlashTimer {0.0F};
    uint32_t screenFlashColor {0};
    float shakeTimer {0.0F};
    float shakeIntensity {0.0F};
    unsigned int runSeed {42};

    std::vector<char> mapGrid;
    int mapWidth {0};
    int mapHeight {0};
    int exitX {0};
    int exitY {0};
    std::vector<bool> explored;

    Player player {};
    Settings settings {};
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<UpgradeOption> pendingUpgrades;
    std::vector<ShopItem> shopItems;
    int shopCursor {0};
    std::vector<Pickup> pickups;
    std::vector<Particle> particles;
    std::vector<Decal> decals;
    std::array<LeaderboardEntry, kMaxLeaderboard> leaderboard {};
    int leaderboardCount {0};
};
