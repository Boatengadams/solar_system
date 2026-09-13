#pragma once

#include <string>
#include <filesystem>
#include <vector>

#include "EducationChallenges.hpp"
#include "ExperimentEvaluation.hpp"

namespace bag {

struct EducationReport {
    int completedLessons = 0;
    int totalLessons = 0;
    int completedExperiments = 0;
    int totalExperiments = 0;
    int completedChallenges = 0;
    int totalChallenges = 0;
    double completionRatio = 0.0;
    std::vector<std::string> observations;
};

struct ChallengeProgress {
    int attempts = 0;
    bool completed = false;
    double bestScore = 0.0;
    double latestScore = 0.0;
    ChallengeMetrics latestMetrics;
};

struct ExperimentProgress {
    int attempts = 0;
    bool completed = false;
    double bestScore = 0.0;
    double latestScore = 0.0;
    std::string latestGrade = "INVALID";
    ExperimentEvaluationStatus latestStatus = ExperimentEvaluationStatus::Unsupported;
    ExperimentEvaluationMode latestMode = ExperimentEvaluationMode::AnalyticalReference;
    ExperimentEvaluationMetrics latestMetrics;
};

class EducationProgress {
public:
    static constexpr int CURRENT_SCHEMA_VERSION = 1;

    EducationProgress(int lessonCount, int experimentCount, int challengeCount = 0);

    bool completeLesson(int index);
    bool completeExperiment(int index);
    bool recordObservation(int experimentIndex, std::string observation);
    bool recordChallengeResult(int challengeIndex, const ChallengeResult& result);
    bool recordExperimentEvaluation(int experimentIndex, const ExperimentEvaluation& result);
    bool lessonComplete(int index) const;
    bool experimentComplete(int index) const;
    bool challengeComplete(int index) const;
    const ChallengeProgress* challengeProgress(int index) const;
    const ExperimentProgress* experimentProgress(int index) const;
    void reset();
    EducationReport report() const;
    std::string exportText() const;
    std::string serialize() const;
    bool deserialize(const std::string& serialized);
    bool save(const std::filesystem::path& path) const;
    bool load(const std::filesystem::path& path);

private:
    std::vector<bool> lessons;
    std::vector<bool> experiments;
    std::vector<std::string> experimentObservations;
    std::vector<ExperimentProgress> experimentEvaluations;
    std::vector<ChallengeProgress> challenges;
};

} // namespace bag
