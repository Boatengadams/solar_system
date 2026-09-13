#include "HUD.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "../education/EducationContent.hpp"
#include "../education/EducationChallenges.hpp"
#include "../education/EducationCatalog.hpp"
#include "../education/LearnerReport.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {
namespace {

Color alpha(Color color, float amount) {
    color.a = static_cast<unsigned char>(std::clamp(amount * 255.0f, 0.0f, 255.0f));
    return color;
}

std::string format(double value, int digits = 2) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(digits) << value;
    return output.str();
}

std::string scientific(double value, int digits = 3) {
    std::ostringstream output;
    output << std::scientific << std::setprecision(digits) << value;
    return output.str();
}

std::string distance(double meters) {
    const double au = std::abs(meters) / PhysicsEngine::AU;
    if (au >= 0.01) return format(meters / PhysicsEngine::AU, 3) + " AU";
    if (std::abs(meters) >= 1e9) return format(meters / 1e9, 2) + " billion km";
    return format(meters / 1000.0, 0) + " km";
}

std::string time(double seconds) {
    const double absolute = std::abs(seconds);
    if (absolute < 60) return format(absolute, 1) + " s";
    if (absolute < PhysicsEngine::DAY) return format(absolute / 3600.0, 1) + " h";
    if (absolute < PhysicsEngine::YEAR) return format(absolute / PhysicsEngine::DAY, 1) + " days";
    return format(absolute / PhysicsEngine::YEAR, 2) + " years";
}

} // namespace

Rectangle HUD::panel(float x, float y, float width, float height) const {
    DrawRectangleRounded({x, y, width, height}, 0.12f, 12, alpha({10, 18, 32, 255}, 0.94f));
    DrawRectangleRoundedLines({x, y, width, height}, 0.12f, 12, alpha({90, 160, 220, 255}, 0.28f));
    return {x, y, width, height};
}

void HUD::text(const char* value, float x, float y, float size, Color colorValue) const {
    DrawText(value, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), colorValue);
}

void HUD::line(const char* label, const char* value, float x, float y) const {
    text(label, x, y, 13, alpha({165, 190, 215, 255}, 0.85f));
    text(value, x + 112, y, 13);
}

void HUD::top(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), 58, alpha({4, 10, 20, 255}, 0.96f));
    text("BAGSOLAR", 22, 10, 26, {110, 220, 255, 255});
    text("ASTRONOMICAL LABORATORY", 154, 16, 13, alpha(RAYWHITE, 0.65f));
    text(simulation.paused ? "● PAUSED" : "● LIVE", GetScreenWidth() - 160, 17, 13,
         simulation.paused ? RED : Color{80, 235, 150, 255});
}

void HUD::selectedInfo(const Simulation& simulation) const {
    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[simulation.selected];
    const Rectangle box = panel(GetScreenWidth() - 340, 76, 320, 330);
    text(body.name.c_str(), box.x + 18, box.y + 16, 23, {body.accent.r, body.accent.g, body.accent.b, body.accent.a});
    text(body.type.c_str(), box.x + 18, box.y + 45, 12, alpha(RAYWHITE, 0.55f));
    const std::string mass = scientific(body.mass) + " kg";
    const std::string radius = distance(body.realRadius);
    const std::string bodyDistance = distance(simulation.distanceFromSun(body));
    const std::string velocity = format(length(body.velocity) / 1000.0, 2) + " km/s";
    const std::string escape = format(simulation.escapeVelocity(body) / 1000.0, 2) + " km/s";
    const std::string gravity = format(simulation.surfaceGravity(body), 2) + " m/s²";
    const std::string eccentricity = format(body.eccentricity, 4);
    const std::string energy = scientific(simulation.specificEnergy(body)) + " J/kg";
    const double kineticEnergy = 0.5 * length(body.velocity) * length(body.velocity);
    const double potentialEnergy = -PhysicsEngine::G * PhysicsEngine::SOLAR_MASS /
                                   std::max(simulation.distanceFromSun(body), 1.0);
    line("Mass", mass.c_str(), box.x + 18, box.y + 75);
    line("Radius", radius.c_str(), box.x + 18, box.y + 98);
    line("Distance", bodyDistance.c_str(), box.x + 18, box.y + 121);
    line("Velocity", velocity.c_str(), box.x + 18, box.y + 144);
    line("Escape", escape.c_str(), box.x + 18, box.y + 167);
    line("Gravity", gravity.c_str(), box.x + 18, box.y + 190);
    line("Eccentricity", eccentricity.c_str(), box.x + 18, box.y + 213);
    line("Energy", energy.c_str(), box.x + 18, box.y + 236);
    text("ORBIT STATE", box.x + 18, box.y + 267, 11, alpha({160, 190, 220, 255}, 0.8f));
    text(simulation.specificEnergy(body) < 0 ? "BOUND ORBIT" : "ESCAPE TRAJECTORY", box.x + 18, box.y + 286, 15,
         simulation.specificEnergy(body) < 0 ? Color{80, 235, 150, 255} : ORANGE);
    text(("KE/PE: " + format(kineticEnergy / std::max(std::abs(potentialEnergy), 1.0), 3)).c_str(),
         box.x + 18, box.y + 308, 12, alpha(RAYWHITE, 0.7f));
}

void HUD::lessonPanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const Rectangle box = panel(20, 76, 360, 250);
    const Lesson& lesson = lessonAt(simulation.lesson);
    text("LEARN", box.x + 18, box.y + 16, 11, {100, 220, 255, 255});
    text(lesson.title, box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), lesson.body, {box.x + 18, box.y + 73}, 15, 2, alpha(RAYWHITE, 0.82f));
    text("EDUCATION HOME", box.x + 18, box.y + 174, 10, {255, 205, 105, 255});
    const EducationReport report = simulation.educationProgress.report();
    text(("Lessons " + std::to_string(report.completedLessons) + "/" + std::to_string(report.totalLessons) +
          "  Experiments " + std::to_string(report.completedExperiments) + "/" + std::to_string(report.totalExperiments)).c_str(),
         box.x + 18, box.y + 192, 10, alpha(RAYWHITE, 0.75f));
    text(("Challenges " + std::to_string(report.completedChallenges) + "/" + std::to_string(report.totalChallenges) +
          "  " + std::string(EducationWorkflow::stateName(simulation.educationWorkflow.state()))).c_str(),
         box.x + 18, box.y + 208, 10, alpha(RAYWHITE, 0.75f));
    std::string selectedProgress = "Selected activity";
    for (const EducationActivityProgress& item : simulation.educationWorkflow.home()) {
        const EducationActivity& current = simulation.educationWorkflow.activity();
        if (item.activity.type == current.type && item.activity.index == current.index &&
            current.type != EducationActivityType::Lesson) {
            selectedProgress = "Selected attempts " + std::to_string(item.attempts) +
                "  best " + format(item.bestScore, 1) + "  latest " + format(item.latestScore, 1);
            break;
        }
    }
    text(selectedProgress.c_str(), box.x + 18, box.y + 224, 10, alpha(RAYWHITE, 0.72f));
}

void HUD::bottom(const Simulation& simulation) const {
    DrawRectangle(0, GetScreenHeight() - 54, GetScreenWidth(), 54, alpha({4, 10, 20, 255}, 0.97f));
    text("SPACE", 20, GetScreenHeight() - 36, 12, alpha(RAYWHITE, 0.65f));
    text(simulation.paused ? "Resume" : "Pause", 75, GetScreenHeight() - 36, 12);
    text("+ / -  Time", 155, GetScreenHeight() - 36, 12);
    text("L Lessons", 270, GetScreenHeight() - 36, 12);
    text("E Experiment", 380, GetScreenHeight() - 36, 12);
    text("V Vectors", 520, GetScreenHeight() - 36, 12);
    text("O Orbits", 620, GetScreenHeight() - 36, 12);
    text("T Trails", 715, GetScreenHeight() - 36, 12);
    text("F Fullscreen", 800, GetScreenHeight() - 36, 12);
    text("Right-drag Pan • Wheel Zoom • Click Select • H Home", 920, GetScreenHeight() - 36, 12, alpha(RAYWHITE, 0.55f));
    text("F1 Solar • F2 Earth • F3 Empty", 20, GetScreenHeight() - 16, 11, alpha({150, 210, 240, 255}, 0.7f));
    text("SIM TIME", GetScreenWidth() - 220, 72, 10, alpha(RAYWHITE, 0.45f));
    text(time(simulation.simTime).c_str(), GetScreenWidth() - 220, 87, 17, {100, 220, 255, 255});
    text(("×" + format(simulation.speed, 1)).c_str(), GetScreenWidth() - 120, 87, 15);
}

void HUD::experimentPanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const Rectangle box = panel(20, 345, 360, 285);
    const Experiment& experiment = experimentAt(simulation.experiment);
    text("EXPERIMENT", box.x + 18, box.y + 16, 11, {100, 220, 255, 255});
    text(experiment.title, box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), experiment.prompt, {box.x + 18, box.y + 74}, 15, 2, alpha(RAYWHITE, 0.82f));
    text(experiment.equation, box.x + 18, box.y + 137, 21, {150, 225, 255, 255});
    const bool selectedExperiment = simulation.educationWorkflow.activity().type == EducationActivityType::Experiment &&
        simulation.educationWorkflow.activity().index == simulation.experiment;
    const std::string workflowState = (selectedExperiment ? std::string("STATE ") : "STATE SELECT ACTIVITY: ") +
        EducationWorkflow::stateName(simulation.educationWorkflow.state());
    text(workflowState.c_str(),
         box.x + 18, box.y + 161, 10, alpha(RAYWHITE, 0.62f));
    text("ENTER start  B observe  Y evaluate  Q retry  N next", box.x + 18, box.y + 184, 10, alpha(RAYWHITE, 0.48f));
    if (simulation.lastExperimentEvaluation) {
        const ExperimentEvaluation& result = *simulation.lastExperimentEvaluation;
        text((std::string(experimentEvaluationStatusName(result.status)) + "  " + result.grade + "  " + format(result.score, 1) + "/100").c_str(),
             box.x + 18, box.y + 204, 12, result.passed ? Color{80, 235, 150, 255} : ORANGE);
        if (std::isfinite(result.metrics.measuredPrimaryValue)) {
            const std::string measured = "Measured: " + format(result.metrics.measuredPrimaryValue, 2) +
                (std::isfinite(result.metrics.referencePrimaryValue)
                    ? "  Ref: " + format(result.metrics.referencePrimaryValue, 2) : "");
            text(measured.c_str(), box.x + 18, box.y + 220, 10, alpha({190, 215, 235, 255}, 0.78f));
        } else if (std::isfinite(result.metrics.normalizedError)) {
            text(("Normalized error: " + format(result.metrics.normalizedError, 4)).c_str(),
                 box.x + 18, box.y + 220, 10, alpha({190, 215, 235, 255}, 0.78f));
        } else if (std::isfinite(result.metrics.energyDrift)) {
            text(("Drift E/L: " + format(result.metrics.energyDrift, 4) + " / " +
                  format(result.metrics.angularMomentumDrift, 4)).c_str(),
                 box.x + 18, box.y + 220, 10, alpha({190, 215, 235, 255}, 0.78f));
        }
        DrawTextEx(GetFontDefault(), result.feedback.c_str(), {box.x + 18, box.y + 236}, 10, 1, alpha(RAYWHITE, 0.75f));
        DrawTextEx(GetFontDefault(), ("Next: " + result.nextStep).c_str(), {box.x + 18, box.y + 251}, 9, 1, alpha({190, 215, 235, 255}, 0.72f));
        DrawTextEx(GetFontDefault(), ("Why: " + result.explanation).c_str(), {box.x + 18, box.y + 266}, 9, 1, alpha({190, 215, 235, 255}, 0.68f));
    } else {
        text("P  launch probe at escape velocity", box.x + 18, box.y + 204, 11, alpha(RAYWHITE, 0.65f));
    }
}

void HUD::challengePanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const ChallengeDefinition& challenge = challengeAt(simulation.challenge);
    const Rectangle box = panel(400, 76, 420, 250);
    text("CHALLENGE", box.x + 18, box.y + 16, 11, {255, 205, 105, 255});
    text(challenge.title.c_str(), box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), challenge.description.c_str(), {box.x + 18, box.y + 73}, 14, 2, alpha(RAYWHITE, 0.82f));
    text(("Objective: " + challenge.learningObjective).c_str(), box.x + 18, box.y + 125, 11, alpha({190, 215, 235, 255}, 0.82f));
    text(("State: " + std::string(EducationWorkflow::stateName(simulation.educationWorkflow.state()))).c_str(),
         box.x + 18, box.y + 140, 10, alpha(RAYWHITE, 0.62f));
    if (challenge.kind == ChallengeKind::IntegratorComparison) {
        text(("Method: " + std::string(integratorName(simulation.challengeIntegrator))).c_str(), box.x + 18, box.y + 150, 14, {150, 225, 255, 255});
    } else {
        text(("Answer: " + format(simulation.challengeAnswer, 2) + " SI").c_str(), box.x + 18, box.y + 150, 14, {150, 225, 255, 255});
    }
    if (simulation.lastChallengeResult) {
        const ChallengeResult& result = *simulation.lastChallengeResult;
        text((result.grade + "  " + format(result.score, 1) + "/100").c_str(), box.x + 18, box.y + 177, 15,
             result.passed ? Color{80, 235, 150, 255} : ORANGE);
        DrawTextEx(GetFontDefault(), result.feedback.c_str(), {box.x + 18, box.y + 201}, 12, 2, alpha(RAYWHITE, 0.75f));
        if (challenge.kind == ChallengeKind::HohmannTransfer) {
            text(("Burns: " + format(result.metrics.referenceDepartureDeltaV / 1000.0, 2) + " + " +
                  format(result.metrics.referenceArrivalDeltaV / 1000.0, 2) + " km/s").c_str(),
                 box.x + 18, box.y + 225, 11, alpha({190, 215, 235, 255}, 0.78f));
        }
        DrawTextEx(GetFontDefault(), ("Next: " + result.nextStep).c_str(), {box.x + 18, box.y + 240}, 9, 1, alpha({190, 215, 235, 255}, 0.72f));
        DrawTextEx(GetFontDefault(), ("Why: " + result.explanation).c_str(), {box.x + 18, box.y + 255}, 9, 1, alpha({190, 215, 235, 255}, 0.68f));
    } else {
        text("ENTER start  B observe  [ / ] adjust  I method  C submit", box.x + 18, box.y + 188, 11, alpha(RAYWHITE, 0.58f));
    }
}

void HUD::draw(const Simulation& simulation) const {
    if (simulation.educationScreen) {
        educationScreenPanel(simulation);
        return;
    }
    top(simulation);
    lessonPanel(simulation);
    experimentPanel(simulation);
    challengePanel(simulation);
    selectedInfo(simulation);
    bottom(simulation);
}

void HUD::educationScreenPanel(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    const LearnerReport report = buildLearnerReport(simulation.educationProgress);
    const AuthoredLesson& lesson = authoredLessonAt(simulation.lesson);
    text("EDUCATION", 36, 28, 30, {110, 220, 255, 255});
    text("Offline Newtonian astrodynamics laboratory", 38, 64, 13, alpha(RAYWHITE, 0.62f));

    text("PROGRESS", 38, 108, 12, {255, 205, 105, 255});
    text(("Lessons      " + std::to_string(report.completedLessons) + " / " + std::to_string(report.totalLessons)).c_str(), 38, 132, 14);
    text(("Experiments  " + std::to_string(report.completedExperiments) + " / " + std::to_string(report.totalExperiments)).c_str(), 38, 154, 14);
    text(("Challenges   " + std::to_string(report.completedChallenges) + " / " + std::to_string(report.totalChallenges)).c_str(), 38, 176, 14);
    DrawRectangle(38, 202, 250, 12, {30, 55, 80, 255});
    DrawRectangle(38, 202, report.totalLessons == 0 ? 0 : 250 * report.completedLessons / report.totalLessons, 12, {80, 200, 255, 255});
    text("Lesson completion", 38, 222, 11, alpha(RAYWHITE, 0.58f));

    text("LESSONS", 38, 270, 12, {255, 205, 105, 255});
    for (int index = 0; index < authoredLessonCount(); ++index) {
        const AuthoredLesson& item = authoredLessonAt(index);
        const bool selected = index == simulation.lesson;
        const bool complete = simulation.educationProgress.lessonComplete(index);
        if (selected) DrawRectangleRounded({32, 292.0f + index * 28.0f, 300, 24}, 0.2f, 8, {35, 82, 112, 255});
        text((std::string(complete ? "✓ " : "• ") + item.title).c_str(), 44, 297.0f + index * 28.0f, 12,
             selected ? RAYWHITE : alpha(RAYWHITE, 0.68f));
    }

    text(lesson.title.c_str(), 380, 108, 25, {150, 225, 255, 255});
    text(lesson.shortDescription.c_str(), 382, 142, 13, alpha(RAYWHITE, 0.82f));
    text("OBJECTIVES", 382, 184, 11, {255, 205, 105, 255});
    for (std::size_t index = 0; index < lesson.objectives.size() && index < 3; ++index)
        text(("• " + lesson.objectives[index]).c_str(), 390, 207.0f + index * 20.0f, 12, alpha(RAYWHITE, 0.78f));
    text(("Difficulty: " + std::string(educationDifficultyName(lesson.difficulty)) +
          "   Duration: " + std::to_string(lesson.estimatedMinutes) + " min").c_str(), 382, 274, 12, alpha(RAYWHITE, 0.65f));
    text("LINKED ACTIVITIES", 382, 314, 11, {255, 205, 105, 255});
    const std::string experiment = lesson.experimentIds.empty() ? "none" : lesson.experimentIds.front();
    const std::string challenge = lesson.challengeIds.empty() ? "none" : lesson.challengeIds.front();
    text(("Experiment: " + experiment).c_str(), 390, 337, 12, alpha(RAYWHITE, 0.78f));
    text(("Challenge: " + challenge).c_str(), 390, 359, 12, alpha(RAYWHITE, 0.78f));
    text(("Status: " + std::string(simulation.educationProgress.lessonComplete(simulation.lesson) ? "COMPLETED" : "NOT COMPLETED")).c_str(), 382, 399, 13,
         simulation.educationProgress.lessonComplete(simulation.lesson) ? Color{80, 235, 150, 255} : ORANGE);

    text("LEARNER REPORT", 760, 108, 12, {255, 205, 105, 255});
    text(report.hasAverageScore ? ("Average score: " + format(report.averageScore, 1)).c_str() : "Average score: no scored activities", 760, 134, 12);
    text("Strongest areas", 760, 174, 11, alpha(RAYWHITE, 0.6f));
    if (report.strongestAreas.empty()) text("None yet", 770, 196, 12, alpha(RAYWHITE, 0.7f));
    for (std::size_t index = 0; index < report.strongestAreas.size() && index < 3; ++index) text(("• " + report.strongestAreas[index]).c_str(), 770, 196.0f + index * 20.0f, 12);
    text("Needs practice", 760, 272, 11, alpha(RAYWHITE, 0.6f));
    if (report.areasNeedingImprovement.empty()) text("None identified", 770, 294, 12, alpha(RAYWHITE, 0.7f));
    for (std::size_t index = 0; index < report.areasNeedingImprovement.size() && index < 3; ++index) text(("• " + report.areasNeedingImprovement[index]).c_str(), 770, 294.0f + index * 20.0f, 12);
    text("RECOMMENDED NEXT", 760, 370, 11, {255, 205, 105, 255});
    text(report.recommendation.available ? report.recommendation.title.c_str() : "None", 760, 394, 14, {150, 225, 255, 255});
    if (report.recommendation.available) DrawTextEx(GetFontDefault(), report.recommendation.reason.c_str(), {760, 420}, 11, 1, alpha(RAYWHITE, 0.72f));
    text("A / D browse lessons   ENTER start   B observe   Y complete   N next   BACKSPACE return", 38, GetScreenHeight() - 34, 12, alpha(RAYWHITE, 0.65f));
}

} // namespace bag
