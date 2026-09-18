#include "ScenarioSerializer.hpp"

#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

#include "BodyFactory.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {
namespace {

using Json = nlohmann::json;

Json vectorJson(Vec3 value) { return Json{value.x, value.y, value.z}; }
Json colorJson(ColorRGBA value) { return Json{value.r, value.g, value.b, value.a}; }

bool finiteVector(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Json bodyJson(const Body& body) {
    return {
        {"id", body.id}, {"name", body.name}, {"type", body.type},
        {"mass_kg", body.mass}, {"radius_m", body.realRadius},
        {"position_m", vectorJson(body.position)}, {"velocity_mps", vectorJson(body.velocity)},
        {"parent_id", body.parentId}, {"visual", {
            {"color_rgba", colorJson(body.color)}, {"accent_rgba", colorJson(body.accent)},
            {"display_radius_px", body.radius}, {"luminous", body.luminous}, {"ringed", body.ringed}
        }},
        {"orbital", {{"semi_major_axis_m", body.semiMajorAxis}, {"eccentricity", body.eccentricity},
                      {"orbital_period_s", body.orbitalPeriod}}},
        {"rotation", {{"sidereal_period_s", body.rotationPeriod}, {"axial_tilt_deg", body.axialTilt}}}
    };
}

DataResult<Vec3> vectorFromJson(const Json& object, const char* field) {
    if (!object.contains(field) || !object[field].is_array() || object[field].size() != 3) return DataResult<Vec3>::failure(std::string("snapshot body requires 3-element '") + field + "'");
    Vec3 value;
    for (int i = 0; i < 3; ++i) {
        if (!object[field][i].is_number()) return DataResult<Vec3>::failure(std::string("snapshot body field '") + field + "' must contain numbers");
        (&value.x)[i] = object[field][i].get<double>();
    }
    if (!finiteVector(value)) return DataResult<Vec3>::failure(std::string("snapshot body field '") + field + "' must be finite");
    return DataResult<Vec3>::success(value);
}

DataResult<ColorRGBA> colorFromJson(const Json& object, const char* field) {
    if (!object.contains(field) || !object[field].is_array() || object[field].size() != 4) return DataResult<ColorRGBA>::failure(std::string("snapshot visual requires 4-element '") + field + "'");
    ColorRGBA value{};
    for (int i = 0; i < 4; ++i) {
        if (!object[field][i].is_number_integer()) return DataResult<ColorRGBA>::failure(std::string("snapshot visual field '") + field + "' must contain integers");
        const int component = object[field][i].get<int>();
        if (component < 0 || component > 255) return DataResult<ColorRGBA>::failure(std::string("snapshot visual field '") + field + "' is outside RGBA range");
        (&value.r)[i] = static_cast<std::uint8_t>(component);
    }
    return DataResult<ColorRGBA>::success(value);
}

} // namespace

SaveResult ScenarioSerializer::save(const std::filesystem::path& path, const SimulationSnapshot& snapshot) {
    std::ofstream output(path);
    if (!output) return {false, "unable to open snapshot for writing: " + path.string()};
    Json document = {
        {"format", "bagsolar_simulation_snapshot_v1"},
        {"scenario", {{"id", snapshot.metadata.id}, {"name", snapshot.metadata.name},
                       {"description", snapshot.metadata.description}, {"epoch", snapshot.metadata.epoch},
                       {"reference_frame", snapshot.metadata.referenceFrame}}},
        {"simulation", {{"time_s", snapshot.simulationTime}, {"timestep_s", snapshot.settings.timestepSeconds},
        {"time_scale", snapshot.settings.timeScale}, {"integrator", snapshot.settings.integrator},
        {"adaptive_timestep", snapshot.settings.adaptiveTimestep},
        {"minimum_timestep_s", snapshot.settings.minimumTimestepSeconds},
        {"maximum_timestep_s", snapshot.settings.maximumTimestepSeconds},
        {"timestep_error_tolerance", snapshot.settings.timestepErrorTolerance},
        {"minimum_safe_separation_m", snapshot.settings.minimumSafeSeparationMeters},
                         {"trails_enabled", snapshot.settings.trailsEnabled}, {"vectors_enabled", snapshot.settings.vectorsEnabled},
                         {"labels_enabled", snapshot.settings.labelsEnabled},
                         {"selection_highlight_enabled", snapshot.settings.selectionHighlightEnabled}, {"paused", snapshot.paused},
                         {"show_orbits", snapshot.showOrbits}, {"show_trails", snapshot.showTrails},
                         {"show_vectors", snapshot.showVectors}, {"show_grid", snapshot.showGrid}}},
        {"bodies", Json::array()}
    };
    for (const Body& body : snapshot.bodies) document["bodies"].push_back(bodyJson(body));
    try {
        output << document.dump(2) << '\n';
    } catch (const Json::exception& error) {
        return {false, "unable to serialize snapshot: " + std::string(error.what())};
    }
    return {true, {}};
}

DataResult<SimulationSnapshot> ScenarioSerializer::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return DataResult<SimulationSnapshot>::failure("unable to open snapshot: " + path.string());
    try {
        Json document;
        input >> document;
        if (!document.is_object() || document.value("format", "") != "bagsolar_simulation_snapshot_v1") return DataResult<SimulationSnapshot>::failure("unsupported or missing snapshot format");
        const auto& scenario = document.at("scenario");
        const auto& simulation = document.at("simulation");
        if (!scenario.is_object() || !simulation.is_object() || !document.contains("bodies") || !document["bodies"].is_array()) return DataResult<SimulationSnapshot>::failure("snapshot requires scenario, simulation, and bodies sections");
        SimulationSnapshot snapshot;
        snapshot.metadata.id = scenario.at("id").get<std::string>();
        snapshot.metadata.name = scenario.at("name").get<std::string>();
        snapshot.metadata.description = scenario.at("description").get<std::string>();
        snapshot.metadata.epoch = scenario.at("epoch").get<std::string>();
        snapshot.metadata.referenceFrame = scenario.at("reference_frame").get<std::string>();
        snapshot.simulationTime = simulation.at("time_s").get<double>();
        snapshot.settings.timestepSeconds = simulation.at("timestep_s").get<double>();
        snapshot.settings.timeScale = simulation.at("time_scale").get<double>();
        snapshot.settings.integrator = simulation.at("integrator").get<std::string>();
        snapshot.settings.adaptiveTimestep = simulation.value("adaptive_timestep", false);
        snapshot.settings.minimumTimestepSeconds = simulation.value("minimum_timestep_s", 1.0);
        snapshot.settings.maximumTimestepSeconds = simulation.value("maximum_timestep_s", 86400.0);
        snapshot.settings.timestepErrorTolerance = simulation.value("timestep_error_tolerance", 1.0e-8);
        snapshot.settings.minimumSafeSeparationMeters = simulation.value("minimum_safe_separation_m", 1.0e7);
        snapshot.settings.trailsEnabled = simulation.value("trails_enabled", true);
        snapshot.settings.vectorsEnabled = simulation.value("vectors_enabled", false);
        snapshot.settings.labelsEnabled = simulation.value("labels_enabled", false);
        snapshot.settings.selectionHighlightEnabled = simulation.value("selection_highlight_enabled", true);
        snapshot.settings.referenceFrame = snapshot.metadata.referenceFrame;
        snapshot.paused = simulation.value("paused", false);
        snapshot.showOrbits = simulation.value("show_orbits", false);
        snapshot.showTrails = simulation.value("show_trails", true);
        snapshot.showVectors = simulation.value("show_vectors", false);
        snapshot.showGrid = simulation.value("show_grid", false);
        Integrator parsedIntegrator;
        if (!std::isfinite(snapshot.simulationTime) || snapshot.simulationTime < 0.0 || snapshot.settings.timestepSeconds <= 0.0 || snapshot.settings.timeScale <= 0.0 ||
            snapshot.settings.minimumTimestepSeconds <= 0.0 || snapshot.settings.maximumTimestepSeconds < snapshot.settings.minimumTimestepSeconds ||
            snapshot.settings.timestepErrorTolerance <= 0.0 || snapshot.settings.minimumSafeSeparationMeters <= 0.0 ||
            !parseIntegrator(snapshot.settings.integrator, parsedIntegrator)) return DataResult<SimulationSnapshot>::failure("snapshot contains invalid simulation settings");
        for (const auto& jsonBody : document["bodies"]) {
            BodyDefinition definition;
            definition.id = jsonBody.at("id").get<std::string>();
            definition.name = jsonBody.at("name").get<std::string>();
            definition.type = jsonBody.at("type").get<std::string>();
            definition.massKg = jsonBody.at("mass_kg").get<double>();
            definition.radiusM = jsonBody.at("radius_m").get<double>();
            auto position = vectorFromJson(jsonBody, "position_m"); if (!position) return DataResult<SimulationSnapshot>::failure(position.error); definition.initialPositionM = *position.value;
            auto velocity = vectorFromJson(jsonBody, "velocity_mps"); if (!velocity) return DataResult<SimulationSnapshot>::failure(velocity.error); definition.initialVelocityMps = *velocity.value;
            definition.parentId = jsonBody.value("parent_id", "");
            const auto& visual = jsonBody.at("visual");
            auto bodyColor = colorFromJson(visual, "color_rgba"); if (!bodyColor) return DataResult<SimulationSnapshot>::failure(bodyColor.error); definition.color = *bodyColor.value;
            auto accentColor = colorFromJson(visual, "accent_rgba"); if (!accentColor) return DataResult<SimulationSnapshot>::failure(accentColor.error); definition.accent = *accentColor.value;
            definition.displayRadiusPx = visual.at("display_radius_px").get<double>();
            definition.luminous = visual.value("luminous", false);
            definition.ringed = visual.value("ringed", false);
            const auto& orbital = jsonBody.at("orbital");
            definition.semiMajorAxisM = orbital.value("semi_major_axis_m", 0.0);
            definition.eccentricity = orbital.value("eccentricity", 0.0);
            definition.orbitalPeriodS = orbital.value("orbital_period_s", 0.0);
            if (jsonBody.contains("rotation") && jsonBody["rotation"].is_object()) {
                const auto& rotation = jsonBody["rotation"];
                definition.rotationPeriodS = rotation.value("sidereal_period_s", definition.rotationPeriodS);
                definition.axialTiltDeg = rotation.value("axial_tilt_deg", definition.axialTiltDeg);
            }
            auto body = BodyFactory::create(definition);
            if (!body) return DataResult<SimulationSnapshot>::failure(body.error);
            snapshot.bodies.push_back(std::move(*body.value));
        }
        return DataResult<SimulationSnapshot>::success(std::move(snapshot));
    } catch (const nlohmann::json::exception& error) {
        return DataResult<SimulationSnapshot>::failure("malformed snapshot JSON: " + std::string(error.what()));
    } catch (const std::exception& error) {
        return DataResult<SimulationSnapshot>::failure("invalid snapshot data: " + std::string(error.what()));
    }
}

} // namespace bag
