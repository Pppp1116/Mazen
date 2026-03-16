#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "build.h"

#include "autolib.h"
#include "compdb.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    const SourceFile *source;
    char *source_abs;
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

typedef struct {
    BuildUnit *unit;
    StringList args;
} CompileTask;

typedef struct {
    CompileTask *items;
    size_t len;
    size_t cap;
} CompileTaskList;

typedef struct {
    pid_t pid;
    CompileTask *task;
} RunningCompile;

static void build_unit_init(BuildUnit *unit) {
    unit->source = NULL;
    unit->source_abs = NULL;
    unit->object_rel = NULL;
    unit->dep_rel = NULL;
    unit->object_abs = NULL;
    unit->dep_abs = NULL;
    string_list_init(&unit->deps);
}

static void build_unit_free(BuildUnit *unit) {
    free(unit->source_abs);
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

static void compile_task_init(CompileTask *task) {
    task->unit = NULL;
    string_list_init(&task->args);
}

static void compile_task_free(CompileTask *task) {
    string_list_free(&task->args);
    compile_task_init(task);
}

static void compile_task_list_init(CompileTaskList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static void compile_task_list_free(CompileTaskList *list) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        compile_task_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static CompileTask *compile_task_list_push(CompileTaskList *list) {
    size_t cap;

    if (list->len == list->cap) {
        cap = list->cap == 0 ? 8 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }

    compile_task_init(&list->items[list->len]);
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

static char *library_path_value(const StringList *dirs) {
    const char *existing = getenv("LD_LIBRARY_PATH");
    String value;
    size_t i;

    if (dirs->len == 0) {
        return existing != NULL && *existing != '\0' ? mazen_xstrdup(existing) : NULL;
    }

    string_init(&value);
    for (i = 0; i < dirs->len; ++i) {
        if (i > 0) {
            string_append_char(&value, ':');
        }
        string_append(&value, dirs->items[i]);
    }
    if (existing != NULL && *existing != '\0') {
        if (value.len > 0) {
            string_append_char(&value, ':');
        }
        string_append(&value, existing);
    }

    return string_take(&value);
}

static bool run_command_passthrough_env(const char *cwd, const StringList *args, const char *library_path,
                                        Diagnostic *diag) {
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
        if (library_path != NULL) {
            setenv("LD_LIBRARY_PATH", library_path, 1);
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

static bool spawn_command(const char *cwd, const StringList *args, pid_t *pid_out, Diagnostic *diag) {
    pid_t child;
    char **argv;

    fflush(stdout);
    fflush(stderr);

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

    *pid_out = child;
    return true;
}

static bool spawn_command_env(const char *cwd, const StringList *args, const char *library_path, pid_t *pid_out,
                              Diagnostic *diag) {
    pid_t child;
    char **argv;

    fflush(stdout);
    fflush(stderr);

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
        if (library_path != NULL) {
            setenv("LD_LIBRARY_PATH", library_path, 1);
        }
        execvp(argv[0], argv);
        fprintf(stderr, "failed to execute %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    *pid_out = child;
    return true;
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

static char *format_item_list(const StringList *items) {
    String text;
    size_t i;

    string_init(&text);
    for (i = 0; i < items->len; ++i) {
        if (i > 0) {
            string_append(&text, ", ");
        }
        string_append(&text, items->items[i]);
    }
    return string_take(&text);
}

static bool path_is_absolute(const char *path) {
    return path != NULL && path[0] == '/';
}

static char *resolve_path(const char *root_dir, const char *path) {
    if (path_is_absolute(path)) {
        return mazen_xstrdup(path);
    }
    return path_join(root_dir, path);
}

static const char *path_after_prefix_dir(const char *path, const char *prefix) {
    size_t len;

    if (path == NULL || prefix == NULL) {
        return NULL;
    }

    len = strlen(prefix);
    if (strcmp(path, prefix) == 0) {
        return path + len;
    }
    if (string_starts_with(path, prefix) && path[len] == '/') {
        return path + len + 1;
    }
    return NULL;
}

static bool ensure_parent_directory(const char *root_dir, const char *path, Diagnostic *diag) {
    char *dir_name = path_dirname(path);
    char *dir_path = path_is_absolute(path) ? mazen_xstrdup(dir_name) : path_join(root_dir, dir_name);
    int rc = ensure_directory(dir_path);

    free(dir_name);
    free(dir_path);
    if (rc != 0) {
        diag_set(diag, "failed to create directory for `%s`", path);
        return false;
    }
    return true;
}

static int effective_job_count(const BuildRequest *request) {
    long detected;

    if (request->jobs > 0) {
        return request->jobs;
    }

    detected = sysconf(_SC_NPROCESSORS_ONLN);
    if (detected < 1) {
        return 1;
    }
    return (int) detected;
}

static void print_include_dirs(const BuildRequest *request) {
    size_t i;

    puts("Include directories:");
    if (request->include_dirs.len == 0) {
        puts("    (none)");
        return;
    }
    for (i = 0; i < request->include_dirs.len; ++i) {
        printf("    %s\n", request->include_dirs.items[i]);
    }
}

static const SourceFile *project_find_source(const ProjectInfo *project, const char *path) {
    size_t i;

    for (i = 0; i < project->sources.len; ++i) {
        if (strcmp(project->sources.items[i].path, path) == 0) {
            return &project->sources.items[i];
        }
    }

    return NULL;
}

static void make_signatures(const BuildRequest *request, const StringList *link_libs, char **compile_sig,
                            char **link_sig) {
    String compile;
    String link;
    size_t i;
    uint64_t compile_hash;
    uint64_t link_hash;

    string_init(&compile);
    string_init(&link);

    string_appendf(&compile, "compiler=%s\nmode=%d\nstandard=%s\ntype=%d\n", request->compiler,
                   (int) request->mode,
                   request->standard != NULL ? request->standard->name : mazen_standard_default()->name,
                   (int) request->target_type);
    string_appendf(&link, "compiler=%s\nmode=%d\ntype=%d\noutput=%s\n", request->compiler, (int) request->mode,
                   (int) request->target_type, request->output_path != NULL ? request->output_path : "");
    if (request->profile_name != NULL) {
        string_appendf(&compile, "profile=%s\n", request->profile_name);
        string_appendf(&link, "profile=%s\n", request->profile_name);
    }

    if (request->entry_path != NULL) {
        string_appendf(&link, "entry=%s\n", request->entry_path);
    }
    for (i = 0; i < request->include_dirs.len; ++i) {
        string_appendf(&compile, "I=%s\n", request->include_dirs.items[i]);
    }
    for (i = 0; i < request->cflags.len; ++i) {
        string_appendf(&compile, "C=%s\n", request->cflags.items[i]);
    }
    for (i = 0; i < request->ldflags.len; ++i) {
        string_appendf(&link, "F=%s\n", request->ldflags.items[i]);
    }
    for (i = 0; i < link_libs->len; ++i) {
        string_appendf(&link, "L=%s\n", link_libs->items[i]);
    }
    for (i = 0; i < request->dependency_outputs.len; ++i) {
        string_appendf(&link, "D=%s\n", request->dependency_outputs.items[i]);
    }
    for (i = 0; i < request->source_paths.len; ++i) {
        string_appendf(&compile, "S=%s\n", request->source_paths.items[i]);
        string_appendf(&link, "S=%s\n", request->source_paths.items[i]);
    }

    compile_hash = fnv1a_hash_string(compile.data == NULL ? "" : compile.data, UINT64_C(1469598103934665603));
    link_hash = fnv1a_hash_string(link.data == NULL ? "" : link.data, UINT64_C(1469598103934665603));

    *compile_sig = hex_u64(compile_hash);
    *link_sig = hex_u64(link_hash);
    string_free(&compile);
    string_free(&link);
}

void build_request_init(BuildRequest *request) {
    request->compiler = "clang";
    request->standard = mazen_standard_default();
    request->mode = MAZEN_BUILD_DEBUG;
    request->profile_name = NULL;
    request->run_after_build = false;
    request->verbose = false;
    request->jobs = 0;
    request->target_name = NULL;
    request->target_type = MAZEN_TARGET_EXECUTABLE;
    request->output_path = NULL;
    request->entry_path = NULL;
    request->compile_commands_path = NULL;
    request->write_compile_commands = true;
    string_list_init(&request->source_paths);
    string_list_init(&request->include_dirs);
    string_list_init(&request->libs);
    string_list_init(&request->cflags);
    string_list_init(&request->ldflags);
    string_list_init(&request->dependency_outputs);
    string_list_init(&request->runtime_library_dirs);
    string_list_init(&request->run_args);
}

void build_request_free(BuildRequest *request) {
    free(request->target_name);
    free(request->output_path);
    free(request->entry_path);
    free(request->compile_commands_path);
    string_list_free(&request->source_paths);
    string_list_free(&request->include_dirs);
    string_list_free(&request->libs);
    string_list_free(&request->cflags);
    string_list_free(&request->ldflags);
    string_list_free(&request->dependency_outputs);
    string_list_free(&request->runtime_library_dirs);
    string_list_free(&request->run_args);
    build_request_init(request);
}

static bool remove_optional_path(const char *root_dir, const char *path, Diagnostic *diag) {
    char *absolute;
    int rc;

    if (path == NULL || *path == '\0') {
        return true;
    }

    absolute = resolve_path(root_dir, path);
    if (!file_exists(absolute) && !dir_exists(absolute)) {
        free(absolute);
        return true;
    }

    rc = remove_tree(absolute);
    if (rc != 0) {
        diag_set(diag, "failed to remove `%s`: %s", absolute, strerror(errno));
        free(absolute);
        return false;
    }

    free(absolute);
    return true;
}

static bool path_exists_resolved(const char *root_dir, const char *path) {
    char *absolute;
    bool exists;

    if (path == NULL || *path == '\0') {
        return false;
    }

    absolute = resolve_path(root_dir, path);
    exists = file_exists(absolute) || dir_exists(absolute);
    free(absolute);
    return exists;
}

static bool remove_optional_and_track(const char *root_dir, const char *path, bool *removed_any, Diagnostic *diag) {
    bool existed = path_exists_resolved(root_dir, path);

    if (!remove_optional_path(root_dir, path, diag)) {
        return false;
    }
    if (existed) {
        *removed_any = true;
    }
    return true;
}

static bool clean_build_outputs_dir(const char *root_dir, bool *removed_any, Diagnostic *diag) {
    char *build_dir = path_join(root_dir, "build");
    DIR *dir;
    struct dirent *entry;

    if (!dir_exists(build_dir)) {
        free(build_dir);
        return true;
    }

    dir = opendir(build_dir);
    if (dir == NULL) {
        diag_set(diag, "failed to read `%s`: %s", build_dir, strerror(errno));
        free(build_dir);
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *child;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, "obj") == 0 || strcmp(entry->d_name, ".mazen") == 0) {
            continue;
        }

        child = path_join(build_dir, entry->d_name);
        if (remove_tree(child) != 0) {
            diag_set(diag, "failed to remove `%s`: %s", child, strerror(errno));
            free(child);
            closedir(dir);
            free(build_dir);
            return false;
        }
        *removed_any = true;
        free(child);
    }

    closedir(dir);
    free(build_dir);
    return true;
}

static bool remove_target_outputs(const char *root_dir, const MazenConfig *config, bool *removed_any, Diagnostic *diag) {
    size_t i;

    if (config == NULL) {
        return true;
    }

    for (i = 0; i < config->targets.len; ++i) {
        const MazenTargetConfig *target = &config->targets.items[i];
        if (target->output == NULL) {
            continue;
        }
        if (!remove_optional_and_track(root_dir, target->output, removed_any, diag)) {
            return false;
        }
    }

    return true;
}

bool build_clean(const char *root_dir, const MazenConfig *config, unsigned int clean_mask, Diagnostic *diag) {
    bool removed_any = false;
    unsigned int mask = clean_mask == 0 ? MAZEN_CLEAN_ALL : clean_mask;
    const char *compdb_path;

    compdb_path = config != NULL && config->compile_commands_path != NULL ? config->compile_commands_path
                                                                          : "compile_commands.json";

    if ((mask & MAZEN_CLEAN_ALL) == MAZEN_CLEAN_ALL) {
        char *build_dir = path_join(root_dir, "build");

        if (dir_exists(build_dir)) {
            if (remove_tree(build_dir) != 0) {
                diag_set(diag, "failed to remove `%s`: %s", build_dir, strerror(errno));
                free(build_dir);
                return false;
            }
            removed_any = true;
        }
        free(build_dir);

        if (!remove_target_outputs(root_dir, config, &removed_any, diag)) {
            return false;
        }
        if (!remove_optional_and_track(root_dir, compdb_path, &removed_any, diag)) {
            return false;
        }
    } else {
        if ((mask & MAZEN_CLEAN_OBJECTS) != 0 &&
            !remove_optional_and_track(root_dir, "build/obj", &removed_any, diag)) {
            return false;
        }
        if ((mask & MAZEN_CLEAN_CACHE) != 0 &&
            !remove_optional_and_track(root_dir, "build/.mazen", &removed_any, diag)) {
            return false;
        }
        if ((mask & MAZEN_CLEAN_OUTPUTS) != 0) {
            if (!clean_build_outputs_dir(root_dir, &removed_any, diag)) {
                return false;
            }
            if (!remove_target_outputs(root_dir, config, &removed_any, diag)) {
                return false;
            }
        }
        if ((mask & MAZEN_CLEAN_COMPDB) != 0 &&
            !remove_optional_and_track(root_dir, compdb_path, &removed_any, diag)) {
            return false;
        }
    }

    if (removed_any) {
        diag_info("Removed build artifacts");
    } else {
        diag_info("No build artifacts to clean");
    }
    return true;
}

static bool create_build_units(const ProjectInfo *project, const BuildRequest *request, BuildUnitList *units,
                               Diagnostic *diag) {
    StringList used;
    char *namespace_stem = NULL;
    size_t i;

    string_list_init(&used);

    if (request->source_paths.len == 0) {
        string_list_free(&used);
        diag_set(diag, "target `%s` did not resolve any source files",
                 request->target_name != NULL ? request->target_name : "(unnamed)");
        return false;
    }

    namespace_stem = sanitize_stem(request->target_name != NULL ? request->target_name : "default");
    for (i = 0; i < request->source_paths.len; ++i) {
        const SourceFile *source = project_find_source(project, request->source_paths.items[i]);
        BuildUnit *unit;
        char *stem;
        char *unique;

        if (source == NULL) {
            free(namespace_stem);
            string_list_free(&used);
            diag_set(diag, "target source `%s` was not found during build planning", request->source_paths.items[i]);
            return false;
        }

        unit = build_unit_list_push(units);
        unit->source = source;
        stem = sanitize_stem(source->path);
        unique = mazen_xstrdup(stem);
        if (string_list_contains(&used, unique)) {
            uint64_t hash = fnv1a_hash_string(source->path, UINT64_C(1469598103934665603));
            free(unique);
            unique = mazen_format("%s_%08llx", stem, (unsigned long long) (hash & 0xffffffffu));
        }
        string_list_push_unique(&used, unique);
        unit->object_rel = mazen_format("build/obj/%s/%s.o", namespace_stem, unique);
        unit->dep_rel = mazen_format("build/obj/%s/%s.d", namespace_stem, unique);
        free(unique);
        free(stem);
    }

    free(namespace_stem);
    string_list_free(&used);
    return true;
}

static bool prepare_paths(const ProjectInfo *project, const BuildRequest *request, BuildUnitList *units,
                          char **output_rel, char **output_abs, char **cache_rel, char **cache_abs,
                          char **compdb_rel, char **compdb_abs, Diagnostic *diag) {
    char *cache_name = sanitize_stem(request->target_name != NULL ? request->target_name : "default");
    char *cache_dir = path_join(project->root_dir, "build/.mazen");
    size_t i;

    if (ensure_directory(cache_dir) != 0) {
        diag_set(diag, "failed to create build cache directory");
        free(cache_name);
        free(cache_dir);
        return false;
    }
    free(cache_dir);

    for (i = 0; i < units->len; ++i) {
        char *object_dir = path_dirname(units->items[i].object_rel);
        char *object_dir_abs = path_join(project->root_dir, object_dir);
        int rc = ensure_directory(object_dir_abs);

        free(object_dir);
        free(object_dir_abs);
        if (rc != 0) {
            diag_set(diag, "failed to create object directory");
            free(cache_name);
            return false;
        }

        units->items[i].source_abs = path_join(project->root_dir, units->items[i].source->path);
        units->items[i].object_abs = path_join(project->root_dir, units->items[i].object_rel);
        units->items[i].dep_abs = path_join(project->root_dir, units->items[i].dep_rel);
    }

    if (!ensure_parent_directory(project->root_dir, request->output_path, diag)) {
        free(cache_name);
        return false;
    }

    *output_rel = mazen_xstrdup(request->output_path);
    *output_abs = resolve_path(project->root_dir, request->output_path);
    *cache_rel = mazen_format("build/.mazen/%s.state.txt", cache_name);
    *cache_abs = path_join(project->root_dir, *cache_rel);

    if (request->compile_commands_path != NULL) {
        if (!ensure_parent_directory(project->root_dir, request->compile_commands_path, diag)) {
            free(cache_name);
            return false;
        }
        *compdb_rel = mazen_xstrdup(request->compile_commands_path);
        *compdb_abs = resolve_path(project->root_dir, request->compile_commands_path);
    }

    free(cache_name);
    return true;
}

static long long dep_mtime(const char *root_dir, const char *dep) {
    if (path_is_absolute(dep)) {
        return file_mtime_ns(dep);
    }

    {
        char *absolute = path_join(root_dir, dep);
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

    source_time = dep_mtime(project->root_dir, unit->source->path);
    if (source_time < 0 || source_time > object_time) {
        return true;
    }

    for (i = 0; i < unit->deps.len; ++i) {
        long long mtime = dep_mtime(project->root_dir, unit->deps.items[i]);
        if (mtime < 0 || mtime > object_time) {
            return true;
        }
    }

    return false;
}

static void add_compile_args(StringList *args, const BuildRequest *request, const BuildUnit *unit) {
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
    if (request->target_type == MAZEN_TARGET_SHARED_LIBRARY) {
        string_list_push(args, "-fPIC");
    }
    build_request_copy_list(args, &request->cflags, false);
    for (i = 0; i < request->include_dirs.len; ++i) {
        string_list_push(args, "-I");
        string_list_push(args, request->include_dirs.items[i]);
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

static void add_link_args(StringList *args, const BuildRequest *request, const BuildUnitList *units,
                          const char *output_rel, const StringList *libs) {
    size_t i;

    string_list_push(args, request->compiler);
    if (request->target_type == MAZEN_TARGET_SHARED_LIBRARY) {
        string_list_push(args, "-shared");
    }
    for (i = 0; i < units->len; ++i) {
        string_list_push(args, units->items[i].object_rel);
    }
    for (i = 0; i < request->dependency_outputs.len; ++i) {
        string_list_push(args, request->dependency_outputs.items[i]);
    }
    string_list_push(args, "-o");
    string_list_push(args, output_rel);
    build_request_copy_list(args, &request->ldflags, false);
    for (i = 0; i < libs->len; ++i) {
        string_list_push_take(args, mazen_format("-l%s", libs->items[i]));
    }
}

static bool refresh_unit_dependencies(BuildUnit *unit, Diagnostic *diag) {
    string_list_clear(&unit->deps);
    if (!cache_parse_depfile(unit->dep_abs, &unit->deps)) {
        diag_set(diag, "failed to read dependency file `%s`", unit->dep_rel);
        return false;
    }
    return true;
}

static void cache_update_record(const ProjectInfo *project, CacheState *state, const BuildUnit *unit) {
    BuildRecord *record = cache_upsert_record(state, unit->source->path);
    size_t i;

    free(record->object_path);
    free(record->dep_path);
    record->object_path = mazen_xstrdup(unit->object_rel);
    record->dep_path = mazen_xstrdup(unit->dep_rel);
    record->source_mtime_ns = dep_mtime(project->root_dir, unit->source->path);
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

static bool output_is_stale(const BuildUnitList *units, const BuildRequest *request, const char *output_abs,
                            bool link_flags_changed) {
    long long output_time = file_mtime_ns(output_abs);
    size_t i;

    if (link_flags_changed || output_time < 0) {
        return true;
    }
    for (i = 0; i < units->len; ++i) {
        if (file_mtime_ns(units->items[i].object_abs) > output_time) {
            return true;
        }
    }
    for (i = 0; i < request->dependency_outputs.len; ++i) {
        if (dep_mtime("", request->dependency_outputs.items[i]) > output_time) {
            return true;
        }
    }
    return false;
}

static const BuildRecord *find_cached_record_const(const CacheState *state, const char *source_path) {
    size_t i;

    for (i = 0; i < state->records.len; ++i) {
        const BuildRecord *record = &state->records.items[i];
        if (record->source_path != NULL && strcmp(record->source_path, source_path) == 0) {
            return record;
        }
    }

    return NULL;
}

static bool auto_lib_cache_is_fresh(const ProjectInfo *project, const BuildRequest *request, const CacheState *state) {
    size_t i;

    if (!state->auto_libs_valid) {
        return false;
    }

    for (i = 0; i < request->source_paths.len; ++i) {
        const char *source_path = request->source_paths.items[i];
        const BuildRecord *record = find_cached_record_const(state, source_path);

        if (record == NULL || record->source_mtime_ns != dep_mtime(project->root_dir, source_path)) {
            return false;
        }
    }

    return true;
}

static void add_compdb_entry(CompDbEntryList *list, const ProjectInfo *project, const BuildUnit *unit,
                             const StringList *args) {
    if (list == NULL) {
        return;
    }

    CompDbEntry *entry = compdb_entry_list_push(list);

    entry->directory = mazen_xstrdup(project->root_dir);
    entry->file = mazen_xstrdup(unit->source_abs);
    entry->command = format_command(args);
    entry->output = mazen_xstrdup(unit->object_abs);
}

static ssize_t find_running_slot(const RunningCompile *running, size_t count, pid_t pid) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (running[i].pid == pid) {
            return (ssize_t) i;
        }
    }
    return -1;
}

static ssize_t find_free_slot(const RunningCompile *running, size_t count) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (running[i].pid == 0) {
            return (ssize_t) i;
        }
    }
    return -1;
}

static void clear_running_slot(RunningCompile *slot) {
    slot->pid = 0;
    if (slot->task != NULL) {
        compile_task_free(slot->task);
        slot->task = NULL;
    }
}

static bool wait_for_compile(const ProjectInfo *project, RunningCompile *running, size_t slot_count, size_t *active_count,
                             bool *failed, Diagnostic *diag) {
    int status;
    pid_t pid;
    ssize_t slot_index;
    RunningCompile *slot;

    for (;;) {
        pid = waitpid(-1, &status, 0);
        if (pid >= 0) {
            break;
        }
        if (errno != EINTR) {
            diag_set(diag, "failed waiting for compiler process: %s", strerror(errno));
            return false;
        }
    }

    slot_index = find_running_slot(running, slot_count, pid);
    if (slot_index < 0) {
        diag_set(diag, "internal error: unknown compiler process finished");
        return false;
    }

    slot = &running[slot_index];
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        if (!refresh_unit_dependencies(slot->task->unit, diag)) {
            return false;
        }
    } else {
        if (!*failed) {
            diag_set(diag, "compilation failed for `%s`", slot->task->unit->source->path);
        }
        *failed = true;
    }

    clear_running_slot(slot);
    --(*active_count);
    (void) project;
    return true;
}

static bool compile_units(const ProjectInfo *project, const BuildRequest *request, BuildUnitList *units,
                          CacheState *state, bool compile_flags_changed, CompDbEntryList *compdb, bool *any_compiled,
                          Diagnostic *diag) {
    CompileTaskList tasks;
    size_t i;
    int jobs = effective_job_count(request);
    RunningCompile *running;
    size_t active_count = 0;
    size_t next_task = 0;
    bool failed = false;

    compile_task_list_init(&tasks);
    *any_compiled = false;

    for (i = 0; i < units->len; ++i) {
        BuildUnit *unit = &units->items[i];
        StringList args;
        bool stale;

        if (!load_cached_dependencies(state, unit) && file_exists(unit->dep_abs)) {
            cache_parse_depfile(unit->dep_abs, &unit->deps);
        }

        string_list_init(&args);
        add_compile_args(&args, request, unit);
        add_compdb_entry(compdb, project, unit, &args);

        stale = object_is_stale(project, unit, compile_flags_changed);
        if (stale) {
            CompileTask *task = compile_task_list_push(&tasks);
            task->unit = unit;
            task->args = args;
        } else {
            diag_note("Up to date: %s", unit->source->path);
            string_list_free(&args);
        }
    }

    if (tasks.len == 0) {
        for (i = 0; i < units->len; ++i) {
            cache_update_record(project, state, &units->items[i]);
        }
        compile_task_list_free(&tasks);
        return true;
    }

    running = mazen_xcalloc((size_t) jobs, sizeof(*running));
    while (next_task < tasks.len || active_count > 0) {
        while (!failed && next_task < tasks.len && active_count < (size_t) jobs) {
            ssize_t slot_index;
            CompileTask *task = &tasks.items[next_task++];
            char *command;

            slot_index = find_free_slot(running, (size_t) jobs);
            if (slot_index < 0) {
                break;
            }

            if (request->verbose) {
                command = format_command(&task->args);
                diag_note("%s", command);
                free(command);
            }
            diag_info("Compiling %s", task->unit->source->path);
            if (!spawn_command(project->root_dir, &task->args, &running[slot_index].pid, diag)) {
                failed = true;
                break;
            }
            running[slot_index].task = task;
            ++active_count;
            *any_compiled = true;
        }

        if (active_count == 0) {
            break;
        }
        if (!wait_for_compile(project, running, (size_t) jobs, &active_count, &failed, diag)) {
            failed = true;
            break;
        }
    }

    while (active_count > 0) {
        bool ignored_failed = failed;
        if (!wait_for_compile(project, running, (size_t) jobs, &active_count, &ignored_failed, diag) && !failed) {
            failed = true;
        }
    }

    for (i = 0; i < units->len; ++i) {
        cache_update_record(project, state, &units->items[i]);
    }

    free(running);
    compile_task_list_free(&tasks);
    return !failed;
}

static bool link_target(const ProjectInfo *project, const BuildRequest *request, const BuildUnitList *units,
                        const char *output_rel, const StringList *libs, Diagnostic *diag) {
    StringList args;
    String output;
    size_t i;
    int exit_code = 0;

    string_list_init(&args);
    string_init(&output);

    if (request->target_type == MAZEN_TARGET_STATIC_LIBRARY) {
        string_list_push(&args, "ar");
        string_list_push(&args, "rcs");
        string_list_push(&args, output_rel);
        for (i = 0; i < units->len; ++i) {
            string_list_push(&args, units->items[i].object_rel);
        }
        if (request->verbose) {
            char *command = format_command(&args);
            diag_note("%s", command);
            free(command);
        }
        diag_info("Archiving %s", output_rel);
    } else {
        add_link_args(&args, request, units, output_rel, libs);
        if (request->verbose) {
            char *command = format_command(&args);
            diag_note("%s", command);
            free(command);
        }
        diag_info("Linking %s", output_rel);
    }

    if (!run_command_capture(project->root_dir, &args, true, &exit_code, &output, diag)) {
        string_list_free(&args);
        string_free(&output);
        return false;
    }
    if (exit_code != 0) {
        if (request->target_type == MAZEN_TARGET_STATIC_LIBRARY) {
            diag_set(diag, "archive tool failed while producing `%s`", output_rel);
        } else {
            StringList retry_libs;
            bool retried = false;
            string_list_init(&retry_libs);
            build_request_copy_list(&retry_libs, libs, true);

            if (autolib_infer_from_linker_output(output.data == NULL ? "" : output.data, &retry_libs) &&
                retry_libs.len > libs->len) {
                char *detected = format_item_list(&retry_libs);
                retried = true;
                diag_note("Retrying link with inferred libraries: %s", detected);
                free(detected);
                string_list_free(&args);
                string_free(&output);
                string_list_init(&args);
                string_init(&output);
                add_link_args(&args, request, units, output_rel, &retry_libs);
                if (request->verbose) {
                    char *command = format_command(&args);
                    diag_note("%s", command);
                    free(command);
                }
                diag_info("Linking %s", output_rel);
                if (!run_command_capture(project->root_dir, &args, true, &exit_code, &output, diag)) {
                    string_list_free(&retry_libs);
                    string_list_free(&args);
                    string_free(&output);
                    return false;
                }
                if (exit_code == 0) {
                    string_list_free(&retry_libs);
                    string_list_free(&args);
                    string_free(&output);
                    return true;
                }
            }

            diag_set(diag, "linker failed while producing `%s`", output_rel);
            {
                const char *hint = linker_hint(output.data == NULL ? "" : output.data);
                if (hint != NULL) {
                    diag_set_hint(diag, "%s", hint);
                } else if (!retried) {
                    diag_set_hint(diag, "Add a library with `mazen --lib NAME` or `libs = [\"NAME\"]`");
                }
            }
            string_list_free(&retry_libs);
        }
        string_list_free(&args);
        string_free(&output);
        return false;
    }

    string_list_free(&args);
    string_free(&output);
    return true;
}

static bool write_compile_database(const char *path, const CompDbEntryList *compdb, Diagnostic *diag) {
    bool wrote = false;

    if (path == NULL) {
        return true;
    }
    if (!compdb_write_if_changed(path, compdb, &wrote, diag)) {
        return false;
    }
    if (wrote) {
        diag_info("Wrote %s", path);
    }
    return true;
}

bool build_write_compile_database(const char *root_dir, const char *path, const CompDbEntryList *compdb,
                                  Diagnostic *diag) {
    char *absolute;
    bool ok;

    if (path == NULL) {
        return true;
    }
    if (!ensure_parent_directory(root_dir, path, diag)) {
        return false;
    }

    absolute = resolve_path(root_dir, path);
    ok = write_compile_database(absolute, compdb, diag);
    free(absolute);
    return ok;
}

static bool run_built_program(const ProjectInfo *project, const BuildRequest *request, const char *output_rel,
                              Diagnostic *diag) {
    StringList args;
    char *library_path;

    if (request->target_type != MAZEN_TARGET_EXECUTABLE) {
        diag_set(diag, "target `%s` is a %s and cannot be run",
                 request->target_name != NULL ? request->target_name : "(unnamed)",
                 mazen_target_type_name(request->target_type));
        return false;
    }

    string_list_init(&args);
    string_list_push(&args, output_rel);
    build_request_copy_list(&args, &request->run_args, false);
    library_path = library_path_value(&request->runtime_library_dirs);
    diag_info("Running %s", output_rel);
    fflush(stdout);
    fflush(stderr);
    if (!((library_path == NULL && run_command_passthrough(project->root_dir, &args, diag)) ||
          (library_path != NULL && run_command_passthrough_env(project->root_dir, &args, library_path, diag)))) {
        free(library_path);
        string_list_free(&args);
        return false;
    }
    free(library_path);
    string_list_free(&args);
    return true;
}

static bool wait_for_pid_status(pid_t pid, int *exit_code, Diagnostic *diag) {
    int status;

    while (waitpid(pid, &status, 0) < 0) {
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

typedef struct {
    pid_t pid;
    const ResolvedTarget *target;
} RunningTest;

static ssize_t find_running_test_slot(const RunningTest *running, size_t count, pid_t pid) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (running[i].pid == pid) {
            return (ssize_t) i;
        }
    }
    return -1;
}

static ssize_t find_free_test_slot(const RunningTest *running, size_t count) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (running[i].pid == 0) {
            return (ssize_t) i;
        }
    }
    return -1;
}

static bool launch_test_process(const ProjectInfo *project, const ResolvedTarget *target, pid_t *pid_out, Diagnostic *diag) {
    StringList args;
    char *library_path;
    bool ok;

    string_list_init(&args);
    string_list_push(&args, target->output_path);
    library_path = library_path_value(&target->runtime_library_dirs);
    diag_info("Running test %s", target->name);
    ok = spawn_command_env(project->root_dir, &args, library_path, pid_out, diag);
    free(library_path);
    string_list_free(&args);
    return ok;
}

bool build_run_tests(const ProjectInfo *project, const ResolvedTargetList *targets, int jobs, bool parallel,
                     const char *filter, Diagnostic *diag) {
    size_t i;
    size_t test_count = 0;
    size_t failures = 0;
    StringList failed;

    (void) filter;
    string_list_init(&failed);

    for (i = 0; i < targets->len; ++i) {
        if (targets->items[i].is_test) {
            ++test_count;
        }
    }

    if (test_count == 0) {
        string_list_free(&failed);
        diag_set(diag, "no test targets were selected");
        return false;
    }

    if (!parallel) {
        for (i = 0; i < targets->len; ++i) {
            const ResolvedTarget *target = &targets->items[i];
            pid_t pid;
            int exit_code = 0;

            if (!target->is_test) {
                continue;
            }
            if (target->type != MAZEN_TARGET_EXECUTABLE) {
                string_list_free(&failed);
                diag_set(diag, "test target `%s` must be an executable", target->name);
                return false;
            }
            if (!launch_test_process(project, target, &pid, diag)) {
                string_list_free(&failed);
                return false;
            }
            if (!wait_for_pid_status(pid, &exit_code, diag)) {
                string_list_free(&failed);
                return false;
            }
            if (exit_code != 0) {
                ++failures;
                string_list_push(&failed, target->name);
                diag_warn("Test failed: %s (status %d)", target->name, exit_code);
            }
        }
    } else {
        long detected_jobs = sysconf(_SC_NPROCESSORS_ONLN);
        size_t concurrency = (size_t) (jobs > 0 ? jobs : (detected_jobs > 0 ? detected_jobs : 1));
        RunningTest *running;
        size_t active = 0;
        size_t next = 0;

        if (concurrency < 1) {
            concurrency = 1;
        }

        running = mazen_xcalloc(concurrency, sizeof(*running));
        while (next < targets->len || active > 0) {
            while (next < targets->len && active < concurrency) {
                const ResolvedTarget *target = &targets->items[next++];
                ssize_t slot;

                if (!target->is_test) {
                    continue;
                }
                if (target->type != MAZEN_TARGET_EXECUTABLE) {
                    free(running);
                    string_list_free(&failed);
                    diag_set(diag, "test target `%s` must be an executable", target->name);
                    return false;
                }

                slot = find_free_test_slot(running, concurrency);
                if (slot < 0) {
                    break;
                }
                if (!launch_test_process(project, target, &running[slot].pid, diag)) {
                    free(running);
                    string_list_free(&failed);
                    return false;
                }
                running[slot].target = target;
                ++active;
            }

            if (active > 0) {
                int status;
                pid_t pid;
                ssize_t slot;
                int exit_code;

                while ((pid = waitpid(-1, &status, 0)) < 0) {
                    if (errno != EINTR) {
                        free(running);
                        string_list_free(&failed);
                        diag_set(diag, "failed waiting for test process: %s", strerror(errno));
                        return false;
                    }
                }

                slot = find_running_test_slot(running, concurrency, pid);
                if (slot < 0) {
                    free(running);
                    string_list_free(&failed);
                    diag_set(diag, "internal error: unknown test process finished");
                    return false;
                }

                if (WIFEXITED(status)) {
                    exit_code = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    exit_code = 128 + WTERMSIG(status);
                } else {
                    exit_code = 1;
                }

                if (exit_code != 0) {
                    ++failures;
                    string_list_push(&failed, running[slot].target->name);
                    diag_warn("Test failed: %s (status %d)", running[slot].target->name, exit_code);
                }

                running[slot].pid = 0;
                running[slot].target = NULL;
                --active;
            }
        }

        free(running);
    }

    if (failures > 0) {
        char *failed_names = format_item_list(&failed);
        diag_set(diag, "%zu of %zu tests failed", failures, test_count);
        if (failed_names != NULL && *failed_names != '\0') {
            diag_set_hint(diag, "Failed tests: %s", failed_names);
        }
        free(failed_names);
        string_list_free(&failed);
        return false;
    }

    diag_info("All %zu test(s) passed", test_count);
    string_list_free(&failed);
    return true;
}

static char *selected_install_prefix(const ProjectInfo *project, const MazenConfig *config, const char *prefix_override) {
    const char *prefix = prefix_override != NULL ? prefix_override
                                                 : (config != NULL && config->install_prefix != NULL
                                                        ? config->install_prefix
                                                        : "/usr/local");
    return resolve_path(project->root_dir, prefix);
}

static char *target_install_path(const ProjectInfo *project, const ResolvedTarget *target, const char *prefix_abs) {
    const char *dir_name;
    char *dir_path;
    char *dst;

    dir_name = target->install_dir != NULL ? target->install_dir
                                           : (target->type == MAZEN_TARGET_EXECUTABLE ? "bin" : "lib");
    if (path_is_absolute(dir_name)) {
        dir_path = mazen_xstrdup(dir_name);
    } else {
        dir_path = path_join(prefix_abs, dir_name);
    }
    dst = path_join(dir_path, path_basename(target->output_path));
    free(dir_path);
    (void) project;
    return dst;
}

static void collect_install_roots(const MazenConfig *config, const ResolvedTargetList *targets, StringList *roots) {
    size_t i;

    if (config != NULL) {
        for (i = 0; i < config->include_dirs.len; ++i) {
            if (strcmp(config->include_dirs.items[i], ".") != 0) {
                string_list_push_unique(roots, config->include_dirs.items[i]);
            }
        }
    }

    for (i = 0; i < targets->len; ++i) {
        size_t j;
        for (j = 0; j < targets->items[i].include_dirs.len; ++j) {
            if (strcmp(targets->items[i].include_dirs.items[j], ".") != 0) {
                string_list_push_unique(roots, targets->items[i].include_dirs.items[j]);
            }
        }
    }
}

static char *header_install_relative(const StringList *roots, const char *header_path) {
    size_t i;

    for (i = 0; i < roots->len; ++i) {
        const char *relative = path_after_prefix_dir(header_path, roots->items[i]);
        if (relative != NULL && *relative != '\0') {
            return mazen_xstrdup(relative);
        }
    }

    {
        const char *relative = path_after_prefix_dir(header_path, "include");
        if (relative != NULL && *relative != '\0') {
            return mazen_xstrdup(relative);
        }
    }

    return mazen_xstrdup(path_basename(header_path));
}

static void collect_install_headers(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                                    StringList *headers) {
    size_t i;

    if (config != NULL && config->install_headers.len > 0) {
        for (i = 0; i < config->install_headers.len; ++i) {
            string_list_push_unique(headers, config->install_headers.items[i]);
        }
        return;
    }

    {
        StringList roots;

        string_list_init(&roots);
        collect_install_roots(config, targets, &roots);
        for (i = 0; i < project->header_files.len; ++i) {
            const char *header = project->header_files.items[i];
            size_t j;
            bool matched = false;

            if (path_after_prefix_dir(header, "include") != NULL) {
                string_list_push_unique(headers, header);
                continue;
            }
            for (j = 0; j < roots.len; ++j) {
                const char *relative = path_after_prefix_dir(header, roots.items[j]);
                if (relative != NULL && *relative != '\0') {
                    string_list_push_unique(headers, header);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
        }
        string_list_free(&roots);
    }
}

static bool install_headers(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                            const char *prefix_abs, Diagnostic *diag) {
    StringList headers;
    StringList roots;
    size_t i;

    string_list_init(&headers);
    string_list_init(&roots);
    collect_install_headers(project, config, targets, &headers);
    collect_install_roots(config, targets, &roots);

    for (i = 0; i < headers.len; ++i) {
        char *src = resolve_path(project->root_dir, headers.items[i]);
        char *relative = header_install_relative(&roots, headers.items[i]);
        char *dst_dir = path_join(prefix_abs, "include");
        char *dst = path_join(dst_dir, relative);

        free(dst_dir);
        free(relative);

        if (!file_exists(src)) {
            string_list_free(&roots);
            string_list_free(&headers);
            diag_set(diag, "install header `%s` does not exist", headers.items[i]);
            free(src);
            free(dst);
            return false;
        }
        if (!ensure_parent_directory(project->root_dir, dst, diag)) {
            string_list_free(&roots);
            string_list_free(&headers);
            free(src);
            free(dst);
            return false;
        }
        if (!copy_file_binary(src, dst)) {
            string_list_free(&roots);
            string_list_free(&headers);
            diag_set(diag, "failed to install header `%s` to `%s`: %s", headers.items[i], dst, strerror(errno));
            free(src);
            free(dst);
            return false;
        }
        diag_info("Installed %s", dst);
        free(src);
        free(dst);
    }

    string_list_free(&roots);
    string_list_free(&headers);
    return true;
}

bool build_install_targets(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                           const char *prefix, Diagnostic *diag) {
    char *prefix_abs = selected_install_prefix(project, config, prefix);
    bool installed_any = false;
    size_t i;

    for (i = 0; i < targets->len; ++i) {
        const ResolvedTarget *target = &targets->items[i];
        char *src;
        char *dst;

        if (!target->install_enabled) {
            continue;
        }

        src = resolve_path(project->root_dir, target->output_path);
        dst = target_install_path(project, target, prefix_abs);
        if (!file_exists(src)) {
            free(prefix_abs);
            diag_set(diag, "target artifact `%s` was not built", target->output_path);
            diag_set_hint(diag, "Build the target before installing");
            free(src);
            free(dst);
            return false;
        }
        if (!ensure_parent_directory(project->root_dir, dst, diag)) {
            free(prefix_abs);
            free(src);
            free(dst);
            return false;
        }
        if (!copy_file_binary(src, dst)) {
            free(prefix_abs);
            diag_set(diag, "failed to install `%s` to `%s`: %s", src, dst, strerror(errno));
            free(src);
            free(dst);
            return false;
        }
        diag_info("Installed %s", dst);
        installed_any = true;
        free(src);
        free(dst);
    }

    if (!install_headers(project, config, targets, prefix_abs, diag)) {
        free(prefix_abs);
        return false;
    }

    if (!installed_any) {
        diag_note("No installable target artifacts were selected");
    }

    free(prefix_abs);
    return true;
}

bool build_uninstall_targets(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                             const char *prefix, Diagnostic *diag) {
    char *prefix_abs = selected_install_prefix(project, config, prefix);
    StringList headers;
    StringList roots;
    bool removed_any = false;
    size_t i;

    string_list_init(&headers);
    string_list_init(&roots);
    collect_install_headers(project, config, targets, &headers);
    collect_install_roots(config, targets, &roots);

    for (i = 0; i < targets->len; ++i) {
        const ResolvedTarget *target = &targets->items[i];
        char *dst;

        if (!target->install_enabled) {
            continue;
        }

        dst = target_install_path(project, target, prefix_abs);
        if (file_exists(dst) || dir_exists(dst)) {
            if (remove_tree(dst) != 0) {
                string_list_free(&roots);
                string_list_free(&headers);
                free(prefix_abs);
                diag_set(diag, "failed to remove `%s`: %s", dst, strerror(errno));
                free(dst);
                return false;
            }
            diag_info("Removed %s", dst);
            removed_any = true;
        }
        free(dst);
    }

    for (i = 0; i < headers.len; ++i) {
        char *relative = header_install_relative(&roots, headers.items[i]);
        char *dst_dir = path_join(prefix_abs, "include");
        char *dst = path_join(dst_dir, relative);

        free(dst_dir);
        free(relative);
        if (file_exists(dst) || dir_exists(dst)) {
            if (remove_tree(dst) != 0) {
                string_list_free(&roots);
                string_list_free(&headers);
                free(prefix_abs);
                diag_set(diag, "failed to remove `%s`: %s", dst, strerror(errno));
                free(dst);
                return false;
            }
            diag_info("Removed %s", dst);
            removed_any = true;
        }
        free(dst);
    }

    if (!removed_any) {
        diag_note("No installed artifacts were removed");
    }

    string_list_free(&roots);
    string_list_free(&headers);
    free(prefix_abs);
    return true;
}

bool build_project(const ProjectInfo *project, const MazenConfig *config, const BuildRequest *request,
                   CompDbEntryList *compdb_out, Diagnostic *diag) {
    CacheState state;
    BuildUnitList units;
    CompDbEntryList compdb;
    StringList auto_libs;
    StringList effective_libs;
    char *output_rel = NULL;
    char *output_abs = NULL;
    char *cache_rel = NULL;
    char *cache_abs = NULL;
    char *compdb_rel = NULL;
    char *compdb_abs = NULL;
    char *compile_sig = NULL;
    char *link_sig = NULL;
    bool compile_flags_changed;
    bool link_flags_changed;
    bool need_compdb_entries;
    bool any_compiled = false;
    bool did_link = false;
    (void) config;

    if (project->sources.len == 0) {
        diag_set(diag, "no C source files were found in `%s`", project->root_dir);
        return false;
    }
    if (request->source_paths.len == 0) {
        diag_set(diag, "target `%s` resolved to no source files",
                 request->target_name != NULL ? request->target_name : "(unnamed)");
        return false;
    }
    if (request->target_type == MAZEN_TARGET_EXECUTABLE && request->entry_path == NULL) {
        diag_set(diag, "no entrypoint containing `main()` was detected for target `%s`",
                 request->target_name != NULL ? request->target_name : "(unnamed)");
        diag_set_hint(diag, "Run `mazen doctor` to inspect discovery results");
        return false;
    }
    if (request->run_after_build && request->target_type != MAZEN_TARGET_EXECUTABLE) {
        diag_set(diag, "target `%s` is a %s and cannot be run",
                 request->target_name != NULL ? request->target_name : "(unnamed)",
                 mazen_target_type_name(request->target_type));
        return false;
    }

    diag_info("Scanning project...");
    diag_info("Found %zu source files", project->sources.len);
    diag_info("Resolved target: %s (%s)", request->target_name != NULL ? request->target_name : project->project_name,
              mazen_target_type_name(request->target_type));
    if (request->entry_path != NULL) {
        diag_info("Detected entrypoint: %s", request->entry_path);
    }
    print_include_dirs(request);
    diag_info("Building %s target", request->mode == MAZEN_BUILD_RELEASE ? "release" : "debug");
    if (effective_job_count(request) > 1) {
        diag_note("Using %d parallel jobs", effective_job_count(request));
    }

    build_unit_list_init(&units);
    cache_state_init(&state);
    compdb_entry_list_init(&compdb);
    string_list_init(&auto_libs);
    string_list_init(&effective_libs);

    if (!create_build_units(project, request, &units, diag)) {
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }
    if (!prepare_paths(project, request, &units, &output_rel, &output_abs, &cache_rel, &cache_abs, &compdb_rel,
                       &compdb_abs, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    build_request_copy_list(&effective_libs, &request->libs, true);

    if (!cache_load(cache_abs, &state, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    if (request->target_type != MAZEN_TARGET_STATIC_LIBRARY) {
        if (auto_lib_cache_is_fresh(project, request, &state)) {
            build_request_copy_list(&auto_libs, &state.auto_libs, false);
        } else {
            autolib_infer(project->root_dir, &request->source_paths, &auto_libs);
            string_list_clear(&state.auto_libs);
            build_request_copy_list(&state.auto_libs, &auto_libs, false);
            state.auto_libs_valid = true;
        }
        build_request_copy_list(&effective_libs, &auto_libs, true);
        if (auto_libs.len > 0) {
            char *detected = format_item_list(&auto_libs);
            diag_note("Auto-detected libraries: %s", detected);
            free(detected);
        }
    }

    make_signatures(request, &effective_libs, &compile_sig, &link_sig);
    compile_flags_changed = state.compile_signature == NULL || strcmp(state.compile_signature, compile_sig) != 0;
    link_flags_changed = state.link_signature == NULL || strcmp(state.link_signature, link_sig) != 0;
    free(state.compile_signature);
    free(state.link_signature);
    state.compile_signature = mazen_xstrdup(compile_sig);
    state.link_signature = mazen_xstrdup(link_sig);

    if (compile_flags_changed) {
        diag_note("Compiler flags changed, invalidating object cache");
    }

    need_compdb_entries = compdb_out != NULL;
    if (!need_compdb_entries && request->write_compile_commands) {
        need_compdb_entries = compdb_abs == NULL || !file_exists(compdb_abs) || compile_flags_changed;
    }

    if (!compile_units(project, request, &units, &state, compile_flags_changed,
                       need_compdb_entries ? &compdb : NULL, &any_compiled, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        free(compile_sig);
        free(link_sig);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    if (output_is_stale(&units, request, output_abs, link_flags_changed || any_compiled)) {
        if (!link_target(project, request, &units, output_rel, &effective_libs, diag)) {
            free(output_rel);
            free(output_abs);
            free(cache_rel);
            free(cache_abs);
            free(compdb_rel);
            free(compdb_abs);
            free(compile_sig);
            free(link_sig);
            string_list_free(&auto_libs);
            string_list_free(&effective_libs);
            build_unit_list_free(&units);
            cache_state_free(&state);
            compdb_entry_list_free(&compdb);
            return false;
        }
        did_link = true;
    } else {
        diag_note("Up to date: %s", output_rel);
    }

    if (!cache_save(cache_abs, &state, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        free(compile_sig);
        free(link_sig);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    if (compdb_out != NULL) {
        compdb_entry_list_append(compdb_out, &compdb);
    }

    if (request->write_compile_commands && need_compdb_entries && !write_compile_database(compdb_abs, &compdb, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        free(compile_sig);
        free(link_sig);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    if (!did_link && !any_compiled) {
        diag_info("Target is up to date");
    }

    if (request->run_after_build && !run_built_program(project, request, output_rel, diag)) {
        free(output_rel);
        free(output_abs);
        free(cache_rel);
        free(cache_abs);
        free(compdb_rel);
        free(compdb_abs);
        free(compile_sig);
        free(link_sig);
        string_list_free(&auto_libs);
        string_list_free(&effective_libs);
        build_unit_list_free(&units);
        cache_state_free(&state);
        compdb_entry_list_free(&compdb);
        return false;
    }

    free(output_rel);
    free(output_abs);
    free(cache_rel);
    free(cache_abs);
    free(compdb_rel);
    free(compdb_abs);
    free(compile_sig);
    free(link_sig);
    string_list_free(&auto_libs);
    string_list_free(&effective_libs);
    build_unit_list_free(&units);
    cache_state_free(&state);
    compdb_entry_list_free(&compdb);
    return true;
}
