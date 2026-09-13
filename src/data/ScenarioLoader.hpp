#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "../core/Star.hpp"
#include "../physics/Body.hpp"
#include "../simulation/SimulationSettings.hpp"
#include "BodyFactory.hpp"

namespace bag {

struct LoadedScenario {
    ScenarioMetadata metadata;
    SimulationSettings settings;
    std::vector<Body> bodies;
    std::vector<Star> stars;
};

class ScenarioLoader {
public:
    explicit ScenarioLoader(std::filesystem::path dataRoot);

    DataResult<LoadedScenario> loadScenario(const std::string& scenarioId, std::mt19937& rng) const;
    DataResult<LoadedScenario> loadScenarioFile(const std::filesystem::path& scenarioPath, std::mt19937& rng) const;
    DataResult<BodyDefinition> loadBodyDefinition(const std::string& bodyId) const;

private:
    std::filesystem::path dataRoot;

    DataResult<ScenarioMetadata> parseScenario(const std::filesystem::path& path,
                                               std::vector<std::string>& bodyIds,
                                               SimulationSettings& settings) const;
    static std::vector<Star> createStars(std::mt19937& rng);
};

} // namespace bag
