#include <cassert>
#include <filesystem>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "rendering/PlanetAssetManager.hpp"
#include "rendering/Selection.hpp"

int main() {
    using namespace bag;
    const auto manifestPath = std::filesystem::path(BAGSOLAR_SOURCE_DIR) / "assets" / "planets" / "manifest.json";
    std::ifstream input(manifestPath);
    assert(input.good());
    nlohmann::json manifest;
    input >> manifest;
    assert(manifest.at("format") == "bagsolar_planet_presentation_v1");

    const std::set<std::string> expected = {
        "sun", "mercury", "venus", "earth", "moon", "mars", "jupiter", "saturn", "uranus", "neptune",
    };
    std::set<std::string> mapped;
    for (const auto& entry : manifest.at("models")) {
        const std::string id = entry.at("body_id").get<std::string>();
        assert(PlanetAssetManager::isSupportedBodyId(id));
        assert(entry.at("visual_scale").get<float>() > 0.0f);
        assert(entry.at("model").get<std::string>().find("..") == std::string::npos);
        if (!entry.at("enabled").get<bool>()) assert(!entry.at("disabled_reason").get<std::string>().empty());
        mapped.insert(id);
    }
    assert(mapped == expected);
    assert(PlanetAssetManager::isSupportedBodyId("moon"));
    assert(!PlanetAssetManager::isSupportedBodyId("pluto"));
    assert(!PlanetAssetManager::isSupportedBodyId("black-hole"));

    bool foundMoon = false;
    for (const auto& entry : manifest.at("models")) {
        if (entry.at("body_id").get<std::string>() != "moon") continue;
        assert(entry.at("model").get<std::string>() == "moon.glb");
        assert(entry.at("enabled").get<bool>());
        foundMoon = true;
    }
    if (!foundMoon) return 1;
    if (!std::filesystem::exists(std::filesystem::path(BAGSOLAR_SOURCE_DIR) / "assets" / "planets" / "moon.glb")) {
        return 1;
    }
    // Model selection uses the presentation extent, not a physical radius.
    assert(presentationSelectionRadius(0.5f) == 1.4f);
    assert(presentationSelectionRadius(4.0f) == 5.4f);
    return 0;
}
