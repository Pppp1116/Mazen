#include "adapter.h"
#include "build.h"
#include "classifier.h"
#include "cli.h"
#include "config.h"
#include "diag.h"
#include "doctor.h"
#include "scanner.h"
#include "standard.h"
#include "target.h"

#include "mazen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    BUILD_PATH_GENERAL = 0,
    BUILD_PATH_TOOL,
    BUILD_PATH_EXAMPLE,
    BUILD_PATH_TEST,
    BUILD_PATH_EXPERIMENTAL
} BuildPathContext;

static const char *BUILD_CONTEXT_TOOL_NAMES[] = {"tool", "tools", "script", "scripts"};
static const char *BUILD_CONTEXT_EXAMPLE_NAMES[] = {"example", "examples", "demo", "demos", "sample", "samples"};
static const char *BUILD_CONTEXT_TEST_NAMES[] = {"test", "tests", "spec", "specs"};
static const char *BUILD_CONTEXT_EXPERIMENT_NAMES[] = {
    "experiment", "experiments", "old", "backup", "scratch", "playground"
};

static char *default_project_name(const char *root_dir) {
    const char *base = path_basename(root_dir);
    if (base == NULL || *base == '\0') {
        return mazen_xstrdup("app");
    }
    return mazen_xstrdup(base);
}

static MazenBuildMode command_build_mode(MazenCommand command) {
    switch (command) {
    case MAZEN_CMD_RELEASE:
    case MAZEN_CMD_INSTALL:
        return MAZEN_BUILD_RELEASE;
    case MAZEN_CMD_BUILD:
    case MAZEN_CMD_RUN:
    case MAZEN_CMD_TEST:
    case MAZEN_CMD_DOCTOR:
    default:
        return MAZEN_BUILD_DEBUG;
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

static bool path_has_prefix_dir(const char *path, const char *prefix) {
    size_t len = strlen(prefix);

    if (strcmp(path, prefix) == 0) {
        return true;
    }
    return string_starts_with(path, prefix) && path[len] == '/';
}

static const SourceFile *project_find_source_by_path(const ProjectInfo *project, const char *path) {
    size_t i;

    for (i = 0; i < project->sources.len; ++i) {
        if (strcmp(project->sources.items[i].path, path) == 0) {
            return &project->sources.items[i];
        }
    }

    return NULL;
}

static BuildPathContext build_path_context(const char *path) {
    if (path_matches_any_component(path, BUILD_CONTEXT_TEST_NAMES, MAZEN_ARRAY_LEN(BUILD_CONTEXT_TEST_NAMES))) {
        return BUILD_PATH_TEST;
    }
    if (path_matches_any_component(path, BUILD_CONTEXT_EXAMPLE_NAMES,
                                   MAZEN_ARRAY_LEN(BUILD_CONTEXT_EXAMPLE_NAMES))) {
        return BUILD_PATH_EXAMPLE;
    }
    if (path_matches_any_component(path, BUILD_CONTEXT_EXPERIMENT_NAMES,
                                   MAZEN_ARRAY_LEN(BUILD_CONTEXT_EXPERIMENT_NAMES))) {
        return BUILD_PATH_EXPERIMENTAL;
    }
    if (path_matches_any_component(path, BUILD_CONTEXT_TOOL_NAMES, MAZEN_ARRAY_LEN(BUILD_CONTEXT_TOOL_NAMES))) {
        return BUILD_PATH_TOOL;
    }
    return BUILD_PATH_GENERAL;
}

static bool context_allows_path(BuildPathContext target_context, const char *path) {
    BuildPathContext path_context = build_path_context(path);

    switch (target_context) {
    case BUILD_PATH_TOOL:
        return path_context == BUILD_PATH_GENERAL || path_context == BUILD_PATH_TOOL;
    case BUILD_PATH_EXAMPLE:
        return path_context == BUILD_PATH_GENERAL || path_context == BUILD_PATH_EXAMPLE;
    case BUILD_PATH_TEST:
        return path_context == BUILD_PATH_GENERAL || path_context == BUILD_PATH_TEST;
    case BUILD_PATH_EXPERIMENTAL:
        return path_context == BUILD_PATH_GENERAL || path_context == BUILD_PATH_EXPERIMENTAL;
    case BUILD_PATH_GENERAL:
    default:
        return path_context == BUILD_PATH_GENERAL;
    }
}

static BuildPathContext target_build_context(const ResolvedTarget *target) {
    size_t i;

    if (target == NULL) {
        return BUILD_PATH_GENERAL;
    }
    if (target->is_test) {
        return BUILD_PATH_TEST;
    }
    if (target->entry_path != NULL) {
        return build_path_context(target->entry_path);
    }
    for (i = 0; i < target->source_paths.len; ++i) {
        BuildPathContext source_context = build_path_context(target->source_paths.items[i]);
        if (source_context != BUILD_PATH_GENERAL) {
            return source_context;
        }
    }
    return BUILD_PATH_GENERAL;
}

static void add_target_inferred_include_dirs(const ProjectInfo *project, const ResolvedTarget *target,
                                             StringList *dirs) {
    StringList flat_includes;
    StringList path_includes;
    BuildPathContext target_context;
    size_t i;

    if (target == NULL) {
        for (i = 0; i < project->include_dirs.len; ++i) {
            string_list_push_unique(dirs, project->include_dirs.items[i]);
        }
        return;
    }

    target_context = target_build_context(target);
    string_list_init(&flat_includes);
    string_list_init(&path_includes);

    string_list_push_unique(dirs, ".");

    for (i = 0; i < project->source_roots.len; ++i) {
        size_t j;

        if (!context_allows_path(target_context, project->source_roots.items[i])) {
            continue;
        }
        for (j = 0; j < target->source_paths.len; ++j) {
            if (path_has_prefix_dir(target->source_paths.items[j], project->source_roots.items[i])) {
                string_list_push_unique(dirs, project->source_roots.items[i]);
                break;
            }
        }
    }

    for (i = 0; i < target->source_paths.len; ++i) {
        const SourceFile *source = project_find_source_by_path(project, target->source_paths.items[i]);
        size_t j;

        if (source == NULL) {
            continue;
        }
        if (source->directory != NULL && context_allows_path(target_context, source->path)) {
            string_list_push_unique(dirs, source->directory);
        }
        for (j = 0; j < source->includes.len; ++j) {
            if (strchr(source->includes.items[j], '/') != NULL) {
                string_list_push_unique(&path_includes, source->includes.items[j]);
            } else {
                string_list_push_unique(&flat_includes, source->includes.items[j]);
            }
        }
    }

    for (i = 0; i < path_includes.len; ++i) {
        const char *include = path_includes.items[i];
        size_t include_len = strlen(include);
        size_t j;

        for (j = 0; j < project->header_files.len; ++j) {
            const char *header = project->header_files.items[j];
            size_t header_len = strlen(header);

            if (!context_allows_path(target_context, header)) {
                continue;
            }
            if (strcmp(header, include) == 0) {
                string_list_push_unique(dirs, ".");
            } else if (header_len > include_len && strcmp(header + header_len - include_len, include) == 0 &&
                       header[header_len - include_len - 1] == '/') {
                size_t root_len = header_len - include_len - 1;
                char *root =
                    root_len == 0 ? mazen_xstrdup(".") : mazen_format("%.*s", (int) root_len, header);
                string_list_push_unique_take(dirs, root);
            }
        }
    }

    for (i = 0; i < project->header_files.len; ++i) {
        const char *header = project->header_files.items[i];

        if (!context_allows_path(target_context, header)) {
            continue;
        }
        if (string_list_contains(&flat_includes, path_basename(header))) {
            char *dir = path_dirname(header);
            string_list_push_unique_take(dirs, dir);
        }
    }

    string_list_free(&path_includes);
    string_list_free(&flat_includes);
}

static void populate_build_request(const CliOptions *cli, const MazenConfig *config, const ProjectInfo *project,
                                   const MazenProfile *profile, const ResolvedTarget *target, bool run_after_build,
                                   BuildRequest *request) {
    size_t i;

    request->compiler = cli->compiler != NULL ? cli->compiler : "clang";
    request->standard = mazen_standard_select(cli->c_standard, config->c_standard);
    request->mode = command_build_mode(cli->command);
    if (profile != NULL && profile->mode_set && cli->command != MAZEN_CMD_RELEASE) {
        request->mode = profile->mode;
    }
    request->profile_name = profile != NULL ? profile->name : NULL;
    request->run_after_build = run_after_build;
    request->verbose = cli->verbose;
    request->jobs = cli->jobs_set ? cli->jobs : (config->jobs_set ? config->jobs : 0);
    request->target_type = target != NULL ? target->type : MAZEN_TARGET_EXECUTABLE;

    free(request->target_name);
    free(request->output_path);
    free(request->entry_path);
    free(request->compile_commands_path);
    request->target_name = mazen_xstrdup(target != NULL ? target->name : project->project_name);
    request->output_path = mazen_xstrdup(target != NULL ? target->output_path : "build/app");
    request->entry_path = target != NULL && target->entry_path != NULL ? mazen_xstrdup(target->entry_path) : NULL;
    request->compile_commands_path =
        mazen_xstrdup(config->compile_commands_path != NULL ? config->compile_commands_path : "compile_commands.json");

    string_list_clear(&request->source_paths);
    string_list_clear(&request->include_dirs);
    string_list_clear(&request->libs);
    string_list_clear(&request->cflags);
    string_list_clear(&request->ldflags);
    string_list_clear(&request->dependency_outputs);
    string_list_clear(&request->runtime_library_dirs);
    string_list_clear(&request->run_args);

    /* Keep auto-discovered include paths scoped to the active target. */
    add_target_inferred_include_dirs(project, target, &request->include_dirs);
    for (i = 0; i < config->include_dirs.len; ++i) {
        string_list_push_unique(&request->include_dirs, config->include_dirs.items[i]);
    }
    if (profile != NULL) {
        for (i = 0; i < profile->include_dirs.len; ++i) {
            string_list_push_unique(&request->include_dirs, profile->include_dirs.items[i]);
        }
    }
    if (target != NULL) {
        for (i = 0; i < target->source_paths.len; ++i) {
            string_list_push_unique(&request->source_paths, target->source_paths.items[i]);
        }
        for (i = 0; i < target->include_dirs.len; ++i) {
            string_list_push_unique(&request->include_dirs, target->include_dirs.items[i]);
        }
    }

    for (i = 0; i < config->libs.len; ++i) {
        string_list_push_unique(&request->libs, config->libs.items[i]);
    }
    if (profile != NULL) {
        for (i = 0; i < profile->libs.len; ++i) {
            string_list_push_unique(&request->libs, profile->libs.items[i]);
        }
    }
    if (target != NULL) {
        for (i = 0; i < target->libs.len; ++i) {
            string_list_push_unique(&request->libs, target->libs.items[i]);
        }
    }

    for (i = 0; i < config->cflags.len; ++i) {
        string_list_push(&request->cflags, config->cflags.items[i]);
    }
    if (profile != NULL) {
        for (i = 0; i < profile->cflags.len; ++i) {
            string_list_push(&request->cflags, profile->cflags.items[i]);
        }
    }
    if (target != NULL) {
        for (i = 0; i < target->cflags.len; ++i) {
            string_list_push(&request->cflags, target->cflags.items[i]);
        }
    }

    for (i = 0; i < config->ldflags.len; ++i) {
        string_list_push(&request->ldflags, config->ldflags.items[i]);
    }
    if (profile != NULL) {
        for (i = 0; i < profile->ldflags.len; ++i) {
            string_list_push(&request->ldflags, profile->ldflags.items[i]);
        }
    }
    if (target != NULL) {
        for (i = 0; i < target->ldflags.len; ++i) {
            string_list_push(&request->ldflags, target->ldflags.items[i]);
        }
        for (i = 0; i < target->dependency_outputs.len; ++i) {
            string_list_push_unique(&request->dependency_outputs, target->dependency_outputs.items[i]);
        }
        for (i = 0; i < target->runtime_library_dirs.len; ++i) {
            string_list_push_unique(&request->runtime_library_dirs, target->runtime_library_dirs.items[i]);
        }
    }

    for (i = 0; i < cli->run_args.len; ++i) {
        string_list_push(&request->run_args, cli->run_args.items[i]);
    }
}

static bool validate_cli_request(const CliOptions *cli, Diagnostic *diag) {
    if (cli->all_targets && cli->target_name != NULL) {
        diag_set(diag, "`--all-targets` cannot be combined with `--target NAME`");
        return false;
    }
    if (cli->all_targets && cli->command == MAZEN_CMD_RUN) {
        diag_set(diag, "`mazen run` only supports a single executable target");
        diag_set_hint(diag, "Use `mazen --target NAME run` or build everything with `mazen --all-targets`");
        return false;
    }
    if (cli->test_parallel && cli->command != MAZEN_CMD_TEST) {
        diag_set(diag, "`--parallel` is only valid with `mazen test`");
        return false;
    }
    if (cli->test_filter != NULL && cli->command != MAZEN_CMD_TEST) {
        diag_set(diag, "`--filter` is only valid with `mazen test`");
        return false;
    }
    if (cli->install_prefix != NULL && cli->command != MAZEN_CMD_INSTALL && cli->command != MAZEN_CMD_UNINSTALL) {
        diag_set(diag, "`--prefix` is only valid with `mazen install` or `mazen uninstall`");
        return false;
    }
    if (cli->clean_mask_set && cli->command != MAZEN_CMD_CLEAN) {
        diag_set(diag, "clean scopes are only valid with `mazen clean`");
        return false;
    }
    if (cli->profile_name != NULL && cli->command != MAZEN_CMD_BUILD && cli->command != MAZEN_CMD_RUN &&
        cli->command != MAZEN_CMD_TEST && cli->command != MAZEN_CMD_RELEASE && cli->command != MAZEN_CMD_INSTALL &&
        cli->command != MAZEN_CMD_UNINSTALL && cli->command != MAZEN_CMD_DOCTOR) {
        diag_set(diag, "`--profile` is not valid with `%s`", cli_command_name(cli->command));
        return false;
    }
    if (cli->target_name != NULL && cli->test_filter != NULL) {
        diag_set(diag, "use either `--target NAME` or `--filter TEXT` for `mazen test`, not both");
        return false;
    }
    return true;
}

static bool build_targets(const CliOptions *cli, const MazenConfig *config, const ProjectInfo *project,
                          const MazenProfile *profile, const ResolvedTargetList *targets, bool run_last_target,
                          Diagnostic *diag) {
    BuildRequest request;
    CompDbEntryList compdb;
    bool batch_compile_database = cli->all_targets || targets->len > 1;
    const char *compile_commands_path =
        config->compile_commands_path != NULL ? config->compile_commands_path : "compile_commands.json";
    size_t i;
    bool ok = false;

    if (targets->len == 0) {
        diag_set(diag, "no buildable targets were resolved");
        return false;
    }

    build_request_init(&request);
    compdb_entry_list_init(&compdb);

    if (targets->len > 1) {
        diag_info("Building %zu targets", targets->len);
    }

    for (i = 0; i < targets->len; ++i) {
        populate_build_request(cli, config, project, profile, &targets->items[i], run_last_target && i + 1 == targets->len,
                               &request);
        request.write_compile_commands = !batch_compile_database;
        if (!build_project(project, config, &request, batch_compile_database ? &compdb : NULL, diag)) {
            goto out;
        }
    }

    if (batch_compile_database &&
        !build_write_compile_database(project->root_dir, compile_commands_path, &compdb, diag)) {
        goto out;
    }

    ok = true;

out:
    compdb_entry_list_free(&compdb);
    build_request_free(&request);
    return ok;
}

static const MazenProfile *select_active_profile(const CliOptions *cli, const MazenConfig *config, Diagnostic *diag) {
    const MazenProfile *profile = mazen_profile_select(&config->profiles, cli->profile_name, config->default_profile,
                                                       diag);

    if ((cli->profile_name != NULL || config->default_profile != NULL) && profile == NULL) {
        return NULL;
    }
    return profile;
}

int main(int argc, char **argv) {
    CliOptions cli;
    MazenConfig config;
    ScanResult scan;
    ProjectInfo project;
    ResolvedTargetList targets;
    MazenProjectAdapter adapter;
    const MazenProfile *profile = NULL;
    Diagnostic diag;
    char *root_dir = NULL;
    int exit_code = EXIT_SUCCESS;

    cli_options_init(&cli);
    config_init(&config);
    scan_result_init(&scan);
    project_info_init(&project);
    resolved_target_list_init(&targets);
    mazen_project_adapter_init(&adapter);
    diag_init(&diag);

    if (!cli_parse(argc, argv, &cli, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    if (cli.command == MAZEN_CMD_HELP) {
        cli_print_help();
        goto out;
    }
    if (cli.command == MAZEN_CMD_VERSION) {
        puts(MAZEN_VERSION_STRING);
        goto out;
    }
    if (cli.command == MAZEN_CMD_STANDARDS) {
        mazen_standard_print_supported(stdout);
        goto out;
    }
    if (!validate_cli_request(&cli, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    root_dir = mazen_getcwd();
    if (root_dir == NULL) {
        diag_set(&diag, "failed to determine current working directory");
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    if (!config_load(root_dir, &config, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    if (!mazen_project_adapter_detect(root_dir, &config, &adapter)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }
    if (mazen_project_adapter_is_active(&adapter)) {
        if (!mazen_project_adapter_handle(&adapter, root_dir, &cli, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        goto out;
    }

    config_merge_cli(&config, &cli);

    if (cli.command == MAZEN_CMD_CLEAN) {
        if (!build_clean(root_dir, &config, cli.clean_mask_set ? cli.clean_mask : MAZEN_CLEAN_ALL, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        goto out;
    }

    if (!scanner_scan(root_dir, &config, &scan, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }
    if (!classifier_analyze(root_dir, &scan, &config, &project, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    project.project_name = config.name != NULL ? mazen_xstrdup(config.name) : default_project_name(root_dir);

    profile = select_active_profile(&cli, &config, &diag);
    if ((cli.profile_name != NULL || config.default_profile != NULL) && profile == NULL) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    switch (cli.command) {
    case MAZEN_CMD_DOCTOR: {
        BuildRequest request;

        if (!mazen_target_plan_build(&project, &config, cli.target_name, cli.all_targets, &targets, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }

        build_request_init(&request);
        populate_build_request(&cli, &config, &project, profile,
                               targets.len > 0 ? &targets.items[targets.len - 1] : NULL, false, &request);
        doctor_print_project(&project, &config, request.compiler, request.standard, request.mode,
                             profile != NULL ? profile->name : NULL,
                             targets.len > 0 ? &targets.items[targets.len - 1] : NULL, request.jobs,
                             request.compile_commands_path, &request.include_dirs);
        doctor_print_resolved_targets(&targets);
        build_request_free(&request);
        break;
    }
    case MAZEN_CMD_LIST:
        if (config.targets.len > 0) {
            mazen_target_print_list(&project, &config);
        } else {
            doctor_print_targets(&project);
        }
        break;
    case MAZEN_CMD_BUILD:
    case MAZEN_CMD_RUN:
    case MAZEN_CMD_RELEASE:
        if (!mazen_target_plan_build(&project, &config, cli.target_name, cli.all_targets, &targets, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_targets(&cli, &config, &project, profile, &targets, cli.command == MAZEN_CMD_RUN, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        break;
    case MAZEN_CMD_TEST:
        if (!mazen_target_plan_tests(&project, &config, cli.target_name, cli.test_filter, &targets, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_targets(&cli, &config, &project, profile, &targets, false, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_run_tests(&project, &targets, cli.jobs_set ? cli.jobs : (config.jobs_set ? config.jobs : 0),
                             cli.test_parallel, cli.test_filter, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        break;
    case MAZEN_CMD_INSTALL:
        if (!mazen_target_plan_build(&project, &config, cli.target_name, cli.all_targets, &targets, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_targets(&cli, &config, &project, profile, &targets, false, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_install_targets(&project, &config, &targets, cli.install_prefix, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        break;
    case MAZEN_CMD_UNINSTALL:
        if (!mazen_target_plan_build(&project, &config, cli.target_name, cli.all_targets, &targets, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        if (!build_uninstall_targets(&project, &config, &targets, cli.install_prefix, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        break;
    default:
        diag_set(&diag, "unsupported command `%s`", cli_command_name(cli.command));
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        break;
    }

out:
    free(root_dir);
    diag_free(&diag);
    resolved_target_list_free(&targets);
    project_info_free(&project);
    scan_result_free(&scan);
    config_free(&config);
    cli_options_free(&cli);
    return exit_code;
}
