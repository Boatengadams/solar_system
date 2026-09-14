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
    if (!std::isfinite(value)) return "Unavailable";
    std::ostringstream output;
    output << std::fixed << std::setprecision(digits) << value;
    return output.str();
}

std::string scientific(double value, int digits = 3) {
    if (!std::isfinite(value)) return "Unavailable";
    std::ostringstream output;
    output << std::scientific << std::setprecision(digits) << value;
    return output.str();
}

std::string distance(double meters) {
    if (!std::isfinite(meters)) return "Unavailable";
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
    DrawRectangle(0, 0, GetScreenWidth(), 70, alpha({3, 9, 18, 255}, 0.97f));
    DrawLine(0, 69, GetScreenWidth(), 69, alpha({55, 170, 230, 255}, 0.35f));
    text("BAGSOLAR", 22, 11, 25, {110, 220, 255, 255});
    text("C++17 ORBITAL MECHANICS LABORATORY", 24, 40, 9, alpha(RAYWHITE, 0.52f));
    text(simulation.paused ? "PAUSED" : "RUNNING", GetScreenWidth() - 300, 15, 11,
         simulation.paused ? RED : Color{80, 235, 150, 255});
    text("SIM TIME", GetScreenWidth() - 218, 11, 9, alpha(RAYWHITE, 0.48f));
    text(time(simulation.simTime).c_str(), GetScreenWidth() - 218, 28, 16, {100, 220, 255, 255});
    text(("×" + format(simulation.speed, 1)).c_str(), GetScreenWidth() - 92, 28, 14, RAYWHITE);
}

void HUD::navigation(const Simulation& simulation) const {
    const struct Tab { const char* label; AppScreen screen; float x; float width; } tabs[] = {
        {"SIMULATION", AppScreen::Simulation, 290.0f, 95.0f},
        {"EDUCATION", AppScreen::Education, 385.0f, 95.0f},
        {"SCENARIOS", AppScreen::ScenarioBrowser, 480.0f, 105.0f},
        {"TELEMETRY", AppScreen::Telemetry, 585.0f, 105.0f},
        {"MISSION", AppScreen::MissionDesigner, 690.0f, 100.0f},
        {"SETTINGS", AppScreen::Settings, 790.0f, 105.0f},
        {"HELP", AppScreen::Help, 895.0f, 80.0f},
    };
    for (const Tab& tab : tabs) {
        const bool active = simulation.screen == tab.screen;
        if (active) DrawRectangleRec({tab.x, 67.0f, tab.width - 5.0f, 3.0f}, {30, 200, 255, 255});
        text(tab.label, tab.x + 7.0f, 29.0f, 10, active ? RAYWHITE : alpha(RAYWHITE, 0.62f));
    }
}

void HUD::selectedInfo(const Simulation& simulation) const {
    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[simulation.selected];
    const float boxWidth = std::min(300.0f, GetScreenWidth() - 40.0f);
    const Rectangle box = panel(18, GetScreenHeight() - 270, boxWidth, 236);
    text("SELECTED BODY", box.x + 16, box.y + 14, 9, {100, 220, 255, 255});
    text(body.name.c_str(), box.x + 16, box.y + 31, 20, {body.accent.r, body.accent.g, body.accent.b, body.accent.a});
    text(body.type.c_str(), box.x + 16, box.y + 57, 11, alpha(RAYWHITE, 0.55f));
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
    line("Mass", mass.c_str(), box.x + 16, box.y + 78);
    line("Radius", radius.c_str(), box.x + 16, box.y + 98);
    line("Distance", bodyDistance.c_str(), box.x + 16, box.y + 118);
    line("Velocity", velocity.c_str(), box.x + 16, box.y + 138);
    line("Eccentricity", eccentricity.c_str(), box.x + 16, box.y + 158);
    line("Energy", energy.c_str(), box.x + 16, box.y + 178);
    text("ORBIT", box.x + 16, box.y + 204, 9, alpha({160, 190, 220, 255}, 0.8f));
    text(simulation.specificEnergy(body) < 0 ? "BOUND" : "ESCAPE TRAJECTORY", box.x + 65, box.y + 201, 11,
         simulation.specificEnergy(body) < 0 ? Color{80, 235, 150, 255} : ORANGE);
    text(("KE/PE " + format(kineticEnergy / std::max(std::abs(potentialEnergy), 1.0), 3)).c_str(),
         box.x + 16, box.y + 218, 10, alpha(RAYWHITE, 0.7f));
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
    const float y = GetScreenHeight() - 28.0f;
    DrawRectangle(0, y - 6, GetScreenWidth(), 34, alpha({3, 9, 18, 255}, 0.90f));
    text("SPACE pause", 18, y + 2, 10, alpha(RAYWHITE, 0.72f));
    text("+/- time", 112, y + 2, 10, alpha(RAYWHITE, 0.72f));
    text("V vectors  O orbits  T trails  G grid", 188, y + 2, 10, alpha(RAYWHITE, 0.72f));
    text("Right-drag pan  •  Wheel zoom  •  Ctrl+S select mode", 470, y + 2, 10, {150, 220, 245, 255});
    text(("Integrator " + std::string(integratorName([&] { Integrator value = Integrator::VelocityVerlet; parseIntegrator(simulation.settings.integrator, value); return value; }())) +
          "  dt " + format(simulation.settings.timestepSeconds, 0) + " s  •  Display scale: enhanced").c_str(),
         GetScreenWidth() - 520, y + 2, 10, alpha(RAYWHITE, 0.62f));
}

void HUD::experimentPanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const Rectangle box = panel(20, 345, 360, 330);
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
        const bool predictionReference = std::string(experiment.id) == "prediction-reference" && simulation.lastPredictionComparison.has_value();
        if (predictionReference) {
            const PredictionComparisonResult& comparison = *simulation.lastPredictionComparison;
            text(("PREDICTION VS REFERENCE  " + std::string(predictionComparisonStatusName(comparison.status)) +
                  "  " + comparison.referenceProvider).c_str(), box.x + 18, box.y + 220, 9, alpha(RAYWHITE, 0.78f));
            text(("Body " + comparison.bodyId + "  Epoch JD " + format(comparison.finalEpoch.value, 6)).c_str(),
                 box.x + 18, box.y + 234, 9, alpha(RAYWHITE, 0.72f));
            text(("Frame " + std::string(referenceFrameName(comparison.frame.type)) + "/" + comparison.frame.originBodyId).c_str(),
                 box.x + 18, box.y + 248, 9, alpha(RAYWHITE, 0.72f));
            text((std::string("Integrator ") + integratorName(comparison.integrator) +
                  "  dt " + format(comparison.requestedTimestepSeconds, 2) + " s  samples " +
                  std::to_string(comparison.samples.size())).c_str(), box.x + 18, box.y + 262, 9, alpha(RAYWHITE, 0.72f));
            if (comparison.success()) {
                text(("Position " + format(comparison.positionErrorMagnitudeM, 3) + " m  Velocity " +
                      format(comparison.velocityErrorMagnitudeMps, 3) + " m/s").c_str(), box.x + 18, box.y + 276, 9, alpha(RAYWHITE, 0.78f));
                if (comparison.energyDefined) text(("Energy Δ " + format(comparison.absoluteEnergyDifferenceJPerKg, 3) +
                    " J/kg  relative " + format(comparison.relativeEnergyDifference, 6)).c_str(),
                    box.x + 18, box.y + 290, 9, alpha(RAYWHITE, 0.72f));
            } else {
                DrawTextEx(GetFontDefault(), comparison.explanation.c_str(), {box.x + 18, box.y + 276}, 9, 1, alpha(ORANGE, 0.82f));
            }
        } else if (std::isfinite(result.metrics.measuredPrimaryValue)) {
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
        if (predictionReference) {
            DrawTextEx(GetFontDefault(), result.feedback.c_str(), {box.x + 18, static_cast<float>(box.y + 304)}, 9, 1, alpha(RAYWHITE, 0.75f));
        } else {
            DrawTextEx(GetFontDefault(), result.feedback.c_str(), {box.x + 18, box.y + 236}, 10, 1, alpha(RAYWHITE, 0.75f));
            DrawTextEx(GetFontDefault(), ("Next: " + result.nextStep).c_str(), {box.x + 18, box.y + 251}, 9, 1, alpha({190, 215, 235, 255}, 0.72f));
            DrawTextEx(GetFontDefault(), ("Why: " + result.explanation).c_str(), {box.x + 18, box.y + 266}, 9, 1, alpha({190, 215, 235, 255}, 0.68f));
        }
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
    switch (simulation.screen) {
    case AppScreen::Simulation:
        top(simulation);
        navigation(simulation);
        selectedInfo(simulation);
        bottom(simulation);
        break;
    case AppScreen::Education: educationScreenPanel(simulation); break;
    case AppScreen::ScenarioBrowser: scenarioScreen(simulation); break;
    case AppScreen::Telemetry: telemetryScreen(simulation); break;
    case AppScreen::MissionDesigner: missionScreen(simulation); break;
    case AppScreen::Settings: settingsScreen(simulation); break;
    case AppScreen::Help: helpScreen(simulation); break;
    }
}

void HUD::educationScreenPanel(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    const LearnerReport report = buildLearnerReport(simulation.educationProgress);
    const AuthoredLesson& lesson = authoredLessonAt(simulation.lesson);
    text("BAGSOLAR EDUCATION", 36, 78, 28, {110, 220, 255, 255});
    text("An interactive orbital-mechanics laboratory", 38, 105, 13, alpha(RAYWHITE, 0.62f));

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
    text("A / D browse lessons   ENTER start   B observe   Y evaluate   N next   ESC return", 38, GetScreenHeight() - 34, 12, alpha(RAYWHITE, 0.65f));
}

void HUD::scenarioScreen(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    text("SCENARIO BROWSER", 40, 94, 28, {110, 220, 255, 255});
    text("Choose a data-driven starting system for the simulation.", 42, 124, 14, alpha(RAYWHITE, 0.68f));
    const struct Scenario { const char* id; const char* title; const char* description; const char* bodies; } scenarios[] = {
        {"default_solar_system", "Default Solar System", "The Sun, planets, and BAGSOLAR-1.", "10 bodies"},
        {"earth_orbit", "Earth Orbit", "A focused two-body Sun/Earth study.", "2 bodies"},
        {"empty_space", "Empty Space", "An empty scene for custom systems.", "0 bodies"},
    };
    for (int index = 0; index < 3; ++index) {
        const float y = 180.0f + index * 96.0f;
        const bool selectedScenario = index == simulation.scenarioBrowserSelection;
        if (selectedScenario) DrawRectangleRounded({40, y - 12, 650, 76}, 0.08f, 8, {28, 70, 98, 255});
        text((std::string(selectedScenario ? "▸ " : "  ") + scenarios[index].title).c_str(), 58, y, 19,
             selectedScenario ? RAYWHITE : alpha(RAYWHITE, 0.72f));
        text(scenarios[index].description, 82, y + 27, 13, alpha(RAYWHITE, 0.72f));
        text(scenarios[index].bodies, 560, y + 27, 12, {150, 225, 255, 255});
    }
    panel(760, 160, 420, 220);
    text("CURRENT SCENARIO", 782, 184, 11, {255, 205, 105, 255});
    text(simulation.scenarioMetadata.name.c_str(), 782, 211, 21, {150, 225, 255, 255});
    DrawTextEx(GetFontDefault(), simulation.scenarioMetadata.description.c_str(), {782, 246}, 14, 2, alpha(RAYWHITE, 0.78f));
    line("Epoch", simulation.scenarioMetadata.epoch.c_str(), 782, 300);
    line("Frame", simulation.scenarioMetadata.referenceFrame.c_str(), 782, 322);
    text("UP/DOWN select   ENTER load   ESC return", 42, GetScreenHeight() - 34, 13, alpha(RAYWHITE, 0.68f));
}

void HUD::telemetryScreen(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    text("TELEMETRY", 40, 94, 28, {110, 220, 255, 255});
    text(simulation.telemetryEnabled() ? "LIVE SESSION" : "NO ACTIVE SESSION", 42, 124, 13,
         simulation.telemetryEnabled() ? Color{80, 235, 150, 255} : ORANGE);
    if (simulation.telemetry.samples.empty()) {
        panel(40, 170, 660, 160);
        text("No telemetry samples available yet.", 64, 214, 19, RAYWHITE);
        text("Press K to start a deterministic 60-second sampling session.", 64, 250, 14, alpha(RAYWHITE, 0.72f));
        text("Select a body in Simulation first to record reference-relative values.", 64, 278, 13, alpha(RAYWHITE, 0.58f));
    } else {
        const TelemetrySample& sample = simulation.telemetry.samples.back();
        panel(40, 160, 660, 390);
        text("LATEST SAMPLE", 64, 188, 11, {255, 205, 105, 255});
        text(sample.bodyId.c_str(), 64, 216, 23, {150, 225, 255, 255});
        line("Time", (format(sample.simulationTimeSeconds, 1) + " s").c_str(), 64, 254);
        line("Position", (format(sample.positionM.x / PhysicsEngine::AU, 5) + ", " +
            format(sample.positionM.y / PhysicsEngine::AU, 5) + ", " + format(sample.positionM.z / PhysicsEngine::AU, 5) + " AU").c_str(), 64, 278);
        line("Velocity", (format(length(sample.velocityMps) / 1000.0, 3) + " km/s").c_str(), 64, 302);
        line("Acceleration", (scientific(length(sample.accelerationMps2)) + " m/s²").c_str(), 64, 326);
        line("Energy", (scientific(sample.specificEnergyJPerKg) + " J/kg").c_str(), 64, 350);
        line("Angular momentum", scientific(sample.angularMomentumMagnitudeM2PerS).c_str(), 64, 374);
        line("Orbital e", format(sample.eccentricity, 5).c_str(), 64, 398);
        line("Apoapsis", distance(sample.apoapsisM).c_str(), 64, 422);
        line("Method", integratorName(sample.integrator), 64, 446);
        line("Status", telemetryStatusName(sample.status), 64, 470);
    }
    panel(760, 160, 420, 250);
    text("SESSION", 782, 188, 11, {255, 205, 105, 255});
    line("Scenario", simulation.telemetry.metadata.scenarioName.c_str(), 782, 218);
    line("Samples", std::to_string(simulation.telemetry.samples.size()).c_str(), 782, 242);
    line("Interval", (format(simulation.telemetry.metadata.samplingIntervalSeconds, 1) + " s").c_str(), 782, 266);
    line("Frame", simulation.telemetry.metadata.referenceFrame.c_str(), 782, 290);
    line("Integrator", simulation.telemetry.metadata.integrator.c_str(), 782, 314);
    text("K start/stop   J export JSON   C export CSV   ESC return", 42, GetScreenHeight() - 34, 13, alpha(RAYWHITE, 0.68f));
}

void HUD::missionScreen(const Simulation& simulation) const {
    (void)simulation;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    text("MISSION TOOLS", 40, 94, 28, {110, 220, 255, 255});
    text("Mission analysis is available through the raylib-free mission tools API.", 42, 124, 14, alpha(RAYWHITE, 0.68f));
    panel(40, 170, 720, 300);
    text("AVAILABLE CAPABILITIES", 64, 198, 11, {255, 205, 105, 255});
    text("• Maneuver nodes and vector delta-v", 70, 236, 17, RAYWHITE);
    text("• Hohmann transfer analysis", 70, 270, 17, RAYWHITE);
    text("• Spacecraft mass and propellant accounting", 70, 304, 17, RAYWHITE);
    text("• Gravity-assist turn-angle analysis", 70, 338, 17, RAYWHITE);
    text("• Trajectory prediction and mission metrics", 70, 372, 17, RAYWHITE);
    text("No mission editor is exposed here yet; nothing is presented as a completed editor.", 70, 422, 13, alpha(ORANGE, 0.85f));
    panel(820, 170, 360, 180);
    text("NEXT STEP", 842, 198, 11, {255, 205, 105, 255});
    DrawTextEx(GetFontDefault(), "Use the mission and spacecraft APIs\nfor deterministic analysis. Return to\nSimulation for visual context.", {842, 230}, 15, 2, alpha(RAYWHITE, 0.78f));
    text("ESC return", 42, GetScreenHeight() - 34, 13, alpha(RAYWHITE, 0.68f));
}

void HUD::settingsScreen(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    text("SETTINGS", 40, 94, 28, {110, 220, 255, 255});
    text("Only live simulation settings are shown here.", 42, 124, 14, alpha(RAYWHITE, 0.68f));
    panel(40, 165, 560, 300);
    text("SIMULATION", 64, 193, 11, {255, 205, 105, 255});
    line("Integrator", simulation.settings.integrator.c_str(), 64, 230);
    line("Timestep", (format(simulation.settings.timestepSeconds, 1) + " s").c_str(), 64, 256);
    line("Speed", ("×" + format(simulation.speed, 1)).c_str(), 64, 282);
    line("Trails", simulation.showTrails ? "ON" : "OFF", 64, 308);
    line("Vectors", simulation.showVectors ? "ON" : "OFF", 64, 334);
    line("Orbits", simulation.showOrbits ? "ON" : "OFF", 64, 360);
    line("Grid", simulation.showGrid ? "ON" : "OFF", 64, 386);
    text("I cycle integrator   +/- change timestep", 64, 430, 13, alpha(RAYWHITE, 0.68f));
    panel(680, 165, 500, 240);
    text("DISPLAY CONTROLS", 704, 193, 11, {255, 205, 105, 255});
    text("V vectors   O orbits   T trails   G grid", 704, 232, 16, RAYWHITE);
    text("F fullscreen   HOME reset camera", 704, 268, 16, RAYWHITE);
    text("SPACE pause/resume   1–4 speed presets", 704, 304, 16, RAYWHITE);
    text("ESC return", 42, GetScreenHeight() - 34, 13, alpha(RAYWHITE, 0.68f));
}

void HUD::helpScreen(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {5, 12, 24, 255});
    top(simulation);
    navigation(simulation);
    text("HELP / CONTROLS", 40, 94, 28, {110, 220, 255, 255});
    text("Every control below is implemented in the current application.", 42, 124, 14, alpha(RAYWHITE, 0.68f));
    panel(40, 160, 520, 500);
    panel(610, 160, 570, 500);
    text("NAVIGATION", 64, 190, 11, {255, 205, 105, 255});
    text("Tab cycle screens   H help   L education", 64, 222, 15, RAYWHITE);
    text("F5 scenarios   F6 telemetry   F7 mission", 64, 250, 15, RAYWHITE);
    text("F8 settings   Esc return to simulation", 64, 278, 15, RAYWHITE);
    text("SIMULATION", 64, 328, 11, {255, 205, 105, 255});
    text("Space pause   +/- time scale   R reset", 64, 360, 15, RAYWHITE);
    text("V vectors   O orbits   T trails   G grid", 64, 388, 15, RAYWHITE);
    text("F fullscreen   HOME reset camera   C focus", 64, 416, 15, RAYWHITE);
    text("F1 solar   F2 Earth orbit   F3 empty space", 64, 444, 15, RAYWHITE);
    text("Ctrl+S activates selection mode; arrows or initials select bodies.", 64, 486, 14, alpha(RAYWHITE, 0.72f));
    text("EDUCATION", 634, 190, 11, {255, 205, 105, 255});
    text("A/D lessons   E/Up/Down experiments", 634, 222, 15, RAYWHITE);
    text("Z/X challenges   Enter start   B observe", 634, 250, 15, RAYWHITE);
    text("Y evaluate   C submit challenge   Q retry", 634, 278, 15, RAYWHITE);
    text("N next activity   [/] adjust answer   I method", 634, 306, 15, RAYWHITE);
    text("TELEMETRY", 634, 356, 11, {255, 205, 105, 255});
    text("K start/stop   J JSON export   C CSV export", 634, 388, 15, RAYWHITE);
    text("PREDICTION VS REFERENCE", 634, 438, 11, {255, 205, 105, 255});
    text("Provider, epoch, frame, origin, integrator, timestep", 634, 470, 14, RAYWHITE);
    text("Position error • velocity error • status", 634, 498, 14, alpha(RAYWHITE, 0.78f));
    text("SCENARIOS / SETTINGS", 634, 542, 11, {255, 205, 105, 255});
    text("Up/Down choose scenario; Enter loads it.", 634, 574, 14, RAYWHITE);
    text("I changes integrator; +/- changes timestep.", 634, 602, 14, RAYWHITE);
    text("ESC return", 42, GetScreenHeight() - 34, 13, alpha(RAYWHITE, 0.68f));
}

} // namespace bag
