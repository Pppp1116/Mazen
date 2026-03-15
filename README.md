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
- Explicit multi-target builds with named executables and libraries
- Recursive scanning with sensible ignored directories
- Entry-point inference with `main()` detection and target scoring
- Automatic include directory inference from headers and `#include` usage
- Clang-based incremental compilation with `.d` dependency files
- Parallel compilation with `-j` and `--jobs`
- `compile_commands.json` generation for editor and LSP tooling
- Automatic inference for common system libraries like `m`, `pthread`, `dl`, and `rt`
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

Remove the local `build/` directory.

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
```

Parallel job control:

```sh
mazen -j 8
mazen release --jobs 4
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
compile_commands_path = "build/compile_commands.json"
default_target = "app"

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

[target.math]
type = "static"
sources = ["core/math.c"]
output = "build/libmath.a"

[target.plugin]
type = "shared"
sources = ["plugins/filter.c", "plugins/common.c"]
output = "build/libplugin.so"
cflags = ["-DPLUGIN_BUILD=1"]
include_dirs = ["plugins/include"]
exclude = ["plugins/old"]
```

Supported keys:

- `name`
- `c_standard`
- `jobs`
- `compile_commands_path`
- `default_target`
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
- `entry`
- `output`
- `src_dirs`
- `sources`
- `include_dirs`
- `libs`
- `cflags`
- `ldflags`
- `exclude`

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
compiler, resolved C standard, selected target, job count, and compile database
path.

## Explicit Targets And Libraries

When `mazen.toml` defines `[target.NAME]` sections, MAZEN resolves one target at
a time:

- executable targets produce binaries such as `build/app`
- static library targets produce archives such as `build/libcore.a`
- shared library targets produce shared objects such as `build/libcore.so`

If multiple explicit targets exist, MAZEN uses this precedence:

1. CLI `--target NAME`
2. config `default_target = "NAME"`
3. a single `default = true` target
4. the only configured target, if exactly one exists

Otherwise MAZEN asks you to pick a target explicitly.

Every build also refreshes a compile database for the active target. By
default this is `compile_commands.json` in the project root, or the path set by
`compile_commands_path`.

MAZEN also tries to infer a few common system libraries automatically from
source usage, so projects using headers or symbols like `math.h`, `sqrt`,
`pthread_create`, `dlopen`, or `clock_gettime` usually work without a manual
`--lib` flag. Explicit `libs = [...]` and `--lib NAME` values still take
precedence when you want to pin or extend the link set.

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
