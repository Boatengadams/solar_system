#include "IntegratorBenchmark.hpp"

#include <algorithm>
#include <cmath>

namespace bag {
namespace {

bool advance(PhysicsEngine& engine, std::vector<Body>& bodies, Integrator integrator,
             double duration, double timestep, std::size_t& steps,
             const CloseApproachPolicy& policy) {
    double elapsed = 0.0;
    while (elapsed < duration) {
        const double step = std::min(timestep, duration - elapsed);
        const PhysicsStepResult result = engine.integrate(bodies, step, integrator, policy);
        if (!result.success) return false;
        elapsed += step;
        ++steps;
    }
    return true;
}

Vec3 angularMomentum(const std::vector<Body>& bodies) {
    Vec3 result;
    for (const Body& body : bodies) {
        if (!body.active) continue;
        result.x += body.mass * (body.position.y * body.velocity.z - body.position.z * body.velocity.y);
        result.y += body.mass * (body.position.z * body.velocity.x - body.position.x * body.velocity.z);
        result.z += body.mass * (body.position.x * body.velocity.y - body.position.y * body.velocity.x);
    }
    return result;
}

double measurePeriod(const std::vector<Body>& initial, const std::vector<Body>& final, double duration, double timestep) {
    if (initial.size() < 2 || final.size() < 2 || duration <= 0.0 || timestep <= 0.0) return 0.0;
    const Vec3 initialRelative = initial[1].position - initial[0].position;
    const Vec3 finalRelative = final[1].position - final[0].position;
    const double initialAngle = std::atan2(initialRelative.y, initialRelative.x);
    const double finalAngle = std::atan2(finalRelative.y, finalRelative.x);
    double delta = finalAngle - initialAngle;
    while (delta < 0.0) delta += 2.0 * 3.14159265358979323846;
    const double turns = delta / (2.0 * 3.14159265358979323846);
    if (turns <= 0.5) return 0.0;
    return duration / turns;
}

} // namespace

std::vector<IntegratorBenchmarkMetrics> compareIntegrators(
    const std::vector<Body>& initialBodies, const IntegratorBenchmarkConfig& config) {
    std::vector<IntegratorBenchmarkMetrics> results;
    if (initialBodies.empty() || !std::isfinite(config.timestep) || !std::isfinite(config.duration) ||
        config.timestep <= 0.0 || config.duration <= 0.0 || config.trackedBody >= initialBodies.size()) return results;

    const double referenceStep = config.referenceTimestep > 0.0 ? config.referenceTimestep : config.timestep / 16.0;
    if (!std::isfinite(referenceStep) || referenceStep <= 0.0) return results;
    PhysicsEngine engine;
    std::vector<Body> reference = initialBodies;
    std::size_t referenceSteps = 0;
    if (!advance(engine, reference, Integrator::RK4, config.duration, referenceStep, referenceSteps, config.closeApproach)) return results;
    const double initialEnergy = PhysicsEngine::totalEnergy(initialBodies);

    for (const Integrator integrator : {Integrator::Euler, Integrator::SemiImplicitEuler,
                                        Integrator::VelocityVerlet, Integrator::RK4}) {
        IntegratorBenchmarkMetrics metrics;
        metrics.integrator = integrator;
        metrics.timestep = config.timestep;
        metrics.elapsedSimulationTime = config.duration;
        std::vector<Body> sample = initialBodies;
        if (!advance(engine, sample, integrator, config.duration, config.timestep, metrics.integrationSteps, config.closeApproach)) {
            results.push_back(metrics);
            continue;
        }
        metrics.valid = true;
        const double energyScale = std::max(std::abs(initialEnergy), 1.0);
        metrics.energyDrift = std::abs(PhysicsEngine::totalEnergy(sample) - initialEnergy) / energyScale;
        const double initialAngularMomentum = std::max(length(angularMomentum(initialBodies)), 1.0);
        metrics.angularMomentumDrift = length(angularMomentum(sample) - angularMomentum(initialBodies)) / initialAngularMomentum;
        metrics.positionError = length(sample[config.trackedBody].position - reference[config.trackedBody].position);
        metrics.velocityError = length(sample[config.trackedBody].velocity - reference[config.trackedBody].velocity);
        const double measuredPeriod = measurePeriod(initialBodies, sample, config.duration, config.timestep);
        if (measuredPeriod > 0.0) {
            const Vec3 relativePosition = initialBodies[1].position - initialBodies[0].position;
            const Vec3 relativeVelocity = initialBodies[1].velocity - initialBodies[0].velocity;
            const double mu = PhysicsEngine::G * (initialBodies[0].mass + initialBodies[1].mass);
            const double specificEnergy = 0.5 * dot(relativeVelocity, relativeVelocity) - mu / length(relativePosition);
            if (specificEnergy < 0.0) {
                const double semiMajorAxis = -mu / (2.0 * specificEnergy);
                const double analyticalPeriod = 2.0 * 3.14159265358979323846 * std::sqrt(semiMajorAxis * semiMajorAxis * semiMajorAxis / mu);
                metrics.orbitalPeriodError = std::abs(measuredPeriod - analyticalPeriod) / analyticalPeriod;
            }
        }
        results.push_back(metrics);
    }
    return results;
}

} // namespace bag
