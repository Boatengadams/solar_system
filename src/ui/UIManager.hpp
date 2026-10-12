#pragma once

#include <raylib.h>

#include "UiLayout.hpp"
#include "../simulation/Simulation.hpp"

namespace bag {

class Renderer;

// Owns the application shell: the top bar, the navigation rail, the screen tab
// strip, and the simulation control card.
//
// Two invariants make the shell's input behaviour predictable, and both were
// violated before this class was reworked:
//
//  1. ChromeLayout is rebuilt exactly once per frame, in update(), before the
//     renderer reads the pointer and before anything draws. Drawing and
//     hit-testing therefore read identical rectangles.
//  2. Chrome is never hidden in a way that disables input. The previous
//     auto-hide timer removed the buttons from the screen *and* from the
//     pointer-ownership test, so a click landing on a vanished button was
//     routed to the 3D camera instead -- which is why a nearby button only
//     worked after the user had clicked somewhere else first.
class UIManager {
public:
    void initialize();
    void shutdown();
    void beginFrame();
    void endFrame();

    // Advances chrome animation and rebuilds the frame layout. Must be the
    // first per-frame call, ahead of InputController and Renderer, so the
    // renderer resolves the pointer against the same rectangles that will be
    // drawn this frame.
    void update(const Simulation& simulation);

    // Non-const because the rail disclosure toggle and the screen tab strip are
    // genuine user actions that mutate shell state from within the draw pass --
    // raym3 components report activation by return value while drawing.
    void draw(Simulation& simulation, Renderer& renderer);

    // True when the pointer is over a shell-owned region, so the 3D camera must
    // not begin a gesture. Reads the layout built this frame.
    bool pointerOverUi() const;
    // Same question for the press currently being resolved.
    bool pointerPressStartedOverUi() const;

    const ui::ChromeLayout& layout() const { return layout_; }

private:
    void drawTopBar(Simulation& simulation, Renderer& renderer);
    void drawRail(Simulation& simulation);
    void drawSimulationCard(Simulation& simulation, Renderer& renderer);
    // Apple-style toggle whose whole row is the target. See UiLayout.hpp for why
    // this exists instead of raym3's SwitchComponent.
    bool drawToggle(Rectangle row, const char* label, bool* value);

    bool initialized_ = false;
    bool sidebarOpen_ = false;
    // Presentation-only animation state. The target stays authoritative; the
    // current value is allowed to catch up over several frames. It is advanced
    // in exactly one place (update) so the drawn and hit-tested widths agree.
    float sidebarProgress_ = 0.0f;
    ui::ChromeLayout layout_{};
};

} // namespace bag
