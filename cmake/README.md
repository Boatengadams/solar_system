# CMake helpers

| Path | Role |
| --- | --- |
| `BagsLabPlatform.cmake` | Detects `BAGS_LAB_TARGET_PLATFORM`, `BAGS_LAB_TARGET_ARCH`, `BAGS_LAB_DIST_ID` |
| `toolchains/linux-arm64.cmake` | Optional aarch64 cross toolchain (**NOT YET TESTED** package) |
| `toolchains/windows-x64-mingw.cmake` | Optional MinGW cross toolchain (**NOT VERIFIED** runtime) |

Core build logic remains in the top-level `CMakeLists.txt` and `CMakePresets.json`.
See `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.
