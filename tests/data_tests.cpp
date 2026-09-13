#include <cassert>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "data/BodyFactory.hpp"
#include "data/ScenarioLoader.hpp"
#include "data/ScenarioSerializer.hpp"

namespace {

void writeText(const std::filesystem::path& path, const std::string& value) {
    std::ofstream output(path);
    assert(output);
    output << value;
}

} // namespace

int main() {
    using namespace bag;
    const std::filesystem::path dataRoot = std::filesystem::path(BAGSOLAR_SOURCE_DIR) / "data";
    std::mt19937 rng{1234};
    ScenarioLoader loader(dataRoot);

    const auto earth = loader.loadBodyDefinition("earth");
    assert(earth);
    assert(earth.value->id == "earth");
    assert(earth.value->massKg > 5.9e24 && earth.value->massKg < 6.1e24);
    assert(earth.value->radiusM > 6.3e6 && earth.value->radiusM < 6.5e6);

    const auto defaultScenario = loader.loadScenario("default_solar_system", rng);
    assert(defaultScenario);
    assert(defaultScenario.value->bodies.size() == 10);
    assert(defaultScenario.value->metadata.referenceFrame == "heliocentric");

    const auto earthScenario = loader.loadScenario("earth_orbit", rng);
    assert(earthScenario);
    assert(earthScenario.value->bodies.size() == 2);

    const auto emptyScenario = loader.loadScenario("empty_space", rng);
    assert(emptyScenario);
    assert(emptyScenario.value->bodies.empty());

    const auto missingBody = loader.loadBodyDefinition("does-not-exist");
    assert(!missingBody);

    CustomBodyData custom{"custom", "Custom Body", "Planet", 1.0e20, 1.0e6, {1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    assert(BodyFactory::createCustom(custom));
    custom.massKg = -1.0;
    assert(!BodyFactory::createCustom(custom));

    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "bagsolar_phase2_data_tests";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "bodies");
    std::filesystem::create_directories(tempRoot / "scenarios");
    writeText(tempRoot / "bodies" / "malformed.json", "{not valid json");
    writeText(tempRoot / "bodies" / "missing_mass.json", R"({
        "id": "missing_mass", "name": "Missing Mass", "type": "Planet",
        "radius_m": 1.0, "initial_position_m": [0, 0, 0], "initial_velocity_mps": [0, 0, 0]
    })");
    writeText(tempRoot / "bodies" / "invalid_mass.json", R"({
        "id": "invalid_mass", "name": "Invalid Mass", "type": "Planet", "mass_kg": -1,
        "radius_m": 1.0, "initial_position_m": [0, 0, 0], "initial_velocity_mps": [0, 0, 0]
    })");
    writeText(tempRoot / "scenarios" / "unknown_reference.json", R"({
        "id": "unknown_reference", "name": "Unknown", "description": "Invalid", "epoch": "J2000",
        "reference_frame": "heliocentric", "body_references": ["missing"],
        "settings": {"timestep_s": 3600, "time_scale": 1, "integrator": "velocity_verlet"}
    })");

    ScenarioLoader invalidLoader(tempRoot);
    assert(!invalidLoader.loadBodyDefinition("malformed"));
    assert(!invalidLoader.loadBodyDefinition("missing_mass"));
    assert(!invalidLoader.loadBodyDefinition("invalid_mass"));
    assert(!invalidLoader.loadScenario("unknown_reference", rng));

    SimulationSnapshot snapshot;
    snapshot.metadata = defaultScenario.value->metadata;
    snapshot.settings = defaultScenario.value->settings;
    snapshot.simulationTime = 12345.0;
    snapshot.bodies = defaultScenario.value->bodies;
    const std::filesystem::path snapshotPath = tempRoot / "snapshot.json";
    assert(ScenarioSerializer::save(snapshotPath, snapshot).success);
    const auto restored = ScenarioSerializer::load(snapshotPath);
    assert(restored);
    assert(restored.value->simulationTime == snapshot.simulationTime);
    assert(restored.value->bodies.size() == snapshot.bodies.size());
    assert(restored.value->bodies[3].id == "earth");

    std::filesystem::remove_all(tempRoot);
    return 0;
}
