#include "InputController.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "../education/EducationContent.hpp"
#include "../ui/UiTheme.hpp"

namespace bag {

namespace {

constexpr std::array<const char*, 3> SCENARIOS = {"default_solar_system", "earth_orbit", "empty_space"};

void selectScreen(Simulation& simulation, AppScreen screen) {
    simulation.setScreen(screen);
}

bool navigationClick(Simulation& simulation) {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return false;
    const Vector2 mouse = GetMousePosition();
    if (mouse.y > ui::topBarHeight()) return false;
    const ui::NavLayout nav = ui::buildNavLayout(static_cast<float>(GetScreenWidth()));
    for (std::size_t index = 0; index < ui::kNavTabs.size(); ++index) {
        if (CheckCollisionPointRec(mouse, nav.hits[index])) {
            selectScreen(simulation, ui::kNavTabs[index].screen);
            return true;
        }
    }
    return false;
}

void cycleScreen(Simulation& simulation) {
    constexpr int screenCount = 8;
    const int next = (static_cast<int>(simulation.screen) + 1) % screenCount;
    simulation.setScreen(static_cast<AppScreen>(next));
}

bool keyboardSelectSimulationBody(Simulation& simulation, bool selectionMode) {
    if (simulation.screen != AppScreen::Simulation) return false;
    if (!selectionMode) return false;

    int direction = 0;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) direction = -1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) direction = 1;
    if (direction == 0) return false;

    std::vector<int> activeIndices;
    activeIndices.reserve(simulation.bodies.size());
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        if (simulation.bodies[static_cast<std::size_t>(i)].active) activeIndices.push_back(i);
    }
    return simulation.selectBody(cycleActiveBody(simulation.selected, activeIndices, direction));
}

bool letterSelectSimulationBody(Simulation& simulation, bool selectionMode) {
    if (simulation.screen != AppScreen::Simulation || !selectionMode) return false;

    struct KeyBody { KeyboardKey key; char initial; };
    constexpr KeyBody keys[] = {
        {KEY_B, 'b'}, {KEY_E, 'e'}, {KEY_J, 'j'}, {KEY_M, 'm'},
        {KEY_N, 'n'}, {KEY_S, 's'}, {KEY_U, 'u'}, {KEY_V, 'v'},
    };
    char pressedInitial = 0;
    for (const KeyBody& key : keys) {
        if (IsKeyPressed(key.key)) {
            pressedInitial = key.initial;
            break;
        }
    }
    if (pressedInitial == 0) return false;

    std::vector<int> matches;
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        const Body& body = simulation.bodies[static_cast<std::size_t>(i)];
        if (body.active && !body.name.empty() &&
            static_cast<char>(std::tolower(static_cast<unsigned char>(body.name.front()))) == pressedInitial) {
            matches.push_back(i);
        }
    }
    if (matches.empty()) return false;

    const auto current = std::find(matches.begin(), matches.end(), simulation.selected);
    const int next = current == matches.end()
        ? matches.front()
        : matches[static_cast<std::size_t>((std::distance(matches.begin(), current) + 1) % matches.size())];
    return simulation.selectBody(next);
}

void focusEarthIfPresent(Simulation& simulation, Renderer& renderer) {
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (simulation.bodies[static_cast<std::size_t>(index)].id != "earth") continue;
        simulation.selectBody(index);
        simulation.isolateSelected();
        renderer.focusBodyIndex(simulation, index, true);
        return;
    }
    renderer.resetSystemView();
}

void focusSelectedAlone(Simulation& simulation, Renderer& renderer) {
    if (simulation.selected < 0) return;
    if (!simulation.soloStudy) renderer.saveViewBeforeIsolate();
    if (!simulation.isolateSelected()) return;
    // Same snap-follow framing used for fill-screen study of the selected body.
    renderer.focusBodyIndex(simulation, simulation.selected, true);
}

void applyScenarioCamera(Simulation& simulation, Renderer& renderer, const std::string& scenarioId) {
    if (scenarioId == "earth_orbit") focusEarthIfPresent(simulation, renderer);
    else renderer.resetSystemView();
}

} // namespace

void InputController::update(Simulation& simulation, Renderer& renderer) {
    if (simulation.screen == AppScreen::Simulation) renderer.update(simulation);
    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_S)) {
        selectionMode = true;
        if (!simulation.bodies.empty()) simulation.selectBody(0);
        return;
    }
    if (keyboardSelectSimulationBody(simulation, selectionMode)) return;
    if (letterSelectSimulationBody(simulation, selectionMode)) return;
    if (navigationClick(simulation)) return;

    if (simulation.screen == AppScreen::Simulation) {
        Vector2 pickMouse{};
        // Clicks only select. Z owns isolate zoom; Q walks back.
        const bool focusClick = renderer.consumePendingFocusPick(pickMouse);
        const bool bodyClick = !focusClick && renderer.consumePendingBodyPick(pickMouse) &&
                               !renderer.isCameraManipulating();
        if (focusClick || bodyClick) {
            const int hit = renderer.hitTest(simulation, pickMouse);
            if (simulation.soloStudy) {
                // Leave isolate and restore the previous screen, then apply the click.
                simulation.exitIsolation();
                renderer.restoreViewAfterIsolate();
            }
            if (hit < 0) simulation.clearSelection();
            else simulation.selectBody(hit);
            return;
        }
    }

    if (IsKeyPressed(KEY_TAB)) cycleScreen(simulation);
    if (IsKeyPressed(KEY_H) && simulation.screen != AppScreen::Settings) selectScreen(simulation, AppScreen::Help);
    if (IsKeyPressed(KEY_L)) {
        selectScreen(simulation, simulation.screen == AppScreen::Education ? AppScreen::Simulation : AppScreen::Education);
    }
    if (IsKeyPressed(KEY_F9)) {
        selectScreen(simulation,
                     simulation.screen == AppScreen::LearningLab ? AppScreen::Simulation : AppScreen::LearningLab);
    }
    if (IsKeyPressed(KEY_F5)) selectScreen(simulation, AppScreen::ScenarioBrowser);
    if (IsKeyPressed(KEY_F6)) selectScreen(simulation, AppScreen::Telemetry);
    if (IsKeyPressed(KEY_F7)) selectScreen(simulation, AppScreen::MissionDesigner);
    if (IsKeyPressed(KEY_F8)) selectScreen(simulation, AppScreen::Settings);

    // Q loop: un-isolate (restore screen) → deselect → leave selection mode.
    if (IsKeyPressed(KEY_Q)) {
        if (simulation.screen == AppScreen::Simulation) {
            if (simulation.soloStudy) {
                simulation.exitIsolation();
                renderer.restoreViewAfterIsolate();
                return;
            }
            if (simulation.selected >= 0) {
                simulation.clearSelection();
                return;
            }
            if (selectionMode) {
                selectionMode = false;
                return;
            }
        } else if (simulation.screen == AppScreen::Education) {
            simulation.retryEducationActivity();
            return;
        } else if (simulation.screen == AppScreen::LearningLab) {
            if (simulation.learningLabSession.state().step != LearningLabStep::Selecting) {
                simulation.selectCurriculumActivity(simulation.curriculumActivityIndex);
                return;
            }
            selectScreen(simulation, AppScreen::Simulation);
            return;
        } else {
            selectScreen(simulation, AppScreen::Simulation);
            return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        selectionMode = false;
        if (simulation.soloStudy) renderer.restoreViewAfterIsolate();
        simulation.clearSelection();
        if (simulation.screen != AppScreen::Simulation) selectScreen(simulation, AppScreen::Simulation);
    }

    if (simulation.screen == AppScreen::Education) {
        if (IsKeyPressed(KEY_A)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson - 1);
        if (IsKeyPressed(KEY_D)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson + 1);
        if (IsKeyPressed(KEY_ENTER)) simulation.startEducationActivity();
        if (IsKeyPressed(KEY_B)) simulation.beginEducationObservation();
        if (IsKeyPressed(KEY_Y)) simulation.evaluateCurrentExperiment();
        if (IsKeyPressed(KEY_N)) simulation.continueEducationActivity();
        return;
    }

    if (simulation.screen == AppScreen::LearningLab) {
        if (IsKeyPressed(KEY_LEFT_BRACKET)) simulation.cycleLearnerGrade(-1);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) simulation.cycleLearnerGrade(1);
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_UP)) {
            simulation.selectCurriculumActivity(simulation.curriculumActivityIndex - 1);
        }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_DOWN)) {
            simulation.selectCurriculumActivity(simulation.curriculumActivityIndex + 1);
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (simulation.learningLabSession.state().step == LearningLabStep::Selecting) {
                simulation.startCurriculumActivity();
            } else {
                simulation.advanceCurriculumActivity();
            }
        }
        if (IsKeyPressed(KEY_N)) simulation.advanceCurriculumActivity();
        if (IsKeyPressed(KEY_ONE)) simulation.toggleCurriculumAnswerOption(0);
        if (IsKeyPressed(KEY_TWO)) simulation.toggleCurriculumAnswerOption(1);
        if (IsKeyPressed(KEY_THREE)) simulation.toggleCurriculumAnswerOption(2);
        if (IsKeyPressed(KEY_FOUR)) simulation.toggleCurriculumAnswerOption(3);
        if (IsKeyPressed(KEY_FIVE)) simulation.toggleCurriculumAnswerOption(4);
        if (IsKeyPressed(KEY_SIX)) simulation.toggleCurriculumAnswerOption(5);
        if (IsKeyPressed(KEY_C)) simulation.submitCurriculumAssessment();
        return;
    }

    if (simulation.screen == AppScreen::ScenarioBrowser) {
        if (IsKeyPressed(KEY_UP)) simulation.scenarioBrowserSelection =
            (simulation.scenarioBrowserSelection + static_cast<int>(SCENARIOS.size()) - 1) % static_cast<int>(SCENARIOS.size());
        if (IsKeyPressed(KEY_DOWN)) simulation.scenarioBrowserSelection =
            (simulation.scenarioBrowserSelection + 1) % static_cast<int>(SCENARIOS.size());
        if (IsKeyPressed(KEY_ENTER)) {
            const char* scenarioId = SCENARIOS[static_cast<std::size_t>(simulation.scenarioBrowserSelection)];
            if (simulation.loadScenario(scenarioId)) {
                selectScreen(simulation, AppScreen::Simulation);
                applyScenarioCamera(simulation, renderer, scenarioId);
            }
        }
        return;
    }

    if (simulation.screen == AppScreen::Telemetry) {
        if (IsKeyPressed(KEY_K)) {
            if (simulation.telemetryEnabled()) simulation.stopTelemetry();
            else {
                std::string bodyId;
                if (simulation.selected >= 0 && simulation.selected < static_cast<int>(simulation.bodies.size()))
                    bodyId = simulation.bodies[static_cast<std::size_t>(simulation.selected)].id;
                simulation.startTelemetry(60.0, bodyId);
            }
        }
        if (IsKeyPressed(KEY_J)) simulation.exportTelemetryJson("telemetry.json");
        if (IsKeyPressed(KEY_C)) simulation.exportTelemetryCsv("telemetry.csv");
        return;
    }

    if (simulation.screen == AppScreen::Settings) {
        if (IsKeyPressed(KEY_I)) simulation.cycleIntegrator();
        if (IsKeyPressed(KEY_H)) simulation.settings.selectionHighlightEnabled = !simulation.settings.selectionHighlightEnabled;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) simulation.adjustTimestep(2.0);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) simulation.adjustTimestep(0.5);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            const Vector2 mouse = GetMousePosition();
            const float leftX = ui::pagePad();
            const float leftY = ui::contentTop() + 70.0f;
            if (mouse.x >= leftX + 20.0f && mouse.x <= leftX + 520.0f &&
                mouse.y >= leftY + 246.0f && mouse.y <= leftY + 276.0f) {
                simulation.settings.selectionHighlightEnabled = !simulation.settings.selectionHighlightEnabled;
            }
        }
        return;
    }

    if (simulation.screen == AppScreen::MissionDesigner || simulation.screen == AppScreen::Help) return;

    if (IsKeyPressed(KEY_HOME)) {
        simulation.exitIsolation();
        renderer.resetSystemView();
    }
    if (IsKeyPressed(KEY_Z)) {
        if (simulation.selected >= 0) {
            focusSelectedAlone(simulation, renderer);
        } else {
            simulation.selectEducationActivity(EducationActivityType::Challenge, simulation.challenge - 1);
        }
    }
    if (IsKeyPressed(KEY_C)) {
        simulation.submitChallenge();
    }

    if (IsKeyPressed(KEY_SPACE)) simulation.paused = !simulation.paused;
    if (IsKeyPressed(KEY_A)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson - 1);
    if (IsKeyPressed(KEY_D)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson + 1);
    if (IsKeyPressed(KEY_E)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
    if (IsKeyPressed(KEY_UP)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment - 1);
    if (IsKeyPressed(KEY_DOWN)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
    if (IsKeyPressed(KEY_ENTER)) simulation.startEducationActivity();
    if (IsKeyPressed(KEY_B)) simulation.beginEducationObservation();
    if (IsKeyPressed(KEY_Y)) simulation.evaluateCurrentExperiment();
    // KEY_Z with a selection zooms; without selection it steps challenges (handled above).
    if (IsKeyPressed(KEY_X)) simulation.selectEducationActivity(EducationActivityType::Challenge, simulation.challenge + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) simulation.adjustChallengeAnswer(-0.05);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) simulation.adjustChallengeAnswer(0.05);
    if (IsKeyPressed(KEY_I)) simulation.cycleChallengeIntegrator();
    if (IsKeyPressed(KEY_N)) simulation.continueEducationActivity();
    if (IsKeyPressed(KEY_V)) {
        simulation.showVectors = !simulation.showVectors;
        simulation.settings.vectorsEnabled = simulation.showVectors;
    }
    if (IsKeyPressed(KEY_O)) simulation.showOrbits = !simulation.showOrbits;
    if (IsKeyPressed(KEY_T)) {
        simulation.showTrails = !simulation.showTrails;
        simulation.settings.trailsEnabled = simulation.showTrails;
    }
    if (IsKeyPressed(KEY_G)) simulation.showGrid = !simulation.showGrid;
    if (IsKeyPressed(KEY_R)) simulation.reset();
    if (IsKeyPressed(KEY_F1)) {
        if (simulation.loadScenario("default_solar_system")) applyScenarioCamera(simulation, renderer, "default_solar_system");
    }
    if (IsKeyPressed(KEY_F2)) {
        if (simulation.loadScenario("earth_orbit")) applyScenarioCamera(simulation, renderer, "earth_orbit");
    }
    if (IsKeyPressed(KEY_F3)) {
        if (simulation.loadScenario("empty_space")) applyScenarioCamera(simulation, renderer, "empty_space");
    }
    if (IsKeyPressed(KEY_ONE)) simulation.setSpeed(1.0);
    if (IsKeyPressed(KEY_TWO)) simulation.setSpeed(10.0);
    if (IsKeyPressed(KEY_THREE)) simulation.setSpeed(100.0);
    if (IsKeyPressed(KEY_FOUR)) simulation.setSpeed(1000.0);
    if (IsKeyPressed(KEY_FIVE)) simulation.setSpeed(10000.0);
    if (IsKeyPressed(KEY_SIX)) simulation.setSpeed(100000.0);
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) simulation.adjustSpeed(1);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) simulation.adjustSpeed(-1);
    if (IsKeyPressed(KEY_P)) {
        if (simulation.launchProbe(3500.0)) {
            renderer.focusBodyIndex(simulation, simulation.selected, true);
        }
    }
}

} // namespace bag
