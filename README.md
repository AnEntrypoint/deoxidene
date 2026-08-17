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
| `cargo test` + Miri-class UB detection | `deoxidene.sh test-sanitize` (ASan + UBSan preset), gated in CI |
| `rustup target add` + cross-compile | CMakePresets triplets for x64/arm64 across Linux/Windows/macOS, plus a `wasm32-wasip1` toolchain + vcpkg overlay triplet |
| `cargo build --release` publishing to crates.io | `.github/workflows/release.yml`: tag push cross-builds every target + wasip1, uploads as GitHub Release assets |

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

WebAssembly: `wasip1-release` (requires `WASI_SDK_PREFIX` pointing at a wasi-sdk install), producing a `.wasm` module runnable under wasmtime.

Sanitizer presets (native, non-wasip1, non-MSVC only): `asan`, `ubsan`, `asan-ubsan`.

All presets are declared in `CMakePresets.json`; `cmake --list-presets` enumerates them.

## Install the agent skill

Copy `skill/deoxidene/SKILL.md` into `~/.claude/skills/deoxidene/` (or your agent's skill directory) to teach an AI agent the invariants: always `Result<T,E>` not exceptions, always `gsl::` wrappers not raw pointers, always run `tidy` + `test-sanitize` before declaring a change done, always drive builds through `deoxidene.sh`, never raw `cmake`.

## License

Dual-licensed under MIT (`LICENSE-MIT`) or Apache-2.0 (`LICENSE-APACHE`), your choice — matching Rust's own convention.
