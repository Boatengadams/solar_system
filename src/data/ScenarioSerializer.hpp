#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "DataResult.hpp"
#include "../physics/Body.hpp"
#include "../simulation/SimulationSettings.hpp"

namespace bag {

struct SimulationSnapshot {
    ScenarioMetadata metadata;
    SimulationSettings settings;
    double simulationTime = 0.0;
    bool paused = false;
    bool showOrbits = true;
    bool showTrails = true;
    bool showVectors = false;
    bool showGrid = false;
    std::vector<Body> bodies;
};

struct SaveResult {
    bool success = false;
    std::string error;
};

class ScenarioSerializer {
public:
    static SaveResult save(const std::filesystem::path& path, const SimulationSnapshot& snapshot);
    static DataResult<SimulationSnapshot> load(const std::filesystem::path& path);
};

} // namespace bag
