#pragma once

#include <string>

class Engine {
public:
    explicit Engine(std::string name);
    std::string greet() const;

private:
    std::string name_;
};
