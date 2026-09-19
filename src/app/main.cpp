#include <raylib.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../core/Logger.hpp"
#include "../data/ResourceRoot.hpp"
#include "../input/InputController.hpp"
#include "../rendering/Renderer.hpp"
#include "../simulation/Simulation.hpp"
#include "../ui/HUD.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool startupTraceEnabled() {
    const char* value = std::getenv("BAGS_LAB_STARTUP_TRACE");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

} // namespace

int main() {
    using namespace bag;

    const bool trace = startupTraceEnabled();
    const auto appStart = Clock::now();

    const auto resources = ResourceRoot::resolve();
    if (!resources) {
        logError(resources.error);
        return 1;
    }
    if (trace) logInfo("[STARTUP] Resource root resolved (" + std::to_string(msSince(appStart)) + " ms)");

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    const auto windowStart = Clock::now();
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
    if (trace) logInfo("[STARTUP] Window initialized (" + std::to_string(msSince(windowStart)) + " ms)");
    SetTargetFPS(60);

    const auto simStart = Clock::now();
    Simulation simulation(resources.dataRoot);
    if (trace) {
        logInfo("[STARTUP] Simulation initialized (" + std::to_string(msSince(simStart)) + " ms, stars=" +
                std::to_string(simulation.stars.size()) + ", bodies=" + std::to_string(simulation.bodies.size()) + ")");
    }

    const auto rendererStart = Clock::now();
    Renderer renderer(resources.planetAssetsRoot());
    if (trace) logInfo("[STARTUP] Renderer/assets initialized (" + std::to_string(msSince(rendererStart)) + " ms)");

    HUD hud;
    InputController input;
    if (trace) logInfo("[STARTUP] Entering main loop (" + std::to_string(msSince(appStart)) + " ms total)");

    int frameIndex = 0;
    while (!WindowShouldClose()) {
        const float deltaSeconds = GetFrameTime();
        const auto frameStart = Clock::now();

        input.update(simulation, renderer);
        const auto afterInput = Clock::now();
        if (!simulation.paused) simulation.integrate(deltaSeconds);
        const auto afterIntegrate = Clock::now();

        BeginDrawing();
        if (trace && frameIndex == 0) logInfo("[FRAME] first BeginDrawing");
        renderer.background(simulation);
        renderer.scene(simulation);
        const auto afterScene = Clock::now();
        hud.draw(simulation, renderer.getYaw());
        EndDrawing();
        const auto afterEnd = Clock::now();

        if (trace && frameIndex < 3) {
            logInfo("[FRAME] #" + std::to_string(frameIndex) +
                    " input=" + std::to_string(std::chrono::duration<double, std::milli>(afterInput - frameStart).count()) +
                    " ms integrate=" +
                    std::to_string(std::chrono::duration<double, std::milli>(afterIntegrate - afterInput).count()) +
                    " ms scene=" +
                    std::to_string(std::chrono::duration<double, std::milli>(afterScene - afterIntegrate).count()) +
                    " ms hud+swap=" +
                    std::to_string(std::chrono::duration<double, std::milli>(afterEnd - afterScene).count()) +
                    " ms total=" + std::to_string(msSince(frameStart)) + " ms");
            std::clog.flush();
        }
        ++frameIndex;
        // Auto-exit after a few traced frames so CI/smoke can capture timings.
        if (trace && frameIndex >= 3) {
            logInfo("[STARTUP] Trace complete — exiting after 3 frames");
            break;
        }
    }

    renderer.unloadAssets();
    CloseWindow();
    logInfo("BAGS_LAB shut down");
    return 0;
}
