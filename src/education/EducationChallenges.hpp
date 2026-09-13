#pragma once

#include <string>
#include <vector>

#include "../physics/PhysicsEngine.hpp"

namespace bag {

enum class ChallengeDifficulty {
    Introductory,
    Intermediate,
    Advanced,
};

enum class ChallengeKind {
    EscapeVelocity,
    CircularOrbit,
    IntegratorComparison,
    TimestepSelection,
    HohmannTransfer,
};

struct ChallengeScoringRules {
    // Relative error is dimensionless. The thresholds are part of each
    // challenge definition so scoring is inspectable rather than hidden.
    double fullCreditRelativeError = 0.01;
    double passingRelativeError = 0.05;
    double energyWeight = 0.0;
    double positionWeight = 0.0;
    double velocityWeight = 0.0;
    double stabilityWeight = 0.0;
    double stabilityThreshold = 0.0;
};

struct ChallengeDefinition {
    std::string id;
    std::string title;
    std::string description;
    std::string learningObjective;
    std::string scenario;
    ChallengeDifficulty difficulty = ChallengeDifficulty::Introductory;
    ChallengeKind kind = ChallengeKind::EscapeVelocity;
    double centralMassKg = PhysicsEngine::SOLAR_MASS;
    double innerRadiusM = PhysicsEngine::AU;
    double outerRadiusM = 1.524 * PhysicsEngine::AU;
    double centralBodyRadiusM = 0.0;
    double durationSeconds = 30.0 * PhysicsEngine::DAY;
    double referenceTimestepSeconds = 3.0 * 3600.0;
    double defaultTimestepSeconds = 6.0 * 3600.0;
    double defaultAnswer = 0.0;
    Integrator defaultIntegrator = Integrator::VelocityVerlet;
    ChallengeScoringRules scoring;
    std::vector<std::string> hints;
    std::string explanation;
    std::string nextStep;
};

struct ChallengeAnswer {
    // primaryValue and secondaryValue are SI units where applicable.
    double primaryValue = 0.0;
    double secondaryValue = 0.0;
    double timestepSeconds = 0.0;
    Integrator integrator = Integrator::VelocityVerlet;
};

struct ChallengeMetrics {
    double learnerPrimaryValue = 0.0;
    double learnerSecondaryValue = 0.0;
    double expectedPrimaryValue = 0.0;
    double expectedSecondaryValue = 0.0;
    double absolutePrimaryError = 0.0;
    double absoluteSecondaryError = 0.0;
    double primaryRelativeError = 0.0;
    double secondaryRelativeError = 0.0;
    double energyDrift = 0.0;
    double angularMomentumDrift = 0.0;
    double positionError = 0.0;
    double velocityError = 0.0;
    double orbitalPeriodError = 0.0;
    double normalizedNumericalError = 0.0;
    double referenceDepartureDeltaV = 0.0;
    double referenceArrivalDeltaV = 0.0;
    double referenceTotalDeltaV = 0.0;
    bool numericallyStable = false;
};

struct IntegratorAssessment {
    Integrator integrator = Integrator::VelocityVerlet;
    bool valid = false;
    bool numericallyStable = false;
    double normalizedError = 0.0;
    double score = 0.0;
};

struct ChallengeResult {
    bool valid = false;
    bool passed = false;
    double score = 0.0;
    std::string grade = "INVALID";
    ChallengeMetrics metrics;
    std::vector<IntegratorAssessment> comparison;
    std::string objectiveResult;
    std::string feedback;
    std::string explanation;
    std::string nextStep;
};

const ChallengeDefinition& challengeAt(int index);
const ChallengeDefinition* findChallenge(const std::string& id);
int challengeCount();
bool validChallengeDefinition(const ChallengeDefinition& challenge);
ChallengeResult evaluateChallenge(const ChallengeDefinition& challenge,
                                  const ChallengeAnswer& answer);
const char* challengeDifficultyName(ChallengeDifficulty difficulty);
const char* challengeKindName(ChallengeKind kind);
const char* challengeGrade(double score, bool passed);

} // namespace bag
