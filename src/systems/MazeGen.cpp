#include "systems/MazeGen.hpp"

#include <algorithm>
#include <queue>
#include <random>
#include <stack>
#include <vector>

namespace MazeGen {

MazeResult generate(int level, unsigned int seed) {
    int mazeW = 21 + level * 4;
    int mazeH = 21 + level * 4;
    if (mazeW > 51) mazeW = 51;
    if (mazeH > 51) mazeH = 51;
    if (mazeW % 2 == 0) mazeW--;
    if (mazeH % 2 == 0) mazeH--;

    std::vector<char> grid(mazeW * mazeH, '#');
    std::mt19937 rng(seed);

    auto idx = [mazeW](int x, int y) { return y * mazeW + x; };

    // Recursive backtracker (iterative with explicit stack)
    grid[idx(1, 1)] = '.';
    std::stack<std::pair<int, int>> stk;
    stk.push({1, 1});

    const int dx[] = {0, 0, -2, 2};
    const int dy[] = {-2, 2, 0, 0};

    while (!stk.empty()) {
        auto [cx, cy] = stk.top();

        std::vector<int> dirs = {0, 1, 2, 3};
        std::shuffle(dirs.begin(), dirs.end(), rng);

        bool pushed = false;
        for (int d : dirs) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx > 0 && nx < mazeW - 1 && ny > 0 && ny < mazeH - 1 &&
                grid[idx(nx, ny)] == '#') {
                grid[idx(cx + dx[d] / 2, cy + dy[d] / 2)] = '.';
                grid[idx(nx, ny)] = '.';
                stk.push({nx, ny});
                pushed = true;
                break;
            }
        }
        if (!pushed) {
            stk.pop();
        }
    }

    // BFS from spawn to find the farthest open cell for exit placement
    std::vector<int> dist(mazeW * mazeH, -1);
    std::queue<std::pair<int, int>> bfsQ;
    dist[idx(1, 1)] = 0;
    bfsQ.push({1, 1});

    const int bfsDx[] = {1, -1, 0, 0};
    const int bfsDy[] = {0, 0, 1, -1};

    int farX = 1, farY = 1, farDist = 0;

    while (!bfsQ.empty()) {
        auto [bx, by] = bfsQ.front();
        bfsQ.pop();
        for (int d = 0; d < 4; ++d) {
            int nx = bx + bfsDx[d];
            int ny = by + bfsDy[d];
            if (nx >= 0 && nx < mazeW && ny >= 0 && ny < mazeH &&
                dist[idx(nx, ny)] == -1 && grid[idx(nx, ny)] == '.') {
                dist[idx(nx, ny)] = dist[idx(bx, by)] + 1;
                bfsQ.push({nx, ny});
                if (dist[idx(nx, ny)] > farDist) {
                    farDist = dist[idx(nx, ny)];
                    farX = nx;
                    farY = ny;
                }
            }
        }
    }

    grid[idx(farX, farY)] = 'E';

    auto exitReachable = [&]() {
        std::vector<bool> visited(mazeW * mazeH, false);
        std::queue<std::pair<int, int>> q;
        visited[idx(1, 1)] = true;
        q.push({1, 1});

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();
            if (x == farX && y == farY) return true;

            for (int d = 0; d < 4; ++d) {
                int nx = x + bfsDx[d];
                int ny = y + bfsDy[d];
                if (nx < 0 || nx >= mazeW || ny < 0 || ny >= mazeH) continue;
                if (visited[idx(nx, ny)]) continue;
                char c = grid[idx(nx, ny)];
                if (c == '.' || c == 'E') {
                    visited[idx(nx, ny)] = true;
                    q.push({nx, ny});
                }
            }
        }
        return false;
    };

    // Place locked doors at random corridor cells
    int numDoors = level / 2;
    if (numDoors > 5) numDoors = 5;

    if (numDoors > 0) {
        std::vector<std::pair<int, int>> candidates;
        for (int y = 1; y < mazeH - 1; ++y) {
            for (int x = 1; x < mazeW - 1; ++x) {
                if (grid[idx(x, y)] != '.') continue;

                // Skip cells adjacent to spawn or exit
                bool nearSpawn = (std::abs(x - 1) + std::abs(y - 1)) <= 1;
                bool nearExit = (std::abs(x - farX) + std::abs(y - farY)) <= 1;
                if (nearSpawn || nearExit) continue;

                // Must be a corridor: exactly 2 open neighbors
                int openNeighbors = 0;
                for (int d = 0; d < 4; ++d) {
                    char c = grid[idx(x + bfsDx[d], y + bfsDy[d])];
                    if (c == '.' || c == 'E') ++openNeighbors;
                }
                if (openNeighbors == 2) {
                    candidates.push_back({x, y});
                }
            }
        }

        std::shuffle(candidates.begin(), candidates.end(), rng);
        int placed = 0;
        for (const auto& candidate : candidates) {
            if (placed >= numDoors) break;
            int doorIndex = idx(candidate.first, candidate.second);
            grid[doorIndex] = 'D';
            if (exitReachable()) {
                ++placed;
            } else {
                grid[doorIndex] = '.';
            }
        }
    }

    MazeResult result;
    result.grid = std::move(grid);
    result.width = mazeW;
    result.height = mazeH;
    result.startX = 1;
    result.startY = 1;
    result.exitX = farX;
    result.exitY = farY;
    return result;
}

}  // namespace MazeGen
