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
constexpr float kPlayerStartX      = 2.5F;
constexpr float kPlayerStartY      = 2.5F;
constexpr float kPlayerBaseSpeed   = 3.5F;
constexpr float kFixedStep         = 1.0F / 120.0F;
constexpr float kPi                = 3.14159265358979F;
constexpr float kTwoPi             = 6.28318530717959F;

}  // namespace GameConstants
