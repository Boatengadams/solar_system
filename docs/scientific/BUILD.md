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

The CMake build defines the BAGS_LAB application and retains the historical
`planets` executable target for compatibility. It also defines CTest targets,
including `bagsolar_physics_tests`. Run them with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The Makefile remains available as a second Linux application build path and
compiles the same `src/` sources. Its `test` target invokes the CMake Debug
preset, builds that CTest tree, and then runs CTest; it is not a separate test
implementation.

The application requires this runtime data tree:

```text
data/bodies/*.json
data/scenarios/*.json
```

For a source-tree build, the resolver finds `data/` beside the source checkout
when the executable is under `build/` or at the repository root. For an
installed build, it finds `${prefix}/share/bags_lab/data` relative to the
installed executable in `${prefix}/bin` (legacy `${prefix}/share/bagsolar/data`
is still accepted), independent of the caller's working directory. Missing
required resources produce an explicit initialization failure.

Phase 6 also builds `bagsolar_ephemeris`. It has no mandatory HTTP library;
the explicit Horizons transport invokes the local `curl` executable when the
`--horizons` command is selected. Offline local/JSON operation and all normal
tests work without network access.

Phase 8 SPICE support is deliberately optional and reports an explicit
unavailable status unless CSPICE is supplied through `CSPICE_INCLUDE_DIR` and
`CSPICE_LIBRARY`. The official NAIF static archive `cspice.a` is supported.
The current build provides `CMakePresets.json`, Linux GitHub Actions, install
rules, and CPack TGZ packaging:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
cmake --build build --target package
```

For a release-style offline smoke run from a checkout, use the deterministic
helper from any working directory:

```sh
bash tools/release_smoke.sh
```

It archives the current committed checkout into a temporary directory, configures and
builds with the existing warning flags, runs CTest plus the scientific,
education, and local-ephemeris executables, then validates an install and an
extracted TGZ package outside the source tree. It uses no network access,
CSPICE installation, or external kernels.

The individual validation executables are also available after a build:

```sh
./build/bagsolar_validation
./build/bagsolar_education_tests
./build/bagsolar_ephemeris --local earth 2451545.0 heliocentric
```

Install and validate from outside the source tree:

```sh
cmake --install build --prefix /tmp/bags-lab-install
(cd /tmp && /tmp/bags-lab-install/bin/bagsolar_resource_smoke)
```

The CTest targets `bagsolar_resource_smoke`, `bagsolar_install_smoke`, and
`bagsolar_package_smoke` exercise source-tree resolution, a temporary install,
and an extracted TGZ package without requiring a display or network access.

The development build uses `build/` and source-tree data discovery. The
installed build uses `bin/` plus `${prefix}/share/bags_lab/data` (legacy
`share/bagsolar/data` still accepted); run it from an unrelated directory with
`bagsolar_resource_smoke`. The packaged build has the same relocatable layout
beneath its extracted package directory.

To request the optional path, configure a separate build directory. CMake
fails immediately if the requested CSPICE header or library is missing:

```sh
cmake -S . -B build-spice -DBAGSOLAR_ENABLE_SPICE=ON \
  -DCSPICE_INCLUDE_DIR=/opt/cspice/include \
  -DCSPICE_LIBRARY=/opt/cspice/lib/cspice.a
```

CSPICE and kernel files are external resources and are never downloaded or
committed by BAGS_LAB.

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
