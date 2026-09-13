#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "physics/PhysicsEngine.hpp"
#include "physics/IntegratorBenchmark.hpp"

namespace {

bool closeEnough(double actual, double expected, double relativeTolerance) {
    return std::abs(actual - expected) <= std::abs(expected) * relativeTolerance;
}

std::vector<bag::Body> earthSystem() {
    bag::Body sun;
    sun.mass = bag::PhysicsEngine::SOLAR_MASS;
    bag::Body earth;
    earth.mass = bag::PhysicsEngine::EARTH_MASS;
    earth.position = {bag::PhysicsEngine::AU, 0.0, 0.0};
    earth.velocity = {0.0, bag::PhysicsEngine::orbitalVelocity(earth), 0.0};
    return {sun, earth};
}

bag::Body freeBody(double x, double vx) {
    bag::Body body;
    body.position = {x, 0.0, 0.0};
    body.velocity = {vx, 0.0, 0.0};
    body.realRadius = 1.0;
    return body;
}

} // namespace

int main() {
    using namespace bag;

    Body sun;
    sun.mass = PhysicsEngine::SOLAR_MASS;
    sun.position = {0.0, 0.0, 0.0};
    sun.velocity = {0.0, 0.0, 0.0};

    Body earth;
    earth.mass = PhysicsEngine::EARTH_MASS;
    earth.realRadius = PhysicsEngine::EARTH_RADIUS;
    earth.position = {PhysicsEngine::AU, 0.0, 0.0};
    earth.velocity = {0.0, PhysicsEngine::orbitalVelocity(earth), 0.0};

    const double expectedOrbitalVelocity = 29785.142169221024;
    assert(closeEnough(PhysicsEngine::orbitalVelocity(earth), expectedOrbitalVelocity, 1.0e-12));
    assert(PhysicsEngine::specificEnergy(earth) < 0.0);
    const double defaultEnergy = PhysicsEngine::specificEnergy(earth);
    const double earthMassEnergy = 0.5 * dot(earth.velocity, earth.velocity) -
        PhysicsEngine::G * PhysicsEngine::EARTH_MASS / PhysicsEngine::AU;
    assert(closeEnough(PhysicsEngine::specificEnergy(earth, PhysicsEngine::EARTH_MASS), earthMassEnergy, 1.0e-12));
    assert(PhysicsEngine::specificEnergy(earth) == defaultEnergy);
    assert(PhysicsEngine::escapeVelocity(earth) > PhysicsEngine::orbitalVelocity(earth));
    assert(closeEnough(PhysicsEngine::surfaceGravity(earth), 9.820302293385645, 1.0e-12));

    const OrbitalElements earthElements = PhysicsEngine::orbitalElements(earth);
    assert(earthElements.valid);
    assert(closeEnough(earthElements.semiMajorAxis, PhysicsEngine::AU, 1.0e-12));
    assert(earthElements.eccentricity < 1.0e-12);
    const double expectedEarthPeriod = 2.0 * 3.14159265358979323846 *
        std::sqrt(PhysicsEngine::AU * PhysicsEngine::AU * PhysicsEngine::AU /
                  (PhysicsEngine::G * PhysicsEngine::SOLAR_MASS));
    assert(closeEnough(earthElements.period, expectedEarthPeriod, 1.0e-12));

    const HohmannTransfer transfer = PhysicsEngine::hohmannTransfer(PhysicsEngine::AU, 1.524 * PhysicsEngine::AU);
    assert(transfer.valid);
    assert(transfer.departureDeltaV > 0.0);
    assert(transfer.arrivalDeltaV > 0.0);
    assert(closeEnough(transfer.totalDeltaV, transfer.departureDeltaV + transfer.arrivalDeltaV, 1.0e-12));
    assert(PhysicsEngine::hohmannTransfer(0.0, PhysicsEngine::AU).valid == false);
    const HohmannTransfer reverseTransfer = PhysicsEngine::hohmannTransfer(1.524 * PhysicsEngine::AU, PhysicsEngine::AU);
    assert(reverseTransfer.valid);
    assert(closeEnough(reverseTransfer.transferSemiMajorAxis, transfer.transferSemiMajorAxis, 1.0e-12));
    assert(closeEnough(reverseTransfer.transferTime, transfer.transferTime, 1.0e-12));
    assert(!PhysicsEngine::hohmannTransfer(PhysicsEngine::AU, PhysicsEngine::AU, -1.0).valid);

    Body elliptical = earth;
    const double semiMajorAxis = 1.2 * PhysicsEngine::AU;
    const double eccentricity = 0.25;
    elliptical.position = {semiMajorAxis * (1.0 - eccentricity), 0.0, 0.0};
    elliptical.velocity = {0.0, std::sqrt(PhysicsEngine::G * PhysicsEngine::SOLAR_MASS *
                                           (2.0 / length(elliptical.position) - 1.0 / semiMajorAxis)), 0.0};
    const OrbitalElements ellipticalElements = PhysicsEngine::orbitalElements(elliptical);
    assert(ellipticalElements.valid && ellipticalElements.type == OrbitType::Elliptical);
    assert(closeEnough(ellipticalElements.semiMajorAxis, semiMajorAxis, 1.0e-12));
    assert(closeEnough(ellipticalElements.eccentricity, eccentricity, 1.0e-12));

    Body escaping = earth;
    escaping.velocity = {0.0, PhysicsEngine::escapeVelocity(escaping) * 1.1, 0.0};
    const OrbitalElements hyperbolicElements = PhysicsEngine::orbitalElements(escaping);
    assert(hyperbolicElements.valid && hyperbolicElements.type == OrbitType::Hyperbolic);
    assert(hyperbolicElements.semiMajorAxis < 0.0);
    assert(hyperbolicElements.apoapsis == 0.0);
    Body parabolic = earth;
    parabolic.velocity = {0.0, PhysicsEngine::escapeVelocity(parabolic), 0.0};
    const OrbitalElements parabolicElements = PhysicsEngine::orbitalElements(parabolic);
    assert(parabolicElements.valid && parabolicElements.type == OrbitType::Parabolic);
    Body radial = earth;
    radial.velocity = {PhysicsEngine::escapeVelocity(radial), 0.0, 0.0};
    assert(!PhysicsEngine::orbitalElements(radial).valid);
    Body invalidOrbit = earth;
    invalidOrbit.position = {0.0, 0.0, 0.0};
    assert(!PhysicsEngine::orbitalElements(invalidOrbit).valid);

    std::vector<Body> bodies{sun, earth};
    const Vec3 initialEarthPosition = bodies[1].position;
    PhysicsEngine physics;
    physics.applyGravity(bodies, 1.0);
    assert(bodies[1].position.x < initialEarthPosition.x);
    assert(std::isfinite(bodies[1].velocity.x));
    assert(std::isfinite(bodies[1].velocity.y));

    Integrator parsed;
    assert(parseIntegrator("euler", parsed) && parsed == Integrator::Euler);
    assert(parseIntegrator("semi_implicit_euler", parsed) && parsed == Integrator::SemiImplicitEuler);
    assert(parseIntegrator("velocity_verlet", parsed) && parsed == Integrator::VelocityVerlet);
    assert(parseIntegrator("rk4", parsed) && parsed == Integrator::RK4);
    assert(!parseIntegrator("bogus", parsed));

    for (const Integrator integrator : {Integrator::Euler, Integrator::SemiImplicitEuler,
                                        Integrator::VelocityVerlet, Integrator::RK4}) {
        auto system = earthSystem();
        const double initialRadius = length(system[1].position);
        physics.integrate(system, 3600.0, integrator);
        assert(std::isfinite(system[1].position.x));
        assert(std::isfinite(system[1].velocity.y));
        assert(length(system[1].position) > 0.9 * initialRadius);
    }

    auto rk4System = earthSystem();
    const double initialEnergy = PhysicsEngine::specificEnergy(rk4System[1]);
    for (int step = 0; step < 24; ++step) physics.integrate(rk4System, 3600.0, Integrator::RK4);
    assert(closeEnough(PhysicsEngine::specificEnergy(rk4System[1]), initialEnergy, 1.0e-8));

    const CloseApproachPolicy approachPolicy{300.0};
    std::vector<Body> interactionBodies{freeBody(0.0, 0.0), freeBody(100.0, 0.0)};
    interactionBodies[0].realRadius = 100.0;
    interactionBodies[1].realRadius = 100.0;
    const InteractionReport interactions = PhysicsEngine::inspectInteractions(interactionBodies, approachPolicy);
    assert(interactions.closeApproach.triggered);
    assert(!interactions.closeApproach.unstable);
    assert(interactions.collisions.size() == 1);
    assert(interactions.collisions[0].penetration > 0.0);

    AdaptiveTimestepSettings adaptive;
    adaptive.minimumTimestep = 10.0;
    adaptive.maximumTimestep = 20.0;
    adaptive.errorTolerance = 1.0e-12;
    std::vector<Body> freeBodies{freeBody(0.0, 1.0), freeBody(1.0e9, -1.0)};
    double adaptiveStep = 100.0;
    PhysicsStepResult bounded = physics.integrateAdaptive(freeBodies, adaptiveStep, Integrator::RK4, adaptive);
    assert(bounded.success);
    assert(bounded.timestepUsed == 20.0);
    assert(adaptiveStep <= adaptive.maximumTimestep && adaptiveStep >= adaptive.minimumTimestep);

    adaptiveStep = 10.0;
    adaptive.errorTolerance = 1.0e-20;
    PhysicsStepResult stable = physics.integrateAdaptive(freeBodies, adaptiveStep, Integrator::RK4, adaptive);
    assert(stable.success);
    assert(stable.timestepUsed == adaptive.minimumTimestep);

    std::vector<Body> unstableBodies{freeBody(0.0, 0.0), freeBody(0.0, 0.0)};
    adaptive.errorTolerance = 1.0e-8;
    adaptive.closeApproach.minimumSafeSeparation = 1000.0;
    adaptiveStep = adaptive.minimumTimestep;
    PhysicsStepResult failed = physics.integrateAdaptive(unstableBodies, adaptiveStep, Integrator::RK4, adaptive);
    assert(!failed.success && failed.numericalInstability);
    assert(failed.timestepUsed == adaptive.minimumTimestep);

    adaptiveStep = std::numeric_limits<double>::quiet_NaN();
    const PhysicsStepResult invalidStep = physics.integrateAdaptive(freeBodies, adaptiveStep, Integrator::RK4, adaptive);
    assert(!invalidStep.success && invalidStep.numericalInstability);
    assert(invalidStep.timestepUsed == 0.0);

    std::vector<Body> invalidState = freeBodies;
    invalidState[0].position.x = std::numeric_limits<double>::quiet_NaN();
    const auto invalidInteractions = PhysicsEngine::inspectInteractions(invalidState, adaptive.closeApproach);
    assert(invalidInteractions.closeApproach.unstable);

    std::vector<Body> rollbackState = freeBodies;
    rollbackState[0].position.x = std::numeric_limits<double>::infinity();
    const PhysicsStepResult rollback = physics.integrate(rollbackState, 1.0, Integrator::RK4, adaptive.closeApproach);
    assert(!rollback.success);
    assert(std::isinf(rollbackState[0].position.x));

    IntegratorBenchmarkConfig benchmarkConfig;
    benchmarkConfig.timestep = 3600.0;
    benchmarkConfig.duration = PhysicsEngine::DAY;
    benchmarkConfig.referenceTimestep = 900.0;
    const auto benchmark = compareIntegrators(earthSystem(), benchmarkConfig);
    assert(benchmark.size() == 4);
    for (const auto& metrics : benchmark) {
        assert(metrics.valid);
        assert(metrics.integrationSteps == 24);
        assert(std::isfinite(metrics.energyDrift));
        assert(std::isfinite(metrics.angularMomentumDrift));
        assert(std::isfinite(metrics.positionError));
        assert(std::isfinite(metrics.velocityError));
        assert(std::isfinite(metrics.orbitalPeriodError));
    }

    return 0;
}
