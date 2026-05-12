#pragma once

#include <vector>

enum class GameStateId {
    MainMenu,
    DifficultySelect,
    Playing,
    Paused,
    UpgradeSelection,
    Shop,
    GameOver,
    Leaderboard,
    Settings,
};

class StateMachine {
public:
    StateMachine();

    void clearAndSet(GameStateId state);
    void push(GameStateId state);
    void pop();
    GameStateId current() const;
    bool empty() const;

private:
    std::vector<GameStateId> stack_;
};
