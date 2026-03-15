#include "config.h"
#include "standard.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void config_string_list_merge(StringList *dst, const StringList *src) {
    size_t i;
    for (i = 0; i < src->len; ++i) {
        string_list_push_unique(dst, src->items[i]);
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

void config_init(MazenConfig *config) {
    config->present = false;
    config->path = NULL;
    config->name = NULL;
    config->c_standard = NULL;
    string_list_init(&config->include_dirs);
    string_list_init(&config->libs);
    string_list_init(&config->exclude);
    string_list_init(&config->src_dirs);
    string_list_init(&config->cflags);
    string_list_init(&config->ldflags);
}

void config_free(MazenConfig *config) {
    free(config->path);
    free(config->name);
    free(config->c_standard);
    string_list_free(&config->include_dirs);
    string_list_free(&config->libs);
    string_list_free(&config->exclude);
    string_list_free(&config->src_dirs);
    string_list_free(&config->cflags);
    string_list_free(&config->ldflags);
    config_init(config);
}

bool config_load(const char *root_dir, MazenConfig *config, Diagnostic *diag) {
    char *path = path_join(root_dir, "mazen.toml");
    char *content;
    char *cursor;
    char *line;
    size_t line_number = 0;

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

    config->present = true;
    config->path = path;

    cursor = content;
    line = next_line(&cursor);
    while (line != NULL) {
        char *cleaned;
        char *trimmed;
        char *equals;
        char *key;
        char *value_text;
        char *value_copy = NULL;

        ++line_number;
        cleaned = strip_comments(line);
        trimmed = trim_in_place(cleaned);
        if (*trimmed == '\0') {
            free(cleaned);
            line = next_line(&cursor);
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

        if (strcmp(key, "name") == 0) {
            char *parsed = NULL;
            if (!parse_string_value(value_copy, line_number, &parsed, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
            free(config->name);
            config->name = parsed;
        } else if (strcmp(key, "c_standard") == 0) {
            char *parsed = NULL;
            if (!parse_string_value(value_copy, line_number, &parsed, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
            if (mazen_standard_find(parsed) == NULL) {
                char *bad_value = mazen_xstrdup(parsed);
                free(parsed);
                free(value_copy);
                free(cleaned);
                free(content);
                diag_set(diag, "mazen.toml:%zu: unsupported c_standard `%s`", line_number, bad_value);
                diag_set_hint(diag, "Run `mazen standards` to list supported values");
                free(bad_value);
                return false;
            }
            free(config->c_standard);
            config->c_standard = parsed;
        } else if (strcmp(key, "include_dirs") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->include_dirs, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else if (strcmp(key, "libs") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->libs, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else if (strcmp(key, "exclude") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->exclude, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else if (strcmp(key, "src_dirs") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->src_dirs, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else if (strcmp(key, "cflags") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->cflags, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else if (strcmp(key, "ldflags") == 0) {
            if (!parse_string_array(value_copy, line_number, &config->ldflags, diag)) {
                free(value_copy);
                free(cleaned);
                free(content);
                return false;
            }
        } else {
            free(value_copy);
            free(cleaned);
            free(content);
            diag_set(diag, "mazen.toml:%zu: unsupported key `%s`", line_number, key);
            return false;
        }

        free(value_copy);
        free(cleaned);
        line = next_line(&cursor);
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

    config_string_list_merge(&config->include_dirs, &options->include_dirs);
    config_string_list_merge(&config->libs, &options->libs);
    config_string_list_merge(&config->exclude, &options->excludes);
    config_string_list_merge(&config->src_dirs, &options->src_dirs);
    config_string_list_merge(&config->cflags, &options->cflags);
    config_string_list_merge(&config->ldflags, &options->ldflags);
}
