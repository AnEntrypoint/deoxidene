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

The other presets leave all three off. Benchmark numbers depend on the machine, compiler and system load, so compare runs from the same host only.

## Install the agent skill

Copy `skill/deoxidene/SKILL.md` into `~/.claude/skills/deoxidene/` (or your agent's skill directory) to teach an AI agent the invariants: always `Result<T,E>` not exceptions, always `gsl::` wrappers not raw pointers, always run `tidy` + `test-sanitize` before declaring a change done, always drive builds through `deoxidene.sh`, never raw `cmake`.

## License

Dual-licensed under MIT (`LICENSE-MIT`) or Apache-2.0 (`LICENSE-APACHE`), your choice — matching Rust's own convention.
