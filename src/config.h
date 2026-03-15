#ifndef MAZEN_CONFIG_H
#define MAZEN_CONFIG_H

#include "cli.h"
#include "common.h"
#include "diag.h"

typedef struct {
    bool present;
    char *path;
    char *name;
    char *c_standard;
    StringList include_dirs;
    StringList libs;
    StringList exclude;
    StringList src_dirs;
    StringList cflags;
    StringList ldflags;
} MazenConfig;

void config_init(MazenConfig *config);
void config_free(MazenConfig *config);
bool config_load(const char *root_dir, MazenConfig *config, Diagnostic *diag);
void config_merge_cli(MazenConfig *config, const CliOptions *options);

#endif
