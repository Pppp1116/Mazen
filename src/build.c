#include "build.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    const SourceFile *source;
    char *object_rel;
    char *dep_rel;
    char *object_abs;
    char *dep_abs;
    StringList deps;
} BuildUnit;

typedef struct {
    BuildUnit *items;
    size_t len;
    size_t cap;
} BuildUnitList;

typedef enum {
    PATH_CONTEXT_GENERAL = 0,
    PATH_CONTEXT_TOOL,
    PATH_CONTEXT_EXAMPLE,
    PATH_CONTEXT_TEST,
    PATH_CONTEXT_EXPERIMENTAL
} PathContext;

static const char *CONTEXT_TOOL_NAMES[] = {"tool", "tools", "script", "scripts"};
static const char *CONTEXT_EXAMPLE_NAMES[] = {"example", "examples", "demo", "demos", "sample", "samples"};
static const char *CONTEXT_TEST_NAMES[] = {"test", "tests", "spec", "specs"};
static const char *CONTEXT_EXPERIMENT_NAMES[] = {
    "experiment", "experiments", "old", "backup", "scratch", "playground"
};

static void build_unit_init(BuildUnit *unit) {
    unit->source = NULL;
    unit->object_rel = NULL;
    unit->dep_rel = NULL;
    unit->object_abs = NULL;
    unit->dep_abs = NULL;
    string_list_init(&unit->deps);
}

static void build_unit_free(BuildUnit *unit) {
    free(unit->object_rel);
    free(unit->dep_rel);
    free(unit->object_abs);
    free(unit->dep_abs);
    string_list_free(&unit->deps);
    build_unit_init(unit);
}

static void build_unit_list_init(BuildUnitList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static void build_unit_list_free(BuildUnitList *list) {
    size_t i;
    for (i = 0; i < list->len; ++i) {
        build_unit_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static BuildUnit *build_unit_list_push(BuildUnitList *list) {
    size_t cap;
    if (list->len == list->cap) {
        cap = list->cap == 0 ? 8 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }
    build_unit_init(&list->items[list->len]);
    return &list->items[list->len++];
}

static void append_quoted_arg(String *buffer, const char *arg) {
    size_t i;
    bool needs_quotes = strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL;
    if (!needs_quotes) {
        string_append(buffer, arg);
        return;
    }
    string_append_char(buffer, '\'');
    for (i = 0; arg[i] != '\0'; ++i) {
        if (arg[i] == '\'') {
            string_append(buffer, "'\\''");
        } else {
            string_append_char(buffer, arg[i]);
        }
    }
    string_append_char(buffer, '\'');
}

static char *format_command(const StringList *args) {
    String text;
    size_t i;
    string_init(&text);
    for (i = 0; i < args->len; ++i) {
        if (i > 0) {
            string_append_char(&text, ' ');
        }
        append_quoted_arg(&text, args->items[i]);
    }
    return string_take(&text);
}

static char **argv_from_list(const StringList *args) {
    char **argv = mazen_xcalloc(args->len + 1, sizeof(*argv));
    size_t i;
    for (i = 0; i < args->len; ++i) {
        argv[i] = args->items[i];
    }
    argv[args->len] = NULL;
    return argv;
}

static bool run_command_capture(const char *cwd, const StringList *args, bool echo_output, int *exit_code,
                                String *captured, Diagnostic *diag) {
    int pipes[2];
    pid_t child;
    int status;
    char buffer[4096];
    ssize_t count;
    char **argv;

    if (pipe(pipes) != 0) {
        diag_set(diag, "failed to create process pipe: %s", strerror(errno));
        return false;
    }

    child = fork();
    if (child < 0) {
        close(pipes[0]);
        close(pipes[1]);
        diag_set(diag, "failed to fork: %s", strerror(errno));
        return false;
    }

    if (child == 0) {
        argv = argv_from_list(args);
        if (cwd != NULL && chdir(cwd) != 0) {
            fprintf(stderr, "failed to enter %s: %s\n", cwd, strerror(errno));
            _exit(127);
        }
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        execvp(argv[0], argv);
        fprintf(stderr, "failed to execute %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    close(pipes[1]);
    while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0) {
        if (captured != NULL) {
            string_append_n(captured, buffer, (size_t) count);
        }
        if (echo_output) {
            ssize_t written = 0;
            while (written < count) {
                ssize_t rc = write(STDERR_FILENO, buffer + written, (size_t) (count - written));
                if (rc < 0) {
                    break;
                }
                written += rc;
            }
        }
    }
    close(pipes[0]);

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            diag_set(diag, "failed waiting for child process: %s", strerror(errno));
            return false;
        }
    }

    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *exit_code = 128 + WTERMSIG(status);
    } else {
        *exit_code = 1;
    }

    return true;
}

static bool run_command_passthrough(const char *cwd, const StringList *args, Diagnostic *diag) {
    pid_t child;
    int status;
    char **argv;

    child = fork();
    if (child < 0) {
        diag_set(diag, "failed to fork: %s", strerror(errno));
        return false;
    }

    if (child == 0) {
        argv = argv_from_list(args);
        if (cwd != NULL && chdir(cwd) != 0) {
            fprintf(stderr, "failed to enter %s: %s\n", cwd, strerror(errno));
            _exit(127);
        }
        execvp(argv[0], argv);
        fprintf(stderr, "failed to execute %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            diag_set(diag, "failed waiting for child process: %s", strerror(errno));
            return false;
        }
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    if (WIFEXITED(status)) {
        diag_set(diag, "program exited with status %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        diag_set(diag, "program terminated by signal %d", WTERMSIG(status));
    } else {
        diag_set(diag, "program execution failed");
    }
    return false;
}

static void print_include_dirs(const ProjectInfo *project) {
    size_t i;
    puts("Include directories:");
    for (i = 0; i < project->include_dirs.len; ++i) {
        printf("    %s\n", project->include_dirs.items[i]);
    }
}

static bool path_matches_any_component(const char *path, const char *const *items, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (path_has_component(path, items[i])) {
            return true;
        }
    }
    return false;
}

static PathContext source_path_context(const SourceFile *source) {
    if (path_matches_any_component(source->path, CONTEXT_TEST_NAMES, MAZEN_ARRAY_LEN(CONTEXT_TEST_NAMES))) {
        return PATH_CONTEXT_TEST;
    }
    if (path_matches_any_component(source->path, CONTEXT_EXAMPLE_NAMES, MAZEN_ARRAY_LEN(CONTEXT_EXAMPLE_NAMES))) {
        return PATH_CONTEXT_EXAMPLE;
    }
    if (path_matches_any_component(source->path, CONTEXT_EXPERIMENT_NAMES,
                                   MAZEN_ARRAY_LEN(CONTEXT_EXPERIMENT_NAMES))) {
        return PATH_CONTEXT_EXPERIMENTAL;
    }
    if (path_matches_any_component(source->path, CONTEXT_TOOL_NAMES, MAZEN_ARRAY_LEN(CONTEXT_TOOL_NAMES))) {
        return PATH_CONTEXT_TOOL;
    }
    return PATH_CONTEXT_GENERAL;
}

static bool include_source_for_target(const ProjectInfo *project, size_t index) {
    const SourceFile *selected;
    const SourceFile *source;
    PathContext selected_context;
    PathContext source_context;

    if (!project->has_selected_entry) {
        return false;
    }

    selected = &project->sources.items[project->selected_entry];
    source = &project->sources.items[index];
    selected_context = source_path_context(selected);
    source_context = source_path_context(source);

    if (index == project->selected_entry) {
        return true;
    }
    if (source->has_main) {
        return false;
    }

    switch (selected_context) {
    case PATH_CONTEXT_TOOL:
        if (source_context != PATH_CONTEXT_GENERAL && source_context != PATH_CONTEXT_TOOL) {
            return false;
        }
        return source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
               source->role == SOURCE_ROLE_TOOL;
    case PATH_CONTEXT_EXAMPLE:
        if (source_context != PATH_CONTEXT_GENERAL && source_context != PATH_CONTEXT_EXAMPLE) {
            return false;
        }
        return source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
               source->role == SOURCE_ROLE_EXAMPLE;
    case PATH_CONTEXT_TEST:
        if (source_context != PATH_CONTEXT_GENERAL && source_context != PATH_CONTEXT_TEST) {
            return false;
        }
        return source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
               source->role == SOURCE_ROLE_TEST;
    case PATH_CONTEXT_EXPERIMENTAL:
        if (source_context != PATH_CONTEXT_GENERAL && source_context != PATH_CONTEXT_EXPERIMENTAL) {
            return false;
        }
        return source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
               source->role == SOURCE_ROLE_EXPERIMENTAL;
    case PATH_CONTEXT_GENERAL:
    default:
        if (source_context != PATH_CONTEXT_GENERAL) {
            return false;
        }
        return source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR;
    }
}

static void make_signatures(const ProjectInfo *project, const BuildRequest *request, char **compile_sig,
                            char **link_sig) {
    String compile;
    String link;
    size_t i;
    uint64_t compile_hash;
    uint64_t link_hash;

    string_init(&compile);
    string_init(&link);

    string_appendf(&compile, "compiler=%s\nmode=%d\nstandard=%s\n", request->compiler, (int) request->mode,
                   request->standard != NULL ? request->standard->name : mazen_standard_default()->name);
    string_appendf(&link, "compiler=%s\nmode=%d\nbinary=%s\n", request->compiler, (int) request->mode,
                   request->binary_name);

    for (i = 0; i < project->include_dirs.len; ++i) {
        string_appendf(&compile, "I=%s\n", project->include_dirs.items[i]);
    }
    for (i = 0; i < request->cflags.len; ++i) {
        string_appendf(&compile, "C=%s\n", request->cflags.items[i]);
    }
    for (i = 0; i < request->libs.len; ++i) {
        string_appendf(&link, "L=%s\n", request->libs.items[i]);
    }
    for (i = 0; i < request->ldflags.len; ++i) {
        string_appendf(&link, "F=%s\n", request->ldflags.items[i]);
    }

    compile_hash = fnv1a_hash_string(compile.data == NULL ? "" : compile.data, UINT64_C(1469598103934665603));
    link_hash = fnv1a_hash_string(link.data == NULL ? "" : link.data, UINT64_C(1469598103934665603));

    *compile_sig = hex_u64(compile_hash);
    *link_sig = hex_u64(link_hash);
    string_free(&compile);
    string_free(&link);
}

static void build_request_copy_list(StringList *dst, const StringList *src, bool unique) {
    size_t i;
    for (i = 0; i < src->len; ++i) {
        if (unique) {
            string_list_push_unique(dst, src->items[i]);
        } else {
            string_list_push(dst, src->items[i]);
        }
    }
}

void build_request_init(BuildRequest *request) {
    request->compiler = "clang";
    request->standard = mazen_standard_default();
    request->mode = MAZEN_BUILD_DEBUG;
    request->run_after_build = false;
    request->verbose = false;
    request->binary_name = NULL;
    string_list_init(&request->libs);
    string_list_init(&request->cflags);
    string_list_init(&request->ldflags);
    string_list_init(&request->run_args);
}

void build_request_free(BuildRequest *request) {
    free(request->binary_name);
    string_list_free(&request->libs);
    string_list_free(&request->cflags);
    string_list_free(&request->ldflags);
    string_list_free(&request->run_args);
    build_request_init(request);
}

bool build_clean(const char *root_dir, Diagnostic *diag) {
    char *build_dir = path_join(root_dir, "build");
    int rc;

    if (!dir_exists(build_dir)) {
        diag_info("No build directory to clean");
        free(build_dir);
        return true;
    }

    rc = remove_tree(build_dir);
    if (rc != 0) {
        diag_set(diag, "failed to remove `%s`: %s", build_dir, strerror(errno));
        free(build_dir);
        return false;
    }

    diag_info("Removed build artifacts");
    free(build_dir);
    return true;
}

static void create_build_units(const ProjectInfo *project, BuildUnitList *units) {
    StringList used;
    size_t i;

    string_list_init(&used);

    for (i = 0; i < project->sources.len; ++i) {
        BuildUnit *unit;
        char *stem;
        char *unique;

        if (!include_source_for_target(project, i)) {
            continue;
        }

        unit = build_unit_list_push(units);
        unit->source = &project->sources.items[i];
        stem = sanitize_stem(unit->source->path);
        unique = mazen_xstrdup(stem);
        if (string_list_contains(&used, unique)) {
            uint64_t hash = fnv1a_hash_string(unit->source->path, UINT64_C(1469598103934665603));
            free(unique);
            unique = mazen_format("%s_%08llx", stem, (unsigned long long) (hash & 0xffffffffu));
        }
        string_list_push_unique(&used, unique);
        unit->object_rel = mazen_format("build/obj/%s.o", unique);
        unit->dep_rel = mazen_format("build/obj/%s.d", unique);
        free(unique);
        free(stem);
    }

    string_list_free(&used);
}

static bool prepare_paths(const ProjectInfo *project, const BuildRequest *request, BuildUnitList *units,
                          char **binary_rel, char **binary_abs, Diagnostic *diag) {
    size_t i;
    char *obj_dir = path_join(project->root_dir, "build/obj");
    char *cache_dir = path_join(project->root_dir, "build/.mazen");

    if (ensure_directory(obj_dir) != 0 || ensure_directory(cache_dir) != 0) {
        diag_set(diag, "failed to create build directories");
        free(obj_dir);
        free(cache_dir);
        return false;
    }

    for (i = 0; i < units->len; ++i) {
        units->items[i].object_abs = path_join(project->root_dir, units->items[i].object_rel);
        units->items[i].dep_abs = path_join(project->root_dir, units->items[i].dep_rel);
    }

    *binary_rel = mazen_format("build/%s", request->binary_name);
    *binary_abs = path_join(project->root_dir, *binary_rel);
    free(obj_dir);
    free(cache_dir);
    return true;
}

static long long dep_mtime(const ProjectInfo *project, const char *dep) {
    if (dep[0] == '/') {
        return file_mtime_ns(dep);
    }
    {
        char *absolute = path_join(project->root_dir, dep);
        long long mtime = file_mtime_ns(absolute);
        free(absolute);
        return mtime;
    }
}

static bool object_is_stale(const ProjectInfo *project, const BuildUnit *unit, bool flags_changed) {
    long long object_time = file_mtime_ns(unit->object_abs);
    long long source_time;
    size_t i;

    if (flags_changed || object_time < 0 || !file_exists(unit->dep_abs)) {
        return true;
    }
    if (unit->deps.len == 0) {
        return true;
    }

    source_time = dep_mtime(project, unit->source->path);
    if (source_time < 0 || source_time > object_time) {
        return true;
    }

    for (i = 0; i < unit->deps.len; ++i) {
        long long mtime = dep_mtime(project, unit->deps.items[i]);
        if (mtime < 0 || mtime > object_time) {
            return true;
        }
    }

    return false;
}

static void add_common_compile_flags(StringList *args, const BuildRequest *request, const ProjectInfo *project,
                                     const BuildUnit *unit) {
    size_t i;
    string_list_push(args, request->compiler);
    string_list_push_take(args, mazen_standard_flag(request->standard));
    string_list_push(args, "-Wall");
    string_list_push(args, "-Wextra");
    if (request->mode == MAZEN_BUILD_RELEASE) {
        string_list_push(args, "-O2");
    } else {
        string_list_push(args, "-g");
        string_list_push(args, "-O0");
    }
    for (i = 0; i < request->cflags.len; ++i) {
        string_list_push(args, request->cflags.items[i]);
    }
    for (i = 0; i < project->include_dirs.len; ++i) {
        string_list_push(args, "-I");
        string_list_push(args, project->include_dirs.items[i]);
    }
    string_list_push(args, "-MMD");
    string_list_push(args, "-MF");
    string_list_push(args, unit->dep_rel);
    string_list_push(args, "-c");
    string_list_push(args, unit->source->path);
    string_list_push(args, "-o");
    string_list_push(args, unit->object_rel);
}

static const char *linker_hint(const char *output) {
    if (strstr(output, "undefined reference to `sqrt") != NULL ||
        strstr(output, "undefined reference to `sin") != NULL ||
        strstr(output, "undefined reference to `cos") != NULL ||
        strstr(output, "undefined reference to `pow") != NULL) {
        return "Try linking the math library:\n    mazen --lib m";
    }
    if (strstr(output, "undefined reference to `pthread_") != NULL ||
        strstr(output, "undefined reference to `sem_") != NULL) {
        return "Try linking pthread support:\n    mazen --lib pthread";
    }
    return NULL;
}

static bool compile_unit(const ProjectInfo *project, const BuildRequest *request, BuildUnit *unit, Diagnostic *diag) {
    StringList args;
    String output;
    int exit_code = 0;

    string_list_init(&args);
    string_init(&output);
    add_common_compile_flags(&args, request, project, unit);

    if (request->verbose) {
        char *command = format_command(&args);
        diag_note("%s", command);
        free(command);
    }
    diag_info("Compiling %s", unit->source->path);

    if (!run_command_capture(project->root_dir, &args, true, &exit_code, &output, diag)) {
        string_list_free(&args);
        string_free(&output);
        return false;
    }
    if (exit_code != 0) {
        diag_set(diag, "compilation failed for `%s`", unit->source->path);
        string_list_free(&args);
        string_free(&output);
        return false;
    }

    string_list_free(&args);
    string_free(&output);
    return true;
}

static bool refresh_unit_dependencies(BuildUnit *unit, Diagnostic *diag) {
    string_list_clear(&unit->deps);
    if (!cache_parse_depfile(unit->dep_abs, &unit->deps)) {
        diag_set(diag, "failed to read dependency file `%s`", unit->dep_rel);
        return false;
    }
    return true;
}

static bool link_target(const ProjectInfo *project, const BuildRequest *request, const BuildUnitList *units,
                        const char *binary_rel, Diagnostic *diag) {
    StringList args;
    String output;
    size_t i;
    int exit_code = 0;

    string_list_init(&args);
    string_init(&output);
    string_list_push(&args, request->compiler);
    for (i = 0; i < units->len; ++i) {
        string_list_push(&args, units->items[i].object_rel);
    }
    string_list_push(&args, "-o");
    string_list_push(&args, binary_rel);
    for (i = 0; i < request->ldflags.len; ++i) {
        string_list_push(&args, request->ldflags.items[i]);
    }
    for (i = 0; i < request->libs.len; ++i) {
        string_list_push_take(&args, mazen_format("-l%s", request->libs.items[i]));
    }

    if (request->verbose) {
        char *command = format_command(&args);
        diag_note("%s", command);
        free(command);
    }

    diag_info("Linking %s", binary_rel);
    if (!run_command_capture(project->root_dir, &args, true, &exit_code, &output, diag)) {
        string_list_free(&args);
        string_free(&output);
        return false;
    }
    if (exit_code != 0) {
        const char *hint = linker_hint(output.data == NULL ? "" : output.data);
        diag_set(diag, "linker failed while producing `%s`", binary_rel);
        if (hint != NULL) {
            diag_set_hint(diag, "%s", hint);
        }
        string_list_free(&args);
        string_free(&output);
        return false;
    }

    string_list_free(&args);
    string_free(&output);
    return true;
}

static void cache_update_record(const ProjectInfo *project, CacheState *state, const BuildUnit *unit) {
    BuildRecord *record = cache_upsert_record(state, unit->source->path);
    size_t i;

    free(record->object_path);
    free(record->dep_path);
    record->object_path = mazen_xstrdup(unit->object_rel);
    record->dep_path = mazen_xstrdup(unit->dep_rel);
    record->source_mtime_ns = dep_mtime(project, unit->source->path);
    record->object_mtime_ns = file_mtime_ns(unit->object_abs);
    string_list_clear(&record->deps);
    for (i = 0; i < unit->deps.len; ++i) {
        string_list_push(&record->deps, unit->deps.items[i]);
    }
}

static bool load_cached_dependencies(const CacheState *state, BuildUnit *unit) {
    size_t i;
    for (i = 0; i < state->records.len; ++i) {
        const BuildRecord *record = &state->records.items[i];
        if (record->source_path != NULL && strcmp(record->source_path, unit->source->path) == 0) {
            size_t j;
            for (j = 0; j < record->deps.len; ++j) {
                string_list_push(&unit->deps, record->deps.items[j]);
            }
            return true;
        }
    }
    return false;
}

static bool binary_is_stale(const BuildUnitList *units, const char *binary_abs, bool link_flags_changed) {
    long long binary_time = file_mtime_ns(binary_abs);
    size_t i;

    if (link_flags_changed || binary_time < 0) {
        return true;
    }
    for (i = 0; i < units->len; ++i) {
        if (file_mtime_ns(units->items[i].object_abs) > binary_time) {
            return true;
        }
    }
    return false;
}

static bool run_built_program(const ProjectInfo *project, const BuildRequest *request, const char *binary_rel,
                              Diagnostic *diag) {
    StringList args;

    string_list_init(&args);
    string_list_push(&args, binary_rel);
    build_request_copy_list(&args, &request->run_args, false);
    diag_info("Running %s", binary_rel);
    fflush(stdout);
    fflush(stderr);
    if (!run_command_passthrough(project->root_dir, &args, diag)) {
        string_list_free(&args);
        return false;
    }
    string_list_free(&args);
    return true;
}

bool build_project(const ProjectInfo *project, const MazenConfig *config, const BuildRequest *request,
                   Diagnostic *diag) {
    CacheState state;
    BuildUnitList units;
    char *binary_rel = NULL;
    char *binary_abs = NULL;
    char *cache_path = NULL;
    char *compile_sig = NULL;
    char *link_sig = NULL;
    bool compile_flags_changed;
    bool link_flags_changed;
    bool any_compiled = false;
    bool did_link = false;
    size_t i;
    (void) config;

    if (project->sources.len == 0) {
        diag_set(diag, "no C source files were found in `%s`", project->root_dir);
        return false;
    }
    if (!project->has_selected_entry) {
        diag_set(diag, "no entrypoint containing `main()` was detected");
        diag_set_hint(diag, "Run `mazen doctor` to inspect discovery results");
        return false;
    }

    diag_info("Scanning project...");
    diag_info("Found %zu source files", project->sources.len);
    if (project->multiple_entrypoints) {
        puts("Multiple entrypoints detected:");
        for (i = 0; i < project->sources.len; ++i) {
            if (project->sources.items[i].has_main) {
                printf("    %s\n", project->sources.items[i].path);
            }
        }
        printf("Selected: %s\n", project->sources.items[project->selected_entry].path);
    } else {
        diag_info("Detected entrypoint: %s", project->sources.items[project->selected_entry].path);
    }
    print_include_dirs(project);
    diag_info("Building %s target", request->mode == MAZEN_BUILD_RELEASE ? "release" : "debug");

    build_unit_list_init(&units);
    cache_state_init(&state);
    create_build_units(project, &units);
    if (!prepare_paths(project, request, &units, &binary_rel, &binary_abs, diag)) {
        build_unit_list_free(&units);
        cache_state_free(&state);
        return false;
    }

    cache_path = path_join(project->root_dir, "build/.mazen/state.txt");
    if (!cache_load(cache_path, &state, diag)) {
        free(binary_rel);
        free(binary_abs);
        free(cache_path);
        build_unit_list_free(&units);
        cache_state_free(&state);
        return false;
    }

    make_signatures(project, request, &compile_sig, &link_sig);
    compile_flags_changed = state.compile_signature == NULL || strcmp(state.compile_signature, compile_sig) != 0;
    link_flags_changed = state.link_signature == NULL || strcmp(state.link_signature, link_sig) != 0;
    free(state.compile_signature);
    free(state.link_signature);
    state.compile_signature = mazen_xstrdup(compile_sig);
    state.link_signature = mazen_xstrdup(link_sig);

    if (compile_flags_changed) {
        diag_note("Compiler flags changed, invalidating object cache");
    }

    for (i = 0; i < units.len; ++i) {
        BuildUnit *unit = &units.items[i];
        if (!load_cached_dependencies(&state, unit) && file_exists(unit->dep_abs)) {
            cache_parse_depfile(unit->dep_abs, &unit->deps);
        }

        if (object_is_stale(project, unit, compile_flags_changed)) {
            if (!compile_unit(project, request, unit, diag)) {
                free(binary_rel);
                free(binary_abs);
                free(cache_path);
                free(compile_sig);
                free(link_sig);
                build_unit_list_free(&units);
                cache_state_free(&state);
                return false;
            }
            if (!refresh_unit_dependencies(unit, diag)) {
                free(binary_rel);
                free(binary_abs);
                free(cache_path);
                free(compile_sig);
                free(link_sig);
                build_unit_list_free(&units);
                cache_state_free(&state);
                return false;
            }
            any_compiled = true;
        } else {
            diag_note("Up to date: %s", unit->source->path);
        }
        cache_update_record(project, &state, unit);
    }

    if (binary_is_stale(&units, binary_abs, link_flags_changed || any_compiled)) {
        if (!link_target(project, request, &units, binary_rel, diag)) {
            free(binary_rel);
            free(binary_abs);
            free(cache_path);
            free(compile_sig);
            free(link_sig);
            build_unit_list_free(&units);
            cache_state_free(&state);
            return false;
        }
        did_link = true;
    } else {
        diag_note("Executable is up to date");
    }

    if (!cache_save(cache_path, &state, diag)) {
        free(binary_rel);
        free(binary_abs);
        free(cache_path);
        free(compile_sig);
        free(link_sig);
        build_unit_list_free(&units);
        cache_state_free(&state);
        return false;
    }

    if (!any_compiled && !did_link) {
        diag_info("Nothing to rebuild");
    }

    if (request->run_after_build && !run_built_program(project, request, binary_rel, diag)) {
        free(binary_rel);
        free(binary_abs);
        free(cache_path);
        free(compile_sig);
        free(link_sig);
        build_unit_list_free(&units);
        cache_state_free(&state);
        return false;
    }

    free(binary_rel);
    free(binary_abs);
    free(cache_path);
    free(compile_sig);
    free(link_sig);
    build_unit_list_free(&units);
    cache_state_free(&state);
    return true;
}
