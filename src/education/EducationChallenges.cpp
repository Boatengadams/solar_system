#include "EducationChallenges.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include "../physics/IntegratorBenchmark.hpp"
#include "../validation/ValidationCases.hpp"

namespace bag {
namespace {

constexpr double PI = 3.14159265358979323846;

double clampScore(double score) {
    return std::max(0.0, std::min(100.0, score));
}

double relativeError(double actual, double expected) {
    if (!std::isfinite(actual) || !std::isfinite(expected)) return std::numeric_limits<double>::infinity();
    return std::abs(actual - expected) / std::max(std::abs(expected), 1.0e-12);
}

double scoreRelativeErrorInternal(double error, const ChallengeScoringRules& rules) {
    if (!std::isfinite(error) || rules.fullCreditRelativeError < 0.0 ||
        rules.passingRelativeError < rules.fullCreditRelativeError) return 0.0;
    if (error <= rules.fullCreditRelativeError) return 100.0;
    if (error >= rules.passingRelativeError) return 0.0;
    const double span = rules.passingRelativeError - rules.fullCreditRelativeError;
    return clampScore(100.0 * (rules.passingRelativeError - error) / span);
}

bool finiteDefinition(const ChallengeDefinition& challenge) {
    const ChallengeScoringRules& rules = challenge.scoring;
    return !challenge.id.empty() && !challenge.title.empty() && !challenge.description.empty() &&
        !challenge.learningObjective.empty() && std::isfinite(challenge.centralMassKg) &&
        challenge.centralMassKg > 0.0 && std::isfinite(challenge.innerRadiusM) &&
        challenge.innerRadiusM > 0.0 && std::isfinite(challenge.outerRadiusM) &&
        challenge.outerRadiusM > 0.0 && std::isfinite(challenge.centralBodyRadiusM) &&
        challenge.centralBodyRadiusM >= 0.0 && std::isfinite(challenge.durationSeconds) &&
        challenge.durationSeconds > 0.0 && std::isfinite(challenge.referenceTimestepSeconds) &&
        challenge.referenceTimestepSeconds > 0.0 && std::isfinite(challenge.defaultTimestepSeconds) &&
        challenge.defaultTimestepSeconds > 0.0 && std::isfinite(challenge.defaultAnswer) &&
        rules.fullCreditRelativeError >= 0.0 && rules.passingRelativeError >= rules.fullCreditRelativeError &&
        rules.energyWeight >= 0.0 && rules.positionWeight >= 0.0 && rules.velocityWeight >= 0.0 &&
        rules.stabilityWeight >= 0.0 && rules.stabilityThreshold >= 0.0;
}

ChallengeDefinition makeEscapeChallenge() {
    ChallengeDefinition result;
    result.id = "escape-velocity";
    result.title = "Escape velocity";
    result.description = "Choose the speed that makes total specific orbital energy reach zero.";
    result.learningObjective = "Relate escape velocity to gravitational potential and kinetic energy.";
    result.scenario = "A probe starts one astronomical unit from the Sun.";
    result.kind = ChallengeKind::EscapeVelocity;
    result.defaultAnswer = std::sqrt(2.0 * PhysicsEngine::G * result.centralMassKg / result.innerRadiusM);
    result.scoring.fullCreditRelativeError = 0.01;
    result.scoring.passingRelativeError = 0.05;
    result.hints = {"Escape is the zero-specific-energy boundary.", "Use v = sqrt(2GM/r)."};
    result.explanation = "Below the target, specific orbital energy is negative and the trajectory is bound; at the target it is zero.";
    result.nextStep = "Try a speed just below and just above the boundary and compare the orbit classification.";
    return result;
}

ChallengeDefinition makeCircularChallenge() {
    ChallengeDefinition result;
    result.id = "circular-orbit";
    result.title = "Circular orbit";
    result.description = "Choose the tangential velocity for a circular orbit at one astronomical unit.";
    result.learningObjective = "Connect orbital speed with radius and central gravitational parameter.";
    result.scenario = "A small body moves tangentially around a point-mass Sun.";
    result.kind = ChallengeKind::CircularOrbit;
    result.defaultAnswer = std::sqrt(PhysicsEngine::G * result.centralMassKg / result.innerRadiusM);
    result.scoring.fullCreditRelativeError = 0.005;
    result.scoring.passingRelativeError = 0.02;
    result.hints = {"A circular orbit has zero radial velocity.", "Use v = sqrt(GM/r)."};
    result.explanation = "The circular speed supplies exactly the centripetal acceleration needed at this radius.";
    result.nextStep = "Change the radius mentally: farther circular orbits move more slowly.";
    return result;
}

ChallengeDefinition makeIntegratorChallenge() {
    ChallengeDefinition result;
    result.id = "integrator-comparison";
    result.title = "Choose a numerical method";
    result.description = "Select the method with the lowest documented error for this orbit and timestep.";
    result.learningObjective = "Compare stability, conservation, and truncation error without calling one method universally best.";
    result.scenario = "A 30-day Sun/Earth two-body orbit uses a six-hour timestep.";
    result.kind = ChallengeKind::IntegratorComparison;
    result.defaultAnswer = 0.0;
    result.defaultIntegrator = Integrator::VelocityVerlet;
    result.scoring.energyWeight = 0.35;
    result.scoring.positionWeight = 0.35;
    result.scoring.velocityWeight = 0.20;
    result.scoring.stabilityWeight = 0.10;
    result.scoring.stabilityThreshold = 1.0;
    result.scoring.fullCreditRelativeError = 0.25;
    result.scoring.passingRelativeError = 1.0;
    result.hints = {"Inspect both long-duration conservation and endpoint error.", "The reference is a smaller-step RK4 trajectory, not reality."};
    result.explanation = "Euler, Semi-Implicit Euler, Velocity-Verlet, and RK4 trade cost, stability, and error differently; the result depends on this problem and timestep.";
    result.nextStep = "Repeat with a smaller timestep and see whether the ranking changes.";
    return result;
}

ChallengeDefinition makeTimestepChallenge() {
    ChallengeDefinition result;
    result.id = "timestep-selection";
    result.title = "Choose a timestep";
    result.description = "Choose a timestep that keeps a Velocity-Verlet orbit accurate and stable.";
    result.learningObjective = "Observe how timestep size changes truncation error and long-duration orbital behavior.";
    result.scenario = "A 30-day Sun/Earth orbit is propagated with Velocity-Verlet.";
    result.kind = ChallengeKind::TimestepSelection;
    result.defaultAnswer = result.defaultTimestepSeconds;
    result.scoring.fullCreditRelativeError = 0.25;
    result.scoring.passingRelativeError = 1.0;
    result.scoring.energyWeight = 0.25;
    result.scoring.positionWeight = 0.50;
    result.scoring.velocityWeight = 0.25;
    result.scoring.stabilityWeight = 0.0;
    result.scoring.stabilityThreshold = 1.0;
    result.hints = {"A smaller step is not automatically the best engineering choice.", "Compare error against the computational work implied by step count."};
    result.explanation = "Reducing timestep generally lowers local truncation error, but the useful choice balances accuracy, stability, and computation.";
    result.nextStep = "Halve the timestep and compare the score and number of integration steps.";
    return result;
}

ChallengeDefinition makeHohmannChallenge() {
    ChallengeDefinition result;
    result.id = "hohmann-transfer";
    result.title = "Hohmann transfer delta-v";
    result.description = "Find the total delta-v for a coplanar transfer from Earth's orbit to Mars's orbit.";
    result.learningObjective = "Connect circular velocity, the transfer ellipse, and the two efficient tangential burns.";
    result.scenario = "Radii are measured from the Sun's center; both initial and target orbits are circular and coplanar.";
    result.difficulty = ChallengeDifficulty::Intermediate;
    result.kind = ChallengeKind::HohmannTransfer;
    result.centralBodyRadiusM = 6.957e8;
    result.defaultAnswer = PhysicsEngine::hohmannTransfer(result.innerRadiusM, result.outerRadiusM, result.centralMassKg).totalDeltaV;
    result.scoring.fullCreditRelativeError = 0.01;
    result.scoring.passingRelativeError = 0.05;
    result.hints = {"The first burn raises apoapsis; the second burn circularizes there.", "Use the total of the two tangential burns."};
    result.explanation = "A Hohmann transfer uses an ellipse tangent to both circular orbits. It is delta-v efficient for coplanar circular orbits under the point-mass model, not a universal optimum for every mission.";
    result.nextStep = "Inspect the two reference burns separately and compare their magnitudes with the total.";
    return result;
}

const std::vector<ChallengeDefinition>& catalog() {
    static const std::vector<ChallengeDefinition> challenges = {
        makeEscapeChallenge(), makeCircularChallenge(), makeIntegratorChallenge(), makeTimestepChallenge(), makeHohmannChallenge()};
    return challenges;
}

std::vector<Body> orbitFixture() {
    const double sunMass = PhysicsEngine::SOLAR_MASS;
    const double earthMass = PhysicsEngine::EARTH_MASS;
    const double mu = PhysicsEngine::G * (sunMass + earthMass);
    const double speed = std::sqrt(mu / PhysicsEngine::AU);
    const double sunFraction = earthMass / (sunMass + earthMass);
    const double earthFraction = sunMass / (sunMass + earthMass);
    Body sun;
    sun.id = "challenge-sun";
    sun.mass = sunMass;
    sun.position = {-sunFraction * PhysicsEngine::AU, 0.0, 0.0};
    sun.velocity = {0.0, -sunFraction * speed, 0.0};
    Body earth;
    earth.id = "challenge-earth";
    earth.mass = earthMass;
    earth.position = {earthFraction * PhysicsEngine::AU, 0.0, 0.0};
    earth.velocity = {0.0, earthFraction * speed, 0.0};
    return {sun, earth};
}

void finishText(ChallengeResult& result, const ChallengeDefinition& challenge) {
    result.grade = challengeGrade(result.score, result.passed);
    result.explanation = challenge.explanation;
    result.nextStep = challenge.nextStep;
}

ChallengeResult evaluateScalar(const ChallengeDefinition& challenge, const ChallengeAnswer& answer,
                               double expected, const std::string& name, bool secondary = false) {
    ChallengeResult result;
    if (!validChallengeDefinition(challenge)) return result;
    const double actual = secondary ? answer.secondaryValue : answer.primaryValue;
    const double error = relativeError(actual, expected);
    result.valid = std::isfinite(error);
    result.metrics.learnerPrimaryValue = actual;
    result.metrics.expectedPrimaryValue = expected;
    result.metrics.absolutePrimaryError = std::abs(actual - expected);
    if (secondary) result.metrics.secondaryRelativeError = error;
    else result.metrics.primaryRelativeError = error;
    result.score = scoreRelativeError(error, challenge.scoring);
    result.passed = result.valid && error <= challenge.scoring.passingRelativeError;
    result.objectiveResult = name + (result.passed ? " is within the documented tolerance." : " is outside the documented tolerance.");
    result.feedback = result.passed ? "Good: the answer respects the analytical reference." : "Compare the answer with the analytical reference and inspect the hint.";
    finishText(result, challenge);
    return result;
}

ChallengeResult evaluateHohmann(const ChallengeDefinition& challenge, const ChallengeAnswer& answer) {
    ChallengeResult result;
    if (!validChallengeDefinition(challenge)) return result;
    const HohmannTransfer reference = PhysicsEngine::hohmannTransfer(
        challenge.innerRadiusM, challenge.outerRadiusM, challenge.centralMassKg);
    if (!reference.valid || !std::isfinite(answer.primaryValue)) return result;
    result.valid = true;
    result.metrics.learnerPrimaryValue = answer.primaryValue;
    result.metrics.expectedPrimaryValue = reference.totalDeltaV;
    result.metrics.absolutePrimaryError = std::abs(answer.primaryValue - reference.totalDeltaV);
    result.metrics.primaryRelativeError = relativeError(answer.primaryValue, reference.totalDeltaV);
    result.metrics.referenceDepartureDeltaV = reference.departureDeltaV;
    result.metrics.referenceArrivalDeltaV = reference.arrivalDeltaV;
    result.metrics.referenceTotalDeltaV = reference.totalDeltaV;
    result.score = scoreRelativeError(result.metrics.primaryRelativeError, challenge.scoring);
    result.passed = result.metrics.primaryRelativeError <= challenge.scoring.passingRelativeError;
    result.objectiveResult = result.passed ? "Total delta-v is within the documented Hohmann reference tolerance."
                                           : "Total delta-v is outside the documented Hohmann reference tolerance.";
    result.feedback = result.passed ? "Good: the answer matches the two-burn analytical reference."
                                    : "Compare the total with the departure and arrival burn components.";
    finishText(result, challenge);
    return result;
}

ChallengeResult scoreNumericalMetrics(const ChallengeDefinition& challenge,
                                      const std::vector<IntegratorBenchmarkMetrics>& metrics,
                                      Integrator selectedIntegrator) {
    ChallengeResult result;
    if (!validChallengeDefinition(challenge)) return result;
    if (metrics.empty()) return result;

    const auto makeAssessment = [&](const IntegratorBenchmarkMetrics& metric) {
        IntegratorAssessment assessment;
        assessment.integrator = metric.integrator;
        assessment.valid = metric.valid && std::isfinite(metric.energyDrift) &&
            std::isfinite(metric.positionError) && std::isfinite(metric.velocityError);
        assessment.numericallyStable = assessment.valid && metric.energyDrift <= challenge.scoring.stabilityThreshold;
        const double velocityScale = std::sqrt(PhysicsEngine::G * PhysicsEngine::SOLAR_MASS / PhysicsEngine::AU);
        const double normalized = challenge.scoring.energyWeight * metric.energyDrift +
            challenge.scoring.positionWeight * metric.positionError / PhysicsEngine::AU +
            challenge.scoring.velocityWeight * metric.velocityError / velocityScale +
            challenge.scoring.stabilityWeight * (assessment.numericallyStable ? 0.0 : challenge.scoring.stabilityThreshold);
        assessment.normalizedError = assessment.valid ? normalized : std::numeric_limits<double>::infinity();
        assessment.score = scoreRelativeError(assessment.normalizedError, challenge.scoring);
        return assessment;
    };

    for (const auto& metric : metrics) result.comparison.push_back(makeAssessment(metric));
    const auto selected = std::find_if(metrics.begin(), metrics.end(), [&](const IntegratorBenchmarkMetrics& metric) {
        return metric.integrator == selectedIntegrator;
    });
    if (selected == metrics.end()) return result;
    const IntegratorAssessment assessment = makeAssessment(*selected);
    result.valid = assessment.valid;
    result.score = assessment.score;
    result.passed = result.valid && assessment.normalizedError <= challenge.scoring.passingRelativeError;
    result.metrics.energyDrift = selected->energyDrift;
    result.metrics.angularMomentumDrift = selected->angularMomentumDrift;
    result.metrics.positionError = selected->positionError;
    result.metrics.velocityError = selected->velocityError;
    result.metrics.orbitalPeriodError = selected->orbitalPeriodError;
    result.metrics.normalizedNumericalError = assessment.normalizedError;
    result.metrics.numericallyStable = assessment.numericallyStable;
    result.objectiveResult = std::string(integratorName(selectedIntegrator)) +
        (result.passed ? " stays within the challenge error envelope." : " exceeds the challenge error envelope.");
    result.feedback = result.passed ? "This method is a defensible choice for this timestep and orbit." : "Try another method or reduce the timestep; inspect energy and endpoint errors.";
    finishText(result, challenge);
    return result;
}

ChallengeResult evaluateNumerical(const ChallengeDefinition& challenge, const ChallengeAnswer& answer) {
    if (!validChallengeDefinition(challenge)) return {};
    const std::vector<Body> bodies = orbitFixture();
    IntegratorBenchmarkConfig config;
    config.timestep = challenge.kind == ChallengeKind::TimestepSelection && answer.timestepSeconds > 0.0
        ? answer.timestepSeconds : challenge.defaultTimestepSeconds;
    config.duration = challenge.durationSeconds;
    config.referenceTimestep = challenge.referenceTimestepSeconds;
    const auto metrics = compareIntegrators(bodies, config);
    return scoreNumericalMetrics(challenge, metrics, answer.integrator);
}

} // namespace

double scoreRelativeError(double error, const ChallengeScoringRules& rules) {
    return scoreRelativeErrorInternal(error, rules);
}

const ChallengeDefinition& challengeAt(int index) {
    const auto& challenges = catalog();
    const int safeIndex = ((index % static_cast<int>(challenges.size())) + static_cast<int>(challenges.size())) % static_cast<int>(challenges.size());
    return challenges[static_cast<std::size_t>(safeIndex)];
}

const ChallengeDefinition* findChallenge(const std::string& id) {
    const auto& challenges = catalog();
    const auto found = std::find_if(challenges.begin(), challenges.end(), [&](const ChallengeDefinition& challenge) { return challenge.id == id; });
    return found == challenges.end() ? nullptr : &*found;
}

int challengeCount() { return static_cast<int>(catalog().size()); }

bool validChallengeDefinition(const ChallengeDefinition& challenge) {
    if (!finiteDefinition(challenge)) return false;
    if (challenge.kind == ChallengeKind::HohmannTransfer &&
        (challenge.centralBodyRadiusM <= 0.0 || challenge.innerRadiusM <= challenge.centralBodyRadiusM ||
         challenge.outerRadiusM <= challenge.centralBodyRadiusM || challenge.innerRadiusM == challenge.outerRadiusM)) return false;
    if (challenge.kind == ChallengeKind::IntegratorComparison || challenge.kind == ChallengeKind::TimestepSelection) {
        const double totalWeight = challenge.scoring.energyWeight + challenge.scoring.positionWeight +
            challenge.scoring.velocityWeight + challenge.scoring.stabilityWeight;
        if (totalWeight <= 0.0 || !std::isfinite(totalWeight) || std::abs(totalWeight - 1.0) > 1.0e-9) return false;
    }
    return true;
}

ChallengeResult evaluateChallenge(const ChallengeDefinition& challenge, const ChallengeAnswer& answer) {
    switch (challenge.kind) {
    case ChallengeKind::EscapeVelocity:
        return evaluateScalar(challenge, answer, std::sqrt(2.0 * PhysicsEngine::G * challenge.centralMassKg / challenge.innerRadiusM), "Escape velocity");
    case ChallengeKind::CircularOrbit:
        return evaluateScalar(challenge, answer, std::sqrt(PhysicsEngine::G * challenge.centralMassKg / challenge.innerRadiusM), "Circular-orbit velocity");
    case ChallengeKind::IntegratorComparison:
    case ChallengeKind::TimestepSelection:
        return evaluateNumerical(challenge, answer);
    case ChallengeKind::HohmannTransfer:
        return evaluateHohmann(challenge, answer);
    }
    return {};
}

ChallengeResult evaluateIntegratorMetrics(const ChallengeDefinition& challenge,
                                           const std::vector<IntegratorBenchmarkMetrics>& metrics,
                                           Integrator selectedIntegrator) {
    return scoreNumericalMetrics(challenge, metrics, selectedIntegrator);
}

const char* challengeDifficultyName(ChallengeDifficulty difficulty) {
    switch (difficulty) {
    case ChallengeDifficulty::Introductory: return "introductory";
    case ChallengeDifficulty::Intermediate: return "intermediate";
    case ChallengeDifficulty::Advanced: return "advanced";
    }
    return "unknown";
}

const char* challengeKindName(ChallengeKind kind) {
    switch (kind) {
    case ChallengeKind::EscapeVelocity: return "escape_velocity";
    case ChallengeKind::CircularOrbit: return "circular_orbit";
    case ChallengeKind::IntegratorComparison: return "integrator_comparison";
    case ChallengeKind::TimestepSelection: return "timestep_selection";
    case ChallengeKind::HohmannTransfer: return "hohmann_transfer";
    }
    return "unknown";
}

const char* challengeGrade(double score, bool passed) {
    if (!std::isfinite(score) || !passed) return "RETRY";
    if (score >= 90.0) return "EXCELLENT";
    if (score >= 75.0) return "SOLID";
    return "PASS";
}

} // namespace bag
