# Build Specification

## Requirements
- C++17 or newer as intentionally selected by the project
- CMake
- raylib
- CTest (provided by CMake) for the regression suite
- the bundled nlohmann/json headers for JSON data and persistence

Phase 2 adds the local nlohmann/json package under
`third_party/nlohmann-json3-dev/usr/include`. The package is MIT licensed and
is used only by the data layer for JSON parsing and serialization.

## Build Principles
- Support Linux first.
- Add Windows and macOS CI/builds.
- Keep dependencies documented.
- Prefer reproducible builds.
- Keep Debug and Release configurations.

The Phase 1 CMake build defines the `planets` application and a CTest target
named `bagsolar_physics_tests`. Run both with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The Makefile remains available as a second Linux build path and compiles the
same `src/` sources.

The application expects the external data tree relative to the working
directory:

```text
data/bodies/*.json
data/scenarios/*.json
```

Run from the project root so the default `data` path resolves correctly.

Phase 6 also builds `bagsolar_ephemeris`. It has no mandatory HTTP library;
the explicit Horizons transport invokes the local `curl` executable when the
`--horizons` command is selected. Offline local/JSON operation and all normal
tests work without network access.

Phase 8 SPICE support is deliberately optional and reports an explicit
unavailable status unless CSPICE is supplied through `CSPICE_INCLUDE_DIR` and
`CSPICE_LIBRARY`. The official NAIF static archive `cspice.a` is supported.
Phase 10 provides `CMakePresets.json`, Linux GitHub Actions, install rules, and
CPack TGZ packaging:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
cmake --build build --target package
```

To request the optional path, configure a separate build directory. CMake
fails immediately if the requested CSPICE header or library is missing:

```sh
cmake -S . -B build-spice -DBAGSOLAR_ENABLE_SPICE=ON \
  -DCSPICE_INCLUDE_DIR=/opt/cspice/include \
  -DCSPICE_LIBRARY=/opt/cspice/lib/cspice.a
```

CSPICE and kernel files are external resources and are never downloaded or
committed by BAGSOLAR.

With a valid local manifest, run the real integration test with:

```sh
export BAGSOLAR_SPICE_TEST_MANIFEST=/path/to/spice-manifest.json
ctest --test-dir build-spice -R bagsolar_spice_live_tests --output-on-failure
```

## CI
CI should perform:
1. Configure
2. Build
3. Unit tests
4. Warning checks where practical
5. Packaging smoke test
