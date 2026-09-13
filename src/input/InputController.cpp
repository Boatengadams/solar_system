#include "InputController.hpp"

#include <algorithm>

#include "../education/EducationContent.hpp"

namespace bag {

void InputController::update(Simulation& simulation, Renderer& renderer) const {
    renderer.update(simulation);
    if (IsKeyPressed(KEY_SPACE)) simulation.paused = !simulation.paused;
    if (IsKeyPressed(KEY_L)) simulation.education = !simulation.education;
    if (IsKeyPressed(KEY_A)) simulation.lesson = (simulation.lesson + lessonCount() - 1) % lessonCount();
    if (IsKeyPressed(KEY_D)) simulation.lesson = (simulation.lesson + 1) % lessonCount();
    if (IsKeyPressed(KEY_E)) simulation.experiment = (simulation.experiment + 1) % experimentCount();
    if (IsKeyPressed(KEY_UP)) simulation.experiment = (simulation.experiment + experimentCount() - 1) % experimentCount();
    if (IsKeyPressed(KEY_DOWN)) simulation.experiment = (simulation.experiment + 1) % experimentCount();
    if (IsKeyPressed(KEY_Z)) simulation.setChallenge(simulation.challenge - 1);
    if (IsKeyPressed(KEY_X)) simulation.setChallenge(simulation.challenge + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) simulation.adjustChallengeAnswer(-0.05);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) simulation.adjustChallengeAnswer(0.05);
    if (IsKeyPressed(KEY_I)) simulation.cycleChallengeIntegrator();
    if (IsKeyPressed(KEY_C)) simulation.submitChallenge();
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
