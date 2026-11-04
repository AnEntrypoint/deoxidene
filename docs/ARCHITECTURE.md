# Architecture

This document records what deoxidene is built on, why, and what has been measured. Each decision is tagged:

- **Measured**: a number or a build result from this repository's tooling, with the spike sources under `docs/spikes/`.
- **Checked**: a fact confirmed from an upstream release page or the vendor's documentation, not yet built here.
- **Planned**: decided, not implemented.

## Principles

1. **One core language, one toolchain story.** The core is C++23. Each supported OS needs only CMake 3.25+, Ninja and a C++23 compiler.
2. **Predictable frames.** The frame loop does not allocate and does not wait on a garbage collector. Buffers are created at startup or on a loading step, and reused.
3. **Native where it matters, scripted where users extend.** Performance code is native and can be reloaded while the app runs. User-facing logic runs in an embedded interpreter.
4. **The host owns the data, the code is replaceable.** State lives in host-owned memory. Plugins and scripts receive views and handles, never ownership.

## Decisions at a glance

| Area | Decision | Status | Evidence |
| --- | --- | --- | --- |
| Core language | C++23 | Measured | Library and tests on Linux (GCC 15.2); Windows and macOS builds and CI pending |
| Dependencies | `std` only, plus vendored `third_party/tl/expected.hpp` as fallback | Measured | Committed `34a7981` |
| Toolchain for cross builds | `zig c++` (zig 0.14.1) | Measured | Hello program builds and links for Windows x86-64, macOS arm64, Linux x86-64 and aarch64; not run |
| Hot reload | Versioned native plugin libraries, reloaded at frame boundaries | Measured on Linux | Spike below |
| Scripting | QuickJS-NG, interpreter, ES2023 | Measured on Linux | Spike below |
| GPU compute and graphics | WebGPU through the `webgpu.h` C API, shaders in WGSL, with wgpu-native binaries | Checked | Release assets listed below; not yet integrated |
| Parallel work | Owned job system, fixed worker pool | Planned | Nothing built yet |
| Memory | Frame arenas and persistent pools, spans and handles across boundaries | Planned | Zero-copy buffer sharing with QuickJS measured |

## Execution tiers

### Tier 0: native plugins (performance code and HMR)

Hot code is compiled into a shared library (`.so`, `.dylib` or `.dll`). The host loads it, calls its entry points through a C struct table, and reloads it when the source changes. Plugins run at native speed, with no runtime and no GC.

This is the compile-on-change route to JIT-like iteration. It gives the edit-and-run cycle a JIT provides, without an in-process compiler backend. Measured numbers are in *Hot reload pipeline*.

### Tier 1: QuickJS-NG (user scripts, gameplay logic, configuration)

QuickJS-NG is a maintained fork of QuickJS. It is an interpreter that implements ES2023, with no JIT. It is chosen over LuaJIT, Lua 5.4 and V8 for these reasons:

- Its C API builds as a C library. The GCC host build gives a static library of 1,465,350 bytes. Cross builds were not measured.
- It wraps host memory in a JavaScript `ArrayBuffer` and `Uint8Array` without copying. **Measured:** JS writes and host writes land at the same address, with no mismatches over 64 bytes.
- Startup is about 0.1 ms per runtime. **Measured:** 94 to 106 µs for a new runtime and context.
- A native function call from JS costs about 23 to 27 ns over an equivalent JS loop. **Measured** on a noisy host (load average 6 to 10 on 12 cores), two runs.

Lua 5.4 is smaller and a fine interpreter, but it has no typed arrays. Zero-copy buffer sharing would need a userdata layer. That is why QuickJS was chosen. Lua 5.4 was not measured.

### Tier 2: GPU compute and graphics (WebGPU)

WebGPU is one API over Vulkan, Metal and Direct3D 12. Shaders are written in WGSL. The `webgpu.h` C API is reached through **wgpu-native**.

**Checked:** the wgpu-native v29.0.1.1 release lists prebuilt archives for Linux (x86-64, aarch64), macOS (x86-64, aarch64), and Windows (x86-64 and aarch64, both MSVC and GNU). Nothing has been downloaded or linked yet. Dawn is the other candidate, but it publishes nightly builds only.

Rust is present only as a prebuilt binary in this tier. It is not a build dependency.

## Choices we rejected

| Option | Reason |
| --- | --- |
| Mojo as the core or a first-class language | **Checked:** Mojo runs natively on Linux and macOS. Windows is reachable only through WSL, with native Windows still in progress. The compiler was open-sourced in August 2026, but outside compiler contributions are not accepted yet. A required native Windows build would fail. Revisit when native Windows ships. |
| Go for lifecycle and orchestration | A second runtime with its own garbage collector and scheduler in the same process. Calls into C++ go through cgo. This conflicts with predictable frames. |
| Rust for lifecycle and orchestration | Strong, but a second toolchain with slower builds, and the same FFI boundary as C++. The orchestration needs here are a job graph and state machines, which C++ covers without a boundary. Rust stays as a prebuilt library in Tier 2 only. |
| Zig as the implementation language | **Checked:** Zig 0.16.0 is the latest stable release, with breaking changes in each version and no 1.0 date. Zig is used as the toolchain now, and reconsidered at 1.0. |
| LuaJIT | **Checked:** arm64 Windows support exists upstream but is not in a release. Building it is also fiddly on Windows. |
| V8 or JavaScriptCore | Large builds and binary sizes, and their JIT is not needed for the plugin model above. |
| LLVM ORC as an in-process JIT | A heavy dependency. Compile-on-change with `zig c++` gives the iteration speed without it. |
| WebAssembly runtimes (wasmtime, wasm3) | Good as a sandbox for untrusted plugins. Deferred: wasmtime is large and Rust-built. Revisit when plugins come from outside the team. |

## Hot reload pipeline

1. **Watch.** Poll the modification time of each plugin source file about ten times a second in development. Polling is portable and cheap for a small tree.
2. **Build.** Compile the changed plugin as a shared library, with `-shared -fPIC`, using `zig c++` or the project's C++ compiler. Only the changed plugin is rebuilt.
3. **Load.** Load the new library under a versioned file name (`plugin.gen<N>.so` or `.dll`). Windows locks a loaded DLL, so the name must change on every reload. Unload the previous generation after the swap.
4. **Swap at a frame boundary.** Replace the function table between frames, never inside one. A failed build keeps the old plugin running and shows the compiler output.
5. **State stays with the host.** The host owns the state struct. Plugins receive a pointer. The struct carries an ABI version, and a version change means a migration hook or a reset.

**Measured** (Linux x86-64, spike in `docs/spikes/hmr/`, zig 0.14.1, plugin compiled with `-O1`):

| Step | Run 1 | Run 2 (committed spike) |
| --- | --- | --- |
| Cold build of the plugin | 162.1 ms | 126.2 ms |
| Rebuild after an edit (three edits) | 145.6, 162.5, 153.7 ms | 129.6, 126.4, 141.4 ms |
| Load and bind of the new generation | 0.09 to 0.13 ms | 0.10 to 0.14 ms |
| Edit to live code (three edits) | 146.0, 173.2, 164.4 ms | 129.9, 149.0, 141.8 ms |

The frame count check matched the expected total (7497) in both runs, so each reload took effect on the very next frame.

Also **measured:** the system `g++` builds the same plugin in 0.07 to 0.21 s. zig is no faster for the development loop, so the development loop uses the project's own compiler. zig is the cross-target tool.

**Gaps in this pipeline:** the spike builds on the frame thread, so each rebuild stalls one frame by about 150 ms. The planned fix builds on a worker thread and swaps at the next boundary. Windows loading (`LoadLibrary`) is not yet written.

## Memory model

- **Frame arena.** A linear allocator reset at the end of each frame. Nothing is freed one object at a time.
- **Persistent pools.** Fixed-size buffers with free lists, created at startup and reused across frames.
- **Views, not owners, across boundaries.** Plugins and scripts receive `std::span` views or generation-checked handles. Raw owning pointers do not cross a boundary.
- **Zero-copy into scripts.** A pooled buffer is exposed to QuickJS as an `ArrayBuffer` (measured above). The script and the host see the same bytes.
- **Staging for GPU.** Uploads go through a ring of reused staging buffers. Each frame takes the next slot.
- **Allocation budget.** Planned: a debug build counts allocations inside the frame loop and fails a test when a frame allocates.

## Parallelism

- **Job system (planned).** A fixed pool with one worker per hardware thread. It offers `parallel_for` over chunks and a fork-join graph per frame. It is written in-house, with no TBB dependency. A work-stealing deque is added only when profiling shows the shared queue is the bottleneck.
- **SIMD (planned).** Start with the compiler's auto-vectorization over structure-of-arrays layouts, which needs no dependency. Google Highway is the candidate for hand-written kernels, as an opt-in module. Not yet evaluated here.
- **Serial and parallel sharing.** Both use the same spans and arenas, so a job reads and writes the buffer that the serial code produced, without a copy.
- **GPU sharing.** Compute and graphics read the same staging and storage buffers through WebGPU.
- **Cluster work.** Not in the core. An optional MPI module is the path if it is needed.

## Repository layout (target)

```
include/deoxidene/   core headers: Result, NotNull, SmallVec, parse (planned: arena, pool, jobs)
third_party/tl/      vendored tl::expected (CC0), fallback only
tests/               CTest suite
bench/               benchmarks
examples/hello/      smallest program
docs/                this document and the spike sources under docs/spikes/
scripts/             convenience wrappers around the CMake presets
skill/deoxidene/     agent instructions
```

Planned, not yet present: `host/` (window loop and plugin loader), `plugins/` (Tier 0 modules), `scripts/js/` (Tier 1), `shaders/` (Tier 2).

## What is verified and what is not

Verified on Linux x86-64 (GCC 15.2):

- The four CMake presets `debug`, `release`, `asan-ubsan` and `native-perf` configure, build and pass CTest.
- The clang-tidy gate passes on `include/deoxidene/*.hpp` with warnings as errors.
- The hello program compiles and links for Windows x86-64, macOS arm64, and Linux x86-64 and aarch64, with zig 0.14.1. Nothing was run on those targets.

Not verified:

- Windows (MSVC and MinGW) and macOS **builds of the library and tests**. CI has not run.
- The scripts on Windows (`scripts/deoxidene.ps1`), since no PowerShell is installed here.
- Hot reload on Windows and macOS.
- QuickJS-NG cross builds (only the GCC host build was measured) and its v0.18.0 release.
- wgpu-native: the release assets are listed, but nothing has been downloaded or linked.

## Open risks

- **Zig is pre-1.0.** Each release changes the standard library. Keep the zig pin in one place once it is used.
- **Mojo on Windows.** The core does not depend on Mojo, so this is a missed option rather than a blocker. Revisit when native Windows ships.
- **Noisy measurements.** The host had a load average of 6 to 10 during the call-overhead runs. Repeat the numbers on an idle machine before quoting them externally.
- **MSVC behaviour** of the `SmallVec` and `NotNull` code is untested until CI runs.
