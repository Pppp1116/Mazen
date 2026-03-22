# MAZEN

MAZEN is a zero-configuration build system for C projects.

It is designed for the common case where you just want to enter a project
directory and run:

```sh
mazen
mazen run
mazen clean
mazen release
```

without writing a `Makefile`.

MAZEN favors deterministic project discovery, readable diagnostics, fast
incremental rebuilds, and low friction for messy real-world trees.

## Features

- Zero-config project discovery for C source trees
- Named executable, static-library, and shared-library targets
- Internal target dependency graphs for library and application build ordering
- Batch builds with `--all-targets` for configured or discovered targets
- Recursive scanning with sensible ignored directories
- Entry-point inference with `main()` detection and target scoring
- Automatic include directory inference from headers and `#include` usage
- Clang-based incremental compilation with `.d` dependency files
- Parallel compilation with `-j` and `--jobs`
- First-class `mazen test` workflow with filtering and parallel execution
- Install and uninstall workflows with configurable prefixes
- Named build profiles in `mazen.toml`
- `compile_commands.json` generation for editor and LSP tooling
- Automatic inference for common system libraries like `m`, `pthread`, `dl`, `rt`, `curl`, `openssl`,
  `sqlite3`, `glib-2.0`, `gobject-2.0`, `pango-1.0`, and more
- Build cache stored in `build/.mazen/`
- Richer `mazen.toml` overrides with target sections
- Helpful commands for diagnostics and cleanup

## Project Layout

```text
Mazen/
├── Makefile
├── README.md
├── include/
│   └── mazen.h
├── src/
│   ├── autolib.c
│   ├── autolib.h
│   ├── build.c
│   ├── build.h
│   ├── cache.c
│   ├── cache.h
│   ├── classifier.c
│   ├── classifier.h
│   ├── cli.c
│   ├── cli.h
│   ├── compdb.c
│   ├── compdb.h
│   ├── common.c
│   ├── common.h
│   ├── config.c
│   ├── config.h
│   ├── diag.c
│   ├── diag.h
│   ├── doctor.c
│   ├── doctor.h
│   ├── main.c
│   ├── profile.c
│   ├── profile.h
│   ├── scanner.c
│   ├── scanner.h
│   ├── standard.c
│   ├── standard.h
│   ├── target.c
│   └── target.h
└── examples/
    ├── messy/
    ├── organized/
    └── simple/
```

## Building MAZEN

Requirements:

- Linux or another POSIX-like system
- A C compiler to build MAZEN itself
- Clang available in `PATH` for projects MAZEN builds

Build MAZEN:

```sh
make
```

This produces:

```text
./mazen
```

Install globally:

```sh
sudo make install
```

## Commands

```sh
mazen
```

Build the current project in debug mode.

```sh
mazen run
```

Build and run the detected executable.

```sh
mazen release
```

Build the current project in release mode with `-O2`.

```sh
mazen clean
```

Remove build artifacts. You can also clean specific scopes:

```sh
mazen clean
mazen clean objects
mazen clean cache
mazen clean outputs compdb
```

```sh
mazen test
```

Build and run discovered or configured test targets.

```sh
mazen install
mazen uninstall
```

Install or remove built artifacts using the configured prefix, or `--prefix`.

```sh
mazen doctor
```

Print project discovery details, source classification, include directories,
and the selected entrypoint.

```sh
mazen list
```

List configured targets from `mazen.toml`, or detected entry targets when no
explicit targets are defined.

```sh
mazen standards
```

List supported C language standards and dialects.

## Large Trees And Native Adapters

For some very large projects, the right move is to stop pretending they are
zero-config single-binary trees and hand control back to the native build
system.

Today MAZEN does this automatically for Linux kernel style source trees when no
`mazen.toml` is present. In that mode:

- `mazen`, `mazen build`, and `mazen release` delegate to Kbuild via `make`
- `mazen --target GOAL` forwards a specific Kbuild goal such as `defconfig` or
  `bzImage`
- when `.config` already exists, normal builds refresh it with
  `olddefconfig` first so auto mode does not block on interactive Kconfig
  prompts
- `mazen clean` delegates to `make clean` and also removes
  `compile_commands.json`
- `mazen doctor` reports adapter details instead of generic source discovery
- if `scripts/clang-tools/gen_compile_commands.py` is present, MAZEN refreshes
  `compile_commands.json` through that native helper when needed

Native Kbuild variables still work as usual because the delegated process
inherits your environment:

```sh
ARCH=arm64 LLVM=1 mazen --target defconfig
ARCH=arm64 LLVM=1 mazen --target Image
```

## Language Standards

MAZEN selects the C language standard using this precedence:

1. CLI `--std`
2. `mazen.toml` `c_standard`
3. fallback default `c17`

Supported values:

```text
c89
c99
c11
c17
c23
gnu89
gnu99
gnu11
gnu17
gnu23
c2y
gnu2y
```

Examples:

```sh
mazen --std c99
mazen run --std gnu11
```

## Target Selection

MAZEN still works in zero-config mode, but it can also build explicit named
targets from `mazen.toml`.

Examples:

```sh
mazen
mazen --target app
mazen run --target app
mazen --target corelib
mazen --all-targets
mazen release --all-targets
```

Parallel job control:

```sh
mazen -j 8
mazen release --jobs 4
mazen test --parallel -j 4
```

## Default Compiler Flags

Debug builds:

```text
-std=<selected-standard>
-Wall
-Wextra
-g
-O0
```

Release builds:

```text
-std=<selected-standard>
-Wall
-Wextra
-O2
```

Dependency generation:

```text
-MMD -MF
```

## Discovery Model

MAZEN always treats the current working directory as the project root.

### Ignored directories

The recursive scanner ignores these directories by default:

```text
build
out
dist
.git
.svn
node_modules
target
bin
obj
tmp
temp
cache
```

### Discovery modes

Convention mode:

- Prefer directories named `src` or `source` as source roots
- Prefer directories named `include` as include roots

Flat mode:

- If there are no source-root conventions, root-level `.c` files are treated as
  the project sources

Discovery mode:

- MAZEN recursively scans the tree and infers a target from scattered sources

### Source classification

Each `.c` file is classified using:

- path segments
- file name patterns
- `main()` detection
- known test-framework include patterns

MAZEN distinguishes:

- entrypoint sources
- general implementation sources
- tests
- examples and demos
- experimental and backup code
- vendor sources
- tool-style sources

### Entrypoint selection

MAZEN parses sources to detect `main()` definitions like:

```c
int main()
int main(void)
int main(int argc, char **argv)
int main(int argc, char *argv[])
```

When multiple entrypoints exist, MAZEN scores them and chooses the best
candidate. High-priority cases include:

- `main.c` in the project root
- `src/main.c`
- `source/main.c`

Lower-priority cases include:

- `tools/*`
- `examples/*`
- `tests/*`

If multiple candidates exist, MAZEN prints the list and the selected target.

### Include directory inference

MAZEN infers include directories by:

1. collecting directories that contain `.h` files
2. preferring `include/` directories
3. inspecting quoted include directives such as:

```c
#include "util.h"
#include "core/math.h"
```

The inferred include roots are then passed to Clang with `-I`.

## Incremental Builds and Cache

Each source compiles to an object file in:

```text
build/obj/<target>/
```

Examples:

```text
build/obj/app/main.o
build/obj/app/core_math.o
```

MAZEN also stores:

- dependency files next to object files
- target-specific cache metadata in `build/.mazen/*.state.txt`
- compile database output at `compile_commands.json` by default

The cache tracks:

- compile signature
- link signature
- source/object paths
- source and object timestamps
- dependency lists

Objects rebuild automatically when:

- source files change
- dependent headers change
- compiler flags change
- dependency files disappear

## Optional Configuration

MAZEN works without configuration, but it also supports a small optional
`mazen.toml`.

Example:

```toml
name = "myapp"
c_standard = "c23"
jobs = 6
default_profile = "asan"
compile_commands_path = "build/compile_commands.json"
default_target = "app"
install_prefix = "/usr/local"
install_headers = ["include/myapp/api.h", "include/myapp/math.h"]

include_dirs = ["vendor/include"]
libs = ["m", "pthread"]
exclude = ["examples", "tests"]
src_dirs = ["src", "core"]
cflags = ["-DDEBUG_LOGGING=1"]
ldflags = ["-Wl,--as-needed"]

[target.app]
type = "executable"
entry = "src/main.c"
sources = ["src/main.c", "src/app.c", "core/math.c"]
output = "build/myapp"
deps = ["math"]

[target.math]
type = "static"
sources = ["core/math.c"]
output = "build/libmath.a"
install_dir = "lib"

[target.plugin]
type = "shared"
sources = ["plugins/filter.c", "plugins/common.c"]
output = "build/libplugin.so"
cflags = ["-DPLUGIN_BUILD=1"]
include_dirs = ["plugins/include"]
exclude = ["plugins/old"]

[target.unit_math]
type = "executable"
entry = "tests/unit_math.c"
sources = ["tests/unit_math.c"]
deps = ["math"]
test = true
install = false

[profile.asan]
mode = "debug"
cflags = ["-fsanitize=address", "-fno-omit-frame-pointer"]
ldflags = ["-fsanitize=address"]

[profile.size]
mode = "release"
cflags = ["-Oz"]
```

Supported keys:

- `name`
- `c_standard`
- `jobs`
- `default_profile`
- `compile_commands_path`
- `default_target`
- `install_prefix`
- `install_headers`
- `include_dirs`
- `libs`
- `exclude`
- `src_dirs`
- `sources`
- `cflags`
- `ldflags`

Supported target-section keys:

- `[target.NAME]`
- `type = "executable" | "static" | "shared"`
- `default = true`
- `test = true | false`
- `install = true | false`
- `entry`
- `output`
- `install_dir`
- `deps = ["other-target"]`
- `src_dirs`
- `sources`
- `include_dirs`
- `libs`
- `cflags`
- `ldflags`
- `exclude`

Supported profile-section keys:

- `[profile.NAME]`
- `mode = "debug" | "release"`
- `include_dirs`
- `libs`
- `cflags`
- `ldflags`

Command-line options can extend or override config values:

```sh
mazen --name custom-app
mazen --std gnu17
mazen --target app
mazen -j 8
mazen --lib m
mazen -I vendor/include
mazen --src-dir core
mazen --exclude examples
```

The `doctor` command reports the active toolchain selection, including the
compiler, resolved C standard, build mode, active profile, selected target, job
count, and compile database path. With `mazen doctor --all-targets`, it also
prints the full resolved target set for a batch build.

## Explicit Targets, Dependencies, And Libraries

MAZEN supports debug and release builds for executable, static-library, and
shared-library targets.

By default, MAZEN resolves one target at a time. When `mazen.toml` defines
`[target.NAME]` sections, you can either select a single target or build every
resolved target in one pass with `--all-targets`:

- executable targets produce binaries such as `build/app`
- static library targets produce archives such as `build/libcore.a`
- shared library targets produce shared objects such as `build/libcore.so`

Examples:

```sh
mazen --target app
mazen release --target plugin
mazen --all-targets
mazen release --all-targets
```

If multiple explicit targets exist, MAZEN uses this precedence:

1. CLI `--target NAME`
2. config `default_target = "NAME"`
3. a single `default = true` target
4. the only configured target, if exactly one exists

Otherwise MAZEN asks you to pick a target explicitly, unless you build them all
with `--all-targets`.

Targets can also depend on other internal targets. MAZEN resolves and builds
them in topological order, then links dependent artifacts automatically:

```toml
[target.math]
type = "static"
src_dirs = ["libmath"]

[target.engine]
type = "static"
src_dirs = ["libengine"]
deps = ["math"]

[target.app]
type = "executable"
entry = "app/main.c"
deps = ["engine"]
```

That produces the expected build order:

```text
math -> engine -> app
```

Every build also refreshes a compile database. By default this is
`compile_commands.json` in the project root, or the path set by
`compile_commands_path`. Batch builds merge compile commands from every built
target into a single compile database.

### Library resolution (manual libs + auto-lib)

For non-static targets, MAZEN resolves link libraries in layers:

1. manual global libs (`libs = [...]` in top-level config)
2. manual profile libs (`[profile.NAME].libs`)
3. manual target libs (`[target.NAME].libs`, including propagated dependency target libs)
4. auto-lib inference from the selected target's source files

All layers are merged uniquely, so duplicates are removed while preserving
the first-seen order.

Auto-lib currently uses confidence-gated heuristic signals:

- **Always enabled (default):**
  - known headers and exact call identifiers (for example `math.h` + `sqrt` => `-lm`)
  - exact pkg-config token lookup derived from system includes when available
- **Low-confidence mode only (`MAZEN_AUTOLIB_LOW_CONFIDENCE`):**
  - identifier prefix matching (for example `pthread_` => `-lpthread`)
  - fuzzy pkg-config matching from include-derived tokens
  - linker-output retry inference on first link failure

Low-confidence mode is **off by default**. To enable it, set:

- `MAZEN_AUTOLIB_LOW_CONFIDENCE=1`
- accepted truthy values: `1`, `true`, `yes`, `on`

When enabled, if the first link fails with undefined references, MAZEN performs
one retry pass by scanning linker output and appending any newly inferred
libraries, then relinking.

Current trade-off: this broad heuristic table gives wide coverage, but it can
also produce false positives and adds scanning/maintenance overhead as rules
grow.

Notes:

- Auto-lib is only applied to executable/shared-library links (not static
  archive targets).
- The inferred set is cached in `build/.mazen/*.state.txt` and reused when
  source mtimes are unchanged (mtime-based freshness).
- You can always force additional libraries with `--lib NAME` or config
  `libs = [...]`.

### Auto-lib confidence model (current behavior)

Auto-lib inference is split into confidence classes:

- **High confidence:** exact header match + pkg-config resolution
- **Medium confidence:** exact symbol/function match
- **Low confidence:** prefix-only heuristics and linker-output retry guesses

Default behavior auto-applies high + medium confidence only. Low-confidence
behaviors require explicit opt-in via `MAZEN_AUTOLIB_LOW_CONFIDENCE`.

## Build Performance Notes

In large trees, the slowest stages are usually:

1. **Initial recursive scan + source classification**
   - walks the tree, stats files, and reads/parses each discovered `.c` file for
     role/entrypoint/include inference
2. **Cold compile phase**
   - first build has no reusable object cache, so every target source compiles
3. **Link phase (especially large executables/shared libs)**
   - link is a single final step per target, so this is a serialization point
4. **Auto-lib inference on cache miss**
   - selected target sources are read and analyzed to infer external libs

How MAZEN already makes this faster:

- **Parallel compilation** via `-j/--jobs` (or auto CPU count when omitted)
- **Incremental object rebuilds** using depfiles + cached timestamps/signatures
- **Skip relink when up-to-date** using output/object/dependency timestamps and
  link-signature tracking
- **Auto-lib cache reuse** when source mtimes are unchanged
- **Compile DB write-if-changed** to avoid unnecessary filesystem churn

Parallelization today:

- **Compile stage:** parallel, up to configured/auto job count
- **Test execution:** optional parallel mode with `mazen test --parallel`
- **Link stage:** currently one link process per target (not internally parallel)

Does it adapt to different PCs? Yes:

- If no job count is provided, MAZEN uses online CPU count from the host
- You can override with `-j N` per machine/CI runner
- Includes/libs/flags remain configurable per project/profile/target while
  concurrency follows host capabilities by default

Potential next improvements:

- **No-op speed path first:**
  - persist per-source extracted include/symbol fingerprints
  - avoid reparsing depfiles until an object is actually reconsidered
  - memoize repeated path normalization and command-string assembly in hot paths
- **Cheaper auto-lib refresh:**
  - cache extracted signals per source and only rescan changed files
  - merge target-level inference from cached per-source signals
  - persist "why this lib was inferred" for auditability
- **Stronger cache keys:**
  - move from mtime-only freshness toward lightweight content hashing of
    extracted signals
- **Scalability/robustness additions:**
  - response-file support for very long compiler/link command lines
  - optional parallel target builds for independent targets in `--all-targets`
  - longer-term persistent worker/daemon model for lower process-spawn overhead

### Performance and portability caveats (current state)

- Process model is currently one compiler subprocess per source file
  (`fork/execvp` style orchestration), which is robust but adds spawn overhead
  on very large trees.
- Link is currently a single command per target (with one optional retry after
  linker-output-based inference), so link planning is intentionally simple.
- The implementation is currently Clang-first and POSIX-first (`fork`, `execvp`,
  `waitpid`, `ar`, `LD_LIBRARY_PATH`-style assumptions), which keeps Linux
  workflows strong but narrows out-of-the-box portability.

### Internal invariants (maintainer contract)

When changing `build.c`, `autolib.c`, cache logic, or target propagation,
these behavioral guarantees should remain true:

- **Compile invalidation correctness:**
  - rebuild when source, included headers, depfiles, or compile flags change.
- **Link invalidation correctness:**
  - relink when any linked object/library dependency or link flags change.
- **Compile DB identity:**
  - each compile context must be represented by its actual compile command;
    shared sources used by different targets/flags must remain distinct entries.
- **Auto-lib cache freshness:**
  - inferred libraries must only be reused when tracked source state and
    confidence mode are still compatible with the cached decision.
- **Selective rebuild isolation:**
  - unrelated targets should not recompile when only another target's sources
    or private dependencies change.
- **Diagnostic category accuracy:**
  - graph failures, compilation failures, and linker failures must be reported
    under the correct category with contextually-appropriate hints.

The regression matrix in `tests/regression.sh` contains contract tests for all
of the above and should stay green before merging behavior changes.

### Performance baseline tracking

Use the performance matrix script to compare machine-relative behavior over time:

```sh
bash tests/perf_matrix.sh
# or:
make test-perf
```

It reports clean/no-op/incremental timings, compile/link counts, auto-lib cache
behavior, and `-j 1` vs `-j 8` scaling on generated fixture projects. This is
intended for trend tracking ("faster/slower than previous commit"), not hard
threshold gating across different machines.

## Tests, Profiles, And Installation

Test targets can be discovered heuristically from `tests/` paths, or defined
explicitly with `test = true`. Use `mazen test` to build and run them:

```sh
mazen test
mazen test --filter math
mazen test --parallel -j 4
```

MAZEN exits non-zero if any selected test fails, which makes it usable in CI.

Profiles let you keep reusable flag sets in `mazen.toml`:

```sh
mazen --profile asan
mazen test --profile asan
mazen install --profile size
```

Profile precedence is:

1. CLI `--profile`
2. config `default_profile`
3. no profile

Install and uninstall workflows use `/usr/local` by default, or the configured
`install_prefix`, or an explicit CLI prefix:

```sh
mazen install
mazen install --prefix /tmp/pkgroot
mazen uninstall --prefix /tmp/pkgroot
```

Executables install into `bin/`, libraries into `lib/`, and headers into
`include/` unless a target overrides `install_dir`.

## Typical Output

```text
Scanning project...
Found 7 source files
Detected entrypoint: src/main.c
Include directories:
    .
    include
    core
Building debug target
Compiling src/main.c
Compiling src/util.c
Linking build/myproject
```

## Example Projects

### Simple flat project

```text
examples/simple/
├── main.c
├── util.c
└── util.h
```

Build it:

```sh
cd examples/simple
../../mazen
../../mazen run
```

### Organized project

```text
examples/organized/
├── include/
│   └── app.h
└── src/
    ├── app.c
    └── main.c
```

Build it:

```sh
cd examples/organized
../../mazen
../../mazen release
```

### Messy project

```text
examples/messy/
├── core/
│   ├── main.c
│   ├── math.c
│   └── math.h
├── tools/
│   └── generator.c
└── vendor/
    └── mini/
        ├── log.c
        └── log.h
```

MAZEN will detect multiple entrypoints, choose `core/main.c`, and ignore the
tool entrypoint for the default target.

Try it:

```sh
cd examples/messy
../../mazen doctor
../../mazen list
../../mazen run
```

## Self-Hosting

Once built, MAZEN can analyze or rebuild its own repository:

```sh
./mazen doctor
./mazen
```

## Notes

- MAZEN currently targets Clang-first Linux workflows
- The implementation is portable C17 with POSIX filesystem/process APIs
- The architecture is modular so extra target types and backends can be added
  later
