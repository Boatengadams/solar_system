#include "EducationProgress.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bag {

EducationProgress::EducationProgress(int lessonCount, int experimentCount, int challengeCount)
    : lessons(static_cast<std::size_t>(std::max(0, lessonCount)), false),
      experiments(static_cast<std::size_t>(std::max(0, experimentCount)), false),
      experimentObservations(static_cast<std::size_t>(std::max(0, experimentCount))),
      challenges(static_cast<std::size_t>(std::max(0, challengeCount))) {}

bool EducationProgress::completeLesson(int index) {
    if (index < 0 || index >= static_cast<int>(lessons.size())) return false;
    lessons[static_cast<std::size_t>(index)] = true;
    return true;
}

bool EducationProgress::completeExperiment(int index) {
    if (index < 0 || index >= static_cast<int>(experiments.size())) return false;
    experiments[static_cast<std::size_t>(index)] = true;
    return true;
}

bool EducationProgress::recordObservation(int experimentIndex, std::string observation) {
    if (experimentIndex < 0 || experimentIndex >= static_cast<int>(experiments.size()) || observation.empty()) return false;
    experimentObservations[static_cast<std::size_t>(experimentIndex)] = std::move(observation);
    experiments[static_cast<std::size_t>(experimentIndex)] = true;
    return true;
}

bool EducationProgress::recordChallengeResult(int challengeIndex, const ChallengeResult& result) {
    if (challengeIndex < 0 || challengeIndex >= static_cast<int>(challenges.size()) || !result.valid ||
        !std::isfinite(result.score) || result.score < 0.0 || result.score > 100.0) return false;
    ChallengeProgress& progress = challenges[static_cast<std::size_t>(challengeIndex)];
    ++progress.attempts;
    progress.latestScore = result.score;
    progress.bestScore = std::max(progress.bestScore, result.score);
    progress.completed = progress.completed || result.passed;
    progress.latestMetrics = result.metrics;
    return true;
}

bool EducationProgress::lessonComplete(int index) const { return index >= 0 && index < static_cast<int>(lessons.size()) && lessons[static_cast<std::size_t>(index)]; }
bool EducationProgress::experimentComplete(int index) const { return index >= 0 && index < static_cast<int>(experiments.size()) && experiments[static_cast<std::size_t>(index)]; }
bool EducationProgress::challengeComplete(int index) const { return index >= 0 && index < static_cast<int>(challenges.size()) && challenges[static_cast<std::size_t>(index)].completed; }
const ChallengeProgress* EducationProgress::challengeProgress(int index) const {
    if (index < 0 || index >= static_cast<int>(challenges.size())) return nullptr;
    return &challenges[static_cast<std::size_t>(index)];
}

void EducationProgress::reset() {
    std::fill(lessons.begin(), lessons.end(), false);
    std::fill(experiments.begin(), experiments.end(), false);
    std::fill(experimentObservations.begin(), experimentObservations.end(), std::string{});
    challenges.assign(challenges.size(), ChallengeProgress{});
}

EducationReport EducationProgress::report() const {
    EducationReport result;
    result.totalLessons = static_cast<int>(lessons.size());
    result.totalExperiments = static_cast<int>(experiments.size());
    result.totalChallenges = static_cast<int>(challenges.size());
    result.completedLessons = static_cast<int>(std::count(lessons.begin(), lessons.end(), true));
    result.completedExperiments = static_cast<int>(std::count(experiments.begin(), experiments.end(), true));
    result.completedChallenges = static_cast<int>(std::count_if(challenges.begin(), challenges.end(), [](const ChallengeProgress& progress) { return progress.completed; }));
    const int total = result.totalLessons + result.totalExperiments + result.totalChallenges;
    result.completionRatio = total == 0 ? 0.0 : static_cast<double>(result.completedLessons + result.completedExperiments + result.completedChallenges) / total;
    for (const std::string& observation : experimentObservations) if (!observation.empty()) result.observations.push_back(observation);
    return result;
}

std::string EducationProgress::exportText() const {
    std::ostringstream output;
    output << "BAGSOLAR_EDUCATION_PROGRESS_V1\n";
    output << "lessons," << lessons.size() << "\n";
    output << "experiments," << experiments.size() << "\n";
    output << "challenges," << challenges.size() << "\n";
    for (std::size_t index = 0; index < challenges.size(); ++index) {
        const ChallengeProgress& progress = challenges[index];
        output << "challenge," << index << ',' << progress.attempts << ',' << (progress.completed ? 1 : 0)
               << ',' << std::fixed << std::setprecision(6) << progress.bestScore << ',' << progress.latestScore << "\n";
    }
    return output.str();
}

} // namespace bag
