#include "core/Game.hpp"
#include "GameConstants.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/MusicSystem.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/SaveSystem.hpp"
#include "systems/UpdateSystem.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>

Game::Game() = default;

Game::~Game() {
    shutdown();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("Iron Maze",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               world_.width,
                               world_.height,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    AudioSystem::init();
    MusicSystem::init();
    SaveSystem::load(world_);
    AudioSystem::setMasterVolume(world_.settings.masterVolume);
    MusicSystem::setVolume(world_.settings.masterVolume);
    initialized_ = true;
    return true;
}

void Game::run() {
    if (!initialized_) {
        return;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();
    float accumulator = 0.0F;

    while (!world_.quitRequested) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            InputSystem::handleInput(world_, states_, event);
        }

        const Uint64 nowCounter = SDL_GetPerformanceCounter();
        const Uint64 freq = SDL_GetPerformanceFrequency();
        const float frameSeconds = static_cast<float>(
            static_cast<double>(nowCounter - lastCounter) / static_cast<double>(freq));
        lastCounter = nowCounter;
        accumulator = std::min(0.25F, accumulator + frameSeconds);

        while (accumulator >= GameConstants::kFixedStep) {
            UpdateSystem::update(world_, states_, GameConstants::kFixedStep);
            accumulator -= GameConstants::kFixedStep;
        }

        AudioSystem::setMasterVolume(world_.settings.masterVolume);
        MusicSystem::setVolume(world_.settings.masterVolume);

        RenderSystem::render(world_, states_.current(), renderer_);
    }
}

void Game::shutdown() {
    if (!initialized_) {
        return;
    }

    SaveSystem::save(world_);
    MusicSystem::shutdown();
    AudioSystem::shutdown();

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    initialized_ = false;
}
