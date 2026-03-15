#include "build.h"
#include "classifier.h"
#include "cli.h"
#include "config.h"
#include "diag.h"
#include "doctor.h"
#include "scanner.h"
#include "standard.h"

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
                                   BuildRequest *request) {
    size_t i;
    const char *name = config->name != NULL ? config->name : project->project_name;

    request->compiler = cli->compiler != NULL ? cli->compiler : "clang";
    request->standard = mazen_standard_select(cli->c_standard, config->c_standard);
    request->mode = cli->command == MAZEN_CMD_RELEASE ? MAZEN_BUILD_RELEASE : MAZEN_BUILD_DEBUG;
    request->run_after_build = cli->command == MAZEN_CMD_RUN;
    request->verbose = cli->verbose;
    free(request->binary_name);
    request->binary_name = mazen_xstrdup(name);

    for (i = 0; i < config->libs.len; ++i) {
        string_list_push_unique(&request->libs, config->libs.items[i]);
    }
    for (i = 0; i < config->cflags.len; ++i) {
        string_list_push(&request->cflags, config->cflags.items[i]);
    }
    for (i = 0; i < config->ldflags.len; ++i) {
        string_list_push(&request->ldflags, config->ldflags.items[i]);
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
    Diagnostic diag;
    char *root_dir;
    int exit_code = EXIT_SUCCESS;

    cli_options_init(&cli);
    config_init(&config);
    scan_result_init(&scan);
    project_info_init(&project);
    build_request_init(&request);
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
        if (!build_clean(root_dir, &diag)) {
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
        populate_build_request(&cli, &config, &project, &request);
        doctor_print_project(&project, &config, request.compiler, request.standard);
        break;
    case MAZEN_CMD_LIST:
        doctor_print_targets(&project);
        break;
    case MAZEN_CMD_BUILD:
    case MAZEN_CMD_RUN:
    case MAZEN_CMD_RELEASE:
        populate_build_request(&cli, &config, &project, &request);
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
    project_info_free(&project);
    scan_result_free(&scan);
    config_free(&config);
    cli_options_free(&cli);
    return exit_code;
}
