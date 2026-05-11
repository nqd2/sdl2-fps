#include "core/StateMachine.hpp"

StateMachine::StateMachine() {
    clearAndSet(GameStateId::MainMenu);
}

void StateMachine::clearAndSet(GameStateId state) {
    stack_.clear();
    stack_.push_back(state);
}

void StateMachine::push(GameStateId state) {
    stack_.push_back(state);
}

void StateMachine::pop() {
    if (stack_.size() > 1U) {
        stack_.pop_back();
    }
}

GameStateId StateMachine::current() const {
    return stack_.empty() ? GameStateId::MainMenu : stack_.back();
}

bool StateMachine::empty() const {
    return stack_.empty();
}
