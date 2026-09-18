#include <cassert>
#include <cmath>
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
    assert(earth.value->rotationPeriodS > 86000.0 && earth.value->rotationPeriodS < 87000.0); // ~24 h
    assert(earth.value->axialTiltDeg > 23.0 && earth.value->axialTiltDeg < 24.0);

    // Educational rotation table: day length, tilt, and retrograde/sideways sense.
    const auto mercury = loader.loadBodyDefinition("mercury");
    assert(mercury);
    assert(mercury.value->rotationPeriodS > 5.0e6 && mercury.value->rotationPeriodS < 5.2e6); // ~59 d
    assert(mercury.value->axialTiltDeg < 1.0); // nearly upright

    const auto venus = loader.loadBodyDefinition("venus");
    assert(venus);
    assert(venus.value->rotationPeriodS < 0.0); // retrograde
    assert(std::abs(venus.value->rotationPeriodS) > 2.0e7); // ~243 Earth days
    assert(venus.value->axialTiltDeg > 170.0);

    const auto mars = loader.loadBodyDefinition("mars");
    assert(mars);
    assert(mars.value->rotationPeriodS > 88000.0 && mars.value->rotationPeriodS < 90000.0); // ~24.6 h
    assert(mars.value->axialTiltDeg > 24.0 && mars.value->axialTiltDeg < 26.0);

    const auto jupiter = loader.loadBodyDefinition("jupiter");
    assert(jupiter);
    assert(jupiter.value->rotationPeriodS > 35000.0 && jupiter.value->rotationPeriodS < 37000.0); // ~10 h

    const auto saturn = loader.loadBodyDefinition("saturn");
    assert(saturn);
    assert(saturn.value->rotationPeriodS > 37000.0 && saturn.value->rotationPeriodS < 40000.0); // ~10.7 h
    assert(saturn.value->axialTiltDeg > 25.0 && saturn.value->axialTiltDeg < 28.0);

    const auto uranus = loader.loadBodyDefinition("uranus");
    assert(uranus);
    assert(uranus.value->rotationPeriodS < 0.0);
    assert(std::abs(uranus.value->rotationPeriodS) > 60000.0 && std::abs(uranus.value->rotationPeriodS) < 65000.0); // ~17 h
    assert(uranus.value->axialTiltDeg > 95.0 && uranus.value->axialTiltDeg < 100.0); // sideways

    const auto neptune = loader.loadBodyDefinition("neptune");
    assert(neptune);
    assert(neptune.value->rotationPeriodS > 57000.0 && neptune.value->rotationPeriodS < 59000.0); // ~16 h
    assert(neptune.value->axialTiltDeg > 27.0 && neptune.value->axialTiltDeg < 30.0);

    const auto defaultScenario = loader.loadScenario("default_solar_system", rng);
    assert(defaultScenario);
    assert(defaultScenario.value->bodies.size() == 11);
    assert(defaultScenario.value->metadata.referenceFrame == "heliocentric");

    const auto earthScenario = loader.loadScenario("earth_orbit", rng);
    assert(earthScenario);
    assert(earthScenario.value->bodies.size() == 3);

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
