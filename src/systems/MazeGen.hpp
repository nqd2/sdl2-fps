#pragma once

#include <vector>

namespace MazeGen {

struct MazeResult {
    std::vector<char> grid;
    int width;
    int height;
    int startX, startY;
    int exitX, exitY;
};

MazeResult generate(int level, unsigned int seed);

}  // namespace MazeGen
