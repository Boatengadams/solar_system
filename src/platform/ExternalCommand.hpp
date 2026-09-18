#pragma once

#include <string>
#include <vector>

namespace bag {

// Captured stdout/stderr from an argv-based child process (no shell).
struct ExternalCommandResult {
    int exitCode = -1;
    std::string output;
    std::string error;
};

// Run `argv[0]` as the program with the remaining arguments.
// Never invokes a shell. On Windows uses CreateProcessW; on POSIX uses fork/exec.
ExternalCommandResult runExternalCommand(const std::vector<std::string>& argv);

} // namespace bag
