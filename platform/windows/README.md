# Windows packaging notes

**Status: BUILD-READY CMake foundation / NOT YET TESTED runtime**

Native Windows packages must be built and verified on a Windows x86_64 host.
This Linux development host does **not** claim Windows runtime verification.

## Primary release toolchain

**MSVC** (Visual Studio 2022, `-A x64`) is the intended Windows release path.

Optional MinGW-w64 cross-compile from Linux (`cmake/toolchains/windows-x64-mingw.cmake`)
exists for packaging experiments only and is **not verified**.

## Expected layout after packaging

```text
dist/windows-x64/
  BAGS_LAB.exe
  BAGS_LAB.bat          # from launcher/windows/
  data/
  assets/planets/
  *.dll                 # only redistributable non-system DLLs actually required
  LICENSE
  README.md
  THIRD_PARTY_LICENSES.md
```

## Suggested Windows build flow

```bat
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release
ctest --test-dir build-windows-x64 -C Release --output-on-failure
```

Use `-DBAGS_LAB_BUNDLE_RAYLIB=ON` (enabled by the preset) so Windows does not
depend on pkg-config raylib.

Resource discovery uses the executable directory (no fixed `C:` / `D:` drive
letter). Optional CSPICE remains a developer-supplied capability and must never
become a hidden runtime requirement.

Horizons HTTP uses an argv-based `curl` launch via `ExternalCommand` (no shell).
Windows hosts still need `curl.exe` on PATH for optional online Horizons; offline
local/JSON providers remain the default laboratory path.
