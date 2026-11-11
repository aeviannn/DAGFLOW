#include "cli.hpp"
#include "command_line.hpp"

#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<const char*> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) arguments.push_back(argv[index]);
    const int commandStatus = dagflow::app::runCommandLine(
        argc, arguments.data(), std::cout, std::cerr);
    if (commandStatus >= 0) return commandStatus;
    return dagflow::app::runCli(std::cin, std::cout, std::cerr);
}
