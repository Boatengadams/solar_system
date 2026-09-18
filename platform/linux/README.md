# Linux packaging notes

## Portable runtime (`dist/linux-x64`) — VERIFIED

Generate on a Linux x86_64 host:

```sh
./tools/package_portable.sh linux-x64
```

The portable package includes:

- `BAGS_LAB` / `BAGS_LAB.EXE` executable (static raylib 6.0 + bundled GLFW)
- `BAGS_LAB.sh` / `BAGS_LAB.BAT` launcher (VFAT-safe temp-exec strategy)
- `data/` and `assets/planets/` (manifest-referenced GLBs)
- **No** dynamic `libraylib.so`
- **No** `libtiff`
- **No** `libglfw.so.3` (GLFW is statically linked from raylib’s tree)

When `lib/` exists, it holds only non-system runtime libraries that the
dependency audit requires. With the current static-raylib portable build,
`lib/` is typically empty/absent. Do **not** copy a host `libglfw.so.3` into
`lib/`.

## Host libraries still required

This remains a dynamically linked desktop build for system graphics stacks.
Typical Linux hosts still provide:

- glibc / libstdc++ / libgcc
- X11 / OpenGL stacks used by raylib’s desktop backend

GLFW itself is **not** required from the host for the portable package.

## Linux ARM64 (`dist/linux-arm64`) — NOT YET TESTED

Same conceptual layout. Package only on an aarch64 Linux host (or after a
verified cross-compile smoke). The dispatcher refuses to fabricate ARM64
packages on x86_64.

## Verification checklist (linux-x64)

- Resource resolution from a foreign working directory
- USB VFAT launcher (`BAGS_LAB.sh`) from `/tmp`
- Portable dependency audit (no libtiff / no dynamic libraylib / no libglfw)
- Clean-host / second-machine run without installing GLFW, raylib, or TIFF
- GUI start on a Linux host with display available
