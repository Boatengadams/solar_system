#pragma once

#include <string>
#include <vector>

namespace bag {

struct EducationReport {
    int completedLessons = 0;
    int totalLessons = 0;
    int completedExperiments = 0;
    int totalExperiments = 0;
    double completionRatio = 0.0;
    std::vector<std::string> observations;
};

class EducationProgress {
public:
    EducationProgress(int lessonCount, int experimentCount);

    bool completeLesson(int index);
    bool completeExperiment(int index);
    bool recordObservation(int experimentIndex, std::string observation);
    bool lessonComplete(int index) const;
    bool experimentComplete(int index) const;
    void reset();
    EducationReport report() const;

private:
    std::vector<bool> lessons;
    std::vector<bool> experiments;
    std::vector<std::string> experimentObservations;
};

} // namespace bag
