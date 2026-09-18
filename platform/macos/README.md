# macOS packaging notes

**Status: BUILD-READY CMake foundation / NOT YET TESTED runtime**

Native macOS packages must be built and verified on a macOS host. This Linux
development host does **not** claim macOS runtime verification. There is no
supported macOS cross-compile from Linux in this project.

## Expected layout after packaging

```text
dist/macos-x64/   or   dist/macos-arm64/
  BAGS_LAB.app/
    Contents/
      Info.plist
      MacOS/BAGS_LAB
      Resources/
        data/
        assets/
  BAGS_LAB.command      # convenience launcher
  LICENSE
  README.md
  THIRD_PARTY_LICENSES.md
```

`ResourceRoot` resolves `…/Contents/MacOS/BAGS_LAB` → `…/Contents/Resources/data`
(path-layout tested on Linux; Darwin runtime **NOT YET TESTED**).

## Suggested macOS build flow

```sh
cmake --preset macos-arm64   # or macos-x64
cmake --build --preset macos-arm64
ctest --test-dir build-macos-arm64 --output-on-failure
```

Presets set `CMAKE_OSX_ARCHITECTURES` and `BAGS_LAB_BUNDLE_RAYLIB=ON`.
`BAGS_LAB` is configured as a `MACOSX_BUNDLE` target on Apple.

Optional CSPICE remains external and developer-supplied. Do not claim SPICE
precision when it is unavailable.
