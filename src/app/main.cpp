#include <raylib.h>

#include "../core/Logger.hpp"
#include "../data/ResourceRoot.hpp"
#include "../input/InputController.hpp"
#include "../rendering/Renderer.hpp"
#include "../simulation/Simulation.hpp"
#include "../ui/HUD.hpp"

int main() {
    using namespace bag;

    const auto resources = ResourceRoot::resolve();
    if (!resources) {
        logError(resources.error);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(Renderer::SCREEN_W, Renderer::SCREEN_H, "BAGS_LAB");
    if (!IsWindowReady()) {
        logError("Unable to initialize the raylib window");
        return 1;
    }
    SetWindowMinSize(1100, 700);
    // Escape is owned by InputController (clear selection / return to Simulation).
    // Disable raylib's default "Escape closes window" binding.
    SetExitKey(KEY_NULL);
    logInfo("BAGS_LAB initialized");
    SetTargetFPS(60);

    Simulation simulation(resources.dataRoot);
    Renderer renderer(resources.planetAssetsRoot());
    HUD hud;
    InputController input;

    while (!WindowShouldClose()) {
        const float deltaSeconds = GetFrameTime();
        input.update(simulation, renderer);
        if (!simulation.paused) simulation.integrate(deltaSeconds);

        BeginDrawing();
        renderer.background(simulation);
        renderer.scene(simulation);
        hud.draw(simulation);
        EndDrawing();
    }

    renderer.unloadAssets();
    CloseWindow();
    logInfo("BAGS_LAB shut down");
    return 0;
}
