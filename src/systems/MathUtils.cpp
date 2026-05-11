#include "systems/MathUtils.hpp"

#include <random>

namespace {
std::mt19937 gRng(1337U);
}

float randomFloat(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(gRng);
}

namespace RNG {

void seed(unsigned int s) {
    gRng.seed(s);
}

int uniformInt(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(gRng);
}

}  // namespace RNG
