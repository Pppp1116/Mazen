#include "cache.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAZEN_AUTO_LIB_CACHE_VERSION 2U

static void build_record_list_reserve(BuildRecordList *list, size_t needed) {
    size_t cap;
    if (list->cap >= needed) {
        return;
    }
    cap = list->cap == 0 ? 8 : list->cap;
    while (cap < needed) {
        cap *= 2;
    }
    list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
    list->cap = cap;
}

static char *escape_text(const char *text) {
    String out;
    size_t i;

    string_init(&out);
    for (i = 0; text[i] != '\0'; ++i) {
        switch (text[i]) {
        case '\\':
            string_append(&out, "\\\\");
            break;
        case '\n':
            string_append(&out, "\\n");
            break;
        case '\r':
            string_append(&out, "\\r");
            break;
        case '\t':
            string_append(&out, "\\t");
            break;
        case '=':
            string_append(&out, "\\=");
            break;
        default:
            string_append_char(&out, text[i]);
            break;
        }
    }
    return string_take(&out);
}

static char *unescape_text(const char *text) {
    String out;
    size_t i;

    string_init(&out);
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\\' && text[i + 1] != '\0') {
            ++i;
            switch (text[i]) {
            case 'n':
                string_append_char(&out, '\n');
                break;
            case 'r':
                string_append_char(&out, '\r');
                break;
            case 't':
                string_append_char(&out, '\t');
                break;
            default:
                string_append_char(&out, text[i]);
                break;
            }
        } else {
            string_append_char(&out, text[i]);
        }
    }
    return string_take(&out);
}

void build_record_init(BuildRecord *record) {
    record->source_path = NULL;
    record->object_path = NULL;
    record->dep_path = NULL;
    record->source_mtime_ns = -1;
    record->object_mtime_ns = -1;
    string_list_init(&record->deps);
}

void build_record_free(BuildRecord *record) {
    free(record->source_path);
    free(record->object_path);
    free(record->dep_path);
    string_list_free(&record->deps);
    build_record_init(record);
}

void build_record_list_init(BuildRecordList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void build_record_list_free(BuildRecordList *list) {
    size_t i;
    for (i = 0; i < list->len; ++i) {
        build_record_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void cache_state_init(CacheState *state) {
    state->compile_signature = NULL;
    state->link_signature = NULL;
    state->auto_libs_valid = false;
    state->auto_lib_low_confidence = false;
    state->auto_lib_source_hash = NULL;
    string_list_init(&state->auto_libs);
    build_record_list_init(&state->records);
}

void cache_state_free(CacheState *state) {
    free(state->compile_signature);
    free(state->link_signature);
    free(state->auto_lib_source_hash);
    string_list_free(&state->auto_libs);
    build_record_list_free(&state->records);
    cache_state_init(state);
}

BuildRecord *cache_find_record(CacheState *state, const char *source_path) {
    size_t i;
    for (i = 0; i < state->records.len; ++i) {
        if (strcmp(state->records.items[i].source_path, source_path) == 0) {
            return &state->records.items[i];
        }
    }
    return NULL;
}

BuildRecord *cache_upsert_record(CacheState *state, const char *source_path) {
    BuildRecord *record = cache_find_record(state, source_path);
    if (record != NULL) {
        return record;
    }

    build_record_list_reserve(&state->records, state->records.len + 1);
    record = &state->records.items[state->records.len++];
    build_record_init(record);
    record->source_path = mazen_xstrdup(source_path);
    return record;
}

static void append_record(String *buffer, const BuildRecord *record) {
    size_t i;
    char *source = escape_text(record->source_path == NULL ? "" : record->source_path);
    char *object = escape_text(record->object_path == NULL ? "" : record->object_path);
    char *dep = escape_text(record->dep_path == NULL ? "" : record->dep_path);

    string_append(buffer, "record_begin\n");
    string_appendf(buffer, "source=%s\n", source);
    string_appendf(buffer, "object=%s\n", object);
    string_appendf(buffer, "dep=%s\n", dep);
    string_appendf(buffer, "source_mtime=%lld\n", record->source_mtime_ns);
    string_appendf(buffer, "object_mtime=%lld\n", record->object_mtime_ns);
    string_appendf(buffer, "deps=%zu\n", record->deps.len);
    for (i = 0; i < record->deps.len; ++i) {
        char *dep_item = escape_text(record->deps.items[i]);
        string_appendf(buffer, "dep_item=%s\n", dep_item);
        free(dep_item);
    }
    string_append(buffer, "record_end\n");

    free(source);
    free(object);
    free(dep);
}

bool cache_save(const char *path, const CacheState *state, Diagnostic *diag) {
    String buffer;
    size_t i;
    char *compile_sig = escape_text(state->compile_signature == NULL ? "" : state->compile_signature);
    char *link_sig = escape_text(state->link_signature == NULL ? "" : state->link_signature);

    string_init(&buffer);
    string_appendf(&buffer, "compile_signature=%s\n", compile_sig);
    string_appendf(&buffer, "link_signature=%s\n", link_sig);
    string_appendf(&buffer, "auto_lib_cache_version=%u\n", MAZEN_AUTO_LIB_CACHE_VERSION);
    string_appendf(&buffer, "auto_lib_low_confidence=%d\n", state->auto_lib_low_confidence ? 1 : 0);
    {
        char *auto_hash = escape_text(state->auto_lib_source_hash == NULL ? "" : state->auto_lib_source_hash);
        string_appendf(&buffer, "auto_lib_source_hash=%s\n", auto_hash);
        free(auto_hash);
    }
    string_appendf(&buffer, "auto_libs=%zu\n", state->auto_libs.len);
    for (i = 0; i < state->auto_libs.len; ++i) {
        char *auto_lib = escape_text(state->auto_libs.items[i]);
        string_appendf(&buffer, "auto_lib=%s\n", auto_lib);
        free(auto_lib);
    }
    for (i = 0; i < state->records.len; ++i) {
        append_record(&buffer, &state->records.items[i]);
    }

    free(compile_sig);
    free(link_sig);

    if (!write_text_file(path, buffer.data == NULL ? "" : buffer.data, buffer.len)) {
        diag_set(diag, "failed to write cache file `%s`", path);
        string_free(&buffer);
        return false;
    }

    string_free(&buffer);
    return true;
}

static bool parse_key_value(char *line, char **key, char **value) {
    char *equals = strchr(line, '=');
    if (equals == NULL) {
        return false;
    }
    *equals = '\0';
    *key = line;
    *value = equals + 1;
    return true;
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

bool cache_load(const char *path, CacheState *state, Diagnostic *diag) {
    char *content;
    char *cursor;
    char *line;
    BuildRecord *current = NULL;
    size_t expected_deps = 0;
    size_t expected_auto_libs = 0;
    bool saw_auto_lib_count = false;
    bool saw_auto_lib_version = false;
    unsigned int auto_lib_version = 0;

    if (!file_exists(path)) {
        return true;
    }

    content = read_text_file(path, NULL);
    if (content == NULL) {
        diag_set(diag, "failed to read cache file `%s`", path);
        return false;
    }

    cursor = content;
    for (line = next_line(&cursor); line != NULL; line = next_line(&cursor)) {
        char *key;
        char *value;
        if (*line == '\0') {
            continue;
        }
        if (strcmp(line, "record_begin") == 0) {
            build_record_list_reserve(&state->records, state->records.len + 1);
            current = &state->records.items[state->records.len++];
            build_record_init(current);
            expected_deps = 0;
            continue;
        }
        if (strcmp(line, "record_end") == 0) {
            if (current != NULL && expected_deps != current->deps.len) {
                free(content);
                diag_set(diag, "cache file `%s` is malformed", path);
                return false;
            }
            current = NULL;
            continue;
        }
        if (!parse_key_value(line, &key, &value)) {
            free(content);
            diag_set(diag, "cache file `%s` is malformed", path);
            return false;
        }

        if (current == NULL) {
            if (strcmp(key, "compile_signature") == 0) {
                free(state->compile_signature);
                state->compile_signature = unescape_text(value);
            } else if (strcmp(key, "link_signature") == 0) {
                free(state->link_signature);
                state->link_signature = unescape_text(value);
            } else if (strcmp(key, "auto_lib_cache_version") == 0) {
                saw_auto_lib_version = true;
                auto_lib_version = (unsigned int) strtoul(value, NULL, 10);
            } else if (strcmp(key, "auto_lib_low_confidence") == 0) {
                state->auto_lib_low_confidence = atoi(value) != 0;
            } else if (strcmp(key, "auto_lib_source_hash") == 0) {
                free(state->auto_lib_source_hash);
                state->auto_lib_source_hash = unescape_text(value);
            } else if (strcmp(key, "auto_libs") == 0) {
                saw_auto_lib_count = true;
                expected_auto_libs = (size_t) strtoull(value, NULL, 10);
            } else if (strcmp(key, "auto_lib") == 0) {
                string_list_push_take(&state->auto_libs, unescape_text(value));
            }
            continue;
        }

        if (strcmp(key, "source") == 0) {
            current->source_path = unescape_text(value);
        } else if (strcmp(key, "object") == 0) {
            current->object_path = unescape_text(value);
        } else if (strcmp(key, "dep") == 0) {
            current->dep_path = unescape_text(value);
        } else if (strcmp(key, "source_mtime") == 0) {
            current->source_mtime_ns = strtoll(value, NULL, 10);
        } else if (strcmp(key, "object_mtime") == 0) {
            current->object_mtime_ns = strtoll(value, NULL, 10);
        } else if (strcmp(key, "deps") == 0) {
            expected_deps = (size_t) strtoull(value, NULL, 10);
        } else if (strcmp(key, "dep_item") == 0) {
            string_list_push_take(&current->deps, unescape_text(value));
        }
    }

    if (saw_auto_lib_version && saw_auto_lib_count && auto_lib_version == MAZEN_AUTO_LIB_CACHE_VERSION &&
        expected_auto_libs == state->auto_libs.len) {
        state->auto_libs_valid = true;
    } else {
        string_list_clear(&state->auto_libs);
        free(state->auto_lib_source_hash);
        state->auto_lib_source_hash = NULL;
        state->auto_libs_valid = false;
        state->auto_lib_low_confidence = false;
    }

    free(content);
    return true;
}

bool cache_parse_depfile(const char *path, StringList *deps) {
    char *content;
    char *colon;
    char *cursor;

    content = read_text_file(path, NULL);
    if (content == NULL) {
        return false;
    }

    {
        String joined;
        size_t i;
        string_init(&joined);
        for (i = 0; content[i] != '\0'; ++i) {
            if (content[i] == '\\' && content[i + 1] == '\n') {
                ++i;
                continue;
            }
            if (content[i] == '\\' && content[i + 1] == '\r' && content[i + 2] == '\n') {
                i += 2;
                continue;
            }
            string_append_char(&joined, content[i]);
        }
        free(content);
        content = string_take(&joined);
    }

    colon = strchr(content, ':');
    if (colon == NULL) {
        free(content);
        return false;
    }
    cursor = colon + 1;

    while (*cursor != '\0') {
        String token;
        string_init(&token);
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        while (*cursor != '\0' && !isspace((unsigned char) *cursor)) {
            if (*cursor == '\\' && cursor[1] != '\0') {
                ++cursor;
            }
            string_append_char(&token, *cursor++);
        }
        if (token.len > 0) {
            string_list_push_unique_take(deps, string_take(&token));
        } else {
            string_free(&token);
        }
    }

    free(content);
    return true;
}
