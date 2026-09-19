# BAGS_LAB — Linux x86_64 portable runtime

**Status: VERIFIED** (static raylib 6.0 + bundled GLFW; clean Kali host without libglfw/raylib/tiff)

## Launch

On every filesystem, use the shell launcher:

```sh
bash BAGS_LAB.sh
```

On its first run, it detects missing host runtime libraries and, if needed,
asks for administrator authorization to install the small Linux runtime set.
Later launches on that computer do not reinstall anything. Do not launch the
raw `BAGS_LAB` / `BAGS_LAB.EXE` first on a new computer: Linux resolves their
libraries before the program gets an opportunity to repair them.

Resources (`data/`, `assets/`) sit next to this executable and are resolved from
the binary location (or via `BAGS_LAB_RESOURCE_ROOT` when the VFAT launcher runs
a temporary ELF copy).

## Dependencies

This portable build links **static raylib 6.0** with raylib’s **bundled GLFW**
(statically linked). It does **not** require `libraylib.so`, `libtiff`, or
`libglfw.so.3`. Host X11/OpenGL (and glibc/libstdc++) stacks are still expected
from the Linux installation.

Starfield rendering uses cheap `DrawPoint3D` points (not per-star `DrawSphere`).

See `platform/linux/README.md` in the source tree for the development vs
portable test matrix and Xinerama build-host requirements.
