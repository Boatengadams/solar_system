#pragma once

#include <string>

namespace bag {

struct SimulationSettings {
    double timestepSeconds = 3600.0;
    bool adaptiveTimestep = false;
    double minimumTimestepSeconds = 1.0;
    double maximumTimestepSeconds = 86400.0;
    double timestepErrorTolerance = 1.0e-8;
    double minimumSafeSeparationMeters = 1.0e7;
    double timeScale = 1.0;
    std::string integrator = "velocity_verlet";
    bool trailsEnabled = true;
    bool vectorsEnabled = false;
    bool labelsEnabled = false;
    std::string referenceFrame = "heliocentric";
};

struct ScenarioMetadata {
    std::string id;
    std::string name;
    std::string description;
    std::string epoch;
    std::string referenceFrame;
    std::string lesson;
    std::string experiment;
};

} // namespace bag
