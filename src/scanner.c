#include "scanner.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int lstat(const char *path, struct stat *buf);

static const char *DEFAULT_IGNORED_DIRS[] = {
    "build", "out", "dist", ".git", ".svn", "node_modules", "target",
    "bin", "obj", "tmp", "temp", "cache"
};

static const char *SOURCE_ROOT_NAMES[] = {"src", "source"};
static const char *INCLUDE_ROOT_NAMES[] = {"include"};
static const char *VENDOR_ROOT_NAMES[] = {"vendor", "vendors", "third_party", "third-party", "external", "deps"};

static bool list_matches_name(const char *name, const char *const *items, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(name, items[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool path_matches_exclusion(const char *relative, const char *basename, const MazenConfig *config) {
    size_t i;

    if (list_matches_name(basename, DEFAULT_IGNORED_DIRS, MAZEN_ARRAY_LEN(DEFAULT_IGNORED_DIRS))) {
        return true;
    }

    for (i = 0; i < config->exclude.len; ++i) {
        const char *item = config->exclude.items[i];
        size_t len = strlen(item);
        if (strcmp(basename, item) == 0 || strcmp(relative, item) == 0) {
            return true;
        }
        if (string_starts_with(relative, item) && relative[len] == '/') {
            return true;
        }
    }

    return false;
}

static void record_dir_kind(const char *relative, const char *basename, const MazenConfig *config, ScanResult *scan) {
    size_t i;

    if (list_matches_name(basename, SOURCE_ROOT_NAMES, MAZEN_ARRAY_LEN(SOURCE_ROOT_NAMES))) {
        string_list_push_unique(&scan->source_roots, relative);
    }
    if (list_matches_name(basename, INCLUDE_ROOT_NAMES, MAZEN_ARRAY_LEN(INCLUDE_ROOT_NAMES))) {
        string_list_push_unique(&scan->include_roots, relative);
    }
    if (list_matches_name(basename, VENDOR_ROOT_NAMES, MAZEN_ARRAY_LEN(VENDOR_ROOT_NAMES))) {
        string_list_push_unique(&scan->vendor_roots, relative);
    }
    for (i = 0; i < config->src_dirs.len; ++i) {
        if (strcmp(relative, config->src_dirs.items[i]) == 0 || strcmp(basename, config->src_dirs.items[i]) == 0) {
            string_list_push_unique(&scan->source_roots, relative);
        }
    }
}

static bool scan_dir(const char *root_dir, const char *relative, const MazenConfig *config, ScanResult *scan,
                     Diagnostic *diag) {
    char *absolute = relative[0] == '\0' ? mazen_xstrdup(root_dir) : path_join(root_dir, relative);
    DIR *dir = opendir(absolute);
    struct dirent *entry;

    if (dir == NULL) {
        diag_set(diag, "failed to open `%s`: %s", absolute, strerror(errno));
        free(absolute);
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *child_rel;
        char *child_abs;
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        child_rel = relative[0] == '\0' ? mazen_xstrdup(entry->d_name) : path_join(relative, entry->d_name);
        child_abs = path_join(root_dir, child_rel);

        if (lstat(child_abs, &st) != 0) {
            diag_set(diag, "failed to stat `%s`: %s", child_abs, strerror(errno));
            free(child_rel);
            free(child_abs);
            closedir(dir);
            free(absolute);
            return false;
        }

        if (S_ISLNK(st.st_mode)) {
            free(child_rel);
            free(child_abs);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (path_matches_exclusion(child_rel, entry->d_name, config)) {
                string_list_push_unique(&scan->ignored_dirs, child_rel);
            } else {
                record_dir_kind(child_rel, entry->d_name, config, scan);
                if (!scan_dir(root_dir, child_rel, config, scan, diag)) {
                    free(child_rel);
                    free(child_abs);
                    closedir(dir);
                    free(absolute);
                    return false;
                }
            }
        } else if (S_ISREG(st.st_mode)) {
            if (string_ends_with(entry->d_name, ".c")) {
                if (!strchr(child_rel, '/')) {
                    ++scan->root_c_file_count;
                }
                string_list_push(&scan->c_files, child_rel);
            } else if (string_ends_with(entry->d_name, ".h")) {
                string_list_push(&scan->header_files, child_rel);
            }
        }

        free(child_rel);
        free(child_abs);
    }

    closedir(dir);
    free(absolute);
    return true;
}

void scan_result_init(ScanResult *scan) {
    string_list_init(&scan->c_files);
    string_list_init(&scan->header_files);
    string_list_init(&scan->ignored_dirs);
    string_list_init(&scan->source_roots);
    string_list_init(&scan->include_roots);
    string_list_init(&scan->vendor_roots);
    scan->root_c_file_count = 0;
}

void scan_result_free(ScanResult *scan) {
    string_list_free(&scan->c_files);
    string_list_free(&scan->header_files);
    string_list_free(&scan->ignored_dirs);
    string_list_free(&scan->source_roots);
    string_list_free(&scan->include_roots);
    string_list_free(&scan->vendor_roots);
    scan_result_init(scan);
}

void scanner_collect_default_ignored(StringList *list) {
    size_t i;
    for (i = 0; i < MAZEN_ARRAY_LEN(DEFAULT_IGNORED_DIRS); ++i) {
        string_list_push_unique(list, DEFAULT_IGNORED_DIRS[i]);
    }
}

bool scanner_scan(const char *root_dir, const MazenConfig *config, ScanResult *scan, Diagnostic *diag) {
    if (!scan_dir(root_dir, "", config, scan, diag)) {
        return false;
    }

    string_list_sort(&scan->c_files);
    string_list_sort(&scan->header_files);
    string_list_sort(&scan->ignored_dirs);
    string_list_sort(&scan->source_roots);
    string_list_sort(&scan->include_roots);
    string_list_sort(&scan->vendor_roots);
    return true;
}
