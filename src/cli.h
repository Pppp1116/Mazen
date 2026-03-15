#ifndef MAZEN_CLI_H
#define MAZEN_CLI_H

#include "common.h"
#include "diag.h"

typedef struct {
    MazenCommand command;
    bool verbose;
    bool all_targets;
    bool test_parallel;
    int jobs;
    bool jobs_set;
    unsigned int clean_mask;
    bool clean_mask_set;
    char *compiler;
    char *c_standard;
    char *profile_name;
    char *test_filter;
    char *install_prefix;
    char *name_override;
    char *target_name;
    StringList include_dirs;
    StringList libs;
    StringList src_dirs;
    StringList excludes;
    StringList cflags;
    StringList ldflags;
    StringList run_args;
} CliOptions;

void cli_options_init(CliOptions *options);
void cli_options_free(CliOptions *options);
bool cli_parse(int argc, char **argv, CliOptions *options, Diagnostic *diag);
void cli_print_help(void);
const char *cli_command_name(MazenCommand command);

#endif
