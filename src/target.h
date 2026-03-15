#ifndef MAZEN_TARGET_H
#define MAZEN_TARGET_H

#include "common.h"
#include "diag.h"

struct MazenConfig;

typedef enum {
    MAZEN_TARGET_EXECUTABLE = 0,
    MAZEN_TARGET_STATIC_LIBRARY,
    MAZEN_TARGET_SHARED_LIBRARY
} MazenTargetType;

typedef struct {
    char *name;
    MazenTargetType type;
    bool type_set;
    bool default_target;
    bool test_target;
    bool test_set;
    bool install_target;
    bool install_set;
    char *entry;
    char *output;
    char *install_dir;
    StringList src_dirs;
    StringList include_dirs;
    StringList libs;
    StringList cflags;
    StringList ldflags;
    StringList sources;
    StringList exclude;
    StringList deps;
} MazenTargetConfig;

typedef struct {
    MazenTargetConfig *items;
    size_t len;
    size_t cap;
} MazenTargetConfigList;

typedef struct {
    char *name;
    MazenTargetType type;
    char *output_path;
    char *entry_path;
    char *install_dir;
    bool from_config;
    bool is_default;
    bool is_test;
    bool install_enabled;
    StringList source_paths;
    StringList include_dirs;
    StringList libs;
    StringList cflags;
    StringList ldflags;
    StringList dep_names;
    StringList dependency_outputs;
    StringList runtime_library_dirs;
} ResolvedTarget;

typedef struct {
    ResolvedTarget *items;
    size_t len;
    size_t cap;
} ResolvedTargetList;

void mazen_target_config_init(MazenTargetConfig *target);
void mazen_target_config_free(MazenTargetConfig *target);
void mazen_target_config_list_init(MazenTargetConfigList *list);
void mazen_target_config_list_free(MazenTargetConfigList *list);
MazenTargetConfig *mazen_target_config_list_find(MazenTargetConfigList *list, const char *name);
const MazenTargetConfig *mazen_target_config_list_find_const(const MazenTargetConfigList *list, const char *name);
MazenTargetConfig *mazen_target_config_list_push(MazenTargetConfigList *list, const char *name);

const char *mazen_target_type_name(MazenTargetType type);
bool mazen_target_type_from_text(const char *text, MazenTargetType *type);

void resolved_target_init(ResolvedTarget *target);
void resolved_target_free(ResolvedTarget *target);
void resolved_target_list_init(ResolvedTargetList *list);
void resolved_target_list_free(ResolvedTargetList *list);

bool mazen_target_resolve(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                          ResolvedTarget *resolved, Diagnostic *diag);
bool mazen_target_resolve_all(const ProjectInfo *project, const struct MazenConfig *config, ResolvedTargetList *list,
                              Diagnostic *diag);
bool mazen_target_plan_build(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                             bool all_targets, ResolvedTargetList *list, Diagnostic *diag);
bool mazen_target_plan_tests(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                             const char *filter, ResolvedTargetList *list, Diagnostic *diag);
void mazen_target_print_list(const ProjectInfo *project, const struct MazenConfig *config);

#endif
