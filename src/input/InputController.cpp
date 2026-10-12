#include "InputController.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

#include "../education/EducationContent.hpp"
#include "../ui/UiLayout.hpp"
#include "../ui/UiTheme.hpp"

namespace bag {

namespace {

constexpr std::array<const char*, 3> SCENARIOS = {"default_solar_system", "earth_orbit", "empty_space"};

void selectScreen(Simulation& simulation, AppScreen screen) {
    simulation.setScreen(screen);
}

// Contextual tabs are independent overlays: opening one closes the other
// analysis panels, while scene-only toggles (grid, moons, lighting) remain
// independent of the sidebar and of each other.
void closeContextualTabs(Simulation& simulation, bool keepVectors = false,
                         bool keepGravity = false, bool keepClassification = false,
                         bool keepSearch = false) {
    if (!keepVectors) {
        simulation.showVectors = false;
        simulation.settings.vectorsEnabled = false;
    }
    if (!keepGravity && simulation.gravityMode) {
        simulation.gravityMode = false;
        simulation.resetMassMultipliers();
    }
    if (!keepClassification) simulation.showClassification = false;
    if (!keepSearch) {
        simulation.searchActive = false;
        simulation.searchQuery.clear();
    }
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
        if (simulation.bodies[static_cast<std::size_t>(i)].active &&
            simulation.bodyVisibleInView(i)) {
            activeIndices.push_back(i);
        }
    }
    return simulation.selectBody(cycleActiveBody(simulation.selected, activeIndices, direction));
}

// Body letter-picking is a selection-mode affordance. It must not shadow the
// global display toggles, so it only claims letters that are otherwise free.
// V is deliberately absent: it is reserved for Forces & Motion. S is also
// absent, because Ctrl+S is the shortcut that turns selection mode on --
// listing it here meant that once selection mode was active, pressing V or S
// silently selected a body instead of toggling anything, which is exactly how
// those two shortcuts came to look broken.
bool letterSelectSimulationBody(Simulation& simulation, bool selectionMode) {
    if (simulation.screen != AppScreen::Simulation || !selectionMode) return false;

    struct KeyBody { KeyboardKey key; char initial; };
    constexpr KeyBody keys[] = {
        {KEY_B, 'b'}, {KEY_E, 'e'}, {KEY_J, 'j'}, {KEY_M, 'm'},
        {KEY_N, 'n'}, {KEY_U, 'u'},
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
        if (body.active && simulation.bodyVisibleInView(i) && !body.name.empty() &&
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

bool updateBodySearch(Simulation& simulation) {
    if (!simulation.searchActive) return false;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            simulation.searchActive = false;
            simulation.searchQuery.clear();
        } else if (!simulation.searchQuery.empty()) {
            simulation.searchQuery.pop_back();
        }
        return true;
    }
    if (IsKeyPressed(KEY_ENTER)) {
        simulation.searchActive = false;
        return true;
    }
    for (int code = GetCharPressed(); code > 0; code = GetCharPressed()) {
        if (code >= 32 && code <= 126 && simulation.searchQuery.size() < 32) {
            simulation.searchQuery.push_back(static_cast<char>(code));
        }
    }
    std::string query = simulation.searchQuery;
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (query.empty()) return true;
    int match = -1;
    std::vector<std::string> seenAsteroidSources;
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
        if (!body.active || (simulation.searchMoonScope && body.type != "Moon")) continue;
        if (body.type == "Asteroid") {
            if (std::find(seenAsteroidSources.begin(), seenAsteroidSources.end(),
                          body.visualAssetId) != seenAsteroidSources.end()) continue;
            seenAsteroidSources.push_back(body.visualAssetId);
        }
        std::string haystack = body.name + " " + body.id + " " + body.visualAssetId;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (haystack.find(query) != std::string::npos) {
            match = index;
            break;
        }
    }
    if (match >= 0) simulation.selectBody(match);
    return true;
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

void focusEarthMoonEducational(Simulation& simulation, Renderer& renderer) {
    // F2: dedicated Earth–Moon system framing (not the same as select Earth + Z).
    int earthIndex = -1;
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (simulation.bodies[static_cast<std::size_t>(index)].id == "earth" &&
            simulation.bodies[static_cast<std::size_t>(index)].active) {
            earthIndex = index;
            break;
        }
    }
    if (earthIndex < 0) {
        if (simulation.loadScenario("earth_orbit")) {
            for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                if (simulation.bodies[static_cast<std::size_t>(index)].id == "earth") {
                    earthIndex = index;
                    break;
                }
            }
        }
    }
    if (earthIndex < 0) {
        renderer.resetSystemView();
        return;
    }
    // Ensure sun is active for Earth-Moon view
    for (std::size_t index = 0; index < simulation.bodies.size(); ++index) {
        if (simulation.bodies[index].id == "sun") {
            simulation.bodies[index].active = true;
            break;
        }
    }
    if (!simulation.soloStudy && !simulation.earthMoonFocus) renderer.saveViewBeforeIsolate();
    simulation.selectBody(earthIndex);
    simulation.isolateSelected();
    simulation.earthMoonFocus = true;
    renderer.focusEarthMoonSystem(simulation, true);
}

void focusSelectedAlone(Simulation& simulation, Renderer& renderer) {
    if (simulation.selected < 0) return;
    if (!simulation.soloStudy) renderer.saveViewBeforeIsolate();
    simulation.earthMoonFocus = false;
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
    if (simulation.screen == AppScreen::Simulation && updateBodySearch(simulation)) return;
    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_S)) {
        closeContextualTabs(simulation);
        selectionMode = true;
        if (!simulation.bodies.empty()) simulation.selectBody(0);
        return;
    }
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_N)) {
        closeContextualTabs(simulation, false, false, true);
        simulation.showClassification = !simulation.showClassification;
        return;
    }
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_M)) {
        simulation.showMoons = !simulation.showMoons;
        simulation.showOrbits = false;
        return;
    }
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_L)) {
        if (renderer.villaFocusActive()) renderer.exitEarthVilla();
        else renderer.focusEarthVilla(simulation);
        return;
    }
    if (simulation.screen == AppScreen::Simulation && renderer.villaFocusActive()) {
        if (IsKeyPressed(KEY_F)) {
            renderer.focusEarthForest(simulation);
            return;
        }
        const int area =
            IsKeyPressed(KEY_ZERO) ? 0 :
            IsKeyPressed(KEY_ONE) ? 1 :
            IsKeyPressed(KEY_TWO) ? 2 :
            IsKeyPressed(KEY_THREE) ? 3 :
            IsKeyPressed(KEY_FOUR) ? 4 :
            IsKeyPressed(KEY_FIVE) ? 5 :
            IsKeyPressed(KEY_SIX) ? 6 :
            IsKeyPressed(KEY_SEVEN) ? 7 :
            IsKeyPressed(KEY_EIGHT) ? 8 :
            IsKeyPressed(KEY_NINE) ? 9 : -1;
        if (area >= 0) {
            renderer.selectVillaArea(area);
            return;
        }
    }
    // Ctrl+G toggles the reference grid. It is a display-only setting, so it is
    // deliberately not gated on the active screen: the grid reads the same way
    // on every page, and gating it made the shortcut appear to work or not work
    // depending on which tab happened to be open.
    if (controlDown && IsKeyPressed(KEY_G)) {
        if (simulation.screen == AppScreen::Simulation) closeContextualTabs(simulation);
        simulation.showGrid = !simulation.showGrid;
        return;
    }
    if (keyboardSelectSimulationBody(simulation, selectionMode)) return;
    if (letterSelectSimulationBody(simulation, selectionMode)) return;

    if (simulation.screen == AppScreen::Simulation) {
        Vector2 pickMouse{};
        // Clicks only select. Z owns isolate zoom; Q walks back.
        const bool focusClick = renderer.consumePendingFocusPick(pickMouse);
        const bool bodyClick = !focusClick && renderer.consumePendingBodyPick(pickMouse) &&
                               !renderer.isCameraManipulating();
        if (focusClick || bodyClick) {
            const int hit = renderer.hitTest(simulation, pickMouse);
            // F2: empty click / off-object click exits; hits stay on Sun/Earth/Moon only.
            if (simulation.earthMoonFocus) {
                if (hit < 0) {
                    simulation.exitIsolation();
                    renderer.restoreViewAfterIsolate();
                } else if (!simulation.showVectors) {
                    simulation.selectBody(hit);
                }
                return;
            }
            if (simulation.soloStudy) {
                // Leave isolate and restore the previous screen, then apply the click.
                simulation.exitIsolation();
                renderer.restoreViewAfterIsolate();
            }
            if (hit < 0) simulation.clearSelection();
            else if (!simulation.showVectors) simulation.selectBody(hit);
            else simulation.clearSelection();
            return;
        }
    }

    // TAB: Cycle edit field in G mode, otherwise cycle screens
    if (IsKeyPressed(KEY_TAB)) {
        if (simulation.gravityMode) {
            simulation.cycleGravityModeEditField();
        } else {
            cycleScreen(simulation);
        }
        return;
    }
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

    // Q loop: exit F2 / un-isolate (restore screen) → deselect → leave selection mode → turn off V mode
    if (IsKeyPressed(KEY_Q)) {
        if (simulation.screen == AppScreen::Simulation) {
            if (simulation.gravityMode) {
                simulation.gravityMode = false;
                simulation.resetMassMultipliers();
                return;
            }
            if (simulation.earthMoonFocus || simulation.soloStudy) {
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
            // Deactivate V mode vectors
            if (simulation.showVectors) {
                simulation.showVectors = false;
                simulation.settings.vectorsEnabled = false;
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
            const float leftX = ui::contentGutter();
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

    // Presentation state only: it turns on explanatory overlays without
    // changing masses, forces, integration, or the user's camera controls.
    if (IsKeyPressed(KEY_F10)) {
        simulation.cycleVisualizationMode();
        if (simulation.educationalVisualization() && simulation.selected >= 0) {
            renderer.frameGravityInteraction(simulation, simulation.selected, false);
        }
    }

    if (IsKeyPressed(KEY_SPACE)) simulation.paused = !simulation.paused;
    if (IsKeyPressed(KEY_A)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson - 1);
    if (IsKeyPressed(KEY_D)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson + 1);
    if (IsKeyPressed(KEY_E)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
    
    // In gravity mode, ↑/↓ adjust the selected edit field (mass or radius)
    if (simulation.gravityMode && simulation.selected >= 0) {
       if (IsKeyPressed(KEY_UP)) {
           if (simulation.gravityModeEditField == 0) {
               simulation.adjustSelectedMassMultiplier(1);
           } else if (simulation.gravityModeEditField == 1) {
               simulation.adjustSelectedRadius(1);
           }
           return;
       }
       if (IsKeyPressed(KEY_DOWN)) {
           if (simulation.gravityModeEditField == 0) {
               simulation.adjustSelectedMassMultiplier(-1);
           } else if (simulation.gravityModeEditField == 1) {
               simulation.adjustSelectedRadius(-1);
           }
           return;
       }
    } else if (!simulation.gravityMode) {
       if (IsKeyPressed(KEY_UP)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment - 1);
       if (IsKeyPressed(KEY_DOWN)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
       if (IsKeyPressed(KEY_TAB)) cycleScreen(simulation);
    }
    if (IsKeyPressed(KEY_ENTER)) simulation.startEducationActivity();
    if (IsKeyPressed(KEY_B)) simulation.beginEducationObservation();
    if (IsKeyPressed(KEY_Y)) simulation.evaluateCurrentExperiment();
    // KEY_Z with a selection zooms; without selection it steps challenges (handled above).
    if (IsKeyPressed(KEY_X)) simulation.selectEducationActivity(EducationActivityType::Challenge, simulation.challenge + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) simulation.adjustChallengeAnswer(-0.05);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) simulation.adjustChallengeAnswer(0.05);
    if (IsKeyPressed(KEY_I)) simulation.cycleChallengeIntegrator();
    // Plain N advances education; Ctrl+N toggles classification overlay (handled above with Ctrl).
    if (!controlDown && IsKeyPressed(KEY_N)) simulation.continueEducationActivity();
    if (IsKeyPressed(KEY_V)) {
        closeContextualTabs(simulation, true);
        simulation.showVectors = !simulation.showVectors;
        simulation.settings.vectorsEnabled = simulation.showVectors;
    }
    if (IsKeyPressed(KEY_O)) {
        simulation.showOrbits = !simulation.showOrbits;
        if (!simulation.showOrbits) simulation.showMoons = false;
        return;
    }
    if (IsKeyPressed(KEY_T)) {
        simulation.showTrails = !simulation.showTrails;
        simulation.settings.trailsEnabled = simulation.showTrails;
    }
    // G = Gravity Simulation mode (mass analysis). Grid moved to Ctrl+G.
    if (!controlDown && IsKeyPressed(KEY_G)) {
        closeContextualTabs(simulation, false, true);
        simulation.gravityMode = !simulation.gravityMode;
        if (simulation.gravityMode) {
            // Entering gravity mode: auto-select first active body if none selected
            if (simulation.selected < 0) {
                for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                    if (simulation.bodies[static_cast<std::size_t>(index)].active &&
                        simulation.bodies[static_cast<std::size_t>(index)].type != "Star") {
                        simulation.selectBody(index);
                        break;
                    }
                }
            }
            // Reset edit field to mass when entering
            simulation.gravityModeEditField = 0;
        } else {
            // Leaving gravity mode restores base masses so experiments stay reversible.
            simulation.resetMassMultipliers();
        }
    }
    if (IsKeyPressed(KEY_SLASH)) simulation.toggleVisualScaleMode();
    if (IsKeyPressed(KEY_R)) {
        if (simulation.gravityMode && simulation.selected >= 0) {
            // In gravity mode: reset selected body's mass to default
            simulation.resetSelectedMass();
        } else {
            // Normal mode: reset entire simulation
            simulation.reset();
            simulation.resetMassMultipliers();
        }
    }
    if (IsKeyPressed(KEY_F1)) {
        simulation.earthMoonFocus = false;
        if (simulation.loadScenario("default_solar_system")) applyScenarioCamera(simulation, renderer, "default_solar_system");
    }
    if (IsKeyPressed(KEY_F2)) {
        if (simulation.earthMoonFocus) {
            simulation.exitIsolation();
            renderer.restoreViewAfterIsolate();
        } else {
            focusEarthMoonEducational(simulation, renderer);
        }
    }
    if (IsKeyPressed(KEY_F3)) {
        simulation.earthMoonFocus = false;
        if (simulation.loadScenario("empty_space")) applyScenarioCamera(simulation, renderer, "empty_space");
    }
    if (IsKeyPressed(KEY_F4)) renderer.togglePlanetLighting();
    // KEY_ONE: Toggle planet group classification if Ctrl+N is active, else set speed
    if (IsKeyPressed(KEY_ONE)) {
        if (simulation.showMoons) {
            simulation.showOrbits = true;
        } else if (simulation.showClassification) {
            simulation.showPlanetGroups = !simulation.showPlanetGroups;
        } else {
            simulation.setSpeed(1.0);
        }
    }
    if (IsKeyPressed(KEY_TWO)) {
        closeContextualTabs(simulation, false, false, false, true);
        simulation.searchActive = true;
        simulation.searchQuery.clear();
        simulation.searchMoonScope = simulation.showMoons;
        return;
    }
    if (IsKeyPressed(KEY_THREE)) simulation.setSpeed(100.0);
    if (IsKeyPressed(KEY_FOUR)) simulation.setSpeed(1000.0);
    if (IsKeyPressed(KEY_FIVE)) simulation.setSpeed(100000.0);
    if (IsKeyPressed(KEY_SIX)) simulation.setSpeed(1000000.0);
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) simulation.adjustSpeed(1);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) simulation.adjustSpeed(-1);
    if (IsKeyPressed(KEY_P)) {
        if (simulation.launchProbe(3500.0)) {
            renderer.focusBodyIndex(simulation, simulation.selected, true);
        }
    }
}

} // namespace bag
