#include "doctor.h"

#include "diag.h"

#include <stdio.h>

static void print_string_list(const char *title, const StringList *list) {
    size_t i;
    printf("%s\n", title);
    if (list->len == 0) {
        puts("    (none)");
        return;
    }
    for (i = 0; i < list->len; ++i) {
        printf("    %s\n", list->items[i]);
    }
}

void doctor_print_project(const ProjectInfo *project, const MazenConfig *config, const char *compiler,
                          const MazenCStandard *standard, const ResolvedTarget *target, int jobs,
                          const char *compile_commands_path) {
    size_t i;

    printf("Project root: %s\n", project->root_dir);
    printf("Project name: %s\n", project->project_name);
    printf("Compiler: %s\n", compiler);
    printf("C standard: %s\n", standard != NULL ? standard->name : mazen_standard_default()->name);
    if (target != NULL) {
        printf("Target: %s\n", target->name);
        printf("Target type: %s\n", mazen_target_type_name(target->type));
        printf("Target output: %s\n", target->output_path);
        if (target->entry_path != NULL) {
            printf("Target entrypoint: %s\n", target->entry_path);
        }
    }
    if (jobs > 0) {
        printf("Parallel jobs: %d\n", jobs);
    } else {
        puts("Parallel jobs: auto");
    }
    printf("Compile database: %s\n", compile_commands_path != NULL ? compile_commands_path : "(disabled)");
    printf("Discovery mode: %s\n", discovery_mode_name(project->mode));
    printf("Configuration: %s\n", config->present ? config->path : "(none)");
    printf("Sources: %zu\n", project->sources.len);
    print_string_list("Source roots:", &project->source_roots);
    print_string_list("Include directories:", &project->include_dirs);
    print_string_list("Vendor directories:", &project->vendor_dirs);
    print_string_list("Ignored directories:", &project->ignored_dirs);
    puts("Detected sources:");
    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        printf("    %-18s %s", source_role_name(source->role), source->path);
        if (source->has_main) {
            printf(" [main score=%d]", source->entry_score);
        }
        putchar('\n');
    }
    if (project->has_selected_entry) {
        printf("Selected entrypoint: %s\n", project->sources.items[project->selected_entry].path);
    } else {
        puts("Selected entrypoint: (none)");
    }
}

void doctor_print_targets(const ProjectInfo *project) {
    size_t i;
    bool found = false;

    puts("Detected targets:");
    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        if (!source->has_main) {
            continue;
        }
        found = true;
        printf("    %s", source->path);
        if (project->has_selected_entry && i == project->selected_entry) {
            printf(" [selected]");
        }
        printf(" score=%d role=%s\n", source->entry_score, source_role_name(source->role));
    }
    if (!found) {
        puts("    (no entrypoints detected)");
    }
}
