#pragma once

#include "entities/Components.hpp"

namespace Map {

constexpr int kWidth = 16;
constexpr int kHeight = 16;
constexpr float kPlayerRadius = 0.22F;

constexpr const char* kRows[kHeight] = {
    "################",
    "#......#.......#",
    "#......#.......#",
    "#..............#",
    "#..##..........#",
    "#..............#",
    "#......####....#",
    "#..............#",
    "#..............#",
    "#....##........#",
    "#..............#",
    "#...........#..#",
    "#..............#",
    "#...#..........#",
    "#..............#",
    "################",
};

bool isWall(float x, float y);
bool canMoveTo(const Vec2& p);
Vec2 randomOpenCell();
bool hasLineOfSight(Vec2 from, Vec2 to);

}  // namespace Map
