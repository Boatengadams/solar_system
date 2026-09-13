#include "ValidationRunner.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include "ValidationCases.hpp"
#include "ValidationMetrics.hpp"

namespace bag {
namespace {

struct Propagation {
    bool valid = false;
    std::size_t steps = 0;
    std::vector<Body> finalBodies;
    ValidationMetrics metrics;
    double minimumRadius = 0.0;
    double maximumRadius = 0.0;
    double maximumSpeedDrift = 0.0;
};

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

Vec3 relativePosition(const std::vector<Body>& bodies) { return bodies[1].position - bodies[0].position; }
Vec3 relativeVelocity(const std::vector<Body>& bodies) { return bodies[1].velocity - bodies[0].velocity; }

bool finiteBodies(const std::vector<Body>& bodies) {
    for (const Body& body : bodies) {
        if (!body.active || !finite(body.mass) || !finite(body.position.x) || !finite(body.position.y) || !finite(body.position.z) ||
            !finite(body.velocity.x) || !finite(body.velocity.y) || !finite(body.velocity.z)) return false;
    }
    return true;
}

Propagation propagate(const ValidationCase& testCase, Integrator integrator, double timestep, double duration,
                       bool measurePeriod) {
    Propagation result;
    if (testCase.bodies.size() != 2 || timestep <= 0.0 || duration <= 0.0) return result;
    PhysicsEngine engine;
    std::vector<Body> bodies = testCase.bodies;
    const double initialEnergy = PhysicsEngine::totalEnergy(bodies);
    const Vec3 initialAngularMomentum = angularMomentum(bodies);
    const double angularScale = std::max(length(initialAngularMomentum), 1.0);
    double elapsed = 0.0;
    const double initialRadius = length(relativePosition(bodies));
    const double initialSpeed = length(relativeVelocity(bodies));
    result.minimumRadius = initialRadius;
    result.maximumRadius = initialRadius;
    double maximumEnergyDrift = 0.0;
    double maximumAngularDrift = 0.0;
    double previousAngle = std::atan2(relativePosition(bodies).y, relativePosition(bodies).x);
    double unwrappedAngle = previousAngle;
    double measuredPeriod = 0.0;
    while (elapsed < duration) {
        const double step = std::min(timestep, duration - elapsed);
        const PhysicsStepResult stepResult = engine.integrate(bodies, step, integrator);
        if (!stepResult.success || !finiteBodies(bodies)) return result;
        elapsed += step;
        ++result.steps;
        const double radius = length(relativePosition(bodies));
        result.minimumRadius = std::min(result.minimumRadius, radius);
        result.maximumRadius = std::max(result.maximumRadius, radius);
        result.maximumSpeedDrift = std::max(result.maximumSpeedDrift, relativeError(length(relativeVelocity(bodies)), initialSpeed, initialSpeed));
        const double currentEnergy = PhysicsEngine::totalEnergy(bodies);
        maximumEnergyDrift = std::max(maximumEnergyDrift, relativeError(currentEnergy, initialEnergy, std::max(std::abs(initialEnergy), 1.0)));
        maximumAngularDrift = std::max(maximumAngularDrift, relativeError(length(angularMomentum(bodies)), length(initialAngularMomentum), angularScale));
        if (measurePeriod && measuredPeriod == 0.0) {
            const double angle = std::atan2(relativePosition(bodies).y, relativePosition(bodies).x);
            double delta = angle - previousAngle;
            if (delta < -3.14159265358979323846) delta += 2.0 * 3.14159265358979323846;
            if (delta > 3.14159265358979323846) delta -= 2.0 * 3.14159265358979323846;
            unwrappedAngle += delta;
            if (unwrappedAngle >= 2.0 * 3.14159265358979323846) measuredPeriod = elapsed;
            previousAngle = angle;
        }
    }
    result.valid = true;
    result.finalBodies = std::move(bodies);
    result.metrics.energyDrift = maximumEnergyDrift;
    result.metrics.angularMomentumDrift = maximumAngularDrift;
    result.metrics.measuredPeriod = measuredPeriod;
    result.metrics.analyticalPeriod = testCase.analyticalPeriod;
    if (measuredPeriod > 0.0) result.metrics.orbitalPeriodError = relativeError(measuredPeriod, testCase.analyticalPeriod);
    return result;
}

ValidationResult makeResult(const std::string& name, const ValidationCase& testCase, Integrator integrator,
                            double timestep, double duration, double tolerance, const Propagation& propagation,
                            bool passed, const std::string& message) {
    ValidationResult result;
    result.caseName = name;
    result.initialStateId = testCase.id;
    result.integrator = integrator;
    result.status = propagation.valid ? (passed ? ValidationStatus::Pass : ValidationStatus::NumericalFailure) : ValidationStatus::NumericalFailure;
    result.passed = passed;
    result.timestep = timestep;
    result.duration = duration;
    result.integrationSteps = propagation.steps;
    result.tolerance = tolerance;
    result.metrics = propagation.metrics;
    result.message = message;
    return result;
}

void addAnalyticalClassification(ValidationReport& report, const ValidationTolerances& tolerances) {
    const ValidationCase circular = circularOrbitCase();
    const Body& initialBody = circular.bodies[1];
    const OrbitalElements circularElements = PhysicsEngine::orbitalElements(initialBody, PhysicsEngine::SOLAR_MASS + PhysicsEngine::EARTH_MASS);
    report.results.push_back({"analytical-circular-elements", circular.id, Integrator::RK4,
        ValidationStatus::Pass, circularElements.valid && circularElements.type == OrbitType::Elliptical,
        0.0, 0.0, 0, tolerances.ellipticalElementRelative, {},
        circularElements.valid ? "circular state is a zero-eccentricity bound orbit" : "invalid circular analytical state"});

    Body hyperbolic = initialBody;
    hyperbolic.velocity = {0.0, PhysicsEngine::escapeVelocity(hyperbolic) * 1.2, 0.0};
    const OrbitalElements hyperbolicElements = PhysicsEngine::orbitalElements(hyperbolic);
    const bool hyperbolicPass = hyperbolicElements.valid && hyperbolicElements.type == OrbitType::Hyperbolic && hyperbolicElements.eccentricity > 1.0 && PhysicsEngine::specificEnergy(hyperbolic) > 0.0;
    report.results.push_back({"analytical-hyperbolic-classification", circular.id, Integrator::RK4,
        hyperbolicPass ? ValidationStatus::Pass : ValidationStatus::NumericalFailure, hyperbolicPass, 0.0, 0.0, 0, tolerances.hyperbolicEnergyRelative, {},
        hyperbolicPass ? "positive-energy state remains hyperbolic" : "hyperbolic state was misclassified"});
}

void addOrbitValidation(ValidationReport& report, const ValidationTolerances& tolerances,
                        const ValidationCase& testCase, Integrator integrator, double timestep) {
    const double duration = testCase.analyticalPeriod * 1.2;
    const Propagation propagation = propagate(testCase, integrator, timestep, duration, true);
    const Propagation reference = propagate(testCase, Integrator::RK4, timestep / 8.0, duration, false);
    ValidationMetrics metrics = propagation.metrics;
    if (propagation.valid) {
        const double initialRadius = length(relativePosition(testCase.bodies));
        const double finalRadius = length(relativePosition(propagation.finalBodies));
        metrics.relativeError = relativeError(finalRadius, initialRadius, initialRadius);
        if (reference.valid) {
            metrics.positionError = length(relativePosition(propagation.finalBodies) - relativePosition(reference.finalBodies));
            metrics.velocityError = length(relativeVelocity(propagation.finalBodies) - relativeVelocity(reference.finalBodies));
        } else {
            metrics.positionError = std::numeric_limits<double>::infinity();
            metrics.velocityError = std::numeric_limits<double>::infinity();
        }
    }
    const bool circularPass = testCase.eccentricity == 0.0 && metrics.relativeError <= tolerances.circularRadiusRelative;
    const double periapsisError = propagation.valid ? relativeError(propagation.minimumRadius, testCase.semiMajorAxis * (1.0 - testCase.eccentricity), testCase.semiMajorAxis) : std::numeric_limits<double>::infinity();
    const double apoapsisError = propagation.valid ? relativeError(propagation.maximumRadius, testCase.semiMajorAxis * (1.0 + testCase.eccentricity), testCase.semiMajorAxis) : std::numeric_limits<double>::infinity();
    const bool ellipticalPass = testCase.eccentricity > 0.0 && periapsisError <= tolerances.ellipticalElementRelative && apoapsisError <= tolerances.ellipticalElementRelative;
    Body relativeState;
    if (propagation.valid) {
        relativeState.position = relativePosition(propagation.finalBodies);
        relativeState.velocity = relativeVelocity(propagation.finalBodies);
    }
    const OrbitalElements finalElements = propagation.valid ? PhysicsEngine::orbitalElements(relativeState, PhysicsEngine::SOLAR_MASS + PhysicsEngine::EARTH_MASS) : OrbitalElements{};
    const double semiMajorAxisError = finalElements.valid ? relativeError(finalElements.semiMajorAxis, testCase.semiMajorAxis, testCase.semiMajorAxis) : std::numeric_limits<double>::infinity();
    const double eccentricityError = finalElements.valid ? absoluteError(finalElements.eccentricity, testCase.eccentricity) : std::numeric_limits<double>::infinity();
    const bool pass = propagation.valid && (circularPass || ellipticalPass) &&
        (testCase.eccentricity > 0.0 ? (semiMajorAxisError <= tolerances.ellipticalElementRelative && eccentricityError <= tolerances.ellipticalElementRelative) : propagation.maximumSpeedDrift <= tolerances.circularRadiusRelative) &&
        metrics.energyDrift <= tolerances.circularEnergyRelative && metrics.angularMomentumDrift <= tolerances.circularAngularMomentumRelative &&
        (metrics.measuredPeriod == 0.0 || metrics.orbitalPeriodError <= tolerances.periodRelative);
    std::ostringstream orbitMessage;
    orbitMessage << (pass ? "bounded orbit remained finite" : "bounded orbit exceeded conservation envelope")
                 << "; periapsis_error=" << periapsisError << "; apoapsis_error=" << apoapsisError
                 << "; semi_major_axis_error=" << semiMajorAxisError << "; eccentricity_error=" << eccentricityError
                 << "; speed_drift=" << propagation.maximumSpeedDrift;
    ValidationResult result = makeResult(testCase.id + "-bounded-orbit", testCase, integrator, timestep, duration,
        tolerances.circularRadiusRelative, propagation, pass, orbitMessage.str());
    result.metrics = metrics;
    result.metrics.absoluteError = periapsisError;
    result.metrics.maximumError = apoapsisError;
    report.results.push_back(std::move(result));
}

void addConvergenceValidation(ValidationReport& report, const ValidationTolerances& tolerances) {
    const ValidationCase testCase = circularOrbitCase();
    const double duration = testCase.analyticalPeriod;
    const double coarseStep = 6.0 * 3600.0;
    const double fineStep = coarseStep / 2.0;
    const Propagation coarse = propagate(testCase, Integrator::VelocityVerlet, coarseStep, duration, false);
    const Propagation fine = propagate(testCase, Integrator::VelocityVerlet, fineStep, duration, false);
    const Propagation reference = propagate(testCase, Integrator::RK4, fineStep / 4.0, duration, false);
    ValidationResult result;
    result.caseName = "velocity-verlet-timestep-convergence";
    result.initialStateId = testCase.id;
    result.integrator = Integrator::VelocityVerlet;
    result.timestep = fineStep;
    result.duration = duration;
    result.integrationSteps = fine.steps;
    result.tolerance = tolerances.convergenceImprovement;
    if (coarse.valid && fine.valid && reference.valid) {
        const double positionScale = length(relativePosition(testCase.bodies));
        const double coarsePositionError = relativeError(length(relativePosition(coarse.finalBodies) - relativePosition(reference.finalBodies)), 0.0, positionScale);
        const double finePositionError = relativeError(length(relativePosition(fine.finalBodies) - relativePosition(reference.finalBodies)), 0.0, positionScale);
        result.metrics.absoluteError = coarsePositionError;
        result.metrics.relativeError = finePositionError;
        result.passed = finePositionError <= coarsePositionError * (1.0 - tolerances.convergenceImprovement);
        result.status = result.passed ? ValidationStatus::Pass : ValidationStatus::NumericalFailure;
        result.message = result.passed ? "halving timestep reduced endpoint radius error" : "timestep refinement did not improve endpoint error";
    } else {
        result.status = ValidationStatus::NumericalFailure;
        result.message = "timestep refinement propagation failed";
    }
    report.results.push_back(std::move(result));
}

} // namespace

const char* validationStatusName(ValidationStatus status) {
    switch (status) {
    case ValidationStatus::Pass: return "PASS";
    case ValidationStatus::ExpectedFailure: return "EXPECTED_FAILURE";
    case ValidationStatus::NumericalFailure: return "NUMERICAL_FAILURE";
    case ValidationStatus::InvalidInput: return "INVALID_INPUT";
    }
    return "INVALID_INPUT";
}

bool ValidationReport::allPassed() const {
    return !results.empty() && std::all_of(results.begin(), results.end(), [](const ValidationResult& result) { return result.passed && result.status == ValidationStatus::Pass; });
}

std::string ValidationReport::toCsv() const {
    std::ostringstream output;
    output << "case,initial_state,integrator,status,passed,timestep_s,duration_s,steps,tolerance,absolute_error,relative_error,maximum_error,rms_error,energy_drift,angular_momentum_drift,position_error_m,velocity_error_mps,period_error,analytical_period_s,measured_period_s,message\n";
    output << std::setprecision(17);
    for (const ValidationResult& result : results) {
        output << result.caseName << ',' << result.initialStateId << ',' << integratorName(result.integrator) << ','
               << validationStatusName(result.status) << ',' << (result.passed ? "true" : "false") << ','
               << result.timestep << ',' << result.duration << ',' << result.integrationSteps << ',' << result.tolerance << ','
               << result.metrics.absoluteError << ',' << result.metrics.relativeError << ',' << result.metrics.maximumError << ','
               << result.metrics.rmsError << ',' << result.metrics.energyDrift << ',' << result.metrics.angularMomentumDrift << ','
               << result.metrics.positionError << ',' << result.metrics.velocityError << ',' << result.metrics.orbitalPeriodError << ','
               << result.metrics.analyticalPeriod << ',' << result.metrics.measuredPeriod << ',' << result.message << '\n';
    }
    return output.str();
}

ValidationReport runValidationSuite(const ValidationTolerances& tolerances) {
    ValidationReport report;
    addAnalyticalClassification(report, tolerances);
    const ValidationCase circular = circularOrbitCase();
    for (const Integrator integrator : {Integrator::Euler, Integrator::SemiImplicitEuler, Integrator::VelocityVerlet, Integrator::RK4}) {
        addOrbitValidation(report, tolerances, circular, integrator, 6.0 * 3600.0);
    }
    addOrbitValidation(report, tolerances, ellipticalOrbitCase(), Integrator::VelocityVerlet, 3.0 * 3600.0);
    addConvergenceValidation(report, tolerances);
    return report;
}

bool writeValidationCsv(const std::filesystem::path& path, const ValidationReport& report) {
    std::ofstream output(path);
    if (!output) return false;
    output << report.toCsv();
    return static_cast<bool>(output);
}

} // namespace bag
