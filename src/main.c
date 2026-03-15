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

static void populate_build_request(const CliOptions *cli, const MazenConfig *config, const ProjectInfo *project,
                                   const ResolvedTarget *target, BuildRequest *request) {
    size_t i;

    request->compiler = cli->compiler != NULL ? cli->compiler : "clang";
    request->standard = mazen_standard_select(cli->c_standard, config->c_standard);
    request->mode = cli->command == MAZEN_CMD_RELEASE ? MAZEN_BUILD_RELEASE : MAZEN_BUILD_DEBUG;
    request->run_after_build = cli->command == MAZEN_CMD_RUN;
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
    string_list_clear(&request->run_args);

    for (i = 0; i < project->include_dirs.len; ++i) {
        string_list_push_unique(&request->include_dirs, project->include_dirs.items[i]);
    }
    for (i = 0; i < config->include_dirs.len; ++i) {
        string_list_push_unique(&request->include_dirs, config->include_dirs.items[i]);
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
    if (target != NULL) {
        for (i = 0; i < target->libs.len; ++i) {
            string_list_push_unique(&request->libs, target->libs.items[i]);
        }
    }
    for (i = 0; i < config->cflags.len; ++i) {
        string_list_push(&request->cflags, config->cflags.items[i]);
    }
    if (target != NULL) {
        for (i = 0; i < target->cflags.len; ++i) {
            string_list_push(&request->cflags, target->cflags.items[i]);
        }
    }
    for (i = 0; i < config->ldflags.len; ++i) {
        string_list_push(&request->ldflags, config->ldflags.items[i]);
    }
    if (target != NULL) {
        for (i = 0; i < target->ldflags.len; ++i) {
            string_list_push(&request->ldflags, target->ldflags.items[i]);
        }
    }
    for (i = 0; i < cli->run_args.len; ++i) {
        string_list_push(&request->run_args, cli->run_args.items[i]);
    }
}

int main(int argc, char **argv) {
    CliOptions cli;
    MazenConfig config;
    ScanResult scan;
    ProjectInfo project;
    BuildRequest request;
    ResolvedTarget target;
    Diagnostic diag;
    char *root_dir;
    int exit_code = EXIT_SUCCESS;

    cli_options_init(&cli);
    config_init(&config);
    scan_result_init(&scan);
    project_info_init(&project);
    build_request_init(&request);
    resolved_target_init(&target);
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
            free(root_dir);
            exit_code = EXIT_FAILURE;
            goto out;
        }
        config_merge_cli(&config, &cli);
        if (!build_clean(root_dir, &config, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
        }
        free(root_dir);
        goto out;
    }

    if (!config_load(root_dir, &config, &diag)) {
        diag_print_error(&diag);
        free(root_dir);
        exit_code = EXIT_FAILURE;
        goto out;
    }
    config_merge_cli(&config, &cli);

    if (!scanner_scan(root_dir, &config, &scan, &diag)) {
        diag_print_error(&diag);
        free(root_dir);
        exit_code = EXIT_FAILURE;
        goto out;
    }
    if (!classifier_analyze(root_dir, &scan, &config, &project, &diag)) {
        diag_print_error(&diag);
        free(root_dir);
        exit_code = EXIT_FAILURE;
        goto out;
    }

    project.project_name = config.name != NULL ? mazen_xstrdup(config.name) : default_project_name(root_dir);
    free(root_dir);

    switch (cli.command) {
    case MAZEN_CMD_DOCTOR:
        if (!mazen_target_resolve(&project, &config, cli.target_name, &target, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        populate_build_request(&cli, &config, &project, &target, &request);
        doctor_print_project(&project, &config, request.compiler, request.standard, &target, request.jobs,
                             request.compile_commands_path);
        if (config.targets.len > 0) {
            mazen_target_print_list(&project, &config);
        }
        break;
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
        if (!mazen_target_resolve(&project, &config, cli.target_name, &target, &diag)) {
            diag_print_error(&diag);
            exit_code = EXIT_FAILURE;
            break;
        }
        populate_build_request(&cli, &config, &project, &target, &request);
        if (!build_project(&project, &config, &request, &diag)) {
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
    diag_free(&diag);
    build_request_free(&request);
    resolved_target_free(&target);
    project_info_free(&project);
    scan_result_free(&scan);
    config_free(&config);
    cli_options_free(&cli);
    return exit_code;
}
