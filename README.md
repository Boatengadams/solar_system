# BAGSOLAR

Interactive C++17 [raylib](https://www.raylib.com/) astrodynamics laboratory.

The application is in the prototype-to-engineering transition. The original
BAGSOLAR behavior is preserved while the code is organized into raylib-free
physics/simulation modules and raylib-facing rendering, UI, and input modules.

Project specifications and the Phase 0/1 implementation plan are in
`BAGSOLAR_codex_docs/`.

## Build and run

Install the system dependency once by using the following command in your terminal :

```sh
sudo apt update
sudo apt install g++ cmake pkg-config libraylib-dev
```

to configure this and build follow this process:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the regression tests with:

```sh
ctest --test-dir build --output-on-failure
```

If you do not have CMake, the equivalent is:

```sh
make
```

Run it with:

```sh
./build/planets
```

For the Makefile build, run `./planets` instead.

## Source layout

```text
src/app          Application composition and main loop
src/core         Raylib-free value types
src/physics      Bodies and gravitational calculations
src/simulation   Simulation state and clock
src/data         Built-in baseline scenario loader
src/education    Lessons and experiments
src/spacecraft   Spacecraft propulsion and maneuver domain
src/missions     Mission analysis and trajectory prediction
src/astronomy    Ephemeris and optional advanced-data providers
src/rendering    Raylib scene rendering
src/ui           Raylib HUD
src/input        Keyboard and mouse mapping
tests            Dependency-free physics regression tests
```

## Data-driven scenarios

Celestial body definitions are stored in `data/bodies/` and scenario files in
`data/scenarios/`. The application starts with `default_solar_system.json`.
Use `F1` for the default solar system, `F2` for Earth Orbit, and `F3` for Empty
Space. Reset reloads the currently selected scenario from its JSON source.

Phase 2 also provides `Simulation::saveSnapshot()` and
`Simulation::loadSnapshot()` as the foundation for future save/load UI.

In VS Code, run **Tasks: Run Build Task** after configuring, or use the included
debug launch configuration.

Release/education tools:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
cmake --build build --target package
```

SPICE is not bundled. The optional provider boundary reports unavailable until
CSPICE and compatible kernels are supplied. Enable it with
`-DBAGSOLAR_ENABLE_SPICE=ON`, `CSPICE_INCLUDE_DIR`, and `CSPICE_LIBRARY`; use
`bagsolar_ephemeris --spice MANIFEST BODY JULIAN_DATE FRAME` for an explicit
query.
