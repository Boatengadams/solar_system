#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <raylib.h>

namespace bag {

// Presentation metadata only.  It deliberately has no scientific fields.
struct PlanetPresentation {
    std::string bodyId;
    std::string modelFile;
    bool enabled = false;
    std::string fallbackReason;
    float visualScale = 1.0f;
    Vector3 localRotationDegrees{};
    Vector3 localTranslation{};
    bool modelIncludesRings = false;
};

// Owns GPU model resources for the renderer lifetime.  A failed or absent
// model is intentionally not an error for the simulation: callers use their
// procedural presentation fallback when has() is false.
class PlanetAssetManager {
public:
    explicit PlanetAssetManager(std::filesystem::path planetDirectory = {});

    bool load();
    bool has(const std::string& bodyId) const;
    const PlanetPresentation* presentation(const std::string& bodyId) const;
    const Model* model(const std::string& bodyId) const;
    float modelRadius(const std::string& bodyId) const;
    Vector3 modelCenter(const std::string& bodyId) const;
    void unload();

    static bool isSupportedBodyId(const std::string& bodyId) {
        return bodyId == "sun" || bodyId == "mercury" || bodyId == "venus" || bodyId == "earth" ||
               bodyId == "moon" || bodyId == "mars" || bodyId == "jupiter" || bodyId == "saturn" ||
               bodyId == "uranus" || bodyId == "neptune";
    }
    static bool isValidGlbFile(const std::filesystem::path& path);

private:
    struct LoadedAsset {
        PlanetPresentation presentation;
        Model model{};
        float radius = 0.0f;
        Vector3 center{};
    };

    std::filesystem::path planetDirectory;
    std::unordered_map<std::string, PlanetPresentation> metadata;
    std::unordered_map<std::string, LoadedAsset> loaded;
};

} // namespace bag
