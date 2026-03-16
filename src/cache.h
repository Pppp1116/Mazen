#ifndef MAZEN_CACHE_H
#define MAZEN_CACHE_H

#include "common.h"
#include "diag.h"

typedef struct {
    char *source_path;
    char *object_path;
    char *dep_path;
    long long source_mtime_ns;
    long long object_mtime_ns;
    StringList deps;
} BuildRecord;

typedef struct {
    BuildRecord *items;
    size_t len;
    size_t cap;
} BuildRecordList;

typedef struct {
    char *compile_signature;
    char *link_signature;
    bool auto_libs_valid;
    StringList auto_libs;
    BuildRecordList records;
} CacheState;

void build_record_init(BuildRecord *record);
void build_record_free(BuildRecord *record);
void build_record_list_init(BuildRecordList *list);
void build_record_list_free(BuildRecordList *list);

void cache_state_init(CacheState *state);
void cache_state_free(CacheState *state);
bool cache_load(const char *path, CacheState *state, Diagnostic *diag);
bool cache_save(const char *path, const CacheState *state, Diagnostic *diag);
BuildRecord *cache_find_record(CacheState *state, const char *source_path);
BuildRecord *cache_upsert_record(CacheState *state, const char *source_path);
bool cache_parse_depfile(const char *path, StringList *deps);

#endif
