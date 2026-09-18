# BAGS_LAB — macOS x64 portable runtime

**Status: PLANNED / NOT YET TESTED**

No macOS binary or `.app` bundle is shipped here yet.

Produce on a macOS host with `CMAKE_OSX_ARCHITECTURES=x86_64`:

```sh
cmake --preset macos-x64
cmake --build --preset macos-x64
```

Intended package: `BAGS_LAB.app` with resources under `Contents/Resources/`.

See `platform/macos/README.md` and `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.
