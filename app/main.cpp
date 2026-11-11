#include "cli.hpp"

#include <iostream>

int main() {
    return dagflow::app::runCli(std::cin, std::cout, std::cerr);
}
