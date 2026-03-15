#ifndef MAZEN_COMMON_H
#define MAZEN_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define MAZEN_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} String;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} StringList;

typedef enum {
    MAZEN_CMD_BUILD = 0,
    MAZEN_CMD_RUN,
    MAZEN_CMD_TEST,
    MAZEN_CMD_RELEASE,
    MAZEN_CMD_CLEAN,
    MAZEN_CMD_INSTALL,
    MAZEN_CMD_UNINSTALL,
    MAZEN_CMD_DOCTOR,
    MAZEN_CMD_LIST,
    MAZEN_CMD_STANDARDS,
    MAZEN_CMD_HELP,
    MAZEN_CMD_VERSION
} MazenCommand;

typedef enum {
    MAZEN_BUILD_DEBUG = 0,
    MAZEN_BUILD_RELEASE
} MazenBuildMode;

typedef enum {
    MAZEN_CLEAN_OBJECTS = 1u << 0,
    MAZEN_CLEAN_CACHE = 1u << 1,
    MAZEN_CLEAN_COMPDB = 1u << 2,
    MAZEN_CLEAN_OUTPUTS = 1u << 3,
    MAZEN_CLEAN_ALL = (1u << 4) - 1u
} MazenCleanMode;

typedef enum {
    DISCOVERY_CONVENTION = 0,
    DISCOVERY_FLAT,
    DISCOVERY_SCATTERED
} DiscoveryMode;

typedef enum {
    SOURCE_ROLE_GENERAL = 0,
    SOURCE_ROLE_TEST,
    SOURCE_ROLE_EXAMPLE,
    SOURCE_ROLE_EXPERIMENTAL,
    SOURCE_ROLE_VENDOR,
    SOURCE_ROLE_TOOL
} SourceRole;

typedef struct {
    char *path;
    char *directory;
    char *basename;
    bool has_main;
    bool is_entry_candidate;
    SourceRole role;
    int entry_score;
    StringList includes;
} SourceFile;

typedef struct {
    SourceFile *items;
    size_t len;
    size_t cap;
} SourceList;

typedef struct {
    char *root_dir;
    char *project_name;
    DiscoveryMode mode;
    StringList ignored_dirs;
    StringList source_roots;
    StringList include_dirs;
    StringList vendor_dirs;
    StringList header_files;
    SourceList sources;
    size_t selected_entry;
    bool has_selected_entry;
    bool multiple_entrypoints;
} ProjectInfo;

void *mazen_xmalloc(size_t size);
void *mazen_xcalloc(size_t count, size_t size);
void *mazen_xrealloc(void *ptr, size_t size);
char *mazen_xstrdup(const char *text);
char *mazen_vformat(const char *fmt, va_list args);
char *mazen_format(const char *fmt, ...);

void string_init(String *string);
void string_clear(String *string);
void string_free(String *string);
void string_append(String *string, const char *text);
void string_append_n(String *string, const char *text, size_t len);
void string_append_char(String *string, char ch);
void string_appendf(String *string, const char *fmt, ...);
char *string_take(String *string);

void string_list_init(StringList *list);
void string_list_clear(StringList *list);
void string_list_free(StringList *list);
void string_list_push(StringList *list, const char *item);
void string_list_push_take(StringList *list, char *item);
void string_list_push_unique(StringList *list, const char *item);
void string_list_push_unique_take(StringList *list, char *item);
bool string_list_contains(const StringList *list, const char *item);
void string_list_sort(StringList *list);

void source_file_init(SourceFile *source);
void source_file_free(SourceFile *source);
void source_list_init(SourceList *list);
void source_list_free(SourceList *list);
SourceFile *source_list_push(SourceList *list);

void project_info_init(ProjectInfo *project);
void project_info_free(ProjectInfo *project);

char *mazen_getcwd(void);
char *path_join(const char *lhs, const char *rhs);
char *path_join3(const char *a, const char *b, const char *c);
char *path_dirname(const char *path);
const char *path_basename(const char *path);
char *path_without_extension(const char *path);
bool path_has_component(const char *path, const char *component);
size_t path_depth(const char *path);
bool string_starts_with(const char *text, const char *prefix);
bool string_ends_with(const char *text, const char *suffix);
char *trim_in_place(char *text);
bool file_exists(const char *path);
bool dir_exists(const char *path);
long long file_mtime_ns(const char *path);
int ensure_directory(const char *path);
int remove_tree(const char *path);
char *read_text_file(const char *path, size_t *len_out);
bool write_text_file(const char *path, const char *data, size_t len);
bool copy_file_binary(const char *src_path, const char *dst_path);
char *sanitize_stem(const char *path);
uint64_t fnv1a_hash_bytes(const void *data, size_t len, uint64_t seed);
uint64_t fnv1a_hash_string(const char *text, uint64_t seed);
char *hex_u64(uint64_t value);
int compare_cstrings(const void *lhs, const void *rhs);

const char *source_role_name(SourceRole role);
const char *discovery_mode_name(DiscoveryMode mode);

#endif
