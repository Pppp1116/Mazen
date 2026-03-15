#ifndef MAZEN_BUILD_H
#define MAZEN_BUILD_H

#include "cache.h"
#include "cli.h"
#include "common.h"
#include "config.h"
#include "diag.h"
#include "standard.h"

typedef struct {
    const char *compiler;
    const MazenCStandard *standard;
    MazenBuildMode mode;
    bool run_after_build;
    bool verbose;
    char *binary_name;
    StringList libs;
    StringList cflags;
    StringList ldflags;
    StringList run_args;
} BuildRequest;

void build_request_init(BuildRequest *request);
void build_request_free(BuildRequest *request);
bool build_clean(const char *root_dir, Diagnostic *diag);
bool build_project(const ProjectInfo *project, const MazenConfig *config, const BuildRequest *request,
                   Diagnostic *diag);

#endif
