#ifndef MAZEN_BUILD_H
#define MAZEN_BUILD_H

#include "cache.h"
#include "cli.h"
#include "compdb.h"
#include "common.h"
#include "config.h"
#include "diag.h"
#include "standard.h"
#include "target.h"

typedef struct {
    const char *compiler;
    const MazenCStandard *standard;
    MazenBuildMode mode;
    const char *profile_name;
    bool run_after_build;
    bool verbose;
    int jobs;
    char *target_name;
    MazenTargetType target_type;
    char *output_path;
    char *entry_path;
    char *compile_commands_path;
    bool write_compile_commands;
    StringList source_paths;
    StringList include_dirs;
    StringList libs;
    StringList cflags;
    StringList ldflags;
    StringList dependency_outputs;
    StringList runtime_library_dirs;
    StringList run_args;
} BuildRequest;

void build_request_init(BuildRequest *request);
void build_request_free(BuildRequest *request);
bool build_clean(const char *root_dir, const MazenConfig *config, unsigned int clean_mask, Diagnostic *diag);
bool build_project(const ProjectInfo *project, const MazenConfig *config, const BuildRequest *request,
                   CompDbEntryList *compdb_out, Diagnostic *diag);
bool build_write_compile_database(const char *root_dir, const char *path, const CompDbEntryList *compdb,
                                  Diagnostic *diag);
bool build_run_tests(const ProjectInfo *project, const ResolvedTargetList *targets, int jobs, bool parallel,
                     const char *filter, Diagnostic *diag);
bool build_install_targets(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                           const char *prefix, Diagnostic *diag);
bool build_uninstall_targets(const ProjectInfo *project, const MazenConfig *config, const ResolvedTargetList *targets,
                             const char *prefix, Diagnostic *diag);

#endif
