#pragma once

#include "entities/Components.hpp"

namespace AudioSystem {

void init();
void shutdown();
void play(SoundId id);
void setMasterVolume(float vol);

}  // namespace AudioSystem
