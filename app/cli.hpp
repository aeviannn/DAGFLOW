#pragma once

#include <iosfwd>

namespace dagflow::app {

int runCli(std::istream& input, std::ostream& output, std::ostream& errors);

} // namespace dagflow::app
