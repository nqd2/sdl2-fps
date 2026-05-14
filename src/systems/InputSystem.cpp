#include "systems/InputSystem.hpp"
#include "systems/CombatSystem.hpp"
#include "systems/MathUtils.hpp"
#include "systems/SaveSystem.hpp"
#include "data/GameData.hpp"
#include "GameConstants.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

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

float clampFloat(float value, float lo, float hi) {
    return std::max(lo, std::min(hi, value));
}

int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(hi, value));
}

int categoryCount() {
    return static_cast<int>(SettingsCategory::COUNT);
}

int rowsForCategory(int category) {
    switch (static_cast<SettingsCategory>(category)) {
        case SettingsCategory::Audio:         return 4;
        case SettingsCategory::Controls:      return 4;
        case SettingsCategory::Video:         return 3;
        case SettingsCategory::Accessibility: return 2;
        default:                              return 1;
    }
}

TitleBarButton hitTestTitleButton(const World& world, int mx, int my) {
    if (my >= kTitleBarHeight) return TitleBarButton::None;
    int btnW = 52;
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

void applyDisplaySettings(World& world, SDL_Window* window) {
    if (world.settings.fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(window, &world.width, &world.height);
        return;
    }

    SDL_SetWindowFullscreen(window, 0);
    world.width = GameConstants::kResolutions[world.settings.resolutionIndex][0];
    world.height = GameConstants::kResolutions[world.settings.resolutionIndex][1];
    SDL_SetWindowSize(window, world.width, world.height);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void saveSettings(World& world) {
    world.player.mouseSensitivity = world.settings.mouseSensitivity;
    SaveSystem::save(world);
}

bool adjustFloat(float& value, float delta, float lo, float hi) {
    float next = clampFloat(value + delta, lo, hi);
    if (std::fabs(next - value) < 0.0001F) return false;
    value = next;
    return true;
}

bool cycleInt(int& value, int delta, int lo, int hi) {
    int next = value + delta;
    if (next < lo) next = hi;
    if (next > hi) next = lo;
    if (next == value) return false;
    value = next;
    return true;
}

bool adjustSetting(World& world, SDL_Window* window, int delta, bool activate) {
    Settings& s = world.settings;
    int category = clampInt(world.settingsCategory, 0, categoryCount() - 1);
    int row = clampInt(world.settingsCursor, 0, rowsForCategory(category) - 1);
    int step = activate && delta == 0 ? 1 : delta;
    bool changed = false;

    switch (static_cast<SettingsCategory>(category)) {
        case SettingsCategory::Audio:
            if (row == 0) changed = adjustFloat(s.masterVolume, 0.05F * static_cast<float>(step), 0.0F, 1.0F);
            else if (row == 1) changed = adjustFloat(s.musicVolume, 0.05F * static_cast<float>(step), 0.0F, 1.0F);
            else if (row == 2) changed = adjustFloat(s.sfxVolume, 0.05F * static_cast<float>(step), 0.0F, 1.0F);
            else if (row == 3 && (activate || delta != 0)) { s.muted = !s.muted; changed = true; }
            break;
        case SettingsCategory::Controls:
            if (row == 0) {
                int preset = static_cast<int>(s.controlPreset);
                changed = cycleInt(preset, step, 0, static_cast<int>(ControlPreset::COUNT) - 1);
                s.controlPreset = static_cast<ControlPreset>(preset);
            } else if (row == 1) {
                changed = adjustFloat(s.mouseSensitivity, 0.0005F * static_cast<float>(step), 0.001F, 0.010F);
            } else if (row == 2 && (activate || delta != 0)) {
                s.invertMouseX = !s.invertMouseX;
                changed = true;
            } else if (row == 3) {
                int minimap = static_cast<int>(s.minimapMode);
                changed = cycleInt(minimap, step, 0, static_cast<int>(MinimapMode::COUNT) - 1);
                s.minimapMode = static_cast<MinimapMode>(minimap);
            }
            break;
        case SettingsCategory::Video:
            if (row == 0) {
                changed = cycleInt(s.resolutionIndex, step, 0, GameConstants::kNumResolutions - 1);
                if (changed && !s.fullscreen) applyDisplaySettings(world, window);
            } else if (row == 1 && (activate || delta != 0)) {
                s.fullscreen = !s.fullscreen;
                applyDisplaySettings(world, window);
                changed = true;
            } else if (row == 2) {
                changed = cycleInt(s.uiScaleIndex, step, 0, 2);
            }
            break;
        case SettingsCategory::Accessibility:
            if (row == 0) changed = adjustFloat(s.screenShakeScale, 0.5F * static_cast<float>(step), 0.0F, 1.0F);
            else if (row == 1) changed = adjustFloat(s.screenFlashScale, 0.5F * static_cast<float>(step), 0.0F, 1.0F);
            break;
        default:
            break;
    }

    if (changed) saveSettings(world);
    return changed;
}

void advanceToNextLevel(World& world, StateMachine& states) {
    world.level += 1;
    world.wave = world.level;
    CombatSystem::spawnLevel(world);
    setMouseCapture(world, true);
    states.clearAndSet(GameStateId::Playing);
}

void returnToMainMenu(World& world, StateMachine& states) {
    setMouseCapture(world, false);
    states.clearAndSet(GameStateId::MainMenu);
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
                world.settings.muted = !world.settings.muted;
                saveSettings(world);
                return;
            case TitleBarButton::None:
                break;
        }
    }

    const GameStateId state = states.current();

    if (event.type == SDL_MOUSEMOTION && state == GameStateId::Playing && world.mouseCaptured) {
        float mouseDir = world.settings.invertMouseX ? -1.0F : 1.0F;
        world.player.facingRadians = wrapAngle(
            world.player.facingRadians + static_cast<float>(event.motion.xrel) * world.player.mouseSensitivity * mouseDir);
    }

    if (event.type == SDL_KEYDOWN) {
        SDL_Keycode k = event.key.keysym.sym;
        if (k == SDLK_ESCAPE) {
            if (state == GameStateId::Playing) {
                setMouseCapture(world, false);
                states.push(GameStateId::Paused);
            } else if (state == GameStateId::Paused) {
                setMouseCapture(world, true);
                states.pop();
            } else if (state == GameStateId::DifficultySelect || state == GameStateId::Leaderboard ||
                       state == GameStateId::Settings || state == GameStateId::GameOver) {
                returnToMainMenu(world, states);
            } else if (state == GameStateId::Shop || state == GameStateId::UpgradeSelection) {
                advanceToNextLevel(world, states);
            } else if (state == GameStateId::MainMenu) {
                world.quitRequested = true;
            }
            return;
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
            if (k == SDLK_RETURN) {
                CombatSystem::resetRun(world);
                setMouseCapture(world, true);
                states.clearAndSet(GameStateId::Playing);
            } else if (k == SDLK_d) {
                states.clearAndSet(GameStateId::DifficultySelect);
            } else if (k == SDLK_l) {
                states.clearAndSet(GameStateId::Leaderboard);
            }
        } else if (state == GameStateId::Shop) {
            int itemCount = static_cast<int>(world.shopItems.size());
            if (k == SDLK_q) {
                returnToMainMenu(world, states);
            } else if (itemCount <= 0) {
                if (k == SDLK_n || k == SDLK_RETURN || k == SDLK_SPACE) advanceToNextLevel(world, states);
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
            } else if (k == SDLK_n) {
                advanceToNextLevel(world, states);
            }
        } else if (state == GameStateId::Settings) {
            world.settingsCategory = clampInt(world.settingsCategory, 0, categoryCount() - 1);
            world.settingsCursor = clampInt(world.settingsCursor, 0, rowsForCategory(world.settingsCategory) - 1);
            if (k == SDLK_TAB) {
                world.settingsCategory = (world.settingsCategory + 1) % categoryCount();
                world.settingsCursor = 0;
            } else if (k == SDLK_UP || k == SDLK_w) {
                int rowCount = rowsForCategory(world.settingsCategory);
                world.settingsCursor = (world.settingsCursor - 1 + rowCount) % rowCount;
            } else if (k == SDLK_DOWN || k == SDLK_s) {
                int rowCount = rowsForCategory(world.settingsCategory);
                world.settingsCursor = (world.settingsCursor + 1) % rowCount;
            } else if (k == SDLK_LEFT || k == SDLK_a) {
                adjustSetting(world, window, -1, false);
            } else if (k == SDLK_RIGHT || k == SDLK_d) {
                adjustSetting(world, window, 1, false);
            } else if (k == SDLK_RETURN || k == SDLK_SPACE) {
                adjustSetting(world, window, 0, true);
            }
        } else if (state == GameStateId::UpgradeSelection) {
            auto pick = [&](int idx) {
                if (idx >= 0 && idx < static_cast<int>(world.pendingUpgrades.size())) {
                    CombatSystem::applyUpgrade(world, world.pendingUpgrades[idx]);
                    advanceToNextLevel(world, states);
                }
            };
            if (k == SDLK_1) pick(0);
            else if (k == SDLK_2) pick(1);
            else if (k == SDLK_3) pick(2);
            else if (k == SDLK_n) advanceToNextLevel(world, states);
            else if (k == SDLK_q) returnToMainMenu(world, states);
        } else if (state == GameStateId::Paused) {
            if (k == SDLK_q) returnToMainMenu(world, states);
        } else if (state == GameStateId::Playing) {
            if (k == SDLK_1) world.player.currentWeapon = WeaponType::Pistol;
            else if (k == SDLK_2 && world.player.unlockedWeapons >= 2) world.player.currentWeapon = WeaponType::Shotgun;
            else if (k == SDLK_3 && world.player.unlockedWeapons >= 3) world.player.currentWeapon = WeaponType::Rapid;
            else if (k == SDLK_TAB && world.settings.minimapMode == MinimapMode::Toggle) world.showMinimap = !world.showMinimap;
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
