# BAGS_LAB

BAGS_LAB is a portable C++17 scientific orbital-mechanics laboratory. It is the
final product identity of the BAGSOLAR scientific codebase: Newtonian N-body
simulation, numerical integration, telemetry, ephemeris providers, education,
curriculum Learning Lab, spacecraft/mission tools, validation, and a raylib 3D
renderer — preserved and packaged for USB-first development and distribution.

This is **not** a rewrite. Scientific algorithms, SI units, and validation
contracts remain intact. Internal identifiers may still say `bag` / `bagsolar`
where changing them would add risk without user benefit.

## What BAGS_LAB Is

An interactive desktop laboratory for learning and experimenting with orbital
mechanics. The scientific core is independent of the GUI, so physics,
validation, telemetry, and education tests can run headlessly.

## Core Capabilities

- 3D solar-system simulation with presentation-scaled planet models
- Newtonian gravity and multiple integrators (Euler, semi-implicit Euler,
  Velocity Verlet, RK4)
- Orbital elements, conservation diagnostics, adaptive timestep handling
- Telemetry sessions with CSV/JSON export and compatibility checks
- Local deterministic, JSON, optional JPL Horizons, and optional CSPICE
  ephemeris boundaries
- Education lessons, experiments, challenges, and learner reports
- Ghana curriculum Learning Lab (offline)
- Spacecraft / mission analytical tools
- Prediction → Experiment → Measure → Analyze workflows
- Offline-first operation for the default laboratory

BAGS_LAB is a Newtonian educational/scientific simulator. It is **not** a
high-fidelity production astrodynamics propagator.

## Portable USB Use

BAGS_LAB is designed so the whole project can live on a USB drive:

| Mode | What you get | Needs toolchain? |
| --- | --- | --- |
| **Development project** | Source, tests, CMake, docs, Git, assets, data | Yes, on the machine used to build |
| **Portable runtime** | Platform-native executable + data/assets + launcher | No |

Copy the project folder anywhere. Paths are resolved from the executable
location, not from `/home/...`, `C:\`, or a fixed USB letter.

Runtime packages are **native per platform**. One USB may carry Linux, Windows,
and macOS builds side-by-side. There is no universal binary.

## Running (prebuilt runtime)

### Linux x86_64

```sh
./dist/linux-x64/BAGS_LAB.sh
# or
./launcher/linux/BAGS_LAB.sh
```

### Windows x86_64

```bat
dist\windows-x64\BAGS_LAB.bat
```

Native Windows packages must be produced on Windows (or a Windows cross
environment). Verification on this development host covers Linux.

### macOS

```sh
./dist/macos/BAGS_LAB.command
```

Native macOS packages must be produced on macOS. Verification pending.

## Development

Install a C++17 toolchain, CMake ≥ 3.16, pkg-config, and raylib (development
only — packaged runtimes bundle/link what they need).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/BAGS_LAB
```

Presets:

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Makefile convenience targets still exist for quick local builds.

Optional CSPICE:

```sh
cmake -S . -B build-spice \
  -DBAGSOLAR_ENABLE_SPICE=ON \
  -DCSPICE_INCLUDE_DIR=/path/to/cspice/include \
  -DCSPICE_LIBRARY=/path/to/cspice/lib/cspice.a
```

If CSPICE is unavailable, BAGS_LAB still runs using local/JSON providers. It
never claims SPICE precision when SPICE is off.

### Portable package (Linux host)

```sh
./tools/package_portable.sh linux-x64
```

Creates `dist/linux-x64/` with executable, `data/`, `assets/`, launcher, and
collected runtime libraries where supported.

## Project Structure

```
BAGS_LAB/
├── src/                 Application and scientific domains
├── tests/               CTest suite
├── data/                Bodies, scenarios, curriculum (runtime)
├── assets/              Planet presentation models
├── docs/                User/developer and scientific documentation
├── tools/               Packaging and release smoke scripts
├── cmake/               CMake helpers
├── platform/            Per-OS packaging notes
├── launcher/            Portable launchers
├── dist/                Generated ready-to-run packages
├── CMakeLists.txt
├── Makefile
└── LICENSE
```

## Scientific Model

- SI units throughout the physics core
- Heliocentric presentation with explicit frame/epoch metadata where used
- Presentation scale: 20 display units per AU (renderer only)
- Optional Horizons requires network; optional CSPICE requires local kernels

See `docs/scientific/` for physics, telemetry, ephemeris, and validation notes.

## Assets and Licensing

Source is MIT (`LICENSE`). Third-party terms are summarized in
`docs/THIRD_PARTY_LICENSES.md`. Planet GLB models carry claimed Creative Commons
metadata; attribute authors when redistributing visual packages.

## Git / GitHub

Git travels with the project. **Credentials do not.**

```sh
git status
git add -A
git commit -m "Describe your change"
git remote add origin <your-repository-url>   # optional, when you choose
git push -u origin main
```

Do not store PATs, passwords, SSH private keys, or `.env` secrets inside this
tree. Authenticate with your own Git credential helper or SSH agent outside the
project.

## License

MIT License — see `LICENSE`. Copyright notice retains BAGSOLAR / BAGS_LAB
contributors.
