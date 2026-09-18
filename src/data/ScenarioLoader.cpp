#include "ScenarioLoader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <set>

#include <nlohmann/json.hpp>

#include "../physics/PhysicsEngine.hpp"

namespace bag {
namespace {

using Json = nlohmann::json;

DataResult<Json> readJson(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return DataResult<Json>::failure("unable to open JSON file: " + path.string());
    try {
        Json document;
        input >> document;
        return DataResult<Json>::success(std::move(document));
    } catch (const Json::parse_error& error) {
        return DataResult<Json>::failure("malformed JSON in " + path.string() + ": " + error.what());
    } catch (const Json::exception& error) {
        return DataResult<Json>::failure("invalid JSON in " + path.string() + ": " + error.what());
    }
}

DataResult<std::string> requiredString(const Json& object, const char* field, const std::string& context) {
    if (!object.contains(field) || !object[field].is_string() || object[field].get<std::string>().empty()) {
        return DataResult<std::string>::failure(context + " requires non-empty string field '" + field + "'");
    }
    return DataResult<std::string>::success(object[field].get<std::string>());
}

DataResult<double> requiredNumber(const Json& object, const char* field, const std::string& context) {
    if (!object.contains(field) || !object[field].is_number()) {
        return DataResult<double>::failure(context + " requires numeric field '" + field + "'");
    }
    const double value = object[field].get<double>();
    if (!std::isfinite(value)) return DataResult<double>::failure(context + " field '" + field + "' must be finite");
    return DataResult<double>::success(value);
}

DataResult<Vec3> requiredVector(const Json& object, const char* field, const std::string& context) {
    if (!object.contains(field) || !object[field].is_array() || object[field].size() != 3) {
        return DataResult<Vec3>::failure(context + " requires 3-element numeric array '" + field + "'");
    }
    Vec3 value;
    for (int index = 0; index < 3; ++index) {
        if (!object[field][index].is_number()) return DataResult<Vec3>::failure(context + " field '" + field + "' must contain numbers");
        (&value.x)[index] = object[field][index].get<double>();
        if (!std::isfinite((&value.x)[index])) return DataResult<Vec3>::failure(context + " field '" + field + "' must be finite");
    }
    return DataResult<Vec3>::success(value);
}

ColorRGBA colorOrDefault(const Json& object, const char* field, ColorRGBA fallback) {
    if (!object.contains(field) || !object[field].is_array() || object[field].size() != 4) return fallback;
    ColorRGBA value{};
    for (int index = 0; index < 4; ++index) {
        if (!object[field][index].is_number_integer()) return fallback;
        const int component = object[field][index].get<int>();
        if (component < 0 || component > 255) return fallback;
        (&value.r)[index] = static_cast<std::uint8_t>(component);
    }
    return value;
}

} // namespace

ScenarioLoader::ScenarioLoader(std::filesystem::path root) : dataRoot(std::move(root)) {}

DataResult<BodyDefinition> ScenarioLoader::loadBodyDefinition(const std::string& bodyId) const {
    const std::filesystem::path path = dataRoot / "bodies" / (bodyId + ".json");
    const DataResult<Json> document = readJson(path);
    if (!document) return DataResult<BodyDefinition>::failure(document.error);
    const auto& json = *document.value;
    if (!json.is_object()) return DataResult<BodyDefinition>::failure("body file must contain a JSON object: " + path.string());

    try {
    const std::string context = "body file " + path.string();
    BodyDefinition definition;
    auto id = requiredString(json, "id", context); if (!id) return DataResult<BodyDefinition>::failure(id.error); definition.id = *id.value;
    auto name = requiredString(json, "name", context); if (!name) return DataResult<BodyDefinition>::failure(name.error); definition.name = *name.value;
    auto type = requiredString(json, "type", context); if (!type) return DataResult<BodyDefinition>::failure(type.error); definition.type = *type.value;
    auto mass = requiredNumber(json, "mass_kg", context); if (!mass) return DataResult<BodyDefinition>::failure(mass.error); definition.massKg = *mass.value;
    auto radius = requiredNumber(json, "radius_m", context); if (!radius) return DataResult<BodyDefinition>::failure(radius.error); definition.radiusM = *radius.value;
    if (definition.massKg <= 0.0) return DataResult<BodyDefinition>::failure(context + " field 'mass_kg' must be positive");
    if (definition.radiusM <= 0.0) return DataResult<BodyDefinition>::failure(context + " field 'radius_m' must be positive");
    auto position = requiredVector(json, "initial_position_m", context); if (!position) return DataResult<BodyDefinition>::failure(position.error); definition.initialPositionM = *position.value;
    auto velocity = requiredVector(json, "initial_velocity_mps", context); if (!velocity) return DataResult<BodyDefinition>::failure(velocity.error); definition.initialVelocityMps = *velocity.value;

    if (json.contains("parent_id")) {
        if (!json["parent_id"].is_string()) return DataResult<BodyDefinition>::failure(context + " field 'parent_id' must be a string");
        definition.parentId = json["parent_id"].get<std::string>();
    }
    if (json.contains("visual")) {
        if (!json["visual"].is_object()) return DataResult<BodyDefinition>::failure(context + " field 'visual' must be an object");
        const auto& visual = json["visual"];
        definition.color = colorOrDefault(visual, "color_rgba", definition.color);
        definition.accent = colorOrDefault(visual, "accent_rgba", definition.accent);
        if (visual.contains("display_radius_px")) {
            if (!visual["display_radius_px"].is_number() || visual["display_radius_px"].get<double>() <= 0.0) return DataResult<BodyDefinition>::failure(context + " visual.display_radius_px must be positive");
            definition.displayRadiusPx = visual["display_radius_px"].get<double>();
        }
        if (visual.contains("luminous")) {
            if (!visual["luminous"].is_boolean()) return DataResult<BodyDefinition>::failure(context + " visual.luminous must be boolean");
            definition.luminous = visual["luminous"].get<bool>();
        }
        if (visual.contains("ringed")) {
            if (!visual["ringed"].is_boolean()) return DataResult<BodyDefinition>::failure(context + " visual.ringed must be boolean");
            definition.ringed = visual["ringed"].get<bool>();
        }
    }
    if (json.contains("orbital")) {
        if (!json["orbital"].is_object()) return DataResult<BodyDefinition>::failure(context + " field 'orbital' must be an object");
        const auto& orbital = json["orbital"];
        if (orbital.contains("semi_major_axis_m")) definition.semiMajorAxisM = orbital["semi_major_axis_m"].get<double>();
        if (orbital.contains("eccentricity")) definition.eccentricity = orbital["eccentricity"].get<double>();
        if (orbital.contains("orbital_period_s")) definition.orbitalPeriodS = orbital["orbital_period_s"].get<double>();
        if (!std::isfinite(definition.semiMajorAxisM) || !std::isfinite(definition.eccentricity) || !std::isfinite(definition.orbitalPeriodS)) return DataResult<BodyDefinition>::failure(context + " orbital values must be finite");
    }
    if (json.contains("rotation")) {
        if (!json["rotation"].is_object()) return DataResult<BodyDefinition>::failure(context + " field 'rotation' must be an object");
        const auto& rotation = json["rotation"];
        if (rotation.contains("sidereal_period_s")) {
            if (!rotation["sidereal_period_s"].is_number()) return DataResult<BodyDefinition>::failure(context + " rotation.sidereal_period_s must be a number");
            definition.rotationPeriodS = rotation["sidereal_period_s"].get<double>();
        }
        if (rotation.contains("axial_tilt_deg")) {
            if (!rotation["axial_tilt_deg"].is_number()) return DataResult<BodyDefinition>::failure(context + " rotation.axial_tilt_deg must be a number");
            definition.axialTiltDeg = rotation["axial_tilt_deg"].get<double>();
        }
        if (!std::isfinite(definition.rotationPeriodS) || definition.rotationPeriodS == 0.0) {
            return DataResult<BodyDefinition>::failure(context + " rotation.sidereal_period_s must be finite and non-zero");
        }
        if (!std::isfinite(definition.axialTiltDeg)) {
            return DataResult<BodyDefinition>::failure(context + " rotation.axial_tilt_deg must be finite");
        }
    }
    return DataResult<BodyDefinition>::success(std::move(definition));
    } catch (const Json::exception& error) {
        return DataResult<BodyDefinition>::failure("invalid body JSON in " + path.string() + ": " + error.what());
    } catch (const std::exception& error) {
        return DataResult<BodyDefinition>::failure("invalid body data in " + path.string() + ": " + error.what());
    }
}

DataResult<ScenarioMetadata> ScenarioLoader::parseScenario(const std::filesystem::path& path,
                                                            std::vector<std::string>& bodyIds,
                                                            SimulationSettings& settings) const {
    const DataResult<Json> document = readJson(path);
    if (!document) return DataResult<ScenarioMetadata>::failure(document.error);
    const auto& json = *document.value;
    if (!json.is_object()) return DataResult<ScenarioMetadata>::failure("scenario file must contain a JSON object: " + path.string());
    try {
    const std::string context = "scenario file " + path.string();
    ScenarioMetadata metadata;
    auto id = requiredString(json, "id", context); if (!id) return DataResult<ScenarioMetadata>::failure(id.error); metadata.id = *id.value;
    auto name = requiredString(json, "name", context); if (!name) return DataResult<ScenarioMetadata>::failure(name.error); metadata.name = *name.value;
    auto description = requiredString(json, "description", context); if (!description) return DataResult<ScenarioMetadata>::failure(description.error); metadata.description = *description.value;
    auto epoch = requiredString(json, "epoch", context); if (!epoch) return DataResult<ScenarioMetadata>::failure(epoch.error); metadata.epoch = *epoch.value;
    auto frame = requiredString(json, "reference_frame", context); if (!frame) return DataResult<ScenarioMetadata>::failure(frame.error); metadata.referenceFrame = *frame.value;
    if (!json.contains("body_references") || !json["body_references"].is_array()) return DataResult<ScenarioMetadata>::failure(context + " requires array field 'body_references'");
    std::set<std::string> uniqueIds;
    for (const auto& reference : json["body_references"]) {
        if (!reference.is_string() || reference.get<std::string>().empty()) return DataResult<ScenarioMetadata>::failure(context + " body_references must contain non-empty strings");
        bodyIds.push_back(reference.get<std::string>());
        if (!uniqueIds.insert(bodyIds.back()).second) return DataResult<ScenarioMetadata>::failure(context + " contains duplicate body reference '" + bodyIds.back() + "'");
    }
    if (!json.contains("settings") || !json["settings"].is_object()) return DataResult<ScenarioMetadata>::failure(context + " requires object field 'settings'");
    const auto& settingsJson = json["settings"];
    auto timestep = requiredNumber(settingsJson, "timestep_s", context + ".settings"); if (!timestep) return DataResult<ScenarioMetadata>::failure(timestep.error); settings.timestepSeconds = *timestep.value;
    auto scale = requiredNumber(settingsJson, "time_scale", context + ".settings"); if (!scale) return DataResult<ScenarioMetadata>::failure(scale.error); settings.timeScale = *scale.value;
    auto integrator = requiredString(settingsJson, "integrator", context + ".settings"); if (!integrator) return DataResult<ScenarioMetadata>::failure(integrator.error); settings.integrator = *integrator.value;
    if (settings.timestepSeconds <= 0.0 || settings.timeScale <= 0.0) return DataResult<ScenarioMetadata>::failure(context + " settings timestep_s and time_scale must be positive");
    if (settingsJson.contains("adaptive_timestep")) settings.adaptiveTimestep = settingsJson["adaptive_timestep"].get<bool>();
    if (settingsJson.contains("minimum_timestep_s")) settings.minimumTimestepSeconds = settingsJson["minimum_timestep_s"].get<double>();
    if (settingsJson.contains("maximum_timestep_s")) settings.maximumTimestepSeconds = settingsJson["maximum_timestep_s"].get<double>();
    if (settingsJson.contains("timestep_error_tolerance")) settings.timestepErrorTolerance = settingsJson["timestep_error_tolerance"].get<double>();
    if (settingsJson.contains("minimum_safe_separation_m")) settings.minimumSafeSeparationMeters = settingsJson["minimum_safe_separation_m"].get<double>();
    if (settings.minimumTimestepSeconds <= 0.0 || settings.maximumTimestepSeconds < settings.minimumTimestepSeconds ||
        settings.timestepErrorTolerance <= 0.0 || settings.minimumSafeSeparationMeters <= 0.0) {
        return DataResult<ScenarioMetadata>::failure(context + " contains invalid adaptive timestep or close-approach settings");
    }
    Integrator parsedIntegrator;
    if (!parseIntegrator(settings.integrator, parsedIntegrator)) return DataResult<ScenarioMetadata>::failure(context + " uses unsupported integrator '" + settings.integrator + "'");
    settings.referenceFrame = metadata.referenceFrame;
    if (settingsJson.contains("trails_enabled")) settings.trailsEnabled = settingsJson["trails_enabled"].get<bool>();
    if (settingsJson.contains("vectors_enabled")) settings.vectorsEnabled = settingsJson["vectors_enabled"].get<bool>();
    if (settingsJson.contains("labels_enabled")) settings.labelsEnabled = settingsJson["labels_enabled"].get<bool>();
    if (settingsJson.contains("selection_highlight_enabled")) settings.selectionHighlightEnabled = settingsJson["selection_highlight_enabled"].get<bool>();
    if (json.contains("educational") && json["educational"].is_object()) {
        if (json["educational"].contains("lesson")) metadata.lesson = json["educational"]["lesson"].get<std::string>();
        if (json["educational"].contains("experiment")) metadata.experiment = json["educational"]["experiment"].get<std::string>();
    }
    return DataResult<ScenarioMetadata>::success(std::move(metadata));
    } catch (const Json::exception& error) {
        return DataResult<ScenarioMetadata>::failure("invalid scenario JSON in " + path.string() + ": " + error.what());
    } catch (const std::exception& error) {
        return DataResult<ScenarioMetadata>::failure("invalid scenario data in " + path.string() + ": " + error.what());
    }
}

DataResult<LoadedScenario> ScenarioLoader::loadScenario(const std::string& scenarioId, std::mt19937& rng) const {
    return loadScenarioFile(dataRoot / "scenarios" / (scenarioId + ".json"), rng);
}

DataResult<LoadedScenario> ScenarioLoader::loadScenarioFile(const std::filesystem::path& scenarioPath, std::mt19937& rng) const {
    std::vector<std::string> bodyIds;
    SimulationSettings settings;
    auto metadata = parseScenario(scenarioPath, bodyIds, settings);
    if (!metadata) return DataResult<LoadedScenario>::failure(metadata.error);
    LoadedScenario loaded;
    loaded.metadata = *metadata.value;
    loaded.settings = settings;
    loaded.stars = createStars(rng);
    for (const std::string& bodyId : bodyIds) {
        auto definition = loadBodyDefinition(bodyId);
        if (!definition) return DataResult<LoadedScenario>::failure("scenario '" + loaded.metadata.id + "': " + definition.error);
        auto body = BodyFactory::create(*definition.value);
        if (!body) return DataResult<LoadedScenario>::failure("scenario '" + loaded.metadata.id + "': " + body.error);
        loaded.bodies.push_back(std::move(*body.value));
    }
    for (Body& body : loaded.bodies) {
        if (!body.parentId.empty()) {
            auto parent = std::find_if(loaded.bodies.begin(), loaded.bodies.end(), [&](const Body& candidate) { return candidate.id == body.parentId; });
            if (parent == loaded.bodies.end()) return DataResult<LoadedScenario>::failure("scenario '" + loaded.metadata.id + "': unknown parent body '" + body.parentId + "'");
            body.parent = static_cast<int>(std::distance(loaded.bodies.begin(), parent));
        }
    }
    return DataResult<LoadedScenario>::success(std::move(loaded));
}

std::vector<Star> ScenarioLoader::createStars(std::mt19937& rng) {
    std::vector<Star> stars;
    std::uniform_real_distribution<float> position(-6000.0f, 6000.0f);
    std::uniform_real_distribution<float> radius(0.4f, 1.8f);
    std::uniform_real_distribution<float> brightness(0.3f, 1.0f);
    for (int i = 0; i < 1500; ++i) stars.push_back({{position(rng), position(rng)}, radius(rng), brightness(rng)});
    return stars;
}

} // namespace bag
