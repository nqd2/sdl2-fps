#include "systems/InputSystem.hpp"
#include "systems/CombatSystem.hpp"
#include "systems/MathUtils.hpp"
#include "data/GameData.hpp"
#include "GameConstants.hpp"

#include <SDL.h>

#include <algorithm>

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

namespace {

TitleBarButton hitTestTitleButton(const World& world, int mx, int my) {
    if (my >= kTitleBarHeight) return TitleBarButton::None;
    int btnW = 46;
    int closeX = world.width - btnW;
    int maxX   = closeX - btnW;
    int minX   = maxX - btnW;
    int muteX  = minX - btnW;
    if (mx >= closeX)            return TitleBarButton::Close;
    if (mx >= maxX)              return TitleBarButton::Maximize;
    if (mx >= minX)              return TitleBarButton::Minimize;
    if (mx >= muteX)             return TitleBarButton::Mute;
    return TitleBarButton::None;
}

}  // namespace

void handleInput(World& world, StateMachine& states, const SDL_Event& event, SDL_Window* window) {
    if (event.type == SDL_QUIT) { world.quitRequested = true; return; }

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            world.width = event.window.data1;
            world.height = event.window.data2;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        TitleBarButton btn = hitTestTitleButton(world, event.button.x, event.button.y);
        switch (btn) {
            case TitleBarButton::Close:
                world.quitRequested = true;
                return;
            case TitleBarButton::Minimize:
                SDL_MinimizeWindow(window);
                return;
            case TitleBarButton::Maximize: {
                Uint32 flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_MAXIMIZED)
                    SDL_RestoreWindow(window);
                else
                    SDL_MaximizeWindow(window);
                return;
            }
            case TitleBarButton::Mute:
                if (world.settings.muted) {
                    world.settings.muted = false;
                    world.settings.masterVolume = world.settings.volumeBeforeMute;
                } else {
                    world.settings.volumeBeforeMute = world.settings.masterVolume;
                    world.settings.muted = true;
                }
                return;
            case TitleBarButton::None:
                break;
        }
    }

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
            else if (state == GameStateId::Settings) { states.clearAndSet(GameStateId::MainMenu); }
            else if (state == GameStateId::Shop) {
                world.level += 1;
                world.wave = world.level;
                CombatSystem::spawnLevel(world);
                setMouseCapture(world, true);
                states.clearAndSet(GameStateId::Playing);
            }
            else if (state == GameStateId::MainMenu || state == GameStateId::GameOver) { world.quitRequested = true; }
        }

        if (state == GameStateId::MainMenu) {
            if (k == SDLK_RETURN) states.clearAndSet(GameStateId::DifficultySelect);
            else if (k == SDLK_l) states.clearAndSet(GameStateId::Leaderboard);
            else if (k == SDLK_s) states.clearAndSet(GameStateId::Settings);
        } else if (state == GameStateId::DifficultySelect) {
            if (k == SDLK_1) { world.difficulty = Difficulty::Easy; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
            else if (k == SDLK_2) { world.difficulty = Difficulty::Normal; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
            else if (k == SDLK_3) { world.difficulty = Difficulty::Hard; CombatSystem::resetRun(world); setMouseCapture(world, true); states.clearAndSet(GameStateId::Playing); }
        } else if (state == GameStateId::GameOver) {
            if (k == SDLK_RETURN) { states.clearAndSet(GameStateId::DifficultySelect); }
        } else if (state == GameStateId::Shop) {
            int itemCount = static_cast<int>(world.shopItems.size());
            if (itemCount <= 0) {
                if (k == SDLK_n || k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_SPACE) {
                    world.level += 1;
                    world.wave = world.level;
                    CombatSystem::spawnLevel(world);
                    setMouseCapture(world, true);
                    states.clearAndSet(GameStateId::Playing);
                }
            } else if (k == SDLK_UP || k == SDLK_w) {
                world.shopCursor = (world.shopCursor - 1 + itemCount) % itemCount;
            } else if (k == SDLK_DOWN || k == SDLK_s) {
                world.shopCursor = (world.shopCursor + 1) % itemCount;
            } else if (k == SDLK_RETURN || k == SDLK_SPACE) {
                if (world.shopCursor >= 0 && world.shopCursor < itemCount) {
                    auto& item = world.shopItems[world.shopCursor];
                    if (world.score >= item.cost) {
                        world.score -= item.cost;
                        switch (item.type) {
                            case ShopItem::Type::Upgrade: {
                                auto pool = allUpgradeOptions();
                                if (item.upgradeIndex >= 0 && item.upgradeIndex < static_cast<int>(pool.size()))
                                    CombatSystem::applyUpgrade(world, pool[item.upgradeIndex]);
                                break;
                            }
                            case ShopItem::Type::Health:
                                world.player.hp = std::min(world.player.maxHp, world.player.hp + 5);
                                break;
                            case ShopItem::Type::Key:
                                world.player.keys += 1;
                                break;
                        }
                        world.shopItems.erase(world.shopItems.begin() + world.shopCursor);
                        if (world.shopCursor >= static_cast<int>(world.shopItems.size()))
                            world.shopCursor = std::max(0, static_cast<int>(world.shopItems.size()) - 1);
                    }
                }
            } else if (k == SDLK_n || k == SDLK_ESCAPE) {
                world.level += 1;
                world.wave = world.level;
                CombatSystem::spawnLevel(world);
                setMouseCapture(world, true);
                states.clearAndSet(GameStateId::Playing);
            }
        } else if (state == GameStateId::Settings) {
            if (k == SDLK_1) {
                world.settings.masterVolume = std::min(1.0F, world.settings.masterVolume + 0.1F);
            } else if (k == SDLK_2) {
                world.settings.masterVolume = std::max(0.0F, world.settings.masterVolume - 0.1F);
            } else if (k == SDLK_3) {
                world.settings.mouseSensitivity = std::min(0.010F, world.settings.mouseSensitivity + 0.001F);
                world.player.mouseSensitivity = world.settings.mouseSensitivity;
            } else if (k == SDLK_4) {
                world.settings.mouseSensitivity = std::max(0.001F, world.settings.mouseSensitivity - 0.001F);
                world.player.mouseSensitivity = world.settings.mouseSensitivity;
            } else if (k == SDLK_5) {
                world.settings.resolutionIndex = (world.settings.resolutionIndex + 1) % GameConstants::kNumResolutions;
                world.width = GameConstants::kResolutions[world.settings.resolutionIndex][0];
                world.height = GameConstants::kResolutions[world.settings.resolutionIndex][1];
                SDL_SetWindowSize(window, world.width, world.height);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
        } else if (state == GameStateId::UpgradeSelection) {
            auto pick = [&](int idx) {
                if (idx >= 0 && idx < static_cast<int>(world.pendingUpgrades.size())) {
                    CombatSystem::applyUpgrade(world, world.pendingUpgrades[idx]);
                    world.level += 1;
                    world.wave = world.level;
                    CombatSystem::spawnLevel(world);
                    setMouseCapture(world, true);
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
            else if (k == SDLK_e) CombatSystem::tryOpenDoor(world);

            // F1-F7: stat allocation
            if (world.player.statPoints > 0) {
                int statIdx = -1;
                if (k == SDLK_F1) statIdx = static_cast<int>(StatId::MaxHp);
                else if (k == SDLK_F2) statIdx = static_cast<int>(StatId::HpRegen);
                else if (k == SDLK_F3) statIdx = static_cast<int>(StatId::BulletDamage);
                else if (k == SDLK_F4) statIdx = static_cast<int>(StatId::FireRate);
                else if (k == SDLK_F5) statIdx = static_cast<int>(StatId::MoveSpeed);
                else if (k == SDLK_F6) statIdx = static_cast<int>(StatId::BulletRange);
                else if (k == SDLK_F7) statIdx = static_cast<int>(StatId::BodyArmor);
                if (statIdx >= 0 && world.player.stats[statIdx] < kMaxStatLevel) {
                    world.player.stats[statIdx] += 1;
                    world.player.statPoints -= 1;
                    CombatSystem::applyStatEffects(world);
                }
            }
        }
    }
}

}  // namespace InputSystem
