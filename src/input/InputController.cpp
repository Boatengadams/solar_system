#include "InputController.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "../education/EducationContent.hpp"

namespace bag {

namespace {

constexpr std::array<const char*, 3> SCENARIOS = {"default_solar_system", "earth_orbit", "empty_space"};

void selectScreen(Simulation& simulation, AppScreen screen) {
    simulation.setScreen(screen);
}

bool navigationClick(Simulation& simulation) {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetMousePosition().y > 70.0f) return false;
    const float x = GetMousePosition().x;
    if (x >= 290.0f && x < 385.0f) selectScreen(simulation, AppScreen::Simulation);
    else if (x >= 385.0f && x < 480.0f) selectScreen(simulation, AppScreen::Education);
    else if (x >= 480.0f && x < 585.0f) selectScreen(simulation, AppScreen::ScenarioBrowser);
    else if (x >= 585.0f && x < 690.0f) selectScreen(simulation, AppScreen::Telemetry);
    else if (x >= 690.0f && x < 790.0f) selectScreen(simulation, AppScreen::MissionDesigner);
    else if (x >= 790.0f && x < 895.0f) selectScreen(simulation, AppScreen::Settings);
    else if (x >= 895.0f && x < 975.0f) selectScreen(simulation, AppScreen::Help);
    else return false;
    return true;
}

void cycleScreen(Simulation& simulation) {
    const int next = (static_cast<int>(simulation.screen) + 1) % 7;
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
    simulation.selected = cycleActiveBody(simulation.selected, activeIndices, direction);
    return true;
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

    // Repeated initials cycle through duplicates, e.g. M: Mercury -> Mars.
    const auto current = std::find(matches.begin(), matches.end(), simulation.selected);
    const int next = current == matches.end()
        ? matches.front()
        : matches[static_cast<std::size_t>((std::distance(matches.begin(), current) + 1) % matches.size())];
    simulation.selected = next;
    return true;
}

} // namespace

void InputController::update(Simulation& simulation, Renderer& renderer) {
    if (simulation.screen == AppScreen::Simulation) renderer.update(simulation);
    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (simulation.screen == AppScreen::Simulation && controlDown && IsKeyPressed(KEY_S)) {
        selectionMode = true;
        simulation.selected = simulation.bodies.empty() ? -1 : 0;
        return;
    }
    if (keyboardSelectSimulationBody(simulation, selectionMode)) return;
    if (letterSelectSimulationBody(simulation, selectionMode)) return;
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
        selectionMode = false;
        simulation.selected = -1;
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
}

} // namespace bag
