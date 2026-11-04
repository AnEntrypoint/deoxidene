# deoxidene

Build and modify C++23 code in a deoxidene-based project with Rust-equivalent safety guarantees, using this repo's own tooling rather than raw compiler/build invocations.

## Invariants (non-negotiable, every emission)

- **Errors are `deoxidene::Result<T, E>`, never exceptions.** Include `deoxidene/result.hpp`. Return `deoxidene::Fail<E, T>(err)` / `deoxidene::Ok<T, E>(val)`. Never `throw`. Compile with `-fno-exceptions` to make violations a compile error. `Result` is `std::expected`; the vendored `third_party/tl/expected.hpp` is used only when `<expected>` is unavailable.
- **Standard C++23 only.** No package manager and no third-party libraries. Besides the standard library, use these in-house headers:
  - `deoxidene/ownership.hpp`: `NotNull<T*>` (non-null contract at API boundaries), `Span<T>` (= `std::span<T>`), `Owner<T>` (= `T`, documentation only), `Box<T>` (unique ownership), `Rc<T>` (shared ownership).
  - `deoxidene/containers.hpp`: `SmallVec<T, N>` (no heap allocation up to N elements). Element types must be trivially copyable and default-constructible. Use `std::vector` once a collection can outgrow N.
  - `deoxidene/arena.hpp`: `Arena` (one reserved block, bump allocation, `reset()` reuses it). `allocate<T>(n)` returns `Result<std::span<T>, ArenaError>`; `T` must be trivially destructible. Frame-scoped data lives here. A span is invalid after `reset()`.
  - `deoxidene/pool.hpp`: `Pool<T>(capacity)` (every slot allocated up front). `acquire()` returns `std::optional<Index>`, `release(index)` returns the slot to the free list, and the slot keeps its contents. Persistent objects that cycle in and out live here, so the steady state allocates nothing.
  - `deoxidene/jobs.hpp`: `JobSystem` (fixed workers, `default_workers()` is one less than the hardware threads). `parallel_for(count, chunk, body)` calls `body(begin, end)` on disjoint ranges. The body must be const-callable and must not throw. Calls are serialized, so a body must not call `parallel_for` itself, and only one thread submits at a time.
- **No raw owning pointers, ever.** Include `deoxidene/ownership.hpp`. Use `Box<T>`, `Rc<T>`, `Span<T>`, `NotNull<T*>`. Never `new`/`delete`, never a bare `T*` parameter that could be null without a documented reason.
- **Every change is verified via this repo's own scripts, never raw `cmake`/`clang++` invocations directly** (except transient standalone syntax checks during authoring, which must still be followed by the real preset build before declaring done). Use `scripts/deoxidene.ps1` on Windows (no WSL/git-bash dependency required), `scripts/deoxidene.sh` on Linux/macOS:
  - `build [preset]` — configure + build a preset (default `release`).
  - `test [preset]` — build a preset (default `debug`), then run its CTest suite from `tests/` with output on failure. Must exit 0 before a change is done.
  - `tidy` — clang-tidy gate (C++ Core Guidelines checks, the borrow-checker-equivalent static pass). Must return zero warnings before a change is done.
  - `test-sanitize` — ASan + UBSan build and run. Must exit 0 on the real path before a change is done.
  - `run [preset] [args]` — build then run the example.
  - `bench` — build the `native-perf` preset (LTO, host CPU tuning, benchmarks) and run `deoxidene_bench`. Exits non-zero if the Result and raw parsers disagree.
  - `pgo` — profile-guided build of `native-perf`: instrumented build, trains on `deoxidene_bench`, then rebuilds optimized from that profile. GCC only.
- **Cross-target claims require the real preset, not an assumption.** `cmake --list-presets` names every supported triplet (native x64/arm64 across Linux/Windows/macOS). A change claimed "portable" without exercising the relevant preset (locally if the toolchain is present, otherwise via this repo's CI matrix in `.github/workflows/ci.yml`) is an unwitnessed claim.
- **`.clang-tidy` at repo root is the sole style/safety authority.** Never add a per-file suppression without a `// NOLINT(check-name): reason` comment naming why.

## Workflow

1. Read the existing header/example shapes in `include/deoxidene/` and `examples/hello/main.cpp` before adding new code — match the established Result/ownership idiom rather than inventing a parallel one.
2. Write the change using `Result<T,E>` and the `deoxidene::` ownership aliases exclusively.
3. Run `./scripts/deoxidene.sh test` — the CTest suite must pass.
4. Run `./scripts/deoxidene.sh tidy` — fix every warning before proceeding.
5. Run `./scripts/deoxidene.sh test-sanitize` — a clean run is required; any ASan/UBSan abort means the change has a real memory-safety or UB defect to fix, not suppress.
6. If the change touches a specific target triplet's concerns (MSVC-specific sanitizer absence, etc.), verify against that preset directly, or push and watch the corresponding CI matrix leg.
7. Never leave a `TODO`/`FIXME`/deferred-verification marker in place of running steps 3–6.

## Common commands (verified against this repo)

```bash
cmake --list-presets                     # enumerate every declared build target
./scripts/deoxidene.sh build release     # native release build
./scripts/deoxidene.sh test              # debug build + CTest suite — must exit 0
./scripts/deoxidene.sh test-sanitize     # ASan+UBSan build and run — must exit 0
./scripts/deoxidene.sh test tsan         # ThreadSanitizer build and run, for JobSystem changes
./scripts/deoxidene.sh tidy              # clang-tidy gate — must be warning-free
./scripts/deoxidene.sh run release 7     # build then run the hello example with arg 7
./scripts/deoxidene.sh bench             # native-perf build + run benchmark
```
