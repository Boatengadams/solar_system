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
#include "UiTheme.hpp"

namespace bag {
namespace {

using ui::theme;
using ui::withAlpha;

std::string format(double value, int digits = 2) {
    if (!std::isfinite(value)) return "—";
    std::ostringstream output;
    output << std::fixed << std::setprecision(digits) << value;
    return output.str();
}

std::string scientific(double value, int digits = 3) {
    if (!std::isfinite(value)) return "—";
    std::ostringstream output;
    output << std::scientific << std::setprecision(digits) << value;
    return output.str();
}

std::string distance(double meters) {
    if (!std::isfinite(meters)) return "—";
    const double au = std::abs(meters) / PhysicsEngine::AU;
    if (au >= 0.01) return format(meters / PhysicsEngine::AU, 3) + " AU";
    if (std::abs(meters) >= 1e9) return format(meters / 1e9, 2) + " ×10⁹ km";
    return format(meters / 1000.0, 0) + " km";
}

std::string time(double seconds) {
    const double absolute = std::abs(seconds);
    if (absolute < 60) return format(absolute, 1) + " s";
    if (absolute < PhysicsEngine::DAY) return format(absolute / 3600.0, 1) + " h";
    if (absolute < PhysicsEngine::YEAR) return format(absolute / PhysicsEngine::DAY, 1) + " d";
    return format(absolute / PhysicsEngine::YEAR, 2) + " y";
}

std::string dayLengthLabel(double periodSeconds) {
    if (!std::isfinite(periodSeconds) || periodSeconds == 0.0) return "—";
    const double absolute = std::abs(periodSeconds);
    if (absolute < PhysicsEngine::DAY * 2.5) return format(absolute / 3600.0, 1) + " h";
    return format(absolute / PhysicsEngine::DAY, 1) + " Earth days";
}

std::string spinDirectionLabel(const Body& body) {
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return "—";
    if (std::abs(body.axialTilt) >= 80.0) return "Sideways";
    if (body.rotationPeriod < 0.0) return "Retrograde";
    return "Prograde";
}

std::string formatSpeed(double speed) {
    if (speed >= 1000.0) return "×" + format(speed, 0);
    if (speed < 1.0) return "×" + format(speed, 2);
    if (speed < 10.0) return "×" + format(speed, 1);
    return "×" + format(speed, 0);
}

void drawWrapped(const std::string& content, float x, float y, float maxWidth, int size, Color color, int maxLines = 4) {
    if (content.empty() || maxLines <= 0) return;
    std::string line;
    int linesDrawn = 0;
    auto flush = [&]() {
        DrawText(line.c_str(), static_cast<int>(x), static_cast<int>(y + linesDrawn * (size + 4)), size, color);
        ++linesDrawn;
        line.clear();
    };
    std::string word;
    for (std::size_t i = 0; i <= content.size(); ++i) {
        const bool end = i == content.size();
        const char ch = end ? ' ' : content[i];
        if (ch == ' ' || ch == '\n' || end) {
            if (!word.empty()) {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (MeasureText(candidate.c_str(), size) > static_cast<int>(maxWidth) && !line.empty()) {
                    flush();
                    if (linesDrawn >= maxLines) return;
                    line = word;
                } else {
                    line = candidate;
                }
                word.clear();
            }
            if (ch == '\n') {
                flush();
                if (linesDrawn >= maxLines) return;
            }
        } else {
            word.push_back(ch);
        }
    }
    if (!line.empty() && linesDrawn < maxLines) flush();
}

} // namespace

void HUD::text(const char* value, float x, float y, float size, Color colorValue) const {
    DrawText(value, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), colorValue);
}

void HUD::pageChrome(const Simulation& simulation, const char* title, const char* subtitle) const {
    const auto& t = theme();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), t.bg);
    // Soft atmospheric wash
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           withAlpha(ui::rgba(18, 36, 48), 0.55f), t.bg);
    topBar(simulation);
    text(title, ui::pagePad(), ui::contentTop(), 28, t.text);
    text(subtitle, ui::pagePad(), ui::contentTop() + 34.0f, 14, t.textMuted);
}

void HUD::topBar(const Simulation& simulation) const {
    const auto& t = theme();
    const float barH = ui::topBarHeight();
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(barH), withAlpha(t.bgElevated, 0.96f));
    DrawRectangle(0, static_cast<int>(barH) - 1, GetScreenWidth(), 1, withAlpha(t.border, 0.7f));

    text("BAGS_LAB", 24, 14, 24, t.accentStrong);
    text("Orbital lab", 26, 40, 11, t.textDim);

    const ui::NavLayout nav = ui::buildNavLayout(static_cast<float>(GetScreenWidth()));
    for (std::size_t index = 0; index < ui::kNavTabs.size(); ++index) {
        const bool active = simulation.screen == ui::kNavTabs[index].screen;
        ui::drawChip(nav.hits[index], ui::kNavTabs[index].label, active);
    }

    const float right = static_cast<float>(GetScreenWidth());
    const Rectangle status = {right - 250.0f, 14.0f, 100.0f, 36.0f};
    ui::drawChip(status, simulation.paused ? "Paused" : "Live", !simulation.paused,
                 simulation.paused ? t.warn : t.success);

    text(time(simulation.simTime).c_str(), right - 138.0f, 16.0f, 16, t.accentStrong);
    text(formatSpeed(simulation.speed).c_str(), right - 138.0f, 36.0f, 13, t.textMuted);
}

void HUD::bottomBar(const Simulation& simulation) const {
    (void)simulation;
    const auto& t = theme();
    const float y = static_cast<float>(GetScreenHeight()) - ui::bottomBarHeight();
    DrawRectangle(0, static_cast<int>(y), GetScreenWidth(), static_cast<int>(ui::bottomBarHeight()),
                  withAlpha(t.bgElevated, 0.94f));
    DrawRectangle(0, static_cast<int>(y), GetScreenWidth(), 1, withAlpha(t.border, 0.55f));
    text("Drag orbit  ·  Right/Middle pan  ·  Wheel zoom  ·  Click select  ·  Z zoom  ·  Q back",
         24, y + 14.0f, 13, t.textMuted);
    text("Space pause   P pulse probe   ± speed   5/6 fast",
         static_cast<float>(GetScreenWidth()) - 390.0f, y + 14.0f, 13, t.textDim);
}

void HUD::selectedInfo(const Simulation& simulation) const {
    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const auto& t = theme();
    const float boxWidth = std::min(340.0f, GetScreenWidth() - 40.0f);
    const float boxHeight = simulation.soloStudy ? 400.0f : 300.0f;
    const Rectangle box = {
        20.0f,
        static_cast<float>(GetScreenHeight()) - ui::bottomBarHeight() - boxHeight - 16.0f,
        boxWidth,
        boxHeight,
    };
    ui::drawPanel(box, true);
    DrawRectangleRounded({box.x, box.y, 5.0f, box.height}, 0.2f, 8,
                         {body.accent.r, body.accent.g, body.accent.b, 255});

    text(simulation.soloStudy ? "STUDY" : "SELECTED", box.x + 18, box.y + 14, 11,
         simulation.soloStudy ? t.accent : t.textDim);
    text(body.name.c_str(), box.x + 18, box.y + 32, 22, t.text);
    text(body.type.c_str(), box.x + 18, box.y + 58, 12, t.textMuted);

    ui::drawLabelValue("Mass", (scientific(body.mass) + " kg").c_str(), box.x + 18, box.y + 88);
    ui::drawLabelValue("Radius", distance(body.realRadius).c_str(), box.x + 18, box.y + 110);
    ui::drawLabelValue("Distance", distance(simulation.distanceFromSun(body)).c_str(), box.x + 18, box.y + 132);
    ui::drawLabelValue("Velocity", (format(length(body.velocity) / 1000.0, 2) + " km/s").c_str(), box.x + 18, box.y + 154);
    ui::drawLabelValue("Day length", dayLengthLabel(body.rotationPeriod).c_str(), box.x + 18, box.y + 176);
    ui::drawLabelValue("Axial tilt", (format(body.axialTilt, 1) + " deg").c_str(), box.x + 18, box.y + 198);
    ui::drawLabelValue("Spin", spinDirectionLabel(body).c_str(), box.x + 18, box.y + 220);

    if (simulation.soloStudy) {
        text("Learn", box.x + 18, box.y + 252, 11, t.accent);
        drawWrapped(simulation.selectedBodyLesson(), box.x + 18, box.y + 272, box.width - 36.0f, 13, t.textMuted, 4);
        text("Amber axis = tilt · cyan equator · orange tick = spin", box.x + 18, box.y + 348, 11, t.textDim);
        text("Spin motion exaggerated for study · Q exit", box.x + 18, box.y + 368, 11, t.textDim);
    } else {
        text("Z fills the screen with this body   ·   Q back", box.x + 18, box.y + 252, 12, t.textDim);
        if (body.id == "earth" || body.id == "moon" || body.type == "Moon") {
            text("Earth study also keeps the Moon nearby", box.x + 18, box.y + 272, 12, t.textDim);
        }
    }
}

void HUD::draw(const Simulation& simulation) const {
    switch (simulation.screen) {
    case AppScreen::Simulation:
        topBar(simulation);
        selectedInfo(simulation);
        bottomBar(simulation);
        break;
    case AppScreen::Education: educationScreenPanel(simulation); break;
    case AppScreen::LearningLab: learningLabScreenPanel(simulation); break;
    case AppScreen::ScenarioBrowser: scenarioScreen(simulation); break;
    case AppScreen::Telemetry: telemetryScreen(simulation); break;
    case AppScreen::MissionDesigner: missionScreen(simulation); break;
    case AppScreen::Settings: settingsScreen(simulation); break;
    case AppScreen::Help: helpScreen(simulation); break;
    }
}

void HUD::educationScreenPanel(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Learn", "Guided orbital-mechanics lessons, experiments, and challenges");
    const LearnerReport report = buildLearnerReport(simulation.educationProgress);
    const AuthoredLesson& lesson = authoredLessonAt(simulation.lesson);

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 340.0f, 560.0f};
    const Rectangle mid = {ui::pagePad() + 360.0f, ui::contentTop() + 70.0f, 420.0f, 560.0f};
    const Rectangle right = {ui::pagePad() + 800.0f, ui::contentTop() + 70.0f, 340.0f, 560.0f};
    ui::drawPanel(left);
    ui::drawPanel(mid);
    ui::drawPanel(right);

    text("Progress", left.x + 20, left.y + 18, 12, t.accent);
    text(("Lessons  " + std::to_string(report.completedLessons) + " / " + std::to_string(report.totalLessons)).c_str(),
         left.x + 20, left.y + 44, 14, t.text);
    text(("Experiments  " + std::to_string(report.completedExperiments) + " / " + std::to_string(report.totalExperiments)).c_str(),
         left.x + 20, left.y + 68, 14, t.text);
    text(("Challenges  " + std::to_string(report.completedChallenges) + " / " + std::to_string(report.totalChallenges)).c_str(),
         left.x + 20, left.y + 92, 14, t.text);
    DrawRectangleRounded({left.x + 20, left.y + 126, 300, 8}, 0.5f, 8, t.surfaceAlt);
    const float fill = report.totalLessons == 0 ? 0.0f
        : 300.0f * static_cast<float>(report.completedLessons) / static_cast<float>(report.totalLessons);
    DrawRectangleRounded({left.x + 20, left.y + 126, fill, 8}, 0.5f, 8, t.accent);

    text("Lessons", left.x + 20, left.y + 160, 12, t.textDim);
    for (int index = 0; index < authoredLessonCount(); ++index) {
        const AuthoredLesson& item = authoredLessonAt(index);
        const bool selected = index == simulation.lesson;
        const bool complete = simulation.educationProgress.lessonComplete(index);
        const float y = left.y + 186.0f + index * 34.0f;
        if (selected) DrawRectangleRounded({left.x + 12, y - 6, 316, 30}, 0.2f, 8, withAlpha(t.accent, 0.18f));
        text((std::string(complete ? "● " : "○ ") + item.title).c_str(), left.x + 22, y, 14,
             selected ? t.text : t.textMuted);
    }

    text(lesson.title.c_str(), mid.x + 22, mid.y + 22, 24, t.text);
    text(lesson.shortDescription.c_str(), mid.x + 22, mid.y + 56, 14, t.textMuted);
    text("Objectives", mid.x + 22, mid.y + 100, 12, t.accent);
    for (std::size_t index = 0; index < lesson.objectives.size() && index < 4; ++index) {
        text(("• " + lesson.objectives[index]).c_str(), mid.x + 22, mid.y + 126.0f + index * 24.0f, 14, t.text);
    }
    text(("Difficulty  " + std::string(educationDifficultyName(lesson.difficulty))).c_str(),
         mid.x + 22, mid.y + 250, 13, t.textMuted);
    text(("Duration  " + std::to_string(lesson.estimatedMinutes) + " min").c_str(),
         mid.x + 22, mid.y + 274, 13, t.textMuted);
    text(simulation.educationProgress.lessonComplete(simulation.lesson) ? "Completed" : "In progress",
         mid.x + 22, mid.y + 310, 16,
         simulation.educationProgress.lessonComplete(simulation.lesson) ? t.success : t.warn);

    text("Learner report", right.x + 20, right.y + 18, 12, t.accent);
    text(report.hasAverageScore ? ("Avg score  " + format(report.averageScore, 1)).c_str() : "Avg score  —",
         right.x + 20, right.y + 48, 14, t.text);
    text("Strongest", right.x + 20, right.y + 90, 12, t.textDim);
    if (report.strongestAreas.empty()) text("None yet", right.x + 20, right.y + 114, 13, t.textMuted);
    for (std::size_t i = 0; i < report.strongestAreas.size() && i < 3; ++i)
        text(("• " + report.strongestAreas[i]).c_str(), right.x + 20, right.y + 114.0f + i * 22.0f, 13, t.text);
    text("Practice next", right.x + 20, right.y + 200, 12, t.textDim);
    if (report.areasNeedingImprovement.empty()) text("None identified", right.x + 20, right.y + 224, 13, t.textMuted);
    for (std::size_t i = 0; i < report.areasNeedingImprovement.size() && i < 3; ++i)
        text(("• " + report.areasNeedingImprovement[i]).c_str(), right.x + 20, right.y + 224.0f + i * 22.0f, 13, t.text);
    text("Recommended", right.x + 20, right.y + 320, 12, t.accent);
    text(report.recommendation.available ? report.recommendation.title.c_str() : "None",
         right.x + 20, right.y + 348, 15, t.accentStrong);

    text("A / D browse   Enter start   B observe   Y evaluate   N next   Esc back",
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::learningLabScreenPanel(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Learning Lab", "Ghana curriculum activities on the shared simulation engine");

    if (!simulation.curriculumReady) {
        const Rectangle box = {ui::pagePad(), ui::contentTop() + 70.0f, 900.0f, 140.0f};
        ui::drawPanel(box);
        text("Curriculum pack failed to load", box.x + 24, box.y + 36, 20, t.warn);
        DrawTextEx(GetFontDefault(), simulation.curriculumLoadError.c_str(),
                   {box.x + 24, box.y + 74}, 14, 1, t.textMuted);
        return;
    }

    const std::string gradeKey = std::string("grade.") + gradeIdString(simulation.learnerGrade);
    const std::string gradeLabel = simulation.curriculumI18n.translate(gradeKey, gradeIdString(simulation.learnerGrade));
    text((gradeLabel + "  ·  " + presentationLayerName(simulation.learnerPresentationLayer()) +
          "  ·  " + simulation.curriculumCatalog.manifest.curriculumVersion).c_str(),
         ui::pagePad(), ui::contentTop() + 58.0f, 14, t.textMuted);

    const auto activities = simulation.activitiesForLearnerGrade();
    const Rectangle left = {ui::pagePad(), ui::contentTop() + 90.0f, 360.0f, 520.0f};
    const Rectangle right = {ui::pagePad() + 380.0f, ui::contentTop() + 90.0f, 760.0f, 520.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Activities", left.x + 20, left.y + 18, 12, t.accent);
    if (activities.empty()) text("No activities for this grade.", left.x + 20, left.y + 54, 14, t.textMuted);
    const int visible = std::min(static_cast<int>(activities.size()), 14);
    for (int index = 0; index < visible; ++index) {
        const CurriculumActivity* activity = activities[static_cast<std::size_t>(index)];
        const bool selected = index == simulation.curriculumActivityIndex;
        const float y = left.y + 52.0f + index * 30.0f;
        if (selected) DrawRectangleRounded({left.x + 12, y - 5, 336, 28}, 0.2f, 8, withAlpha(t.accent, 0.18f));
        const std::string title = simulation.curriculumI18n.translate(activity->titleKey, activity->id);
        text(ui::shorten(title, 34).c_str(), left.x + 22, y, 14, selected ? t.text : t.textMuted);
    }

    const CurriculumActivity* activity = simulation.selectedCurriculumActivity();
    if (!activity) {
        text("Select a grade activity to begin.", right.x + 24, right.y + 28, 16, t.textMuted);
        return;
    }

    const std::string title = simulation.curriculumI18n.translate(activity->titleKey, activity->id);
    text(title.c_str(), right.x + 24, right.y + 22, 22, t.text);
    text((std::string(curriculumAlignmentName(activity->alignment)) + "  ·  " + activity->curriculumReference).c_str(),
         right.x + 24, right.y + 52, 13,
         activity->alignment == CurriculumAlignment::Official ? t.success : t.warn);
    text((activity->subject + " / " + activity->strand + " / " + activity->subStrand).c_str(),
         right.x + 24, right.y + 74, 13, t.textMuted);
    DrawTextEx(GetFontDefault(), activity->learningObjective.c_str(),
               {right.x + 24, right.y + 104}, 14, 1, t.text);

    const LearningLabState& lab = simulation.learningLabSession.state();
    text(("Session  " + std::string(LearningLabSession::stepName(lab.step))).c_str(),
         right.x + 24, right.y + 170, 12, t.accent);

    if (lab.step == LearningLabStep::Selecting) {
        text("Press Enter to start the inquiry sequence.", right.x + 24, right.y + 198, 14, t.textMuted);
        DrawTextEx(GetFontDefault(),
                   simulation.curriculumI18n.translate(activity->instructionsKey, activity->instructionsKey).c_str(),
                   {right.x + 24, right.y + 228}, 14, 1, t.text);
    } else if (const CurriculumActivity* active = simulation.learningLabSession.activity()) {
        if (lab.stepIndex >= 0 && lab.stepIndex < static_cast<int>(active->steps.size())) {
            const ActivityStep& step = active->steps[static_cast<std::size_t>(lab.stepIndex)];
            text(simulation.curriculumI18n.translate(step.titleKey, LearningLabSession::stepName(lab.step)).c_str(),
                 right.x + 24, right.y + 198, 16, t.text);
            if (!step.bodyKey.empty()) {
                DrawTextEx(GetFontDefault(),
                           simulation.curriculumI18n.translate(step.bodyKey, step.bodyKey).c_str(),
                           {right.x + 24, right.y + 228}, 14, 1, t.textMuted);
            }
        }

        if (lab.step == LearningLabStep::Assess) {
            const CurriculumQuestion* question = simulation.selectedCurriculumQuestion();
            if (question) {
                DrawTextEx(GetFontDefault(),
                           simulation.curriculumI18n.translate(question->questionKey, question->id).c_str(),
                           {right.x + 24, right.y + 280}, 14, 1, t.text);
                for (std::size_t index = 0; index < question->options.size() && index < 6; ++index) {
                    const QuestionOption& option = question->options[index];
                    const bool chosen = std::find(simulation.curriculumAnswerSelection.begin(),
                                                  simulation.curriculumAnswerSelection.end(),
                                                  option.id) != simulation.curriculumAnswerSelection.end();
                    const float y = right.y + 316.0f + static_cast<float>(index) * 24.0f;
                    text((std::to_string(index + 1) + "  " + (chosen ? "● " : "○ ") +
                          simulation.curriculumI18n.translate(option.labelKey, option.id)).c_str(),
                         right.x + 24, y, 14, chosen ? t.accentStrong : t.textMuted);
                }
            }
        }

        if ((lab.step == LearningLabStep::Feedback || lab.step == LearningLabStep::Complete) &&
            simulation.lastCurriculumAssessment) {
            const AssessmentResult& result = *simulation.lastCurriculumAssessment;
            text((std::string(result.correct ? "Passed" : "Try again") + "  " + format(result.score, 0) + "/100").c_str(),
                 right.x + 24, right.y + 280, 18, result.correct ? t.success : t.warn);
            text(simulation.curriculumI18n.translate(result.feedbackKey, result.feedbackKey).c_str(),
                 right.x + 24, right.y + 312, 14, t.text);
            if (simulation.lastCurriculumMisconception) {
                text(simulation.curriculumI18n.translate(simulation.lastCurriculumMisconception->responseKey,
                                                         simulation.lastCurriculumMisconception->responseKey).c_str(),
                     right.x + 24, right.y + 348, 14, t.warn);
            }
        }
    }

    text("[ ] grade   A/D activity   Enter start   N next   1–6 answers   C submit   Esc back",
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::scenarioScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Scenes", "Choose a starting system for the laboratory");

    const struct Scenario { const char* id; const char* title; const char* description; const char* bodies; } scenarios[] = {
        {"default_solar_system", "Solar System", "Sun, planets, Moon, and BAGSOLAR-1.", "11 bodies"},
        {"earth_orbit", "Earth Orbit", "Focused Sun–Earth–Moon study.", "3 bodies"},
        {"empty_space", "Empty Space", "Blank scene for custom systems.", "0 bodies"},
    };

    for (int index = 0; index < 3; ++index) {
        const float y = ui::contentTop() + 80.0f + index * 110.0f;
        const Rectangle card = {ui::pagePad(), y, 720.0f, 92.0f};
        const bool selected = index == simulation.scenarioBrowserSelection;
        ui::drawPanel(card, selected);
        if (selected) DrawRectangleRoundedLines(card, 0.08f, 16, t.accent);
        text(scenarios[index].title, card.x + 24, card.y + 22, 22, selected ? t.text : t.textMuted);
        text(scenarios[index].description, card.x + 24, card.y + 54, 14, t.textMuted);
        text(scenarios[index].bodies, card.x + 560, card.y + 36, 14, t.accentStrong);
    }

    const Rectangle side = {ui::pagePad() + 760.0f, ui::contentTop() + 80.0f, 380.0f, 320.0f};
    ui::drawPanel(side);
    text("Current", side.x + 22, side.y + 22, 12, t.accent);
    text(simulation.scenarioMetadata.name.c_str(), side.x + 22, side.y + 52, 22, t.text);
    DrawTextEx(GetFontDefault(), simulation.scenarioMetadata.description.c_str(),
               {side.x + 22, side.y + 92}, 14, 1, t.textMuted);
    ui::drawLabelValue("Epoch", simulation.scenarioMetadata.epoch.c_str(), side.x + 22, side.y + 170);
    ui::drawLabelValue("Frame", simulation.scenarioMetadata.referenceFrame.c_str(), side.x + 22, side.y + 196);

    text("↑↓ select   Enter load   Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::telemetryScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Telemetry", "Deterministic sampling of scientific state");

    const Rectangle main = {ui::pagePad(), ui::contentTop() + 70.0f, 760.0f, 500.0f};
    const Rectangle side = {ui::pagePad() + 790.0f, ui::contentTop() + 70.0f, 350.0f, 500.0f};
    ui::drawPanel(main);
    ui::drawPanel(side);

    text(simulation.telemetryEnabled() ? "Live session" : "No active session",
         main.x + 24, main.y + 24, 16, simulation.telemetryEnabled() ? t.success : t.warn);

    if (simulation.telemetry.samples.empty()) {
        text("No samples yet.", main.x + 24, main.y + 80, 20, t.text);
        text("Press K to start a 60 s session. Select a body first for relative values.",
             main.x + 24, main.y + 118, 14, t.textMuted);
    } else {
        const TelemetrySample& sample = simulation.telemetry.samples.back();
        text(sample.bodyId.c_str(), main.x + 24, main.y + 70, 24, t.accentStrong);
        ui::drawLabelValue("Time", (format(sample.simulationTimeSeconds, 1) + " s").c_str(), main.x + 24, main.y + 120);
        ui::drawLabelValue("Velocity", (format(length(sample.velocityMps) / 1000.0, 3) + " km/s").c_str(), main.x + 24, main.y + 146);
        ui::drawLabelValue("Acceleration", (scientific(length(sample.accelerationMps2)) + " m/s²").c_str(), main.x + 24, main.y + 172);
        ui::drawLabelValue("Energy", (scientific(sample.specificEnergyJPerKg) + " J/kg").c_str(), main.x + 24, main.y + 198);
        ui::drawLabelValue("Ang. mom.", scientific(sample.angularMomentumMagnitudeM2PerS).c_str(), main.x + 24, main.y + 224);
        ui::drawLabelValue("Eccentricity", format(sample.eccentricity, 5).c_str(), main.x + 24, main.y + 250);
        ui::drawLabelValue("Apoapsis", distance(sample.apoapsisM).c_str(), main.x + 24, main.y + 276);
        ui::drawLabelValue("Method", integratorName(sample.integrator), main.x + 24, main.y + 302);
        ui::drawLabelValue("Status", telemetryStatusName(sample.status), main.x + 24, main.y + 328);
    }

    text("Session", side.x + 22, side.y + 22, 12, t.accent);
    ui::drawLabelValue("Scenario", simulation.telemetry.metadata.scenarioName.c_str(), side.x + 22, side.y + 60);
    ui::drawLabelValue("Samples", std::to_string(simulation.telemetry.samples.size()).c_str(), side.x + 22, side.y + 86);
    ui::drawLabelValue("Interval", (format(simulation.telemetry.metadata.samplingIntervalSeconds, 1) + " s").c_str(),
                       side.x + 22, side.y + 112);
    ui::drawLabelValue("Frame", simulation.telemetry.metadata.referenceFrame.c_str(), side.x + 22, side.y + 138);
    ui::drawLabelValue("Integrator", simulation.telemetry.metadata.integrator.c_str(), side.x + 22, side.y + 164);

    text("K start/stop   J JSON   C CSV   Esc back",
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::missionScreen(const Simulation& simulation) const {
    (void)simulation;
    const auto& t = theme();
    pageChrome(simulation, "Mission Tools", "Analytical APIs for maneuvers and trajectory studies");

    const Rectangle main = {ui::pagePad(), ui::contentTop() + 70.0f, 820.0f, 420.0f};
    const Rectangle side = {ui::pagePad() + 850.0f, ui::contentTop() + 70.0f, 290.0f, 420.0f};
    ui::drawPanel(main);
    ui::drawPanel(side);

    text("Available capabilities", main.x + 24, main.y + 24, 12, t.accent);
    const char* items[] = {
        "Maneuver nodes and vector delta-v",
        "Hohmann transfer analysis",
        "Spacecraft mass and propellant accounting",
        "Gravity-assist turn-angle analysis",
        "Trajectory prediction and mission metrics",
    };
    for (int i = 0; i < 5; ++i) text(("•  " + std::string(items[i])).c_str(), main.x + 28, main.y + 70.0f + i * 36.0f, 16, t.text);
    text("No full mission editor is exposed yet — this screen stays honest about the API surface.",
         main.x + 24, main.y + 280, 14, t.warn);

    text("Next step", side.x + 20, side.y + 24, 12, t.accent);
    DrawTextEx(GetFontDefault(),
               "Use the mission and spacecraft APIs for deterministic analysis. Return to Sim for visual context.",
               {side.x + 20, side.y + 60}, 14, 1, t.textMuted);

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::settingsScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Settings", "Live simulation and presentation controls");

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 560.0f, 460.0f};
    const Rectangle right = {ui::pagePad() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 460.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Simulation", left.x + 24, left.y + 22, 12, t.accent);
    ui::drawLabelValue("Integrator", simulation.settings.integrator.c_str(), left.x + 24, left.y + 60);
    ui::drawLabelValue("Timestep", (format(simulation.settings.timestepSeconds, 1) + " s").c_str(), left.x + 24, left.y + 88);
    ui::drawLabelValue("Speed", formatSpeed(simulation.speed).c_str(), left.x + 24, left.y + 116);
    ui::drawLabelValue("Trails", simulation.showTrails ? "On" : "Off", left.x + 24, left.y + 144);
    ui::drawLabelValue("Vectors", simulation.showVectors ? "On" : "Off", left.x + 24, left.y + 172);
    ui::drawLabelValue("Orbits", simulation.showOrbits ? "On" : "Off", left.x + 24, left.y + 200);
    ui::drawLabelValue("Grid", simulation.showGrid ? "On" : "Off", left.x + 24, left.y + 228);
    ui::drawLabelValue("Highlight", simulation.settings.selectionHighlightEnabled ? "On" : "Off", left.x + 24, left.y + 256);
    DrawRectangleLinesEx({left.x + 20, left.y + 246, 500, 30}, 1.0f, withAlpha(t.accent, 0.45f));
    text("Click Highlight row or press H to toggle", left.x + 24, left.y + 300, 13, t.textMuted);
    text(("Lab grade " + std::string(gradeIdString(simulation.learnerGrade)) + " · " +
          presentationLayerName(simulation.learnerPresentationLayer())).c_str(),
         left.x + 24, left.y + 330, 13, t.textMuted);
    text("I integrator   ± timestep", left.x + 24, left.y + 370, 13, t.textDim);

    text("Camera & display", right.x + 24, right.y + 22, 12, t.accent);
    text("Left-drag   orbit", right.x + 24, right.y + 60, 15, t.text);
    text("Right / Middle-drag   pan", right.x + 24, right.y + 90, 15, t.text);
    text("Wheel   zoom   ·   Shift+Wheel faster", right.x + 24, right.y + 120, 15, t.text);
    text("Click body   select   ·   Z zoom alone", right.x + 24, right.y + 150, 15, t.text);
    text("Q back one step   ·   Home system view", right.x + 24, right.y + 180, 15, t.text);
    text("Double-click   focus body under cursor", right.x + 24, right.y + 210, 15, t.text);
    text("F fullscreen", right.x + 24, right.y + 240, 15, t.text);
    text("V O T G   vectors / orbits / trails / grid", right.x + 24, right.y + 290, 14, t.textMuted);

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::helpScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Help", "Controls that exist in the current build");

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 560.0f, 520.0f};
    const Rectangle right = {ui::pagePad() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 520.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Navigate", left.x + 24, left.y + 22, 12, t.accent);
    text("Tab cycle screens", left.x + 24, left.y + 56, 15, t.text);
    text("L Learn   F9 Lab   F5 Scenes", left.x + 24, left.y + 86, 15, t.text);
    text("F6 Telemetry   F7 Mission   F8 Settings", left.x + 24, left.y + 116, 15, t.text);
    text("F1–F3 load scenes   H Help   Esc → Sim", left.x + 24, left.y + 146, 15, t.text);

    text("Simulate", left.x + 24, left.y + 200, 12, t.accent);
    text("Space pause   ± speed   R reset", left.x + 24, left.y + 234, 15, t.text);
    text("V O T G overlays   P pulse probe", left.x + 24, left.y + 264, 15, t.text);
    text("1/2/3/4/5/6 speed ×1 … ×100000", left.x + 24, left.y + 294, 14, t.text);
    text("Click select · Z zoom · Q back one step", left.x + 24, left.y + 324, 14, t.textMuted);

    text("Camera", left.x + 24, left.y + 370, 12, t.accent);
    text("Drag orbit · Right/Middle pan · Wheel zoom", left.x + 24, left.y + 404, 14, t.text);
    text("Z zoom · Q back · Home reset · Click select", left.x + 24, left.y + 434, 14, t.text);

    text("Learn / Lab", right.x + 24, right.y + 22, 12, t.accent);
    text("A/D lessons   Enter start   B observe", right.x + 24, right.y + 56, 15, t.text);
    text("Y evaluate   N next   Q retry", right.x + 24, right.y + 86, 15, t.text);
    text("Lab: [ ] grade · 1–6 answers · C submit", right.x + 24, right.y + 116, 15, t.text);

    text("Telemetry", right.x + 24, right.y + 180, 12, t.accent);
    text("K start/stop   J JSON   C CSV", right.x + 24, right.y + 214, 15, t.text);

    text("Tips", right.x + 24, right.y + 280, 12, t.accent);
    text("Keep Sim uncluttered — open Learn or Lab for teaching flows.", right.x + 24, right.y + 314, 14, t.textMuted);
    text("Selection highlight can be toggled in Settings.", right.x + 24, right.y + 344, 14, t.textMuted);

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

} // namespace bag
