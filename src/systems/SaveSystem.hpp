#pragma once

#include "entities/Components.hpp"

namespace SaveSystem {

void load(World& world);
void save(const World& world);

}  // namespace SaveSystem
