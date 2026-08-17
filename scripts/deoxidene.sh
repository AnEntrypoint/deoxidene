#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    cat <<'EOF'
deoxidene - cargo-equivalent wrapper over CMakePresets + vcpkg

Usage:
  deoxidene.sh new <name>          Scaffold a new consumer project from this template
  deoxidene.sh build [preset]      Configure + build (default preset: release)
  deoxidene.sh run [preset] [args] Build then run the hello example
  deoxidene.sh test-sanitize       Build + run under asan-ubsan preset
  deoxidene.sh tidy                Run clang-tidy over include/
EOF
}

cmd_new() {
    local name="${1:?usage: deoxidene.sh new <name>}"
    mkdir -p "$name"
    cp -r "$REPO_ROOT"/{CMakeLists.txt,CMakePresets.json,vcpkg.json,vcpkg-configuration.json,.clang-tidy,cmake,vcpkg-triplets,include} "$name"/
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

cmd_test_sanitize() {
    cmd_build asan-ubsan
    "$REPO_ROOT/build/asan-ubsan/examples/hello/deoxidene_hello"
}

cmd_tidy() {
    cmd_build debug
    clang-tidy -p "$REPO_ROOT/build/debug" "$REPO_ROOT"/include/deoxidene/*.hpp
}

case "${1:-}" in
    new) shift; cmd_new "$@" ;;
    build) shift; cmd_build "$@" ;;
    run) shift; cmd_run "$@" ;;
    test-sanitize) cmd_test_sanitize ;;
    tidy) cmd_tidy ;;
    *) usage; exit 1 ;;
esac
