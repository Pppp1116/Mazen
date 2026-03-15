#ifndef MAZEN_SCANNER_H
#define MAZEN_SCANNER_H

#include "common.h"
#include "config.h"
#include "diag.h"

typedef struct {
    StringList c_files;
    StringList header_files;
    StringList ignored_dirs;
    StringList source_roots;
    StringList include_roots;
    StringList vendor_roots;
    size_t root_c_file_count;
} ScanResult;

void scan_result_init(ScanResult *scan);
void scan_result_free(ScanResult *scan);
void scanner_collect_default_ignored(StringList *list);
bool scanner_scan(const char *root_dir, const MazenConfig *config, ScanResult *scan, Diagnostic *diag);

#endif
