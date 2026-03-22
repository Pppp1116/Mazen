#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAZEN_BIN="$ROOT_DIR/mazen"

if [[ ! -x "$MAZEN_BIN" ]]; then
  make -C "$ROOT_DIR" -j4
fi

if command -v rg >/dev/null 2>&1; then
  COUNT_BACKEND="rg"
elif command -v grep >/dev/null 2>&1; then
  COUNT_BACKEND="grep"
else
  echo "[ERROR] perf matrix requires rg or grep for output counting" >&2
  exit 1
fi

if command -v python3 >/dev/null 2>&1; then
  TIMER_BACKEND="python3"
elif command -v python >/dev/null 2>&1; then
  TIMER_BACKEND="python"
else
  probe_ns="$(date +%s%N 2>/dev/null || true)"
  if [[ "$probe_ns" =~ ^[0-9]+$ && "$probe_ns" != *N ]]; then
    TIMER_BACKEND="date_ns"
  else
    TIMER_BACKEND="date_sec"
    echo "[WARN] high-resolution timer unavailable; using second-resolution timing" >&2
  fi
fi

count_matches() {
  local pattern="$1"
  local text="$2"
  local count
  if [[ "$COUNT_BACKEND" == "rg" ]]; then
    count="$(printf '%s\n' "$text" | rg -c "$pattern" || true)"
  else
    count="$(printf '%s\n' "$text" | grep -Ec "$pattern" || true)"
  fi
  printf '%s\n' "${count:-0}"
}

now_ns() {
  case "$TIMER_BACKEND" in
    python3)
      python3 - <<'PY2'
import time
print(time.time_ns())
PY2
      ;;
    python)
      python - <<'PY2'
import time
print(int(time.time() * 1_000_000_000))
PY2
      ;;
    date_ns)
      date +%s%N
      ;;
    *)
      printf '%s000000000\n' "$(date +%s)"
      ;;
  esac
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

PROJECT="$TMP_ROOT/perf_case"
mkdir -p "$PROJECT/src" "$PROJECT/include"

cat > "$PROJECT/include/mathx.h" <<'SRC'
#ifndef MATHX_H
#define MATHX_H
int add2(int a, int b);
int mul2(int x);
#endif
SRC

cat > "$PROJECT/src/add.c" <<'SRC'
#include "mathx.h"
int add2(int a, int b) { return a + b; }
SRC

cat > "$PROJECT/src/mul.c" <<'SRC'
#include "mathx.h"
int mul2(int x) { return x * 2; }
SRC

cat > "$PROJECT/src/main.c" <<'SRC'
#include "mathx.h"
int main(void) {
  return mul2(add2(1, 2)) == 6 ? 0 : 1;
}
SRC

cat > "$PROJECT/mazen.toml" <<'TOML'
include_dirs = ["include"]
TOML

SCALING="$TMP_ROOT/scaling_case"
mkdir -p "$SCALING/src"
for i in $(seq 1 36); do
  cat > "$SCALING/src/f$i.c" <<SRC
int f$i(void) { return $i; }
SRC
done
{
  for i in $(seq 1 36); do
    printf 'int f%d(void);\n' "$i"
  done
  printf 'int main(void) { return ('
  for i in $(seq 1 36); do
    printf 'f%d()' "$i"
    [[ $i -lt 36 ]] && printf '+'
  done
  printf ') == 666 ? 0 : 1; }\n'
} > "$SCALING/src/main.c"

measure_case() {
  local label="$1"
  local cwd="$2"
  shift 2
  local out rc start end elapsed_ms
  start=$(now_ns)
  set +e
  out="$(cd "$cwd" && "$@" 2>&1)"
  rc=$?
  set -e
  end=$(now_ns)
  elapsed_ms=$(( (end - start) / 1000000 ))
  local compiles links up_to_date autolib
  compiles=$(count_matches '^Compiling ' "$out")
  links=$(count_matches '^Linking ' "$out")
  up_to_date=$(count_matches 'Target is up to date' "$out")
  autolib=$(count_matches 'Auto-detected libraries:' "$out")
  printf '%-34s rc=%d ms=%-6d compiles=%-3d links=%-2d up_to_date=%-2d autolib=%-2d\n' \
    "$label" "$rc" "$elapsed_ms" "$compiles" "$links" "$up_to_date" "$autolib"
  if [[ $rc -ne 0 ]]; then
    printf -- '--- failure output (%s) ---\n%s\n' "$label" "$out"
    return "$rc"
  fi
}

echo "Performance matrix (machine-relative, no fixed thresholds)"
echo "---------------------------------------------------------"

measure_case "clean build" "$PROJECT" "$MAZEN_BIN"
measure_case "immediate no-op rebuild" "$PROJECT" "$MAZEN_BIN"

cat > "$PROJECT/src/add.c" <<'SRC'
#include "mathx.h"
int add2(int a, int b) { return a + b + 1; }
SRC
measure_case "single .c edit rebuild" "$PROJECT" "$MAZEN_BIN"

cat > "$PROJECT/include/mathx.h" <<'SRC'
#ifndef MATHX_H
#define MATHX_H
int add2(int a, int b);
int mul2(int x);
int id2(int x);
#endif
SRC
cat > "$PROJECT/src/mul.c" <<'SRC'
#include "mathx.h"
int mul2(int x) { return x * 2; }
int id2(int x) { return x; }
SRC
cat > "$PROJECT/src/main.c" <<'SRC'
#include "mathx.h"
int main(void) {
  return id2(mul2(add2(1, 2))) == 8 ? 0 : 1;
}
SRC
measure_case "single header edit rebuild" "$PROJECT" "$MAZEN_BIN"

measure_case "flag-change rebuild" "$PROJECT" "$MAZEN_BIN" --std c11

AUTOLIB="$TMP_ROOT/autolib_perf"
mkdir -p "$AUTOLIB/src"
cat > "$AUTOLIB/src/main.c" <<'SRC'
double sqrt(double);
int main(void) { return sqrt(9.0) == 3.0 ? 0 : 1; }
SRC
measure_case "autolib fresh-cache pass" "$AUTOLIB" "$MAZEN_BIN"
measure_case "autolib warm-cache pass" "$AUTOLIB" "$MAZEN_BIN"

measure_case "scaling clean -j1" "$SCALING" "$MAZEN_BIN" -j 1
measure_case "scaling no-op -j1" "$SCALING" "$MAZEN_BIN" -j 1
(cd "$SCALING" && "$MAZEN_BIN" clean objects >/dev/null)
measure_case "scaling clean -j8" "$SCALING" "$MAZEN_BIN" -j 8
measure_case "scaling no-op -j8" "$SCALING" "$MAZEN_BIN" -j 8
