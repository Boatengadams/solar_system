# BAGS_LAB Multi-Platform Build Architecture

Phase 2 foundation document. Status labels:

| Label | Meaning |
| --- | --- |
| **VERIFIED** | Native build + package/runtime checks completed |
| **BUILD-READY** | CMake/presets/tooling exist; runtime not yet proven |
| **PLANNED** | Design agreed; implementation incomplete |
| **NOT YET TESTED** | Must not be advertised as supported |

Scientific algorithms, integrators, constants, and education content are out of
scope for portability work.

---

## 1. Platform matrix

| Dist ID | OS | Arch | Package status | Runtime status |
| --- | --- | --- | --- | --- |
| `linux-x64` | Linux | x86_64 | VERIFIED | VERIFIED (static bundled GLFW) |
| `linux-arm64` | Linux | aarch64 | BUILD-READY | NOT YET TESTED |
| `windows-x64` | Windows | x64 | BUILD-READY CMake | NOT YET TESTED |
| `macos-x64` | macOS | x86_64 | BUILD-READY CMake | NOT YET TESTED |
| `macos-arm64` | macOS | arm64 | BUILD-READY CMake | NOT YET TESTED |

`dist/<id>/` may contain only `README.md` until a real package is produced.
Fabricated binaries are forbidden.

---

## 2. CMake architecture

Key options / variables:

| Name | Role |
| --- | --- |
| `BAGS_LAB_BUNDLE_RAYLIB` | Fetch/static-link raylib 6.0 (portable; no TIFF) |
| `BAGS_LAB_PORTABLE` | Hint that configure is for relocatable/USB packaging |
| `BAGS_LAB_INSTALL_PLANET_MODELS` | Stage presentation GLBs |
| `BAGSOLAR_ENABLE_SPICE` | Optional CSPICE (never a hidden USB requirement) |

Auto-detected (via `cmake/BagsLabPlatform.cmake`):

| Name | Example |
| --- | --- |
| `BAGS_LAB_TARGET_PLATFORM` | `linux` / `windows` / `macos` |
| `BAGS_LAB_TARGET_ARCH` | `x64` / `arm64` |
| `BAGS_LAB_DIST_ID` | `linux-x64` |
| `BAGS_LAB_HOST_MATCHES_TARGET` | TRUE on native hosts |

Platform detection prefers CMake `WIN32` / `APPLE` / `UNIX`,
`CMAKE_SYSTEM_PROCESSOR`, and (on Apple) `CMAKE_OSX_ARCHITECTURES`.

`PkgConfig` is required only when **not** bundling raylib. Windows release
builds must use `BAGS_LAB_BUNDLE_RAYLIB=ON`.

Apple builds create `BAGS_LAB` as `MACOSX_BUNDLE`. Install rules place resources
under `BAGS_LAB.app/Contents/Resources/` (layout prepared; Darwin runtime
**NOT YET TESTED**).

---

## 3. Preset architecture

`CMakePresets.json` separates developer presets from portable/target presets:

| Configure preset | Host requirement | Notes |
| --- | --- | --- |
| `debug` / `release` | any | developer |
| `portable` | any | host defaults + bundle raylib |
| `linux-x64` | Linux | VERIFIED packaging path |
| `linux-arm64` | Linux + toolchain file | cross template; NOT YET TESTED |
| `windows-x64` | Windows | MSVC generator; condition Darwin≠ |
| `windows-x64-mingw-cross` | Linux | MinGW experiment; NOT VERIFIED |
| `macos-x64` / `macos-arm64` | Darwin | `CMAKE_OSX_ARCHITECTURES` |

Matching `buildPresets` exist. `testPresets` currently emphasize host/linux.
`packagePresets` include `linux-x64` TGZ only.

Presets with `condition` on `hostSystemName` do **not** imply cross-compilation
works on the wrong OS.

---

## 4. Toolchain architecture

| File | Purpose | Status |
| --- | --- | --- |
| `cmake/toolchains/linux-arm64.cmake` | `aarch64-linux-gnu-*` cross | BUILD-READY template |
| `cmake/toolchains/windows-x64-mingw.cmake` | MinGW-w64 cross | BUILD-READY / NOT VERIFIED |

No macOS SDK cross-toolchain from Linux is provided or claimed.

---

## 5. ResourceRoot architecture

`src/data/ResourceRoot.*` resolves runtime `data/` without cwd dependence and
without developer absolute paths.

Resolution order for `resolve(executablePath)`:

1. `<exeDir>/data` (USB / portable side-by-side)
2. `<exeDir>/../data` (source `build/` layout)
3. macOS: `<…>.app/Contents/Resources/data` when path matches `.app/Contents/MacOS/`
4. `<prefix>/share/bags_lab/data`
5. legacy `<prefix>/share/bagsolar/data`

Optional env: `BAGS_LAB_RESOURCE_ROOT` → package root or `data/` directory
(Linux VFAT temp-exec launcher).

Forbidden as baked-in requirements: `/home/…`, `/run/media/…`,
`C:\Users\…`, `/usr/local` hardcodes, `/home/kali/spice`.

---

## 6. raylib strategy

| Target | Strategy | Status |
| --- | --- | --- |
| Linux portable | Static raylib 6.0 via FetchContent; bundled GLFW (static) + host X11/GL | VERIFIED |
| Linux developer | System pkg-config raylib allowed | supported for `build/` |
| Windows | Bundle static raylib 6.0 (`USE_EXTERNAL_GLFW=OFF`) | BUILD-READY / NOT YET TESTED |
| macOS | Bundle static raylib 6.0 | BUILD-READY / NOT YET TESTED |
| Linux ARM64 | Same static strategy as linux-x64 | BUILD-READY / NOT YET TESTED |

Portable Linux must not regress to requiring `libraylib.so`, `libtiff.so.3`, or
`libglfw.so.3`. Do not copy a developer-machine `libglfw.so.3` into `lib/`.
Only redistributable, actually-needed libraries may be staged into `lib/` /
DLL folders.

---

## 7. Windows strategy

- Primary release toolchain: **MSVC** (`windows-x64` preset).
- MinGW cross from Linux is optional and **not** acceptance-verified.
- Runtime: `BAGS_LAB.exe` + adjacent `data/` + `assets/` + required DLLs.
- Launcher: `launcher/windows/BAGS_LAB.bat` prepends package dir to `PATH`.
- Horizons: argv-based `curl.exe` via `ExternalCommand` (no `cmd` shell string);
  offline providers remain default.
- Do not require Bash to launch the Windows application.

---

## 8. macOS strategy

- Native Apple toolchain only; set `CMAKE_OSX_ARCHITECTURES` per preset.
- `MACOSX_BUNDLE` target + Resources install layout prepared.
- Convenience launcher: `launcher/macos/BAGS_LAB.command`.
- Prefer `dist/macos-x64` and `dist/macos-arm64` over legacy `dist/macos`.
- Codesign/notarize: PLANNED (Phase 4+); not claimed here.

---

## 9. Linux ARM64 strategy

- Dist ID `linux-arm64`; ELF AArch64.
- Packaging model mirrors linux-x64 (exe + VFAT launcher names + data/assets).
- `package_portable.sh` refuses to fabricate ARM64 packages on x86_64 until a
  verified cross or native smoke exists.
- Physics/rendering code must not change for architecture alone.

---

## 10. Packaging architecture

```text
tools/package_portable.sh   # dispatcher (platform id → implementation)
tools/package_linux.sh      # Linux staging + audit (VERIFIED for linux-x64)
dist/<platform-id>/         # real package or README stub
```

Dispatcher accepts: `linux-x64`, `linux-arm64`, `windows-x64`, `macos-x64`,
`macos-arm64`. Unsupported combinations exit non-zero with a clear message.

Windows/macOS native packaging scripts are Phase 3/4 deliverables; Phase 2 only
defines layouts and refuses fake binaries.

---

## 11. Launcher architecture

| Launcher | Role | Status |
| --- | --- | --- |
| `launcher/linux/BAGS_LAB.sh` | VFAT temp-exec + `BAGS_LAB_RESOURCE_ROOT` | **VERIFIED** |
| `launcher/windows/BAGS_LAB.bat` | PATH + relative exe discovery | BUILD-READY |
| `launcher/macos/BAGS_LAB.command` | `.app` / dist discovery | BUILD-READY |

Launchers must not claim success for missing binaries.

---

## 12. CI architecture (prepared, not fully implemented)

Layers (each must be explicit; configure-only is not “green”):

1. **Build validation** — compile on runner OS/arch
2. **Test validation** — CTest / scientific suites
3. **Package validation** — `package_portable` / CPack smoke
4. **Dependency audit** — no libtiff / unexpected NEEDED; license list
5. **Artifact upload** — only after package validation
6. **Real-device acceptance** — manual/USB (outside CI)

| Job (planned) | Runner | Phase |
| --- | --- | --- |
| Linux x64 developer | `ubuntu-latest` | exists today (`.github/workflows/ci.yml`) |
| Linux x64 portable | Ubuntu + `BAGS_LAB_BUNDLE_RAYLIB` | Phase 8 |
| Linux ARM64 | `ubuntu-24.04-arm` or self-hosted | Phase 8 |
| Windows x64 | `windows-latest` MSVC | Phase 8 |
| macOS ARM64 / x64 | `macos-latest` | Phase 8 |

Current CI still installs system raylib 5.5 for the developer path; portable
raylib 6.0 is the USB release path. Aligning CI portable jobs is Phase 8 work.

---

## 13. Dependency policy

- Default laboratory: offline; no network required.
- Bundle only libraries that are (a) required by the binary, (b) redistributable,
  (c) recorded in third-party docs.
- System graphics stacks (X11/GL on Linux portable; system frameworks on macOS;
  system DLLs on Windows) may remain host-provided when documented. Portable
  Linux must statically link raylib’s bundled GLFW — do not depend on or copy
  host `libglfw.so.3`.
- Never copy developer-tree libraries from arbitrary absolute paths into `dist/`.

---

## 14. License / redistribution policy

For every bundled runtime component record: name, version, license, source,
redistribution status (`docs/THIRD_PARTY_LICENSES.md`).

CSPICE and proprietary kernels are **not** bundled by default. Enabling SPICE
requires explicit developer-supplied include/library paths and never implies
redistribution rights.

---

## 15. Verification matrix

| Check | linux-x64 | others |
| --- | --- | --- |
| Configure + build | VERIFIED | host-specific |
| Full CTest | VERIFIED on Linux host | when native |
| Portable audit (no libtiff/libraylib/libglfw) | VERIFIED | TBD |
| Package smoke | VERIFIED | TBD |
| ResourceRoot layouts (incl. `.app` path sim) | VERIFIED path tests | Darwin TBD |
| VFAT USB launcher | VERIFIED | N/A / TBD |
| GUI acceptance on virgin host | partial / open | open |

---

## 16. Known limitations

- Only linux-x64 portable runtime is verified.
- CI does not yet build the portable multi-OS matrix.
- Windows/macOS packages are documentation stubs.
- linux-arm64 packaging refused on x86_64 until verified.
- Horizons still depends on a host `curl` binary when online queries are used.
- macOS codesign/notarize not implemented.
- `platform/linux` previously documented shared `libraylib` staging; portable
  reference is now static raylib (docs corrected in Phase 2).

---

## 17. Phase 3 implementation plan

**Phase 3 focus: Windows x64 package (native MSVC host)**

1. On a Windows x64 machine, configure with preset `windows-x64`.
2. Build Release; run CTest.
3. Stage `dist/windows-x64/` with exe, bat, data, assets, required DLLs.
4. Run dependency listing + license notes for any bundled DLL.
5. Launch via `BAGS_LAB.bat` from a foreign working directory / USB copy.
6. Confirm ResourceRoot; confirm no developer absolute paths.
7. Document results as VERIFIED or list failures — do not claim success from Linux.

Do not start Phase 3 packaging claims until that native Windows run exists.
