# Build Specification

Canonical build and packaging instructions for **BAGS_LAB**. Detailed scientific
notes also live under `docs/scientific/BUILD.md`. Multi-platform packaging
architecture: `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.

## Requirements

- C++17
- CMake ≥ 3.16 (portable static raylib builds need ≥ 3.25)
- Developer Linux/macOS builds: pkg-config + system raylib **or** `-DBAGS_LAB_BUNDLE_RAYLIB=ON`
- Windows release path: `-DBAGS_LAB_BUNDLE_RAYLIB=ON` (MSVC preset `windows-x64`)
- CTest (via CMake)
- Bundled nlohmann/json headers under `third_party/nlohmann-json3-dev/`

## Portable Linux package (VERIFIED: linux-x64)

```sh
./tools/package_portable.sh linux-x64
./dist/linux-x64/BAGS_LAB.sh
```

The verified portable package statically links raylib 6.0 and raylib’s bundled
GLFW (no `libraylib.so`, no `libtiff`, no `libglfw.so.3`). Host X11/OpenGL
(and glibc/libstdc++) stacks are still expected on typical Linux desktops.

Other `dist/*` IDs exist as stubs until native builds are verified. The packager
refuses to fabricate binaries.

## Quick start

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/BAGS_LAB
```

Presets:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug --output-on-failure

cmake --preset linux-x64    # portable Linux x64 (native Linux host)
cmake --build --preset linux-x64
```

Makefile targets (`BAGS_LAB`, `run`, `test`, `package`, `validation`, …) remain
available as a Linux convenience path.

## Resource layout

Required runtime data:

```text
data/bodies/*.json
data/scenarios/*.json
```

`ResourceRoot` resolves data relative to the executable:

| Layout | Example |
| --- | --- |
| Source / portable | `<root>/BAGS_LAB` + `<root>/data` |
| macOS `.app` | `Contents/MacOS/BAGS_LAB` → `Contents/Resources/data` |
| Install prefix | `<prefix>/bin/BAGS_LAB` + `<prefix>/share/bags_lab/data` |
| Legacy install | `<prefix>/share/bagsolar/data` (still accepted) |
| VFAT override | `BAGS_LAB_RESOURCE_ROOT` → package root or `data/` |

Assets resolve as `<projectRoot>/assets` (planet models under `assets/planets`).
Missing required data produces an explicit startup failure.

## Windows / macOS / Linux ARM64

| Target | Status | Produce on |
| --- | --- | --- |
| `linux-x64` | **VERIFIED** | Linux x86_64 |
| `linux-arm64` | BUILD-READY / NOT YET TESTED | Linux aarch64 |
| `windows-x64` | PLANNED / NOT YET TESTED | Windows + MSVC |
| `macos-x64` | PLANNED / NOT YET TESTED | macOS host |
| `macos-arm64` | PLANNED / NOT YET TESTED | macOS host |

See `platform/*/README.md`. Do not claim a platform “runs” until native
runtime acceptance exists.

## Optional CSPICE

```sh
cmake -S . -B build-spice \
  -DBAGSOLAR_ENABLE_SPICE=ON \
  -DCSPICE_INCLUDE_DIR=/path/to/cspice/include \
  -DCSPICE_LIBRARY=/path/to/cspice/lib/cspice.a
```

CSPICE/kernels are external and never downloaded or committed by BAGS_LAB. When
SPICE is off, the laboratory still runs with local/JSON providers and does not
claim SPICE precision. Developer paths such as `/home/<user>/spice` must never
become hidden runtime requirements.

Live SPICE integration tests (optional):

```sh
export BAGSOLAR_SPICE_TEST_MANIFEST=/path/to/spice-manifest.json
ctest --test-dir build-spice -R bagsolar_spice_live_tests --output-on-failure
```

## Release smoke

```sh
bash tools/release_smoke.sh
```

Archives the committed checkout into a temporary directory, builds with warning
flags, runs CTest plus scientific/education/local-ephemeris checks, then
validates install and TGZ package layouts without network or CSPICE.

## Validation executables

```sh
./build/bagsolar_validation
./build/bagsolar_education_tests
./build/bagsolar_ephemeris --local earth 2451545.0 heliocentric
```

Install smoke:

```sh
cmake --install build --prefix /tmp/bags-lab-install
(cd /tmp && /tmp/bags-lab-install/bin/bagsolar_resource_smoke)
```

## CI

Current GitHub Actions workflow builds/tests on **Ubuntu** (developer raylib path).
A full portable multi-OS matrix is prepared in the multiplatform architecture
doc but is **not** claimed green for Windows/macOS/ARM64 until those jobs exist
and execute real package validation.
