#include "profile.h"

#include <stdlib.h>
#include <string.h>

void mazen_profile_init(MazenProfile *profile) {
    profile->name = NULL;
    profile->mode_set = false;
    profile->mode = MAZEN_BUILD_DEBUG;
    string_list_init(&profile->include_dirs);
    string_list_init(&profile->libs);
    string_list_init(&profile->cflags);
    string_list_init(&profile->ldflags);
}

void mazen_profile_free(MazenProfile *profile) {
    free(profile->name);
    string_list_free(&profile->include_dirs);
    string_list_free(&profile->libs);
    string_list_free(&profile->cflags);
    string_list_free(&profile->ldflags);
    mazen_profile_init(profile);
}

void mazen_profile_list_init(MazenProfileList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void mazen_profile_list_free(MazenProfileList *list) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        mazen_profile_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

MazenProfile *mazen_profile_list_find(MazenProfileList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

const MazenProfile *mazen_profile_list_find_const(const MazenProfileList *list, const char *name) {
    size_t i;

    for (i = 0; i < list->len; ++i) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

MazenProfile *mazen_profile_list_push(MazenProfileList *list, const char *name) {
    size_t cap;
    MazenProfile *profile;

    if (list->len == list->cap) {
        cap = list->cap == 0 ? 4 : list->cap * 2;
        list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
        list->cap = cap;
    }

    profile = &list->items[list->len++];
    mazen_profile_init(profile);
    profile->name = mazen_xstrdup(name);
    return profile;
}

bool mazen_build_mode_from_text(const char *text, MazenBuildMode *mode) {
    if (strcmp(text, "debug") == 0) {
        *mode = MAZEN_BUILD_DEBUG;
        return true;
    }
    if (strcmp(text, "release") == 0) {
        *mode = MAZEN_BUILD_RELEASE;
        return true;
    }
    return false;
}

const char *mazen_build_mode_name(MazenBuildMode mode) {
    switch (mode) {
    case MAZEN_BUILD_RELEASE:
        return "release";
    case MAZEN_BUILD_DEBUG:
    default:
        return "debug";
    }
}

const MazenProfile *mazen_profile_select(const MazenProfileList *profiles, const char *requested_name,
                                         const char *default_name, Diagnostic *diag) {
    const char *name = requested_name != NULL ? requested_name : default_name;
    const MazenProfile *profile;

    if (name == NULL) {
        return NULL;
    }

    profile = mazen_profile_list_find_const(profiles, name);
    if (profile == NULL) {
        diag_set(diag, "profile `%s` is not defined in mazen.toml", name);
        return NULL;
    }

    return profile;
}
