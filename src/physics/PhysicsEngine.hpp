#pragma once

#include <string>
#include <cstddef>
#include <limits>
#include <vector>

#include "Body.hpp"

namespace bag {

enum class Integrator {
    Euler,
    SemiImplicitEuler,
    VelocityVerlet,
    RK4,
};

enum class OrbitType {
    Invalid,
    Elliptical,
    Parabolic,
    Hyperbolic,
};

const char* integratorName(Integrator integrator);
bool parseIntegrator(const std::string& name, Integrator& integrator);

struct OrbitalElements {
    bool valid = false;
    OrbitType type = OrbitType::Invalid;
    double semiMajorAxis = 0.0;
    double eccentricity = 0.0;
    double periapsis = 0.0;
    double apoapsis = 0.0;
    double period = 0.0;
};

struct HohmannTransfer {
    bool valid = false;
    double transferSemiMajorAxis = 0.0;
    double departureDeltaV = 0.0;
    double arrivalDeltaV = 0.0;
    double totalDeltaV = 0.0;
    double transferTime = 0.0;
};

struct CollisionInfo {
    std::size_t bodyA = 0;
    std::size_t bodyB = 0;
    double separation = 0.0;
    double penetration = 0.0;
};

struct CloseApproachReport {
    bool triggered = false;
    bool unstable = false;
    double minimumDistance = std::numeric_limits<double>::infinity();
    std::size_t bodyA = 0;
    std::size_t bodyB = 0;
};

struct InteractionReport {
    CloseApproachReport closeApproach;
    std::vector<CollisionInfo> collisions;
};

struct CloseApproachPolicy {
    // Numerical regularization threshold, not a physical collision radius.
    double minimumSafeSeparation = 1.0e7;
};

struct AdaptiveTimestepSettings {
    double minimumTimestep = 1.0;
    double maximumTimestep = 86400.0;
    double errorTolerance = 1.0e-8;
    CloseApproachPolicy closeApproach;
};

struct PhysicsStepResult {
    bool success = false;
    bool accepted = false;
    bool numericalInstability = false;
    double timestepUsed = 0.0;
    double suggestedTimestep = 0.0;
    double errorEstimate = 0.0;
    InteractionReport interactions;
    std::string error;
};

class PhysicsEngine {
public:
    static constexpr double G = 6.67430e-11;
    static constexpr double AU = 1.495978707e11;
    static constexpr double DAY = 86400.0;
    static constexpr double YEAR = 365.25 * DAY;
    static constexpr double SOLAR_MASS = 1.98847e30;
    static constexpr double EARTH_MASS = 5.9722e24;
    static constexpr double EARTH_RADIUS = 6.371e6;
    static constexpr double LIGHT_SPEED = 299792458.0;
    static constexpr double MIN_PHYSICS_DISTANCE = 1.0e7;
    static constexpr int MAX_TRAIL = 160;

    PhysicsStepResult applyGravity(std::vector<Body>& bodies, double dt) const;
    PhysicsStepResult integrate(std::vector<Body>& bodies, double dt, Integrator integrator,
                                const CloseApproachPolicy& policy = {}) const;
    PhysicsStepResult integrateAdaptive(std::vector<Body>& bodies, double& timestep,
                                        Integrator integrator,
                                        const AdaptiveTimestepSettings& settings) const;
    static InteractionReport inspectInteractions(const std::vector<Body>& bodies,
                                                 const CloseApproachPolicy& policy = {});
    static double totalEnergy(const std::vector<Body>& bodies);
    static Vec3 totalMomentum(const std::vector<Body>& bodies);

    static double distanceFromSun(const Body& body);
    static double specificEnergy(const Body& body);
    static double escapeVelocity(const Body& body);
    static double orbitalVelocity(const Body& body);
    static double surfaceGravity(const Body& body);
    // The body's position and velocity are relative to the central body.
    static OrbitalElements orbitalElements(const Body& body, double centralMass = SOLAR_MASS);
    static HohmannTransfer hohmannTransfer(double innerRadius, double outerRadius,
                                           double centralMass = SOLAR_MASS);
};

} // namespace bag
