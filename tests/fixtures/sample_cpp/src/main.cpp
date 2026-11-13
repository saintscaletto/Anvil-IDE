#include <iostream>
#include <string>

#include "engine.h"

int main() {
    Engine engine("developer");
    std::cout << engine.greet() << '\n';
    return 0;
}
