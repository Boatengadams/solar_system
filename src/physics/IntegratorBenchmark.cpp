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
        std::vector<Body> sample = initialBodies;
        if (!advance(engine, sample, integrator, config.duration, config.timestep, metrics.integrationSteps, config.closeApproach)) {
            results.push_back(metrics);
            continue;
        }
        metrics.valid = true;
        const double energyScale = std::max(std::abs(initialEnergy), 1.0);
        metrics.energyDrift = std::abs(PhysicsEngine::totalEnergy(sample) - initialEnergy) / energyScale;
        metrics.positionError = length(sample[config.trackedBody].position - reference[config.trackedBody].position);
        metrics.velocityError = length(sample[config.trackedBody].velocity - reference[config.trackedBody].velocity);
        results.push_back(metrics);
    }
    return results;
}

} // namespace bag
