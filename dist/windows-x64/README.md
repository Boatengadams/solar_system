# BAGS_LAB — Windows x64 portable runtime

**Status: PLANNED / NOT YET TESTED**

No Windows binary is shipped here yet. Do not claim Windows support until a
native Windows build and runtime acceptance have been completed.

## Intended layout (future)

```text
dist/windows-x64/
  BAGS_LAB.exe
  BAGS_LAB.bat
  data/
  assets/planets/
  *.dll          # only redistributable runtime DLLs actually required
  LICENSE
  README.md
  THIRD_PARTY_LICENSES.md
```

## Build (native Windows host)

Primary release toolchain: **MSVC** (Visual Studio 2022 x64).

```bat
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release
ctest --test-dir build-windows-x64 -C Release --output-on-failure
```

MinGW cross-compile from Linux is an optional experiment only
(`windows-x64-mingw-cross` preset) and is **not** a verified release path.

See `platform/windows/README.md` and `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.
