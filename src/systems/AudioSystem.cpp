#include "systems/AudioSystem.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace AudioSystem {

namespace {

constexpr int kSampleRate = 22050;
constexpr int kAudioChannels = 1;

SDL_AudioDeviceID gDevice = 0;
float gMasterVolume = 0.8F;

struct SoundBuffer {
    std::vector<Sint16> samples;
};

SoundBuffer gSounds[static_cast<int>(SoundId::Count)];

float deterministicNoise(int sample) {
    uint32_t value = static_cast<uint32_t>(sample) * 1103515245U + 12345U;
    return static_cast<float>(value & 0x7FFFU) / 32768.0F - 0.5F;
}

void generateSound(SoundId id) {
    auto& buf = gSounds[static_cast<int>(id)];
    int len = 0;
    switch (id) {
        case SoundId::ShootPistol: {
            len = kSampleRate * 80 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = 600.0F - 400.0F * t;
                float env = (1.0F - t) * (1.0F - t);
                float noise = deterministicNoise(i);
                buf.samples[i] = static_cast<Sint16>((std::sin(t * freq * 6.28F) * 0.4F + noise * 0.6F) * env * 8000.0F);
            }
            break;
        }
        case SoundId::ShootShotgun: {
            len = kSampleRate * 120 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float env = (1.0F - t);
                float noise = deterministicNoise(i);
                buf.samples[i] = static_cast<Sint16>(noise * env * 12000.0F);
            }
            break;
        }
        case SoundId::ShootRapid: {
            len = kSampleRate * 50 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = 800.0F - 300.0F * t;
                float env = (1.0F - t) * (1.0F - t);
                buf.samples[i] = static_cast<Sint16>(std::sin(t * freq * 6.28F) * env * 6000.0F);
            }
            break;
        }
        case SoundId::EnemyHit: {
            len = kSampleRate * 40 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float env = 1.0F - t;
                buf.samples[i] = static_cast<Sint16>(std::sin(t * 1200.0F * 6.28F) * env * 5000.0F);
            }
            break;
        }
        case SoundId::EnemyDeath: {
            len = kSampleRate * 150 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = 400.0F - 350.0F * t;
                float env = (1.0F - t);
                buf.samples[i] = static_cast<Sint16>(std::sin(t * freq * 6.28F) * env * 7000.0F);
            }
            break;
        }
        case SoundId::PlayerHurt: {
            len = kSampleRate * 200 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float env = (1.0F - t) * 0.8F;
                float buzz = std::sin(t * 120.0F * 6.28F) * 0.5F + std::sin(t * 180.0F * 6.28F) * 0.5F;
                buf.samples[i] = static_cast<Sint16>(buzz * env * 6000.0F);
            }
            break;
        }
        case SoundId::PickupCollect: {
            len = kSampleRate * 150 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = 600.0F + 600.0F * t;
                float env = std::max(0.0F, 1.0F - t * 1.2F);
                buf.samples[i] = static_cast<Sint16>(std::sin(t * freq * 6.28F) * env * 5000.0F);
            }
            break;
        }
        case SoundId::WaveClear: {
            len = kSampleRate * 300 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = (t < 0.5F) ? 523.0F : 659.0F;
                float env = std::max(0.0F, 1.0F - t);
                buf.samples[i] = static_cast<Sint16>(std::sin(t * freq * 6.28F) * env * 5000.0F);
            }
            break;
        }
        case SoundId::Explosion: {
            len = kSampleRate * 250 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float env = (1.0F - t) * (1.0F - t);
                float noise = deterministicNoise(i);
                float bass = std::sin(t * 60.0F * 6.28F) * 0.5F;
                buf.samples[i] = static_cast<Sint16>((noise * 0.6F + bass) * env * 14000.0F);
            }
            break;
        }
        case SoundId::DoorOpen: {
            len = kSampleRate * 200 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float env = std::max(0.0F, 1.0F - t * 1.5F);
                float creak = std::sin(t * 200.0F * 6.28F) * 0.3F + std::sin(t * 350.0F * 6.28F) * 0.2F;
                float noise = deterministicNoise(i);
                buf.samples[i] = static_cast<Sint16>((creak + noise * 0.15F) * env * 6000.0F);
            }
            break;
        }
        case SoundId::LevelClear: {
            len = kSampleRate * 500 / 1000;
            buf.samples.resize(len);
            for (int i = 0; i < len; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(len);
                float freq = 523.0F;
                if (t > 0.2F) freq = 659.0F;
                if (t > 0.4F) freq = 784.0F;
                if (t > 0.6F) freq = 1047.0F;
                float env = std::max(0.0F, 1.0F - t * 0.8F);
                buf.samples[i] = static_cast<Sint16>(std::sin(t * freq * 6.28F) * env * 5000.0F);
            }
            break;
        }
        default: break;
    }
}

}  // namespace

void init() {
    SDL_AudioSpec want {};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = kAudioChannels;
    want.samples = 1024;
    want.callback = nullptr;
    gDevice = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (gDevice == 0) {
        std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(gDevice, 0);
    for (int i = 0; i < static_cast<int>(SoundId::Count); ++i)
        generateSound(static_cast<SoundId>(i));
}

void shutdown() {
    if (gDevice != 0) {
        SDL_CloseAudioDevice(gDevice);
        gDevice = 0;
    }
}

void play(SoundId id) {
    if (gDevice == 0) return;
    auto& buf = gSounds[static_cast<int>(id)];
    if (buf.samples.empty()) return;
    if (gMasterVolume < 0.01F) return;

    std::vector<Sint16> scaled(buf.samples.size());
    for (size_t i = 0; i < buf.samples.size(); ++i)
        scaled[i] = static_cast<Sint16>(buf.samples[i] * gMasterVolume);

    if (SDL_QueueAudio(gDevice, scaled.data(),
                       static_cast<Uint32>(scaled.size() * sizeof(Sint16))) != 0) {
        std::fprintf(stderr, "SDL_QueueAudio failed: %s\n", SDL_GetError());
    }
}

void setMasterVolume(float vol) {
    gMasterVolume = std::max(0.0F, std::min(1.0F, vol));
}

}  // namespace AudioSystem
