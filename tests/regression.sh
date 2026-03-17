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

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "$haystack" == *"$needle"* ]]; then
    fail "expected output to not contain '$needle'"
  fi
}


assert_equals() {
  local lhs="$1"
  local rhs="$2"
  if [[ "$lhs" != "$rhs" ]]; then
    fail "expected [$lhs] == [$rhs]"
  fi
}



if command -v python3 >/dev/null 2>&1; then
  PYTHON3_BIN="python3"
elif command -v python >/dev/null 2>&1 && python - <<'PY2' >/dev/null 2>&1
import sys
raise SystemExit(0 if sys.version_info[0] == 3 else 1)
PY2
then
  PYTHON3_BIN="python"
else
  fail "python3 is required for regression fixtures (python 3 interpreter not found)"
fi

sha256_file() {
  local path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$path" | awk '{print $1}'
    return
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$path" | awk '{print $1}'
    return
  fi
  if command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 "$path" | awk '{print $NF}'
    return
  fi
  if command -v python3 >/dev/null 2>&1; then
    python3 - <<'PY2' "$path"
import hashlib, sys
with open(sys.argv[1], 'rb') as f:
    print(hashlib.sha256(f.read()).hexdigest())
PY2
    return
  fi
  if command -v python >/dev/null 2>&1; then
    python - <<'PY2' "$path"
import hashlib, sys
with open(sys.argv[1], 'rb') as f:
    print(hashlib.sha256(f.read()).hexdigest())
PY2
    return
  fi
  fail "no sha256 tool found (tried sha256sum, shasum, openssl, python3, python)"
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

note "Test 5: autolib infers libraries from symbol usage"
AUTO_LIB_SYMBOL="$TMP_ROOT/auto_lib_symbol"
mkdir -p "$AUTO_LIB_SYMBOL/src"
cat > "$AUTO_LIB_SYMBOL/src/main.c" <<'SRC'
double sqrt(double);
int main(void) {
  return sqrt(4.0) == 2.0 ? 0 : 1;
}
SRC
OUT="$(run_capture "$AUTO_LIB_SYMBOL" "$MAZEN_BIN")"
assert_file "$AUTO_LIB_SYMBOL/build/auto_lib_symbol"

note "Test 6: autolib ignores project-local functions with library-like names"
AUTO_LIB_LOCAL="$TMP_ROOT/auto_lib_local"
mkdir -p "$AUTO_LIB_LOCAL/src"
cat > "$AUTO_LIB_LOCAL/src/config.c" <<'SRC'
void config_init(void) {}
SRC
cat > "$AUTO_LIB_LOCAL/src/main.c" <<'SRC'
void config_init(void);
int main(void) {
  config_init();
  return 0;
}
SRC
OUT="$(run_capture "$AUTO_LIB_LOCAL" "$MAZEN_BIN")"
assert_not_contains "$OUT" "Auto-detected libraries: config"
assert_file "$AUTO_LIB_LOCAL/build/auto_lib_local"

note "Test 7: install and uninstall target + headers"
PREFIX="$TMP_ROOT/prefix"
run_cmd "$GRAPH_OK" "$MAZEN_BIN" install --prefix "$PREFIX"
assert_file "$PREFIX/bin/app"
assert_file "$PREFIX/include/proj/math.h"
run_cmd "$GRAPH_OK" "$MAZEN_BIN" uninstall --prefix "$PREFIX"
assert_not_exists "$PREFIX/bin/app"
assert_not_exists "$PREFIX/include/proj/math.h"

note "Test 8: example projects build and run"
cp -R "$ROOT_DIR/examples/simple" "$TMP_ROOT/example_simple"
cp -R "$ROOT_DIR/examples/organized" "$TMP_ROOT/example_organized"
OUT="$(run_capture "$TMP_ROOT/example_simple" "$ROOT_DIR/mazen" run)"
assert_contains "$OUT" "simple example: hello from MAZEN"
run_cmd "$TMP_ROOT/example_organized" "$ROOT_DIR/mazen"
assert_file "$TMP_ROOT/example_organized/build/example_organized"


note "Test 9: auto-lib cache invalidates on content change even when mtime is restored"
AUTO_LIB_HASH="$TMP_ROOT/auto_lib_hash"
mkdir -p "$AUTO_LIB_HASH/src"
cat > "$AUTO_LIB_HASH/src/main.c" <<'SRC'
double sqrt(double);
int main(void) {
  return sqrt(9.0) == 3.0 ? 0 : 1;
}
SRC
OUT="$(run_capture "$AUTO_LIB_HASH" "$MAZEN_BIN")"
ORIG_NS="$($PYTHON3_BIN - <<'PY2' "$AUTO_LIB_HASH/src/main.c"
import os, sys
print(os.stat(sys.argv[1]).st_mtime_ns)
PY2
)"
cat > "$AUTO_LIB_HASH/src/main.c" <<'SRC'
int main(void) {
  return 0;
}
SRC
$PYTHON3_BIN - <<'PY2' "$AUTO_LIB_HASH/src/main.c" "$ORIG_NS"
import os, sys
path = sys.argv[1]
ns = int(sys.argv[2])
os.utime(path, ns=(ns, ns))
PY2
run_cmd "$AUTO_LIB_HASH" "$MAZEN_BIN" clean objects
OUT="$(run_capture "$AUTO_LIB_HASH" "$MAZEN_BIN")"
assert_not_contains "$OUT" "Auto-detected libraries: m"

note "Test 10: low-confidence autolib mode can be enabled explicitly"
AUTO_LIB_LOW="$TMP_ROOT/auto_lib_low"
mkdir -p "$AUTO_LIB_LOW/src"
cat > "$AUTO_LIB_LOW/src/main.c" <<'SRC'
#include <stdio.h>
int pthread_mutex_doesnotexist(void);
int main(void) {
  printf("%d\n", pthread_mutex_doesnotexist());
  return 0;
}
SRC
set +e
OUT="$(run_capture "$AUTO_LIB_LOW" env MAZEN_AUTOLIB_LOW_CONFIDENCE=1 "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "expected unresolved symbol test to fail"
assert_contains "$OUT" "Auto-detected libraries: pthread"
assert_contains "$OUT" "linker failed"


note "Test 11: auto-lib cache must not be reused across confidence-mode changes"
AUTO_LIB_MODE="$TMP_ROOT/auto_lib_mode"
mkdir -p "$AUTO_LIB_MODE/src"
cat > "$AUTO_LIB_MODE/src/main.c" <<'SRC'
int pthread_mutex_lock(void *);
int main(void) {
  return pthread_mutex_lock(0);
}
SRC
OUT="$(run_capture "$AUTO_LIB_MODE" env MAZEN_AUTOLIB_LOW_CONFIDENCE=1 "$MAZEN_BIN")"
assert_contains "$OUT" "Auto-detected libraries: pthread"
assert_file "$AUTO_LIB_MODE/build/auto_lib_mode"
OUT="$(run_capture "$AUTO_LIB_MODE" "$MAZEN_BIN")"
assert_not_contains "$OUT" "Auto-detected libraries: pthread"


note "Test 12: complex dependency graph, mixed target types, and cache stability"
COMPLEX="$TMP_ROOT/complex_graph"
mkdir -p "$COMPLEX/core" "$COMPLEX/engine" "$COMPLEX/app" "$COMPLEX/include/proj"
cat > "$COMPLEX/include/proj/core.h" <<'HDR'
#ifndef PROJ_CORE_H
#define PROJ_CORE_H
double core_metric(double x);
#endif
HDR
cat > "$COMPLEX/include/proj/engine.h" <<'HDR'
#ifndef PROJ_ENGINE_H
#define PROJ_ENGINE_H
double engine_eval(double x);
#endif
HDR
cat > "$COMPLEX/core/core.c" <<'SRC'
#include <math.h>
#include "proj/core.h"
double core_metric(double x) {
  return sqrt(x * x + 4.0);
}
SRC
cat > "$COMPLEX/engine/engine.c" <<'SRC'
#include "proj/core.h"
#include "proj/engine.h"
double engine_eval(double x) {
  return core_metric(x) + 1.0;
}
SRC
cat > "$COMPLEX/app/main.c" <<'SRC'
#include <stdio.h>
#include "proj/engine.h"
int main(void) {
  double v = engine_eval(3.0);
  if (v < 4.5 || v > 4.8) {
    return 2;
  }
  printf("ok %.2f\n", v);
  return 0;
}
SRC
cat > "$COMPLEX/mazen.toml" <<'TOML'
name = "complex"
jobs = 4
include_dirs = ["include"]

[target.core]
type = "static"
sources = ["core/core.c"]
include_dirs = ["include"]

[target.engine]
type = "shared"
sources = ["engine/engine.c"]
include_dirs = ["include"]
deps = ["core"]
libs = ["m"]

[target.app]
type = "executable"
entry = "app/main.c"
sources = ["app/main.c"]
include_dirs = ["include"]
deps = ["engine"]
default = true
TOML
OUT="$(run_capture "$COMPLEX" "$MAZEN_BIN" --all-targets -j 4)"
assert_file "$COMPLEX/build/libcore.a"
assert_file "$COMPLEX/build/libengine.so"
assert_file "$COMPLEX/build/app"
OUT="$(run_capture "$COMPLEX" env LD_LIBRARY_PATH="$COMPLEX/build" ./build/app)"
assert_contains "$OUT" "ok"
OUT="$(run_capture "$COMPLEX" "$MAZEN_BIN" --all-targets -j 4)"
assert_contains "$OUT" "Target is up to date"


note "Test 13: missing configured source reports clear failure"
MISS_SRC="$TMP_ROOT/missing_source"
mkdir -p "$MISS_SRC/src"
cat > "$MISS_SRC/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$MISS_SRC/mazen.toml" <<'TOML'
[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c", "src/does_not_exist.c"]
default = true
TOML
set +e
OUT="$(run_capture "$MISS_SRC" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "missing configured source should fail"
assert_contains "$OUT" "does_not_exist.c"

note "Test 14: nonexistent dependency target is rejected"
MISS_DEP="$TMP_ROOT/missing_dep"
mkdir -p "$MISS_DEP/src"
cat > "$MISS_DEP/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$MISS_DEP/mazen.toml" <<'TOML'
[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c"]
deps = ["not_a_target"]
default = true
TOML
set +e
OUT="$(run_capture "$MISS_DEP" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "nonexistent dependency target should fail"
assert_contains "$OUT" "not_a_target"

note "Test 15: compile invalidation layers (.c/.h/flags/depfile)"
INVALIDATE="$TMP_ROOT/invalidation"
mkdir -p "$INVALIDATE/src"
cat > "$INVALIDATE/src/add.h" <<'SRC'
int add(int a, int b);
SRC
cat > "$INVALIDATE/src/add.c" <<'SRC'
#include "add.h"
int add(int a, int b) { return a + b; }
SRC
cat > "$INVALIDATE/src/main.c" <<'SRC'
#include "add.h"
int main(void) { return add(1, 2) == 3 ? 0 : 1; }
SRC
run_cmd "$INVALIDATE" "$MAZEN_BIN"
OUT="$(run_capture "$INVALIDATE" "$MAZEN_BIN")"
assert_contains "$OUT" "Target is up to date"
cat > "$INVALIDATE/src/add.c" <<'SRC'
#include "add.h"
int add(int a, int b) { return a + b + 1; }
SRC
OUT="$(run_capture "$INVALIDATE" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/add.c"
assert_not_contains "$OUT" "Compiling src/main.c"
cat > "$INVALIDATE/src/add.h" <<'SRC'
int add(int a, int b);
int mul2(int v);
SRC
cat > "$INVALIDATE/src/add.c" <<'SRC'
#include "add.h"
int add(int a, int b) { return a + b; }
int mul2(int v) { return v * 2; }
SRC
cat > "$INVALIDATE/src/main.c" <<'SRC'
#include "add.h"
int main(void) { return mul2(add(1, 2)) == 6 ? 0 : 1; }
SRC
OUT="$(run_capture "$INVALIDATE" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/add.c"
assert_contains "$OUT" "Compiling src/main.c"
OUT="$(run_capture "$INVALIDATE" "$MAZEN_BIN" --std c99)"
assert_contains "$OUT" "Compiler flags changed, invalidating object cache"
DEPFILE="$INVALIDATE/build/obj/invalidation/src_add.d"
[[ -f "$DEPFILE" ]] || fail "expected depfile to exist before deletion"
rm -f "$DEPFILE"
OUT="$(run_capture "$INVALIDATE" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/add.c"

note "Test 16: duplicate symbol link failure is surfaced"
DUPSYM="$TMP_ROOT/dupsym"
mkdir -p "$DUPSYM/src"
cat > "$DUPSYM/src/a.c" <<'SRC'
int clash(void) { return 1; }
SRC
cat > "$DUPSYM/src/b.c" <<'SRC'
int clash(void) { return 2; }
SRC
cat > "$DUPSYM/src/main.c" <<'SRC'
int clash(void);
int main(void) { return clash() == 1 ? 0 : 1; }
SRC
set +e
OUT="$(run_capture "$DUPSYM" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "duplicate symbol should fail link"
assert_contains "$OUT" "linker failed"

note "Test 17: string/comment symbol text should not trigger auto-lib"
AUTO_STR="$TMP_ROOT/auto_lib_string"
mkdir -p "$AUTO_STR/src"
cat > "$AUTO_STR/src/main.c" <<'SRC'
/* sqrt should not trigger math linking from comments */
const char *s = "sqrt";
int main(void) { return s[0] == 's' ? 0 : 1; }
SRC
OUT="$(run_capture "$AUTO_STR" "$MAZEN_BIN")"
assert_not_contains "$OUT" "Auto-detected libraries: m"

note "Test 18: parallel builds produce identical executable artifacts"
PAR="$TMP_ROOT/parallel_eq"
mkdir -p "$PAR/src"
for i in $(seq 1 18); do
  cat > "$PAR/src/f$i.c" <<SRC
int f$i(void) { return $i; }
SRC
done
cat > "$PAR/src/main.c" <<'SRC'
int f1(void); int f2(void); int f3(void); int f4(void); int f5(void); int f6(void); int f7(void); int f8(void); int f9(void);
int f10(void); int f11(void); int f12(void); int f13(void); int f14(void); int f15(void); int f16(void); int f17(void); int f18(void);
int main(void) { return (f1()+f2()+f3()+f4()+f5()+f6()+f7()+f8()+f9()+f10()+f11()+f12()+f13()+f14()+f15()+f16()+f17()+f18()) == 171 ? 0 : 1; }
SRC
run_cmd "$PAR" "$MAZEN_BIN" -j 1
SHA1_A="$(sha256_file "$PAR/build/parallel_eq")"
run_cmd "$PAR" "$MAZEN_BIN" clean objects
run_cmd "$PAR" "$MAZEN_BIN" -j 8
SHA1_B="$(sha256_file "$PAR/build/parallel_eq")"
assert_equals "$SHA1_A" "$SHA1_B"

note "Test 19: compile_commands.json includes all source files"
COMPDB="$TMP_ROOT/compdb_case"
mkdir -p "$COMPDB/src"
cat > "$COMPDB/src/a.c" <<'SRC'
int a(void) { return 1; }
SRC
cat > "$COMPDB/src/b.c" <<'SRC'
int a(void);
int main(void) { return a() == 1 ? 0 : 1; }
SRC
run_cmd "$COMPDB" "$MAZEN_BIN" --std c11
$PYTHON3_BIN - <<'PY2' "$COMPDB/compile_commands.json"
import json, sys
p=sys.argv[1]
with open(p,'r',encoding='utf-8') as f:
    data=json.load(f)
files={item['file'] for item in data}
if len(data) != 2:
    raise SystemExit(f'expected 2 compile commands, got {len(data)}')
for suffix in ('/src/a.c','/src/b.c'):
    if not any(f.endswith(suffix) for f in files):
        raise SystemExit(f'missing {suffix} in compile_commands')
for item in data:
    if '-std=c11' not in item.get('command',''):
        raise SystemExit('missing -std=c11 in compile command')
PY2

note "Test 20: syntax errors report compile failure with filename"
SYNTAX="$TMP_ROOT/syntax_err"
mkdir -p "$SYNTAX/src"
cat > "$SYNTAX/src/main.c" <<'SRC'
int main(void) { return ; }
SRC
set +e
OUT="$(run_capture "$SYNTAX" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "syntax error should fail"
assert_contains "$OUT" "main.c"
assert_contains "$OUT" "compilation failed"

note "Test 21: same-second multi-edit invalidates on each content change"
SAMESEC="$TMP_ROOT/same_second"
mkdir -p "$SAMESEC/src"
cat > "$SAMESEC/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
run_cmd "$SAMESEC" "$MAZEN_BIN"
BASE_SEC="$($PYTHON3_BIN - <<'PY2' "$SAMESEC/src/main.c"
import os, sys
print((os.stat(sys.argv[1]).st_mtime_ns // 1_000_000_000) + 5)
PY2
)"
cat > "$SAMESEC/src/main.c" <<'SRC'
int main(void) { return 1; }
SRC
$PYTHON3_BIN - <<'PY2' "$SAMESEC/src/main.c" "$BASE_SEC" 100
import os, sys
path = sys.argv[1]
sec = int(sys.argv[2])
nsec = int(sys.argv[3])
os.utime(path, ns=(sec * 1_000_000_000 + nsec, sec * 1_000_000_000 + nsec))
PY2
OUT="$(run_capture "$SAMESEC" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/main.c"
cat > "$SAMESEC/src/main.c" <<'SRC'
int main(void) { return 2; }
SRC
$PYTHON3_BIN - <<'PY2' "$SAMESEC/src/main.c" "$BASE_SEC" 200
import os, sys
path = sys.argv[1]
sec = int(sys.argv[2])
nsec = int(sys.argv[3])
os.utime(path, ns=(sec * 1_000_000_000 + nsec, sec * 1_000_000_000 + nsec))
PY2
OUT="$(run_capture "$SAMESEC" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/main.c"

note "Test 22: transitive header changes invalidate dependent sources"
TRANSITIVE="$TMP_ROOT/transitive_header"
mkdir -p "$TRANSITIVE/src"
cat > "$TRANSITIVE/src/b.h" <<'SRC'
#define SCALE_FACTOR 3
SRC
cat > "$TRANSITIVE/src/a.h" <<'SRC'
#include "b.h"
int scale(int x);
SRC
cat > "$TRANSITIVE/src/scale.c" <<'SRC'
#include "a.h"
int scale(int x) { return x * SCALE_FACTOR; }
SRC
cat > "$TRANSITIVE/src/main.c" <<'SRC'
#include "a.h"
int main(void) { return scale(2) == 6 ? 0 : 1; }
SRC
run_cmd "$TRANSITIVE" "$MAZEN_BIN"
cat > "$TRANSITIVE/src/b.h" <<'SRC'
#define SCALE_FACTOR 4
SRC
OUT="$(run_capture "$TRANSITIVE" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/scale.c"
assert_contains "$OUT" "Compiling src/main.c"

note "Test 23: auto-lib cache transitions (fresh skip + removal drop)"
AUTO_CACHE="$TMP_ROOT/auto_cache_transitions"
mkdir -p "$AUTO_CACHE/src"
cat > "$AUTO_CACHE/src/main.c" <<'SRC'
double sqrt(double);
int main(void) { return sqrt(25.0) == 5.0 ? 0 : 1; }
SRC
OUT="$(run_capture "$AUTO_CACHE" "$MAZEN_BIN")"
assert_contains "$OUT" "Auto-detected libraries: m"
OUT="$(run_capture "$AUTO_CACHE" "$MAZEN_BIN")"
assert_contains "$OUT" "Target is up to date"
cat > "$AUTO_CACHE/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
run_cmd "$AUTO_CACHE" "$MAZEN_BIN" clean objects
OUT="$(run_capture "$AUTO_CACHE" "$MAZEN_BIN")"
assert_not_contains "$OUT" "Auto-detected libraries: m"

note "Test 24: selective rebuild only recompiles affected target sources"
SELECTIVE="$TMP_ROOT/selective_rebuild"
mkdir -p "$SELECTIVE/src"
cat > "$SELECTIVE/src/app1.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$SELECTIVE/src/app2.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$SELECTIVE/mazen.toml" <<'TOML'
[target.app1]
type = "executable"
entry = "src/app1.c"
sources = ["src/app1.c"]
default = true

[target.app2]
type = "executable"
entry = "src/app2.c"
sources = ["src/app2.c"]
TOML
run_cmd "$SELECTIVE" "$MAZEN_BIN" --all-targets
cat > "$SELECTIVE/src/app1.c" <<'SRC'
int main(void) { return 1; }
SRC
OUT="$(run_capture "$SELECTIVE" "$MAZEN_BIN" --all-targets)"
assert_contains "$OUT" "Compiling src/app1.c"
assert_not_contains "$OUT" "Compiling src/app2.c"

note "Test 25: linker failure edge cases are reported correctly"
RETRY_FAIL="$TMP_ROOT/retry_fail"
mkdir -p "$RETRY_FAIL/src"
cat > "$RETRY_FAIL/src/main.c" <<'SRC'
int pthread_mutex_doesnotexist(void);
int main(void) { return pthread_mutex_doesnotexist(); }
SRC
set +e
OUT="$(run_capture "$RETRY_FAIL" env MAZEN_AUTOLIB_LOW_CONFIDENCE=1 "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "retry-fail case should fail"
assert_contains "$OUT" "Auto-detected libraries: pthread"
assert_contains "$OUT" "linker failed"

RETRY_NONLIB="$TMP_ROOT/retry_nonlib"
mkdir -p "$RETRY_NONLIB/src"
cat > "$RETRY_NONLIB/src/main.c" <<'SRC'
int missing_symbol_which_has_no_known_lib(void);
int main(void) { return missing_symbol_which_has_no_known_lib(); }
SRC
set +e
OUT="$(run_capture "$RETRY_NONLIB" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "non-library unresolved symbol should fail"
assert_not_contains "$OUT" "Retrying link with inferred libraries"
assert_contains "$OUT" "linker failed"

note "Test 26: install/uninstall only touches managed files"
SAFE_INST="$TMP_ROOT/install_safety"
cp -R "$GRAPH_OK" "$SAFE_INST"
mkdir -p "$PREFIX/bin"
echo "keep me" > "$PREFIX/bin/keep.txt"
run_cmd "$SAFE_INST" "$MAZEN_BIN" install --prefix "$PREFIX"
assert_file "$PREFIX/bin/app"
assert_file "$PREFIX/bin/keep.txt"
run_cmd "$SAFE_INST" "$MAZEN_BIN" uninstall --prefix "$PREFIX"
assert_not_exists "$PREFIX/bin/app"
assert_file "$PREFIX/bin/keep.txt"

note "Test 27: duplicate basenames in different directories produce unique objects"
DUPBASE="$TMP_ROOT/dup_basename"
mkdir -p "$DUPBASE/src/util" "$DUPBASE/lib/util"
cat > "$DUPBASE/src/util/add.c" <<'SRC'
int add_src(int a, int b) { return a + b; }
SRC
cat > "$DUPBASE/lib/util/add.c" <<'SRC'
int add_lib(int a, int b) { return a + b + 1; }
SRC
cat > "$DUPBASE/src/main.c" <<'SRC'
int add_src(int a, int b);
int add_lib(int a, int b);
int main(void) { return (add_src(1, 2) == 3 && add_lib(1, 2) == 4) ? 0 : 1; }
SRC
cat > "$DUPBASE/mazen.toml" <<'TOML'
[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c", "src/util/add.c", "lib/util/add.c"]
default = true
TOML
run_cmd "$DUPBASE" "$MAZEN_BIN"
assert_file "$DUPBASE/build/obj/app/src_util_add.o"
assert_file "$DUPBASE/build/obj/app/lib_util_add.o"

note "Test 28: parallel stress loop remains stable"
PAR_STRESS="$TMP_ROOT/parallel_stress"
mkdir -p "$PAR_STRESS/src"
for i in $(seq 1 24); do
  cat > "$PAR_STRESS/src/n$i.c" <<SRC
int n$i(void) { return $i; }
SRC
done
cat > "$PAR_STRESS/src/main.c" <<'SRC'
int n1(void); int n2(void); int n3(void); int n4(void); int n5(void); int n6(void); int n7(void); int n8(void);
int n9(void); int n10(void); int n11(void); int n12(void); int n13(void); int n14(void); int n15(void); int n16(void);
int n17(void); int n18(void); int n19(void); int n20(void); int n21(void); int n22(void); int n23(void); int n24(void);
int main(void) {
  return (n1()+n2()+n3()+n4()+n5()+n6()+n7()+n8()+n9()+n10()+n11()+n12()+n13()+n14()+n15()+n16()+n17()+n18()+n19()+n20()+n21()+n22()+n23()+n24()) == 300 ? 0 : 1;
}
SRC
for i in $(seq 1 20); do
  run_cmd "$PAR_STRESS" "$MAZEN_BIN" -j 8 >/dev/null
done


note "Test 29: cross-target transitive rebuild isolation + compile DB stability"
XISO="$TMP_ROOT/cross_isolation"
mkdir -p "$XISO/include" "$XISO/src"
cat > "$XISO/include/b.h" <<'SRC'
int b_value(void);
SRC
cat > "$XISO/include/a.h" <<'SRC'
#include "b.h"
int a_value(void);
SRC
cat > "$XISO/src/b.c" <<'SRC'
#include "b.h"
int b_value(void) { return 7; }
SRC
cat > "$XISO/src/a.c" <<'SRC'
#include "a.h"
int a_value(void) { return b_value() + 1; }
SRC
cat > "$XISO/src/main_a.c" <<'SRC'
#include "a.h"
int main(void) { return a_value() == 8 ? 0 : 1; }
SRC
cat > "$XISO/src/main_c.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$XISO/mazen.toml" <<'TOML'
include_dirs = ["include"]

[target.b]
type = "static"
sources = ["src/b.c"]
include_dirs = ["include"]

[target.a]
type = "executable"
entry = "src/main_a.c"
sources = ["src/main_a.c", "src/a.c"]
include_dirs = ["include"]
deps = ["b"]
default = true

[target.c]
type = "executable"
entry = "src/main_c.c"
sources = ["src/main_c.c"]
TOML
run_cmd "$XISO" "$MAZEN_BIN" --all-targets
DB_BEFORE_SHA="$(sha256_file "$XISO/compile_commands.json")"
cat > "$XISO/src/b.c" <<'SRC'
#include "b.h"
int b_value(void) { return 9; }
SRC
OUT="$(run_capture "$XISO" "$MAZEN_BIN" --all-targets)"
assert_contains "$OUT" "Compiling src/b.c"
assert_contains "$OUT" "Linking build/a"
assert_not_contains "$OUT" "Compiling src/main_c.c"
DB_AFTER_SHA="$(sha256_file "$XISO/compile_commands.json")"
assert_equals "$DB_BEFORE_SHA" "$DB_AFTER_SHA"

note "Test 30: shared runtime matrix from build tree and installed prefix"
SHMAT="$TMP_ROOT/shared_matrix"
mkdir -p "$SHMAT/include" "$SHMAT/src"
cat > "$SHMAT/include/base.h" <<'SRC'
int base_mul2(int x);
SRC
cat > "$SHMAT/include/mid.h" <<'SRC'
int mid_add(int x);
SRC
cat > "$SHMAT/src/base.c" <<'SRC'
#include "base.h"
int base_mul2(int x) { return x * 2; }
SRC
cat > "$SHMAT/src/mid.c" <<'SRC'
#include "base.h"
#include "mid.h"
int mid_add(int x) { return base_mul2(x) + 3; }
SRC
cat > "$SHMAT/src/main.c" <<'SRC'
#include <stdio.h>
#include "mid.h"
int main(void) {
  int v = mid_add(4);
  printf("%d\n", v);
  return v == 11 ? 0 : 1;
}
SRC
cat > "$SHMAT/mazen.toml" <<'TOML'
include_dirs = ["include"]

[target.base]
type = "shared"
sources = ["src/base.c"]
include_dirs = ["include"]

[target.mid]
type = "shared"
sources = ["src/mid.c"]
include_dirs = ["include"]
deps = ["base"]

[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c"]
include_dirs = ["include"]
deps = ["mid"]
default = true
TOML
run_cmd "$SHMAT" "$MAZEN_BIN" --all-targets
OUT="$(run_capture "$SHMAT" env LD_LIBRARY_PATH="$SHMAT/build" ./build/app)"
assert_contains "$OUT" "11"
MAT_PREFIX="$TMP_ROOT/matrix_prefix"
run_cmd "$SHMAT" "$MAZEN_BIN" install --prefix "$MAT_PREFIX"
OUT="$(run_capture "$SHMAT" env LD_LIBRARY_PATH="$MAT_PREFIX/lib" "$MAT_PREFIX/bin/app")"
assert_contains "$OUT" "11"
run_cmd "$SHMAT" "$MAZEN_BIN" uninstall --prefix "$MAT_PREFIX"

note "Test 31: cache behavior persists across fresh process restarts"
RST="$TMP_ROOT/restart_cache"
mkdir -p "$RST/include" "$RST/src"
cat > "$RST/include/deep.h" <<'SRC'
int deep(int x);
SRC
cat > "$RST/src/deep.c" <<'SRC'
#include "deep.h"
int deep(int x) { return x + 10; }
SRC
cat > "$RST/src/main.c" <<'SRC'
#include "deep.h"
int main(void) { return deep(1) == 11 ? 0 : 1; }
SRC
cat > "$RST/mazen.toml" <<'TOML'
include_dirs = ["include"]
TOML
OUT="$(bash -lc 'cd "$1" && "$2"' _ "$RST" "$MAZEN_BIN" 2>&1)"
assert_contains "$OUT" "Compiling src/deep.c"
OUT="$(bash -lc 'cd "$1" && "$2"' _ "$RST" "$MAZEN_BIN" 2>&1)"
assert_contains "$OUT" "Target is up to date"
cat > "$RST/src/deep.c" <<'SRC'
#include "deep.h"
int deep(int x) { return x + 20; }
SRC
OUT="$(bash -lc 'cd "$1" && "$2"' _ "$RST" "$MAZEN_BIN" 2>&1)"
assert_contains "$OUT" "Compiling src/deep.c"

note "Test 32: broken artifact recovery for object/depfile/binary/compile DB"
RECOV="$TMP_ROOT/recovery"
mkdir -p "$RECOV/src"
cat > "$RECOV/src/helper.c" <<'SRC'
int helper(void) { return 1; }
SRC
cat > "$RECOV/src/main.c" <<'SRC'
int helper(void);
int main(void) { return helper() == 1 ? 0 : 1; }
SRC
run_cmd "$RECOV" "$MAZEN_BIN"
rm -f "$RECOV/build/obj/recovery/src_helper.o"
OUT="$(run_capture "$RECOV" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/helper.c"
rm -f "$RECOV/build/obj/recovery/src_helper.d"
OUT="$(run_capture "$RECOV" "$MAZEN_BIN")"
assert_contains "$OUT" "Compiling src/helper.c"
rm -f "$RECOV/build/recovery"
OUT="$(run_capture "$RECOV" "$MAZEN_BIN")"
assert_contains "$OUT" "Linking build/recovery"
rm -f "$RECOV/compile_commands.json"
OUT="$(run_capture "$RECOV" "$MAZEN_BIN")"
assert_contains "$OUT" "Wrote"
assert_file "$RECOV/compile_commands.json"

note "Test 33: compile DB keeps distinct commands for overlapping multi-target source"
CDBX="$TMP_ROOT/compdb_overlap"
mkdir -p "$CDBX/src"
cat > "$CDBX/src/shared.c" <<'SRC'
int shared(void) {
#ifdef MODE_FAST
  return 2;
#else
  return 1;
#endif
}
SRC
cat > "$CDBX/src/main_fast.c" <<'SRC'
int shared(void);
int main(void) { return shared() == 2 ? 0 : 1; }
SRC
cat > "$CDBX/src/main_safe.c" <<'SRC'
int shared(void);
int main(void) { return shared() == 1 ? 0 : 1; }
SRC
cat > "$CDBX/mazen.toml" <<'TOML'
[target.fast]
type = "executable"
entry = "src/main_fast.c"
sources = ["src/main_fast.c", "src/shared.c"]
cflags = ["-DMODE_FAST"]

[target.safe]
type = "executable"
entry = "src/main_safe.c"
sources = ["src/main_safe.c", "src/shared.c"]
default = true
TOML
run_cmd "$CDBX" "$MAZEN_BIN" --all-targets
$PYTHON3_BIN - <<'PY2' "$CDBX/compile_commands.json"
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as f:
    data = json.load(f)
shared = [x.get('command','') for x in data if x.get('file','').endswith('/src/shared.c')]
if len(shared) < 2:
    raise SystemExit('expected shared.c to appear for multiple target contexts')
if not any('-DMODE_FAST' in c for c in shared):
    raise SystemExit('expected one shared.c compile command with -DMODE_FAST')
if not any('-DMODE_FAST' not in c for c in shared):
    raise SystemExit('expected one shared.c compile command without -DMODE_FAST')
PY2

note "Test 34: diagnostic contract categories and hints"
DIAG_COMP="$TMP_ROOT/diag_compile"
mkdir -p "$DIAG_COMP/src"
cat > "$DIAG_COMP/src/main.c" <<'SRC'
int main(void) { return ; }
SRC
set +e
OUT="$(run_capture "$DIAG_COMP" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "compile diagnostic case should fail"
assert_contains "$OUT" "compilation failed"
assert_contains "$OUT" "main.c"
assert_not_contains "$OUT" "linker failed"

DIAG_LINK="$TMP_ROOT/diag_link"
mkdir -p "$DIAG_LINK/src"
cat > "$DIAG_LINK/src/main.c" <<'SRC'
int missing_lib_symbol(void);
int main(void) { return missing_lib_symbol(); }
SRC
set +e
OUT="$(run_capture "$DIAG_LINK" "$MAZEN_BIN")"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "link diagnostic case should fail"
assert_contains "$OUT" "linker failed"
assert_contains "$OUT" "Add a library with"
assert_not_contains "$OUT" "compilation failed"

DIAG_GRAPH="$TMP_ROOT/diag_graph"
mkdir -p "$DIAG_GRAPH/src"
cat > "$DIAG_GRAPH/src/main.c" <<'SRC'
int main(void) { return 0; }
SRC
cat > "$DIAG_GRAPH/mazen.toml" <<'TOML'
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
OUT="$(run_capture "$DIAG_GRAPH" "$MAZEN_BIN" --all-targets)"
RC=$?
set -e
[[ $RC -ne 0 ]] || fail "graph diagnostic case should fail"
assert_contains "$OUT" "target dependency cycle detected"
assert_not_contains "$OUT" "Add a library with"

note "All regression checks passed"
