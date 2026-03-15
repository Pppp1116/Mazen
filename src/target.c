#include "target.h"

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    PATH_CONTEXT_GENERAL = 0,
    PATH_CONTEXT_TOOL,
    PATH_CONTEXT_EXAMPLE,
    PATH_CONTEXT_TEST,
    PATH_CONTEXT_EXPERIMENTAL
} PathContext;

static const char *CONTEXT_TOOL_NAMES[] = {"tool", "tools", "script", "scripts"};
static const char *CONTEXT_EXAMPLE_NAMES[] = {"example", "examples", "demo", "demos", "sample", "samples"};
static const char *CONTEXT_TEST_NAMES[] = {"test", "tests", "spec", "specs"};
static const char *CONTEXT_EXPERIMENT_NAMES[] = {
    "experiment", "experiments", "old", "backup", "scratch", "playground"
};

static void string_list_copy_unique(StringList *dst, const StringList *src) {
    size_t i;
    for (i = 0; i < src->len; ++i) {
        string_list_push_unique(dst, src->items[i]);
    }
}

static bool path_has_prefix_dir(const char *path, const char *prefix) {
    size_t len = strlen(prefix);

    if (strcmp(path, prefix) == 0) {
        return true;
    }
    return string_starts_with(path, prefix) && path[len] == '/';
}

static bool path_is_excluded(const char *path, const StringList *excludes) {
    size_t i;

    for (i = 0; i < excludes->len; ++i) {
        const char *item = excludes->items[i];
        size_t len = strlen(item);
        if (strcmp(path, item) == 0 || strcmp(path_basename(path), item) == 0) {
            return true;
        }
        if (string_starts_with(path, item) && path[len] == '/') {
            return true;
        }
    }

    return false;
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

static bool text_looks_like_test(const char *text) {
    if (text == NULL || *text == '\0') {
        return false;
    }
    if (path_matches_any_component(text, CONTEXT_TEST_NAMES, MAZEN_ARRAY_LEN(CONTEXT_TEST_NAMES))) {
        return true;
    }
    return strstr(text, "_test") != NULL || strstr(text, "test_") != NULL || strstr(text, "spec") != NULL;
}

static PathContext source_path_context(const SourceFile *source) {
    if (path_matches_any_component(source->path, CONTEXT_TEST_NAMES, MAZEN_ARRAY_LEN(CONTEXT_TEST_NAMES))) {
        return PATH_CONTEXT_TEST;
    }
    if (path_matches_any_component(source->path, CONTEXT_EXAMPLE_NAMES, MAZEN_ARRAY_LEN(CONTEXT_EXAMPLE_NAMES))) {
        return PATH_CONTEXT_EXAMPLE;
    }
    if (path_matches_any_component(source->path, CONTEXT_EXPERIMENT_NAMES,
                                   MAZEN_ARRAY_LEN(CONTEXT_EXPERIMENT_NAMES))) {
        return PATH_CONTEXT_EXPERIMENTAL;
    }
    if (path_matches_any_component(source->path, CONTEXT_TOOL_NAMES, MAZEN_ARRAY_LEN(CONTEXT_TOOL_NAMES))) {
        return PATH_CONTEXT_TOOL;
    }
    return PATH_CONTEXT_GENERAL;
}

static const SourceFile *project_find_source(const ProjectInfo *project, const char *path, size_t *index_out) {
    size_t i;

    for (i = 0; i < project->sources.len; ++i) {
        if (strcmp(project->sources.items[i].path, path) == 0) {
            if (index_out != NULL) {
                *index_out = i;
            }
            return &project->sources.items[i];
        }
    }

    return NULL;
}

static void collect_legacy_executable_sources(const ProjectInfo *project, size_t entry_index, StringList *paths) {
    const SourceFile *selected = &project->sources.items[entry_index];
    PathContext selected_context = source_path_context(selected);
    size_t i;

    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        PathContext source_context = source_path_context(source);

        if (i == entry_index) {
            string_list_push_unique(paths, source->path);
            continue;
        }
        if (source->has_main) {
            continue;
        }

        switch (selected_context) {
        case PATH_CONTEXT_TOOL:
            if ((source_context == PATH_CONTEXT_GENERAL || source_context == PATH_CONTEXT_TOOL) &&
                (source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
                 source->role == SOURCE_ROLE_TOOL)) {
                string_list_push_unique(paths, source->path);
            }
            break;
        case PATH_CONTEXT_EXAMPLE:
            if ((source_context == PATH_CONTEXT_GENERAL || source_context == PATH_CONTEXT_EXAMPLE) &&
                (source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
                 source->role == SOURCE_ROLE_EXAMPLE)) {
                string_list_push_unique(paths, source->path);
            }
            break;
        case PATH_CONTEXT_TEST:
            if ((source_context == PATH_CONTEXT_GENERAL || source_context == PATH_CONTEXT_TEST) &&
                (source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
                 source->role == SOURCE_ROLE_TEST)) {
                string_list_push_unique(paths, source->path);
            }
            break;
        case PATH_CONTEXT_EXPERIMENTAL:
            if ((source_context == PATH_CONTEXT_GENERAL || source_context == PATH_CONTEXT_EXPERIMENTAL) &&
                (source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR ||
                 source->role == SOURCE_ROLE_EXPERIMENTAL)) {
                string_list_push_unique(paths, source->path);
            }
            break;
        case PATH_CONTEXT_GENERAL:
        default:
            if (source_context == PATH_CONTEXT_GENERAL &&
                (source->role == SOURCE_ROLE_GENERAL || source->role == SOURCE_ROLE_VENDOR)) {
                string_list_push_unique(paths, source->path);
            }
            break;
        }
    }
}

static bool collect_sources_from_dirs(const ProjectInfo *project, const StringList *dirs, const StringList *excludes,
                                      StringList *paths) {
    size_t i;
    bool found_any = false;

    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        size_t j;

        if (path_is_excluded(source->path, excludes)) {
            continue;
        }
        for (j = 0; j < dirs->len; ++j) {
            if (path_has_prefix_dir(source->path, dirs->items[j])) {
                string_list_push_unique(paths, source->path);
                found_any = true;
                break;
            }
        }
    }

    return found_any;
}

static bool collect_explicit_sources(const ProjectInfo *project, const StringList *sources, const StringList *excludes,
                                     StringList *paths, Diagnostic *diag) {
    size_t i;

    for (i = 0; i < sources->len; ++i) {
        const SourceFile *source = project_find_source(project, sources->items[i], NULL);
        if (source == NULL) {
            diag_set(diag, "target source `%s` was not found", sources->items[i]);
            return false;
        }
        if (path_is_excluded(source->path, excludes)) {
            continue;
        }
        string_list_push_unique(paths, source->path);
    }

    return true;
}

static char *default_output_path(const struct MazenConfig *config, const MazenTargetConfig *target) {
    const char *base_name = target->name;

    if (config->targets.len == 1 && config->name != NULL) {
        base_name = config->name;
    }

    switch (target->type_set ? target->type : MAZEN_TARGET_EXECUTABLE) {
    case MAZEN_TARGET_STATIC_LIBRARY:
        return mazen_format("build/lib%s.a", base_name);
    case MAZEN_TARGET_SHARED_LIBRARY:
        return mazen_format("build/lib%s.so", base_name);
    case MAZEN_TARGET_EXECUTABLE:
    default:
        return mazen_format("build/%s", base_name);
    }
}

void mazen_target_config_init(MazenTargetConfig *target) {
    target->name = NULL;
    target->type = MAZEN_TARGET_EXECUTABLE;
    target->type_set = false;
    target->default_target = false;
    target->test_target = false;
    target->test_set = false;
    target->install_target = false;
    target->install_set = false;
    target->entry = NULL;
    target->output = NULL;
    target->install_dir = NULL;
    string_list_init(&target->src_dirs);
    string_list_init(&target->include_dirs);
    string_list_init(&target->libs);
    string_list_init(&target->cflags);
    string_list_init(&target->ldflags);
    string_list_init(&target->sources);
    string_list_init(&target->exclude);
    string_list_init(&target->deps);
}

void mazen_target_config_free(MazenTargetConfig *target) {
    free(target->name);
    free(target->entry);
    free(target->output);
    free(target->install_dir);
    string_list_free(&target->src_dirs);
    string_list_free(&target->include_dirs);
    string_list_free(&target->libs);
    string_list_free(&target->cflags);
    string_list_free(&target->ldflags);
    string_list_free(&target->sources);
    string_list_free(&target->exclude);
    string_list_free(&target->deps);
    mazen_target_config_init(target);
}

void mazen_target_config_list_init(MazenTargetConfigList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void mazen_target_config_list_free(MazenTargetConfigList *list) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        mazen_target_config_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

MazenTargetConfig *mazen_target_config_list_find(MazenTargetConfigList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

const MazenTargetConfig *mazen_target_config_list_find_const(const MazenTargetConfigList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

MazenTargetConfig *mazen_target_config_list_push(MazenTargetConfigList *list, const char *name) {
    size_t cap;
    MazenTargetConfig *target;

    if (list->len == list->cap) {
        cap = list->cap == 0 ? 4 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }

    target = &list->items[list->len++];
    mazen_target_config_init(target);
    target->name = mazen_xstrdup(name);
    return target;
}

const char *mazen_target_type_name(MazenTargetType type) {
    switch (type) {
    case MAZEN_TARGET_STATIC_LIBRARY:
        return "static-library";
    case MAZEN_TARGET_SHARED_LIBRARY:
        return "shared-library";
    case MAZEN_TARGET_EXECUTABLE:
    default:
        return "executable";
    }
}

bool mazen_target_type_from_text(const char *text, MazenTargetType *type) {
    if (strcmp(text, "executable") == 0) {
        *type = MAZEN_TARGET_EXECUTABLE;
        return true;
    }
    if (strcmp(text, "static") == 0 || strcmp(text, "static-library") == 0 ||
        strcmp(text, "static_library") == 0) {
        *type = MAZEN_TARGET_STATIC_LIBRARY;
        return true;
    }
    if (strcmp(text, "shared") == 0 || strcmp(text, "shared-library") == 0 ||
        strcmp(text, "shared_library") == 0) {
        *type = MAZEN_TARGET_SHARED_LIBRARY;
        return true;
    }
    return false;
}

void resolved_target_init(ResolvedTarget *target) {
    target->name = NULL;
    target->type = MAZEN_TARGET_EXECUTABLE;
    target->output_path = NULL;
    target->entry_path = NULL;
    target->install_dir = NULL;
    target->from_config = false;
    target->is_default = false;
    target->is_test = false;
    target->install_enabled = false;
    string_list_init(&target->source_paths);
    string_list_init(&target->include_dirs);
    string_list_init(&target->libs);
    string_list_init(&target->cflags);
    string_list_init(&target->ldflags);
    string_list_init(&target->dep_names);
    string_list_init(&target->dependency_outputs);
    string_list_init(&target->runtime_library_dirs);
}

void resolved_target_free(ResolvedTarget *target) {
    free(target->name);
    free(target->output_path);
    free(target->entry_path);
    free(target->install_dir);
    string_list_free(&target->source_paths);
    string_list_free(&target->include_dirs);
    string_list_free(&target->libs);
    string_list_free(&target->cflags);
    string_list_free(&target->ldflags);
    string_list_free(&target->dep_names);
    string_list_free(&target->dependency_outputs);
    string_list_free(&target->runtime_library_dirs);
    resolved_target_init(target);
}

void resolved_target_list_init(ResolvedTargetList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void resolved_target_list_free(ResolvedTargetList *list) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        resolved_target_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static ResolvedTarget *resolved_target_list_push(ResolvedTargetList *list) {
    size_t cap;

    if (list->len == list->cap) {
        cap = list->cap == 0 ? 4 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }

    resolved_target_init(&list->items[list->len]);
    return &list->items[list->len++];
}

static bool resolved_target_name_exists(const ResolvedTargetList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (list->items[i].name != NULL && strcmp(list->items[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static char *make_unique_target_name(const ResolvedTargetList *list, const char *base_name) {
    size_t suffix = 2;

    if (!resolved_target_name_exists(list, base_name)) {
        return mazen_xstrdup(base_name);
    }

    for (;;) {
        char *candidate = mazen_format("%s_%zu", base_name, suffix++);
        if (!resolved_target_name_exists(list, candidate)) {
            return candidate;
        }
        free(candidate);
    }
}

static bool determine_batch_default_name(const struct MazenConfig *config, const char **name_out, Diagnostic *diag) {
    const MazenTargetConfig *chosen = NULL;
    size_t i;
    size_t default_count = 0;

    *name_out = NULL;

    if (config->default_target != NULL) {
        chosen = mazen_target_config_list_find_const(&config->targets, config->default_target);
        if (chosen == NULL) {
            diag_set(diag, "default_target `%s` does not match a configured target", config->default_target);
            return false;
        }
        *name_out = chosen->name;
        return true;
    }

    for (i = 0; i < config->targets.len; ++i) {
        if (config->targets.items[i].default_target) {
            chosen = &config->targets.items[i];
            ++default_count;
        }
    }

    if (default_count == 1) {
        *name_out = chosen->name;
        return true;
    }
    if (config->targets.len == 1) {
        *name_out = config->targets.items[0].name;
    }
    return true;
}

static char *legacy_batch_target_basename(const ProjectInfo *project, const struct MazenConfig *config,
                                          const SourceFile *source, size_t source_index, size_t entry_count) {
    if (entry_count == 1 || (project->has_selected_entry && source_index == project->selected_entry)) {
        return mazen_xstrdup(config->name != NULL ? config->name : project->project_name);
    }
    return sanitize_stem(source->path);
}

static const ResolvedTarget *resolved_target_list_find_const(const ResolvedTargetList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

static size_t config_target_index(const struct MazenConfig *config, const char *name) {
    size_t i;

    for (i = 0; i < config->targets.len; ++i) {
        if (strcmp(config->targets.items[i].name, name) == 0) {
            return i;
        }
    }
    return config->targets.len;
}

static bool source_list_looks_like_test(const ProjectInfo *project, const StringList *paths) {
    size_t i;

    for (i = 0; i < paths->len; ++i) {
        const SourceFile *source = project_find_source(project, paths->items[i], NULL);

        if (source != NULL && source->role == SOURCE_ROLE_TEST) {
            return true;
        }
        if (text_looks_like_test(paths->items[i])) {
            return true;
        }
    }
    return false;
}

static bool target_cfg_looks_like_test(const ProjectInfo *project, const MazenTargetConfig *target_cfg) {
    size_t i;

    if (text_looks_like_test(target_cfg->name) || text_looks_like_test(target_cfg->entry)) {
        return true;
    }
    for (i = 0; i < target_cfg->src_dirs.len; ++i) {
        if (text_looks_like_test(target_cfg->src_dirs.items[i])) {
            return true;
        }
    }
    if (source_list_looks_like_test(project, &target_cfg->sources)) {
        return true;
    }
    return false;
}

static void add_runtime_dir(StringList *dirs, const char *path) {
    char *dir = path_dirname(path);
    string_list_push_unique_take(dirs, dir);
}

static bool finalize_resolved_dependencies(ResolvedTargetList *list, ResolvedTarget *resolved, Diagnostic *diag) {
    size_t i;

    for (i = 0; i < resolved->dep_names.len; ++i) {
        const ResolvedTarget *dependency = resolved_target_list_find_const(list, resolved->dep_names.items[i]);
        size_t j;

        if (dependency == NULL) {
            diag_set(diag, "internal error: dependency `%s` was not resolved before `%s`", resolved->dep_names.items[i],
                     resolved->name);
            return false;
        }
        if (dependency->type == MAZEN_TARGET_EXECUTABLE) {
            diag_set(diag, "target `%s` cannot depend on executable target `%s`", resolved->name, dependency->name);
            return false;
        }

        for (j = 0; j < dependency->include_dirs.len; ++j) {
            string_list_push_unique(&resolved->include_dirs, dependency->include_dirs.items[j]);
        }
        for (j = 0; j < dependency->libs.len; ++j) {
            string_list_push_unique(&resolved->libs, dependency->libs.items[j]);
        }

        string_list_push_unique(&resolved->dependency_outputs, dependency->output_path);
        for (j = 0; j < dependency->dependency_outputs.len; ++j) {
            string_list_push_unique(&resolved->dependency_outputs, dependency->dependency_outputs.items[j]);
        }

        if (dependency->type == MAZEN_TARGET_SHARED_LIBRARY) {
            add_runtime_dir(&resolved->runtime_library_dirs, dependency->output_path);
        }
        for (j = 0; j < dependency->runtime_library_dirs.len; ++j) {
            string_list_push_unique(&resolved->runtime_library_dirs, dependency->runtime_library_dirs.items[j]);
        }
    }

    return true;
}

static void mark_default_target(ResolvedTargetList *list, const char *name) {
    size_t i;

    if (name == NULL) {
        return;
    }
    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            list->items[i].is_default = true;
            return;
        }
    }
}

static const MazenTargetConfig *select_config_target(const struct MazenConfig *config, const char *requested_name,
                                                     bool *is_default, Diagnostic *diag) {
    const MazenTargetConfig *chosen = NULL;
    size_t i;
    size_t default_count = 0;

    *is_default = false;

    if (config->targets.len == 0) {
        return NULL;
    }

    if (requested_name != NULL) {
        chosen = mazen_target_config_list_find_const(&config->targets, requested_name);
        if (chosen == NULL) {
            diag_set(diag, "target `%s` is not defined in mazen.toml", requested_name);
            return NULL;
        }
        return chosen;
    }

    if (config->default_target != NULL) {
        chosen = mazen_target_config_list_find_const(&config->targets, config->default_target);
        if (chosen == NULL) {
            diag_set(diag, "default_target `%s` does not match a configured target", config->default_target);
            return NULL;
        }
        *is_default = true;
        return chosen;
    }

    for (i = 0; i < config->targets.len; ++i) {
        if (config->targets.items[i].default_target) {
            chosen = &config->targets.items[i];
            ++default_count;
        }
    }
    if (default_count > 1) {
        diag_set(diag, "multiple targets are marked as default in mazen.toml");
        return NULL;
    }
    if (default_count == 1) {
        *is_default = true;
        return chosen;
    }

    if (config->targets.len == 1) {
        *is_default = true;
        return &config->targets.items[0];
    }

    diag_set(diag, "multiple explicit targets are configured; use `--target NAME` or set `default_target`");
    return NULL;
}

static bool resolve_config_target(const ProjectInfo *project, const struct MazenConfig *config,
                                  const MazenTargetConfig *target_cfg, bool is_default, ResolvedTarget *resolved,
                                  Diagnostic *diag) {
    StringList excludes;
    const SourceFile *entry_source = NULL;
    size_t entry_index = 0;
    size_t i;
    size_t main_count = 0;

    resolved->from_config = true;
    resolved->is_default = is_default;
    resolved->name = mazen_xstrdup(target_cfg->name);
    resolved->type = target_cfg->type_set ? target_cfg->type : MAZEN_TARGET_EXECUTABLE;
    resolved->is_test = target_cfg->test_set ? target_cfg->test_target : target_cfg_looks_like_test(project, target_cfg);
    resolved->install_enabled = target_cfg->install_set ? target_cfg->install_target : !resolved->is_test;
    resolved->install_dir = target_cfg->install_dir != NULL ? mazen_xstrdup(target_cfg->install_dir) : NULL;
    string_list_copy_unique(&resolved->include_dirs, &target_cfg->include_dirs);
    string_list_copy_unique(&resolved->libs, &target_cfg->libs);
    string_list_copy_unique(&resolved->cflags, &target_cfg->cflags);
    string_list_copy_unique(&resolved->ldflags, &target_cfg->ldflags);
    string_list_copy_unique(&resolved->dep_names, &target_cfg->deps);

    string_list_init(&excludes);
    string_list_copy_unique(&excludes, &config->exclude);
    string_list_copy_unique(&excludes, &target_cfg->exclude);

    if (target_cfg->sources.len > 0 &&
        !collect_explicit_sources(project, &target_cfg->sources, &excludes, &resolved->source_paths, diag)) {
        string_list_free(&excludes);
        return false;
    }
    if (target_cfg->src_dirs.len > 0) {
        collect_sources_from_dirs(project, &target_cfg->src_dirs, &excludes, &resolved->source_paths);
    } else if (target_cfg->sources.len == 0) {
        if (config->sources.len > 0 &&
            !collect_explicit_sources(project, &config->sources, &excludes, &resolved->source_paths, diag)) {
            string_list_free(&excludes);
            return false;
        } else if (resolved->source_paths.len == 0 && config->src_dirs.len > 0) {
            collect_sources_from_dirs(project, &config->src_dirs, &excludes, &resolved->source_paths);
        }
    }

    if (target_cfg->entry != NULL) {
        entry_source = project_find_source(project, target_cfg->entry, &entry_index);
        if (entry_source == NULL) {
            string_list_free(&excludes);
            diag_set(diag, "target `%s` entry `%s` was not found", target_cfg->name, target_cfg->entry);
            return false;
        }
        if (!entry_source->has_main) {
            string_list_free(&excludes);
            diag_set(diag, "target `%s` entry `%s` does not define main()", target_cfg->name, target_cfg->entry);
            return false;
        }
        resolved->entry_path = mazen_xstrdup(entry_source->path);
    }

    if (resolved->type == MAZEN_TARGET_EXECUTABLE) {
        if (resolved->source_paths.len == 0 && resolved->entry_path != NULL) {
            collect_legacy_executable_sources(project, entry_index, &resolved->source_paths);
        }
        if (resolved->entry_path != NULL) {
            string_list_push_unique(&resolved->source_paths, resolved->entry_path);
        }
        if (resolved->source_paths.len == 0) {
            string_list_free(&excludes);
            diag_set(diag, "target `%s` needs `entry`, `sources`, or `src_dirs`", target_cfg->name);
            return false;
        }
        if (resolved->entry_path == NULL) {
            for (i = 0; i < resolved->source_paths.len; ++i) {
                size_t source_index;
                const SourceFile *source = project_find_source(project, resolved->source_paths.items[i], &source_index);
                if (source != NULL && source->has_main) {
                    resolved->entry_path = mazen_xstrdup(source->path);
                    entry_index = source_index;
                    ++main_count;
                }
            }
            if (main_count == 0) {
                string_list_free(&excludes);
                diag_set(diag, "executable target `%s` does not include an entrypoint", target_cfg->name);
                return false;
            }
            if (main_count > 1) {
                string_list_free(&excludes);
                diag_set(diag, "executable target `%s` includes multiple entrypoints; set `entry = \"...\"`",
                         target_cfg->name);
                return false;
            }
        }

        if (resolved->entry_path != NULL) {
            StringList filtered;
            string_list_init(&filtered);
            for (i = 0; i < resolved->source_paths.len; ++i) {
                const SourceFile *source = project_find_source(project, resolved->source_paths.items[i], NULL);
                if (source == NULL) {
                    continue;
                }
                if (source->has_main && strcmp(source->path, resolved->entry_path) != 0) {
                    continue;
                }
                string_list_push_unique(&filtered, source->path);
            }
            string_list_free(&resolved->source_paths);
            resolved->source_paths = filtered;
        }
    } else {
        if (resolved->source_paths.len == 0) {
            string_list_free(&excludes);
            diag_set(diag, "library target `%s` needs `sources` or `src_dirs`", target_cfg->name);
            return false;
        }
        for (i = 0; i < resolved->source_paths.len; ++i) {
            const SourceFile *source = project_find_source(project, resolved->source_paths.items[i], NULL);
            if (source != NULL && source->has_main) {
                string_list_free(&excludes);
                diag_set(diag, "library target `%s` includes entrypoint source `%s`", target_cfg->name, source->path);
                return false;
            }
        }
    }

    if (target_cfg->output != NULL) {
        resolved->output_path = mazen_xstrdup(target_cfg->output);
    } else {
        resolved->output_path = default_output_path(config, target_cfg);
    }

    string_list_free(&excludes);
    return true;
}

bool mazen_target_resolve(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                          ResolvedTarget *resolved, Diagnostic *diag) {
    const MazenTargetConfig *target_cfg;
    bool is_default = false;

    if (config->targets.len == 0) {
        size_t entry_index = 0;

        if (requested_name != NULL) {
            diag_set(diag, "target `%s` was requested, but mazen.toml does not define explicit targets",
                     requested_name);
            return false;
        }
        resolved->from_config = false;
        resolved->is_default = true;
        resolved->name = mazen_xstrdup(config->name != NULL ? config->name : project->project_name);
        resolved->type = MAZEN_TARGET_EXECUTABLE;
        resolved->is_test = false;
        resolved->install_enabled = true;
        resolved->output_path = mazen_format("build/%s", config->name != NULL ? config->name : project->project_name);
        if (project->has_selected_entry) {
            entry_index = project->selected_entry;
            resolved->entry_path = mazen_xstrdup(project->sources.items[entry_index].path);
            collect_legacy_executable_sources(project, entry_index, &resolved->source_paths);
        }
        return true;
    }

    target_cfg = select_config_target(config, requested_name, &is_default, diag);
    if (target_cfg == NULL) {
        return false;
    }

    return resolve_config_target(project, config, target_cfg, is_default, resolved, diag);
}

bool mazen_target_resolve_all(const ProjectInfo *project, const struct MazenConfig *config, ResolvedTargetList *list,
                              Diagnostic *diag) {
    if (config->targets.len == 0) {
        size_t i;
        size_t entry_count = 0;

        for (i = 0; i < project->sources.len; ++i) {
            if (project->sources.items[i].has_main) {
                ++entry_count;
            }
        }

        if (entry_count <= 1) {
            ResolvedTarget single;
            ResolvedTarget *slot;

            resolved_target_init(&single);
            if (!mazen_target_resolve(project, config, NULL, &single, diag)) {
                resolved_target_free(&single);
                return false;
            }
            slot = resolved_target_list_push(list);
            *slot = single;
            return true;
        }

        for (i = 0; i < project->sources.len; ++i) {
            const SourceFile *source = &project->sources.items[i];
            ResolvedTarget *resolved;
            char *base_name;
            char *unique_name;

            if (!source->has_main) {
                continue;
            }

            resolved = resolved_target_list_push(list);
            resolved->from_config = false;
            resolved->is_default = project->has_selected_entry && i == project->selected_entry;
            resolved->type = MAZEN_TARGET_EXECUTABLE;
            resolved->is_test = source->role == SOURCE_ROLE_TEST;
            resolved->install_enabled = !resolved->is_test;
            base_name = legacy_batch_target_basename(project, config, source, i, entry_count);
            unique_name = make_unique_target_name(list, base_name);
            free(base_name);
            resolved->name = unique_name;
            resolved->output_path = mazen_format("build/%s", resolved->name);
            resolved->entry_path = mazen_xstrdup(source->path);
            collect_legacy_executable_sources(project, i, &resolved->source_paths);
            string_list_push_unique(&resolved->source_paths, source->path);
        }

        return true;
    }

    {
        const char *default_name = NULL;
        size_t i;

        if (!determine_batch_default_name(config, &default_name, diag)) {
            return false;
        }

        for (i = 0; i < config->targets.len; ++i) {
            ResolvedTarget resolved;
            ResolvedTarget *slot;
            bool is_default = default_name != NULL && strcmp(default_name, config->targets.items[i].name) == 0;

            resolved_target_init(&resolved);
            if (!resolve_config_target(project, config, &config->targets.items[i], is_default, &resolved, diag)) {
                resolved_target_free(&resolved);
                return false;
            }
            slot = resolved_target_list_push(list);
            *slot = resolved;
        }
    }

    return true;
}

static bool collect_config_graph_target(const ProjectInfo *project, const struct MazenConfig *config, size_t index,
                                        unsigned char *states, StringList *stack, ResolvedTargetList *list,
                                        Diagnostic *diag) {
    const MazenTargetConfig *target_cfg = &config->targets.items[index];
    size_t i;
    ResolvedTarget resolved;
    ResolvedTarget *slot;

    if (states[index] == 2) {
        return true;
    }
    if (states[index] == 1) {
        String cycle;
        size_t j;

        string_init(&cycle);
        for (j = 0; j < stack->len; ++j) {
            if (j > 0) {
                string_append(&cycle, " -> ");
            }
            string_append(&cycle, stack->items[j]);
        }
        if (stack->len > 0) {
            string_append(&cycle, " -> ");
        }
        string_append(&cycle, target_cfg->name);
        diag_set(diag, "target dependency cycle detected: %s", cycle.data != NULL ? cycle.data : target_cfg->name);
        string_free(&cycle);
        return false;
    }

    states[index] = 1;
    string_list_push(stack, target_cfg->name);
    for (i = 0; i < target_cfg->deps.len; ++i) {
        size_t dep_index = config_target_index(config, target_cfg->deps.items[i]);
        if (dep_index == config->targets.len) {
            diag_set(diag, "target `%s` depends on undefined target `%s`", target_cfg->name, target_cfg->deps.items[i]);
            states[index] = 0;
            stack->len -= 1;
            free(stack->items[stack->len]);
            stack->items[stack->len] = NULL;
            return false;
        }
        if (!collect_config_graph_target(project, config, dep_index, states, stack, list, diag)) {
            states[index] = 0;
            stack->len -= 1;
            free(stack->items[stack->len]);
            stack->items[stack->len] = NULL;
            return false;
        }
    }

    resolved_target_init(&resolved);
    if (!resolve_config_target(project, config, target_cfg, false, &resolved, diag)) {
        states[index] = 0;
        stack->len -= 1;
        free(stack->items[stack->len]);
        stack->items[stack->len] = NULL;
        resolved_target_free(&resolved);
        return false;
    }
    if (!finalize_resolved_dependencies(list, &resolved, diag)) {
        states[index] = 0;
        stack->len -= 1;
        free(stack->items[stack->len]);
        stack->items[stack->len] = NULL;
        resolved_target_free(&resolved);
        return false;
    }

    slot = resolved_target_list_push(list);
    *slot = resolved;
    states[index] = 2;
    stack->len -= 1;
    free(stack->items[stack->len]);
    stack->items[stack->len] = NULL;
    return true;
}

static bool select_test_root(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                             const char *filter, bool *selected, Diagnostic *diag) {
    size_t i;
    size_t selected_count = 0;

    (void) project;
    for (i = 0; i < config->targets.len; ++i) {
        const MazenTargetConfig *target = &config->targets.items[i];
        bool is_test = target->test_set ? target->test_target : target_cfg_looks_like_test(project, target);
        bool matches = true;

        if (!is_test) {
            selected[i] = false;
            continue;
        }
        if (requested_name != NULL) {
            matches = strcmp(target->name, requested_name) == 0;
        } else if (filter != NULL) {
            matches = strstr(target->name, filter) != NULL ||
                      (target->entry != NULL && strstr(target->entry, filter) != NULL);
        }

        selected[i] = matches;
        if (matches) {
            ++selected_count;
        }
    }

    if (requested_name != NULL && selected_count == 0) {
        diag_set(diag, "target `%s` is not defined as a test target", requested_name);
        return false;
    }
    if (filter != NULL && selected_count == 0) {
        diag_set(diag, "no test targets matched `%s`", filter);
        return false;
    }
    if (requested_name == NULL && filter == NULL && selected_count == 0) {
        diag_set(diag, "no test targets are defined");
        diag_set_hint(diag, "Mark explicit test targets with `test = true` in mazen.toml");
        return false;
    }

    return true;
}

bool mazen_target_plan_build(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                             bool all_targets, ResolvedTargetList *list, Diagnostic *diag) {
    if (config->targets.len == 0) {
        if (all_targets) {
            return mazen_target_resolve_all(project, config, list, diag);
        }

        {
            ResolvedTarget resolved;
            ResolvedTarget *slot;

            resolved_target_init(&resolved);
            if (!mazen_target_resolve(project, config, requested_name, &resolved, diag)) {
                resolved_target_free(&resolved);
                return false;
            }
            slot = resolved_target_list_push(list);
            *slot = resolved;
            return true;
        }
    }

    {
        unsigned char *states = mazen_xcalloc(config->targets.len, sizeof(*states));
        StringList stack;
        const char *default_name = NULL;
        bool ok = true;
        size_t i;

        string_list_init(&stack);

        if (all_targets) {
            if (!determine_batch_default_name(config, &default_name, diag)) {
                ok = false;
            } else {
                for (i = 0; ok && i < config->targets.len; ++i) {
                    ok = collect_config_graph_target(project, config, i, states, &stack, list, diag);
                }
            }
        } else {
            const MazenTargetConfig *selected = NULL;
            bool ignored_default = false;

            selected = select_config_target(config, requested_name, &ignored_default, diag);
            if (selected == NULL) {
                ok = false;
            } else {
                default_name = selected->name;
                ok = collect_config_graph_target(project, config, config_target_index(config, selected->name), states,
                                                 &stack, list, diag);
            }
        }

        if (ok) {
            mark_default_target(list, default_name);
        }

        string_list_free(&stack);
        free(states);
        return ok;
    }
}

bool mazen_target_plan_tests(const ProjectInfo *project, const struct MazenConfig *config, const char *requested_name,
                             const char *filter, ResolvedTargetList *list, Diagnostic *diag) {
    if (config->targets.len == 0) {
        size_t i;

        if (requested_name != NULL) {
            diag_set(diag, "test target `%s` was requested, but mazen.toml does not define explicit targets",
                     requested_name);
            return false;
        }

        for (i = 0; i < project->sources.len; ++i) {
            const SourceFile *source = &project->sources.items[i];
            ResolvedTarget *resolved;
            char *base_name;

            if (!source->has_main || source->role != SOURCE_ROLE_TEST) {
                continue;
            }
            if (filter != NULL && strstr(source->path, filter) == NULL) {
                continue;
            }

            resolved = resolved_target_list_push(list);
            resolved->from_config = false;
            resolved->is_test = true;
            resolved->install_enabled = false;
            resolved->type = MAZEN_TARGET_EXECUTABLE;
            base_name = sanitize_stem(source->path);
            resolved->name = base_name;
            resolved->output_path = mazen_format("build/%s", resolved->name);
            resolved->entry_path = mazen_xstrdup(source->path);
            collect_legacy_executable_sources(project, i, &resolved->source_paths);
            string_list_push_unique(&resolved->source_paths, source->path);
        }

        if (list->len == 0) {
            diag_set(diag, filter != NULL ? "no test entrypoints matched `%s`" : "no test entrypoints were detected",
                     filter);
            diag_set_hint(diag, "Add test sources under `tests/` or define explicit test targets in mazen.toml");
            return false;
        }
        return true;
    }

    {
        unsigned char *states = mazen_xcalloc(config->targets.len, sizeof(*states));
        bool *selected = mazen_xcalloc(config->targets.len, sizeof(*selected));
        StringList stack;
        bool ok;
        size_t i;

        string_list_init(&stack);
        ok = select_test_root(project, config, requested_name, filter, selected, diag);
        if (ok) {
            for (i = 0; ok && i < config->targets.len; ++i) {
                if (!selected[i]) {
                    continue;
                }
                ok = collect_config_graph_target(project, config, i, states, &stack, list, diag);
            }
        }

        string_list_free(&stack);
        free(selected);
        free(states);
        return ok;
    }
}

void mazen_target_print_list(const ProjectInfo *project, const struct MazenConfig *config) {
    size_t i;
    (void) project;

    puts("Configured targets:");
    for (i = 0; i < config->targets.len; ++i) {
        const MazenTargetConfig *target = &config->targets.items[i];
        printf("    %s type=%s", target->name, mazen_target_type_name(target->type_set ? target->type
                                                                                         : MAZEN_TARGET_EXECUTABLE));
        if ((config->default_target != NULL && strcmp(config->default_target, target->name) == 0) ||
            (config->default_target == NULL && target->default_target)) {
            printf(" [default]");
        }
        if (target->test_set ? target->test_target : target_cfg_looks_like_test(project, target)) {
            printf(" [test]");
        }
        if (target->install_set && !target->install_target) {
            printf(" [no-install]");
        }
        if (target->entry != NULL) {
            printf(" entry=%s", target->entry);
        }
        if (target->output != NULL) {
            printf(" output=%s", target->output);
        }
        if (target->deps.len > 0) {
            size_t j;
            printf(" deps=");
            for (j = 0; j < target->deps.len; ++j) {
                printf("%s%s", j == 0 ? "" : ",", target->deps.items[j]);
            }
        }
        putchar('\n');
    }
}
