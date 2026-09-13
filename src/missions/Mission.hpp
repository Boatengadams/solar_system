#pragma once

#include <string>
#include <vector>

#include "../physics/Body.hpp"
#include "../physics/PhysicsEngine.hpp"
#include "../spacecraft/Spacecraft.hpp"

namespace bag {

struct ManeuverNode {
    double epochSeconds = 0.0;
    double radialDeltaVMps = 0.0;
    double progradeDeltaVMps = 0.0;
    double normalDeltaVMps = 0.0;
    double burnDurationSeconds = 0.0;

    double totalDeltaVMps() const;
    Vec3 deltaVelocityMps() const { return {radialDeltaVMps, progradeDeltaVMps, normalDeltaVMps}; }
};

enum class MissionObjective { Orbit, Escape, HohmannTransfer, GravityAssist };

struct Mission {
    std::string id;
    std::string name;
    std::string originBodyId;
    std::string destinationBodyId;
    double departureEpochSeconds = 0.0;
    double arrivalEpochSeconds = 0.0;
    Spacecraft spacecraft;
    MissionObjective objective = MissionObjective::Orbit;
    std::vector<ManeuverNode> maneuvers;
};

struct MissionAnalysis {
    bool valid = false;
    double totalDeltaVMps = 0.0;
    double propellantConsumedKg = 0.0;
    double timeOfFlightSeconds = 0.0;
    std::string message;
};

MissionAnalysis analyzeMission(const Mission& mission);
MissionAnalysis addHohmannTransfer(Mission& mission, double innerRadiusM, double outerRadiusM,
                                   double centralMassKg = PhysicsEngine::SOLAR_MASS);

struct GravityAssistResult {
    bool valid = false;
    double turnAngleRadians = 0.0;
    double incomingRelativeSpeedMps = 0.0;
    double outgoingRelativeSpeedMps = 0.0;
    std::string message;
};

GravityAssistResult gravityAssistTurn(double gravitationalParameter,
                                      double periapsisRadiusM,
                                      double incomingRelativeSpeedMps);

} // namespace bag
