#ifndef MAZEN_COMPDB_H
#define MAZEN_COMPDB_H

#include "common.h"
#include "diag.h"

typedef struct {
    char *directory;
    char *file;
    char *command;
    char *output;
} CompDbEntry;

typedef struct {
    CompDbEntry *items;
    size_t len;
    size_t cap;
} CompDbEntryList;

void compdb_entry_list_init(CompDbEntryList *list);
void compdb_entry_list_free(CompDbEntryList *list);
CompDbEntry *compdb_entry_list_push(CompDbEntryList *list);
void compdb_entry_list_append(CompDbEntryList *dst, const CompDbEntryList *src);
bool compdb_write(const char *path, const CompDbEntryList *list, Diagnostic *diag);

#endif
