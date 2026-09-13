#include <cassert>
#include <cmath>

#include "missions/Mission.hpp"
#include "missions/TrajectoryPrediction.hpp"

using namespace bag;

namespace {
Spacecraft testCraft() {
    Spacecraft spacecraft;
    spacecraft.id = "test-craft";
    spacecraft.dryMassKg = 1000.0;
    spacecraft.propellantMassKg = 6000.0;
    spacecraft.maximumThrustN = 10000.0;
    spacecraft.specificImpulseS = 300.0;
    spacecraft.positionM = {PhysicsEngine::AU, 0.0, 0.0};
    spacecraft.velocityMps = {0.0, 29784.0, 0.0};
    return spacecraft;
}

void testRocketEquation() {
    Spacecraft spacecraft = testCraft();
    assert(spacecraft.valid());
    assert(spacecraft.totalMassKg() == 7000.0);
    assert(spacecraft.availableDeltaVMps() > 0.0);
    const double before = spacecraft.propellantMassKg;
    const BurnResult burn = applyImpulse(spacecraft, {100.0, 0.0, 0.0});
    assert(burn.success && burn.deltaVMps == 100.0 && spacecraft.propellantMassKg < before);
    const double velocity = spacecraft.velocityMps.x;
    const double propellant = spacecraft.propellantMassKg;
    assert(!applyImpulse(spacecraft, {1.0e9, 0.0, 0.0}).success);
    assert(spacecraft.velocityMps.x == velocity && spacecraft.propellantMassKg == propellant);
}

void testMissions() {
    Mission mission;
    mission.departureEpochSeconds = 0.0;
    mission.spacecraft = testCraft();
    const MissionAnalysis transfer = addHohmannTransfer(mission, PhysicsEngine::AU, 1.523679 * PhysicsEngine::AU);
    assert(transfer.valid && mission.maneuvers.size() == 2 && transfer.timeOfFlightSeconds > 0.0);
    const GravityAssistResult assist = gravityAssistTurn(3.986004418e14, PhysicsEngine::EARTH_RADIUS + 300000.0, 10000.0);
    assert(assist.valid && assist.turnAngleRadians > 0.0 && assist.outgoingRelativeSpeedMps == assist.incomingRelativeSpeedMps);
}

void testTrajectory() {
    Body sun;
    sun.id = "sun";
    sun.mass = PhysicsEngine::SOLAR_MASS;
    Spacecraft spacecraft = testCraft();
    const TrajectoryPredictionResult result = predictTrajectory(sun, spacecraft, 3600.0, 60.0, {{600.0, 0.0, 10.0, 0.0, 0.0}});
    assert(result.success && result.points.size() > 2 && std::isfinite(result.points.back().positionM.x));
}
}

int main() {
    testRocketEquation();
    testMissions();
    testTrajectory();
    return 0;
}
