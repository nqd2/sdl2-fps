#pragma once

namespace GameConstants {

constexpr float kEnemySize          = 0.34F;
constexpr float kPlayerRadius       = 0.22F;
constexpr float kPickupRadius       = 0.4F;
constexpr float kExplodeRadius      = 1.5F;
constexpr float kInvulnerabilitySeconds = 0.55F;
constexpr int   kBaseWaveKills      = 12;
constexpr float kFovRadians         = 1.047197551F;    // 60 degrees
constexpr int   kTexSize            = 64;
constexpr int   kMaxParticles       = 200;
constexpr float kEnemySpeedScale    = 0.02F;
constexpr float kPlayerBaseSpeed   = 3.5F;
constexpr float kFixedStep         = 1.0F / 120.0F;
constexpr float kPi                = 3.14159265358979F;
constexpr float kTwoPi             = 6.28318530717959F;
constexpr int   kFogRevealRadius   = 4;
constexpr float kDoorInteractDist  = 1.2F;
constexpr float kExitReachDist     = 0.6F;
constexpr float kProjectileSpeed   = 4.0F;
constexpr float kProjectileRadius  = 0.1F;

constexpr int kResolutions[][2] = {{640, 360}, {1280, 720}, {1920, 1080}};
constexpr int kNumResolutions = 3;

constexpr int kRenderScale    = 2;
constexpr int kMaxDecalGrid   = 64 * 64;

constexpr float kStatMaxHpPerPoint   = 3.0F;
constexpr float kStatRegenPerPoint   = 0.4F;
constexpr float kStatDmgMulPerPoint  = 0.12F;
constexpr float kStatFireRatePerPoint = 0.10F;
constexpr float kStatSpeedPerPoint   = 0.25F;
constexpr float kStatRangePerPoint   = 1.5F;
constexpr float kStatArmorPerPoint   = 0.08F;
constexpr float kBaseHitscanRange    = 14.0F;

}  // namespace GameConstants
