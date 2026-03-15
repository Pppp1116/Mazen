#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int lstat(const char *path, struct stat *buf);

static void die_out_of_memory(void) {
    fputs("fatal: out of memory\n", stderr);
    exit(EXIT_FAILURE);
}

void *mazen_xmalloc(size_t size) {
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        die_out_of_memory();
    }
    return ptr;
}

void *mazen_xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    if (ptr == NULL) {
        die_out_of_memory();
    }
    return ptr;
}

void *mazen_xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size == 0 ? 1 : size);
    if (next == NULL) {
        die_out_of_memory();
    }
    return next;
}

char *mazen_xstrdup(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    len = strlen(text);
    copy = mazen_xmalloc(len + 1);
    memcpy(copy, text, len + 1);
    return copy;
}

char *mazen_vformat(const char *fmt, va_list args) {
    va_list copy;
    int needed;
    char *buffer;

    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        return mazen_xstrdup("formatting error");
    }

    buffer = mazen_xmalloc((size_t) needed + 1);
    vsnprintf(buffer, (size_t) needed + 1, fmt, args);
    return buffer;
}

char *mazen_format(const char *fmt, ...) {
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = mazen_vformat(fmt, args);
    va_end(args);
    return buffer;
}

static void string_reserve(String *string, size_t needed) {
    size_t cap;

    if (string->cap >= needed) {
        return;
    }

    cap = string->cap == 0 ? 32 : string->cap;
    while (cap < needed) {
        cap *= 2;
    }

    string->data = mazen_xrealloc(string->data, cap);
    string->cap = cap;
}

void string_init(String *string) {
    string->data = NULL;
    string->len = 0;
    string->cap = 0;
}

void string_clear(String *string) {
    if (string->data != NULL) {
        string->data[0] = '\0';
    }
    string->len = 0;
}

void string_free(String *string) {
    free(string->data);
    string->data = NULL;
    string->len = 0;
    string->cap = 0;
}

void string_append(String *string, const char *text) {
    string_append_n(string, text, strlen(text));
}

void string_append_n(String *string, const char *text, size_t len) {
    string_reserve(string, string->len + len + 1);
    memcpy(string->data + string->len, text, len);
    string->len += len;
    string->data[string->len] = '\0';
}

void string_append_char(String *string, char ch) {
    string_reserve(string, string->len + 2);
    string->data[string->len++] = ch;
    string->data[string->len] = '\0';
}

void string_appendf(String *string, const char *fmt, ...) {
    va_list args;
    char *formatted;

    va_start(args, fmt);
    formatted = mazen_vformat(fmt, args);
    va_end(args);
    string_append(string, formatted);
    free(formatted);
}

char *string_take(String *string) {
    char *data = string->data;
    if (data == NULL) {
        data = mazen_xstrdup("");
    }
    string->data = NULL;
    string->len = 0;
    string->cap = 0;
    return data;
}

static void string_list_reserve(StringList *list, size_t needed) {
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

void string_list_init(StringList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void string_list_clear(StringList *list) {
    size_t i;
    for (i = 0; i < list->len; ++i) {
        free(list->items[i]);
    }
    list->len = 0;
}

void string_list_free(StringList *list) {
    string_list_clear(list);
    free(list->items);
    list->items = NULL;
    list->cap = 0;
}

void string_list_push(StringList *list, const char *item) {
    string_list_push_take(list, mazen_xstrdup(item));
}

void string_list_push_take(StringList *list, char *item) {
    string_list_reserve(list, list->len + 1);
    list->items[list->len++] = item;
}

bool string_list_contains(const StringList *list, const char *item) {
    size_t i;
    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i], item) == 0) {
            return true;
        }
    }
    return false;
}

void string_list_push_unique(StringList *list, const char *item) {
    if (!string_list_contains(list, item)) {
        string_list_push(list, item);
    }
}

void string_list_push_unique_take(StringList *list, char *item) {
    if (string_list_contains(list, item)) {
        free(item);
    } else {
        string_list_push_take(list, item);
    }
}

void string_list_sort(StringList *list) {
    qsort(list->items, list->len, sizeof(*list->items), compare_cstrings);
}

void source_file_init(SourceFile *source) {
    source->path = NULL;
    source->directory = NULL;
    source->basename = NULL;
    source->has_main = false;
    source->is_entry_candidate = false;
    source->role = SOURCE_ROLE_GENERAL;
    source->entry_score = 0;
    string_list_init(&source->includes);
}

void source_file_free(SourceFile *source) {
    free(source->path);
    free(source->directory);
    free(source->basename);
    string_list_free(&source->includes);
    source_file_init(source);
}

void source_list_init(SourceList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void source_list_free(SourceList *list) {
    size_t i;
    for (i = 0; i < list->len; ++i) {
        source_file_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

SourceFile *source_list_push(SourceList *list) {
    size_t cap;

    if (list->len == list->cap) {
        cap = list->cap == 0 ? 8 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }

    source_file_init(&list->items[list->len]);
    return &list->items[list->len++];
}

void project_info_init(ProjectInfo *project) {
    project->root_dir = NULL;
    project->project_name = NULL;
    project->mode = DISCOVERY_SCATTERED;
    string_list_init(&project->ignored_dirs);
    string_list_init(&project->source_roots);
    string_list_init(&project->include_dirs);
    string_list_init(&project->vendor_dirs);
    string_list_init(&project->header_files);
    source_list_init(&project->sources);
    project->selected_entry = 0;
    project->has_selected_entry = false;
    project->multiple_entrypoints = false;
}

void project_info_free(ProjectInfo *project) {
    free(project->root_dir);
    free(project->project_name);
    string_list_free(&project->ignored_dirs);
    string_list_free(&project->source_roots);
    string_list_free(&project->include_dirs);
    string_list_free(&project->vendor_dirs);
    string_list_free(&project->header_files);
    source_list_free(&project->sources);
    project_info_init(project);
}

char *mazen_getcwd(void) {
    size_t size = 256;
    char *buffer = NULL;

    for (;;) {
        buffer = mazen_xrealloc(buffer, size);
        if (getcwd(buffer, size) != NULL) {
            return buffer;
        }
        if (errno != ERANGE) {
            free(buffer);
            return NULL;
        }
        size *= 2;
    }
}

char *path_join(const char *lhs, const char *rhs) {
    size_t lhs_len = strlen(lhs);
    size_t rhs_len = strlen(rhs);
    bool needs_sep = lhs_len > 0 && lhs[lhs_len - 1] != '/';
    char *joined = mazen_xmalloc(lhs_len + rhs_len + (needs_sep ? 2 : 1));

    memcpy(joined, lhs, lhs_len);
    if (needs_sep) {
        joined[lhs_len++] = '/';
    }
    memcpy(joined + lhs_len, rhs, rhs_len + 1);
    return joined;
}

char *path_join3(const char *a, const char *b, const char *c) {
    char *ab = path_join(a, b);
    char *abc = path_join(ab, c);
    free(ab);
    return abc;
}

char *path_dirname(const char *path) {
    const char *slash = strrchr(path, '/');

    if (slash == NULL) {
        return mazen_xstrdup(".");
    }

    if (slash == path) {
        char *root = mazen_xmalloc(2);
        root[0] = '/';
        root[1] = '\0';
        return root;
    }

    return mazen_format("%.*s", (int) (slash - path), path);
}

const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

char *path_without_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');

    if (dot == NULL || (slash != NULL && dot < slash)) {
        return mazen_xstrdup(path);
    }

    return mazen_format("%.*s", (int) (dot - path), path);
}

bool path_has_component(const char *path, const char *component) {
    const char *cursor = path;
    size_t component_len = strlen(component);

    while (*cursor != '\0') {
        const char *slash = strchr(cursor, '/');
        size_t len = slash == NULL ? strlen(cursor) : (size_t) (slash - cursor);
        if (len == component_len && strncmp(cursor, component, len) == 0) {
            return true;
        }
        if (slash == NULL) {
            break;
        }
        cursor = slash + 1;
    }

    return false;
}

size_t path_depth(const char *path) {
    size_t depth = 0;
    const char *cursor = path;

    while (*cursor != '\0') {
        const char *slash = strchr(cursor, '/');
        size_t len = slash == NULL ? strlen(cursor) : (size_t) (slash - cursor);
        if (len > 0 && !(len == 1 && cursor[0] == '.')) {
            ++depth;
        }
        if (slash == NULL) {
            break;
        }
        cursor = slash + 1;
    }

    return depth;
}

bool string_starts_with(const char *text, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

bool string_ends_with(const char *text, const char *suffix) {
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > text_len) {
        return false;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

char *trim_in_place(char *text) {
    char *start = text;
    char *end;

    while (*start != '\0' && isspace((unsigned char) *start)) {
        ++start;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1])) {
        --end;
    }
    *end = '\0';
    return start;
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

long long file_mtime_ns(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long long) st.st_mtime * 1000000000LL;
}

static int mkdir_if_missing(const char *path) {
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}

int ensure_directory(const char *path) {
    char *copy;
    char *cursor;
    int rc = 0;

    if (path == NULL || path[0] == '\0' || strcmp(path, ".") == 0) {
        return 0;
    }

    copy = mazen_xstrdup(path);
    for (cursor = copy + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (copy[0] != '\0' && mkdir_if_missing(copy) != 0) {
                rc = -1;
                goto out;
            }
            *cursor = '/';
        }
    }

    if (mkdir_if_missing(copy) != 0) {
        rc = -1;
    }

out:
    free(copy);
    return rc;
}

int remove_tree(const char *path) {
    struct stat st;

    if (lstat(path, &st) != 0) {
        return errno == ENOENT ? 0 : -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry;

        if (dir == NULL) {
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            char *child;
            int rc;

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            child = path_join(path, entry->d_name);
            rc = remove_tree(child);
            free(child);
            if (rc != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);
        return rmdir(path);
    }

    return unlink(path);
}

char *read_text_file(const char *path, size_t *len_out) {
    FILE *file = fopen(path, "rb");
    char *buffer;
    size_t size;
    size_t read_count;

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    {
        long end = ftell(file);
        if (end < 0) {
            fclose(file);
            return NULL;
        }
        size = (size_t) end;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = mazen_xmalloc(size + 1);
    read_count = fread(buffer, 1, size, file);
    fclose(file);
    if (read_count != size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    if (len_out != NULL) {
        *len_out = size;
    }
    return buffer;
}

bool write_text_file(const char *path, const char *data, size_t len) {
    FILE *file = fopen(path, "wb");
    size_t written;

    if (file == NULL) {
        return false;
    }

    written = fwrite(data, 1, len, file);
    fclose(file);
    return written == len;
}

char *sanitize_stem(const char *path) {
    char *stem = path_without_extension(path);
    size_t i;

    for (i = 0; stem[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char) stem[i];
        if (isalnum(ch)) {
            continue;
        }
        stem[i] = '_';
    }

    if (stem[0] == '\0') {
        free(stem);
        return mazen_xstrdup("object");
    }

    return stem;
}

uint64_t fnv1a_hash_bytes(const void *data, size_t len, uint64_t seed) {
    const unsigned char *bytes = data;
    uint64_t hash = seed;
    size_t i;

    for (i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

uint64_t fnv1a_hash_string(const char *text, uint64_t seed) {
    return fnv1a_hash_bytes(text, strlen(text), seed);
}

char *hex_u64(uint64_t value) {
    return mazen_format("%016llx", (unsigned long long) value);
}

int compare_cstrings(const void *lhs, const void *rhs) {
    const char *const *left = lhs;
    const char *const *right = rhs;
    return strcmp(*left, *right);
}

const char *source_role_name(SourceRole role) {
    switch (role) {
    case SOURCE_ROLE_GENERAL:
        return "general";
    case SOURCE_ROLE_TEST:
        return "test";
    case SOURCE_ROLE_EXAMPLE:
        return "example";
    case SOURCE_ROLE_EXPERIMENTAL:
        return "experimental";
    case SOURCE_ROLE_VENDOR:
        return "vendor";
    case SOURCE_ROLE_TOOL:
        return "tool";
    default:
        return "unknown";
    }
}

const char *discovery_mode_name(DiscoveryMode mode) {
    switch (mode) {
    case DISCOVERY_CONVENTION:
        return "convention";
    case DISCOVERY_FLAT:
        return "flat";
    case DISCOVERY_SCATTERED:
        return "discovery";
    default:
        return "unknown";
    }
}
