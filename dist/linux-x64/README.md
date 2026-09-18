# BAGS_LAB — Linux x86_64 portable runtime

**Status: VERIFIED** (static raylib 6.0 + bundled GLFW; clean Kali host without libglfw/raylib/tiff)

Run:

```sh
./BAGS_LAB.sh
```

Or (on filesystems that preserve `+x`):

```sh
./BAGS_LAB
```

Resources (`data/`, `assets/`) sit next to this executable and are resolved from
the binary location (or via `BAGS_LAB_RESOURCE_ROOT` when the VFAT launcher runs
a temporary ELF copy).

This portable build links **static raylib 6.0** with raylib’s **bundled GLFW**
(statically linked). It does **not** require `libraylib.so`, `libtiff`, or
`libglfw.so.3`. Host X11/OpenGL (and glibc/libstdc++) stacks are still expected
from the Linux installation.
