#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    cat <<'EOF'
deoxidene - cargo-equivalent wrapper over CMakePresets

Usage:
  deoxidene.sh new <name>          Scaffold a new consumer project from this template
  deoxidene.sh build [preset]      Configure + build (default preset: release)
  deoxidene.sh run [preset] [args] Build then run the hello example
  deoxidene.sh test [preset]       Build then run the CTest suite (default preset: debug)
  deoxidene.sh test-sanitize       Build + run under asan-ubsan preset
  deoxidene.sh tidy                Run clang-tidy over include/
  deoxidene.sh bench               Build native-perf (LTO, host CPU tuning) and run the benchmark
  deoxidene.sh pgo                 Profile-guided build of native-perf, trained on the benchmark (GCC only)
EOF
}

cmd_new() {
    local name="${1:?usage: deoxidene.sh new <name>}"
    mkdir -p "$name"
    cp -r "$REPO_ROOT"/{CMakeLists.txt,CMakePresets.json,.clang-tidy,include,third_party,tests} "$name"/
    mkdir -p "$name/src"
    echo "Scaffolded $name from deoxidene template."
}

cmd_build() {
    local preset="${1:-release}"
    cmake --preset "$preset" -S "$REPO_ROOT" -B "$REPO_ROOT/build/$preset"
    cmake --build "$REPO_ROOT/build/$preset"
}

cmd_run() {
    local preset="${1:-release}"
    shift || true
    cmd_build "$preset"
    "$REPO_ROOT/build/$preset/examples/hello/deoxidene_hello" "$@"
}

cmd_test() {
    local preset="${1:-debug}"
    cmd_build "$preset"
    ctest --test-dir "$REPO_ROOT/build/$preset" --output-on-failure
}

cmd_test_sanitize() {
    cmd_build asan-ubsan
    "$REPO_ROOT/build/asan-ubsan/examples/hello/deoxidene_hello"
}

cmd_bench() {
    cmd_build native-perf
    "$REPO_ROOT/build/native-perf/bench/deoxidene_bench"
}

cmd_pgo() {
    local profile="$REPO_ROOT/build/pgo-profile"
    local dir="$REPO_ROOT/build/pgo"
    rm -rf "$profile" "$dir"
    cmake --preset native-perf -S "$REPO_ROOT" -B "$dir" -DCMAKE_CXX_FLAGS="-fprofile-generate=$profile"
    cmake --build "$dir" --target deoxidene_bench
    "$dir/bench/deoxidene_bench" > /dev/null
    cmake --preset native-perf -S "$REPO_ROOT" -B "$dir" -DCMAKE_CXX_FLAGS="-fprofile-use=$profile -fprofile-partial-training -Wno-missing-profile"
    cmake --build "$dir" --target deoxidene_bench
    "$dir/bench/deoxidene_bench"
}

cmd_tidy() {
    cmd_build debug
    clang-tidy -p "$REPO_ROOT/build/debug" "$REPO_ROOT"/include/deoxidene/*.hpp --warnings-as-errors='*'
}

case "${1:-}" in
    new) shift; cmd_new "$@" ;;
    build) shift; cmd_build "$@" ;;
    run) shift; cmd_run "$@" ;;
    test) shift; cmd_test "$@" ;;
    test-sanitize) cmd_test_sanitize ;;
    tidy) cmd_tidy ;;
    bench) cmd_bench ;;
    pgo) cmd_pgo ;;
    *) usage; exit 1 ;;
esac
