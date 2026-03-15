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

    for (i = 0; i < project->include_dirs.len; ++i) {
        string_list_push_unique(&request->include_dirs, project->include_dirs.items[i]);
    }
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
    const MazenProfile *profile = NULL;
    Diagnostic diag;
    char *root_dir = NULL;
    int exit_code = EXIT_SUCCESS;

    cli_options_init(&cli);
    config_init(&config);
    scan_result_init(&scan);
    project_info_init(&project);
    resolved_target_list_init(&targets);
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

    if (cli.command == MAZEN_CMD_CLEAN) {
        if (!config_load(root_dir, &config, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            goto out;
        }
        config_merge_cli(&config, &cli);
        if (!build_clean(root_dir, &config, cli.clean_mask_set ? cli.clean_mask : MAZEN_CLEAN_ALL, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        goto out;
    }

    if (!config_load(root_dir, &config, &diag)) {
        diag_print_error(&diag);
        exit_code = EXIT_FAILURE;
        goto out;
    }
    config_merge_cli(&config, &cli);

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
                             request.compile_commands_path);
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
