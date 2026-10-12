#include "PlanetAssetManager.hpp"

#include <algorithm>
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

bool PlanetAssetManager::load(bool fast) {
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
        // Fast startup defers every GLB; the main loop uploads one model per frame.
        if (fast) {
            logInfo("Fast startup: deferring planet model for " + bodyId);
            continue;
        }
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
            UnloadModel(loadedModel);
            logError("raylib could not load planet model for " + bodyId + "; using procedural sphere");
            continue;
        }
        if (bodyId == "jupiter" && loadedModel.meshCount > 1) {
            // The source export contains one planet mesh followed by large
            // transparent ring/overlay meshes. They dominate the bounds and
            // make the planet render as a tiny dot at its presentation scale.
            loadedModel.meshCount = 1;
            logInfo("Jupiter presentation: using planet mesh 0; excluding overlay meshes");
            for (int materialIndex = 0; materialIndex < loadedModel.materialCount; ++materialIndex) {
                logInfo("Jupiter material " + std::to_string(materialIndex) +
                        ": albedoTexture=" +
                        std::to_string(loadedModel.materials[materialIndex]
                                           .maps[MATERIAL_MAP_ALBEDO].texture.id) +
                        " diffuseTexture=" +
                        std::to_string(loadedModel.materials[materialIndex]
                                           .maps[MATERIAL_MAP_DIFFUSE].texture.id));
            }
            // raylib keeps a default material in slot 0 for this converted
            // asset; the surface texture is loaded in slot 1.
            if (loadedModel.meshMaterial != nullptr && loadedModel.materialCount > 1) {
                loadedModel.meshMaterial[0] = 1;
            }
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

std::size_t PlanetAssetManager::totalEnabledCount() const {
    return static_cast<std::size_t>(std::count_if(metadata.begin(), metadata.end(), [](const auto& entry) { return entry.second.enabled; }));
}

bool PlanetAssetManager::loadNext() {
    for (const auto& [bodyId, item] : metadata) {
        if (!item.enabled || loaded.find(bodyId) != loaded.end()) continue;
        const auto path = planetDirectory / item.modelFile;
        if (!isValidGlbFile(path)) { logError("Deferred planet model unavailable or invalid for " + bodyId); continue; }
        Model model = LoadModel(path.string().c_str());
        if (model.meshCount <= 0 || model.meshes == nullptr) {
            UnloadModel(model);
            logError("Deferred raylib model load failed for " + bodyId);
            continue;
        }
        const BoundingBox bounds = GetModelBoundingBox(model);
        const Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), 0.5f);
        const float radius = Vector3Length(extent);
        if (!std::isfinite(radius) || radius <= std::numeric_limits<float>::epsilon()) {
            UnloadModel(model);
            logError("Deferred planet model has invalid bounds for " + bodyId);
            continue;
        }
        loaded.emplace(bodyId, LoadedAsset{item, model, radius, center});
        logInfo("Loaded deferred planet model: " + bodyId);
        return true;
    }
    return false;
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

void PlanetAssetManager::setShader(const std::string& bodyId, Shader shader) {
    const auto found = loaded.find(bodyId);
    if (found == loaded.end()) return;
    Model& model = found->second.model;
    for (int index = 0; index < model.materialCount; ++index) model.materials[index].shader = shader;
}

void PlanetAssetManager::unload() {
    for (auto& [bodyId, asset] : loaded) {
        (void)bodyId;
        UnloadModel(asset.model);
    }
    loaded.clear();
}

} // namespace bag
