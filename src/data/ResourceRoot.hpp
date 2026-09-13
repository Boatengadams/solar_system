#pragma once

#include <filesystem>
#include <string>

namespace bag {

struct ResourceRootResult {
    std::filesystem::path dataRoot;
    std::string error;

    explicit operator bool() const { return error.empty() && !dataRoot.empty(); }
};

class ResourceRoot {
public:
    // Resolve runtime data relative to the current executable. The resolver
    // supports both a source checkout and a relocatable install tree.
    static ResourceRootResult resolve();

    // This overload makes path behavior deterministic and testable without
    // depending on the caller's current working directory.
    static ResourceRootResult resolve(const std::filesystem::path& executablePath);

    static bool isDataRoot(const std::filesystem::path& dataRoot, std::string& error);
};

} // namespace bag
