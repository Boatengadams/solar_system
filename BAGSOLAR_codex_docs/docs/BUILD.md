# Build Specification

## Requirements
- C++17 or newer as intentionally selected by the project
- CMake
- raylib
- test framework
- optional JSON library
- optional UI/plotting dependencies

Phase 2 adds the local nlohmann/json single-header package under
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

## CI
CI should perform:
1. Configure
2. Build
3. Unit tests
4. Warning checks where practical
5. Packaging smoke test
