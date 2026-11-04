# deoxidene

A C++23 boilerplate that closes the ergonomic and distributability gap between C/C++ and Rust, without leaving the C++ ecosystem.

The architecture decisions, the execution tiers (native hot-reload plugins, QuickJS-NG scripts, WebGPU), the options we rejected, and the measurements behind them are in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Gap closed

| Rust mechanism | deoxidene mechanism |
| --- | --- |
| `Result<T, E>` | `deoxidene::Result<T, E>` over `std::expected` (fallback: vendored `tl::expected`) |
| `Option<T>` | `std::optional<T>` used directly at API boundaries |
| Ownership/borrow checker | `NotNull<T*>`, `std::span`, `Owner<T>`, `Box<T>`/`Rc<T>` aliases over `unique_ptr`/`shared_ptr`, enforced by `.clang-tidy`'s `cppcoreguidelines-*`/`bugprone-*` gate |
| `cargo` | `CMakePresets.json`: one set of commands on every OS, no package manager |
| `cargo new` / `build` / `run` | `deoxidene.sh new` / `build` / `run`, or the same steps through `cmake --preset` |
| `cargo test` | `ctest --test-dir build/debug`, or `deoxidene.sh test` |
| Miri-class UB detection | `deoxidene.sh test-sanitize` (ASan + UBSan preset), gated in CI |
| `rustup target add` + cross-compile | CMake presets for x64 and arm64 on Linux, Windows and macOS |
| `cargo build --release` publishing | `.github/workflows/release.yml`: a tag push builds every target and uploads Release assets |

## Requirements

Three tools, on every OS:

- CMake 3.25 or newer
- Ninja
- A C++23 compiler

Install Ninja with `winget install Ninja-build.Ninja` on Windows, `brew install ninja` on macOS, or `apt install ninja-build` on Linux.

Nothing else is needed. There is no package manager and no network access at configure time.

Verified here: GCC 15.2 on Linux. Windows (MSVC) and macOS (Apple Clang) run in CI, which has not run yet for this change.

## Quickstart

The same commands work on Linux, macOS and Windows:

```
cmake --preset release
cmake --build build/release
build/release/examples/hello/deoxidene_hello 42
```

On Windows the binary is `build\release\examples\hello\deoxidene_hello.exe`.

Run the tests:

```
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Optional wrappers exist for the common steps: `scripts/deoxidene.sh` on Linux and macOS, `scripts/deoxidene.ps1` on Windows. They run `new`, `build`, `run`, `test`, `test-sanitize`, `tidy`, `bench` and `pgo`:

```powershell
.\scripts\deoxidene.ps1 run
.\scripts\deoxidene.ps1 new my-project
```

## Target matrix

Native release presets: `x64-linux-release`, `arm64-linux-release`, `x64-windows-release`, `arm64-windows-release`, `x64-macos-release`, `arm64-macos-release`.

Development presets: `debug`, `release`, `native-perf`. Sanitizer presets: `asan`, `ubsan`, `asan-ubsan`. The sanitizer presets are for GCC and Clang only, not MSVC.

`cmake --list-presets` enumerates every preset in `CMakePresets.json`.

## Library selection

The rule: standard C++23 first, one vendored header only where a toolchain lacks a feature, and nothing that needs a package manager.

| Need | Choice | Why |
| --- | --- | --- |
| `Result` | `std::expected`, with the vendored `third_party/tl/expected.hpp` (tl::expected, CC0) as fallback | `<expected>` ships in libc++ from Xcode 15, in libstdc++ 12 and later, and in MSVC; the fallback keeps older Apple toolchains building without an Xcode upgrade |
| Non-owning view | `std::span` | Standard since C++20 |
| Owning pointers | `std::unique_ptr` and `std::shared_ptr`, as `Box` and `Rc` | Standard |
| Non-null pointer | `deoxidene::NotNull<T*>`, in `ownership.hpp` | Replaces GSL's `not_null`; terminates on a null value |
| Small vector | `deoxidene::SmallVec<T, N>`, in `containers.hpp` | Inline storage up to N elements, then `std::vector`; replaces Boost.Container's `small_vector` |
| Parsing | `std::from_chars` | Standard and locale-free |

Rejected, and why:

| Library | Why not |
| --- | --- |
| Microsoft GSL | Only three aliases were used. It needs vcpkg or a fetch step on Windows. `std::span` and a 20-line `NotNull` cover the same ground. |
| Boost.Container | Installing Boost is heavy on Windows and the project used one container. `SmallVec` is about 50 lines. |
| mimalloc | It replaces the allocator on Linux and macOS when linked as a shared library. On Windows a static library does not reliably replace `new` and `delete`, and the override needs a DLL and a header in every translation unit. It cannot be a default on all three OSes. |
| vcpkg | Every machine must clone and bootstrap it and set `VCPKG_ROOT`. Dependencies built from source take minutes on each CI runner. |

## Native performance profile

The `native-perf` preset builds a Release for the build host with link-time optimization and `-march=native` tuning, plus the benchmark target. `./scripts/deoxidene.sh bench` (or `.\scripts\deoxidene.ps1 bench`) builds it and runs `deoxidene_bench`. The bench times `deoxidene::Result` parsing against a plain bool-returning parser, and `SmallVec` against `std::vector` with `reserve`. It exits non-zero if the two sides of either pair disagree.

- `DEOXIDENE_NATIVE_TUNE=ON` compiles for the build host's CPU. Those binaries do not run on other CPUs.
- `DEOXIDENE_LTO=ON` enables link-time optimization for deoxidene executables when the toolchain supports it.
- `DEOXIDENE_BUILD_BENCH=ON` builds the benchmarks in `bench/`.
- `DEOXIDENE_BUILD_TESTS=ON` builds the tests in `tests/`. The base preset turns it on.

Current numbers, measured with `deoxidene.sh bench` on one x86-64 Linux host (g++ 15.2, `native-perf`), best of 3 runs of 7 repeats each, in ns per operation:

| Benchmark | ns/op |
| --- | --- |
| parse, raw bool parser | 6.23 |
| parse, `Result` (`from_chars`) | 5.32 |
| collect 4 ints, `std::vector` with `reserve(4)` | 9.18 |
| collect 4 ints, `SmallVec<int, 8>` | 1.47 |

- Numbers move between runs and sessions on the same host. Two earlier sessions measured the raw parser at 5.6 to 5.8 ns. Compare runs from one session only.
- `SmallVec` avoids the heap only while the collection fits in N. Use `std::vector` for collections that can grow past it.
- The collection benchmarks force each container into memory before reading it. Without that, the optimizer removes the storage and the numbers measure nothing.

The older flag comparison below was measured before the current parser and `SmallVec`. Its numbers are not comparable to the table above.

| Build | raw | `Result` |
| --- | --- | --- |
| Release, `-O2 -g` (`RelWithDebInfo`) | 6.18 | 6.74 |
| `-O3` | 6.19 | 6.96 |
| `-O3 -march=native` | 6.18 | 6.30 |
| `-O3` with LTO | 6.34 | 6.81 |
| `native-perf` (`-O3`, LTO, `-march=native`) | 5.71 | 6.03 |
| `native-perf` with PGO (`deoxidene.sh pgo`) | 4.71 | 4.81 |

PGO here is trained on the same benchmark it is measured on, so the last row is in-sample. For a real application, profile your own representative workload. `deoxidene.sh pgo` is GCC only.

## Install the agent skill

Copy `skill/deoxidene/SKILL.md` into `~/.claude/skills/deoxidene/` (or your agent's skill directory) to teach an AI agent the invariants: always `Result<T,E>` not exceptions, always `NotNull` and `Owner` instead of raw owning pointers, always run `tidy` and `test-sanitize` before declaring a change done, and always drive builds through the presets.

## License

Dual-licensed under MIT (`LICENSE-MIT`) or Apache-2.0 (`LICENSE-APACHE`), your choice, matching Rust's own convention. The vendored `third_party/tl` header is CC0.
