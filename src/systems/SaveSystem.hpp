#pragma once

#include "entities/Components.hpp"

#include <iosfwd>

namespace SaveSystem {

void load(World& world);
void save(const World& world);
void loadSettingsFromStream(World& world, std::istream& in);
void saveSettingsToStream(const World& world, std::ostream& out);

}  // namespace SaveSystem
