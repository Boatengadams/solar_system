#include "ResourceRoot.hpp"

#include <array>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

#ifndef BAGSOLAR_INSTALL_DATA_SUBDIR
#define BAGSOLAR_INSTALL_DATA_SUBDIR "share/bagsolar/data"
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
    const std::array<std::filesystem::path, 3> candidates = {
        binDirectory / "data",
        prefixDirectory / "data",
        prefixDirectory / BAGSOLAR_INSTALL_DATA_SUBDIR,
    };

    std::string lastError;
    for (const auto& candidate : candidates) {
        std::string error;
        if (isDataRoot(candidate, error)) return {candidate, {}};
        lastError = std::move(error);
    }

    return {{}, "unable to locate BAGSOLAR runtime data for executable '" + executable.string() + "'; " + lastError};
}

} // namespace bag
