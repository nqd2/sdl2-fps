#include "systems/SaveSystem.hpp"

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

}  // namespace

void load(World& world) {
    const std::string path = getSavePath();
    std::ifstream in(path);
    if (!in.is_open()) return;
    int bestWave = 1;
    if (in >> bestWave) world.bestWave = std::max(1, bestWave);
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

void save(const World& world) {
    const std::string path = getSavePath();
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        std::fprintf(stderr, "Failed to open save file: %s\n", path.c_str());
        return;
    }
    out << world.bestWave << '\n';
    out << world.leaderboardCount << '\n';
    for (int i = 0; i < world.leaderboardCount; ++i) {
        const auto& e = world.leaderboard[i];
        out << e.score << ' ' << e.wave << ' '
            << static_cast<int>(e.difficulty) << ' ' << e.timeSeconds << '\n';
    }
}

}  // namespace SaveSystem
