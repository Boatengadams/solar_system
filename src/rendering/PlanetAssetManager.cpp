#include "PlanetAssetManager.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <cmath>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <raymath.h>

#include "../core/Logger.hpp"

namespace bag {
namespace {

bool finiteVector(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 jsonVector(const nlohmann::json& value, const char* field) {
    if (!value.contains(field)) return {};
    const auto& array = value.at(field);
    if (!array.is_array() || array.size() != 3) throw std::runtime_error(std::string(field) + " must contain three numbers");
    return {array.at(0).get<float>(), array.at(1).get<float>(), array.at(2).get<float>()};
}

} // namespace

PlanetAssetManager::PlanetAssetManager(std::filesystem::path directory) : planetDirectory(std::move(directory)) {}

bool PlanetAssetManager::isValidGlbFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 12> header{};
    if (!input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()))) return false;
    const auto valueAt = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(header[offset]) |
               (static_cast<std::uint32_t>(header[offset + 1]) << 8U) |
               (static_cast<std::uint32_t>(header[offset + 2]) << 16U) |
               (static_cast<std::uint32_t>(header[offset + 3]) << 24U);
    };
    constexpr std::uint32_t GLB_MAGIC = 0x46546C67U;
    return valueAt(0) == GLB_MAGIC && valueAt(4) == 2U && valueAt(8) >= 20U;
}

bool PlanetAssetManager::load() {
    unload();
    metadata.clear();
    const auto manifestPath = planetDirectory / "manifest.json";
    std::ifstream input(manifestPath);
    if (!input) {
        logInfo("Planet assets unavailable; using procedural presentation");
        return false;
    }

    try {
        nlohmann::json manifest;
        input >> manifest;
        if (manifest.value("format", "") != "bagsolar_planet_presentation_v1" || !manifest.contains("models") ||
            !manifest.at("models").is_array()) {
            logError("Planet asset manifest is not a BAGSOLAR presentation manifest; using procedural presentation");
            return false;
        }
        for (const auto& entry : manifest.at("models")) {
            PlanetPresentation item;
            item.bodyId = entry.at("body_id").get<std::string>();
            item.modelFile = entry.at("model").get<std::string>();
            item.enabled = entry.value("enabled", false);
            item.fallbackReason = entry.value("disabled_reason", "presentation fallback selected");
            item.visualScale = entry.value("visual_scale", 1.0f);
            item.localRotationDegrees = jsonVector(entry, "local_rotation_degrees");
            item.localTranslation = jsonVector(entry, "local_translation");
            item.modelIncludesRings = entry.value("model_includes_rings", false);
            if (!isSupportedBodyId(item.bodyId) || item.modelFile.empty() || item.modelFile.find("..") != std::string::npos ||
                !std::isfinite(item.visualScale) || item.visualScale <= 0.0f || !finiteVector(item.localRotationDegrees) ||
                !finiteVector(item.localTranslation)) {
                logError("Ignoring invalid planet presentation manifest entry: " + item.bodyId);
                continue;
            }
            metadata[item.bodyId] = item;
        }
    } catch (const std::exception& error) {
        logError(std::string("Unable to parse planet asset manifest: ") + error.what());
        metadata.clear();
        return false;
    }

    for (const auto& [bodyId, item] : metadata) {
        if (!item.enabled) {
            logInfo("Planet model disabled for " + bodyId + ": " + item.fallbackReason);
            continue;
        }
        const auto path = planetDirectory / item.modelFile;
        if (!isValidGlbFile(path)) {
            logError("Planet model unavailable or invalid for " + bodyId + "; using procedural sphere");
            continue;
        }
        Model loadedModel = LoadModel(path.string().c_str());
        if (loadedModel.meshCount <= 0 || loadedModel.meshes == nullptr) {
            logError("raylib could not load planet model for " + bodyId + "; using procedural sphere");
            continue;
        }
        const BoundingBox bounds = GetModelBoundingBox(loadedModel);
        const Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), 0.5f);
        const float radius = Vector3Length(extent);
        if (!std::isfinite(radius) || radius <= std::numeric_limits<float>::epsilon()) {
            UnloadModel(loadedModel);
            logError("Planet model has invalid bounds for " + bodyId + "; using procedural sphere");
            continue;
        }
        loaded.emplace(bodyId, LoadedAsset{item, loadedModel, radius, center});
        logInfo("Loaded planet model: " + bodyId);
    }
    return !loaded.empty();
}

bool PlanetAssetManager::has(const std::string& bodyId) const { return loaded.find(bodyId) != loaded.end(); }

const PlanetPresentation* PlanetAssetManager::presentation(const std::string& bodyId) const {
    const auto found = loaded.find(bodyId);
    return found == loaded.end() ? nullptr : &found->second.presentation;
}

const Model* PlanetAssetManager::model(const std::string& bodyId) const {
    const auto found = loaded.find(bodyId);
    return found == loaded.end() ? nullptr : &found->second.model;
}

float PlanetAssetManager::modelRadius(const std::string& bodyId) const {
    const auto found = loaded.find(bodyId);
    return found == loaded.end() ? 0.0f : found->second.radius;
}

Vector3 PlanetAssetManager::modelCenter(const std::string& bodyId) const {
    const auto found = loaded.find(bodyId);
    return found == loaded.end() ? Vector3{} : found->second.center;
}

void PlanetAssetManager::unload() {
    for (auto& [bodyId, asset] : loaded) {
        (void)bodyId;
        UnloadModel(asset.model);
    }
    loaded.clear();
}

} // namespace bag
