#include "classifier.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TEST_NAMES[] = {"test", "tests", "spec", "specs"};
static const char *EXAMPLE_NAMES[] = {"example", "examples", "demo", "demos", "sample", "samples"};
static const char *EXPERIMENT_NAMES[] = {"experiment", "experiments", "old", "backup", "scratch", "playground"};
static const char *TOOL_NAMES[] = {"tool", "tools", "script", "scripts"};
static const char *VENDOR_NAMES[] = {"vendor", "vendors", "third_party", "third-party", "external", "deps"};

static bool list_component_match(const char *path, const char *const *items, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (path_has_component(path, items[i])) {
            return true;
        }
    }
    return false;
}

static bool basename_matches_pattern(const char *basename, const char *needle) {
    return strstr(basename, needle) != NULL;
}

static bool range_contains(const char *text, size_t len, const char *needle) {
    size_t needle_len = strlen(needle);
    size_t i;

    if (needle_len == 0 || needle_len > len) {
        return false;
    }

    for (i = 0; i + needle_len <= len; ++i) {
        if (strncmp(text + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool includes_test_framework(const char *content) {
    const char *cursor = content;

    while (*cursor != '\0') {
        while (*cursor != '\0' && *cursor != '#') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (strncmp(cursor, "#include", 8) == 0 &&
            (cursor[8] == '\0' || isspace((unsigned char) cursor[8]))) {
            const char *line_end = strchr(cursor, '\n');
            size_t len = line_end == NULL ? strlen(cursor) : (size_t) (line_end - cursor);
            if (range_contains(cursor, len, "criterion/criterion.h") ||
                range_contains(cursor, len, "cmocka.h") ||
                range_contains(cursor, len, "unity.h") ||
                range_contains(cursor, len, "munit.h") ||
                range_contains(cursor, len, "check.h")) {
                return true;
            }
        }
        while (*cursor != '\0' && *cursor != '\n') {
            ++cursor;
        }
    }

    return false;
}

static char *strip_comments_and_literals(const char *content) {
    String out;
    size_t i = 0;
    enum {
        STATE_NORMAL,
        STATE_SLASH,
        STATE_LINE_COMMENT,
        STATE_BLOCK_COMMENT,
        STATE_BLOCK_COMMENT_STAR,
        STATE_STRING,
        STATE_STRING_ESCAPE,
        STATE_CHAR,
        STATE_CHAR_ESCAPE
    } state = STATE_NORMAL;

    string_init(&out);
    while (content[i] != '\0') {
        char ch = content[i++];
        switch (state) {
        case STATE_NORMAL:
            if (ch == '/') {
                state = STATE_SLASH;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_STRING;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_CHAR;
            } else {
                string_append_char(&out, ch);
            }
            break;
        case STATE_SLASH:
            if (ch == '/') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_LINE_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            } else {
                string_append_char(&out, '/');
                string_append_char(&out, ch);
                state = STATE_NORMAL;
            }
            break;
        case STATE_LINE_COMMENT:
            if (ch == '\n') {
                string_append_char(&out, '\n');
                state = STATE_NORMAL;
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_BLOCK_COMMENT:
            if (ch == '*') {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT_STAR;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_BLOCK_COMMENT_STAR:
            if (ch == '/') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
                state = STATE_BLOCK_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
            } else {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            }
            break;
        case STATE_STRING:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_STRING_ESCAPE;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_STRING_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_STRING;
            break;
        case STATE_CHAR:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_CHAR_ESCAPE;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_CHAR_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_CHAR;
            break;
        }
    }

    if (state == STATE_SLASH) {
        string_append_char(&out, '/');
    }
    return string_take(&out);
}

static bool consume_main_definition(const char *sanitized) {
    const char *cursor = sanitized;

    while (*cursor != '\0') {
        if (strncmp(cursor, "int", 3) == 0 &&
            (cursor == sanitized || !(isalnum((unsigned char) cursor[-1]) || cursor[-1] == '_')) &&
            !(isalnum((unsigned char) cursor[3]) || cursor[3] == '_')) {
            const char *walk = cursor + 3;
            int depth = 0;
            while (*walk != '\0' && isspace((unsigned char) *walk)) {
                ++walk;
            }
            if (strncmp(walk, "main", 4) != 0 ||
                (isalnum((unsigned char) walk[4]) || walk[4] == '_')) {
                ++cursor;
                continue;
            }
            walk += 4;
            while (*walk != '\0' && isspace((unsigned char) *walk)) {
                ++walk;
            }
            if (*walk != '(') {
                ++cursor;
                continue;
            }
            depth = 1;
            ++walk;
            while (*walk != '\0' && depth > 0) {
                if (*walk == '(') {
                    ++depth;
                } else if (*walk == ')') {
                    --depth;
                }
                ++walk;
            }
            if (depth != 0) {
                return false;
            }
            while (*walk != '\0' && isspace((unsigned char) *walk)) {
                ++walk;
            }
            if (*walk == '{') {
                return true;
            }
        }
        ++cursor;
    }

    return false;
}

static void collect_includes(const char *content, StringList *includes) {
    const char *cursor = content;

    while (*cursor != '\0') {
        while (*cursor != '\0' && *cursor != '#') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        ++cursor;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (strncmp(cursor, "include", 7) != 0 || (isalnum((unsigned char) cursor[7]) || cursor[7] == '_')) {
            while (*cursor != '\0' && *cursor != '\n') {
                ++cursor;
            }
            continue;
        }
        cursor += 7;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (*cursor == '"') {
            String include;
            string_init(&include);
            ++cursor;
            while (*cursor != '\0' && *cursor != '"' && *cursor != '\n') {
                if (*cursor == '\\' && cursor[1] != '\0') {
                    ++cursor;
                }
                string_append_char(&include, *cursor++);
            }
            if (*cursor == '"') {
                string_list_push_unique_take(includes, string_take(&include));
            } else {
                string_free(&include);
            }
        } else {
            while (*cursor != '\0' && *cursor != '\n') {
                ++cursor;
            }
        }
    }
}

static SourceRole classify_role(const char *path, const char *basename, const char *content) {
    if (list_component_match(path, VENDOR_NAMES, MAZEN_ARRAY_LEN(VENDOR_NAMES))) {
        return SOURCE_ROLE_VENDOR;
    }
    if (list_component_match(path, TEST_NAMES, MAZEN_ARRAY_LEN(TEST_NAMES)) ||
        basename_matches_pattern(basename, "_test") ||
        basename_matches_pattern(basename, "test_") ||
        includes_test_framework(content)) {
        return SOURCE_ROLE_TEST;
    }
    if (list_component_match(path, EXAMPLE_NAMES, MAZEN_ARRAY_LEN(EXAMPLE_NAMES)) ||
        basename_matches_pattern(basename, "example") ||
        basename_matches_pattern(basename, "demo")) {
        return SOURCE_ROLE_EXAMPLE;
    }
    if (list_component_match(path, EXPERIMENT_NAMES, MAZEN_ARRAY_LEN(EXPERIMENT_NAMES)) ||
        basename_matches_pattern(basename, "scratch") ||
        basename_matches_pattern(basename, "playground")) {
        return SOURCE_ROLE_EXPERIMENTAL;
    }
    if (list_component_match(path, TOOL_NAMES, MAZEN_ARRAY_LEN(TOOL_NAMES))) {
        return SOURCE_ROLE_TOOL;
    }
    return SOURCE_ROLE_GENERAL;
}

static bool path_in_source_roots(const char *path, const StringList *roots) {
    size_t i;
    for (i = 0; i < roots->len; ++i) {
        size_t len = strlen(roots->items[i]);
        if (strcmp(path, roots->items[i]) == 0) {
            return true;
        }
        if (string_starts_with(path, roots->items[i]) && path[len] == '/') {
            return true;
        }
    }
    return false;
}

static int entry_score(const SourceFile *source, const StringList *source_roots) {
    int score = 0;

    if (strcmp(source->path, "main.c") == 0) {
        score += 1000;
    } else if (strcmp(source->path, "src/main.c") == 0 || strcmp(source->path, "source/main.c") == 0) {
        score += 950;
    } else if (strcmp(source->basename, "main.c") == 0) {
        score += 600;
    }

    if (path_in_source_roots(source->path, source_roots)) {
        score += 180;
    }
    if (strcmp(source->directory, ".") == 0) {
        score += 120;
    }

    switch (source->role) {
    case SOURCE_ROLE_GENERAL:
        score += 160;
        break;
    case SOURCE_ROLE_TOOL:
        score -= 120;
        break;
    case SOURCE_ROLE_EXAMPLE:
        score -= 240;
        break;
    case SOURCE_ROLE_TEST:
        score -= 320;
        break;
    case SOURCE_ROLE_EXPERIMENTAL:
        score -= 420;
        break;
    case SOURCE_ROLE_VENDOR:
        score -= 700;
        break;
    }

    score -= (int) path_depth(source->path) * 5;
    return score;
}

static bool better_entry_candidate(const SourceFile *lhs, const SourceFile *rhs) {
    if (lhs->entry_score != rhs->entry_score) {
        return lhs->entry_score > rhs->entry_score;
    }
    if (path_depth(lhs->path) != path_depth(rhs->path)) {
        return path_depth(lhs->path) < path_depth(rhs->path);
    }
    return strcmp(lhs->path, rhs->path) < 0;
}

static void add_include_root(StringList *roots, const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }
    string_list_push_unique(roots, path);
}

static void infer_include_roots(ProjectInfo *project, const ScanResult *scan, const MazenConfig *config) {
    size_t i, j;

    add_include_root(&project->include_dirs, ".");
    for (i = 0; i < config->include_dirs.len; ++i) {
        add_include_root(&project->include_dirs, config->include_dirs.items[i]);
    }
    for (i = 0; i < scan->include_roots.len; ++i) {
        add_include_root(&project->include_dirs, scan->include_roots.items[i]);
    }
    for (i = 0; i < project->source_roots.len; ++i) {
        add_include_root(&project->include_dirs, project->source_roots.items[i]);
    }

    for (i = 0; i < scan->header_files.len; ++i) {
        char *dir = path_dirname(scan->header_files.items[i]);
        add_include_root(&project->include_dirs, dir);
        free(dir);
    }

    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        for (j = 0; j < source->includes.len; ++j) {
            const char *include = source->includes.items[j];
            size_t k;
            if (strchr(include, '/') == NULL) {
                for (k = 0; k < scan->header_files.len; ++k) {
                    if (strcmp(path_basename(scan->header_files.items[k]), include) == 0) {
                        char *dir = path_dirname(scan->header_files.items[k]);
                        add_include_root(&project->include_dirs, dir);
                        free(dir);
                    }
                }
            } else {
                for (k = 0; k < scan->header_files.len; ++k) {
                    const char *header = scan->header_files.items[k];
                    size_t header_len = strlen(header);
                    size_t include_len = strlen(include);
                    if (strcmp(header, include) == 0) {
                        add_include_root(&project->include_dirs, ".");
                    } else if (header_len > include_len &&
                               strcmp(header + header_len - include_len, include) == 0 &&
                               header[header_len - include_len - 1] == '/') {
                        size_t root_len = header_len - include_len - 1;
                        char *root = root_len == 0 ? mazen_xstrdup(".") : mazen_format("%.*s", (int) root_len, header);
                        add_include_root(&project->include_dirs, root);
                        free(root);
                    }
                }
            }
        }
    }
}

bool classifier_analyze(const char *root_dir, const ScanResult *scan, const MazenConfig *config, ProjectInfo *project,
                        Diagnostic *diag) {
    size_t i;
    size_t selected = 0;
    size_t entry_count = 0;
    (void) root_dir;

    project->root_dir = mazen_xstrdup(root_dir);

    if (config->src_dirs.len > 0 || scan->source_roots.len > 0) {
        project->mode = DISCOVERY_CONVENTION;
    } else if (scan->c_files.len > 0 && scan->root_c_file_count == scan->c_files.len) {
        project->mode = DISCOVERY_FLAT;
    } else {
        project->mode = DISCOVERY_SCATTERED;
    }

    scanner_collect_default_ignored(&project->ignored_dirs);
    for (i = 0; i < config->exclude.len; ++i) {
        string_list_push_unique(&project->ignored_dirs, config->exclude.items[i]);
    }
    for (i = 0; i < scan->ignored_dirs.len; ++i) {
        string_list_push_unique(&project->ignored_dirs, scan->ignored_dirs.items[i]);
    }
    for (i = 0; i < config->src_dirs.len; ++i) {
        string_list_push_unique(&project->source_roots, config->src_dirs.items[i]);
    }
    for (i = 0; i < scan->source_roots.len; ++i) {
        string_list_push_unique(&project->source_roots, scan->source_roots.items[i]);
    }
    for (i = 0; i < scan->vendor_roots.len; ++i) {
        string_list_push_unique(&project->vendor_dirs, scan->vendor_roots.items[i]);
    }
    for (i = 0; i < scan->header_files.len; ++i) {
        string_list_push(&project->header_files, scan->header_files.items[i]);
    }

    for (i = 0; i < scan->c_files.len; ++i) {
        char *path = scan->c_files.items[i];
        char *absolute = path_join(root_dir, path);
        char *content = read_text_file(absolute, NULL);
        char *sanitized;
        SourceFile *source;

        if (content == NULL) {
            free(absolute);
            diag_set(diag, "failed to read `%s`", path);
            return false;
        }

        sanitized = strip_comments_and_literals(content);
        source = source_list_push(&project->sources);
        source->path = mazen_xstrdup(path);
        source->directory = path_dirname(path);
        source->basename = mazen_xstrdup(path_basename(path));
        source->has_main = consume_main_definition(sanitized);
        source->role = classify_role(path, source->basename, content);
        collect_includes(content, &source->includes);
        source->entry_score = source->has_main ? entry_score(source, &project->source_roots) : 0;
        source->is_entry_candidate = source->has_main;

        free(content);
        free(sanitized);
        free(absolute);
    }

    for (i = 0; i < project->sources.len; ++i) {
        const SourceFile *source = &project->sources.items[i];
        if (!source->has_main) {
            continue;
        }
        if (!project->has_selected_entry || better_entry_candidate(source, &project->sources.items[selected])) {
            selected = i;
            project->has_selected_entry = true;
        }
        ++entry_count;
    }

    project->selected_entry = selected;
    project->multiple_entrypoints = entry_count > 1;
    infer_include_roots(project, scan, config);
    return true;
}
