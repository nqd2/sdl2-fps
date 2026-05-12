#include "systems/SaveSystem.hpp"
#include "GameConstants.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace SaveSystem {

namespace {

constexpr int kSettingsVersion = 2;

float clampFloat(float value, float lo, float hi) {
    return std::max(lo, std::min(hi, value));
}

int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(hi, value));
}

std::string getSavePath() {
    char* prefPath = SDL_GetPrefPath("IronMaze", "V1");
    if (prefPath == nullptr) {
        std::fprintf(stderr, "SDL_GetPrefPath failed: %s, using CWD\n", SDL_GetError());
        return "progress.sav";
    }
    std::string path(prefPath);
    SDL_free(prefPath);
    path += "progress.sav";
    return path;
}

std::string getSettingsPath() {
    char* prefPath = SDL_GetPrefPath("IronMaze", "V1");
    if (prefPath == nullptr) return "settings.sav";
    std::string path(prefPath);
    SDL_free(prefPath);
    path += "settings.sav";
    return path;
}

void applyLoadedSettings(World& world) {
    Settings& s = world.settings;
    s.masterVolume = clampFloat(s.masterVolume, 0.0F, 1.0F);
    s.musicVolume = clampFloat(s.musicVolume, 0.0F, 1.0F);
    s.sfxVolume = clampFloat(s.sfxVolume, 0.0F, 1.0F);
    s.mouseSensitivity = clampFloat(s.mouseSensitivity, 0.001F, 0.010F);
    s.resolutionIndex = clampInt(s.resolutionIndex, 0, GameConstants::kNumResolutions - 1);
    int control = clampInt(static_cast<int>(s.controlPreset), 0, static_cast<int>(ControlPreset::COUNT) - 1);
    s.controlPreset = static_cast<ControlPreset>(control);
    s.uiScaleIndex = clampInt(s.uiScaleIndex, 0, 2);
    s.screenShakeScale = clampFloat(s.screenShakeScale, 0.0F, 1.0F);
    s.screenFlashScale = clampFloat(s.screenFlashScale, 0.0F, 1.0F);
    int minimap = clampInt(static_cast<int>(s.minimapMode), 0, static_cast<int>(MinimapMode::COUNT) - 1);
    s.minimapMode = static_cast<MinimapMode>(minimap);

    world.player.mouseSensitivity = s.mouseSensitivity;
    if (!s.fullscreen) {
        world.width = GameConstants::kResolutions[s.resolutionIndex][0];
        world.height = GameConstants::kResolutions[s.resolutionIndex][1];
    }
}

}  // namespace

void loadSettingsFromStream(World& world, std::istream& in) {
    std::string first;
    if (!(in >> first)) return;

    Settings s {};
    if (first == std::to_string(kSettingsVersion)) {
        int muted = 0;
        int fullscreen = 0;
        int invertMouse = 0;
        int control = 0;
        int minimap = 0;
        if (in >> s.masterVolume >> s.musicVolume >> s.sfxVolume
               >> s.mouseSensitivity >> s.resolutionIndex
               >> muted >> fullscreen >> invertMouse
               >> control >> s.uiScaleIndex
               >> s.screenShakeScale >> s.screenFlashScale >> minimap) {
            s.muted = muted != 0;
            s.fullscreen = fullscreen != 0;
            s.invertMouseX = invertMouse != 0;
            s.controlPreset = static_cast<ControlPreset>(control);
            s.minimapMode = static_cast<MinimapMode>(minimap);
            world.settings = s;
            applyLoadedSettings(world);
        }
        return;
    }

    std::istringstream oldFirst(first);
    float oldVolume = 0.8F;
    float oldSensitivity = 0.003F;
    int oldResolution = 2;
    if (oldFirst >> oldVolume && in >> oldSensitivity >> oldResolution) {
        s.masterVolume = oldVolume;
        s.mouseSensitivity = oldSensitivity;
        s.resolutionIndex = oldResolution;
        world.settings = s;
        applyLoadedSettings(world);
    }
}

void saveSettingsToStream(const World& world, std::ostream& out) {
    const Settings& s = world.settings;
    out << kSettingsVersion << ' '
        << s.masterVolume << ' '
        << s.musicVolume << ' '
        << s.sfxVolume << ' '
        << s.mouseSensitivity << ' '
        << s.resolutionIndex << ' '
        << (s.muted ? 1 : 0) << ' '
        << (s.fullscreen ? 1 : 0) << ' '
        << (s.invertMouseX ? 1 : 0) << ' '
        << static_cast<int>(s.controlPreset) << ' '
        << s.uiScaleIndex << ' '
        << s.screenShakeScale << ' '
        << s.screenFlashScale << ' '
        << static_cast<int>(s.minimapMode) << '\n';
}

void load(World& world) {
    const std::string path = getSavePath();
    std::ifstream in(path);
    if (in.is_open()) {
        int bestWave = 1;
        if (in >> bestWave) world.bestWave = std::max(1, bestWave);
        int bestLevel = 1;
        if (in >> bestLevel) world.bestLevel = std::max(1, bestLevel);
        int count = 0;
        if (in >> count) {
            world.leaderboardCount = std::min(count, kMaxLeaderboard);
            for (int i = 0; i < world.leaderboardCount; ++i) {
                int score, wave2, diff;
                float time2;
                if (in >> score >> wave2 >> diff >> time2) {
                    Difficulty d = Difficulty::Normal;
                    if (isValidDifficulty(diff)) d = static_cast<Difficulty>(diff);
                    world.leaderboard[i] = {score, wave2, d, time2};
                }
            }
        }
    }

    const std::string settingsPath = getSettingsPath();
    std::ifstream sin(settingsPath);
    if (sin.is_open()) {
        loadSettingsFromStream(world, sin);
    }
}

void save(const World& world) {
    const std::string path = getSavePath();
    std::ofstream out(path, std::ios::trunc);
    if (out.is_open()) {
        out << world.bestWave << '\n';
        out << world.bestLevel << '\n';
        out << world.leaderboardCount << '\n';
        for (int i = 0; i < world.leaderboardCount; ++i) {
            const auto& e = world.leaderboard[i];
            out << e.score << ' ' << e.wave << ' '
                << static_cast<int>(e.difficulty) << ' ' << e.timeSeconds << '\n';
        }
    } else {
        std::fprintf(stderr, "Failed to open save file: %s\n", path.c_str());
    }

    const std::string settingsPath = getSettingsPath();
    std::ofstream sout(settingsPath, std::ios::trunc);
    if (sout.is_open()) {
        saveSettingsToStream(world, sout);
    }
}

}  // namespace SaveSystem
