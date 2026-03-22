#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapter.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *KERNEL_COMPILE_COMMANDS_PATH = "compile_commands.json";
static const char *KERNEL_COMPILE_COMMANDS_HELPER = "scripts/clang-tools/gen_compile_commands.py";
static const size_t DELEGATED_OUTPUT_TAIL_LIMIT = 65536;

static bool run_kernel_goal(const char *root_dir, const CliOptions *cli, const char *goal, bool parallelize,
                            Diagnostic *diag);

static bool looks_like_linux_kernel_tree(const char *root_dir) {
    char *makefile = path_join(root_dir, "Makefile");
    char *kconfig = path_join(root_dir, "Kconfig");
    char *arch = path_join(root_dir, "arch");
    char *scripts = path_join(root_dir, "scripts");
    char *kernel = path_join(root_dir, "kernel");
    char *init = path_join(root_dir, "init");
    bool detected = file_exists(makefile) && file_exists(kconfig) && dir_exists(arch) && dir_exists(scripts) &&
                    (dir_exists(kernel) || dir_exists(init));

    free(makefile);
    free(kconfig);
    free(arch);
    free(scripts);
    free(kernel);
    free(init);
    return detected;
}

static const char *adapter_name(const MazenProjectAdapter *adapter) {
    switch (adapter->kind) {
    case MAZEN_PROJECT_ADAPTER_LINUX_KERNEL:
        return "linux-kernel";
    case MAZEN_PROJECT_ADAPTER_NONE:
    default:
        return "none";
    }
}

static const char *adapter_build_system(const MazenProjectAdapter *adapter) {
    switch (adapter->kind) {
    case MAZEN_PROJECT_ADAPTER_LINUX_KERNEL:
        return "make (Kbuild)";
    case MAZEN_PROJECT_ADAPTER_NONE:
    default:
        return "(none)";
    }
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

static void string_keep_tail(String *buffer, size_t max_len) {
    size_t trim;

    if (buffer->len <= max_len) {
        return;
    }

    trim = buffer->len - max_len;
    memmove(buffer->data, buffer->data + trim, buffer->len - trim + 1);
    buffer->len -= trim;
}

static void append_output_tail(String *buffer, const char *chunk, size_t len, size_t max_len) {
    if (max_len == 0 || len == 0) {
        return;
    }
    if (len >= max_len) {
        string_clear(buffer);
        string_append_n(buffer, chunk + len - max_len, max_len);
        return;
    }

    string_append_n(buffer, chunk, len);
    string_keep_tail(buffer, max_len);
}

static int effective_job_count(const CliOptions *cli) {
    long detected;

    if (cli->jobs_set && cli->jobs > 0) {
        return cli->jobs;
    }

    detected = sysconf(_SC_NPROCESSORS_ONLN);
    if (detected < 1) {
        return 1;
    }
    return (int) detected;
}

static bool run_command_passthrough(const char *cwd, const StringList *args, Diagnostic *diag, String *captured_tail) {
    pid_t child;
    int status;
    char **argv;
    int pipe_fds[2];
    bool capture_output = captured_tail != NULL;

    argv = argv_from_list(args);
    if (capture_output) {
        string_init(captured_tail);
        if (pipe(pipe_fds) != 0) {
            free(argv);
            diag_set(diag, "failed to create delegated build output pipe: %s", strerror(errno));
            return false;
        }
    }

    child = fork();
    if (child < 0) {
        if (capture_output) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
        free(argv);
        diag_set(diag, "failed to fork delegated build process: %s", strerror(errno));
        return false;
    }

    if (child == 0) {
        if (cwd != NULL && chdir(cwd) != 0) {
            fprintf(stderr, "failed to enter %s: %s\n", cwd, strerror(errno));
            _exit(127);
        }
        if (capture_output) {
            close(pipe_fds[0]);
            if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 || dup2(pipe_fds[1], STDERR_FILENO) < 0) {
                fprintf(stderr, "failed to connect delegated build output: %s\n", strerror(errno));
                _exit(127);
            }
            close(pipe_fds[1]);
        }
        execvp(argv[0], argv);
        fprintf(stderr, "failed to execute %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    if (capture_output) {
        char chunk[4096];
        ssize_t count;

        close(pipe_fds[1]);
        while ((count = read(pipe_fds[0], chunk, sizeof(chunk))) > 0) {
            ssize_t written = 0;

            append_output_tail(captured_tail, chunk, (size_t) count, DELEGATED_OUTPUT_TAIL_LIMIT);
            while (written < count) {
                ssize_t rc = write(STDOUT_FILENO, chunk + written, (size_t) (count - written));
                if (rc < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    break;
                }
                written += rc;
            }
        }
        close(pipe_fds[0]);
    }

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            free(argv);
            diag_set(diag, "failed waiting for delegated build process: %s", strerror(errno));
            return false;
        }
    }
    free(argv);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    if (WIFEXITED(status)) {
        diag_set(diag, "delegated build system exited with status %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        diag_set(diag, "delegated build system terminated by signal %d", WTERMSIG(status));
    } else {
        diag_set(diag, "delegated build system failed");
    }
    return false;
}

static bool extract_missing_host_tool(const char *text, char **tool_out) {
    static const char *MARKERS[] = {": command not found", ": not found"};
    size_t i;

    if (text == NULL || *text == '\0' || tool_out == NULL) {
        return false;
    }

    for (i = 0; i < MAZEN_ARRAY_LEN(MARKERS); ++i) {
        const char *marker = MARKERS[i];
        const char *cursor = text;

        while (cursor != NULL && *cursor != '\0') {
            const char *hit = strstr(cursor, marker);
            const char *line_start;
            const char *token_start;
            const char *token_end;

            if (hit == NULL) {
                break;
            }

            line_start = hit;
            while (line_start > text && line_start[-1] != '\n') {
                --line_start;
            }

            token_start = hit;
            while (token_start > line_start && token_start[-1] != ':') {
                --token_start;
            }
            token_end = hit;

            while (token_start < token_end && (*token_start == ' ' || *token_start == '\t' || *token_start == '\'')) {
                ++token_start;
            }
            while (token_end > token_start &&
                   (token_end[-1] == ' ' || token_end[-1] == '\t' || token_end[-1] == '\'')) {
                --token_end;
            }

            if (token_end > token_start) {
                *tool_out = mazen_format("%.*s", (int) (token_end - token_start), token_start);
                return true;
            }

            cursor = hit + strlen(marker);
        }
    }

    {
        const char *cursor = text;

        while (cursor != NULL && *cursor != '\0') {
            static const char *MARKER = ": No such file or directory";
            const char *hit = strstr(cursor, MARKER);
            const char *line_start;
            const char *tool_start;
            const char *tool_end;

            if (hit == NULL) {
                break;
            }

            line_start = hit;
            while (line_start > text && line_start[-1] != '\n') {
                --line_start;
            }

            if (!string_starts_with(line_start, "make:")) {
                cursor = hit + strlen(MARKER);
                continue;
            }

            tool_start = line_start + strlen("make:");
            while (tool_start < hit && (*tool_start == ' ' || *tool_start == '\t')) {
                ++tool_start;
            }

            tool_end = hit;
            while (tool_end > tool_start && (tool_end[-1] == ' ' || tool_end[-1] == '\t')) {
                --tool_end;
            }

            if (tool_end > tool_start && memchr(tool_start, '/', (size_t) (tool_end - tool_start)) == NULL) {
                *tool_out = mazen_format("%.*s", (int) (tool_end - tool_start), tool_start);
                return true;
            }

            cursor = hit + strlen(MARKER);
        }
    }

    return false;
}

static bool extract_missing_shared_library(const char *text, char **tool_out, char **library_out) {
    static const char *PREFIX = ": error while loading shared libraries: ";
    static const char *SUFFIX = ": cannot open shared object file: No such file or directory";
    const char *cursor = text;

    if (text == NULL || *text == '\0' || tool_out == NULL || library_out == NULL) {
        return false;
    }

    while (cursor != NULL && *cursor != '\0') {
        const char *hit = strstr(cursor, PREFIX);
        const char *line_start;
        const char *tool_start;
        const char *tool_end;
        const char *lib_start;
        const char *lib_end;

        if (hit == NULL) {
            break;
        }

        line_start = hit;
        while (line_start > text && line_start[-1] != '\n') {
            --line_start;
        }

        tool_end = hit;
        tool_start = tool_end;
        while (tool_start > line_start && tool_start[-1] != ' ' && tool_start[-1] != '\t' &&
               tool_start[-1] != '\'' && tool_start[-1] != '"') {
            --tool_start;
        }
        while (tool_start < tool_end &&
               (*tool_start == ' ' || *tool_start == '\t' || *tool_start == '\'' || *tool_start == '"')) {
            ++tool_start;
        }
        while (tool_end > tool_start &&
               (tool_end[-1] == ' ' || tool_end[-1] == '\t' || tool_end[-1] == '\'' || tool_end[-1] == '"')) {
            --tool_end;
        }

        lib_start = hit + strlen(PREFIX);
        lib_end = strstr(lib_start, SUFFIX);
        if (tool_end > tool_start && lib_end != NULL && lib_end > lib_start) {
            *tool_out = mazen_format("%.*s", (int) (tool_end - tool_start), tool_start);
            *library_out = mazen_format("%.*s", (int) (lib_end - lib_start), lib_start);
            return true;
        }

        cursor = hit + strlen(PREFIX);
    }

    return false;
}

static void refine_kernel_failure(const String *captured_tail, const CliOptions *cli, Diagnostic *diag) {
    char *tool = NULL;
    char *library = NULL;

    if (captured_tail == NULL || captured_tail->len == 0) {
        return;
    }

    if (extract_missing_shared_library(captured_tail->data, &tool, &library)) {
        diag_set(diag, "linux-kernel build failed: `%s` is missing shared library `%s`", tool, library);
        diag_set_hint(diag, "Install the matching runtime for `%s`, or export LD_LIBRARY_PATH so `%s` can find `%s`",
                      tool, tool, library);
        free(tool);
        free(library);
        return;
    }

    if (extract_missing_host_tool(captured_tail->data, &tool)) {
        diag_set(diag, "linux-kernel build failed: missing host tool `%s`", tool);
        diag_set_hint(diag, "Install `%s` and rerun `mazen%s%s`", tool,
                      cli->target_name != NULL ? " --target " : "",
                      cli->target_name != NULL ? cli->target_name : "");
        free(tool);
    }
}

static bool kernel_adapter_validate(const CliOptions *cli, Diagnostic *diag) {
    if (cli->all_targets) {
        diag_set(diag, "`--all-targets` is not supported for linux-kernel projects");
        diag_set_hint(diag, "Use `--target MAKE_GOAL` to select a Kbuild goal");
        return false;
    }
    if (cli->profile_name != NULL) {
        diag_set(diag, "`--profile` is not supported for linux-kernel projects");
        return false;
    }
    if (cli->name_override != NULL || cli->c_standard != NULL || cli->include_dirs.len > 0 || cli->libs.len > 0 ||
        cli->src_dirs.len > 0 || cli->excludes.len > 0 || cli->cflags.len > 0 || cli->ldflags.len > 0) {
        diag_set(diag, "source-inference flags are not supported for linux-kernel projects");
        diag_set_hint(diag, "MAZEN delegates linux-kernel trees to Kbuild; use native Kbuild variables or flags");
        return false;
    }
    if (cli->command == MAZEN_CMD_CLEAN && cli->target_name != NULL) {
        diag_set(diag, "`--target` is not supported with `mazen clean` for linux-kernel projects");
        return false;
    }
    if (cli->command == MAZEN_CMD_CLEAN && cli->clean_mask_set && cli->clean_mask != MAZEN_CLEAN_ALL) {
        diag_set(diag, "partial clean scopes are not supported for linux-kernel projects");
        diag_set_hint(diag, "Use `mazen clean` to delegate to `make clean`");
        return false;
    }

    switch (cli->command) {
    case MAZEN_CMD_BUILD:
    case MAZEN_CMD_RELEASE:
    case MAZEN_CMD_CLEAN:
    case MAZEN_CMD_DOCTOR:
    case MAZEN_CMD_LIST:
        return true;
    case MAZEN_CMD_RUN:
    case MAZEN_CMD_TEST:
    case MAZEN_CMD_INSTALL:
    case MAZEN_CMD_UNINSTALL:
        diag_set(diag, "`mazen %s` is not supported for linux-kernel projects", cli_command_name(cli->command));
        diag_set_hint(diag, "Use `mazen build`, `mazen clean`, `mazen doctor`, `mazen list`, or `--target MAKE_GOAL`");
        return false;
    case MAZEN_CMD_HELP:
    case MAZEN_CMD_STANDARDS:
    case MAZEN_CMD_VERSION:
    default:
        return true;
    }
}

static void add_kernel_make_args(StringList *args, const CliOptions *cli, const char *goal, bool parallelize) {
    string_list_push(args, "make");

    if (parallelize) {
        string_list_push_take(args, mazen_format("-j%d", effective_job_count(cli)));
    }
    if (cli->verbose) {
        string_list_push(args, "V=1");
    }
    if (cli->compiler != NULL) {
        string_list_push_take(args, mazen_format("CC=%s", cli->compiler));
    }
    if (goal != NULL) {
        string_list_push(args, goal);
    }
}

static bool kernel_config_exists(const char *root_dir) {
    char *config_path = path_join(root_dir, ".config");
    bool exists = file_exists(config_path);
    free(config_path);
    return exists;
}

static bool goal_configures_kernel(const char *goal) {
    return goal != NULL && strstr(goal, "config") != NULL;
}

static bool goal_is_kernel_maintenance_only(const char *goal) {
    if (goal == NULL) {
        return false;
    }

    return strcmp(goal, "help") == 0 || strcmp(goal, "kernelversion") == 0 || strcmp(goal, "clean") == 0 ||
           strcmp(goal, "mrproper") == 0 || strcmp(goal, "distclean") == 0 || strcmp(goal, "tags") == 0 ||
           strcmp(goal, "TAGS") == 0;
}

static bool goal_may_refresh_kernel_compile_commands(const char *goal) {
    return !goal_configures_kernel(goal) && !goal_is_kernel_maintenance_only(goal);
}

static bool kernel_goal_needs_config_sync(const char *goal) {
    return !goal_configures_kernel(goal) && !goal_is_kernel_maintenance_only(goal);
}

static bool command_exists(const char *name) {
    char *path_env;
    char *copy;
    char *cursor;

    if (name == NULL || *name == '\0') {
        return false;
    }
    if (strchr(name, '/') != NULL) {
        return access(name, X_OK) == 0;
    }

    path_env = getenv("PATH");
    if (path_env == NULL || *path_env == '\0') {
        return false;
    }

    copy = mazen_xstrdup(path_env);
    cursor = copy;
    while (cursor != NULL) {
        char *next = strchr(cursor, ':');
        char *dir;
        char *candidate;
        bool found;

        if (next != NULL) {
            *next = '\0';
        }

        dir = *cursor == '\0' ? mazen_xstrdup(".") : mazen_xstrdup(cursor);
        candidate = path_join(dir, name);
        found = access(candidate, X_OK) == 0;
        free(candidate);
        free(dir);
        if (found) {
            free(copy);
            return true;
        }

        cursor = next != NULL ? next + 1 : NULL;
    }

    free(copy);
    return false;
}

static const char *kernel_python_interpreter(void) {
    if (command_exists("python3")) {
        return "python3";
    }
    if (command_exists("python")) {
        return "python";
    }
    return NULL;
}

static bool kernel_compile_commands_helper_exists(const char *root_dir) {
    char *helper = path_join(root_dir, KERNEL_COMPILE_COMMANDS_HELPER);
    bool exists = file_exists(helper);

    free(helper);
    return exists;
}

static bool kernel_compile_commands_is_stale(const char *root_dir, const CliOptions *cli) {
    char *compdb = path_join(root_dir, KERNEL_COMPILE_COMMANDS_PATH);
    char *config = path_join(root_dir, ".config");
    long long compdb_time;
    long long config_time;
    bool stale;

    if (!file_exists(compdb)) {
        free(compdb);
        free(config);
        return true;
    }

    compdb_time = file_mtime_ns(compdb);
    config_time = file_mtime_ns(config);
    stale = cli->compiler != NULL || compdb_time <= 0 || (config_time > 0 && config_time > compdb_time);

    free(compdb);
    free(config);
    return stale;
}

static bool kernel_refresh_compile_commands(const char *root_dir, const CliOptions *cli, Diagnostic *diag) {
    StringList args;
    const char *python = kernel_python_interpreter();
    char *compdb = NULL;
    bool ok;

    if (!goal_may_refresh_kernel_compile_commands(cli->target_name)) {
        return true;
    }
    if (!kernel_compile_commands_helper_exists(root_dir)) {
        return true;
    }
    if (!kernel_compile_commands_is_stale(root_dir, cli)) {
        return true;
    }
    if (python == NULL) {
        diag_warn("Skipping kernel compile database generation: python3/python not found");
        return true;
    }

    string_list_init(&args);
    string_list_push(&args, python);
    string_list_push(&args, KERNEL_COMPILE_COMMANDS_HELPER);

    diag_info("Refreshing %s via linux-kernel helper", KERNEL_COMPILE_COMMANDS_PATH);
    ok = run_command_passthrough(root_dir, &args, diag, NULL);
    string_list_free(&args);
    if (!ok) {
        return false;
    }

    compdb = path_join(root_dir, KERNEL_COMPILE_COMMANDS_PATH);
    if (file_exists(compdb)) {
        diag_info("Wrote %s", compdb);
    } else {
        diag_warn("Kernel compile database helper completed without writing %s", KERNEL_COMPILE_COMMANDS_PATH);
    }
    free(compdb);
    return true;
}

static bool kernel_sync_existing_config(const char *root_dir, const CliOptions *cli, Diagnostic *diag) {
    if (!kernel_config_exists(root_dir) || !kernel_goal_needs_config_sync(cli->target_name)) {
        return true;
    }

    diag_note("Refreshing existing kernel config via olddefconfig");
    return run_kernel_goal(root_dir, cli, "olddefconfig", false, diag);
}

static bool kernel_remove_compile_commands(const char *root_dir, Diagnostic *diag) {
    char *compdb = path_join(root_dir, KERNEL_COMPILE_COMMANDS_PATH);

    if (!file_exists(compdb)) {
        free(compdb);
        return true;
    }
    if (remove_tree(compdb) != 0) {
        diag_set(diag, "failed to remove `%s`: %s", compdb, strerror(errno));
        free(compdb);
        return false;
    }

    diag_info("Removed %s", compdb);
    free(compdb);
    return true;
}

static void kernel_adapter_doctor(const char *root_dir, const CliOptions *cli) {
    printf("Project root: %s\n", root_dir);
    printf("Project adapter: %s\n", adapter_name(&(MazenProjectAdapter){.kind = MAZEN_PROJECT_ADAPTER_LINUX_KERNEL}));
    printf("Delegated build system: %s\n",
           adapter_build_system(&(MazenProjectAdapter){.kind = MAZEN_PROJECT_ADAPTER_LINUX_KERNEL}));
    printf("Requested make goal: %s\n", cli->target_name != NULL ? cli->target_name : "(default)");
    if (cli->jobs_set) {
        printf("Parallel jobs: %d\n", cli->jobs);
    } else {
        printf("Parallel jobs: auto (%d)\n", effective_job_count(cli));
    }
    printf("Compiler override: %s\n", cli->compiler != NULL ? cli->compiler : "(make default)");
    printf("Verbose build: %s\n", cli->verbose ? "enabled" : "disabled");
    printf("Kernel config: %s\n", kernel_config_exists(root_dir) ? ".config present" : "missing (.config not found)");
    if (kernel_compile_commands_helper_exists(root_dir)) {
        if (kernel_python_interpreter() != NULL) {
            printf("Compile database: %s via native helper\n", KERNEL_COMPILE_COMMANDS_PATH);
        } else {
            printf("Compile database: %s helper found, but python3/python is unavailable\n",
                   KERNEL_COMPILE_COMMANDS_PATH);
        }
    } else {
        puts("Compile database: native helper not found");
    }
    puts("Supported commands: build, release, clean, doctor, list");
}

static bool run_kernel_goal(const char *root_dir, const CliOptions *cli, const char *goal, bool parallelize,
                            Diagnostic *diag) {
    StringList args;
    String captured_tail;
    bool ok;

    string_list_init(&args);
    add_kernel_make_args(&args, cli, goal, parallelize);
    diag_info("Delegating to linux-kernel build system");
    if (cli->verbose) {
        char *command = format_command(&args);
        diag_note("%s", command);
        free(command);
    }
    ok = run_command_passthrough(root_dir, &args, diag, &captured_tail);
    if (!ok) {
        refine_kernel_failure(&captured_tail, cli, diag);
    }
    string_free(&captured_tail);
    string_list_free(&args);
    return ok;
}

void mazen_project_adapter_init(MazenProjectAdapter *adapter) {
    adapter->kind = MAZEN_PROJECT_ADAPTER_NONE;
}

bool mazen_project_adapter_detect(const char *root_dir, const MazenConfig *config, MazenProjectAdapter *adapter) {
    mazen_project_adapter_init(adapter);

    if (config != NULL && config->present) {
        return true;
    }
    if (looks_like_linux_kernel_tree(root_dir)) {
        adapter->kind = MAZEN_PROJECT_ADAPTER_LINUX_KERNEL;
    }
    return true;
}

bool mazen_project_adapter_is_active(const MazenProjectAdapter *adapter) {
    return adapter != NULL && adapter->kind != MAZEN_PROJECT_ADAPTER_NONE;
}

bool mazen_project_adapter_handle(const MazenProjectAdapter *adapter, const char *root_dir, const CliOptions *cli,
                                  Diagnostic *diag) {
    if (adapter == NULL || adapter->kind == MAZEN_PROJECT_ADAPTER_NONE) {
        diag_set(diag, "internal error: inactive project adapter was invoked");
        return false;
    }

    switch (adapter->kind) {
    case MAZEN_PROJECT_ADAPTER_LINUX_KERNEL:
        if (!kernel_adapter_validate(cli, diag)) {
            return false;
        }
        switch (cli->command) {
        case MAZEN_CMD_DOCTOR:
            kernel_adapter_doctor(root_dir, cli);
            return true;
        case MAZEN_CMD_LIST:
            return run_kernel_goal(root_dir, cli, "help", false, diag);
        case MAZEN_CMD_CLEAN:
            if (!run_kernel_goal(root_dir, cli, "clean", false, diag)) {
                return false;
            }
            return kernel_remove_compile_commands(root_dir, diag);
        case MAZEN_CMD_BUILD:
        case MAZEN_CMD_RELEASE:
            if (!kernel_config_exists(root_dir) && !goal_configures_kernel(cli->target_name)) {
                diag_note("No .config detected; run `mazen --target defconfig build` or configure Kbuild first if needed");
            }
            if (!kernel_sync_existing_config(root_dir, cli, diag)) {
                return false;
            }
            if (!run_kernel_goal(root_dir, cli, cli->target_name, true, diag)) {
                return false;
            }
            return kernel_refresh_compile_commands(root_dir, cli, diag);
        default:
            diag_set(diag, "`mazen %s` is not supported for linux-kernel projects", cli_command_name(cli->command));
            return false;
        }
    case MAZEN_PROJECT_ADAPTER_NONE:
    default:
        diag_set(diag, "internal error: unknown project adapter");
        return false;
    }
}
