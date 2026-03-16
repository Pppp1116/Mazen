#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAZEN_BIN="$ROOT_DIR/mazen"

fail() {
  echo "[FAIL] $*" >&2
  exit 1
}

note() {
  echo "[INFO] $*"
}

assert_file() {
  local path="$1"
  [[ -f "$path" ]] || fail "expected file to exist: $path"
}

assert_not_exists() {
  local path="$1"
  [[ ! -e "$path" ]] || fail "expected path to be absent: $path"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "$haystack" != *"$needle"* ]]; then
    fail "expected output to contain '$needle'"
  fi
}

run_cmd() {
  local cwd="$1"
  shift
  (cd "$cwd" && "$@")
}

run_capture() {
  local cwd="$1"
  shift
  (cd "$cwd" && "$@" 2>&1)
}

note "Building MAZEN binary"
make -C "$ROOT_DIR" -j4
[[ -x "$MAZEN_BIN" ]] || fail "expected executable binary after build: $MAZEN_BIN"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

note "Test 1: parser rejects unsupported c_standard"
PARSER_BAD="$TMP_ROOT/parser_bad"
mkdir -p "$PARSER_BAD/src"
cat > "$PARSER_BAD/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$PARSER_BAD/mazen.toml" <<'TOML'
name = "bad"
c_standard = "c77"
TOML
set +e
OUT="$(run_capture "$PARSER_BAD" "$MAZEN_BIN" doctor)"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "unsupported c_standard should fail"
assert_contains "$OUT" "unsupported c_standard"

note "Test 2: graph planning and build across target dependencies"
GRAPH_OK="$TMP_ROOT/graph_ok"
mkdir -p "$GRAPH_OK/src" "$GRAPH_OK/include/proj"
cat > "$GRAPH_OK/include/proj/math.h" <<'HDR'
#ifndef PROJ_MATH_H
#define PROJ_MATH_H
int add(int a, int b);
#endif
HDR
cat > "$GRAPH_OK/src/math.c" <<'SRC'
#include "proj/math.h"
int add(int a, int b) { return a + b; }
SRC
cat > "$GRAPH_OK/src/main.c" <<'SRC'
#include <stdio.h>
#include "proj/math.h"
int main(void) {
  printf("%d\n", add(2, 3));
  return 0;
}
SRC
cat > "$GRAPH_OK/mazen.toml" <<'TOML'
name = "graph"
install_headers = ["include/proj/math.h"]

[target.core]
type = "static"
sources = ["src/math.c"]
include_dirs = ["include"]

[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c"]
include_dirs = ["include"]
deps = ["core"]
default = true
TOML
run_cmd "$GRAPH_OK" "$MAZEN_BIN" --all-targets
assert_file "$GRAPH_OK/build/libcore.a"
assert_file "$GRAPH_OK/build/app"

note "Test 3: cycle detection in target graph"
GRAPH_CYCLE="$TMP_ROOT/graph_cycle"
mkdir -p "$GRAPH_CYCLE/src"
cat > "$GRAPH_CYCLE/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$GRAPH_CYCLE/mazen.toml" <<'TOML'
[target.a]
type = "static"
sources = ["src/main.c"]
deps = ["b"]

[target.b]
type = "static"
sources = ["src/main.c"]
deps = ["a"]
TOML
set +e
OUT="$(run_capture "$GRAPH_CYCLE" "$MAZEN_BIN" --all-targets)"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "cycle in graph should fail"
assert_contains "$OUT" "target dependency cycle detected"

note "Test 4: incremental build is stable and run works"
BUILD_APP="$TMP_ROOT/build_app"
mkdir -p "$BUILD_APP/src"
cat > "$BUILD_APP/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
run_cmd "$BUILD_APP" "$MAZEN_BIN"
OUT="$(run_capture "$BUILD_APP" "$MAZEN_BIN")"
assert_contains "$OUT" "Target is up to date"
run_cmd "$BUILD_APP" "$MAZEN_BIN" run
assert_file "$BUILD_APP/compile_commands.json"

note "Test 5: install and uninstall target + headers"
PREFIX="$TMP_ROOT/prefix"
run_cmd "$GRAPH_OK" "$MAZEN_BIN" install --prefix "$PREFIX"
assert_file "$PREFIX/bin/app"
assert_file "$PREFIX/include/proj/math.h"
run_cmd "$GRAPH_OK" "$MAZEN_BIN" uninstall --prefix "$PREFIX"
assert_not_exists "$PREFIX/bin/app"
assert_not_exists "$PREFIX/include/proj/math.h"

note "Test 6: example projects build and run"
cp -R "$ROOT_DIR/examples/simple" "$TMP_ROOT/example_simple"
cp -R "$ROOT_DIR/examples/organized" "$TMP_ROOT/example_organized"
OUT="$(run_capture "$TMP_ROOT/example_simple" "$ROOT_DIR/mazen" run)"
assert_contains "$OUT" "simple example: hello from MAZEN"
run_cmd "$TMP_ROOT/example_organized" "$ROOT_DIR/mazen"
assert_file "$TMP_ROOT/example_organized/build/example_organized"

note "All regression checks passed"
