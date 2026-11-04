# Spikes

Small programs behind the measurements in `docs/ARCHITECTURE.md`. They are references for the numbers, not part of the library build.

## HMR (`hmr/`)

Builds a host that loads a plugin shared library, rebuilds it with `zig c++` after each edit, and swaps it in at a frame boundary.

```
mkdir -p /tmp/hmr-run && cp docs/spikes/hmr/* /tmp/hmr-run/ && cd /tmp/hmr-run
zig c++ host.cpp -o host -std=c++23 -O1 -ldl
./host
```

The host copies `plugin_v1.cpp`, `plugin_v2.cpp` and `plugin_v3.cpp` over `plugin.cpp` to simulate edits, so run it in a copy. It prints the cold build time, and for each reload the build, load and edit-to-live times. The last line is the frame count and the total. The total is 7497 for the unmodified sources.

## QuickJS-NG (`quickjs/probe.c`)

The zero-copy probe for QuickJS-NG. It needs the QuickJS-NG source tree, which is fetched from the GitHub archive of `quickjs-ng/quickjs` (not a git clone). Build the library, then compile the probe against `quickjs.h` and `libquickjs.a`, and run it. It prints the buffer addresses and whether the host and JavaScript see the same memory.
