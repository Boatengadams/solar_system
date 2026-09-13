#include "InputController.hpp"

#include <algorithm>
#include <array>

#include "../education/EducationContent.hpp"

namespace bag {

namespace {

constexpr std::array<const char*, 3> SCENARIOS = {"default_solar_system", "earth_orbit", "empty_space"};

void selectScreen(Simulation& simulation, AppScreen screen) {
    simulation.setScreen(screen);
}

bool navigationClick(Simulation& simulation) {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetMousePosition().y > 58.0f) return false;
    const float x = GetMousePosition().x;
    if (x >= 280.0f && x < 370.0f) selectScreen(simulation, AppScreen::Simulation);
    else if (x >= 370.0f && x < 460.0f) selectScreen(simulation, AppScreen::Education);
    else if (x >= 460.0f && x < 570.0f) selectScreen(simulation, AppScreen::ScenarioBrowser);
    else if (x >= 570.0f && x < 680.0f) selectScreen(simulation, AppScreen::Telemetry);
    else if (x >= 680.0f && x < 820.0f) selectScreen(simulation, AppScreen::MissionDesigner);
    else if (x >= 820.0f && x < 930.0f) selectScreen(simulation, AppScreen::Settings);
    else if (x >= 930.0f && x < 1030.0f) selectScreen(simulation, AppScreen::Help);
    else return false;
    return true;
}

void cycleScreen(Simulation& simulation) {
    const int next = (static_cast<int>(simulation.screen) + 1) % 7;
    simulation.setScreen(static_cast<AppScreen>(next));
}

} // namespace

void InputController::update(Simulation& simulation, Renderer& renderer) const {
    if (simulation.screen == AppScreen::Simulation) renderer.update(simulation);
    if (navigationClick(simulation)) return;
    if (IsKeyPressed(KEY_TAB)) cycleScreen(simulation);
    if (IsKeyPressed(KEY_H)) selectScreen(simulation, AppScreen::Help);
    if (IsKeyPressed(KEY_L)) {
        selectScreen(simulation, simulation.screen == AppScreen::Education ? AppScreen::Simulation : AppScreen::Education);
    }
    if (IsKeyPressed(KEY_F5)) selectScreen(simulation, AppScreen::ScenarioBrowser);
    if (IsKeyPressed(KEY_F6)) selectScreen(simulation, AppScreen::Telemetry);
    if (IsKeyPressed(KEY_F7)) selectScreen(simulation, AppScreen::MissionDesigner);
    if (IsKeyPressed(KEY_F8)) selectScreen(simulation, AppScreen::Settings);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
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

    if (simulation.screen == AppScreen::ScenarioBrowser) {
        if (IsKeyPressed(KEY_UP)) simulation.scenarioBrowserSelection =
            (simulation.scenarioBrowserSelection + static_cast<int>(SCENARIOS.size()) - 1) % static_cast<int>(SCENARIOS.size());
        if (IsKeyPressed(KEY_DOWN)) simulation.scenarioBrowserSelection =
            (simulation.scenarioBrowserSelection + 1) % static_cast<int>(SCENARIOS.size());
        if (IsKeyPressed(KEY_ENTER)) {
            if (simulation.loadScenario(SCENARIOS[static_cast<std::size_t>(simulation.scenarioBrowserSelection)]))
                selectScreen(simulation, AppScreen::Simulation);
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
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) simulation.adjustTimestep(2.0);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) simulation.adjustTimestep(0.5);
        return;
    }

    if (simulation.screen == AppScreen::MissionDesigner || simulation.screen == AppScreen::Help) return;

    if (IsKeyPressed(KEY_SPACE)) simulation.paused = !simulation.paused;
    if (IsKeyPressed(KEY_A)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson - 1);
    if (IsKeyPressed(KEY_D)) simulation.selectEducationActivity(EducationActivityType::Lesson, simulation.lesson + 1);
    if (IsKeyPressed(KEY_E)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
    if (IsKeyPressed(KEY_UP)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment - 1);
    if (IsKeyPressed(KEY_DOWN)) simulation.selectEducationActivity(EducationActivityType::Experiment, simulation.experiment + 1);
    if (IsKeyPressed(KEY_ENTER)) simulation.startEducationActivity();
    if (IsKeyPressed(KEY_B)) simulation.beginEducationObservation();
    if (IsKeyPressed(KEY_Y)) simulation.evaluateCurrentExperiment();
    if (IsKeyPressed(KEY_Z)) simulation.selectEducationActivity(EducationActivityType::Challenge, simulation.challenge - 1);
    if (IsKeyPressed(KEY_X)) simulation.selectEducationActivity(EducationActivityType::Challenge, simulation.challenge + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) simulation.adjustChallengeAnswer(-0.05);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) simulation.adjustChallengeAnswer(0.05);
    if (IsKeyPressed(KEY_I)) simulation.cycleChallengeIntegrator();
    if (IsKeyPressed(KEY_C)) simulation.submitChallenge();
    if (IsKeyPressed(KEY_N)) simulation.continueEducationActivity();
    if (IsKeyPressed(KEY_Q)) simulation.retryEducationActivity();
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
    if (IsKeyPressed(KEY_F1)) simulation.loadScenario("default_solar_system");
    if (IsKeyPressed(KEY_F2)) simulation.loadScenario("earth_orbit");
    if (IsKeyPressed(KEY_F3)) simulation.loadScenario("empty_space");
    if (IsKeyPressed(KEY_ONE)) simulation.setSpeed(1.0);
    if (IsKeyPressed(KEY_TWO)) simulation.setSpeed(10.0);
    if (IsKeyPressed(KEY_THREE)) simulation.setSpeed(100.0);
    if (IsKeyPressed(KEY_FOUR)) simulation.setSpeed(1000.0);
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) simulation.setSpeed(simulation.speed * 2.0);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) simulation.setSpeed(simulation.speed / 2.0);
    if (IsKeyPressed(KEY_P) && simulation.bodies.size() > 9) simulation.launchProbe(11000.0);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        const int selected = renderer.hitTest(simulation, GetMousePosition());
        if (selected >= 0) simulation.selected = selected;
    }
}

} // namespace bag
