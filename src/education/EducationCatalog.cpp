#include "EducationCatalog.hpp"

#include <algorithm>
#include <cmath>

#include "EducationChallenges.hpp"
#include "EducationContent.hpp"

namespace bag {
namespace {

std::vector<AuthoredLesson> makeCatalog() {
    return {
        {"gravity", "Gravity and Newton's law", "Mass and distance determine gravitational acceleration.",
         {"Relate attraction to mass and separation.", "Recognize gravity as an interaction between bodies."}, {},
         {"Why gravity matters", "Change distance and mass", "Inspect acceleration"},
         {"gravity-lab"}, {"circular-orbit"}, EducationDifficulty::Introductory, 12},
        {"escape-velocity", "Escape velocity", "Find the boundary between a bound orbit and escape trajectory.",
         {"Connect kinetic and gravitational potential energy.", "Identify zero specific energy as the escape boundary."}, {"gravity"},
         {"Bound versus unbound", "Measure the boundary speed", "Compare nearby trajectories"},
         {"escape-velocity"}, {"escape-velocity"}, EducationDifficulty::Introductory, 15},
        {"circular-orbits", "Circular orbits", "Understand the speed required to continually miss the central body.",
         {"Relate circular speed to radius and central mass.", "Distinguish a circular state from a merely bound state."}, {"gravity"},
         {"Free fall", "Tangential velocity", "Orbit shape"},
         {"kepler-test"}, {"circular-orbit"}, EducationDifficulty::Introductory, 15},
        {"kepler-period", "Kepler and orbital period", "Compare orbital distance with the time required to complete an orbit.",
         {"Use the period-radius relationship qualitatively.", "Compare measured periods with an analytical orbital reference."}, {"circular-orbits"},
         {"Period as a measurement", "Change orbital radius", "Compare the scaling"},
         {"kepler-test"}, {"circular-orbit"}, EducationDifficulty::Intermediate, 18},
        {"orbital-energy", "Orbital energy", "Use specific orbital energy to classify bound and unbound motion.",
         {"Interpret negative, zero, and positive specific energy.", "Separate a physical classification from a numerical score."}, {"escape-velocity"},
         {"Specific energy", "Move across the boundary", "Classify the trajectory"},
         {"orbit-energy"}, {"escape-velocity"}, EducationDifficulty::Intermediate, 15},
        {"numerical-integration", "Numerical integration", "See how integration methods approximate continuous motion.",
         {"Compare endpoint error and conservation behavior.", "Treat a smaller-step numerical trajectory as a teaching reference."}, {"circular-orbits"},
         {"Continuous equations", "Compare integrators", "Inspect error and stability"},
         {"numerical-methods"}, {"integrator-comparison"}, EducationDifficulty::Intermediate, 22},
        {"timestep-selection", "Timestep selection", "Study how step size affects accuracy, stability, and computational work.",
         {"Recognize convergence as a comparison between numerical runs.", "Choose a useful step without claiming one universal optimum."}, {"numerical-integration"},
         {"Coarse and refined runs", "Measure improvement", "Balance accuracy and work"},
         {"timestep-sensitivity"}, {"timestep-selection"}, EducationDifficulty::Intermediate, 20},
        {"hohmann-transfers", "Hohmann transfers", "Understand two tangential burns between coplanar circular orbits.",
         {"Separate departure and arrival burns.", "Interpret total delta-v under the point-mass model."}, {"circular-orbits", "orbital-energy"},
         {"Two circular orbits", "Transfer ellipse", "Compare the two burns"},
         {"hohmann-lab"}, {"hohmann-transfer"}, EducationDifficulty::Intermediate, 25},
        {"gravity-assists", "Gravity assists", "Inspect how a flyby changes direction in a patched-conic model.",
         {"Relate turn angle to periapsis and incoming speed.", "Distinguish a model result from a complete mission analysis."}, {"orbital-energy"},
         {"Flyby geometry", "Vary periapsis", "Vary incoming speed"},
         {"assist-lab"}, {}, EducationDifficulty::Advanced, 25},
    };
}

const std::vector<AuthoredLesson>& catalog() {
    static const std::vector<AuthoredLesson> lessons = makeCatalog();
    return lessons;
}

bool duplicateValues(const std::vector<std::string>& values) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (std::find(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values[index]) !=
            values.begin() + static_cast<std::ptrdiff_t>(index)) return true;
    }
    return false;
}

bool hasCycle(const std::vector<AuthoredLesson>& lessons, int index,
              std::vector<int>& visiting, std::vector<int>& complete) {
    if (visiting[static_cast<std::size_t>(index)] != 0) return visiting[static_cast<std::size_t>(index)] == 1;
    visiting[static_cast<std::size_t>(index)] = 1;
    for (const std::string& prerequisite : lessons[static_cast<std::size_t>(index)].prerequisites) {
        const auto found = std::find_if(lessons.begin(), lessons.end(), [&](const AuthoredLesson& lesson) { return lesson.id == prerequisite; });
        if (found != lessons.end() && hasCycle(lessons, static_cast<int>(std::distance(lessons.begin(), found)), visiting, complete)) return true;
    }
    visiting[static_cast<std::size_t>(index)] = 2;
    complete[static_cast<std::size_t>(index)] = 1;
    return false;
}

} // namespace

const AuthoredLesson& authoredLessonAt(int index) {
    const auto& lessons = catalog();
    const int safeIndex = ((index % static_cast<int>(lessons.size())) + static_cast<int>(lessons.size())) % static_cast<int>(lessons.size());
    return lessons[static_cast<std::size_t>(safeIndex)];
}

const AuthoredLesson* findAuthoredLesson(const std::string& id) {
    const auto& lessons = catalog();
    const auto found = std::find_if(lessons.begin(), lessons.end(), [&](const AuthoredLesson& lesson) { return lesson.id == id; });
    return found == lessons.end() ? nullptr : &*found;
}

int authoredLessonCount() { return static_cast<int>(catalog().size()); }

EducationCatalogValidation validateEducationCatalog(const std::vector<AuthoredLesson>& lessons) {
    EducationCatalogValidation result;
    for (std::size_t index = 0; index < lessons.size(); ++index) {
        const AuthoredLesson& lesson = lessons[index];
        if (lesson.id.empty() || lesson.title.empty() || lesson.shortDescription.empty() || lesson.objectives.empty() || lesson.sections.empty()) {
            result.errors.push_back("lesson " + std::to_string(index) + " has required authored fields missing");
        }
        if (std::find_if(lessons.begin(), lessons.begin() + static_cast<std::ptrdiff_t>(index), [&](const AuthoredLesson& other) { return other.id == lesson.id; }) !=
            lessons.begin() + static_cast<std::ptrdiff_t>(index)) result.errors.push_back("duplicate lesson id: " + lesson.id);
        if (lesson.estimatedMinutes <= 0) result.errors.push_back("lesson has invalid duration: " + lesson.id);
        if (duplicateValues(lesson.prerequisites) || duplicateValues(lesson.experimentIds) || duplicateValues(lesson.challengeIds)) result.errors.push_back("lesson has duplicate references: " + lesson.id);
        for (const std::string& prerequisite : lesson.prerequisites) {
            const bool found = std::find_if(lessons.begin(), lessons.end(), [&](const AuthoredLesson& candidate) { return candidate.id == prerequisite; }) != lessons.end();
            if (!found) result.errors.push_back("dangling prerequisite: " + prerequisite);
        }
        for (const std::string& experiment : lesson.experimentIds) {
            bool found = false;
            for (int experimentIndex = 0; experimentIndex < experimentCount(); ++experimentIndex) found = found || experimentAt(experimentIndex).id == experiment;
            if (!found) result.errors.push_back("dangling experiment reference: " + experiment);
        }
        for (const std::string& challenge : lesson.challengeIds) if (!findChallenge(challenge)) result.errors.push_back("dangling challenge reference: " + challenge);
    }
    std::vector<int> visiting(lessons.size(), 0);
    std::vector<int> complete(lessons.size(), 0);
    for (std::size_t index = 0; index < lessons.size(); ++index) if (hasCycle(lessons, static_cast<int>(index), visiting, complete)) result.errors.push_back("prerequisite cycle detected");
    result.valid = result.errors.empty();
    return result;
}

EducationCatalogValidation validateEducationCatalog() { return validateEducationCatalog(catalog()); }

const char* educationDifficultyName(EducationDifficulty difficulty) {
    switch (difficulty) {
    case EducationDifficulty::Introductory: return "INTRODUCTORY";
    case EducationDifficulty::Intermediate: return "INTERMEDIATE";
    case EducationDifficulty::Advanced: return "ADVANCED";
    }
    return "UNKNOWN";
}

} // namespace bag
