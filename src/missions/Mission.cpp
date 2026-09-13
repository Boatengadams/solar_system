#include "Mission.hpp"

#include <cmath>

namespace bag {

double ManeuverNode::totalDeltaVMps() const {
    return length(deltaVelocityMps());
}

MissionAnalysis analyzeMission(const Mission& mission) {
    if (!mission.spacecraft.valid()) return {false, 0.0, 0.0, 0.0, "invalid spacecraft"};
    if (mission.arrivalEpochSeconds < mission.departureEpochSeconds) return {false, 0.0, 0.0, 0.0, "arrival precedes departure"};
    double totalDeltaV = 0.0;
    double propellant = 0.0;
    double remainingMass = mission.spacecraft.totalMassKg();
    const double exhaustVelocity = mission.spacecraft.specificImpulseS * Spacecraft::STANDARD_GRAVITY_MPS2;
    for (const ManeuverNode& node : mission.maneuvers) {
        if (!std::isfinite(node.epochSeconds) || node.epochSeconds < mission.departureEpochSeconds || node.epochSeconds > mission.arrivalEpochSeconds || node.burnDurationSeconds < 0.0) return {false, 0.0, 0.0, 0.0, "maneuver lies outside mission interval"};
        const double deltaV = node.totalDeltaVMps();
        const double required = remainingMass - remainingMass / std::exp(deltaV / exhaustVelocity);
        if (!std::isfinite(required)) return {false, 0.0, 0.0, 0.0, "maneuver delta-v is invalid"};
        totalDeltaV += deltaV;
        propellant += required;
        remainingMass -= required;
    }
    if (propellant > mission.spacecraft.propellantMassKg) return {false, totalDeltaV, propellant, mission.arrivalEpochSeconds - mission.departureEpochSeconds, "mission exceeds propellant budget"};
    return {true, totalDeltaV, propellant, mission.arrivalEpochSeconds - mission.departureEpochSeconds, {}};
}

MissionAnalysis addHohmannTransfer(Mission& mission, double innerRadiusM, double outerRadiusM, double centralMassKg) {
    const HohmannTransfer transfer = PhysicsEngine::hohmannTransfer(innerRadiusM, outerRadiusM, centralMassKg);
    if (!transfer.valid) return {false, 0.0, 0.0, 0.0, "invalid Hohmann transfer inputs"};
    mission.maneuvers.clear();
    mission.maneuvers.push_back({mission.departureEpochSeconds, 0.0, transfer.departureDeltaV, 0.0, 0.0});
    mission.maneuvers.push_back({mission.departureEpochSeconds + transfer.transferTime, 0.0, transfer.arrivalDeltaV, 0.0, 0.0});
    mission.arrivalEpochSeconds = mission.departureEpochSeconds + transfer.transferTime;
    return analyzeMission(mission);
}

GravityAssistResult gravityAssistTurn(double gravitationalParameter, double periapsisRadiusM, double incomingRelativeSpeedMps) {
    if (!std::isfinite(gravitationalParameter) || !std::isfinite(periapsisRadiusM) || !std::isfinite(incomingRelativeSpeedMps) || gravitationalParameter <= 0.0 || periapsisRadiusM <= 0.0 || incomingRelativeSpeedMps <= 0.0) return {false, 0.0, 0.0, 0.0, "gravity-assist parameters must be positive and finite"};
    const double eccentricity = 1.0 + periapsisRadiusM * incomingRelativeSpeedMps * incomingRelativeSpeedMps / gravitationalParameter;
    const double turnAngle = 2.0 * std::asin(1.0 / eccentricity);
    return {true, turnAngle, incomingRelativeSpeedMps, incomingRelativeSpeedMps, {}};
}

} // namespace bag
