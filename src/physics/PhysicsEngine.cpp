#include "PhysicsEngine.hpp"

#include <algorithm>
#include <cmath>

namespace bag {
namespace {

bool finiteVec(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool validPolicy(const CloseApproachPolicy& policy) {
    return std::isfinite(policy.minimumSafeSeparation) && policy.minimumSafeSeparation > 0.0;
}

// Numerical warning threshold, not a physical collision threshold.
constexpr double UNSTABLE_SEPARATION_FRACTION = 0.25;

std::vector<Vec3> accelerations(const std::vector<Body>& bodies, const CloseApproachPolicy& policy,
                                InteractionReport& report, bool& valid) {
    std::vector<Vec3> result(bodies.size());
    valid = validPolicy(policy);
    if (!valid) return result;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].active || !std::isfinite(bodies[i].mass) || bodies[i].mass < 0.0 || !finiteVec(bodies[i].position) || !finiteVec(bodies[i].velocity)) {
            if (bodies[i].active) valid = false;
            continue;
        }
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            if (!bodies[j].active) continue;
            const Vec3 delta = bodies[j].position - bodies[i].position;
            const double distance = length(delta);
            if (!std::isfinite(distance)) { valid = false; continue; }
            if (distance < report.closeApproach.minimumDistance) {
                report.closeApproach.minimumDistance = distance;
                report.closeApproach.bodyA = i;
                report.closeApproach.bodyB = j;
            }
            if (distance < policy.minimumSafeSeparation) report.closeApproach.triggered = true;
            const double radiusA = bodies[i].realRadius > 0.0 ? bodies[i].realRadius : bodies[i].radius;
            const double radiusB = bodies[j].realRadius > 0.0 ? bodies[j].realRadius : bodies[j].radius;
            if (radiusA > 0.0 && radiusB > 0.0 && distance <= radiusA + radiusB) {
                report.collisions.push_back({i, j, distance, radiusA + radiusB - distance});
            }
            const double safeDistance = std::max(distance, policy.minimumSafeSeparation);
            const double distanceSquared = safeDistance * safeDistance;
            const double inverseDistanceCubed = 1.0 / (safeDistance * distanceSquared);
            const Vec3 factor = delta * (PhysicsEngine::G * inverseDistanceCubed);
            result[i] = result[i] + factor * bodies[j].mass;
            result[j] = result[j] - factor * bodies[i].mass;
        }
    }
    report.closeApproach.unstable = report.closeApproach.triggered &&
        report.closeApproach.minimumDistance < policy.minimumSafeSeparation * UNSTABLE_SEPARATION_FRACTION;
    return result;
}

bool finiteBodies(const std::vector<Body>& bodies) {
    for (const Body& body : bodies) {
        if (body.active && (!std::isfinite(body.mass) || body.mass < 0.0 || !finiteVec(body.position) || !finiteVec(body.velocity))) return false;
    }
    return true;
}

double relativeDifference(Vec3 a, Vec3 b) {
    const double numerator = length(a - b);
    const double scale = std::max({1.0, length(a), length(b)});
    return numerator / scale;
}

} // namespace

const char* integratorName(Integrator integrator) {
    switch (integrator) {
    case Integrator::Euler: return "euler";
    case Integrator::SemiImplicitEuler: return "semi_implicit_euler";
    case Integrator::VelocityVerlet: return "velocity_verlet";
    case Integrator::RK4: return "rk4";
    }
    return "velocity_verlet";
}

bool parseIntegrator(const std::string& name, Integrator& integrator) {
    if (name == "euler") integrator = Integrator::Euler;
    else if (name == "semi_implicit_euler") integrator = Integrator::SemiImplicitEuler;
    else if (name == "velocity_verlet") integrator = Integrator::VelocityVerlet;
    else if (name == "rk4") integrator = Integrator::RK4;
    else return false;
    return true;
}

OrbitalElements PhysicsEngine::orbitalElements(const Body& body, double centralMass) {
    OrbitalElements result;
    const double radius = length(body.position);
    const double mu = G * centralMass;
    if (!std::isfinite(radius) || radius <= 0.0 || !std::isfinite(mu) || mu <= 0.0) return result;

    const double speedSquared = dot(body.velocity, body.velocity);
    const double specificEnergyValue = 0.5 * speedSquared - mu / radius;
    if (!std::isfinite(specificEnergyValue)) return result;

    const Vec3 angularMomentum = {
        body.position.y * body.velocity.z - body.position.z * body.velocity.y,
        body.position.z * body.velocity.x - body.position.x * body.velocity.z,
        body.position.x * body.velocity.y - body.position.y * body.velocity.x,
    };
    const double angularMomentumScale = std::sqrt(mu * radius);
    if (length(angularMomentum) <= angularMomentumScale * 1.0e-12) return result;

    const Vec3 eccentricityVector = (body.position * (speedSquared - mu / radius) - body.velocity * dot(body.position, body.velocity)) / mu;
    const double eccentricity = length(eccentricityVector);
    if (!std::isfinite(eccentricity)) return result;
    result.valid = true;
    result.eccentricity = eccentricity;
    const double angularMomentumSquared = dot(angularMomentum, angularMomentum);
    result.periapsis = angularMomentumSquared / (mu * (1.0 + eccentricity));
    constexpr double PARABOLIC_ENERGY_TOLERANCE = 1.0e-12;
    const double energyScale = std::max(mu / radius, 1.0);
    if (std::abs(specificEnergyValue) <= PARABOLIC_ENERGY_TOLERANCE * energyScale) {
        result.type = OrbitType::Parabolic;
        return result;
    }
    result.semiMajorAxis = -mu / (2.0 * specificEnergyValue);
    if (specificEnergyValue < 0.0) {
        result.type = OrbitType::Elliptical;
        result.apoapsis = result.semiMajorAxis * (1.0 + eccentricity);
        result.period = 2.0 * 3.14159265358979323846 * std::sqrt(result.semiMajorAxis * result.semiMajorAxis * result.semiMajorAxis / mu);
    } else {
        result.type = OrbitType::Hyperbolic;
    }
    return result;
}

HohmannTransfer PhysicsEngine::hohmannTransfer(double innerRadius, double outerRadius, double centralMass) {
    HohmannTransfer result;
    const double mu = G * centralMass;
    if (!std::isfinite(innerRadius) || !std::isfinite(outerRadius) || !std::isfinite(mu) ||
        innerRadius <= 0.0 || outerRadius <= 0.0 || centralMass <= 0.0 || innerRadius == outerRadius) return result;

    const double transferAxis = 0.5 * (innerRadius + outerRadius);
    const double circularInnerVelocity = std::sqrt(mu / innerRadius);
    const double circularOuterVelocity = std::sqrt(mu / outerRadius);
    const double transferAtInner = std::sqrt(mu * (2.0 / innerRadius - 1.0 / transferAxis));
    const double transferAtOuter = std::sqrt(mu * (2.0 / outerRadius - 1.0 / transferAxis));
    result.valid = true;
    result.transferSemiMajorAxis = transferAxis;
    result.departureDeltaV = std::abs(transferAtInner - circularInnerVelocity);
    result.arrivalDeltaV = std::abs(circularOuterVelocity - transferAtOuter);
    result.totalDeltaV = result.departureDeltaV + result.arrivalDeltaV;
    result.transferTime = 3.14159265358979323846 * std::sqrt(transferAxis * transferAxis * transferAxis / mu);
    return result;
}

PhysicsStepResult PhysicsEngine::applyGravity(std::vector<Body>& bodies, double dt) const {
    return integrate(bodies, dt, Integrator::VelocityVerlet);
}

PhysicsStepResult PhysicsEngine::integrate(std::vector<Body>& bodies, double dt, Integrator integrator,
                                           const CloseApproachPolicy& policy) const {
    PhysicsStepResult result;
    const std::vector<Body> originalBodies = bodies;
    const auto failWithoutMutation = [&]() {
        bodies = originalBodies;
        return result;
    };
    result.timestepUsed = dt;
    result.suggestedTimestep = dt;
    if (!std::isfinite(dt) || dt <= 0.0 || bodies.empty() || !validPolicy(policy) || !finiteBodies(bodies)) {
        result.error = "invalid integration input";
        result.numericalInstability = true;
        return result;
    }
    const auto beforeInteractions = inspectInteractions(bodies, policy);
    result.interactions = beforeInteractions;
    bool valid = false;
    const auto initialAcceleration = accelerations(bodies, policy, result.interactions, valid);
    if (!valid) { result.error = "invalid state during acceleration calculation"; result.numericalInstability = true; return failWithoutMutation(); }

    if (integrator == Integrator::Euler || integrator == Integrator::SemiImplicitEuler) {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (!bodies[i].active) continue;
            if (integrator == Integrator::Euler) bodies[i].position = bodies[i].position + bodies[i].velocity * dt;
            bodies[i].velocity = bodies[i].velocity + initialAcceleration[i] * dt;
            if (integrator == Integrator::SemiImplicitEuler) bodies[i].position = bodies[i].position + bodies[i].velocity * dt;
        }
        result.success = finiteBodies(bodies);
        result.accepted = result.success;
        if (!result.success) { result.error = "non-finite state after integration"; result.numericalInstability = true; return failWithoutMutation(); }
        return result;
    }

    if (integrator == Integrator::VelocityVerlet) {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (!bodies[i].active) continue;
            bodies[i].velocity = bodies[i].velocity + initialAcceleration[i] * (0.5 * dt);
            bodies[i].position = bodies[i].position + bodies[i].velocity * dt;
        }
        InteractionReport updatedReport;
        const auto updatedAcceleration = accelerations(bodies, policy, updatedReport, valid);
        result.interactions.closeApproach = result.interactions.closeApproach.triggered ? result.interactions.closeApproach : updatedReport.closeApproach;
        result.interactions.collisions.insert(result.interactions.collisions.end(), updatedReport.collisions.begin(), updatedReport.collisions.end());
        if (!valid) { result.error = "invalid state during updated acceleration calculation"; result.numericalInstability = true; return failWithoutMutation(); }
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].active) bodies[i].velocity = bodies[i].velocity + updatedAcceleration[i] * (0.5 * dt);
        }
        result.success = finiteBodies(bodies);
        result.accepted = result.success;
        if (!result.success) { result.error = "non-finite state after integration"; result.numericalInstability = true; return failWithoutMutation(); }
        return result;
    }

    const auto makeStage = [&](const std::vector<Body>& source, const std::vector<Vec3>& kPosition,
                               const std::vector<Vec3>& kVelocity, double scale) {
        std::vector<Body> stage = source;
        for (std::size_t i = 0; i < stage.size(); ++i) {
            if (!stage[i].active) continue;
            stage[i].position = stage[i].position + kPosition[i] * scale;
            stage[i].velocity = stage[i].velocity + kVelocity[i] * scale;
        }
        return stage;
    };
    std::vector<Vec3> k1Position(bodies.size()), k2Position(bodies.size()), k3Position(bodies.size()), k4Position(bodies.size());
    std::vector<Vec3> k1Velocity = initialAcceleration, k2Velocity, k3Velocity, k4Velocity;
    for (std::size_t i = 0; i < bodies.size(); ++i) if (bodies[i].active) k1Position[i] = bodies[i].velocity;
    auto stage = makeStage(bodies, k1Position, k1Velocity, 0.5 * dt);
    InteractionReport stageReport;
    k2Velocity = accelerations(stage, policy, stageReport, valid);
    if (!valid) { result.error = "invalid RK4 intermediate state"; result.numericalInstability = true; return failWithoutMutation(); }
    for (std::size_t i = 0; i < bodies.size(); ++i) if (bodies[i].active) k2Position[i] = stage[i].velocity;
    stage = makeStage(bodies, k2Position, k2Velocity, 0.5 * dt);
    k3Velocity = accelerations(stage, policy, stageReport, valid);
    if (!valid) { result.error = "invalid RK4 intermediate state"; result.numericalInstability = true; return failWithoutMutation(); }
    for (std::size_t i = 0; i < bodies.size(); ++i) if (bodies[i].active) k3Position[i] = stage[i].velocity;
    stage = makeStage(bodies, k3Position, k3Velocity, dt);
    k4Velocity = accelerations(stage, policy, stageReport, valid);
    if (!valid) { result.error = "invalid RK4 intermediate state"; result.numericalInstability = true; return failWithoutMutation(); }
    for (std::size_t i = 0; i < bodies.size(); ++i) if (bodies[i].active) k4Position[i] = stage[i].velocity;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].active) continue;
        bodies[i].position = bodies[i].position + (k1Position[i] + k2Position[i] * 2.0 + k3Position[i] * 2.0 + k4Position[i]) * (dt / 6.0);
        bodies[i].velocity = bodies[i].velocity + (k1Velocity[i] + k2Velocity[i] * 2.0 + k3Velocity[i] * 2.0 + k4Velocity[i]) * (dt / 6.0);
    }
    result.success = finiteBodies(bodies);
    result.accepted = result.success;
    if (!result.success) { result.error = "non-finite state after integration"; result.numericalInstability = true; return failWithoutMutation(); }
    return result;
}

PhysicsStepResult PhysicsEngine::integrateAdaptive(std::vector<Body>& bodies, double& timestep,
                                                   Integrator integrator,
                                                   const AdaptiveTimestepSettings& settings) const {
    PhysicsStepResult result;
    if (!std::isfinite(timestep) || timestep <= 0.0 ||
        !std::isfinite(settings.minimumTimestep) || !std::isfinite(settings.maximumTimestep) ||
        !std::isfinite(settings.errorTolerance) || settings.minimumTimestep <= 0.0 ||
        settings.maximumTimestep < settings.minimumTimestep || settings.errorTolerance <= 0.0) {
        result.error = "invalid adaptive timestep or proposed step";
        result.numericalInstability = true;
        return result;
    }
    timestep = std::clamp(timestep, settings.minimumTimestep, settings.maximumTimestep);
    const double attempted = timestep;
    std::vector<Body> full = bodies;
    std::vector<Body> half = bodies;
    const PhysicsStepResult fullResult = integrate(full, attempted, integrator, settings.closeApproach);
    const PhysicsStepResult halfOne = integrate(half, attempted * 0.5, integrator, settings.closeApproach);
    const PhysicsStepResult halfTwo = halfOne.success ? integrate(half, attempted * 0.5, integrator, settings.closeApproach) : PhysicsStepResult{};
    result.timestepUsed = attempted;
    result.interactions = fullResult.interactions;
    if (!fullResult.success || !halfOne.success || !halfTwo.success) {
        result.error = "adaptive trial became numerically unstable";
        result.numericalInstability = true;
    } else {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (!bodies[i].active) continue;
            result.errorEstimate = std::max(result.errorEstimate, relativeDifference(full[i].position, half[i].position));
            result.errorEstimate = std::max(result.errorEstimate, relativeDifference(full[i].velocity, half[i].velocity));
        }
        result.interactions.closeApproach = halfTwo.interactions.closeApproach.triggered ? halfTwo.interactions.closeApproach : result.interactions.closeApproach;
        if (result.interactions.closeApproach.unstable) {
            result.error = "close approach is below the numerically safe separation";
            result.numericalInstability = true;
        } else if (result.errorEstimate <= settings.errorTolerance) {
            bodies = std::move(half);
            result.success = true;
            result.accepted = true;
            const double growth = result.errorEstimate < settings.errorTolerance * 0.1 ? 1.5 : 1.1;
            result.suggestedTimestep = std::clamp(attempted * growth, settings.minimumTimestep, settings.maximumTimestep);
            timestep = result.suggestedTimestep;
            return result;
        } else {
            result.error = "adaptive trial exceeded error tolerance";
        }
    }
    const double reduced = std::max(settings.minimumTimestep, attempted * 0.5);
    result.suggestedTimestep = reduced;
    timestep = reduced;
    if (attempted <= settings.minimumTimestep) {
        result.error = result.error.empty() ? "minimum adaptive timestep cannot satisfy stability requirements" : result.error;
    }
    return result;
}

InteractionReport PhysicsEngine::inspectInteractions(const std::vector<Body>& bodies, const CloseApproachPolicy& policy) {
    InteractionReport report;
    if (!validPolicy(policy)) { report.closeApproach.unstable = true; return report; }
    if (!finiteBodies(bodies)) { report.closeApproach.unstable = true; return report; }
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].active) continue;
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            if (!bodies[j].active) continue;
            const double distance = length(bodies[j].position - bodies[i].position);
            if (!std::isfinite(distance)) { report.closeApproach.unstable = true; continue; }
            if (distance < report.closeApproach.minimumDistance) {
                report.closeApproach.minimumDistance = distance;
                report.closeApproach.bodyA = i;
                report.closeApproach.bodyB = j;
            }
            if (distance < policy.minimumSafeSeparation) report.closeApproach.triggered = true;
            const double radiusA = bodies[i].realRadius > 0.0 ? bodies[i].realRadius : bodies[i].radius;
            const double radiusB = bodies[j].realRadius > 0.0 ? bodies[j].realRadius : bodies[j].radius;
            if (radiusA > 0.0 && radiusB > 0.0 && distance <= radiusA + radiusB) report.collisions.push_back({i, j, distance, radiusA + radiusB - distance});
        }
    }
    report.closeApproach.unstable = report.closeApproach.unstable ||
        (report.closeApproach.triggered && report.closeApproach.minimumDistance < policy.minimumSafeSeparation * UNSTABLE_SEPARATION_FRACTION);
    return report;
}

double PhysicsEngine::totalEnergy(const std::vector<Body>& bodies) {
    double energy = 0.0;
    for (const Body& body : bodies) if (body.active) energy += 0.5 * body.mass * dot(body.velocity, body.velocity);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i].active) continue;
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            if (!bodies[j].active) continue;
            const double distance = length(bodies[j].position - bodies[i].position);
            if (distance > 0.0 && std::isfinite(distance)) energy -= G * bodies[i].mass * bodies[j].mass / distance;
        }
    }
    return energy;
}

Vec3 PhysicsEngine::totalMomentum(const std::vector<Body>& bodies) {
    Vec3 momentum;
    for (const Body& body : bodies) if (body.active) momentum = momentum + body.velocity * body.mass;
    return momentum;
}

double PhysicsEngine::distanceFromSun(const Body& body) {
    return length(body.position);
}

double PhysicsEngine::specificEnergy(const Body& body) {
    return 0.5 * dot(body.velocity, body.velocity) - G * SOLAR_MASS / std::max(distanceFromSun(body), 1.0);
}

double PhysicsEngine::escapeVelocity(const Body& body) {
    return std::sqrt(2.0 * G * SOLAR_MASS / std::max(distanceFromSun(body), 1.0));
}

double PhysicsEngine::orbitalVelocity(const Body& body) {
    return std::sqrt(G * SOLAR_MASS / std::max(distanceFromSun(body), 1.0));
}

double PhysicsEngine::surfaceGravity(const Body& body) {
    return G * body.mass / (body.realRadius * body.realRadius);
}

} // namespace bag
