#ifndef MAZEN_PROFILE_H
#define MAZEN_PROFILE_H

#include "common.h"
#include "diag.h"

typedef struct {
    char *name;
    bool mode_set;
    MazenBuildMode mode;
    StringList include_dirs;
    StringList libs;
    StringList cflags;
    StringList ldflags;
} MazenProfile;

typedef struct {
    MazenProfile *items;
    size_t len;
    size_t cap;
} MazenProfileList;

void mazen_profile_init(MazenProfile *profile);
void mazen_profile_free(MazenProfile *profile);
void mazen_profile_list_init(MazenProfileList *list);
void mazen_profile_list_free(MazenProfileList *list);
MazenProfile *mazen_profile_list_find(MazenProfileList *list, const char *name);
const MazenProfile *mazen_profile_list_find_const(const MazenProfileList *list, const char *name);
MazenProfile *mazen_profile_list_push(MazenProfileList *list, const char *name);

bool mazen_build_mode_from_text(const char *text, MazenBuildMode *mode);
const char *mazen_build_mode_name(MazenBuildMode mode);
const MazenProfile *mazen_profile_select(const MazenProfileList *profiles, const char *requested_name,
                                         const char *default_name, Diagnostic *diag);

#endif
