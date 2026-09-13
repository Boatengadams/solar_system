#include "EducationProgress.hpp"

#include <algorithm>

namespace bag {

EducationProgress::EducationProgress(int lessonCount, int experimentCount)
    : lessons(static_cast<std::size_t>(std::max(0, lessonCount)), false),
      experiments(static_cast<std::size_t>(std::max(0, experimentCount)), false),
      experimentObservations(static_cast<std::size_t>(std::max(0, experimentCount))) {}

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

bool EducationProgress::lessonComplete(int index) const { return index >= 0 && index < static_cast<int>(lessons.size()) && lessons[static_cast<std::size_t>(index)]; }
bool EducationProgress::experimentComplete(int index) const { return index >= 0 && index < static_cast<int>(experiments.size()) && experiments[static_cast<std::size_t>(index)]; }

void EducationProgress::reset() {
    std::fill(lessons.begin(), lessons.end(), false);
    std::fill(experiments.begin(), experiments.end(), false);
    std::fill(experimentObservations.begin(), experimentObservations.end(), std::string{});
}

EducationReport EducationProgress::report() const {
    EducationReport result;
    result.totalLessons = static_cast<int>(lessons.size());
    result.totalExperiments = static_cast<int>(experiments.size());
    result.completedLessons = static_cast<int>(std::count(lessons.begin(), lessons.end(), true));
    result.completedExperiments = static_cast<int>(std::count(experiments.begin(), experiments.end(), true));
    const int total = result.totalLessons + result.totalExperiments;
    result.completionRatio = total == 0 ? 0.0 : static_cast<double>(result.completedLessons + result.completedExperiments) / total;
    for (const std::string& observation : experimentObservations) if (!observation.empty()) result.observations.push_back(observation);
    return result;
}

} // namespace bag
