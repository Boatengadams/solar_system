#include "ResourceRoot.hpp"

#include <array>
#include <cstdlib>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

// Install-layout fallbacks. Prefer bags_lab; keep bagsolar for older packages.
#ifndef BAGS_LAB_INSTALL_DATA_SUBDIR
#ifdef BAGSOLAR_INSTALL_DATA_SUBDIR
#define BAGS_LAB_INSTALL_DATA_SUBDIR BAGSOLAR_INSTALL_DATA_SUBDIR
#else
#define BAGS_LAB_INSTALL_DATA_SUBDIR "share/bags_lab/data"
#endif
#endif

#ifndef BAGS_LAB_LEGACY_INSTALL_DATA_SUBDIR
#define BAGS_LAB_LEGACY_INSTALL_DATA_SUBDIR "share/bagsolar/data"
#endif

namespace bag {
namespace {

std::filesystem::path currentExecutablePath() {
#if defined(_WIN32)
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return std::filesystem::path(std::wstring(buffer.data(), length));
#elif defined(__APPLE__)
    uint32_t length = 0;
    if (_NSGetExecutablePath(nullptr, &length) != -1 || length == 0) return {};
    std::vector<char> buffer(length + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &length) != 0) return {};
    return std::filesystem::path(buffer.data());
#elif defined(__linux__)
    std::array<char, PATH_MAX> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) return {};
    buffer[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data());
#else
    return {};
#endif
}

std::filesystem::path absoluteNormalized(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    return absolute.lexically_normal();
}

// If executable is .../Something.app/Contents/MacOS/<bin>, return Contents/.
std::filesystem::path macosBundleContentsDirectory(const std::filesystem::path& executable) {
    const auto macosDir = executable.parent_path();
    if (macosDir.filename() != "MacOS") return {};
    const auto contentsDir = macosDir.parent_path();
    if (contentsDir.filename() != "Contents") return {};
    const auto appDir = contentsDir.parent_path();
    if (appDir.extension() != ".app") return {};
    return contentsDir;
}

} // namespace

bool ResourceRoot::isDataRoot(const std::filesystem::path& dataRoot, std::string& error) {
    std::error_code filesystemError;
    if (!std::filesystem::is_directory(dataRoot, filesystemError) || filesystemError) {
        error = "data directory does not exist: " + dataRoot.string();
        return false;
    }

    constexpr std::array<const char*, 2> requiredFiles = {
        "bodies/sun.json",
        "scenarios/default_solar_system.json",
    };
    for (const char* relativePath : requiredFiles) {
        const auto path = dataRoot / relativePath;
        if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError) {
            error = "required runtime resource is missing: " + path.string();
            return false;
        }
    }
    return true;
}

ResourceRootResult ResourceRoot::resolve() {
    // Portable USB launchers may run a temporary ELF copy from /tmp while the
    // authoritative package (data/assets) remains on the USB. Prefer that root.
    if (const char* overrideRoot = std::getenv("BAGS_LAB_RESOURCE_ROOT")) {
        if (overrideRoot[0] != '\0') {
            const auto root = absoluteNormalized(overrideRoot);
            if (root.empty()) {
                return {{}, "BAGS_LAB_RESOURCE_ROOT is set but could not be resolved"};
            }
            std::string error;
            if (isDataRoot(root / "data", error)) {
                return {root / "data", {}};
            }
            // Allow pointing directly at the data directory.
            if (isDataRoot(root, error)) {
                return {root, {}};
            }
            return {{}, "BAGS_LAB_RESOURCE_ROOT is set but invalid: " + error};
        }
    }

    const auto executablePath = currentExecutablePath();
    if (executablePath.empty()) {
        return {{}, "unable to determine the current executable path"};
    }
    return resolve(executablePath);
}

ResourceRootResult ResourceRoot::resolve(const std::filesystem::path& executablePath) {
    const auto executable = absoluteNormalized(executablePath);
    if (executable.empty()) {
        return {{}, "unable to resolve executable path: " + executablePath.string()};
    }

    const auto binDirectory = executable.parent_path();
    const auto prefixDirectory = binDirectory.parent_path();
    const auto bundleContents = macosBundleContentsDirectory(executable);

    // Layouts (first match wins):
    // 1) USB / portable: <package>/BAGS_LAB next to <package>/data
    // 2) source build:   <repo>/build/BAGS_LAB with <repo>/data
    // 3) macOS .app:     Contents/MacOS/BAGS_LAB → Contents/Resources/data
    // 4) install prefix: <prefix>/bin/BAGS_LAB with <prefix>/share/bags_lab/data
    // 5) legacy install: <prefix>/share/bagsolar/data
    std::vector<std::filesystem::path> candidates = {
        binDirectory / "data",
        prefixDirectory / "data",
    };
    if (!bundleContents.empty()) {
        candidates.push_back(bundleContents / "Resources" / "data");
    }
    candidates.push_back(prefixDirectory / BAGS_LAB_INSTALL_DATA_SUBDIR);
    candidates.push_back(prefixDirectory / BAGS_LAB_LEGACY_INSTALL_DATA_SUBDIR);

    std::string lastError;
    for (const auto& candidate : candidates) {
        std::string error;
        if (isDataRoot(candidate, error)) return {candidate, {}};
        lastError = std::move(error);
    }

    return {{}, "unable to locate BAGS_LAB runtime data for executable '" + executable.string() + "': " + lastError};
}

} // namespace bag
