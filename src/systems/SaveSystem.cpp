#include "systems/SaveSystem.hpp"
#include "GameConstants.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

namespace SaveSystem {

namespace {

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

}  // namespace

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
        float vol, sens;
        int resIdx;
        if (sin >> vol >> sens >> resIdx) {
            world.settings.masterVolume = std::max(0.0F, std::min(1.0F, vol));
            world.settings.mouseSensitivity = std::max(0.001F, std::min(0.010F, sens));
            world.settings.resolutionIndex = std::max(0, std::min(GameConstants::kNumResolutions - 1, resIdx));
            world.player.mouseSensitivity = world.settings.mouseSensitivity;
            world.width = GameConstants::kResolutions[world.settings.resolutionIndex][0];
            world.height = GameConstants::kResolutions[world.settings.resolutionIndex][1];
        }
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
        sout << world.settings.masterVolume << ' '
             << world.settings.mouseSensitivity << ' '
             << world.settings.resolutionIndex << '\n';
    }
}

}  // namespace SaveSystem
