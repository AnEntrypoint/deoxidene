# deoxidene

A C++23 boilerplate that closes the ergonomic and distributability gap between C/C++ and Rust — without leaving the C++ ecosystem.

## Gap closed

| Rust mechanism | deoxidene mechanism |
| --- | --- |
| `Result<T, E>` | `deoxidene::Result<T, E>` over `std::expected` (fallback: `tl::expected`) |
| `Option<T>` | `std::optional<T>` used directly at API boundaries |
| Ownership/borrow checker | `gsl::not_null`, `gsl::span`, `gsl::owner`, `Box<T>`/`Rc<T>` aliases over `unique_ptr`/`shared_ptr`, enforced by `.clang-tidy`'s `cppcoreguidelines-*`/`bugprone-*` gate |
| `cargo` | `vcpkg.json` manifest mode + `CMakePresets.json`, both pinned, both driven by `scripts/deoxidene.sh`/`.ps1` |
| `cargo new` / `build` / `run` | `deoxidene.sh new` / `build` / `run` |
| `cargo add <crate>` | `deoxidene.sh add <package>` appends to `vcpkg.json` |
| `cargo test` + Miri-class UB detection | `deoxidene.sh test-sanitize` (ASan + UBSan preset), gated in CI |
| `rustup target add` + cross-compile | CMakePresets triplets for x64/arm64 across Linux/Windows/macOS |
| `cargo build --release` publishing to crates.io | `.github/workflows/release.yml`: tag push cross-builds every target, uploads as GitHub Release assets |

## Quickstart

Windows (PowerShell, no WSL/git-bash required):

```powershell
git clone https://github.com/AnEntrypoint/deoxidene
cd deoxidene
.\scripts\deoxidene.ps1 run
```

Linux / macOS:

```bash
git clone https://github.com/AnEntrypoint/deoxidene
cd deoxidene
./scripts/deoxidene.sh run
```

Scaffold a new project from this template:

```powershell
.\scripts\deoxidene.ps1 new my-project
cd my-project
..\deoxidene\scripts\deoxidene.ps1 run
```

Add a vcpkg dependency:

```powershell
.\scripts\deoxidene.ps1 add fmt
```

Run under AddressSanitizer + UndefinedBehaviorSanitizer:

```powershell
.\scripts\deoxidene.ps1 test-sanitize
```

Run clang-tidy (C++ Core Guidelines, treated as the borrow-checker-equivalent static gate):

```powershell
.\scripts\deoxidene.ps1 tidy
```

## Target matrix

Native release presets: `x64-linux-release`, `arm64-linux-release`, `x64-windows-release`, `arm64-windows-release`, `x64-macos-release`, `arm64-macos-release`.

Sanitizer presets (native, non-MSVC only): `asan`, `ubsan`, `asan-ubsan`.

Every `deoxidene.sh`/`.ps1` build command checks `VCPKG_ROOT` up front and prints a specific, actionable fix if either is missing, instead of surfacing CMake's/vcpkg's own generic toolchain error.

All presets are declared in `CMakePresets.json`; `cmake --list-presets` enumerates them.

## Native performance profile

The `native-perf` preset builds a native Release for the build host with link-time optimization, `-march=native` tuning and the benchmark target. `./scripts/deoxidene.sh bench` (or `.\scripts\deoxidene.ps1 bench`) builds it and runs `deoxidene_bench`: it times `deoxidene::Result` parsing against a plain bool-returning parser, prints ns/op for each, and exits non-zero if their results differ.

- `DEOXIDENE_NATIVE_TUNE=ON` compiles for the build host's CPU. Those binaries do not run on other CPUs.
- `DEOXIDENE_LTO=ON` enables link-time optimization for deoxidene executables when the toolchain supports it.
- `DEOXIDENE_BUILD_BENCH=ON` builds the benchmarks in `bench/`.

The other presets leave all three off. Benchmark numbers depend on the machine, compiler and system load, so compare runs from the same host only. Measured with `deoxidene.sh bench` on one x86-64 Linux host (g++ 15.2), best of 3 runs of 7 repeats each, in ns per parse:

| Build | raw | `Result` |
| --- | --- | --- |
| Release, `-O2 -g` (`RelWithDebInfo`) | 6.18 | 6.74 |
| `-O3` | 6.19 | 6.96 |
| `-O3 -march=native` | 6.18 | 6.30 |
| `-O3` with LTO | 6.34 | 6.81 |
| `native-perf` (`-O3`, LTO, `-march=native`) | 5.71 | 6.03 |
| `native-perf` with PGO (`deoxidene.sh pgo`) | 4.71 | 4.81 |

PGO here is trained on the same benchmark it is measured on, so the last row is in-sample. For a real application, profile your own representative workload.

## Library selection

Each dependency either replaces hand-written code or a slower default:

| Library | Used for | Why |
| --- | --- | --- |
| Microsoft GSL | `owner`, `not_null`, `span` | Header-only aliases with no runtime cost |
| `std::expected` (tl-expected fallback) | `Result<T, E>` | Error flow without exceptions |
| `std::from_chars` | `parse_positive_int` | Standard and locale-free; shorter than a hand-written digit loop and measured no slower |
| Boost.Container | `deoxidene::SmallVec<T, N>` | Stores up to N elements inline, so small short-lived collections never touch the heap |
| mimalloc | process allocator (`DEOXIDENE_MIMALLOC=ON`, set by `native-perf`) | Faster allocation under churn, with no code change |

Measured with `deoxidene.sh bench` on one x86-64 Linux host (g++ 15.2), `native-perf` with mimalloc linked, best of 3 runs of 7 repeats, in ns/op:

| Benchmark | ns/op |
| --- | --- |
| parse, raw bool parser | 5.61 |
| parse, `Result` (`from_chars`) | 4.73 |
| collect 4 ints, `std::vector` with `reserve(4)` | 6.95 |
| collect 4 ints, `SmallVec<int, 8>` | 1.34 |

- `SmallVec` avoids the heap only while the collection fits in N. Use `std::vector` for collections that can grow past it.
- The collection benchmarks force each container into memory before reading it. Without that, the optimizer removed `SmallVec`'s storage and the numbers did not measure allocation.
- Keep mimalloc off in sanitizer builds: it replaces the allocator that ASan instruments. The `asan` and `asan-ubsan` presets leave it off.
- mimalloc is verified on Linux only; the Windows and macOS presets do not set `DEOXIDENE_MIMALLOC`.
- The flag table above was measured before the `from_chars` parser and the mimalloc preset, so its numbers are not comparable to this section.

## Install the agent skill

Copy `skill/deoxidene/SKILL.md` into `~/.claude/skills/deoxidene/` (or your agent's skill directory) to teach an AI agent the invariants: always `Result<T,E>` not exceptions, always `gsl::` wrappers not raw pointers, always run `tidy` + `test-sanitize` before declaring a change done, always drive builds through `deoxidene.sh`, never raw `cmake`.

## License

Dual-licensed under MIT (`LICENSE-MIT`) or Apache-2.0 (`LICENSE-APACHE`), your choice — matching Rust's own convention.
