#include "TrajectoryPrediction.hpp"

#include <algorithm>
#include <cmath>

namespace bag {
namespace {
Vec3 maneuverDeltaVelocity(const ManeuverNode& node, const Body& central, const Body& vehicle) {
    const Vec3 relativePosition = vehicle.position - central.position;
    const Vec3 relativeVelocity = vehicle.velocity - central.velocity;
    const Vec3 radial = normalized(relativePosition);
    const Vec3 normal = normalized(cross(relativePosition, relativeVelocity));
    const Vec3 prograde = normalized(cross(normal, radial));
    return radial * node.radialDeltaVMps + prograde * node.progradeDeltaVMps + normal * node.normalDeltaVMps;
}
}

TrajectoryPredictionResult predictTrajectory(const Body& centralBody, const Spacecraft& initialSpacecraft,
                                             double durationSeconds, double timestepSeconds,
                                             const std::vector<ManeuverNode>& maneuvers) {
    if (!initialSpacecraft.valid() || centralBody.mass <= 0.0 || !std::isfinite(centralBody.mass) || durationSeconds <= 0.0 || timestepSeconds <= 0.0 || !std::isfinite(durationSeconds) || !std::isfinite(timestepSeconds)) return {false, {}, 0.0, "invalid trajectory prediction inputs"};
    Spacecraft spacecraft = initialSpacecraft;
    Body vehicle;
    vehicle.id = spacecraft.id.empty() ? "spacecraft" : spacecraft.id;
    vehicle.name = spacecraft.name;
    vehicle.type = "spacecraft";
    vehicle.position = spacecraft.positionM;
    vehicle.velocity = spacecraft.velocityMps;
    vehicle.mass = spacecraft.totalMassKg();
    vehicle.radius = 1.0;
    Body central = centralBody;
    std::vector<Body> bodies{central, vehicle};
    std::vector<ManeuverNode> ordered = maneuvers;
    std::sort(ordered.begin(), ordered.end(), [](const ManeuverNode& a, const ManeuverNode& b) { return a.epochSeconds < b.epochSeconds; });
    for (const ManeuverNode& node : ordered) if (node.epochSeconds < 0.0 || node.epochSeconds > durationSeconds) return {false, {}, 0.0, "maneuver lies outside prediction interval"};
    TrajectoryPredictionResult result;
    result.success = true;
    result.points.push_back({0.0, vehicle.position, vehicle.velocity});
    PhysicsEngine physics;
    CloseApproachPolicy policy;
    policy.minimumSafeSeparation = PhysicsEngine::MIN_PHYSICS_DISTANCE;
    double time = 0.0;
    std::size_t maneuverIndex = 0;
    while (time < durationSeconds) {
        while (maneuverIndex < ordered.size() && ordered[maneuverIndex].epochSeconds <= time + 1e-9) {
            const BurnResult burn = applyImpulse(spacecraft, maneuverDeltaVelocity(ordered[maneuverIndex], bodies[0], bodies[1]));
            if (!burn.success) return {false, result.points, time, burn.message};
            bodies[1].velocity = spacecraft.velocityMps;
            bodies[1].mass = spacecraft.totalMassKg();
            ++maneuverIndex;
        }
        const double step = std::min(timestepSeconds, durationSeconds - time);
        const PhysicsStepResult physicsResult = physics.integrate(bodies, step, Integrator::VelocityVerlet, policy);
        if (!physicsResult.success) return {false, result.points, time, physicsResult.error};
        time += physicsResult.timestepUsed;
        result.points.push_back({time, bodies[1].position, bodies[1].velocity});
    }
    result.finalTimeSeconds = time;
    return result;
}

} // namespace bag
