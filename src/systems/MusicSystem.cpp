#include "systems/MusicSystem.hpp"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace MusicSystem {

namespace {

constexpr int kSampleRate = 22050;
constexpr int kBufferLen = kSampleRate * 4;

SDL_AudioDeviceID gDevice = 0;
std::vector<Sint16> gMusicBuffer;
int gPlayPos = 0;
float gVolume = 0.4F;

void audioCallback(void* /*userdata*/, Uint8* stream, int len) {
    auto* out = reinterpret_cast<Sint16*>(stream);
    int samples = len / static_cast<int>(sizeof(Sint16));
    for (int i = 0; i < samples; ++i) {
        out[i] = static_cast<Sint16>(gMusicBuffer[gPlayPos] * gVolume);
        gPlayPos = (gPlayPos + 1) % static_cast<int>(gMusicBuffer.size());
    }
}

void generateAmbientLoop() {
    gMusicBuffer.resize(kBufferLen);
    for (int i = 0; i < kBufferLen; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSampleRate);

        // Low drone with slow LFO
        float lfo = 0.5F + 0.5F * std::sin(t * 0.3F * 6.28F);
        float droneFreq = 50.0F + 15.0F * lfo;
        float drone = std::sin(t * droneFreq * 6.28F) * 0.4F;

        // Second harmonic
        float drone2 = std::sin(t * droneFreq * 2.0F * 6.28F) * 0.15F;

        // Sub bass rumble
        float sub = std::sin(t * 30.0F * 6.28F) * 0.2F * (0.5F + 0.5F * std::sin(t * 0.1F * 6.28F));

        // Occasional metallic ping
        float ping = 0.0F;
        int pingPeriod = kSampleRate * 3;
        int posInPeriod = i % pingPeriod;
        if (posInPeriod < kSampleRate / 4) {
            float pt = static_cast<float>(posInPeriod) / static_cast<float>(kSampleRate / 4);
            float pingEnv = (1.0F - pt) * (1.0F - pt);
            ping = std::sin(pt * 2400.0F * 6.28F) * pingEnv * 0.08F;
        }

        // Second ping (different interval)
        float ping2 = 0.0F;
        int ping2Period = kSampleRate * 7;
        int pos2 = i % ping2Period;
        if (pos2 < kSampleRate / 6) {
            float pt = static_cast<float>(pos2) / static_cast<float>(kSampleRate / 6);
            float pingEnv = (1.0F - pt) * (1.0F - pt);
            ping2 = std::sin(pt * 1800.0F * 6.28F) * pingEnv * 0.06F;
        }

        float sample = drone + drone2 + sub + ping + ping2;

        // Echo/reverb approximation
        if (i > kSampleRate / 4) {
            sample += static_cast<float>(gMusicBuffer[i - kSampleRate / 4]) / 32768.0F * 0.3F;
        }
        if (i > kSampleRate / 2) {
            sample += static_cast<float>(gMusicBuffer[i - kSampleRate / 2]) / 32768.0F * 0.15F;
        }

        sample = std::max(-1.0F, std::min(1.0F, sample));
        gMusicBuffer[i] = static_cast<Sint16>(sample * 6000.0F);
    }
}

}  // namespace

void init() {
    generateAmbientLoop();

    SDL_AudioSpec want {};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audioCallback;
    want.userdata = nullptr;

    gDevice = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (gDevice == 0) {
        std::fprintf(stderr, "MusicSystem: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(gDevice, 0);
}

void shutdown() {
    if (gDevice != 0) {
        SDL_CloseAudioDevice(gDevice);
        gDevice = 0;
    }
    gMusicBuffer.clear();
}

void setVolume(float vol) {
    gVolume = std::max(0.0F, std::min(1.0F, vol)) * 0.5F;
}

}  // namespace MusicSystem
