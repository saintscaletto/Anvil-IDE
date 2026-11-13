#include "engine.h"

Engine::Engine(std::string name) : name_(std::move(name)) {}

std::string Engine::greet() const {
    return "Anvil is ready, " + name_;
}
