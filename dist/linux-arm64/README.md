# BAGS_LAB — Linux ARM64 portable runtime

**Status: BUILD-READY / NOT YET TESTED**

This directory is reserved for a native (or verified cross) `linux-arm64`
portable package. It intentionally contains **no fabricated binaries**.

## When available

Produce on a Linux aarch64 host:

```sh
./tools/package_portable.sh linux-arm64
```

Expected layout matches `linux-x64`:

- `BAGS_LAB` / `BAGS_LAB.EXE` (VFAT-friendly names)
- `BAGS_LAB.sh` / `BAGS_LAB.BAT`
- `data/`, `assets/`
- optional `lib/` only for non-system runtime libraries that are actually required

Static raylib 6.0 (no `libraylib.so`, no `libtiff`) is the intended portable
strategy, same as the verified linux-x64 reference.

See `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.
