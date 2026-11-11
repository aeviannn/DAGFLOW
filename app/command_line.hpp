#pragma once

#include <iosfwd>

namespace dagflow::app {

// Executes a machine-readable command. Returns -1 when no command was supplied,
// allowing main() to preserve the interactive CLI path.
int runCommandLine(int argc, const char* const argv[],
                   std::ostream& output, std::ostream& errors);

} // namespace dagflow::app
