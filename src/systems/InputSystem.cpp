#include "systems/InputSystem.hpp"
#include "systems/CombatSystem.hpp"
#include "systems/MathUtils.hpp"
#include "data/GameData.hpp"

#include <SDL.h>

namespace InputSystem {

void setMouseCapture(World& world, bool capture) {
    if (capture && !world.mouseCaptured) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        world.mouseCaptured = true;
    } else if (!capture && world.mouseCaptured) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        world.mouseCaptured = false;
    }
}

void handleInput(World& world, StateMachine& states, const SDL_Event& event) {
    if (event.type == SDL_QUIT) { world.quitRequested = true; return; }
    const GameStateId state = states.current();

    if (event.type == SDL_MOUSEMOTION && state == GameStateId::Playing && world.mouseCaptured) {
        world.player.facingRadians = wrapAngle(
            world.player.facingRadians + static_cast<float>(event.motion.xrel) * world.player.mouseSensitivity);
    }

    if (event.type == SDL_KEYDOWN) {
        SDL_Keycode k = event.key.keysym.sym;
        if (k == SDLK_ESCAPE) {
            if (state == GameStateId::Playing) { setMouseCapture(world, false); states.push(GameStateId::Paused); }
            else if (state == GameStateId::Paused) { setMouseCapture(world, true); states.pop(); }
            else if (state == GameStateId::DifficultySelect) { states.clearAndSet(GameStateId::MainMenu); }
            else if (state == GameStateId::Leaderboard) { states.clearAndSet(GameStateId::MainMenu); }
            else if (state == GameStateId::MainMenu || state == GameStateId::GameOver) { world.quitRequested = true; }
        }

        if (state == GameStateId::MainMenu) {
            if (k == SDLK_RETURN) states.clearAndSet(GameStateId::DifficultySelect);
            else if (k == SDLK_l) states.clearAndSet(GameStateId::Leaderboard);
        } else if (state == GameStateId::DifficultySelect) {
            if (k == SDLK_1) { world.difficulty = Difficulty::Easy; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
            else if (k == SDLK_2) { world.difficulty = Difficulty::Normal; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
            else if (k == SDLK_3) { world.difficulty = Difficulty::Hard; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
        } else if (state == GameStateId::GameOver) {
            if (k == SDLK_RETURN) { states.clearAndSet(GameStateId::DifficultySelect); }
        } else if (state == GameStateId::UpgradeSelection) {
            auto pick = [&](int idx) {
                if (idx < static_cast<int>(world.pendingUpgrades.size())) {
                    CombatSystem::applyUpgrade(world, world.pendingUpgrades[idx]);
                    world.wave += 1;
                    CombatSystem::spawnWave(world);
                    states.clearAndSet(GameStateId::Playing);
                }
            };
            if (k == SDLK_1) pick(0);
            else if (k == SDLK_2) pick(1);
            else if (k == SDLK_3) pick(2);
        } else if (state == GameStateId::Playing) {
            if (k == SDLK_1) world.player.currentWeapon = WeaponType::Pistol;
            else if (k == SDLK_2 && world.player.unlockedWeapons >= 2) world.player.currentWeapon = WeaponType::Shotgun;
            else if (k == SDLK_3 && world.player.unlockedWeapons >= 3) world.player.currentWeapon = WeaponType::Rapid;
            else if (k == SDLK_TAB) world.showMinimap = !world.showMinimap;
        }
    }
}

}  // namespace InputSystem
