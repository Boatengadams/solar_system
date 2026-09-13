#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "EphemerisProvider.hpp"

namespace bag {

enum class SpiceKernelType { SPK, LSK, PCK, FK };

const char* spiceKernelTypeName(SpiceKernelType type);
bool parseSpiceKernelType(const std::string& value, SpiceKernelType& type);

struct SpiceKernelEntry {
    std::filesystem::path path;
    SpiceKernelType type = SpiceKernelType::SPK;
    int loadOrder = 0;
    bool enabled = true;
};

struct SpiceKernelManifest {
    std::vector<SpiceKernelEntry> entries;
    bool valid() const;
    std::string error() const;
};

struct SpiceManifestResult {
    bool success = false;
    SpiceKernelManifest manifest;
    std::string error;
};

SpiceManifestResult loadSpiceKernelManifest(const std::filesystem::path& path);

struct SpiceLoadResult {
    bool success = false;
    EphemerisStatus status = EphemerisStatus::PROVIDER_UNAVAILABLE;
    std::string error;
    std::vector<std::filesystem::path> loadedKernels;
};

class SpiceKernelManager {
public:
    SpiceLoadResult load(const SpiceKernelManifest& manifest);
    void unload();
    bool loaded() const { return !loadedKernels.empty(); }
    const std::vector<std::filesystem::path>& loadedKernelsView() const { return loadedKernels; }

private:
    std::vector<std::filesystem::path> loadedKernels;
};

class SpiceEphemerisProvider final : public EphemerisProvider {
public:
    explicit SpiceEphemerisProvider(SpiceKernelManifest manifest = {});
    ~SpiceEphemerisProvider() override;

    static bool compiledIn();
    SpiceLoadResult loadKernels();
    void unloadKernels();
    bool kernelsLoaded() const { return manager.loaded(); }
    const std::vector<std::filesystem::path>& loadedKernels() const { return manager.loadedKernelsView(); }
    EphemerisResult getState(const EphemerisRequest& request) override;
    const char* name() const override { return "SpiceEphemerisProvider"; }
    const SpiceKernelManifest& manifest() const { return kernelManifest; }

private:
    SpiceKernelManifest kernelManifest;
    SpiceKernelManager manager;
};

double julianDateToSpiceEt(const Epoch& epoch, bool* valid = nullptr);
bool spiceTargetId(const std::string& bodyId, std::string& targetId);
bool spiceOriginName(const Frame& frame, std::string& observer);
bool spiceFrameName(const Frame& frame, std::string& frameName);

} // namespace bag
