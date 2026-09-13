#include <raylib.h>

#include "../core/Logger.hpp"
#include "../input/InputController.hpp"
#include "../rendering/Renderer.hpp"
#include "../simulation/Simulation.hpp"
#include "../ui/HUD.hpp"

int main() {
    using namespace bag;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(Renderer::SCREEN_W, Renderer::SCREEN_H, "BAGSOLAR — Astronomical Laboratory");
    if (!IsWindowReady()) {
        logError("Unable to initialize the raylib window");
        return 1;
    }
    logInfo("BAGSOLAR initialized");
    SetTargetFPS(60);

    Simulation simulation;
    Renderer renderer;
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

    CloseWindow();
    logInfo("BAGSOLAR shut down");
    return 0;
}
