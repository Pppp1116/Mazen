#include "config.h"

#include "standard.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    CONFIG_SECTION_GLOBAL = 0,
    CONFIG_SECTION_TARGET
} ConfigSectionKind;

typedef struct {
    ConfigSectionKind kind;
    MazenTargetConfig *target;
} ConfigSection;

static void config_string_list_merge(StringList *dst, const StringList *src, bool unique) {
    size_t i;

    for (i = 0; i < src->len; ++i) {
        if (unique) {
            string_list_push_unique(dst, src->items[i]);
        } else {
            string_list_push(dst, src->items[i]);
        }
    }
}

static void skip_spaces(const char **cursor) {
    while (**cursor != '\0' && isspace((unsigned char) **cursor)) {
        ++(*cursor);
    }
}

static bool parse_quoted_value(const char **cursor, size_t line, char **out, Diagnostic *diag) {
    String buffer;

    if (**cursor != '"') {
        diag_set(diag, "mazen.toml:%zu: expected quoted string", line);
        return false;
    }

    string_init(&buffer);
    ++(*cursor);
    while (**cursor != '\0' && **cursor != '"') {
        if (**cursor == '\\') {
            ++(*cursor);
            if (**cursor == '\0') {
                string_free(&buffer);
                diag_set(diag, "mazen.toml:%zu: unterminated escape sequence", line);
                return false;
            }
            switch (**cursor) {
            case 'n':
                string_append_char(&buffer, '\n');
                break;
            case 'r':
                string_append_char(&buffer, '\r');
                break;
            case 't':
                string_append_char(&buffer, '\t');
                break;
            case '"':
                string_append_char(&buffer, '"');
                break;
            case '\\':
                string_append_char(&buffer, '\\');
                break;
            default:
                string_append_char(&buffer, **cursor);
                break;
            }
            ++(*cursor);
            continue;
        }
        string_append_char(&buffer, **cursor);
        ++(*cursor);
    }

    if (**cursor != '"') {
        string_free(&buffer);
        diag_set(diag, "mazen.toml:%zu: unterminated string", line);
        return false;
    }
    ++(*cursor);

    *out = string_take(&buffer);
    return true;
}

static bool parse_string_value(const char *text, size_t line, char **out, Diagnostic *diag) {
    const char *cursor = text;

    skip_spaces(&cursor);
    if (!parse_quoted_value(&cursor, line, out, diag)) {
        return false;
    }
    skip_spaces(&cursor);
    if (*cursor != '\0') {
        free(*out);
        *out = NULL;
        diag_set(diag, "mazen.toml:%zu: unexpected trailing content", line);
        return false;
    }

    return true;
}

static bool parse_string_array(const char *text, size_t line, StringList *list, Diagnostic *diag) {
    const char *cursor = text;

    skip_spaces(&cursor);
    if (*cursor != '[') {
        diag_set(diag, "mazen.toml:%zu: expected array value", line);
        return false;
    }
    ++cursor;

    for (;;) {
        char *value = NULL;

        skip_spaces(&cursor);
        if (*cursor == ']') {
            ++cursor;
            skip_spaces(&cursor);
            if (*cursor != '\0') {
                diag_set(diag, "mazen.toml:%zu: unexpected trailing content", line);
                return false;
            }
            return true;
        }

        if (!parse_quoted_value(&cursor, line, &value, diag)) {
            return false;
        }
        string_list_push_take(list, value);

        skip_spaces(&cursor);
        if (*cursor == ',') {
            ++cursor;
            continue;
        }
        if (*cursor == ']') {
            ++cursor;
            skip_spaces(&cursor);
            if (*cursor != '\0') {
                diag_set(diag, "mazen.toml:%zu: unexpected trailing content", line);
                return false;
            }
            return true;
        }

        diag_set(diag, "mazen.toml:%zu: expected `,` or `]`", line);
        return false;
    }
}

static bool parse_bool_value(const char *text, size_t line, bool *out, Diagnostic *diag) {
    char *copy = mazen_xstrdup(text);
    char *trimmed = trim_in_place(copy);
    bool ok = true;

    if (strcmp(trimmed, "true") == 0) {
        *out = true;
    } else if (strcmp(trimmed, "false") == 0) {
        *out = false;
    } else {
        diag_set(diag, "mazen.toml:%zu: expected `true` or `false`", line);
        ok = false;
    }

    free(copy);
    return ok;
}

static bool parse_int_value(const char *text, size_t line, int *out, Diagnostic *diag) {
    char *copy = mazen_xstrdup(text);
    char *trimmed = trim_in_place(copy);
    char *end = NULL;
    long value;

    if (*trimmed == '\0') {
        free(copy);
        diag_set(diag, "mazen.toml:%zu: expected integer value", line);
        return false;
    }

    value = strtol(trimmed, &end, 10);
    if (*end != '\0') {
        free(copy);
        diag_set(diag, "mazen.toml:%zu: expected integer value", line);
        return false;
    }
    if (value <= 0) {
        free(copy);
        diag_set(diag, "mazen.toml:%zu: integer value must be greater than zero", line);
        return false;
    }
    if (value > INT_MAX) {
      free(copy);
      diag_set(diag, "mazen.toml:%zu: integer value is too large", line);
      return false;
      
    }

    *out = (int) value;
    free(copy);
    return true;
}

static char *strip_comments(const char *line) {
    String cleaned;
    bool in_string = false;
    bool escape = false;
    const char *cursor = line;

    string_init(&cleaned);
    while (*cursor != '\0') {
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (*cursor == '\\') {
                escape = true;
            } else if (*cursor == '"') {
                in_string = false;
            }
        } else if (*cursor == '"') {
            in_string = true;
        } else if (*cursor == '#') {
            break;
        }

        string_append_char(&cleaned, *cursor);
        ++cursor;
    }

    return string_take(&cleaned);
}

static bool array_is_unclosed(const char *text) {
    bool in_string = false;
    bool escape = false;
    int depth = 0;
    const char *cursor = text;

    while (*cursor != '\0') {
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (*cursor == '\\') {
                escape = true;
            } else if (*cursor == '"') {
                in_string = false;
            }
        } else if (*cursor == '"') {
            in_string = true;
        } else if (*cursor == '[') {
            ++depth;
        } else if (*cursor == ']') {
            --depth;
        }
        ++cursor;
    }

    return depth > 0;
}

static char *next_line(char **cursor) {
    char *line;
    char *newline;

    if (cursor == NULL || *cursor == NULL || **cursor == '\0') {
        return NULL;
    }

    line = *cursor;
    newline = strchr(line, '\n');
    if (newline != NULL) {
        *newline = '\0';
        *cursor = newline + 1;
    } else {
        *cursor = line + strlen(line);
    }
    return line;
}

static bool parse_section_header(const char *text, size_t line, ConfigSection *section, MazenConfig *config,
                                 Diagnostic *diag) {
    size_t len = strlen(text);
    char *name;

    if (len < 3 || text[0] != '[' || text[len - 1] != ']') {
        diag_set(diag, "mazen.toml:%zu: malformed section header", line);
        return false;
    }

    name = mazen_format("%.*s", (int) (len - 2), text + 1);
    if (strcmp(name, "target") == 0) {
        free(name);
        diag_set(diag, "mazen.toml:%zu: target sections must use `[target.NAME]`", line);
        return false;
    }
    if (string_starts_with(name, "target.")) {
        const char *target_name = name + 7;
        MazenTargetConfig *target;

        if (*target_name == '\0') {
            free(name);
            diag_set(diag, "mazen.toml:%zu: target sections must use `[target.NAME]`", line);
            return false;
        }
        if (mazen_target_config_list_find(&config->targets, target_name) != NULL) {
            char *duplicate_name = mazen_xstrdup(target_name);
            free(name);
            diag_set(diag, "mazen.toml:%zu: duplicate target section `%s`", line, duplicate_name);
            free(duplicate_name);
            return false;
        }

        target = mazen_target_config_list_push(&config->targets, target_name);
        section->kind = CONFIG_SECTION_TARGET;
        section->target = target;
        free(name);
        return true;
    }

    free(name);
    diag_set(diag, "mazen.toml:%zu: unsupported section header", line);
    return false;
}

static bool assign_string_field(char **field, const char *value_text, size_t line, Diagnostic *diag) {
    char *parsed = NULL;

    if (!parse_string_value(value_text, line, &parsed, diag)) {
        return false;
    }

    free(*field);
    *field = parsed;
    return true;
}

static bool assign_string_list_field(StringList *list, const char *value_text, size_t line, Diagnostic *diag) {
    StringList parsed;
    size_t i;

    string_list_init(&parsed);
    if (!parse_string_array(value_text, line, &parsed, diag)) {
        string_list_free(&parsed);
        return false;
    }

    string_list_clear(list);
    for (i = 0; i < parsed.len; ++i) {
        string_list_push(list, parsed.items[i]);
    }
    string_list_free(&parsed);
    return true;
}

static bool assign_global_key(MazenConfig *config, const char *key, const char *value_text, size_t line,
                              Diagnostic *diag) {
    if (strcmp(key, "name") == 0) {
        return assign_string_field(&config->name, value_text, line, diag);
    }
    if (strcmp(key, "c_standard") == 0) {
        char *parsed = NULL;
        if (!parse_string_value(value_text, line, &parsed, diag)) {
            return false;
        }
        if (mazen_standard_find(parsed) == NULL) {
            char *bad_value = mazen_xstrdup(parsed);
            free(parsed);
            diag_set(diag, "mazen.toml:%zu: unsupported c_standard `%s`", line, bad_value);
            diag_set_hint(diag, "Run `mazen standards` to list supported values");
            free(bad_value);
            return false;
        }
        free(config->c_standard);
        config->c_standard = parsed;
        return true;
    }
    if (strcmp(key, "jobs") == 0) {
        int jobs = 0;
        if (!parse_int_value(value_text, line, &jobs, diag)) {
            return false;
        }
        config->jobs = jobs;
        config->jobs_set = true;
        return true;
    }
    if (strcmp(key, "default_target") == 0) {
        return assign_string_field(&config->default_target, value_text, line, diag);
    }
    if (strcmp(key, "compile_commands_path") == 0) {
        return assign_string_field(&config->compile_commands_path, value_text, line, diag);
    }
    if (strcmp(key, "include_dirs") == 0) {
        return assign_string_list_field(&config->include_dirs, value_text, line, diag);
    }
    if (strcmp(key, "libs") == 0) {
        return assign_string_list_field(&config->libs, value_text, line, diag);
    }
    if (strcmp(key, "exclude") == 0) {
        return assign_string_list_field(&config->exclude, value_text, line, diag);
    }
    if (strcmp(key, "src_dirs") == 0) {
        return assign_string_list_field(&config->src_dirs, value_text, line, diag);
    }
    if (strcmp(key, "sources") == 0) {
        return assign_string_list_field(&config->sources, value_text, line, diag);
    }
    if (strcmp(key, "cflags") == 0) {
        return assign_string_list_field(&config->cflags, value_text, line, diag);
    }
    if (strcmp(key, "ldflags") == 0) {
        return assign_string_list_field(&config->ldflags, value_text, line, diag);
    }

    diag_set(diag, "mazen.toml:%zu: unsupported key `%s`", line, key);
    return false;
}

static bool assign_target_key(MazenTargetConfig *target, const char *key, const char *value_text, size_t line,
                              Diagnostic *diag) {
    if (strcmp(key, "type") == 0) {
        char *parsed = NULL;
        MazenTargetType type;

        if (!parse_string_value(value_text, line, &parsed, diag)) {
            return false;
        }
        if (!mazen_target_type_from_text(parsed, &type)) {
            diag_set(diag, "mazen.toml:%zu: unsupported target type `%s`", line, parsed);
            free(parsed);
            return false;
        }
        target->type = type;
        target->type_set = true;
        free(parsed);
        return true;
    }
    if (strcmp(key, "default") == 0) {
        bool value = false;
        if (!parse_bool_value(value_text, line, &value, diag)) {
            return false;
        }
        target->default_target = value;
        return true;
    }
    if (strcmp(key, "entry") == 0) {
        return assign_string_field(&target->entry, value_text, line, diag);
    }
    if (strcmp(key, "output") == 0) {
        return assign_string_field(&target->output, value_text, line, diag);
    }
    if (strcmp(key, "src_dirs") == 0) {
        return assign_string_list_field(&target->src_dirs, value_text, line, diag);
    }
    if (strcmp(key, "include_dirs") == 0) {
        return assign_string_list_field(&target->include_dirs, value_text, line, diag);
    }
    if (strcmp(key, "libs") == 0) {
        return assign_string_list_field(&target->libs, value_text, line, diag);
    }
    if (strcmp(key, "cflags") == 0) {
        return assign_string_list_field(&target->cflags, value_text, line, diag);
    }
    if (strcmp(key, "ldflags") == 0) {
        return assign_string_list_field(&target->ldflags, value_text, line, diag);
    }
    if (strcmp(key, "sources") == 0) {
        return assign_string_list_field(&target->sources, value_text, line, diag);
    }
    if (strcmp(key, "exclude") == 0) {
        return assign_string_list_field(&target->exclude, value_text, line, diag);
    }

    diag_set(diag, "mazen.toml:%zu: unsupported target key `%s`", line, key);
    return false;
}

void config_init(MazenConfig *config) {
    config->present = false;
    config->path = NULL;
    config->name = NULL;
    config->c_standard = NULL;
    config->jobs = 0;
    config->jobs_set = false;
    config->default_target = NULL;
    config->compile_commands_path = NULL;
    string_list_init(&config->include_dirs);
    string_list_init(&config->libs);
    string_list_init(&config->exclude);
    string_list_init(&config->src_dirs);
    string_list_init(&config->sources);
    string_list_init(&config->cflags);
    string_list_init(&config->ldflags);
    mazen_target_config_list_init(&config->targets);
}

void config_free(MazenConfig *config) {
    free(config->path);
    free(config->name);
    free(config->c_standard);
    free(config->default_target);
    free(config->compile_commands_path);
    string_list_free(&config->include_dirs);
    string_list_free(&config->libs);
    string_list_free(&config->exclude);
    string_list_free(&config->src_dirs);
    string_list_free(&config->sources);
    string_list_free(&config->cflags);
    string_list_free(&config->ldflags);
    mazen_target_config_list_free(&config->targets);
    config_init(config);
}

bool config_load(const char *root_dir, MazenConfig *config, Diagnostic *diag) {
    char *path = path_join(root_dir, "mazen.toml");
    char *content;
    char *cursor;
    char *line;
    size_t line_number = 0;
    ConfigSection section;

    if (!file_exists(path)) {
        free(path);
        return true;
    }

    content = read_text_file(path, NULL);
    if (content == NULL) {
        diag_set(diag, "failed to read `%s`", path);
        free(path);
        return false;
    }

    section.kind = CONFIG_SECTION_GLOBAL;
    section.target = NULL;
    config->present = true;
    config->path = path;

    cursor = content;
    for (line = next_line(&cursor); line != NULL; line = next_line(&cursor)) {
        char *cleaned;
        char *trimmed;
        char *equals;
        char *key;
        char *value_text;
        char *value_copy = NULL;
        bool ok = false;

        ++line_number;
        cleaned = strip_comments(line);
        trimmed = trim_in_place(cleaned);
        if (*trimmed == '\0') {
            free(cleaned);
            continue;
        }

        if (trimmed[0] == '[') {
            ok = parse_section_header(trimmed, line_number, &section, config, diag);
            free(cleaned);
            if (!ok) {
                free(content);
                return false;
            }
            continue;
        }

        equals = strchr(trimmed, '=');
        if (equals == NULL) {
            free(cleaned);
            free(content);
            diag_set(diag, "mazen.toml:%zu: expected `key = value`", line_number);
            return false;
        }

        *equals = '\0';
        key = trim_in_place(trimmed);
        value_text = trim_in_place(equals + 1);

        if (array_is_unclosed(value_text)) {
            String combined;
            string_init(&combined);
            string_append(&combined, value_text);

            while (array_is_unclosed(combined.data == NULL ? "" : combined.data)) {
                char *continued;

                line = next_line(&cursor);
                if (line == NULL) {
                    string_free(&combined);
                    free(cleaned);
                    free(content);
                    diag_set(diag, "mazen.toml:%zu: unterminated array value", line_number);
                    return false;
                }

                ++line_number;
                continued = strip_comments(line);
                string_append_char(&combined, ' ');
                string_append(&combined, trim_in_place(continued));
                free(continued);
            }

            value_copy = string_take(&combined);
        } else {
            value_copy = mazen_xstrdup(value_text);
        }

        if (section.kind == CONFIG_SECTION_GLOBAL) {
            ok = assign_global_key(config, key, value_copy, line_number, diag);
        } else {
            ok = assign_target_key(section.target, key, value_copy, line_number, diag);
        }

        free(value_copy);
        free(cleaned);
        if (!ok) {
            free(content);
            return false;
        }
    }

    free(content);
    return true;
}

void config_merge_cli(MazenConfig *config, const CliOptions *options) {
    if (options->name_override != NULL) {
        free(config->name);
        config->name = mazen_xstrdup(options->name_override);
    }
    if (options->c_standard != NULL) {
        free(config->c_standard);
        config->c_standard = mazen_xstrdup(options->c_standard);
    }
    if (options->jobs_set) {
        config->jobs = options->jobs;
        config->jobs_set = true;
    }

    config_string_list_merge(&config->include_dirs, &options->include_dirs, true);
    config_string_list_merge(&config->libs, &options->libs, true);
    config_string_list_merge(&config->exclude, &options->excludes, true);
    config_string_list_merge(&config->src_dirs, &options->src_dirs, true);
    config_string_list_merge(&config->cflags, &options->cflags, false);
    config_string_list_merge(&config->ldflags, &options->ldflags, false);
}
