#ifndef MAZEN_CLI_H
#define MAZEN_CLI_H

#include "common.h"
#include "diag.h"

typedef struct {
    MazenCommand command;
    bool verbose;
    char *compiler;
    char *c_standard;
    char *name_override;
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
