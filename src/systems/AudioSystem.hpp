#pragma once

#include "entities/Components.hpp"

namespace AudioSystem {

void init();
void shutdown();
void play(SoundId id);

}  // namespace AudioSystem
