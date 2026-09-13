#include <cassert>

#include "education/EducationContent.hpp"
#include "education/EducationProgress.hpp"

int main() {
    using namespace bag;
    assert(lessonCount() >= 9);
    assert(experimentCount() >= 6);
    EducationProgress progress(lessonCount(), experimentCount());
    assert(progress.completeLesson(0));
    assert(progress.completeExperiment(0));
    assert(progress.recordObservation(1, "period increased after moving the orbit outward"));
    assert(!progress.completeLesson(-1));
    const EducationReport report = progress.report();
    assert(report.completedLessons == 1 && report.completedExperiments == 2);
    assert(report.completionRatio > 0.0 && report.observations.size() == 1);
    progress.reset();
    assert(progress.report().completedLessons == 0 && progress.report().completedExperiments == 0);
    return 0;
}
