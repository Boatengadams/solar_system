# BAGSOLAR Phase 0 and Phase 1 Implementation Plan

## Baseline assessment

The existing application is a working C++17/raylib prototype in `project.cpp`.
It combines the simulation model, gravitational integration, scenario setup,
rendering, HUD, input handling, lessons, experiments, and `main()` in one
translation unit. The current build links raylib through CMake/pkg-config and
the Makefile. There is no test target, no `src/` tree, no data loader, and no
repository metadata available for a baseline tag.

The prototype already contains the baseline behavior required by the project
specification: the Sun, eight planets, BAGSOLAR-1, Newtonian N-body gravity,
velocity-Verlet stepping, camera controls, selection, telemetry, lessons, and
experiments.

## Phase 0 — baseline and reproducibility

1. Record the current product behavior and build requirements.
2. Preserve the existing raylib-only dependency model.
3. Establish a CMake build with CTest enabled.
4. Add a small dependency-free physics regression executable.
5. Remove existing compiler warnings without changing simulation behavior.
6. Document the absence of Git metadata; create a repository tag only if Git is
   introduced by the project owner.

### Phase 0 Definition of Done

- The application builds in Debug mode with CMake.
- A test command exists and passes.
- Existing warnings are removed or explicitly explained.
- The original controls and baseline scenario remain available.
- Build and testing instructions are documented.

Status: complete. CMake/CTest and Makefile builds are reproducible in this
workspace, the baseline physics regression test passes, and both build paths
are warning-free. A Git tag remains unavailable because this directory is not
a Git repository.

## Phase 1 — modular foundation

Refactor the prototype into small modules while keeping behavior equivalent:

```text
src/
  app/main.cpp
  core/Vector2.hpp, Vector3.hpp, Color.hpp
  physics/Body.hpp, PhysicsEngine.hpp/.cpp
  simulation/Simulation.hpp/.cpp
  data/ScenarioLoader.hpp/.cpp
  education/EducationContent.hpp/.cpp
  rendering/Renderer.hpp/.cpp
  ui/HUD.hpp/.cpp
  input/InputController.hpp/.cpp
tests/physics_tests.cpp
```

Responsibilities:

- `core`: raylib-free value types shared by domain systems.
- `physics`: body state, gravitational acceleration, integration, and pure
  telemetry calculations.
- `simulation`: clock/state management, reset, default scenario lifecycle, and
  spacecraft launch behavior.
- `data`: the Phase 1 built-in baseline scenario adapter. External JSON belongs
  to Phase 2.
- `education`: lesson and experiment content, independent of rendering.
- `rendering`: raylib camera and scene drawing; it reads simulation state.
- `ui`: raylib HUD drawing only.
- `input`: raylib input mapping that mutates simulation/UI state through the
  public simulation interface.
- `app`: application composition and the main loop.

### Phase 1 Definition of Done

- `project.cpp` responsibilities are split into the planned modules.
- Physics/domain headers do not include raylib.
- Rendering reads domain state and does not own physics updates.
- The default scenario, controls, lessons, experiments, telemetry, and probe
  behavior remain available.
- CMake builds the modular application and runs the regression tests.
- No unexplained compiler warnings remain.
- Architecture and build documentation reflect the implemented structure.

## Risks and architectural conflicts

1. **No Git repository:** the Phase 0 tag cannot be created here. This is
   documented rather than silently pretending a tag exists.
2. **Raylib colors and vectors in domain state:** replacing them with small
   raylib-free value types is required to keep physics independently testable.
3. **Hardcoded scenario data:** a built-in scenario loader preserves behavior in
   Phase 1; JSON loading is intentionally deferred to Phase 2.
4. **Single-file behavior coupling:** extracting responsibilities can expose
   hidden ordering assumptions in the main loop. The loop order will remain
   input → simulation → render.
5. **No test framework:** a small CTest executable using standard C++ avoids a
   new dependency. A framework can be evaluated later if test volume warrants
   it.
6. **Numerical behavior:** the existing fixed one-hour solver step and constants
   remain unchanged during Phase 1. Integrator expansion belongs to Phase 3.

## Verification sequence

After each meaningful extraction:

1. Configure/build with CMake.
2. Run CTest and the physics regression executable.
3. Treat new warnings as failures to fix before continuing.
4. Update the relevant build/architecture documentation.

## Phase 1 completion record

Phase 1 is complete. The application now has separate core, physics,
simulation, data, education, rendering, UI, input, and app modules; a
dependency-free logger; and a CTest physics regression target. The old
single-file `project.cpp` entry point was removed after both build systems were
verified against `src/`.

Verification:

```text
cmake --build build --parallel              PASS
ctest --test-dir build --output-on-failure  PASS (1/1)
make clean && make                         PASS
compiler warnings                           NONE
physics headers including raylib             NONE
```

## Phase 2 completion record

Phase 2 is complete without beginning Phase 3. Body definitions and scenarios
are external JSON files under `data/`; validated loading and construction live
in `src/data/`; scenario selection and reset are wired into the application;
snapshot save/load and custom-body construction foundations are available; and
unit conversions are centralized in `src/core/Units.hpp`.

Additional verification:

```text
bagsolar_physics_tests  PASS
bagsolar_data_tests     PASS
CTest total             PASS (2/2)
CMake build             PASS
Makefile build          PASS
compiler warnings       NONE
```
