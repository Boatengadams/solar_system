#pragma once

#include <filesystem>
#include <string>

namespace bag {

struct ResourceRootResult {
    std::filesystem::path dataRoot;
    std::string error;

    explicit operator bool() const { return error.empty() && !dataRoot.empty(); }

    // Project/package root that contains `data/` (and usually `assets/`).
    std::filesystem::path projectRoot() const {
        return dataRoot.empty() ? std::filesystem::path{} : dataRoot.parent_path();
    }

    std::filesystem::path assetsRoot() const {
        const auto root = projectRoot();
        return root.empty() ? std::filesystem::path{} : root / "assets";
    }

    std::filesystem::path planetAssetsRoot() const {
        const auto assets = assetsRoot();
        return assets.empty() ? std::filesystem::path{} : assets / "planets";
    }
};

class ResourceRoot {
public:
    // Resolve runtime data relative to the current executable. The resolver
    // supports source checkouts, USB-side-by-side layouts, macOS
    // Contents/Resources, and relocatable installs. It never depends on the
    // process working directory or machine-fixed paths.
    // Optional override: BAGS_LAB_RESOURCE_ROOT may point at the package root
    // (containing data/) or directly at the data directory — used by the Linux
    // VFAT launcher when it runs a temporary ELF copy from /tmp.
    static ResourceRootResult resolve();

    // Deterministic overload for tests and packaging smoke checks.
    static ResourceRootResult resolve(const std::filesystem::path& executablePath);

    static bool isDataRoot(const std::filesystem::path& dataRoot, std::string& error);
};

} // namespace bag
